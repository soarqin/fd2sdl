/* 24618完整source的单页scope/单帧消费。真实Game terrain、actor、mask、page
 * 均执行；仅上传/音频设备/故障注入为私有TU替身。像素expected由独立原码供给。 */
#include "scene.h"
#include "scene_transition.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while(0)

static unsigned allocations,page_creates,page_destroys,terrain_calls,mask_calls,uploads,cues,rng_calls;
static unsigned fail_alloc,fail_page,fail_terrain,fail_mask,quit_terrain,quit_upload;
static void *transition_calloc(size_t n,size_t size) {
    allocations++;
    return fail_alloc ? NULL : calloc(n,size);
}
static fd2_field_page *transition_page_create(void) {
    page_creates++;
    if (fail_page) return NULL;
    fd2_field_page *page=fd2_field_page_create();
    uint8_t *pixels=malloc(FD2_FIELD_PAGE_SIZE);
    if (!page || !pixels) { free(pixels); fd2_field_page_destroy(page); return NULL; }
    /* 原malloc未写域显式seed，不拿宿主calloc0假称原未定义字节。 */
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)pixels[i]=(uint8_t)(i*7u+43u);
    if(fd2_field_page_write(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456))abort();
    free(pixels);return page;
}
static void transition_page_destroy(fd2_field_page *page) {
    if(page)page_destroys++;
    fd2_field_page_destroy(page);
}
static int transition_terrain(fd2_field_game *g,fd2_vga *v,fd2_field_page *p) {
    terrain_calls++;
    int result=fd2_field_game_effect_terrain_begin(g,v,p);
    if(quit_terrain)v->input.quit_requested=1;
    return fail_terrain ? -1 : result;
}
static int transition_mask(const fd2_field_game *g,fd2_vga *v,const fd2_field_page *p,
        const uint8_t lut[256],int x,int y,int radius,int first,int circle) {
    mask_calls++;
    int result=fd2_field_game_effect_mask_frame(g,v,p,lut,x,y,radius,first,circle);
    return fail_mask ? -1 : result;
}
static void transition_upload(fd2_vga *v) {
    uploads++;
    if(quit_upload)v->input.quit_requested=1;
}
static int transition_audio(fd2_field_audio *audio,fd2_field_sfx cue) {
    if(!audio || cue!=FD2_FIELD_SFX_STAGE_TRANSITION)abort();
    cues++;return 0;
}
void transition_release(fd2_scene_transition_scope **scope);

#define fd2_scene_transition_bank80 transition_bank80
#define fd2_scene_transition_begin transition_begin
#define fd2_scene_transition_play_sfx transition_play_sfx
#define fd2_scene_transition_frame transition_frame
#define fd2_scene_transition_release transition_release
#define calloc transition_calloc
#define fd2_field_page_create transition_page_create
#define fd2_field_page_destroy transition_page_destroy
#define fd2_field_game_effect_terrain_begin transition_terrain
#define fd2_field_game_effect_mask_frame transition_mask
#define fd2_vga_upload_current_frame transition_upload
#define fd2_field_audio_play transition_audio
#include "../scene_transition.c"
#undef fd2_field_audio_play
#undef fd2_vga_upload_current_frame
#undef fd2_field_game_effect_mask_frame
#undef fd2_field_game_effect_terrain_begin
#undef fd2_field_page_destroy
#undef fd2_field_page_create
#undef calloc
#undef fd2_scene_transition_release
#undef fd2_scene_transition_frame
#undef fd2_scene_transition_play_sfx
#undef fd2_scene_transition_begin
#undef fd2_scene_transition_bank80

static void reset_spies(void) {
    allocations=page_creates=page_destroys=terrain_calls=mask_calls=uploads=cues=rng_calls=0;
    fail_alloc=fail_page=fail_terrain=fail_mask=quit_terrain=quit_upload=0;
}
#include "s5_effect_fixture.h"

static int safety(void) {
    fixture *f=malloc(sizeof(*f));CHECK(f && fixture_open(f)==0);
    fd2_scene_transition_scope *scope=NULL;
    CHECK(transition_bank80(&f->fdother,&f->audio)==0);
    f->bank.archive.size--;CHECK(transition_bank80(&f->fdother,&f->audio)==-1);f->bank.archive.size++;
    CHECK(transition_bank80(NULL,NULL)==-1);
    CHECK(transition_begin(&scope,f->game,f->vga,INT_MAX,3)==-1 && !scope && !allocations);
    f->vga->input.quit_requested=1;
    CHECK(transition_begin(&scope,f->game,f->vga,4,3)==-2 && !scope && !allocations);
    /* 之后是独立故障用例；只清本测试注入的sticky HOST，不声明App可恢复。 */
    f->vga->input.quit_requested=0;
    fail_alloc=1;CHECK(transition_begin(&scope,f->game,f->vga,4,3)==-1 && !scope && !terrain_calls);
    fail_alloc=0;fail_page=1;
    CHECK(transition_begin(&scope,f->game,f->vga,4,3)==-1 && !scope && !terrain_calls);
    fail_page=0;fail_terrain=1;
    CHECK(transition_begin(&scope,f->game,f->vga,4,3)==-1 && !scope && terrain_calls==1 && page_destroys==1);
    fail_terrain=0;quit_terrain=1;
    CHECK(transition_begin(&scope,f->game,f->vga,4,3)==-2 && !scope && page_destroys==2);
    quit_terrain=0;f->vga->input.quit_requested=0;
    CHECK(transition_begin(&scope,f->game,f->vga,4,3)==0 && scope);
    unsigned old_terrain=terrain_calls;
    CHECK(transition_begin(&scope,f->game,f->vga,4,3)==-1 && terrain_calls==old_terrain);
    CHECK(transition_frame(scope,0,10)==-1 && transition_frame(scope,10,10)==-1 &&
        transition_frame(scope,9,0)==-1 && transition_frame(scope,9,4097)==-1 && !mask_calls);
    size_t stage=f->game->stage;f->game->stage++;
    CHECK(transition_frame(scope,9,10)==-1 && !mask_calls);f->game->stage=stage;
    fd2_field_page *owner=f->vga->field_page;f->vga->field_page=NULL;
    CHECK(transition_frame(scope,9,10)==-1 && !mask_calls);f->vga->field_page=owner;
    CHECK(transition_play_sfx(scope,&f->audio)==0 && cues==1);
    quit_upload=1;
    CHECK(transition_frame(scope,9,10)==-2 && mask_calls==1 && uploads==1);
    CHECK(fd2_field_page_read(owner,0,0,456,336,f->page_bytes,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(f->page_bytes[0]==43 && f->vga->framebuffer[0]==0xa5);
    transition_release(&scope);CHECK(!scope && page_destroys==3);
    transition_release(&scope);CHECK(page_destroys==3 && logic_unchanged(f)==0);
    fixture_close(f);CHECK(fixture_open(f)==0);
    CHECK(transition_begin(&scope,f->game,f->vga,0,0)==0);
    fail_mask=1;
    CHECK(transition_frame(scope,9,10)==-1 && mask_calls==1 && !uploads);
    CHECK(fd2_field_page_read(f->vga->field_page,0,0,456,336,f->page_bytes,FD2_FIELD_PAGE_SIZE,456)==0 && f->page_bytes[0]==43);
    transition_release(&scope);CHECK(logic_unchanged(f)==0);
    fixture_close(f);free(f);return 0;
}
static void hex(const uint8_t *data,size_t length) {
    static const char digits[]="0123456789abcdef";
    for(size_t i=0;i<length;i++) {putchar(digits[data[i]>>4]);putchar(digits[data[i]&15]);}
}
static int vectors(void) {
    char line[160],extra;int x,y,radius,step,audio;
    while(fgets(line,sizeof(line),stdin)) {
        if(sscanf(line,"%d %d %d %d %d %c",&x,&y,&radius,&step,&audio,&extra)!=5 ||
            (audio!=0 && audio!=1))return 1;
        fixture *f=malloc(sizeof(*f));CHECK(f && fixture_open(f)==0);
        fd2_scene_transition_scope *scope=NULL;
        CHECK(transition_bank80(&f->fdother,audio?&f->audio:NULL)==0);
        CHECK(transition_begin(&scope,f->game,f->vga,x,y)==0);
        CHECK(transition_play_sfx(scope,audio?&f->audio:NULL)==0);
        printf("{\"frames\":[");
        for(int i=0;i<9;i++) {
            CHECK(transition_frame(scope,(unsigned)(9-i),radius+i*step)==0);
            CHECK(fd2_field_page_read(f->vga->field_page,0,0,456,336,f->page_bytes,FD2_FIELD_PAGE_SIZE,456)==0);
            printf("%s{\"page_hex\":\"",i?",":"");hex(f->page_bytes,FD2_FIELD_PAGE_SIZE);
            printf("\",\"vga_hex\":\"");hex(f->vga->framebuffer,64000);printf("\"}");
        }
        transition_release(&scope);
        CHECK(transition_bank80(&f->fdother,audio?&f->audio:NULL)==0 && logic_unchanged(f)==0);
        printf("],\"terrain_calls\":%u,\"uploads\":%u,\"cues\":%u,\"page_creates\":%u,\"page_destroys\":%u,\"logic_unchanged\":true}\n",
            terrain_calls,uploads,cues,page_creates,page_destroys);
        fixture_close(f);free(f);
    }
    return ferror(stdin)?1:0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--s5-transition-vectors"))return vectors();
    CHECK(argc==1 && safety()==0);
    puts("scene_transition_source: local scope, actual Game/page/mask and failure-prefix checks PASS");
    return 0;
}
