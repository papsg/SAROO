/* paps-g 1326a62c/31151513 (SSR-116); public adaptation by papsg. */
/* Actual complete FatFs provider. Only file-backed disk/mutex services are
 * substituted; direct guards and public f_printf exercise the same ftoa. */
#include "disk.inc"
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include "ff.c"

_Static_assert(SZ_NUM_BUF == 32, "Reconcile the documented numeric output limit");
static unsigned checks, errors;
static unsigned guard_errors, content_errors;
static const double integer_values[]={0.0,1.0,-1.0,10.0,-10.0,100.0,-100.0};

static void ascii(TCHAR *out,const char *in) {
    do {*out++=(unsigned char)*in;} while(*in++);
}
static void known(char *out,char kind,int precision,double value) {
    bool negative=value<0;
    unsigned magnitude=(unsigned)(negative?-value:value);
    char whole[8];snprintf(whole,sizeof whole,"%u",magnitude);
    unsigned fixed=(kind=='f'?strlen(whole):5)+(precision>0)+negative;
    if((unsigned)precision+fixed>31) {
        strcpy(out,negative?"-OV":"+OV");return;
    }
    if(negative)*out++='-';
    if(kind=='f') {strcpy(out,whole);out+=strlen(whole);}
    else *out++=value==0?'0':'1';
    if(precision) {
        *out++=FF_PRINT_FLOAT==2?',':'.';
        for(int n=0;n<precision;n++)*out++='0';
    }
    if(kind!='f') {*out++=kind;*out++='+';*out++='0';*out++=magnitude==100?'2':magnitude==10?'1':'0';}
    *out=0;
}
static void observe(bool valid,const char *kind,int precision,double value,
                    const char *got,const char *want,unsigned guards) {
    checks++;
    if(!valid) {
        if(errors<8)fprintf(stderr,"%s precision=%d value=%g guards=%u got=%s want=%s\n",
                            kind,precision,value,guards,got,want);
        errors++;
    }
}
static void direct(void) {
    for(unsigned k=0;k<3;k++)for(int precision=0;precision<=99;precision++)
      for(unsigned v=0;v<sizeof integer_values/sizeof *integer_values;v++) {
        char storage[2+128],want[128];memset(storage,0x5a,sizeof storage);
        const char kind="eEf"[k];
        ftoa(storage+2,integer_values[v],precision,(TCHAR)kind);
        unsigned changed=0;
        for(unsigned n=0;n<sizeof storage;n++)
            if((n<2 || n>=2+SZ_NUM_BUF) && (unsigned char)storage[n]!=0x5a)changed++;
        known(want,kind,precision,integer_values[v]);
        bool same=!strcmp(storage+2,want);
        guard_errors+=!!changed;content_errors+=!same;
        char label[]={kind,0};observe(!changed && same,label,precision,integer_values[v],storage+2,want,changed);
      }
}
static void start(const char *image) {
    disk=fopen(image,"r+b");assert(disk && !fseeko(disk,0,SEEK_END));
    sectors=ftello(disk)/512;
    BYTE work[32768];MKFS_PARM options={.fmt=FM_FAT32|FM_SFD,.n_fat=1,.au_size=512};
    TCHAR root[4];ascii(root,"0:");
    assert(!f_mkfs(root,&options,work,sizeof work));
}
static void public_case(const char *format,double value,const char *want) {
    FIL file;TCHAR path[16],fmt[80];ascii(path,"/RESULT.TXT");ascii(fmt,format);
    assert(!f_open(&file,path,FA_CREATE_ALWAYS|FA_WRITE));
    int written=f_printf(&file,fmt,value);
    assert(!f_close(&file) && !f_open(&file,path,FA_READ));
    char got[256]={0};UINT count;
    assert(!f_read(&file,got,sizeof got-1,&count) && !f_close(&file));
    bool valid=written==(int)strlen(want) && count==strlen(want) && !strcmp(got,want);
    observe(valid,format,-1,value,got,want,0);
}
static void extremes(bool file) {
    static const struct {double value;int precision;char kind;const char *want;} cases[]={
        {DBL_MAX,0,'e',"+OV"},{-DBL_MAX,0,'E',"-OV"},
        {DBL_MAX,6,'E',"+OV"},{-DBL_MAX,0,'f',"-OV"},
        {1e99,6,'e',"1.000000e+99"},{-1e99,6,'E',"-1.000000E+99"},
        {9.99e99,0,'e',"+OV"},{-9.99e99,0,'E',"-OV"}
    };
    if(!file) {puts("extreme-formatter-entry");fflush(stdout);}
    for(unsigned n=0;n<sizeof cases/sizeof *cases;n++) {
        char format[24],want[64];snprintf(format,sizeof format,"%%.%d%c",cases[n].precision,cases[n].kind);
        strcpy(want,cases[n].want);
        if(FF_PRINT_FLOAT==2)for(char *q=want;*q;q++)if(*q=='.')*q=',';
        if(file)public_case(format,cases[n].value,want);
        else {
            char buffer[SZ_NUM_BUF];
            ftoa(buffer,cases[n].value,cases[n].precision,(TCHAR)cases[n].kind);
            observe(!strcmp(buffer,want),format,cases[n].precision,
                    cases[n].value,buffer,want,0);
        }
    }
}
static void public_matrix(void) {
    for(unsigned k=0;k<3;k++)for(int precision=0;precision<=99;precision++)
      for(unsigned v=0;v<sizeof integer_values/sizeof *integer_values;v++) {
        char format[24],want[128];snprintf(format,sizeof format,"%%.%d%c",precision,"eEf"[k]);
        known(want,"eEf"[k],precision,integer_values[v]);public_case(format,integer_values[v],want);
      }
    public_case("[%8.99e]",0,"[     +OV]");
    extremes(true);
}
static void fits(void) {
    for(unsigned k=0;k<3;k++)for(unsigned v=0;v<sizeof integer_values/sizeof *integer_values;v++) {
        double value=integer_values[v];unsigned magnitude=(unsigned)(value<0?-value:value);
        unsigned digits=magnitude==100?3:magnitude==10?2:1;
        int precision="eEf"[k]=='f'?30-digits-(value<0):25-(value<0);
        char format[24],want[128];snprintf(format,sizeof format,"%%.%d%c",precision,"eEf"[k]);
        known(want,"eEf"[k],precision,value);assert(strlen(want)==31);
        public_case(format,value,want);
    }
}
static void controls(void) {
    const double values[]={0.0,-0.0,1.0,-1.0,12.125,-12.125,9.5,-9.5,9.99,-9.99};
    const int precisions[]={0,1,6};
    for(unsigned k=0;k<3;k++)for(unsigned p=0;p<3;p++)for(unsigned v=0;v<10;v++) {
        char format[24],want[128];snprintf(format,sizeof format,"%%.%d%c",precisions[p],"eEf"[k]);
        /* R0.15 treats negative zero as zero; preserve that existing behavior. */
        snprintf(want,sizeof want,format,values[v]==0?0.0:values[v]);
        if(FF_PRINT_FLOAT==2)for(char *q=want;*q;q++)if(*q=='.')*q=',';
        public_case(format,values[v],want);
    }
    for(unsigned k=0;k<3;k++) {
        char format[24];snprintf(format,sizeof format,"%%.99%c","eEf"[k]);
        public_case(format,INFINITY,"+INF");public_case(format,-INFINITY,"-INF");
        public_case(format,NAN,"NaN");
    }
    public_case("%.6e",1e100,"+OV");public_case("%.6E",-1e100,"-OV");
    public_case("[%8.99e]",1,"[     +OV]");
    public_case("[%-8.99E]",-1,"[-OV     ]");
    public_case("%e",0,FF_PRINT_FLOAT==2?"0,000000e+00":"0.000000e+00");
    public_case("%f",1,FF_PRINT_FLOAT==2?"1,000000":"1.000000");
    public_case("%.0e\n",0,FF_USE_STRFUNC==2?"0e+00\r\n":"0e+00\n");
}
int main(int argc,char **argv) {
    assert(argc==3);const char *mode=argv[2];
    if(!strcmp(mode,"direct"))direct();
    else if(!strcmp(mode,"extreme"))extremes(false);
    else {
        start(argv[1]);FATFS fs={0};TCHAR root[4];ascii(root,"0:");
        assert(!f_mount(&fs,root,1));
        if(!strcmp(mode,"public"))public_matrix();
        else if(!strcmp(mode,"fits"))fits();
        else if(!strcmp(mode,"controls"))controls();
        else {
            assert(!strcmp(mode,"lower26") || !strcmp(mode,"lower99") || !strcmp(mode,"upper99"));
            public_case(!strcmp(mode,"lower26")?"%.26e":!strcmp(mode,"lower99")?"%.99e":"%.99E",0,"+OV");
        }
        assert(!f_mount(NULL,root,0) && !fclose(disk));
    }
    assert(checks);
    printf("mode=%s checks=%u errors=%u guard_errors=%u content_errors=%u float=%d strings=%d\n",mode,checks,errors,guard_errors,content_errors,FF_PRINT_FLOAT,FF_USE_STRFUNC);
    return errors?42:0;
}
