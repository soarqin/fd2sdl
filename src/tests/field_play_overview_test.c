#include "field_visual.h"
#include <stdint.h>
/* 总览modal无窗口回归：测试TU私有替身控制分配、present和当前帧键态；
 * 包含正式field_play.c以测试其私有modal，不给生产代码新增测试hook。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "scene.h"
#include "bios_clock.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint64_t test_clock;
static int timed_frames, close_frame, quit_frame, modal_started, modal_done;
static int finite_frames,source_timed_frames;
static int actual_close_frame;
static int background_calls, dynamic_seen, old_pulse_seen, bad_free;
static int early_open_keys;
static int allocations, frees, fail_allocation, route_mode, action_queries;
static int post_modal_queries, next_outer_present, hold_enter;
static int menu_close_calls;
static SDL_Scancode close_key;
static void *allocated[2];
static fd2_field_game *current_game;
static fd2_field_game route_before;
static fd2_field_rng test_rng;
static int route_owner_unchanged;
static uint16_t route_rng_before;
static int trace_clocks, clock_count;
static char clock_order[64];
static fd2_field_visual_state entry_visual, steady_visual;
static uint16_t entry_jitter_tick, entry_water_tick;
static uint8_t entry_jitter_phase, entry_water_phase;

static void clock_mark(char mark) {
    if (trace_clocks && clock_count < (int)sizeof(clock_order)-1) {
        clock_order[clock_count++] = mark;
        clock_order[clock_count] = '\0';
    }
}
static int overview_actor_phase(fd2_field_game *game,uint64_t now) {
    clock_mark('A');
    return fd2_field_game_tick_actor_phase_visual(game,now);
}
static int overview_terrain_phase(fd2_field_game *game,uint64_t now) {
    clock_mark('T');
    return fd2_field_game_tick_terrain_visual(game,now);
}
static int overview_raw26(fd2_vga *vga,uint64_t now) {
    clock_mark('J');
    return fd2_field_actor_visual_tick(vga,now);
}
static int overview_water(fd2_vga *vga,uint64_t now) {
    clock_mark('W');
    return fd2_field_water_tick(vga,now);
}

static void *overview_malloc(size_t size) {
    allocations++;
    if (allocations == fail_allocation) return NULL;
    void *memory = malloc(size);
    if (allocations <= 2) allocated[allocations - 1] = memory;
    return memory;
}

static void overview_free(void *memory) {
    if (!memory) return;
    int found = 0;
    for (size_t i = 0; i < 2; i++) {
        if (allocated[i] == memory) { allocated[i] = NULL; found = 1; break; }
    }
    if (!found) { bad_free = 1; return; }
    frees++;
    free(memory);
}

static uint64_t overview_ticks(void) { return test_clock; }
static int overview_delay(uint32_t ms) { test_clock += ms; return 0;
}
static int overview_wait_deadline(fd2_vga *vga) {
    vga->frame_deadline_ns = vga->frame_interval_ns = 0;
return 0;
}

static int owner_unchanged(const fd2_field_game *game,
                            const fd2_field_game *before) {
    fd2_field_game normalized = *game;
    normalized.idle_phase = before->idle_phase;
    normalized.last_idle_ms = before->last_idle_ms;
    normalized.terrain_visual_phase = before->terrain_visual_phase;
    normalized.last_terrain_visual_ms = before->last_terrain_visual_ms;
    normalized.range_visual_phase = before->range_visual_phase;
    normalized.last_range_visual_ms = before->last_range_visual_ms;
    normalized.command_highlight_phase = before->command_highlight_phase;
    normalized.last_command_highlight_ms = before->last_command_highlight_ms;
    normalized.system_menu.highlight_phase = before->system_menu.highlight_phase;
    normalized.system_menu.last_highlight_ms = before->system_menu.last_highlight_ms;
    return memcmp(&normalized, before, sizeof(normalized)) == 0;
}

static void set_physical_frame(fd2_vga *vga, int frame) {
    for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
        fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
    if (hold_enter)
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
    if (frame == 3 && early_open_keys) { /* 无入口clear，早键在首稳态才清。 */
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_G, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    }
    if (frame == 13 && close_frame > 13)
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_LSHIFT, 1);
    if (frame == close_frame) {
        fd2_input_set_test_key_state(&vga->input, close_key, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    }
    if (frame == close_frame + 2) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_F2, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_UP, 1);
    }
    fd2_input_begin_test_frame(&vga->input, test_clock);
    if (quit_frame != 0 && frame == quit_frame) vga->input.quit_requested = 1;
}

static void overview_submit(fd2_vga *vga,uint32_t ms) {
    if (fd2_input_has_any_key(&vga->input)) old_pulse_seen = 1;
    timed_frames++;
    test_clock += ms;
    vga->frame_deadline_ns = test_clock * 1000000u;
    vga->frame_interval_ns = (uint64_t)ms * 1000000u;
    if (timed_frames > 12 && timed_frames <= close_frame &&
        vga->framebuffer[10 * VGA_STRIDE + 10] != 20)
        dynamic_seen = 1;
    set_physical_frame(vga, timed_frames);
}

static int overview_present_timed(fd2_vga *vga,uint32_t ms) {
    source_timed_frames++;
    if(modal_started && (timed_frames<12 || actual_close_frame))bad_free=1;
    overview_submit(vga,ms);
return 0;
}
static int overview_present(fd2_vga *vga) {
    /* 每次CPU提交仍选择16ms外部耗时保原向量；finite无source timed。 */
    if(modal_started && !modal_done && (timed_frames<12 ||
            (actual_close_frame && timed_frames<actual_close_frame+12))) {
        finite_frames++;overview_submit(vga,16);
        vga->frame_deadline_ns=vga->frame_interval_ns=0;
        return 0;
    }
    if (route_mode && !modal_started) {
        /* 正式loop的打开帧：Enter先被action消费，Right仍待modal清除。 */
        test_clock += 16;
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga->input, test_clock);
    } else if (!modal_done) {
        if (fd2_input_has_any_key(&vga->input)) old_pulse_seen = 1;
        modal_done = 1;
        test_clock += 16;
        /* 恢复背景这一帧也产生普通键，返回前必须清掉。 */
        set_physical_frame(vga, 0);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_DOWN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        fd2_input_begin_test_frame(&vga->input, test_clock);
        if (quit_frame < 0) vga->input.quit_requested = 1;
    } else if (route_mode) {
        next_outer_present++;
        route_owner_unchanged = owner_unchanged(current_game, &route_before) &&
            ((fd2_field_rng *)current_game->attack_rng_userdata)->state == route_rng_before;
        vga->input.quit_requested = 1; /* 只在下一正常呈现帧结束测试。 */
    }
    overview_wait_deadline(vga);
return 0;
}
static void overview_copy_upload(fd2_vga *vga) {
    uint64_t deadline=vga->frame_deadline_ns,interval=vga->frame_interval_ns;
    vga->frame_deadline_ns=vga->frame_interval_ns=0;
    overview_present(vga);
    vga->frame_deadline_ns=deadline;vga->frame_interval_ns=interval;
}
/* 计划key脉冲不是关闭事实：早键可在首steady退出。source的首次clear
 * 正好锁定真实关闭边界；后续12相位仍须逐次观察，不能误当finalrestore。 */
static void overview_clear(fd2_input *input) {
    if(modal_started && !modal_done && !actual_close_frame)actual_close_frame=timed_frames;
    fd2_input_clear_keys(input);
}

static void overview_background(fd2_field_game *game, fd2_vga *vga) {
    clock_mark('R');
    if (trace_clocks && background_calls == 0) {
        entry_visual = vga->field_visual;
        entry_jitter_tick = vga->actor_jitter_tick;
        entry_jitter_phase = vga->actor_jitter_phase;
        entry_water_tick = vga->field_water_last_tick;
        entry_water_phase = vga->field_water_phase;
    } else if (trace_clocks && background_calls == 1) steady_visual = vga->field_visual;
    if (route_mode && !modal_started) {
        route_before = *game;
        route_rng_before = ((fd2_field_rng *)game->attack_rng_userdata)->state;
    }
    modal_started = 1;
    background_calls++;
    memset(vga->framebuffer, 20 + game->idle_phase, sizeof(vga->framebuffer));
}

/* modal adapter使用固定时间保既有像素/输入夹具；正式live多读与page组合
 * 由field_visual_live、field_unit_live和field_unit_render单独核，不混入真时钟。 */
static int overview_runtime_refresh(fd2_field_game *game,fd2_vga *vga,int steady) {
    uint64_t now=overview_ticks()*UINT64_C(1000000);
    if(steady && (overview_actor_phase(game,now)!=0 || overview_water(vga,now)<0))return -1;
    if(overview_terrain_phase(game,now)!=0 || overview_raw26(vga,now)<0)return -1;
    overview_background(game,vga);
    return 0;
}

static int overview_open(fd2_field_game *, fd2_vga *, const fd2_archive *, size_t);
/* public dispatcher夹具没有map RLE/FDOTHER，也不宣称App绑定验收。
 * 实际App clock/API与背景像素由本TU的独立用例覆盖，生产不加fallback。 */
static int overview_bind(fd2_field_game *game,fd2_vga *vga) {
    if (!route_mode) return fd2_field_game_bind_visual(game, vga);
    return game && game->ready && vga ? 0 : -1;
}
static void overview_render(fd2_field_game *game,fd2_vga *vga) {
    if (!route_mode) fd2_field_game_render(game, vga);
    else memset(vga->framebuffer, 20+game->idle_phase, sizeof(vga->framebuffer));
}
static int overview_menu_refresh(fd2_field_game *game,fd2_vga *vga) {
    overview_render(game,vga);return 0; /* public route无原地图资源，只签dispatcher。 */
}
static int overview_field_refresh(fd2_field_game *game,fd2_vga *vga,int phase) {
    if(phase!=0)return -1;
    overview_render(game,vga);return 0;
}
static int overview_menu_close(fd2_field_game *game, fd2_vga *vga,
                               int opening, uint32_t frame_delay_ms) {
    /* 此夹具只拥有总览资源；菜单收框是明确替身，不让旧的缺资源错误被忽略。 */
    CHECK(route_mode && game == current_game && vga && !opening && frame_delay_ms == 16);
    CHECK(game->ready && game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU &&
          game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY &&
          game->system_menu.selected == 0);
    menu_close_calls++;
    return 0;
}
static int overview_take_action(fd2_input *input, fd2_input_context context,
                                 fd2_input_action *action) {
    action_queries++;
    if (modal_done && !next_outer_present) post_modal_queries++;
    return fd2_input_take_action(input, context, action);
}

#define malloc overview_malloc
#define free overview_free
#define SDL_GetTicks overview_ticks
uint64_t overview_ticks_ns(void);
#define SDL_GetTicksNS overview_ticks_ns
#define fd2_delay_ms overview_delay
#define fd2_vga_delay_ms(vga,value) overview_delay(value)
#define fd2_vga_present overview_present
#define fd2_vga_upload_current_frame overview_copy_upload
#define fd2_vga_present_timed overview_present_timed
#define fd2_vga_wait_frame_deadline overview_wait_deadline
#define fd2_field_game_render_background overview_background
#define fd2_field_game_runtime_overview_refresh overview_runtime_refresh
#define fd2_field_game_runtime_menu_refresh overview_menu_refresh
#define fd2_field_game_runtime_refresh overview_field_refresh
#define fd2_field_game_open overview_open
#define fd2_field_game_bind_visual overview_bind
#define fd2_field_game_render overview_render
#define fd2_field_game_animate_system_menu overview_menu_close
#define fd2_input_take_action overview_take_action
#define fd2_input_clear_keys overview_clear
#define fd2_field_game_tick_actor_phase_visual overview_actor_phase
#define fd2_field_game_tick_terrain_visual overview_terrain_phase
#define fd2_field_actor_visual_tick overview_raw26
#define fd2_field_water_tick overview_water
#define fd2_field_water_tick_live(vga_) overview_water((vga_),SDL_GetTicksNS())
#include "../field_play.c"
#undef malloc
#undef free
#undef SDL_GetTicks
#undef fd2_delay_ms
#undef fd2_vga_delay_ms
#undef fd2_vga_upload_current_frame
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef fd2_field_game_render_background
#undef fd2_field_game_runtime_overview_refresh
#undef fd2_field_game_runtime_menu_refresh
#undef fd2_field_game_runtime_refresh
#undef fd2_field_game_open
#undef fd2_field_game_bind_visual
#undef fd2_field_game_render
#undef fd2_field_game_animate_system_menu
#undef fd2_input_take_action
#undef fd2_input_clear_keys
#undef fd2_field_game_tick_actor_phase_visual
#undef fd2_field_game_tick_terrain_visual
#undef fd2_field_actor_visual_tick
#undef fd2_field_water_tick_live
#undef fd2_field_water_tick

static void reset_controls(void) {
    fd2_bios_clock_reset(0,0);
    test_clock = 1000;
    timed_frames = modal_started = modal_done = background_calls = 0;
    finite_frames=source_timed_frames=0;
    actual_close_frame=0;
    dynamic_seen = old_pulse_seen = bad_free = allocations = frees = 0;
    fail_allocation = route_mode = action_queries = post_modal_queries = 0;
    next_outer_present = route_owner_unchanged = 0;
    menu_close_calls = 0;
    trace_clocks = clock_count = 0;
    memset(clock_order, 0, sizeof(clock_order));
    memset(&entry_visual, 0, sizeof(entry_visual));
    memset(&steady_visual, 0, sizeof(steady_visual));
    close_frame = 15;
    quit_frame = 0;
    close_key = SDL_SCANCODE_A;
    hold_enter = 0;
    early_open_keys=0;
    memset(allocated, 0, sizeof(allocated));
    fd2_field_rng_seed(&test_rng, 0x3141);
}

static int image_init(fd2_image *image, int width, int height, uint8_t color) {
    image->pixels = malloc((size_t)width * (size_t)height);
    if (!image->pixels) return -1;
    image->width = width;
    image->height = height;
    memset(image->pixels, color, (size_t)width * (size_t)height);
    return 0;
}

static int init_game(fd2_field_game *game) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->stage = 0;
    game->active_side = 2;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->selected_unit = game->detail_unit = game->attack_target = -1;
    game->detail_acknowledged_unit = -1;
    game->turn_number = 17;
    game->phase_next_action_ms = 12345;
    game->phase_unit_cursor = 3;
    game->camera_cell_x = 5;
    game->camera_cell_y = 11;
    game->cursor_cell_x = 8;
    game->cursor_cell_y = 15;
    game->map.width = game->map.height = 24;
    game->resource_value = 12345678;
    game->units.count = 2;
    game->units.items[0].side = 2;
    game->units.items[1].side = 0;
    game->units.items[0].hp = game->units.items[1].hp = 1;
    game->roster_count = 1;
    memset(game->roster, 0xa5, sizeof(game->roster));
    game->cell_action_completed[0] = 1;
    game->last_idle_ms = game->last_terrain_visual_ms =
        game->last_range_visual_ms = test_clock;
    game->attack_rng = fd2_field_rng_next;
    game->attack_rng_userdata = &test_rng;
    fd2_field_system_menu_init(&game->system_menu);
    const int sizes[4][2] = {{102,17},{170,117},{170,16},{63,15}};
    for (size_t i = 0; i < 4; i++)
        CHECK(image_init(&game->info_assets.overview[i], sizes[i][0], sizes[i][1],
                          (uint8_t)(30 + i)) == 0);
    for (size_t bank = 0; bank < 2; bank++) {
        for (size_t digit = 0; digit < 10; digit++)
            CHECK(image_init(&game->info_assets.digits[bank][digit],
                              digit == 1 ? 5 : 6, 8, (uint8_t)(50 + digit)) == 0);
    }
    CHECK(image_init(&game->info_assets.digit_overflow[1], 18, 8, 80) == 0);
    CHECK(image_init(&game->info_assets.two_digit_overflow, 12, 8, 81) == 0);
    game->info_assets.ready = 1;
    static const uint8_t glyph[32] = {0x80};
    /* 597..602六条局部资源，包含stage2的601/602；真实文本另由info资源测试核。 */
    static const uint8_t text[] = {0,0,0xff,0xff,0,0,0xff,0xff,
                                 0,0,0xff,0xff,0,0,0xff,0xff,
                                 0,0,0xff,0xff,0,0,0xff,0xff};
    game->font = (fd2_font){glyph, sizeof(glyph), 1};
    game->ui_text.offsets = calloc(603, sizeof(*game->ui_text.offsets));
    CHECK(game->ui_text.offsets != NULL);
    for (size_t fragment = 598; fragment <= 602; fragment++)
        game->ui_text.offsets[fragment] = (uint16_t)((fragment - 597) * 4);
    game->ui_text.data = text;
    game->ui_text.size = sizeof(text);
    game->ui_text.fragment_count = 603;
    current_game = game;
    return 0;
}

static int overview_open(fd2_field_game *game, fd2_vga *vga,
                          const fd2_archive *archive, size_t stage) {
    (void)vga; (void)archive; (void)stage;
    if (init_game(game) != 0) return -1;
    fd2_field_system_menu_open_secondary(&game->system_menu);
    game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    return 0;
}

static int test_modal_keys_and_owner(void) {
    const SDL_Scancode keys[] = {SDL_SCANCODE_A, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_F2, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN};
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        reset_controls();
        close_key = keys[i];
        if (i == 0) close_frame = 24; /* 等到idle动画发生变化。 */
        fd2_field_game game;
        CHECK(init_game(&game) == 0);
        if (i == sizeof(keys)/sizeof(keys[0])-2) game.stage = 1;
        if (i == sizeof(keys)/sizeof(keys[0])-1) game.stage = 2;
        fd2_field_game before = game;
        fd2_vga vga = {0};
        fd2_input_init(&vga.input);
        vga.frame_deadline_ns=UINT64_C(999999999999);vga.frame_interval_ns=UINT64_C(23000000);
        CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_OK);
        CHECK(timed_frames == close_frame + 12 && modal_done);
        CHECK(finite_frames==24 && source_timed_frames==close_frame-12);
        CHECK(background_calls == 1 + close_frame - 12);
        CHECK(old_pulse_seen && !fd2_input_has_any_key(&vga.input)); /* 关闭余键由真实尾clear。 */
        CHECK(allocations == 2 && frees == 2 && !bad_free);
        CHECK(vga.frame_deadline_ns==UINT64_C(999999999999) && vga.frame_interval_ns==UINT64_C(23000000));
        CHECK(vga.framebuffer[10 * VGA_STRIDE + 10] == 20);
        CHECK(owner_unchanged(&game, &before) && test_rng.state == 0x3141);
        if (i == 0) CHECK(dynamic_seen);
        fd2_field_game_close(&game);
    }
    return 0;
}

static int test_opening_early_keys_wait_until_first_stable_clear(void) {
    reset_controls();early_open_keys=1;close_frame=100; /* 尚未投计划关闭键。 */
    fd2_field_game game;CHECK(init_game(&game)==0);
    fd2_field_game before=game;fd2_vga vga={0};fd2_input_init(&vga.input);
    CHECK(play_field_overview(&game,&vga)==FD2_SCENE_RESULT_OK);
    CHECK(timed_frames==25 && background_calls==2 && old_pulse_seen && modal_done);
    CHECK(actual_close_frame==13 && finite_frames==24 && source_timed_frames==1);
    CHECK(!fd2_input_has_any_key(&vga.input) && owner_unchanged(&game,&before) && test_rng.state==0x3141);
    CHECK(allocations==2 && frees==2 && !bad_free);
    fd2_field_game_close(&game);return 0;
}

static int test_modal_failure_and_quit(void) {
    for (int mode = 0; mode < 8; mode++) {
        reset_controls();
        fd2_field_game game;
        CHECK(init_game(&game) == 0);
        fd2_vga vga = {0};
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        fd2_input_init(&vga.input);
        fd2_field_game before = game;
        vga.frame_deadline_ns=UINT64_C(999999999999);vga.frame_interval_ns=UINT64_C(23000000);
        if (mode < 2) fail_allocation = mode + 1;
        if (mode == 2) game.ui_text.fragment_count = 598;
        if (mode == 3) vga.input.quit_requested = 1;
        if (mode == 4) quit_frame = 5;
        if (mode == 5) quit_frame = close_frame; /* quit与关闭键同帧 */
        if (mode == 6) quit_frame = close_frame + 3;
        if (mode == 7) quit_frame = -1; /* 恢复背景的最后一次present */
        int expected = mode < 3 ? FD2_SCENE_RESULT_ERROR : FD2_SCENE_RESULT_HOST_QUIT;
        CHECK(play_field_overview(&game, &vga) == expected);
        if (mode == 2) game.ui_text.fragment_count = before.ui_text.fragment_count;
        CHECK(owner_unchanged(&game, &before) && test_rng.state == 0x3141);
        CHECK(!allocated[0] && !allocated[1] && !bad_free);
        CHECK(frees == (mode == 0 || mode == 3 ? 0 : mode == 1 ? 1 : 2));
        CHECK(vga.frame_deadline_ns==UINT64_C(999999999999) && vga.frame_interval_ns==UINT64_C(23000000));
        if (mode < 4) {
            CHECK(timed_frames == 0 && background_calls == 0);
            for (size_t p = 0; p < sizeof(vga.framebuffer); p++)
                CHECK(vga.framebuffer[p] == 0x77);
        } else if (mode == 7) CHECK(timed_frames == close_frame + 12 && modal_done);
        else CHECK(timed_frames == quit_frame && !modal_done);
        fd2_field_game_close(&game);
    }
    return 0;
}

static int test_modal_preconditions(void) {
    for (int mode = 0; mode < 6; mode++) {
        reset_controls();
        fd2_field_game game;
        CHECK(init_game(&game) == 0);
        if (mode == 0) game.stage = SIZE_MAX; /* S5已开放21；用真实无绑定stage核前置拒绝。 */
        if (mode == 1) game.active_side = 0;
        if (mode == 2) game.selected_unit = 0;
        if (mode == 3) game.interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
        if (mode == 4) game.detail_visible = 1;
        if (mode == 5) game.failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
        fd2_field_game before = game;
        fd2_vga vga = {0};
        fd2_input_init(&vga.input);
        CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_ERROR);
        CHECK(allocations == 0 && timed_frames == 0 && background_calls == 0);
        CHECK(owner_unchanged(&game, &before) && test_rng.state == 0x3141);
        fd2_field_game_close(&game);
    }
    /* 缺602仍是真资源错误：不能为开放stage2而跳过晚条目或先present。 */
    reset_controls();
    fd2_field_game game;
    CHECK(init_game(&game) == 0);
    game.stage = 2; game.ui_text.fragment_count = 602;
    fd2_field_game before = game;
    fd2_vga vga = {0}; fd2_input_init(&vga.input);
    memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
    CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_ERROR);
    CHECK(allocations == 2 && frees == 2 && !bad_free && !timed_frames && !background_calls);
    CHECK(owner_unchanged(&game, &before) && test_rng.state == 0x3141);
    for (size_t i = 0; i < sizeof(vga.framebuffer); i++) CHECK(vga.framebuffer[i] == 0x77);
    fd2_field_game_close(&game);
    return 0;
}

static int test_transition_geometry(void) {
    uint8_t source[VGA_W * VGA_H];
    for (int y = 0; y < VGA_H; y++) {
        for (int x = 0; x < VGA_W; x++)
            source[y * VGA_STRIDE + x] = (uint8_t)(0x40 + (x + y * 3) % 0x80);
    }
    fd2_vga vga = {0};
#define PIX(p, x, y) ((p)[(y) * VGA_STRIDE + (x)])
#define FRAME(phase, opening) do { \
    memset(vga.framebuffer, 0x11, sizeof(vga.framebuffer)); \
    overview_transition_frame(&vga, source, phase, opening); } while (0)
    FRAME(0, 1);
    CHECK(PIX(vga.framebuffer, 275, 37) == PIX(source, 75, 37));
    CHECK(PIX(vga.framebuffer, 319, 153) == PIX(source, 119, 153));
    CHECK(PIX(vga.framebuffer, 274, 37) == 0x11);
    FRAME(0, 0);
    CHECK(PIX(vga.framebuffer, 0, 37) == PIX(source, 200, 37));
    CHECK(PIX(vga.framebuffer, 44, 153) == PIX(source, 244, 153));
    CHECK(PIX(vga.framebuffer, 45, 37) == 0x11);
    FRAME(3, 1);
    CHECK(PIX(vga.framebuffer, 109, 0) == PIX(source, 109, 24));
    CHECK(PIX(vga.framebuffer, 109, 11) == PIX(source, 109, 35));
    CHECK(PIX(vga.framebuffer, 109, 12) == 0x11);
    FRAME(5, 1);
    CHECK(PIX(vga.framebuffer, 75, 191) == 0x11);
    FRAME(6, 1);
    CHECK(PIX(vga.framebuffer, 75, 182) == PIX(source, 75, 155));
    CHECK(PIX(vga.framebuffer, 244, 197) == PIX(source, 244, 170));
    FRAME(8, 1);
    CHECK(PIX(vga.framebuffer, 129, 188) == 0x11);
    FRAME(9, 1);
    CHECK(PIX(vga.framebuffer, 129, 184) == PIX(source, 129, 172));
    CHECK(PIX(vga.framebuffer, 191, 198) == PIX(source, 191, 186));
    FRAME(11, 1);
    CHECK(PIX(vga.framebuffer, 129, 176) == PIX(source, 129, 172));
    FRAME(12, 1);
    CHECK(PIX(vga.framebuffer, 129, 172) == PIX(source, 129, 172));
#undef FRAME
#undef PIX
    return 0;
}

static int test_modal_preserves_held_lifecycle(void) {
    reset_controls();
    hold_enter = 1;
    fd2_field_game game;
    CHECK(init_game(&game) == 0);
    fd2_vga vga = {0};
    fd2_input_init(&vga.input);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, test_clock);
    uint64_t repeat_at = vga.input.next_repeat_ms[SDL_SCANCODE_RETURN];
    CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_OK);
    CHECK(vga.input.previous_down[SDL_SCANCODE_RETURN]);
    /* modal内其它make已接管单last-key；clear保物理held，不恢复旧Return。 */
    CHECK(!vga.input.next_repeat_ms[SDL_SCANCODE_RETURN] &&
        vga.input.repeat_key==SDL_SCANCODE_DOWN && !fd2_input_has_any_key(&vga.input));
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_DOWN, 0);
    fd2_input_begin_test_frame(&vga.input, test_clock + 1u);
    fd2_input_begin_test_frame(&vga.input, repeat_at + 1000u);
    CHECK(vga.input.repeat_key==SDL_SCANCODE_UNKNOWN &&
        vga.input.previous_down[SDL_SCANCODE_RETURN]);
    CHECK(!fd2_input_has_any_key(&vga.input));
    fd2_input_event event;
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 0);
    fd2_input_begin_test_frame(&vga.input, repeat_at + 1001u);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, repeat_at + 1002u);
    CHECK(fd2_input_take_key(&vga.input,&event) && !event.repeat && event.source==SDL_SCANCODE_RETURN);
    CHECK(vga.input.next_repeat_ms[SDL_SCANCODE_RETURN]==repeat_at+1502u);
    repeat_at=vga.input.next_repeat_ms[SDL_SCANCODE_RETURN];
    fd2_input_begin_test_frame(&vga.input,repeat_at-1u);
    CHECK(!fd2_input_has_any_key(&vga.input));
    fd2_input_begin_test_frame(&vga.input, repeat_at);
    CHECK(fd2_input_take_key(&vga.input, &event) && event.repeat &&
          event.source == SDL_SCANCODE_RETURN);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 0);
    fd2_input_begin_test_frame(&vga.input, repeat_at + 1);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, repeat_at + 2);
    CHECK(fd2_input_take_key(&vga.input, &event) && !event.repeat);
    fd2_field_game_close(&game);
    return 0;
}

static int test_public_loop_ends_input_frame(void) {
    reset_controls();
    route_mode = 1;
    fd2_vga vga = {0};
    fd2_input_init(&vga.input);
    fd2_archive archive = {0};
    fd2_field_play_result result = fd2_field_play_run(&vga, &archive, 0, 0, NULL, NULL, NULL, 0);
    if (result != FD2_FIELD_PLAY_RETURN_HOST_QUIT)
        fprintf(stderr,"overview dispatcher result=%d started=%d done=%d timed=%d queries=%d\n",
            (int)result,modal_started,modal_done,timed_frames,action_queries);
    CHECK(result == FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    CHECK(modal_done && next_outer_present == 1);
    CHECK(action_queries == 1 && post_modal_queries == 0);
    CHECK(menu_close_calls == 1);
    CHECK(route_owner_unchanged && test_rng.state == 0x3141);
    CHECK(allocations == 2 && frees == 2 && !bad_free);
    return 0;
}

static int test_real_background(void) {
    fd2_archive archive = {0};
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    fd2_field_game game = {0};
    fd2_vga vga = {0};
    CHECK(fd2_field_game_open(&game, &vga, &archive, 0) == 0);
    game.cursor_cell_x = game.camera_cell_x + 1;
    game.cursor_cell_y = game.camera_cell_y + 6;
    int camera_x = game.camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER;
    int camera_y = game.camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER;
    fd2_terrain_render_field_base_animated(&vga, &game.terrain, &game.map,
        camera_x, camera_y, game.terrain_visual_phase, game.idle_phase);
    fd2_field_units_render(&vga, &game.sprites, &game.units,
                           camera_x, camera_y, game.idle_phase);
    fd2_terrain_render_field_overlay(&vga, &game.terrain, &game.map,
                                     camera_x, camera_y, game.terrain_visual_phase);
    for (int y = 0; y < VGA_H; y++) {
        uint8_t *row = vga.framebuffer + y * VGA_STRIDE;
        if (y < 4 || y >= VGA_H - 4) memset(row, 0, VGA_W);
        else { memset(row, 0, 4); memset(row + VGA_W - 4, 0, 4); }
    }
    uint8_t expected[VGA_W * VGA_H];
    memcpy(expected, vga.framebuffer, sizeof(expected));
    fd2_field_interaction states[] = {FD2_FIELD_INTERACTION_BROWSE,
        FD2_FIELD_INTERACTION_UNIT_SELECTED, FD2_FIELD_INTERACTION_COMMAND,
        FD2_FIELD_INTERACTION_SYSTEM_MENU};
    for (size_t i = 0; i < sizeof(states)/sizeof(states[0]); i++) {
        game.interaction = states[i];
        game.selected_unit = 0;
        game.detail_visible = 1;
        game.detail_unit = 0;
        game.info_panel_right = 0;
        memset(vga.framebuffer, 0xff, sizeof(vga.framebuffer));
        fd2_field_game_render_background(&game, &vga);
        CHECK(memcmp(expected, vga.framebuffer, sizeof(expected)) == 0);
        CHECK(game.info_panel_right == 0);
    }
    game.detail_visible = 0;
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    game.selected_unit = -1;
    fd2_field_game_render(&game, &vga);
    CHECK(game.info_panel_right == 1);
    CHECK(memcmp(expected, vga.framebuffer, sizeof(expected)) != 0);
    fd2_field_game_close(&game);
    fd2_archive_close(&archive);
    return 0;
}

static void set_app_clocks(fd2_field_game *game,fd2_vga *vga) {
    vga->field_visual.actor_idle_phase = 2;
    vga->field_visual.cursor_phase = 3;
    vga->field_visual.terrain_phase = 1;
    vga->field_visual.range_phase = 7;
    vga->actor_jitter_phase = 1;
    vga->field_water_phase = 6;
    vga->field_water_last_tick = 2;
    game->visual_owner = &vga->field_visual;
    game->idle_phase = 2;
}

/* 原1b1e7入口无1297d/water，稳态有完整有限叶顺序；不把纯renderer
 * 当作clock推进，也不因复用game_tick而改变无关deadline。 */
static int test_app_clock_entry_and_steady_order(void) {
    reset_controls(); close_frame = 13; trace_clocks = 1;
    fd2_field_game game;
    CHECK(init_game(&game) == 0);
    fd2_vga vga = {0}; fd2_input_init(&vga.input);
    set_app_clocks(&game, &vga);
    CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_OK);
    CHECK(!strcmp(clock_order, "TJRAWTJR"));
    CHECK(entry_visual.actor_idle_phase == 2 && entry_visual.actor_idle_tick == 0 && entry_visual.cursor_phase == 3);
    CHECK(entry_visual.terrain_phase == 0 && entry_visual.terrain_tick == 18 &&
          entry_visual.range_phase == 8 && entry_visual.range_tick == 18);
    CHECK(entry_jitter_tick == 18 && entry_jitter_phase == 0);
    CHECK(entry_water_phase == 6 && entry_water_tick == 2);
    CHECK(steady_visual.actor_idle_phase == 3 && steady_visual.actor_idle_tick == 21 && steady_visual.cursor_phase == 0);
    CHECK(steady_visual.terrain_phase == 1 && steady_visual.terrain_tick == 21 &&
          steady_visual.range_phase == 9 && steady_visual.range_tick == 21);
    CHECK(vga.actor_jitter_tick == 21 && vga.actor_jitter_phase == 1);
    CHECK(vga.field_water_phase == 7 && vga.field_water_last_tick == 21);
    CHECK(game.visual_now_ns == UINT64_C(1192000000) && game.phase_next_action_ms == 12345 && test_rng.state == 0x3141);
    CHECK(!bad_free && allocations == 2 && frees == 2);
    fd2_field_game_close(&game);
    return 0;
}

static int test_app_clock_preflight_and_error_boundaries(void) {
    for (int fault = 0; fault < 4; fault++) {
        reset_controls(); trace_clocks = 1;
        fd2_field_game game;
        CHECK(init_game(&game) == 0);
        fd2_vga vga = {0}; fd2_input_init(&vga.input);
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        set_app_clocks(&game, &vga);
        if (fault < 2) fail_allocation = fault+1;
        if (fault == 2) game.ui_text.fragment_count = 598;
        if (fault == 3) vga.field_visual.terrain_phase = 2;
        fd2_field_visual_state before = vga.field_visual;
        CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_ERROR);
        CHECK(!memcmp(&before, &vga.field_visual, sizeof(before)));
        CHECK(vga.actor_jitter_tick == 0 && vga.actor_jitter_phase == 1 &&
              vga.field_water_phase == 6 && vga.field_water_last_tick == 2);
        CHECK(!background_calls && !timed_frames && test_rng.state == 0x3141);
        CHECK(!strcmp(clock_order, fault == 3 ? "T" : ""));
        for (size_t i = 0; i < sizeof(vga.framebuffer); i++) CHECK(vga.framebuffer[i] == 0x77);
        CHECK(!allocated[0] && !allocated[1] && !bad_free);
        fd2_field_game_close(&game);
    }
    return 0;
}

static int test_actor_phase_api_normal_and_error_contract(void) {
    reset_controls();
    fd2_field_game game;
    CHECK(init_game(&game) == 0);
    fd2_vga vga = {0}; fd2_input_init(&vga.input); set_app_clocks(&game, &vga);
    fd2_field_visual_state before = vga.field_visual;
    CHECK(fd2_field_game_tick_actor_phase_visual(&game,(1000)*UINT64_C(1000000)) == 0);
    CHECK(vga.field_visual.actor_idle_phase == 3 && vga.field_visual.actor_idle_tick == 18 &&
          vga.field_visual.cursor_phase == 0 && game.idle_phase == 3);
    CHECK(vga.field_visual.terrain_phase == before.terrain_phase && vga.field_visual.terrain_tick == before.terrain_tick &&
          vga.field_visual.range_phase == before.range_phase && vga.field_visual.range_tick == before.range_tick);
    CHECK(!game.visual_now_ns && !vga.actor_jitter_tick && vga.actor_jitter_phase == 1 &&
          vga.field_water_phase == 6 && vga.field_water_last_tick == 2);
    CHECK(fd2_field_game_tick_visual(&game,(1000)*UINT64_C(1000000)) == 0);
    CHECK(vga.field_visual.actor_idle_phase == 3 && vga.field_visual.cursor_phase == 1 &&
          vga.field_visual.terrain_phase == 0 && vga.field_visual.range_phase == 8);
    vga.field_visual.actor_idle_phase = 4; before = vga.field_visual;
    CHECK(fd2_field_game_tick_actor_phase_visual(&game,(2000)*UINT64_C(1000000)) == -1 &&
          !memcmp(&before, &vga.field_visual, sizeof(before)));
    vga.field_visual.actor_idle_phase = 3; game.ready = 0; before = vga.field_visual;
    CHECK(fd2_field_game_tick_actor_phase_visual(&game,(2000)*UINT64_C(1000000)) == -1 &&
          !memcmp(&before, &vga.field_visual, sizeof(before)));
    game.ready = 1; vga.field_visual.terrain_phase = 2;
    unsigned cursor_before = vga.field_visual.cursor_phase;
    /* 组合口保已有部分错误合同：1297d成功后，11eee拒坏状态不回滚前者。 */
    CHECK(fd2_field_game_tick_visual(&game,(1000)*UINT64_C(1000000)) == -1 &&
          vga.field_visual.cursor_phase == (cursor_before+1u)%4u && vga.field_visual.terrain_phase == 2);
    CHECK(test_rng.state == 0x3141 && game.phase_next_action_ms == 12345);
    fd2_field_game_close(&game);
    return 0;
}

int main(void) {
    CHECK(test_actor_phase_api_normal_and_error_contract() == 0);
    CHECK(test_app_clock_entry_and_steady_order() == 0);
    CHECK(test_app_clock_preflight_and_error_boundaries() == 0);
    CHECK(test_modal_keys_and_owner() == 0);
    CHECK(test_opening_early_keys_wait_until_first_stable_clear()==0);
    CHECK(test_modal_failure_and_quit() == 0);
    CHECK(test_modal_preconditions() == 0);
    CHECK(test_transition_geometry() == 0);
    CHECK(test_modal_preserves_held_lifecycle() == 0);
    CHECK(test_public_loop_ends_input_frame() == 0);
    CHECK(test_real_background() == 0);
    puts("field_play_overview_test: ok");
    return 0;
}

/* 正式视觉接口使用NS；本测试仍沿原毫秒调度器。 */
uint64_t overview_ticks_ns(void) { return overview_ticks()*UINT64_C(1000000); }
