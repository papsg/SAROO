/* Record-core checks. These do not stand in for Saturn backup-API integration. */
#include "save_journal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint32_t crc32(uint8_t *, int, uint32_t);
static uint8_t blob[SJ_MAX_BYTES], copy[SJ_MAX_BYTES], payload[65536];
static uint8_t claimed[SJ_BITMAP_BYTES];
static uint32_t full_size;
static SaveJournalView view;
static unsigned checks;

static void put(uint8_t *p, uint32_t n)
{ p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n; }
static void reseal(uint8_t *b,uint32_t n)
{
    put(b+32,crc32(b+512,n-512,0));put(b+36,crc32(b,36,0));
}
static uint8_t pattern(unsigned record,unsigned index)
{ return (record*29+index*7)%251; }
static uint32_t make(unsigned mode)
{
    SaveJournalBuilder b;
    uint8_t game[]="TESTGAME00000001";
    unsigned device=mode!=0,count=mode==0 || mode==1 ? 1 : mode==2 ? 2 : 481;
    assert(save_journal_begin(&b,blob,sizeof blob,claimed,device,game,count)==SJ_READY);
    assert(save_journal_seal(&b)==SJ_INVALID);
    for(unsigned i=0;i<count;i++) {
        unsigned size=!i ? (device ? 8192 : 65536) : mode==2 ? 33 : 1024;
        unsigned block=i==1 ? 8063 : 8+((i-2)*17)%8055;
        for(unsigned k=0;k<size;k++)payload[k]=pattern(i,k);
        assert(save_journal_append(&b,i?block*1024:0,payload,size)==SJ_READY);
    }
    assert(save_journal_seal(&b)==SJ_READY);full_size=b.length;
    assert(save_journal_validate(blob,full_size,claimed,&view)==SJ_READY);
    SaveJournalCursor cursor={0};uint32_t offset,size;const uint8_t *data;
    for(unsigned i=0;i<count;i++) {
        data=save_journal_next(&view,&cursor,&offset,&size);assert(data);
        assert(offset==(i ? (i==1 ? 8063 : 8+((i-2)*17)%8055)*1024 : 0));
        for(unsigned k=0;k<size;k++)assert(data[k]==pattern(i,k));
    }
    assert(cursor.position==full_size && cursor.index==count);
    assert(!save_journal_next(&view,&cursor,&offset,&size));checks++;
    return full_size;
}

static void invalid(void)
{
    memset(&view,0xcc,sizeof view);
    assert(save_journal_validate(copy,full_size,claimed,&view)==SJ_INVALID);
    assert(!view.bytes && !view.length && !view.count && !view.device);checks++;
}
static void corruption(void)
{
    make(3);
    for(unsigned at=0;at<40;at++)for(unsigned bit=0;bit<8;bit++) {
        memcpy(copy,blob,full_size);copy[at]^=1u<<bit;invalid();
    }
    const unsigned spots[]={40,41,255,511,512,513,519,520,4095,4359,4360,8191,65535,504070,504071};
    for(unsigned i=0;i<sizeof spots/sizeof spots[0];i++) {
        memcpy(copy,blob,full_size);copy[spots[i]]^=0x80;invalid();
    }
    const unsigned fields[][2]={{4,512},{8,0},{8,482},{12,2},{16,0},
        {512,1024},{516,8191},{520,8191},{520,8064*1024},{520,0xfffffc00},
        {524,0},{524,1025},{528,8063*1024},{532,0xffffffff}};
    for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++) {
        memcpy(copy,blob,full_size);put(copy+fields[i][0],fields[i][1]);reseal(copy,full_size);
        if(fields[i][0]==16)continue; /* MEMS is global; its identity may be empty. */
        invalid();
    }
    memcpy(copy,blob,full_size);put(copy+4,full_size-1);reseal(copy,full_size-1);
    assert(save_journal_validate(copy,full_size-1,claimed,&view)==SJ_INVALID);checks++;
    memcpy(copy,blob,full_size);put(copy+4,full_size+1);reseal(copy,full_size+1);
    assert(save_journal_validate(copy,full_size+1,claimed,&view)==SJ_INVALID);checks++;
    for(unsigned n=1;n<512;n++) {
        assert(save_journal_validate(blob,n,claimed,&view)==SJ_INVALID);checks++;
    }
    make(0);memcpy(copy,blob,full_size);copy[16]=0;reseal(copy,full_size);invalid();
    memset(copy,0,512);
    assert(save_journal_validate(copy,full_size,claimed,&view)==SJ_EMPTY);
    assert(save_journal_validate(NULL,0,NULL,&view)==SJ_EMPTY);checks+=2;
}

static void builder_bounds(void)
{
    SaveJournalBuilder b;uint8_t game[]="TESTGAME00000001";
    for(unsigned device=0;device<4;device++)for(unsigned count=0;count<483;count++) {
        int valid=device<2 && count && count<=481 && (device || count==1);
        memset(blob,0xa5,sizeof blob);
        int r=save_journal_begin(&b,blob,sizeof blob,claimed,device,game,count);
        assert(r==(valid?SJ_READY:SJ_INVALID));checks++;
        if(!valid)for(unsigned i=0;i<sizeof blob;i++)assert(blob[i]==0xa5);
    }
    assert(save_journal_begin(&b,blob,519,claimed,0,game,1)==SJ_INVALID);
    assert(save_journal_begin(&b,blob,sizeof blob+1,claimed,0,game,1)==SJ_INVALID);
    assert(save_journal_begin(&b,blob,66055,claimed,0,game,1)==SJ_READY);
    assert(save_journal_append(&b,0,payload,65536)==SJ_INVALID);
    assert(!b.added && b.length==520);
    assert(save_journal_begin(&b,blob,sizeof blob,claimed,1,game,3)==SJ_READY);
    assert(save_journal_append(&b,0,payload,8192)==SJ_READY);
    assert(save_journal_append(&b,8192,payload,1024)==SJ_READY);
    unsigned before=b.length;
    assert(save_journal_append(&b,8192,payload,1024)==SJ_INVALID);
    assert(b.added==2 && b.length==before);
    assert(save_journal_append(&b,9216,payload,1)==SJ_READY);
    assert(save_journal_seal(&b)==SJ_READY);
    assert(save_journal_validate(blob,b.length,claimed,&view)==SJ_READY);checks+=10;
}

typedef struct {
    uint8_t bytes[SJ_MAX_BYTES];
    unsigned size,calls,fail_at,kind;
} Store;
static Store store;
static int persist(void *context,int offset,const uint8_t *bytes,uint32_t size)
{
    Store *s=context;unsigned call=++s->calls;
    unsigned parts=(full_size+SJ_WRITE_BYTES-1)/SJ_WRITE_BYTES;
    if(call<=parts) {
        unsigned start=(call-1)*SJ_WRITE_BYTES,count=full_size-start;
        if(count>SJ_WRITE_BYTES)count=SJ_WRITE_BYTES;
        assert(offset==(call==1?-1:(int)start) && size==count);
        if(call==1)for(unsigned i=0;i<512;i++)assert(!bytes[i]);
    } else assert(!offset && size==512);
    if(call==s->fail_at && s->kind==0)return -1;
    if(offset<0)s->size=0,offset=0;
    unsigned copied=call==s->fail_at && s->kind==1 ? size/2 : size;
    memcpy(s->bytes+offset,bytes,copied);
    if(s->size<(unsigned)offset+copied)s->size=offset+copied;
    if(call==s->fail_at)return s->kind==1 ? (int)copied : -1;
    return size;
}
static void publication(void)
{
    for(unsigned mode=0;mode<4;mode++) {
    make(mode);unsigned parts=(full_size+SJ_WRITE_BYTES-1)/SJ_WRITE_BYTES;
    for(unsigned phase=0;phase<=parts+2;phase++)
    for(unsigned kind=0;kind<3;kind++) {
        make(mode);memcpy(copy,blob,full_size);memset(&store,0,sizeof store);
        store.fail_at=phase;store.kind=kind;
        int r=save_journal_publish(blob,full_size,persist,&store);
        assert(!memcmp(copy,blob,full_size));
        assert(r==((phase && phase<=parts+1)?SJ_IO_ERROR:SJ_READY));
        int valid=save_journal_validate(store.bytes,store.size,claimed,&view);
        if(valid==SJ_READY)assert(store.size==full_size && !memcmp(copy,store.bytes,full_size));
        if(phase && phase<=parts)assert(valid!=SJ_READY);
        if(r==SJ_READY) {
            assert(valid==SJ_READY);
            r=save_journal_clear(blob,persist,&store);
            assert(r==(phase==parts+2?SJ_IO_ERROR:SJ_EMPTY));
            valid=save_journal_validate(store.bytes,store.size,claimed,&view);
            if(valid==SJ_READY)assert(!memcmp(copy,store.bytes,full_size));
            if(r==SJ_EMPTY)assert(valid==SJ_EMPTY);
        }
        checks++;
    }
    }
}
int main(int argc,char **argv)
{
    assert(argc==2 || argc==3);
    if(!strcmp(argv[1],"emit")) {
        assert(argc==3);make(atoi(argv[2]));assert(fwrite(blob,1,full_size,stdout)==full_size);return 0;
    }
    assert(!strcmp(argv[1],"check"));
    for(unsigned mode=0;mode<4;mode++)make(mode);
    corruption();builder_bounds();publication();
    printf("PASS %u record/bounds/corruption/publication checks\n",checks);
    return 0;
}
