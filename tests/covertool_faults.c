#define COVER_TEST_IMPLEMENTATION
#include "covertool_faults.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

static unsigned fault_hits, triggered, read_calls, write_calls, allocations, probes;
static size_t read_bytes, write_bytes, live_bytes, peak_bytes;
static unsigned char temporary[1024];
static struct { void *p;size_t size; } live[64];
void cover_test_probe(void) { probes++; }

static int fail(const char *operation)
{
    const char *value=getenv("SAROO_COVER_FAULT");
    if(!value)return 0;
    size_t n=strlen(operation);
    if(strncmp(value,operation,n) || (value[n] && value[n]!=':'))return 0;
    unsigned nth=value[n]? (unsigned)strtoul(value+n+1,NULL,10):1;
    if(++fault_hits!=nth)return 0;
    triggered++;errno=EIO;return 1;
}
static void *record(void *p,size_t size)
{
    if(!p)return p;
    for(unsigned i=0;i<64;i++)if(!live[i].p) {
        live[i].p=p;live[i].size=size;live_bytes+=size;allocations++;
        if(live_bytes>peak_bytes)peak_bytes=live_bytes;
        return p;
    }
    abort();
}
void *cover_test_calloc(size_t n,size_t size)
{ if(fail("alloc")){errno=ENOMEM;return NULL;}assert(!size || n<=SIZE_MAX/size);return record(calloc(n,size),n*size); }
void *cover_test_malloc(size_t size)
{ if(fail("alloc")){errno=ENOMEM;return NULL;}return record(malloc(size),size); }
void cover_test_free(void *p)
{
    if(p)for(unsigned i=0;i<64;i++)if(live[i].p==p){live_bytes-=live[i].size;live[i].p=NULL;break;}
    free(p);
}
int cover_test_mkstemp(char *name)
{
    if(fail("create"))return -1;
    int fd=mkstemp(name);if(fd>=0){assert(fd<1024);temporary[fd]=1;}return fd;
}
ssize_t cover_test_pread(int fd,void *out,size_t size,off_t offset)
{
    read_calls++;
    if(fail("read-error"))return -1;
    if(fail("read-zero"))return 0;
    if(fail("read-intr")){errno=EINTR;return -1;}
    if(size>1 && fail("read-short"))size/=2;
    ssize_t result=pread(fd,out,size,offset);if(result>0)read_bytes+=(size_t)result;return result;
}
ssize_t cover_test_pwrite(int fd,const void *data,size_t size,off_t offset)
{
    write_calls++;
    if(fail("write-error"))return -1;
    if(fail("write-zero"))return 0;
    if(fail("write-intr")){errno=EINTR;return -1;}
    if(size>1 && fail("write-short"))size/=2;
    ssize_t result=pwrite(fd,data,size,offset);if(result>0)write_bytes+=(size_t)result;return result;
}
int cover_test_close(int fd)
{
    int temp=fd>=0 && fd<1024 && temporary[fd];
    if(temp)temporary[fd]=0;
    int result=close(fd);
    if(fail(temp?"close-temp":"close-input"))return -1;
    return result;
}
int cover_test_fclose(FILE *fp)
{ int result=fclose(fp);if(fail("fclose"))return EOF;return result; }
int cover_test_fsync(int fd)
{
    struct stat st;assert(!fstat(fd,&st));
    if(fail(S_ISDIR(st.st_mode)?"sync-directory":"sync-file"))return -1;
    return fsync(fd);
}
int cover_test_rename(const char *a,const char *b)
{ if(fail("rename"))return -1;return rename(a,b); }
int cover_test_ftruncate(int fd,off_t length)
{ if(fail("truncate"))return -1;return ftruncate(fd,length); }
#ifdef __APPLE__
int cover_test_fcopyfile(int a,int b,copyfile_state_t state,copyfile_flags_t flags)
{ if(fail("metadata"))return -1;return fcopyfile(a,b,state,flags); }
#endif
__attribute__((destructor)) static void report(void)
{
    const char *path=getenv("SAROO_COVER_STATS");
    if(!path)return;
    FILE *fp=fopen(path,"w");assert(fp);
    fprintf(fp,"{\"triggered\":%u,\"allocations\":%u,\"heap_peak\":%zu,\"heap_live\":%zu,"
               "\"read_calls\":%u,\"read_bytes\":%zu,\"write_calls\":%u,\"write_bytes\":%zu,\"probes\":%u}\n",
            triggered,allocations,peak_bytes,live_bytes,read_calls,read_bytes,write_calls,write_bytes,probes);
    assert(!fclose(fp));
}
