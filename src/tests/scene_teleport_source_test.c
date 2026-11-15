/* 完整22253/22470/22547/22656的实际单叶consumer。合成terrain/actor输入
 * 共用S5 fixture，原LMI/LUT/音效bank只读；输出由独立原码逐前缀比较。 */
#include "scene.h"
#include "scene_teleport.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do {if(!(e)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
static unsigned rng_calls,allocations,page_creates,page_destroys,uploads,cues,bank_opens,mask_pages;
static unsigned fail_alloc,fail_page,fail_bank,fail_commit,quit_upload;
static int sample_seen=-1;
static void reset_spies(void) {
    rng_calls=allocations=page_creates=page_destroys=uploads=cues=bank_opens=mask_pages=0;
    fail_alloc=fail_page=fail_bank=fail_commit=quit_upload=0;sample_seen=-1;
}
static void *teleport_calloc(size_t n,size_t size) {
    allocations++;return fail_alloc?NULL:calloc(n,size);
}
static fd2_field_page *teleport_page_create(void) {
    page_creates++;if(fail_page)return NULL;
    fd2_field_page *page=fd2_field_page_create();uint8_t *pixels=malloc(FD2_FIELD_PAGE_SIZE);
    if(!page || !pixels){free(pixels);fd2_field_page_destroy(page);return NULL;}
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)pixels[i]=(uint8_t)(i*7u+43u);
    if(fd2_field_page_write(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456))abort();
    free(pixels);return page;
}
static void teleport_page_destroy(fd2_field_page *page) {
    if(page)page_destroys++;fd2_field_page_destroy(page);
}
static int teleport_bank_open(fd2_pcm_bank *bank,const fd2_archive *archive,size_t entry) {
    bank_opens++;return fail_bank?-1:fd2_pcm_bank_open(bank,archive,entry);
}
static int teleport_audio(fd2_field_audio *audio,const fd2_pcm_bank *bank,int sample,int loops) {
    if(!audio || !bank || loops!=1 || sample<0 || sample>1)abort();
    sample_seen=sample;cues++;return 0;
}
static int teleport_commit_mask(const fd2_field_game *game,fd2_vga *vga,const fd2_field_page *base,
        const uint8_t lut[256],int x,int y,int radius,int first,int circle) {
    mask_pages++;
    int result=fd2_field_game_effect_mask_page(game,vga,base,lut,x,y,radius,first,circle);
    return fail_commit?-1:result;
}
static void teleport_upload(fd2_vga *vga) {
    uploads++;if(quit_upload)vga->input.quit_requested=1;
}
void test_teleport_release(fd2_scene_teleport_scope **scope);
#define fd2_scene_teleport_validate test_teleport_validate
#define fd2_scene_teleport_begin test_teleport_begin
#define fd2_scene_teleport_sprite test_teleport_sprite
#define fd2_scene_teleport_icon test_teleport_icon
#define fd2_scene_teleport_sound test_teleport_sound
#define fd2_scene_teleport_contract test_teleport_contract
#define fd2_scene_teleport_commit test_teleport_commit
#define fd2_scene_teleport_top test_teleport_top
#define fd2_scene_teleport_wipe test_teleport_wipe
#define fd2_scene_teleport_expand test_teleport_expand
#define fd2_scene_teleport_release test_teleport_release
#define calloc teleport_calloc
#define fd2_field_page_create teleport_page_create
#define fd2_field_page_destroy teleport_page_destroy
#define fd2_pcm_bank_open teleport_bank_open
#define fd2_field_audio_play_borrowed_bank teleport_audio
#define fd2_field_game_effect_mask_page teleport_commit_mask
#define fd2_vga_upload_current_frame teleport_upload
#include "../scene_teleport.c"
#undef fd2_vga_upload_current_frame
#undef fd2_field_game_effect_mask_page
#undef fd2_field_audio_play_borrowed_bank
#undef fd2_pcm_bank_open
#undef fd2_field_page_destroy
#undef fd2_field_page_create
#undef calloc
#undef fd2_scene_teleport_release
#undef fd2_scene_teleport_expand
#undef fd2_scene_teleport_wipe
#undef fd2_scene_teleport_top
#undef fd2_scene_teleport_commit
#undef fd2_scene_teleport_contract
#undef fd2_scene_teleport_sound
#undef fd2_scene_teleport_icon
#undef fd2_scene_teleport_sprite
#undef fd2_scene_teleport_begin
#undef fd2_scene_teleport_validate
#include "s5_effect_fixture.h"

static void expect_xy(fixture *f,int x,int y) {
    f->before->units.items[0].x=(uint8_t)x;f->before->units.items[0].y=(uint8_t)y;
}
static int safety(void) {
    fixture *f=malloc(sizeof(*f));CHECK(f && fixture_open(f)==0);
    fd2_scene_teleport_scope *scope=NULL;
    CHECK(test_teleport_validate(&f->fdother)==0);
    f->game->cursor_cell_x=INT_MAX;f->game->camera_cell_x=INT_MIN;*f->before=*f->game;
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,14,11,14)==-1 &&
        !scope && !memcmp(f->before,f->game,sizeof(*f->game)) && !rng_calls);
    f->game->cursor_cell_x=11;f->game->camera_cell_x=7;*f->before=*f->game;
    fail_alloc=1;CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,14,11,14)==-1 && !scope);
    fail_alloc=0;fail_bank=1;
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,14,11,14)==-1 && !scope);
    fail_bank=0;fail_page=1;
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,14,11,14)==-1 && !scope);
    fail_page=0;
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,FD2_FIELD_MAX_UNITS,11,14,11,14)==-1 && !scope);
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,-1,14,11,14)==-1 && !scope);
    f->vga->input.quit_requested=1;
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,14,11,14)==-2 && !scope);
    f->vga->input.quit_requested=0; /* 后续独立用例，不声明HOST可恢复。 */
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,14,11,14)==0 && scope);
    CHECK(test_teleport_top(scope)==0 && test_teleport_sprite(scope,113)==-1 &&
        test_teleport_sprite(scope,125)==-1 && test_teleport_contract(scope,6,0)==-1 &&
        test_teleport_expand(scope,10)==-1 && test_teleport_wipe(scope,24)==-1 && !uploads);
    CHECK(test_teleport_sound(scope,&f->audio)==0 && sample_seen==1);
    quit_upload=1;CHECK(test_teleport_sprite(scope,114)==-2 && uploads==1);
    CHECK(f->game->units.items[0].x==12 && f->game->units.items[0].y==15);
    test_teleport_release(&scope);test_teleport_release(&scope);
    CHECK(!scope && logic_unchanged(f)==0);fixture_close(f);
    CHECK(fixture_open(f)==0);
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,12,12,11,11)==0);
    CHECK(test_teleport_top(scope)==1 && test_teleport_sound(scope,&f->audio)==0 && sample_seen==0);
    fail_commit=1;CHECK(test_teleport_commit(scope)==-1 && mask_pages==1 && !uploads);
    CHECK(f->game->units.items[0].x==12 && f->game->units.items[0].y==12);
    expect_xy(f,12,12);test_teleport_release(&scope);CHECK(logic_unchanged(f)==0);
    fixture_close(f);CHECK(fixture_open(f)==0);
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,11,11,11)==0);
    unsigned before_upload=uploads;
    CHECK(test_teleport_wipe(scope,18)==0 && uploads==before_upload); /* top原不读不写。 */
    f->game->stage=1;CHECK(test_teleport_commit(scope)==-1 && !mask_pages);f->game->stage=0;
    fd2_field_page *page=f->vga->field_page;f->vga->field_page=NULL;
    CHECK(test_teleport_sprite(scope,114)==-1 && !uploads);f->vga->field_page=page;
    test_teleport_release(&scope);CHECK(logic_unchanged(f)==0);
    /* 防御域：relative center可以相消，但draw-camera仍须用宽差值。
     * 单独调用真实blit/wipe，避免把非法camera送入其它Game地图算法。
     * 极离屏blit只clip，wipe拒绝原越界read；不模拟原版非法内存。 */
    CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,11,14,11,14)==0);
    uint8_t *extreme_base=malloc(FD2_FIELD_PAGE_SIZE),*extreme_vga=malloc(64000);
    fd2_field_game *extreme_game=malloc(sizeof(*extreme_game));CHECK(extreme_base && extreme_vga && extreme_game);
    CHECK(fd2_field_page_read(scope->base,0,0,456,336,extreme_base,FD2_FIELD_PAGE_SIZE,456)==0);
    memcpy(extreme_vga,f->vga->framebuffer,64000);unsigned extreme_uploads=uploads;
    for(unsigned n=0;n<2;n++){
        f->game->camera_cell_x=f->game->camera_cell_y=n?INT_MAX:INT_MIN;
        f->game->cursor_cell_x=f->game->cursor_cell_y=n?INT_MAX:INT_MIN;*extreme_game=*f->game;
        CHECK(teleport_blit(scope,scope->base,124)==0 && test_teleport_wipe(scope,0)==-1);
        CHECK(fd2_field_page_read(scope->base,0,0,456,336,f->page_bytes,FD2_FIELD_PAGE_SIZE,456)==0);
        CHECK(!memcmp(extreme_base,f->page_bytes,FD2_FIELD_PAGE_SIZE) &&
            !memcmp(extreme_vga,f->vga->framebuffer,64000) && !memcmp(extreme_game,f->game,sizeof(*extreme_game)) &&
            uploads==extreme_uploads && !rng_calls);
    }
    f->game->camera_cell_x=7;f->game->camera_cell_y=11;f->game->cursor_cell_x=11;f->game->cursor_cell_y=14;
    free(extreme_base);free(extreme_vga);free(extreme_game);test_teleport_release(&scope);CHECK(logic_unchanged(f)==0);
    fixture_close(f);free(f);return 0;
}
static void hex(const uint8_t *data,size_t size) {
    static const char digits[]="0123456789abcdef";
    for(size_t i=0;i<size;i++){putchar(digits[data[i]>>4]);putchar(digits[data[i]&15]);}
}
/* 原362e8在state2/count29的两次写址是29/30；其余78B不是actor输入。
 * 95只是allocation上界防御用例，不称正常callback82可达状态。 */
static int inactive_vectors(int emit) {
    const unsigned actors[]={29,30,95};
    for(unsigned a=0;a<3;a++)for(unsigned mode=0;mode<5;mode++) {
        fixture *f=malloc(sizeof(*f));CHECK(f && fixture_open(f)==0);
        f->game->units.count=29;
        for(unsigned i=1;i<29;i++)f->game->units.items[i]=f->game->units.items[0];
        memset(f->game->units.items+29,0xa5,(FD2_FIELD_MAX_UNITS-29)*sizeof(fd2_field_unit));
        CHECK(fd2_field_units_sprite_cache_restore(&f->game->units)==0);
        *f->before=*f->game;
        unsigned actor=actors[a];int dx=actor==30?23:21;
        fd2_scene_teleport_scope *scope=NULL;
        CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,actor,dx,18,dx,18)==0);
        CHECK(!memcmp(f->before->units.items+actor,f->game->units.items+actor,sizeof(fd2_field_unit)));
        int result=0,committed=mode==0 || mode==3;
        if(mode==1) { /* 正常释放就是取消；没有到22253的xy写点。 */
            test_teleport_release(&scope);
        } else if(mode==2) {
            f->vga->input.quit_requested=1;result=test_teleport_commit(scope);
            CHECK(result==FD2_SCENE_RESULT_HOST_QUIT);
        } else if(mode==4) {
            fd2_field_page *owner=f->vga->field_page;f->vga->field_page=NULL;
            result=test_teleport_commit(scope);f->vga->field_page=owner;
            CHECK(result==FD2_SCENE_RESULT_ERROR);
        } else {
            fail_commit=mode==3;result=test_teleport_commit(scope);
            CHECK(result==(mode==3?FD2_SCENE_RESULT_ERROR:FD2_SCENE_RESULT_OK));
        }
        if(committed) {
            f->before->units.items[actor].x=(uint8_t)dx;
            f->before->units.items[actor].y=18;
        }
        test_teleport_release(&scope);CHECK(!scope && logic_unchanged(f)==0);
        if(emit) {
            printf("{\"actor\":%u,\"active_count\":29,\"mode\":%u,\"result\":%d,\"committed\":%s,\"record_hex\":\"",
                actor,mode,result,committed?"true":"false");
            hex((const uint8_t *)&f->game->units.items[actor],sizeof(fd2_field_unit));puts("\"}");
        }
        fixture_close(f);free(f);
    }
    return 0;
}
static int record(fixture *f,fd2_scene_teleport_scope *scope,const char *tag,int index,int first) {
    CHECK(fd2_field_page_read(f->vga->field_page,0,0,456,336,f->page_bytes,FD2_FIELD_PAGE_SIZE,456)==0);
    printf("%s{\"tag\":\"%s\",\"index\":%d,\"x\":%u,\"y\":%u,\"page_hex\":\"",first?"":",",tag,index,
        (unsigned)f->game->units.items[0].x,(unsigned)f->game->units.items[0].y);
    hex(f->page_bytes,FD2_FIELD_PAGE_SIZE);printf("\",\"vga_hex\":\"");hex(f->vga->framebuffer,64000);
    if(!strcmp(tag,"icon")) {
        CHECK(fd2_field_page_read(scope->base,0,0,456,336,f->page_bytes,FD2_FIELD_PAGE_SIZE,456)==0);
        printf("\",\"base_hex\":\"");hex(f->page_bytes,FD2_FIELD_PAGE_SIZE);
    }
    printf("\"}");return 0;
}
static int vectors(void) {
    char line[160],extra;int dx,dy,drawx,drawy,focusx,focusy,audio;
    while(fgets(line,sizeof(line),stdin)) {
        if(sscanf(line,"%d %d %d %d %d %d %d %c",&dx,&dy,&drawx,&drawy,&focusx,&focusy,&audio,&extra)!=7 ||
            (audio!=0 && audio!=1))return 1;
        fixture *f=malloc(sizeof(*f));CHECK(f && fixture_open(f)==0);
        f->game->cursor_cell_x=f->game->camera_cell_x+focusx;
        f->game->cursor_cell_y=f->game->camera_cell_y+focusy;*f->before=*f->game;
        fd2_scene_teleport_scope *scope=NULL;
        CHECK(test_teleport_begin(&scope,f->game,f->vga,&f->fdother,0,dx,dy,drawx,drawy)==0);
        printf("{\"frames\":[");CHECK(record(f,scope,"begin",0,1)==0);
        for(unsigned i=114;i<=124;i++){CHECK(test_teleport_sprite(scope,i)==0);CHECK(record(f,scope,"sprite",(int)i,0)==0);}
        CHECK(test_teleport_sound(scope,audio?&f->audio:NULL)==0);
        CHECK(test_teleport_icon(scope)==0 && record(f,scope,"icon",124,0)==0);
        for(int i=5;i>=0;i--){CHECK(test_teleport_contract(scope,(unsigned)i,(unsigned)i)==0);CHECK(record(f,scope,"contract",i,0)==0);}
        CHECK(test_teleport_commit(scope)==0 && record(f,scope,"commit",0,0)==0);
        int top=test_teleport_top(scope);CHECK(top==0 || top==1);
        for(unsigned i=0;i<(top?18u:24u);i++){CHECK(test_teleport_wipe(scope,i)==0);CHECK(record(f,scope,"wipe",(int)i,0)==0);}
        for(unsigned i=0;i<10;i++){CHECK(test_teleport_expand(scope,i)==0);CHECK(record(f,scope,"expand",(int)i,0)==0);}
        test_teleport_release(&scope);expect_xy(f,dx,dy);CHECK(logic_unchanged(f)==0);
        printf("],\"top\":%d,\"sample\":%d,\"uploads\":%u,\"page_creates\":%u,\"page_destroys\":%u,\"logic_unchanged\":true}\n",
            top,sample_seen,uploads,page_creates,page_destroys);
        fixture_close(f);free(f);
    }
    return ferror(stdin)?1:0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--s5-teleport-vectors"))return vectors();
    if(argc==2 && !strcmp(argv[1],"--f1-inactive-teleport-vectors"))return inactive_vectors(1);
    CHECK(argc==1 && safety()==0 && inactive_vectors(0)==0);
    puts("scene_teleport_source: actual Game/page/LMI consumers and error/xy-prefix checks PASS");return 0;
}
