/* A5参考音频：使用正式XMIDI/AIL/Nuked九voice后端离线输出WAV。
 * 依据music_track_play @0x25977（原登记同名）；只生成对照素材，
 * 不把离线PCM或指纹排序当作原版逐样本保真/实时设备通过。 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "bgm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int word(FILE *out, unsigned value) {
    unsigned char bytes[2]={(unsigned char)value,(unsigned char)(value>>8)};
    return fwrite(bytes,1,2,out)==2 ? 0 : -1;
}
static int dword(FILE *out, unsigned value) {
    return word(out,value) || word(out,value>>16) ? -1 : 0;
}
static int header(FILE *out,unsigned frames) {
    return fwrite("RIFF",1,4,out)!=4 || dword(out,36+frames*4) ||
        fwrite("WAVEfmt ",1,8,out)!=8 || dword(out,16) || word(out,1) || word(out,2) ||
        dword(out,44100) || dword(out,44100*4) || word(out,4) || word(out,16) ||
        fwrite("data",1,4,out)!=4 || dword(out,frames*4) ? -1 : 0;
}
int main(int argc,char **argv) {
    if(argc!=4) { fprintf(stderr,"usage: %s TRACK SECONDS NEW_OUTPUT.wav\n",argv[0]);return 2; }
    char *end;
    unsigned long track=strtoul(argv[1],&end,10);
    if(*end || track>=20) return 2;
    unsigned long seconds=strtoul(argv[2],&end,10);
    if(*end || !seconds || seconds>120) return 2;
    FILE *existing=fopen(argv[3],"rb");
    if(existing) { fclose(existing);fprintf(stderr,"输出已存在，未覆盖：%s\n",argv[3]);return 2; }
    fd2_audio_config ac={44100,0,1};
    fd2_bgm_config bc={"original_game/FDMUS.DAT","original_game/SAMPLE.AD",44100,0};
    fd2_audio *audio=fd2_audio_create(&ac);
    fd2_bgm_player *bgm=audio ? fd2_bgm_create(audio,&bc) : NULL;
    FILE *out=NULL;
    int result=1;
    if(!bgm || !fd2_bgm_track_valid(bgm,track) || fd2_bgm_play(bgm,track,0)!=0) goto done;
    out=fopen(argv[3],"wb");
    unsigned frames=(unsigned)seconds*44100;
    if(!out || header(out,frames)!=0) goto done;
    float samples[512*2];
    for(unsigned rendered=0;rendered<frames;) {
        unsigned count=frames-rendered<512 ? frames-rendered : 512;
        if(fd2_audio_render_offline(audio,samples,count)!=count) goto done;
        for(unsigned i=0;i<count*2;i++) {
            float value=samples[i];
            if(!isfinite(value)) goto done;
            value=value>1.0f?1.0f:value< -1.0f? -1.0f:value;
            int pcm=(int)lroundf(value*32767.0f);
            if(word(out,(unsigned)pcm)!=0) goto done;
        }
        rendered+=count;
    }
    if(fflush(out)!=0 || ferror(out)) goto done;
    printf("{\"track\":%lu,\"seconds\":%lu,\"rate\":44100,\"channels\":2,\"bits\":16,\"voices\":9,\"loop\":0,\"offline\":true}\n",track,seconds);
    result=0;
done:
    if(out && fclose(out)!=0) result=1;
    fd2_audio_destroy(audio);
    fd2_bgm_destroy(bgm);
    if(result) fprintf(stderr,"离线曲目素材生成失败：track=%lu。保留已有现场，下一次使用新输出路径。\n",track);
    return result;
}
