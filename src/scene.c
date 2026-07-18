/* 炎龙骑士团 2 SDL3 重写 - 完整新游戏初始过场
 * 逆向依据：new_game_opening_play @code0 0x2231b 的 stage 32→31→0
 * 调用序列、text_dialog_render_tokens @code0 0x5f84、field_focus_move_to
 * @0x37efe、field_actor_move_up_follow_camera @0x38399、
 * field_camera_pan_to @0x387f1、field_movement_script_play @0x3887e，
 * 以及原版实机对照。
 */

#include "scene.h"

#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "dialog_flow.h"
#include "field.h"
#include "field_unit.h"
#include "field_unit_base.h"
#include "font.h"
#include "map_sprite.h"
#include "portrait.h"
#include "text.h"
#include "tile.h"

/* 完整新游戏开场由 new_game_opening_play @code0 0x2231b 驱动：
 * stage 32 / FDTXT[33] → stage 31 / FDTXT[32] → stage 0 / FDTXT[1]。
 * 三段之间穿插 field_camera_pan_to @0x387f1 与
 * field_movement_script_play @0x3887e，不能把 stage 0 fragment 0
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

/* bios_tick_delay @code0 0x7aa9 读取 BIOS 计时器 0x046c；
 * text_dialog_glyph_step @code0 0x64e8 每显示一个字先调用
 * sfx_play(DAT_00003eec, 2, 1)，再等待 1 tick，约 54.9 ms。
 * SDL 端取整为 55 ms。dialog_box_open @0x3b7c0 的展开步间等待则
 * 调用 delay_ms(10)，不是 BIOS tick。 */
#define FD2_BIOS_TICK_MS 55
#define FD2_DIALOG_TRANSITION_DELAY_MS 10
#define FD2_TEXT_GLYPH_DELAY_MS FD2_BIOS_TICK_MS
/* field_animation_phase_update @0x37b91 仅在 BIOS tick 差值大于 4
 * 时推进一次角色 idle phase，即约每 5 tick / 275 ms 一帧。 */
#define FD2_IDLE_FRAME_DELAY_MS (FD2_BIOS_TICK_MS * 5)

/* FDTXT 控制码；text_dialog_render_tokens @code0 0x5f84 使用
 * signed u16 解释。 */
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
    int camera_cell_x;
    int camera_cell_y;
    /* 角色贴近视窗边缘移动时，field_actor_move_up_follow_camera
     * @0x38399 等方向函数会在 6 个步态相位中每次卷动 4 px，最后才
     * 提交一格镜头坐标。 */
    int camera_offset_x;
    int camera_offset_y;
    /* DAT_00003ab1/03ab5：战场焦点格。镜头平移保持二者相对位置，
     * 角色直接移动和对话定位则单独推进焦点。 */
    int focus_cell_x;
    int focus_cell_y;
    /* DAT_00003a45：过场与正式战场共用的 0x50 字节单位记录表。 */
    fd2_field_units units;
    /* field_animation_phase_update @0x37b91 的全局 idle phase。静止 actor
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

static int scene_host_quit_requested;

static int poll_scene_quit(fd2_vga *vga) {
    /* 普通按键只由交互层按需读取；这里只传播持久宿主退出请求。 */
    if (!vga) return 0;
    if (fd2_input_take_quit(&vga->input)) {
        scene_host_quit_requested = 1;
        return 1;
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
    /* 复现 FUN_00013ffe @0x3baca 的布局：参数为 x/y、内部 16×16
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
        arg < field_state->units.count) {
        return field_state->units.items[arg].unit_id;
    }
    if ((control == FD2_TEXT_TOP_PORTRAIT ||
         control == FD2_TEXT_BOTTOM_PORTRAIT) && arg != 0x27u) {
        /* field_visible_actor_find_by_text_id @0x37e74 即使只找到隐藏
         * actor，也保留其记录指针，供 token 路径读取 offset 0x07 立绘。 */
        for (size_t i = 0; i < field_state->units.count; i++) {
            const fd2_field_unit *unit = &field_state->units.items[i];
            if (unit->text_id == (uint8_t)arg)
                return unit->unit_id;
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
        return arg < field_state->units.count ? (int)arg : -1;
    }

    /* -17/-18 的下一 token 是 actor record offset 0x08 文本编号，
     * field_visible_actor_find_by_text_id @0x37e74 只返回未隐藏角色。 */
    return fd2_field_units_find_visible_text_id(&field_state->units,
                                                (uint8_t)arg);
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

static void opening_actor_set(fd2_scene_field_state *state,
                              size_t actor_idx,
                              uint16_t unit_id,
                              int x,
                              int y) {
    if (!state || unit_id > 0xffu) return;
    /* 开场的 text_id 与 unit/DATO id 相同；正式关卡模板会分别填入
     * record offset 0x07/0x08。脚步选择读取 record +0x1f/+0x20，
     * 因此尽量从已确认的原版静态基础表补齐 race/profile；无基础表的
     * 特殊过场角色仍保留零值，与原版 unit ID 0x1c 特判分支分开。 */
    if (fd2_field_units_set(&state->units, actor_idx,
                            (uint8_t)unit_id, (uint8_t)unit_id, 0,
                            x, y) == 0)
        (void)fd2_field_unit_base_apply(
            &state->units.items[actor_idx], 1);
}

static void opening_actor_set_template(
        fd2_scene_field_state *state,
        size_t actor_idx,
        const fd2_field_unit_template *template_record,
        int x,
        int y,
        size_t template_idx) {
    if (!state || !template_record || actor_idx >= FD2_FIELD_MAX_UNITS ||
        x < 0 || x > 0xff || y < 0 || y > 0xff)
        return;

    fd2_field_unit unit;
    memset(&unit, 0, sizeof(unit));
    unit.x = (uint8_t)x;
    unit.y = (uint8_t)y;
    if (fd2_field_unit_stage_template_apply(&unit, template_record) == 0) {
        state->units.items[actor_idx] = unit;
        state->units.walk_frames[actor_idx] = 1;
        state->units.source_template_indices[actor_idx] =
            template_idx <= UINT16_MAX
                ? (uint16_t)template_idx
                : FD2_FIELD_UNIT_NO_TEMPLATE;
        if (state->units.count <= actor_idx)
            state->units.count = actor_idx + 1;
        return;
    }

    opening_actor_set(state, actor_idx, template_record->bytes[1], x, y);
}

static void init_opening_field_state(fd2_scene_field_state *state,
                                     size_t stage,
                                     const fd2_field_map *map,
                                     const fd2_field_metadata *metadata,
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
            if (metadata && i < metadata->unit_template_count)
                opening_actor_set_template(
                    state, 4 + i, &metadata->unit_templates[i],
                    p->x, p->y, i);
            else
                opening_actor_set(state, 4 + i, p->unit_id, p->x, p->y);
            /* field_actor_group_arrival_effect @0x57bad 先按组加入 actor，再播放 12 帧登场
             * 特效；进入 stage 0 时这两组不能提前出现在地图上。 */
            fd2_field_unit_set_hidden(&state->units.items[4 + i], 1);
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
        if (metadata && i < metadata->unit_template_count)
            opening_actor_set_template(state, i, &metadata->unit_templates[i],
                                       p->x, p->y, i);
        else
            opening_actor_set(state, i, p->unit_id, p->x, p->y);
        /* stage 31 的模板 offset 0x15 将 actor 0/1、2/3、4 分到
         * group 1/3/5。new_game_opening_play @code0 0x2231b 分时触发这些组，
         * 并由 field_actor_group_arrival_effect @0x57bad 播放登场效果；
         * 不能在场景加载时一次显示五人。 */
        if (stage == FD2_OPENING_STAGE_31 && i >= 2)
            fd2_field_unit_set_hidden(&state->units.items[i], 1);
    }
    /* 原流程会在各组登场时扩展 DAT_00003beb actor count；预填的
     * group 3/5 槽在激活前也不能参与文本编号查找。 */
    if (stage == FD2_OPENING_STAGE_31 && state->units.count > 2)
        state->units.count = 2;
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

static void render_prologue_actors(fd2_vga *vga,
                                   const fd2_field_map *map,
                                   const fd2_map_sprite_bank *sprites,
                                   const fd2_scene_field_state *field_state) {
    int camera_x = 0;
    int camera_y = 0;
    if (!field_state) return;
    opening_camera_pixels(map, field_state, &camera_x, &camera_y);
    fd2_field_units_render(vga, sprites, &field_state->units,
                           camera_x, camera_y, field_state->idle_phase);
}

static void clear_field_view_border(fd2_vga *vga) {
    if (!vga) return;
    /* field_view_render_tiles @0x3710c 先清空 320×200 缓冲区，再只写
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
    /* map_scene_render_actors @0x41db2 后由 FUN_00010134 调用
     * map_tile_blit_visible @0x37cda 重绘遮挡格。 */
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
    if (scene_host_quit_requested || poll_scene_quit(vga)) return;
    render_scene_base(vga, terrain, map, sprites, field_state);
    fd2_vga_present(vga);
    (void)poll_scene_quit(vga);
}

static void present_base_frame_timed(fd2_vga *vga,
                                     const fd2_terrain_tileset *terrain,
                                     const fd2_field_map *map,
                                     const fd2_map_sprite_bank *sprites,
                                     const fd2_scene_field_state *field_state,
                                     uint32_t frame_ms) {
    if (scene_host_quit_requested || poll_scene_quit(vga)) return;
    render_scene_base(vga, terrain, map, sprites, field_state);
    fd2_vga_present_timed(vga, frame_ms);
    (void)poll_scene_quit(vga);
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
        (size_t)speaker_actor_idx >= field_state->units.count)
        return -1;

    const fd2_field_unit *actor = &field_state->units.items[speaker_actor_idx];

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
         * 在 dialog_box_open @0x3b7c0 中将它从角色格移动到对话框原点；
         * 0x4a 是该帧背景色。 */
        blit_ui_tile_mode(vga, ui, 0, x, y, 0x4a);
        fd2_vga_present_timed(vga, FD2_DIALOG_TRANSITION_DELAY_MS);
        if (poll_scene_quit(vga)) break;
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

    /* dialog_box_open @0x3b7c0 依次用 4×2、8×3、12×4、16×5、
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
        if (poll_scene_quit(vga)) break;
    }
}

/* dialog_box_close @0x3bd57：倒序恢复五级弹框背景，各级等待 10 ms；
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
            if (poll_scene_quit(vga)) break;
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

    /* field_camera_pan_to @0x387f1 先完整调整 X，再调整 Y；每格只等待
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
        (size_t)actor_idx >= field_state->units.count)
        return;

    const fd2_field_unit *actor = &field_state->units.items[actor_idx];

    int max_cam_x = map ? map->width - FD2_FIELD_VIEW_CELLS_X : 0;
    int max_cam_y = map ? map->height - FD2_FIELD_VIEW_CELLS_Y : 0;
    if (max_cam_x < 0) max_cam_x = 0;
    if (max_cam_y < 0) max_cam_y = 0;

    /* dialog_box_open @0x3b7c0 在从角色格展开对话框前自动调用
     * field_focus_move_to @0x37efe；按 actor 索引定位的等价包装为
     * field_focus_move_to_actor @0x37f8f。前者先逐格移动 X、再移动 Y；焦点
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

static void wait_for_dialog_step(fd2_vga *vga, int ms, int *skip) {
    if (!vga || !skip || *skip) return;
    fd2_delay_ms((uint32_t)ms);
    if (fd2_input_take_quit(&vga->input)) {
        scene_host_quit_requested = 1;
        *skip = 1;
    }
}

static void page_pause(fd2_vga *vga, int *skip) {
    if (!vga || !skip || *skip) return;
    if (fd2_input_take_quit(&vga->input)) {
        scene_host_quit_requested = 1;
        *skip = 1;
        return;
    }
    /* FUN_00016c57 @VA 0x16c57：仅在对话等待期间按呈现帧轮询输入。
     * 每轮 present 重建当前帧 IsPressed；此前字形帧的按键不会重放。 */
    for (;;) {
        fd2_vga_present(vga);
        if (fd2_input_take_quit(&vga->input)) {
            scene_host_quit_requested = 1;
            *skip = 1;
            return;
        }
        fd2_input_event event;
        if (fd2_input_take_key(&vga->input, &event)) return;
        fd2_delay_ms(20);
    }
}

static void dialog_scroll_text_up(fd2_vga *vga,
                                  fd2_dialog_state *state,
                                  int fast) {
    if (!vga || !state || !state->is_open) return;
    int text_x = state->area == FD2_DIALOG_TOP
               ? DIALOG_TOP_TEXT_X : DIALOG_BOTTOM_TEXT_X;
    int text_y = state->area == FD2_DIALOG_TOP
               ? DIALOG_TOP_TEXT_Y : DIALOG_BOTTOM_TEXT_Y;
    int text_w = 13 * 16;
    int scroll_h = 72;

    /* dialog_text_scroll_up (FUN_00016e24) @VA 0x16e24 / code0 0x6e24：
     * 对话框到第四行后，
     * 先执行五次 3 px 上卷，再执行一次 4 px 上卷，总计 19 px。
     * 每步对 72 行分别 memmove 0xd0(208) px；源／目标从文字基址前
     * 1 px 开始，之后仅将文字区第 72 行填为背景色 0x4a。 */
    for (int step = 0; step < 6; step++) {
        int dy = step < 5 ? 3 : 4;
        for (int row = 0; row < scroll_h; row++)
            memmove(vga->framebuffer +
                        (size_t)(text_y + row) * VGA_STRIDE + text_x - 1,
                    vga->framebuffer +
                        (size_t)(text_y + row + dy) * VGA_STRIDE + text_x - 1,
                    (size_t)text_w);
        memset(vga->framebuffer +
                   (size_t)(text_y + scroll_h) * VGA_STRIDE + text_x,
               0x4a, (size_t)text_w);
        if (!fast) fd2_vga_present(vga);
    }
}

static void draw_continuation_arrow_frame(fd2_vga *vga,
                                          const fd2_ui_sheet *ui,
                                          int frame,
                                          int x,
                                          int y) {
    /* FUN_0001685c @VA 0x1685c 最终调用不透明 LMI blit
     * FUN_0004ed0b，而不是透明 blit。tile 18/19 的背景像素 0x4a
     * 因此会先覆盖上一帧，两个箭头帧不能彼此累积。 */
    blit_ui_tile(vga, ui, (uint16_t)frame, x, y);
}

static void continuation_pause(fd2_vga *vga,
                               const fd2_ui_sheet *ui,
                               const fd2_dialog_state *state,
                               int *skip) {
    if (!vga || !ui || !state || !skip || *skip) return;
    int arrow_x = state->area == FD2_DIALOG_TOP ? 111 : 191;
    int arrow_y = state->area == FD2_DIALOG_TOP ? 71 : 181;
    int arrow_frame = 18;
    uint64_t next_frame_ms = SDL_GetTicks() + 330u;

    /* FUN_00016c57(1) @VA 0x16c57：先绘制 FDOTHER[5] tile 18，
     * 等待期间在 tile 18/19 间闪烁；读键后以 tile 13 擦除箭头。 */
    draw_continuation_arrow_frame(vga, ui, arrow_frame,
                                  arrow_x, arrow_y);
    for (;;) {
        fd2_vga_present(vga);
        if (fd2_input_take_quit(&vga->input)) {
            scene_host_quit_requested = 1;
            *skip = 1;
            return;
        }
        fd2_input_event event;
        if (fd2_input_take_key(&vga->input, &event)) break;
        uint64_t now = SDL_GetTicks();
        if (now >= next_frame_ms) {
            arrow_frame = arrow_frame == 18 ? 19 : 18;
            draw_continuation_arrow_frame(vga, ui, arrow_frame,
                                          arrow_x, arrow_y);
            next_frame_ms = now + 330u;
        }
        fd2_delay_ms(20);
    }
    blit_ui_tile(vga, ui, 13, arrow_x, arrow_y - 5);
}

static void newline_or_scroll(fd2_vga *vga,
                              const fd2_ui_sheet *ui,
                              fd2_dialog_state *state,
                              int explicit_continue,
                              int fast,
                              int *skip) {
    /* text_dialog_render_tokens @code0 0x5fc4 / 0x6322：-2/-3 先在
     * line==3 时上卷并将 line 退回 2，再统一推进到下一行。-3 随后才
     * 调 FUN_00016c57(1)，因此下箭头出现在已经滚动后的画面。 */
    fd2_dialog_line_transition transition =
        fd2_dialog_flow_line_transition(
            explicit_continue ? FD2_TEXT_PAGE : FD2_TEXT_NEWLINE,
            state->line);
    if (transition.scroll_before_wait) {
        dialog_scroll_text_up(vga, state, fast);
        state->y -= FD2_DIALOG_LINE_PITCH;
    }
    state->line = transition.line;
    state->x = state->area == FD2_DIALOG_TOP
             ? DIALOG_TOP_TEXT_X : DIALOG_BOTTOM_TEXT_X;
    state->y += FD2_DIALOG_LINE_PITCH;

    if (transition.wait == FD2_DIALOG_FLOW_WAIT_SCROLL && !fast)
        continuation_pause(vga, ui, state, skip);
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
                            int fast,
                            int *skip) {
    if (fd2_dialog_flow_event_for_token(
            FD2_TEXT_TOP_PORTRAIT, state->is_open, state->line) ==
        FD2_DIALOG_FLOW_WAIT_BEFORE_SPEAKER) {
        /* text_dialog_render_tokens @VA 0x15f84 的 -17..-20 分支：
         * 已有对话框时先 FUN_00016c57(0) 等待并读取一个键，之后才
         * dialog_box_close 并打开新说话人。此前这里直接关闭，导致同一
         * fragment 内连续两三名说话人的对白根本没有等待点。 */
        if (!fast) page_pause(vga, skip);
        if (skip && *skip) return;
        animate_dialog_box_close(vga, terrain, map, ui, sprites, field_state,
                                 state, fast);
    }

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
                              fd2_field_audio *audio,
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
    /* text_dialog_render_tokens @VA 0x15f84：每个字形只查询该呈现帧的
     * input_check；当前帧有 press 时，本页后续字形关闭逐字 helper，
     * `-3` continuation／新说话人再恢复。
     * fast 验证路径从一开始就禁用逐字 helper，避免瞬间重放整段 PCM。 */
    int glyph_step_enabled = !fast;
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
                            &state, area, portrait_id, speaker_actor_idx, fast,
                            &skip);
            glyph_step_enabled = !fast;
            /* 原版打开新说话人后直接继续 token 循环；不存在额外 250 ms
             * 固定等待。首个字形仍由 glyph-step 的 1 BIOS tick 节奏控制。 */
            continue;
        }

        if (tok == FD2_TEXT_NEWLINE || tok == FD2_TEXT_PAGE) {
            int restores_glyph_step = tok == FD2_TEXT_PAGE;
            newline_or_scroll(vga, ui, &state,
                              tok == FD2_TEXT_PAGE, fast, &skip);
            if (restores_glyph_step && !skip) glyph_step_enabled = !fast;
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
        /* text_dialog_render_tokens @code0 0x5f84 不做按对话框宽度自动折行，
         * 只在 FDTXT token -2/-3 出现时推进一行。此前这里额外自动折行，
         * 会让恰好排到行尾后紧跟 -2 的文本一次换两行。 */
        state.x += 16;
        fd2_vga_present(vga);

        if (glyph_step_enabled) {
            if (scene_host_quit_requested ||
                fd2_input_take_quit(&vga->input)) {
                scene_host_quit_requested = 1;
                skip = 1;
            } else if (fd2_input_has_any_key(&vga->input)) {
                /* 只观察本字形的呈现帧；下一次 present 会丢弃此脉冲，
                 * page_pause 不会再次读到同一次按下。 */
                glyph_step_enabled = 0;
            }
        }
        if (glyph_step_enabled && !skip) {
            /* text_dialog_glyph_step @code0 0x64e8：通过 primary AIL
             * sample handle 播放 FDOTHER[31] sample 2；sfx_play
             * @code0 0x15a96 会先结束该 handle 的上一声。 */
            if (audio)
                (void)fd2_field_audio_play(
                    audio, FD2_FIELD_SFX_DIALOG_GLYPH);
            wait_for_dialog_step(vga, FD2_TEXT_GLYPH_DELAY_MS, &skip);
        }
    }

    if (!fast && !skip)
        page_pause(vga, &skip);
    animate_dialog_box_close(vga, terrain, map, ui, sprites, field_state,
                             &state, fast);
    state.is_open = 0;
    return 0;
}

/* DAT_000027d8 中新游戏开场会用到的移动脚本。字节流按
 * field_movement_script_play @0x3887e 的原始格式登记：group_count，
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

static void opening_footstep_play(fd2_field_audio *audio,
                                  const fd2_scene_field_state *state,
                                  size_t actor_idx) {
    if (!audio || !state || actor_idx >= state->units.count)
        return;

    const fd2_field_unit *actor = &state->units.items[actor_idx];
    (void)fd2_field_audio_play_footstep(
        audio, actor->unit_id, actor->movement_profile, actor->race);
}

static void opening_move_one(fd2_field_unit *actor, uint8_t direction) {
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
        fd2_field_audio *audio,
        size_t actor_idx,
        int cell_count,
        int fast) {
    if (!state || actor_idx >= state->units.count || cell_count < 0)
        return -1;

    /* 新游戏开场直接调用的上移：复现
     * field_actor_move_up_follow_camera @0x38399。该函数在角色靠近视窗
     * 上缘时，于一格的 6 个步行相位中同时移动角色与镜头，每相位 4 px；
     * 格坐标只在 6 相位完成后提交。 */
    for (int cell = 0; cell < cell_count; cell++) {
        fd2_field_unit *actor = &state->units.items[actor_idx];
        int camera_dy = 0;
        if (actor->y - state->camera_cell_y < 2 &&
            state->camera_cell_y > 0)
            camera_dy = -1;

        actor->direction = 2;
        if (!fast) {
            for (size_t phase = 0; phase < sizeof(k_walk_phases); phase++) {
                state->units.walk_frames[actor_idx] = k_walk_phases[phase];
                actor->frame_phase = (uint8_t)phase + 1u;
                state->camera_offset_y = camera_dy * (int)(phase + 1u) * 4;
                render_scene_base(vga, terrain, map, sprites, state);
                fd2_vga_present_timed(vga, FD2_BIOS_TICK_MS);
                fd2_vga_wait_frame_deadline(vga);
                opening_advance_idle(state, FD2_BIOS_TICK_MS);
                /* 原版先显示并等待 phase 1..5，再递增相位并调用 helper；
                 * phase 6 完成后直接提交格坐标，不再推进脚步计数。 */
                if (phase + 1u < sizeof(k_walk_phases))
                    opening_footstep_play(audio, state, actor_idx);
            }
        }

        opening_move_one(actor, 2);
        state->units.walk_frames[actor_idx] = 1;
        actor->frame_phase = 0;
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
                                    fd2_field_audio *audio,
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
            if (actor_id < state->units.count)
                state->units.items[actor_id].direction = pairs[i * 2 + 1] & 3u;
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
                        if (actor_id < state->units.count) {
                            state->units.walk_frames[actor_id] = k_walk_phases[phase];
                            state->units.items[actor_id].frame_phase = (uint8_t)phase + 1u;
                        }
                    }
                    /* 原版只写脚本中 actor 的 record+4；卫兵等静止角色
                     * 不能跟随移动角色一起切换步行动画。 */
                    render_scene_base(vga, terrain, map, sprites, state);
                    if (fade_out) {
                        if (darkness < 0x40) darkness++;
                        fd2_vga_set_brightness(vga, darkness);
                    }
                    /* field_movement_script_play @0x3887e 每个 4 px 相位
                     * 调用 bios_tick_delay(1) 后再等 vsync。宿主端以绝对
                     * deadline 扣除本帧渲染耗时，避免步态节奏抖动。 */
                    fd2_vga_present_timed(vga, FD2_BIOS_TICK_MS);
                    fd2_vga_wait_frame_deadline(vga);
                    opening_advance_idle(state, FD2_BIOS_TICK_MS);
                    /* field_movement_script_play @code0 0x3918..0x3923
                     * 在 phase 1..5 显示和等待结束后，只用脚本首 actor
                     * 调公共 helper；phase 6 与多人同行均不额外调用。 */
                    if (phase + 1u < sizeof(k_walk_phases) && actor_count > 0)
                        opening_footstep_play(audio, state, pairs[0]);
                }
            }
            for (uint8_t i = 0; i < actor_count; i++) {
                uint8_t actor_id = pairs[i * 2];
                uint8_t direction = pairs[i * 2 + 1];
                if (actor_id >= state->units.count) continue;
                opening_move_one(&state->units.items[actor_id], direction);
                state->units.walk_frames[actor_id] = 1;
                state->units.items[actor_id].frame_phase = 0;
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
                        fd2_field_audio *audio,
                        size_t fragment,
                        int fast) {
    if (fragment >= text->fragment_count) return -1;
    return play_text_fragment(vga, terrain, map, dato, ui, sprites, state,
                              font, text, audio, fragment, fast);
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
    /* palette_fade_in_light @0x44739：暗度 0x40→0。 */
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
                                 fd2_field_audio *audio,
                                 size_t first_actor,
                                 size_t last_actor,
                                 int fast) {
    if (last_actor > state->units.count) last_actor = state->units.count;
    if (first_actor >= last_actor) return 0;

    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(fdother, 9, &data, &size) != 0 ||
        size < 6 || memcmp(data, "LMI1", 4) != 0)
        return -1;
    uint16_t frame_count = rd_u16_le(data + 4);
    if (frame_count < 12 || 6u + (size_t)frame_count * 4u > size)
        return -1;

    /* field_actor_group_arrival_effect @0x57bad：按组加入 actor 后，使用
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

        if (frame_idx == 1 && audio) {
            /* field_actor_group_arrival_effect @code0 0x22bc0..0x22bd4：
             * 内部 frame counter 等于 1 时才以 primary handle 播放
             * FDOTHER[95] sample 0；两次 group 调用各触发一次。 */
            (void)fd2_field_audio_play(
                audio, FD2_FIELD_SFX_ACTOR_GROUP_ARRIVAL);
        }
        if (!fast) {
            render_scene_base(vga, terrain, map, sprites, state);
            int camera_x = 0;
            int camera_y = 0;
            opening_camera_pixels(map, state, &camera_x, &camera_y);
            for (size_t i = first_actor; i < last_actor; i++) {
                const fd2_field_unit *actor = &state->units.items[i];
                int x = actor->x * 24 - camera_x + 12 - effect.width / 2;
                int y = actor->y * 24 - camera_y - 6;
                fd2_map_sprite_blit(vga, &effect, x, y, 0);
            }
            fd2_vga_present_timed(vga, FD2_BIOS_TICK_MS);
            opening_advance_idle(state, FD2_BIOS_TICK_MS);
        }
        fd2_image_free(&effect);
    }
    for (size_t i = first_actor; i < last_actor; i++)
        fd2_field_unit_set_hidden(&state->units.items[i], 0);
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
                                fd2_bgm_player *bgm,
                                fd2_field_audio *audio,
                                int fast) {
#define SCENE_QUIT_CHECK() do { if (poll_scene_quit(vga)) return 0; } while (0)
#define MOVE32(id, fade) do { if (play_opening_move_script(vga, terrain, map, sprites, state, audio, k_move_##id, sizeof(k_move_##id), fade, fast) != 0) return -1; SCENE_QUIT_CHECK(); } while (0)
#define TEXT32(n) do { if (opening_text(vga, terrain, map, dato, ui, sprites, state, font, text, audio, n, fast) != 0) return -1; SCENE_QUIT_CHECK(); } while (0)
    SCENE_QUIT_CHECK();
    pan_camera_to(vga, terrain, map, sprites, state, 3, 34, fast);
    MOVE32(63, 0);
    if (opening_actor_move_up_follow_camera(vga, terrain, map, sprites,
                                            state, audio, 2, 15,
                                            fast) != 0)
        return -1;
    TEXT32(0);
    if (opening_actor_move_up_follow_camera(vga, terrain, map, sprites,
                                            state, audio, 2, 13,
                                            fast) != 0)
        return -1;
    TEXT32(1);
    /* new_game_opening_play @VA 0x3231b / code0 0x2231b：标题 track 18
     * 在新游戏开场最初两段对白期间继续播放；到 code0 0x223dd 才调用
     * music_track_play(-1, 0)，按原版 sequence 语义执行 4 秒淡出。
     * movement 0x64 和镜头就位后，code0 0x22413 再启动 track 11。 */
    if (bgm && fd2_bgm_stop(bgm) != 0) return -1;
    MOVE32(64, 1);
    pan_camera_to(vga, terrain, map, sprites, state, 0, 43, fast);
    if (bgm && fd2_bgm_play(bgm, 11, 0) != 0) return -1;
    opening_palette_fade_in(vga, fast);
    MOVE32(65, 0); TEXT32(2);
    MOVE32(66, 0); TEXT32(3);
    MOVE32(67, 0); TEXT32(4);
    MOVE32(68, 0); TEXT32(5);
    MOVE32(69, 1);
#undef TEXT32
#undef MOVE32
#undef SCENE_QUIT_CHECK
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
                                fd2_field_audio *audio,
                                int fast) {
#define SCENE_QUIT_CHECK() do { if (poll_scene_quit(vga)) return 0; } while (0)
#define MOVE31(id) do { if (play_opening_move_script(vga, terrain, map, sprites, state, audio, k_move_##id, sizeof(k_move_##id), 0, fast) != 0) return -1; SCENE_QUIT_CHECK(); } while (0)
#define TEXT31(n) do { if (opening_text(vga, terrain, map, dato, ui, sprites, state, font, text, audio, n, fast) != 0) return -1; SCENE_QUIT_CHECK(); } while (0)
    SCENE_QUIT_CHECK();
    pan_camera_to(vga, terrain, map, sprites, state, 5, 42, fast);
    MOVE31(5a); TEXT31(0);
    MOVE31(5b); TEXT31(1);
    MOVE31(5c); TEXT31(2);
    /* new_game_opening_play @code0 0x2231b：此时才加入 group 3 的 actor 2/3；
     * 随后的 camera (4,41) 左移让这两人进入画面。 */
    fd2_field_unit_set_hidden(&state->units.items[2], 0);
    fd2_field_unit_set_hidden(&state->units.items[3], 0);
    if (state->units.count < 4) state->units.count = 4;
    pan_camera_to(vga, terrain, map, sprites, state, 4, 41, fast);
    TEXT31(3);
    MOVE31(5d); TEXT31(4);
    /* code0 0x1fdd8/0x1fde2：先隐藏 actor 2，再加入 group 5 的
     * actor 4。两者同在 (5,44)，绝不能同时绘制。 */
    if (state->units.count > 2)
        fd2_field_unit_set_hidden(&state->units.items[2], 1);
    fd2_field_unit_set_hidden(&state->units.items[4], 0);
    if (state->units.count < 5) state->units.count = 5;
    TEXT31(5);
    MOVE31(5e); TEXT31(6);
    MOVE31(5f); TEXT31(7);
    MOVE31(60); TEXT31(8);
    MOVE31(61); TEXT31(9);
    if (play_opening_move_script(vga, terrain, map, sprites, state,
                                 audio, k_move_62, sizeof(k_move_62),
                                 1, fast) != 0)
        return -1;
#undef TEXT31
#undef MOVE31
#undef SCENE_QUIT_CHECK
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
                               fd2_field_audio *audio,
                               int fast) {
#define SCENE_QUIT_CHECK() do { if (poll_scene_quit(vga)) return 0; } while (0)
#define MOVE0(id) do { if (play_opening_move_script(vga, terrain, map, sprites, state, audio, k_move_##id, sizeof(k_move_##id), 0, fast) != 0) return -1; SCENE_QUIT_CHECK(); } while (0)
#define TEXT0(n) do { if (opening_text(vga, terrain, map, dato, ui, sprites, state, font, text, audio, n, fast) != 0) return -1; SCENE_QUIT_CHECK(); } while (0)
    SCENE_QUIT_CHECK();
    pan_camera_to(vga, terrain, map, sprites, state, 4, 12, fast);
    MOVE0(00); opening_delay(vga, terrain, map, sprites, state, 200, fast);
    TEXT0(0); opening_delay(vga, terrain, map, sprites, state, 200, fast);
    pan_camera_to(vga, terrain, map, sprites, state, 0, 0, fast);
    if (opening_actor_arrival(vga, fdother, terrain, map, sprites, state,
                              audio, 4, 8, fast) != 0) return -1;
    MOVE0(01);
    pan_camera_to(vga, terrain, map, sprites, state, 0, 15, fast);
    if (opening_actor_arrival(vga, fdother, terrain, map, sprites, state,
                              audio, 8, 12, fast) != 0) return -1;
    MOVE0(02); TEXT0(1);
    opening_delay(vga, terrain, map, sprites, state, 200, fast);
    MOVE0(05);
    if (state->units.count > 9)
        fd2_field_unit_set_hidden(&state->units.items[9], 1);
    opening_delay(vga, terrain, map, sprites, state, 100, fast);
    TEXT0(2);
#undef TEXT0
#undef MOVE0
#undef SCENE_QUIT_CHECK
    return 0;
}

typedef struct {
    size_t stage;
    fd2_field_map map;
    fd2_field_metadata metadata;
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
    fd2_field_metadata_close(&scene->metadata);
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
        fd2_field_metadata_open_stage(&scene->metadata, field, stage) != 0 ||
        fd2_field_placements_open_stage(&scene->placements, field, stage) != 0 ||
        fd2_terrain_tileset_open_stage(&scene->terrain, fdshap, field, stage) != 0 ||
        fd2_text_entry_open_entry(&scene->text, fdtxt, stage + 1u) != 0) {
        opening_stage_close(scene);
        return -1;
    }
    init_opening_field_state(&scene->field_state, stage,
                             &scene->map, &scene->metadata,
                             &scene->placements);
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

int fd2_scene_play_new_game_prologue_handoff(
        fd2_vga *vga,
        const fd2_archive *fdother,
        fd2_bgm_player *bgm,
        fd2_field_audio *audio,
        int once,
        fd2_field_handoff *handoff) {
    fd2_archive field = {0};
    fd2_archive fdshap = {0};
    fd2_archive fdtxt = {0};
    fd2_archive dato = {0};
    fd2_font font = {0};
    fd2_ui_sheet ui = {0};
    fd2_map_sprite_bank sprites = {0};
    fd2_opening_stage scene = {0};
    int result = -1;

    if (handoff) memset(handoff, 0, sizeof(*handoff));
    if (!vga || !fdother) return FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = 0;
    /* 不在标题→新游戏的宿主边界停止 music。原版 new_game_opening_play
     * 让 track 18 继续覆盖最初两段对白，并在自身 code0 0x223dd 才淡出。 */
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
               scene.field_state.units.count, once ? ", fast" : "");
        opening_stage_fade_in(vga, &scene, &sprites, once);
        if (poll_scene_quit(vga)) goto done;

        int play_result;
        if (stages[part] == FD2_OPENING_STAGE_32) {
            play_result = play_stage32_opening(vga, &scene.terrain, &scene.map,
                                               &dato, &ui, &sprites,
                                               &scene.field_state, &font,
                                               &scene.text, bgm, audio, once);
        } else if (stages[part] == FD2_OPENING_STAGE_31) {
            play_result = play_stage31_opening(vga, &scene.terrain, &scene.map,
                                               &dato, &ui, &sprites,
                                               &scene.field_state, &font,
                                               &scene.text, audio, once);
        } else {
            play_result = play_stage0_opening(vga, fdother,
                                              &scene.terrain, &scene.map,
                                              &dato, &ui, &sprites,
                                              &scene.field_state, &font,
                                              &scene.text, audio, once);
        }
        if (play_result != 0) goto done;
        if (poll_scene_quit(vga)) goto done;
    }

    /* FUN_0002fa63 返回后直接进入 stage 0 战斗，不做额外黑场。
     * 预览保留最终战场一小段时间，作为开场结束条件。 */
    fd2_vga_set_brightness(vga, 0);
    if (!once)
        opening_delay(vga, &scene.terrain, &scene.map, &sprites,
                      &scene.field_state, 800, 0);
    if (handoff) {
        handoff->stage = FD2_OPENING_STAGE_0;
        handoff->units = scene.field_state.units;
        handoff->camera_cell_x = scene.field_state.camera_cell_x;
        handoff->camera_cell_y = scene.field_state.camera_cell_y;
        handoff->focus_cell_x = scene.field_state.focus_cell_x;
        handoff->focus_cell_y = scene.field_state.focus_cell_y;
        handoff->idle_phase = scene.field_state.idle_phase;
        handoff->valid = 1;
    }
    result = 0;

done:
    opening_stage_close(&scene);
    fd2_map_sprite_bank_close(&sprites);
    fd2_archive_close(&dato);
    fd2_archive_close(&fdtxt);
    fd2_archive_close(&fdshap);
    fd2_archive_close(&field);
    return scene_host_quit_requested
        ? FD2_SCENE_RESULT_HOST_QUIT : result;
}

static int field_event_append_group(
        fd2_vga *vga,
        const fd2_archive *fdother,
        const fd2_field_game *game,
        fd2_scene_field_state *state,
        uint8_t group,
        int arrival_effect,
        fd2_field_audio *audio,
        int fast) {
    size_t first = state->units.count;
    if (fd2_field_units_append_group(&state->units, &game->metadata,
                                     &game->placements, group) != 0 ||
        state->units.count == first)
        return -1;
    if (!arrival_effect) {
        present_base_frame(vga, &game->terrain, &game->map,
                           &game->sprites, state);
        return 0;
    }
    for (size_t i = first; i < state->units.count; i++)
        fd2_field_unit_set_hidden(&state->units.items[i], 1);
    return opening_actor_arrival(vga, fdother, &game->terrain, &game->map,
                                 &game->sprites, state, audio, first,
                                 state->units.count, fast);
}

static int field_event_move_script(
        fd2_vga *vga,
        const fd2_field_game *game,
        fd2_scene_field_state *state,
        fd2_field_audio *footstep_audio,
        uint8_t script_id,
        int fast) {
    const uint8_t *script;
    size_t script_size;
    if (fd2_field_stage0_movement_script_get(
            script_id, &script, &script_size) != 0)
        return -1;
    return play_opening_move_script(vga, &game->terrain, &game->map,
                                    &game->sprites, state, footstep_audio, script,
                                    script_size, 0, fast);
}

int fd2_scene_play_field_event(fd2_vga *vga,
                               const fd2_archive *fdother,
                               fd2_field_game *game,
                               fd2_field_event_notice *notice,
                               fd2_field_audio *audio,
                               int fast) {
    if (!vga || !fdother || !game || !notice || !notice->handled ||
        !notice->presentation_deferred || notice->kind != FD2_FIELD_EVENT_TURN ||
        game->stage != 0 || notice->action > 3 ||
        notice->presentation_unit_count > game->units.count)
        return FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = 0;

    fd2_archive fdtxt = {0};
    fd2_archive dato = {0};
    fd2_text_entry text = {0};
    fd2_font font = {0};
    fd2_ui_sheet ui = {0};
    fd2_scene_field_state state;
    memset(&state, 0, sizeof(state));
    int result = -1;

    if (fd2_archive_open(&fdtxt, "original_game/FDTXT.DAT") != 0 ||
        fd2_archive_open(&dato, "original_game/DATO.DAT") != 0 ||
        fd2_text_entry_open_entry(&text, &fdtxt, game->stage + 1u) != 0 ||
        fd2_font_open_fdother(&font, fdother, 4) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0)
        goto done;

    state.camera_cell_x = notice->presentation_camera_x;
    state.camera_cell_y = notice->presentation_camera_y;
    state.focus_cell_x = notice->presentation_focus_x;
    state.focus_cell_y = notice->presentation_focus_y;
    state.units = game->units;
    state.units.count = notice->presentation_unit_count;
    state.idle_phase = game->idle_phase;

#define PAN(x, y) pan_camera_to(vga, &game->terrain, &game->map, \
                                &game->sprites, &state, (x), (y), fast)
#define GROUP(id, effect) do { \
    if (field_event_append_group(vga, fdother, game, &state, \
                                 (id), (effect), audio, fast) != 0) goto done; \
} while (0)
#define MOVE(id) do { \
    if (field_event_move_script(vga, game, &state, audio, \
                                (id), fast) != 0) \
        goto done; \
} while (0)
#define TEXT(id) do { \
    if (opening_text(vga, &game->terrain, &game->map, &dato, &ui, \
                     &game->sprites, &state, &font, &text, audio, \
                     (id), fast) != 0) \
        goto done; \
} while (0)

    /* field_stage0_31_turn_action0..3 @0x34531/0x3460b/0x34673/
     * 0x346cd：action 1/2 调用 field_actor_group_arrival_effect，
     * action 0/3 直接追加 group；对话 fragment 依次为 11/3、4、5、6。 */
    switch (notice->action) {
        case 0:
            GROUP(3, 0);
            PAN(5, 8);
            opening_delay(vga, &game->terrain, &game->map,
                          &game->sprites, &state, 100, fast);
            MOVE(7);
            TEXT(11);
            GROUP(7, 0);
            opening_delay(vga, &game->terrain, &game->map,
                          &game->sprites, &state, 100, fast);
            MOVE(8);
            TEXT(3);
            break;
        case 1:
            PAN(11, 16);
            GROUP(4, 1);
            MOVE(3);
            TEXT(4);
            break;
        case 2:
            PAN(0, 16);
            GROUP(5, 1);
            MOVE(4);
            TEXT(5);
            break;
        case 3:
            PAN(11, 11);
            GROUP(6, 0);
            MOVE(6);
            TEXT(6);
            break;
    }

#undef TEXT
#undef MOVE
#undef GROUP
#undef PAN

    if (state.units.count != game->units.count) goto done;
    for (size_t i = 0; i < game->units.count; i++) {
        if (memcmp(&state.units.items[i], &game->units.items[i],
                   sizeof(game->units.items[i])) != 0 ||
            state.units.source_template_indices[i] !=
                game->units.source_template_indices[i])
            goto done;
    }

    game->camera_cell_x = state.camera_cell_x;
    game->camera_cell_y = state.camera_cell_y;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
    game->cursor_cell_x = clamp_int(state.focus_cell_x, 0,
                                    game->map.width - 1);
    game->cursor_cell_y = clamp_int(state.focus_cell_y, 0,
                                    game->map.height - 1);
    game->idle_phase = state.idle_phase;
    notice->presentation_deferred = 0;
    result = 0;

done:
    fd2_text_entry_close(&text);
    fd2_archive_close(&dato);
    fd2_archive_close(&fdtxt);
    return scene_host_quit_requested
        ? FD2_SCENE_RESULT_HOST_QUIT : result;
}

int fd2_scene_play_new_game_prologue(fd2_vga *vga,
                                     const fd2_archive *fdother,
                                     fd2_bgm_player *bgm,
                                     fd2_field_audio *audio,
                                     int once) {
    return fd2_scene_play_new_game_prologue_handoff(
        vga, fdother, bgm, audio, once, NULL);
}
