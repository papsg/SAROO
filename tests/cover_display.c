/* Actual gif_timer with deterministic asynchronous DMA/VDP seams.
 * Adapted from paps-g 681387bc's independent display fixture. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
_Alignas(8) static struct { u8 before[32],data[0x20000],after[32]; } gfx;
_Alignas(8) static struct { u8 before[32],data[0x40000],after[32]; } screen;
#define TEST_GFX_BASE ((uintptr_t)gfx.data)
#define VDP2_VRAM ((uintptr_t)screen.data)
static u32 TVSTAT=8,PRINA,CCCTL,BGON;
static int fbw=320,fbh=240;
static u32 palette[256];
static u8 *dma_out,*dma_in;
static int dma_bytes,dma_pending;
static unsigned polls,transfers;
static u32 display_status(void)
{
    polls++;
    if(dma_pending) {
        assert(gfx.data[0]);
        if(--dma_pending)return 0x3000;
        memcpy(dma_out,dma_in,dma_bytes);
    }
    return 0;
}
#define SDSTAT display_status()
static void scu_dmemcpy(void *out,void *in,int size,int channel)
{
    assert(channel==2 && size>0);
    assert((u8*)out>=screen.data+0x20000 && (u8*)out+size<=screen.data+0x20000+512*240);
    assert((u8*)in>=gfx.data+0x1000 && (u8*)in+size<=gfx.data+0x1000+512*240);
    /* The real helper waits for the preceding transfer, then starts this row. */
    while(display_status()&0x3000) {}
    dma_out=out;dma_in=in;dma_bytes=size;dma_pending=3;transfers++;
}
static u32 LE16(const void *address)
{ const u8 *p=address;return p[0]|(u32)p[1]<<8; }
static void put16(unsigned offset,int value)
{ gfx.data[offset]=value;gfx.data[offset+1]=(unsigned)value>>8; }
static void nbg1_set_cram(int index,int r,int g,int b)
{ assert(index>=0 && index<256);palette[index]=(b<<16)|(g<<8)|r; }
static void vdp2_win0(int scr,int outside,int sx,int sy,int ex,int ey)
{
    (void)outside;
    assert(scr==-1 || (scr==1 && sx>=0 && sy>=0 && ex<fbw && ey<fbh));
}
#include "cover-display.inc"
static u8 pixel(unsigned row,unsigned col) { return (row*71+col*17+(row^col)*3+9)%251; }

int main(int argc,char **argv)
{
    assert(argc==2);unsigned value=atoi(argv[1]);assert(value<11);
    const unsigned widths[]={128,128,1,127,512,127,320,127,128,127,128};
    const unsigned heights[]={128,192,1,97,240,31,240,97,192,97,128};
    unsigned w=widths[value],h=heights[value];
    int x=value==7?-5:value==8?230:value==9?-9:176;
    int y=value==7?-3:value==8?-7:value==9?-4:h>128?24:88;
    unsigned flag=value==5 || value==6 || value==9?1:3;
    unsigned stride=flag>=3?w:512;
    memset(&gfx,0xa5,sizeof(gfx));memset(&screen,0xa5,sizeof(screen));
    for(unsigned row=0;row<h;row++)for(unsigned col=0;col<stride;col++)
        gfx.data[0x1000+row*stride+col]=pixel(row,col);
    for(unsigned i=0;i<1024;i++)gfx.data[0x100+i]=(i*13+7)%256;
    put16(4,w);put16(6,h);put16(8,0);put16(10,0);put16(12,x);put16(14,y);
    gfx.data[0]=flag;gfx.data[2]=1;
    if(value==10) {
        TVSTAT=0;gif_timer();
        assert(!transfers && !polls && gfx.data[0]==flag && gfx.data[2]==1);
        for(unsigned i=0;i<512*240;i++)assert(screen.data[0x20000+i]==0xa5);
        puts("not-vblank: pending display retained");return 0;
    }
    gif_timer();assert(!gfx.data[0] && !gfx.data[2] && !dma_pending);
    unsigned rows=0;
    for(int row=0;row<240;row++)for(int col=0;col<512;col++) {
        u8 expected=0;
        if(col<fbw && col>=x && col<x+(int)w && row>=y && row<y+(int)h)
            expected=pixel(row-y,col-x);
        assert(screen.data[0x20000+row*512+col]==expected);
    }
    for(int row=0;row<240;row++)if(row>=y && row<y+(int)h)rows++;
    assert(transfers==rows);
    for(unsigned i=0;i<256;i++)assert(palette[i]==
        ((u32)gfx.data[0x100+4*i]<<16|(u32)gfx.data[0x101+4*i]<<8|gfx.data[0x102+4*i]));
    for(unsigned i=0;i<32;i++)
        assert(screen.before[i]==0xa5 && screen.after[i]==0xa5 && gfx.before[i]==0xa5 && gfx.after[i]==0xa5);
    printf("display case=%u flag=%u size=%ux%u rows=%u polls=%u complete=1\n",value,flag,w,h,transfers,polls);
}
