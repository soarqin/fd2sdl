/* B4呈现边界：1e292/1e529/1a866，真实资源，无窗口；不替代原版WGC。 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "field_game.h"
#include "field_audio.h"
#include "portrait.h"
#include "scene.h"
#include "static_data.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static uint64_t now_ms;
static unsigned test_frames, rng_calls, live_images, allocation_calls, fail_allocation;
static unsigned quit_at, key_at, clock_step, arrow_replies;
static int auto_arrows, arrow_held, quit_when_arrow;
static unsigned arrow_draws, arrow_erases;
static fd2_field_game *loot_wait_game;
static int auto_loot_keys, loot_key_sent, quit_when_loot_ready;
static fd2_vga *text_ready_vga;
static unsigned pulse_at_text_ready,pulse_ready_sent,pulse_consumed,pulse_ready_frame,pulse_ready_rng;
static fd2_field_game fixture_game;
static fd2_archive fixture_other;
static fd2_field_page *test_field_page;
static uint8_t arrow_pixels[2][140];
static fd2_field_progress_notice script[8];
static unsigned script_count, script_index, acknowledged;
static unsigned resume_calls;
static int real_progress;
static unsigned real_acknowledged;
static unsigned choice_redraws;
static int choice_check_zeros,choice_zeros_checked;
static uint8_t choice_before[64000];
static uint64_t close_started;
static unsigned inject_common_on_reply,common_injected,single_box_guard;
static unsigned cell_pcm_calls;
static unsigned water_calls;
typedef struct { uint16_t first,second; int key; } notice_wait_step;
typedef struct {
    const char *name; unsigned ticks; uint16_t start;
    notice_wait_step steps[3]; unsigned count; int queued;
} notice_wait_case;
static const notice_wait_case notice_wait_cases[]={
    {"deadline10",10,100,{{100,100,0},{104,104,0},{110,110,0}},3,0},
    {"deadline11",11,100,{{100,100,0},{110,110,0},{111,111,0}},3,0},
    {"queued_key",10,100,{{100,100,1}},1,1},
    {"late_key",10,100,{{100,100,0},{101,101,1}},2,0},
    {"zero_do_once",0,100,{{100,100,0}},1,0},
    {"signed_half_wrap",10,0x7fff,{{0x8000,0x8000,0}},1,0},
    {"ffff_to_zero",10,0xffff,{{0,0,0},{8,8,0},{9,9,0}},3,0},
    {"negative_start",10,0x8000,{{0x8000,0x8000,0},{0x8009,0x8009,0},{0x800a,0x800a,0}},3,0},
    {"second_read_backwards",10,100,{{101,99,0}},1,0}
};
static const notice_wait_case *wait_case;
static unsigned wait_phase,wait_step,wait_clock_reads,wait_key_checks,wait_clear_calls;
static int wait_water_failure;
static int cell_pcm_failure;
static fd2_pcm_player *cell_secondary;
static const fd2_pcm_bank *cell_ui_bank;
static int test_pcm_replace(fd2_pcm_player *player,const fd2_pcm_bank *bank,
        size_t sample,int loops,float gain) {
    CHECK(player==cell_secondary && bank==cell_ui_bank && sample==12 && loops==1 &&
        gain==FD2_GAME_SFX_DEFAULT_GAIN);
    cell_pcm_calls++;return cell_pcm_failure?-1:0;
}

/* 测试投递器只把最小机制图放到caller返回点；实际child/ACK/库存/Play
 * 均由生产consumer执行。这不是新stage绑定或原版剧情验收。 */
static const fd2_static_flow_node injected_common_nodes[]={
    {.id="child",.op="loot_child",.args={2,-1},.edges={-1,-1,-1,-1,1,-1,-1,8}},
    {.id="notice",.op="notice_actor",.args={-1},.edges={2,-1,-1,-1,-1,-1,-1,8}},
    {.id="text",.op="message",.args={480,0,99,127},.edges={3,-1,-1,-1,-1,-1,-1,8}},
    {.id="portrait",.op="portrait_frame",.args={0},.edges={4,-1,-1,-1,-1,-1,-1,8}},
    {.id="wait",.op="wait_key",.args={0},.edges={-1,-1,-1,-1,5,-1,-1,8}},
    {.id="close",.op="close_box",.edges={6,-1,-1,-1,-1,-1,-1,8}},
    {.id="once",.op="event_byte_write",.args={0,0,1,1},.edges={7,-1,-1,-1,-1,-1,-1,8}},
    {.id="done",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}},
    {.id="failed",.op="fail",.edges={-1,-1,-1,-1,-1,-1,-1,-1}}
};
static const fd2_static_flow injected_common_flow={"test.reply.child.once",injected_common_nodes,9,0};
static int test_loot_reply_dispatch(fd2_field_game *game,fd2_field_loot_reply reply,int raw_slot) {
    int result=fd2_field_game_loot_reply(game,reply,raw_slot);
    if(result==0 && inject_common_on_reply) {
        inject_common_on_reply=0;common_injected++;
        CHECK(!game->loot.source && !game->event_resume.active);
        game->event_resume=(fd2_field_event_resume){.flow=&injected_common_flow,
            .pc=0,.actor=0,.active=1,.waiting_legacy=1};
        CHECK(fd2_field_game_common_event_resume(game)==0);
    }
    return result;
}
/* 只在真实Core TEXT_READY返回点注入当前帧脉冲，不替换提交/等待/RNG。
 * 原19239授予在外16c57之前；吞键实现会在take_key处立即失败，另有帧上界。 */
static int test_loot_text_ready_dispatch(fd2_field_game *game) {
    int result=fd2_field_game_loot_text_ready(game);
    if(result==0 && pulse_at_text_ready) {
        CHECK(game==loot_wait_game && text_ready_vga && !pulse_ready_sent && game->loot.text_ready);
        CHECK(fd2_field_unit_inventory_count(&game->units.items[0])==2);
        pulse_ready_sent=1;pulse_ready_frame=test_frames;pulse_ready_rng=rng_calls;
        fd2_input_set_test_key_state(&text_ready_vga->input,SDL_SCANCODE_RETURN,1);
        fd2_input_begin_test_frame(&text_ready_vga->input,now_ms);
    }
    return result;
}

static void *test_malloc(size_t size) {
    if (++allocation_calls == fail_allocation) return NULL;
    return malloc(size);
}
static void *test_calloc(size_t count, size_t size) {
    if (++allocation_calls == fail_allocation) return NULL;
    return calloc(count, size);
}
static int test_decode(fd2_image *image, const fd2_archive *archive, size_t id, size_t frame) {
    int result = fd2_portrait_decode_frame(image, archive, id, frame);
    if (result == 0) { live_images++; if(single_box_guard)CHECK(live_images<=4); }
    return result;
}
static void test_image_free(fd2_image *image) {
    if (image && image->pixels) { CHECK(live_images != 0); live_images--; }
    fd2_image_free(image);
}
static uint64_t test_ticks(void) {
    if(!wait_case)return now_ms;
    CHECK(wait_step<wait_case->count);
    if(wait_phase==0){wait_phase=1;return (uint64_t)wait_case->start*55u;}
    const notice_wait_step *step=&wait_case->steps[wait_step];
    if(wait_phase==1){wait_phase=2;return (uint64_t)step->first*55u;}
    CHECK(wait_phase==3 || wait_phase==4);
    wait_clock_reads++;
    uint16_t tick=wait_phase==3?step->first:step->second;
    wait_phase++;
    return (uint64_t)tick*55u;
}
static void test_delay(uint32_t delay) { now_ms += delay; }
static int test_audio(fd2_field_audio *audio, fd2_field_sfx cue) { (void)audio; (void)cue; return 0; }
static int test_water(fd2_vga *vga, uint64_t now) {
    (void)now;water_calls++;
    if(wait_case) {
        CHECK(wait_phase==2);wait_phase=3;
        if(wait_case->steps[wait_step].key && !(wait_case->queued && wait_step==0)) {
            fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
            fd2_input_begin_test_frame(&vga->input,now);
        }
        if(wait_water_failure)return -1;
    }
    return 0;
}
static int test_notice_has_key(const fd2_input *input) {
    int result=fd2_input_has_any_key(input);
    if(wait_case) {
        CHECK(wait_phase==3 && result==wait_case->steps[wait_step].key);
        wait_key_checks++;
    }
    return result;
}
static int test_notice_take_key(fd2_input *input,fd2_input_event *event) {
    if(wait_case) {
        CHECK(wait_phase==4 || wait_phase==5); /* 入口clear会在任何water之前失败。 */
        wait_clear_calls++;
    }
    int result=fd2_input_take_key(input,event);
    if(result && pulse_ready_sent && !pulse_consumed) {
        /* progress_wait入口先取blink RNG；无源drain会在此前消费，故拒绝。 */
        CHECK(input==&text_ready_vga->input && rng_calls==pulse_ready_rng+1 &&
            test_frames==pulse_ready_frame);
        pulse_consumed=1;
    }
    return result;
}
static int test_message_arrow(fd2_vga *vga, const fd2_archive *other, unsigned frame) {
    if(frame==13)arrow_erases++;else arrow_draws++;
    return fd2_scene_field_message_arrow(vga,other,frame);
}
static void test_render(const fd2_field_game *source, fd2_vga *vga) {
    (void)source; memset(vga->framebuffer, 0x55, sizeof(vga->framebuffer));
}
/* 该TU的fixture_game只含一格逻辑数据，未装地形/actor缓存；这里只核
 * 新19953的1297d/11eee/127a9调用边界，真实page renderer另有独立测试。
 * 保同一App page、全部像素及业务/RNG；不拿空缓存伪装实图绘制通过。 */
static int test_runtime_refresh(fd2_field_game *game,fd2_vga *vga,int mode) {
    CHECK(game && vga && vga->field_page==test_field_page);
    CHECK(mode>=0 && mode<=3);
    if(mode==2)choice_redraws++;
    return 0;
}
static int has_arrow(const fd2_vga *vga) {
    for (size_t image = 0; image < 2; image++) {
        int match = 1;
        for (int row = 0; row < 10; row++)
            if (memcmp(vga->framebuffer + (181 + row) * VGA_STRIDE + 191,
                       arrow_pixels[image] + row * 14, 14)) match = 0;
        if (match) return 1;
    }
    return 0;
}
static void test_frame(fd2_vga *vga, unsigned interval) {
    test_frames++;
    if(pulse_ready_sent && !pulse_consumed)CHECK(test_frames<=pulse_ready_frame+2);
    if(wait_case) {
        CHECK(wait_phase==4 || wait_phase==5);
        wait_step++;wait_phase=1;
        CHECK(wait_step<wait_case->count);
        if(test_frames==quit_at)vga->input.quit_requested=1;
        return;
    }
    if(choice_check_zeros && test_frames==4) {
        unsigned zeros=0;
        for(unsigned button=0;button<2;button++) {
            const fd2_image *image=&fixture_game.command_assets.frames[button?51:48];
            for(int y=0;y<16;y++)for(int x=0;x<24;x++)if(!image->pixels[y*24+x]) {
                size_t at=(size_t)(168+y)*320u+232u+button*32u+(unsigned)x;
                CHECK(vga->framebuffer[at]==choice_before[at]);zeros++;
            }
        }
        CHECK(zeros);choice_zeros_checked=1;
    }
    now_ms += clock_step ? clock_step : interval;
    fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 0);
    int arrow = auto_arrows && has_arrow(vga);
    if (!arrow) arrow_held = 0;
    if (test_frames == key_at || (arrow && !arrow_held)) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        if (arrow) { arrow_held = 1; arrow_replies++; }
    }
    if (auto_loot_keys && loot_wait_game && loot_wait_game->loot.text_ready && !loot_key_sent) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        loot_key_sent=1;
    }
    fd2_input_begin_test_frame(&vga->input, now_ms);
    if (test_frames == quit_at || (quit_when_arrow && has_arrow(vga))) vga->input.quit_requested = 1;
    if (quit_when_loot_ready && loot_wait_game && loot_wait_game->loot.text_ready)
        vga->input.quit_requested=1;
    CHECK(test_frames < 2000);
}
static void test_present(fd2_vga *vga) { test_frame(vga, 16); }
static void test_present_timed(fd2_vga *vga, uint32_t interval) { test_frame(vga, interval); }
static uint32_t test_rng(void *context) { (void)context; rng_calls++; return 0; }
static int test_peek(const fd2_field_game *source, fd2_field_progress_notice *notice) {
    if(real_progress)return fd2_field_game_progress_peek(source,notice);
    if (script_index == script_count) return 0;
    *notice = script[script_index]; return 1;
}
static int test_resume(fd2_field_game *source) {
    resume_calls++;
    return real_progress?fd2_field_game_progress_resume(source):script_index<script_count;
}
static int test_ack(fd2_field_game *source) {
    if(real_progress){real_acknowledged++;return fd2_field_game_progress_ack(source);}
    CHECK(script_index < script_count);
    const fd2_field_progress_notice *notice = &script[script_index];
    if (notice->kind == FD2_FIELD_PROGRESS_GAIN) source->units.items[0].level++;
    if (notice->kind == FD2_FIELD_PROGRESS_LEVEL) CHECK(arrow_replies == 1);
    if (notice->kind == FD2_FIELD_PROGRESS_GROWTH) {
        CHECK(arrow_replies == 2 && source->units.items[0].attack == 10);
        source->units.items[0].attack += (uint16_t)notice->value;
        ((uint8_t *)&source->units.items[0])[0x1c] |= 2; /* LEARN在呈现前OR17。 */
    }
    if (notice->kind == FD2_FIELD_PROGRESS_CLOSE) {
        CHECK(now_ms >= close_started + 11u * 55u);
        CHECK(source->units.items[0].experience == 99 && source->experience_value == 5);
        source->units.items[0].experience = 4;
        source->experience_value = 0;
    }
    script_index++; acknowledged++;
    if (script_index < script_count && script[script_index].kind == FD2_FIELD_PROGRESS_CLOSE)
        close_started = now_ms;
    return 0;
}

#define malloc test_malloc
#define calloc test_calloc
#define fd2_portrait_decode_frame test_decode
#define fd2_image_free test_image_free
#define SDL_GetTicks test_ticks
#define fd2_delay_ms test_delay
#define fd2_field_audio_play test_audio
#define fd2_pcm_play_replace test_pcm_replace
#define fd2_field_water_tick test_water
#define fd2_input_has_any_key test_notice_has_key
#define fd2_input_take_key test_notice_take_key
#define fd2_scene_field_message_arrow test_message_arrow
#define fd2_field_game_render test_render
#define fd2_field_game_runtime_refresh test_runtime_refresh
#define fd2_vga_present test_present
#define fd2_vga_present_timed test_present_timed
#define fd2_field_game_progress_peek test_peek
#define fd2_field_game_progress_ack test_ack
#define fd2_field_game_progress_resume test_resume
#define fd2_field_game_loot_reply test_loot_reply_dispatch
#define fd2_field_game_loot_text_ready test_loot_text_ready_dispatch
#include "field_play.c"

/* 原once攻击回放只属于本测试夹具：保留真实提交后progress的quit／数据错误门，
 * 不再让正式单帧入口演练战斗。此处仍沿用本TU的present／clock／资源替身。 */
static void init_validation_replay(fd2_field_game *replay,
                                   const fd2_field_game *game) {
    *replay = *game;
    replay->move_range =
        (fd2_field_path_result)FD2_FIELD_PATH_RESULT_INITIALIZER;
    replay->move_path = NULL;
    replay->move_path_capacity = 0;
    replay->move_path_length = 0;
    replay->move_preview_valid = 0;
    replay->selected_unit = -1;
    replay->command_selected = -1;
    replay->attack_target = -1;
    replay->last_attack_valid = 0;
    replay->interaction = FD2_FIELD_INTERACTION_BROWSE;
}

static void close_validation_replay(fd2_field_game *replay) {
    if (!replay) return;
    fd2_field_path_close(&replay->move_range);
    free(replay->move_path);
    replay->move_path = NULL;
    replay->move_path_capacity = 0;
    replay->move_path_length = 0;
}

static int move_focus_to(fd2_field_game *game, int x, int y) {
    if (!game) return -1;
    while (game->cursor_cell_x != x) {
        int dx = game->cursor_cell_x < x ? 1 : -1;
        if (!fd2_field_game_move_cursor(game, dx, 0)) return -1;
    }
    while (game->cursor_cell_y != y) {
        int dy = game->cursor_cell_y < y ? 1 : -1;
        if (!fd2_field_game_move_cursor(game, 0, dy)) return -1;
    }
    return 0;
}

static int validate_attack_flow(const fd2_field_game *game,
                                fd2_vga *vga, const fd2_archive *fdother,
                                fd2_field_audio *audio) {
    if (!game || !game->ready || game->active_side != 2u) return -1;
    fd2_field_game replay;
    init_validation_replay(&replay, game);
    fd2_field_rng rng;
    fd2_field_rng_seed(&rng, 0x7a18);
    int result = -1;
    const char *failed_stage = "attack hooks";
    size_t attacker_index = SIZE_MAX;
    fd2_field_ai_physical_candidate plan = {0};

    if (fd2_field_game_set_attack_hooks(
            &replay, NULL, NULL, NULL, NULL,
            fd2_field_rng_next, &rng) != 0)
        goto done;
    failed_stage = "real candidate";
    for (size_t i = 0; i < replay.units.count; i++) {
        const fd2_field_unit *unit = &replay.units.items[i];
        if (unit->side != 2u || fd2_field_unit_is_hidden(unit) ||
            unit->hp == 0 || fd2_field_unit_has_acted(unit))
            continue;
        fd2_field_path_result move_range =
            FD2_FIELD_PATH_RESULT_INITIALIZER;
        int have_plan =
            fd2_field_game_compute_move_range(&replay, i, &move_range) == 0 &&
            fd2_field_ai_choose_physical_candidate(
                &replay.map, &replay.terrain, &replay.units,
                i, 1u, &move_range, &plan) == 0;
        fd2_field_path_close(&move_range);
        if (have_plan) {
            attacker_index = i;
            break;
        }
    }
    if (attacker_index == SIZE_MAX ||
        plan.unit_index >= replay.units.count) {
        goto done;
    }

    /* 运行时 RNG 本来由战场入口播种；在可达候选确定后恢复同一初值，
     * 让两个入口的命中／伤害断言保持确定，同时不改写任何单位属性。 */
    fd2_field_rng_seed(&rng, 0x7a18);
    failed_stage = "move and command";
    fd2_field_unit *attacker = &replay.units.items[attacker_index];
    fd2_field_unit *defender = &replay.units.items[plan.unit_index];
    uint16_t defender_hp_before = defender->hp;
    replay.cursor_cell_x = attacker->x;
    replay.cursor_cell_y = attacker->y;
    if (fd2_field_game_confirm_cursor(&replay) != (int)attacker_index ||
        move_focus_to(&replay, plan.destination_x, plan.destination_y) != 0 ||
        !replay.move_preview_valid ||
        fd2_field_game_confirm_cursor(&replay) != (int)attacker_index ||
        replay.interaction != FD2_FIELD_INTERACTION_MOVING ||
        fd2_field_game_execute_move(&replay, NULL, 0) != 0 ||
        replay.interaction != FD2_FIELD_INTERACTION_COMMAND)
        goto done;

    failed_stage = "target confirmation";
    (void)fd2_field_game_select_command_direction(
        &replay, FD2_FIELD_COMMAND_DIRECTION_UP);
    if (replay.command_selected != FD2_FIELD_COMMAND_ATTACK ||
        fd2_field_game_confirm_cursor(&replay) != (int)attacker_index ||
        replay.interaction != FD2_FIELD_INTERACTION_TARGETING ||
        !replay.move_range.nodes ||
        !fd2_field_path_is_destination(
            &replay.move_range, defender->x, defender->y) ||
        move_focus_to(&replay, defender->x, defender->y) != 0 ||
        fd2_field_game_confirm_cursor(&replay) != (int)attacker_index)
        goto done;

    failed_stage = "damage commit";
    if (!replay.last_attack_valid || !replay.last_attack.hit ||
        defender->hp >= defender_hp_before ||
        !fd2_field_unit_has_acted(attacker) || replay.selected_unit != -1 ||
        replay.interaction != FD2_FIELD_INTERACTION_BROWSE ||
        replay.move_range.nodes != NULL)
        goto done;

    failed_stage = "acted gate";
    /* once同样完成B4逻辑续态；fast不等待按键，不登记真实提示或RNG对照。 */
    int progress_result = play_field_progress(&replay, vga, fdother, audio, 1);
    if (progress_result != FD2_SCENE_RESULT_OK) {
        if (progress_result == FD2_SCENE_RESULT_HOST_QUIT) result = FD2_SCENE_RESULT_HOST_QUIT;
        goto done;
    }
    /* 攻击完成后的 acted 单位同样不能重新进入移动状态。 */
    replay.cursor_cell_x = attacker->x;
    replay.cursor_cell_y = attacker->y;
    if (fd2_field_game_confirm_cursor(&replay) != -1 ||
        replay.selected_unit != -1 ||
        replay.interaction != FD2_FIELD_INTERACTION_BROWSE)
        goto done;
    result = 0;

done:
    if (result != 0 && result != FD2_SCENE_RESULT_HOST_QUIT)
        fprintf(stderr, "field attack validation stopped at: %s\n",
                failed_stage);
    close_validation_replay(&replay);
    return result;
}

#undef malloc
#undef calloc
#undef fd2_portrait_decode_frame
#undef fd2_image_free
#undef SDL_GetTicks
#undef fd2_delay_ms
#undef fd2_field_audio_play
#undef fd2_pcm_play_replace
#undef fd2_field_water_tick
#undef fd2_input_has_any_key
#undef fd2_input_take_key
#undef fd2_scene_field_message_arrow
#undef fd2_field_game_render
#undef fd2_field_game_runtime_refresh
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_field_game_progress_peek
#undef fd2_field_game_progress_ack
#undef fd2_field_game_progress_resume
#undef fd2_field_game_loot_reply
#undef fd2_field_game_loot_text_ready

static void reset(fd2_vga *vga) {
    CHECK(live_images == 0);
    now_ms = test_frames = rng_calls = allocation_calls = fail_allocation = 0;
    water_calls=0;
    wait_case=NULL;wait_phase=wait_step=wait_clock_reads=wait_key_checks=wait_clear_calls=0;
    wait_water_failure=0;
    real_progress=0;real_acknowledged=0;
    quit_at = key_at = clock_step = arrow_replies = 0;
    script_count = script_index = acknowledged = resume_calls = choice_redraws = 0;
    choice_check_zeros=choice_zeros_checked=0;
    auto_arrows = arrow_held = quit_when_arrow = 0;
    arrow_draws=arrow_erases=0;
    loot_wait_game=NULL;auto_loot_keys=loot_key_sent=quit_when_loot_ready=0;
    text_ready_vga=NULL;pulse_at_text_ready=pulse_ready_sent=pulse_consumed=pulse_ready_frame=pulse_ready_rng=0;
    inject_common_on_reply=common_injected=single_box_guard=0;
    memset(vga, 0, sizeof(*vga));
    vga->field_page=test_field_page;
    memset(vga->framebuffer, 0x55, sizeof(vga->framebuffer));
    fd2_input_init(&vga->input);
}
static void test_resources(void) {
    fd2_field_progress_notice notice = {FD2_FIELD_PROGRESS_GAIN, 0, 488, 630, 0, 0};
    field_progress_text text;
    CHECK(progress_text_prepare(&fixture_game, &notice, &text) == 0);
    CHECK(text.count == 9 && text.tokens[4] == 6 && text.tokens[5] == 3 && text.tokens[6] == 0);
    CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == 0 && live_images == 0 && rng_calls == 0);
    notice.value = UINT32_MAX; CHECK(progress_text_prepare(&fixture_game, &notice, &text) == 0);
    CHECK(text.tokens[4]==FIELD_TEXT_SAFE_MINUS && text.tokens[5]==1);
    notice = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_LEARN, 0, 587, 0, 17, 0};
    CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == 0);
    notice.magic_id = 36; CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == -1);
    notice.magic_id = 17; fixture_game.units.items[0].unit_id = 255;
    CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == -1 && live_images == 0);
    fixture_game.units.items[0].unit_id = 0;
    fd2_archive damaged = fixture_other; /* 只借offsets，关闭仍由原owner承担。 */
    damaged.data = malloc(fixture_other.size); CHECK(damaged.data != NULL);
    memcpy(damaged.data, fixture_other.data, fixture_other.size);
    const uint8_t *sheet; size_t sheet_size;
    CHECK(fd2_archive_get(&damaged, 5, &sheet, &sheet_size) == 0 && sheet_size > 86);
    uint32_t last = (uint32_t)sheet[82] | (uint32_t)sheet[83] << 8 |
                    (uint32_t)sheet[84] << 16 | (uint32_t)sheet[85] << 24;
    CHECK(last + 4 < sheet_size);
    damaged.data[(size_t)(sheet - damaged.data) + last] = 13; /* frame19须14×10。 */
    fd2_vga pixels; memset(&pixels, 0xa5, sizeof(pixels));
    CHECK(fd2_scene_field_message_box(&pixels, &damaged) == -1);
    for (size_t i = 0; i < sizeof(pixels.framebuffer); i++) CHECK(pixels.framebuffer[i] == 0xa5);
    CHECK(progress_resources_preflight(&fixture_game, &notice, &damaged) == -1 && live_images == 0);
    free(damaged.data);
}
static void test_signed_number_safe_projection(void) {
    fd2_vga vga;reset(&vga);
    field_progress_text text;
    fd2_field_progress_notice notice={FD2_FIELD_PROGRESS_GAIN,0,488,UINT32_C(0x80000000),0,0};
    CHECK(progress_text_prepare(&fixture_game,&notice,&text)==0);
    CHECK(text.tokens[4]==FIELD_TEXT_SAFE_MINUS && text.tokens[5]==2 &&
          text.tokens[14]==8); /* -2147483648，保留完整signed DWORD。 */
    fd2_field_unit before=fixture_game.units.items[0];
    field_progress_box box={0};
    field_progress_text number={{FIELD_TEXT_SAFE_MINUS,1},2};
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&number,10,10,1)==FD2_SCENE_RESULT_OK);
    int visible=0;
    for (int y=0;y<VGA_H;y++) for (int x=0;x<VGA_W;x++) {
        if (x>=10 && x<26 && y>=10 && y<26) visible|=vga.framebuffer[y*VGA_W+x]!=0x4a;
        if (x<10 || x>=42 || y<10 || y>=27) CHECK(vga.framebuffer[y*VGA_W+x]==0x55);
    }
    CHECK(visible && !rng_calls && !test_frames && !memcmp(&before,&fixture_game.units.items[0],sizeof(before)));
    reset(&vga);vga.input.quit_requested=1;
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&number,10,10,1)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!rng_calls && !memcmp(&before,&fixture_game.units.items[0],sizeof(before)));
    fd2_image minus=fixture_game.info_assets.signs[1];
    fixture_game.info_assets.signs[1].pixels=NULL;
    CHECK(progress_text_prepare(&fixture_game,&notice,&text)==-1);
    fixture_game.info_assets.signs[1]=minus;
}
static void test_box_and_wait(void) {
    fd2_vga vga;
    reset(&vga);
    field_progress_box box = {0};
    CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 0) == 0 && test_frames == 6 && live_images == 4);
    CHECK(!memcmp(vga.framebuffer, box.page->framebuffer, sizeof(vga.framebuffer)));
    for (unsigned i = 0; i < 80; i++)
        CHECK(vga.framebuffer[115 * VGA_STRIDE + 87 - i] == box.portraits[0].pixels[i]);
    test_frames = rng_calls = 0; now_ms = 0; clock_step = 110; key_at = 5;
    CHECK(progress_wait(&box, &fixture_game, &vga, &fixture_other, 0, 1) == 0 && rng_calls == 2);
    drain_frame_keys(&vga); /* 此测试主动建立无键前置；1e5c0本身不清入口。 */
    test_frames = key_at = 0; now_ms = 0; clock_step = 110;
    CHECK(progress_timed_wait(&vga, 10, 0) == 0 && now_ms >= 550);
    test_frames = 0; now_ms = 0; key_at = 1;
    CHECK(progress_timed_wait(&vga, 11, 0) == 0 && now_ms < 605);
    test_frames = key_at = 0;
    CHECK(progress_box_transition(&box, &vga, 0, 0) == 0 && test_frames == 5);
    progress_box_free(&box); CHECK(live_images == 0 && !box.page && !box.background);
    for (unsigned failure = 1; failure <= 2; failure++) {
        reset(&vga); fail_allocation = failure;
        CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 0) == -1 && live_images == 0);
        CHECK(!box.page && !box.background && test_frames == 0);
    }
    reset(&vga); quit_at = 3;
    CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 0) == FD2_SCENE_RESULT_HOST_QUIT);
    progress_box_free(&box); CHECK(live_images == 0);
    reset(&vga); fixture_game.units.items[0].unit_id = 128;
    CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 1) == 0);
    CHECK(box.page->framebuffer[13 * VGA_STRIDE + 123] == box.portraits[0].pixels[0]);
    progress_portrait(&box, &vga, 1, 0);
    CHECK(!memcmp(vga.framebuffer + 13 * VGA_STRIDE + 123, box.portraits[1].pixels, 80));
    progress_box_free(&box); fixture_game.units.items[0].unit_id = 0;
}
static void test_notice_wait_vectors(int json) {
    if(json)fputs("{\"schema\":\"fd2.g4.notice-wait-c.v1\",\"cases\":[",stdout);
    for(unsigned i=0;i<sizeof(notice_wait_cases)/sizeof(notice_wait_cases[0]);i++) {
        fd2_vga vga;reset(&vga);const notice_wait_case *item=&notice_wait_cases[i];
        if(item->queued) {
            fd2_input_set_test_key_state(&vga.input,SDL_SCANCODE_RETURN,1);
            fd2_input_begin_test_frame(&vga.input,0);
        }
        wait_case=item;
        CHECK(progress_timed_wait(&vga,item->ticks,0)==FD2_SCENE_RESULT_OK);
        CHECK(water_calls==item->count && wait_key_checks==item->count &&
            test_frames==item->count-1 && wait_clear_calls==1u+(unsigned)item->steps[item->count-1].key);
        CHECK(!rng_calls && !vga.actor_jitter_tick && !vga.actor_jitter_phase &&
            !vga.field_visual.actor_idle_tick && !vga.field_visual.cursor_phase &&
            !vga.dialog_visual.glyph_counter && !vga.dialog_visual.mouth_phase &&
            !fd2_input_has_any_key(&vga.input));
        unsigned expected_reads=0;
        for(unsigned j=0;j<item->count;j++) {
            int32_t start=item->start<0x8000u?item->start:(int32_t)item->start-65536;
            int32_t now=item->steps[j].first<0x8000u?item->steps[j].first:(int32_t)item->steps[j].first-65536;
            expected_reads+=1u+(unsigned)(now-start<(int32_t)item->ticks);
        }
        CHECK(wait_clock_reads==expected_reads);
        if(json)printf("%s{\"name\":\"%s\",\"water\":%u,\"input\":%u,\"clock_reads\":%u,\"tail_clear\":1}",
            i?",":"",item->name,water_calls,wait_key_checks,wait_clock_reads);
        wait_case=NULL;
    }
    if(json){fputs("]}\n",stdout);return;}
    fd2_vga vga;reset(&vga);wait_case=&notice_wait_cases[0];vga.input.quit_requested=1;
    CHECK(progress_timed_wait(&vga,10,0)==FD2_SCENE_RESULT_HOST_QUIT && !water_calls && !wait_clear_calls);
    reset(&vga);wait_case=&notice_wait_cases[0];quit_at=1;
    CHECK(progress_timed_wait(&vga,10,0)==FD2_SCENE_RESULT_HOST_QUIT && water_calls==1 &&
        wait_clock_reads==2 && !wait_clear_calls && test_frames==1);
    reset(&vga);wait_case=&notice_wait_cases[0];wait_water_failure=1;
    CHECK(progress_timed_wait(&vga,10,0)==FD2_SCENE_RESULT_ERROR && water_calls==1 &&
        !wait_key_checks && !wait_clock_reads && !wait_clear_calls && !test_frames);
    reset(&vga);fd2_input_set_test_key_state(&vga.input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&vga.input,0);
    CHECK(progress_timed_wait(&vga,11,1)==FD2_SCENE_RESULT_OK && !water_calls &&
        !test_frames && !fd2_input_has_any_key(&vga.input)); /* fast仅诊断捷径。 */
}
static void test_script_boundary(void) {
    fd2_vga vga;
    reset(&vga); auto_arrows = 1;
    fixture_game.units.items[0].level = 4; fixture_game.units.items[0].attack = 10;
    fixture_game.units.items[0].experience = 99; fixture_game.experience_value = 5;
    script[0] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_GAIN, 0, 488, 5, 0, 0};
    script[1] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_LEVEL, 0, 489, 5, 0, 0};
    script[2] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_GROWTH, 0, 490, 8, 0, 0};
    script[3] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_LEARN, 0, 587, 0, 17, 0};
    script[4] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_CLOSE, 0, 0, 0, 0, 11};
    script_count = 5;
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == 0);
    CHECK(acknowledged == 5 && fixture_game.units.items[0].level == 5 && fixture_game.units.items[0].attack == 18);
    CHECK(fixture_game.units.items[0].experience == 4 && fixture_game.experience_value == 0 && live_images == 0);
    CHECK(arrow_replies == 2 && rng_calls == 2 && !fd2_input_has_any_key(&vga.input));
    reset(&vga);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, 0);
    CHECK(fd2_input_has_any_key(&vga.input));
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == 0);
    CHECK(fd2_input_has_any_key(&vga.input) && !test_frames && !rng_calls);
    reset(&vga); clock_step = 110;
    script[0] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_STATUS, 0, 487, 4, 0, 10};
    script[1] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_STATUS, 0, 481, 0, 0, 10};
    script_count = 2;
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == 0);
    CHECK(acknowledged == 2 && !rng_calls && !arrow_replies && !live_images);
    CHECK(fixture_game.units.items[0].experience == 4 && fixture_game.experience_value == 0);
    reset(&vga); script_count = 1;
    script[0] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_STATUS, 0, 487, 4, 0, 10};
    fd2_field_unit before = fixture_game.units.items[0];
    vga.input.quit_requested = 1;
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!resume_calls && !acknowledged && !test_frames && !rng_calls && !live_images);
    CHECK(!memcmp(&before, &fixture_game.units.items[0], sizeof(before)));
    reset(&vga); quit_at = 1;
    CHECK(progress_scroll(&vga, 36887u) == FD2_SCENE_RESULT_OK && test_frames == 0);
    CHECK(!acknowledged && !rng_calls && !memcmp(&before, &fixture_game.units.items[0], sizeof(before)));
    test_present(&vga); /* 明确借本TU帧替身；上方#undef后生产present不注入quit_at。 */
    CHECK(test_frames==1 && fd2_input_take_quit(&vga.input));
}
static void test_attack_validation_preserves_quit_result(void) {
    fd2_field_game game={0};fd2_vga vga;reset(&vga);
    uint32_t cells[64]={0};fd2_terrain_attr terrain={0};
    game.ready=1;game.active_side=2;game.selected_unit=-1;game.turn_number=1;
    game.map.width=game.map.height=8;game.map.cells=cells;
    game.terrain.attrs=&terrain;game.terrain.attr_count=1;game.units.count=3;
    memset(game.metadata.turn_events,0xff,sizeof(game.metadata.turn_events));
    memset(game.metadata.cell_lookup,0xff,sizeof(game.metadata.cell_lookup));
    for(size_t i=0;i<3;i++) {
        fd2_field_unit *unit=&game.units.items[i];
        unit->unit_id=i==1?68:0;unit->side=i==1?0:2;
        unit->x=(uint8_t)(3+i);unit->y=i==2?7:3;
        unit->hp=unit->hp_max=100;unit->level=1;unit->movement_profile=1;
        unit->movement_points=3;unit->attack=100;unit->accuracy=1000;
        ((uint8_t*)unit)[0x31]=0xff;
        for(size_t slot=0;slot<8;slot++)((uint8_t*)unit)[10+slot*2]=0x80;
    }
    game.units.items[1].hp=10;
    ((uint8_t*)&game.units.items[0])[10]=0x40;((uint8_t*)&game.units.items[0])[11]=0;
    vga.input.quit_requested=1;
    CHECK(validate_attack_flow(&game,&vga,&fixture_other,NULL)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!acknowledged && !test_frames && !rng_calls && !live_images);
    /* 独立数据错误保持-1；不能只因同时有quit就把资源/数据失败洗成退出。 */
    game.map.cells=NULL;
    CHECK(validate_attack_flow(&game,&vga,&fixture_other,NULL)==-1);
    CHECK(!acknowledged && !test_frames && !rng_calls && !live_images);
}
/* B5共用同一底框：19953四步展开先吞入场键、No短停不等箭头、
 * 交换的-5不能误取新物品名。这里采样原资源，不启动窗口。 */
static void test_loot_presentation_contract(void) {
    fd2_vga vga;
    reset(&vga);
    CHECK(fd2_field_command_assets_open(&fixture_game.command_assets, &fixture_other) == 0);
    field_progress_text text;
    fd2_field_loot_notice notice = {0};
    notice.kind=FD2_FIELD_LOOT_MESSAGE;notice.fragment=425;
    notice.item_id=192;notice.old_item_id=0;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0 && text.count>0);
    notice.old_item_id=255;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==-1);
    notice.old_item_id=0;notice.fragment=432;
    CHECK(loot_resources_preflight(&fixture_game,&notice,&fixture_other)==0);
    CHECK(!rng_calls && !test_frames && !live_images);
    field_progress_box box={0};
    CHECK(progress_box_open(&box,&fixture_game,&vga,&fixture_other,0,1)==0);
    /* 原48/51 normal资源没有raw0；显式隔离两个输入样本检验4ed4f的
     * 透明语义，不能把高亮49的像素分布冒充normal来源。 */
    uint8_t *original_buttons[2],*transparent_buttons[2];
    for(unsigned button=0;button<2;button++) {
        fd2_image *image=&fixture_game.command_assets.frames[button?51:48];
        size_t size=(size_t)image->width*(size_t)image->height;
        original_buttons[button]=image->pixels;
        transparent_buttons[button]=malloc(size);CHECK(transparent_buttons[button]);
        memcpy(transparent_buttons[button],image->pixels,size);
        CHECK(transparent_buttons[button][0]);transparent_buttons[button][0]=0;
        image->pixels=transparent_buttons[button];
    }
    memcpy(choice_before,vga.framebuffer,sizeof(choice_before));choice_check_zeros=1;
    clock_step=110;key_at=10;
    fd2_field_loot_reply reply=FD2_FIELD_LOOT_CANCEL;
    CHECK(loot_choice(&box,&fixture_game,&vga,&reply)==0 && reply==FD2_FIELD_LOOT_YES);
    CHECK(rng_calls==2 && test_frames>=14);
    CHECK(choice_redraws==8); /* begin、六次到门tick、close；整页仍归同一App。 */
    CHECK(choice_zeros_checked); /* 原4ed4f raw0不覆盖UI底板，不只核非0按钮像素。 */
    CHECK(!fd2_input_has_any_key(&vga.input));
    for(unsigned button=0;button<2;button++) {
        fixture_game.command_assets.frames[button?51:48].pixels=original_buttons[button];
        free(transparent_buttons[button]);
    }
    progress_box_free(&box);
    reset(&vga);clock_step=16;key_at=1;
    CHECK(loot_fixed_wait(&vga,200,0)==0 && now_ms>=200 && test_frames>=13);
    CHECK(!fd2_input_has_any_key(&vga.input) && !rng_calls && !water_calls &&
        !vga.actor_jitter_tick && !vga.actor_jitter_phase);
    reset(&vga);clock_step=16;key_at=13;
    CHECK(loot_fixed_wait(&vga,200,0)==0 && test_frames==13 && now_ms==208);
    CHECK(fd2_input_has_any_key(&vga.input)); /* 3790a无尾clear，保最后一帧合法pulse。 */
    CHECK(progress_wait(&box,&fixture_game,&vga,&fixture_other,0,0)==FD2_SCENE_RESULT_OK);
    CHECK(test_frames==13 && rng_calls==1 && !water_calls && !fd2_input_has_any_key(&vga.input) &&
        !vga.actor_jitter_tick && !vga.actor_jitter_phase);
    reset(&vga);clock_step=16;quit_at=2;
    CHECK(loot_fixed_wait(&vga,200,0)==FD2_SCENE_RESULT_HOST_QUIT && now_ms<200);
    CHECK(!water_calls && !vga.actor_jitter_tick && !vga.actor_jitter_phase);
    fd2_field_command_assets_close(&fixture_game.command_assets);
}
/* 190ac的421/422同框累计；1aa1d/190ac外wait0不画也不擦箭头。
 * 与15f84的-3传1独立，不能用test自动点击箭头掩盖原版无箭头的等待。 */
static void test_loot_accumulated_text_and_no_arrow_wait(void) {
    fd2_vga vga;reset(&vga);
    field_progress_box box={0};field_progress_text text;
    fd2_field_loot_notice notice={0};notice.kind=FD2_FIELD_LOOT_MESSAGE;
    notice.fragment=421;
    CHECK(progress_box_open(&box,&fixture_game,&vga,&fixture_other,0,1)==0);
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0);
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&text,99,127,1)==0);
    uint8_t first_line[17*210];
    for(unsigned y=0;y<17;y++)memcpy(first_line+y*210,vga.framebuffer+(127+y)*320+98,210);
    notice.fragment=422;notice.item_id=192;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0);
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&text,99,146,1)==0);
    for(unsigned y=0;y<17;y++)CHECK(!memcmp(first_line+y*210,vga.framebuffer+(127+y)*320+98,210));
    uint8_t unchanged[14*10];
    for(unsigned y=0;y<10;y++)memcpy(unchanged+y*14,vga.framebuffer+(181+y)*320+191,14);
    clock_step=110;key_at=5;rng_calls=0;test_frames=0;now_ms=0;
    CHECK(progress_wait(&box,&fixture_game,&vga,&fixture_other,0,0)==0 && rng_calls==2);
    CHECK(!arrow_draws && !arrow_erases);
    for(unsigned y=0;y<10;y++)CHECK(!memcmp(unchanged+y*14,vga.framebuffer+(181+y)*320+191,14));
    /* 新框423必须先恢复背景/开框，旧421第一行不能沿用。 */
    memcpy(vga.framebuffer,box.background,sizeof(vga.framebuffer));progress_box_free(&box);
    CHECK(progress_box_open(&box,&fixture_game,&vga,&fixture_other,0,1)==0);
    notice.fragment=423;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0);
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&text,99,127,1)==0);
    unsigned different=0;
    for(unsigned y=0;y<17;y++)for(unsigned x=0;x<210;x++)
        different+=first_line[y*210+x]!=vga.framebuffer[(127+y)*320+98+x];
    CHECK(different>100);
    /* 晚坏动态名在绘制前拒绝，完整VGA不写，也不消费RNG。 */
    uint8_t before[64000];memcpy(before,vga.framebuffer,sizeof(before));
    unsigned previous_rng=rng_calls;notice.fragment=422;notice.item_id=255;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==-1);
    CHECK(!memcmp(before,vga.framebuffer,sizeof(before)) && rng_calls==previous_rng);
    test_frames=key_at=0;quit_at=2;now_ms=0;rng_calls=0;
    CHECK(progress_wait(&box,&fixture_game,&vga,&fixture_other,0,0)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(rng_calls==1 && !arrow_draws && !arrow_erases);
    progress_box_free(&box);CHECK(!live_images && !box.page && !box.background);
}
static int test_adjacent(const fd2_field_unit *a,const fd2_field_unit *b,void *context) {
    (void)context;
    return a->side!=b->side && abs((int)a->x-b->x)+abs((int)a->y-b->y)==1;
}
static void test_loot_ui_commit_boundary(void) {
    for(unsigned mode=0;mode<4;mode++) {
        fd2_vga vga;reset(&vga);
        fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
        CHECK(fd2_field_game_open_resources(game,&fixture_other,0)==0);
        for(size_t i=0;i<3;i++) {
            CHECK(fd2_field_units_set(&game->units,i,i==1?96:0,(uint8_t)i,i==1?0:2,
                i==1?8:i==2?10:7,14)==0);
            fd2_field_unit *u=&game->units.items[i];
            u->hp=u->hp_max=i==1?1:100;u->level=1;u->movement_profile=1;
            u->movement_points=3;u->attack=100;u->accuracy=1000;
            ((uint8_t*)u)[0x31]=i==1?0:0xff;
            ((uint8_t*)u)[0x32]=i==1?192:0;
            for(size_t slot=0;slot<8;slot++)((uint8_t*)u)[10+slot*2]=0x80;
        }
        ((uint8_t*)&game->units.items[0])[10]=0x40;
        game->active_side=2;game->selected_unit=0;game->interaction=FD2_FIELD_INTERACTION_COMMAND;
        CHECK(fd2_field_game_set_attack_hooks(game,test_adjacent,NULL,NULL,NULL,test_rng,NULL)==0);
        CHECK(fd2_field_game_set_loot_preflight(game,loot_resources_preflight,&fixture_other)==0);
        CHECK(fd2_field_game_begin_attack(game)==0);
        CHECK(fd2_field_game_resolve_attack(game)==0);
        CHECK(game->loot.notice.fragment==432 && fd2_field_unit_inventory_count(&game->units.items[0])==1);
        test_frames=0;rng_calls=0;
        loot_wait_game=game;auto_loot_keys=mode!=3;
        if(mode==3){text_ready_vga=&vga;pulse_at_text_ready=1;}
        if(mode==1)quit_at=7; /* 六相位开框之后第一字；尚未text_ready。 */
        if(mode==2)quit_when_loot_ready=1;
        int result=play_field_loot(game,&vga,&fixture_other,NULL,0);
        CHECK(result==((mode==1 || mode==2)?FD2_SCENE_RESULT_HOST_QUIT:0));
        CHECK(game->units.items[1].hp==0); /* 上游攻击不因呈现关闭回滚。 */
        CHECK(fd2_field_unit_inventory_count(&game->units.items[0])==(mode==1?1u:2u));
        if(mode==1)CHECK(!game->loot.text_ready);
        if(mode==2)CHECK(game->loot.text_ready && game->loot.notice.fragment==432);
        if(mode==3)CHECK(pulse_ready_sent==1 && pulse_consumed==1 && !game->loot.source);
        CHECK(!live_images);
        CHECK(!arrow_draws && !arrow_erases);
        loot_wait_game=NULL;
        fd2_field_game_close(game);free(game);
    }
}

static void test_loot_reply_starts_common_without_busy_loop(void) {
    fd2_vga vga;reset(&vga);
    fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
    CHECK(fd2_field_game_open_resources(game,&fixture_other,0)==0);
    CHECK(fd2_field_units_set(&game->units,0,0,0,2,7,14)==0);
    game->units.items[0].hp=game->units.items[0].hp_max=100;
    for(size_t slot=0;slot<8;slot++)((uint8_t *)&game->units.items[0])[10+slot*2]=0x80;
    CHECK(fd2_field_game_set_loot_preflight(game,loot_resources_preflight,&fixture_other)==0);
    game->event_preflight=common_event_resources_preflight;game->event_preflight_userdata=&fixture_other;
    game->attack_rng=test_rng;
    const fd2_static_flow *loot=fd2_static_flow_lookup("loot","loot");CHECK(loot);
    int pc=fd2_static_flow_label(loot,"drop.received");CHECK(pc>=0 && pc<256);
    game->loot.source=1;game->loot.count=1;game->loot.actor_index=0;
    game->loot.action_kind=255;game->loot.step=(uint8_t)pc;
    game->loot.descriptors[0][1]=29;
    game->loot.notice=(fd2_field_loot_notice){.kind=FD2_FIELD_LOOT_MESSAGE,
        .actor_index=0,.fragment=432,.item_id=29,.open_box=1,.close_box=1,.wait_key=1};
    inject_common_on_reply=single_box_guard=1;
    CHECK(play_field_progress(game,&vga,&fixture_other,NULL,1)==0);
    CHECK(common_injected==1 && !game->event_resume.active && !game->loot.child.active && !game->loot.source);
    CHECK(game->cell_action_completed[0]==1 && fd2_field_unit_inventory_count(&game->units.items[0])==2);
    const uint8_t *raw=(const uint8_t *)&game->units.items[0];
    CHECK(raw[11]==29 && raw[13]==101 && !live_images && test_frames<2000);
    fd2_field_game_close(game);free(game);
}

typedef struct {
    fd2_vga *vga;
    unsigned calls,rng_at_quit;
    int error;
    fd2_field_units units_at_quit;
} runtime_quit_fixture;
static int quit_from_real_turn_wait(fd2_field_game *game,
        fd2_field_runtime_present_step step,int value,void *userdata) {
    runtime_quit_fixture *trace=userdata;
    CHECK(step==FD2_FIELD_RUNTIME_RECOVERY_WAIT && value==1 && trace->calls++==0);
    /* 真实ACK已提交EX后进入原turn WAIT。这里显式清夹具input位，模拟
     * 仅返回正值的runtime callback；生产take_quit本身是持久读取，不清位。
     * Game的turn_present随后置唯一锁存，不能依赖input位仍存在。 */
    CHECK(game->units.items[0].experience==15 && !game->progress.notice.kind);
    trace->vga->input.quit_requested=1;
    CHECK(fd2_input_take_quit(&trace->vga->input));
    trace->vga->input.quit_requested=0;
    trace->rng_at_quit=rng_calls;trace->units_at_quit=game->units;
    if(trace->error)game->failure=FD2_FIELD_FAILURE_ACTION;
    return 1;
}
static void test_runtime_quit_from_real_reward_ack(void) {
    for(int error=0;error<2;error++) {
        fd2_vga vga;reset(&vga);real_progress=1;
        fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
        CHECK(fd2_field_game_open_resources(game,&fixture_other,0)==0);
        for(size_t i=0;i<2;i++) {
            CHECK(fd2_field_units_set(&game->units,i,i?96:0,(uint8_t)i,i?0:2,7+(int)i,14)==0);
            fd2_field_unit *unit=&game->units.items[i];
            unit->hp=unit->hp_max=100;unit->level=1;
            unit->flags=i?0:0x80;
            for(size_t slot=0;slot<8;slot++)((uint8_t*)unit)[10+slot*2]=0x80;
        }
        game->units.items[0].experience=10;
        game->active_side=2;game->turn_number=1;
        game->selected_unit=game->command_selected=-1;game->interaction=FD2_FIELD_INTERACTION_BROWSE;
        game->pending_cell_event=(fd2_field_pending_cell_event){1,77,4};
        game->attack_rng=test_rng;game->experience_value=5;
        runtime_quit_fixture trace={.vga=&vga,.error=error};
        game->runtime_present=quit_from_real_turn_wait;game->runtime_present_userdata=&trace;
        CHECK(fd2_field_game_set_progress_preflight(game,progress_resources_preflight,&fixture_other)==0);
        CHECK(fd2_field_game_complete_player_controller(game,0)==0 &&
            game->progress.notice.kind==FD2_FIELD_PROGRESS_GAIN);
        CHECK(play_field_progress(game,&vga,&fixture_other,NULL,1)==
            (error?FD2_SCENE_RESULT_ERROR:FD2_SCENE_RESULT_HOST_QUIT));
        CHECK(trace.calls==1 && real_acknowledged==2 && game->runtime_host_quit &&
            game->failure==(error?FD2_FIELD_FAILURE_ACTION:FD2_FIELD_FAILURE_NONE));
        CHECK(!vga.input.quit_requested);
        CHECK(test_frames<100);
        CHECK(!live_images);
        CHECK(rng_calls==trace.rng_at_quit);
        CHECK(!memcmp(&game->units,&trace.units_at_quit,sizeof(game->units)) &&
            game->units.items[0].experience==15 && game->active_side==2 && game->turn_number==1 &&
            game->player_post_waiting_phase && game->pending_cell_event.valid &&
            game->pending_cell_event.action==77 && !game->event_log_count &&
            !game->progress.notice.kind && !game->loot.source && !game->event_resume.active);
        fd2_save_battle_snapshot snapshot;
        CHECK(fd2_field_game_export_battle_snapshot(game,&snapshot)!=0); /* 不导出中途状态。 */
        fd2_field_game_close(game);free(game);
    }
}
static void test_modal_consumed_quit_and_error_priority(void) {
    for(int error=0;error<2;error++)for(int modal=0;modal<3;modal++) {
        fd2_vga vga;reset(&vga);
        fd2_field_game game={0};game.ready=1;game.units.count=1;game.runtime_host_quit=1;
        if(error)game.failure=FD2_FIELD_FAILURE_ACTION;
        if(modal==2)game.event_resume=(fd2_field_event_resume){.flow=&injected_common_flow,.pc=0,.actor=0,.active=1};
        fd2_field_game before=game;
        int result=modal==0?play_field_loot(&game,&vga,&fixture_other,NULL,1):
            modal==1?play_field_progress(&game,&vga,&fixture_other,NULL,1):
            play_common_events(&game,&vga,&fixture_other,NULL,1);
        CHECK(result==(error?FD2_SCENE_RESULT_ERROR:FD2_SCENE_RESULT_HOST_QUIT));
        CHECK(!memcmp(&game,&before,sizeof(game)) && !test_frames && !rng_calls && !live_images &&
            !real_acknowledged && !resume_calls && !acknowledged && !vga.input.quit_requested);
    }
}
static void test_cell_open_secondary_cue(void) {
    fd2_pcm_bank bank={0};CHECK(fd2_pcm_bank_open(&bank,&fixture_other,31)==0);
    CHECK(fd2_pcm_bank_count(&bank)==13);
    fd2_pcm_player primary={0},secondary={0};
    fd2_field_audio audio={.player=&primary,.secondary_player=&secondary,.ui_bank=&bank};
    fd2_vga vga;reset(&vga);
    fd2_field_game game={0};game.ready=1;game.option_values[1]=1;
    field_runtime_presentation runtime={&vga,&audio,0,0,NULL};
    cell_secondary=&secondary;cell_ui_bank=&bank;cell_pcm_calls=0;cell_pcm_failure=0;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_CELL_OPEN_CUE,12,&runtime)==0 && cell_pcm_calls==1);
    CHECK(!test_frames && !rng_calls && !runtime.host_quit);
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_CELL_OPEN_CUE,11,&runtime)==-1 && cell_pcm_calls==1);
    game.option_values[1]=0;audio.ui_bank=NULL;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_CELL_OPEN_CUE,12,&runtime)==0 && cell_pcm_calls==1);
    game.option_values[1]=1;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_CELL_OPEN_CUE,12,&runtime)==-1 && cell_pcm_calls==1);
    audio.secondary_player=NULL;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_CELL_OPEN_CUE,12,&runtime)==0 && cell_pcm_calls==1);
    audio.secondary_player=&secondary;audio.ui_bank=&bank;cell_pcm_failure=1;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_CELL_OPEN_CUE,12,&runtime)==-1 && cell_pcm_calls==2);
    vga.input.quit_requested=1;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_CELL_OPEN_CUE,12,&runtime)==1 && cell_pcm_calls==2 && runtime.host_quit);
    CHECK(!test_frames && !rng_calls);
    fd2_pcm_bank_close(&bank);cell_pcm_failure=0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--notice-wait-json")) {test_notice_wait_vectors(1);return 0;}
    test_field_page=fd2_field_page_create();CHECK(test_field_page);
    CHECK(fd2_archive_open(&fixture_other, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_archive_open(&fixture_game.fdtxt_archive, "original_game/FDTXT.DAT") == 0);
    CHECK(fd2_archive_open(&fixture_game.dato_archive, "original_game/DATO.DAT") == 0);
    CHECK(fd2_text_entry_open_entry(&fixture_game.ui_text, &fixture_game.fdtxt_archive, 0) == 0);
    CHECK(fd2_font_open_fdother(&fixture_game.font, &fixture_other, 4) == 0);
    CHECK(fd2_field_info_assets_open(&fixture_game.info_assets,&fixture_other)==0);
    fixture_game.ready = 1; fixture_game.units.count = 1; fixture_game.attack_rng = test_rng;
    fixture_game.map.cells = calloc(1, sizeof(*fixture_game.map.cells)); CHECK(fixture_game.map.cells != NULL);
    for (unsigned i = 0; i < 2; i++) {
        fd2_vga page = {0};
        CHECK(fd2_scene_field_message_arrow(&page, &fixture_other, 18 + i) == 0);
        for (int row = 0; row < 10; row++)
            memcpy(arrow_pixels[i] + row * 14, page.framebuffer + (181 + row) * VGA_STRIDE + 191, 14);
    }
    test_resources(); test_signed_number_safe_projection(); test_box_and_wait(); test_notice_wait_vectors(0); test_script_boundary();
    test_attack_validation_preserves_quit_result(); test_loot_presentation_contract();
    test_loot_accumulated_text_and_no_arrow_wait();
    test_loot_ui_commit_boundary();
    test_loot_reply_starts_common_without_busy_loop();
    test_runtime_quit_from_real_reward_ack();
    test_modal_consumed_quit_and_error_priority();
    test_cell_open_secondary_cue();
    free(fixture_game.map.cells); fd2_text_entry_close(&fixture_game.ui_text);
    fd2_field_info_assets_close(&fixture_game.info_assets);
    fd2_archive_close(&fixture_game.dato_archive); fd2_archive_close(&fixture_game.fdtxt_archive); fd2_archive_close(&fixture_other);
    fd2_field_page_destroy(test_field_page);
    puts("field_progress_play_test: ok (resources, wait/RNG, staged ACK, cleanup; no GUI)");
    return 0;
}
