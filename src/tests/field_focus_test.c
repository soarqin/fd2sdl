/* field_focus_move_to @0x12cea（原登记 FUN_00010432）与
 * field_controller_input @0x117e7（原登记同名）：焦点/镜头纯状态回归。
 * 经正式方向移动和Z入口验证，不新增测试API，不启动SDL或窗口。 */
#include <stdio.h>
#include <string.h>

#include "field_game.h"
#include "field_ai.h"
#include "field_item.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; } } while (0)

typedef struct {
    uint32_t state;
    size_t calls;
} test_rng;

static uint32_t unused_rng(void *userdata) {
    test_rng *rng = userdata;
    rng->calls++;
    rng->state = rng->state * 1664525u + 1013904223u;
    return rng->state;
}

static uint32_t cells[24 * 24];
static fd2_terrain_attr attr;

static void init_game(fd2_field_game *game, test_rng *rng) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->stage = 0;
    game->active_side = 2;
    game->turn_number = 9;
    game->selected_unit = -1;
    game->detail_unit = -1;
    game->attack_target = -1;
    game->detail_acknowledged_unit = 3;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->map.width = game->map.height = 24;
    game->map.cells = cells;
    game->terrain.attrs = &attr;
    game->terrain.attr_count = 1;
    game->camera_cell_x = 5;
    game->camera_cell_y = 11;
    game->cursor_cell_x = 8;
    game->cursor_cell_y = 15;
    game->phase_unit_cursor = 4;
    game->phase_next_action_ms = 12345;
    game->pending_cell_event = (fd2_field_pending_cell_event){1, 0x42, 17};
    game->event_log_count = 1;
    game->event_total_count = 18;
    game->event_log[0].action = 0x43;
    game->resource_value = 0x12345678u;
    game->roster_count = 2;
    game->roster[1].data_09_1e[0] = 0xa5;
    game->option_values[0] = game->option_values[1] = 1;
    game->units.count = 1;
    game->units.items[0].side = 2;
    game->units.items[0].hp = game->units.items[0].hp_max = 50;
    game->attack_rng = unused_rng;
    game->attack_rng_userdata = rng;
}

/* 焦点、镜头、轮询位置和详情确认缓存是这两个正式入口允许修改的UI字段。
 * 整个session的其他字段（含单位、pending、owner指针和回合）必须原样保留。 */
static int other_state_unchanged(const fd2_field_game *game,
                                 const fd2_field_game *before) {
    fd2_field_game normalized = *game;
    normalized.camera_cell_x = before->camera_cell_x;
    normalized.camera_cell_y = before->camera_cell_y;
    normalized.cursor_cell_x = before->cursor_cell_x;
    normalized.cursor_cell_y = before->cursor_cell_y;
    normalized.focus_cycle_unit = before->focus_cycle_unit;
    normalized.detail_acknowledged_unit = before->detail_acknowledged_unit;
    return memcmp(&normalized, before, sizeof(normalized)) == 0;
}

static int test_direction_thresholds(void) {
    /* 使用移动前relative；raw 11c73/11b62为jge2，11c19为jle10，11bba为jle5。 */
    const struct {
        int cx, cy, x, y, dx, dy, expected_cx, expected_cy, expected_x, expected_y;
    } cases[] = {
        {5,11, 7,15, -1,0, 5,11, 6,15},
        {5,11, 6,15, -1,0, 4,11, 5,15},
        {5,11, 8,13, 0,-1, 5,11, 8,12},
        {5,11, 8,12, 0,-1, 5,10, 8,11},
        {5,11,15,15, 1,0, 5,11,16,15},
        {5,11,16,15, 1,0, 6,11,17,15},
        {5,11, 8,16, 0,1, 5,11, 8,17},
        {5,11, 8,17, 0,1, 5,12, 8,18},
        {0,11, 1,15, -1,0, 0,11, 0,15},
        {5, 0, 8, 1, 0,-1, 5, 0, 8, 0},
        {11,11,22,15, 1,0,11,11,23,15},
        {5,16, 8,22, 0,1, 5,16, 8,23},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        test_rng rng = {0x31415926u, 0};
        fd2_field_game game;
        init_game(&game, &rng);
        game.camera_cell_x = cases[i].cx;
        game.camera_cell_y = cases[i].cy;
        game.cursor_cell_x = cases[i].x;
        game.cursor_cell_y = cases[i].y;
        fd2_field_game before = game;
        CHECK(fd2_field_game_move_cursor(&game, cases[i].dx, cases[i].dy) == 1);
        CHECK(game.cursor_cell_x == cases[i].expected_x &&
              game.cursor_cell_y == cases[i].expected_y);
        CHECK(game.camera_cell_x == cases[i].expected_cx &&
              game.camera_cell_y == cases[i].expected_cy);
        CHECK(other_state_unchanged(&game, &before));
        CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    }
    return 0;
}

static int test_offscreen_and_edge_focus(void) {
    const struct { int x, y, cx, cy; } cases[] = {
        {2,15,1,11}, {20,15,9,11}, {8,5,5,4}, {8,22,5,16},
        {2,5,1,4}, {20,22,9,16}, {0,0,0,0}, {23,23,11,16},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        test_rng rng = {0x31415926u, 0};
        fd2_field_game game;
        init_game(&game, &rng);
        game.units.items[0].x = (uint8_t)cases[i].x;
        game.units.items[0].y = (uint8_t)cases[i].y;
        fd2_field_game before = game;
        CHECK(fd2_field_game_cycle_focus(&game) == 0);
        CHECK(game.cursor_cell_x == cases[i].x && game.cursor_cell_y == cases[i].y);
        CHECK(game.camera_cell_x == cases[i].cx && game.camera_cell_y == cases[i].cy);
        CHECK(other_state_unchanged(&game, &before));
        CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    }
    return 0;
}

static int test_same_target_and_world_clamp(void) {
    test_rng rng = {0x31415926u, 0};
    fd2_field_game game;
    init_game(&game, &rng);
    /* 同目标即使relative0且camera可卷，也不能人为居中或补一格边距。 */
    game.cursor_cell_x = game.units.items[0].x = 5;
    game.cursor_cell_y = game.units.items[0].y = 11;
    fd2_field_game before = game;
    CHECK(fd2_field_game_cycle_focus(&game) == 0);
    CHECK(game.camera_cell_x == 5 && game.camera_cell_y == 11);
    CHECK(other_state_unchanged(&game, &before));
    game.camera_cell_x = game.camera_cell_y = 0;
    game.cursor_cell_x = game.cursor_cell_y = 0;
    before = game;
    CHECK(fd2_field_game_move_cursor(&game, -1, 0) == 0);
    CHECK(fd2_field_game_move_cursor(&game, 0, -1) == 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    game.camera_cell_x = 11;
    game.camera_cell_y = 16;
    game.cursor_cell_x = game.cursor_cell_y = 23;
    before = game;
    CHECK(fd2_field_game_move_cursor(&game, 1, 0) == 0);
    CHECK(fd2_field_game_move_cursor(&game, 0, 1) == 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    return 0;
}

static int test_cycle_gates_and_invalid_target(void) {
    test_rng rng = {0x31415926u, 0};
    fd2_field_game game;
    init_game(&game, &rng);
    game.units.count = 6;
    for (size_t i = 0; i < game.units.count; i++) {
        game.units.items[i].side = 2;
        game.units.items[i].x = 2;
        game.units.items[i].y = 5;
    }
    game.units.items[0].side = 0;
    game.units.items[1].flags = 1;
    game.units.items[2].flags = 4;
    game.units.items[3].flags = 0x80;
    game.units.items[4].hp = 0; /* native flags&0x85/side门不额外检查HP。 */
    game.units.items[5].x = 20;
    game.units.items[5].y = 22;
    fd2_field_game before = game;
    CHECK(fd2_field_game_cycle_focus(&game) == 4);
    CHECK(game.focus_cycle_unit == 5 && game.camera_cell_x == 1 && game.camera_cell_y == 4);
    CHECK(other_state_unchanged(&game, &before));
    CHECK(fd2_field_game_cycle_focus(&game) == 5 && game.focus_cycle_unit == 0);
    CHECK(fd2_field_game_cycle_focus(&game) == 4 && game.focus_cycle_unit == 5);
    CHECK(other_state_unchanged(&game, &before));
    for (size_t i = 0; i < game.units.count; i++) game.units.items[i].flags = 1;
    before = game;
    CHECK(fd2_field_game_cycle_focus(&game) == -1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);

    const uint8_t invalid[][2] = {{24,5}, {2,24}, {255,255}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        init_game(&game, &rng);
        game.units.count = 2;
        game.focus_cycle_unit = 1;
        game.units.items[1].side = 2;
        game.units.items[1].x = invalid[i][0];
        game.units.items[1].y = invalid[i][1];
        before = game;
        CHECK(fd2_field_game_cycle_focus(&game) == -1);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    }
    CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    return 0;
}

typedef struct {
    unsigned count,stop_at;
    int response,set_host;
    fd2_field_runtime_present_step steps[96];
    int modes[96],x[96],y[96],cx[96],cy[96];
} focus_probe;
static int focus_present(fd2_field_game *game,fd2_field_runtime_present_step step,
        int value,void *userdata) {
    focus_probe *probe=userdata;
    if(probe->count>=96u)return -1;
    unsigned at=probe->count++;
    probe->steps[at]=step;probe->modes[at]=value;
    probe->x[at]=game->cursor_cell_x;probe->y[at]=game->cursor_cell_y;
    probe->cx[at]=game->camera_cell_x;probe->cy[at]=game->camera_cell_y;
    if(probe->count==probe->stop_at) {
        if(probe->set_host)game->runtime_host_quit=1;
        return probe->response;
    }
    return 0;
}
static void bind_focus_probe(fd2_field_game *game,focus_probe *probe) {
    game->runtime_present=focus_present;game->runtime_present_userdata=probe;
    game->cursor_cell_x=8;game->cursor_cell_y=15;
    game->units.items[0].x=10;game->units.items[0].y=16;
}

/* 12cea及四方向原门：mode0不卷镜头不刷新，mode6刷新但不wait。
 * 本目标只核Game叶序；Play的真实wait/clear由field_play_result另验。 */
static int test_focus_present_order(void) {
    const uint8_t *item79=fd2_field_item_record_get(79);
    CHECK(item79 && item79[0x10]==31 && item79[0x12]==4 && item79[0x0d]==24);
    for(unsigned mode=0;mode<=11u;mode++) {
        test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={0};
        init_game(&game,&rng);bind_focus_probe(&game,&probe);
        game.ordinary_display_mode=(uint8_t)mode;
        if(mode==6u)game.ordinary_display_mode=(uint8_t)(item79[0x12]+2u);
        fd2_field_game before=game;
        CHECK(fd2_field_game_cycle_focus(&game)==0 && other_state_unchanged(&game,&before));
        CHECK(probe.steps[0]==FD2_FIELD_RUNTIME_FOCUS_BEGIN && probe.modes[0]==(int)mode &&
            probe.x[0]==8 && probe.y[0]==15 && probe.cx[0]==5 && probe.cy[0]==11);
        const int positions[][2]={{9,15},{10,15},{10,16}};
        unsigned at=1;
        for(unsigned i=0;i<3u;i++) {
            if(mode) {
                CHECK(probe.steps[at]==FD2_FIELD_RUNTIME_FOCUS_STEP && probe.modes[at]==(int)mode &&
                    probe.x[at]==positions[i][0] && probe.y[at]==positions[i][1]);at++;
            }
            CHECK(probe.steps[at]==FD2_FIELD_RUNTIME_CLEAR_KEYS && !probe.modes[at] &&
                probe.x[at]==positions[i][0] && probe.y[at]==positions[i][1]);at++;
        }
        CHECK(probe.count==at && !rng.calls && rng.state==0x31415926u);
        memset(&probe,0,sizeof(probe));before=game;
        CHECK(fd2_field_game_cycle_focus(&game)==0 && probe.count==1u &&
            probe.steps[0]==FD2_FIELD_RUNTIME_FOCUS_BEGIN && other_state_unchanged(&game,&before));
    }
    test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={0};
    init_game(&game,&rng);bind_focus_probe(&game,&probe);
    game.units.items[0].x=20;game.units.items[0].y=15;game.ordinary_display_mode=0;
    CHECK(fd2_field_game_cycle_focus(&game)==0 && game.camera_cell_x==9);
    unsigned clears=0,refreshes=0;
    for(unsigned i=1;i<probe.count;i++) {
        if(probe.steps[i]==FD2_FIELD_RUNTIME_CLEAR_KEYS)clears++;
        else {
            CHECK(probe.steps[i]==FD2_FIELD_RUNTIME_FOCUS_STEP && !probe.modes[i] &&
                probe.x[i]>=17 && probe.cx[i]==probe.x[i]-11);
            refreshes++;
        }
    }
    CHECK(clears==12u && refreshes==4u && !rng.calls);
    return 0;
}

static int test_focus_host_and_failure_prefix(void) {
    for(unsigned mode=0;mode<5u;mode++) {
        test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={0};
        init_game(&game,&rng);bind_focus_probe(&game,&probe);game.ordinary_display_mode=1;
        game.units.count=2;game.units.items[1]=game.units.items[0];
        probe.stop_at=mode==0u?1u:mode==4u?3u:2u;
        probe.response=mode==2u || mode==3u?-1:1;probe.set_host=mode==3u;
        fd2_field_game before=game;
        CHECK(fd2_field_game_cycle_focus(&game)==-1 && probe.count==probe.stop_at);
        CHECK(game.cursor_cell_x==(mode?9:8) && game.cursor_cell_y==15 &&
            game.camera_cell_x==5 && game.camera_cell_y==11 && !game.focus_cycle_unit);
        CHECK(game.runtime_host_quit==(mode!=2u) &&
            game.failure==(mode==2u || mode==3u?FD2_FIELD_FAILURE_ACTION:FD2_FIELD_FAILURE_NONE));
        CHECK(!memcmp(&game.units,&before.units,sizeof(game.units)) &&
            !memcmp(&game.pending_cell_event,&before.pending_cell_event,sizeof(game.pending_cell_event)) &&
            game.turn_number==before.turn_number && !rng.calls && rng.state==0x31415926u);
    }
    test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={0};
    init_game(&game,&rng);bind_focus_probe(&game,&probe);game.runtime_host_quit=1;
    fd2_field_game before=game;
    CHECK(fd2_field_game_cycle_focus(&game)==-1 && !probe.count && !memcmp(&game,&before,sizeof(game)));
    game.runtime_host_quit=0;game.failure=FD2_FIELD_FAILURE_EVENT_DATA;before=game;
    CHECK(fd2_field_game_cycle_focus(&game)==-1 && !probe.count && !memcmp(&game,&before,sizeof(game)));
    return 0;
}

/* 初次AI actor focus可早于path安装/首RNG退出；原借用path不能被snapshot洗掉。 */
static int test_ai_focus_host_before_path(void) {
    test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={.stop_at=1,.response=1};
    init_game(&game,&rng);bind_focus_probe(&game,&probe);game.active_side=0;game.stage=UINT8_MAX;
    game.ordinary_display_mode=1;game.units.count=2;
    for(size_t i=0;i<2u;i++) {
        fd2_field_unit *unit=&game.units.items[i];memset(unit,0,sizeof(*unit));
        unit->side=i?2:0;unit->x=i?12:8;unit->y=15;
        unit->hp=unit->hp_max=50;unit->attack=200;unit->defense=1;unit->accuracy=100;
        unit->movement_profile=1;unit->movement_points=3;unit->level=1;
        ((uint8_t *)unit)[0x31]=255;
    }
    ((uint8_t *)&game.units.items[0])[0x0a]=0x40;
    uint8_t borrowed[4]={2,3,2,3};game.move_path=borrowed;game.move_path_capacity=4;game.move_path_length=3;
    fd2_field_path_result range=FD2_FIELD_PATH_RESULT_INITIALIZER;fd2_field_ai_physical_candidate plan;
    CHECK(fd2_field_game_compute_move_range(&game,0,&range)==0);
    CHECK(fd2_field_ai_choose_physical_candidate(&game.map,&game.terrain,&game.units,0,0,&range,&plan)==0);
    fd2_field_path_close(&range);CHECK(plan.priority>=6);
    fd2_field_units before=game.units;
    CHECK(fd2_field_game_commit_ai_physical(&game,0,0,&plan,NULL,0)==0 && game.runtime_host_quit &&
        game.failure==FD2_FIELD_FAILURE_NONE && probe.count==1u);
    CHECK(game.move_path==borrowed && game.move_path_capacity==4 && game.move_path_length==3 &&
        !memcmp(&game.units,&before,sizeof(before)) && game.selected_unit==-1 && !rng.calls &&
        game.progress.resume==FD2_FIELD_PROGRESS_RESUME_NONE);
    return 0;
}

static int test_direct_cursor_refresh(void) {
    for(unsigned mode=0;mode<=11u;mode++) {
        test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={0};
        init_game(&game,&rng);bind_focus_probe(&game,&probe);game.ordinary_display_mode=(uint8_t)mode;
        CHECK(fd2_field_game_move_cursor(&game,1,0)==1 && game.cursor_cell_x==9 && !rng.calls);
        CHECK(probe.count==(mode?1u:0u));
        if(mode)CHECK(probe.steps[0]==FD2_FIELD_RUNTIME_CURSOR_STEP && probe.modes[0]==(int)mode);
        CHECK(fd2_field_game_move_cursor(&game,0,0)==0 && probe.count==(mode?1u:0u));
    }
    for(int error=0;error<2;error++) {
        test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={.stop_at=1,.response=error?-1:1};
        init_game(&game,&rng);bind_focus_probe(&game,&probe);game.ordinary_display_mode=1;
        CHECK(fd2_field_game_move_cursor(&game,1,0)==1 && game.cursor_cell_x==9 && probe.count==1);
        CHECK(game.failure==(error?FD2_FIELD_FAILURE_ACTION:FD2_FIELD_FAILURE_NONE) && game.runtime_host_quit==!error);
        CHECK(!fd2_field_game_move_cursor(&game,1,0) && game.cursor_cell_x==9 && probe.count==1 && !rng.calls);
    }
    test_rng rng={0x31415926u,0};fd2_field_game game;focus_probe probe={0};
    init_game(&game,&rng);bind_focus_probe(&game,&probe);game.ordinary_display_mode=0;game.cursor_cell_x=11;game.camera_cell_x=0;
    CHECK(fd2_field_game_move_cursor(&game,1,0)==1 && game.camera_cell_x==1 && probe.count==1 &&
        probe.steps[0]==FD2_FIELD_RUNTIME_CURSOR_STEP && probe.modes[0]==0);
    return 0;
}

int main(void) {
    if (test_direction_thresholds() || test_offscreen_and_edge_focus() ||
        test_same_target_and_world_clamp() || test_cycle_gates_and_invalid_target() ||
        test_focus_present_order() || test_focus_host_and_failure_prefix() || test_ai_focus_host_before_path() ||
        test_direct_cursor_refresh())
        return 1;
    puts("field_focus_test: ok");
    return 0;
}
