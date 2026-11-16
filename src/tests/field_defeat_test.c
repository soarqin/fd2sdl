#include <stdint.h>
/* 1db65：真实FDOTHER/FDICON/FDSHAP，正式Game common→runtime.defeat→Play
 * 服务。只私有替换时钟、上传与资源故障，不创建窗口，不代替原版运行。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "field_defeat.h"
#include "audio_probe_output.h"
#include "field_game.h"
#include "field_rng.h"
#include "field_background.h"
#include "field_visual.h"

#define CHECK(x) do { if(!(x)) {fprintf(stderr,"field_defeat_test:%d %s\n",__LINE__,#x);exit(1);} } while(0)
static uint64_t test_now;
static unsigned frames,quit_at,refreshes,cue_steps,rng_calls,live_images,live_pages,page_creates,fail_page_create;
static uint8_t *frame_pages;
static uint8_t frame_terrain[25];
static uint8_t frame_idle[25],frame_cursor[25],frame_jitter[25];
static fd2_field_game *current_game;
static const uint8_t *bad_ui;
static size_t bad_ui_size;
static int observe_live_order,live_order[8];
static size_t live_order_count;
uint64_t defeat_ticks_ns(void);

/* 这里只观察正式caller的叶次序；各live叶的多次读取由field_visual_live独立核。
 * 既有完整帧夹具仍用其固定外部时钟，不用host uptime生成像素expected。 */
static void defeat_observe_live(int role) {
    if(observe_live_order) {CHECK(live_order_count<8);live_order[live_order_count++]=role;}
}
static int defeat_terrain_live(fd2_field_visual_state *state) {
    defeat_observe_live(1);return fd2_field_visual_terrain_step(state,defeat_ticks_ns());
}
static int defeat_background_live(fd2_field_page *owner,fd2_field_page *target,
    int x,int y,int camera_x,int camera_y,int px,int py,int advance) {
    if(advance)defeat_observe_live(2);
    return fd2_field_background_draw_from(owner,target,x,y,camera_x,camera_y,px,py,
        advance?defeat_ticks_ns():0,advance);
}
static int defeat_range_live(fd2_field_visual_state *state,int32_t override) {
    defeat_observe_live(3);return fd2_field_visual_range_step(state,defeat_ticks_ns(),override);
}
static int defeat_actor_live(fd2_vga *vga) {
    defeat_observe_live(4);return fd2_field_actor_visual_tick(vga,defeat_ticks_ns());
}

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
#define SDL_GetTicksNS defeat_ticks_ns
#define fd2_archive_get defeat_get
#define fd2_lmi_decode_image defeat_decode
#define fd2_image_free defeat_free_image
#define fd2_field_page_create defeat_page_create
#define fd2_field_page_destroy defeat_page_destroy
#define fd2_field_visual_terrain_step_live defeat_terrain_live
#define fd2_field_background_draw_from_live defeat_background_live
#define fd2_field_visual_range_step_live defeat_range_live
#define fd2_field_actor_visual_tick_live defeat_actor_live
#define fd2_field_defeat_prepare test_defeat_prepare
#define fd2_field_defeat_rotate test_defeat_rotate
#define fd2_field_defeat_actor test_defeat_actor
#define fd2_field_defeat_project test_defeat_project
#define fd2_field_defeat_base test_defeat_base
#define fd2_field_defeat_frame test_defeat_frame
#define fd2_field_defeat_close test_defeat_close
#include "../field_defeat.c"
#undef fd2_field_visual_terrain_step_live
#undef fd2_field_background_draw_from_live
#undef fd2_field_visual_range_step_live
#undef fd2_field_actor_visual_tick_live
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
static void defeat_copy_upload(fd2_vga *vga) {
    uint64_t deadline=vga->frame_deadline_ns,interval=vga->frame_interval_ns;
    vga->frame_deadline_ns=vga->frame_interval_ns=0;
    defeat_present(vga);
    vga->frame_deadline_ns=deadline;vga->frame_interval_ns=interval;
}
#define fd2_vga_present defeat_present
#define fd2_vga_upload_current_frame defeat_copy_upload
#define fd2_vga_present_timed defeat_present_timed
#define fd2_wait_ticks defeat_wait_ticks
#define fd2_delay_ms defeat_delay
#define fd2_field_game_runtime_refresh defeat_refresh
#include "../field_play.c"
#undef fd2_vga_upload_current_frame
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
    observe_live_order=0;live_order_count=0;
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

/* 独立来源不变量：source09完整1db65，筛选1dba2..1dbe5，方向1dd17，
 * flags精确赋1位于1dc61/1dd4c。这里只校业务80B写集，不借像素实现
 * 预测原FX，也不把合成32..96状态称为正常输入可达。 */
static size_t source_visible_count(const fd2_field_units *units,int camera_x,int camera_y) {
    size_t visible=0;
    for(size_t raw=0;raw<units->count;raw++) {
        const fd2_field_unit *u=&units->items[raw];
        int64_t dx=(int64_t)u->x-camera_x,dy=(int64_t)u->y-camera_y;
        if(!u->hp && !(u->flags&1u) && dx>=-1 && dx<=13 && dy>=-1 && dy<=9)visible++;
    }
    return visible;
}
static void source_units_after(fd2_field_units *expected,const fd2_field_units *before,
    int camera_x,int camera_y,unsigned stopped_after_wait) {
    *expected=*before;
    int visible=source_visible_count(before,camera_x,camera_y)!=0;
    for(size_t raw=0;raw<before->count;raw++) {
        const fd2_field_unit *u=&before->items[raw];
        if(u->hp)continue;
        if(visible && !(u->flags&1u))expected->items[raw].direction=
            stopped_after_wait && stopped_after_wait<=13 ? (uint8_t)((stopped_after_wait-1u)%4u):0;
        if(!stopped_after_wait || stopped_after_wait>13)expected->items[raw].flags=1;
    }
}
static void capacity_units(fd2_field_units *units,size_t count) {
    memset(units,0,sizeof(*units));
    for(size_t raw=0;raw<FD2_FIELD_MAX_UNITS;raw++) {
        CHECK(fd2_field_units_set(units,raw,0,0,(uint8_t)(raw%3u),2,3)==0);
        fd2_field_unit *u=&units->items[raw];u->movement_profile=19;
        u->direction=(uint8_t)(raw%4u);u->flags=(uint8_t)(raw&1u?0x80:4);
        for(size_t j=0;j<sizeof(u->data_09_1e);j++)u->data_09_1e[j]=(uint8_t)(raw+j+37u);
        for(size_t j=0;j<sizeof(u->data_28_33);j++)u->data_28_33[j]=(uint8_t)(raw+j+93u);
        if(raw>=count) { /* 若误扫描inactive，会被当可见死者并读坏sprite。 */
            u->direction=255;u->unit_id=255;u->movement_profile=255;u->flags=0;
        }
    }
    units->count=count;
}
static void check_capacity_matrix(fd2_field_game *game,fd2_vga *vga,
    field_runtime_presentation *runtime) {
    fd2_field_units input,expected;unsigned vectors=0;
    for(size_t count=0;count<=FD2_FIELD_MAX_UNITS;count++) {
        if(count>1 && count<30)continue;
        capacity_units(&input,count);reset_case(game,vga,runtime,&input);
        {
            CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,runtime)==(count?0:-1));
            if(count) {
                CHECK(runtime->defeat->count==source_visible_count(&input,0,0));
                if(count>=31)CHECK(runtime->defeat->x[30]==0 && runtime->defeat->y[30]==0);
            }
            CHECK(!memcmp(&game->units,&input,sizeof(input)));
            CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,runtime)==0);
            CHECK(!live_images && !live_pages);
        }
        source_units_after(&expected,&input,0,0,0);
        CHECK(run_case(game)==0 && !game->failure && !game->event_resume.active);
        CHECK(!memcmp(&game->units,&expected,sizeof(expected)));
        CHECK(frames==(count?25u:0u) && refreshes==(count?1u:0u) && cue_steps==(count?1u:0u));
        CHECK(!rng_calls && !runtime->defeat && !live_images && !live_pages);vectors++;
    }
    CHECK(vectors==69);
    /* 容量外首先拒绝；不访问items[96]，也不留下任何视觉owner。 */
    capacity_units(&input,96);reset_case(game,vga,runtime,&input);game->units.count=97;
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,runtime)==-1);
    CHECK(game->units.count==97 && !runtime->defeat && !page_creates && !live_images && !live_pages);
}
static void check_filter_matrix(fd2_field_game *game,fd2_vga *vga,
    field_runtime_presentation *runtime,uint8_t *page_before,uint8_t *page_after) {
    /* 包含四条闭边、四角及各边第一条排除格；坐标都是合法BYTE。 */
    static const int positions[][2]={{9,10},{23,10},{10,9},{10,19},{9,9},{23,19},
        {9,19},{23,9},{8,10},{24,10},{10,8},{10,20}};
    static const uint8_t flags[]={0,4,0x80,0x84,1,5,0x81,0x85};
    fd2_field_units input,expected;
    for(size_t xy=0;xy<sizeof(positions)/sizeof(positions[0]);xy++)
    for(size_t flag=0;flag<sizeof(flags);flag++)
    for(unsigned hp=0;hp<2;hp++) {
        capacity_units(&input,1);input.items[0].x=(uint8_t)positions[xy][0];
        input.items[0].y=(uint8_t)positions[xy][1];input.items[0].flags=flags[flag];
        input.items[0].hp=(uint16_t)hp;input.items[0].direction=3;
        reset_case(game,vga,runtime,&input);game->camera_cell_x=game->camera_cell_y=10;
        size_t visible=source_visible_count(&input,10,10);read_page(vga->field_page,page_before);
        CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,runtime)==(visible?0:-1));
        if(visible) {
            CHECK(runtime->defeat && runtime->defeat->count==1);
            CHECK(runtime->defeat->x[0]==48+24*(positions[xy][0]-10));
            CHECK(runtime->defeat->y[0]==42+24*(positions[xy][1]-10));
        }
        CHECK(!memcmp(&game->units,&input,sizeof(input)));
        read_page(vga->field_page,page_after);CHECK(!memcmp(page_before,page_after,FD2_FIELD_PAGE_SIZE));
        CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,runtime)==0);
        CHECK(!live_images && !live_pages);
        source_units_after(&expected,&input,10,10,0);
        CHECK(run_case(game)==0 && !memcmp(&game->units,&expected,sizeof(expected)));
        CHECK(frames==(visible?25u:0u) && !runtime->defeat && !live_images && !live_pages && !rng_calls);
    }
    /* int极值只检宿主拒绝/无溢出边界，不冒称原camera正常可达。 */
    for(unsigned axis=0;axis<2;axis++)for(unsigned edge=0;edge<2;edge++) {
        capacity_units(&input,1);reset_case(game,vga,runtime,&input);
        if(axis)game->camera_cell_y=edge?INT_MAX:INT_MIN;
        else game->camera_cell_x=edge?INT_MAX:INT_MIN;
        CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,runtime)==-1);
        CHECK(!runtime->defeat && !live_images && !live_pages && !page_creates && !rng_calls);
        CHECK(!memcmp(&game->units,&input,sizeof(input)));
    }
}
static void check_owner_boundaries(fd2_field_game *game,fd2_vga *vga,
    field_runtime_presentation *runtime,fd2_field_game *saved_game,fd2_vga *saved_vga,
    uint8_t *page_before,uint8_t *page_after) {
    fd2_field_units input,expected;capacity_units(&input,96);
    reset_case(game,vga,runtime,&input);vga->input.quit_requested=1;
    *saved_game=*game;*saved_vga=*vga;read_page(vga->field_page,page_before);
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,runtime)==1);
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
    CHECK(!runtime->defeat && !page_creates && !live_images && !live_pages);

    reset_case(game,vga,runtime,&input);
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,runtime)==0);
    /* 失配owner在每个叶的首次绘制/时钟写前失败，保留本scope供统一close。 */
    game->camera_cell_x++;*saved_game=*game;*saved_vga=*vga;
    CHECK(test_defeat_rotate(runtime->defeat,game,vga)==-1);
    CHECK(test_defeat_actor(runtime->defeat,game,vga,0)==-1);
    CHECK(test_defeat_project(runtime->defeat,game,vga)==-1);
    CHECK(test_defeat_base(runtime->defeat,game,vga)==-1);
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
    read_page(vga->field_page,page_after);CHECK(!memcmp(page_before,page_after,FD2_FIELD_PAGE_SIZE));
    game->camera_cell_x--;vga->input.quit_requested=1;*saved_game=*game;*saved_vga=*vga;
    CHECK(test_defeat_rotate(runtime->defeat,game,vga)==1);
    CHECK(test_defeat_actor(runtime->defeat,game,vga,0)==1);
    CHECK(test_defeat_project(runtime->defeat,game,vga)==1);
    CHECK(test_defeat_base(runtime->defeat,game,vga)==1);
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
    read_page(vga->field_page,page_after);CHECK(!memcmp(page_before,page_after,FD2_FIELD_PAGE_SIZE));
    vga->input.quit_requested=0;
    for(size_t raw=0;raw<game->units.count;raw++)game->units.items[raw].flags=1;
    CHECK(test_defeat_base(runtime->defeat,game,vga)==0);
    vga->input.quit_requested=1;*saved_game=*game;*saved_vga=*vga;
    CHECK(test_defeat_frame(runtime->defeat,game,vga,68)==1);
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
    read_page(vga->field_page,page_after);CHECK(!memcmp(page_before,page_after,FD2_FIELD_PAGE_SIZE));
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,runtime)==0);
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,runtime)==0);
    CHECK(!runtime->defeat && !live_images && !live_pages);

    /* 准备失败不写业务/页；晚退出保原方向/flags提交前缀，绝不整体回滚。 */
    reset_case(game,vga,runtime,&input);fail_page_create=2;
    *saved_game=*game;*saved_vga=*vga;read_page(vga->field_page,page_before);
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,runtime)==-1);
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
    read_page(vga->field_page,page_after);CHECK(!memcmp(page_before,page_after,FD2_FIELD_PAGE_SIZE));
    CHECK(!runtime->defeat && !live_images && !live_pages);
    static const unsigned stops[]={1,13,14};
    for(size_t i=0;i<sizeof(stops)/sizeof(stops[0]);i++) {
        reset_case(game,vga,runtime,&input);quit_at=stops[i];
        CHECK(run_case(game)>=0 && game->runtime_host_quit && !game->failure && frames==stops[i]);
        source_units_after(&expected,&input,0,0,stops[i]);
        CHECK(!memcmp(&game->units,&expected,sizeof(expected)) && !refreshes && !rng_calls);
        CHECK(!runtime->defeat && !live_images && !live_pages);
    }
}

/* 完整1db65指针序与独立source-stencil常量（FDOTHER5/68、74）：
 * 新15列before使第36项有可见新像素，base alias与App目标相交亦保原顺序。
 * 44866值146、1873处restore-before-alias来自原4ec66/4ebab，非待测C生成。 */
static void check_full_anchor_prefix(fd2_field_game *game,fd2_vga *vga,
    field_runtime_presentation *runtime) {
    static const unsigned counts[]={36,96};
    fd2_field_game *saved=malloc(sizeof(*saved));
    uint8_t *app=malloc(FD2_FIELD_PAGE_SIZE),*base=malloc(FD2_FIELD_PAGE_SIZE);
    uint8_t *probe=malloc(FD2_FIELD_PAGE_SIZE),*vga_saved=malloc(64000);CHECK(saved && app && base && probe && vga_saved);
    for(unsigned n=0;n<2;n++)for(unsigned seed=0;seed<2;seed++) {
        fd2_field_units input;capacity_units(&input,counts[n]);reset_case(game,vga,runtime,&input);
        game->camera_cell_x=game->camera_cell_y=1;
        for(size_t raw=0;raw<counts[n];raw++) {
            game->units.items[raw].x=(uint8_t)(raw%15u);game->units.items[raw].y=(uint8_t)(raw/15u);
            game->units.items[raw].hp=0;game->units.items[raw].flags=0;
        }
        fd2_field_defeat_visual *visual=NULL;
        CHECK(test_defeat_prepare(&visual,game,vga,game->fdother)==0 && visual && visual->count==counts[n]);
        CHECK(visual->x[30]==0 && visual->y[30]==0 && visual->x[31]==48 && visual->y[31]==66);
        if(counts[n]==96)CHECK(visual->x[95]==144 && visual->y[95]==162);
        for(size_t raw=0;raw<counts[n];raw++)game->units.items[raw].flags=1;
        for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++) {
            app[i]=(uint8_t)(i*13u+seed*29u+5u);base[i]=(uint8_t)(i*17u+seed*31u+91u);
        }
        CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,app,FD2_FIELD_PAGE_SIZE,456)==0);
        CHECK(fd2_field_page_write(visual->base,0,0,456,336,base,FD2_FIELD_PAGE_SIZE,456)==0);
        visual->base_ready=1;*saved=*game;
        CHECK(test_defeat_frame(visual,game,vga,68)==0);read_page(vga->field_page,app);
        CHECK(app[44866]==146); /* old35 cap在此为95/124，非完整第36项。 */
        for(unsigned frame=69;frame<74;frame++)CHECK(test_defeat_frame(visual,game,vga,frame)==0);
        read_page(vga->field_page,app);read_page(visual->base,base);memcpy(vga_saved,vga->framebuffer,64000);
        vga->input.quit_requested=1;
        CHECK(test_defeat_frame(visual,game,vga,74)==1);
        read_page(vga->field_page,probe);CHECK(!memcmp(app,probe,FD2_FIELD_PAGE_SIZE));
        read_page(visual->base,probe);CHECK(!memcmp(base,probe,FD2_FIELD_PAGE_SIZE));
        CHECK(!memcmp(vga_saved,vga->framebuffer,64000) && !memcmp(game,saved,sizeof(*game)));
        vga->input.quit_requested=0;
        CHECK(test_defeat_frame(visual,game,vga,80)==-1);
        read_page(vga->field_page,probe);CHECK(!memcmp(app,probe,FD2_FIELD_PAGE_SIZE));
        read_page(visual->base,probe);CHECK(!memcmp(base,probe,FD2_FIELD_PAGE_SIZE));
        CHECK(test_defeat_frame(visual,game,vga,74)==0);
        read_page(vga->field_page,app);read_page(visual->base,base);
        CHECK(app[1873]==(seed?219:188) && base[1873]==32);
        for(unsigned frame=75;frame<80;frame++)CHECK(test_defeat_frame(visual,game,vga,frame)==0);
        CHECK(!memcmp(game,saved,sizeof(*game)) && !rng_calls && !frames && test_now==1000);
        test_defeat_close(&visual);CHECK(!visual && !live_images && !live_pages);
        if(counts[n]==96u && seed==1u) {
            const uint8_t *ui;size_t size;CHECK(fd2_archive_get(game->fdother,5,&ui,&size)==0);
            uint8_t *invalid=malloc(size);CHECK(invalid);memcpy(invalid,ui,size);
            uint32_t offset=defeat_u32(invalid+6+79*4);invalid[offset]=invalid[offset+1]=0;
            for(size_t raw=0;raw<96u;raw++)game->units.items[raw].flags=0;
            *saved=*game;read_page(vga->field_page,app);memcpy(vga_saved,vga->framebuffer,64000);
            bad_ui=invalid;bad_ui_size=size;
            CHECK(test_defeat_prepare(&visual,game,vga,game->fdother)==-1 && !visual);
            bad_ui=NULL;bad_ui_size=0;free(invalid);
            read_page(vga->field_page,probe);CHECK(!memcmp(app,probe,FD2_FIELD_PAGE_SIZE));
            CHECK(!memcmp(vga_saved,vga->framebuffer,64000) && !memcmp(game,saved,sizeof(*game)) &&
                !live_images && !live_pages && !rng_calls && !frames && test_now==1000);
        }
    }
    free(vga_saved);free(probe);free(base);free(app);free(saved);
}

/* 私有actual-only页导出。A/B是显式给定的合法before-state，不把原fresh
 * 临时页猜成0；expected由独立Python原资源reader/原写序生成，C只导出actual。 */
static int f4_defeat_write_u32(FILE *file,uint32_t value) {
    uint8_t bytes[4]={(uint8_t)value,(uint8_t)(value>>8),(uint8_t)(value>>16),(uint8_t)(value>>24)};
    return fwrite(bytes,1,4,file)==4?0:-1;
}
static int f4_defeat_page_probe(unsigned dead_count,unsigned seed,const char *path,int prefix_wide) {
    if(!path || seed>1u || (prefix_wide ? (dead_count!=36u && dead_count!=96u) :
        (dead_count!=1u && (dead_count<30u || dead_count>35u))))return 2;
    const char *paths[]={path};
    fd2_probe_outputs *destination=fd2_probe_outputs_prepare("original_game",paths,1);
    if(!destination)return 2; /* 实际parent／junction保护在任何资源读取之前。 */
    FILE *file=NULL;int result=1;
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga));uint8_t *pixels=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && before && vga && pixels && fd2_field_game_open_resources(game,&other,0)==0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    capacity_units(&game->units,dead_count);
    for(size_t raw=0;raw<dead_count;raw++) {
        game->units.items[raw].x=(uint8_t)(prefix_wide ? raw%15u:2u+raw%7u);
        game->units.items[raw].y=(uint8_t)(prefix_wide ? raw/15u:2u+raw/7u);
        game->units.items[raw].hp=0;game->units.items[raw].flags=0;
    }
    game->camera_cell_x=game->camera_cell_y=prefix_wide?1:0;test_now=1000;
    fd2_field_defeat_visual *visual=NULL;
    CHECK(test_defeat_prepare(&visual,game,vga,&other)==0 && visual && visual->count==dead_count);
    for(size_t raw=0;raw<dead_count;raw++)game->units.items[raw].flags=1;
    *before=*game;
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)pixels[i]=(uint8_t)(i*13u+seed*29u+5u);
    CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456)==0);
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)pixels[i]=(uint8_t)(i*17u+seed*31u+91u);
    CHECK(fd2_field_page_write(visual->base,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456)==0);
    memset(vga->framebuffer,109,sizeof(vga->framebuffer));visual->base_ready=1;
    file=fd2_probe_output_open(destination,0);
    if(!file){result=2;goto output_done;} /* 同一helper再次校parent并独占创建。 */
    if(fwrite(prefix_wide?"F4DFX002":"F4DFX001",1,8,file)!=8 || f4_defeat_write_u32(file,dead_count)!=0 ||
        f4_defeat_write_u32(file,seed)!=0 || f4_defeat_write_u32(file,FD2_FIELD_PAGE_SIZE)!=0 ||
        f4_defeat_write_u32(file,12)!=0)goto output_done;
    for(unsigned frame=68;frame<80;frame++) {
        CHECK(test_defeat_frame(visual,game,vga,frame)==0);
        CHECK(!memcmp(game,before,sizeof(*game)) && test_now==1000 && vga->field_page==visual->app);
        CHECK(!vga->actor_jitter_phase && !vga->actor_jitter_tick && !rng_calls);
        read_page(vga->field_page,pixels);
        if(fwrite(pixels,1,FD2_FIELD_PAGE_SIZE,file)!=FD2_FIELD_PAGE_SIZE)goto output_done;
        read_page(visual->base,pixels);
        if(fwrite(pixels,1,FD2_FIELD_PAGE_SIZE,file)!=FD2_FIELD_PAGE_SIZE ||
            fwrite(vga->framebuffer,1,64000,file)!=64000)goto output_done;
    }
    if(fflush(file)!=0 || ferror(file))goto output_done;
    if(fclose(file)!=0){file=NULL;goto output_done;}
    file=NULL;result=0;
output_done:
    if(file && fclose(file)!=0)result=1;
    test_defeat_close(&visual);
    CHECK(!visual && !live_images && !live_pages);
    fd2_field_page_destroy(vga->field_page);fd2_field_game_close(game);fd2_archive_close(&other);
    fd2_probe_outputs_close(destination);
    free(pixels);free(vga);free(before);free(game);return result;
}

int main(int argc,char **argv) {
    if(argc!=1) {
        if(argc==5 && !strcmp(argv[1],"--f4-defeat-prefix-pages")) {
            unsigned count=!strcmp(argv[2],"36")?36u:!strcmp(argv[2],"96")?96u:0u;
            unsigned seed=!strcmp(argv[3],"0")?0u:!strcmp(argv[3],"1")?1u:2u;
            return f4_defeat_page_probe(count,seed,argv[4],1);
        }
        if(argc==5 && !strcmp(argv[1],"--f4-defeat-pages")) {
            unsigned count=!strcmp(argv[2],"1")?1u:!strcmp(argv[2],"30")?30u:
                !strcmp(argv[2],"31")?31u:!strcmp(argv[2],"32")?32u:
                !strcmp(argv[2],"33")?33u:!strcmp(argv[2],"34")?34u:!strcmp(argv[2],"35")?35u:0u;
            unsigned seed=!strcmp(argv[3],"0")?0u:!strcmp(argv[3],"1")?1u:2u;
            return f4_defeat_page_probe(count,seed,argv[4],0);
        }
        return 2;
    }

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
    observe_live_order=1;
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,&runtime)==0);
    CHECK(!live_order_count); /* PREPARE的advance0没有真实clock叶。 */
    CHECK(runtime.defeat && runtime.defeat->count==1 && live_images==12 && live_pages==1);
    CHECK(runtime.defeat->x[0]==96 && runtime.defeat->y[0]==114);
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !memcmp(vga,saved_vga,sizeof(*vga)));
    read_page(vga->field_page,actual);CHECK(!memcmp(actual,base,FD2_FIELD_PAGE_SIZE));
    CHECK(test_defeat_rotate(runtime.defeat,game,vga)==0);
    static const int source_live_order[]={1,2,3,4};
    CHECK(live_order_count==3 && !memcmp(live_order,source_live_order,3*sizeof(int)));
    CHECK(test_defeat_actor(runtime.defeat,game,vga,0)==0);
    CHECK(live_order_count==4 && !memcmp(live_order,source_live_order,sizeof(source_live_order)));
    CHECK(test_defeat_actor(runtime.defeat,game,vga,2)==0 && live_order_count==4);
    vga->input.quit_requested=1;
    CHECK(test_defeat_rotate(runtime.defeat,game,vga)==1 && live_order_count==4);
    game->failure=FD2_FIELD_FAILURE_ACTION;
    CHECK(test_defeat_rotate(runtime.defeat,game,vga)==-1 && live_order_count==4);
    game->failure=FD2_FIELD_FAILURE_NONE;vga->input.quit_requested=0;observe_live_order=0;
    CHECK(!memcmp(game,saved_game,sizeof(*game)) && !rng_calls);
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

    /* 31条热记录仍完整提交；第31个目的按原写序落临时页，父合同另账。 */
    fd2_field_units many={0};
    for(size_t i=0;i<31;i++) {CHECK(fd2_field_units_set(&many,i,0,0,0,2,3)==0);many.items[i].movement_profile=19;}
    reset_case(game,vga,&runtime,&many);
    CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_BEGIN,0,&runtime)==0 && runtime.defeat->count==31 &&
        runtime.defeat->x[30]==0 && runtime.defeat->y[30]==0);
    CHECK(game->units.count==31);CHECK(play_runtime_step(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,&runtime)==0);
    CHECK(run_case(game)==0 && frames==25 && !runtime.defeat && !live_images && !live_pages);
    CHECK(game->units.count==31);for(size_t i=0;i<31;i++)CHECK(game->units.items[i].flags==1 && game->units.items[i].direction==0);
    /* 没有屏内见证，原common入口直接flags1，不触发任何资源/呈现。 */
    reset_case(game,vga,&runtime,&original);game->units.items[0].x=22;*saved_vga=*vga;
    CHECK(run_case(game)==0 && !frames && !page_creates && !refreshes && !cue_steps);
    CHECK(game->units.items[0].direction==2 && game->units.items[1].direction==3 && game->units.items[2].direction==3);
    for(size_t i=0;i<3;i++)CHECK(game->units.items[i].flags==1);
    CHECK(!memcmp(vga,saved_vga,sizeof(*vga)));
    check_full_anchor_prefix(game,vga,&runtime);
    check_capacity_matrix(game,vga,&runtime);
    check_filter_matrix(game,vga,&runtime,base,actual);
    check_owner_boundaries(game,vga,&runtime,saved_game,saved_vga,base,actual);
    fd2_field_page_destroy(vga->field_page);fd2_field_game_close(game);fd2_archive_close(&other);
    free(game);free(saved_game);free(vga);free(saved_vga);free(trace);free(expected);free(actual);free(base);
    puts("field_defeat_test: App pages/25 cold frames; 69 active-count vectors, 192 filter vectors, 96-record owner/quit/failure write sets passed; no GUI/original-execution or highD natural parent equivalence claim");
    return 0;
}

/* 正式视觉接口使用NS；本测试仍沿原毫秒调度器。 */
uint64_t defeat_ticks_ns(void) { return defeat_ticks()*UINT64_C(1000000); }
