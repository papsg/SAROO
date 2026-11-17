/* Adapted from paps-g's AUD-RES-034 native dispatcher matrix.
 * Exercise the selected RTX callback router before scheduling starts. IRQs are
 * masked for the test; this checks routing and ring drainage, not concurrency. */
static os_object_t dispatch_objects[16];
static unsigned dispatch_seen[16], dispatch_calls;

static void dispatch_record(void *object, unsigned expected_id) {
    unsigned slot=16;
    for (unsigned i=0; i<16; i++) {
        if (object==&dispatch_objects[i]) slot=i;
    }
    CHECK(slot<16);
    unsigned id=dispatch_objects[slot].id;
    CHECK(id==expected_id ||
          (expected_id==osRtxIdMessage && id==osRtxIdMessageQueue));
    CHECK(dispatch_seen[slot]++==0);
    dispatch_calls++;
}
static void dispatch_thread(os_thread_t *p) { dispatch_record(p,osRtxIdThread); }
static void dispatch_event(os_event_flags_t *p) { dispatch_record(p,osRtxIdEventFlags); }
static void dispatch_semaphore(os_semaphore_t *p) { dispatch_record(p,osRtxIdSemaphore); }
static void dispatch_pool(os_memory_pool_t *p) { dispatch_record(p,osRtxIdMemoryPool); }
static void dispatch_message(os_message_t *p) { dispatch_record(p,osRtxIdMessage); }

static void dispatch_contract(void) {
    __typeof__(osRtxInfo.post_process) saved=osRtxInfo.post_process;
    CHECK(osRtxInfo.isr_queue.max==16);
    __disable_irq();
    osRtxInfo.post_process.thread=dispatch_thread;
    osRtxInfo.post_process.event_flags=dispatch_event;
    osRtxInfo.post_process.semaphore=dispatch_semaphore;
    osRtxInfo.post_process.memory_pool=dispatch_pool;
    osRtxInfo.post_process.message=dispatch_message;
    for (unsigned offset=0; offset<16; offset++) {
        osRtxInfo.isr_queue.in=osRtxInfo.isr_queue.out=offset;
        for (unsigned base=0; base<256; base+=16) {
            memset(dispatch_seen,0,sizeof(dispatch_seen));
            for (unsigned i=0; i<16; i++) {
                dispatch_objects[i].id=base+i;
                osRtxPostProcess(&dispatch_objects[i]);
            }
            CHECK(osRtxInfo.isr_queue.cnt==16);
            osRtxPendSV_Handler();
            ClrPendSV();
            CHECK(osRtxInfo.isr_queue.cnt==0);
            CHECK(osRtxInfo.isr_queue.in==offset && osRtxInfo.isr_queue.out==offset);
            for (unsigned i=0; i<16; i++) {
                unsigned id=base+i;
                unsigned valid=id==osRtxIdThread || id==osRtxIdEventFlags ||
                    id==osRtxIdSemaphore || id==osRtxIdMemoryPool ||
                    id==osRtxIdMessage || id==osRtxIdMessageQueue;
                CHECK(dispatch_seen[i]==valid);
            }
        }
    }
    CHECK(dispatch_calls==96);
    osRtxInfo.post_process=saved;
    __enable_irq();
    output("DISPATCH objects="); number(4096);
    output(" callbacks="); number(dispatch_calls); output("\n");
}
