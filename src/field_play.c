/* 炎龙骑士团 2 SDL3 重写 - 正式战场循环入口
 *
 * 依据field_controller_input @0x117e7、field_player_command_execute @0x18d8c、
 * field_player_magic_command_execute @0x1cff0与
 * field_player_item_command_execute @0x1bbdc（原登记均同名），以及
 * field_unit_inventory_select @0x1b932（原sub_1B932）。
 * B4依据field_unit_experience_reward_execute @0x1e292（原sub_1E292）、
 * field_growth_stat_present_apply @0x1e529（原sub_1E529）及
 * field_phase_status_tick @0x1a866（原登记同名）。
 * B5依据field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D）、
 * field_special_cell_execute @0x190ac（原sub_190AC）、
 * field_stage0_victory_execute @0x22ef6（原sub_22EF6）及
 * persistent_roster_sync_from_battle @0x11506（原sub_11506）。
 * 此处编排输入、呈现和场景退出；玩法记录与提交仍由field_game持有。
 */

#include "field_play.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "field_ai.h"
#include "battle_animation.h"
#include "field_attack.h"
#include "field_game.h"
#include "field_rng.h"
#include "field_visual.h"
#include "font.h"
#include "input.h"
#include "portrait.h"
#include "text.h"
#include "scene.h"

/* 菜单机器码每相位执行一次 field buffer 合成，但没有 BIOS tick delay。
 * SDL 以一个约 60 Hz 宿主帧分隔四个 present，避免连续上传被合并；
 * 该值不登记为原版 DOS 时序。 */
#define FD2_FIELD_COMMAND_FRAME_MS 16
#define FD2_FIELD_AUTOMATIC_ACTION_MS 150

static int play_field_progress(fd2_field_game *game, fd2_vga *vga,
                                const fd2_archive *fdother,
                                fd2_field_audio *audio, int fast);

/* field_unit_selection_summary_wait @0x18b84（原sub_18B84）：18890初选后
 * 非消费式等待下一键，field_unit_summary_draw @0x18c6d（原sub_18C6D）
 * 只在这段等待显示。原caller已消费最初确认；返回时保留新键供mode4
 * 调度，包含无映射普通键，不建立第二份interaction或跨帧输入队列。 */
static int play_field_move_selection_wait_key(fd2_field_game *game,
                                               fd2_vga *vga) {
    if (!game || !vga || !game->ready ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count)
        return FD2_SCENE_RESULT_ERROR;
    for (;;) {
        if (game->failure != FD2_FIELD_FAILURE_NONE)
            return FD2_SCENE_RESULT_ERROR;
        if (fd2_input_take_quit(&vga->input))
            return FD2_SCENE_RESULT_HOST_QUIT;
        if (fd2_input_has_any_key(&vga->input))
            return FD2_SCENE_RESULT_OK;
        (void)fd2_field_water_tick(vga, SDL_GetTicks());
        fd2_field_game_tick(game, SDL_GetTicks());
        fd2_field_game_render(game, vga);
        const fd2_field_unit *unit = &game->units.items[game->selected_unit];
        if (fd2_field_unit_summary_draw(vga, &game->info_assets, &game->font,
                &game->ui_text, unit, (int)unit->x - game->camera_cell_x) != 0)
            return FD2_SCENE_RESULT_ERROR;
        fd2_vga_present(vga);
        fd2_delay_ms(16);
    }
}

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

void fd2_field_play_context_init(fd2_field_play_context *context,
                                 fd2_bgm_player *bgm) {
    if (!context) return;
    memset(context, 0, sizeof(*context));
    fd2_field_rng_seed(&context->rng, field_rng_initial_seed());
    context->bgm = bgm;
    context->initialized = 1;
}

int fd2_field_play_request_music(fd2_field_play_context *context,
    size_t stage, int track, const char *reason) {
    if (!context || !context->initialized || track < -1) return -1;
    int result = 0;
    if (context->bgm) {
        result = track == -1 ? fd2_bgm_stop(context->bgm) :
            fd2_bgm_play(context->bgm, (size_t)track, 0);
    }
    printf("music request: stage=%zu track=%d reason=%s available=%d result=%d current=%d loop=0\n",
        stage, track, reason ? reason : "unspecified", context->bgm != NULL, result,
        context->bgm ? fd2_bgm_current_track(context->bgm) : -1);
    fflush(stdout);
    if (result != 0) {
        context->music_error = 1;
        fprintf(stderr, "音乐请求失败：stage=%zu track=%d。\n", stage, track);
    }
    return result;
}

static void field_phase_music(size_t stage, int track,
                               fd2_field_music_reason reason, void *userdata) {
    static const char *const reasons[] = {"phase-stop", "player", "enemy"};
    const char *name = (unsigned)reason < sizeof(reasons) / sizeof(reasons[0])
        ? reasons[(unsigned)reason] : "unknown-phase";
    (void)fd2_field_play_request_music(userdata, stage, track, name);
}

/* 16f55/19df7：沿用同一只读notice页，不能先present占位索引底色。 */
static int play_system_confirmation_text(fd2_field_game *game, fd2_vga *vga,
        size_t fragment, int fast) {
    if (fd2_field_game_render_system_notice(game, vga, fragment) != 0) return -1;
    fd2_vga_present(vga);
    if (!fast) fd2_delay_ms(250);
    return 0;
}

static void drain_frame_keys(fd2_vga *vga) {
    while (fd2_input_take_key(&vga->input, NULL)) {}
}

/* field_bottom_notice_open @0x1956b（原sub_1956B）与
 * field_bottom_notice_close @0x196cb（原sub_196CB）共用滑入框，借当前焦点。
 * page保留原始空框供关闭，后续文字只写真实VGA，不能回写成另一份单位真值。 */
typedef struct {
    uint8_t *background;
    fd2_vga *page;
    fd2_image portraits[4];
    int portrait_origin;
    int row;
    /* shortcut: 跨框嘴型相位留B6统一接入；此处仅复现框内合法帧。 */
    unsigned mouth_phase;
    unsigned glyph_counter;
} field_progress_box;

typedef struct {
    uint16_t tokens[128];
    size_t count;
} field_progress_text;

static int progress_text_append(field_progress_text *text, uint16_t token,
                                 const fd2_font *font) {
    if (text->count >= sizeof(text->tokens) / sizeof(text->tokens[0]) ||
        (token < 0xff00u && token >= font->glyph_count)) return -1;
    text->tokens[text->count++] = token;
    return 0;
}

typedef struct {
    uint32_t value;
    size_t new_fragment;
    size_t old_fragment;
} field_text_parameters;

static int field_text_expand(const fd2_field_game *game,
                                 const field_text_parameters *parameters,
                                 size_t fragment, unsigned depth,
                                 field_progress_text *text) {
    const uint8_t *tokens;
    size_t count;
    if (depth > 1 || fd2_text_entry_get_fragment(
            &game->ui_text, fragment, &tokens, &count) != 0) return -1;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(tokens, i);
        if (token == 0xffffu) return 0;
        if (token == 0xfffau) { /* 15f84的-6：十进制字符直接选字模0..9。 */
            char number[16];
            if (parameters->value > INT32_MAX) return -1;
            snprintf(number, sizeof(number), "%u", (unsigned)parameters->value);
            for (size_t j = 0; number[j]; j++)
                if (progress_text_append(text, (uint16_t)(number[j] - '0'), &game->font) != 0)
                    return -1;
        } else if (token == 0xfffcu || token == 0xfffbu) {
            /* 15f84的-4/-5嵌入动态片段；-2/-3仍是换行/等待续行。 */
            size_t nested = token == 0xfffcu
                ? parameters->new_fragment : parameters->old_fragment;
            if (depth || nested == SIZE_MAX ||
                field_text_expand(game, parameters, nested, depth + 1, text) != 0)
                return -1;
        } else {
            if (token >= 0xff00u && token != 0xfffeu && token != 0xfffdu) return -1;
            if (progress_text_append(text, token, &game->font) != 0) return -1;
        }
    }
    return -1;
}

static int progress_text_prepare(const fd2_field_game *game,
                                  const fd2_field_progress_notice *notice,
                                  field_progress_text *text) {
    if (!game || !notice || !text || !game->font.glyphs ||
        !game->font.glyph_count || game->font.glyph_count > game->font.size / 32u) return -1;
    memset(text, 0, sizeof(*text));
    switch (notice->kind) {
        case FD2_FIELD_PROGRESS_CLOSE: return notice->fragment == 0 ? 0 : -1;
        case FD2_FIELD_PROGRESS_GAIN: if (notice->fragment != 488) return -1; break;
        case FD2_FIELD_PROGRESS_LEVEL: if (notice->fragment != 489) return -1; break;
        case FD2_FIELD_PROGRESS_GROWTH:
            if (notice->fragment < 490 || notice->fragment > 494) return -1;
            break;
        case FD2_FIELD_PROGRESS_LEARN: if (notice->fragment != 587) return -1; break;
        case FD2_FIELD_PROGRESS_STATUS:
            if (notice->fragment < 481 || notice->fragment > 487) return -1;
            break;
        default: return -1;
    }
    field_text_parameters parameters = {notice->value, SIZE_MAX, SIZE_MAX};
    if (notice->kind == FD2_FIELD_PROGRESS_LEARN) {
        if (notice->magic_id >= 36) return -1;
        parameters.new_fragment = 441u + notice->magic_id;
    }
    return field_text_expand(game, &parameters, notice->fragment, 0, text);
}

static int progress_resources_preflight(const fd2_field_game *game,
                                        const fd2_field_progress_notice *notice,
                                        void *userdata) {
    const fd2_archive *fdother = userdata;
    field_progress_text text;
    if (!game || !notice || notice->actor_index >= game->units.count ||
        !game->map.cells || fd2_scene_field_message_validate(fdother) != 0 ||
        progress_text_prepare(game, notice, &text) != 0) return -1;
    for (size_t i = 0; i < 4; i++) {
        fd2_image image = {0};
        int valid = fd2_portrait_decode_frame(&image, &game->dato_archive,
            game->units.items[notice->actor_index].unit_id, i) == 0 &&
            image.width == 80 && image.height == 80;
        fd2_image_free(&image);
        if (!valid) return -1;
    }
    return 0;
}

static void progress_box_free(field_progress_box *box) {
    if (!box) return;
    for (size_t i = 0; i < 4; i++) fd2_image_free(&box->portraits[i]);
    free(box->background);
    free(box->page);
    memset(box, 0, sizeof(*box));
}

static void progress_portrait(field_progress_box *box, fd2_vga *vga,
                               unsigned frame, int opening) {
    const fd2_image *image = &box->portraits[frame];
    int x = box->portrait_origin % VGA_STRIDE;
    int y = box->portrait_origin / VGA_STRIDE;
    int reverse = opening || box->portrait_origin == 36887;
    /* 1956b初始一律reverse；16559仅普通下框reverse，128..132保持forward。 */
    for (int row = 0; row < image->height; row++)
        for (int column = 0; column < image->width; column++) {
            int dx = reverse ? x - column : x + column;
            if (dx >= 0 && dx < VGA_W && y + row >= 0 && y + row < VGA_H)
                vga->framebuffer[(y + row) * VGA_STRIDE + dx] =
                    image->pixels[(size_t)row * (size_t)image->width + (size_t)column];
        }
}

static int progress_box_transition(field_progress_box *box, fd2_vga *vga,
                                     int opening, int fast) {
    for (int phase = opening ? 5 : 1; opening ? phase >= 0 : phase < 6;
         phase += opening ? -1 : 1) {
        int y = 112 + phase * 13;
        memcpy(vga->framebuffer, box->background, VGA_W * VGA_H);
        for (int row = 0; row < 86 && y + row < VGA_H; row++)
            memcpy(vga->framebuffer + (y + row) * VGA_STRIDE + 5,
                box->page->framebuffer + (112 + row) * VGA_STRIDE + 5, 310);
        if (!fast) fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    }
    return FD2_SCENE_RESULT_OK;
}

static int progress_box_open(field_progress_box *box, fd2_field_game *game,
                              fd2_vga *vga, const fd2_archive *fdother,
                              size_t actor_index, int fast) {
    static const int special_origins[5] = {4283, 1707, 3939, 1398, 3644};
    if (!box || box->page || actor_index >= game->units.count) return -1;
    unsigned id = game->units.items[actor_index].unit_id;
    box->portrait_origin = id >= 128 && id <= 132 ? special_origins[id - 128] : 36887;
    box->row = 2;
    box->background = malloc(VGA_W * VGA_H);
    box->page = calloc(1, sizeof(*box->page));
    if (!box->background || !box->page) goto fail;
    memcpy(box->background, vga->framebuffer, VGA_W * VGA_H);
    memcpy(box->page->framebuffer, box->background, VGA_W * VGA_H);
    for (size_t i = 0; i < 4; i++)
        if (fd2_portrait_decode_frame(&box->portraits[i], &game->dato_archive, id, i) != 0 ||
            box->portraits[i].width != 80 || box->portraits[i].height != 80) goto fail;
    if (fd2_scene_field_message_box(box->page, fdother) != 0) goto fail;
    progress_portrait(box, box->page, 0, 1);
    return progress_box_transition(box, vga, 1, fast);
fail:
    progress_box_free(box);
    return FD2_SCENE_RESULT_ERROR;
}

static int progress_wait(field_progress_box *box, fd2_field_game *game,
                          fd2_vga *vga, const fd2_archive *fdother, int fast,
                          int arrow_mode) {
    if (!game->attack_rng) return FD2_SCENE_RESULT_ERROR;
    uint32_t counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
    if (fast) return FD2_SCENE_RESULT_OK;
    unsigned arrow = 18, arrow_ticks = 0;
    int blinking = 0;
    uint64_t last = SDL_GetTicks();
    /* dialog_wait_key @0x16c57（原登记同名）：仅实参==1画18/19并在返回时擦13。
     * 190ac/1aa1d的外部等键均传0，仍保留相同blink/RNG与water等待。 */
    if (arrow_mode == 1 && fd2_scene_field_message_arrow(vga, fdother, arrow) != 0) return -1;
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        if (fd2_input_take_key(&vga->input, NULL)) break;
        uint64_t now = SDL_GetTicks();
        (void)fd2_field_water_tick(vga, now);
        if (now < last || now - last >= 110u) {
            if (arrow_mode == 1 && ++arrow_ticks == 3) {
                arrow_ticks = 0;
                arrow = arrow == 18 ? 19 : 18;
                if (fd2_scene_field_message_arrow(vga, fdother, arrow) != 0) return -1;
            }
            if (blinking) {
                progress_portrait(box, vga, 0, 0);
                counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
                blinking = 0;
            } else if (counter-- == 0) {
                progress_portrait(box, vga, 3, 0);
                blinking = 1;
            }
            last = now;
        }
        fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
    }
    return arrow_mode == 1 ? fd2_scene_field_message_arrow(vga, fdother, 13) : FD2_SCENE_RESULT_OK;
}

static int progress_scroll(fd2_vga *vga, int fast) {
    /* dialog_text_scroll_up @0x16e24：底框208×72，五次3px再一次4px。 */
    for (int step = 0; step < 6; step++) {
        int delta = step < 5 ? 3 : 4;
        for (int row = 0; row < 72; row++)
            memmove(vga->framebuffer + (119 + row) * VGA_STRIDE + 94,
                vga->framebuffer + (119 + row + delta) * VGA_STRIDE + 94, 208);
        memset(vga->framebuffer + 191 * VGA_STRIDE + 95, 0x4a, 208);
        if (!fast) fd2_vga_present(vga);
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    }
    return FD2_SCENE_RESULT_OK;
}

static int field_text_play(field_progress_box *box, fd2_field_game *game,
                               fd2_vga *vga, const fd2_archive *fdother,
                               fd2_field_audio *audio,
                               const field_progress_text *text, int x, int y, int fast) {
    int start_x = x;
    int glyph_step = !fast;
    for (size_t i = 0; i < text->count; i++) {
        uint16_t token = text->tokens[i];
        if (token == 0xfffeu || token == 0xfffdu) {
            x = start_x;
            y += 19;
            if (token == 0xfffdu) {
                progress_portrait(box, vga, 0, 0);
                int waited = progress_wait(box, game, vga, fdother, fast, 1);
                if (waited != FD2_SCENE_RESULT_OK) return waited;
                glyph_step = !fast;
            }
            continue;
        }
        fd2_font_draw_glyph(vga, &game->font, token, x, y, 0xcd, 0x4c, 0x4a);
        x += 16;
        if (!fast) fd2_vga_present(vga);
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        if (fd2_input_has_any_key(&vga->input)) glyph_step = 0;
        if (glyph_step) {
            if (++box->glyph_counter == 2) {
                box->glyph_counter = 0;
                box->mouth_phase = (box->mouth_phase + 1u) % 4u;
                progress_portrait(box, vga, box->mouth_phase == 3 ? 1 : box->mouth_phase, 0);
            }
            if (audio) (void)fd2_field_audio_play(audio, FD2_FIELD_SFX_DIALOG_GLYPH);
            fd2_delay_ms(55);
        }
    }
    return FD2_SCENE_RESULT_OK;
}

static int progress_text_play(field_progress_box *box, fd2_field_game *game,
                               fd2_vga *vga, const fd2_archive *fdother,
                               fd2_field_audio *audio,
                               const fd2_field_progress_notice *notice, int fast) {
    field_progress_text text;
    if (progress_text_prepare(game, notice, &text) != 0) return -1;
    int x = notice->kind == FD2_FIELD_PROGRESS_STATUS ? 99 : 95;
    int y = notice->kind == FD2_FIELD_PROGRESS_STATUS ? 127 :
        notice->kind == FD2_FIELD_PROGRESS_GAIN ? 119 :
        notice->kind == FD2_FIELD_PROGRESS_LEVEL ? 138 : 119 + 19 * box->row;
    return field_text_play(box, game, vga, fdother, audio, &text, x, y, fast);
}

static int progress_timed_wait(fd2_vga *vga, unsigned ticks, int fast) {
    drain_frame_keys(vga);
    if (fast) return FD2_SCENE_RESULT_OK;
    uint64_t start = SDL_GetTicks();
    while (SDL_GetTicks() - start < (uint64_t)ticks * 55u) {
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        if (fd2_input_has_any_key(&vga->input)) break;
        (void)fd2_field_water_tick(vga, SDL_GetTicks());
        fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
    }
    drain_frame_keys(vga);
    return FD2_SCENE_RESULT_OK;
}

typedef enum {
    FIELD_LIST_ERROR = -1,
    FIELD_LIST_CANCELLED = 0,
    FIELD_LIST_CONFIRMED = 1,
    FIELD_LIST_HOST_QUIT = 2
} field_list_result;

typedef enum { FIELD_LIST_MAGIC, FIELD_LIST_ITEM, FIELD_LIST_GIVE } field_list_kind;

static fd2_field_magic_list_key magic_list_key(fd2_input_key key) {
    switch (key) {
        case FD2_INPUT_KEY_UP: return FD2_FIELD_MAGIC_LIST_KEY_UP;
        case FD2_INPUT_KEY_DOWN: return FD2_FIELD_MAGIC_LIST_KEY_DOWN;
        case FD2_INPUT_KEY_LEFT: return FD2_FIELD_MAGIC_LIST_KEY_LEFT;
        case FD2_INPUT_KEY_RIGHT: return FD2_FIELD_MAGIC_LIST_KEY_RIGHT;
        case FD2_INPUT_KEY_ENTER:
        case FD2_INPUT_KEY_SPACE:
        case FD2_INPUT_KEY_KEYPAD_CONFIRM:
            return FD2_FIELD_MAGIC_LIST_KEY_CONFIRM;
        case FD2_INPUT_KEY_ESCAPE:
        case FD2_INPUT_KEY_CANCEL: return FD2_FIELD_MAGIC_LIST_KEY_CANCEL;
        default: return FD2_FIELD_MAGIC_LIST_KEY_OTHER;
    }
}

static fd2_field_item_list_key item_list_key(fd2_input_key key) {
    switch (key) {
        case FD2_INPUT_KEY_UP: return FD2_FIELD_ITEM_LIST_KEY_UP;
        case FD2_INPUT_KEY_DOWN: return FD2_FIELD_ITEM_LIST_KEY_DOWN;
        case FD2_INPUT_KEY_LEFT: return FD2_FIELD_ITEM_LIST_KEY_LEFT;
        case FD2_INPUT_KEY_RIGHT: return FD2_FIELD_ITEM_LIST_KEY_RIGHT;
        case FD2_INPUT_KEY_ENTER:
        case FD2_INPUT_KEY_SPACE:
        case FD2_INPUT_KEY_KEYPAD_CONFIRM:
            return FD2_FIELD_ITEM_LIST_KEY_CONFIRM;
        case FD2_INPUT_KEY_ESCAPE:
        case FD2_INPUT_KEY_CANCEL: return FD2_FIELD_ITEM_LIST_KEY_CANCEL;
        default: return FD2_FIELD_ITEM_LIST_KEY_OTHER;
    }
}

static int field_list_selected(const fd2_field_game *game, field_list_kind kind) {
    if (kind == FIELD_LIST_GIVE) return game->item_give_selected;
    return kind == FIELD_LIST_MAGIC ? game->magic_list_selected : game->item_list_selected;
}

static int field_list_step(fd2_field_game *game, field_list_kind kind,
                            fd2_input_key key, uint8_t *confirmed) {
    if (kind == FIELD_LIST_MAGIC)
        return fd2_field_game_magic_list_step(game, magic_list_key(key), confirmed);
    int result = kind == FIELD_LIST_GIVE
        ? fd2_field_game_item_give_list_step(game, item_list_key(key))
        : fd2_field_game_item_list_step(game, item_list_key(key));
    if (result == 1) *confirmed = (uint8_t)field_list_selected(game, kind);
    return result;
}

static int field_list_draw(fd2_vga *vga, const fd2_field_game *game,
                            const fd2_field_unit *unit,
                            field_list_kind kind, int selected) {
    if (kind == FIELD_LIST_MAGIC)
        return fd2_field_magic_list_draw(vga, &game->info_assets, &game->font,
                                         &game->ui_text, unit, selected);
    return fd2_field_item_list_draw(vga, &game->info_assets, &game->font,
                                    &game->ui_text, unit, selected);
}

/* field_player_magic_command_execute @0x1cff0（原登记同名）与
 * field_unit_inventory_select @0x1b932（原sub_1B932）的列表呈现／等待；
 * EQT留页依据field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）。
 * 按键分别复用@0x1d51d（原sub_1D51D）与@0x1b9de（原sub_1B9DE）。
 * 不结束LIST、不扣资源、不提交临时移动或pending。
 * sub_16C57 @0x16c57每次等键入口和眨眼复位均消费session共享RNG；
 * 16ms只为SDL呈现，110ms只推进一次，不宣称与DOS墙钟相位同步。 */
static field_list_result play_field_unit_list(
    fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
    field_list_kind kind, uint8_t *confirmed) {
    if (!vga) return FIELD_LIST_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FIELD_LIST_HOST_QUIT;
    if (!game || !game->ready || !confirmed ||
        game->failure != FD2_FIELD_FAILURE_NONE || !game->attack_rng ||
        game->interaction != (kind == FIELD_LIST_MAGIC ? FD2_FIELD_INTERACTION_MAGIC_LIST :
            kind == FIELD_LIST_GIVE ? FD2_FIELD_INTERACTION_ITEM_GIVE_LIST : FD2_FIELD_INTERACTION_ITEM_LIST) ||
        game->detail_visible || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->units.count > FD2_FIELD_MAX_UNITS)
        return FIELD_LIST_ERROR;
    if (kind == FIELD_LIST_ITEM &&
        !(game->command_selected == 0 ? fd2_field_game_item_use_is_available(game) :
          game->command_selected == 1 ? fd2_field_game_item_give_is_available(game) :
          game->command_selected == 2 ? fd2_field_game_item_equip_is_available(game) :
          game->command_selected == 3 && fd2_field_game_item_drop_is_available(game)))
        return FIELD_LIST_ERROR;
    if (kind == FIELD_LIST_GIVE && !fd2_field_game_item_give_is_available(game))
        return FIELD_LIST_ERROR;
    uint8_t candidate_id = 0;
    /* 统一复用core的actor／移动／完整record门，OTHER不改索引或ID。 */
    if (field_list_step(game, kind, FD2_INPUT_KEY_OTHER, &candidate_id) != 0)
        return FIELD_LIST_ERROR;
    int unit_index = kind == FIELD_LIST_GIVE ? game->item_give_recipient : game->selected_unit;
    if (unit_index < 0 || (size_t)unit_index >= game->units.count) return FIELD_LIST_ERROR;
    const fd2_field_unit *unit = &game->units.items[unit_index];
    size_t count = kind == FIELD_LIST_MAGIC
        ? fd2_field_unit_magic_list_build(unit, NULL, 0)
        : fd2_field_unit_inventory_count(unit);
    size_t maximum = kind == FIELD_LIST_MAGIC ? FD2_FIELD_MAGIC_LIST_MAX_COUNT : 8u;
    if (count == 0 || count > maximum || field_list_selected(game, kind) < 0 ||
        (size_t)field_list_selected(game, kind) >= count ||
        !fd2_field_game_detail_upper_is_ready(game, unit))
        return FIELD_LIST_ERROR;

    fd2_image portraits[2] = {{0}, {0}};
    uint8_t *background = NULL;
    fd2_vga *page = NULL;
    field_list_result result = FIELD_LIST_ERROR;
    int initial_info_panel_right = game->info_panel_right;
    int entered = 0;
    if (fd2_portrait_decode_frame(&portraits[0], &game->dato_archive,
                                  unit->unit_id, 0) != 0 ||
        fd2_portrait_decode_frame(&portraits[1], &game->dato_archive,
                                  unit->unit_id, 3) != 0)
        goto done;
    for (size_t i = 0; i < 2; i++)
        if (!portraits[i].pixels || portraits[i].width != 80 ||
            portraits[i].height != 80) goto done;
    background = malloc(VGA_W * VGA_H);
    if (!background) goto done;
    page = malloc(sizeof(*page));
    if (!page) goto done;
    /* renderer只需一个离屏VGA目标；不复制宿主input／palette的owner。 */
    memset(page, 0, sizeof(*page));
    fd2_field_game_render(game, page);
    memcpy(background, page->framebuffer, VGA_W * VGA_H);
    fd2_field_detail_draw(page, &game->info_assets, &game->font,
                          &game->ui_text, unit, &portraits[0]);
    if (kind == FIELD_LIST_ITEM && game->command_selected == 2) {
        /* 1c142整flag清0会唤醒hidden equipped；首帧前预检全部潜在名称。
         * 只用80B临时记录，不预演装备或更改真实库存/信息面板owner。 */
        fd2_field_unit visible = *unit;
        uint8_t *raw = (uint8_t *)&visible;
        for (size_t slot = 0; slot < 8u; slot++)
            if ((raw[0x0a + slot * 2u] & 0xc0u) == 0xc0u)
                raw[0x0a + slot * 2u] = 0;
        if (field_list_draw(page, game, &visible, kind, -1) != 0)
            goto done;
    }
    if (field_list_draw(page, game, unit, kind, -1) != 0)
        goto done;
    /* 完整资源预检结束后才写实际VGA、耗键或消费RNG。 */
    if (fd2_input_take_quit(&vga->input)) {
        result = FIELD_LIST_HOST_QUIT;
        goto done;
    }
    entered = 1;
    drain_frame_keys(vga);
    for (int phase = 11; phase >= 0; phase--) {
        fd2_field_detail_transition_frame(vga->framebuffer,
            page->framebuffer, background, phase);
        fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
        if (fd2_input_take_quit(&vga->input)) {
            result = FIELD_LIST_HOST_QUIT;
            goto done;
        }
        drain_frame_keys(vga);
    }
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) {
            result = FIELD_LIST_HOST_QUIT;
            goto done;
        }
        if (field_list_draw(vga, game, unit, kind, field_list_selected(game, kind)) != 0)
            goto done;
        /* 16c57的counter/flag/tick每次调用重置；立绘本身跨键保留。 */
        uint32_t counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
        int blink = 0;
        uint64_t last = SDL_GetTicks();
        fd2_input_event event;
        for (;;) {
            if (fd2_input_take_quit(&vga->input)) {
                result = FIELD_LIST_HOST_QUIT;
                goto done;
            }
            if (fd2_input_take_key(&vga->input, &event)) break;
            uint64_t now = SDL_GetTicks();
            (void)fd2_field_water_tick(vga, now);
            if (now < last || now - last >= 110u) {
                if (blink) {
                    fd2_map_sprite_blit(vga, &portraits[0], 8, 10, -1);
                    counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
                    blink = 0;
                } else if (counter-- == 0u) {
                    fd2_map_sprite_blit(vga, &portraits[1], 8, 10, -1);
                    blink = 1;
                }
                last = now;
            }
            fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
        }
        if (fd2_input_take_quit(&vga->input)) {
            result = FIELD_LIST_HOST_QUIT;
            goto done;
        }
        int previous = field_list_selected(game, kind);
        int step = field_list_step(game, kind, event.key, &candidate_id);
        if (step == -2) goto done;
        if (step == 1 && kind == FIELD_LIST_ITEM && game->command_selected == 2) {
            /* field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）确认留同页；耗尽本帧普通键，退出先于不可逆装备。 */
            drain_frame_keys(vga);
            if (fd2_input_take_quit(&vga->input)) {
                result = FIELD_LIST_HOST_QUIT;
                goto done;
            }
            if (!fd2_field_game_item_equip_is_available(game)) goto done;
            int applied = fd2_field_game_equip_item(game);
            if (applied < 0) goto done;
            if (applied) {
                if (fd2_input_take_quit(&vga->input)) {
                    result = FIELD_LIST_HOST_QUIT;
                    goto done;
                }
                if (!fd2_field_game_detail_upper_is_ready(game, unit)) goto done;
                memcpy(page->framebuffer, vga->framebuffer, VGA_W * VGA_H);
                /* 1bffe→17eef重画DATO首帧0及upper，再184c0(-1)重画底页。
                 * 同时更新关闭动画的page，不能闪回换装前的flag/AP。 */
                fd2_field_detail_draw(page, &game->info_assets, &game->font,
                                      &game->ui_text, unit, &portraits[0]);
                if (field_list_draw(page, game, unit, kind, -1) != 0)
                    goto done;
                memcpy(vga->framebuffer, page->framebuffer, VGA_W * VGA_H);
                fd2_vga_present(vga);
                if (fd2_input_take_quit(&vga->input)) {
                    result = FIELD_LIST_HOST_QUIT;
                    goto done;
                }
            }
            continue;
        }
        if (step != 0) {
            result = step == 1 ? FIELD_LIST_CONFIRMED : FIELD_LIST_CANCELLED;
            break;
        }
        /* native上下即使单项回绕也发声；左右只在实际跨列时发声。 */
        if (audio && game->option_values[1] &&
            (event.key == FD2_INPUT_KEY_UP || event.key == FD2_INPUT_KEY_DOWN ||
             previous != field_list_selected(game, kind)))
            (void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);
    }
    for (int phase = 0; phase <= 11; phase++) {
        fd2_field_detail_transition_frame(vga->framebuffer,
            page->framebuffer, background, phase);
        fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
        if (fd2_input_take_quit(&vga->input)) {
            result = FIELD_LIST_HOST_QUIT;
            goto done;
        }
        drain_frame_keys(vga);
    }
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    fd2_vga_present(vga);
    if (fd2_input_take_quit(&vga->input)) {
        result = FIELD_LIST_HOST_QUIT;
        goto done;
    }
    drain_frame_keys(vga);
    if (result == FIELD_LIST_CONFIRMED) *confirmed = candidate_id;
done:
    /* 离屏render会选信息面板侧位；未开始呈现的失败不能提交该UI owner。 */
    if (!entered && (result == FIELD_LIST_ERROR || result == FIELD_LIST_HOST_QUIT))
        game->info_panel_right = initial_info_panel_right;
    fd2_vga_wait_frame_deadline(vga);
    free(page);
    free(background);
    fd2_image_free(&portraits[1]);
    fd2_image_free(&portraits[0]);
    return result;
}

static field_list_result play_field_magic_list(
    fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
    uint8_t *confirmed_id) {
    return play_field_unit_list(game, vga, audio, FIELD_LIST_MAGIC, confirmed_id);
}

/* 物品列表仅在完整关页后返回raw槽；四种子操作由command_selected区分。 */
static field_list_result play_field_item_list(
    fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
    uint8_t *confirmed_raw_slot) {
    return play_field_unit_list(game, vga, audio, FIELD_LIST_ITEM, confirmed_raw_slot);
}

static field_list_result play_field_item_give_list(fd2_field_game *game,
        fd2_vga *vga, fd2_field_audio *audio, uint8_t *confirmed_raw_slot) {
    return play_field_unit_list(game, vga, audio, FIELD_LIST_GIVE, confirmed_raw_slot);
}

/* 18d8c的1cff0重入：列表Esc回COMMAND，目标Esc由调用者先复位为LIST。
 * 只编排呈现和目标状态，不在这里提交费用或效果。 */
static int play_field_magic_selection(fd2_field_game *game, fd2_vga *vga,
                                       fd2_field_audio *audio) {
    uint8_t magic_id = 0;
    field_list_result result = play_field_magic_list(game, vga, audio, &magic_id);
    if (result == FIELD_LIST_HOST_QUIT) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_ERROR) return FD2_SCENE_RESULT_ERROR;
    if (result == FIELD_LIST_CONFIRMED)
        return fd2_field_game_begin_magic_targeting(game, magic_id) == 0
            ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
    if (fd2_field_game_end_magic_list(game) != 0) return FD2_SCENE_RESULT_ERROR;
    if (audio && game->option_values[1])
        (void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);
    if (fd2_field_game_animate_command(game, vga, 1,
            FD2_FIELD_COMMAND_FRAME_MS) != 0)
        return FD2_SCENE_RESULT_ERROR;
    return fd2_input_take_quit(&vga->input)
        ? FD2_SCENE_RESULT_HOST_QUIT : FD2_SCENE_RESULT_OK;
}

static int play_command_transition(fd2_field_game *game, fd2_vga *vga,
                                    fd2_field_audio *audio, int opening) {
    if (game->failure != FD2_FIELD_FAILURE_NONE) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (audio && game->option_values[1])
        (void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);
    int result = fd2_field_game_animate_command(
        game, vga, opening, FD2_FIELD_COMMAND_FRAME_MS);
    drain_frame_keys(vga);
    if (result != 0 || game->failure != FD2_FIELD_FAILURE_NONE)
        return FD2_SCENE_RESULT_ERROR;
    return fd2_input_take_quit(&vga->input)
        ? FD2_SCENE_RESULT_HOST_QUIT : FD2_SCENE_RESULT_OK;
}

/* field_command_menu_close @0x176b4（原登记同名）关闭后清键；退出先于下一层提交。 */
static int play_system_menu_close(fd2_field_game *game, fd2_vga *vga,
                                   fd2_field_audio *audio) {
    if (game->failure != FD2_FIELD_FAILURE_NONE) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (audio && game->option_values[1])
        (void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);
    int result = fd2_field_game_animate_system_menu(
        game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
    drain_frame_keys(vga);
    if (result != 0 || game->failure != FD2_FIELD_FAILURE_NONE)
        return FD2_SCENE_RESULT_ERROR;
    return fd2_input_take_quit(&vga->input)
        ? FD2_SCENE_RESULT_HOST_QUIT : FD2_SCENE_RESULT_OK;
}

/* 1bbdc四种子操作共用列表；完整关页才提交raw槽，按当前owner重开菜单。 */
static int play_field_item_selection(fd2_field_game *game, fd2_vga *vga,
                                      fd2_field_audio *audio) {
    uint8_t slot = 0xff;
    field_list_result result = play_field_item_list(game, vga, audio, &slot);
    if (result == FIELD_LIST_HOST_QUIT) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_ERROR) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_CONFIRMED) {
        if (slot != game->item_list_selected) return FD2_SCENE_RESULT_ERROR;
        if (game->command_selected == 0)
            return fd2_field_game_item_use_is_available(game) &&
                fd2_field_game_begin_item_targeting(game) == 0
                    ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
        if (game->command_selected == 1)
            return fd2_field_game_item_give_is_available(game) &&
                fd2_field_game_begin_item_give(game) == 0 ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
        if (game->command_selected != 3 || !fd2_field_game_item_drop_is_available(game) ||
            fd2_field_game_resolve_item_drop(game) != 0)
            return FD2_SCENE_RESULT_ERROR;
    } else if (fd2_field_game_end_item_list(game) != 0) {
        return FD2_SCENE_RESULT_ERROR;
    }
    return play_command_transition(game, vga, audio, 1);
}

static int play_field_item_give_selection(fd2_field_game *game, fd2_vga *vga,
                                         fd2_field_audio *audio) {
    uint8_t slot = 0xff;
    field_list_result result = play_field_item_give_list(game, vga, audio, &slot);
    if (result == FIELD_LIST_ERROR) return FD2_SCENE_RESULT_ERROR;
    if (result == FIELD_LIST_HOST_QUIT || fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_CONFIRMED) {
        if (slot != game->item_give_selected || !fd2_field_game_item_give_is_available(game) ||
            fd2_field_game_resolve_item_give(game) != 0) return FD2_SCENE_RESULT_ERROR;
    } else if (fd2_field_game_cancel_item_give(game) != 0) return FD2_SCENE_RESULT_ERROR;
    return play_command_transition(game, vga, audio, 1);
}

/* 18d8c在1bbdc返回-1后重开COMMAND，仍不回滚临时移动。 */
static int play_field_item_menu_cancel(fd2_field_game *game, fd2_vga *vga,
                                        fd2_field_audio *audio) {
    int result = play_command_transition(game, vga, audio, 0);
    if (result != FD2_SCENE_RESULT_OK) return result;
    if (fd2_field_game_end_item_menu(game) != 0) return FD2_SCENE_RESULT_ERROR;
    return play_command_transition(game, vga, audio, 1);
}

static fd2_input_context field_input_context(const fd2_field_game *game) {
    if (game->detail_visible) return FD2_INPUT_CONTEXT_FIELD_DETAIL;
    switch (game->interaction) {
        case FD2_FIELD_INTERACTION_UNIT_SELECTED:
        case FD2_FIELD_INTERACTION_MOVING: return FD2_INPUT_CONTEXT_FIELD_MOVEMENT;
        case FD2_FIELD_INTERACTION_COMMAND:
        case FD2_FIELD_INTERACTION_ITEM_MENU: return FD2_INPUT_CONTEXT_FIELD_COMMAND;
        case FD2_FIELD_INTERACTION_MAGIC_TARGETING:
        case FD2_FIELD_INTERACTION_ITEM_TARGETING:
        case FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING:
        case FD2_FIELD_INTERACTION_ITEM_TELEPORTING:
            /* 两种raw候选轮询共用既有方向／取消／Z／小键盘5映射。 */
            return FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING;
        case FD2_FIELD_INTERACTION_TARGETING: return FD2_INPUT_CONTEXT_FIELD_TARGETING;
        case FD2_FIELD_INTERACTION_SYSTEM_MENU: return FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU;
        case FD2_FIELD_INTERACTION_AUXILIARY: return FD2_INPUT_CONTEXT_FIELD_AUXILIARY;
        default: return FD2_INPUT_CONTEXT_FIELD;
    }
}

static const char *field_action_command_name(fd2_field_interaction previous, int command) {
    /* ITEM子菜单的Use索引0与顶层ATTACK不同。 */
    if (previous == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
        previous == FD2_FIELD_INTERACTION_ITEM_TELEPORTING) return "item";
    return command == FD2_FIELD_COMMAND_ATTACK ? "attack" :
        command == FD2_FIELD_COMMAND_MAGIC ? "magic" :
        command == FD2_FIELD_COMMAND_ITEM ? "item" : "wait";
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
    if (!game || !vga || !game->ready || game->stage > 2u ||
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
    /* DS:0x1e61控制sequence音量，DS:0x1e62控制SFX；修改bus不重启曲目。 */
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

/* field_unit_detail_present @0x17aed（原sub_17AED）：任意普通键推进一页。
 * native16c57完整等待／眨眼RNG另验，本adapter不编造额外随机流。 */
static int play_field_detail_step(fd2_field_game *game, fd2_vga *vga,
                                    fd2_field_audio *audio) {
    if (!vga) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (!game || !game->detail_visible || game->detail_unit < 0 ||
        (size_t)game->detail_unit >= game->units.count)
        return FD2_SCENE_RESULT_ERROR;
    int has_next = game->detail_page == FD2_FIELD_DETAIL_EQUIPMENT &&
        fd2_field_unit_magic_list_build(&game->units.items[game->detail_unit], NULL, 0) != 0;
    int result = has_next
        ? fd2_field_game_animate_detail_magic_page(game, vga, detail_phase_sfx, audio)
        : fd2_field_game_animate_detail(game, vga, 0, detail_phase_sfx, audio);
    drain_frame_keys(vga);
    if (result < 0) return FD2_SCENE_RESULT_ERROR;
    if (result == 1 || fd2_input_take_quit(&vga->input))
        return FD2_SCENE_RESULT_HOST_QUIT;
    if (!has_next) fd2_field_game_close_detail(game);
    return FD2_SCENE_RESULT_OK;
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

static int present_field_events_mode(fd2_field_game *game,
                                     fd2_vga *vga,
                                     const fd2_archive *fdother,
                                     fd2_field_audio *audio, int fast) {
    if (!game || !vga || !fdother || game->failure != FD2_FIELD_FAILURE_NONE)
        return -1;
    for (size_t i = 0; i < game->event_log_count; i++) {
        fd2_field_event_notice *notice = &game->event_log[i];
        if (!notice->handled) return -1;
        if (!notice->presentation_deferred) continue;
        int result = fd2_scene_play_field_event(
            vga, fdother, game, notice, audio, fast);
        if (result == FD2_SCENE_RESULT_HOST_QUIT)
            return FD2_SCENE_RESULT_HOST_QUIT;
        if (result != 0)
            return -1;
        notice->presentation_deferred = 0;
    }
    return 0;
}

/* 1aa1d/190ac仅借mode0列表，选中项保存在本次modal，不冒充玩家菜单owner。 */
static int loot_inventory_select(fd2_field_game *game, fd2_vga *vga,
    fd2_field_audio *audio, size_t actor, int *raw_slot) {
    if (actor >= game->units.count || !raw_slot || !game->attack_rng)
        return FD2_SCENE_RESULT_ERROR;
    const fd2_field_unit *unit = &game->units.items[actor];
    int selected = 0;
    if (fd2_field_item_list_select_step(unit, &selected,
            FD2_FIELD_ITEM_LIST_KEY_OTHER, 0) != 0 ||
        !fd2_field_game_detail_upper_is_ready(game, unit)) return -1;
    fd2_image portraits[2] = {{0}};
    fd2_vga *page = calloc(1, sizeof(*page));
    uint8_t *background = malloc(VGA_W * VGA_H);
    int result = FD2_SCENE_RESULT_ERROR;
    if (!page || !background) goto done;
    for (unsigned i = 0; i < 2; i++)
        if (fd2_portrait_decode_frame(&portraits[i], &game->dato_archive,
                unit->unit_id, i ? 3 : 0) != 0 ||
            portraits[i].width != 80 || portraits[i].height != 80) goto done;
    memcpy(background, vga->framebuffer, VGA_W * VGA_H);
    memcpy(page->framebuffer, background, VGA_W * VGA_H);
    fd2_field_detail_draw(page, &game->info_assets, &game->font,
        &game->ui_text, unit, &portraits[0]);
    if (fd2_field_item_list_draw(page, &game->info_assets, &game->font,
            &game->ui_text, unit, -1) != 0) goto done;
    drain_frame_keys(vga);
    for (int opening = 1; opening >= 0; opening--) {
        for (int phase = opening ? 11 : 0; opening ? phase >= 0 : phase <= 11;
                phase += opening ? -1 : 1) {
            fd2_field_detail_transition_frame(vga->framebuffer,
                page->framebuffer, background, phase);
            int cue = fd2_field_detail_sfx_for_phase(opening, phase);
            if (audio && game->option_values[1] && cue >= 0)
                (void)fd2_field_audio_play(audio, opening
                    ? FD2_FIELD_SFX_DETAIL_OPEN : FD2_FIELD_SFX_DETAIL_CLOSE);
            fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
            if (fd2_input_take_quit(&vga->input)) {
                result = FD2_SCENE_RESULT_HOST_QUIT; goto done;
            }
            drain_frame_keys(vga);
        }
        if (!opening) break;
        for (;;) {
            if (fd2_field_item_list_draw(vga, &game->info_assets, &game->font,
                    &game->ui_text, unit, selected) != 0) goto done;
            uint32_t counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
            uint64_t last = SDL_GetTicks();
            int blinking = 0;
            fd2_input_event event;
            for (;;) {
                if (fd2_input_take_quit(&vga->input)) {
                    result = FD2_SCENE_RESULT_HOST_QUIT; goto done;
                }
                if (fd2_input_take_key(&vga->input, &event)) break;
                uint64_t now = SDL_GetTicks();
                (void)fd2_field_water_tick(vga, now);
                if (now < last || now - last >= 110u) {
                    if (blinking) {
                        fd2_map_sprite_blit(vga, &portraits[0], 8, 10, -1);
                        counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
                        blinking = 0;
                    } else if (counter-- == 0u) {
                        fd2_map_sprite_blit(vga, &portraits[1], 8, 10, -1);
                        blinking = 1;
                    }
                    last = now;
                }
                fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
            }
            int previous = selected;
            int step = fd2_field_item_list_select_step(unit, &selected,
                item_list_key(event.key), 0);
            if (step == -2) goto done;
            if (step != 0) {
                *raw_slot = step == 1 ? selected : -1;
                result = FD2_SCENE_RESULT_OK;
                break;
            }
            if (audio && game->option_values[1] &&
                (previous != selected || event.key == FD2_INPUT_KEY_UP || event.key == FD2_INPUT_KEY_DOWN))
                (void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);
        }
    }
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    fd2_vga_present(vga);
    drain_frame_keys(vga);
done:
    free(background); free(page);
    for (unsigned i = 0; i < 2; i++) fd2_image_free(&portraits[i]);
    return result;
}

static int loot_text_prepare(const fd2_field_game *game,
    const fd2_field_loot_notice *notice, field_progress_text *text) {
    if (!game || !notice || !text || !game->font.glyphs ||
        !game->font.glyph_count || game->font.glyph_count > game->font.size / 32u)
        return -1;
    memset(text, 0, sizeof(*text));
    field_text_parameters parameters = {notice->value,
        fd2_field_item_record_get(notice->item_id) ? 181u + notice->item_id : SIZE_MAX,
        fd2_field_item_record_get(notice->old_item_id) ? 181u + notice->old_item_id : SIZE_MAX};
    return field_text_expand(game, &parameters, notice->fragment, 0, text);
}

static int loot_resources_preflight(const fd2_field_game *game,
    const fd2_field_loot_notice *notice, void *userdata) {
    const fd2_archive *fdother = userdata;
    if (!game || !notice || notice->actor_index >= game->units.count ||
        !game->map.cells || fd2_scene_field_message_validate(fdother) != 0)
        return -1;
    if (notice->kind == FD2_FIELD_LOOT_STORY)
        return fd2_scene_field_story_validate(game, notice->fragment);
    if (notice->kind == FD2_FIELD_LOOT_FIELD_EVENT) {
        if (game->stage == 0 && notice->fragment == 4)
            return fd2_scene_field_story_validate(game, 7);
        if (game->stage == 1 && notice->fragment == 6) {
            fd2_field_game shadow = *game;
            if (fd2_field_units_append_group_placed(&shadow.units, &game->metadata,
                    &game->placements, &game->map, 3, 1) != 0) return -1;
            return fd2_scene_field_story_validate(&shadow, 4);
        }
        return -1;
    }
    const fd2_field_unit *unit = &game->units.items[notice->actor_index];
    if (notice->kind == FD2_FIELD_LOOT_INVENTORY) {
        fd2_vga *page = calloc(1, sizeof(*page));
        if (!page) return -1;
        int valid = fd2_field_game_detail_upper_is_ready(game, unit) &&
            fd2_field_item_list_draw(page, &game->info_assets, &game->font,
                &game->ui_text, unit, -1) == 0;
        free(page);
        if (!valid) return -1;
    } else if (notice->kind == FD2_FIELD_LOOT_MESSAGE || notice->kind == FD2_FIELD_LOOT_CHOICE) {
        field_progress_text text;
        if (loot_text_prepare(game, notice, &text) != 0) return -1;
        if (notice->kind == FD2_FIELD_LOOT_CHOICE) {
            static const unsigned ids[] = {48,49,51,52};
            if (!game->command_assets.ready) return -1;
            for (size_t i = 0; i < sizeof(ids)/sizeof(ids[0]); i++) {
                const fd2_image *image = &game->command_assets.frames[ids[i]];
                if (!image->pixels || image->width != 24 || image->height != 16) return -1;
            }
        }
    } else return notice->kind == FD2_FIELD_LOOT_DELAY ? 0 : -1;
    for (unsigned frame = 0; frame < 4; frame++) {
        fd2_image image = {0};
        int valid = fd2_portrait_decode_frame(&image, &game->dato_archive,
            unit->unit_id, frame) == 0 && image.width == 80 && image.height == 80;
        fd2_image_free(&image);
        if (!valid) return -1;
    }
    return 0;
}

/* sub_19953 @0x19953：Yes/No固定24×16，phase/2高亮；返回前phase0。 */
static void loot_choice_draw(fd2_field_game *game, fd2_vga *vga,
    const uint8_t *background, int displacement, int selected, unsigned phase) {
    for (int row = 0; row < 16; row++)
        memcpy(vga->framebuffer + (168 + row) * VGA_STRIDE + 232,
            background + row * 56, 56);
    fd2_map_sprite_blit(vga, &game->command_assets.frames[48u + (selected == 0 && phase/2u)], 248-displacement, 168, -1);
    fd2_map_sprite_blit(vga, &game->command_assets.frames[51u + (selected == 1 && phase/2u)], 248+displacement, 168, -1);
}

static int loot_choice(field_progress_box *box, fd2_field_game *game,
    fd2_vga *vga, fd2_field_loot_reply *reply) {
    int selected = 0, blinking = 0;
    unsigned phase = 0;
    uint32_t counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
    uint64_t last = SDL_GetTicks();
    uint8_t background[56*16];
    for (int row = 0; row < 16; row++)
        memcpy(background + row * 56,
            vga->framebuffer + (168 + row) * VGA_STRIDE + 232, 56);
    drain_frame_keys(vga);
    for (int step = 1; step <= 4; step++) {
        loot_choice_draw(game, vga, background, step*4, selected, 0);
        fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        drain_frame_keys(vga);
    }
    for (;;) {
        loot_choice_draw(game, vga, background, 16, selected, phase);
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        fd2_input_event event;
        if (fd2_input_take_key(&vga->input, &event)) {
            if (event.key == FD2_INPUT_KEY_LEFT) selected = 0;
            else if (event.key == FD2_INPUT_KEY_RIGHT) selected = 1;
            else if (event.key == FD2_INPUT_KEY_ENTER || event.key == FD2_INPUT_KEY_SPACE ||
                     event.key == FD2_INPUT_KEY_KEYPAD_CONFIRM || event.key == FD2_INPUT_KEY_ESCAPE ||
                     event.key == FD2_INPUT_KEY_CANCEL) {
                *reply = event.key == FD2_INPUT_KEY_ESCAPE || event.key == FD2_INPUT_KEY_CANCEL
                    ? FD2_FIELD_LOOT_CANCEL : selected ? FD2_FIELD_LOOT_NO : FD2_FIELD_LOOT_YES;
                for (int step = 3; step >= 0; step--) {
                    loot_choice_draw(game, vga, background, step*4, selected, 0);
                    fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
                    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
                }
                for (int row = 0; row < 16; row++)
                    memcpy(vga->framebuffer + (168+row)*VGA_STRIDE+232,
                        background+row*56, 56);
                drain_frame_keys(vga);
                return FD2_SCENE_RESULT_OK;
            }
        }
        uint64_t now = SDL_GetTicks();
        (void)fd2_field_water_tick(vga, now);
        if (now < last || now - last >= 110u) {
            phase = (phase + 1u) % 4u;
            if (blinking) {
                progress_portrait(box, vga, 0, 0);
                counter = game->attack_rng(game->attack_rng_userdata) % 30u + 10u;
                blinking = 0;
            } else if (counter-- == 0u) {
                progress_portrait(box, vga, 3, 0); blinking = 1;
            }
            last = now;
        }
        fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
    }
}

static int loot_fixed_wait(fd2_vga *vga, unsigned milliseconds, int fast) {
    uint64_t start = SDL_GetTicks();
    while (!fast && SDL_GetTicks() - start < milliseconds) {
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        (void)fd2_field_water_tick(vga, SDL_GetTicks());
        fd2_vga_present_timed(vga, FD2_FIELD_COMMAND_FRAME_MS);
    }
    drain_frame_keys(vga);
    return fd2_input_take_quit(&vga->input) ? FD2_SCENE_RESULT_HOST_QUIT : 0;
}

static int play_field_loot(fd2_field_game *game, fd2_vga *vga,
    const fd2_archive *fdother, fd2_field_audio *audio, int fast) {
    field_progress_box box = {0};
    int result = 0;
    for (;;) {
        fd2_field_loot_notice notice;
        int pending = fd2_field_game_loot_peek(game, &notice);
        if (pending < 0) { result = -1; break; }
        if (!pending) { if (box.page) result = -1; break; }
        if (fd2_input_take_quit(&vga->input)) { result = FD2_SCENE_RESULT_HOST_QUIT; break; }
        printf("field loot: kind=%d actor=%zu fragment=%u value=%u item=%u old=%u open=%u append=%u close=%u key=%u delay=%u\n",
            (int)notice.kind, notice.actor_index, notice.fragment, notice.value,
            notice.item_id, notice.old_item_id, notice.open_box, notice.append_line,
            notice.close_box, notice.wait_key, notice.wait_ms);
        fflush(stdout);
        fd2_field_loot_reply reply = FD2_FIELD_LOOT_ACK;
        int slot = -1;
        if (notice.kind == FD2_FIELD_LOOT_MESSAGE || notice.kind == FD2_FIELD_LOOT_CHOICE) {
            field_progress_text text;
            if (loot_text_prepare(game, &notice, &text) != 0) { result = -1; break; }
            if (notice.open_box) {
                if (box.page) { result = -1; break; }
                fd2_field_game_render(game, vga);
                result = progress_box_open(&box, game, vga, fdother, notice.actor_index, fast);
                if (result != 0) break;
            }
            if (!box.page) { result = -1; break; }
            result = field_text_play(&box, game, vga, fdother, audio, &text,
                99, notice.append_line ? 146 : 127, fast);
            if (result != 0) break;
            if (fd2_input_take_quit(&vga->input)) { result = FD2_SCENE_RESULT_HOST_QUIT; break; }
            if (notice.kind == FD2_FIELD_LOOT_MESSAGE &&
                fd2_field_game_loot_text_ready(game) != 0) { result = -1; break; }
            progress_portrait(&box, vga, 0, 0);
            if (notice.kind == FD2_FIELD_LOOT_CHOICE) result = loot_choice(&box, game, vga, &reply);
            else if (notice.wait_key) {
                drain_frame_keys(vga);
                result = progress_wait(&box, game, vga, fdother, fast, 0);
            }
            if (result == 0 && notice.wait_ms) result = loot_fixed_wait(vga, notice.wait_ms, fast);
        } else if (notice.kind == FD2_FIELD_LOOT_INVENTORY) {
            /* 原选择页独立开合；先完成仍留着的底框再进入mode0。 */
            if (box.page) {
                result = progress_box_transition(&box, vga, 0, fast);
                if (result != 0) break;
                memcpy(vga->framebuffer, box.background, VGA_W * VGA_H);
                progress_box_free(&box);
            }
            result = loot_inventory_select(game, vga, audio, notice.actor_index, &slot);
            reply = slot < 0 ? FD2_FIELD_LOOT_CANCEL : FD2_FIELD_LOOT_SLOT;
        } else if (notice.kind == FD2_FIELD_LOOT_DELAY) {
            result = loot_fixed_wait(vga, notice.wait_ms, fast);
        } else if (notice.kind == FD2_FIELD_LOOT_STORY) {
            result = fd2_scene_play_field_story(vga, fdother, game, notice.fragment, audio, fast);
        } else if (notice.kind == FD2_FIELD_LOOT_FIELD_EVENT) {
            result = present_field_events_mode(game, vga, fdother, audio, fast);
        } else result = -1;
        if (result != 0) break;
        if (notice.close_box && box.page) {
            result = progress_box_transition(&box, vga, 0, fast);
            if (result != 0) break;
            memcpy(vga->framebuffer, box.background, VGA_W * VGA_H);
            progress_box_free(&box);
            fd2_vga_present(vga);
        }
        if (fd2_input_take_quit(&vga->input)) { result = FD2_SCENE_RESULT_HOST_QUIT; break; }
        if (fd2_field_game_loot_reply(game, reply, slot) != 0) { result = -1; break; }
    }
    progress_box_free(&box);
    return result;
}

static int play_field_progress(fd2_field_game *game, fd2_vga *vga,
                                const fd2_archive *fdother,
                                fd2_field_audio *audio, int fast) {
    if (!game || !vga || !fdother || game->failure != FD2_FIELD_FAILURE_NONE)
        return FD2_SCENE_RESULT_ERROR;
    field_progress_box box = {0};
    int result = FD2_SCENE_RESULT_OK;
    int played = 0;
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            break;
        }
        result = play_field_loot(game, vga, fdother, audio, fast);
        if (result != FD2_SCENE_RESULT_OK) break;
        result = present_field_events_mode(game, vga, fdother, audio, fast);
        if (result != FD2_SCENE_RESULT_OK) break;
        int resumed = fd2_field_game_progress_resume(game);
        if (resumed < 0) { result = FD2_SCENE_RESULT_ERROR; break; }
        fd2_field_progress_notice notice;
        int pending = fd2_field_game_progress_peek(game, &notice);
        if (pending < 0) { result = FD2_SCENE_RESULT_ERROR; break; }
        if (!pending) {
            if (resumed > 0) continue;
            if (box.page) result = FD2_SCENE_RESULT_ERROR;
            break;
        }
        played = 1;
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            break;
        }
        if (!box.page) {
            if (notice.kind != FD2_FIELD_PROGRESS_GAIN && notice.kind != FD2_FIELD_PROGRESS_STATUS) {
                result = FD2_SCENE_RESULT_ERROR;
                break;
            }
            if (progress_resources_preflight(game, &notice, (void *)fdother) != 0) {
                result = FD2_SCENE_RESULT_ERROR;
                break;
            }
            if (notice.kind == FD2_FIELD_PROGRESS_GAIN) drain_frame_keys(vga);
            fd2_field_game_render(game, vga);
            result = progress_box_open(&box, game, vga, fdother, notice.actor_index, fast);
            if (result != FD2_SCENE_RESULT_OK) break;
        }
        printf("field progress: kind=%d actor=%zu fragment=%u value=%u magic=%u\n",
            (int)notice.kind, notice.actor_index, (unsigned)notice.fragment,
            (unsigned)notice.value, (unsigned)notice.magic_id);
        fflush(stdout);
        if (notice.kind == FD2_FIELD_PROGRESS_CLOSE) {
            result = progress_timed_wait(vga, notice.wait_ticks, fast);
        } else {
            if (notice.kind == FD2_FIELD_PROGRESS_LEVEL || notice.kind == FD2_FIELD_PROGRESS_GROWTH)
                drain_frame_keys(vga);
            if (notice.kind == FD2_FIELD_PROGRESS_GROWTH && box.row == 3) {
                result = progress_scroll(vga, fast);
                if (result != FD2_SCENE_RESULT_OK) break;
                box.row = 2;
            }
            result = progress_text_play(&box, game, vga, fdother, audio, &notice, fast);
            if (result != FD2_SCENE_RESULT_OK) break;
            if (notice.kind == FD2_FIELD_PROGRESS_GAIN) progress_portrait(&box, vga, 0, 0);
            if (notice.kind == FD2_FIELD_PROGRESS_STATUS)
                result = progress_timed_wait(vga, notice.wait_ticks, fast);
        }
        if (result != FD2_SCENE_RESULT_OK) break;
        if (notice.kind == FD2_FIELD_PROGRESS_STATUS || notice.kind == FD2_FIELD_PROGRESS_CLOSE) {
            result = progress_box_transition(&box, vga, 0, fast);
            if (result != FD2_SCENE_RESULT_OK) break;
            memcpy(vga->framebuffer, box.background, VGA_W * VGA_H);
            progress_box_free(&box);
            fd2_field_game_render(game, vga);
            if (!fast) fd2_vga_present(vga);
        }
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            break;
        }
        if (fd2_field_game_progress_ack(game) != 0) {
            result = FD2_SCENE_RESULT_ERROR;
            break;
        }
        if (notice.kind == FD2_FIELD_PROGRESS_GROWTH) box.row++;
    }
    if (result == FD2_SCENE_RESULT_OK && played) drain_frame_keys(vga);
    progress_box_free(&box);
    return result;
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

fd2_field_play_result fd2_field_play_run(fd2_vga *vga,
                       const fd2_archive *fdother,
                       size_t stage,
                       int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio,
                       const char *save_path,
                       int load_active) {
    return fd2_field_play_run_context(vga, fdother, stage, once, handoff,
        field_audio, save_path, load_active, NULL);
}

fd2_field_play_result fd2_field_play_run_context(fd2_vga *vga,
                       const fd2_archive *fdother, size_t stage, int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio, const char *save_path,
                       int load_active, fd2_field_play_context *context) {
    fd2_field_game game;
    if (!vga || !fdother || (context && !context->initialized))
        return FD2_FIELD_PLAY_RETURN_ERROR;
    int prepared_entry = context && context->has_preparation && !load_active;
    int opened = prepared_entry
        ? (context->preparation.stage_id != stage ? -1 :
            stage == 1u ? fd2_field_game_prepare_stage1(&game, fdother, &context->preparation) :
            stage == 2u ? fd2_field_game_prepare_stage2(&game, fdother, &context->preparation) : -1)
        : load_active ? fd2_field_game_open_resources(&game, fdother, stage)
        : fd2_field_game_open(&game, vga, fdother, stage);
    if (opened != 0) {
        fprintf(stderr, "cannot load stage %zu field game\n", stage);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    /* M6 已接入原版武器 record +0x0b/+0x0c 范围与 side==0 目标过滤。
     * loader 初值已捕获；原版每次进入战场控制循环前还会按计时器低
     * 8 位推进该流，活动快照未保存 RNG，因此读档不承诺续接同一序列。
     * profile 暴击基础值与武器 effect type 4 加值均使用已确认原版表。 */
    fd2_field_rng m6_rng;
    fd2_field_rng_seed(&m6_rng, field_rng_initial_seed());
    fd2_field_rng *runtime_rng = context ? &context->rng : &m6_rng;
    if (fd2_field_game_set_attack_hooks(
            &game, NULL, NULL, NULL, NULL,
            fd2_field_rng_next, runtime_rng) != 0) {
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    if (context) {
        game.experience_value = context->experience_value;
        game.message_number = context->message_number;
        if (fd2_field_game_set_music_request(&game, field_phase_music, context) != 0) {
            fd2_field_game_close(&game);
            return FD2_FIELD_PLAY_RETURN_ERROR;
        }
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
    if (load_active) {
        const uint8_t *palette;
        size_t size;
        if (fd2_archive_get(fdother, 0, &palette, &size) != 0 || size < 768) {
            fd2_field_game_close(&game);
            return FD2_FIELD_PLAY_RETURN_ERROR;
        }
        /* 标题CONTINUE先恢复共用调色板，活动快照不重播205da开场。 */
        fd2_vga_set_palette(vga, palette);
        fd2_vga_set_brightness(vga, 0);
    }
    if (prepared_entry) {
        int entry_result = stage == 1u
            ? fd2_scene_play_stage1_entry(vga, fdother, &game, field_audio, once)
            : fd2_scene_play_stage2_entry(vga, fdother, &game, field_audio, once);
        if (entry_result != FD2_SCENE_RESULT_OK) {
            fd2_field_game_close(&game);
            return entry_result == FD2_SCENE_RESULT_HOST_QUIT
                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
        }
        context->has_preparation = 0;
    }
    fd2_field_game_set_save_resource_available(
        &game, fd2_field_game_save_resource_available(&game, save_path));
    apply_field_audio_options(&game, field_audio);
    if (context) {
        const fd2_field_stage_music *music = fd2_field_stage_music_get(game.stage);
        if (!music || fd2_field_play_request_music(context, game.stage,
                music->primary, load_active ? "continue" : "entry") != 0) {
            fd2_field_game_close(&game);
            return FD2_FIELD_PLAY_RETURN_ERROR;
        }
    }
    if (!once && (fd2_field_game_set_progress_preflight(
            &game, progress_resources_preflight, (void *)fdother) != 0 ||
        fd2_field_game_set_loot_preflight(
            &game, loot_resources_preflight, (void *)fdother) != 0)) {
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }

    /* B6同步服务属于本轮Play，借用App的两个PCM handle与FDOTHER。
     * present只读业务，原11cac点经窄接口推进同一Game视觉锚；退出先解除借用再释放。 */
    fd2_battle_animation battle = {0};
    if (fd2_battle_animation_open(&battle, vga,
            field_audio ? field_audio->player : NULL,
            field_audio ? field_audio->secondary_player : NULL,
            fdother, &game.figani_archive, &game) != 0) {
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    fd2_battle_animation_set_skip(&battle, once);
    game.physical_present = fd2_battle_animation_physical_present;
    game.physical_present_userdata = &battle;
    game.magic_present = fd2_battle_animation_magic_present;
    game.magic_present_userdata = &battle;

    printf("field play: stage=%zu, map=%dx%d, units=%zu, camera=(%d,%d), "
           "cursor=(%d,%d), turn=%u, phase=%u, handoff=%s%s\n",
           game.stage, game.map.width, game.map.height, game.units.count,
           game.camera_cell_x, game.camera_cell_y,
           game.cursor_cell_x, game.cursor_cell_y,
           game.turn_number, game.active_side,
           handoff ? "yes" : "no",
           once ? ", once" : "");
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
        if (battle.failed || (context && context->music_error) || game.failure != FD2_FIELD_FAILURE_NONE) {
            report_session_failure(&game);
            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
            break;
        }
        if (!once) {
            int had_progress = game.loot.source || game.progress.notice.kind || game.progress.resume;
            int progress_result = play_field_progress(&game, vga, fdother, field_audio, 0);
            if (progress_result != FD2_SCENE_RESULT_OK) {
                play_result = progress_result == FD2_SCENE_RESULT_HOST_QUIT
                    ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                if (progress_result == FD2_SCENE_RESULT_ERROR)
                    fprintf(stderr, "战场奖励、状态提示或必需演出失败。\n");
                break;
            }
            if (had_progress) input_ready_reported = 0; /* 仍在实际present/quit检查后重报ready。 */
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
        int continuation_pending = game.loot.source != 0u;
        int automatic_continuation = continuation_pending &&
            game.loot.notice.kind == FD2_FIELD_LOOT_NONE;
        if (march_waiting_phase && !continuation_pending && (game.active_side == 2u ||
                game.battle_result != FD2_FIELD_BATTLE_ONGOING)) {
            drain_frame_keys(vga);
            march_waiting_phase = 0;
        }
        if (!once && !continuation_pending && game.battle_result != FD2_FIELD_BATTLE_ONGOING) {
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
        if (automatic_continuation || (game.active_side != 2 &&
            game.interaction == FD2_FIELD_INTERACTION_BROWSE &&
            game.phase_next_action_ms != 0 &&
            SDL_GetTicks() >= game.phase_next_action_ms)) {
            was_waiting_phase = game.player_post_waiting_phase;
            int ai_result = fd2_field_game_process_automatic_action_visual(
                &game, vga, 55, march_waiting_phase);
            if (was_waiting_phase && !game.player_post_waiting_phase)
                drain_frame_keys(vga);
            if (ai_result >= 0 && march_waiting_phase && !game.loot.source &&
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
                   &vga->input, field_input_context(&game),
                   &input_action)) {
            fd2_field_action action = field_action_from_input(input_action);
            if (game.detail_visible) {
                int result = play_field_detail_step(&game, vga, field_audio);
                end_input_frame = 1;
                if (result != FD2_SCENE_RESULT_OK) {
                    play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                        ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                    running = 0;
                    if (result == FD2_SCENE_RESULT_ERROR)
                        fprintf(stderr, "单位详情翻页或关闭失败。\n");
                } else if (!game.detail_visible && detail_controller_actor >= 0) {
                    int actor = detail_controller_actor;
                    detail_controller_actor = -1;
                    if (fd2_field_game_complete_player_controller(
                            &game, (size_t)actor) != 0) {
                        report_session_failure(&game);
                        play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                        running = 0;
                    }
                }
                continue;
            }
            if (game.active_side != 2 && action != FD2_FIELD_ACTION_EXIT)
                continue;
            if (game.interaction == FD2_FIELD_INTERACTION_ITEM_MENU &&
                (action == FD2_FIELD_ACTION_UP || action == FD2_FIELD_ACTION_DOWN ||
                 action == FD2_FIELD_ACTION_LEFT || action == FD2_FIELD_ACTION_RIGHT)) {
                fd2_field_command_direction direction = action == FD2_FIELD_ACTION_UP
                    ? FD2_FIELD_COMMAND_DIRECTION_UP : action == FD2_FIELD_ACTION_DOWN
                    ? FD2_FIELD_COMMAND_DIRECTION_DOWN : action == FD2_FIELD_ACTION_LEFT
                    ? FD2_FIELD_COMMAND_DIRECTION_LEFT : FD2_FIELD_COMMAND_DIRECTION_RIGHT;
                if (fd2_field_game_select_item_menu_direction(&game, direction) < 0) {
                    fprintf(stderr, "物品子菜单方向状态无效。\n");
                    play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                    running = 0;
                }
                continue;
            }
            switch (action) {
                case FD2_FIELD_ACTION_EXIT:
                    play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                    running = 0;
                    break;
                case FD2_FIELD_ACTION_CANCEL:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                        fd2_field_system_page page = game.system_menu.page;
                        /* Secondary/options cancel first closes the current
                         * radial page, then parent page opens again. Parent
                         * cancel closes the menu before returning to browse. */
                        int result = play_system_menu_close(&game, vga, field_audio);
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            break;
                        }
                        (void)fd2_field_game_cancel_system_menu(&game);
                        /* 只结束回到浏览的帧；新opening中采到的键仍归新菜单。 */
                        end_input_frame = game.interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU;
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
                        int result = play_command_transition(&game, vga, field_audio, 0);
                        if (result == FD2_SCENE_RESULT_OK && !fd2_field_game_cancel_selection(&game))
                            result = FD2_SCENE_RESULT_ERROR;
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                        }
                    } else if (game.interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
                               game.interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
                               game.interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING ||
                               game.interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING) {
                        int result = game.interaction == FD2_FIELD_INTERACTION_ITEM_MENU
                            ? play_field_item_menu_cancel(&game, vga, field_audio)
                            : game.interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING
                                ? fd2_field_game_cancel_item_give(&game) == 0
                                    ? play_command_transition(&game, vga, field_audio, 1)
                                    : FD2_SCENE_RESULT_ERROR
                            : fd2_field_game_cancel_item_targeting(&game) == 0
                                ? play_field_item_selection(&game, vga, field_audio)
                                : FD2_SCENE_RESULT_ERROR;
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            if (result == FD2_SCENE_RESULT_ERROR)
                                fprintf(stderr, "物品取消或列表重开失败。\n");
                        }
                    } else if (game.interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING) {
                        int result = fd2_field_game_cancel_magic_targeting(&game) == 0
                            ? play_field_magic_selection(&game, vga, field_audio)
                            : FD2_SCENE_RESULT_ERROR;
                        end_input_frame = 1;
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            if (result == FD2_SCENE_RESULT_ERROR)
                                fprintf(stderr, "法术目标取消或列表重开失败。\n");
                        }
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
                        int detail_result = fd2_field_game_open_detail(&game, (size_t)unit_at) != 0
                            ? -1 : fd2_field_game_animate_detail(
                                &game, vga, 1, detail_phase_sfx, field_audio);
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                        int host_quit = detail_result == 1 || fd2_input_take_quit(&vga->input);
                        if (detail_result < 0 || host_quit) {
                            if (detail_result < 0)
                                fprintf(stderr, "单位详情页资源无效。\n");
                            play_result = detail_result < 0
                                ? FD2_FIELD_PLAY_RETURN_ERROR : FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                            running = 0;
                        } else detail_controller_actor = -1;
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
                    if (game.interaction == FD2_FIELD_INTERACTION_TARGETING ||
                        game.interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
                        game.interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
                        game.interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING) {
                        int result = game.interaction == FD2_FIELD_INTERACTION_TARGETING
                            ? fd2_field_game_cycle_attack_target(&game)
                            : game.interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING
                            ? fd2_field_game_cycle_item_give_target(&game)
                            : game.interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING
                            ? fd2_field_game_cycle_item_target(&game)
                            : fd2_field_game_cycle_magic_target(&game);
                        if (result < 0) {
                            fprintf(stderr, "目标轮换状态无效。\n");
                            play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                        }
                    } else if (game.interaction == FD2_FIELD_INTERACTION_BROWSE) {
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
                            int detail_result = fd2_field_game_begin_player_controller(
                                    &game, (size_t)unit_at) != 0 ? -1 :
                                fd2_field_game_open_detail(
                                    &game, (size_t)unit_at) != 0 ? -1 :
                                fd2_field_game_animate_detail(&game, vga, 1,
                                    detail_phase_sfx, field_audio);
                            drain_frame_keys(vga);
                            end_input_frame = 1;
                            int host_quit = detail_result == 1 || fd2_input_take_quit(&vga->input);
                            if (detail_result < 0 || host_quit) {
                                if (detail_result < 0)
                                    fprintf(stderr, "单位详情页资源无效。\n");
                                play_result = detail_result < 0
                                    ? FD2_FIELD_PLAY_RETURN_ERROR : FD2_FIELD_PLAY_RETURN_HOST_QUIT;
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
                            FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                        /* open/input/close primitive 的原版边界：确认后先按
                         * 当前 command IDs 收起，再进入下一 dispatcher。
                         * confirmation page 独立绘制 FDTXT 与 YES/NO。 */
                        int result = play_system_menu_close(&game, vga, field_audio);
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            if (game.failure != FD2_FIELD_FAILURE_NONE) report_session_failure(&game);
                            running = 0;
                            break;
                        }
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
                                if (context) {
                                    const fd2_field_stage_music *music = fd2_field_stage_music_get(game.stage);
                                    if (!music || fd2_field_play_request_music(context,
                                            game.stage, music->primary, "active-load") != 0) {
                                        play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                                        running = 0;
                                        break;
                                    }
                                }
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
                        /* 结果文字会present新帧；回浏览时不将该帧交给下一层。
                         * 仍在SYSTEM的新opening保留自己的输入语义。 */
                        if (game.interaction == FD2_FIELD_INTERACTION_BROWSE) {
                            drain_frame_keys(vga);
                            end_input_frame = 1;
                        }
                        break;
                    }
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_AUXILIARY) {
                        game.interaction = FD2_FIELD_INTERACTION_BROWSE;
                        break;
                    }
                    if (previous_interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
                        (previous_interaction == FD2_FIELD_INTERACTION_COMMAND &&
                         game.command_selected == FD2_FIELD_COMMAND_ITEM)) {
                        int result = play_command_transition(&game, vga, field_audio, 0);
                        if (result != FD2_SCENE_RESULT_OK) {
                            if (result == FD2_SCENE_RESULT_ERROR)
                                fprintf(stderr, "物品菜单收起失败。\n");
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            end_input_frame = 1;
                            break;
                        }
                    } else if (previous_interaction == FD2_FIELD_INTERACTION_COMMAND) {
                        int result = play_command_transition(&game, vga, field_audio, 0);
                        end_input_frame = 1;
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            break;
                        }
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
                    if (battle.failed || game.failure != FD2_FIELD_FAILURE_NONE) {
                        report_session_failure(&game);
                        play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                        running = 0;
                        break;
                    }
                    if (previous_interaction == FD2_FIELD_INTERACTION_BROWSE &&
                        game.interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED) {
                        int result = play_field_move_selection_wait_key(&game, vga);
                        if (result != FD2_SCENE_RESULT_OK) {
                            if (result == FD2_SCENE_RESULT_ERROR) {
                                report_session_failure(&game);
                                fprintf(stderr, "单位初选状态板或等待失败。\n");
                            }
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT
                                : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            break;
                        }
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
                    if (game.interaction == FD2_FIELD_INTERACTION_MAGIC_LIST) {
                        int result = play_field_magic_selection(&game, vga, field_audio);
                        end_input_frame = 1;
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            if (result == FD2_SCENE_RESULT_ERROR)
                                fprintf(stderr, "法术列表或目标选择资源无效。\n");
                            break;
                        }
                    }
                    if (game.interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
                        game.interaction == FD2_FIELD_INTERACTION_ITEM_LIST ||
                        game.interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST ||
                        (game.interaction == FD2_FIELD_INTERACTION_COMMAND && unit_index >= 0 &&
                         previous_interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING)) {
                        int result = (game.interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
                                      game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                            ? play_command_transition(&game, vga, field_audio, 1)
                            : game.interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST
                                ? play_field_item_give_selection(&game, vga, field_audio)
                            : play_field_item_selection(&game, vga, field_audio);
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                        if (result != FD2_SCENE_RESULT_OK) {
                            play_result = result == FD2_SCENE_RESULT_HOST_QUIT
                                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
                            running = 0;
                            if (result == FD2_SCENE_RESULT_ERROR)
                                fprintf(stderr, "物品菜单、列表或目标选择资源无效。\n");
                            break;
                        }
                    }
                    if (action_unit >= 0 &&
                        (size_t)action_unit < game.units.count &&
                        (previous_interaction ==
                             FD2_FIELD_INTERACTION_COMMAND ||
                         previous_interaction ==
                             FD2_FIELD_INTERACTION_TARGETING ||
                         previous_interaction ==
                             FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
                         previous_interaction ==
                             FD2_FIELD_INTERACTION_ITEM_TARGETING ||
                         previous_interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING) &&
                        fd2_field_unit_has_acted(
                            &game.units.items[action_unit]) &&
                        game.selected_unit == -1) {
                        drain_frame_keys(vga);
                        end_input_frame = 1;
                        printf("field action: unit=%d command=%s acted=1 remaining=%zu phase=%u\n",
                               action_unit,
                               field_action_command_name(previous_interaction, action_command),
                               fd2_field_game_remaining_units(&game, 2u),
                               (unsigned)game.active_side);
                    }
                    /* AI结果不属于本次玩家确认，不能用本帧HP计算其累计伤害。 */
                    if (game.last_attack_valid &&
                        previous_interaction == FD2_FIELD_INTERACTION_TARGETING) {
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
                    }
                    game.last_attack_valid = 0;
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
            if (battle.failed || game.failure != FD2_FIELD_FAILURE_NONE) {
                report_session_failure(&game);
                play_result = FD2_FIELD_PLAY_RETURN_ERROR;
                running = 0;
            }
            if (game.player_post_waiting_phase) end_input_frame = 1;
        }
        fd2_delay_ms(16);
    }

    if (context) {
        /* 22ef6/22f37：本关整段对白/脚本成功后才交接下一准备owner。 */
        if (play_result == FD2_FIELD_PLAY_RETURN_VICTORY) {
            fd2_save_manual_state prepared;
            int scene_result = FD2_SCENE_RESULT_ERROR;
            if (game.stage != 0u && game.stage != 1u) {
                fprintf(stderr, "当前关卡结算尚未开放，已停止跨关（stage=%zu）。\n", game.stage);
            } else if (fd2_field_game_settlement_prepare(&game, &prepared) == 0 &&
                (game.stage != 0u || fd2_scene_field_story_validate(&game, 9) == 0) &&
                fd2_field_play_request_music(context, game.stage, -1, "victory") == 0) {
                scene_result = game.stage == 0u
                    ? fd2_scene_play_field_story(vga, fdother, &game, 9, field_audio, once)
                    : fd2_scene_play_stage1_victory(vga, fdother, &game, field_audio, once);
                if (scene_result == FD2_SCENE_RESULT_OK) {
                    if (fd2_field_game_settlement_commit(&game, &prepared) != 0)
                        scene_result = FD2_SCENE_RESULT_ERROR;
                    else {
                        context->preparation = prepared;
                        context->has_preparation = 1;
                        printf("field settlement: stage=%u persistent=%u resource=%u\n",
                            prepared.stage_id, prepared.unit_count, prepared.gold_or_flags);
                        fflush(stdout);
                    }
                }
            }
            if (scene_result != FD2_SCENE_RESULT_OK)
                play_result = scene_result == FD2_SCENE_RESULT_HOST_QUIT
                    ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
        } else if (play_result == FD2_FIELD_PLAY_RETURN_DEFEAT) {
            int scene_result = fd2_scene_play_game_over(vga, fdother, context->bgm, once);
            if (scene_result != FD2_SCENE_RESULT_OK)
                play_result = scene_result == FD2_SCENE_RESULT_HOST_QUIT
                    ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
        }
        context->experience_value = game.experience_value;
        context->message_number = game.message_number;
    }
    game.physical_present = NULL;
    game.physical_present_userdata = NULL;
    game.magic_present = NULL;
    game.magic_present_userdata = NULL;
    fd2_battle_animation_close(&battle);
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
