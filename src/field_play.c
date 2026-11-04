/* 炎龙骑士团 2 SDL3 重写 - 正式战场循环入口
 *
 * 当前阶段完成 session 生命周期、开场状态交接、键盘光标、移动、
 * 四向图形指令菜单、无演出攻击和取消回退。
 */

#include "field_play.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "field_ai.h"
#include "field_attack.h"
#include "field_game.h"
#include "field_rng.h"
#include "field_visual.h"
#include "font.h"
#include "input.h"
#include "text.h"
#include "scene.h"

/* 菜单机器码每相位执行一次 field buffer 合成，但没有 BIOS tick delay。
 * SDL 以一个约 60 Hz 宿主帧分隔四个 present，避免连续上传被合并；
 * 该值不登记为原版 DOS 时序。 */
#define FD2_FIELD_COMMAND_FRAME_MS 16
#define FD2_FIELD_AUTOMATIC_ACTION_MS 150

/* 正式输入回归可固定计时器派生的 RNG 初值；普通运行未设置环境变量时
 * 保持现有战场初值。该入口只控制随机流，不改 actor 坐标或属性。 */
static uint16_t field_rng_initial_seed(void) {
    const char *value = getenv("FD2SDL_FIELD_RNG_SEED");
    if (!value || *value == '\0') return 0x7a18u;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 0);
    if (!end || *end != '\0' || parsed > UINT16_MAX) return 0x7a18u;
    return (uint16_t)parsed;
}

/* FDTXT[410..420] 当前只确认普通字形、0xfffe换行、0xffff结束。
 * 先验证完整span，避免后半段坏字形在提示已画出后才被静默跳过。 */
static int system_confirmation_fragment(const fd2_field_game *game,
                                          size_t fragment,
                                          const uint8_t **tokens,
                                          size_t *draw_count) {
    if (!game || !game->ready || !game->font.glyphs ||
        game->font.glyph_count == 0 ||
        game->font.glyph_count > game->font.size / 32u)
        return -1;
    const uint8_t *data;
    size_t count;
    if (fd2_text_entry_get_fragment(&game->ui_text, fragment,
                                    &data, &count) != 0)
        return -1;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(data, i);
        if (token == 0xffffu) {
            *tokens = data;
            *draw_count = i;
            return 0;
        }
        if (token == 0xfffeu) continue;
        if (token >= 0xff00u || token >= game->font.glyph_count)
            return -1;
    }
    return -1;
}

static int play_system_confirmation_text(fd2_field_game *game,
                                         fd2_vga *vga,
                                         size_t fragment,
                                         int fast) {
    const uint8_t *tokens;
    size_t token_count;
    if (!vga || system_confirmation_fragment(
            game, fragment, &tokens, &token_count) != 0)
        return -1;
    /* 预检已完成；这里只呈现已确定提示，不提交YES/NO或玩法。 */
    for (int row = 0; row < 86; row++)
        memset(vga->framebuffer + (112 + row) * VGA_STRIDE + 5,
               0xcd, 310u);
    int x = 15;
    int y = 119;
    for (size_t i = 0; i < token_count; i++) {
        uint16_t token = fd2_text_token_at(tokens, i);
        if (token == 0xfffeu) {
            x = 15;
            y += 16;
            continue;
        }
        fd2_font_draw_glyph(vga, &game->font, token,
                            x, y, 0xcd, 0x4c, -1);
        x += 16;
    }
    fd2_vga_present(vga);
    if (!fast) fd2_delay_ms(250);
    return 0;
}

static void drain_frame_keys(fd2_vga *vga) {
    while (fd2_input_take_key(&vga->input, NULL)) {}
}

static void overview_copy_rect(uint8_t *dst, const uint8_t *src,
                                int x, int y, int src_x, int src_y,
                                int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (x < 0) { src_x -= x; width += x; x = 0; }
    if (y < 0) { src_y -= y; height += y; y = 0; }
    if (x + width > VGA_W) width = VGA_W - x;
    if (y + height > VGA_H) height = VGA_H - y;
    if (width <= 0 || height <= 0) return;
    for (int row = 0; row < height; row++)
        memcpy(dst + (size_t)(y + row) * VGA_STRIDE + x,
               src + (size_t)(src_y + row) * VGA_STRIDE + src_x,
               (size_t)width);
}

/* sub_1B1E7 @0x1b1e7（原登记 sub_1B1E7）：四块12相位开合。
 * 主体分别用 sub_1AF99 @0x1af99／sub_1AF1E @0x1af1e 的右入／左出；
 * 其余三块依次复现 sub_1B019 @0x1b019、sub_1B0AD @0x1b0ad、
 * sub_1B14B @0x1b14b，不借用角色详情或关卡转场动画。 */
static void overview_transition_frame(fd2_vga *vga, const uint8_t *overview,
                                        int phase, int opening) {
    int x = 75;
    if (phase < 5)
        x += (opening ? 50 : -50) * (4 - phase);
    overview_copy_rect(vga->framebuffer, overview,
                        x, 37, 75, 37, 170, 117);
    if (phase >= 3) {
        int y = phase <= 7 ? 19 - 6 * (7 - phase) : 19;
        overview_copy_rect(vga->framebuffer, overview,
                            109, y, 109, 19, 102, 17);
    }
    if (phase >= 5) {
        int offset = phase <= 9 ? 9 * (9 - phase) : 0;
        /* 原版 phase5 的 height=36-45=-9，signed 循环复制0行。 */
        int height = offset + 171 > 200 ? offset - 45 : 16;
        overview_copy_rect(vga->framebuffer, overview,
                            75, 155 + offset, 75, 155, 170, height);
    }
    if (phase >= 8) {
        int offset = 4 * (12 - phase);
        /* @0x1b14b：phase8 offset=16；16+187>200，height=16-28=-12，
         * 因此不绘；不能把 y188 通用裁剪为12行而改变原版外观。 */
        int height = offset + 187 > 200 ? offset - 28 : 15;
        overview_copy_rect(vga->framebuffer, overview,
                            129, 172 + offset, 129, 172, 63, height);
    }
}

/* 同步只读 modal：不调用玩法提交／事件／RNG，两个像素缓冲归本次调用。
 * 原版使用 input_check 与 @0x4e381 清BIOS队列；SDL只耗尽当前帧脉冲，
 * 不重置物理 held／repeat 状态。 */
static int play_field_overview(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !vga || !game->ready || game->stage != 0 ||
        game->failure != FD2_FIELD_FAILURE_NONE || game->active_side != 2 ||
        game->selected_unit != -1 || game->detail_visible ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    drain_frame_keys(vga);
    uint8_t *background = NULL, *overview = NULL;
    int result = FD2_SCENE_RESULT_ERROR;
    background = malloc(VGA_W * VGA_H);
    if (!background) goto done;
    overview = malloc(VGA_W * VGA_H);
    if (!overview) goto done;

    /* renderer先在未present的原frame上预检／合成；不用复制整个VGA/input
     * 对象借用framebuffer。成功缓存四块后，A才改为最初干净背景。 */
    memcpy(background, vga->framebuffer, VGA_W * VGA_H);
    fd2_field_overview data = {
        game->stage, game->turn_number, game->resource_value, &game->units
    };
    if (fd2_field_overview_draw(vga, &game->info_assets, &game->font,
                                &game->ui_text, &data) != 0) {
        memcpy(vga->framebuffer, background, VGA_W * VGA_H);
        goto done;
    }
    memcpy(overview, vga->framebuffer, VGA_W * VGA_H);
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    fd2_field_game_render_background(game, vga);
    memcpy(background, vga->framebuffer, VGA_W * VGA_H);
    for (int phase = 0; phase < 12; phase++) {
        memcpy(vga->framebuffer, background, VGA_W * VGA_H);
        overview_transition_frame(vga, overview, phase, 1);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            goto done;
        }
        drain_frame_keys(vga);
    }
    int reported_open = 0;
    for (;;) {
        (void)fd2_field_water_tick(vga, SDL_GetTicks()); /* 稳定总览等待。 */
        fd2_field_game_tick(game, SDL_GetTicks());
        fd2_field_game_render_background(game, vga);
        /* phase12为四块稳定位置；静态内容已预检并缓存，不重复格式化。 */
        overview_transition_frame(vga, overview, 12, 1);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            goto done;
        }
        if (!reported_open) {
            puts("field overview: opened");
            fflush(stdout);
            reported_open = 1;
        }
        if (fd2_input_take_key(&vga->input, NULL)) {
            drain_frame_keys(vga);
            break;
        }
    }
    for (int phase = 11; phase >= 0; phase--) {
        memcpy(vga->framebuffer, background, VGA_W * VGA_H);
        overview_transition_frame(vga, overview, phase, 0);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            goto done;
        }
        drain_frame_keys(vga);
    }
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    fd2_vga_present(vga);
    if (fd2_input_take_quit(&vga->input)) {
        result = FD2_SCENE_RESULT_HOST_QUIT;
        goto done;
    }
    drain_frame_keys(vga);
    puts("field overview: closed");
    fflush(stdout);
    result = FD2_SCENE_RESULT_OK;
done:
    fd2_vga_wait_frame_deadline(vga);
    free(overview);
    free(background);
    return result;
}

/* sub_2000A @0x2000a（原登记 sub_2000A）：F1/PageUp独立全场缩放图。
 * stage0/shape0/24×24/288帧是本项目能力门，不是原版关卡限制。
 * 预检先于任何framebuffer写入；复用session的terrain.frames，不替换owner。 */
static int auxiliary_field_ready(const fd2_field_game *game) {
    if (!game || !game->ready || game->stage != 0 ||
        game->failure != FD2_FIELD_FAILURE_NONE || game->active_side != 2 ||
        game->selected_unit != -1 || game->detail_visible ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->player_post_waiting_phase ||
        game->map.width != 24 || game->map.height != 24 || !game->map.cells ||
        game->metadata.shape_index != 0 || game->terrain.shape_index != 0 ||
        game->terrain.sheet.width != 24 || game->terrain.sheet.height != 24 ||
        game->terrain.sheet.frame_count != 288 || !game->terrain.frames ||
        !game->terrain.attrs || game->units.count > FD2_FIELD_MAX_UNITS ||
        game->cursor_cell_x < 0 || game->cursor_cell_x >= game->map.width ||
        game->cursor_cell_y < 0 || game->cursor_cell_y >= game->map.height ||
        game->camera_cell_x < 0 ||
        game->camera_cell_x > game->map.width - FD2_FIELD_VIEW_CELLS_X ||
        game->camera_cell_y < 0 ||
        game->camera_cell_y > game->map.height - FD2_FIELD_VIEW_CELLS_Y)
        return 0;
    for (int y = 0; y < game->map.height; y++) {
        for (int x = 0; x < game->map.width; x++) {
            size_t index = fd2_field_cell_terrain(
                fd2_field_map_cell(&game->map, x, y));
            const fd2_image *frame = fd2_terrain_frame(&game->terrain, index);
            if (index >= game->terrain.attr_count || !frame ||
                !frame->pixels || frame->width != 24 || frame->height != 24)
                return 0;
        }
    }
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if ((unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            unit->unit_id == 121 || unit->race == 10)
            continue;
        if (unit->side > 2 || unit->x >= game->map.width ||
            unit->y >= game->map.height)
            return 0;
    }
    return 1;
}

/* field_view_render_scaled @0x1f558（原登记同名）：tile跨度0xc00，
 * 单像素0x80。既有sampler对负坐标作floor，绘制内区312×192；先清整帧
 * 保留4px黑边。phase=0固定使用原terrain_id，不推进地形动画或遮挡层。 */
static void auxiliary_transition_frame(const fd2_field_game *game,
                                        fd2_vga *vga, int phase) {
    int start_x = game->camera_cell_x * 0xc00 + FD2_FIELD_VIEW_CELLS_X * 0x600;
    int start_y = game->camera_cell_y * 0xc00 + FD2_FIELD_VIEW_CELLS_Y * 0x600;
    int end_x = game->map.width * 0x600;
    int end_y = game->map.height * 0x600;
    /* 开1..7与关6..1共用参数；中心系数为(phase+1)/8，非phase/8。 */
    int center_x = start_x + (end_x - start_x) * (phase + 1) / 8;
    int center_y = start_y + (end_y - start_y) * (phase + 1) / 8;
    int step = 0x80 + phase * 0x280 / 7;
    fd2_vga_clear(vga, 0);
    fd2_terrain_render_field_scaled_animated(vga, &game->terrain, &game->map,
                                             center_x, center_y, step, 0, 0);
}

/* sub_1F6EF @0x1f6ef（原登记 sub_1F6EF）：stage0每tile4px，
 * 只写3×3固色块。所有参与者均已预检，focus最后覆盖同格actor。 */
static void auxiliary_marker(fd2_vga *vga, const fd2_field_map *map,
                              int x, int y, uint8_t color) {
    int sx = x * 4 + 160 - map->width * 2;
    int sy = y * 4 + 100 - map->height * 2;
    for (int row = 0; row < 3; row++)
        memset(vga->framebuffer + (size_t)(sy + row) * VGA_STRIDE + sx, color, 3);
}

static void auxiliary_markers(const fd2_field_game *game, fd2_vga *vga,
                               int phase) {
    /* object2 canonical0x5208a：side0/1/2的8色bank起点。 */
    static const uint8_t bases[3] = {32, 80, 72};
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if ((unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            unit->unit_id == 121 || unit->race == 10)
            continue;
        auxiliary_marker(vga, &game->map, unit->x, unit->y,
                           (uint8_t)(bases[unit->side] + phase));
    }
    auxiliary_marker(vga, &game->map, game->cursor_cell_x,
                       game->cursor_cell_y, (uint8_t)(24 + phase));
}

static void auxiliary_next_phase(int *phase, int *direction) {
    *phase += *direction;
    if (*phase < 0 || *phase > 7) {
        *direction = -*direction;
        *phase += *direction; /* 端点重复：7..0,0..7,7.. */
    }
}

static int play_field_auxiliary(fd2_field_game *game, fd2_vga *vga) {
    if (!vga) return FD2_SCENE_RESULT_ERROR;
    /* 主loop已先处理quit；私有入口也保持宿主关闭先于资源／能力门。 */
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (!auxiliary_field_ready(game)) return FD2_SCENE_RESULT_ERROR;
    drain_frame_keys(vga);
    int result = FD2_SCENE_RESULT_HOST_QUIT;
    /* 原版开合每次复制整帧，没有DOS帧时长证据；16ms是SDL呈现间隔。 */
    for (int phase = 1; phase <= 7; phase++) {
        auxiliary_transition_frame(game, vga, phase);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) goto done;
        drain_frame_keys(vga);
    }
    int phase = 7, direction = -1;
    uint64_t last_pulse = SDL_GetTicks();
    int reported_open = 0;
    for (;;) {
        /* 稳态地形保留开phase7的原始cache采样；仅重写同位置方块。
         * 不调用game_tick，不改palette/dac，冻结进入时的当前水色周期。 */
        auxiliary_markers(game, vga, phase);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) goto done;
        if (!reported_open) {
            puts("field auxiliary: opened");
            fflush(stdout);
            reported_open = 1;
        }
        if (fd2_input_take_key(&vga->input, NULL)) {
            drain_frame_keys(vga);
            break;
        }
        uint64_t now = SDL_GetTicks();
        if (now < last_pulse || now - last_pulse >= 110) {
            auxiliary_next_phase(&phase, &direction);
            last_pulse = now;
        }
    }
    for (int close_phase = 6; close_phase >= 1; close_phase--) {
        auxiliary_transition_frame(game, vga, close_phase);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) goto done;
        /* 原版此处没有额外BIOS清队。SDL耗尽当前帧脉冲并在调用方结束
         * 本帧输入，避免关闭方向／确认键落回browse；保留held/repeat。 */
        drain_frame_keys(vga);
    }
    /* sub_2000A @0x2000a尾调用sub_11CAC(0)：恢复普通画面前才恢复水色。
     * 只调用视觉water tick；开/稳/关期间不tick，不追赶modal停留时间。 */
    (void)fd2_field_water_tick(vga, SDL_GetTicks());
    fd2_field_game_render(game, vga);
    fd2_vga_present(vga);
    if (fd2_input_take_quit(&vga->input)) goto done;
    drain_frame_keys(vga);
    puts("field auxiliary: closed");
    fflush(stdout);
    result = FD2_SCENE_RESULT_OK;
done:
    fd2_vga_wait_frame_deadline(vga);
    return result;
}


static void apply_field_audio_options(const fd2_field_game *game,
                                      fd2_field_audio *field_audio) {
    if (!game || !field_audio || !field_audio->player ||
        !field_audio->player->audio)
        return;
    /* DS:0x1e61 控制 sequence 音量，DS:0x1e62 控制 SFX 播放门。当前
     * 尚无 XMIDI source；仍设置两条 bus，供后续音乐后端直接继承。 */
    (void)fd2_audio_set_bus_gain(
        field_audio->player->audio, FD2_AUDIO_BUS_MUSIC,
        game->option_values[0] ? 1.0f : 0.0f);
    (void)fd2_audio_set_bus_gain(
        field_audio->player->audio, FD2_AUDIO_BUS_SFX,
        game->option_values[1] ? 1.0f : 0.0f);
}

static void detail_phase_sfx(void *userdata, int opening, int phase) {
    fd2_field_audio *audio = userdata;
    if (!audio || fd2_field_detail_sfx_for_phase(opening, phase) < 0) return;
    /* 原版详情路径在上述 phase 调用
     * sfx_play(DAT_00003eec,sample,1)；全局 AIL handle 会重启前一声。 */
    (void)fd2_field_audio_play(audio, opening
        ? FD2_FIELD_SFX_DETAIL_OPEN : FD2_FIELD_SFX_DETAIL_CLOSE);
}

typedef enum {
    FD2_FIELD_ACTION_NONE = 0,
    FD2_FIELD_ACTION_LEFT,
    FD2_FIELD_ACTION_RIGHT,
    FD2_FIELD_ACTION_UP,
    FD2_FIELD_ACTION_DOWN,
    FD2_FIELD_ACTION_CONFIRM,
    FD2_FIELD_ACTION_CANCEL,
    FD2_FIELD_ACTION_DETAIL,
    FD2_FIELD_ACTION_AUXILIARY,
    FD2_FIELD_ACTION_FOCUS_CYCLE,
    FD2_FIELD_ACTION_EXIT
} fd2_field_action;

static fd2_field_action field_action_from_input(fd2_input_action action) {
    switch (action) {
        case FD2_INPUT_ACTION_LEFT: return FD2_FIELD_ACTION_LEFT;
        case FD2_INPUT_ACTION_RIGHT: return FD2_FIELD_ACTION_RIGHT;
        case FD2_INPUT_ACTION_UP: return FD2_FIELD_ACTION_UP;
        case FD2_INPUT_ACTION_DOWN: return FD2_FIELD_ACTION_DOWN;
        case FD2_INPUT_ACTION_CONFIRM: return FD2_FIELD_ACTION_CONFIRM;
        case FD2_INPUT_ACTION_CANCEL: return FD2_FIELD_ACTION_CANCEL;
        case FD2_INPUT_ACTION_FIELD_DETAIL: return FD2_FIELD_ACTION_DETAIL;
        case FD2_INPUT_ACTION_FIELD_AUXILIARY:
            return FD2_FIELD_ACTION_AUXILIARY;
        case FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE:
            return FD2_FIELD_ACTION_FOCUS_CYCLE;
        case FD2_INPUT_ACTION_EXIT: return FD2_FIELD_ACTION_EXIT;
        default: return FD2_FIELD_ACTION_NONE;
    }
}

static int detail_sprite_matches(const fd2_vga *vga,
                                 const fd2_image *image,
                                 int dst_x, int dst_y) {
    if (!vga || !image || !image->pixels) return 0;
    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            uint8_t pixel = image->pixels[(size_t)y * image->width + x];
            if (pixel != 0)
                return vga->framebuffer[(dst_y + y) * VGA_STRIDE +
                                        dst_x + x] == pixel;
        }
    }
    return 0;
}

static int validate_field_info(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !vga || !game->info_assets.ready ||
        game->info_assets.panel.width != 69 ||
        game->info_assets.panel.height != 34 || game->units.count == 0)
        return -1;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    int saved_panel_right = game->info_panel_right;
    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    fd2_field_game_render(game, vga);
    /* 左侧底板、unit sprite 与三位 HP 都应覆盖对应位置。 */
    int ok = vga->framebuffer[161 * VGA_STRIDE + 5] != 0 &&
             vga->framebuffer[182 * VGA_STRIDE + 14] != 0;
    const uint8_t *record = (const uint8_t *)&game->units.items[0];
    if (!ok || game->units.items[0].attack != 16 ||
        game->units.items[0].defense != 12 ||
        game->units.items[0].accuracy != 97 ||
        game->units.items[0].evasion != 2 ||
        record[0x0a] != 0x40 || record[0x0b] != 0 ||
        record[0x0c] != 0x40 || record[0x0d] != 132 ||
        record[0x0e] != 0 || record[0x0f] != 192 ||
        fd2_field_game_open_detail(game, 0) != 0)
        ok = 0;
    static const uint8_t other_equipment[][2] = {
        {52, 164}, {20, 128}, {72, 178},
    };
    for (size_t unit_index = 1; ok && unit_index < 4; unit_index++) {
        const uint8_t *other = (const uint8_t *)&game->units.items[unit_index];
        if (other[0x0a] != 0x40 ||
            other[0x0b] != other_equipment[unit_index - 1][0] ||
            other[0x0c] != 0x40 ||
            other[0x0d] != other_equipment[unit_index - 1][1])
            ok = 0;
        for (size_t slot = 2; slot < 8; slot++) {
            if (other[0x0a + slot * 2u] != 0x80u ||
                other[0x0b + slot * 2u] != 0xffu)
                ok = 0;
        }
    }
    if (ok) {
        uint8_t *status_record = (uint8_t *)&game->units.items[0];
        uint8_t saved_status[6];
        memcpy(saved_status, status_record + 0x22, sizeof(saved_status));
        memset(status_record + 0x22, 1, sizeof(saved_status));
        fd2_field_game_render(game, vga);
        ok = game->detail_visible && game->detail_portrait.pixels &&
             vga->framebuffer[7 * VGA_STRIDE + 92] != 0 &&
             vga->framebuffer[94 * VGA_STRIDE + 5] != 0 &&
             detail_sprite_matches(vga,
                                   &game->info_assets.detail_border[1],
                                   5, 7) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.status_icons[0],
                                   194, 68) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.digits[0][0],
                                   267, 41) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.digits[2][0],
                                   117, 67) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.detail_icons[3],
                                   13, 101) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.detail_icons[5],
                                   108, 105);
        memcpy(status_record + 0x22, saved_status, sizeof(saved_status));
    }
    fd2_field_game_close_detail(game);
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->info_panel_right = saved_panel_right;
    return ok ? 0 : -1;
}

static int validate_move_ranges(const fd2_field_game *game) {
    if (!game || !game->ready || game->units.count < 4) return -1;
    printf("field move ranges:");
    for (size_t i = 0; i < 4; i++) {
        fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (fd2_field_game_compute_move_range(game, i, &range) != 0 ||
            !fd2_field_path_is_destination(&range,
                                            game->units.items[i].x,
                                            game->units.items[i].y)) {
            fd2_field_path_close(&range);
            return -1;
        }
        size_t destination_count = 0;
        size_t node_count = (size_t)range.width * (size_t)range.height;
        for (size_t node = 0; node < node_count; node++) {
            if (range.nodes[node].distance != FD2_FIELD_PATH_UNREACHABLE &&
                range.nodes[node].can_stop)
                destination_count++;
        }
        if (destination_count < 2) {
            fd2_field_path_close(&range);
            return -1;
        }
        printf(" unit%zu=%zu", i, destination_count);
        fd2_field_path_close(&range);
    }
    putchar('\n');
    return 0;
}

static int validate_ai_queries(const fd2_field_game *game) {
    if (!game || !game->ready || game->units.count != 12) return -1;
    fd2_field_units units_before = game->units;
    uint32_t turn_before = game->turn_number;
    uint8_t side_before = game->active_side;
    fd2_field_interaction interaction_before = game->interaction;
    size_t phase_cursor_before = game->phase_unit_cursor;
    int selected_before = game->selected_unit;
    int cursor_x_before = game->cursor_cell_x;
    int cursor_y_before = game->cursor_cell_y;
    for (size_t i = 4; i < 12; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (unit->side != 0 || fd2_field_ai_behavior(unit) != 0 ||
            unit->ai_param_35 != 0 || unit->ai_param_36 != 0)
            return -1;

        fd2_field_ai_target target;
        fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
        fd2_field_ai_destination destination;
        fd2_field_ai_physical_candidate physical = {0};
        int ok = fd2_field_ai_nearest_opponent(
                     &game->units, i, 0, &target) == 0 &&
                 fd2_field_game_compute_move_range(game, i, &range) == 0 &&
                 fd2_field_ai_choose_destination(
                     &range, target.x, target.y, &destination) == 0 &&
                 fd2_field_path_is_destination(
                     &range, destination.x, destination.y) &&
                 destination.manhattan_distance <=
                     target.manhattan_distance;
        int physical_result = ok ? fd2_field_ai_choose_physical_candidate(
            &game->map, &game->terrain, &game->units, i, 0,
            &range, &physical) : -1;
        if (physical_result == 0) {
            ok = physical.unit_index < game->units.count &&
                 game->units.items[physical.unit_index].side != 0 &&
                 fd2_field_path_is_destination(
                     &range, physical.destination_x,
                     physical.destination_y) &&
                 (physical.priority == 0 || physical.priority == 8 ||
                  physical.priority == 0x12) &&
                 fd2_field_ai_select_attack_action(
                     physical.priority, 0, 0, 0) ==
                     (physical.priority >= 6
                         ? FD2_FIELD_AI_ACTION_PHYSICAL
                         : FD2_FIELD_AI_ACTION_NONE);
        }
        fd2_field_path_close(&range);
        if (!ok) return -1;
    }
    return memcmp(&game->units, &units_before, sizeof(units_before)) == 0 &&
           game->turn_number == turn_before && game->active_side == side_before &&
           game->interaction == interaction_before &&
           game->phase_unit_cursor == phase_cursor_before &&
           game->selected_unit == selected_before &&
           game->cursor_cell_x == cursor_x_before &&
           game->cursor_cell_y == cursor_y_before
        ? 0 : -1;
}

static int validate_move_interaction(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga || game->units.count == 0)
        return -1;

    fd2_field_unit saved_unit = game->units.items[0];
    uint8_t saved_walk_frame = game->units.walk_frames[0];
    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    int result = -1;

    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    if (fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        !game->move_preview_valid)
        goto done;

    int target_x = -1;
    int target_y = -1;
    uint32_t farthest = 0;
    for (int y = 0; y < game->move_range.height; y++) {
        for (int x = 0; x < game->move_range.width; x++) {
            uint32_t distance = fd2_field_path_distance(&game->move_range,
                                                        x, y);
            if (!fd2_field_path_is_destination(&game->move_range, x, y) ||
                (x == game->move_origin_x && y == game->move_origin_y) ||
                distance < farthest)
                continue;
            farthest = distance;
            target_x = x;
            target_y = y;
        }
    }
    if (target_x < 0) goto done;

    game->cursor_cell_x = target_x;
    game->cursor_cell_y = target_y;
    if (fd2_field_game_update_move_preview(game) != 0 ||
        !game->move_preview_valid || game->move_path_length == 0)
        goto done;

    int expected_x = game->move_origin_x;
    int expected_y = game->move_origin_y;
    int expected_camera_x = game->move_origin_camera_x;
    int expected_camera_y = game->move_origin_camera_y;
    int max_camera_x = game->map.width - FD2_FIELD_VIEW_CELLS_X;
    int max_camera_y = game->map.height - FD2_FIELD_VIEW_CELLS_Y;
    if (max_camera_x < 0) max_camera_x = 0;
    if (max_camera_y < 0) max_camera_y = 0;
    for (size_t i = 0; i < game->move_path_length; i++) {
        switch (game->move_path[i]) {
            case 0:
                if (expected_y - expected_camera_y >= 6 &&
                    expected_camera_y < max_camera_y)
                    expected_camera_y++;
                expected_y++;
                break;
            case 1:
                if (expected_x - expected_camera_x < 2 &&
                    expected_camera_x > 0)
                    expected_camera_x--;
                expected_x--;
                break;
            case 2:
                if (expected_y - expected_camera_y < 2 &&
                    expected_camera_y > 0)
                    expected_camera_y--;
                expected_y--;
                break;
            case 3:
                if (expected_x - expected_camera_x >= 11 &&
                    expected_camera_x < max_camera_x)
                    expected_camera_x++;
                expected_x++;
                break;
            default:
                goto done;
        }
    }

    if (expected_x != target_x || expected_y != target_y ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_MOVING ||
        fd2_field_game_execute_move(game, vga, 0) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND ||
        !game->command_assets.ready ||
        game->command_selected < 0 ||
        !game->command_disabled[FD2_FIELD_COMMAND_MAGIC] ||
        !game->command_disabled[FD2_FIELD_COMMAND_ITEM] ||
        game->command_disabled[FD2_FIELD_COMMAND_WAIT] ||
        game->units.items[0].x != target_x ||
        game->units.items[0].y != target_y ||
        game->camera_cell_x != expected_camera_x ||
        game->camera_cell_y != expected_camera_y ||
        game->units.items[0].frame_phase != 0 ||
        game->camera_pixel_offset_x != 0 ||
        game->camera_pixel_offset_y != 0)
        goto done;

    if (!fd2_field_game_cancel_selection(game) ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        game->units.items[0].x != game->move_origin_x ||
        game->units.items[0].y != game->move_origin_y ||
        game->units.items[0].direction != game->move_origin_direction ||
        game->camera_cell_x != game->move_origin_camera_x ||
        game->camera_cell_y != game->move_origin_camera_y ||
        !game->move_preview_valid)
        goto done;

    game->cursor_cell_x = target_x;
    game->cursor_cell_y = target_y;
    if (fd2_field_game_update_move_preview(game) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        fd2_field_game_execute_move(game, vga, 0) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->selected_unit != -1 || game->move_range.nodes != NULL)
        goto done;

    /* 独立覆盖四个方向的单格执行和镜头阈值，不依赖上面所选路径恰好
     * 包含哪些方向。 */
    static const struct {
        uint8_t direction;
        uint8_t start_x, start_y;
        uint8_t camera_x, camera_y;
        uint8_t end_x, end_y;
        uint8_t end_camera_x, end_camera_y;
    } cases[] = {
        {0, 5, 6, 0, 0, 5, 7, 0, 1},
        {1, 2, 5, 1, 0, 1, 5, 0, 0},
        {2, 5, 2, 0, 1, 5, 1, 0, 0},
        {3, 11, 5, 0, 0, 12, 5, 1, 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        game->units.items[0].x = cases[i].start_x;
        game->units.items[0].y = cases[i].start_y;
        game->units.items[0].frame_phase = 0;
        game->camera_cell_x = cases[i].camera_x;
        game->camera_cell_y = cases[i].camera_y;
        game->selected_unit = 0;
        game->interaction = FD2_FIELD_INTERACTION_MOVING;
        game->move_preview_valid = 1;
        game->move_path_length = 1;
        game->move_path[0] = cases[i].direction;
        if (fd2_field_game_execute_move(game, vga, 0) != 0 ||
            game->interaction != FD2_FIELD_INTERACTION_COMMAND ||
            game->units.items[0].x != cases[i].end_x ||
            game->units.items[0].y != cases[i].end_y ||
            game->units.items[0].direction != cases[i].direction ||
            game->units.items[0].frame_phase != 0 ||
            game->camera_cell_x != cases[i].end_camera_x ||
            game->camera_cell_y != cases[i].end_camera_y ||
            game->camera_pixel_offset_x != 0 ||
            game->camera_pixel_offset_y != 0)
            goto done;
    }
    result = 0;

done:
    fd2_field_path_close(&game->move_range);
    game->units.items[0] = saved_unit;
    game->units.walk_frames[0] = saved_walk_frame;
    game->camera_cell_x = saved_camera_x;
    game->camera_cell_y = saved_camera_y;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->selected_unit = -1;
    game->move_path_length = 0;
    game->move_preview_valid = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return result;
}

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

static int wait_all_available_players(fd2_field_game *game,
                                      size_t *waited_count,
                                      int require_nonzero_move) {
    if (!game || !waited_count || game->active_side != 2u) return -1;
    *waited_count = 0;
    int moved_nonzero = 0;
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        if (unit->side != 2u || fd2_field_unit_is_hidden(unit) ||
            unit->hp == 0 || fd2_field_unit_has_acted(unit))
            continue;
        (*waited_count)++;
        int origin_x = unit->x;
        int origin_y = unit->y;
        game->cursor_cell_x = origin_x;
        game->cursor_cell_y = origin_y;
        if (fd2_field_game_confirm_cursor(game) != (int)i ||
            game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED)
            return -1;
        if (require_nonzero_move && !moved_nonzero) {
            static const int dx[4] = {0, -1, 0, 1};
            static const int dy[4] = {1, 0, -1, 0};
            int found_destination = 0;
            for (size_t direction = 0; direction < 4; direction++) {
                int x = origin_x + dx[direction];
                int y = origin_y + dy[direction];
                if (!fd2_field_path_is_destination(&game->move_range, x, y))
                    continue;
                if (move_focus_to(game, x, y) != 0) return -1;
                found_destination = 1;
                break;
            }
            if (!found_destination) return -1;
        }
        if (fd2_field_game_confirm_cursor(game) != (int)i ||
            game->interaction != FD2_FIELD_INTERACTION_MOVING ||
            fd2_field_game_execute_move(game, NULL, 0) != 0 ||
            game->interaction != FD2_FIELD_INTERACTION_COMMAND)
            return -1;
        if (unit->x != origin_x || unit->y != origin_y)
            moved_nonzero = 1;
        (void)fd2_field_game_select_command_direction(
            game, FD2_FIELD_COMMAND_DIRECTION_DOWN);
        if (game->command_selected != FD2_FIELD_COMMAND_WAIT ||
            fd2_field_game_confirm_cursor(game) != (int)i ||
            !fd2_field_unit_has_acted(unit) || game->selected_unit != -1 ||
            game->interaction != FD2_FIELD_INTERACTION_BROWSE)
            return -1;

        /* 同一回合重新确认已行动单位必须失败，不能再次进入移动状态。 */
        game->cursor_cell_x = unit->x;
        game->cursor_cell_y = unit->y;
        if (fd2_field_game_confirm_cursor(game) != -1 ||
            game->selected_unit != -1 ||
            game->interaction != FD2_FIELD_INTERACTION_BROWSE)
            return -1;
    }
    return *waited_count > 0 && game->active_side == 1u &&
           fd2_field_game_remaining_units(game, 2u) == 0 &&
           (!require_nonzero_move || moved_nonzero) ? 0 : -1;
}

/* 正式 stage 0 玩家回合回归。逐名使用与交互循环相同的
 * BROWSE → UNIT_SELECTED → MOVING → COMMAND → wait 路径；不得用
 * fd2_field_game_end_active_phase() 代替玩家操作。 */
static int validate_player_round_flow(const fd2_field_game *game) {
    if (!game || !game->ready || game->active_side != 2u) return -1;
    fd2_field_game replay;
    init_validation_replay(&replay, game);
    uint32_t initial_turn = replay.turn_number;
    size_t player_count = 0;
    int result = -1;

    if (wait_all_available_players(&replay, &player_count, 1) != 0)
        goto done;
    /* 使用正式自动 phase，不直接跳过 side 1/0。 */
    for (size_t guard = 0; replay.active_side != 2u && guard < 256u; guard++) {
        if (fd2_field_game_process_automatic_action(&replay) < 0)
            goto done;
    }
    if (replay.active_side != 2u || replay.turn_number != initial_turn + 1u ||
        fd2_field_game_remaining_units(&replay, 2u) != player_count)
        goto done;
    result = 0;

done:
    close_validation_replay(&replay);
    return result;
}

/* M6 正式资源垂直切片：不改写 actor 坐标、HP 或战斗属性。先从真实
 * 移动范围查询本回合可达的物理候选，再走玩家移动、菜单、目标选择、
 * 伤害提交和 acted 门。 */
static int validate_attack_flow(const fd2_field_game *game,
                                int require_reachable_attack) {
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
        if (!require_reachable_attack) result = 0;
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
    /* 攻击完成后的 acted 单位同样不能重新进入移动状态。 */
    replay.cursor_cell_x = attacker->x;
    replay.cursor_cell_y = attacker->y;
    if (fd2_field_game_confirm_cursor(&replay) != -1 ||
        replay.selected_unit != -1 ||
        replay.interaction != FD2_FIELD_INTERACTION_BROWSE)
        goto done;
    result = 0;

done:
    if (result != 0)
        fprintf(stderr, "field attack validation stopped at: %s\n",
                failed_stage);
    close_validation_replay(&replay);
    return result;
}

static int validate_deferred_presentations(
        const fd2_field_game *game,
        fd2_vga *vga,
        const fd2_archive *fdother,
        fd2_field_audio *audio,
        size_t *validated_count) {
    if (!game || !vga || !fdother || !validated_count) return -1;
    while (*validated_count < game->event_log_count) {
        size_t index = (*validated_count)++;
        const fd2_field_event_notice *notice = &game->event_log[index];
        if (!notice->handled || !notice->presentation_deferred) continue;
        fd2_field_game replay = *game;
        fd2_field_event_notice replay_notice = *notice;
        if (fd2_scene_play_field_event(vga, fdother, &replay,
                                       &replay_notice, audio, 1) != 0 ||
            replay_notice.presentation_deferred)
            return -1;
    }
    return 0;
}

static int validate_turn_flow(fd2_field_game *game, fd2_vga *vga,
                              const fd2_archive *fdother,
                              fd2_field_audio *audio) {
    if (!game || !game->ready || !vga || !fdother || game->turn_number != 1 ||
        game->active_side != 2 || game->selected_unit != -1 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->move_range.nodes != NULL ||
        game->camera_pixel_offset_x != 0 ||
        game->camera_pixel_offset_y != 0)
        return -1;

    fd2_field_units saved_units = game->units;
    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    uint32_t saved_turn = game->turn_number;
    uint8_t saved_side = game->active_side;
    size_t saved_phase_cursor = game->phase_unit_cursor;
    uint64_t saved_phase_time = game->phase_next_action_ms;
    fd2_field_event_notice saved_log[FD2_FIELD_EVENT_LOG_CAPACITY];
    memcpy(saved_log, game->event_log, sizeof(saved_log));
    size_t saved_log_count = game->event_log_count;
    uint32_t saved_total = game->event_total_count;
    uint32_t saved_unhandled = game->unhandled_event_count;
    uint32_t saved_dropped = game->dropped_event_count;
    int result = -1;

    /* 原地确认等价于最小待机：仍经过移动确认状态，但路径长度为 0。 */
    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    if (fd2_field_game_confirm_cursor(game) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_MOVING ||
        fd2_field_game_execute_move(game, vga, 0) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        !fd2_field_unit_has_acted(&game->units.items[0]) ||
        fd2_field_game_remaining_units(game, 2) != 3)
        goto done;

    game->units = saved_units;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->event_log_count = 0;
    game->event_total_count = 0;
    game->unhandled_event_count = 0;
    game->dropped_event_count = 0;

    size_t guard = 0;
    size_t validated_presentations = 0;
    while (game->turn_number <= 6) {
        if (game->active_side != 2 ||
            fd2_field_game_end_active_phase(game) != 0 ||
            validate_deferred_presentations(
                game, vga, fdother, audio, &validated_presentations) != 0)
            goto done;
        while (game->active_side != 2) {
            /* 该回归验证 stage event／phase 调度，不依赖 AI 战斗结果；
             * 明确结束自动 phase，避免候选 RNG 改写脚本夹具。 */
            if (fd2_field_game_end_active_phase(game) != 0 ||
                ++guard > 128 ||
                validate_deferred_presentations(
                    game, vga, fdother, audio, &validated_presentations) != 0)
                goto done;
        }
    }

    if (game->turn_number != 7 || game->active_side != 2 ||
        fd2_field_game_remaining_units(game, 2) != 6 ||
        game->units.count != 27 ||
        fd2_field_game_group_count(game, 3) != 1 ||
        fd2_field_game_group_count(game, 7) != 1 ||
        fd2_field_game_group_count(game, 4) != 4 ||
        fd2_field_game_group_count(game, 5) != 5 ||
        fd2_field_game_group_count(game, 6) != 4 ||
        game->units.items[12].x != 11 || game->units.items[12].y != 13 ||
        game->units.items[12].direction != 1 ||
        game->units.items[13].x != 11 || game->units.items[13].y != 12 ||
        game->units.items[13].direction != 1 ||
        game->units.items[14].x != 15 || game->units.items[14].y != 21 ||
        game->units.items[18].x != 3 || game->units.items[18].y != 21 ||
        game->units.items[23].x != 18 || game->units.items[23].y != 15 ||
        game->units.items[26].x != 19 || game->units.items[26].y != 14 ||
        game->camera_cell_x != 11 || game->camera_cell_y != 11 ||
        game->event_log_count != 4 || game->event_total_count != 4 ||
        game->unhandled_event_count != 0 || game->dropped_event_count != 0 ||
        validated_presentations != 4)
        goto done;
    static const uint8_t expected_turn[] = {3, 4, 5, 6};
    static const uint8_t expected_side[] = {1, 0, 0, 1};
    static const uint8_t expected_action[] = {0, 1, 2, 3};
    static const uint8_t expected_pre_units[] = {12, 14, 18, 23};
    for (size_t i = 0; i < 4; i++) {
        const fd2_field_event_notice *notice = &game->event_log[i];
        if (notice->kind != FD2_FIELD_EVENT_TURN ||
            notice->turn_number != expected_turn[i] ||
            notice->phase != expected_side[i] ||
            notice->action != expected_action[i] || !notice->handled ||
            !notice->presentation_deferred ||
            notice->presentation_unit_count != expected_pre_units[i])
            goto done;
    }
    result = 0;

done:
    fd2_field_path_close(&game->move_range);
    game->units = saved_units;
    game->camera_cell_x = saved_camera_x;
    game->camera_cell_y = saved_camera_y;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->turn_number = saved_turn;
    game->active_side = saved_side;
    game->phase_unit_cursor = saved_phase_cursor;
    game->phase_next_action_ms = saved_phase_time;
    memcpy(game->event_log, saved_log, sizeof(saved_log));
    game->event_log_count = saved_log_count;
    game->event_total_count = saved_total;
    game->unhandled_event_count = saved_unhandled;
    game->dropped_event_count = saved_dropped;
    game->selected_unit = -1;
    game->move_path_length = 0;
    game->move_preview_valid = 0;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return result;
}

static int present_field_events(fd2_field_game *game,
                                fd2_vga *vga,
                                const fd2_archive *fdother,
                                fd2_field_audio *audio) {
    if (!game || !vga || !fdother || game->failure != FD2_FIELD_FAILURE_NONE)
        return -1;
    for (size_t i = 0; i < game->event_log_count; i++) {
        fd2_field_event_notice *notice = &game->event_log[i];
        if (!notice->handled) return -1;
        if (!notice->presentation_deferred) continue;
        int result = fd2_scene_play_field_event(
            vga, fdother, game, notice, audio, 0);
        if (result == FD2_SCENE_RESULT_HOST_QUIT)
            return FD2_SCENE_RESULT_HOST_QUIT;
        if (result != 0)
            return -1;
        notice->presentation_deferred = 0;
    }
    return 0;
}

static void report_field_events(const fd2_field_game *game,
                                size_t *reported_count,
                                uint32_t *reported_dropped) {
    if (!game || !reported_count || !reported_dropped) return;
    while (*reported_count < game->event_log_count) {
        const fd2_field_event_notice *notice =
            &game->event_log[*reported_count];
        const char *status = notice->handled
            ? (notice->presentation_deferred
                   ? "handled, presentation deferred"
                   : "handled")
            : "unhandled, stopped";
        if (notice->kind == FD2_FIELD_EVENT_TURN) {
            printf("field event: turn=%u phase=%u slot=%zu action=%u %s\n",
                   notice->turn_number, notice->phase, notice->slot,
                   notice->action, status);
        } else {
            printf("field event: cell=(%u,%u) unit=%u slot=%zu code=%u %s\n",
                   notice->x, notice->y, notice->unit_index,
                   notice->slot, notice->action, status);
        }
        (*reported_count)++;
    }
    if (*reported_dropped != game->dropped_event_count) {
        uint32_t delta = game->dropped_event_count - *reported_dropped;
        printf("field event: %u diagnostic record(s) dropped from bounded log\n", delta);
        *reported_dropped = game->dropped_event_count;
    }
    if (game->event_log_count != 0 || game->dropped_event_count != 0)
        fflush(stdout);
}

static void report_session_failure(const fd2_field_game *game) {
    if (game->failure == FD2_FIELD_FAILURE_NONE) return;
    const fd2_field_event_notice *notice = &game->failure_notice;
    fprintf(stderr,
            "field session stopped: reason=%d stage=%zu turn=%u phase=%u "
            "actor=%u slot=%zu action=%u cell=(%u,%u); committed state retained\n",
            (int)game->failure, game->stage, notice->turn_number,
            notice->phase, notice->unit_index, notice->slot,
            notice->action, notice->x, notice->y);
}

static int validate_field_effects(fd2_field_game *game, fd2_vga *vga,
                                  fd2_field_audio *field_audio) {
    if (!game || !game->ready || !vga || game->units.count == 0) return -1;
    int result = -1;
    fd2_field_effect_frames frames = {0};
    uint8_t units[] = {0};
    uint8_t invalid_unit[] = {(uint8_t)game->units.count};

    if (fd2_field_game_prepare_actor_group_flash(
            game, vga, invalid_unit, 1, 0xc0, &frames) == 0 ||
        frames.storage != NULL ||
        fd2_field_game_prepare_stage_transition(
            game, vga, 0, 0, 0, 8, &frames) == 0 ||
        frames.storage != NULL)
        goto done;
    if (fd2_field_game_prepare_stage_transition(
            game, vga, INT_MIN, INT_MAX, 10, 8, &frames) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);

    if (fd2_field_game_prepare_actor_group_flash(
            game, vga, units, 1, 0xc0, &frames) != 0 ||
        !frames.snapshots[0] || !frames.snapshots[1] ||
        memcmp(frames.snapshots[0], frames.snapshots[1], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&frames, vga, NULL, 0) != 0 ||
        memcmp(vga->framebuffer, frames.snapshots[0], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);
    if (fd2_field_game_play_actor_group_flash(
            game, vga, field_audio, units, 1, 0xc0, 0) != 0)
        goto done;

    if (fd2_field_game_prepare_earthquake(game, vga, &frames) != 0 ||
        memcmp(frames.snapshots[0], frames.snapshots[1], VGA_W * VGA_H) == 0 ||
        memcmp(frames.snapshots[1], frames.snapshots[2], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&frames, vga, NULL, 0) != 0 ||
        /* 第 59 帧是 cycle[3]，应落在 source 1。 */
        memcmp(vga->framebuffer, frames.snapshots[1], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);
    if (fd2_field_game_play_earthquake(
            game, vga, field_audio, 0) != 0)
        goto done;

    int center_x = (game->cursor_cell_x - game->camera_cell_x) * 24 + 12;
    int center_y = (game->cursor_cell_y - game->camera_cell_y) * 24 + 16;
    if (fd2_field_game_prepare_stage_transition(
            game, vga, center_x, center_y, 10, 8, &frames) != 0 ||
        memcmp(frames.snapshots[0], frames.snapshots[8], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&frames, vga, NULL, 0) != 0 ||
        memcmp(vga->framebuffer, frames.snapshots[8], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);
    if (fd2_field_game_play_stage_transition(
            game, vga, field_audio, center_x, center_y, 10, 8, 0) != 0)
        goto done;
    result = 0;

done:
    fd2_field_effect_frames_close(&frames);
    fd2_field_game_render(game, vga);
    return result;
}

static int validate_cursor_controls(fd2_field_game *game) {
    if (!game || !game->ready || game->units.count == 0) return -1;

    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    int saved_selected = game->selected_unit;
    fd2_field_interaction saved_interaction = game->interaction;
    uint8_t saved_flags = game->units.items[0].flags;
    uint16_t saved_hp = game->units.items[0].hp;
    int result = -1;

    game->camera_cell_x = 5;
    game->camera_cell_y = 5;
    game->cursor_cell_x = 7;
    game->cursor_cell_y = 7;
    if (!fd2_field_game_move_cursor(game, -1, 0) ||
        game->cursor_cell_x != 6 || game->camera_cell_x != 4)
        goto done;

    game->camera_cell_x = 5;
    game->cursor_cell_x = 16;
    if (!fd2_field_game_move_cursor(game, 1, 0) ||
        game->cursor_cell_x != 17 || game->camera_cell_x != 6)
        goto done;

    game->camera_cell_y = 5;
    game->cursor_cell_y = 7;
    if (!fd2_field_game_move_cursor(game, 0, -1) ||
        game->cursor_cell_y != 6 || game->camera_cell_y != 4)
        goto done;

    game->camera_cell_y = 5;
    game->cursor_cell_y = 11;
    if (!fd2_field_game_move_cursor(game, 0, 1) ||
        game->cursor_cell_y != 12 || game->camera_cell_y != 6)
        goto done;

    game->camera_cell_x = 0;
    game->camera_cell_y = 0;
    game->cursor_cell_x = 0;
    game->cursor_cell_y = 0;
    if (fd2_field_game_move_cursor(game, -1, 0) ||
        fd2_field_game_move_cursor(game, 0, -1) ||
        game->cursor_cell_x != 0 || game->cursor_cell_y != 0)
        goto done;

    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    if (fd2_field_game_unit_at(game, game->cursor_cell_x,
                               game->cursor_cell_y) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        !fd2_field_game_cancel_selection(game))
        goto done;

    fd2_field_unit_set_hidden(&game->units.items[0], 1);
    if (fd2_field_game_unit_at(game, game->cursor_cell_x,
                               game->cursor_cell_y) != -1)
        goto done;
    game->units.items[0].flags = saved_flags;
    game->units.items[0].hp = 0;
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    if (fd2_field_game_confirm_cursor(game) != -1)
        goto done;
    game->units.items[0].hp = saved_hp;

    game->cursor_cell_x = game->units.items[4].x;
    game->cursor_cell_y = game->units.items[4].y;
    game->selected_unit = -1;
    if (fd2_field_game_confirm_cursor(game) != -1)
        goto done;
    result = 0;

done:
    game->camera_cell_x = saved_camera_x;
    game->camera_cell_y = saved_camera_y;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->selected_unit = saved_selected;
    game->interaction = saved_interaction;
    game->units.items[0].flags = saved_flags;
    game->units.items[0].hp = saved_hp;
    return result;
}

fd2_field_play_result fd2_field_play_run(fd2_vga *vga,
                       const fd2_archive *fdother,
                       size_t stage,
                       int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio,
                       const char *save_path,
                       int load_active) {
    fd2_field_game game;
    if (!vga || !fdother) return FD2_FIELD_PLAY_RETURN_ERROR;
    if (fd2_field_game_open(&game, vga, fdother, stage) != 0) {
        fprintf(stderr, "cannot load stage %zu field game\n", stage);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    /* M6 已接入原版武器 record +0x0b/+0x0c 范围与 side==0 目标过滤。
     * loader 初值已捕获；原版每次进入战场控制循环前还会按计时器低
     * 8 位推进该流，活动快照未保存 RNG，因此读档不承诺续接同一序列。
     * profile 暴击基础值与武器 effect type 4 加值均使用已确认原版表。 */
    fd2_field_rng m6_rng;
    fd2_field_rng_seed(&m6_rng, field_rng_initial_seed());
    if (fd2_field_game_set_attack_hooks(
            &game, NULL, NULL, NULL, NULL,
            fd2_field_rng_next, &m6_rng) != 0) {
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    if (handoff && fd2_field_game_apply_handoff(&game, handoff) != 0) {
        fprintf(stderr, "cannot apply stage %zu opening handoff\n", stage);
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    if (load_active && fd2_field_game_load_active(&game, save_path) != 0) {
        fprintf(stderr, "cannot import active field save: %s\n",
                save_path ? save_path : "(null)");
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    fd2_field_game_set_save_resource_available(
        &game, fd2_field_game_save_resource_available(&game, save_path));
    apply_field_audio_options(&game, field_audio);
    if (once) {
#define VALIDATE_STEP(name, expression) do { \
        if ((expression) != 0) { \
            fprintf(stderr, "field validation failed: %s\n", (name)); \
            fd2_field_game_close(&game); \
            return FD2_FIELD_PLAY_RETURN_ERROR; \
        } \
    } while (0)
        VALIDATE_STEP("cursor", validate_cursor_controls(&game));
        VALIDATE_STEP("info", validate_field_info(&game, vga));
        VALIDATE_STEP("move-range", validate_move_ranges(&game));
        VALIDATE_STEP("ai-query", validate_ai_queries(&game));
        VALIDATE_STEP("move-interaction", validate_move_interaction(&game, vga));
        VALIDATE_STEP("player-round", validate_player_round_flow(&game));
        VALIDATE_STEP("attack", validate_attack_flow(
            &game, handoff == NULL && !load_active));
        VALIDATE_STEP("turn", validate_turn_flow(&game, vga, fdother,
                                                   field_audio));
        VALIDATE_STEP("effect", validate_field_effects(
            &game, vga, field_audio));
#undef VALIDATE_STEP
    }

    printf("field play: stage=%zu, map=%dx%d, units=%zu, camera=(%d,%d), "
           "cursor=(%d,%d), turn=%u, phase=%u, handoff=%s%s\n",
           game.stage, game.map.width, game.map.height, game.units.count,
           game.camera_cell_x, game.camera_cell_y,
           game.cursor_cell_x, game.cursor_cell_y,
           game.turn_number, game.active_side,
           handoff ? "yes" : "no",
           once ? ", cursor-check=ok, info-check=ok, detail-check=ok, move-check=ok, "
                  "ai-query-check=ok, move-exec-check=ok, player-round-check=ok, command-check=ok, attack-check=ok, turn-check=ok, event-check=ok, effect-check=ok, fast"
                : "");
    fflush(stdout);

    size_t reported_event_count = 0;
    uint32_t reported_dropped_events = 0;
    uint32_t reported_turn = game.turn_number;
    uint8_t reported_side = game.active_side;
    int input_ready_reported = 0;
    /* 普通Enter详情返回controller后置；F2是原版独立直接返回分支。 */
    int detail_controller_actor = -1;
    /* march原版同步turncycle返回后清BIOS；SDL仅记输入尾，不保存pending。 */
    int march_waiting_phase = 0;
    int running = 1;
    fd2_field_play_result play_result = FD2_FIELD_PLAY_RETURN_COMPLETE;
    while (running) {
        if (game.failure != FD2_FIELD_FAILURE_NONE) {
            report_session_failure(&game);
            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
            break;
        }
        int was_waiting_phase = game.player_post_waiting_phase;
        if (fd2_field_game_resume_player_controller(&game) < 0) {
            report_session_failure(&game);
            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
            break;
        }
        if (was_waiting_phase && !game.player_post_waiting_phase)
            drain_frame_keys(vga); /* native controller尾119a6清BIOS脉冲。 */
        (void)fd2_field_water_tick(vga, SDL_GetTicks()); /* 普通战场等待。 */
        fd2_field_game_tick(&game, SDL_GetTicks());
        game.battle_result = fd2_field_game_battle_result(&game);
        if (march_waiting_phase && (game.active_side == 2u ||
                game.battle_result != FD2_FIELD_BATTLE_ONGOING)) {
            drain_frame_keys(vga);
            march_waiting_phase = 0;
        }
        if (!once && game.battle_result != FD2_FIELD_BATTLE_ONGOING) {
            play_result = game.battle_result == FD2_FIELD_BATTLE_VICTORY
                ? FD2_FIELD_PLAY_RETURN_VICTORY : FD2_FIELD_PLAY_RETURN_DEFEAT;
            printf("field battle: %s, stage=%zu\n",
                   game.battle_result == FD2_FIELD_BATTLE_VICTORY
                       ? "victory" : "defeat",
                   game.stage);
            fflush(stdout);
            running = 0;
        }
        if (game.turn_number != reported_turn ||
            game.active_side != reported_side) {
            printf("field phase: turn=%u phase=%u remaining=%zu\n",
                   game.turn_number, game.active_side,
                   fd2_field_game_remaining_units(&game, game.active_side));
            fflush(stdout);
            reported_turn = game.turn_number;
            reported_side = game.active_side;
        }
        if (!once) {
            int event_result = present_field_events(&game, vga, fdother,
                                                    field_audio);
            if (event_result == FD2_SCENE_RESULT_HOST_QUIT) {
                play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                running = 0;
            } else if (event_result != 0) {
                fprintf(stderr, "field event presentation failed\n");
                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                running = 0;
            }
        }
        report_field_events(&game, &reported_event_count,
                            &reported_dropped_events);
        if (march_waiting_phase)
            fd2_field_game_render_background(&game, vga);
        else
            fd2_field_game_render(&game, vga);
        fd2_vga_present(vga);
        /* 宿主关闭先于下一 actor 调度；已有错误仍保留错误优先级。 */
        if (play_result != FD2_FIELD_PLAY_RETURN_ERROR &&
            fd2_input_take_quit(&vga->input)) {
            play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
            break;
        }
        if (once || !running) break;
        if (game.active_side != 2 &&
            game.interaction == FD2_FIELD_INTERACTION_BROWSE &&
            game.phase_next_action_ms != 0 &&
            SDL_GetTicks() >= game.phase_next_action_ms) {
            was_waiting_phase = game.player_post_waiting_phase;
            int ai_result = fd2_field_game_process_automatic_action_visual(
                &game, vga, 55, march_waiting_phase);
            if (was_waiting_phase && !game.player_post_waiting_phase)
                drain_frame_keys(vga);
            if (ai_result >= 0 && march_waiting_phase &&
                (game.active_side == 2u ||
                 game.battle_result != FD2_FIELD_BATTLE_ONGOING)) {
                drain_frame_keys(vga);
                march_waiting_phase = 0;
            }
            if (ai_result >= 0 && game.active_side != 2)
                game.phase_next_action_ms =
                    SDL_GetTicks() + FD2_FIELD_AUTOMATIC_ACTION_MS;
            if (ai_result < 0) {
                report_session_failure(&game);
                fprintf(stderr, "field automatic action unsupported or failed\n");
                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                running = 0;
            }
            continue;
        }
        if (!input_ready_reported) {
            printf("field input ready: stage=%zu turn=%u phase=%u\n",
                   game.stage, game.turn_number, game.active_side);
            fflush(stdout);
            input_ready_reported = 1;
        }
        fd2_input_action input_action;
        int end_input_frame = 0;
        while (running && !end_input_frame &&
               fd2_input_take_action(
                   &vga->input,
                   game.interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED ||
                           game.interaction == FD2_FIELD_INTERACTION_MOVING
                       ? FD2_INPUT_CONTEXT_FIELD_MOVEMENT
                       : game.interaction == FD2_FIELD_INTERACTION_COMMAND
                       ? FD2_INPUT_CONTEXT_FIELD_COMMAND
                       : game.interaction == FD2_FIELD_INTERACTION_TARGETING
                           ? FD2_INPUT_CONTEXT_FIELD_TARGETING
                           : game.interaction ==
                                 FD2_FIELD_INTERACTION_SYSTEM_MENU
                               ? FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU
                               : game.interaction ==
                                     FD2_FIELD_INTERACTION_MANUAL_SLOT
                                   ? FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT
                                   : game.interaction ==
                                         FD2_FIELD_INTERACTION_AUXILIARY
                                       ? FD2_INPUT_CONTEXT_FIELD_AUXILIARY
                                       : FD2_INPUT_CONTEXT_FIELD,
                   &input_action)) {
            fd2_field_action action = field_action_from_input(input_action);
            if (game.detail_visible) {
                if (action == FD2_FIELD_ACTION_CONFIRM ||
                    action == FD2_FIELD_ACTION_CANCEL) {
                    if (fd2_field_game_animate_detail(
                            &game, vga, 0, detail_phase_sfx, field_audio) != 0) {
                        fprintf(stderr, "无法关闭单位详情页。\n");
                        play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                        running = 0;
                        break;
                    }
                    fd2_field_game_close_detail(&game);
                    if (detail_controller_actor >= 0) {
                        int actor = detail_controller_actor;
                        detail_controller_actor = -1;
                        if (fd2_field_game_complete_player_controller(
                                &game, (size_t)actor) != 0) {
                            report_session_failure(&game);
                            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                        }
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                    }
                }
                continue;
            }
            if (game.active_side != 2 && action != FD2_FIELD_ACTION_EXIT)
                continue;
            switch (action) {
                case FD2_FIELD_ACTION_EXIT:
                    play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                    running = 0;
                    break;
                case FD2_FIELD_ACTION_CANCEL:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT) {
                        (void)fd2_field_game_cancel_manual_slot_picker(&game);
                    } else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                        fd2_field_system_page page = game.system_menu.page;
                        /* Secondary/options cancel first closes the current
                         * radial page, then parent page opens again. Parent
                         * cancel closes the menu before returning to browse. */
                        if (game.option_values[1])
                            (void)fd2_field_audio_play(
                                field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_system_menu(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                        (void)fd2_field_game_cancel_system_menu(&game);
                        if (page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION &&
                            play_system_confirmation_text(
                                &game, vga, FD2_FIELD_SYSTEM_TEXT_CANCELLED, once) != 0) {
                            fprintf(stderr, "取消提示资源无效。\n");
                            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            break;
                        }
                        if (page != FD2_FIELD_SYSTEM_PAGE_PARENT &&
                            game.interaction ==
                                FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                            if (game.option_values[1])
                                (void)fd2_field_audio_play(
                                    field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                            (void)fd2_field_game_animate_system_menu(
                                &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                        }
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_AUXILIARY) {
                        game.interaction = FD2_FIELD_INTERACTION_BROWSE;
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_UNIT_SELECTED ||
                               game.interaction == FD2_FIELD_INTERACTION_MOVING) {
                        (void)fd2_field_game_cancel_selection(&game);
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_COMMAND) {
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                        (void)fd2_field_game_cancel_selection(&game);
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_TARGETING &&
                               fd2_field_game_cancel_attack(&game)) {
                        /* field_player_command_execute_core @0x18d8c：
                         * target selector 返回 -1 后重新进入菜单循环。 */
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    break;
                case FD2_FIELD_ACTION_DETAIL: {
                    int unit_at = fd2_field_game_unit_at(
                        &game, game.cursor_cell_x, game.cursor_cell_y);
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE &&
                        unit_at >= 0 &&
                        game.units.items[unit_at].unit_id != 0x79 &&
                        game.units.items[unit_at].race != 0x0a &&
                        game.units.items[unit_at].hp != 0) {
                        if (fd2_field_game_open_detail(&game, (size_t)unit_at) != 0 ||
                            fd2_field_game_animate_detail(
                                &game, vga, 1, detail_phase_sfx, field_audio) != 0) {
                            fprintf(stderr, "单位详情页资源无效。\n");
                            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                        }
                    }
                    break;
                }
                case FD2_FIELD_ACTION_AUXILIARY:
                    /* field_controller_input @0x117e7（原登记同名）的
                     * F1/PageUp直接调用sub_2000A，不走玩家动作后置。 */
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE) {
                        int auxiliary_result = play_field_auxiliary(&game, vga);
                        end_input_frame = 1;
                        if (auxiliary_result == FD2_SCENE_RESULT_HOST_QUIT) {
                            play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                            running = 0;
                        } else if (auxiliary_result != FD2_SCENE_RESULT_OK) {
                            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                        }
                    }
                    break;
                case FD2_FIELD_ACTION_FOCUS_CYCLE:
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE) {
                        int unit = fd2_field_game_cycle_focus(&game);
                        printf("field focus: unit=%d, cell=(%d,%d), interaction=browse\n",
                               unit, game.cursor_cell_x, game.cursor_cell_y);
                        fflush(stdout);
                    }
                    break;
                case FD2_FIELD_ACTION_CONFIRM: {
                    int unit_at = fd2_field_game_unit_at(
                        &game, game.cursor_cell_x, game.cursor_cell_y);
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE &&
                        unit_at >= 0) {
                        const fd2_field_unit *focused =
                            &game.units.items[unit_at];
                        const uint8_t *record = (const uint8_t *)focused;
                        /* code0 0x1884：只有可行动的 side 2 普通单位进入
                         * 移动／操作路径；敌方、已行动单位和特殊记录先走
                         * 原版的单位详情 helper。 */
                        if (focused->unit_id != 0x79 &&
                            focused->race != 0x0a &&
                            (focused->side != 2u ||
                             fd2_field_unit_has_acted(focused) ||
                             record[0x26] != 0)) {
                            if (focused->hp == 0) break;
                            if (focused->side == 2u &&
                                fd2_field_unit_has_acted(focused)) {
                                printf("field acted gate: unit=%d, interaction=browse\n",
                                       unit_at);
                                fflush(stdout);
                            }
                            if (fd2_field_game_open_detail(
                                    &game, (size_t)unit_at) != 0 ||
                                fd2_field_game_animate_detail(
                                    &game, vga, 1,
                                    detail_phase_sfx, field_audio) != 0) {
                                fprintf(stderr, "单位详情页资源无效。\n");
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                            } else {
                                detail_controller_actor = unit_at;
                            }
                            break;
                        }
                    }
                    game.detail_acknowledged_unit = -1;
                    fd2_field_interaction previous_interaction =
                        game.interaction;
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT) {
                        int slot = fd2_field_game_confirm_manual_slot_picker(
                            &game);
                        if (slot >= 0) {
                            int success;
                            size_t fragment = 0;
                            if (game.manual_slot_picker.mode ==
                                    FD2_FIELD_MANUAL_SLOT_MODE_SAVE) {
                                success = fd2_field_game_save_manual_slot(
                                    &game, save_path, (size_t)slot) == 0;
                                fragment = success
                                    ? FD2_FIELD_MANUAL_TEXT_SAVE_SUCCESS
                                    : FD2_FIELD_SYSTEM_TEXT_SAVE_FAILURE;
                                fd2_field_game_set_save_resource_available(
                                    &game,
                                    fd2_field_game_save_resource_available(
                                        &game, save_path));
                            } else {
                                success = fd2_field_game_load_manual_slot(
                                    &game, save_path, (size_t)slot) == 0;
                                fragment = success
                                    ? FD2_FIELD_MANUAL_TEXT_LOAD_SUCCESS
                                    : FD2_FIELD_SYSTEM_TEXT_LOAD_FAILURE;
                                if (success)
                                    apply_field_audio_options(&game,
                                                              field_audio);
                            }
                            game.manual_slot_result_fragment =
                                (uint16_t)fragment;
                            game.manual_slot_result_until_ms =
                                fragment != 0 ? SDL_GetTicks() + 1000u : 0;
                            if (fragment != 0)
                                (void)play_system_confirmation_text(
                                    &game, vga, fragment, once);
                            if (!success)
                                fprintf(stderr,
                                        "field manual slot %s failed: %d\n",
                                        game.manual_slot_picker.mode ==
                                                FD2_FIELD_MANUAL_SLOT_MODE_SAVE
                                            ? "save" : "load", slot);
                        }
                        break;
                    }
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                        /* open/input/close primitive 的原版边界：确认后先按
                         * 当前 command IDs 收起，再进入下一 dispatcher。
                         * confirmation page 独立绘制 FDTXT 与 YES/NO。 */
                        if (game.option_values[1])
                            (void)fd2_field_audio_play(
                                field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_system_menu(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                        fd2_field_system_action system_action =
                            fd2_field_game_confirm_system_menu(&game);
                        if (system_action ==
                                FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING) {
                            apply_field_audio_options(&game, field_audio);
                            unsigned index = game.system_menu.selected;
                            printf("field option: index=%u value=%u\n", index,
                                   game.option_values[index]);
                            fflush(stdout);
                        }
                        if (system_action ==
                                FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION) {
                            size_t fragment = 0;
                            if (fd2_field_system_menu_get_confirmation_fragment(
                                    &game.system_menu,
                                    FD2_FIELD_SYSTEM_CONFIRM_PROMPT,
                                    &fragment) != 0 ||
                                play_system_confirmation_text(
                                    &game, vga, fragment, once) != 0) {
                                fprintf(stderr, "系统确认提示资源无效。\n");
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                                break;
                            }
                        } else if (system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_SCREEN_TRANSITION) {
                            int overview_result = play_field_overview(&game, vga);
                            end_input_frame = 1;
                            if (overview_result == FD2_SCENE_RESULT_HOST_QUIT) {
                                play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                                running = 0;
                            } else if (overview_result != FD2_SCENE_RESULT_OK) {
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                            }
                        } else if (system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_SAVE ||
                                   system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_LOAD) {
                            /* secondary dispatcher @code0 0x9df7 操作活动
                             * snapshot；四槽 hand_save/hand_load 属于独立
                             * dispatcher @code0 0x19300，不能在此替换。 */
                            size_t fragment = 0;
                            int success =
                                fd2_field_game_execute_storage_action(
                                    &game, system_action, save_path,
                                    &fragment) == 1;
                            if (success && system_action ==
                                               FD2_FIELD_SYSTEM_ACTION_LOAD) {
                                reported_event_count = 0;
                                reported_dropped_events = 0;
                                apply_field_audio_options(&game, field_audio);
                            }
                            if (fragment != 0) {
                                game.system_result_fragment =
                                    (uint16_t)fragment;
                                game.system_result_until_ms =
                                    SDL_GetTicks() + 1000u;
                                if (play_system_confirmation_text(
                                        &game, vga, fragment, once) != 0) {
                                    /* 存储动作已返回；呈现失败不能回滚或把已写
                                     * 的Save误报成未写。Load成功也已替换session。 */
                                    const char *status = system_action == FD2_FIELD_SYSTEM_ACTION_SAVE
                                        ? (success ? "活动存档已写入" : "活动存档返回失败")
                                        : (success ? "活动读档已完成" : "活动读档返回失败");
                                    fprintf(stderr, "%s，但结果提示资源无效。\n", status);
                                    play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                    running = 0;
                                    break;
                                }
                            }
                            if (!success)
                                fprintf(stderr,
                                        "field %s failed: %s\n",
                                        system_action ==
                                                FD2_FIELD_SYSTEM_ACTION_SAVE
                                            ? "save" : "load",
                                        save_path ? save_path : "(null)");
                            if (game.interaction ==
                                    FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                                if (game.option_values[1])
                                    (void)fd2_field_audio_play(
                                        field_audio,
                                        FD2_FIELD_SFX_COMMAND_MENU);
                                (void)fd2_field_game_animate_system_menu(
                                    &game, vga, 1,
                                    FD2_FIELD_COMMAND_FRAME_MS);
                            }
                        } else if (system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE) {
                            /* field_secondary_menu_execute @0x19df7：
                             * FDTXT[416] 后返回 -1，main 最终退出游戏。 */
                            if (play_system_confirmation_text(
                                    &game, vga,
                                    FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED,
                                    once) != 0) {
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                                break;
                            }
                            if (fd2_input_take_quit(&vga->input)) {
                                play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                                running = 0;
                                break;
                            }
                            puts("field leave: exiting game");
                            fflush(stdout);
                            play_result = FD2_FIELD_PLAY_RETURN_LEAVE_GAME;
                            running = 0;
                        } else if (system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_MARCH_CONFIRM) {
                            /* @0x16f55：418提示在固定anchor和逐actor提交之前。 */
                            if (play_system_confirmation_text(
                                    &game, vga, FD2_FIELD_SYSTEM_TEXT_MARCH_ACCEPTED,
                                    1) != 0) {
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                                break;
                            }
                            /* @0x16f55 内push200→j___delay；宿主保留名义毫秒参数，非DOS时钟精确复刻。 */
                            if (!once) fd2_delay_ms(200);
                            if (fd2_input_take_quit(&vga->input)) {
                                play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                                running = 0;
                                break;
                            }
                            printf("field march: anchor=(%d,%d) turn=%u phase=%u\n",
                                   game.cursor_cell_x, game.cursor_cell_y,
                                   game.turn_number, (unsigned)game.active_side);
                            fflush(stdout);
                            int march_result = fd2_field_game_march(&game, vga, 55);
                            if (march_result == 0) march_waiting_phase = 1;
                            drain_frame_keys(vga);
                            end_input_frame = 1;
                            if (march_result == 1) {
                                play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                                running = 0;
                            } else if (march_result != 0) {
                                report_session_failure(&game);
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                            }
                        } else if (system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM) {
                            /* field_empty_focus_menu_execute @0x16f55：
                             * FDTXT[420] 提示先于 field_turn_cycle_run。 */
                            if (play_system_confirmation_text(
                                    &game, vga,
                                    FD2_FIELD_SYSTEM_TEXT_END_TURN_ACCEPTED,
                                    once) != 0) {
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                                break;
                            }
                            if (fd2_input_take_quit(&vga->input)) {
                                play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                                running = 0;
                                break;
                            }
                            printf("field end turn: turn=%u phase=%u remaining=%zu\n",
                                   game.turn_number, game.active_side,
                                   fd2_field_game_remaining_units(&game, 2));
                            fflush(stdout);
                            game.interaction = FD2_FIELD_INTERACTION_BROWSE;
                            if (fd2_field_game_end_player_turn(&game) != 0) {
                                report_session_failure(&game);
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                            }
                        } else if (system_action == FD2_FIELD_SYSTEM_ACTION_BACK &&
                                   game.interaction == FD2_FIELD_INTERACTION_BROWSE) {
                            if (play_system_confirmation_text(
                                    &game, vga, FD2_FIELD_SYSTEM_TEXT_CANCELLED, once) != 0) {
                                fprintf(stderr, "取消提示资源无效。\n");
                                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                running = 0;
                            }
                        } else {
                            if (game.option_values[1])
                                (void)fd2_field_audio_play(
                                    field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                            (void)fd2_field_game_animate_system_menu(
                                &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                        }
                        break;
                    }
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_AUXILIARY) {
                        game.interaction = FD2_FIELD_INTERACTION_BROWSE;
                        break;
                    }
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_COMMAND) {
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    int action_unit = game.selected_unit;
                    int action_command = game.command_selected;
                    uint16_t attack_hp_before =
                        previous_interaction ==
                                FD2_FIELD_INTERACTION_TARGETING &&
                            unit_at >= 0 &&
                            (size_t)unit_at < game.units.count
                        ? game.units.items[unit_at].hp : 0;
                    int unit_index;
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_BROWSE && unit_at < 0) {
                        unit_index = fd2_field_game_open_system_menu(&game);
                    } else {
                        unit_index = fd2_field_game_confirm_cursor(&game);
                    }
                    if (game.failure != FD2_FIELD_FAILURE_NONE) {
                        report_session_failure(&game);
                        play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                        running = 0;
                        break;
                    }
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU &&
                        previous_interaction == FD2_FIELD_INTERACTION_BROWSE) {
                        if (game.option_values[1])
                            (void)fd2_field_audio_play(
                                field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_system_menu(
                            &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    if (game.interaction == FD2_FIELD_INTERACTION_MOVING) {
                        if (fd2_field_game_execute_move(&game, vga, 55) != 0) {
                            report_session_failure(&game);
                            fprintf(stderr, "field move execution failed\n");
                            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            break;
                        }
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    if (action_unit >= 0 &&
                        (size_t)action_unit < game.units.count &&
                        (previous_interaction ==
                             FD2_FIELD_INTERACTION_COMMAND ||
                         previous_interaction ==
                             FD2_FIELD_INTERACTION_TARGETING) &&
                        fd2_field_unit_has_acted(
                            &game.units.items[action_unit]) &&
                        game.selected_unit == -1) {
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                        printf("field action: unit=%d command=%s acted=1 remaining=%zu phase=%u\n",
                               action_unit,
                               action_command == FD2_FIELD_COMMAND_ATTACK
                                   ? "attack" : "wait",
                               fd2_field_game_remaining_units(&game, 2u),
                               (unsigned)game.active_side);
                    }
                    if (game.last_attack_valid) {
                        printf("field attack: target=%d strikes=%u hit=%u critical=%u base=%u rolled=%u damage=%u total=%u hp=%u defeated=%u hidden=%u\n",
                               game.attack_target,
                               (unsigned)game.last_attack_strikes,
                               game.last_attack.hit,
                               game.last_attack.critical,
                               game.last_attack.base_damage,
                               game.last_attack.rolled_damage,
                               game.last_attack.damage,
                               (unsigned)(attack_hp_before -
                                          game.last_attack.hp_after),
                               game.last_attack.hp_after,
                               game.last_attack.defeated,
                               game.attack_target >= 0 &&
                                   (size_t)game.attack_target < game.units.count
                                   ? (unsigned)fd2_field_unit_is_hidden(
                                         &game.units.items[game.attack_target])
                                   : 0u);
                        game.last_attack_valid = 0;
                    }
                    uint16_t terrain_id = fd2_field_cell_terrain(
                        fd2_field_map_cell(&game.map,
                                           game.cursor_cell_x,
                                           game.cursor_cell_y));
                    printf("field cursor: cell=(%d,%d), terrain=%u, "
                           "unit=%d, selected=%d, interaction=%d, "
                           "camera=(%d,%d), camera-offset=(%d,%d)\n",
                           game.cursor_cell_x, game.cursor_cell_y,
                           (unsigned)terrain_id, unit_at, unit_index,
                           (int)game.interaction,
                           game.camera_cell_x, game.camera_cell_y,
                           game.camera_pixel_offset_x,
                           game.camera_pixel_offset_y);
                    fflush(stdout);
                    break;
                }
                case FD2_FIELD_ACTION_LEFT:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, -1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_LEFT);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_LEFT);
                    else
                        fd2_field_game_move_cursor(&game, -1, 0);
                    break;
                case FD2_FIELD_ACTION_RIGHT:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, 1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT);
                    else
                        fd2_field_game_move_cursor(&game, 1, 0);
                    break;
                case FD2_FIELD_ACTION_UP:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, -1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_UP);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_UP);
                    else
                        fd2_field_game_move_cursor(&game, 0, -1);
                    break;
                case FD2_FIELD_ACTION_DOWN:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, 1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_DOWN);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_DOWN);
                    else
                        fd2_field_game_move_cursor(&game, 0, 1);
                    break;
                case FD2_FIELD_ACTION_NONE:
                    break;
            }
            if (game.failure != FD2_FIELD_FAILURE_NONE) {
                report_session_failure(&game);
                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                running = 0;
            }
            if (game.player_post_waiting_phase) end_input_frame = 1;
        }
        fd2_delay_ms(16);
    }

    fd2_field_game_close(&game);
    return play_result;
}

int fd2_field_effect_play_run(fd2_vga *vga,
                              const fd2_archive *fdother,
                              size_t stage,
                              fd2_field_audio *field_audio) {
    if (!vga || !fdother) return -1;
    fd2_field_game game;
    if (fd2_field_game_open(&game, vga, fdother, stage) != 0)
        return -1;
    int result = -1;
    uint8_t unit_indices[] = {0};
    int center_screen_x =
        (game.cursor_cell_x - game.camera_cell_x) * 24 + 12;
    int center_screen_y =
        (game.cursor_cell_y - game.camera_cell_y) * 24 + 16;

    fd2_field_game_render(&game, vga);
    fd2_vga_present(vga);
    if (fd2_field_game_play_actor_group_flash(
            &game, vga, field_audio, unit_indices, 1, 0xc0, 1) != 0)
        goto done;
    if (fd2_field_game_play_earthquake(
            &game, vga, field_audio, 1) != 0)
        goto done;
    if (fd2_field_game_play_stage_transition(
            &game, vga, field_audio, center_screen_x, center_screen_y,
            10, 8, 1) != 0)
        goto done;
    printf("field effects: stage=%zu, actor-flash=ok, earthquake=ok, "
           "stage-transition=ok, timed\n", stage);
    fflush(stdout);
    result = 0;

done:
    fd2_field_game_close(&game);
    return result;
}
