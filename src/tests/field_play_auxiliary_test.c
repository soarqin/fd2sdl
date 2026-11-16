#include "field_visual.h"
#include <stdint.h>
/* F1/PageUp纯绘制与modal无窗口回归；测试TU控制呈现，不新增生产hook。
 * 依据field_auxiliary_map_execute @0x2000a（原sub_2000A）、field_view_render_scaled @0x1f558、
 * field_auxiliary_marker_draw @0x1f6ef（原sub_1F6EF）。原版probe02只提供原版运行证据，尚无SDL配对。 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "field_unit_base.h"
#include "scene.h"
#include "bios_clock.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint32_t cells[64 * 64];
static fd2_field_page *test_page;
static uint8_t page_snapshot[FD2_FIELD_PAGE_SIZE],page_readback[FD2_FIELD_PAGE_SIZE];
static int bad_page,close_reads;
static SDL_Scancode close_read_source;
static fd2_image test_frames[288];
static fd2_terrain_attr attrs[288];
static uint8_t pixels[288][24 * 24];
static uint64_t clock_ms;
static int timed_frames, close_frame, quit_frame, restored, old_pulse;
static int render_calls, tick_calls, hold_f1, route_mode, outer_frames;
static int refresh_calls, fail_refresh;
static int after_restore_queries, route_closed;
static int bad_geometry, bad_pulse;
static int water_calls, water_changes, bad_frozen_water, bad_restore_order;
static int frozen_water_valid;
static uint8_t frozen_dac[768], frozen_water_phase;
static uint16_t frozen_water_tick;
static uint64_t restore_water_ns, pulse_start_ms;
static int water_failure_probe;
static int finite_frames,modal_active,source_timed_frames;
static int steady_presents,host_pumps,host_polls;
static fd2_vga *pump_vga;
static uint64_t source_clock[4];
static unsigned source_clock_count,source_clock_reads;
static int source_clock_mode,source_clock_bad;
static void (*source_clock_observer)(unsigned);

/* 独立PIT有理数：本fixture均在数秒内，直接乘法不会溢出。 */
static uint16_t oracle_tick_low_ns(uint64_t ns) {
    return (uint16_t)(ns*1193182u/(65536u*UINT64_C(1000000000)));
}
static int32_t oracle_tick_signed_ms(uint64_t ms) {
    uint16_t tick=oracle_tick_low_ns(ms*UINT64_C(1000000));
    return tick<32768u?(int32_t)tick:(int32_t)tick-65536;
}
/* 2000a稳态按每次16ms fixture宿主采样读WORD，以unsigned DWORD差>=2
 * 推进一步；原16项序列独立于生产auxiliary_next_phase，不能固定每7帧。 */
static int oracle_pulse_phase(int frame) {
    static const int phases[16]={7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7};
    unsigned index=0;int32_t last=oracle_tick_signed_ms(pulse_start_ms);
    for(int prior=8;prior<frame;prior++) {
        int32_t now=oracle_tick_signed_ms(pulse_start_ms+(uint64_t)(prior-7)*16u);
        if((uint32_t)now-(uint32_t)last>=2u) {index=(index+1u)&15u;last=now;}
    }
    return phases[index];
}
static int oracle_body_count(void) {
    int bodies=1;int32_t last=oracle_tick_signed_ms(pulse_start_ms);
    for(int sample=9;sample<=close_frame;sample++) {
        int32_t now=oracle_tick_signed_ms(pulse_start_ms+(uint64_t)(sample-8)*16u);
        if((uint32_t)now-(uint32_t)last>=2u){bodies++;last=now;}
    }
    return bodies;
}
static SDL_Scancode close_key;
static fd2_field_game route_before;
static fd2_field_rng test_rng;
static uint16_t route_rng_before;
static fd2_field_game *live_game;
static int check_scaled_frame(const fd2_field_game *, const fd2_vga *, int);

static void init_game(fd2_field_game *game) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->active_side = 2;
    game->selected_unit = game->detail_unit = game->attack_target = -1;
    game->detail_acknowledged_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->map = (fd2_field_map){24, 24, cells};
    game->terrain.sheet.width = game->terrain.sheet.height = 24;
    game->terrain.sheet.frame_count = 288;
    game->terrain.frames = test_frames;
    game->terrain.attrs = attrs;
    game->terrain.attr_count = 288;
    game->camera_cell_x = 5;
    game->camera_cell_y = 11;
    game->cursor_cell_x = 8;
    game->cursor_cell_y = 15;
    game->turn_number = 13;
    game->resource_value = 123456;
    game->idle_phase = 3;
    game->terrain_visual_phase = 1;
    game->last_idle_ms = game->last_terrain_visual_ms = clock_ms;
    game->units.count = 2;
    game->units.items[0] = (fd2_field_unit){.x=7,.y=14,.side=2,.hp=1};
    game->units.items[1] = (fd2_field_unit){.x=3,.y=4,.side=0,.hp=1};
    game->roster_count = 1;
    memset(game->roster, 0xa5, sizeof(game->roster));
    game->pending_cell_event.valid = 0;
    fd2_field_system_menu_init(&game->system_menu);
    fd2_field_rng_seed(&test_rng, 0x3141);
    game->attack_rng = fd2_field_rng_next;
    game->attack_rng_userdata = &test_rng;
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24; x++)
            cells[y * 24 + x] = (uint32_t)((x * 7 + y * 13) % 288) | 0xf0000000u;
    for (size_t i = 0; i < 288; i++) {
        test_frames[i] = (fd2_image){24, 24, pixels[i]};
        attrs[i].flags = 0x1c; /* 非零动画phase会选不同帧；本页须原始ID。 */
        for (int y = 0; y < 24; y++)
            for (int x = 0; x < 24; x++)
                pixels[i][y * 24 + x] = (uint8_t)(i + y * 3 + x * 5);
    }
    live_game = game;
}

static void controls_reset(void) {
    fd2_bios_clock_reset(0,0);
    clock_ms = 1000;
    timed_frames = restored = old_pulse = render_calls = tick_calls = 0;
    refresh_calls=fail_refresh=0;
    hold_f1 = route_mode = outer_frames = after_restore_queries = route_closed = 0;
    bad_geometry = bad_pulse = 0;
    water_calls = water_changes = bad_frozen_water = bad_restore_order = 0;
    water_failure_probe=finite_frames=modal_active=source_timed_frames=0;
    steady_presents=host_pumps=host_polls=0;pump_vga=NULL;
    source_clock_count=source_clock_reads=0;source_clock_mode=source_clock_bad=0;
    source_clock_observer=NULL;
    frozen_water_valid = 0;
    restore_water_ns = pulse_start_ms = 0;
    close_frame = 11;
    quit_frame = 0;
    close_key = SDL_SCANCODE_A;
    live_game = NULL;
    bad_page=close_reads=0;close_read_source=SDL_SCANCODE_UNKNOWN;
    memset(page_snapshot,0xd3,sizeof(page_snapshot));
    if(fd2_field_page_write(test_page,0,0,456,336,page_snapshot,sizeof(page_snapshot),456)!=0)abort();
}

static uint64_t auxiliary_ticks(void) { return clock_ms; }
static void auxiliary_wait(fd2_vga *vga) {
    vga->frame_deadline_ns = vga->frame_interval_ns = 0;
}

static void input_frame(fd2_vga *vga, int frame) {
    for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
        fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
    if (hold_f1) fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_F1, 1);
    if (frame == 3 || frame == close_frame + 2) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_DOWN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_G, 1);
    }
    if (frame == close_frame) {
        fd2_input_set_test_key_state(&vga->input, close_key, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    }
    fd2_input_begin_test_frame(&vga->input, clock_ms);
    if (frame == quit_frame && quit_frame > 0) vga->input.quit_requested = 1;
}

static void auxiliary_submit(fd2_vga *vga,uint32_t ms) {
    if(water_failure_probe) {
        timed_frames++;clock_ms+=ms;
        fd2_input_begin_test_frame(&vga->input,clock_ms);
        if(water_calls)vga->input.quit_requested=1; /* 旧吞错实现最多多present一轮即失败。 */
        return;
    }
    if (fd2_input_has_any_key(&vga->input)) old_pulse = 1;
    timed_frames++;
    if(fd2_field_page_read(vga->field_page,0,0,456,336,page_readback,sizeof(page_readback),456)!=0)bad_page=1;
    if(timed_frames<=7 || timed_frames>close_frame) {
        if(memcmp(page_readback,vga->framebuffer,64000))bad_page=1;
        memcpy(page_snapshot,page_readback,64000);
    } else if(memcmp(page_readback,page_snapshot,64000))bad_page=1; /* marker只写VGA。 */
    for(size_t i=64000;i<sizeof(page_readback);i++)if(page_readback[i]!=0xd3)bad_page=1;
    if (!frozen_water_valid) {
        memcpy(frozen_dac, vga->dac, sizeof(frozen_dac));
        frozen_water_phase = vga->field_water_phase;
        frozen_water_tick = vga->field_water_last_tick;
        frozen_water_valid = 1;
    }
    if (memcmp(frozen_dac, vga->dac, sizeof(frozen_dac)) != 0 ||
        frozen_water_phase != vga->field_water_phase ||
        frozen_water_tick != vga->field_water_last_tick)
        bad_frozen_water = 1; /* 7开/稳定/6关的每次present前均检查。 */
    if (timed_frames <= 7 || timed_frames > close_frame) {
        int phase = timed_frames <= 7 ? timed_frames : close_frame + 7 - timed_frames;
        if (check_scaled_frame(live_game, vga, phase) != 0) bad_geometry = 1;
    } else {
        int expected = oracle_pulse_phase(timed_frames);
        int x = live_game->cursor_cell_x * 4 + 112;
        int y = live_game->cursor_cell_y * 4 + 52;
        if (vga->framebuffer[y * VGA_STRIDE + x] != 24 + expected) bad_pulse = 1;
    }
    clock_ms += ms;
    if(timed_frames==7)pulse_start_ms=clock_ms;
    vga->frame_deadline_ns = clock_ms * 1000000u;
    vga->frame_interval_ns = (uint64_t)ms * 1000000u;
    input_frame(vga, timed_frames);
}
/* finite提交及host pump均选择16ms外部耗时保旧输入时间向量；
 * timed/实际marker提交分别计数，不把宿主采样次数称为画面提交数。 */
static void auxiliary_present_timed(fd2_vga *vga,uint32_t ms) {
    source_timed_frames++;
    if(modal_active && (timed_frames<7 || close_reads))bad_geometry=1;
    auxiliary_submit(vga,ms);
}
static int auxiliary_prefix(fd2_field_page *page,const uint8_t *source_pixels,size_t size) {
    int result=fd2_field_page_phase_prefix_write(page,source_pixels,size);
    if(!result)modal_active=1;
    return result;
}
static int auxiliary_read_key(fd2_input *input,fd2_input_event *event) {
    fd2_input_event read;int result=fd2_input_take_key(input,&read);
    if(result) {
        close_reads++;close_read_source=read.source;
        if(timed_frames!=close_frame)bad_page=1;
        if(event)*event=read;
    }
    return result;
}

static void auxiliary_present(fd2_vga *vga) {
    if(water_failure_probe) {
        finite_frames++;
        if(water_calls)vga->input.quit_requested=1;
        return;
    }
    if(modal_active && !restored && (timed_frames<7 || (close_reads && timed_frames<close_frame+6))) {
        pump_vga=vga;
        finite_frames++;auxiliary_submit(vga,16);
        vga->frame_deadline_ns=vga->frame_interval_ns=0;
        return;
    }
    if(modal_active && !restored && !close_reads) {
        /* 只有实际source body会提交；空轮时钟/输入由下面pump推进。 */
        pump_vga=vga;
        steady_presents++;
        vga->frame_deadline_ns=vga->frame_interval_ns=0;
        return;
    }
    if (route_mode && timed_frames == 0) {
        outer_frames++;
        clock_ms += 16;
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_F1, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga->input, clock_ms);
        route_before = *live_game;
        route_rng_before = ((fd2_field_rng *)live_game->attack_rng_userdata)->state;
    } else if (!restored) {
        restored = 1;
        clock_ms += 16;
        input_frame(vga, 0);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_DOWN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        fd2_input_begin_test_frame(&vga->input, clock_ms);
        if (quit_frame == -1) vga->input.quit_requested = 1;
    } else if (route_mode) {
        outer_frames++;
        vga->input.quit_requested = 1;
    }
    auxiliary_wait(vga);
}
static void auxiliary_copy_upload(fd2_vga *vga) {
    uint64_t deadline=vga->frame_deadline_ns,interval=vga->frame_interval_ns;
    vga->frame_deadline_ns=vga->frame_interval_ns=0;
    auxiliary_present(vga);
    vga->frame_deadline_ns=deadline;vga->frame_interval_ns=interval;
}
static void auxiliary_poll(fd2_input *input) {
    if(modal_active && !restored && !close_reads) {
        if(!pump_vga || input!=&pump_vga->input)bad_page=1;
        host_polls++;return;
    }
    fd2_input_poll_host_events(input);
}
static void auxiliary_delay(uint32_t ms) {
    if(modal_active && !restored && !close_reads && ms==1u) {
        host_pumps++;
        /* 请求1ms的H让出允许被测试环境调度为16ms；非生产固定帧门。 */
        if(!pump_vga){bad_page=1;return;}
        uint64_t deadline=pump_vga->frame_deadline_ns,interval=pump_vga->frame_interval_ns;
        auxiliary_submit(pump_vga,16);
        pump_vga->frame_deadline_ns=deadline;pump_vga->frame_interval_ns=interval;
        return;
    }
    fd2_delay_ms(ms);
}

static void auxiliary_render(fd2_field_game *game, fd2_vga *vga) {
    (void)game;
    render_calls++;
    if (timed_frames && !restored && water_calls == 0) bad_restore_order = 1;
    memset(vga->framebuffer, 0x99, sizeof(vga->framebuffer));
}
static void auxiliary_tick(fd2_field_game *game, uint64_t now) {
    (void)game; (void)now;
    tick_calls++;
}
static int auxiliary_open(fd2_field_game *game, fd2_vga *vga,
                            const fd2_archive *archive, size_t stage) {
    (void)vga; (void)archive; (void)stage;
    init_game(game);
    live_game = game;
    return 0;
}
/* 正式dispatcher本用例只拥有synthetic map/cache；像overview同类夹具，
 * App背景重建单独由真实资源用例验，不借缺失FDOTHER制造错误出口。 */
static int auxiliary_bind(fd2_field_game *game,fd2_vga *vga) {
    return route_mode ? game && game->ready && vga && vga->field_page ? 0:-1:
        fd2_field_game_bind_visual(game,vga);
}
static void auxiliary_close(fd2_field_game *game) {
    /* Play先解除物理/法术/共享runtime的同步借用；其余Game与RNG逐字保持。 */
    int detached = !game->physical_present && !game->physical_present_userdata &&
        !game->magic_present && !game->magic_present_userdata &&
        !game->runtime_present && !game->runtime_present_userdata;
    route_before.physical_present = NULL; route_before.physical_present_userdata = NULL;
    route_before.magic_present = NULL; route_before.magic_present_userdata = NULL;
    route_before.runtime_present=NULL;route_before.runtime_present_userdata=NULL;
    route_closed = detached && live_game == game &&
        memcmp(game, &route_before, sizeof(*game)) == 0 &&
        ((fd2_field_rng *)game->attack_rng_userdata)->state == route_rng_before;
}
static int auxiliary_take_action(fd2_input *input, fd2_input_context context,
                                   fd2_input_action *action) {
    if (restored && outer_frames == 1) after_restore_queries++;
    return fd2_input_take_action(input, context, action);
}

static int auxiliary_water_tick(fd2_vga *vga, uint64_t now) {
    water_calls++;
    if (timed_frames > 0 && !restored) {
        if (timed_frames != close_frame + 6) bad_restore_order = 1;
        restore_water_ns = now;
    }
    int result = fd2_field_water_tick(vga,now); /* 调用者已给NS，不再次缩放。 */
    if (result == 1) water_changes++;
    return result;
}

/* 私有Play TU只核2000a尾确实委托完整11cac(0)；相位写序由Game真资源用例核。 */
static int auxiliary_runtime_refresh(fd2_field_game *game,fd2_vga *vga,int mode) {
    refresh_calls++;
    if(mode!=0 || fail_refresh)return -1;
    if(auxiliary_water_tick(vga,clock_ms*UINT64_C(1000000))<0)return -1;
    auxiliary_render(game,vga);return 0;
}

#define fd2_field_water_tick auxiliary_water_tick
#define fd2_field_water_tick_live(vga_) auxiliary_water_tick((vga_),SDL_GetTicksNS())
#define SDL_GetTicks auxiliary_ticks
uint64_t auxiliary_ticks_ns(void);
#define SDL_GetTicksNS auxiliary_ticks_ns
#define fd2_vga_present auxiliary_present
#define fd2_vga_upload_current_frame auxiliary_copy_upload
#define fd2_vga_present_timed auxiliary_present_timed
#define fd2_vga_wait_frame_deadline auxiliary_wait
#define fd2_field_game_render auxiliary_render
#define fd2_field_game_runtime_refresh auxiliary_runtime_refresh
#define fd2_field_game_tick auxiliary_tick
#define fd2_field_game_open auxiliary_open
#define fd2_field_game_bind_visual auxiliary_bind
#define fd2_field_game_close auxiliary_close
#define fd2_input_take_action auxiliary_take_action
#define fd2_input_take_key auxiliary_read_key
#define fd2_input_poll_host_events auxiliary_poll
#define fd2_delay_ms auxiliary_delay
#define fd2_field_page_phase_prefix_write auxiliary_prefix
#include "field_play.c"
#undef fd2_field_page_phase_prefix_write
#undef fd2_field_water_tick_live
#undef fd2_field_water_tick
#undef SDL_GetTicks
#undef fd2_vga_upload_current_frame
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef fd2_field_game_render
#undef fd2_field_game_runtime_refresh
#undef fd2_field_game_tick
#undef fd2_field_game_open
#undef fd2_field_game_bind_visual
#undef fd2_field_game_close
#undef fd2_input_take_action
#undef fd2_input_take_key
#undef fd2_input_poll_host_events
#undef fd2_delay_ms

static int floor_tile(int value) {
    return value >= 0 ? value / 3072 : -((-value + 3071) / 3072);
}

static int check_scaled_frame(const fd2_field_game *game, const fd2_vga *vga,
                                int phase) {
    int step = 128 + phase * (game->map.height>40 ? 896:640) / 7;
    int start_x = game->camera_cell_x * 3072 + 13 * 1536;
    int start_y = game->camera_cell_y * 3072 + 8 * 1536;
    int cx = start_x + (game->map.width * 1536 - start_x) * (phase + 1) / 8;
    int cy = start_y + (game->map.height * 1536 - start_y) * (phase + 1) / 8;
    int checked_zero = 0, checked_negative = 0;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t expected = 0;
            if (x >= 4 && x < 316 && y >= 4 && y < 196) {
                int wx = cx + (x - 160) * step;
                int wy = cy + (y - 100) * step;
                int tx = floor_tile(wx), ty = floor_tile(wy);
                if (wx < 0 || wy < 0) checked_negative++;
                if (tx >= 0 && tx < game->map.width && ty >= 0 && ty < game->map.height) {
                    int px = (wx - tx * 3072) / 128;
                    int py = (wy - ty * 3072) / 128;
                    unsigned id=game->map.cells[ty*game->map.width+tx]&1023u;
                    expected = (uint8_t)(id + py * 3 + px * 5);
                }
            }
            if (expected == 0) checked_zero++;
            CHECK(vga->framebuffer[y * VGA_STRIDE + x] == expected);
        }
    }
    CHECK(checked_zero > 0);
    if (phase == 7) CHECK(checked_negative > 0);
    return 0;
}

static int test_full_frame_and_transition(void) {
    fd2_field_game game;
    fd2_vga vga = {.field_page=test_page};
    controls_reset(); init_game(&game);
    fd2_field_game before = game;
    uint8_t cache_before[sizeof(pixels)];
    memcpy(cache_before, pixels, sizeof(pixels));
    CHECK(auxiliary_field_ready(&game));
    for (int phase = 1; phase <= 7; phase++) {
        memset(vga.framebuffer, 0xcd, sizeof(vga.framebuffer));
        auxiliary_transition_frame(&game, &vga, phase);
        CHECK(check_scaled_frame(&game, &vga, phase) == 0);
    }
    for (int phase = 6; phase >= 1; phase--) {
        auxiliary_transition_frame(&game, &vga, phase);
        CHECK(check_scaled_frame(&game, &vga, phase) == 0);
    }
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(memcmp(pixels, cache_before, sizeof(pixels)) == 0);
    return 0;
}

static int test_marker_filter_order_and_pulse(void) {
    fd2_field_game game;
    fd2_vga vga = {.field_page=test_page};
    controls_reset(); init_game(&game);
    game.units.count = 7;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    game.units.items[0].hp = 0; /* native不按HP/acted/AI可执行位过滤。 */
    game.units.items[1].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE;
    game.units.items[2] = (fd2_field_unit){.x=1,.y=1,.side=1};
    game.units.items[3] = (fd2_field_unit){.x=2,.y=2,.side=255,.flags=1};
    game.units.items[4] = (fd2_field_unit){.x=3,.y=3,.side=255,.unit_id=121};
    game.units.items[5] = (fd2_field_unit){.x=4,.y=4,.side=255,.race=10};
    game.units.items[6] = (fd2_field_unit){.x=3,.y=4,.side=2};
    game.cursor_cell_x = 7; game.cursor_cell_y = 14;
    CHECK(auxiliary_field_ready(&game));
    for (int phase = 0; phase < 8; phase++) {
        memset(vga.framebuffer, 0xcd, sizeof(vga.framebuffer));
        auxiliary_markers(&game, &vga, phase);
        int changed = 0;
        for (int y = 0; y < 200; y++) {
            for (int x = 0; x < 320; x++) {
                uint8_t expected = 0xcd;
                if (x >= 140 && x < 143 && y >= 108 && y < 111) expected = (uint8_t)(24 + phase);
                if (x >= 124 && x < 127 && y >= 68 && y < 71) expected = (uint8_t)(72 + phase);
                if (x >= 116 && x < 119 && y >= 56 && y < 59) expected = (uint8_t)(80 + phase);
                CHECK(vga.framebuffer[y * VGA_STRIDE + x] == expected);
                if (expected != 0xcd) changed++;
            }
        }
        CHECK(changed == 27);
    }
    int phase = 7, direction = -1;
    static const int expected[] = {7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,7,6};
    for (size_t i = 0; i < sizeof(expected)/sizeof(expected[0]); i++) {
        CHECK(phase == expected[i]);
        auxiliary_next_phase(&phase, &direction);
    }
    return 0;
}

static int test_cross_stage_shapes_height_boundary_and_page_prefix(void) {
    static const int cases[][5]={{1,27,21,1,192},{2,18,26,2,288},
        {7,29,45,7,257},{13,40,40,5,257},{14,50,41,6,257},{28,31,64,9,257}};
    for(size_t row=0;row<sizeof(cases)/sizeof(cases[0]);row++) {
        controls_reset();fd2_field_game game;init_game(&game);
        game.stage=(size_t)cases[row][0];game.map.width=cases[row][1];game.map.height=cases[row][2];
        game.metadata.shape_index=(uint8_t)cases[row][3];game.terrain.shape_index=(size_t)cases[row][3];
        game.terrain.sheet.frame_count=(size_t)cases[row][4];game.terrain.attr_count=(size_t)cases[row][4];
        for(int y=0;y<game.map.height;y++)for(int x=0;x<game.map.width;x++)
            cells[y*game.map.width+x]=(uint32_t)((x*7+y*13)%cases[row][4]);
        fd2_field_game before=game;fd2_vga vga={.field_page=test_page};
        CHECK(auxiliary_field_ready(&game));
        for(int phase=1;phase<=7;phase++) {
            CHECK(auxiliary_transition_frame(&game,&vga,phase)==0 && check_scaled_frame(&game,&vga,phase)==0);
            CHECK(fd2_field_page_read(test_page,0,0,456,336,page_readback,sizeof(page_readback),456)==0);
            CHECK(!memcmp(page_readback,vga.framebuffer,64000));
            for(size_t i=64000;i<sizeof(page_readback);i++)CHECK(page_readback[i]==0xd3);
        }
        uint8_t base[64000];memcpy(base,vga.framebuffer,sizeof(base));
        auxiliary_markers(&game,&vga,5);
        int scale=game.map.height>40?3:4;
        for(int y=0;y<200;y++)for(int x=0;x<320;x++) {
            uint8_t expected=base[y*320+x];
            for(size_t u=0;u<=game.units.count;u++) {
                int wx=u==game.units.count?game.cursor_cell_x:game.units.items[u].x;
                int wy=u==game.units.count?game.cursor_cell_y:game.units.items[u].y;
                int sx=wx*scale+160-game.map.width*scale/2,sy=wy*scale+100-game.map.height*scale/2;
                if(x>=sx && x<sx+scale-1 && y>=sy && y<sy+scale-1)
                    expected=u==game.units.count?29:game.units.items[u].side==2?77:37;
            }
            CHECK(vga.framebuffer[y*320+x]==expected);
        }
        CHECK(fd2_field_page_read(test_page,0,0,456,336,page_readback,sizeof(page_readback),456)==0 &&
            !memcmp(page_readback,base,64000)); /* marker没有回写App。 */
        CHECK(!memcmp(&game,&before,sizeof(game)) && test_rng.state==0x3141);
    }
    return 0;
}

static int test_modal_keys_owner_and_held(void) {
    const SDL_Scancode keys[] = {SDL_SCANCODE_A, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_F1, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN};
    for (size_t k = 0; k < sizeof(keys)/sizeof(keys[0]); k++) {
        fd2_field_game game;
        fd2_vga vga = {.field_page=test_page};
        controls_reset(); init_game(&game);
        close_key = keys[k];
        hold_f1 = close_key != SDL_SCANCODE_F1;
        fd2_input_init(&vga.input);
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_F1, 1);
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga.input, clock_ms);
        /* 同一fallback快照按scan处理，Right最后make，F1不保独立deadline。 */
        uint64_t repeat_at = vga.input.next_repeat_ms[SDL_SCANCODE_RIGHT];
        CHECK(vga.input.repeat_key==SDL_SCANCODE_RIGHT &&
            !vga.input.next_repeat_ms[SDL_SCANCODE_F1]);
        memset(vga.palette, 11, sizeof(vga.palette));
        memset(vga.dac, 37, sizeof(vga.dac)); /* 非初始palette，不得重置水DAC。 */
        vga.field_water_phase = 7;
        vga.field_water_last_tick = oracle_tick_low_ns(clock_ms*UINT64_C(1000000));
        fd2_field_game before = game;
        vga.frame_deadline_ns=UINT64_C(999999999999);vga.frame_interval_ns=UINT64_C(23000000);
        CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_OK);
        CHECK(finite_frames==13 && !source_timed_frames && host_pumps==close_frame-7 && host_polls==host_pumps);
        CHECK(steady_presents==oracle_body_count());
        CHECK(timed_frames == close_frame + 6 && restored && render_calls == 1);
        CHECK(refresh_calls==1);
        CHECK(old_pulse && close_reads==1 && close_read_source==close_key);
        fd2_input_event queued;
        if(close_key!=SDL_SCANCODE_RIGHT) {
            CHECK(fd2_input_take_key(&vga.input,&queued) && queued.source==SDL_SCANCODE_RIGHT);
        }
        const SDL_Scancode remaining[]={SDL_SCANCODE_G,SDL_SCANCODE_DOWN,SDL_SCANCODE_RETURN,SDL_SCANCODE_DOWN};
        for(size_t i=0;i<sizeof(remaining)/sizeof(remaining[0]);i++)
            CHECK(fd2_input_take_key(&vga.input,&queued) && queued.source==remaining[i]);
        CHECK(!fd2_input_has_any_key(&vga.input) && !bad_page);
        CHECK(!bad_geometry && !bad_pulse);
        CHECK(!tick_calls && test_rng.state == 0x3141);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0);
        for (size_t p = 0; p < sizeof(vga.framebuffer); p++) CHECK(vga.framebuffer[p] == 0x99);
        for (size_t p = 0; p < sizeof(vga.palette); p++) CHECK(vga.palette[p] == 11);
        fd2_vga expected_dac = {0};
        memset(expected_dac.dac, 37, sizeof(expected_dac.dac));
        CHECK(fd2_field_water_palette_apply(&expected_dac, 8) == 0);
        CHECK(memcmp(vga.dac, expected_dac.dac, sizeof(vga.dac)) == 0);
        CHECK(!bad_frozen_water && !bad_restore_order);
        CHECK(water_calls == 1 && water_changes == 1);
        CHECK(vga.field_water_phase == 8 &&
              vga.field_water_last_tick == oracle_tick_low_ns(restore_water_ns));
        CHECK(vga.frame_deadline_ns==UINT64_C(999999999999) && vga.frame_interval_ns==UINT64_C(23000000));
        if (hold_f1) {
            CHECK(vga.input.previous_down[SDL_SCANCODE_F1]);
            /* 中间close/Down/Return的make已替换last-key；clear保held并不
             * 意味着每个held都有repeat，松开last-key也不恢复旧F1。 */
            CHECK(!vga.input.next_repeat_ms[SDL_SCANCODE_F1]);
            fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_DOWN, 0);
            fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 0);
            fd2_input_begin_test_frame(&vga.input, clock_ms + 1u);
            fd2_input_begin_test_frame(&vga.input, repeat_at + 1000u);
            CHECK(vga.input.repeat_key==SDL_SCANCODE_UNKNOWN);
            CHECK(!fd2_input_has_any_key(&vga.input));
            fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_F1, 0);
            fd2_input_begin_test_frame(&vga.input, repeat_at + 1001u);
            fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_F1, 1);
            fd2_input_begin_test_frame(&vga.input, repeat_at + 1002u);
            fd2_input_event event;
            CHECK(fd2_input_take_key(&vga.input,&event) && !event.repeat && event.source==SDL_SCANCODE_F1);
            uint64_t new_repeat_at=vga.input.next_repeat_ms[SDL_SCANCODE_F1];
            CHECK(new_repeat_at==repeat_at+1002u+500u);
            fd2_input_begin_test_frame(&vga.input,new_repeat_at-1u);
            CHECK(!fd2_input_has_any_key(&vga.input));
            fd2_input_begin_test_frame(&vga.input,new_repeat_at);
            CHECK(fd2_input_take_key(&vga.input, &event) && event.repeat && event.source == SDL_SCANCODE_F1);
        }
    }
    controls_reset();
    fd2_field_game game;
    fd2_vga vga = {.field_page=test_page};
    init_game(&game);
    fd2_input_init(&vga.input);
    close_frame = 130; /* mock时钟跨过0/7的两个重复端点，不实际等待。 */
    vga.field_water_phase = 15;
    vga.field_water_last_tick = oracle_tick_low_ns(clock_ms*UINT64_C(1000000));
    CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_OK);
    CHECK(!bad_geometry && !bad_pulse && timed_frames == 136);
    CHECK(!bad_frozen_water && !bad_restore_order);
    CHECK(water_calls == 1 && water_changes == 1 && vga.field_water_phase == 0);
    CHECK(vga.field_water_last_tick == oracle_tick_low_ns(restore_water_ns));
    return 0;
}

static int test_invalid_preflight_and_quit(void) {
    for (int mode = 0; mode < 15; mode++) {
        fd2_field_game game;
        fd2_vga vga = {.field_page=test_page};
        controls_reset(); init_game(&game);
        fd2_input_init(&vga.input);
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        if (mode == 0) game.map.width = 65;
        if (mode == 1) game.active_side = 0;
        if (mode == 2) game.selected_unit = 0;
        if (mode == 3) game.interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
        if (mode == 4) game.detail_visible = 1;
        if (mode == 5) game.failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
        if (mode == 6) game.terrain.sheet.width = 23;
        if (mode == 7) game.terrain.sheet.frame_count = 0;
        if (mode == 8) test_frames[0].pixels = NULL;
        if (mode == 9) game.cursor_cell_x = INT_MAX;
        if (mode == 10) game.camera_cell_y = INT_MIN;
        if (mode == 11) game.units.count = FD2_FIELD_MAX_UNITS + 1;
        if (mode == 12) game.units.items[0].side = 3;
        if (mode == 13) game.units.items[0].x = 24;
        if (mode == 14) game.player_post_waiting_phase = 1;
        fd2_field_game before = game;
        CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_ERROR);
        CHECK(!timed_frames && !restored && !render_calls && !tick_calls);
        CHECK(water_calls == 0 && water_changes == 0);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && test_rng.state == 0x3141);
        for (size_t p = 0; p < sizeof(vga.framebuffer); p++) CHECK(vga.framebuffer[p] == 0x77);
    }
    const int quit_at[] = {1, 7, 8, 11, 13, 17, -1, 0};
    for (size_t q = 0; q < sizeof(quit_at)/sizeof(quit_at[0]); q++) {
        fd2_field_game game;
        fd2_vga vga = {.field_page=test_page};
        controls_reset(); init_game(&game);
        fd2_input_init(&vga.input);
        vga.frame_deadline_ns=UINT64_C(999999999999);vga.frame_interval_ns=UINT64_C(23000000);
        quit_frame = quit_at[q];
        if (quit_frame == 0) {
            vga.input.quit_requested = 1;
            game.terrain.frames = NULL; /* quit先于资源预检。 */
        }
        fd2_field_game before = game;
        CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_HOST_QUIT);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && !tick_calls);
        CHECK(vga.frame_deadline_ns==UINT64_C(999999999999) && vga.frame_interval_ns==UINT64_C(23000000));
        CHECK(timed_frames == (quit_frame > 0 ? quit_frame : quit_frame < 0 ? close_frame + 6 : 0));
        CHECK(!bad_frozen_water && !bad_restore_order);
        CHECK(water_calls == (quit_frame < 0 ? 1 : 0));
        CHECK(water_changes == (quit_frame < 0 ? 1 : 0));
    }
    return 0;
}

static int test_formal_route_no_key_fallthrough(void) {
    for (int key = 0; key < 2; key++) {
        controls_reset();
        route_mode = 1;
        close_key = key ? SDL_SCANCODE_RETURN : SDL_SCANCODE_RIGHT;
        fd2_vga vga = {.field_page=test_page};
        fd2_input_init(&vga.input);
        fd2_archive archive = {0};
        CHECK(fd2_field_play_run(&vga, &archive, 0, 0, NULL, NULL, NULL, 0) == FD2_FIELD_PLAY_RETURN_HOST_QUIT);
        CHECK(restored && route_closed && outer_frames == 2);
        CHECK(!after_restore_queries && timed_frames == close_frame + 6);
        CHECK(tick_calls == 0); /* 11aa8由BIOS门委托11cac，不再每outer轮调Game_tick。 */
    }
    return 0;
}

static int test_restore_failure_does_not_present_success(void) {
    controls_reset();fail_refresh=1;
    fd2_field_game game;init_game(&game);
    fd2_vga vga={.field_page=test_page};fd2_input_init(&vga.input);
    fd2_field_game before=game;
    CHECK(play_field_auxiliary(&game,&vga)==FD2_SCENE_RESULT_ERROR);
    CHECK(refresh_calls==1 && timed_frames==close_frame+6 && !restored);
    CHECK(!render_calls && !water_calls && !tick_calls);
    CHECK(!memcmp(&game,&before,sizeof(game)) && test_rng.state==0x3141);
    CHECK(!vga.frame_deadline_ns && !vga.frame_interval_ns);
    return 0;
}

static uint64_t framebuffer_hash(const fd2_vga *vga) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < sizeof(vga->framebuffer); i++)
        hash = (hash ^ vga->framebuffer[i]) * UINT64_C(1099511628211);
    return hash;
}

static int test_real_stage0_original_probe_indices(void) {
    fd2_archive archive = {0};
    fd2_field_game game = {0};
    fd2_vga vga = {.field_page=test_page};
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(&game, &vga, &archive, 0) == 0);
    CHECK(fd2_field_game_load_active(&game, "original_game/START.SAV") == 0);
    CHECK(game.units.count == 12 && game.roster_count == 4);
    /* probe02在START焦点(8,15)向左一格空地，native WGC完整帧已过。
     * 常数来自独立Python raw-map/shape oracle的64000索引字节FNV1a；
     * 包含所有黑边与0色，不忽略透明色。并非SDL截图或RGB自比较。 */
    static const uint64_t expected[8] = {
        UINT64_C(0x044a271d72c68e67), UINT64_C(0x96ad271b316edae7),
        UINT64_C(0x6db48fc463b62c9b), UINT64_C(0xb38df99d88ce5fb3),
        UINT64_C(0x7c62cdf4c0062257), UINT64_C(0x6612b50896382be7),
        UINT64_C(0xf637b6466cb3e923), UINT64_C(0x4c87740d1ee7781b)
    };
    game.cursor_cell_x = 7;
    game.cursor_cell_y = 15;
    CHECK(auxiliary_field_ready(&game));
    fd2_field_game before = game;
    uint8_t palette_before[768], dac_before[768];
    memcpy(palette_before, vga.palette, sizeof(palette_before));
    memcpy(dac_before, vga.dac, sizeof(dac_before));
    auxiliary_transition_frame(&game, &vga, 7);
    CHECK(framebuffer_hash(&vga) == UINT64_C(0xa1d60e0b92728d88));
    for (int phase = 0; phase < 8; phase++) {
        auxiliary_markers(&game, &vga, phase);
        CHECK(framebuffer_hash(&vga) == expected[phase]);
    }
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(memcmp(vga.palette, palette_before, sizeof(palette_before)) == 0);
    CHECK(memcmp(vga.dac, dac_before, sizeof(dac_before)) == 0);
    fd2_field_game_close(&game);
    fd2_archive_close(&archive);
    return 0;
}

static uint32_t water_failure_rng(void *userdata) {(*(unsigned *)userdata)++;return 7;}
/* 三个16c57 consumer的真实预检资源；非法water在首轮明确失败，
 * 不得继续读时钟、眨眼RNG或steady present。SDL故障域，不称原有效相位。 */
static int test_wait_water_error_prefix(void) {
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    for(int kind=0;kind<3;kind++) {
        controls_reset();water_failure_probe=1;
        fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));CHECK(game&&vga);
        CHECK(fd2_field_game_open_resources(game,&other,0)==0);
        game->units.count=1;CHECK(fd2_field_unit_persistent_create(&game->units.items[0],0)==0);
        fd2_field_unit *unit=&game->units.items[0];unit->flags=0;unit->side=2;unit->hp=unit->hp_max=100;
        /* 合法密集一槽，真实ID0武器；不借空库存令前检先失败。 */
        for(unsigned i=0;i<8;i++){((uint8_t *)unit)[10+i*2]=0x80;((uint8_t *)unit)[11+i*2]=255;}
        ((uint8_t *)unit)[10]=0x40;((uint8_t *)unit)[11]=0;
        game->active_side=2;game->selected_unit=0;game->interaction=FD2_FIELD_INTERACTION_ITEM_LIST;
        game->command_selected=2;game->item_list_selected=0;
        unsigned rng_calls=0;game->attack_rng=water_failure_rng;game->attack_rng_userdata=&rng_calls;
        /* 本地受控空FIFO；不把每轮free的heap input注册为全局宿主owner。 */
        vga->field_page=test_page;memset(&vga->input,0,sizeof(vga->input));vga->field_water_phase=255;
        vga->field_water_last_tick=321;memset(vga->dac,0x45,768);
        fd2_field_unit before=*unit;int result;
        if(kind==0) {
            field_progress_box box={0};
            result=progress_wait(&box,game,vga,&other,0,0);
            CHECK(result==FD2_SCENE_RESULT_ERROR && !timed_frames);
        } else if(kind==1) {
            uint8_t confirmed=211;
            result=play_field_unit_list(game,vga,NULL,FIELD_LIST_ITEM,&confirmed);
            CHECK(result==FIELD_LIST_ERROR && confirmed==211 && !timed_frames && finite_frames==12);
        } else {
            int slot=211;result=loot_inventory_select(game,vga,NULL,0,&slot);
            CHECK(result==FD2_SCENE_RESULT_ERROR && slot==211 && !timed_frames && finite_frames==12);
        }
        CHECK(water_calls==1 && !water_changes && rng_calls==1 &&
            vga->field_water_phase==255 && vga->field_water_last_tick==321 &&
            !memcmp(unit,&before,sizeof(before)) && !fd2_input_has_any_key(&vga->input));
        for(size_t p=0;p<768;p++)CHECK(vga->dac[p]==0x45);
        fd2_field_game_close(game);free(vga);free(game);
    }
    water_failure_probe=0;fd2_archive_close(&other);return 0;
}

/* 独立PIT有理数，不由生产时钟转换生成expected；65536后的正常
 * 累积tick仍观察同一BIOS低WORD回绕。 */
static uint64_t exact_tick_ns(uint32_t tick) {
    return ((uint64_t)tick*65536u*UINT64_C(1000000000)+1193181u)/1193182u;
}
static int32_t signed_low_tick(uint32_t tick) {
    unsigned low=tick&65535u;return low<32768u?(int32_t)low:(int32_t)low-65536;
}
static int *observed_phase,*observed_direction;
static int32_t *observed_anchor;
static int read_phase[2],read_direction[2];
static int32_t read_anchor[2];
static const uint8_t *read_pixels[2];
static const fd2_vga *observed_vga;
static void auxiliary_read_observer(unsigned index) {
    if(index>=2u || *observed_phase!=read_phase[index] ||
            *observed_direction!=read_direction[index] || *observed_anchor!=read_anchor[index] ||
            memcmp(observed_vga->framebuffer,read_pixels[index],64000))source_clock_bad=1;
}
/* 固定24x24夹具由2000a坐标式直接算3x3窗，literal32/72/24基色。
 * 不调用待验marker/phase函数生成expected；focus有意覆盖同格side2。 */
static void expected_auxiliary_markers(uint8_t *frame_pixels,int phase) {
    const int x[3]={7,3,7},y[3]={14,4,14},base[3]={72,32,24};
    for(unsigned i=0;i<3;i++)for(int row=0;row<3;row++)
        memset(frame_pixels+(y[i]*4+52+row)*320+x[i]*4+112,base[i]+phase,3);
}
static int test_postfirst_gate_marker_fresh_clock(void) {
    const struct {
        uint32_t prior,comparison,fresh;int due,phase,direction,next_phase,next_direction;
    } cases[]={
        {100,100,0,0,6,-1,6,-1}, {100,101,0,0,6,-1,6,-1},
        {100,102,105,1,6,-1,5,-1}, {100,99,100,1,6,-1,5,-1},
        {32767,32768,32769,1,6,-1,5,-1}, {65535,65536,0,0,6,-1,6,-1},
        {65534,65536,65537,1,6,-1,5,-1}, {100,200,200,1,6,-1,5,-1},
        {100,102,104,1,0,-1,0,1}, {100,102,103,1,7,1,7,-1}
    };
    fd2_field_game *game=malloc(sizeof(*game)),*game_before=malloc(sizeof(*game));
    fd2_vga *vga=calloc(1,sizeof(*vga));uint8_t *before=malloc(64000),*expected=malloc(64000);
    CHECK(game && game_before && vga && before && expected);
    for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
        controls_reset();init_game(game);game->cursor_cell_x=7;game->cursor_cell_y=14;*game_before=*game;
        memset(vga,0,sizeof(*vga));vga->field_page=test_page;memset(vga->framebuffer,0xa7,64000);
        memset(vga->dac,0x43,sizeof(vga->dac));vga->field_water_phase=5;vga->field_water_last_tick=0x1234;
        vga->frame_deadline_ns=123456;vga->frame_interval_ns=789;
        memcpy(before,vga->framebuffer,64000);memcpy(expected,before,64000);
        if(cases[c].due)expected_auxiliary_markers(expected,cases[c].phase);
        int phase=cases[c].phase,direction=cases[c].direction,have_anchor=1,key_ready=77,updated=77;
        int32_t anchor=signed_low_tick(cases[c].prior);
        source_clock_mode=1;source_clock_count=cases[c].due?2u:1u;
        source_clock[0]=exact_tick_ns(cases[c].comparison);source_clock[1]=exact_tick_ns(cases[c].fresh);
        observed_phase=&phase;observed_direction=&direction;observed_anchor=&anchor;observed_vga=vga;
        read_phase[0]=phase;read_direction[0]=direction;read_anchor[0]=anchor;read_pixels[0]=before;
        read_phase[1]=cases[c].next_phase;read_direction[1]=cases[c].next_direction;
        read_anchor[1]=anchor;read_pixels[1]=expected;source_clock_observer=auxiliary_read_observer;
        CHECK(play_auxiliary_wait_step(game,vga,&phase,&direction,&have_anchor,&anchor,&key_ready,&updated)==0);
        CHECK(!key_ready && updated==cases[c].due && have_anchor==1 && phase==cases[c].next_phase &&
            direction==cases[c].next_direction && anchor==signed_low_tick(cases[c].due?cases[c].fresh:cases[c].prior));
        CHECK(source_clock_reads==source_clock_count && !source_clock_bad && !memcmp(vga->framebuffer,expected,64000));
        CHECK(!memcmp(game,game_before,sizeof(*game)) && test_rng.state==0x3141 && !water_calls && !water_changes &&
            !finite_frames && !steady_presents && !source_timed_frames && !host_pumps && !host_polls);
        CHECK(vga->field_water_phase==5 && vga->field_water_last_tick==0x1234 &&
            vga->frame_deadline_ns==123456 && vga->frame_interval_ns==789 && !vga->input.queued_count);
        for(unsigned i=0;i<768;i++)CHECK(vga->dac[i]==0x43);
        CHECK(fd2_field_page_read(test_page,0,0,456,336,page_readback,sizeof(page_readback),456)==0 &&
            !memcmp(page_readback,page_snapshot,sizeof(page_readback)));
        if(c==2u) {
            /* compare102/fresh105：下一106差1不得误借compare旧锚再绘。 */
            source_clock_reads=0;source_clock_count=1;source_clock_observer=NULL;source_clock[0]=exact_tick_ns(106);
            CHECK(play_auxiliary_wait_step(game,vga,&phase,&direction,&have_anchor,&anchor,&key_ready,&updated)==0 &&
                !updated && !key_ready && phase==5 && anchor==105 && source_clock_reads==1 &&
                !memcmp(vga->framebuffer,expected,64000));
        }
    }
    source_clock_mode=0;source_clock_observer=NULL;
    free(expected);free(before);free(vga);free(game_before);free(game);return 0;
}
static int test_candidate_first_and_early_key_priority(void) {
    fd2_field_game *game=malloc(sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
    uint8_t *expected=malloc(64000);CHECK(game && vga && expected);
    for(int kind=0;kind<4;kind++) {
        controls_reset();init_game(game);game->cursor_cell_x=7;game->cursor_cell_y=14;
        memset(vga,0,sizeof(*vga));vga->field_page=test_page;memset(vga->framebuffer,0x69,64000);
        memcpy(expected,vga->framebuffer,64000);
        int phase=7,direction=-1,have_anchor=0,key_ready=77,updated=77;
        int32_t anchor=0x12345678; /* 未资格首锚哨兵只证明C没有读取，不是原初值。 */
        if(kind) {
            vga->input.queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_OTHER,.bios_code=0x1061};
            vga->input.queued_keys[1]=(fd2_input_event){.key=FD2_INPUT_KEY_ENTER,.bios_code=0x1c0d};
            vga->input.queued_count=2;
        }
        if(kind>=2)game->runtime_host_quit=1;
        if(kind==3)game->failure=FD2_FIELD_FAILURE_ACTION;
        source_clock_mode=1;source_clock_count=kind?0u:1u;source_clock[0]=exact_tick_ns(120);
        if(!kind) {
            expected_auxiliary_markers(expected,7);observed_phase=&phase;observed_direction=&direction;
            observed_anchor=&anchor;observed_vga=vga;read_phase[0]=6;read_direction[0]=-1;
            read_anchor[0]=anchor;read_pixels[0]=expected;source_clock_observer=auxiliary_read_observer;
        }
        CHECK(play_auxiliary_wait_step(game,vga,&phase,&direction,&have_anchor,&anchor,&key_ready,&updated)==
            (kind==3?FD2_SCENE_RESULT_ERROR:kind==2?FD2_SCENE_RESULT_HOST_QUIT:0));
        CHECK(!source_clock_bad && source_clock_reads==(kind?0u:1u) && !water_calls && test_rng.state==0x3141 &&
            !memcmp(vga->framebuffer,expected,64000) && !steady_presents && !host_pumps);
        if(kind)CHECK(!have_anchor && phase==7 && direction==-1 && anchor==0x12345678 && vga->input.queued_count==2);
        else CHECK(have_anchor && phase==6 && direction==-1 && anchor==120 && updated==1 && !key_ready);
        if(kind==1)CHECK(key_ready==1 && !updated);
    }
    source_clock_mode=0;source_clock_observer=NULL;free(expected);free(vga);free(game);return 0;
}
/* 每次仅一个合法2tick body；独立原phase序列含两端重复，不追赶。
 * have_anchor=1是条件postfirst输入，不声称已证明原首FS/GS。 */
static int test_postfirst_exact_phase_sequence(void) {
    const int phases[]={7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,7,6};
    fd2_field_game *game=malloc(sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));uint8_t *expected=malloc(64000);
    CHECK(game && vga && expected);controls_reset();init_game(game);game->cursor_cell_x=7;game->cursor_cell_y=14;
    vga->field_page=test_page;memset(vga->framebuffer,0x69,64000);memcpy(expected,vga->framebuffer,64000);
    int phase=7,direction=-1,have_anchor=1,key_ready,updated;int32_t anchor=98;
    source_clock_mode=1;source_clock_count=2;
    for(unsigned i=0;i<sizeof(phases)/sizeof(phases[0]);i++) {
        CHECK(phase==phases[i]);source_clock_reads=0;source_clock[0]=source_clock[1]=exact_tick_ns(100+i*2);
        expected_auxiliary_markers(expected,phases[i]);
        CHECK(play_auxiliary_wait_step(game,vga,&phase,&direction,&have_anchor,&anchor,&key_ready,&updated)==0 &&
            !key_ready && updated==1 && source_clock_reads==2 && anchor==(int32_t)(100+i*2) &&
            !memcmp(vga->framebuffer,expected,64000) && !source_clock_bad && test_rng.state==0x3141 && !water_calls);
    }
    source_clock_mode=0;free(expected);free(vga);free(game);return 0;
}

int main(void) {
    test_page=fd2_field_page_create();CHECK(test_page);
    CHECK(test_full_frame_and_transition() == 0);
    CHECK(test_marker_filter_order_and_pulse() == 0);
    CHECK(test_cross_stage_shapes_height_boundary_and_page_prefix() == 0);
    CHECK(test_modal_keys_owner_and_held() == 0);
    CHECK(test_invalid_preflight_and_quit() == 0);
    CHECK(test_formal_route_no_key_fallthrough() == 0);
    CHECK(test_restore_failure_does_not_present_success() == 0);
    CHECK(test_real_stage0_original_probe_indices() == 0);
    CHECK(test_wait_water_error_prefix() == 0);
    CHECK(test_postfirst_gate_marker_fresh_clock()==0);
    CHECK(test_candidate_first_and_early_key_priority()==0);
    CHECK(test_postfirst_exact_phase_sequence()==0);
    fd2_field_page_destroy(test_page);
    puts("field auxiliary tests: ok");
    return 0;
}

/* 正式视觉接口使用NS；单步source向量直接提供独立PIT边界，旧modal
 * 的外部宿主采样仍沿原毫秒调度器。 */
uint64_t auxiliary_ticks_ns(void) {
    if(source_clock_mode) {
        unsigned index=source_clock_reads++;
        if(index>=source_clock_count){source_clock_bad=1;return 0;}
        if(source_clock_observer)source_clock_observer(index);
        return source_clock[index];
    }
    return auxiliary_ticks()*UINT64_C(1000000);
}
