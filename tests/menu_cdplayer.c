/* Adapted from paps-g 0b9dc270. Actual menu functions with modeled CD registers. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef struct { u16 cr1, cr2, cr3, cr4; } CDCMD;
enum { HIRQ_CMOK=1, HIRQ_DRDY=2, HIRQ_PEND=16, BIOS_100V=1 };
static volatile u16 HIRQ, CR1, CR2, CR3, CR4;
static u8 ip_buffer[32768], toc[408], payload[32768];
static u16 bios_flags[2];
static int bios_type, use_sys_bup, use_sys_load, disc_type;
static int waits, wait_failure, response=0x100, command_result, end_result;
static int toc_result, selector_result, connect_result, play_result, get_result;
static int words=16384, plays, transfers, get_calls, ends, reads, players, player_mode;
static int no_pend, diagnostics;

static void printk(const char *fmt, ...) { (void)fmt; diagnostics++; }
static void clear_hirq(int bits) { HIRQ &= ~bits; }
static int wait_hirq(int bits) {
    (void)bits;
    waits++;
    CR1 = response;
    return waits == wait_failure ? -3 : 0;
}
static int cdc_cmd(int wait, CDCMD *cmd, CDCMD *resp, char *name) {
    (void)wait; (void)cmd; (void)name;
    memset(resp, 0, sizeof(*resp));
    resp->cr1=0x0034; resp->cr2=0x5678;
    return command_result;
}
static int cdc_end_trans(int *count) {
    ends++;
    if(count) *count = words;
    return end_result;
}
static u16 toc_word(void) {
    u16 v;
    assert(reads < 204);
    memcpy(&v, toc + reads++ * 2, 2);
    return v;
}
#define REG16(address) toc_word()
static int __attribute__((unused)) cdc_get_toc(u8 *buf) { memcpy(buf, toc, sizeof(toc)); return toc_result; }
static int cdc_reset_selector(int mode, int id) { (void)mode; (void)id; return selector_result; }
static int cdc_cddev_connect(int id) { (void)id; return connect_result; }
static int cdc_play_fad(int mode, int start, int length) {
    assert(mode==0 && start==150 && length==16);
    plays++;
    if(!no_pend) HIRQ |= HIRQ_PEND;
    return play_result;
}
static void cdc_dump(int count) { (void)count; }
static int cdc_get_del_data(int id, int start, int sectors) {
    assert(id==0 && start==0 && sectors==16);
    get_calls++;
    return get_result;
}
static void cdc_trans_data(u8 *buf, int length) {
    assert(!get_result && length==32768);
    transfers++;
    memcpy(buf, payload, length);
}
static int my_bios_loadcd_init(void) { return 0; }
static __attribute__((unused)) int cdblock_off(void) { return 0; }
static void my_cdplayer(void) { players++; player_mode=use_sys_load; }
static int rom_result, rom_checks;
static __attribute__((unused)) int prepare_game_rom(const char *id)
{
    assert(id == (const char *)(ip_buffer + 0x20) && transfers == 1);
    rom_checks++;
    return rom_result;
}
#include "actual.inc"

int main(int argc, char **argv) {
    assert(argc==2);
    int n=atoi(argv[1]), result;
    memset(toc, 0xff, sizeof(toc));
    toc[0]=0x41; toc[396]=0x41; toc[397]=1; toc[400]=0x41; toc[401]=1;
    memcpy(payload, "SEGA SEGASATURN ", 16);
    memcpy(payload+32, "VALID GAME ID   ", 16);
    memset(payload+96, 'G', 32);
    if(n<=4) {
        CDCMD cmd={0}, resp={0};
        if(n==0) wait_failure=1;
        if(n==1) wait_failure=2;
        if(n==2) response=0xff00;
        if(n==3) response=0x8100;
        result=actual_cdc_cmd(HIRQ_DRDY, &cmd, &resp, "probe");
        assert(result == (n<2 ? -3 : n==2 ? -5 : n==3 ? -6 : 0));
        assert(waits == (n==0 || n==2 || n==3 ? 1 : 2));
    } else if(n<=7) {
        u8 buf[408]; memset(buf, 0x5a, sizeof(buf));
        if(n==5) command_result=-5;
        if(n==6) end_result=-3;
        result=actual_get_toc(buf);
        assert(result == (n==5 ? -5 : n==6 ? -3 : 0));
        assert(reads == (n==5 ? 0 : 204));
        assert(ends == (n==5 ? 0 : 1));
        if(n==5) { for(unsigned i=0;i<sizeof(buf);i++) assert(buf[i]==0x5a); }
        else assert(!memcmp(buf,toc,sizeof(buf)));
    } else if(n<=13) {
        if(n==8) toc_result=-3;
        if(n==9) toc[0]=0x01;
        if(n==10) toc[397]=0;
        if(n==11) toc[397]=2;
        if(n==12) toc[401]=100;
        if(n==13) toc[0]=0xff;
        result=my_bios_loadcd_read();
        assert(result == (n==9 ? -2 : -1));
        assert(!plays && !get_calls && !transfers);
    } else if(n<=23) {
        if(n==14) selector_result=-3;
        if(n==15) connect_result=-3;
        if(n==16) play_result=-3;
        if(n==17) get_result=-5;
        if(n==18) end_result=-3;
        if(n==19) words=16383;
        if(n==20) payload[0]='X';
        if(n==22) bios_type=2;
        if(n==23) no_pend=1;
        result=my_bios_loadcd_read();
        assert(result == (n==20 ? -2 : n==21 || n==22 ? 0 : -1));
        if(n<=17 || n==23) assert(!transfers);
        else assert(transfers==1);
        if(n==21 || n==22) {
            assert(bios_flags[n==22 ? 1 : 0]==1);
            assert(!memcmp(ip_buffer, payload, sizeof(payload)));
        } else assert(!bios_flags[0] && !bios_flags[1]);
    } else if(n<=26) {
        int type=n==24 ? 0 : n==25 ? 0x80 : 2;
        toc[0]=0x01;
        result=bios_cd_cmd(type);
        assert(result==-2 && players==1 && !plays && !transfers);
        assert(player_mode == (n==26));
        assert(use_sys_bup == (n==25));
        assert(disc_type == (n==26 ? 2 : 0));
    } else if(n<=29) {
        if(n==27) { toc[397]=99; toc[401]=99; toc[392]=0x01; }
        if(n==28) toc[401]=99;
        if(n==29) { toc[397]=100; toc[401]=99; }
        result=my_bios_loadcd_read();
        assert(result == (n==27 ? -2 : n==28 ? 0 : -1));
        assert(transfers == (n==28));
    } else if (n >= 33) {
        static const int types[] = {0, 2, 4, 0x80, 0x84};
        static const int results[] = {0, -1, -4};
        assert(n < 48);
        int type = types[(n - 33) / 3];
        rom_result = results[(n - 33) % 3];
        assert(bios_cd_cmd(type) == rom_result);
        assert(rom_checks == 1 && transfers == 1 && !players);
        assert(use_sys_bup == ((type & 0x80) != 0) && disc_type == (type & 0x7f));
    } else {
        int count=-99;
        HIRQ=0xffff;
        if(n!=31) command_result=-3;
        result=actual_end_trans(n==32 ? NULL : &count);
        assert(result == (n==31 ? 0 : -3));
        assert(count == (n==31 ? 0x345678 : -99));
        assert(HIRQ == (n==31 ? (0xffff & ~HIRQ_DRDY) : 0xffff));
    }
    puts("pass");
    return 0;
}
