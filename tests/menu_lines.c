/* papsg adaptation of paps-g SSR-017. Production line conversion; bounded pixel callback and closed-form oracle. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(INT_MAX==2147483647,"32-bit int required");
static int x_origin,y_origin,major_delta,x_major;
static int64_t minor_origin,step;
static unsigned emitted,lines;
static uint64_t pixels;
static void put_pixel(int x,int y,int color)
{
    assert(x>=0 && x<512 && y>=0 && y<256 && color==7);
    assert(emitted<=(unsigned)major_delta);
    int minor=(int)((minor_origin+(int64_t)emitted*step)/65536);
    int ex=x_major?x_origin+(int)emitted:minor;
    int ey=x_major?minor:y_origin+(int)emitted;
    assert(x==ex && y==ey);
    ++emitted;++pixels;
}
#include "line.inc"
static void line_case(int x1,int y1,int x2,int y2)
{
    const int original[]={x1,y1,x2,y2};
    int64_t dx=(int64_t)x2-x1,dy=(int64_t)y2-y1;
    int64_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    x_major=ax>=ay;
    if((x_major && x1>x2)||(!x_major && y1>y2)) {
        int old=x1;x1=x2;x2=old;old=y1;y1=y2;y2=old;
        dx=-dx;dy=-dy;
    }
    x_origin=x1;y_origin=y1;major_delta=(int)(x_major?ax:ay);
    minor_origin=(int64_t)(x_major?y1:x1)*65536;
    step=major_delta?((x_major?dy:dx)*65536)/major_delta:0;
    emitted=0;
    put_line(original[0],original[1],original[2],original[3],7);
    assert(emitted==(unsigned)major_delta+1);
    ++lines;
}
int main(int argc,char **argv)
{
    const int cases[][4]={{10,10,20,20},{10,10,10,20},{10,10,10,10},
        {10,10,20,10},{20,20,10,10},{10,20,20,10},{20,10,10,30},
        {20,10,10,20},{10,30,20,10},{0,255,511,0},{511,0,0,255},
        {511,0,0,254},{0,0,511,255},{511,255,0,0},{511,255,511,255}};
    assert(argc==2);
    if(!strcmp(argv[1],"grid")) {
        for(int a=0;a<16;++a) for(int b=0;b<16;++b)
            for(int c=0;c<16;++c) for(int d=0;d<16;++d) line_case(a,b,c,d);
        assert(lines==65536);
    } else {
        unsigned n=(unsigned)strtoul(argv[1],NULL,10);assert(n<sizeof cases/sizeof cases[0]);
        const int *v=cases[n];line_case(v[0],v[1],v[2],v[3]);
    }
    printf("line cases=%u pixels=%llu pass\n",lines,(unsigned long long)pixels);
    return 0;
}
