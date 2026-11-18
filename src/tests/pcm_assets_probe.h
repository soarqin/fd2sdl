/* 25a96/25b45 -> 404c0/49690/49891：私有stdout资产流，不读设备或原路径。 */
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static int asset_put(uint32_t value) {
    uint8_t b[4]={(uint8_t)value,(uint8_t)(value>>8),(uint8_t)(value>>16),(uint8_t)(value>>24)};
    return fwrite(b,1,4,stdout)==4;
}
static int asset_snapshot(uint32_t ci,uint32_t sequence,uint32_t op,int result,
    const fd2_pcm_driver *d,const struct fd2_audio *a,const sb_input *s,
    const uint8_t *raw,uint32_t raw_size) {
    if(!asset_put(ci)||!asset_put(sequence)||!asset_put(op)||!asset_put((uint32_t)result))return 0;
    for(unsigned h=0;h<2;h++) {
        const fd2_dig_sample *p=&d->dig.samples[h];uint32_t offset=UINT32_MAX;
        if(p->data) {
            uintptr_t at=(uintptr_t)p->data,base=(uintptr_t)raw;
            if(at<base || at-base>raw_size || p->length>raw_size-(at-base))return 0;
            offset=(uint32_t)(at-base);
        }
        if(!asset_put(p->status)||!asset_put(p->position)||!asset_put(p->loop_count)||
           !asset_put(p->length)||!asset_put(offset))return 0;
    }
    if(!asset_put(d->dig.playing)||!asset_put((uint32_t)(int32_t)d->dig.last_half)||
       !asset_put(d->dig.entered_active)||!asset_put(d->dig.empty_services)||
       !asset_put((uint32_t)d->dig.first_error)||!asset_put(s->starts)||
       !asset_put(s->reads)||!asset_put(s->pauses)||!asset_put(a->syncs))return 0;
    for(unsigned n=0;n<2048;n++)if(!asset_put((uint32_t)d->dig.build[n]))return 0;
    return fwrite(d->dig.dma,1,sizeof(d->dig.dma),stdout)==sizeof(d->dig.dma);
}
static int asset_case(FILE *input,const fd2_archive *archive,const uint8_t *raw,
    uint32_t raw_size,uint32_t ci,uint32_t *records,uint32_t *services) {
    uint32_t index,count;int okay=0,opened=0;
    fd2_pcm_driver d;fd2_pcm_player p,q;struct fd2_audio a={0};sb_input s={0};
    if(!read_u32(input,&index)||!read_u32(input,&count)||index!=ci||!count||count>512)return 0;
    if(!setup(&d,&p,&q,&a,&s))return 0;
    opened=1;
    if(!asset_snapshot(ci,0,0,0,&d,&a,&s,raw,raw_size))goto done;
    ++*records;
    for(uint32_t sequence=1;sequence<=count;sequence++) {
        uint32_t event[6];int result;
        for(unsigned k=0;k<6;k++)if(!read_u32(input,&event[k]))goto done;
        if(event[0]==1) {
            fd2_pcm_bank bank={0};
            if(event[1]>1 || !event[4] || event[4]>3 || event[5])goto done;
            if(fd2_pcm_bank_open(&bank,archive,event[2])<0)goto done;
            result=fd2_pcm_play_replace(event[1]?&q:&p,&bank,event[3],(int)event[4],GAIN);
            fd2_pcm_bank_close(&bank); /* 只释放目录索引；借用raw在全部case后才释放。 */
        } else if(event[0]==2) {
            if(event[1]>1 || event[2]||event[3]||event[4]||event[5])goto done;
            result=fd2_pcm_stop(event[1]?&q:&p);
        } else if(event[0]==3) {
            if(event[1]>1 || event[2]||event[3]||event[4]||event[5])goto done;
            s.half=(int16_t)event[1];result=fd2_audio_dig_service(&d.dig);++*services;
        } else goto done;
        if(result!=(event[0]==3?1:0))goto done;
        if(!asset_snapshot(ci,sequence,event[0],result,&d,&a,&s,raw,raw_size))goto done;
        ++*records;
    }
    okay=1;
done:
    if(opened)cleanup(&d);
    return okay;
}
static int assets_stream(void) {
    uint8_t magic[8],*raw=NULL;uint32_t size=0,cases=0,records=0,services=0;
    fd2_archive archive={0};int okay=0;
#ifdef _WIN32
    if(_setmode(_fileno(stdin),_O_BINARY)==-1 || _setmode(_fileno(stdout),_O_BINARY)==-1)return 0;
#endif
    if(fread(magic,1,8,stdin)!=8 || memcmp(magic,"D2AS01\0\0",8) ||
       !read_u32(stdin,&size)||!read_u32(stdin,&cases)||size!=3382481||cases!=226)goto done;
    raw=malloc(size);if(!raw || fread(raw,1,size,stdin)!=size)goto done;
    if(fwrite("D2AO01\0\0",1,8,stdout)!=8 || !asset_put(size)||
       fwrite(raw,1,size,stdout)!=size || fd2_archive_open_mem(&archive,raw,size)<0)goto done;
    for(uint32_t ci=0;ci<cases;ci++)
        if(!asset_case(stdin,&archive,raw,size,ci,&records,&services)) {
            fprintf(stderr,"assets-stream case=%" PRIu32 " records=%" PRIu32 " services=%" PRIu32 " failed\n",ci,records,services);
            goto done;
        }
    if(fgetc(stdin)!=EOF || ferror(stdin) || unexpected_normal_api || services!=1556)goto done;
    if(fwrite("D2AE01\0\0",1,8,stdout)!=8 || !asset_put(cases)||
       !asset_put(records)||!asset_put(services)||fflush(stdout))goto done;
    okay=1;
done:
    fd2_archive_close(&archive);free(raw);
    return okay;
}
