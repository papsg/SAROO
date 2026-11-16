/* Adapted from paps-g be30cf11; public C/FatFs, synthetic media only. */
#define main startup_test_main
#include "mcu_save_startup.c"
#undef main
#include <stdarg.h>
_Alignas(8) static struct { u8 before[32],image[0x23000],after[32]; } memory;
#define IMGINFO_ADDR ((uintptr_t)memory.image)
static u32 sort_space[1024] __attribute__((unused));
#define TEST_SORT_ADDR ((uintptr_t)sort_space)
#include "sort.inc"
static int category_num,sort_mode;
static char category[32];
static char *get_category(int id) { assert(id>=0 && id<category_num);return category; }
static int injected,reads,opens,closes,allocs,frees;
static int require_catalog_lock;
static unsigned image_clears;
static FRESULT scan_open(DIR *dir,const char *name)
{ assert(!require_catalog_lock || lock_depth==1);opens++;return injected==1?FR_DISK_ERR:f_opendir(dir,name); }
static FRESULT scan_read(DIR *dir,FILINFO *info)
{ assert(!require_catalog_lock || lock_depth==1);reads++;return injected==3 && reads==2?FR_DISK_ERR:f_readdir(dir,info); }
static FRESULT scan_close(DIR *dir)
{ assert(!require_catalog_lock || lock_depth==1);closes++;FRESULT r=f_closedir(dir);return r?r:injected==4?FR_DISK_ERR:FR_OK; }
static void *scan_alloc(size_t size) { allocs++;return injected==2?NULL:malloc(size); }
static void scan_free(void *ptr) { if(ptr)frees++;free(ptr); }
static void *scan_memset(void *out,int byte,size_t size)
{
    uintptr_t a=(uintptr_t)out,b=(uintptr_t)memory.image;
    if(a>=b && a<b+sizeof(memory.image)) image_clears+=size;
    return memset(out,byte,size);
}
static int sprintk(char *out,const char *format,...)
{ va_list args;va_start(args,format);int n=vsprintf(out,format,args);va_end(args);return n; }
#define f_opendir scan_open
#define f_readdir scan_read
#define f_closedir scan_close
#define malloc scan_alloc
#define free scan_free
#define memset scan_memset
#ifdef SAROO_COVER_INTEGRATION
void cover_reset(void);
#else
static void cover_reset(void) {}
#endif
#include "discovery.inc"
#undef f_opendir
#undef f_readdir
#undef f_closedir
#undef malloc
#undef free
#undef memset
static int build_path(int index,char *out)
{
#include "path.inc"
}
/* Connect actual MCU publication to the actual menu consumer. The host's
 * little-endian catalog is explicit; this is not a physical bus test. */
static int binary_reads;
static int le32(const void *address)
{ const u8 *p=address;return (int)((u32)p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24); }
#define LE32(address) le32(address)
static unsigned long binary_number(const char *s,char **end,int base,void *unused)
{ assert(!unused);return strtoul(s,end,base); }
static int read_file(const char *name,int offset,int size,void *out)
{
    binary_reads++;assert(!strcmp(name,"/SAROO/BIN/One.bin") && !offset && !size);
    assert((uintptr_t)out==0x06004000);return -7;
}
static int read_file_bounded(char *name,int offset,int size,void *out,u32 capacity)
{ assert(capacity==0xfc000); return read_file(name,offset,size,out); }
static void pad_suspend(void) { assert(0); }
static void pad_resume(void) { assert(0); }
#define strtoul binary_number
#include "binary.inc"
#undef strtoul
static void reset_scan(void) { injected=reads=opens=closes=allocs=frees=image_clears=0; }
static void create(const char *parent,const char *name,int dir)
{
    char path[1024];assert(snprintf(path,sizeof(path),"%s/%s",parent,name)<(int)sizeof(path));
    if(dir) assert(f_mkdir(path)==0);
    else {FIL fp;assert(f_open(&fp,path,FA_CREATE_NEW|FA_WRITE)==0 && f_close(&fp)==0);}
}
static void guards(void)
{
    for(unsigned i=0;i<32;i++) assert(memory.before[i]==0xa5 && memory.after[i]==0xa5);
}
static void discover(int kind)
{
    const char *parent="/SAROO/ISO";char name[256],out[256];memset(out,0xcc,sizeof(out));
    if(kind==0)create(parent,"README",0);
    if(kind==1) {create(parent,"a.ISO",0);create(parent,"chosen.cUe",0);}
    if(kind==2) {create(parent,"fake.cue",1);create(parent,".hidden.cue",0);create(parent,"real.iso",0);}
    if(kind==3) {create(parent,"first.iso",0);create(parent,"second.ISO",0);}
    if(kind>=4) {
        int n=kind==5?245:244;memset(name,'a',n);memcpy(name+n-4,".iso",5);create(parent,name,0);
    }
    reset_scan();if(kind==6)injected=3; /* ISO candidate followed by a read failure. */
    int result=find_cue_iso((char*)parent,out);
    if(kind==0) assert(result==0 && !out[0]);
    else if(kind==1)assert(result==1 && !strcmp(out,"/SAROO/ISO/chosen.cUe"));
    else if(kind==2)assert(result==2 && !strcmp(out,"/SAROO/ISO/real.iso"));
    else if(kind==3)assert(result==2 && !strcmp(out,"/SAROO/ISO/second.ISO"));
    else if(kind==4)assert(result==2 && strlen(out)==255);
    else assert(result<0 && !out[0]);
    assert(opens==1 && closes==1 && allocs==1 && frees==1);guards();
}
static void failure(const char *mode,int which)
{
    int scan=!strcmp(mode,"scan-fault"),bins=!strcmp(mode,"bins-fault");
    const char *parent=bins?"/SAROO/BIN":"/SAROO/ISO";
    create(parent,scan?"first.iso":"first",!scan&&!bins);
    create(parent,scan?"second.iso":"second",!scan&&!bins);
    total_disc=7;*(u32*)memory.image=7;reset_scan();injected=which;
    char out[256]={0};int r=scan?find_cue_iso((char*)parent,out):bins?list_bins(0):list_disc(0,0);
    assert(r<0);
    if(scan)assert(!out[0]);else assert(!total_disc && !*(u32*)memory.image);
    assert(closes==(which!=1) && frees==(which!=1 && which!=2));guards();
    reset_scan();r=scan?find_cue_iso((char*)parent,out):bins?list_bins(0):list_disc(0,0);
    assert(r==(scan?2:0));if(!scan)assert(total_disc==2 && *(u32*)memory.image==2);
}
static void list_case(int bins,int order)
{
    const char *parent=bins?"/SAROO/BIN":"/SAROO/ISO";
    const char *names[3]={"Zulu","Alpha","Middle"};
    for(int i=0;i<3;i++)create(parent,names[i],!bins);
    create(parent,".hidden",!bins);create(parent,"not-an-entry",bins);
    reset_scan();sort_mode=order;assert((bins?list_bins(1):list_disc(0,1))==0);
    assert(total_disc==3 && *(u32*)memory.image==3);
    const int expected[3][3]={{0,1,2},{1,2,0},{0,2,1}};
    for(int i=0;i<3;i++) {
        const char *wanted=names[expected[bins?0:order][i]];
        assert(!strcmp(path_str+disc_path[i],wanted) && find_disc((char*)wanted)==i);
    }
    assert(find_disc("missing")==-1);guards();
}
static void capacity(int count,int long_names,int order)
{
    char name[256];
    for(int i=0;i<count;i++) {
        snprintf(name,sizeof(name),"G%07d",i);
        if(long_names) {memset(name+8,'x',247);name[255]=0;}
        create("/SAROO/ISO",name,1);
    }
    reset_scan();sort_mode=order;int r=list_disc(0,0);int fits=count<=(long_names?520:2559);
    assert((r==0)==fits && total_disc==(fits?count:0) && *(u32*)memory.image==(u32)(fits?count:0));
    if(fits) {
        for(int i=0;i<count;i++) {
            char expected[9];snprintf(expected,sizeof(expected),"G%07d",order==2?count-1-i:i);
            assert(disc_path[i]>=0x2800 && disc_path[i]<0x23000);
            assert(!memcmp(path_str+disc_path[i],expected,8));
            assert(strlen(path_str+disc_path[i])==(unsigned)(long_names?255:8));
        }
        if(long_names)assert(pbptr==0x23000);
    }
    guards();
}
static void path_case(int value)
{
    char out[256],before[256];memset(out,0xcc,sizeof(out));memcpy(before,out,sizeof(out));
    total_disc=1;disc_path[0]=0x2800;strcpy(path_str+0x2800,"Game");
    category_num=value>=3;strcpy(category,"Category");category_current=0;
    if(value==1 || value==2){memset(path_str+0x2800,'g',value==1?244:245);path_str[0x2800+(value==1?244:245)]=0;}
    if(value==4)category_current=-1;
    if(value==5)category_current=1;
    if(value==6)memset(category,'c',32);
    if(value==7){memset(path_str+0x2800,'g',240);path_str[0x2800+240]=0;}
    int r=build_path(0,out);
    if(value==0)assert(!r && !strcmp(out,"/SAROO/ISO/Game"));
    else if(value==1)assert(!r && strlen(out)==255);
    else if(value==3)assert(!r && !strcmp(out,"/SAROO/ISO/Category/Game"));
    else assert(r<0);
    if(value==2 || value==4 || value==5 || value==6)assert(!memcmp(out,before,sizeof(out)));
    guards();
}
static void reuse(void)
{
    create("/SAROO/ISO","One",1);create("/SAROO/ISO","Two",1);
    reset_scan();assert(list_disc(0,0)==0);unsigned first=image_clears;
    assert(f_unlink("/SAROO/ISO/Two")==0);
    reset_scan();assert(list_disc(0,0)==0 && total_disc==1 && !strcmp(path_str+disc_path[0],"One"));
    fprintf(stderr,"catalog first_clear=%u second_clear=%u cursor=%d\n",first,image_clears,pbptr);
    assert(!first && !image_clears);guards();
}
static void producer_consumer(void)
{
    create("/SAROO/BIN","One.bin",0);create("/SAROO/BIN","Two.bin",0);
    reset_scan();assert(!list_bins(0) && total_disc==2 && !binary_reads);
    int old_second=disc_path[1];assert(!strcmp(path_str+old_second,"Two.bin"));
    assert(f_unlink("/SAROO/BIN/Two.bin")==0);
    reset_scan();assert(!list_bins(0) && total_disc==1);
    if (!image_clears) assert(disc_path[1]==old_second && !strcmp(path_str+old_second,"Two.bin"));
    assert(run_binary(1,1)==-1 && !binary_reads);
    assert(run_binary(0,1)==-7 && binary_reads==1);binary_reads=0;
    reset_scan();injected=3;assert(list_bins(0)<0 && !total_disc && !*(u32*)memory.image);
    assert(run_binary(0,1)==-1 && !binary_reads);
    assert(f_unlink("/SAROO/BIN/One.bin")==0);
    reset_scan();assert(!list_bins(0) && !total_disc && !*(u32*)memory.image);
    assert(run_binary(0,1)==-1 && run_binary(1,1)==-1 && !binary_reads);guards();
}

static int list_folder(const char *path)
{
#ifdef HAS_BIN_DIRECTORY
    require_catalog_lock = 1;
    int result = list_bin_dir(path);
    require_catalog_lock = 0;
    assert(!lock_depth);
    return result;
#else
    (void)path;
    return list_bins(0);
#endif
}

static void folders(void)
{
    const char *root = "/SAROO/BIN", *folder = "/SAROO/BIN/Tools@Demo";
    create(root, "Tools@Demo", 1);
    create(root, "Top.bin", 0);
    create(folder, "Child", 1);
    create(folder, "Café.bin", 0);
    create(folder, ".hidden", 0);

    reset_scan();
    assert(!list_folder(root) && total_disc == 2 && *(u32 *)memory.image == 2);
    assert((u32)disc_path[0] == (0x80000000u | 0x2800));
    assert(!strcmp(path_str + 0x2800, "Tools@Demo"));
    char game[256];
    assert(find_disc("Tools@Demo") == -1 && build_path(0, game) == -1);
    assert(opens == 1 && closes == 1 && allocs == 1 && frees == 1);

    reset_scan();
    assert(!list_folder(folder) && total_disc == 2 && *(u32 *)memory.image == 2);
    assert((u32)disc_path[0] == (0x80000000u | 0x2800));
    assert(!strcmp(path_str + 0x2800, "Child"));
    assert(disc_path[1] == 0x2806 && !strcmp(path_str + disc_path[1], "Café.bin"));
    assert(find_disc("Child") == -1);
    assert(!list_bins(0) && total_disc == 1 && !strcmp(path_str + disc_path[0], "Top.bin"));
    guards();
}

static void folder_paths(void)
{
    static const char *bad[] = {
        "", "/SAROO/ISO", "/SAROO/BIN2", "/saroo/BIN", "/SAROO/BIN/",
        "/SAROO/BIN//Child", "/SAROO/BIN/..", "/SAROO/BIN/./Child",
        "/SAROO/BIN/.hidden", "/SAROO/BIN/A.", "/SAROO/BIN/A /Child",
        "/SAROO/BIN/A\\B", "/SAROO/BIN/A:B", "/SAROO/BIN/A?B",
        "/SAROO/BIN/A\x7f", "/SAROO/BIN/A\x1f"
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        reset_scan();
        assert(list_folder(bad[i]) == -FR_INVALID_NAME);
        assert(!opens && !total_disc && !*(u32 *)memory.image);
    }
    char name[229], path[241];
    memset(name, 'x', sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    create("/SAROO/BIN", name, 1);
    assert(snprintf(path, sizeof(path), "/SAROO/BIN/%s", name) == 239);
    reset_scan();
    assert(!list_folder(path) && opens == 1 && closes == 1 && !total_disc);
    path[239] = 'x';
    path[240] = 0;
    reset_scan();
    assert(list_folder(path) == -FR_INVALID_NAME && !opens);
    guards();
}

static void folder_io(int which)
{
    const char *folder = "/SAROO/BIN/Work";
    create("/SAROO/BIN", "Work", 1);
    create(folder, "One.bin", 0);
    create(folder, "Two.bin", 0);
    assert(!list_folder(folder) && total_disc == 2);
    reset_scan();
    injected = which;
    assert(list_folder(folder) < 0 && !total_disc && !*(u32 *)memory.image);
    assert(opens == 1 && closes == (which != 1));
    assert(frees == (which >= 3));
    reset_scan();
    assert(!list_folder(folder) && total_disc == 2 && *(u32 *)memory.image == 2);
    guards();
}
/* Adapted from paps-g b483124c. Exercise the real sorter and comparator
 * against libc ordering and an independent count of entry identities. */
enum { SORT_MAX_ENTRIES = (0x2800 - 4) / 4 };
static unsigned sort_comparisons;

static int count_path_cmp(const void *a, const void *b)
{
    ++sort_comparisons;
    return path_cmp(a, b);
}

static int reference_path_cmp(const void *a, const void *b)
{
    const char *left = path_str + *(const int *)a;
    const char *right = path_str + *(const int *)b;
    return sort_mode == 1 ? strcmp(left, right) : strcmp(right, left);
}

static void sort_sample(unsigned count, unsigned pattern, unsigned prefix, int bounded)
{
    int reference[SORT_MAX_ENTRIES];
    u8 seen[SORT_MAX_ENTRIES] = {0};
    unsigned stride = prefix + 9;
    assert(count <= SORT_MAX_ENTRIES && 0x2800 + count * stride <= sizeof memory.image);
    for (unsigned i = 0; i < count; ++i) {
        unsigned key = i;
        if (pattern == 1) key = count - 1 - i;
        if (pattern == 2) key = 0;
        if (pattern == 3) key = i % 7;
        unsigned offset = 0x2800 + i * stride;
        disc_path[i] = (int)offset;
        memset(path_str + offset, 'x', prefix);
        assert(snprintf(path_str + offset + prefix, 9, "G%07u", key) == 8);
    }
    if (pattern == 4) {
        u32 state = 1;
        for (unsigned i = count; i > 1; --i) {
            state = state * 1664525u + 1013904223u;
            unsigned j = state % i;
            int temp = disc_path[i - 1];
            disc_path[i - 1] = disc_path[j];
            disc_path[j] = temp;
        }
    }
    memcpy(reference, disc_path, count * sizeof *reference);
    qsort(reference, count, sizeof *reference, reference_path_cmp);
    sort_comparisons = 0;
    qsort4(disc_path, count, count_path_cmp);
    for (unsigned i = 0; i < count; ++i) {
        unsigned offset = (unsigned)disc_path[i];
        assert(offset >= 0x2800 && (offset - 0x2800) % stride == 0);
        unsigned entry = (offset - 0x2800) / stride;
        assert(entry < count && !seen[entry]++);
        assert(reference_path_cmp(reference + i, disc_path + i) == 0);
    }
    guards();
    if (bounded) {
        unsigned levels = 0;
        for (unsigned size = 1; size < count; size *= 2)
            ++levels;
        unsigned limit = 2 * count * (levels + 1);
        fprintf(stderr, "catalog count=%u shape=%u order=%d comparisons=%u limit=%u\n",
                count, pattern, sort_mode, sort_comparisons, limit);
        assert(sort_comparisons <= limit);
    }
}

static void sort_controls(void)
{
    const unsigned sizes[] = {0, 1, 2, 3, 15, 64, 127, 520, SORT_MAX_ENTRIES};
    for (sort_mode = 1; sort_mode <= 2; ++sort_mode) {
        for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; ++i)
            for (unsigned pattern = 0; pattern < 5; ++pattern)
                sort_sample(sizes[i], pattern, 0, 0);
        for (unsigned pattern = 0; pattern < 5; ++pattern)
            sort_sample(520, pattern, 247, 0);
    }
}

/* SSR-097 tests adapted from paps-g b6b04d6d. */
static void create_sfn_only(const char *name)
{
    /* Author one valid CP437 8.3 record, with no adjacent LFN. The file and
     * allocation are produced by FatFs; only the eight OEM name bytes change. */
    FIL fp;
    assert(f_open(&fp, "/SAROO/ISO/ABCDEFGH.ISO", FA_CREATE_NEW | FA_WRITE) == FR_OK);
    FATFS *fs = fp.obj.fs;
    LBA_t sector = fp.dir_sect;
    size_t offset = fp.dir_ptr - fs->win;
    assert(offset <= 512 - 32);
    UINT written; BYTE marker = 0x5a, data[512];
    assert(f_write(&fp, &marker, 1, &written) == FR_OK && written == 1);
    assert(f_close(&fp) == FR_OK);
    assert(disk_read(0, data, sector, 1) == RES_OK);
    assert(!memcmp(data + offset, "ABCDEFGHISO", 11));
    assert(offset < 32 || data[offset - 32 + 11] != 0x0f);
    memset(data + offset, 0x90, 8);
    assert(disk_write(0, data, sector, 1) == RES_OK);
    assert(disk_ioctl(0, CTRL_SYNC, NULL) == RES_OK);
    assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(fs, "0:", 1) == FR_OK);
    char path[64];
    assert(snprintf(path, sizeof(path), "/SAROO/ISO/%s", name) < (int)sizeof(path));
    assert(f_open(&fp, path, FA_READ) == FR_OK);
    data[0] = 0; written = 0;
    assert(f_read(&fp, data, 1, &written) == FR_OK && written == 1 && data[0] == marker);
    assert(f_close(&fp) == FR_OK);
}
static void utf8_name(int kind)
{
    char name[1024], dir_name[1024], expected[1024], out[256], actual_alt[64] = {0};
    size_t used = 0;
    if (kind == 0 || kind == 1) {
        int count = kind == 0 ? 100 : 130;
        for (int i = 0; i < count; ++i) { name[used++] = (char)0xc3; name[used++] = (char)0xa9; }
        memcpy(name + used, ".iso", 5); used += 4;
    } else if (kind == 2) {
        for (int i = 0; i < 251; ++i) { name[used++] = (char)0xe0; name[used++] = (char)0xa0; name[used++] = (char)0x80; }
        memcpy(name + used, ".iso", 5); used += 4;
    } else if (kind == 4) {
        for (int i = 0; i < 255; ++i) { name[used++] = (char)0xe0; name[used++] = (char)0xa0; name[used++] = (char)0x80; }
    } else {
        for (int i = 0; i < 8; ++i) { name[used++] = (char)0xc3; name[used++] = (char)0x89; }
        memcpy(name + used, ".ISO", 5); used += 4;
    }
    name[used] = 0;
    if (kind == 3) create_sfn_only(name);
    else create(kind == 4 ? "/SAROO/BIN" : "/SAROO/ISO", name, 0);
    strcpy(dir_name, name);
    if (kind < 3) dir_name[strlen(dir_name) - 4] = 0;
    if (kind != 3) create("/SAROO/ISO", dir_name, 1);
    DIR dir; FILINFO info;
    const char *parent = kind == 4 ? "/SAROO/BIN" : "/SAROO/ISO";
    assert(f_opendir(&dir, parent) == FR_OK);
    int found = 0;
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0])
        if (!strcmp(info.fname, name)) { found = 1; strcpy(actual_alt, info.altname); }
    fprintf(stderr, "utf8 kind=%d expected_bytes=%zu found=%d FILINFO=%zu LFN=%zu SFN=%zu\n",
            kind, used, found, sizeof(info), sizeof(info.fname), sizeof(info.altname));
    assert(found);
    assert(f_closedir(&dir) == FR_OK);
    if (kind == 3) assert(!actual_alt[0]); /* No LFN or case-only alternate. */
    assert(snprintf(expected, sizeof(expected), "%s/%s", parent, name) < (int)sizeof(expected));
    memset(out, 0xcc, sizeof(out));
    int result = find_cue_iso((char *)parent, out);
    if (kind == 0 || kind == 3) assert(result == 2 && !strcmp(out, expected));
    else if (kind == 4) assert(result == 0 && !out[0]);
    else assert(result == -FR_INVALID_NAME && !out[0]);
    if (kind != 3) {
        reset_scan();
        assert(list_disc(0, 0) == FR_OK && total_disc == 1);
        assert(!strcmp(path_str + disc_path[0], dir_name));
        if (kind > 0) assert(build_path(0, out) < 0);
    }
    guards();
}
/* Independent boundary corpus: bytes and UTF-16 units are different budgets. */
static void utf8_identity(int kind)
{
    char name[766], path[1024];
    unsigned used = 0;
    if (!kind) {
        memcpy(name, "Mixed-ascii.bin", 16); used = 15;
    } else if (kind == 1) {
        name[used++] = 'A';
        for (unsigned i = 0; i < 127; ++i) { name[used++] = (char)0xc3; name[used++] = (char)0xa9; }
    } else if (kind == 2) {
        for (unsigned i = 0; i < 125; ++i) {
            name[used++] = (char)0xf0; name[used++] = (char)0x90;
            name[used++] = (char)0x80; name[used++] = (char)0x80;
        }
        memcpy(name + used, "abcde", 5); used += 5;
    } else {
        assert(kind == 3);
        for (unsigned i = 0; i < 11; ++i) {
            if (i == 8) name[used++] = '.';
            name[used++] = (char)0xe2; name[used++] = (char)0x94; name[used++] = (char)0x82;
        }
    }
    name[used] = 0;
    assert(snprintf(path, sizeof(path), "/SAROO/BIN/%s", kind == 3 ? "ABCDEFGH.XYZ" : name) < (int)sizeof(path));
    FIL fp; UINT count; BYTE payload = 0x5a, sector[512];
    assert(!f_open(&fp, path, FA_CREATE_NEW | FA_WRITE));
    FATFS *fs = fp.obj.fs;
    LBA_t where = fp.dir_sect;
    unsigned offset = (unsigned)(fp.dir_ptr - fs->win);
    assert(!f_write(&fp, &payload, 1, &count) && count == 1 && !f_close(&fp));
    if (kind == 3) {
        assert(offset <= 480 && disk_read(0, sector, where, 1) == RES_OK);
        assert(!memcmp(sector + offset, "ABCDEFGHXYZ", 11));
        assert(offset < 32 || sector[offset - 32 + 11] != 0x0f);
        memset(sector + offset, 0xb3, 11);
        assert(disk_write(0, sector, where, 1) == RES_OK && disk_ioctl(0, CTRL_SYNC, NULL) == RES_OK);
    }
    assert(!f_mount(NULL, "0:", 0) && !f_mount(fs, "0:", 1));
    assert(snprintf(path, sizeof(path), "/SAROO/BIN/%s", name) < (int)sizeof(path));
    assert(!f_open(&fp, path, FA_READ) && !f_read(&fp, sector, 1, &count));
    assert(count == 1 && sector[0] == payload && !f_close(&fp));
    reset_scan();
    assert(!list_bins(0) && total_disc == 1);
    fprintf(stderr, "identity kind=%d bytes=%u FILINFO=%zu LFN=%zu SFN=%zu\n",
            kind, used, sizeof(FILINFO), sizeof(((FILINFO *)0)->fname), sizeof(((FILINFO *)0)->altname));
    assert(!strcmp(path_str + disc_path[0], name));
    assert(allocs == 1 && frees == 1); guards();
}

static void maximum_leaf(char *name, unsigned index)
{
    for (unsigned i = 0; i < 255; ++i) {
        name[3*i] = (char)0xe0; name[3*i+1] = (char)0xa0; name[3*i+2] = (char)0x80;
    }
    name[763] = (char)(0xa0 + index / 64);
    name[764] = (char)(0x80 + index % 64);
    name[765] = 0;
}

static void utf8_capacity(void)
{
    char name[766], path[1024];
    for (unsigned i = 0; i < 174; ++i) {
        maximum_leaf(name, i); create("/SAROO/BIN", name, 0);
        if (i != 172 && i != 173) continue;
        reset_scan();
        int result = list_bins(0);
        if (i == 172) {
            assert(!result && total_disc == 173 && 0x23000u - pbptr == 602);
            for (unsigned j = 0; j < 173; ++j) {
                maximum_leaf(name, j);
                assert(!strcmp(path_str + disc_path[j], name));
            }
        } else assert(result == -FR_NOT_ENOUGH_CORE && !total_disc && !*(u32 *)memory.image);
        assert(allocs == 1 && frees == 1); guards();
    }
    maximum_leaf(name, 173);
    assert(snprintf(path, sizeof(path), "/SAROO/BIN/%s", name) < (int)sizeof(path));
    assert(!f_unlink(path)); reset_scan();
    assert(!list_bins(0) && total_disc == 173 && allocs == 1 && frees == 1);
    guards();
}

#ifndef MCU_DISCOVERY_TEST_ENTRY
#define MCU_DISCOVERY_TEST_ENTRY main
#endif
int MCU_DISCOVERY_TEST_ENTRY(int argc,char **argv)
{
    assert(argc==4);const char *mode=argv[2];int value=atoi(argv[3]);
    disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(f_mount(&fs,"0:",1)==0);
    assert(f_mkdir("/SAROO")==0 && f_mkdir("/SAROO/ISO")==0 && f_mkdir("/SAROO/BIN")==0);
    memset(&memory,0xa5,sizeof(memory));fault="none";
    if(!strcmp(mode,"utf8"))utf8_name(value);
    else if(!strcmp(mode,"identity"))utf8_identity(value);
    else if(!strcmp(mode,"utf8-capacity"))utf8_capacity();
    else if(!strcmp(mode,"discover"))discover(value);
    else if(!strcmp(mode,"folders"))folders();
    else if(!strcmp(mode,"folder-paths"))folder_paths();
    else if(!strcmp(mode,"folder-io"))folder_io(value);
    else if(strstr(mode,"fault"))failure(mode,value);
    else if(!strcmp(mode,"list") || !strcmp(mode,"bins"))list_case(!strcmp(mode,"bins"),value);
    else if(strstr(mode,"capacity"))capacity(value,!strcmp(mode,"name-capacity"),!strcmp(mode,"sorted-capacity")?1:!strcmp(mode,"reverse-capacity")?2:0);
    else if(!strcmp(mode,"path"))path_case(value);
    else if(!strcmp(mode,"producer-consumer"))producer_consumer();
    else if (!strcmp(mode, "sort-controls")) sort_controls();
    else if (!strcmp(mode, "sort-bound")) {
        for (sort_mode = 1; sort_mode <= 2; ++sort_mode)
            sort_sample(SORT_MAX_ENTRIES, value, 0, 1);
    }
    else if(!strcmp(mode,"category")) {
        category_num=1;strcpy(category,"Category");assert(list_disc(value,0)<0 && !opens && !total_disc && !*(u32*)memory.image);
    } else {assert(!strcmp(mode,"reuse"));reuse();}
    printf("discovery %s value=%d entries=%d reads=%d opens=%d closes=%d clear_bytes=%u\n",mode,value,total_disc,reads,opens,closes,image_clears);
    assert(!fclose(disk));return 0;
}
