/* SPDX-License-Identifier: MIT
 * 独立Fraction期望在vectors01.bin；不调用C生成期望，无SDL/时钟/设备。
 */
#include "mixdown_dc.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1; } }while(0)
static int read_i32(FILE *f,int32_t *out)
{
    unsigned char b[4];uint32_t u;
    if(fread(b,1,4,f)!=4)return 0;
    u=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
    *out=u<=INT32_MAX?(int32_t)u:(int32_t)((int64_t)u-INT64_C(4294967296));return 1;
}
static int protocol_and_invalid(void)
{
    fd2_mixdown_dc s={{17,-19}},before=s;
    int32_t fm[2]={0,0},sb[2]={1,-1},gain[2]={8192,8192};
    fd2_mixdown_frame out,old;
    memset(&out,0x5a,sizeof(out));old=out;
    CHECK(fd2_mixdown_dc_step(NULL,fm,sb,gain,1,&out)==FD2_MIXDOWN_H_ARGUMENT);
    CHECK(fd2_mixdown_dc_step(&s,NULL,sb,gain,1,&out)==FD2_MIXDOWN_H_ARGUMENT);
    CHECK(fd2_mixdown_dc_step(&s,fm,sb,gain,2,&out)==FD2_MIXDOWN_H_ARGUMENT);
    CHECK(!memcmp(&s,&before,sizeof(s))&&!memcmp(&out,&old,sizeof(out)));
    return 0;
}
int main(int argc,char **argv)
{
    FILE *f;unsigned char magic[8];int32_t count,i,v[18];
    CHECK(argc==2&&!protocol_and_invalid());
    f=fopen(argv[1],"rb");CHECK(f!=NULL);
    CHECK(fread(magic,1,8,f)==8&&!memcmp(magic,"FD2DC01\0",8));
    CHECK(read_i32(f,&count)&&count>0&&count<4096);
    for(i=0;i<count;i++) {
        fd2_mixdown_dc state,before;fd2_mixdown_frame out,old;
        int result;unsigned k;
        for(k=0;k<18;k++)CHECK(read_i32(f,&v[k]));
        state.adjustment[0]=v[6];state.adjustment[1]=v[7];before=state;
        memset(&out,0x5a,sizeof(out));old=out;
        result=fd2_mixdown_dc_step(&state,v,v+2,v+4,v[8],&out);
        if(result!=v[9]||state.adjustment[0]!=v[16]||state.adjustment[1]!=v[17]||
           (!result&&(out.summed[0]!=v[10]||out.summed[1]!=v[11]||out.corrected[0]!=v[12]||
             out.corrected[1]!=v[13]||out.pcm[0]!=v[14]||out.pcm[1]!=v[15]))) {
            fprintf(stderr,"vector%d rc%d/%d dc%d,%d/%d,%d sum%d,%d ns%d,%d pcm%d,%d\n",
              i,result,v[9],state.adjustment[0],state.adjustment[1],v[16],v[17],out.summed[0],out.summed[1],
              out.corrected[0],out.corrected[1],out.pcm[0],out.pcm[1]);fclose(f);return 1;
        }
        if(result)CHECK(!memcmp(&state,&before,sizeof(state))&&!memcmp(&out,&old,sizeof(out)));
    }
    CHECK(fgetc(f)==EOF&&!ferror(f)&&fclose(f)==0);
    printf("PASS %d independent Fraction frames plus invalid/atomic H checks; no mixer/clock/device\n",count);
    return 0;
}
