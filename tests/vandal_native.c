/* SPDX-License-Identifier: GPL-2.0-or-later
 * Execute both Vandal AIPs, their native CD callers, the supplied stock BIOS
 * decoder and native VDP shadow routines. Only the CD register transaction,
 * sector FIFO, BIOS clock effects and one SMPC status read are modeled.
 * This is not whole-game, hardware timing or physical acceptance. */
/* Host-only native startup comparison adapted from paps-g 679cacde/4d135425.
 * The retained SAROO control is attributed to tpunix/yzb. No BIOS or original
 * game bytes are included. Yabause headers/library are supplied separately;
 * their original GPL notices remain with that dependency.
 */
#include "yui.h"
#include "peripheral.h"
#include "cs0.h"
#include "m68kcore.h"
#include "vidsoft.h"
#include "memory.h"
#include "sh2cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SH2Interface_struct *SH2CoreList[]={&SH2Interpreter,NULL};
PerInterface_struct *PERCoreList[]={&PERDummy,NULL};
CDInterface *CDCoreList[]={&DummyCD,NULL};
SoundInterface_struct *SNDCoreList[]={&SNDDummy,NULL};
VideoInterface_struct *VIDCoreList[]={&VIDDummy,NULL};
M68K_struct *M68KCoreList[]={&M68KDummy,&M68KMusashi,NULL};
void YuiErrorMsg(const char *s){fprintf(stderr,"%s\n",s);exit(1);}
void YuiSwapBuffers(void){}
#define LIMIT 0x100000U
#define LOW 0x00200000U
#define HIGH 0x06000000U
static readbytefunc read_byte;
static readwordfunc read_word;
static readlongfunc read_long;
static writebytefunc write_byte;
static writewordfunc write_word;
static writelongfunc write_long;
static void fail(const char *s,u32 address)
{fprintf(stderr,"FAIL %s %08x\n",s,address);exit(1);}
static u32 physical(u32 address){return address&0x1fffffffU;}

#define IP_BASE 0x06002000U
#define GAME_BASE 0x06040000U
#define CLOCK_MODEL 0x060f0000U
#define READ_BYTES 0x2f800U
#define PACK_OFFSET 0x2e600U
static unsigned char boot_ip[6144], boot_post_ip[6144];
static unsigned char boot_packed[4608], boot_post_packed[4608], boot_fifo[READ_BYTES];
static unsigned char boot_expected[2*LIMIT], boot_actual[2*LIMIT];
static unsigned char boot_video[288], boot_expected_video[288];
static unsigned char boot_vram[0x80000], boot_expected_vram[0x80000];
static unsigned char boot_cram[0x1000], boot_expected_cram[0x1000];
static u16 boot_cr[4];
static unsigned boot_command_reply;
static unsigned boot_variant, boot_seed, boot_mode, boot_fifo_at, boot_commands;
static unsigned boot_status_reads, boot_clock_calls, boot_decode_calls, boot_decoded;
static unsigned boot_filter_calls, boot_reader_calls, boot_init_calls, boot_apply_calls;
static unsigned boot_video_writes, boot_late_code_writes, boot_smpc_reads, boot_stack_depth;
static unsigned boot_vram_writes, boot_cram_writes;
static unsigned boot_data_adjustments;
static u32 boot_sp, boot_read_base, boot_step_pc;
static int boot_active;
static void boot_cd_command(void);
static u32 boot_initial_sr(void){unsigned profile=boot_seed%4;return (profile*5U<<4)|profile|(profile<<8);}
static const unsigned char boot_old_stub[48]={
 0xd1,9,0x61,0x12,0x41,0x0b,0xe4,1,0xbc,0x44,0,9,0xe3,1,0xc7,0x1b,
 0x91,9,0x62,2,0x22,0x11,0xba,0xe5,0x10,0x32,0xe0,9,0x61,0xc3,0x71,0x54,
 0x71,0x70,0x4c,0x2b,0x21,1,0x80,1,6,0,3,0x20,0,0x0b,0,9
};
#define BCHK(c,m) do {if(!(c))fail(m,MSH2->regs.PC);}while(0)
static unsigned boot_load(const char *path,unsigned char *out,unsigned bytes)
{
 FILE *f=fopen(path,"rb");if(!f)return 0;
 unsigned n=(unsigned)fread(out,1,bytes,f);
 int ok=!ferror(f) && fgetc(f)==EOF && n==bytes;
 return !fclose(f) && ok;
}
static void boot_put(SH2_struct *sh,u32 at,const unsigned char *p,unsigned bytes)
{for(unsigned i=0;i<bytes;++i)MappedMemoryWriteByteNocache(sh,at+i,p[i]);}
static void boot_snapshot(SH2_struct *sh,unsigned char *out)
{
 for(unsigned bank=0;bank<2;++bank)for(unsigned i=0;i<LIMIT;++i)
  out[bank*LIMIT+i]=MappedMemoryReadByteNocache(sh,(bank?HIGH:LOW)+i);
}
static u8 FASTCALL boot_read_byte(SH2_struct *sh,u32 a)
{
 u32 p=physical(a);
 if(p==0x00100063U){++boot_smpc_reads;return 0;}
 if(p>=0x05e00000U && p<0x05e80000U)return boot_vram[p-0x05e00000U];
 if(p>=0x05f00000U && p<0x05f01000U)return boot_cram[p-0x05f00000U];
 return read_byte(sh,a);
}
static u16 FASTCALL boot_read_word(SH2_struct *sh,u32 a)
{
 u32 p=physical(a);
 if((p>=0x05e00000U && p<=0x05e7fffeU) || (p>=0x05f00000U && p<=0x05f00ffeU))
  return (u16)((u16)boot_read_byte(sh,a)<<8|boot_read_byte(sh,a+1));
 if(p==0x05890008U)return ++boot_status_reads<=boot_seed?0:0x00c3;
 if(p>=0x05890018U && p<=0x05890024U && !((p-0x05890018U)&3U)){
  /* A complete command reply is followed by stable periodic PAUSE status. */
  u16 value=p==0x05890018U?(boot_command_reply?0x0100:0x2100):0;
  if(p==0x05890024U)boot_command_reply=0;
  return value;
 }
 if((p&0xffff0000U)==0x05890000U)fail("unexpected CD register",p);
 return read_word(sh,a);
}
static u32 FASTCALL boot_read_long(SH2_struct *sh,u32 a)
{
 u32 p=physical(a);
 if((p>=0x05e00000U && p<=0x05e7fffcU) || (p>=0x05f00000U && p<=0x05f00ffcU))
  return (u32)boot_read_word(sh,a)<<16|boot_read_word(sh,a+2);
 if(physical(a)==0x05818000U){
  BCHK(boot_fifo_at<=READ_BYTES-4,"FIFO extent exceeded");
  const unsigned char *b=boot_fifo+boot_fifo_at;boot_fifo_at+=4;
  return (u32)b[0]<<24|(u32)b[1]<<16|(u32)b[2]<<8|b[3];
 }
 if((physical(a)&0xffff0000U)==0x05810000U)fail("unexpected FIFO address",physical(a));
 return read_long(sh,a);
}
static void boot_write_guard(SH2_struct *sh,u32 a,unsigned bytes)
{
 if(!boot_active)return;
 a=physical(a);
 if(a>=boot_sp-512 && a<=boot_sp-bytes){
  unsigned depth=boot_sp-a;if(depth>boot_stack_depth)boot_stack_depth=depth;return;
 }
 if(a>=GAME_BASE && a<=GAME_BASE+5440-bytes){
  if(!boot_decoded)BCHK(boot_decode_calls==1 && sh->regs.PC<0x80000U,
                       "program write outside the original BIOS decoder");
  if(boot_decoded && a<GAME_BASE+0xc6 && a+bytes>GAME_BASE+0xc4){
   ++boot_late_code_writes;
   /* Yabause changes PC to the JMP target before executing its delay slot. */
   BCHK(!boot_variant && boot_step_pc==IP_BASE+0xd22 && bytes==2 &&
        a==GAME_BASE+0xc4 && sh->regs.R[0]==9 && sh->regs.R[1]==a,
        "native path wrote the decoded instruction");
  }else if(boot_decoded){
   BCHK(a==GAME_BASE+0x1b0 && bytes==4,"unexpected post-decode program write");
   ++boot_data_adjustments;
  }
  return;
 }
 if((a>=0x06000d00U && a<=0x06000e20U-bytes) ||
    (a>=0x06002d80U && a<=0x06002d88U-bytes) ||
    (a>=0x06003478U && a<=0x0600347aU-bytes) ||
    (a>=0x0600350cU && a<=0x06003518U-bytes))return;
 if(a>=boot_read_base && a<=boot_read_base+READ_BYTES-bytes){
  BCHK(!boot_decoded,"post-decode source write");return;
 }
 fail("write outside native outputs/private stack",a);
}
static void FASTCALL boot_write_byte(SH2_struct *sh,u32 a,u8 v)
{
 u32 p=physical(a);
 if(p>=0x05e00000U && p<0x05e80000U){
  BCHK(boot_clock_calls==1 && boot_init_calls==1,"VDP VRAM initialization order");
  boot_vram[p-0x05e00000U]=v;++boot_vram_writes;return;
 }
 if(p>=0x05f00000U && p<0x05f01000U){
  BCHK(boot_clock_calls==1 && boot_init_calls==1,"VDP CRAM initialization order");
  boot_cram[p-0x05f00000U]=v;++boot_cram_writes;return;
 }
 boot_write_guard(sh,a,1);write_byte(sh,a,v);
}
static void FASTCALL boot_write_word(SH2_struct *sh,u32 a,u16 v)
{
 u32 p=physical(a);
 if((p>=0x05e00000U && p<0x05e80000U) || (p>=0x05f00000U && p<0x05f01000U)){
  boot_write_byte(sh,a,(u8)(v>>8));boot_write_byte(sh,a+1,(u8)v);return;
 }
 if(p==0x05890008U)return; /* The CD transaction model stays ready. */
 if(p>=0x05890018U && p<=0x05890024U && !((p-0x05890018U)&3U)){
  boot_cr[(p-0x05890018U)/4]=v;
  if(p==0x05890024U)boot_cd_command();
  return;
 }
 if(p>=0x05f80000U && p<0x05f80120U){
  unsigned at=p-0x05f80000U;
  BCHK(!(at&1U) && boot_clock_calls==1 && boot_apply_calls==1,"VDP publication order");
  boot_video[at]=(u8)(v>>8);boot_video[at+1]=(u8)v;++boot_video_writes;return;
 }
 boot_write_guard(sh,a,2);write_word(sh,a,v);
}
static void FASTCALL boot_write_long(SH2_struct *sh,u32 a,u32 v)
{
 u32 p=physical(a);
 if((p>=0x05e00000U && p<0x05e80000U) || (p>=0x05f00000U && p<0x05f01000U)){
  boot_write_word(sh,a,(u16)(v>>16));boot_write_word(sh,a+2,(u16)v);return;
 }
 boot_write_guard(sh,a,4);write_long(sh,a,v);
}
static void boot_cd_command(void)
{
 unsigned command=boot_cr[0]>>8;
 BCHK(command==0x30 || command==0x10 || command==0x63 || command==6,"unexpected native CD command");
 if(command==0x30)BCHK(!boot_commands,"initial device selection");
 if(command==0x10){
  u32 start=(u32)(boot_cr[0]&255U)<<16|boot_cr[1];
  BCHK((start&0x7fffffU)==292214,"native read start FAD");
 }
 ++boot_commands;boot_command_reply=1;
}
int main(int argc,char **argv)
{
 if(argc!=6 && argc!=7)return 1;
 int broken=argc==7?(!strcmp(argv[6],"break-packed")?1:!strcmp(argv[6],"break-source")?2:!strcmp(argv[6],"break-reader")?3:!strcmp(argv[6],"break-filter")?4:0):0;
 if(argc==7 && !broken)return 1;
 if(!boot_load(argv[2],boot_ip,sizeof boot_ip) || !boot_load(argv[3],boot_post_ip,sizeof boot_post_ip) ||
    !boot_load(argv[4],boot_packed,sizeof boot_packed) || !boot_load(argv[5],boot_post_packed,sizeof boot_post_packed))return 1;
 if(memcmp(boot_ip,"SEGA SEGASATURN ",16) || boot_ip[0xf70]!=0x4c || boot_ip[0xf71]!=0x2b)return 1;
 for(unsigned i=0;i<sizeof boot_ip;++i)if((i<0xf00 || i>=0xf7a) && boot_ip[i]!=boot_post_ip[i])return 1;
 for(unsigned i=0;i<sizeof boot_packed;++i)
  if(boot_post_packed[i]!=(i==0xd6?0:i==0xd7?9:boot_packed[i]))return 1;
 if(boot_packed[0xd6]!=0x22 || boot_packed[0xd7]!=2)return 1;
 yabauseinit_struct init={0};init.biospath=argv[1];init.skip_load=1;
 init.percoretype=PERCORE_DUMMY;init.sh2coretype=SH2CORE_INTERPRETER;
 init.vidcoretype=VIDCORE_DUMMY;init.sndcoretype=SNDCORE_DUMMY;
 init.m68kcoretype=M68KCORE_DUMMY;init.cdcoretype=CDCORE_DUMMY;
 init.carttype=CART_NONE;init.regionid=1;init.videoformattype=VIDEOFORMATTYPE_NTSC;
 init.sh2_cache_enabled=1;if(YabauseInit(&init))return 1;
 SH2_struct *sh=MSH2;unsigned cases=0,max_steps=0,max_stack=0,dead_differences=0;
 read_byte=sh->MappedMemoryReadByte;read_word=sh->MappedMemoryReadWord;read_long=sh->MappedMemoryReadLong;
 write_byte=sh->MappedMemoryWriteByte;write_word=sh->MappedMemoryWriteWord;write_long=sh->MappedMemoryWriteLong;
 for(boot_mode=0;boot_mode<2;++boot_mode)for(boot_seed=0;boot_seed<5;++boot_seed){
  sh2regs_struct reference_entry={0},reference_live={0},reference_clock={0};unsigned old_stack=0;
  for(boot_variant=0;boot_variant<2;++boot_variant){
   unsigned char loaded_aip[122];
   boot_active=0;SH2Reset(sh);cache_clear(&sh->onchip.cache);
   if(boot_mode)cache_enable(&sh->onchip.cache);else cache_disable(&sh->onchip.cache);
   sh->onchip.CCR=boot_mode;
   sh->MappedMemoryReadByte=boot_read_byte;sh->MappedMemoryReadWord=boot_read_word;sh->MappedMemoryReadLong=boot_read_long;
   sh->MappedMemoryWriteByte=boot_write_byte;sh->MappedMemoryWriteWord=boot_write_word;sh->MappedMemoryWriteLong=boot_write_long;
   sh->ReadLongList[0x581]=boot_read_long;sh->ReadWordList[0x589]=boot_read_word;
   sh->WriteWordList[0x589]=boot_write_word;sh->WriteWordList[0x5f8]=boot_write_word;
   for(unsigned bank=0;bank<2;++bank)for(unsigned i=0;i<LIMIT;++i)
    MappedMemoryWriteByteNocache(sh,(bank?HIGH:LOW)+i,(u8)(i*29U+boot_seed*71U+bank*13U));
   boot_put(sh,IP_BASE,boot_variant?boot_post_ip:boot_ip,sizeof boot_ip);
   if(!boot_variant){boot_put(sh,IP_BASE+0xd00,boot_old_stub,sizeof boot_old_stub);MappedMemoryWriteWordNocache(sh,IP_BASE+0xf70,0xaec6);}
   if(boot_variant && broken==2){
    unsigned found=0;for(unsigned at=0xf00;at<0xf7a;at+=2)
     if(MappedMemoryReadWordNocache(sh,IP_BASE+at)==0x34dc){MappedMemoryWriteWordNocache(sh,IP_BASE+at,0x34ec);++found;}
    BCHK(found==1,"broken-source control site");
   }
   if(boot_variant && broken==3){
    unsigned found=0;
    for(unsigned at=0xf00;at<0xf78;at+=2){
     u16 op=MappedMemoryReadWordNocache(sh,IP_BASE+at);
     s32 displacement=(s32)((op&0xfff)^0x800)-0x800;
     if((op&0xf000)==0xb000 && IP_BASE+at+4+displacement*2==HIGH+0x300a){
      BCHK(MappedMemoryReadWordNocache(sh,IP_BASE+at+2)==0xe400,"reader delay-slot preimage");
      MappedMemoryWriteWordNocache(sh,IP_BASE+at+2,9);++found;
     }
    }
    BCHK(found==1,"broken-reader control site");
   }
   if(boot_variant && broken==4){
    /* Restore the rejected PC-relative literal load in the BSR delay slot. */
    u16 literal=MappedMemoryReadWordNocache(sh,IP_BASE+0xf0a);
    BCHK((literal&0xff00)==0xdc00 && (literal&255)>0 &&
      MappedMemoryReadWordNocache(sh,IP_BASE+0xf0c)==0xb0fc &&
      MappedMemoryReadWordNocache(sh,IP_BASE+0xf0e)==0x2fc2,"filter control preimage");
    MappedMemoryWriteWordNocache(sh,IP_BASE+0xf0a,0xb0fd);
    MappedMemoryWriteWordNocache(sh,IP_BASE+0xf0c,literal-1);
   }
   for(unsigned i=0;i<sizeof loaded_aip;++i)
    loaded_aip[i]=MappedMemoryReadByteNocache(sh,IP_BASE+0xf00+i);
   for(unsigned i=0;i<16;++i)MappedMemoryWriteWordNocache(sh,HIGH+0x380+i*2,(u16)(1U<<i));
   MappedMemoryWriteLongNocache(sh,HIGH+0x320,CLOCK_MODEL);
   MappedMemoryWriteLongNocache(sh,HIGH+0x2d7c,HIGH+0xd00);
   MappedMemoryWriteLongNocache(sh,HIGH+0x2d80,120);
   MappedMemoryWriteLongNocache(sh,HIGH+0x2d84,0);
   boot_sp=boot_seed==4?HIGH+0x2000:HIGH+0x5000+boot_seed*0x1000;boot_read_base=boot_sp+0x8000;
   for(unsigned i=0;i<READ_BYTES;++i)boot_fifo[i]=(u8)(i*37U+boot_seed*11U);
   memcpy(boot_fifo+PACK_OFFSET,boot_variant && broken!=1?boot_post_packed:boot_packed,sizeof boot_packed);
   boot_fifo_at=boot_commands=boot_status_reads=boot_clock_calls=boot_decode_calls=boot_decoded=0;
   boot_filter_calls=boot_reader_calls=boot_init_calls=boot_apply_calls=boot_video_writes=0;
   boot_late_code_writes=boot_smpc_reads=boot_stack_depth=0;memset(boot_video,0x5a,sizeof boot_video);
   boot_data_adjustments=0;
   boot_vram_writes=boot_cram_writes=0;memset(boot_vram,0x5a,sizeof boot_vram);memset(boot_cram,0x5a,sizeof boot_cram);
   memset(boot_cr,0,sizeof boot_cr);boot_command_reply=0;
   sh2regs_struct regs={0};for(unsigned i=0;i<16;++i)regs.R[i]=boot_seed==4?0:0x13570000U+i*0x10203U+boot_seed*17U;
   regs.R[15]=boot_sp;regs.PC=IP_BASE+0xf00;regs.PR=boot_seed==4?0:0x060ef000;
   regs.SR.all=boot_initial_sr();regs.VBR=HIGH;regs.GBR=0x060d0000;
   regs.MACH=0x12340000+boot_seed;regs.MACL=0x98760000+boot_seed;
   SH2SetRegisters(sh,&regs);sh->cycles=0;boot_active=1;unsigned steps=0;
   while(sh->regs.PC!=GAME_BASE && steps++<2000000){
    u32 pc=sh->regs.PC;
    if(pc==HIGH+0x3108){++boot_filter_calls;BCHK(sh->regs.R[4]==0,"filter parameter");}
    if(pc==HIGH+0x300a){++boot_reader_calls;BCHK(sh->regs.R[4]==0 && sh->regs.R[5]==READ_BYTES && sh->regs.R[6]==boot_read_base,"reader geometry");}
    if(pc==0x1f04){
     ++boot_decode_calls;BCHK(sh->regs.R[4]==boot_read_base+PACK_OFFSET+2 &&
       sh->regs.R[5]==boot_sp-16 && sh->regs.R[6]==5440 &&
       MappedMemoryReadLongNocache(sh,sh->regs.R[5])==GAME_BASE,"decoder arguments");
     BCHK(boot_fifo_at==READ_BYTES && sh->regs.R[13]==boot_read_base+READ_BYTES,"native reader final pointer");
    }
    if(boot_decode_calls && !boot_decoded && pc>=HIGH+0x2f00 && pc<HIGH+0x2f7a)boot_decoded=1;
    if(pc==CLOCK_MODEL){
     BCHK(boot_decoded && sh->regs.R[4]==1 && sh->regs.R[15]==boot_sp && !boot_clock_calls &&
       boot_data_adjustments==1 && MappedMemoryReadLongNocache(sh,GAME_BASE+0x1b0)==0x1aafb,"clock arguments/order");
     if(!boot_variant)reference_clock=sh->regs;
     else{
      BCHK(!memcmp(sh->regs.R+8,reference_clock.R+8,8*sizeof(u32)) &&
       sh->regs.SR.all==reference_clock.SR.all && sh->regs.VBR==reference_clock.VBR &&
       sh->regs.GBR==reference_clock.GBR && sh->regs.MACH==reference_clock.MACH &&
       sh->regs.MACL==reference_clock.MACL,"clock entry live-state mismatch");
     }
     ++boot_clock_calls;memset(boot_video,0,sizeof boot_video);cache_clear(&sh->onchip.cache);
     /* Lost VRAM is not a documented promise of zero-filled memory. */
     memset(boot_vram,(u8)(boot_seed*71U),sizeof boot_vram);
     memset(boot_cram,(u8)(boot_seed*29U),sizeof boot_cram);
     if(boot_mode)cache_enable(&sh->onchip.cache);
     for(unsigned i=0;i<8;++i)sh->regs.R[i]=0xa5a50000U+i*0x10203U+boot_seed;
     sh->regs.SR.all=boot_initial_sr();sh->regs.PC=sh->regs.PR;sh->cycles=0;continue;
    }
    if(pc==HIGH+0x2594){++boot_init_calls;BCHK(boot_clock_calls==1 && boot_decoded,"VDP init order");}
    if(pc==HIGH+0x22e4){++boot_apply_calls;BCHK(boot_init_calls==1 && MappedMemoryReadWordNocache(sh,HIGH+0xd00)==0x8001,"VDP apply order");}
    /* Discard interpreter scheduling debt before each architectural step.
     * This fixture measures state, not cycles or physical timing. */
    boot_step_pc=pc;sh->cycles=0;SH2Exec(sh,1);
   }
   BCHK(steps<2000000 && boot_decode_calls==1 && boot_decoded && boot_filter_calls==1 && boot_reader_calls==1,"incomplete native startup");
   BCHK(boot_commands==26 && boot_clock_calls==1 && boot_init_calls==1 && boot_apply_calls==1 && boot_video_writes==144,"native command/display counts");
   BCHK(boot_data_adjustments==1 && boot_vram_writes>0,"native relocation/video effects");
   BCHK(boot_late_code_writes==!boot_variant && sh->regs.R[15]==boot_sp && MappedMemoryReadWordNocache(sh,GAME_BASE+0xc4)==9,"native entry state/patch-writer count");
   if(!boot_variant){reference_entry=sh->regs;old_stack=boot_stack_depth;boot_snapshot(sh,boot_expected);memcpy(boot_expected_video,boot_video,sizeof boot_video);
    memcpy(boot_expected_vram,boot_vram,sizeof boot_vram);memcpy(boot_expected_cram,boot_cram,sizeof boot_cram);}
   else{
    for(unsigned i=2;i<16;++i)BCHK(sh->regs.R[i]==reference_entry.R[i],"entry live register mismatch");
    BCHK(sh->regs.SR.all==reference_entry.SR.all && sh->regs.VBR==reference_entry.VBR &&
      sh->regs.GBR==reference_entry.GBR && sh->regs.MACH==reference_entry.MACH && sh->regs.MACL==reference_entry.MACL,"entry special register mismatch");
    boot_snapshot(sh,boot_actual);
    memcpy(boot_expected+LIMIT+0x2d00,boot_ip+0xd00,sizeof boot_old_stub);
    /* Deliberate control instructions belong to that case's source oracle;
     * the control must fail because of its runtime effects. */
    memcpy(boot_expected+LIMIT+0x2f00,loaded_aip,sizeof loaded_aip);
    memcpy(boot_expected+LIMIT+(boot_read_base-HIGH)+PACK_OFFSET+0xd6,boot_post_packed+0xd6,2);
    unsigned depth=old_stack>boot_stack_depth?old_stack:boot_stack_depth;
    for(unsigned i=0;i<depth;++i){unsigned at=LIMIT+(boot_sp-HIGH)-depth+i;
     dead_differences+=boot_expected[at]!=boot_actual[at];}
    BCHK(!memcmp(boot_expected,boot_actual,sizeof boot_actual),"complete RAM/private stack mismatch");
    BCHK(!memcmp(boot_video,boot_expected_video,sizeof boot_video),"complete published VDP state mismatch");
    BCHK(!memcmp(boot_vram,boot_expected_vram,sizeof boot_vram) && !memcmp(boot_cram,boot_expected_cram,sizeof boot_cram),"complete VDP VRAM/CRAM mismatch");
   }
   /* R0/R1/PR may differ at handoff. Execute their actual first definitions. */
   unsigned live_steps=0;
   while(sh->regs.PC!=GAME_BASE+0x1da && live_steps++<32){boot_step_pc=sh->regs.PC;sh->cycles=0;SH2Exec(sh,1);}
   BCHK(live_steps<32 && boot_smpc_reads==1,"entry definition path");
   if(!boot_variant)reference_live=sh->regs;
   else BCHK(!memcmp(&reference_live,&sh->regs,sizeof reference_live),"native first-use state mismatch");
   if(steps>max_steps)max_steps=steps;if(boot_stack_depth>max_stack)max_stack=boot_stack_depth;
   boot_active=0;
  }
  ++cases;
 }
 printf("{\"complete\":true,\"cases\":%u,\"cache_modes\":2,\"seeds\":5,\"max_scheduling_calls\":%u,\"private_stack_bytes\":%u,\"dead_stack_differences\":%u,\"full_wram_bytes\":2097152,\"vdp_register_bytes\":288,\"vdp2_vram_bytes\":524288,\"vdp2_cram_bytes\":4096,\"native_fifo_bytes\":%u,\"native_filter_and_reader_executed\":true,\"native_decoder_executed\":true,\"native_vdp_routines_executed\":true,\"native_late_code_writes\":0,\"entry_first_definitions_checked\":true,\"device_accessed\":false}\n",cases,max_steps,max_stack,dead_differences,READ_BYTES);
 puts("LIMIT: modeled CD command replies/FIFO prefix, BIOS clock effects and one SMPC read; no complete BIOS boot, repeated disc reads, game, timing, audio or physical acceptance");
 YabauseDeInit();return 0;
}
