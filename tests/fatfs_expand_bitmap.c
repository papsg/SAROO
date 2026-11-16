/* papsg adaptation of paps-g SSR-013. Private-helper test: synthetic bitmap state, not a mounted exFAT API test. */
#define main shared_fixture_main
#include "fatfs_seek.c"
#undef main
#include "ff.c"

int main(void)
{
    disk=tmpfile();assert(disk);sectors=1;
    unsigned cases=0,invalid=0,missed=0;
    for(unsigned heap=2;heap<=8;++heap) {
        for(unsigned bits=0;bits<(1u<<heap);++bits) {
            BYTE bitmap[512];memset(bitmap,0xff,sizeof bitmap);bitmap[0]=(BYTE)bits;
            assert(!fseeko(disk,0,SEEK_SET) && fwrite(bitmap,1,sizeof bitmap,disk)==sizeof bitmap);
            for(unsigned origin=0;origin<heap;++origin) {
                for(unsigned needed=1;needed<=heap+1;++needed) {
                    FATFS fs={0};fs.fs_type=FS_EXFAT;fs.n_fatent=heap+2;
                    fs.winsect=(LBA_t)-1;
                    DWORD found=find_bitmap(&fs,origin+2,needed);
                    int exists=0;
                    for(unsigned first=0;first+needed<=heap;++first) {
                        unsigned mask=((1u<<needed)-1)<<first;
                        if(!(bits&mask)) exists=1;
                    }
                    int valid=found>=2 && found<=heap+1 && needed<=heap+2-found;
                    if(valid) valid=!(bits&(((1u<<needed)-1)<<(found-2)));
                    if(found && !valid) invalid++;
                    if(!found && exists) missed++;
                    cases++;
                }
            }
        }
    }
    printf("bitmap cases=%u invalid=%u missed=%u writes=%u\n",cases,invalid,missed,writes);
    assert(!writes && !fclose(disk));
    return invalid || missed ? 1 : 0;
}
