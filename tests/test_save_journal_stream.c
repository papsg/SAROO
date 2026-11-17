/* Split publication must produce the same durable SJR1 bytes and write batches
 * as the independently built contiguous record, even when file I/O reuses the
 * entire prefix allocation after consuming each request. */
#define main journal_core_main
#include "test_save_journal.c"
#undef main

static struct {
    uint8_t before[32], bytes[SJ_WRITE_BYTES], after[32];
} workspace;
static unsigned stream_checks;

static uint32_t get_word(const uint8_t *p)
{ return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }

static SaveJournalParts split_record(const uint8_t *record, unsigned split)
{
    assert(split <= sizeof workspace.bytes && split <= full_size);
    memset(&workspace,0xa5,sizeof workspace);
    memcpy(workspace.bytes,record,split);
    return (SaveJournalParts){workspace.bytes,record+split,split,full_size-split};
}

static void guards(void)
{
    for(unsigned i=0;i<32;i++)
        assert(workspace.before[i]==0xa5 && workspace.after[i]==0xa5);
}

static int bounce_persist(void *context,int offset,const uint8_t *bytes,uint32_t size)
{
    int result=persist(context,offset,bytes,size);
    /* Scratch is not retained input. This also poisons failed requests and the
     * final marker so an accidental dependency on post-call contents fails. */
    memset(workspace.bytes,0x6d,sizeof workspace.bytes);
    guards();
    return result;
}

static void publish_splits(void)
{
    for(unsigned mode=0;mode<4;mode++) {
        make(mode);memcpy(copy,blob,full_size);
        unsigned records=get_word(blob+8),split=512+records*8;
        unsigned chunks=(full_size+SJ_WRITE_BYTES-1)/SJ_WRITE_BYTES;
        for(unsigned boundary=0;boundary<=records && split<=SJ_WRITE_BYTES;boundary++) {
            for(unsigned phase=0;phase<=chunks+2;phase++)for(unsigned kind=0;kind<3;kind++) {
                SaveJournalParts parts=split_record(blob,split);
                memset(&store,0,sizeof store);store.fail_at=phase;store.kind=kind;
                assert(save_journal_validate_parts(&parts,claimed)==SJ_READY);
                int r=save_journal_publish_parts(&parts,claimed,bounce_persist,&store);
                assert(r==((phase && phase<=chunks+1)?SJ_IO_ERROR:SJ_READY));
                assert(!memcmp(blob,copy,full_size));guards();
                int valid=save_journal_validate(store.bytes,store.size,claimed,&view);
                if(valid==SJ_READY)assert(store.size==full_size && !memcmp(blob,store.bytes,full_size));
                if(phase && phase<=chunks)assert(valid!=SJ_READY);
                if(r==SJ_READY) {
                    assert(valid==SJ_READY && store.calls==chunks+1);
                    r=save_journal_clear(workspace.bytes,bounce_persist,&store);
                    assert(r==(phase==chunks+2?SJ_IO_ERROR:SJ_EMPTY));
                    valid=save_journal_validate(store.bytes,store.size,claimed,&view);
                    if(valid==SJ_READY)assert(!memcmp(blob,store.bytes,full_size));
                    if(r==SJ_EMPTY)assert(valid==SJ_EMPTY);
                    guards();
                }
                stream_checks++;
            }
            if(boundary<records)split+=get_word(blob+512+boundary*8+4);
        }
    }
}

static void build_splits(void)
{
    uint8_t game[]="TESTGAME00000001";
    for(unsigned mode=0;mode<4;mode++) {
        make(mode);
        unsigned records=get_word(blob+8),split=512+records*8;
        for(unsigned boundary=0;boundary<=records && split<=SJ_WRITE_BYTES;boundary++) {
            SaveJournalBuilder b;SaveJournalParts parts;
            memset(&workspace,0xa5,sizeof workspace);
            assert(save_journal_begin(&b,workspace.bytes,sizeof workspace.bytes,claimed,
                                      mode!=0,game,records)==SJ_READY);
            unsigned cursor=512+records*8;
            assert(save_journal_seal_parts(&b,NULL,&parts)==SJ_INVALID);
            assert(!parts.prefix && !parts.payload && !parts.prefix_bytes && !parts.payload_bytes);
            for(unsigned i=0;i<records;i++) {
                unsigned offset=get_word(blob+512+i*8),size=get_word(blob+512+i*8+4);
                if(cursor<split)assert(save_journal_append(&b,offset,blob+cursor,size)==SJ_READY);
                else {
                    assert(save_journal_reference(&b,offset,size)==SJ_READY);
                    assert(save_journal_append(&b,offset,blob+cursor,size)==SJ_INVALID);
                    assert(save_journal_seal(&b)==SJ_INVALID);
                }
                cursor+=size;
            }
            assert(cursor==full_size && b.length==full_size);
            assert(save_journal_seal_parts(&b,split==full_size?NULL:blob+split,&parts)==SJ_READY);
            assert(parts.prefix_bytes==split && parts.payload_bytes==full_size-split);
            assert(!memcmp(parts.prefix,blob,split));
            assert(save_journal_validate_parts(&parts,claimed)==SJ_READY);
            assert(save_journal_reference(&b,8192,1)==SJ_INVALID);
            /* Runtime reuses these controls after the builder's last read.
             * Compare against the independent split result at every boundary. */
            union { SaveJournalBuilder builder; SaveJournalParts parts; } reused;
            reused.builder = b;
            assert(save_journal_seal_parts(&reused.builder, split==full_size?NULL:blob+split,
                                           &reused.parts)==SJ_READY);
            assert(reused.parts.prefix == parts.prefix && reused.parts.payload == parts.payload &&
                   reused.parts.prefix_bytes == parts.prefix_bytes &&
                   reused.parts.payload_bytes == parts.payload_bytes);
            reused.builder = b; reused.builder.added = 0;
            assert(save_journal_seal_parts(&reused.builder, NULL, &reused.parts)==SJ_INVALID);
            assert(!reused.parts.prefix && !reused.parts.payload &&
                   !reused.parts.prefix_bytes && !reused.parts.payload_bytes);
            stream_checks += 2;
            guards();stream_checks++;
            if(boundary<records)split+=get_word(blob+512+boundary*8+4);
        }
    }
    SaveJournalBuilder b;SaveJournalParts parts;
    assert(save_journal_begin(&b,workspace.bytes,sizeof workspace.bytes,claimed,1,game,3)==SJ_READY);
    assert(save_journal_reference(&b,0,8192)==SJ_READY);
    assert(save_journal_reference(&b,8192,1024)==SJ_READY);
    unsigned before=b.length;
    assert(save_journal_reference(&b,8192,1024)==SJ_INVALID);
    assert(b.added==2 && b.length==before);
    assert(save_journal_reference(&b,9216,1024)==SJ_READY);
    assert(save_journal_seal_parts(&b,NULL,&parts)==SJ_INVALID);
    assert(save_journal_seal_parts(&b,payload,NULL)==SJ_INVALID);
    stream_checks+=5;
}

static void invalid_split(const uint8_t *record,unsigned split)
{
    SaveJournalParts parts=split_record(record,split);
    assert(save_journal_validate_parts(&parts,claimed)==SJ_INVALID);
    memset(&store,0,sizeof store);
    assert(save_journal_publish_parts(&parts,claimed,bounce_persist,&store)==SJ_INVALID);
    assert(!store.calls);guards();stream_checks++;
}

static void validation(void)
{
    make(3);unsigned table=512+481*8;
    for(unsigned split=512;split<65536;split++) {
        SaveJournalParts parts=split_record(blob,split);
        int boundary=split==table || (split>=table+8192 && (split-table-8192)%1024==0);
        assert(save_journal_validate_parts(&parts,claimed)==(boundary?SJ_READY:SJ_INVALID));
        guards();stream_checks++;
    }
    for(unsigned at=0;at<40;at++)for(unsigned bit=0;bit<8;bit++) {
        memcpy(copy,blob,full_size);copy[at]^=1u<<bit;invalid_split(copy,table);
    }
    const unsigned positions[]={40,511,512,519,520,4359,4360,12551,13576,65535,504071};
    for(unsigned i=0;i<sizeof positions/sizeof positions[0];i++) {
        memcpy(copy,blob,full_size);copy[positions[i]]^=0x80;invalid_split(copy,table);
    }
    const unsigned fields[][2]={{4,512},{8,482},{12,2},{512,1024},{516,8191},
        {520,8191},{520,8064*1024},{524,0},{524,1025},{528,8063*1024},{532,0xffffffff}};
    for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++) {
        memcpy(copy,blob,full_size);put(copy+fields[i][0],fields[i][1]);
        reseal(copy,full_size);invalid_split(copy,table);
    }
    SaveJournalParts parts=split_record(blob,table),bad=parts;
    assert(save_journal_validate_parts(NULL,claimed)==SJ_INVALID);
    assert(save_journal_validate_parts(&parts,NULL)==SJ_INVALID);
    bad.prefix=NULL;assert(save_journal_validate_parts(&bad,claimed)==SJ_INVALID);
    bad=parts;bad.prefix_bytes=511;assert(save_journal_validate_parts(&bad,claimed)==SJ_INVALID);
    bad=parts;bad.prefix_bytes=65537;assert(save_journal_validate_parts(&bad,claimed)==SJ_INVALID);
    bad=parts;bad.payload_bytes=SJ_MAX_BYTES;assert(save_journal_validate_parts(&bad,claimed)==SJ_INVALID);
    bad=parts;bad.payload=NULL;assert(save_journal_validate_parts(&bad,claimed)==SJ_INVALID);
    bad=parts;bad.payload_bytes--;assert(save_journal_validate_parts(&bad,claimed)==SJ_INVALID);
    memset(workspace.bytes,0,512);
    assert(save_journal_validate_parts(&parts,claimed)==SJ_EMPTY);guards();stream_checks+=9;

    make(1);parts=split_record(blob,full_size);parts.payload=NULL;
    assert(save_journal_validate_parts(&parts,claimed)==SJ_READY);
    memset(&store,0,sizeof store);
    assert(save_journal_publish_parts(&parts,claimed,bounce_persist,&store)==SJ_READY);
    assert(!memcmp(blob,store.bytes,full_size));guards();stream_checks++;

    /* A split view may refer to adjacent bytes of one valid record, but the
     * publisher needs a separate full-sized scratch allocation. */
    make(2);table=512+2*8;
    const unsigned overlaps[]={528,8192,65535};
    for(unsigned i=0;i<sizeof overlaps/sizeof overlaps[0];i++) {
        unsigned gap=overlaps[i];
        assert(gap+full_size-table<=sizeof copy);
        memcpy(copy,blob,full_size);memcpy(copy+gap,blob+table,full_size-table);
        parts=(SaveJournalParts){copy,copy+gap,table,full_size-table};
        assert(save_journal_validate_parts(&parts,claimed)==SJ_READY);
        memset(&store,0,sizeof store);
        assert(save_journal_publish_parts(&parts,claimed,bounce_persist,&store)==SJ_INVALID);
        assert(!store.calls);stream_checks++;
    }

    /* Cover both address orders and the exact disjoint boundary. A one-byte
     * overlap before prefix is still a valid view if the shared byte is 'S',
     * but publication must reject it before zeroing the commit header. */
    unsigned tail=full_size-table;
    blob[full_size-1]='S';reseal(blob,full_size);
    memcpy(copy,blob+table,tail);memcpy(copy+tail-1,blob,table);
    parts=(SaveJournalParts){copy+tail-1,copy,table,tail};
    assert(save_journal_validate_parts(&parts,claimed)==SJ_READY);
    memset(&store,0,sizeof store);
    assert(save_journal_publish_parts(&parts,claimed,persist,&store)==SJ_INVALID);
    assert(!store.calls);stream_checks++;

    for(unsigned before=0;before<2;before++) {
        uint8_t *prefix=before?copy+tail:copy;
        uint8_t *source=before?copy:copy+SJ_WRITE_BYTES;
        memcpy(source,blob+table,tail);memcpy(prefix,blob,table);
        parts=(SaveJournalParts){prefix,source,table,tail};
        assert(save_journal_validate_parts(&parts,claimed)==SJ_READY);
        memset(&store,0,sizeof store);
        assert(save_journal_publish_parts(&parts,claimed,persist,&store)==SJ_READY);
        assert(store.size==full_size && !memcmp(blob,store.bytes,full_size));
        stream_checks++;
    }
}

int main(void)
{
    validation();build_splits();publish_splits();
    printf("PASS %u streamed validation/publication cases\n",stream_checks);
    return 0;
}
