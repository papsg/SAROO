/* Adapted from paps-g 24a4b069. The actual menu module runs with the host
 * storage seam; these images use host byte order, not Saturn bus timing. */
#define main existing_save_main
#define initialize initialize_legacy
#include "firmware_save_resize.c"
#undef initialize
#undef main

static void initialize(void)
{
    initialize_legacy();
}

static void power_cycle(void) { reload_model(); }

static void stat_is(unsigned size, unsigned free, unsigned count)
{
    BUPSTAT st;
    assert(!sro_bup_stat(0, size, &st));
    assert(st.total_size == 32768 && st.total_block == 512 && st.block_size == 64);
    assert(st.free_block == free && st.data_num == count);
    assert(st.free_size == (free ? free * 58 - 30 : 0));
}
static void native_boundaries(void)
{
    static const unsigned sizes[] = {0,1,27,28,29,85,86,87,992,1024,1276,29550};
    static const unsigned costs[] = {1,1,1,1,2,2,2,3,18,19,23,510};
    for (unsigned n = 0; n < sizeof sizes / sizeof *sizes; n++) {
        initialize(); stat_is(sizes[n],510,510/costs[n]);
        BUPDIR dir = directory("NATIVE00001",sizes[n]), out;
        u8 *data = payload(sizes[n],19);
        assert(!sro_bup_write(0,&dir,data,0));
        assert(sro_bup_dir(0,dir.file_name,1,&out)==1 && out.block_size==costs[n]);
        stat_is(sizes[n],510-costs[n],(510-costs[n])/costs[n]);
        power_cycle(); check_save(0,&dir,data);
        assert(!sro_bup_delete(0,dir.file_name)); power_cycle(); stat_is(sizes[n],510,510/costs[n]);
        free(data);
    }
    initialize(); BUPDIR huge = directory("TOOBIG00001",29551); u8 byte=7;
    assert(sro_bup_write(0,&huge,&byte,0)==BUP_NOT_ENOUGH_MEMORY);
    stat_is(29551,510,0);
}
static void native_resizes(void)
{
    static const unsigned sizes[][2] = {
        {0, 28}, {28, 29}, {29, 28}, {86, 87}, {87, 86},
        {992, 1276}, {1276, 992}, {32, 29492}, {29492, 32}
    };
    for (unsigned n = 0; n < sizeof sizes / sizeof *sizes; n++) {
        initialize();
        BUPDIR dir = directory("RESIZE00001", sizes[n][0]);
        BUPDIR keep = directory("KEEP0000001", 1);
        u8 *old = payload(sizes[n][0], 13);
        u8 *next = payload(sizes[n][1], 27);
        u8 *neighbor = payload(1, 39);
        assert(!sro_bup_write(0, &dir, old, 0));
        assert(!sro_bup_write(0, &keep, neighbor, 0));
        power_cycle();
        dir.data_size = sizes[n][1];
        dir.date++;
        memcpy(dir.comment, "Resized!  ", 10);
        assert(!sro_bup_write(0, &dir, next, 0));
        power_cycle();
        check_save(0, &dir, next);
        check_save(0, &keep, neighbor);
        assert(BUPMEM->free_block ==
               510 - 1 - 1 - save_data_blocks(0, dir.data_size));
        BUPDIR refusal = dir;
        refusal.data_size = 29551;
        assert(sro_bup_write(0, &refusal, next, 0) == BUP_NOT_ENOUGH_MEMORY);
        assert(sro_bup_write(0, &dir, next, 1) == BUP_FOUND);
        power_cycle();
        check_save(0, &dir, next);
        check_save(0, &keep, neighbor);
        free(old); free(next); free(neighbor);
    }
}
static void tiny_files(void)
{
    initialize();
    for (unsigned i=0;i<510;i++) {
        char name[12]; snprintf(name,sizeof name,"TINY%07u",i);
        BUPDIR dir=directory(name,1);u8 byte=i;
        assert(!sro_bup_write(0,&dir,&byte,0));
    }
    power_cycle();stat_is(1,0,0);
    assert(sro_bup_dir(0,"",0,NULL)==-510);
    BUPDIR extra=directory("EXTRA000001",1);u8 byte=0;
    assert(sro_bup_write(0,&extra,&byte,0)==BUP_NOT_ENOUGH_MEMORY);
    for(unsigned i=0;i<510;i++) {
        char name[12];snprintf(name,sizeof name,"TINY%07u",i);
        BUPDIR dir=directory(name,1);u8 expected=i;
        check_save(0,&dir,&expected);
        if(i%2==0) assert(!sro_bup_delete(0,name));
    }
    /* Fragmented full storage reuses released headers as payload blocks. */
    stat_is(0,255,255);BUPDIR big=directory("FRAGMENT001",255*58-30);
    u8 *data=payload(big.data_size,73);assert(!sro_bup_write(0,&big,data,0));
    power_cycle();check_save(0,&big,data);stat_is(0,0,0);free(data);
}
static void legacy_refusal(void)
{
    initialize();
    BUPMEM->total_size = 65536; BUPMEM->block_size = 128;
    BUPMEM->bitmap[0] = 1; BUPMEM->free_block = 511;
    u8 before[65536]; memcpy(before, test_shared, sizeof before);
    BUPSTAT st;
    assert(sro_bup_stat(0, 992, &st) == BUP_BROKEN);
    assert(sro_bup_read(0, "ANY", before) == BUP_BROKEN);
    assert(!flushes && !memcmp(before, test_shared, sizeof before));
}
static void saga_slots(void)
{
    initialize();u8 *data=payload(1276,93);
    for(unsigned i=0;i<3;i++) {
        char name[12];snprintf(name,sizeof name,"SAGA%07u",i);BUPDIR dir=directory(name,1276);
        assert(!sro_bup_write(0,&dir,data,0));
    }
    power_cycle();stat_is(1276,441,19);free(data);
}
static void cartridge_contract(void)
{
    static const unsigned sizes[]={0,960,961,1024,1025,479*1024};
    static const unsigned costs[]={1,1,2,2,3,480};
    for(unsigned i=0;i<6;i++) {
        initialize();BUPSTAT st;BUPDIR dir=directory("CART0000001",sizes[i]),out;
        u8 *data=payload(sizes[i],31);
        assert(!sro_bup_write(1,&dir,data,0));power_cycle();check_save(1,&dir,data);
        assert(sro_bup_dir(1,dir.file_name,1,&out)==1 && out.block_size==costs[i]);
        assert(!sro_bup_stat(1,sizes[i],&st));
        assert(st.total_size==8388608 && st.total_block==8192 && st.block_size==1024);
        assert(st.free_block==8056-costs[i]);free(data);
    }
    initialize();memset((void*)MEMS->bitmap,255,1008);set_bitmap((u8*)MEMS->bitmap,8000,0);
    bup_update(MEMS_HDR);power_cycle();BUPSTAT st;
    assert(!sro_bup_stat(1,960,&st) && st.free_block==1 && st.free_size==960 && st.data_num==1);
    BUPDIR dir=directory("LAST0000001",960);u8 *data=payload(960,71);
    assert(!sro_bup_write(1,&dir,data,0));power_cycle();check_save(1,&dir,data);
    assert(!sro_bup_stat(1,0,&st) && !st.free_block && !st.free_size && !st.data_num);free(data);
}
int main(void)
{
    native_boundaries();native_resizes();tiny_files();legacy_refusal();saga_slots();cartridge_contract();
    puts("Native allocation, resize, 510 files, fragmentation, Saga slots and cartridge boundaries pass.");
}
