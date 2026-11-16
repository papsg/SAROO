/* Adapted by papsg from paps-g SSR-067 (093a51b6). Test-only public RTX
 * pool APIs, real SVC/external IRQ/PendSV; no SAROO board services. */
static osMemoryPoolId_t pool;
static void *owned[32], *released_block;
static unsigned capacity, block_size;
static volatile unsigned armed, hooks, masked_hooks, irq_count;
static volatile unsigned bad_count, impossible_count;
static volatile uint32_t observed_count, observed_space;

static void pend_pool_irq(void) {
    NVIC_SetPendingIRQ((IRQn_Type)0);
    __DSB(); __ISB();
}

void pool_publication_probe(os_mp_info_t *info, unsigned allocating) {
    if (armed && info == &osRtxMemoryPoolId(pool)->mp_info &&
        allocating == (TEST_POOL_PHASE == 1)) {
        CHECK(__get_IPSR() == (TEST_POOL_PHASE == 2 ? 0u : 11u));
        armed = 0;
        hooks++;
        masked_hooks += __get_PRIMASK() != 0;
        pend_pool_irq();
    }
}

static void pool_irq(void) {
    CHECK(__get_IPSR() == 16 && !__get_PRIMASK());
    CHECK(osMemoryPoolGetCapacity(pool) == capacity);
    CHECK(osMemoryPoolGetBlockSize(pool) == block_size);
    CHECK(!osMemoryPoolAlloc(pool, 1)); /* ISR timeout must be zero. */
    void *block = NULL;
    if (TEST_POOL_PHASE != 1) {
        block = osMemoryPoolAlloc(pool, 0);
        CHECK(block == released_block);
    } else {
        CHECK(!osMemoryPoolAlloc(pool, 0)); /* The thread owns every block. */
    }
    observed_count = osMemoryPoolGetCount(pool);
    observed_space = osMemoryPoolGetSpace(pool);
    bad_count += observed_count != capacity || observed_space != 0;
    impossible_count += observed_count > capacity || observed_space > capacity;
    if (block) {
        *(uint32_t *)block = 0x12345678;
        CHECK(osMemoryPoolFree(pool, block) == osOK);
    }
    irq_count++;
}

static void main_task(void *argument) {
    (void)argument;
    SysTick->CTRL = 0; /* Publication is driven by the one explicit test IRQ. */
    const unsigned counts[] = {1, 2, 7, 32}, sizes[] = {4, 8, 12, 64};
    NVIC_SetPriority((IRQn_Type)0, 5);
    NVIC_EnableIRQ((IRQn_Type)0);
    unsigned cases = 0;
    for (unsigned c = 0; c < 4; c++) for (unsigned s = 0; s < 4; s++) {
        capacity = counts[c]; block_size = sizes[s];
        pool = osMemoryPoolNew(capacity, block_size, NULL);
        CHECK(pool);
        CHECK(osMemoryPoolGetCount(pool) == 0 && osMemoryPoolGetSpace(pool) == capacity);
        for (unsigned cycle = 0; cycle < 16; cycle++) {
            CHECK(!osRtxInfo.isr_queue.cnt && !__get_PRIMASK());
            unsigned initial = capacity - (TEST_POOL_PHASE == 1);
            for (unsigned i = 0; i < initial; i++) {
                owned[i] = osMemoryPoolAlloc(pool, 0);
                CHECK(owned[i]);
                for (unsigned j = 0; j < i; j++) CHECK(owned[i] != owned[j]);
                *(uint32_t *)owned[i] = 0xa5000000u + i;
            }
            CHECK(osMemoryPoolGetCount(pool) == initial);
            CHECK(osMemoryPoolGetSpace(pool) == capacity - initial);
            unsigned victim = cycle % capacity;
            if (TEST_POOL_PHASE != 1) released_block = owned[victim];
#if TEST_POOL_INSIDE
            armed = 1;
#endif
            if (TEST_POOL_PHASE == 2) __disable_irq();
            if (TEST_POOL_PHASE == 1) {
                owned[capacity - 1] = osMemoryPoolAlloc(pool, 0);
                CHECK(owned[capacity - 1]);
                *(uint32_t *)owned[capacity - 1] = 0xa5000000u + capacity - 1;
            } else {
                CHECK(osMemoryPoolFree(pool, released_block) == osOK);
            }
            if (TEST_POOL_PHASE == 2) {
                CHECK(__get_PRIMASK() == 1 && irq_count == cases);
                CHECK(osMemoryPoolGetCount(pool) == capacity - 1);
                CHECK(osMemoryPoolGetSpace(pool) == 1);
                __enable_irq(); __DSB(); __ISB();
            }
#if !TEST_POOL_INSIDE
            pend_pool_irq();
#endif
            CHECK(irq_count == cases + 1 && !armed && !__get_PRIMASK());
            CHECK(!osRtxInfo.isr_queue.cnt);
            CHECK(osMemoryPoolGetCount(pool) == capacity - (TEST_POOL_PHASE != 1));
            CHECK(osMemoryPoolGetSpace(pool) == (TEST_POOL_PHASE != 1));
            for (unsigned i = 0; i < capacity; i++) if (TEST_POOL_PHASE == 1 || i != victim) {
                CHECK(*(uint32_t *)owned[i] == 0xa5000000u + i);
                CHECK(osMemoryPoolFree(pool, owned[i]) == osOK);
            }
            CHECK(!osMemoryPoolGetCount(pool) && osMemoryPoolGetSpace(pool) == capacity);
            cases++;
        }
        CHECK(osMemoryPoolDelete(pool) == osOK);
    }
    NVIC_DisableIRQ((IRQn_Type)0);
    CHECK(cases == 256 && irq_count == cases);
    CHECK(hooks == (TEST_POOL_INSIDE ? cases : 0));
    CHECK(masked_hooks <= hooks);
    output("POOL cases="); number(cases);
    output(" bad="); number(bad_count);
    output(" impossible="); number(impossible_count);
    output(" count="); number(observed_count);
    output(" space="); number(observed_space);
    output(" hooks="); number(hooks);
    output(" masked="); number(masked_hooks);
    output(" recovered=1\n");
    finish(bad_count ? 42 : 0);
}
