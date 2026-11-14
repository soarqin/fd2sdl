/* S5真实22253冷helper的display/失败/HOST前缀；与Play测试链接同一目标。
 * 仅在实际teleport_commit后的下一节点交付宿主退出，不借纯等待资源预检。 */
#include "field_game.h"
#include "static_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)

typedef struct {fd2_field_game *game;fd2_vga *vga;unsigned injected;} scope_host_trace;
static scope_host_trace *scope_host;
static unsigned rng_calls;
static void reset_spies(void) {rng_calls=0;scope_host=NULL;}
static const fd2_static_flow_node *scope_node_at(const fd2_static_flow *flow,int pc) {
    const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
    if(scope_host && node && !strcmp(flow->id,"stage.source.teleport") &&
            !strcmp(node->op,"teleport_top") && !scope_host->injected &&
            scope_host->game->units.items[0].x==22 && scope_host->game->units.items[0].y==18) {
        scope_host->vga->input.quit_requested=1;scope_host->injected++;
    }
    return node;
}

/* 仅本TU重编真实Scene供节点边界I/O替身；全部导出改名，生产无测试开关。 */
#define fd2_scene_palette_fade scope_scene_palette_fade
#define fd2_scene_stage_transition_finish scope_scene_stage_transition_finish
#define fd2_scene_play_teleport_scope scope_scene_play_teleport_scope
#define fd2_scene_play_new_game_prologue scope_scene_play_new_game_prologue
#define fd2_scene_play_new_game_prologue_handoff scope_scene_play_new_game_prologue_handoff
#define fd2_scene_play_new_game_prologue_handoff_with_runtime scope_scene_play_new_game_prologue_handoff_with_runtime
#define fd2_scene_play_common_step scope_scene_play_common_step
#define fd2_scene_common_step_validate scope_scene_common_step_validate
#define fd2_scene_common_step_validate_view scope_scene_common_step_validate_view
#define fd2_scene_common_step_validate_resource_view scope_scene_common_step_validate_resource_view
#define fd2_scene_arrival_visual_validate scope_scene_arrival_visual_validate
#define fd2_scene_arrival_visual_prepare scope_scene_arrival_visual_prepare
#define fd2_scene_arrival_visual_play scope_scene_arrival_visual_play
#define fd2_scene_arrival_visual_close scope_scene_arrival_visual_close
#define fd2_scene_stage_victory_validate scope_scene_stage_victory_validate
#define fd2_scene_play_stage_entry scope_scene_play_stage_entry
#define fd2_scene_play_stage_victory scope_scene_play_stage_victory
#define fd2_scene_play_stage_source_flow scope_scene_play_stage_source_flow
#define fd2_scene_play_stage1_entry scope_scene_play_stage1_entry
#define fd2_scene_play_stage2_entry scope_scene_play_stage2_entry
#define fd2_scene_play_stage1_victory scope_scene_play_stage1_victory
#define fd2_scene_play_field_story scope_scene_play_field_story
#define fd2_scene_field_story_validate scope_scene_field_story_validate
#define fd2_scene_play_game_over scope_scene_play_game_over
#define fd2_scene_field_message_validate scope_scene_field_message_validate
#define fd2_scene_field_notice_page scope_scene_field_notice_page
#define fd2_scene_field_message_box scope_scene_field_message_box
#define fd2_scene_field_message_arrow scope_scene_field_message_arrow
#define fd2_static_flow_at scope_node_at
#include "../scene.c"
#undef fd2_static_flow_at
#undef fd2_scene_palette_fade
#undef fd2_scene_stage_transition_finish
#undef fd2_scene_play_teleport_scope
#undef fd2_scene_play_new_game_prologue
#undef fd2_scene_play_new_game_prologue_handoff
#undef fd2_scene_play_new_game_prologue_handoff_with_runtime
#undef fd2_scene_play_common_step
#undef fd2_scene_common_step_validate
#undef fd2_scene_common_step_validate_view
#undef fd2_scene_common_step_validate_resource_view
#undef fd2_scene_arrival_visual_validate
#undef fd2_scene_arrival_visual_prepare
#undef fd2_scene_arrival_visual_play
#undef fd2_scene_arrival_visual_close
#undef fd2_scene_stage_victory_validate
#undef fd2_scene_play_stage_entry
#undef fd2_scene_play_stage_victory
#undef fd2_scene_play_stage_source_flow
#undef fd2_scene_play_stage1_entry
#undef fd2_scene_play_stage2_entry
#undef fd2_scene_play_stage1_victory
#undef fd2_scene_play_field_story
#undef fd2_scene_field_story_validate
#undef fd2_scene_play_game_over
#undef fd2_scene_field_message_validate
#undef fd2_scene_field_notice_page
#undef fd2_scene_field_message_box
#undef fd2_scene_field_message_arrow
#include "s5_effect_fixture.h"

static int setup(fixture *f,int bad_overlay) {
    CHECK(fixture_open(f)==0);
    fd2_field_game *g=f->game;
    g->map.width=g->map.height=40;g->map.cells=calloc(1600,sizeof(uint32_t));CHECK(g->map.cells);
    g->units.count=35;
    for(unsigned i=0;i<35;i++) {
        g->units.items[i]=g->units.items[0];g->units.items[i].ai_behavior_raw=0xa7;
    }
    CHECK(fd2_field_units_sprite_cache_restore(&g->units)==0);
    if(bad_overlay) {
        g->map.cells[18*40+22]=1;g->terrain.attrs[1].flags=0x80;
        f->terrain[2*600]=0xff;
    }
    return 0;
}
static void close_game_fixture(fixture *f) {
    free(f->game->map.cells);f->game->map.cells=f->cells;fixture_close(f);
}
static int shared_scope_preserves_display(unsigned display,int mode) {
    fixture *f=malloc(sizeof(*f));CHECK(f && setup(f,mode==1)==0);
    fd2_field_game *game=f->game;game->ordinary_display_mode=(uint8_t)display;
    game->camera_cell_x=16;game->camera_cell_y=14;
    game->cursor_cell_x=22;game->cursor_cell_y=18;
    scope_host_trace trace={game,f->vga,0};
    if(mode==2)scope_host=&trace;
    *f->before=*game;
    fd2_scene_teleport_scope *scope=NULL;
    CHECK(fd2_scene_teleport_begin(&scope,game,f->vga,&f->fdother,0,22,18,22,18)==0 && scope);
    int result=scope_scene_play_teleport_scope(f->vga,&f->fdother,game,NULL,1,&scope);
    CHECK(result==(mode==0?FD2_SCENE_RESULT_OK:mode==1?FD2_SCENE_RESULT_ERROR:FD2_SCENE_RESULT_HOST_QUIT));
    CHECK(game->units.items[0].x==22 && game->units.items[0].y==18 && game->ordinary_display_mode==display);
    CHECK(trace.injected==(mode==2?1u:0u) && (mode==0?!scope:scope!=NULL));
    f->before->units.items[0].x=22;f->before->units.items[0].y=18;
    fd2_scene_teleport_release(&scope);CHECK(logic_unchanged(f)==0);
    scope_host=NULL;close_game_fixture(f);free(f);return 0;
}
static int shared_scope_rejects_missing_cursor(void) {
    fixture *f=malloc(sizeof(*f));CHECK(f && setup(f,0)==0);
    fd2_field_game *game=f->game;game->ordinary_display_mode=1;
    size_t count=game->visuals.cursor_frame_count;game->visuals.cursor_frame_count=0;*f->before=*game;
    fd2_scene_teleport_scope *scope=NULL;
    CHECK(fd2_scene_teleport_begin(&scope,game,f->vga,&f->fdother,0,22,18,22,18)==0);
    CHECK(scope_scene_play_teleport_scope(f->vga,&f->fdother,game,NULL,1,&scope)==FD2_SCENE_RESULT_ERROR);
    CHECK(scope && game->units.items[0].x==12 && game->units.items[0].y==15 && game->ordinary_display_mode==1);
    fd2_scene_teleport_release(&scope);CHECK(logic_unchanged(f)==0);
    game->visuals.cursor_frame_count=count;close_game_fixture(f);free(f);return 0;
}
int callback82_scope_checks(void) {
    for(unsigned display=0;display<2;display++)for(int mode=0;mode<3;mode++)
        CHECK(shared_scope_preserves_display(display,mode)==0);
    CHECK(shared_scope_rejects_missing_cursor()==0);
    return 0;
}
