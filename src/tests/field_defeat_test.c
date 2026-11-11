/* 1db65：真实FDOTHER/FDICON/FDSHAP，正式Game common→runtime.defeat→Play
 * 服务。只私有替换时钟、上传与资源故障，不创建窗口，不代替原版运行。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_defeat.h"
#include "field_game.h"
#include "field_rng.h"

#define CHECK(x) do { if(!(x)) {fprintf(stderr,"field_defeat_test:%d %s\n",__LINE__,#x);exit(1);} } while(0)
static uint64_t test_now;
static unsigned frames,quit_at,refreshes,cue_steps,rng_calls,live_images,live_pages,page_creates,fail_page_create;
static uint8_t *frame_pages;
static uint8_t frame_terrain[25];
static uint8_t frame_idle[25],frame_cursor[25],frame_jitter[25];
static fd2_field_game *current_game;
static const uint8_t *bad_ui;
static size_t bad_ui_size;

static uint64_t defeat_ticks(void) {return test_now;}
static int defeat_get(const fd2_archive *a,size_t i,const uint8_t **data,size_t *size) {
    if(i==5 && bad_ui) {*data=bad_ui;*size=bad_ui_size;return 0;}
    return fd2_archive_get(a,i,data,size);
}
static int defeat_decode(fd2_image *image,const uint8_t *data,size_t size) {
    int result=fd2_lmi_decode_image(image,data,size);
    if(!result)live_images++;
    return result;
}
static void defeat_free_image(fd2_image *image) {
    if(image && image->pixels) {CHECK(live_images);live_images--;}
    fd2_image_free(image);
}
static fd2_field_page *defeat_page_create(void) {
    if(++page_creates==fail_page_create)return NULL;
    fd2_field_page *page=fd2_field_page_create();if(page)live_pages++;
    return page;
}
static void defeat_page_destroy(fd2_field_page *page) {
    if(page) {CHECK(live_pages);live_pages--;}
    fd2_field_page_destroy(page);
}

#define SDL_GetTicks defeat_ticks
#define fd2_archive_get defeat_get
#define fd2_lmi_decode_image defeat_decode
#define fd2_image_free defeat_free_image
#define fd2_field_page_create defeat_page_create
#define fd2_field_page_destroy defeat_page_destroy
#define fd2_field_defeat_prepare test_defeat_prepare
#define fd2_field_defeat_rotate test_defeat_rotate
#define fd2_field_defeat_actor test_defeat_actor
#define fd2_field_defeat_project test_defeat_project
#define fd2_field_defeat_base test_defeat_base
#define fd2_field_defeat_frame test_defeat_frame
#define fd2_field_defeat_close test_defeat_close
#include "../field_defeat.c"
#undef fd2_archive_get
#undef fd2_lmi_decode_image
#undef fd2_image_free
#undef fd2_field_page_create
#undef fd2_field_page_destroy

static void defeat_present(fd2_vga *vga) {
    CHECK(frames<25 && current_game);
    if(frame_pages)CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,
        frame_pages+(size_t)frames*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE,456)==0);
    frame_terrain[frames]=(uint8_t)vga->field_visual.terrain_phase;
    frame_idle[frames]=(uint8_t)vga->field_visual.actor_idle_phase;
    frame_cursor[frames]=(uint8_t)vga->field_visual.cursor_phase;
    frame_jitter[frames]=vga->actor_jitter_phase;
    /* 13次旋转wait前方向已经逐raw提交，但所有死者flags仍保旧值。 */
    if(current_game->units.count==4) {
        if(frames<13) {
            CHECK(current_game->units.items[0].direction==frames%4u);
            CHECK(current_game->units.items[1].direction==frames%4u);
            CHECK(current_game->units.items[0].flags==0 && current_game->units.items[1].flags==0);
            CHECK(current_game->units.items[2].flags==0x81);
        } else for(size_t raw=0;raw<3;raw++)CHECK(current_game->units.items[raw].flags==1);
        CHECK(current_game->units.items[2].direction==3 && current_game->units.items[3].direction==1);
    }
    CHECK(vga->field_visual.actor_idle_phase==2 && vga->field_visual.cursor_phase==3);
    CHECK(!vga->field_water_phase && !vga->field_water_last_tick && !vga->info_panel_suppressed);
    frames++;test_now+=17;
    if(frames==quit_at)vga->input.quit_requested=1;
}
static void defeat_present_timed(fd2_vga *vga,uint32_t ms) {(void)ms;defeat_present(vga);}
static void defeat_wait_ticks(uint32_t ticks) {CHECK(ticks==1);test_now+=55;}
static void defeat_delay(uint32_t ms) {test_now+=ms;}
static int defeat_refresh(fd2_field_game *game,fd2_vga *vga,int mode) {
    CHECK(mode==0 && frames==25);refreshes++;
    return fd2_field_game_runtime_refresh(game,vga,mode);
}
#define fd2_vga_present defeat_present
#define fd2_vga_present_timed defeat_present_timed
#define fd2_wait_ticks defeat_wait_ticks
#define fd2_delay_ms defeat_delay
#define fd2_field_game_runtime_refresh defeat_refresh
#include "../field_play.c"
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_wait_ticks
#undef fd2_delay_ms
#undef fd2_field_game_runtime_refresh
#undef SDL_GetTicks

static uint32_t defeat_rng(void *p) {(void)p;rng_calls++;return 7;}
static int trace_runtime(fd2_field_game *game,fd2_field_runtime_present_step step,int value,void *userdata) {
    if(step==FD2_FIELD_RUNTIME_DEFEAT_CUE)cue_steps++;
    return play_runtime_step(game,step,value,userdata);
}
static const fd2_static_flow_node defeat_nodes[]={
    {.id="start",.op="clear_keys",.edges={1,-1,-1,-1,-1,-1,-1,-1}},
    {.id="death",.op="finalize_defeated",.edges={2,-1,-1,-1,-1,-1,-1,-1}},
    {.id="done",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}}
};
static const fd2_static_flow defeat_flow={"test.defeat.actual-owner",defeat_nodes,3,0};

static void reset_case(fd2_field_game *game,fd2_vga *vga,field_runtime_presentation *runtime,
    const fd2_field_units *original) {
    CHECK(!live_images && !live_pages && !runtime->defeat);
    game->units=*original;game->runtime_host_quit=0;game->failure=FD2_FIELD_FAILURE_NONE;
    game->event_resume=(fd2_field_event_resume){0};
    game->option_values[3]=0;game->ordinary_display_mode=0;
    game->camera_cell_x=game->camera_cell_y=0;
    game->visual_owner=&vga->field_visual;
    game->runtime_present=trace_runtime;game->runtime_present_userdata=runtime;
    game->attack_rng=defeat_rng;
    memset(&vga->input,0,sizeof(vga->input));fd2_input_init(&vga->input);
    memset(vga->framebuffer,109,sizeof(vga->framebuffer));
    vga->field_visual=(fd2_field_visual_state){.actor_idle_phase=2,.actor_idle_tick=16,
        .cursor_phase=3,.terrain_tick=16,.range_tick=16};
    vga->field_water_phase=0;vga->field_water_last_tick=0;
    vga->actor_jitter_phase=0;vga->actor_jitter_tick=0;vga->info_panel_suppressed=0;
    runtime->host_quit=0;runtime->fast=0;
    test_now=1000;frames=quit_at=refreshes=cue_steps=rng_calls=page_creates=fail_page_create=0;
    bad_ui=NULL;frame_pages=NULL;
    uint8_t *initial=malloc(FD2_FIELD_PAGE_SIZE);CHECK(initial);memset(initial,85,FD2_FIELD_PAGE_SIZE);
    CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,initial,FD2_FIELD_PAGE_SIZE,456)==0);free(initial);
}
static int run_case(fd2_field_game *game) {
    /* 夹具在有限clear叶暂停；正式reply推进原PC后进入完整defeat。
     * 不借已删除的旧Scene重放返回口，业务与视觉仍由实际consumer执行。 */
    game->event_resume=(fd2_field_event_resume){.flow=&defeat_flow,.pc=0,.actor=0,.active=1};
    int result=fd2_field_game_common_event_reply(game,FD2_FLOW_NEXT);
    if(result<0 || (game->event_resume.active && !game->runtime_host_quit))
        fprintf(stderr,"defeat common result=%d failure=%d quit=%u active=%u pc=%d frames=%u refresh=%u cue=%u live=%u/%u\n",
            result,(int)game->failure,game->runtime_host_quit,game->event_resume.active,
            game->event_resume.pc,frames,refreshes,cue_steps,live_images,live_pages);
    return result;
}
static void expected_image(uint8_t *page,const fd2_image *image,int x,int y) {
    for(int row=0;row<image->height;row++)for(int col=0;col<image->width;col++) {
        uint8_t c=image->pixels[(size_t)row*(size_t)image->width+(size_t)col];
        if(c && x+col>=0 && x+col<456 && y+row>=0 && y+row<336)
            page[(size_t)(y+row)*456u+(size_t)(x+col)]=c;
    }
}
static void read_page(const fd2_field_page *page,uint8_t *data) {
    CHECK(fd2_field_page_read(page,0,0,456,336,data,FD2_FIELD_PAGE_SIZE,456)==0);
}

int main(void) {
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    fd2_field_game *game=calloc(1,sizeof(*game)),*saved_game=malloc(sizeof(*game));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*saved_vga=malloc(sizeof(*vga));
    uint8_t *trace=malloc(25u*FD2_FIELD_PAGE_SIZE),*expected=malloc(FD2_FIELD_PAGE_SIZE);
    uint8_t *actual=malloc(FD2_FIELD_PAGE_SIZE),*base=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && saved_game && vga && saved_vga && trace && expected && actual && base);
    CHECK(fd2_field_game_open_resources(game,&other,0)==0);
    for(size_t i=0;i<4;i++)CHECK(fd2_field_units_set(&game->units,i,(uint8_t)i,(uint8_t)i,2,
        i==1?22:i==0?2:i==2?4:5,i==1?31:3)==0);
    game->units.items[0].direction=2;game->units.items[1].direction=3;
    game->units.items[2].direction=3;game->units.items[2].flags=0x81;
    game->units.items[3].direction=1;game->units.items[3].hp=17;
    for(size_t i=0;i<4;i++)game->units.items[i].movement_profile=19;
    fd2_field_units original=game->units;vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    field_runtime_presentation runtime={vga,NULL,0,0,NULL};current_game=game;
    reset_case(game,vga,&runtime,&original);*saved_game=*game;*saved_vga=*vga;read_page(vga->field_page,base);
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,&runtime)==0);
    CHECK(runtime.defeat && runtime.defeat->count==1 && live_images==12 && live_pages==1);
    CHECK(runtime.defeat->x[0]==96 && runtime.defeat->y[0]==114);
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
    read_page(vga->field_page,actual);CHECK(!memcmp(actual,base,FD2_FIELD_PAGE_SIZE));
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,&runtime)==0 && !runtime.defeat);
    CHECK(!live_images && !live_pages);

    reset_case(game,vga,&runtime,&original);frame_pages=trace;
    CHECK(run_case(game)==0 && !game->failure && !game->event_resume.active);
    CHECK(frames==25 && refreshes==1 && cue_steps==1 && !rng_calls && !runtime.defeat);
    CHECK(!live_images && !live_pages && !vga->info_panel_suppressed);
    for(size_t i=0;i<3;i++)CHECK(game->units.items[i].flags==1 && !game->units.items[i].hp);
    CHECK(game->units.items[0].direction==0 && game->units.items[1].direction==0 &&
        game->units.items[2].direction==3 && game->units.items[3].direction==1);
    fd2_field_page *model=fd2_field_page_create();CHECK(model);memset(expected,85,FD2_FIELD_PAGE_SIZE);
    for(size_t frame=0;frame<13;frame++) {
        CHECK(frame_idle[frame]==2 && frame_cursor[frame]==3);
        CHECK(fd2_field_page_write(model,0,0,456,336,expected,FD2_FIELD_PAGE_SIZE,456)==0);
        CHECK(fd2_terrain_render_page_block(model,&game->terrain,&game->map,72,72,13,8,0,0,
            frame_terrain[frame],2,NULL,NULL)==0);read_page(model,expected);
        fd2_image corpse={24,24,game->sprites.decoded_frames+(2u+3u*(frame%4u))*576u};
        fd2_image survivor={24,24,game->sprites.decoded_frames+41u*576u};
        expected_image(expected,&corpse,72+2*24,66+3*24);
        expected_image(expected,&survivor,72+5*24,66+3*24);
        CHECK(!memcmp(expected,trace+frame*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    }
    fd2_field_page_destroy(model);model=fd2_field_page_create();CHECK(model);
    CHECK(fd2_terrain_render_page_block(model,&game->terrain,&game->map,72,72,13,8,0,0,
        frame_terrain[13],2,NULL,NULL)==0);read_page(model,base);
    fd2_image survivor={24,24,game->sprites.decoded_frames+41u*576u};
    expected_image(base,&survivor,72+5*24,66+3*24);
    const uint8_t *ui;size_t size;CHECK(fd2_archive_get(&other,5,&ui,&size)==0);
    for(unsigned index=68;index<80;index++) {
        uint32_t a=defeat_u32(ui+6+4*index),b=defeat_u32(ui+10+4*index);
        fd2_image image={0};CHECK(fd2_lmi_decode_image(&image,ui+a,b-a)==0);
        if(index>=74)memcpy(expected,base,FD2_FIELD_PAGE_SIZE);
        expected_image(expected,&image,96,114);fd2_image_free(&image);
        CHECK(!memcmp(expected,trace+(size_t)(13+index-68)*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    }
    fd2_field_page_destroy(model);

    /* 晚图79、未来旋转方向3的合法目录坏RLE、第二临时页分配失败均原子。 */
    for(unsigned kind=0;kind<3;kind++) {
        reset_case(game,vga,&runtime,&original);*saved_game=*game;*saved_vga=*vga;read_page(vga->field_page,base);
        uint8_t *bad=NULL;uint32_t offset=0;
        if(kind==0) {
            bad=malloc(size);CHECK(bad);memcpy(bad,ui,size);uint32_t a=defeat_u32(bad+6+79*4);
            bad[a]=bad[a+1]=0;bad_ui=bad;bad_ui_size=size;
        } else if(kind==1) {offset=game->sprites.offsets[11];game->sprites.offsets[11]=(uint32_t)game->sprites.size;}
        else fail_page_create=2;
        CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,&runtime)==-1);
        if(kind==1)game->sprites.offsets[11]=offset;
        CHECK(!runtime.defeat && !live_images && !live_pages && !frames && !rng_calls);
        CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
        read_page(vga->field_page,actual);CHECK(!memcmp(actual,base,FD2_FIELD_PAGE_SIZE));
        free(bad);bad_ui=NULL;
    }
    static const unsigned stops[]={4,13,14,20};
    for(size_t i=0;i<sizeof(stops)/sizeof(stops[0]);i++) {
        reset_case(game,vga,&runtime,&original);frame_pages=trace;quit_at=stops[i];
        CHECK(run_case(game)>=0 && game->runtime_host_quit && !game->failure);
        CHECK(frames==stops[i] && !refreshes && !runtime.defeat && !live_images && !live_pages && !rng_calls);
        CHECK(game->event_resume.active && game->event_resume.flow==&defeat_flow &&
            game->event_resume.pc==1 &&
            !strcmp(defeat_flow.nodes[game->event_resume.pc].op,"finalize_defeated"));
        *saved_game=*game;
        CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_NEXT)==-1 &&
            !memcmp(game,saved_game,sizeof(*game)) && !rng_calls);
        read_page(vga->field_page,actual);CHECK(!memcmp(actual,trace+(frames-1u)*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
        CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,&runtime)==0 && !runtime.defeat);
    }

    /* 原30锚之后只省FX读取，全部31个合法热记录仍旋转/flags1。 */
    fd2_field_units many={0};
    for(size_t i=0;i<31;i++) {CHECK(fd2_field_units_set(&many,i,0,0,0,2,3)==0);many.items[i].movement_profile=19;}
    reset_case(game,vga,&runtime,&many);
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,&runtime)==0 && runtime.defeat->count==30);
    CHECK(game->units.count==31);CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,&runtime)==0);
    CHECK(run_case(game)==0 && frames==25 && !runtime.defeat && !live_images && !live_pages);
    CHECK(game->units.count==31);for(size_t i=0;i<31;i++)CHECK(game->units.items[i].flags==1 && game->units.items[i].direction==0);
    /* 没有屏内见证，原common入口直接flags1，不触发任何资源/呈现。 */
    reset_case(game,vga,&runtime,&original);game->units.items[0].x=22;*saved_vga=*vga;
    CHECK(run_case(game)==0 && !frames && !page_creates && !refreshes && !cue_steps);
    CHECK(game->units.items[0].direction==2 && game->units.items[1].direction==3 && game->units.items[2].direction==3);
    for(size_t i=0;i<3;i++)CHECK(game->units.items[i].flags==1);
    CHECK(!memcmp(vga,saved_vga,sizeof(*vga)));
    fd2_field_page_destroy(vga->field_page);fd2_field_game_close(game);fd2_archive_close(&other);
    free(game);free(saved_game);free(vga);free(saved_vga);free(trace);free(expected);free(actual);free(base);
    puts("field_defeat_test: real App pages/25 cold frames/flags boundary/late resources/quit/30-anchor bound passed; no GUI");
    return 0;
}
