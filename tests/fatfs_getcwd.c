/* paps-g 097ad56b/6e9c9e63 (SSR-115); public adaptation by papsg. */
/* Real selected FatFs with a synthetic file-backed disk and host mutexes.
 * Expected paths/capacities are authored here, independently of f_getcwd. */
#include "disk.inc"
#include <stdbool.h>

static unsigned checks;

static void ascii(TCHAR *out,const char *in) {
    do {*out++=(unsigned char)*in;} while(*in++);
}
static unsigned units(const TCHAR *s) {unsigned n=0;while(s[n])n++;return n;}
static void append(TCHAR *out,const TCHAR *tail) {
    out+=units(out);do {*out++=*tail;} while(*tail++);
}
static void expected(TCHAR *out,const TCHAR *tail) {
#if FF_VOLUMES < 2
    out[0]=0;
#elif FF_STR_VOLUME_ID == 0
    ascii(out,"0:");
#elif FF_STR_VOLUME_ID == 1
    ascii(out,"RAM:");
#else
    ascii(out,"/RAM");
#endif
    append(out,tail);
}
static void input_path(TCHAR *out,const TCHAR *tail) {
#if FF_STR_VOLUME_ID == 2
    ascii(out,"/RAM");
#else
    ascii(out,"0:");
#endif
    append(out,tail);
}
static bool probe(const TCHAR *want,unsigned capacity,bool heap) {
    unsigned required=units(want)+1;
    unsigned padding=heap?0:2;
    unsigned allocated=capacity+2*padding;
    TCHAR *storage=malloc((allocated?allocated:1)*sizeof(TCHAR));assert(storage);
    for(unsigned n=0;n<(allocated?allocated:1);n++)storage[n]=(TCHAR)0x5a;
    TCHAR *buffer=storage+padding;
    FRESULT result=f_getcwd(buffer,capacity);
    bool valid=capacity>=required?result==FR_OK:result==FR_NOT_ENOUGH_CORE;
    if(capacity>=required)valid=valid && !memcmp(buffer,want,required*sizeof(TCHAR));
    unsigned changed_guards=0;
    if(!heap)for(unsigned n=0;n<padding;n++) {
        changed_guards+=storage[n]!=(TCHAR)0x5a;
        changed_guards+=storage[padding+capacity+n]!=(TCHAR)0x5a;
    }
    valid=valid && !changed_guards;
    if(!valid)fprintf(stderr,"capacity=%u required=%u result=%d width=%zu heap=%d changed_guard_units=%u\n",
                      capacity,required,result,sizeof(TCHAR),heap,changed_guards);
    free(storage);checks++;return valid;
}
int main(int argc,char **argv) {
    assert(argc==4);
    const char *mode=argv[2];
    bool exfat=!strcmp(mode,"exfat");
    disk=fopen(argv[1],"r+b");assert(disk && !fseeko(disk,0,SEEK_END));
    sectors=ftello(disk)/512;
    BYTE work[32768];
    TCHAR root[96],tail[96],input[96],want[96];
    ascii(tail,"/");input_path(root,tail);
    MKFS_PARM options={.fmt=(exfat?FM_EXFAT:FM_FAT32)|FM_SFD,.n_fat=1,
                       .au_size=exfat?65536:512};
    assert(!f_mkfs(root,&options,work,sizeof work));
    FATFS fs={0};assert(!f_mount(&fs,root,1));
    assert(fs.fs_type==(exfat?FS_EXFAT:FS_FAT32));
    bool valid=true;
    for(unsigned place=0;place<5;place++) {
        if(place==0)ascii(tail,"/");
        if(place==1)ascii(tail,"/AB");
        if(place==2)ascii(tail,"/AB/CDE");
        if(place==3) {
#if FF_USE_LFN
            ascii(tail,"/Long folder");
#else
            ascii(tail,"/FOLDER");
#endif
        }
        if(place==4) {
#if FF_LFN_UNICODE == 0
            ascii(tail,"/ASCII");
#elif FF_LFN_UNICODE == 2
            ascii(tail,"/Caf\xc3\xa9");
#else
            ascii(tail,"/Caf");tail[4]=0xe9;tail[5]=0;
#endif
        }
        input_path(input,tail);
        if(place)assert(!f_mkdir(input));
        assert(!f_chdir(input));
        if(exfat)ascii(tail,"/"); /* R0.15 explicitly returns root on exFAT. */
        expected(want,tail);
        unsigned required=units(want)+1;
        if(!strcmp(mode,"valid") || exfat)valid=probe(want,required+4,false) && valid;
        if(!strcmp(mode,"exact") || exfat)valid=probe(want,required,false) && valid;
        if(!strcmp(mode,"short"))valid=probe(want,required-1,false) && valid;
        if(!strcmp(mode,"minimum")) {
            valid=probe(want,0,false) && valid;
            valid=probe(want,1,false) && valid;
        }
        if(!strcmp(mode,"heap"))valid=probe(want,required-1,true) && valid;
    }
    assert(!f_mount(NULL,root,0) && !fclose(disk));
    printf("mode=%s provider=%s width=%zu checks=%u valid=%d\n",mode,argv[3],sizeof(TCHAR),checks,valid);
    return valid?0:42;
}
