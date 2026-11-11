// Adapted from paps-g e019e57c (MCU-AUDIO-001), actual-source register model.
// No physical audio timing or receiver model.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
constexpr uint32_t SPI_IFCR_SUSPC=1u<<11, SPI_IFCR_TIFREC=1u<<8;
constexpr uint32_t SPI_IFCR_OVRC=1u<<6, SPI_IFCR_UDRC=1u<<5;
constexpr uint32_t status_mask=SPI_IFCR_SUSPC|SPI_IFCR_TIFREC|SPI_IFCR_OVRC|SPI_IFCR_UDRC;
constexpr int DMA1_Stream1_IRQn=12;
constexpr unsigned status_bits[]={5,6,8,11};
struct Write { int id; uintptr_t value; };
static std::vector<Write> writes;
static unsigned masked, notifications;
struct Register {
    uintptr_t value=0;
    int id=0;
    operator uintptr_t() const { return value; }
    Register &operator=(uintptr_t next);
};
struct Spi { Register CR1, CR2, CFG1, CFG2, IER, IFCR, SR, I2SCFGR, TXDR; } spi;
struct Stream { Register CR, NDTR, PAR, M0AR, M1AR; } stream;
struct Dma { Register LISR, LIFCR; } dma;
struct Mux { Register CCR; } mux;
#define SPI2 (&spi)
#define DMA1_Stream1 (&stream)
#define DMA1 (&dma)
#define DMAMUX1_Channel1 (&mux)
static unsigned unsafe_starts, acknowledgments;
Register &Register::operator=(uintptr_t next) {
    writes.push_back({id,next}); value=next;
    if (id==6) { // SPI_IFCR is write-one-to-clear; SPE is not a substitute.
        assert(!(next&~status_mask));
        assert(!(spi.CR1.value&1));
        spi.SR.value &= ~next;
        acknowledgments++;
    }
    if (id==1 && (next&0x200) && (spi.SR.value&status_mask)) unsafe_starts++;
    if (id==21) dma.LISR.value &= ~next;
    return *this;
}
static void __DMB() {}
static void NVIC_EnableIRQ(int n) { assert(n==12); }
static int disable_irq() { return masked++; }
static void restore_irq(int key) { assert(masked==unsigned(key)+1); masked=key; }
static void (*copy_hook)()=nullptr;
static void memcpy32(void *to, const void *from, unsigned n) {
    std::memcpy(to,from,n/2);
    if(copy_hook) copy_hook();
    std::memcpy(static_cast<u8*>(to)+n/2,static_cast<const u8*>(from)+n/2,n-n/2);
}
#include "audio.inc"
alignas(4) static u8 ring[32][2352], input[2352];
static void notify() { notifications++; }
static void reset(uint32_t flags) {
    spi={}; stream={}; dma={}; mux={};
    int id=1;
    for(Register *r:{&spi.CR1,&spi.CR2,&spi.CFG1,&spi.CFG2,&spi.IER,&spi.IFCR,&spi.SR,&spi.I2SCFGR,&spi.TXDR})r->id=id++;
    stream.CR.id=10; stream.NDTR.id=11; stream.PAR.id=12;
    stream.M0AR.id=13;stream.M1AR.id=14;dma.LIFCR.id=21;mux.CCR.id=30;
    spi.SR.value=flags; writes.clear();unsafe_starts=acknowledgments=masked=notifications=0;
    awp=arp=afull=aplay=0;spi2_dma_notify=notify;copy_hook=nullptr;
    for(unsigned i=0;i<32;i++) audio_buf[i]=ring[i];
    std::memset(ring,0xcc,sizeof ring);
    std::memset(input,0x55,sizeof input);
}
static void init_flags() {
    for(unsigned n=0;n<16;n++) {
        unsigned combination=(n+1)&15;
        uint32_t f=0;
        for(unsigned b=0;b<4;b++)if(combination&(1u<<b))f|=1u<<status_bits[b];
        reset(f);spi2_init();
        assert(!unsafe_starts && !(spi.SR.value&status_mask));
        assert(acknowledgments==1);
        assert(spi.I2SCFGR.value==0x02040015 && spi.IER.value==0);
        assert(awp==0 && arp==0 && afull==0);
        for(unsigned i=0;i<32;i++)assert(reinterpret_cast<uintptr_t>(audio_buf[i])==0x30002000u+i*2352u);
    }
}
static void restart_flags() {
    for(unsigned n=0;n<16;n++) {
        unsigned combination=(n+1)&15;
        uint32_t f=0;
        for(unsigned b=0;b<4;b++)if(combination&(1u<<b))f|=1u<<status_bits[b];
        reset(f);spi2_dma_start(ring[3],ring[4],2352);
        assert(!unsafe_starts && !(spi.SR.value&status_mask));
        assert(acknowledgments==1);
        assert(stream.M0AR.value==reinterpret_cast<uintptr_t>(ring[3]));
        assert(stream.M1AR.value==reinterpret_cast<uintptr_t>(ring[4]));
        assert(stream.NDTR.value==1176 && stream.CR.value==0x42d51);
        assert(spi.CFG1.value==0x8000 && spi.CR1.value==0x201);
    }
}
static void full_queue() {
    reset(status_mask);
    for(unsigned i=0;i<32;i++) {
        std::memset(input,i+1,sizeof input);
        assert(fill_audio_buffer(input)==int(i+1));
        assert(!std::memcmp(ring[i],input,sizeof input));
    }
    assert(!unsafe_starts && acknowledgments==1 && afull && awp==arp);
    auto count=writes.size();
    assert(fill_audio_buffer(input)==-1 && writes.size()==count);
    assert(masked==0);
}
static void repeated_start() {
    reset(0);
    for(unsigned i=0;i<50;i++) {
        spi.SR.value=status_mask;
        spi2_dma_start(ring[i%32],ring[(i+1)%32],2352);
        assert(!unsafe_starts && spi.SR.value==0);
    }
    assert(acknowledgments==50);
}
// Independent DBM event probe: hardware swaps its target before invoking the ISR.
static void complete_sector() {
    assert(stream.CR.value&1);
    stream.CR.value ^= 1u<<19;
    stream.NDTR.value=1176;
    dma.LISR.value|=0x800;
}
static uintptr_t active_address() {
    return (stream.CR.value&(1u<<19)) ? stream.M1AR.value : stream.M0AR.value;
}
static void unpublished_probe() {
    reset(0);assert(fill_audio_buffer(input)==1);assert(fill_audio_buffer(input)==2);
    complete_sector();DMA1_Stream1_IRQHandler();
    assert(arp==1 && awp==2);
    complete_sector(); // First read of automatically selected bank can precede ISR.
    const auto *data=reinterpret_cast<const u8*>(active_address());
    std::fprintf(stderr,"DBM selected slot %zu while awp=%d arp=%d; first byte=0x%02x, published byte=0x55\n",(reinterpret_cast<uintptr_t>(data)-reinterpret_cast<uintptr_t>(ring))/2352,awp,arp,data[0]);
    assert(data!=audio_buf[awp]); // This investigation probe still fails: not a fixed claim.
}
static void valid_controls() {
    reset(0);
    for(unsigned i=0;i<32;i++) {
        std::memset(input,i+1,sizeof input);
        assert(fill_audio_buffer(input)==int(i+1));
        assert(!std::memcmp(ring[i],input,sizeof input));
    }
    assert(!unsafe_starts && afull && awp==arp && !masked);
    auto count=writes.size();assert(fill_audio_buffer(input)==-1 && writes.size()==count);
    assert(stream.M0AR.value==reinterpret_cast<uintptr_t>(ring[0]));
    assert(stream.M1AR.value==reinterpret_cast<uintptr_t>(ring[1]));
    assert(stream.NDTR.value==1176 && stream.CR.value==0x42d51);
    reset(0);spi2_init();
    assert(!unsafe_starts && spi.I2SCFGR.value==0x02040015 && spi.CR1.value==0x201);
    assert(mux.CCR.value==40 && !masked);
}
int main(int argc,char **argv) {
    assert(argc==2);
    switch(std::atoi(argv[1])) {
        case 0:init_flags();break;
        case 1:restart_flags();break;
        case 2:full_queue();break;
        case 3:repeated_start();break;
        case 4:unpublished_probe();break;
        case 5:valid_controls();break;
        default:assert(false);
    }
    std::puts("pass");
}
