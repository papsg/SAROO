//
// SEGA SATURN SAROO COVER TOOL - by Bucanero (github.com/bucanero)
//

/*
    Copyright (c) 2025 Bucanero <github.com/bucanero>

    SAROO COVER TOOL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SAROO COVER TOOL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    apayloadlong with SAROO COVER TOOL.  If not, see <http://www.gnu.org/licenses/>.

*/

/* Safety maintenance adapted from paps-g 2231f4b5; Bucanero notice retained. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <copyfile.h>
#endif

#define MAX_COVERS 16384u
#define INDEX_BYTES (MAX_COVERS * 32u)
#define MAX_IMAGE (1024u + 128u * 192u)
/* Room for a complete maximum image with a 124-byte Windows DIB header. */
static uint8_t image[MAX_IMAGE + 138];
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static unsigned le16(const uint8_t *p) { return p[0] | (unsigned)p[1]<<8; }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i)); }
static void put16(uint8_t *p,unsigned v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
static int invalid(void) { errno=EINVAL;return -1; }
static int input(const char *name,off_t *length)
{
    int fd=open(name,O_RDONLY|O_NONBLOCK);struct stat st;
    if(fd<0)return -1;
    if(fstat(fd,&st)<0 || !S_ISREG(st.st_mode)) { close(fd);return invalid(); }
    *length=st.st_size;return fd;
}
static int read_at(int fd,void *buffer,size_t size,off_t offset)
{
    uint8_t *p=buffer;
    while(size) {
        ssize_t n=pread(fd,p,size,offset);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){if(!n)errno=EIO;return -1;}
        p+=n;size-=(size_t)n;offset+=n;
    }
    return 0;
}
static int write_at(int fd,const void *buffer,size_t size,off_t offset)
{
    const uint8_t *p=buffer;
    while(size) {
        ssize_t n=pwrite(fd,p,size,offset);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){if(!n)errno=EIO;return -1;}
        p+=n;size-=(size_t)n;offset+=n;
    }
    return 0;
}
struct stage { int fd;char temporary[32];const char *name; };
static void discard(struct stage *s)
{
    int saved=errno;
    if(s->fd>=0)close(s->fd);
    if(s->temporary[0])unlink(s->temporary);
    s->fd=-1;s->temporary[0]=0;errno=saved;
}
static int begin(struct stage *s,const char *name,int source)
{
    struct stat st;int old=-1;
    *s=(struct stage){.fd=-1,.name=name};
    if(lstat(name,&st)==0) {
        struct stat input_st;
        if(!S_ISREG(st.st_mode) || fstat(source,&input_st)<0 ||
           (st.st_dev==input_st.st_dev && st.st_ino==input_st.st_ino))return invalid();
        old=open(name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
        if(old<0)return -1;
        if(fstat(old,&st)<0 || !S_ISREG(st.st_mode)){close(old);return invalid();}
#ifdef __APPLE__
        /* Do not copy deletion-blocking flags onto an owned temporary. */
        if(st.st_flags & (UF_IMMUTABLE|UF_APPEND|SF_IMMUTABLE|SF_APPEND)) {
            close(old);errno=EPERM;return -1;
        }
#endif
    } else if(errno!=ENOENT)return -1;
    strcpy(s->temporary,".saroo-cover-XXXXXX");s->fd=mkstemp(s->temporary);
    if(s->fd<0){s->temporary[0]=0;if(old>=0)close(old);return -1;}
    int result=0;
    if(old>=0) {
        struct stat fresh;
        if(fstat(s->fd,&fresh)<0 ||
           ((st.st_uid!=fresh.st_uid || st.st_gid!=fresh.st_gid) && fchown(s->fd,st.st_uid,st.st_gid)<0))result=-1;
#ifdef __APPLE__
        if(!result && fcopyfile(old,s->fd,NULL,COPYFILE_METADATA)<0)result=-1;
#else
        if(!result && fchmod(s->fd,st.st_mode&07777)<0)result=-1;
#endif
        if(close(old)<0)result=-1;
    }
    if(result)discard(s);
    return result;
}
static int finish(struct stage *s)
{
    if(fsync(s->fd)<0)return -1;
    int result=close(s->fd);s->fd=-1;
    if(result<0)return -1;
    if(rename(s->temporary,s->name)<0)return -1;
    s->temporary[0]=0;
    int parent=open(".",O_RDONLY|O_DIRECTORY);
    result=parent<0?-1:fsync(parent);
    if(parent>=0 && close(parent)<0)result=-1;
    if(result)fprintf(stderr,"Replaced %s, but directory synchronization failed.\n",s->name);
    return result;
}
static uint32_t adler32(const uint8_t *p,size_t size)
{
    uint32_t a=1,b=0;
    /* Modulo once per bounded block, instead of twice per byte. */
    while(size) {
        size_t n=size>5552?5552:size;size-=n;
        while(n--){a+=*p++;b+=a;}
        a%=65521;b%=65521;
    }
    return b<<16|a;
}
static uint32_t identity_hash(const uint8_t *key)
{
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<16;i++){hash^=key[i];hash*=16777619u;}
    return hash;
}
struct slot { uint32_t hash,index; };
struct dictionary { struct slot *slots;unsigned capacity,count; };
static unsigned bucket(uint32_t hash,unsigned capacity)
{ hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;hash*=0x846ca68bu;hash^=hash>>16;return hash&(capacity-1); }
static int room(struct dictionary *d)
{
    if(d->capacity && d->count<d->capacity/2)return 0;
    unsigned capacity=d->capacity?d->capacity*2:64;
    struct slot *slots=calloc(capacity,sizeof(*slots));
    if(!slots)return -1;
    for(unsigned i=0;i<d->capacity;i++)if(d->slots[i].index) {
        unsigned n=bucket(d->slots[i].hash,capacity);
        while(slots[n].index)n=(n+1)&(capacity-1);
        slots[n]=d->slots[i];
    }
    free(d->slots);d->slots=slots;d->capacity=capacity;return 0;
}
static int dimensions(unsigned w,unsigned h)
{ return w && w<=128 && h && h<=192; }
static int header_valid(const uint8_t *h,off_t length)
{
    unsigned w=le16(h+20),height=le16(h+22);uint32_t offset=le32(h+16);
    return h[0] && dimensions(w,height) && offset>=INDEX_BYTES &&
           length>=offset && (uint64_t)offset+1024+w*height<=(uint64_t)length;
}
static int hex(int c)
{
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}
static int safe_byte(unsigned c)
{ return (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-'; }
static void filename(char out[64],const uint8_t *h)
{
    static const char digits[]="0123456789ABCDEF";
    char *p=out;memcpy(p,"cover_",6);p+=6;
    for(unsigned i=0;i<12 && h[i];i++) {
        unsigned c=h[i];
        if(safe_byte(c))*p++=(char)c;
        else {*p++='%';*p++=digits[c>>4];*p++=digits[c&15];}
    }
    snprintf(p,14,"~%08X.bmp",le32(h+12));
}
static int identity(const char *name,uint8_t *h)
{
    const char *p=strrchr(name,'/');p=p?p+1:name;
    if(strncmp(p,"cover_",6))return invalid();p+=6;
    unsigned count=0;memset(h,0,32);
    while(*p && *p!='~') {
        unsigned c=(unsigned char)*p++;
        if(c=='%') {
            if(!p[0] || !p[1] || hex(p[0])<0 || hex(p[1])<0)return invalid();
            c=(unsigned)(hex(p[0])*16+hex(p[1]));p+=2;
        }
        if(!c || count==12)return invalid();h[count++]=(uint8_t)c;
    }
    if(!count || *p++!='~')return invalid();
    uint32_t hash=0;
    for(unsigned i=0;i<8;i++){if(hex(*p)<0)return invalid();hash=hash<<4|(unsigned)hex(*p++);}
    if(strcmp(p,".bmp"))return invalid();put32(h+12,hash);return 0;
}
static void flip_rows(uint8_t *pixels,unsigned stride,unsigned width,unsigned height)
{
    uint8_t row[128];
    for(unsigned y=0;y<height/2;y++) {
        uint8_t *a=pixels+y*stride,*b=pixels+(height-y-1)*stride;
        memcpy(row,a,width);memcpy(a,b,width);memcpy(b,row,width);
    }
}
static int bitmap(const char *name,uint8_t *header)
{
    off_t length;int fd=input(name,&length);uint8_t bmp[54];int result=-1;
    if(fd<0)return -1;
    size_t prefix=(uint64_t)length<sizeof(image)?(size_t)length:sizeof(image);
    if(length<54 || read_at(fd,image,prefix,0))goto done;
    memcpy(bmp,image,sizeof(bmp));
    uint32_t dib=le32(bmp+14),w=le32(bmp+18),raw_h=le32(bmp+22);
    unsigned h=raw_h&0x80000000u ? (~raw_h)+1u:raw_h;
    uint32_t offset=le32(bmp+10),stride=(w+3u)&~3u,declared=le32(bmp+2);
    if(bmp[0]!='B' || bmp[1]!='M' ||
       (dib!=40 && dib!=52 && dib!=56 && dib!=108 && dib!=124) ||
       le16(bmp+26)!=1 || le16(bmp+28)!=8 || le32(bmp+30)!=0 ||
       (le32(bmp+46)!=0 && le32(bmp+46)!=256) || !dimensions(w,h) ||
       offset<14u+dib+1024u || declared>(uint64_t)length ||
       (uint64_t)offset+(uint64_t)stride*h>declared) {invalid();goto done;}
    if(identity(name,header))goto done;
    memmove(image,image+14u+dib,1024);
    if((uint64_t)offset+stride*h<=prefix)
        memmove(image+1024,image+offset,stride*h);
    else if(read_at(fd,image+1024,stride*h,offset))goto done;
    if(!(raw_h&0x80000000u))flip_rows(image+1024,stride,w,h);
    if(stride!=w)for(unsigned y=0;y<h;y++)
        memmove(image+1024+y*w,image+1024+y*stride,w);
    put16(header+20,w);put16(header+22,h);result=0;
done:
    if(close(fd)<0)result=-1;
    return result;
}
/* Compare the complete palette and canonical pixels after a hash match. */
static int same_image(int fd,const uint8_t *header,unsigned bytes)
{
    uint8_t data[4096];off_t offset=le32(header+16);
    for(unsigned done=0;done<bytes;) {
        unsigned n=bytes-done;if(n>sizeof(data))n=sizeof(data);
        if(read_at(fd,data,n,offset+done))return -1;
        if(memcmp(data,image+done,n))return 0;
        done+=n;
    }
    return 1;
}
/* Validate all metadata and reject duplicate exported names before extraction. */
static int validate_index(int fd,off_t length,unsigned *count)
{
    struct dictionary d={0};int result=-1;*count=0;
    if(length<INDEX_BYTES)return invalid();
    for(unsigned i=0;i<MAX_COVERS;i++) {
        uint8_t h[32],other[32];if(read_at(fd,h,32,i*32u))goto done;
        if(!h[0])break;
        if(!header_valid(h,length)){invalid();goto done;}
        /* Canonicalize padding after the serial terminator for identity. */
        for(unsigned n=1;n<12;n++)if(!h[n-1])h[n]=0;
        if(room(&d))goto done;
        uint32_t hash=identity_hash(h);unsigned slot=bucket(hash,d.capacity);
        while(d.slots[slot].index) {
            if(d.slots[slot].hash==hash) {
                if(read_at(fd,other,32,(d.slots[slot].index-1)*32u))goto done;
                for(unsigned n=1;n<12;n++)if(!other[n-1])other[n]=0;
                if(!memcmp(other,h,16)){invalid();goto done;}
            }
            slot=(slot+1)&(d.capacity-1);
        }
        d.slots[slot]=(struct slot){hash,i+1};d.count++;(*count)++;
    }
    result=0;
done:free(d.slots);return result;
}
static int line_read(FILE *file,char *line,size_t capacity)
{
    size_t used=0;int c;
    while((c=fgetc(file))!=EOF && c!='\n') {
        if(!c || used+1>=capacity)return invalid();
        line[used++]=(char)c;
    }
    if(ferror(file))return -1;
    line[used]=0;
    return c!=EOF || used ? 1:0;
}
static int pack(FILE *list)
{
    struct stage out={.fd=-1};struct dictionary d={0};char line[4096];
    unsigned count=0;uint32_t end=INDEX_BYTES;int result=-1;
    if(begin(&out,"cover.bin",fileno(list)) || ftruncate(out.fd,INDEX_BYTES))goto done;
    int line_status;
    while((line_status=line_read(list,line,sizeof(line)))>0) {
        size_t n=strlen(line);
        while(n && (line[n-1]=='\r' || line[n-1]=='\n'))line[--n]=0;
        if(!n || count==MAX_COVERS){invalid();goto done;}
        uint8_t h[32],other[32];if(bitmap(line,h) || room(&d))goto done;
        unsigned w=le16(h+20),height=le16(h+22),bytes=1024+w*height;
        uint32_t hash=adler32(image,bytes) ^ bytes;
        unsigned slot=bucket(hash,d.capacity);int match=0;
        while(d.slots[slot].index) {
            if(d.slots[slot].hash==hash) {
                if(read_at(out.fd,other,32,(d.slots[slot].index-1)*32u))goto done;
                /* Each record keeps its dimensions; identical payload bytes may
                 * be shared across shapes when their byte counts agree. */
                if(le16(other+20)*le16(other+22)==w*height) {
                    match=same_image(out.fd,other,bytes);if(match<0)goto done;
                    if(match)break;
                }
            }
            slot=(slot+1)&(d.capacity-1);
        }
        if(match)put32(h+16,le32(other+16));
        else {
            put32(h+16,end);if(write_at(out.fd,image,bytes,end))goto done;end+=bytes;
            d.slots[slot]=(struct slot){hash,count+1};d.count++;
        }
        if(write_at(out.fd,h,32,count*32u))goto done;count++;
    }
    if(line_status<0)goto done;
    /* Release the image dictionary before validating serial/hash uniqueness. */
    free(d.slots);d=(struct dictionary){0};
    unsigned validated;if(validate_index(out.fd,end,&validated) || validated!=count)goto done;
    if(fclose(list)){list=NULL;goto done;}list=NULL;
    result=finish(&out);
    if(!result)printf("Packed %u images into cover.bin\n",count);
done:
    if(list && fclose(list))result=-1;
    free(d.slots);discard(&out);return result;
}
static int export_image(int fd,const uint8_t *h,const char *name)
{
    unsigned w=le16(h+20),height=le16(h+22),stride=(w+3)&~3u;
    uint8_t bmp[54]={0};struct stage out={.fd=-1};int result=-1;
    if(read_at(fd,image,1024+w*height,le32(h+16)) || begin(&out,name,fd))goto done;
    bmp[0]='B';bmp[1]='M';put32(bmp+2,1078+stride*height);put32(bmp+10,1078);
    put32(bmp+14,40);put32(bmp+18,w);put32(bmp+22,height);put16(bmp+26,1);put16(bmp+28,8);
    put32(bmp+34,stride*height);put32(bmp+38,0x0b13);put32(bmp+42,0x0b13);put32(bmp+46,256);
    if(stride!=w)for(unsigned y=height;y>0;y--) {
        uint8_t *row=image+1024+(y-1)*stride;
        memmove(row,image+1024+(y-1)*w,w);memset(row+w,0,stride-w);
    }
    flip_rows(image+1024,stride,w,height);
    if(write_at(out.fd,bmp,54,0) || write_at(out.fd,image,1024+stride*height,54))goto done;
    result=finish(&out);
done:discard(&out);return result;
}
static int unpack(int fd,off_t length)
{
    unsigned count;struct stage list={.fd=-1};int result=-1;off_t cursor=0;
    if(validate_index(fd,length,&count) || begin(&list,"coverlist.txt",fd))goto done;
    for(unsigned i=0;i<count;i++) {
        uint8_t h[32];char name[64];if(read_at(fd,h,32,i*32u))goto done;
        if(!header_valid(h,length)){invalid();goto done;}
        filename(name,h);if(export_image(fd,h,name))goto done;
        size_t n=strlen(name);name[n++]='\n';
        if(write_at(list.fd,name,n,cursor))goto done;cursor+=(off_t)n;
    }
    if(close(fd)){fd=-1;goto done;}fd=-1;
    result=finish(&list);
    if(!result)printf("Unpacked %u images; wrote coverlist.txt\n",count);
done:if(fd>=0 && close(fd))result=-1;discard(&list);return result;
}
static int disc_hash(int fd)
{
    uint8_t ip[256],h[32]={0};int result=-1;
    if(read_at(fd,ip,sizeof(ip),0))goto done;
    uint8_t *p=ip;
    if(memcmp(p,"SEGA SEGASATURN ",16))p+=16;
    if(memcmp(p,"SEGA SEGASATURN ",16)){invalid();goto done;}
    char gid[17];memcpy(gid,p+32,16);gid[16]=0;
    char *stop=strrchr(gid,'V');if(stop)*stop=0;
    stop=strchr(gid,' ');if(stop)*stop=0;
    size_t n=strlen(gid);if(!n || n>12){invalid();goto done;}
    memcpy(h,gid,n);put32(h+12,adler32(p+48,64));char name[64];filename(name,h);
    printf("Game ID: %s\nIP Hash: %08X\nCover  : %s\n",gid,le32(h+12),name);result=0;
done:if(close(fd))result=-1;return result;
}
int main(int argc,char **argv)
{
    if(argc!=3 || (strcmp(argv[1],"-p") && strcmp(argv[1],"-u") && strcmp(argv[1],"-i"))) {
        fprintf(stderr,"Usage: %s -p coverlist.txt | -u cover.bin | -i disc-image\n",argv[0]);return 1;
    }
    off_t length;int fd=input(argv[2],&length),result=-1;
    if(fd>=0) {
        if(argv[1][1]=='p') {
            FILE *list=fdopen(fd,"r");if(list)result=pack(list);else close(fd);
        } else if(argv[1][1]=='u')result=unpack(fd,length);
        else result=disc_hash(fd);
    }
    if(result)fprintf(stderr,"Cover operation failed: %s\n",strerror(errno));
    return result?1:0;
}
