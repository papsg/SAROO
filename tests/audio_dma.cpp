// Adapted from paps-g 104b25fb/e8a47c86/247c1dbd, actual-source host model.
// Independent DMA/FIFO events; no physical timing or receiver qualification.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <deque>
#include <vector>
#include <utility>
using u8=uint8_t; using u16=uint16_t; using u32=uint32_t;
constexpr unsigned SPI_IFCR_SUSPC=0x800, SPI_IFCR_TIFREC=0x100, SPI_IFCR_OVRC=0x40, SPI_IFCR_UDRC=0x20;
constexpr unsigned SPI_SR_TXP=2, SPI_SR_UDR=0x20, SPI_IER_UDRIE=0x20;
constexpr unsigned RCC_APB1LENR_TIM7EN=0x20;
constexpr int DMA1_Stream1_IRQn=12, SPI2_IRQn=36, TIM7_IRQn=55;
struct Register { uintptr_t value=0; int id=0; operator uintptr_t() const; Register& operator=(uintptr_t); Register& operator&=(uintptr_t v){return *this=value&v;} Register& operator|=(uintptr_t v){return *this=value|v;} };
struct Spi {Register CR1,CR2,CFG1,CFG2,IER,IFCR,SR,I2SCFGR,TXDR;} spi;
struct Stream {Register CR,NDTR,PAR,M0AR,M1AR,FCR;} stream;
struct Dma {Register LISR,LIFCR;} dma;
struct Timer {Register CR1,CNT,SR,PSC,ARR,EGR,DIER;} timer;
struct Mux {Register CCR;} mux;
struct Rcc {Register APB1LENR;} rcc;
#define SPI2 (&spi)
#define DMA1_Stream1 (&stream)
#define DMA1 (&dma)
#define TIM7 (&timer)
#define DMAMUX1_Channel1 (&mux)
#define RCC (&rcc)
#define _Static_assert static_assert
static void __DMB();
static int disable_irq();
static void restore_irq(int);
static void NVIC_EnableIRQ(int);
static void NVIC_SetPriority(int,int);
static void memcpy32(void*,void*,int);
[[maybe_unused]] static void write_scan_byte(u8*, unsigned, u8);
static void *mapped_memset(void*,int,size_t);
#define memset mapped_memset
#include "audio.inc"
#undef memset
struct Sample {uint16_t value;int tag;unsigned word;uint64_t at=0;};
constexpr uintptr_t payload_base=0x30002000, quiet_base=0x30014600;
constexpr size_t payload_bytes=33*2352;
static_assert(ABUF_SILENCE==quiet_base);
static std::array<u8,payload_bytes> memory,visible;
static std::array<bool,33> dirty;
static std::array<int,32> tags;
static std::deque<Sample> fifo;
static std::vector<Sample> output;
static Sample preload{},last_pair[2]{};
static bool have_preload,bus_stalled,hold_disable,inside_model,in_isr,timer_pending;
static unsigned mask,copying_slot,channel,reload_count,clear_pending,unsafe_reads,active_writes,discarded,irq_calls,underrun_irqs,timer_irqs,wakeups,flag_clears;
static uintptr_t cursor;
static uint64_t model_time;
static int64_t timer_left;
static bool clock_stalled,underrun_active,permanent_dma_fault;
static unsigned underrun_channel,repeated_words;
static bool suspend_requested;
static unsigned suspend_updates,forced_stops,healthy_stops,worker_flushes;
static int input_tag;
static void (*copy_hook)();
static void (*read_hook)(int);
static std::vector<std::pair<int,unsigned>> accepted;
static u8 input[2352];
static void dma_feed();
static void run_irqs();
static void word_tick();
static size_t offset(uintptr_t p){assert(p>=payload_base && p<payload_base+payload_bytes);return p-payload_base;}
static void preload_word(){
    if(have_preload || !(stream.CR.value&1) || bus_stalled)return;
    if(permanent_dma_fault){stream.CR.value&=~uintptr_t(1);dma.LISR.value|=0x200;return;}
    size_t off=offset(cursor);assert(off+2<=payload_bytes && !(off&1));
    unsigned slot=unsigned(off/2352),word=unsigned(off%2352)/2;
    if(slot<32){
        bool published=((slot-(arp&31))&31)<awp-arp;
        if(!published || copying_slot==slot || dirty[slot]) {
            std::fprintf(stderr,"unsafe DMA read: slot=%u awp=%u arp=%u copying=%u dirty=%d\n",slot,unsigned(awp),unsigned(arp),copying_slot,int(dirty[slot]));
            unsafe_reads++;assert(false);
        }
    }else assert(!dirty[32]);
    uint16_t data;std::memcpy(&data,visible.data()+off,2);
    preload={data,slot<32?tags[slot]:-1,word};have_preload=true;cursor+=2;
}
static void dma_feed(){
    if(inside_model)return;
    inside_model=true;
    if(!bus_stalled && (stream.CR.value&1))preload_word();
    while((stream.CR.value&1) && (spi.CR1.value&1) && (spi.CFG1.value&0x8000) && fifo.size()<8){
        preload_word();
        // RM0433 15.3.20: memory backpressure sets FEIF without stopping DMA
        // or acknowledging the pending peripheral request. A prefetched word
        // can still reach the peripheral while the memory bus is stalled.
        if(!have_preload){if(stream.CR.value&1)dma.LISR.value|=0x40;break;}
        fifo.push_back(preload);have_preload=false;
        assert(stream.NDTR.value);stream.NDTR.value--;
        if(stream.NDTR.value==0){
            dma.LISR.value|=0x800;
            if(stream.CR.value&((1u<<18)|0x100)){
                if(stream.CR.value&(1u<<18))stream.CR.value^=1u<<19;
                stream.NDTR.value=reload_count;
                cursor=stream.CR.value&(1u<<19)?stream.M1AR.value:stream.M0AR.value;
            }else stream.CR.value&=~uintptr_t(1);
        }else if(stream.NDTR.value==reload_count/2)dma.LISR.value|=0x400;
        preload_word(); // Direct mode preloads the next source before the ISR.
    }
    if(fifo.size()<8)spi.SR.value|=2;else spi.SR.value&=~uintptr_t(2);
    inside_model=false;
}
Register::operator uintptr_t() const {
    dma_feed();
    uintptr_t result=id==1 ? value&~uintptr_t(0x400) : value;
    if(read_hook)read_hook(id);
    return result;
}
Register& Register::operator=(uintptr_t v){
    uintptr_t old=value;
    if((id==13 || id==14) && (stream.CR.value&1) && (!(stream.CR.value&(1u<<18)) || (((stream.CR.value>>19)&1)==unsigned(id==14)))){
        active_writes++;dma.LISR.value|=0x200;stream.CR.value&=~uintptr_t(1);have_preload=false;return *this;
    }
    value=v;
    if(id==1){
        if(v&0x400){suspend_requested=true;suspend_updates=0;}
        if(!(v&1)){
            assert(!(stream.CR.value&1) && "stop DMA before disabling its peripheral");
            if((old&0x201)==0x201){
                assert(clock_stalled && suspend_requested && suspend_updates>=2 &&
                       "a healthy stereo frame must finish before SPE clears");
                forced_stops++;
            }
            if((old&1) && !clock_stalled){
                assert(!last_pair[0].value && !last_pair[1].value);
                healthy_stops++;
            }
            for(auto s:fifo)if(s.tag>=0)discarded++;
            fifo.clear();channel=0;underrun_active=false;last_pair[0]={0,-1,0};last_pair[1]={0,-1,1};
            suspend_requested=false;
        }
        if((v&0x200) && (spi.SR.value&0x800))value&=~uintptr_t(0x200);
    }else if(id==5){
        // During active recovery an uncleared UDR must remain masked.
        if((v&0x20) && (spi.SR.value&0x20) && aplay==AUDIO_PLAYING)assert(false);
    }else if(id==6){
        assert(!(v&~uintptr_t(0x960)));flag_clears++;
        if((v&0x20) && (spi.CR1.value&0x200) && (spi.SR.value&0x20))clear_pending=1;
        else spi.SR.value&=~(v&0x20);
        spi.SR.value&=~(v&~uintptr_t(0x20));
    }else if(id==10){
        if((old&1) && !(v&1)){
            if(hold_disable)value|=1;
            else {dma.LISR.value|=0x800;have_preload=false;}
        }
        if(!(old&1) && (value&1)){
            if(value&(1u<<18))value|=0x100; // DBM forces circular reload.
            cursor=value&(1u<<19)?stream.M1AR.value:stream.M0AR.value;
            have_preload=false;
        }
    }else if(id==11){assert(!(stream.CR.value&1));reload_count=unsigned(v);}
    else if(id==15)assert(!(stream.CR.value&1)); // FCR is protected while enabled.
    else if(id==21)dma.LISR.value&=~v;
    else if(id==40){timer_left=(v&1)?int64_t(timer.ARR.value+1)*4410:0;}
    else if(id==42){if(v==0)timer_pending=false;}
    dma_feed();return *this;
}
static void __DMB(){for(unsigned s=0;s<33;s++)if(dirty[s]){std::memcpy(visible.data()+s*2352,memory.data()+s*2352,2352);dirty[s]=false;}}
static int disable_irq(){int old=mask;mask=1;return old;}
static void restore_irq(int old){mask=unsigned(old);}
static void NVIC_EnableIRQ(int n){assert(n==12 || n==36 || n==55);}
static void NVIC_SetPriority(int n,int priority){NVIC_EnableIRQ(n);assert(priority==0);}
static void *mapped_memset(void *p,int v,size_t n){
    auto addr=reinterpret_cast<uintptr_t>(p);size_t off=offset(addr);assert(off+n<=payload_bytes);
    std::memset(memory.data()+off,v,n);for(size_t i=off/2352;i<=(off+n-1)/2352;i++)dirty[i]=true;return p;
}
static void memcpy32(void *dst,void *src,int n){
    size_t off=offset(reinterpret_cast<uintptr_t>(dst));assert(n==2352 && off%2352==0 && off/2352<32);
    copying_slot=unsigned(off/2352);tags[copying_slot]=input_tag;dirty[copying_slot]=true;
    std::memcpy(memory.data()+off,src,n/2);
    if(copy_hook)copy_hook();
    std::memcpy(memory.data()+off+n/2,static_cast<u8*>(src)+n/2,n-n/2);
    copying_slot=99;
}
[[maybe_unused]] static void write_scan_byte(u8 *slot, unsigned i, u8 value)
{
    const auto off = offset(reinterpret_cast<uintptr_t>(slot));
    assert(i < ABUF_BYTES && off % ABUF_BYTES == 0 && off / ABUF_BYTES < ABUF_NUM);
    assert(!mask && audio_copying);
    copying_slot = off / ABUF_BYTES;
    tags[copying_slot] = input_tag;
    dirty[copying_slot] = true;
    memory[off + i] = value;
    if (i == ABUF_BYTES / 2 && copy_hook) copy_hook();
    if (i == ABUF_BYTES - 1) copying_slot = 99;
}
static void notify(){wakeups++;}
static void reset_model(){
    spi={};stream={};dma={};timer={};mux={};rcc={};
    int id=1;for(auto *p:{&spi.CR1,&spi.CR2,&spi.CFG1,&spi.CFG2,&spi.IER,&spi.IFCR,&spi.SR,&spi.I2SCFGR,&spi.TXDR})p->id=id++;
    id=10;for(auto *p:{&stream.CR,&stream.NDTR,&stream.PAR,&stream.M0AR,&stream.M1AR,&stream.FCR})p->id=id++;
    dma.LISR.id=20;dma.LIFCR.id=21;mux.CCR.id=30;
    id=40;for(auto *p:{&timer.CR1,&timer.CNT,&timer.SR,&timer.PSC,&timer.ARR,&timer.EGR,&timer.DIER})p->id=id++;
    rcc.APB1LENR.id=50;memory.fill(0xcc);visible=memory;dirty.fill(false);tags.fill(-99);fifo.clear();output.clear();accepted.clear();
    have_preload=bus_stalled=hold_disable=inside_model=in_isr=timer_pending=false;
    mask=channel=reload_count=clear_pending=unsafe_reads=active_writes=discarded=irq_calls=underrun_irqs=timer_irqs=wakeups=flag_clears=0;
    underrun_active=permanent_dma_fault=false;underrun_channel=repeated_words=0;
    suspend_requested=false;suspend_updates=forced_stops=healthy_stops=worker_flushes=0;
    copying_slot=99;cursor=0;model_time=0;timer_left=0;clock_stalled=false;copy_hook=nullptr;read_hook=nullptr;awp=arp=0;aplay=AUDIO_IDLE;spi2_dma_notify=notify;
}
static void run_irqs(){
    if(mask || in_isr)return;
    for(unsigned n=0;n<8;n++){
        auto f=dma.LISR.value;auto c=stream.CR.value;
        in_isr=true;
        if(((f&0x800)&&(c&0x10)) || ((f&0x200)&&(c&4)) || ((f&0x100)&&(c&2)) || ((f&0x40)&&(stream.FCR.value&0x80))){irq_calls++;DMA1_Stream1_IRQHandler();}
        else if((spi.SR.value&0x20)&&(spi.IER.value&0x20)){irq_calls++;underrun_irqs++;SPI2_IRQHandler();}
        else if(timer_pending){
            irq_calls++;timer_irqs++;
            if(suspend_requested && (timer.SR.value&1))suspend_updates++;
            TIM7_IRQHandler();
        }
        else {in_isr=false;return;}
        in_isr=false;
    }
    assert(false && "interrupt storm");
}
static void word_tick(){
    if(clear_pending){clear_pending=0;spi.SR.value&=~uintptr_t(0x20);}
    dma_feed();
    if(!clock_stalled && (spi.CR1.value&0x201)==0x201){
        // RM0433 50.9.12: cleared UDR plus data cannot resume on the wrong
        // stereo channel. Repeated last-pair words are tracked separately from
        // consumed payload, so exact input order does not imply inaudible output.
        if(underrun_active && !(spi.SR.value&0x20) && !fifo.empty() && channel==underrun_channel)underrun_active=false;
        if(!underrun_active && !(spi.SR.value&0x20) && !fifo.empty()){
            auto s=fifo.front();fifo.pop_front();
            assert(s.tag<0 || (s.word&1)==channel);
            s.at=model_time;output.push_back(s);last_pair[channel]=s;
        }else {
            if(!underrun_active){underrun_active=true;underrun_channel=channel;spi.SR.value|=0x20;}
            assert(last_pair[channel].tag<0 || (last_pair[channel].word&1)==channel);
            repeated_words++;
        }
        channel^=1;
        if(channel==0 && (spi.CR1.value&0x400)) {spi.CR1.value&=~uintptr_t(0x600);spi.SR.value|=0x800;}
    }
    dma_feed();
    // Rational time units: one 44.1 kHz stereo halfword is5000 units;
    // one configured10us timer count is4410 units. IRQ delivery is quantized
    // to word boundaries (at most one halfword late), not native timing proof.
    model_time+=5000;
    if(timer.CR1.value&1){
        timer_left-=5000;
        if(timer_left<=0){timer.CR1.value&=~uintptr_t(1);timer.SR.value|=1;timer_pending=(timer.DIER.value&1)!=0;}
    }
}
static void step(unsigned words,bool interrupts=true){for(unsigned i=0;i<words;i++){word_tick();if(interrupts)run_irqs();}}
static void fire_timer(){
    if(clear_pending){clear_pending=0;spi.SR.value&=~uintptr_t(0x20);}
    dma_feed();
    if(timer.CR1.value&1){timer.CR1.value&=~uintptr_t(1);timer.SR.value|=1;timer_pending=(timer.DIER.value&1)!=0;}
    run_irqs();
}
static void prepare_input(int tag)
{
    input_tag=tag;
    for(unsigned i=0;i<1176;i++){uint16_t v=uint16_t((tag*53+i*7)^0x55aa);std::memcpy(input+i*2,&v,2);}
}

static int publish(int tag){
    prepare_input(tag);
    int count=fill_audio_buffer(input);
    if(count>=0)for(unsigned i=0;i<1176;i++)accepted.push_back({tag,i});
    return count;
}
static void check_output(){
    std::vector<std::pair<int,unsigned>> actual;
    for(auto s:output)if(s.tag>=0){assert(s.value==uint16_t((s.tag*53+s.word*7)^0x55aa));actual.push_back({s.tag,s.word});}
    if(actual!=accepted){std::fprintf(stderr,"sample count expected=%zu actual=%zu discarded=%u\n",accepted.size(),actual.size(),discarded);}
    assert(actual==accepted);assert(!unsafe_reads && !discarded);
}
static bool drained()
{
    if (!TEST_LINK_CONTINUITY) return aplay == AUDIO_IDLE && awp == arp;
    return aplay == AUDIO_PLAYING && (spi.CR1.value & 0x201) == 0x201 &&
           (stream.CR.value & 1) && awp == arp &&
           std::none_of(fifo.begin(), fifo.end(), [](Sample s) { return s.tag >= 0; });
}

static void drain_without_flush()
{
    for (unsigned i = 0; i < 40000 && !drained(); ++i) step(1);
    assert(drained());
    if (!TEST_LINK_CONTINUITY) assert(!(stream.CR.value & 1) && !(spi.CR1.value & 1));
    check_output();
}

static void drain()
{
    spi2_flush();
    drain_without_flush();
}

/* Keep payload-relative offsets for checks that inject a fault at a specific
 * DMA word. The continuous link sends one initial quiet bank. */
static void align_payload_start()
{
    if (!TEST_LINK_CONTINUITY) return;
    for (unsigned i = 0; i < 2 * ABUF_WORDS &&
         (fifo.empty() || fifo.front().tag < 0); ++i) step(1);
    assert(!fifo.empty() && fifo.front().tag >= 0 && fifo.front().word == 0);
    assert(fifo.size() == 8 && stream.NDTR.value == ABUF_WORDS - 8);
}

static void begin_recovery_suspend()
{
    if (!TEST_LINK_CONTINUITY) return;
    /* Inject a stale active DMA stream on a restart. Ordinary idle playback
     * under continuity has no reason to request a suspension. */
    assert(aplay == AUDIO_PLAYING && (stream.CR.value & 1));
    aplay = AUDIO_IDLE;
    audio_start(0);
    assert(aplay == AUDIO_SUSPENDING);
}
static void init_case(){
    constexpr unsigned bits[]={5,6,8,11};
    for(unsigned comb=0;comb<16;comb++){
        reset_model();for(unsigned b=0;b<4;b++)if(comb&(1u<<b))spi.SR.value|=1u<<bits[b];
        spi2_init();assert(!(spi.SR.value&0x960));
        assert(aplay == (TEST_LINK_CONTINUITY ? AUDIO_PLAYING : AUDIO_IDLE));
        assert(bool(spi.CR1.value & 1) == bool(TEST_LINK_CONTINUITY));
        assert(spi.I2SCFGR.value==0x02040015 && timer.PSC.value==1999 && timer.ARR.value==99);
        assert(publish(1)==1 && publish(2)==2);drain();
    }
}
static void full_case(){reset_model();spi2_init();for(int i=0;i<32;i++)assert(publish(i)==i+1);assert(publish(33)==-1);assert(awp-arp==32);drain();}
static void wrap_case(){reset_model();spi2_init();awp=arp=UINT32_MAX-10;for(int round=0;round<5;round++){for(int i=0;i<32;i++)assert(publish(round*32+i)==i+1);drain();}}
static void low_water(){reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(1176);assert(arp==1);step(1176);assert(arp==2);assert(!unsafe_reads);drain();}
static void late_case(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(1176);assert(arp==1);
    step(600);assert(dma.LISR.value&0x400); // HTIF must not suppress a valid late publication.
    assert(publish(3)>=1);drain();
    unsigned gap=0;bool began=false,finished=false;
    for(auto s:output){if(s.tag==2)began=true;if(s.tag==3)finished=true;if(began&&!finished&&s.tag<0)gap++;}
    assert(gap==0);
}
static void copy_progress(){step(2352);}
static void copy_case(){reset_model();spi2_init();publish(1);publish(2);copy_hook=copy_progress;publish(3);copy_hook=nullptr;drain();}
static void single_case(){
    reset_model();spi2_init();publish(1);
    assert(aplay == (TEST_LINK_CONTINUITY ? AUDIO_PLAYING : AUDIO_IDLE));
    drain();
}
static void startup_stall(){reset_model();bus_stalled=true;spi2_init();publish(1);publish(2);assert(aplay==AUDIO_STARTING && !(spi.CR1.value&0x200));for(int i=0;i<3;i++)fire_timer();assert(timer_irqs==3);bus_stalled=false;fire_timer();assert(aplay==AUDIO_PLAYING);drain();}
static void underrun_case(){
    reset_model();spi2_init();for(int i=0;i<8;i++)publish(i);step(100);bus_stalled=true;step(20);assert(spi.SR.value&0x20);assert(underrun_irqs==1);assert(!(spi.IER.value&0x20));
    for(int i=0;i<3;i++){step(88);fire_timer();}assert(underrun_irqs==1);
    bus_stalled=false;fire_timer();assert(!(spi.IER.value&0x20));step(4);fire_timer();assert(!(spi.SR.value&0x20) && (spi.IER.value&0x20));drain();
}
static void guard_race(int id){if(id==20){read_hook=nullptr;step(unsigned(stream.NDTR.value),false);}}
static void guard_miss(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(1176);read_hook=guard_race;publish(3);read_hook=nullptr;run_irqs();
    assert(active_writes==1 && aplay==AUDIO_RECOVER && !unsafe_reads);
    step(2);fire_timer();drain_without_flush();
    assert(!unsafe_reads);
}
static void init_stop_stall(){reset_model();stream.CR.value=1;have_preload=true;preload={0,-1,0};cursor=quiet_base+2;hold_disable=true;spi2_init();assert(aplay==AUDIO_INIT && !(spi.CR1.value&1));for(int i=0;i<3;i++)fire_timer();assert(aplay==AUDIO_INIT);hold_disable=false;fire_timer();assert(aplay==(TEST_LINK_CONTINUITY ? AUDIO_PLAYING : AUDIO_IDLE));publish(1);drain();}
static void near_boundary_case(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(1176);
    while(stream.NDTR.value>16)step(1);
    auto before=output.size();auto began=model_time;publish(3);
    while(!drained()){step(1);assert(model_time-began<50000000);}
    check_output();
    unsigned quiet=0;for(size_t i=before;i<output.size() && output[i].tag!=3;i++)if(output[i].tag<0)quiet++;
    std::printf("near-boundary quiet halfwords=%u (model %.3fms)\n",quiet,quiet*1000.0/88200);
    uint64_t end=0,start=0;for(auto x:output){if(x.tag==2)end=x.at;if(x.tag==3 && !start)start=x.at;}
    auto gap=start-end-5000;
    std::printf("complete boundary gap including stopped clock=%.3fus\n",gap/441.0);
    const auto bank_bound = uint64_t(ABUF_WORDS + AUDIO_PUBLISH_GUARD + 8) * 5000;
    assert(gap <= (TEST_LINK_CONTINUITY ? bank_bound : 441u * 500u));
    assert(quiet <= (TEST_LINK_CONTINUITY ? ABUF_WORDS + 8 : 49));
}
static void quiet_resume_case(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(2352+200);
    assert(arp==awp && aplay==AUDIO_PLAYING);
    auto before=output.size();auto began=model_time;publish(3);
    while(!drained()){step(1);assert(model_time-began<50000000);}
    check_output();unsigned quiet=0;
    for(size_t i=before;i<output.size() && output[i].tag!=3;i++)if(output[i].tag<0)quiet++;
    std::printf("mid-silence resume halfwords=%u (model %.3fms)\n",quiet,quiet*1000.0/88200);
    uint64_t first=0;for(auto x:output)if(x.tag==3){first=x.at;break;}
    std::printf("publish-to-resume including stopped clock=%.3fus\n",(first-began)/441.0);
    const auto bank_bound = uint64_t(ABUF_WORDS + AUDIO_PUBLISH_GUARD + 8) * 5000;
    assert(first-began <= (TEST_LINK_CONTINUITY ? bank_bound : 441u * 200u));
    assert(quiet <= (TEST_LINK_CONTINUITY ? ABUF_WORDS + 8 : 19));
}
static void pending_completion_case(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(1176,false);
    assert((dma.LISR.value&0x800) && arp==0);publish(3);run_irqs();drain();
}
static void stop_retry_case(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(2352);
    hold_disable=true;step(1176);
    begin_recovery_suspend();
    assert(aplay==AUDIO_SUSPENDING);
    step(2); /* Let the healthy stereo frame finish before delayed DMA stop. */
    assert(!(spi.CR1.value&0x200));
    for(int i=0;i<3;i++)fire_timer();
    assert(aplay==AUDIO_SUSPENDING && (stream.CR.value&1) && (spi.CR1.value&1));
    publish(3);hold_disable=false;fire_timer();drain();
}
static void stopped_clock_recovery(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(1176);read_hook=guard_race;publish(3);read_hook=nullptr;run_irqs();
    clock_stalled=true;unsigned before=timer_irqs;step(882);
    assert(timer_irqs-before<=12 && !unsafe_reads);
    clock_stalled=false;drain();
}
static void restart_flags_case(){
    constexpr unsigned bits[]={5,6,8,11};
    reset_model();spi2_init();
    for(unsigned repetition=0;repetition<50;repetition++){
        if (TEST_LINK_CONTINUITY) reset_model();
        unsigned comb=repetition%16;for(unsigned b=0;b<4;b++)if(comb&(1u<<b))spi.SR.value|=1u<<bits[b];
        if (TEST_LINK_CONTINUITY) spi2_init();
        publish(repetition*2);publish(repetition*2+1);
        assert(!(spi.SR.value&0x960) && aplay==AUDIO_PLAYING);
        drain();
    }
}
static void partial_transfer_error(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(200);
    stream.CR.value&=~uintptr_t(1);have_preload=false;dma.LISR.value|=0x200;run_irqs();
    drain();
    std::printf("exact payload continuation; hardware repeated %u modeled halfwords during fault\n",repeated_words);
}
// The actual worker audio admission, flush and pacing branch.
constexpr int PLAYTYPE_SECTOR=1, PLAYTYPE_FILE=2, PLAYTYPE_DIR=3;
struct { int mode; u32 fad_end; } worker_track;
struct {
    int play_type, play_wait;
    u32 fad, play_fad_end, repcnt, max_repeat;
    decltype(worker_track) *play_track;
    int pause_request = 0, status = 0;
} cdb;
struct { u8 *data; u32 fad; } wblk={input,0};
static unsigned cdda_reader_audio;
static u8 buf_subcode_mode;
static unsigned worker_wakeups, delay_calls, delay_ticks;
static bool timer_fails, delay_error;
static void disk_task_wakeup(){++worker_wakeups;}
#include "audio-notify.inc"
static int hw_delay(unsigned ticks){++delay_calls;delay_ticks=ticks;return timer_fails;}
static void publish_subrw(u32 fad){assert(fad==cdb.fad);}
static void counted_worker_flush(){worker_flushes++;spi2_flush();}
static void worker_tail(){
    wblk.fad=cdb.fad;
#define spi2_flush counted_worker_flush
#include "audio-worker.inc"
#undef spi2_flush
    }
_restart_wait:
    return;
_delay_error:
    delay_error=true;
}
static void worker_final_case(){
    for(int kind:{PLAYTYPE_SECTOR,PLAYTYPE_FILE,PLAYTYPE_DIR})for(int mode:{1,3})
    for(bool final:{false,true})for(bool repeats:{false,true}){
        reset_model();spi2_init();worker_track.mode=mode;
        cdb={kind,0,150,final?151u:152u,0,repeats?1u:0u,&worker_track};
        input_tag=40;cdda_reader_audio=0;worker_tail();
        bool audio=kind==PLAYTYPE_SECTOR && mode==3;
        assert(awp==unsigned(audio));
        if (TEST_LINK_CONTINUITY) assert(cdda_reader_audio==unsigned(audio));
        assert(worker_flushes==unsigned(audio && final && !repeats));
        if (!TEST_LINK_CONTINUITY) assert((aplay==AUDIO_PLAYING)==(audio && final && !repeats));
    }
    reset_model();spi2_init();for(int i=0;i<32;i++)publish(i);
    worker_track.mode=3;cdb={PLAYTYPE_SECTOR,0,150,151,0,0,&worker_track};
    cdda_reader_audio=0;worker_tail();
    assert(cdb.play_wait==1 && awp==32 && !worker_flushes && !cdda_reader_audio);drain();
}
/* Adapted from paps-g 7b6f6ebd. An accepted track tail must play even
 * when the next sector is data, delayed or never arrives. */
static void worker_track_tail_case(){
    for (u32 end:{301u,450u,300u}) for (bool repeat:{false,true})
    for (unsigned phase:{0u,580u,1170u}) {
        reset_model();spi2_init();step(phase);
        worker_track={3,299};
        cdb={PLAYTYPE_SECTOR,0,299,end,0,unsigned(repeat),&worker_track};
        cdda_reader_audio=0;buf_subcode_mode=0;
        prepare_input(1);worker_tail();
        assert(awp-arp==1 && !cdb.play_wait);
        assert(worker_flushes==unsigned(end==300 && !repeat));
        for (unsigned word=0;word<ABUF_WORDS;++word) accepted.emplace_back(1,word);
        drain_without_flush();
    }
}
static void quiet_tail_case(){
    reset_model();spi2_init();publish(1);publish(2);
    while(arp!=awp)step(1);
    assert(std::any_of(fifo.begin(),fifo.end(),[](Sample x){return x.tag>=0;}));
    publish(3);drain(); // Early quiet bank still contains the preceding valid FIFO tail.
}
static void fifo_warning_completion_case(){
    // A warning alone, on either side of completion, and coalesced with TC.
    for(unsigned words:{0u,200u,1167u,1168u,1176u,2344u}){
        reset_model();spi2_init();for(int i=0;i<4;i++)publish(i);align_payload_start();
        if(words){step(words-1);step(1,false);}dma.LISR.value|=0x40;
        auto old_read=arp;bool completion=(dma.LISR.value&0x800)!=0;
        run_irqs();
        assert(aplay==AUDIO_PLAYING && (stream.CR.value&1));
        assert(arp==old_read+unsigned(completion));
        assert(!(timer.CR1.value&1) && !underrun_irqs);
        drain();
    }
}
static void fifo_warning_startup_case(){
    reset_model();bus_stalled=true;spi2_init();publish(1);publish(2);
    assert((dma.LISR.value&0x40) && (stream.CR.value&1));
    run_irqs();assert(aplay==AUDIO_STARTING && !(spi.CR1.value&0x200));
    for(int i=0;i<3;i++)fire_timer();
    assert(aplay==AUDIO_STARTING && timer_irqs==3);
    bus_stalled=false;fire_timer();assert(aplay==AUDIO_PLAYING);drain();
}
static void fifo_warning_stall_case(){
    // Short stalls retain the FIFO's valid tail; longer ones also exercise
    // independent I2S UDR handling. Stall across both DBM bank boundaries.
    for(unsigned start:{200u,1167u,2343u})for(unsigned duration:{2u,6u,20u}){
        reset_model();spi2_init();for(int i=0;i<6;i++)publish(i);align_payload_start();
        step(start);bus_stalled=true;step(duration);
        assert((dma.LISR.value&0x40) || (dma.LISR.value&0x800));
        assert(aplay==AUDIO_PLAYING && (stream.CR.value&1));
        if(duration<8)assert(!(spi.SR.value&0x20) && !underrun_irqs && !(timer.CR1.value&1));
        else assert((spi.SR.value&0x20) && underrun_irqs==1);
        bus_stalled=false;drain();
    }
}
static void inject_transfer_error(){
    assert(stream.CR.value&1);
    stream.CR.value&=~uintptr_t(1);have_preload=false;dma.LISR.value|=0x200;run_irqs();
}
static void residual_transfer_case(){
    // Both stereo phases, before/at/after either bank switch and sequence wrap.
    for(unsigned start:{0u,1u,2u,199u,200u,201u,1166u,1167u,1168u,1169u,2343u,2344u,2345u}){
        reset_model();spi2_init();awp=arp=UINT32_MAX-1;
        for(int i=0;i<4;i++)publish(i);align_payload_start();step(start);inject_transfer_error();drain();
    }
}
static void latched_underrun_error_case(){
    for(unsigned start:{1155u,1156u,1157u,2331u,2332u,2333u}){
        reset_model();spi2_init();for(int i=0;i<4;i++)publish(i);align_payload_start();
        step(start);bus_stalled=true;step(20);assert(spi.SR.value&0x20);
        bus_stalled=false;dma_feed();assert(fifo.size()==8);
        inject_transfer_error();drain(); // UDR does not prove a refilled FIFO empty.
    }
}
static void repeated_residual_case(){
    reset_model();spi2_init();for(int i=0;i<32;i++)publish(i);align_payload_start();step(201);inject_transfer_error();
    for(unsigned n=0;n<3;n++){
        while(!(stream.CR.value&1))step(1);
        step(21+n);inject_transfer_error();
    }
    drain();
}
static void pending_error_completion_case(){
    for(unsigned start:{1168u,2344u}){
        reset_model();spi2_init();for(int i=0;i<4;i++)publish(i);align_payload_start();
        step(start-1);step(1,false);assert(dma.LISR.value&0x800);
        inject_transfer_error();drain();
    }
}
static void permanent_transfer_error_case(){
    reset_model();spi2_init();for(int i=0;i<4;i++)publish(i);align_payload_start();step(201);
    permanent_dma_fault=true;inject_transfer_error();
    unsigned irq_before=irq_calls;step(882);
    assert(irq_calls-irq_before<25 && !unsafe_reads && !discarded);
    permanent_dma_fault=false;drain();
}
static void residual_startup_case(){
    for(unsigned words:{1u,2u,7u}){
        reset_model();bus_stalled=true;
        if (TEST_LINK_CONTINUITY) { stream.CR.value=1; hold_disable=true; }
        spi2_init();publish(1);publish(2);
        if (TEST_LINK_CONTINUITY) { hold_disable=false; fire_timer(); }
        assert(aplay==AUDIO_STARTING);
        // Some accepted peripheral writes precede a source-side error before
        // CSTART. Transfer only the prefetched words admitted by the model.
        bus_stalled=false;inside_model=true;
        for(unsigned i=0;i<words;i++){preload_word();fifo.push_back(preload);have_preload=false;stream.NDTR.value--;}
        inside_model=false;bus_stalled=true;
        inject_transfer_error();bus_stalled=false;drain();
    }
}
static void residual_publication_case(){
    for(bool second:{false,true}){
        reset_model();spi2_init();publish(1);if(second)publish(2);else spi2_flush();align_payload_start();
        step(201);inject_transfer_error();while(!(stream.CR.value&1))step(1);
        assert(!(stream.CR.value&(1u<<18)));
        publish(3);publish(4);assert(!active_writes);drain();
    }
}
static void every_residual_count_case(){
    std::array<bool,1177> observed{};
    for(unsigned cut=0;cut<1176;cut++){
        reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(cut);
        observed[stream.NDTR.value]=true;inject_transfer_error();drain();
    }
    for(unsigned left=1;left<=1176;left++)assert(observed[left]);
}
static void delayed_error_disable_case(){
    // A pending destination beat may finish before EN acknowledges stop. The
    // timer cannot configure that stream or erase its late completion flag.
    for(bool completes:{false,true}){
        reset_model();spi2_init();publish(1);publish(2);align_payload_start();
        step(completes?1167:201);bus_stalled=true;
        dma.LISR.value|=0x200;run_irqs();assert(aplay==AUDIO_RECOVER);
        for(int n=0;n<3;n++)fire_timer();assert(aplay==AUDIO_RECOVER && (stream.CR.value&1));
        if(completes){bus_stalled=false;word_tick();assert(dma.LISR.value&0x800);}
        stream.CR.value&=~uintptr_t(1);have_preload=false;bus_stalled=false;
        // Deliver the timer before the already pending DMA IRQ, deliberately.
        if(timer.CR1.value&1){timer.CR1.value&=~uintptr_t(1);timer.SR.value|=1;TIM7_IRQHandler();}
        run_irqs();drain();
    }
}
static void quiet_error_phase_case(){
    for(unsigned position:{2344u,2345u,2346u,2347u,2352u,2353u,2552u,2553u,3499u,3500u}){
        reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(position);
        assert(arp==awp && aplay==AUDIO_PLAYING);
        inject_transfer_error();publish(3);publish(4);auto began=model_time;drain();
        uint64_t resumed=0;for(auto x:output)if(x.tag==3){resumed=x.at;break;}
        const auto bound = uint64_t(ABUF_WORDS + AUDIO_PUBLISH_GUARD + 8) * 5000 + 441000;
        assert(resumed && resumed-began <= (TEST_LINK_CONTINUITY ? bound : 441u * 1500u));
    }
}
// A hardware completion/error may arrive after LISR is read but before W1C.
static void completion_during_irq_snapshot(int id){
    if(id!=20)return;
    read_hook=nullptr;bus_stalled=false;word_tick();
    assert(dma.LISR.value&0x800);
    stream.CR.value&=~uintptr_t(1);have_preload=false;
}
static void late_completion_snapshot_case(){
    for(unsigned position:{1167u,2343u}){
        reset_model();spi2_init();for(int i=0;i<4;i++)publish(i);align_payload_start();step(position);
        assert(stream.NDTR.value==1);bus_stalled=true;dma.LISR.value|=0x200;
        read_hook=completion_during_irq_snapshot;run_irqs();read_hook=nullptr;
        drain();
    }
}
static void error_during_irq_snapshot(int id){
    if(id!=20)return;
    read_hook=nullptr;stream.CR.value&=~uintptr_t(1);have_preload=false;
    dma.LISR.value|=0x200;
}
static void late_error_snapshot_case(){
    for(unsigned position:{1168u,2344u}){
        reset_model();spi2_init();for(int i=0;i<4;i++)publish(i);align_payload_start();
        step(position-1);step(1,false); /* Only the final beat delays its IRQ. */
        assert(dma.LISR.value&0x800);
        read_hook=error_during_irq_snapshot;run_irqs();read_hook=nullptr;
        drain();
    }
}
// Only actual timer updates may advance a pending suspend deadline.
static void quiet_suspend_stale_case(){
    reset_model();spi2_init();publish(1);publish(2);
    if (TEST_LINK_CONTINUITY) {
        drain_without_flush();
        begin_recovery_suspend();
    } else {
        for(unsigned n=0;n<6000 && aplay!=AUDIO_SUSPENDING;n++)step(1);
    }
    assert(aplay==AUDIO_SUSPENDING && (spi.CR1.value&0x200));
    auto deadline=timer_left;auto arr=timer.ARR.value;auto cnt=timer.CNT.value;
    for(unsigned n=0;n<4;n++){
        SPI2_IRQHandler();TIM7_IRQHandler();
        assert(aplay==AUDIO_SUSPENDING && (spi.CR1.value&1));
        assert(timer_left==deadline && timer.ARR.value==arr && timer.CNT.value==cnt);
    }
    step(2,false);assert(!(spi.CR1.value&0x200) && (spi.SR.value&0x800));
    // Do not infer suspend acknowledgment from CSTART alone.
    spi.SR.value&=~uintptr_t(0x800);SPI2_IRQHandler();
    assert(aplay==AUDIO_SUSPENDING && (spi.CR1.value&1));
    spi.SR.value|=0x800;SPI2_IRQHandler();
    assert(drained() && !forced_stops && healthy_stops == 1);
    check_output();
}
static void quiet_stalled_suspend_case(){
    reset_model();spi2_init();publish(1);publish(2);align_payload_start();step(2352+200);
    assert(arp==awp && aplay==AUDIO_PLAYING);
    begin_recovery_suspend();
    clock_stalled=true;publish(3);
    if (!TEST_LINK_CONTINUITY) fire_timer();
    assert(aplay==AUDIO_SUSPENDING && (spi.CR1.value&0x200));
    auto deadline=timer_left;
    for(unsigned n=0;n<5;n++){
        SPI2_IRQHandler();TIM7_IRQHandler();assert(timer_left==deadline);
        assert(aplay==AUDIO_SUSPENDING && !forced_stops);
    }
    fire_timer();assert(aplay==AUDIO_SUSPENDING && !forced_stops && suspend_updates==1);
    fire_timer();assert(forced_stops==1 && (aplay==AUDIO_PLAYING || aplay==AUDIO_STARTING));
    clock_stalled=false;drain();assert(!unsafe_reads);
}

/* Adapted from paps-g 30f343ff and fb0bf193. Use the actual public handoff
 * and terminal branches with the same DMA/FIFO model as ordinary playback. */
constexpr int STAT_PAUSE=1, STAT_ERROR=9;
static unsigned pause_acks;
static void invalidate_directory() {}
static void set_pause_ok(){assert(!mask);++pause_acks;}
static void log_play_end() {}
static void set_peri_report() {}
static int finish_read(int valid){
    assert(!valid);cdb.status=STAT_ERROR;cdb.play_type=0;cdb.play_track=nullptr;return 0;
}
#define SSLOG(...) ((void)0)
#include "audio-ack.inc"
#undef SSLOG
static void worker_error(bool timer_error){
    if (timer_error) goto _delay_error;
    else goto _read_error;
#include "audio-error.inc"
_restart_wait:;
}

static void start_session(){
    reset_model();spi2_init();awp=arp=UINT32_MAX-1;
    for (int tag=1; tag<=4; ++tag) assert(publish(tag)==tag);
    align_payload_start();
}

static void keep_staged_tail(){
    accepted.clear();
    for (auto s:output) if (s.tag>=0) accepted.emplace_back(s.tag,s.word);
    for (auto s:fifo) if (s.tag>=0) accepted.emplace_back(s.tag,s.word);
}

static void check_replacement(unsigned expected_wakes){
    assert(awp==0 && arp==0 && wakeups==expected_wakes);
    assert((spi.CR1.value&0x201)==0x201 && !suspend_requested);
    assert(!forced_stops && !healthy_stops && !active_writes);
    assert(publish(100)==1 && publish(101)==2);
    drain_without_flush();
}

static void worker_retirement(){
    for (int reason=0;reason<3;++reason) {
        start_session();step(199);
        // Replacement can arrive on the next data track. A failed lookup
        // can already have dropped the cursor. Neither changes ownership.
        worker_track.mode=1;cdb={PLAYTYPE_SECTOR,0,150,180,0,0,reason==1?nullptr:&worker_track};
        cdda_reader_audio=1;
        pause_acks=0;
        keep_staged_tail();
        unsigned before=wakeups;
        if (reason==0) {
            cdb.pause_request=1;
            assert(!acknowledge_drive_request() && pause_acks==1 && !cdb.pause_request);
        } else worker_error(reason==2);
        assert(!cdb.play_type);
        check_replacement(before+1);
        assert(!cdda_reader_audio);
    }

    start_session();step(199);
    cdda_reader_audio=1;
    worker_track.mode=3;cdb={PLAYTYPE_SECTOR,0,150,180,0,0,&worker_track};
    pause_acks=0;cdb.pause_request=-PLAYTYPE_SECTOR;
    const auto write=awp, read=arp;
    assert(acknowledge_drive_request() && pause_acks==1 && !cdb.pause_request);
    assert(cdb.play_type==PLAYTYPE_SECTOR && awp==write && arp==read && cdda_reader_audio);
    drain();
}

#if TEST_LINK_CONTINUITY
static void cancel_all_phases(){
    for (unsigned position=0; position<3*ABUF_WORDS; ++position) {
        start_session();step(position);
        keep_staged_tail();
        unsigned before=wakeups;
        spi2_cancel();
        for (int n=0;n<3;++n) { spi2_cancel();SPI2_IRQHandler();TIM7_IRQHandler(); }
        check_replacement(before+1);
    }
}

static void interrupt_copy(){
    assert(!mask);
    spi2_cancel();
    assert(aplay==AUDIO_CANCELING && fill_audio_buffer(input)==-1);
}

static void pending_retirement(){
    for (unsigned phase:{0u,1u,199u,200u,1167u,1168u}) {
        start_session();step(phase);
        hold_disable=true;
        const auto write=awp, before=wakeups;
        spi2_cancel();
        for (int n=0;n<3;++n) {
            assert(aplay==AUDIO_CANCELING && (stream.CR.value&1) && (spi.CR1.value&0x201)==0x201);
            assert(fill_audio_buffer(input)==-1 && awp==write && wakeups==before);
            spi2_cancel();fire_timer();
        }
        keep_staged_tail();hold_disable=false;fire_timer();
        check_replacement(before+1);
    }
    start_session();step(199);keep_staged_tail();
    const auto before=wakeups;
    copy_hook=interrupt_copy;
    assert(publish(99)==-1);
    copy_hook=nullptr;
    check_replacement(before+1);

    reset_model();bus_stalled=true;spi2_init();
    assert(aplay==AUDIO_STARTING && !(spi.CR1.value&0x200));
    assert(publish(1)==1 && publish(2)==2);
    hold_disable=true;
    spi2_cancel();
    assert(aplay==AUDIO_CANCELING && fill_audio_buffer(input)==-1 && !wakeups);
    keep_staged_tail();hold_disable=bus_stalled=false;
    fire_timer();check_replacement(1);

    reset_model();bus_stalled=hold_disable=true;stream.CR.value=1;
    spi2_init();assert(aplay==AUDIO_INIT);
    assert(publish(1)==1 && publish(2)==2);
    spi2_cancel();assert(fill_audio_buffer(input)==-1);
    hold_disable=false;fire_timer();
    assert(spi.CFG1.value==0x00008000 && spi.I2SCFGR.value==0x02040015);
    keep_staged_tail();bus_stalled=false;fire_timer();
    check_replacement(1);

    start_session();step(201);keep_staged_tail();
    unsigned previous=wakeups;
    clock_stalled=true;spi2_cancel();
    assert(wakeups==previous+1 && awp==0 && arp==0);
    step(200);
    clock_stalled=false;check_replacement(previous+1);
}

/* Adapted from paps-g ddb07367. Drive the real producer through refusal
 * and the real completion callback before retrying the same FAD. */
static void worker_pacing()
{
    reset_model();spi2_init();spi2_dma_notify=disk_task_notify;
    buf_subcode_mode=0;delay_calls=worker_wakeups=0;timer_fails=delay_error=false;
    worker_track.mode=3;cdb={PLAYTYPE_SECTOR,0,150,1000,0,0,&worker_track};
    for (int tag=0;tag<32;++tag) {
        prepare_input(tag);worker_tail();
        assert(!delay_calls && !delay_error && !cdb.play_wait);
        for (unsigned word=0;word<ABUF_WORDS;++word) accepted.emplace_back(tag,word);
        ++cdb.fad;
    }
    assert(awp-arp==32);
    auto before=cdb.fad;
    prepare_input(32);worker_tail();
    assert(cdb.play_wait && cdb.fad==before && awp-arp==32 && !worker_wakeups && !delay_calls);
    for (unsigned i=0;i<2*ABUF_WORDS && !worker_wakeups;++i) step(1);
    assert(!cdb.play_wait && worker_wakeups==1 && awp-arp==31);
    worker_tail();
    assert(!cdb.play_wait && awp-arp==32 && !delay_calls);
    for (unsigned word=0;word<ABUF_WORDS;++word) accepted.emplace_back(32,word);
    drain_without_flush();

    for (unsigned depth:{4u,5u}) for (bool fail:{false,true}) {
        reset_model();spi2_init();buf_subcode_mode=1;
        delay_calls=delay_ticks=0;timer_fails=fail;delay_error=false;
        cdb={PLAYTYPE_SECTOR,0,150,1000,0,0,&worker_track};
        for (unsigned i=1;i<depth;++i) assert(publish(i)==int(i));
        prepare_input(100);worker_tail();
        assert(delay_calls==1 && delay_ticks==(depth==4 ? 10000u : 13500u));
        assert(delay_error==fail && !cdb.play_wait && awp-arp==depth);
        for (unsigned word=0;word<ABUF_WORDS;++word) accepted.emplace_back(100,word);
        drain_without_flush();
    }
    buf_subcode_mode=0;timer_fails=false;
}

static void continuous_idle(){
    reset_model();spi2_init();step(2*88200);
    assert(drained() && output.size()==2*88200 && !repeated_words);
    for (auto s:output) assert(s.tag<0 && !s.value);
    assert(!suspend_requested && !forced_stops && !healthy_stops && !wakeups);
    for (int n=0;n<8;++n) spi2_cancel();
    assert(!wakeups && drained());
}

static void quiet_rearm(){
    reset_model();spi2_init();step(2*ABUF_WORDS);
    assert(drained() && stream.M0AR.value==quiet_base && stream.M1AR.value==quiet_base);
    stream.CR.value&=~uintptr_t(1);stream.NDTR.value=0;dma.LISR.value=0;have_preload=false;
    step(16);assert(spi.SR.value&SPI_SR_UDR);
    assert(publish(1)==1 && publish(2)==2);
    drain_without_flush();
    assert(!healthy_stops && !forced_stops);
}
#endif

#if TEST_AUDIO_SCAN
static void scan_attenuation()
{
    const int16_t samples[] = {-32768, -32767, -5, -1, 0, 3, 4, 32767};
    reset_model();
    spi2_init();
    input_tag = 66;
    for (unsigned i = 0; i < ABUF_WORDS; ++i) {
        uint16_t value = samples[i % 8];
        input[i * 2] = value;
        input[i * 2 + 1] = value >> 8;
    }
    copy_hook = [] {
        assert(audio_copying && awp == arp);
        step(2 * ABUF_WORDS);
        assert(awp == arp && !unsafe_reads);
    };
    assert(fill_scan_audio_buffer(input) == 1);
    copy_hook = nullptr;
    for (unsigned i = 0; i < ABUF_WORDS; ++i) {
        uint16_t expected = samples[i % 8] / 4;
        assert(visible[i * 2] == (expected & 255) && visible[i * 2 + 1] == (expected >> 8));
    }
    for (unsigned i = 0; i < 40000 && !drained(); ++i) step(1);
    assert(drained());
    unsigned received = 0;
    for (auto word : output)
        if (word.tag == input_tag) {
            assert(word.word == received);
            assert(word.value == uint16_t(samples[received % 8] / 4));
            ++received;
        }
    assert(received == ABUF_WORDS && !unsafe_reads && !discarded);

    reset_model();
    spi2_init();
    copy_hook = interrupt_copy;
    assert(fill_scan_audio_buffer(input) == -1);
    copy_hook = nullptr;
    assert(awp == arp && !audio_copying && !unsafe_reads);
    assert(publish(7) == 1 && publish(8) == 2);
    drain_without_flush();
    check_output();
}
#endif

int main(int argc,char **argv){assert(argc==2);switch(std::atoi(argv[1])){
case 37:worker_retirement();break;case 43:worker_track_tail_case();break;
#if TEST_AUDIO_SCAN
case 44:scan_attenuation();break;
#endif
#if TEST_LINK_CONTINUITY
case 38:cancel_all_phases();break;case 39:pending_retirement();break;
case 40:continuous_idle();break;case 41:quiet_rearm();break;case 42:worker_pacing();break;
#endif
case 35:late_completion_snapshot_case();break;case 36:late_error_snapshot_case();break;case 33:quiet_suspend_stale_case();break;case 34:quiet_stalled_suspend_case();break;case 0:init_case();break;case 1:full_case();break;case 2:wrap_case();break;case 3:low_water();break;case 4:late_case();break;case 5:copy_case();break;case 6:single_case();break;case 7:startup_stall();break;case 8:underrun_case();break;case 9:guard_miss();break;case 10:init_stop_stall();break;case 11:near_boundary_case();break;case 12:quiet_resume_case();break;case 13:pending_completion_case();break;case 14:stop_retry_case();break;case 15:stopped_clock_recovery();break;case 16:restart_flags_case();break;case 17:partial_transfer_error();break;case 18:quiet_tail_case();break;case 19:worker_final_case();break;case 20:fifo_warning_completion_case();break;case 21:fifo_warning_startup_case();break;case 22:fifo_warning_stall_case();break;case 23:residual_transfer_case();break;case 24:latched_underrun_error_case();break;case 25:repeated_residual_case();break;case 26:pending_error_completion_case();break;case 27:permanent_transfer_error_case();break;case 28:residual_startup_case();break;case 29:residual_publication_case();break;case 30:every_residual_count_case();break;case 31:delayed_error_disable_case();break;case 32:quiet_error_phase_case();break;default:assert(false);}
std::puts("pass");}
