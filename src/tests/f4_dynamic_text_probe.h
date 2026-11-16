/* 30个UI0动态片段的正式consumer夹具；不提供第二formatter/decoder。
 * 请求参数由独立源计划给出，输出原资源同buffer握手和全64000 owner。
 * input track0无键至真正ACK，track1先peek已排队键；双底仅测值写集合。 */
#ifndef FD2_F4_DYNAMIC_TEXT_PROBE_H
#define FD2_F4_DYNAMIC_TEXT_PROBE_H
#ifndef FD2_F4_READER_PROBE_H
#define FD2_F4_PROTOCOL_ONLY
#include "f4_reader_probe.h"
#undef FD2_F4_PROTOCOL_ONLY
#endif
#include <inttypes.h>
#include <errno.h>
typedef struct {
    unsigned id,fragment;uint32_t value;
    unsigned name,old_name,x,y,portrait,track,seed;
} f4_dynamic_request;
static int f4_dynamic_parse(const char *line,f4_dynamic_request *r) {
    unsigned long long v[10];char extra;
    if(sscanf(line,"dynamic %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %c",
        &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&extra)!=10)return -1;
    if(v[0]>10000u || v[1]>700u || v[2]>UINT32_MAX || v[3]>65535u || v[4]>65535u ||
       v[5]>319u || v[6]>199u || v[7]>135u || v[8]>1u || v[9]!=0u)return -1;
    *r=(f4_dynamic_request){(unsigned)v[0],(unsigned)v[1],(uint32_t)v[2],(unsigned)v[3],(unsigned)v[4],
        (unsigned)v[5],(unsigned)v[6],(unsigned)v[7],(unsigned)v[8],(unsigned)v[9]};return 0;
}
static int f4_dynamic_emit(const f4_dynamic_request *r,const uint8_t *pixels,const uint8_t *mask,
    unsigned written,const fd2_text_expanded *text,int result,unsigned ticks,unsigned pcm,unsigned rng,
    unsigned takes,unsigned injected,unsigned queued,unsigned peeked,uint32_t rng_before,uint32_t rng_after,
    uint64_t deadline_before,uint64_t deadline_after,int prefix,int business_unchanged) {
    uint8_t palette[768],pmask[768]={0};memset(palette,165,768);
    if(printf("{\"schema\":\"fd2-f4-reader-v1\",\"status\":0,\"op\":\"dynamic\",\"a\":%u,\"b\":%u,\"variant\":%u,"
        "\"stride\":320,\"height\":200,\"page_bytes\":64000,\"payload_bytes\":129536,\"pixel_addresses_written\":%u,\"palette_addresses_written\":0,\"owner_guard\":true,"
        "\"consumer_result\":%d,\"ticks\":%u,\"pcm\":%u,\"rng\":%u,\"takes\":%u,\"injected\":%u,\"queued_after\":%u,\"peeked_before\":%u,"
        "\"rng_before\":%" PRIu32 ",\"rng_after\":%" PRIu32 ",\"deadline_before\":%" PRIu64 ",\"deadline_after\":%" PRIu64 ","
        "\"source_prefix_only\":%s,\"business_unchanged\":%s,\"request\":[%u,%u,%" PRIu32 ",%u,%u,%u,%u,%u,%u,%u],\"tokens\":[",
        r->id,r->fragment,r->track*2u+r->seed,written,result,ticks,pcm,rng,takes,injected,queued,peeked,
        rng_before,rng_after,deadline_before,deadline_after,prefix?"true":"false",business_unchanged?"true":"false",
        r->id,r->fragment,r->value,r->name,r->old_name,r->x,r->y,r->portrait,r->track,r->seed)<0)return -1;
    for(size_t i=0;i<text->count;i++)if(printf("%s%u",i?",":"",(unsigned)text->tokens[i])<0)return -1;
    if(fputs("]}\n",stdout)==EOF || fwrite(pixels,1,64000,stdout)!=64000 || fwrite(palette,1,768,stdout)!=768 ||
       fwrite(mask,1,64000,stdout)!=64000 || fwrite(pmask,1,768,stdout)!=768)return -1;
    return fflush(stdout)?-1:0;
}
static int f4_dynamic_palette_unchanged(const fd2_vga *vga) {
    for(size_t i=0;i<sizeof(vga->palette);i++)if(vga->palette[i]!=165u)return 0;
    return 1;
}
static int f4_dynamic_queue(fd2_vga *vga,unsigned track) {
    fd2_input_init(&vga->input);
    if(track) {
        vga->input.queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_ENTER,.bios_code=0x1c0d};
        vga->input.queued_count=1;
        if(!fd2_input_has_any_key(&vga->input) || vga->input.queued_count!=1u)return -1;
    }
    return 0;
}
static int f4_dynamic_images(fd2_image portraits[4],const fd2_archive *dato,unsigned portrait) {
    for(unsigned i=0;i<4u;i++)if(fd2_portrait_decode_frame(&portraits[i],dato,portrait,i) ||
        portraits[i].width!=80 || portraits[i].height!=80)return -1;
    return 0;
}
static void f4_dynamic_images_close(fd2_image portraits[4]) {
    for(unsigned i=0;i<4u;i++)fd2_image_free(&portraits[i]);
}
#endif
