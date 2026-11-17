/* Adapted by papsg from paps-g e8dc1006 (AUD-MCU-036).
 * Real Cortex-M7 RTX variable-block allocator, independent of board devices. */
static uint64_t range_pool[256];
static uint8_t range_snapshot[sizeof(range_pool)];
static unsigned range_cases, range_refusals;

void EvrRtxMemoryAlloc(void *mem, uint32_t size, uint32_t type, void *block) {
    (void)size; (void)type;
    if (mem == range_pool && block == NULL) range_refusals++;
}

static uint32_t range_used(void) { return ((const uint32_t *)range_pool)[1]; }
static void range_reset(void) {
    memset(range_pool, 0xA5, sizeof(range_pool));
    CHECK(osRtxMemoryInit(range_pool, sizeof(range_pool)) == 1);
    CHECK(range_used() == 16);
}

static void range_contract(void) {
    range_reset();
    uint32_t before=range_used();
    void *wrapped=osRtxMemoryAlloc(range_pool, UINT32_MAX-7U, 0);
    output("RANGE wrapped pointer=");number((uint32_t)wrapped);
    output(" used=");number(before);output(",");number(range_used());output("\n");
    CHECK(wrapped == NULL);
    CHECK(range_used() == before);

    for (unsigned fragmented=0;fragmented<2;fragmented++) {
        range_reset();
        void *first=NULL,*hole=NULL,*last=NULL;
        if (fragmented) {
            first=osRtxMemoryAlloc(range_pool,64,0);
            hole=osRtxMemoryAlloc(range_pool,96,0);
            last=osRtxMemoryAlloc(range_pool,64,0);
            CHECK(first && hole && last);
            memset(first,0x51,64);memset(last,0xA2,64);
            CHECK(osRtxMemoryFree(range_pool,hole)==1);
        }
        memcpy(range_snapshot,range_pool,sizeof(range_pool));
        for (uint32_t delta=0;delta<32;delta++) for (uint32_t type=0;type<4;type++) {
            unsigned refused=range_refusals;
            CHECK(osRtxMemoryAlloc(range_pool,UINT32_MAX-delta,type)==NULL);
            CHECK(range_refusals==refused+1);
            CHECK(memcmp(range_snapshot,range_pool,sizeof(range_pool))==0);
            range_cases++;
        }
        if (fragmented) {
            CHECK(osRtxMemoryAlloc(range_pool,96,0)==hole);
            CHECK(osRtxMemoryFree(range_pool,first)==1);
            CHECK(osRtxMemoryFree(range_pool,hole)==1);
            CHECK(osRtxMemoryFree(range_pool,last)==1);
        }
        CHECK(range_used()==16);
    }
    for (uint32_t size=1;size<=33;size++) for (uint32_t type=0;type<4;type++) {
        range_reset();before=range_used();
        void *block=osRtxMemoryAlloc(range_pool,size,type);
        CHECK(block && (((uint32_t)block & 7U)==0));
        CHECK(range_used()-before==((size+15U)&~7U));
        memset(block,0x59,size);
        CHECK(osRtxMemoryFree(range_pool,block)==1);
        CHECK(range_used()==16);
        range_cases++;
    }
    range_reset();
    memcpy(range_snapshot,range_pool,sizeof(range_pool));
    for (uint32_t extra=1;extra<=16;extra++) {
        CHECK(osRtxMemoryAlloc(range_pool,sizeof(range_pool)-24U+extra,0)==NULL);
        CHECK(memcmp(range_snapshot,range_pool,sizeof(range_pool))==0);
        range_cases++;
    }
    void *full=osRtxMemoryAlloc(range_pool,sizeof(range_pool)-24U,0);
    CHECK(full && range_used()==sizeof(range_pool));
    memset(full,0xAB,sizeof(range_pool)-24U);
    CHECK(osRtxMemoryAlloc(range_pool,1,0)==NULL);
    CHECK(osRtxMemoryFree(range_pool,full)==1);
    CHECK(range_used()==16);
    range_cases++;
    output("PASS allocation cases=");number(range_cases);
    output(" refusals=");number(range_refusals);output("\n");
}

static void range_worker(void *argument) { (void)argument; }
static void range_thread_contract(void) {
    uint32_t *common=(uint32_t *)osRtxInfo.mem.common;
    uint32_t used_before=common[1];
    osThreadAttr_t attr={.name="range-worker",.stack_size=128,.priority=osPriorityLow};
    for (uint32_t delta=7;delta<32;delta+=8) {
        attr.stack_size=UINT32_MAX-delta;
        CHECK(osThreadNew(range_worker,NULL,&attr)==NULL);
        CHECK(common[1]==used_before);
    }
    attr.stack_size=128;
    osThreadId_t thread=osThreadNew(range_worker,NULL,&attr);
    CHECK(thread!=NULL);
    CHECK(osThreadTerminate(thread)==osOK);
    CHECK(common[1]==used_before);
    output("PASS oversized-stack cleanup=0x00000004\n");
}
