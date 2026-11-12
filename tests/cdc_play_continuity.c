/* Adapted from paps-g b8c71ee2. Exercise the full disk-task control flow
 * with authored sector input and deterministic command-task preemption. */
#include <pthread.h>
#include <setjmp.h>
#include "drive-harness.inc"

enum checkpoint { NO_COMMAND, STARTUP, FIRST_READ };
static enum checkpoint gate_at;
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int worker_at_gate, request_waiting, acknowledged, command_returned, worker_finished;
static int ack_status, ack_type, initial_status, lookup_error, read_error, finish_after;
static int sem_calls, play_delay, sector_delay, seek_delays, last_read_status;
static int delay_failure, delay_calls, filtered, queued;
static int delay_request, delay_request_at, audio_depth, filter_result, accepted;
static int resume_end, hirq_at_ack, filtered_at_ack, queued_at_ack;
static int error_reports, reported_reader;
static int replace_on_eof, eof_publications, lookup_calls;
static u32 ack_fad, last_read_fad;
static jmp_buf worker_exit;

static void wait_for_command(void)
{
    assert(!pthread_mutex_lock(&gate));
    worker_at_gate = 1;
    assert(!pthread_cond_broadcast(&changed));
    while (!request_waiting) assert(!pthread_cond_wait(&changed, &gate));
    assert(!pthread_mutex_unlock(&gate));
}

static void wait_for_ack(void)
{
    assert(!pthread_mutex_lock(&gate));
    request_waiting = 1;
    assert(!pthread_cond_broadcast(&changed));
    while (!acknowledged && !worker_finished)
        assert(!pthread_cond_wait(&changed, &gate));
    assert(acknowledged);
    assert(!pthread_mutex_unlock(&gate));
}

static void acknowledge(void)
{
    assert(!pthread_mutex_lock(&gate));
    ack_status = cdb.status;
    ack_type = cdb.play_type;
    ack_fad = cdb.fad;
    acknowledged = 1;
    assert(!pthread_cond_broadcast(&changed));
    while (!command_returned) assert(!pthread_cond_wait(&changed, &gate));
    assert(!pthread_mutex_unlock(&gate));
}

static int next_disc = -1;
static const int sem_wait_disc = 0, osErrorTimeout = -2;
int sd_card_insert(void) { abort(); }
static void fs_lock(void) { abort(); }
static void fs_unlock(void) { abort(); }
static int disk_reidentify(void) { abort(); }
static int load_disc(int disc, int start_save) { abort(); }
static void unload_disc(void) { abort(); }
static int handle_diread(void) { abort(); }
static void advance_file_unit(void) { assert(!cdb.file_unit); }
static void set_peri_report(void)
{
    if (cdb.status == STAT_ERROR) {
        error_reports++;
        reported_reader = cdb.play_type;
    }
}
static void led_event(int event) { assert(event == 1); }
static void spi2_flush(void) {}
static int fill_audio_buffer(const u8 *data)
{
    assert(data == sector_bytes[0]);
    queued++;
    return audio_depth;
}
static int filter_sector(TRACK_INFO *track, BLOCK *block)
{
    assert(track == cdb.play_track && block->data == sector_bytes[0]);
    filtered++;
    accepted += filter_result == 0;
    return filter_result;
}

static int osSemaphoreAcquire(int semaphore, int ticks)
{
    assert(semaphore == sem_wait_disc && ticks >= 1);
    if (sem_calls++) {
        if (sem_calls == 2 && cdb.pause_request) return 0;
        longjmp(worker_exit, 1);
    }
    cdb.status = initial_status;
    if (gate_at == STARTUP) wait_for_command();
    return 0;
}

static int fail_sector(int result)
{
    cdb.status = STAT_ERROR;
    if (!HAS_FAILED_READ_RETIREMENT && cdb.play_type != PLAYTYPE_DIR) {
        cdb.play_type = 0;
        cdb.play_track = NULL;
    }
    return result;
}

static int get_sector(int fad, BLOCK *block)
{
    if (!block) {
        lookup_calls++;
        if (lookup_error) {
            cdb.play_track = NULL;
            return fail_sector(-1);
        }
        if (!cdb.play_track) set_drive_position(fad);
        return 0;
    }
    last_read_fad = fad;
    last_read_status = cdb.status;
    read_calls++;
    if (gate_at == FIRST_READ && read_calls == 1) wait_for_command();
    if (read_error) return fail_sector(-2);
    if (read_calls == (unsigned)finish_after) longjmp(worker_exit, 1);
    block->data = sector_bytes[0];
    return 0;
}

static int worker_delay(int ticks)
{
    if (ticks == play_delay) seek_delays++;
    ++delay_calls;
    if (delay_calls == delay_request_at) cdb.pause_request = delay_request;
    return delay_calls == delay_failure ? -1 : 0;
}

static void eof_release_probe(void)
{
    if (!replace_on_eof) return;
    assert(!cdb.play_type && (hirq & HIRQ_PEND));
    ++eof_publications;
    cdb.index = 2; /* The higher-priority command publishes its new index. */
}

#define LEDEV_CSCT 1
#define hw_delay worker_delay
#include "disk-task.inc"
#undef hw_delay

static void *run_worker(void *unused)
{
    if (!setjmp(worker_exit)) disk_task(NULL);
    assert(!pthread_mutex_lock(&gate));
    worker_finished = 1;
    assert(!pthread_cond_broadcast(&changed));
    assert(!pthread_mutex_unlock(&gate));
    return NULL;
}

static void prepare(u32 fad, int audio, int status)
{
    ready();
    gate_at = NO_COMMAND;
    initial_status = status;
    cdb.play_type = PLAYTYPE_SECTOR;
    set_drive_position(fad);
    if (audio) cdb.play_track->mode = 3;
    cdb.options = audio ? 0 : 8;
    cdb.old_fad = fad - 1;
    play_delay = 700;
    worker_at_gate = request_waiting = acknowledged = command_returned = worker_finished = 0;
    ack_status = ack_type = read_error = sem_calls = seek_delays = last_read_status = 0;
    delay_failure = delay_calls = filtered = queued = sector_delay = pend_delay = 0;
    delay_request = delay_request_at = filter_result = accepted = resume_end = 0;
    hirq_at_ack = filtered_at_ack = queued_at_ack = 0;
    audio_depth = 5;
    lookup_error = error_reports = reported_reader = 0;
    replace_on_eof = eof_publications = lookup_calls = 0;
    ack_fad = last_read_fad = 0;
    finish_after = 1;
}

static pthread_t start_update(enum checkpoint checkpoint, int timeout)
{
    gate_at = checkpoint;
    drive_ack_hook = acknowledge;
    if (timeout) acknowledge_after = -1;
    else drive_wait_hook = wait_for_ack;
    pthread_t worker;
    assert(!pthread_create(&worker, NULL, run_worker, NULL));
    assert(!pthread_mutex_lock(&gate));
    while (!worker_at_gate) assert(!pthread_cond_wait(&changed, &gate));
    assert(!pthread_mutex_unlock(&gate));
    return worker;
}

static void finish_update(pthread_t worker)
{
    assert(!pthread_mutex_lock(&gate));
    request_waiting = command_returned = 1;
    assert(!pthread_cond_broadcast(&changed));
    assert(!pthread_mutex_unlock(&gate));
    assert(!pthread_join(worker, NULL));
}

static void range_update(void)
{
    for (unsigned audio = 0; audio < 2; audio++) {
        for (unsigned endpoint = 0; endpoint < 2; endpoint++) {
            for (unsigned repeat = 0; repeat < 16; repeat++) {
                prepare(249, audio, STAT_PLAY);
                cdb.play_fad_end = endpoint ? 250 : 700;
                finish_after = 2;
                request(0x800096, 0x8000aa, 0x80 | repeat);
                pthread_t worker = start_update(FIRST_READ, 0);
                assert(!play_cd());
                assert(ack_fad == 250 && ack_status == STAT_PLAY && ack_type == PLAYTYPE_SECTOR);
                assert(cdb.fad == 250 && cdb.old_fad == 249 && cdb.options == (audio ? 0 : 8));
                assert(cdb.play_fad_end == 320 && cdb.max_repeat == (int)repeat);
                assert(waits == 1 && wakeups == 1 && !cdb.pause_request);
                finish_update(worker);
                assert(read_calls == 2 && last_read_fad == 250 && last_read_status == STAT_PLAY);
                assert(!seek_delays && !(hirq & HIRQ_PEND));
                assert(buf_fad_start == 123 && buf_fad_end == 456);
            }
        }
    }
}

static void paused_update(void)
{
    prepare(150, 0, STAT_PAUSE);
    cdb.play_wait = 1;
    request(0xffffff, 0xffffff, 0xff);
    pthread_t worker = start_update(STARTUP, 0);
    assert(!play_cd());
    assert(ack_status == STAT_PAUSE && ack_type == PLAYTYPE_SECTOR);
    assert(!cdb.play_wait && wakeups == 1);
    finish_update(worker);
    assert(last_read_status == STAT_PLAY && last_read_fad == 150 && !seek_delays);
}

static void shorter_range(void)
{
    for (unsigned repeat = 0; repeat < 2; repeat++) {
        prepare(249, 0, STAT_PLAY);
        finish_after = 2;
        request(0x800096, 0x800064, 0x80 | repeat);
        pthread_t worker = start_update(FIRST_READ, 0);
        assert(!play_cd() && ack_fad == 250 && ack_type == PLAYTYPE_SECTOR);
        finish_update(worker);
        if (repeat) {
            assert(cdb.repcnt == 1 && last_read_fad == 150 && read_calls == 2);
            assert(cdb.play_type == PLAYTYPE_SECTOR && !(hirq & HIRQ_PEND));
        } else {
            assert(cdb.status == STAT_PAUSE && !cdb.play_type && read_calls == 1);
            assert(hirq == HIRQ_PEND);
        }
    }
}

static void timed_out_update(void)
{
    prepare(249, 0, STAT_PLAY);
    cdb.max_repeat = 3;
    cdb.repcnt = 1;
    finish_after = 2;
    request(0x800096, 0x8000aa, 0x84);
    pthread_t worker = start_update(FIRST_READ, 1);
    assert(!play_cd());
    assert(waits == 500 && cdb.pause_request && reply[0] >> 8 == (STAT_WAIT | STAT_PLAY));
    assert(cdb.play_fad_end == 700 && cdb.max_repeat == 3 && !hirq_clear);
    finish_update(worker);
    assert(!cdb.pause_request && cdb.max_repeat == 3 && cdb.repcnt == 1);
    assert(cdb.play_fad_end == 700 && read_calls == 2 && last_read_fad == 250);
}

static void failed_reader(void)
{
    prepare(249, 0, STAT_PLAY);
    read_error = 1;
    request(0x8000f9, 0x800064, 0x80);
    pthread_t worker = start_update(FIRST_READ, 0);
    assert(!play_cd());
    assert(ack_status == STAT_PAUSE && !ack_type && waits == 1 && wakeups == 2);
    assert(cdb.play_type == PLAYTYPE_SECTOR && cdb.fad == 249 && cdb.play_fad_end == 349);
    finish_update(worker);
    assert(read_calls == 1 && !(hirq & HIRQ_PEND) && !cdb.pause_request);
    if (HAS_FAILED_READ_RETIREMENT)
        assert(error_reports == 1 && reported_reader == PLAYTYPE_SECTOR);
}

static void replacement(void)
{
    for (int kind = PLAYTYPE_SECTOR; kind <= PLAYTYPE_DIR; kind++) {
        prepare(250, 0, STAT_PLAY);
        cdb.play_type = kind;
        cdb.rdir_state = 2;
        cdb.cdir_pfnum = 7;
        request(0x101, 0x100, kind == PLAYTYPE_SECTOR ? 0 : 0x80);
        pthread_t worker = start_update(STARTUP, 0);
        assert(!play_cd());
        assert(ack_status == STAT_PAUSE && !ack_type && waits == 1 && wakeups == 2);
        assert(cdb.fad == (kind == PLAYTYPE_SECTOR ? 150u : 250u));
        if (kind == PLAYTYPE_DIR) assert(!cdb.rdir_state && !cdb.cdir_pfnum);
        finish_update(worker);
        assert(!read_calls && !seek_delays && !cdb.pause_request);
    }
}

static void timer_failure(void)
{
    for (int delay = 1; delay <= 2; delay++) {
        prepare(150, 0, STAT_PAUSE);
        delay_failure = delay;
        run_worker(NULL);
        assert(delay_calls == delay && !read_calls);
        assert(cdb.status == STAT_ERROR && !cdb.play_type && !cdb.play_track);
        assert(!(hirq & (HIRQ_CSCT | HIRQ_PEND)) && !cdb.play_wait);

        prepare(250, 0, STAT_PLAY);
        cdb.play_fad_end = cdb.fad + 1;
        pend_delay = 2000;
        finish_after = 0;
        delay_failure = delay;
        run_worker(NULL);
        assert(delay_calls == delay && read_calls == 1 && filtered == 1);
        assert(cdb.status == STAT_ERROR && !cdb.play_type && !(hirq & HIRQ_PEND));
    }
    for (int audio = 0; audio <= 1; audio++) {
        prepare(250, audio, STAT_PLAY);
        finish_after = 0;
        sector_delay = 2000;
        delay_failure = 1;
        run_worker(NULL);
        assert(cdb.status == STAT_ERROR && !cdb.play_type && !cdb.play_track);
        assert(cdb.fad == 250 && read_calls == 1 && !filtered && queued == audio);
        assert(!(hirq & (HIRQ_CSCT | HIRQ_PEND)) && !cdb.play_wait);
    }

    prepare(150, 0, STAT_PAUSE);
    cdb.play_type = PLAYTYPE_DIR;
    cdb.rdir_state = 2;
    cdb.cdir_pfnum = 7;
    delay_failure = 1;
    run_worker(NULL);
    assert(cdb.status == STAT_ERROR && !cdb.play_type && !cdb.play_track);
    assert(!cdb.rdir_state && !cdb.cdir_pfnum && hirq == HIRQ_EFLS);
}

/* Adapted from the terminal-ownership cases in paps-g c3bfa9ef. */
static void failed_input_terminal(void)
{
    for (int kind = PLAYTYPE_SECTOR; kind <= PLAYTYPE_DIR; ++kind) {
        for (int startup = 0; startup <= 1; ++startup) {
            prepare(250, 0, STAT_PLAY);
            cdb.play_type = kind;
            cdb.rdir_state = 2;
            cdb.cdir_pfnum = 7;
            cdb.max_repeat = 3;
            cdb.repcnt = 1;
            cdb.pause_request = 0;
            int free_before = cdb.block_free;
            PARTITION partition = cdb.part[0];
            lookup_error = startup;
            read_error = !startup;
            run_worker(NULL);
            assert(error_reports == 1 && reported_reader == kind);
            assert(cdb.status == STAT_ERROR && !cdb.play_type && !cdb.play_track);
            assert(hirq == (kind == PLAYTYPE_SECTOR ? 0 : HIRQ_EFLS));
            assert(cdb.fad == 250 && cdb.repcnt == 1 && read_calls == (unsigned)!startup);
            assert(!filtered && !queued && cdb.block_free == free_before);
            assert(!memcmp(&cdb.part[0], &partition, sizeof partition));
            assert(cdb.rdir_state == (kind == PLAYTYPE_DIR ? 0 : 2));
            assert(cdb.cdir_pfnum == (kind == PLAYTYPE_DIR ? 0 : 7));
        }
    }
}

/* Idle and final-release cases adapted from paps-g 218dcb45. */
static void idle_wake(void)
{
    const int statuses[] = {STAT_STANDBY, STAT_PAUSE, STAT_ERROR};
    for (unsigned i = 0; i < sizeof statuses / sizeof *statuses; ++i) {
        prepare(250, 0, statuses[i]);
        cdb.play_type = 0;
        cdb.play_track = NULL;
        cdb.fad = i ? 251 : 0;
        cdb.index = 3;
        cdb.track = 2;
        run_worker(NULL);
        assert(cdb.status == statuses[i] && cdb.fad == (i ? 251 : 0));
        assert(cdb.index == 3 && cdb.track == 2 && !cdb.play_track);
        assert(!lookup_calls && !read_calls && !hirq && !error_reports);
    }
}

static void eof_release(void)
{
    prepare(250, 0, STAT_PLAY);
    cdb.play_fad_end = 251;
    cdb.index = 4;
    finish_after = 0;
    replace_on_eof = 1;
    run_worker(NULL);
    assert(eof_publications == 1 && cdb.index == 2);
    assert(hirq == HIRQ_PEND && read_calls == 1 && filtered == 1);
}

static void acknowledge_delayed_request(void)
{
    ack_fad = cdb.fad;
    ack_type = cdb.play_type;
    hirq_at_ack = hirq;
    filtered_at_ack = filtered;
    queued_at_ack = queued;
    filter_result = 0;
    if (resume_end) cdb.play_fad_end = resume_end;
}

static void delay_handoff(int stage, int retained)
{
    drive_ack_hook = acknowledge_delayed_request;
    delay_request_at = stage;
    delay_request = retained ? -PLAYTYPE_SECTOR : 1;
}

static void seek_handoff(void)
{
    for (int stage = 1; stage <= 2; stage++)
        for (int retained = 0; retained <= 1; retained++) {
            prepare(150, 0, STAT_PAUSE);
            delay_handoff(stage, retained);
            run_worker(NULL);
            assert(delay_calls == stage && pause_signals == 1 && ack_fad == 150);
            assert(!cdb.pause_request && !hirq);
            assert(ack_type == (retained ? PLAYTYPE_SECTOR : 0));
            assert(read_calls == (unsigned)retained);
            if (retained) assert(last_read_fad == 150 && last_read_status == STAT_PLAY);
        }
}

static void eof_handoff(void)
{
    for (int stage = 1; stage <= 2; stage++)
        for (int mode = 0; mode < 3; mode++) {
            int retained = mode == 2;
            prepare(250, 0, STAT_PLAY);
            if (mode == 1) cdb.play_type = PLAYTYPE_FILE;
            cdb.play_fad_end = 251;
            cdb.index = 4;
            pend_delay = 2000;
            finish_after = retained ? 2 : 0;
            if (retained) resume_end = 252;
            delay_handoff(stage, retained);
            run_worker(NULL);
            assert(delay_calls == stage && pause_signals == 1 && ack_fad == 251);
            assert(!cdb.pause_request && !(hirq_at_ack & (HIRQ_EFLS | HIRQ_PEND)));
            assert(!(hirq & (HIRQ_EFLS | HIRQ_PEND)) && cdb.index == 4);
            assert(ack_type == (retained ? PLAYTYPE_SECTOR : 0));
            assert(read_calls == 1u + retained && filtered == 1 && accepted == 1);
            if (retained) assert(last_read_fad == 251 && last_read_status == STAT_PLAY);
        }
}

static void sector_handoff(void)
{
    for (int path = 0; path < 4; path++)
        for (int retained = 0; retained <= 1; retained++) {
            int audio = path == 1 || path == 2;
            int full = path == 3;
            prepare(250, audio, STAT_PLAY);
            audio_depth = path == 1 ? 4 : 5;
            filter_result = full ? 2 : 0;
            sector_delay = 2000;
            finish_after = retained ? (full ? 3 : 2) : 0;
            delay_handoff(1, retained);
            run_worker(NULL);
            assert(!cdb.pause_request && pause_signals == 1);
            assert(ack_type == (retained ? PLAYTYPE_SECTOR : 0));
            assert(ack_fad == (full ? 250u : 251u));
            assert(filtered_at_ack == !audio && queued_at_ack == audio);
            assert(cdb.fad == (full && !retained ? 250u : 251u));
            assert(read_calls == 1u + retained * (full ? 2 : 1));
            assert(accepted == (!audio && (!full || retained)) && queued == audio);
        }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *test = argv[1];
    if (!strcmp(test, "range")) range_update();
    else if (!strcmp(test, "shorter")) shorter_range();
    else if (!strcmp(test, "pickup")) {
        prepare(250, 0, STAT_PAUSE);
        cdb.play_type = 0;
        request(0x101, 0x100, 0x80);
        assert(!play_cd() && cdb.fad == 250);
    } else if (!strcmp(test, "paused")) paused_update();
    else if (!strcmp(test, "resume")) {
        for (unsigned audio = 0; audio < 2; audio++) {
            prepare(250, audio, STAT_PAUSE);
            run_worker(NULL);
            assert(last_read_status == STAT_PLAY && !seek_delays && read_calls == 1);
        }
    } else if (!strcmp(test, "timeout")) timed_out_update();
    else if (!strcmp(test, "error")) failed_reader();
    else if (!strcmp(test, "replace")) replacement();
    else if (!strcmp(test, "timer")) timer_failure();
    else if (!strcmp(test, "input-terminal")) failed_input_terminal();
    else if (!strcmp(test, "idle-wake")) idle_wake();
    else if (!strcmp(test, "eof-release")) eof_release();
    else if (!strcmp(test, "seek-handoff")) seek_handoff();
    else if (!strcmp(test, "eof-handoff")) eof_handoff();
    else if (!strcmp(test, "sector-handoff")) sector_handoff();
    else if (!strcmp(test, "controls")) {
        prepare(150, 0, STAT_PAUSE);
        run_worker(NULL);
        assert(last_read_status == STAT_SEEK && seek_delays == 1 && read_calls == 1);
        prepare(249, 0, STAT_PLAY);
        read_error = 1;
        run_worker(NULL);
        assert(cdb.status == STAT_ERROR && !cdb.play_type && !(hirq & HIRQ_PEND));
    } else abort();
    free(media);
    puts("pass: actual worker and PLAY command");
}
