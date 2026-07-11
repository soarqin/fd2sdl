/* 炎龙骑士团 2 SDL3 重写 - 完整新游戏初始过场
 * 逆向依据：new_game_opening_play @0x2fa63 的 stage 32→31→0
 * 调用序列、text_dialog_render_tokens @0x136cc、field_focus_move_to
 * @0x10432、field_actor_move_up_follow_camera @0x108cd、
 * field_camera_pan_to @0x10d25、field_movement_script_play @0x10db2，
 * 以及原版实机对照。
 */

#include "scene.h"

#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "field.h"
#include "font.h"
#include "map_sprite.h"
#include "portrait.h"
#include "text.h"
#include "tile.h"

/* 完整新游戏开场由 new_game_opening_play @0x2fa63（code0 0x1fa63）驱动：
 * stage 32 / FDTXT[33] → stage 31 / FDTXT[32] → stage 0 / FDTXT[1]。
 * 三段之间穿插 field_camera_pan_to @0x10d25 与
 * field_movement_script_play @0x10db2，不能把 stage 0 fragment 0
 * 单独放到 stage 32 背景上播放。 */
#define FD2_OPENING_STAGE_32 32
#define FD2_OPENING_STAGE_31 31
#define FD2_OPENING_STAGE_0 0
#define FD2_FIELD_VIEW_X 4
#define FD2_FIELD_VIEW_Y 4
#define FD2_FIELD_VIEW_CELLS_X 13
#define FD2_FIELD_VIEW_CELLS_Y 8
#define FD2_FIELD_VIEW_W (FD2_FIELD_VIEW_CELLS_X * 24)
#define FD2_FIELD_VIEW_H (FD2_FIELD_VIEW_CELLS_Y * 24)
#define FD2_SCENE_MAX_UNIT_PORTRAITS 64
#define FD2_SCENE_MAX_ACTORS 64

#define DIALOG_X 5
#define DIALOG_W 310
#define DIALOG_H 86
#define DIALOG_TOP_Y 2
#define DIALOG_BOTTOM_Y 112

#define DIALOG_TOP_TEXT_X 15
#define DIALOG_TOP_TEXT_Y 9
#define DIALOG_BOTTOM_TEXT_X 95
#define DIALOG_BOTTOM_TEXT_Y 119
#define DIALOG_TOP_PORTRAIT_X 232
#define DIALOG_TOP_PORTRAIT_Y 5
#define DIALOG_BOTTOM_PORTRAIT_X 8
#define DIALOG_BOTTOM_PORTRAIT_Y 115

/* bios_tick_delay @0x151f1 读取 BIOS 计时器 0x046c；
 * text_dialog_render_tokens @0x136cc 每显示一个字调用一次，约 54.9 ms。
 * SDL 端取整为 55 ms。dialog_box_open @0x13cf4 的展开步间等待则
 * 调用 delay_ms(10)，不是 BIOS tick。 */
#define FD2_BIOS_TICK_MS 55
#define FD2_DIALOG_TRANSITION_DELAY_MS 10
#define FD2_TEXT_GLYPH_DELAY_MS FD2_BIOS_TICK_MS
#define FD2_DIALOG_PAGE_DELAY_MS 1500
/* field_animation_phase_update @0x100c5 仅在 BIOS tick 差值大于 4
 * 时推进一次角色 idle phase，即约每 5 tick / 275 ms 一帧。 */
#define FD2_IDLE_FRAME_DELAY_MS (FD2_BIOS_TICK_MS * 5)

/* FDTXT 控制码；FUN_000136cc @0x136cc 使用 signed u16 解释。 */
enum {
    FD2_TEXT_END = -1,
    FD2_TEXT_NEWLINE = -2,
    FD2_TEXT_PAGE = -3,
    FD2_TEXT_TOP_PORTRAIT = -17,
    FD2_TEXT_BOTTOM_PORTRAIT = -18,
    FD2_TEXT_TOP_UNIT_PORTRAIT = -19,
    FD2_TEXT_BOTTOM_UNIT_PORTRAIT = -20,
};

typedef enum {
    FD2_DIALOG_TOP,
    FD2_DIALOG_BOTTOM,
} fd2_dialog_area;

typedef struct {
    const uint8_t *data;
    size_t size;
    uint16_t count;
    const uint32_t *offsets;
} fd2_ui_sheet;

typedef struct {
    uint8_t sprite_group;
    uint8_t direction;
    uint8_t frame;
    uint8_t move_phase;
    uint8_t visible;
    int x;
    int y;
} fd2_scene_actor;

typedef struct {
    int camera_cell_x;
    int camera_cell_y;
    /* 角色贴近视窗边缘移动时，field_actor_move_up_follow_camera
     * @0x108cd 等方向函数会在 6 个步态相位中每次卷动 4 px，最后才
     * 提交一格镜头坐标。 */
    int camera_offset_x;
    int camera_offset_y;
    /* DAT_00003ab1/03ab5：战场焦点格。镜头平移保持二者相对位置，
     * 角色直接移动和对话定位则单独推进焦点。 */
    int focus_cell_x;
    int focus_cell_y;
    fd2_scene_actor actors[FD2_SCENE_MAX_ACTORS];
    size_t actor_count;
    /* FDTXT -19/-20 控制码按 DAT_00003a45 + unit_idx*0x50 + 7
     * 取 DATO 立绘编号。当前预览只构造已确认的单位索引，未知索引回退为
     * 直接 DATO 编号，等完整战场 actor 表复现后再补齐。 */
    uint8_t unit_portrait_known[FD2_SCENE_MAX_UNIT_PORTRAITS];
    uint8_t unit_portrait_id[FD2_SCENE_MAX_UNIT_PORTRAITS];
    /* actor record offset 0x08：-17/-18 用于查找说话角色的文本编号；
     * 与 offset 0x07 的 DATO 立绘编号分开保存。 */
    uint8_t unit_text_known[FD2_SCENE_MAX_UNIT_PORTRAITS];
    uint8_t unit_text_id[FD2_SCENE_MAX_UNIT_PORTRAITS];
    /* field_animation_phase_update @0x100c5 的全局 idle phase。静止 actor
     * 共用此相位；移动 actor 由 record offset 0x04 的步行相位覆盖。 */
    uint8_t idle_phase;
    int idle_elapsed_ms;
} fd2_scene_field_state;

typedef struct {
    fd2_dialog_area area;
    uint16_t portrait_id;
    int has_portrait;
    int is_open;
    int speaker_actor_idx;
    int x;
    int y;
    int line;
} fd2_dialog_state;

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int ui_sheet_open_fdother(fd2_ui_sheet *sheet,
                                 const fd2_archive *fdother,
                                 size_t entry_idx) {
    memset(sheet, 0, sizeof(*sheet));
    const uint8_t *data;
    size_t size;
    if (!fdother || fd2_archive_get(fdother, entry_idx, &data, &size) != 0)
        return -1;
    if (size < 10 || memcmp(data, "LMI1", 4) != 0)
        return -1;

    uint16_t count = rd_u16_le(data + 4);
    if (count == 0 || 6u + (size_t)count * 4u > size)
        return -1;

    for (uint16_t i = 0; i < count; i++) {
        uint32_t off = rd_u32_le(data + 6 + (size_t)i * 4u);
        if (off >= size) return -1;
        if (i > 0) {
            uint32_t prev = rd_u32_le(data + 6 + (size_t)(i - 1) * 4u);
            if (off <= prev) return -1;
        }
    }

    sheet->data = data;
    sheet->size = size;
    sheet->count = count;
    sheet->offsets = (const uint32_t *)(const void *)(data + 6);
    return 0;
}

static int ui_sheet_get_tile(const fd2_ui_sheet *sheet, uint16_t idx,
                             const uint8_t **pixels, int *w, int *h) {
    if (!sheet || !sheet->data || idx >= sheet->count) return -1;
    uint32_t off = rd_u32_le(sheet->data + 6 + (size_t)idx * 4u);
    uint32_t end = (idx + 1 < sheet->count)
                 ? rd_u32_le(sheet->data + 6 + (size_t)(idx + 1) * 4u)
                 : (uint32_t)sheet->size;
    if (end < off || end - off < 4) return -1;

    int tw = rd_u16_le(sheet->data + off);
    int th = rd_u16_le(sheet->data + off + 2);
    if (tw <= 0 || th <= 0) return -1;
    if ((size_t)(end - off - 4) != (size_t)tw * (size_t)th) return -1;

    if (pixels) *pixels = sheet->data + off + 4;
    if (w) *w = tw;
    if (h) *h = th;
    return 0;
}

static int poll_skip(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) return 1;
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_ESCAPE:
                case SDLK_RETURN:
                case SDLK_SPACE:
                    return 1;
                default:
                    break;
            }
        }
    }
    return 0;
}

static uint16_t text_token_signed(const uint8_t *tokens, size_t idx) {
    return fd2_text_token_at(tokens, idx);
}

static void blit_ui_tile_mode(fd2_vga *vga, const fd2_ui_sheet *ui,
                              uint16_t tile_idx, int x, int y,
                              int transparent_index) {
    const uint8_t *pixels;
    int w, h;
    if (!vga || ui_sheet_get_tile(ui, tile_idx, &pixels, &w, &h) != 0)
        return;

    for (int row = 0; row < h; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= VGA_H) continue;
        for (int col = 0; col < w; col++) {
            uint8_t px = pixels[(size_t)row * (size_t)w + (size_t)col];
            if (transparent_index >= 0 && px == (uint8_t)transparent_index)
                continue;
            int dx = x + col;
            if (dx < 0 || dx >= VGA_W) continue;
            vga->framebuffer[(size_t)dy * VGA_STRIDE + (size_t)dx] = px;
        }
    }
}

static void blit_ui_tile(fd2_vga *vga, const fd2_ui_sheet *ui,
                         uint16_t tile_idx, int x, int y) {
    /* 对照 FUN_00013fa4 @0x13fa4 / FUN_0004c453 @0x4c453：
     * FDOTHER[5] LMI1 小块是 4 字节宽高头 + 原始 8bpp 像素。 */
    blit_ui_tile_mode(vga, ui, tile_idx, x, y, -1);
}

static void draw_dialog_tiles(fd2_vga *vga, const fd2_ui_sheet *ui,
                              int x, int y, int tile_w, int tile_h) {
    /* 复现 FUN_00013ffe @0x13ffe 的布局：参数为 x/y、内部 16×16
     * 区块数 tile_w/tile_h；最终对话框调用为 x=5,y=2 或 0x70,
     * tile_w=19,tile_h=5，尺寸正好 310×86。 */
    int inner_w = tile_w - 2;
    int right_x = x + 3 + tile_w * 16;
    int bottom_y = y + 3 + tile_h * 16;

    blit_ui_tile(vga, ui, 1, x, y);
    blit_ui_tile(vga, ui, 2, right_x, y);
    blit_ui_tile(vga, ui, 3, x, bottom_y);
    blit_ui_tile(vga, ui, 4, right_x, bottom_y);

    blit_ui_tile(vga, ui, 5, x + 3, y);
    blit_ui_tile(vga, ui, 6, x + 0x13 + inner_w * 16, y);
    blit_ui_tile(vga, ui, 7, x + 3, bottom_y);
    blit_ui_tile(vga, ui, 8, x + 0x13 + inner_w * 16, bottom_y);

    blit_ui_tile(vga, ui, 0x0e, x, y + 3);
    blit_ui_tile(vga, ui, 0x0f, x + 0x23 + inner_w * 16, y + 3);
    blit_ui_tile(vga, ui, 0x10, x, y + 3 + 16 * (tile_h - 1));
    blit_ui_tile(vga, ui, 0x11, x + 0x23 + inner_w * 16,
                 y + 3 + 16 * (tile_h - 1));

    for (int i = 0; i < inner_w; i++) {
        int tx = x + 0x13 + i * 16;
        blit_ui_tile(vga, ui, 9, tx, y);
        blit_ui_tile(vga, ui, 0x0c, tx, bottom_y);
    }
    for (int j = 1; j < tile_h - 1; j++) {
        int ty = y + 3 + j * 16;
        blit_ui_tile(vga, ui, 0x0a, x, ty);
        blit_ui_tile(vga, ui, 0x0b, right_x, ty);
    }
    for (int row = 0; row < tile_h; row++) {
        for (int col = 0; col < tile_w; col++) {
            blit_ui_tile(vga, ui, 0x0d,
                         x + 3 + col * 16,
                         y + 3 + row * 16);
        }
    }
}

static void draw_dialog_box(fd2_vga *vga, const fd2_ui_sheet *ui,
                            fd2_dialog_area area) {
    int y = (area == FD2_DIALOG_TOP) ? DIALOG_TOP_Y : DIALOG_BOTTOM_Y;
    draw_dialog_tiles(vga, ui, DIALOG_X, y, 19, 5);
}

static uint16_t resolve_portrait_control(const fd2_scene_field_state *field_state,
                                         int16_t control,
                                         uint16_t arg) {
    if (!field_state) return arg;
    if ((control == FD2_TEXT_TOP_UNIT_PORTRAIT ||
         control == FD2_TEXT_BOTTOM_UNIT_PORTRAIT) &&
        arg < FD2_SCENE_MAX_UNIT_PORTRAITS &&
        field_state->unit_portrait_known[arg]) {
        return field_state->unit_portrait_id[arg];
    }
    if ((control == FD2_TEXT_TOP_PORTRAIT ||
         control == FD2_TEXT_BOTTOM_PORTRAIT) && arg != 0x27u) {
        /* field_visible_actor_find_by_text_id @0x103a8 即使只找到隐藏
         * actor，也保留其记录指针，供 token 路径读取 offset 0x07 立绘。 */
        for (size_t i = 0;
             i < field_state->actor_count &&
             i < FD2_SCENE_MAX_UNIT_PORTRAITS;
             i++) {
            if (field_state->unit_text_known[i] &&
                field_state->unit_text_id[i] == (uint8_t)arg &&
                field_state->unit_portrait_known[i])
                return field_state->unit_portrait_id[i];
        }
    }
    return arg;
}

static int resolve_speaker_actor(const fd2_scene_field_state *field_state,
                                 int16_t control,
                                 uint16_t arg) {
    if (!field_state) return -1;
    if (control == FD2_TEXT_TOP_UNIT_PORTRAIT ||
        control == FD2_TEXT_BOTTOM_UNIT_PORTRAIT) {
        return arg < field_state->actor_count ? (int)arg : -1;
    }

    /* -17/-18 的下一 token 是 actor record offset 0x08 文本编号，
     * field_visible_actor_find_by_text_id @0x103a8 只返回未隐藏角色；
     * 隐藏角色仍可通过其记录的 offset 0x07 显示立绘，但对话框不从
     * 旧地图坐标弹出。 */
    for (size_t i = 0;
         i < field_state->actor_count && i < FD2_SCENE_MAX_UNIT_PORTRAITS;
         i++) {
        if (field_state->actors[i].visible &&
            field_state->unit_text_known[i] &&
            field_state->unit_text_id[i] == (uint8_t)arg)
            return (int)i;
    }
    return -1;
}

static void draw_portrait(fd2_vga *vga, const fd2_archive *dato,
                          fd2_dialog_area area, uint16_t portrait_id) {
    fd2_image portrait;
    if (fd2_portrait_decode_frame(&portrait, dato, portrait_id, 0) != 0)
        return;

    int x = (area == FD2_DIALOG_TOP) ? DIALOG_TOP_PORTRAIT_X : DIALOG_BOTTOM_PORTRAIT_X;
    int y = (area == FD2_DIALOG_TOP) ? DIALOG_TOP_PORTRAIT_Y : DIALOG_BOTTOM_PORTRAIT_Y;
    fd2_tileset_blit(vga, &portrait, x, y, -1);
    fd2_image_free(&portrait);
}

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static uint8_t opening_sprite_group(uint16_t unit_id) {
    /* DAT_00003a45 offset 2 是 FD2.TMP 的运行期 cache class，不是稳定的
     * 职业号。fdicon_cache_append_unit @0xe761 按 actor offset 7 的 unit id 从
     * FDICON.B24 复制 12 帧并重写 cache class。SDL 版直接读取完整的
     * FDICON.B24，因此帧组就是 unit_id，避免跨 stage 沿用错误 TMP。 */
    return unit_id <= 0xffu ? (uint8_t)unit_id : 0;
}

static void opening_actor_set(fd2_scene_field_state *state,
                              size_t actor_idx,
                              uint16_t unit_id,
                              int x,
                              int y) {
    if (!state || actor_idx >= FD2_SCENE_MAX_ACTORS) return;
    fd2_scene_actor *actor = &state->actors[actor_idx];
    actor->sprite_group = opening_sprite_group(unit_id);
    actor->direction = 0;
    actor->frame = 1;
    actor->move_phase = 0;
    actor->visible = 1;
    actor->x = x;
    actor->y = y;
    if (actor_idx < FD2_SCENE_MAX_UNIT_PORTRAITS) {
        state->unit_portrait_known[actor_idx] = 1;
        state->unit_portrait_id[actor_idx] = (uint8_t)unit_id;
        /* 新游戏开场实际使用的角色，record offset 0x08 文本编号与
         * offset 0x07 unit/DATO 编号相同；仍分字段保存，避免把
         * field_visible_actor_find_by_text_id 的比较对象误写成立绘字段。 */
        state->unit_text_known[actor_idx] = 1;
        state->unit_text_id[actor_idx] = (uint8_t)unit_id;
    }
    if (state->actor_count <= actor_idx)
        state->actor_count = actor_idx + 1;
}

static void init_opening_field_state(fd2_scene_field_state *state,
                                     size_t stage,
                                     const fd2_field_map *map,
                                     const fd2_field_placements *placements) {
    memset(state, 0, sizeof(*state));
    if (!map || !placements || !placements->records) return;

    if (stage == FD2_OPENING_STAGE_0) {
        /* FUN_0002fa63 在进入 stage 0 前依次加入单位 0/9/4/30。
         * 运行时 actor 表先放四名玩家，再放关卡模板；因此移动脚本 0
         * 操作 actor 0..3，脚本 1/2 操作 actor 4..11。 */
        static const uint8_t player_ids[] = {0, 9, 4, 30};
        size_t player_base = placements->count >= 4 ? placements->count - 4 : 0;
        for (size_t i = 0; i < 4 && player_base + i < placements->count; i++) {
            const fd2_field_placement *p = &placements->records[player_base + i];
            opening_actor_set(state, i, player_ids[i], p->x, p->y);
        }
        for (size_t i = 0; i < 8 && i < placements->count; i++) {
            const fd2_field_placement *p = &placements->records[i];
            opening_actor_set(state, 4 + i, p->unit_id, p->x, p->y);
            /* field_actor_group_arrival_effect @0x300e1 先按组加入 actor，再播放 12 帧登场
             * 特效；进入 stage 0 时这两组不能提前出现在地图上。 */
            state->actors[4 + i].visible = 0;
        }
        return;
    }

    size_t keep = stage == FD2_OPENING_STAGE_32 ? placements->count : 5;
    if (keep > placements->count) keep = placements->count;
    for (size_t i = 0; i < keep; i++) {
        const fd2_field_placement *p = &placements->records[i];
        /* stage 32 的 actor 5..20 是走廊两侧卫兵，属于开场可见内容；
         * 后续 (0,0) 的 0x60 槽是战斗占位，不在这段过场显示。 */
        if (stage == FD2_OPENING_STAGE_32 && p->x == 0 && p->y == 0)
            break;
        opening_actor_set(state, i, p->unit_id, p->x, p->y);
        /* stage 31 的模板 offset 0x15 将 actor 0/1、2/3、4 分到
         * group 1/3/5。new_game_opening_play @0x2fa63 依次调用
         * func_0x0000e296(1/3/5)，不能在场景加载时一次显示五人。 */
        if (stage == FD2_OPENING_STAGE_31 && i >= 2)
            state->actors[i].visible = 0;
    }
    /* func_0x0000e296 会在各时点扩展 DAT_00003beb actor count；预填的
     * group 3/5 槽在激活前也不能参与文本编号查找。 */
    if (stage == FD2_OPENING_STAGE_31 && state->actor_count > 2)
        state->actor_count = 2;
}

static void opening_camera_pixels(const fd2_field_map *map,
                                  const fd2_scene_field_state *field_state,
                                  int *out_x,
                                  int *out_y) {
    int world_x = field_state->camera_cell_x * 24 +
                  field_state->camera_offset_x;
    int world_y = field_state->camera_cell_y * 24 +
                  field_state->camera_offset_y;
    int max_x = map ? map->width * 24 - FD2_FIELD_VIEW_W : world_x;
    int max_y = map ? map->height * 24 - FD2_FIELD_VIEW_H : world_y;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    world_x = clamp_int(world_x, 0, max_x);
    world_y = clamp_int(world_y, 0, max_y);
    /* tile renderer 的目标原点是 (0,0)，传入 world-4 可让镜头左上格
     * 正好落在原版战场内区的 (4,4)。 */
    if (out_x) *out_x = world_x - FD2_FIELD_VIEW_X;
    if (out_y) *out_y = world_y - FD2_FIELD_VIEW_Y;
}

static uint8_t opening_actor_frame_phase(
        const fd2_scene_field_state *field_state,
        const fd2_scene_actor *actor) {
    if (actor->move_phase != 0)
        return actor->frame;
    /* map_actor_blit_24x24 @0xb168：record offset 0x04 为 0 时使用
     * 全局 DAT_00003c0b；phase 3 映射回 phase 1。 */
    return field_state->idle_phase == 3 ? 1u : field_state->idle_phase;
}

static void render_prologue_actors(fd2_vga *vga,
                                   const fd2_field_map *map,
                                   const fd2_map_sprite_bank *sprites,
                                   const fd2_scene_field_state *field_state) {
    if (!vga || !sprites || !field_state) return;

    size_t order[FD2_SCENE_MAX_ACTORS];
    size_t order_count = 0;
    for (size_t i = 0; i < field_state->actor_count && i < FD2_SCENE_MAX_ACTORS; i++) {
        const fd2_scene_actor *actor = &field_state->actors[i];
        if (!actor->visible) continue;
        uint8_t frame_phase = opening_actor_frame_phase(field_state, actor);
        size_t frame_idx = (size_t)actor->sprite_group * 12u +
                           (size_t)actor->direction * 3u +
                           (size_t)frame_phase;
        if (frame_idx >= sprites->frame_count) continue;

        size_t pos = order_count++;
        order[pos] = i;
        while (pos > 0) {
            const fd2_scene_actor *a = &field_state->actors[order[pos - 1]];
            const fd2_scene_actor *b = &field_state->actors[order[pos]];
            if (a->y < b->y || (a->y == b->y && a->x <= b->x)) break;
            size_t tmp = order[pos - 1];
            order[pos - 1] = order[pos];
            order[pos] = tmp;
            pos--;
        }
    }

    int camera_x = 0;
    int camera_y = 0;
    opening_camera_pixels(map, field_state, &camera_x, &camera_y);
    for (size_t oi = 0; oi < order_count; oi++) {
        const fd2_scene_actor *actor = &field_state->actors[order[oi]];
        uint8_t frame_phase = opening_actor_frame_phase(field_state, actor);
        size_t frame_idx = (size_t)actor->sprite_group * 12u +
                           (size_t)actor->direction * 3u +
                           (size_t)frame_phase;
        int sx = actor->x * 24 - camera_x;
        int sy = actor->y * 24 - camera_y - 6;
        /* map_actor_blit_24x24 @0xb168 以 record+4 的 1..6 相位，
         * 每帧沿方向移动 4 px；第 6 帧正好跨过一个 24 px 格。 */
        int pixel_step = (int)actor->move_phase * 4;
        switch (actor->direction & 3u) {
            case 0: sy += pixel_step; break;
            case 1: sx -= pixel_step; break;
            case 2: sy -= pixel_step; break;
            case 3: sx += pixel_step; break;
        }
        fd2_map_sprite_blit_frame(vga, sprites, frame_idx, sx, sy, 0);
    }
}

static void clear_field_view_border(fd2_vga *vga) {
    if (!vga) return;
    /* field_view_render_from_cache @0x1cca0 先清空 320×200 缓冲区，再只写
     * VGA offset 0x504（4,4）起的 312×192 战场内区。四边 4 px 因此
     * 保持调色板 index 0 的黑边，不能用相邻地图格填充。 */
    for (int y = 0; y < VGA_H; y++) {
        uint8_t *row = vga->framebuffer + (size_t)y * VGA_STRIDE;
        if (y < 4 || y >= VGA_H - 4) {
            memset(row, 0, VGA_W);
        } else {
            memset(row, 0, 4);
            memset(row + VGA_W - 4, 0, 4);
        }
    }
}

static void render_scene_base(fd2_vga *vga,
                              const fd2_terrain_tileset *terrain,
                              const fd2_field_map *map,
                              const fd2_map_sprite_bank *sprites,
                              const fd2_scene_field_state *field_state) {
    int camera_x = 0;
    int camera_y = 0;
    /* 战场视窗只使用 VGA (4,4) 起的 312×192 内区。先按现有坐标绘制
     * 完整合成画面，再恢复原版四边 4 px 黑边。 */
    opening_camera_pixels(map, field_state, &camera_x, &camera_y);
    fd2_terrain_render_field_base(vga, terrain, map, camera_x, camera_y);
    render_prologue_actors(vga, map, sprites, field_state);
    /* map_scene_render_actors @0xa2e6 后由 FUN_00010134 调用
     * map_tile_blit_visible @0x1020e 重绘遮挡格。 */
    fd2_terrain_render_field_overlay(vga, terrain, map, camera_x, camera_y, 0);
    clear_field_view_border(vga);
}

static void set_camera_cell(fd2_scene_field_state *field_state,
                            const fd2_field_map *map,
                            int cell_x,
                            int cell_y) {
    if (!field_state) return;
    int max_cam_x = map ? map->width - FD2_FIELD_VIEW_CELLS_X : cell_x;
    int max_cam_y = map ? map->height - FD2_FIELD_VIEW_CELLS_Y : cell_y;
    if (max_cam_x < 0) max_cam_x = 0;
    if (max_cam_y < 0) max_cam_y = 0;
    field_state->camera_cell_x = clamp_int(cell_x, 0, max_cam_x);
    field_state->camera_cell_y = clamp_int(cell_y, 0, max_cam_y);
    field_state->camera_offset_x = 0;
    field_state->camera_offset_y = 0;
}

static void present_base_frame(fd2_vga *vga,
                               const fd2_terrain_tileset *terrain,
                               const fd2_field_map *map,
                               const fd2_map_sprite_bank *sprites,
                               const fd2_scene_field_state *field_state) {
    render_scene_base(vga, terrain, map, sprites, field_state);
    fd2_vga_present(vga);
}

static void present_base_frame_timed(fd2_vga *vga,
                                     const fd2_terrain_tileset *terrain,
                                     const fd2_field_map *map,
                                     const fd2_map_sprite_bank *sprites,
                                     const fd2_scene_field_state *field_state,
                                     uint32_t frame_ms) {
    render_scene_base(vga, terrain, map, sprites, field_state);
    fd2_vga_present_timed(vga, frame_ms);
}

static void opening_advance_idle(fd2_scene_field_state *field_state,
                                 int elapsed_ms) {
    if (!field_state || elapsed_ms <= 0) return;
    field_state->idle_elapsed_ms += elapsed_ms;
    while (field_state->idle_elapsed_ms >= FD2_IDLE_FRAME_DELAY_MS) {
        field_state->idle_elapsed_ms -= FD2_IDLE_FRAME_DELAY_MS;
        field_state->idle_phase = (uint8_t)((field_state->idle_phase + 1u) & 3u);
    }
}

static int dialog_area_y(fd2_dialog_area area) {
    return area == FD2_DIALOG_TOP ? DIALOG_TOP_Y : DIALOG_BOTTOM_Y;
}

static int dialog_popup_source(const fd2_field_map *map,
                               const fd2_scene_field_state *field_state,
                               int speaker_actor_idx,
                               int *out_x,
                               int *out_y,
                               int *out_steps) {
    if (!field_state || speaker_actor_idx < 0 ||
        (size_t)speaker_actor_idx >= field_state->actor_count)
        return -1;

    const fd2_scene_actor *actor = &field_state->actors[speaker_actor_idx];

    int camera_x = 0;
    int camera_y = 0;
    opening_camera_pixels(map, field_state, &camera_x, &camera_y);
    int source_x = actor->x * 24 - camera_x;
    int source_y = actor->y * 24 - camera_y;
    if (source_x < -24 || source_y < -24 || source_x >= VGA_W || source_y >= VGA_H)
        return -1;

    if (out_x) *out_x = source_x;
    if (out_y) *out_y = source_y;
    if (out_steps) {
        int steps = source_x / 24 + source_y / 24;
        *out_steps = steps > 0 ? steps : 1;
    }
    return 0;
}

static void animate_dialog_marker(fd2_vga *vga,
                                  const fd2_terrain_tileset *terrain,
                                  const fd2_field_map *map,
                                  const fd2_ui_sheet *ui,
                                  const fd2_map_sprite_bank *sprites,
                                  const fd2_scene_field_state *field_state,
                                  int speaker_actor_idx,
                                  fd2_dialog_area area,
                                  int reverse) {
    int source_x, source_y, steps;
    if (dialog_popup_source(map, field_state, speaker_actor_idx,
                            &source_x, &source_y, &steps) != 0)
        return;

    int target_x = DIALOG_X;
    int target_y = dialog_area_y(area);
    for (int frame = 0; frame <= steps; frame++) {
        int t = reverse ? steps - frame : frame;
        int x = source_x + (target_x - source_x) * t / steps;
        int y = source_y + (target_y - source_y) * t / steps;
        /* 原版弹框期间不调用 field_animation_phase_update；背景角色保持
         * 进入对话时的共享 idle phase。 */
        render_scene_base(vga, terrain, map, sprites, field_state);
        /* FDOTHER[5] tile 0 是 24×24 空心框。FUN_000135e6 @0x135e6
         * 在 dialog_box_open @0x13cf4 中将它从角色格移动到对话框原点；
         * 0x4a 是该帧背景色。 */
        blit_ui_tile_mode(vga, ui, 0, x, y, 0x4a);
        fd2_vga_present_timed(vga, FD2_DIALOG_TRANSITION_DELAY_MS);
        if (poll_skip()) break;
    }
}

static void animate_dialog_box_open(fd2_vga *vga,
                                    const fd2_terrain_tileset *terrain,
                                    const fd2_field_map *map,
                                    const fd2_ui_sheet *ui,
                                    const fd2_map_sprite_bank *sprites,
                                    const fd2_scene_field_state *field_state,
                                    int speaker_actor_idx,
                                    fd2_dialog_area area,
                                    int fast) {
    if (fast) return;

    animate_dialog_marker(vga, terrain, map, ui, sprites, field_state,
                          speaker_actor_idx, area, 0);

    /* dialog_box_open @0x13cf4 依次用 4×2、8×3、12×4、16×5、
     * 19×5 个内部块绘制同一对话框，形成原版的伪缩放弹出效果。 */
    static const uint8_t stages[][2] = {
        {4, 2}, {8, 3}, {12, 4}, {16, 5}, {19, 5},
    };
    for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
        render_scene_base(vga, terrain, map, sprites, field_state);
        draw_dialog_tiles(vga, ui, DIALOG_X, dialog_area_y(area),
                          stages[i][0], stages[i][1]);
        if (i + 1 < sizeof(stages) / sizeof(stages[0]))
            fd2_vga_present_timed(vga, FD2_DIALOG_TRANSITION_DELAY_MS);
        else
            fd2_vga_present(vga);
        if (poll_skip()) break;
    }
}

/* dialog_box_close @0x1428b：倒序恢复五级弹框背景，各级等待 10 ms；
 * 若对话框来自可见角色，再把 24×24 空心框移回角色格。 */
static void animate_dialog_box_close(fd2_vga *vga,
                                     const fd2_terrain_tileset *terrain,
                                     const fd2_field_map *map,
                                     const fd2_ui_sheet *ui,
                                     const fd2_map_sprite_bank *sprites,
                                     const fd2_scene_field_state *field_state,
                                     const fd2_dialog_state *state,
                                     int fast) {
    if (!state || !state->is_open) return;

    if (!fast) {
        static const uint8_t stages[][2] = {
            {16, 5}, {12, 4}, {8, 3}, {4, 2},
        };
        for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
            render_scene_base(vga, terrain, map, sprites, field_state);
            draw_dialog_tiles(vga, ui, DIALOG_X, dialog_area_y(state->area),
                              stages[i][0], stages[i][1]);
            fd2_vga_present_timed(vga, FD2_DIALOG_TRANSITION_DELAY_MS);
            if (poll_skip()) break;
        }
        animate_dialog_marker(vga, terrain, map, ui, sprites, field_state,
                              state->speaker_actor_idx, state->area, 1);
    }
    present_base_frame(vga, terrain, map, sprites, field_state);
}

static void pan_camera_to(fd2_vga *vga,
                          const fd2_terrain_tileset *terrain,
                          const fd2_field_map *map,
                          const fd2_map_sprite_bank *sprites,
                          fd2_scene_field_state *field_state,
                          int target_x,
                          int target_y,
                          int fast) {
    if (!field_state) return;
    int max_cam_x = map ? map->width - FD2_FIELD_VIEW_CELLS_X : target_x;
    int max_cam_y = map ? map->height - FD2_FIELD_VIEW_CELLS_Y : target_y;
    if (max_cam_x < 0) max_cam_x = 0;
    if (max_cam_y < 0) max_cam_y = 0;
    target_x = clamp_int(target_x, 0, max_cam_x);
    target_y = clamp_int(target_y, 0, max_cam_y);

    if (fast) {
        field_state->focus_cell_x += target_x - field_state->camera_cell_x;
        field_state->focus_cell_y += target_y - field_state->camera_cell_y;
        set_camera_cell(field_state, map, target_x, target_y);
        present_base_frame(vga, terrain, map, sprites, field_state);
        return;
    }

    /* field_camera_pan_to @0x10d25 先完整调整 X，再调整 Y；每格只等待
     * 一次 vsync。确认键不打断镜头，留给随后对话消费。 */
    while (field_state->camera_cell_x != target_x) {
        int cx = field_state->camera_cell_x;
        int dx = cx < target_x ? 1 : -1;
        cx += dx;
        field_state->focus_cell_x += dx;
        set_camera_cell(field_state, map, cx, field_state->camera_cell_y);
        present_base_frame_timed(vga, terrain, map, sprites,
                                 field_state, 17);
        opening_advance_idle(field_state, 17);
    }
    while (field_state->camera_cell_y != target_y) {
        int cy = field_state->camera_cell_y;
        int dy = cy < target_y ? 1 : -1;
        cy += dy;
        field_state->focus_cell_y += dy;
        set_camera_cell(field_state, map, field_state->camera_cell_x, cy);
        present_base_frame_timed(vga, terrain, map, sprites,
                                 field_state, 17);
        opening_advance_idle(field_state, 17);
    }
}

static void focus_camera_on_actor(fd2_vga *vga,
                                  const fd2_terrain_tileset *terrain,
                                  const fd2_field_map *map,
                                  const fd2_map_sprite_bank *sprites,
                                  fd2_scene_field_state *field_state,
                                  int actor_idx,
                                  int fast) {
    if (!field_state || actor_idx < 0 ||
        (size_t)actor_idx >= field_state->actor_count)
        return;

    const fd2_scene_actor *actor = &field_state->actors[actor_idx];

    int max_cam_x = map ? map->width - FD2_FIELD_VIEW_CELLS_X : 0;
    int max_cam_y = map ? map->height - FD2_FIELD_VIEW_CELLS_Y : 0;
    if (max_cam_x < 0) max_cam_x = 0;
    if (max_cam_y < 0) max_cam_y = 0;

    /* dialog_box_open @0x13cf4 在从角色格展开对话框前自动调用
     * field_focus_move_to @0x10432；按 actor 索引定位的等价包装为
     * field_focus_move_to_actor @0x104c3。前者先逐格移动 X、再移动 Y；焦点
     * 向外达到 x=2/11、y=2/6 阈值后才同步卷动镜头。最终镜头位置
     * 取决于此前焦点，不等同于直接把角色固定到某个屏幕格。
     * 这不是额外的文本控制码，也不需要过场显式调用镜头指令。 */
    while (field_state->focus_cell_x != actor->x) {
        int dx = field_state->focus_cell_x < actor->x ? 1 : -1;
        int rel_x = field_state->focus_cell_x - field_state->camera_cell_x;
        int camera_x = field_state->camera_cell_x;
        if (dx < 0 && rel_x < 2 && camera_x > 0)
            camera_x--;
        else if (dx > 0 && rel_x >= 11 && camera_x < max_cam_x)
            camera_x++;
        field_state->focus_cell_x += dx;
        set_camera_cell(field_state, map, camera_x,
                        field_state->camera_cell_y);
        if (!fast) {
            present_base_frame_timed(vga, terrain, map, sprites,
                                     field_state, 17);
            opening_advance_idle(field_state, 17);
        }
    }
    while (field_state->focus_cell_y != actor->y) {
        int dy = field_state->focus_cell_y < actor->y ? 1 : -1;
        int rel_y = field_state->focus_cell_y - field_state->camera_cell_y;
        int camera_y = field_state->camera_cell_y;
        if (dy < 0 && rel_y < 2 && camera_y > 0)
            camera_y--;
        else if (dy > 0 && rel_y >= 6 && camera_y < max_cam_y)
            camera_y++;
        field_state->focus_cell_y += dy;
        set_camera_cell(field_state, map, field_state->camera_cell_x,
                        camera_y);
        if (!fast) {
            present_base_frame_timed(vga, terrain, map, sprites,
                                     field_state, 17);
            opening_advance_idle(field_state, 17);
        }
    }

    if (fast)
        present_base_frame(vga, terrain, map, sprites, field_state);
}

static void redraw_dialog(fd2_vga *vga,
                          const fd2_terrain_tileset *terrain,
                          const fd2_field_map *map,
                          const fd2_archive *dato,
                          const fd2_ui_sheet *ui,
                          const fd2_map_sprite_bank *sprites,
                          const fd2_scene_field_state *field_state,
                          fd2_dialog_state *state) {
    render_scene_base(vga, terrain, map, sprites, field_state);
    draw_dialog_box(vga, ui, state->area);
    state->is_open = 1;
    if (state->has_portrait)
        draw_portrait(vga, dato, state->area, state->portrait_id);

    if (state->area == FD2_DIALOG_TOP) {
        state->x = DIALOG_TOP_TEXT_X;
        state->y = DIALOG_TOP_TEXT_Y;
    } else {
        state->x = DIALOG_BOTTOM_TEXT_X;
        state->y = DIALOG_BOTTOM_TEXT_Y;
    }
    state->line = 0;
}

static void wait_or_skip(int ms, int *skip) {
    int elapsed = 0;
    while (!*skip && elapsed < ms) {
        if (poll_skip()) { *skip = 1; break; }
        fd2_delay_ms(20);
        elapsed += 20;
    }
}

static void page_pause(fd2_vga *vga, int *skip) {
    fd2_vga_present(vga);
    wait_or_skip(FD2_DIALOG_PAGE_DELAY_MS, skip);
}

static void newline_or_page(fd2_vga *vga,
                            const fd2_terrain_tileset *terrain,
                            const fd2_field_map *map,
                            const fd2_archive *dato,
                            const fd2_ui_sheet *ui,
                            const fd2_map_sprite_bank *sprites,
                            const fd2_scene_field_state *field_state,
                            fd2_dialog_state *state,
                            int force_page,
                            int fast,
                            int *skip) {
    if (force_page || state->line >= 3) {
        if (!fast)
            page_pause(vga, skip);
        if (!*skip)
            redraw_dialog(vga, terrain, map, dato, ui, sprites, field_state, state);
        return;
    }

    state->line++;
    state->x = (state->area == FD2_DIALOG_TOP) ? DIALOG_TOP_TEXT_X : DIALOG_BOTTOM_TEXT_X;
    state->y += 16;
}

static void set_dialog_area(fd2_vga *vga,
                            const fd2_terrain_tileset *terrain,
                            const fd2_field_map *map,
                            const fd2_archive *dato,
                            const fd2_ui_sheet *ui,
                            const fd2_map_sprite_bank *sprites,
                            fd2_scene_field_state *field_state,
                            fd2_dialog_state *state,
                            fd2_dialog_area area,
                            uint16_t portrait_id,
                            int speaker_actor_idx,
                            int fast) {
    if (state->is_open)
        animate_dialog_box_close(vga, terrain, map, ui, sprites, field_state,
                                 state, fast);

    focus_camera_on_actor(vga, terrain, map, sprites, field_state,
                          speaker_actor_idx, fast);
    state->area = area;
    state->portrait_id = portrait_id;
    state->has_portrait = 1;
    state->speaker_actor_idx = speaker_actor_idx;
    animate_dialog_box_open(vga, terrain, map, ui, sprites, field_state,
                            speaker_actor_idx, area, fast);
    redraw_dialog(vga, terrain, map, dato, ui, sprites, field_state, state);
    fd2_vga_present(vga);
}

static int play_text_fragment(fd2_vga *vga,
                              const fd2_terrain_tileset *terrain,
                              const fd2_field_map *map,
                              const fd2_archive *dato,
                              const fd2_ui_sheet *ui,
                              const fd2_map_sprite_bank *sprites,
                              fd2_scene_field_state *field_state,
                              const fd2_font *font,
                              const fd2_text_entry *text,
                              size_t fragment_idx,
                              int fast) {
    const uint8_t *tokens;
    size_t token_count;
    if (fd2_text_entry_get_fragment(text, fragment_idx, &tokens, &token_count) != 0)
        return -1;

    fd2_dialog_state state;
    memset(&state, 0, sizeof(state));
    state.area = FD2_DIALOG_BOTTOM;
    state.speaker_actor_idx = -1;
    render_scene_base(vga, terrain, map, sprites, field_state);
    fd2_vga_present(vga);

    int skip = 0;
    for (size_t i = 0; i < token_count && !skip; i++) {
        int16_t tok = (int16_t)text_token_signed(tokens, i);
        if (tok == FD2_TEXT_END) break;

        if (tok == FD2_TEXT_TOP_PORTRAIT || tok == FD2_TEXT_BOTTOM_PORTRAIT ||
            tok == FD2_TEXT_TOP_UNIT_PORTRAIT || tok == FD2_TEXT_BOTTOM_UNIT_PORTRAIT) {
            if (i + 1 >= token_count) break;
            uint16_t portrait_id = text_token_signed(tokens, ++i);
            fd2_dialog_area area = (tok == FD2_TEXT_TOP_PORTRAIT ||
                                    tok == FD2_TEXT_TOP_UNIT_PORTRAIT)
                                  ? FD2_DIALOG_TOP : FD2_DIALOG_BOTTOM;
            int speaker_actor_idx = resolve_speaker_actor(field_state, tok,
                                                          portrait_id);
            portrait_id = resolve_portrait_control(field_state, tok, portrait_id);
            set_dialog_area(vga, terrain, map, dato, ui, sprites, field_state,
                            &state, area, portrait_id, speaker_actor_idx, fast);
            if (!fast) wait_or_skip(250, &skip);
            continue;
        }

        if (tok == FD2_TEXT_NEWLINE || tok == FD2_TEXT_PAGE) {
            newline_or_page(vga, terrain, map, dato, ui, sprites, field_state,
                            &state, tok == FD2_TEXT_PAGE, fast, &skip);
            fd2_vga_present(vga);
            continue;
        }

        if (tok < 0) {
            /* -4/-5/-6 等动态嵌入码后续按 FUN_000136cc 补齐；
             * 当前先跳过，避免破坏自动过场播放。 */
            continue;
        }

        fd2_font_draw_glyph(vga, font, (uint16_t)tok, state.x, state.y,
                            0xcd, 0x4c, -1);
        /* FUN_000136cc @0x136cc 不做按对话框宽度自动折行，只在
         * FDTXT token -2/-3 出现时推进一行。此前这里额外自动折行，
         * 会让恰好排到行尾后紧跟 -2 的文本一次换两行。 */
        state.x += 16;
        fd2_vga_present(vga);
        if (!fast) wait_or_skip(FD2_TEXT_GLYPH_DELAY_MS, &skip);
    }

    if (!fast && !skip)
        page_pause(vga, &skip);
    animate_dialog_box_close(vga, terrain, map, ui, sprites, field_state,
                             &state, fast);
    state.is_open = 0;
    return 0;
}

/* DAT_000027d8 中新游戏开场会用到的移动脚本。字节流按
 * field_movement_script_play @0x10db2 的原始格式登记：group_count，
 * 然后是 step_or_mode、actor_count、(actor_id,direction)。数据来自未
 * patch FD2.EXE 的 object3 初始化区与对应 LE relocation。 */
static const uint8_t k_move_00[] = {0x05,0x06,0x04,0x00,0x02,0x01,0x02,0x02,0x02,0x03,0x02,0x88,0x01,0x00,0x01,0x88,0x01,0x00,0x03,0x88,0x01,0x00,0x01,0x84,0x01,0x00,0x00};
static const uint8_t k_move_01[] = {0x01,0x01,0x04,0x04,0x00,0x05,0x00,0x06,0x00,0x07,0x00};
static const uint8_t k_move_02[] = {0x04,0x01,0x04,0x08,0x02,0x09,0x02,0x0a,0x01,0x0b,0x03,0x02,0x04,0x08,0x03,0x09,0x02,0x0a,0x02,0x0b,0x02,0x02,0x04,0x08,0x02,0x09,0x03,0x0a,0x02,0x0b,0x02,0x84,0x05,0x09,0x02,0x00,0x00,0x01,0x00,0x02,0x00,0x03,0x00};
static const uint8_t k_move_05[] = {0x01,0x04,0x01,0x09,0x00};
static const uint8_t k_move_5a[] = {0x05,0x01,0x01,0x00,0x01,0x01,0x02,0x00,0x01,0x01,0x02,0x04,0x02,0x00,0x01,0x01,0x01,0x88,0x02,0x00,0x01,0x01,0x01,0x88,0x02,0x00,0x03,0x01,0x01};
static const uint8_t k_move_5b[] = {0x04,0x01,0x01,0x00,0x00,0x01,0x01,0x00,0x03,0x82,0x01,0x00,0x03,0x80,0x01,0x01,0x01};
static const uint8_t k_move_5c[] = {0x01,0x01,0x01,0x00,0x01};
static const uint8_t k_move_5d[] = {0x08,0x01,0x02,0x00,0x02,0x01,0x01,0x02,0x02,0x00,0x01,0x01,0x01,0x01,0x02,0x00,0x02,0x01,0x01,0x01,0x02,0x00,0x02,0x01,0x02,0x01,0x03,0x00,0x01,0x01,0x02,0x03,0x03,0x01,0x03,0x00,0x01,0x01,0x01,0x03,0x03,0x01,0x03,0x00,0x01,0x01,0x01,0x03,0x00,0x88,0x01,0x03,0x03};
static const uint8_t k_move_5e[] = {0x02,0x01,0x02,0x00,0x01,0x03,0x02,0x84,0x02,0x03,0x00,0x04,0x03};
static const uint8_t k_move_5f[] = {0x02,0x01,0x02,0x00,0x03,0x03,0x00,0x84,0x02,0x00,0x01,0x03,0x01};
static const uint8_t k_move_60[] = {0x03,0x8f,0x01,0x03,0x03,0x8f,0x01,0x03,0x00,0x8f,0x01,0x03,0x02};
static const uint8_t k_move_61[] = {0x03,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x84,0x02,0x00,0x00,0x01,0x02};
static const uint8_t k_move_62[] = {0x09,0x01,0x03,0x00,0x03,0x03,0x03,0x04,0x03,0x01,0x03,0x00,0x00,0x03,0x03,0x04,0x03,0x01,0x03,0x00,0x03,0x01,0x03,0x04,0x03,0x01,0x04,0x00,0x03,0x01,0x03,0x03,0x00,0x04,0x03,0x01,0x04,0x00,0x03,0x01,0x03,0x03,0x03,0x04,0x00,0x01,0x04,0x00,0x00,0x01,0x03,0x03,0x03,0x04,0x03,0x01,0x04,0x00,0x03,0x01,0x00,0x03,0x03,0x04,0x03,0x01,0x04,0x00,0x03,0x01,0x03,0x03,0x00,0x04,0x03,0x01,0x04,0x00,0x03,0x01,0x03,0x03,0x03,0x04,0x00};
static const uint8_t k_move_63[] = {0x01,0x06,0x01,0x02,0x02};
static const uint8_t k_move_64[] = {0x01,0x0a,0x01,0x02,0x00};
static const uint8_t k_move_65[] = {0x01,0x03,0x01,0x04,0x01};
static const uint8_t k_move_66[] = {0x03,0x02,0x01,0x04,0x01,0x01,0x01,0x04,0x02,0x01,0x01,0x04,0x01};
static const uint8_t k_move_67[] = {0x01,0x80,0x01,0x04,0x01};
static const uint8_t k_move_68[] = {0x01,0x82,0x01,0x03,0x03};
static const uint8_t k_move_69[] = {0x05,0x02,0x01,0x03,0x03,0x01,0x01,0x03,0x00,0x01,0x01,0x03,0x03,0x01,0x02,0x03,0x03,0x04,0x00,0x04,0x02,0x03,0x03,0x04,0x03};

static const uint8_t k_walk_phases[] = {1, 2, 1, 0, 1, 2};

static void opening_move_one(fd2_scene_actor *actor, uint8_t direction) {
    if (!actor) return;
    actor->direction = direction & 3u;
    switch (actor->direction) {
        case 0: actor->y++; break;
        case 1: actor->x--; break;
        case 2: actor->y--; break;
        case 3: actor->x++; break;
    }
}

static int opening_actor_move_up_follow_camera(
        fd2_vga *vga,
        const fd2_terrain_tileset *terrain,
        const fd2_field_map *map,
        const fd2_map_sprite_bank *sprites,
        fd2_scene_field_state *state,
        size_t actor_idx,
        int cell_count,
        int fast) {
    if (!state || actor_idx >= state->actor_count || cell_count < 0)
        return -1;

    /* 新游戏开场直接调用的上移：复现
     * field_actor_move_up_follow_camera @0x108cd。该函数在角色靠近视窗
     * 上缘时，于一格的 6 个步行相位中同时移动角色与镜头，每相位 4 px；
     * 格坐标只在 6 相位完成后提交。 */
    for (int cell = 0; cell < cell_count; cell++) {
        fd2_scene_actor *actor = &state->actors[actor_idx];
        int camera_dy = 0;
        if (actor->y - state->camera_cell_y < 2 &&
            state->camera_cell_y > 0)
            camera_dy = -1;

        actor->direction = 2;
        if (!fast) {
            for (size_t phase = 0; phase < sizeof(k_walk_phases); phase++) {
                actor->frame = k_walk_phases[phase];
                actor->move_phase = (uint8_t)phase + 1u;
                state->camera_offset_y = camera_dy * (int)(phase + 1u) * 4;
                render_scene_base(vga, terrain, map, sprites, state);
                fd2_vga_present_timed(vga, FD2_BIOS_TICK_MS);
                opening_advance_idle(state, FD2_BIOS_TICK_MS);
            }
        }

        opening_move_one(actor, 2);
        actor->frame = 1;
        actor->move_phase = 0;
        state->focus_cell_y--;
        set_camera_cell(state, map, state->camera_cell_x,
                        state->camera_cell_y + camera_dy);
    }
    return 0;
}

static int play_opening_move_script(fd2_vga *vga,
                                    const fd2_terrain_tileset *terrain,
                                    const fd2_field_map *map,
                                    const fd2_map_sprite_bank *sprites,
                                    fd2_scene_field_state *state,
                                    const uint8_t *script,
                                    size_t script_len,
                                    int fade_out,
                                    int fast) {
    if (!script || script_len == 0) return -1;
    size_t pos = 1;
    uint8_t group_count = script[0];
    int darkness = 1;

    for (uint8_t group = 0; group < group_count; group++) {
        if (pos + 2 > script_len) return -1;
        uint8_t mode = script[pos++];
        uint8_t actor_count = script[pos++];
        if (pos + (size_t)actor_count * 2u > script_len) return -1;
        const uint8_t *pairs = script + pos;
        pos += (size_t)actor_count * 2u;

        for (uint8_t i = 0; i < actor_count; i++) {
            uint8_t actor_id = pairs[i * 2];
            if (actor_id < state->actor_count)
                state->actors[actor_id].direction = pairs[i * 2 + 1] & 3u;
        }

        if (mode & 0x80u) {
            int frames = mode & 0x7fu;
            if (frames == 0) frames = 1;
            if (!fast) {
                for (int frame = 0; frame < frames; frame++) {
                    present_base_frame_timed(vga, terrain, map, sprites,
                                             state, FD2_BIOS_TICK_MS);
                    opening_advance_idle(state, FD2_BIOS_TICK_MS);
                }
            }
            continue;
        }

        for (uint8_t step = 0; step < mode; step++) {
            if (!fast) {
                for (size_t phase = 0; phase < sizeof(k_walk_phases); phase++) {
                    for (uint8_t i = 0; i < actor_count; i++) {
                        uint8_t actor_id = pairs[i * 2];
                        if (actor_id < state->actor_count) {
                            state->actors[actor_id].frame = k_walk_phases[phase];
                            state->actors[actor_id].move_phase = (uint8_t)phase + 1u;
                        }
                    }
                    /* 原版只写脚本中 actor 的 record+4；卫兵等静止角色
                     * 不能跟随移动角色一起切换步行动画。 */
                    render_scene_base(vga, terrain, map, sprites, state);
                    if (fade_out) {
                        if (darkness < 0x40) darkness++;
                        fd2_vga_set_brightness(vga, darkness);
                    }
                    /* field_movement_script_play @0x10db2 每个 4 px 相位
                     * 调用 bios_tick_delay(1) 后再等 vsync。宿主端以绝对
                     * deadline 扣除本帧渲染耗时，避免步态节奏抖动。 */
                    fd2_vga_present_timed(vga, FD2_BIOS_TICK_MS);
                    opening_advance_idle(state, FD2_BIOS_TICK_MS);
                }
            }
            for (uint8_t i = 0; i < actor_count; i++) {
                uint8_t actor_id = pairs[i * 2];
                uint8_t direction = pairs[i * 2 + 1];
                if (actor_id >= state->actor_count) continue;
                opening_move_one(&state->actors[actor_id], direction);
                state->actors[actor_id].frame = 1;
                state->actors[actor_id].move_phase = 0;
            }
            /* phase 6 的 +24 px 与提交后的格坐标画面相同；非 fast 路径
             * 不再重复 present，以免重置下一相位的 deadline。 */
            if (fast)
                present_base_frame(vga, terrain, map, sprites, state);
        }
    }
    return pos == script_len ? 0 : -1;
}

static int opening_text(fd2_vga *vga,
                        const fd2_terrain_tileset *terrain,
                        const fd2_field_map *map,
                        const fd2_archive *dato,
                        const fd2_ui_sheet *ui,
                        const fd2_map_sprite_bank *sprites,
                        fd2_scene_field_state *state,
                        const fd2_font *font,
                        const fd2_text_entry *text,
                        size_t fragment,
                        int fast) {
    if (fragment >= text->fragment_count) return -1;
    return play_text_fragment(vga, terrain, map, dato, ui, sprites, state,
                              font, text, fragment, fast);
}

static void opening_delay(fd2_vga *vga,
                          const fd2_terrain_tileset *terrain,
                          const fd2_field_map *map,
                          const fd2_map_sprite_bank *sprites,
                          fd2_scene_field_state *state,
                          int ms,
                          int fast) {
    present_base_frame(vga, terrain, map, sprites, state);
    if (!fast) {
        fd2_delay_ms(ms);
        opening_advance_idle(state, ms);
    }
}

static void opening_palette_fade_in(fd2_vga *vga, int fast) {
    if (fast) {
        fd2_vga_set_brightness(vga, 0);
        return;
    }
    /* palette_fade_in_light @0x1cc6d：暗度 0x40→0。 */
    for (int darkness = 0x40; darkness >= 0; darkness -= 4) {
        fd2_vga_set_brightness(vga, darkness);
        fd2_vga_present_timed(vga, 12);
    }
}

static int opening_actor_arrival(fd2_vga *vga,
                                 const fd2_archive *fdother,
                                 const fd2_terrain_tileset *terrain,
                                 const fd2_field_map *map,
                                 const fd2_map_sprite_bank *sprites,
                                 fd2_scene_field_state *state,
                                 size_t first_actor,
                                 size_t last_actor,
                                 int fast) {
    if (last_actor > state->actor_count) last_actor = state->actor_count;
    if (first_actor >= last_actor) return 0;
    if (fast) {
        for (size_t i = first_actor; i < last_actor; i++)
            state->actors[i].visible = 1;
        return 0;
    }

    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(fdother, 9, &data, &size) != 0 ||
        size < 6 || memcmp(data, "LMI1", 4) != 0)
        return -1;
    uint16_t frame_count = rd_u16_le(data + 4);
    if (frame_count < 12 || 6u + (size_t)frame_count * 4u > size)
        return -1;

    /* field_actor_group_arrival_effect @0x300e1：FUN_0000e296 按组加入 actor 后，使用
     * FDOTHER[9] 的 12 帧 LMI1 特效在每个新 actor 格上展开。LMI blit
     * 的 0 色透明，因此每帧先恢复无新 actor 的战场底图。 */
    for (uint16_t frame_idx = 0; frame_idx < 12; frame_idx++) {
        uint32_t start = rd_u32_le(data + 6 + (size_t)frame_idx * 4u);
        uint32_t end = frame_idx + 1 < frame_count
                     ? rd_u32_le(data + 6 + (size_t)(frame_idx + 1) * 4u)
                     : (uint32_t)size;
        if (start >= end || end > size) return -1;
        fd2_image effect;
        if (fd2_lmi_decode_image(&effect, data + start, end - start) != 0)
            return -1;

        render_scene_base(vga, terrain, map, sprites, state);
        int camera_x = 0;
        int camera_y = 0;
        opening_camera_pixels(map, state, &camera_x, &camera_y);
        for (size_t i = first_actor; i < last_actor; i++) {
            const fd2_scene_actor *actor = &state->actors[i];
            int x = actor->x * 24 - camera_x + 12 - effect.width / 2;
            int y = actor->y * 24 - camera_y - 6;
            fd2_map_sprite_blit(vga, &effect, x, y, 0);
        }
        fd2_image_free(&effect);
        fd2_vga_present_timed(vga, FD2_BIOS_TICK_MS);
        opening_advance_idle(state, FD2_BIOS_TICK_MS);
    }
    for (size_t i = first_actor; i < last_actor; i++)
        state->actors[i].visible = 1;
    present_base_frame(vga, terrain, map, sprites, state);
    return 0;
}

static int play_stage32_opening(fd2_vga *vga,
                                const fd2_terrain_tileset *terrain,
                                const fd2_field_map *map,
                                const fd2_archive *dato,
                                const fd2_ui_sheet *ui,
                                const fd2_map_sprite_bank *sprites,
                                fd2_scene_field_state *state,
                                const fd2_font *font,
                                const fd2_text_entry *text,
                                int fast) {
#define MOVE32(id, fade) do { if (play_opening_move_script(vga, terrain, map, sprites, state, k_move_##id, sizeof(k_move_##id), fade, fast) != 0) return -1; } while (0)
#define TEXT32(n) do { if (opening_text(vga, terrain, map, dato, ui, sprites, state, font, text, n, fast) != 0) return -1; } while (0)
    pan_camera_to(vga, terrain, map, sprites, state, 3, 34, fast);
    MOVE32(63, 0);
    if (opening_actor_move_up_follow_camera(vga, terrain, map, sprites,
                                            state, 2, 15, fast) != 0)
        return -1;
    TEXT32(0);
    if (opening_actor_move_up_follow_camera(vga, terrain, map, sprites,
                                            state, 2, 13, fast) != 0)
        return -1;
    TEXT32(1);
    MOVE32(64, 1);
    pan_camera_to(vga, terrain, map, sprites, state, 0, 43, fast);
    opening_palette_fade_in(vga, fast);
    MOVE32(65, 0); TEXT32(2);
    MOVE32(66, 0); TEXT32(3);
    MOVE32(67, 0); TEXT32(4);
    MOVE32(68, 0); TEXT32(5);
    MOVE32(69, 1);
#undef TEXT32
#undef MOVE32
    return 0;
}

static int play_stage31_opening(fd2_vga *vga,
                                const fd2_terrain_tileset *terrain,
                                const fd2_field_map *map,
                                const fd2_archive *dato,
                                const fd2_ui_sheet *ui,
                                const fd2_map_sprite_bank *sprites,
                                fd2_scene_field_state *state,
                                const fd2_font *font,
                                const fd2_text_entry *text,
                                int fast) {
#define MOVE31(id) do { if (play_opening_move_script(vga, terrain, map, sprites, state, k_move_##id, sizeof(k_move_##id), 0, fast) != 0) return -1; } while (0)
#define TEXT31(n) do { if (opening_text(vga, terrain, map, dato, ui, sprites, state, font, text, n, fast) != 0) return -1; } while (0)
    pan_camera_to(vga, terrain, map, sprites, state, 5, 42, fast);
    MOVE31(5a); TEXT31(0);
    MOVE31(5b); TEXT31(1);
    MOVE31(5c); TEXT31(2);
    /* code0 0x1fd56：func_0x0000e296(3) 此时才加入 actor 2/3；
     * 随后的 camera (4,41) 左移让这两人进入画面。 */
    state->actors[2].visible = 1;
    state->actors[3].visible = 1;
    if (state->actor_count < 4) state->actor_count = 4;
    pan_camera_to(vga, terrain, map, sprites, state, 4, 41, fast);
    TEXT31(3);
    MOVE31(5d); TEXT31(4);
    /* code0 0x1fdd8/0x1fde2：先隐藏 actor 2，再加入 group 5 的
     * actor 4。两者同在 (5,44)，绝不能同时绘制。 */
    if (state->actor_count > 2) state->actors[2].visible = 0;
    state->actors[4].visible = 1;
    if (state->actor_count < 5) state->actor_count = 5;
    TEXT31(5);
    MOVE31(5e); TEXT31(6);
    MOVE31(5f); TEXT31(7);
    MOVE31(60); TEXT31(8);
    MOVE31(61); TEXT31(9);
    if (play_opening_move_script(vga, terrain, map, sprites, state,
                                 k_move_62, sizeof(k_move_62), 1, fast) != 0)
        return -1;
#undef TEXT31
#undef MOVE31
    return 0;
}

static int play_stage0_opening(fd2_vga *vga,
                               const fd2_archive *fdother,
                               const fd2_terrain_tileset *terrain,
                               const fd2_field_map *map,
                               const fd2_archive *dato,
                               const fd2_ui_sheet *ui,
                               const fd2_map_sprite_bank *sprites,
                               fd2_scene_field_state *state,
                               const fd2_font *font,
                               const fd2_text_entry *text,
                               int fast) {
#define MOVE0(id) do { if (play_opening_move_script(vga, terrain, map, sprites, state, k_move_##id, sizeof(k_move_##id), 0, fast) != 0) return -1; } while (0)
#define TEXT0(n) do { if (opening_text(vga, terrain, map, dato, ui, sprites, state, font, text, n, fast) != 0) return -1; } while (0)
    pan_camera_to(vga, terrain, map, sprites, state, 4, 12, fast);
    MOVE0(00); opening_delay(vga, terrain, map, sprites, state, 200, fast);
    TEXT0(0); opening_delay(vga, terrain, map, sprites, state, 200, fast);
    pan_camera_to(vga, terrain, map, sprites, state, 0, 0, fast);
    if (opening_actor_arrival(vga, fdother, terrain, map, sprites, state,
                              4, 8, fast) != 0) return -1;
    MOVE0(01);
    pan_camera_to(vga, terrain, map, sprites, state, 0, 15, fast);
    if (opening_actor_arrival(vga, fdother, terrain, map, sprites, state,
                              8, 12, fast) != 0) return -1;
    MOVE0(02); TEXT0(1);
    opening_delay(vga, terrain, map, sprites, state, 200, fast);
    MOVE0(05);
    if (state->actor_count > 9) state->actors[9].visible = 0;
    opening_delay(vga, terrain, map, sprites, state, 100, fast);
    TEXT0(2);
#undef TEXT0
#undef MOVE0
    return 0;
}

typedef struct {
    size_t stage;
    fd2_field_map map;
    fd2_field_placements placements;
    fd2_terrain_tileset terrain;
    fd2_text_entry text;
    fd2_scene_field_state field_state;
} fd2_opening_stage;

static void opening_stage_close(fd2_opening_stage *scene) {
    if (!scene) return;
    fd2_text_entry_close(&scene->text);
    fd2_terrain_tileset_close(&scene->terrain);
    fd2_field_placements_close(&scene->placements);
    fd2_field_map_close(&scene->map);
    memset(scene, 0, sizeof(*scene));
}

static int opening_stage_open(fd2_opening_stage *scene,
                              const fd2_archive *field,
                              const fd2_archive *fdshap,
                              const fd2_archive *fdtxt,
                              size_t stage) {
    opening_stage_close(scene);
    scene->stage = stage;
    if (fd2_field_map_open_stage(&scene->map, field, stage) != 0 ||
        fd2_field_placements_open_stage(&scene->placements, field, stage) != 0 ||
        fd2_terrain_tileset_open_stage(&scene->terrain, fdshap, field, stage) != 0 ||
        fd2_text_entry_open_entry(&scene->text, fdtxt, stage + 1u) != 0) {
        opening_stage_close(scene);
        return -1;
    }
    init_opening_field_state(&scene->field_state, stage,
                             &scene->map, &scene->placements);
    return 0;
}

static void opening_stage_fade_in(fd2_vga *vga,
                                  fd2_opening_stage *scene,
                                  const fd2_map_sprite_bank *sprites,
                                  int fast) {
    render_scene_base(vga, &scene->terrain, &scene->map, sprites,
                      &scene->field_state);
    if (fast) {
        fd2_vga_set_brightness(vga, 0);
        fd2_vga_present(vga);
        return;
    }
    for (int darkness = 0x40; darkness >= 0; darkness -= 4) {
        fd2_vga_set_brightness(vga, darkness);
        fd2_vga_present_timed(vga, 12);
    }
}

int fd2_scene_play_new_game_prologue(fd2_vga *vga,
                                     const fd2_archive *fdother,
                                     int once) {
    fd2_archive field = {0};
    fd2_archive fdshap = {0};
    fd2_archive fdtxt = {0};
    fd2_archive dato = {0};
    fd2_font font = {0};
    fd2_ui_sheet ui = {0};
    fd2_map_sprite_bank sprites = {0};
    fd2_opening_stage scene = {0};
    int result = -1;

    if (!vga || !fdother) return -1;
    if (fd2_archive_open(&field, "original_game/FDFIELD.DAT") != 0 ||
        fd2_archive_open(&fdshap, "original_game/FDSHAP.DAT") != 0 ||
        fd2_archive_open(&fdtxt, "original_game/FDTXT.DAT") != 0 ||
        fd2_archive_open(&dato, "original_game/DATO.DAT") != 0) {
        fprintf(stderr, "cannot open opening archives\n");
        goto done;
    }
    if (fd2_font_open_fdother(&font, fdother, 4) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0 ||
        fd2_map_sprite_bank_open(&sprites, "original_game/FDICON.B24") != 0) {
        fprintf(stderr, "cannot load opening font/UI/FDICON sprites\n");
        goto done;
    }

    const uint8_t *pal;
    size_t pal_len;
    if (fd2_archive_get(fdother, 0, &pal, &pal_len) == 0 && pal_len >= 768)
        fd2_vga_set_palette(vga, pal);
    fd2_vga_set_brightness(vga, 0);

    static const size_t stages[] = {
        FD2_OPENING_STAGE_32,
        FD2_OPENING_STAGE_31,
        FD2_OPENING_STAGE_0,
    };
    for (size_t part = 0; part < sizeof(stages) / sizeof(stages[0]); part++) {
        /* stage 32 脚本 0x69 与 stage 31 脚本 0x62 已在移动中渐暗；
         * 随后的场景初始化只负责加载新图并渐亮。 */
        if (opening_stage_open(&scene, &field, &fdshap, &fdtxt,
                               stages[part]) != 0) {
            fprintf(stderr, "cannot load opening stage %zu\n", stages[part]);
            goto done;
        }
        printf("new-game opening: part=%zu stage=%zu text_entry=%zu actors=%zu%s\n",
               part, stages[part], stages[part] + 1u,
               scene.field_state.actor_count, once ? ", fast" : "");
        opening_stage_fade_in(vga, &scene, &sprites, once);

        int play_result;
        if (stages[part] == FD2_OPENING_STAGE_32) {
            play_result = play_stage32_opening(vga, &scene.terrain, &scene.map,
                                               &dato, &ui, &sprites,
                                               &scene.field_state, &font,
                                               &scene.text, once);
        } else if (stages[part] == FD2_OPENING_STAGE_31) {
            play_result = play_stage31_opening(vga, &scene.terrain, &scene.map,
                                               &dato, &ui, &sprites,
                                               &scene.field_state, &font,
                                               &scene.text, once);
        } else {
            play_result = play_stage0_opening(vga, fdother,
                                              &scene.terrain, &scene.map,
                                              &dato, &ui, &sprites,
                                              &scene.field_state, &font,
                                              &scene.text, once);
        }
        if (play_result != 0) goto done;
    }

    /* FUN_0002fa63 返回后直接进入 stage 0 战斗，不做额外黑场。
     * 预览保留最终战场一小段时间，作为开场结束条件。 */
    fd2_vga_set_brightness(vga, 0);
    if (!once)
        opening_delay(vga, &scene.terrain, &scene.map, &sprites,
                      &scene.field_state, 800, 0);
    result = 0;

done:
    opening_stage_close(&scene);
    fd2_map_sprite_bank_close(&sprites);
    fd2_archive_close(&dato);
    fd2_archive_close(&fdtxt);
    fd2_archive_close(&fdshap);
    fd2_archive_close(&field);
    return result;
}
