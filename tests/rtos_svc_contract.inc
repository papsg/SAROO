/* Adapted by papsg from paps-g dfbad441 (AUD-MCU-034).
 * Exercise real SVC exception return and compiler-visible memory/register effects. */
static uint32_t svc_shared;
static unsigned svc_probe_runs;

static uint32_t svcRtxProbeZero(void) { return ++svc_shared; }
static void svcRtxProbeZeroVoid(void) { svc_shared = 22; }
static uint32_t svcRtxProbeOne(uint32_t *p) { return ++*p; }
__attribute__((naked)) static void svcRtxProbeOneVoid(uint32_t value __attribute__((unused))) {
    __asm__("adds r0,r0,#1\n bx lr");
}
__attribute__((naked)) static uint32_t svcRtxProbeTwo(uint32_t a __attribute__((unused)), uint32_t b __attribute__((unused))) {
    __asm__("adds r0,r0,r1\n movw r1,#0x5a5a\n bx lr");
}
__attribute__((naked)) static uint32_t svcRtxProbeThree(uint32_t a __attribute__((unused)), uint32_t b __attribute__((unused)), uint32_t c __attribute__((unused))) {
    __asm__("adds r0,r0,r1\n adds r0,r0,r2\n movw r1,#0x5a5a\n bx lr");
}
__attribute__((naked)) static uint32_t svcRtxProbeFour(uint32_t a __attribute__((unused)), uint32_t b __attribute__((unused)), uint32_t c __attribute__((unused)), uint32_t d __attribute__((unused))) {
    __asm__("adds r0,r0,r1\n adds r0,r0,r2\n adds r0,r0,r3\n movw r1,#0x5a5a\n bx lr");
}

SVC0_0(ProbeZero,uint32_t)
SVC0_0N(ProbeZeroVoid,void)
SVC0_1(ProbeOne,uint32_t,uint32_t *)
SVC0_1N(ProbeOneVoid,void,uint32_t)
SVC0_2(ProbeTwo,uint32_t,uint32_t,uint32_t)
SVC0_3(ProbeThree,uint32_t,uint32_t,uint32_t,uint32_t)
SVC0_4(ProbeFour,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t)

#define SVC_PROBE __attribute__((noinline,noclone))
SVC_PROBE static uint32_t svc_probe_zero(void) { svc_shared=1; (void)__svcProbeZero(); return svc_shared; }
SVC_PROBE static uint32_t svc_probe_zero_void(void) { svc_shared=11; __svcProbeZeroVoid(); return svc_shared; }
SVC_PROBE static uint32_t svc_probe_shared(void) { svc_shared=1; (void)__svcProbeOne(&svc_shared); return svc_shared; }
SVC_PROBE static uint32_t svc_probe_local(void) { uint32_t local=7; (void)__svcProbeOne(&local); return local; }
SVC_PROBE static uint32_t svc_probe_one_void(uint32_t value) { __svcProbeOneVoid(value); return value; }
SVC_PROBE static uint32_t svc_probe_two(uint32_t value) { return __svcProbeTwo(1,value)+value; }
SVC_PROBE static uint32_t svc_probe_three(uint32_t value) { return __svcProbeThree(1,value,3)+value; }
SVC_PROBE static uint32_t svc_probe_four(uint32_t value) { return __svcProbeFour(1,value,3,4)+value; }

static unsigned svc_probe_mask(void) {
    unsigned failed=0;
    failed |= (svc_probe_zero()!=2)<<0;
    failed |= (svc_probe_zero_void()!=22)<<1;
    failed |= (svc_probe_shared()!=2)<<2;
    failed |= (svc_probe_local()!=8)<<3;
    failed |= (svc_probe_one_void(0x1234)!=0x1234)<<4;
    failed |= (svc_probe_two(0x1234)!=(1+2*0x1234))<<5;
    failed |= (svc_probe_three(0x1234)!=(4+2*0x1234))<<6;
    failed |= (svc_probe_four(0x1234)!=(8+2*0x1234))<<7;
    svc_probe_runs+=8;
    return failed;
}
