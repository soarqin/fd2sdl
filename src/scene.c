/* 炎龙骑士团 2 SDL3 重写 - 新游戏过场与共用战场文本绘制
 * 逆向依据：new_game_opening_play @code0 0x2231b 的 stage 32→31→0
 * 调用序列、text_dialog_render_tokens @code0 0x5f84、field_focus_move_to
 * @0x12cea、field_actor_move_up_follow_camera @0x13185、
 * field_camera_pan_to @0x135dd、field_movement_script_play @0x1366a，
 * 以及原版实机对照。
 * B4底框复用field_bottom_notice_open @0x1956b（原sub_1956B）调用的
 * dialog_box_draw_tiles @0x168b6，箭头依据dialog_wait_key @0x16c57（原sub_16C57）。
 * B5复现field_stage1_entry_play @0x32d18（原sub_32D18）、
 * field_stage0_victory_execute @0x22ef6（原sub_22EF6）及
 * field_defeat_game_over_present @0x22e5c（原sub_22E5C）的呈现顺序。
 * 编排唯一来源为 data/static/flows/scene.json 与 stage.json；本文件只提供领域原语。
 */

#include "settings.h"
#include "scene.h"
#include "bios_clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "dialog_flow.h"
#include "field.h"
#include "field_background.h"
#include "field_unit.h"
#include "field_unit_base.h"
#include "field_visual.h"
#include "figani.h"
#include "font.h"
#include "image.h"
#include "map_sprite.h"
#include "portrait.h"
#include "text.h"
#include "tile.h"
#include "static_flow.h"
#include "static_data.h"
#include "scene_transition.h"
#include "scene_teleport.h"
#include "scene_cinematic.h"

/* 每次调用对应一个原读点；失败不改out，不送错误哨兵给纯tick投影。 */
static int scene_clock_ns(fd2_vga *vga,uint64_t *out){return fd2_vga_observe_checked(vga,SDL_GetTicksNS(),out);}
static int scene_clock_tick(fd2_vga *vga,int32_t *out){uint64_t actual;if(scene_clock_ns(vga,&actual)<0)return -1;*out=fd2_bios_tick_signed_ns(actual);return 0;}
static int scene_clock_low(fd2_vga *vga,uint16_t *out){uint64_t actual;if(scene_clock_ns(vga,&actual)<0)return -1;*out=fd2_bios_tick_low_ns(actual);return 0;}


/* 完整新游戏开场由 new_game_opening_play @code0 0x2231b 驱动：
 * stage 32 / FDTXT[33] → stage 31 / FDTXT[32] → stage 0 / FDTXT[1]。
 * 三段之间穿插 field_camera_pan_to @0x135dd 与
 * field_movement_script_play @0x1366a，不能把 stage 0 fragment 0
 * 单独放到 stage 32 背景上播放。 */
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
 * 共享PIT clock按原tick边界等待。dialog_box_open @0x165ac 的展开步间等待则
 * 调用 delay_ms(10)，不是 BIOS tick。 */
#define FD2_DIALOG_TRANSITION_DELAY_MS 10

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
} fd2_ui_sheet;

typedef struct {
    int camera_cell_x;
    int camera_cell_y;
    /* 角色贴近视窗边缘移动时，field_actor_move_up_follow_camera
     * @0x13185 等方向函数会在 6 个步态相位中每次卷动 4 px，最后才
     * 提交一格镜头坐标。 */
    int camera_offset_x;
    int camera_offset_y;
    /* DAT_00003ab1/03ab5：战场焦点格。镜头平移保持二者相对位置，
     * 角色直接移动和对话定位则单独推进焦点。 */
    int focus_cell_x;
    int focus_cell_y;
    /* DAT_00003a45：过场与正式战场共用的 0x50 字节单位记录表。 */
    fd2_field_units units;
    /* 12c60在active完全未匹配时才查persistent；仅借当前Game，不造副本owner。 */
    const fd2_field_unit *persistent;
    size_t persistent_count;
    const fd2_field_game *info_game; /* 只借资源/选项，info坐标和单位来自本Scene。 */
    fd2_field_game *motion_owner; /* common单叶借原Game，逐实际写点交还motion。 */
    /* 结算中的51A83值投影；旧开场的零初始化仍不启用cursor。 */
    const fd2_image *cursor;
    const fd2_image *ordinary_cursor;
    /* 165ac写回mode1后不重画地图，16b43也只恢复缓存背景。 */
    int dialog_cursor_suppressed;
    fd2_scene_runtime runtime; /* 借现App/Game RNG和消息值，不是第二owner。 */
    const fd2_text_entry *names; /* 15f84动态名固定读FDTXT[0]。 */
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
    /* 165ac/16b43本次对话的五级310×86矩形快照；只借VGA，绝不写page。
     * 生命周期仅当前fragment，非第二持久地图/业务owner。 */
    uint8_t backgrounds[5][DIALOG_W*DIALOG_H];
    unsigned captured;
} fd2_dialog_state;

static void scene_state_borrow_game(fd2_scene_field_state *state,
        const fd2_field_game *game) {
    state->info_game=game;
    state->runtime=(fd2_scene_runtime){game->attack_rng,game->attack_rng_userdata,
        {game->message_number,game->message_new_fragment,game->message_old_fragment},NULL};
    state->names=&game->ui_text;
}

/* 原11eee/12ac6读取同一range LUT；已有Game只借其资源，不另开bank。
 * 新游戏独立32/31/0由1088d清raw3，真实WH attr均无overlay。 */
static const uint8_t *scene_field_range_lut(const fd2_scene_field_state *state,
        const fd2_vga *vga) {
    return state && state->info_game ?
        fd2_field_range_lut(&state->info_game->visuals,vga->field_visual.range_phase):NULL;
}

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

/* FDOTHER5是混合容器：框角等raw小块走1685c，132负号是4e98d RLE。
 * 不能把其17字节RLE当成6×5 raw像素，亦不重解码整张UI表。 */
static int ui_sheet_decode_rle(const fd2_ui_sheet *sheet,uint16_t index,fd2_image *out) {
    if(!sheet || !sheet->data || index>=sheet->count || !out)return -1;
    uint32_t start=rd_u32_le(sheet->data+6u+(size_t)index*4u);
    uint32_t end=index+1u<sheet->count?
        rd_u32_le(sheet->data+6u+((size_t)index+1u)*4u):(uint32_t)sheet->size;
    return start<end && end<=sheet->size?
        fd2_image_decode_buf(out,sheet->data+start,end-start):-1;
}

static int scene_host_quit_requested;

/* 场景出口：时钟／显示错误优先，其次宿主退出，否则保留场景自身结果。 */
static int scene_finish(fd2_vga *vga, int result) {
    if (fd2_bios_clock_error() < 0 || fd2_vga_output_error(vga) < 0)
        return FD2_SCENE_RESULT_ERROR;
    return scene_host_quit_requested ? FD2_SCENE_RESULT_HOST_QUIT : result;
}
static int scene_render_error;

static void scene_motion_sync(const fd2_scene_field_state *state) {
    fd2_field_game *g=state?state->motion_owner:NULL;
    if(!g || g->units.count!=state->units.count)return;
    g->camera_cell_x=state->camera_cell_x;g->camera_cell_y=state->camera_cell_y;
    g->camera_pixel_offset_x=state->camera_offset_x;
    g->camera_pixel_offset_y=state->camera_offset_y;
    g->cursor_cell_x=state->focus_cell_x;g->cursor_cell_y=state->focus_cell_y;
    for(size_t i=0;i<g->units.count;i++) {
        g->units.items[i].x=state->units.items[i].x;
        g->units.items[i].y=state->units.items[i].y;
        g->units.items[i].direction=state->units.items[i].direction;
        g->units.items[i].frame_phase=state->units.items[i].frame_phase;
        g->units.walk_frames[i]=state->units.walk_frames[i];
    }
}

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
    /* 对照 FUN_00013fa4 @0x1685c / FUN_0004c453 @0x4ed0b：
     * FDOTHER[5] LMI1 小块是 4 字节宽高头 + 原始 8bpp 像素。 */
    blit_ui_tile_mode(vga, ui, tile_idx, x, y, -1);
}

static void draw_dialog_tiles(fd2_vga *vga, const fd2_ui_sheet *ui,
                              int x, int y, int tile_w, int tile_h) {
    /* 复现 FUN_00013ffe @0x168b6 的布局：参数为 x/y、内部 16×16
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

int fd2_scene_field_message_validate(const fd2_archive *fdother) {
    fd2_ui_sheet ui;
    if (ui_sheet_open_fdother(&ui, fdother, 5) != 0) return -1;
    /* 168b6/16c57实际读取的19张raw图；先校全span，晚坏图不能半绘。 */
    static const uint8_t dimensions[19][2] = {
        {3,3},{3,3},{3,3},{3,3},{16,3},{16,3},{16,3},{16,3},{16,3},
        {3,16},{3,16},{16,3},{16,16},{3,16},{3,16},{3,16},{3,16},
        {14,10},{14,10}
    };
    for (unsigned i = 0; i < 19; i++) {
        int width, height;
        if (ui_sheet_get_tile(&ui, (uint16_t)(i + 1), NULL, &width, &height) != 0 ||
            width != dimensions[i][0] || height != dimensions[i][1]) return -1;
    }
    return 0;
}

int fd2_scene_field_message_box(fd2_vga *vga, const fd2_archive *fdother) {
    fd2_ui_sheet ui;
    if (!vga || fd2_scene_field_message_validate(fdother) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0) return -1;
    draw_dialog_box(vga, &ui, FD2_DIALOG_BOTTOM);
    return 0;
}

int fd2_scene_field_message_arrow(fd2_vga *vga, const fd2_archive *fdother,
                                  unsigned arrow_frame) {
    fd2_ui_sheet ui;
    if (!vga || (arrow_frame != 13 && arrow_frame != 18 && arrow_frame != 19) ||
        fd2_scene_field_message_validate(fdother) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0) return -1;
    /* 16c57：53A51为已加载map指针，v7=18336；13的擦除原点上移5px。 */
    blit_ui_tile(vga, &ui, (uint16_t)arrow_frame, 191, arrow_frame == 13 ? 176 : 181);
    return 0;
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
        /* field_visible_actor_find_by_text_id @0x12c60（原登记同名）：
         * 首个可见匹配优先；全hidden保最后匹配；active完全没有匹配才
         * 从persistent保最后匹配。函数返回-1不等于记录指针为空。 */
        const fd2_field_unit *matched = NULL;
        for (size_t i = 0; i < field_state->units.count; i++) {
            const fd2_field_unit *unit = &field_state->units.items[i];
            if (unit->text_id == arg) {
                matched = unit;
                if (!fd2_field_unit_is_hidden(unit)) return unit->unit_id;
            }
        }
        if (matched) return matched->unit_id;
        for (size_t i = 0; field_state->persistent && i < field_state->persistent_count; i++)
            if (field_state->persistent[i].text_id == arg) matched = &field_state->persistent[i];
        if (matched) return matched->unit_id;
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
     * field_visible_actor_find_by_text_id @0x12c60 只返回未隐藏角色。 */
    if (arg > UINT8_MAX) return -1;
    return fd2_field_units_find_visible_text_id(&field_state->units,
                                                (uint8_t)arg);
}

static int draw_portrait_frame(fd2_vga *vga, const fd2_archive *dato,
                          fd2_dialog_area area, uint16_t portrait_id, unsigned frame) {
    fd2_image portrait;
    if (fd2_portrait_decode_frame(&portrait, dato, portrait_id, frame) != 0) {
        fprintf(stderr, "cannot decode dialog portrait %u\n", (unsigned)portrait_id);
        return -1;
    }

    /* text_dialog_render_tokens @0x15f84 / sub_16559 @0x16559：下框调用
     * lmi_rle_blit_reverse @0x4ec31（原登记 FUN_0004c379），从(87,115)
     * 向左写80列；上框仍从(232,5)正向写。只镜像独立解码的图像。 */
    if (area == FD2_DIALOG_BOTTOM) {
        for (int row = 0; row < portrait.height; row++) {
            uint8_t *pixels = portrait.pixels + (size_t)row * (size_t)portrait.width;
            for (int left = 0; left < portrait.width / 2; left++) {
                int right = portrait.width - 1 - left;
                uint8_t pixel = pixels[left];
                pixels[left] = pixels[right];
                pixels[right] = pixel;
            }
        }
    }
    int x = (area == FD2_DIALOG_TOP) ? DIALOG_TOP_PORTRAIT_X : DIALOG_BOTTOM_PORTRAIT_X;
    int y = (area == FD2_DIALOG_TOP) ? DIALOG_TOP_PORTRAIT_Y : DIALOG_BOTTOM_PORTRAIT_Y;
    fd2_tileset_blit(vga, &portrait, x, y, -1);
    fd2_image_free(&portrait);
    return 0;
}

static int draw_portrait(fd2_vga *vga, const fd2_archive *dato,
        fd2_dialog_area area, uint16_t portrait_id) {
    return draw_portrait_frame(vga,dato,area,portrait_id,0);
}

/* field_bottom_notice_open @0x1956b（原sub_1956B）的静态页；不播放/提交输入。 */
int fd2_scene_field_notice_page(fd2_vga *vga, const fd2_archive *fdother,
        const fd2_archive *dato, uint16_t portrait_id) {
    fd2_image portrait = {0};
    if (!vga || fd2_scene_field_message_validate(fdother) != 0 ||
        fd2_portrait_decode_frame(&portrait, dato, portrait_id, 0) != 0)
        return -1;
    if (portrait.width != 80 || portrait.height != 80) {
        fd2_image_free(&portrait); return -1;
    }
    /* 1956b初始一律reverse；128..132原表位置，其余为右端(87,115)。 */
    static const int origins[5] = {4283, 1707, 3939, 1398, 3644};
    int origin = portrait_id >= 128 && portrait_id <= 132
        ? origins[portrait_id - 128] : 36887;
    int x = origin % VGA_STRIDE, y = origin / VGA_STRIDE;
    if (fd2_scene_field_message_box(vga, fdother) != 0) {
        fd2_image_free(&portrait); return -1;
    }
    for (int row = 0; row < portrait.height; row++)
        for (int column = 0; column < portrait.width; column++) {
            int dx = x - column, dy = y + row;
            if (dx >= 0 && dx < VGA_W && dy >= 0 && dy < VGA_H)
                vga->framebuffer[dy * VGA_STRIDE + dx] =
                    portrait.pixels[(size_t)row * 80u + (size_t)column];
        }
    fd2_image_free(&portrait);
    return 0;
}

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int opening_actor_set(fd2_scene_field_state *state,
                              size_t actor_idx,
                              uint16_t unit_id,
                              int x,
                              int y) {
    if (!state || unit_id > 0xffu) return -1;
    /* 原开场身份/属性使用同一构造源，未知ID必须失败，不能回退零属性。 */
    fd2_field_units shadow = state->units;
    if (fd2_field_units_set(&shadow, actor_idx,
                            (uint8_t)unit_id, (uint8_t)unit_id, 0,
                            x, y) != 0 ||
        fd2_field_unit_base_apply(&shadow.items[actor_idx], 1) != 0) return -1;
    state->units = shadow;
    return 0;
}

static int opening_actor_set_template(
        fd2_scene_field_state *state,
        size_t actor_idx,
        const fd2_field_unit_template *template_record,
        int x,
        int y,
        size_t template_idx) {
    if (!state || !template_record || actor_idx >= FD2_FIELD_MAX_UNITS ||
        x < 0 || x > 0xff || y < 0 || y > 0xff)
        return -1;

    /* 10c50只写原构造字段，Scene重用目标槽时也保其余已拥有字节。 */
    fd2_field_unit unit = state->units.items[actor_idx];
    unit.x = (uint8_t)x;
    unit.y = (uint8_t)y;
    if (fd2_field_unit_stage_template_apply(&unit, template_record) == 0) {
        fd2_field_units saved = state->units;
        state->units.items[actor_idx] = unit;
        state->units.walk_frames[actor_idx] = 1;
        state->units.source_template_indices[actor_idx] =
            template_idx <= UINT16_MAX
                ? (uint16_t)template_idx
                : FD2_FIELD_UNIT_NO_TEMPLATE;
        if (state->units.count <= actor_idx)
            state->units.count = actor_idx + 1;
        if (fd2_field_units_sprite_cache_append(&state->units, actor_idx) != 0) {
            state->units = saved;
            return -1;
        }
        return 0;
    }
    return -1;
}

static int init_opening_field_state(fd2_scene_field_state *state, size_t stage,
    const fd2_field_map *map, const fd2_field_metadata *metadata,
    const fd2_field_placements *placements, const fd2_scene_runtime *runtime);

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

static void render_scene_base_with_water(fd2_vga *vga,
                              const fd2_terrain_tileset *terrain,
                              const fd2_field_map *map,
                              const fd2_map_sprite_bank *sprites,
                              const fd2_scene_field_state *field_state,int water) {
    if(!vga || !vga->field_page || !field_state) {scene_render_error=1;return;}
    scene_motion_sync(field_state);
    int camera_x=field_state->camera_cell_x,camera_y=field_state->camera_cell_y;
    if(fd2_field_visual_actor_step_live(vga,&vga->field_visual)<0) {
        scene_render_error=1;return;
    }
    if(water && fd2_field_water_tick_live(vga)<0) {scene_render_error=1;return;}
    if(fd2_field_visual_terrain_step_live(vga,&vga->field_visual)<0) {
        scene_render_error=1;return;
    }
    /* 11eee只按原opaque写工作页，最终11cac才复制内区；绝不每次清0或
     * 把live VGA对话/UI当透明底图。相位由真实caller先推进后借入。 */
    if(fd2_field_background_draw_live(vga,vga->field_page,72,72,camera_x,camera_y,
            0,0,1)!=0 ||
        fd2_field_visual_range_step_live(vga,&vga->field_visual,-1)<0 ||
        fd2_terrain_render_page_block(vga->field_page,terrain,map,72,72,13,8,
            camera_x,camera_y,(int)vga->field_visual.terrain_phase,
            (uint8_t)vga->field_visual.actor_idle_phase,NULL,scene_field_range_lut(field_state,vga))!=0) {
        scene_render_error=1;return;
    }
    if (field_state && field_state->cursor &&
        !field_state->dialog_cursor_suppressed &&
        field_state->focus_cell_x >= field_state->camera_cell_x &&
        field_state->focus_cell_x < field_state->camera_cell_x + FD2_FIELD_VIEW_CELLS_X &&
        field_state->focus_cell_y >= field_state->camera_cell_y &&
        field_state->focus_cell_y < field_state->camera_cell_y + FD2_FIELD_VIEW_CELLS_Y)
    {
        const fd2_image *cursor=field_state->cursor;
        int x=72+(field_state->focus_cell_x-camera_x)*24;
        int y=72+(field_state->focus_cell_y-camera_y)*24;
        uint8_t scratch[24*24];
        if(!cursor->pixels || cursor->width!=24 || cursor->height!=24 ||
            fd2_field_page_read(vga->field_page,x,y,24,24,scratch,sizeof(scratch),24)!=0) {
            scene_render_error=1;return;
        }
        /* FDOTHER[1]全20帧独立源扫描无opaque0，decoded0在此域正是SKIP。
         * scratch只借同page本格，不能读live VGA再回灌。 */
        for(size_t i=0;i<sizeof(scratch);i++)
            if(cursor->pixels[i])scratch[i]=cursor->pixels[i];
        if(fd2_field_page_write(vga->field_page,x,y,24,24,scratch,sizeof(scratch),24)!=0) {
            scene_render_error=1;return;
        }
    }
    if(fd2_field_units_render_page_live(vga,vga->field_page,sprites,&field_state->units,
            camera_x,camera_y,(uint8_t)vga->field_visual.actor_idle_phase,
            (uint8_t)vga->field_visual.cursor_phase)!=0) {
        scene_render_error=1;return;
    }
    /* field_actor_terrain_overlays_draw @0x129ec（原sub_129EC）只恢复
     * actor附近遮挡；空的建筑格不能覆盖后来绘制的选择框。 */
    if(fd2_field_units_render_page_terrain_overlays(vga->field_page,terrain,map,
            &field_state->units,camera_x,camera_y,
            (int)vga->field_visual.terrain_phase,NULL,scene_field_range_lut(field_state,vga))!=0 ||
        (field_state->info_game && fd2_field_game_render_page_info(
            field_state->info_game,vga,vga->field_page,&field_state->units,
            camera_x,camera_y,field_state->focus_cell_x,field_state->focus_cell_y,
            (uint8_t)vga->field_visual.actor_idle_phase)!=0) ||
        fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,
            sizeof(vga->framebuffer))!=0) scene_render_error=1;
}

/* 11cbc..11cc3：原实参0调用水色，1跳过；cold caller须显式保留原参。 */
static void render_scene_11cac(fd2_vga *vga,
    const fd2_terrain_tileset *terrain,const fd2_field_map *map,
    const fd2_map_sprite_bank *sprites,const fd2_scene_field_state *state,int skip_water) {
    if(skip_water!=0 && skip_water!=1) {scene_render_error=1;return;}
    render_scene_base_with_water(vga,terrain,map,sprites,state,skip_water==0);
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

/* 11cac/15f84及有限动作提交保同input owner泵，不消费其它owner的deadline。
 * 源ticks/ms由caller随后单独执行；空等H16另用scoped helper。 */
static int scene_present_copy(fd2_vga *vga) {
    if(!vga || fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    fd2_input_begin_frame(&vga->input);
    fd2_vga_upload_current_frame(vga);
    return fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0?-1:0;
}

static int present_base_frame(fd2_vga *vga,
                               const fd2_terrain_tileset *terrain,
                               const fd2_field_map *map,
                               const fd2_map_sprite_bank *sprites,
                               const fd2_scene_field_state *field_state,int skip_water) {
    if(!vga || fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    if (scene_host_quit_requested || poll_scene_quit(vga)) return 0;
    render_scene_11cac(vga, terrain, map, sprites, field_state,skip_water);
    if(scene_render_error || scene_present_copy(vga)<0)return -1;
    (void)poll_scene_quit(vga);
    return 0;
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
    /* opening_camera_pixels已返回world-4；此差值即24*relative+4，
     * 不可重复加viewport边框。 */
    int source_x = actor->x * 24 - camera_x;
    int source_y = actor->y * 24 - camera_y;
    if (source_x < -24 || source_y < -24 || source_x >= VGA_W || source_y >= VGA_H)
        return -1;

    if (out_x) *out_x = source_x;
    if (out_y) *out_y = source_y;
    if (out_steps) {
        /* 165ac/16b43：relative focus之和；N==0不画marker。 */
        *out_steps = (source_x - 4) / 24 + (source_y - 4) / 24;
    }
    return 0;
}

/* 15e9e/165ac/16b43均是本次绘制完成后独立delay_ms(10)。
 * 不借宿主帧预算；合成耗时不能抵扣原等待，也不等前一动画deadline。 */
static int dialog_present_delay(fd2_vga *vga) {
    if(!vga || fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    fd2_vga_upload_current_frame(vga);
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    if(!poll_scene_quit(vga) && fd2_vga_delay_ms(vga,FD2_DIALOG_TRANSITION_DELAY_MS)<0)return -1;
    return 0;
}

static int animate_dialog_marker(fd2_vga *vga,
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
                            &source_x, &source_y, &steps) != 0 || steps <= 0)
        return 0;

    int target_x = DIALOG_X;
    int target_y = dialog_area_y(area);
    (void)terrain;(void)sprites;
    for (int frame = 0; frame <= steps; frame++) {
        /* 165ac/16b43各从本方向端点做signed IDIV；反向数组会错1px。 */
        int from_x = reverse ? target_x : source_x;
        int from_y = reverse ? target_y : source_y;
        int to_x = reverse ? source_x : target_x;
        int to_y = reverse ? source_y : target_y;
        int x = from_x - (from_x - to_x) * frame / steps;
        int y = from_y - (from_y - to_y) * frame / steps;
        uint8_t saved[24*24];
        for(int row=0;row<24;row++)for(int col=0;col<24;col++) {
            int dx=x+col,dy=y+row;
            if(dx>=0&&dx<VGA_W&&dy>=0&&dy<VGA_H)
                saved[row*24+col]=vga->framebuffer[dy*VGA_STRIDE+dx];
        }
        /* sub_15E9E @0x15e9e（原FUN_000135e6）调用4ed34：
         * 仅raw零透明，74同其它非零色一样写入，不是透明底。 */
        blit_ui_tile_mode(vga, ui, 0, x, y, 0);
        if(dialog_present_delay(vga)<0)return -1;
        /* 165ac@16679仅opening在delay之后/restore之前清BIOS FIFO。
         * 16b43没有此写点；HOST已停止时不再耗队。 */
        if (!reverse && !scene_host_quit_requested)
            fd2_input_clear_keys(&vga->input);
        for(int row=0;row<24;row++)for(int col=0;col<24;col++) {
            int dx=x+col,dy=y+row;
            if(dx>=0&&dx<VGA_W&&dy>=0&&dy<VGA_H)
                vga->framebuffer[dy*VGA_STRIDE+dx]=saved[row*24+col];
        }
        if (poll_scene_quit(vga)) break;
    }
    return 0;
}

static void dialog_snapshot(fd2_vga *vga, fd2_dialog_state *state,
        unsigned index, int restore) {
    /* pixel_rect_snapshot_capture @0x4ecbf（原sub_4ECBF）及
     * pixel_rect_snapshot_restore @0x4ec7c（原sub_4EC7C）：
     * pixel_rect_capture_rows @0x4ecf0（原sub_4ECF0）与
     * pixel_rect_restore_rows @0x4eca4（原sub_4ECA4）逐行宽copy，
     * 源header只描述本次借用矩形。
     * 此处把已知310×86/320 stride映射为现有五份像素，不复制原header owner。 */
    int y=dialog_area_y(state->area);
    for(size_t row=0;row<DIALOG_H;row++) {
        uint8_t *screen=vga->framebuffer+((size_t)y+row)*VGA_STRIDE+DIALOG_X;
        uint8_t *saved=state->backgrounds[index]+row*DIALOG_W;
        if(restore)memcpy(screen,saved,DIALOG_W);
        else memcpy(saved,screen,DIALOG_W);
    }
}

static int animate_dialog_box_open(fd2_vga *vga,
                                    const fd2_terrain_tileset *terrain,
                                    const fd2_field_map *map,
                                    const fd2_ui_sheet *ui,
                                    const fd2_map_sprite_bank *sprites,
                                    const fd2_scene_field_state *field_state,
                                    fd2_dialog_state *state,
                                    int speaker_actor_idx,
                                    fd2_dialog_area area,
                                    int fast) {
    state->captured=0;
    if(!fast)if(animate_dialog_marker(vga,terrain,map,ui,sprites,field_state,
        speaker_actor_idx,area,0)<0)return -1;
    if(scene_host_quit_requested)return 0;

    /* dialog_box_open @0x165ac 依次用 4×2、8×3、12×4、16×5、
     * 19×5 个内部块绘制同一对话框，形成原版的伪缩放弹出效果。 */
    static const uint8_t stages[][2] = {
        {4, 2}, {8, 3}, {12, 4}, {16, 5}, {19, 5},
    };
    for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
        dialog_snapshot(vga,state,(unsigned)i,0);
        state->captured=(unsigned)i+1u;
        draw_dialog_tiles(vga, ui, DIALOG_X, dialog_area_y(area),
                          stages[i][0], stages[i][1]);
        if (!fast && i + 1 < sizeof(stages) / sizeof(stages[0]))
            if(dialog_present_delay(vga)<0)return -1;
        if (poll_scene_quit(vga)) break;
    }
    /* 165ac@1684e在第五级后清键，无第五码10ms；fast为诊断投影。 */
    if (!fast && !scene_host_quit_requested)
        fd2_input_clear_keys(&vga->input);
    return 0;
}

/* dialog_box_close @0x16b43：倒序恢复五级弹框背景，各级等待 10 ms；
 * 若对话框来自可见角色，再把 24×24 空心框移回角色格。 */
static int animate_dialog_box_close(fd2_vga *vga,
                                     const fd2_terrain_tileset *terrain,
                                     const fd2_field_map *map,
                                     const fd2_ui_sheet *ui,
                                     const fd2_map_sprite_bank *sprites,
                                     const fd2_scene_field_state *field_state,
                                     fd2_dialog_state *state,
                                     int fast) {
    if (!state || !state->is_open || !state->captured) return 0;

    for(int i=(int)state->captured-1;i>0;i--) {
        dialog_snapshot(vga,state,(unsigned)i,1);
        if(!fast) {
            if(dialog_present_delay(vga)<0)return -1;
            if (poll_scene_quit(vga)) break;
        }
    }
    dialog_snapshot(vga,state,0,1);
    if(!fast)
        if(animate_dialog_marker(vga, terrain, map, ui, sprites, field_state,
                              state->speaker_actor_idx, state->area, 1)<0)return -1;
    return 0;
}

static int pan_camera_to(fd2_vga *vga,
                          const fd2_terrain_tileset *terrain,
                          const fd2_field_map *map,
                          const fd2_map_sprite_bank *sprites,
                          fd2_scene_field_state *field_state,
                          int target_x,
                          int target_y,
                          int fast) {
    if (!field_state) return 0;
    /* field_camera_pan_to @0x135dd（原FUN_00010d25）入口清51A83。 */
    if (field_state->ordinary_cursor) field_state->cursor = NULL;
    if(field_state->motion_owner)field_state->motion_owner->ordinary_display_mode=0;
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
        if(present_base_frame(vga, terrain, map, sprites, field_state,0)<0)return -1;
        return 0;
    }

    /* field_camera_pan_to @0x135dd 先完整调整 X，再调整 Y；每格执行
     * 一次11cac(0)再4e381清普通FIFO；该CALL不是vsync等待。 */
    while (field_state->camera_cell_x != target_x) {
        int cx = field_state->camera_cell_x;
        int dx = cx < target_x ? 1 : -1;
        cx += dx;
        field_state->focus_cell_x += dx;
        set_camera_cell(field_state, map, cx, field_state->camera_cell_y);
        if(present_base_frame(vga, terrain, map, sprites,field_state,0)<0)return -1;
        if(scene_render_error)return -1;
        if(scene_host_quit_requested)return 0;
        fd2_input_clear_keys(&vga->input);
        if(poll_scene_quit(vga))return 0;

    }
    while (field_state->camera_cell_y != target_y) {
        int cy = field_state->camera_cell_y;
        int dy = cy < target_y ? 1 : -1;
        cy += dy;
        field_state->focus_cell_y += dy;
        set_camera_cell(field_state, map, field_state->camera_cell_x, cy);
        if(present_base_frame(vga, terrain, map, sprites,field_state,0)<0)return -1;
        if(scene_render_error)return -1;
        if(scene_host_quit_requested)return 0;
        fd2_input_clear_keys(&vga->input);
        if(poll_scene_quit(vga))return 0;

    }
    return 0;
}

static int focus_camera_to_cell(fd2_vga *vga,
                                  const fd2_terrain_tileset *terrain,
                                  const fd2_field_map *map,
                                  const fd2_map_sprite_bank *sprites,
                                  fd2_scene_field_state *field_state,
                                  int target_x, int target_y,
                                  int fast, uint32_t display_mode) {
    if (!field_state) return 0;

    int max_cam_x = map ? map->width - FD2_FIELD_VIEW_CELLS_X : 0;
    int max_cam_y = map ? map->height - FD2_FIELD_VIEW_CELLS_Y : 0;
    if (max_cam_x < 0) max_cam_x = 0;
    if (max_cam_y < 0) max_cam_y = 0;

    /* dialog_box_open @0x165ac 在从角色格展开对话框前自动调用
     * field_focus_move_to @0x12cea；按 actor 索引定位的等价包装为
     * field_focus_move_to_actor @0x12d7b。前者先逐格移动 X、再移动 Y；焦点
     * 向外达到 x=2/11、y=2/6 阈值后才同步卷动镜头。最终镜头位置
     * 取决于此前焦点，不等同于直接把角色固定到某个屏幕格。
     * 这不是额外的文本控制码，也不需要过场显式调用镜头指令。 */
    /* 12d01入口刷新即使零距离也发生；各步再按原51A83条件等待。 */
    if(!fast) {
        if(present_base_frame(vga,terrain,map,sprites,field_state,0)<0)return -1;
        if(scene_render_error)return -1;
        if(scene_host_quit_requested)return 0;
    }
    while (field_state->focus_cell_x != target_x) {
        int dx = field_state->focus_cell_x < target_x ? 1 : -1;
        int rel_x = field_state->focus_cell_x - field_state->camera_cell_x;
        int camera_x = field_state->camera_cell_x;
        if (dx < 0 && rel_x < 2 && camera_x > 0)
            camera_x--;
        else if (dx > 0 && rel_x >= 11 && camera_x < max_cam_x)
            camera_x++;
        int camera_changed = camera_x != field_state->camera_cell_x;
        field_state->focus_cell_x += dx;
        set_camera_cell(field_state, map, camera_x,
                        field_state->camera_cell_y);
        scene_motion_sync(field_state);
        if (!fast) {
            /* 12b98..12c43：mode0且镜头不动时只改焦点，不推进视觉。 */
            if(display_mode!=0 || camera_changed) {
                if(present_base_frame(vga,terrain,map,sprites,field_state,0)<0)return -1;
                if(scene_render_error)return -1;
        if(scene_host_quit_requested)return 0;
            }
            if(display_mode!=0 && display_mode!=6 && fd2_vga_wait_ticks(vga,1)<0)return -1;
            if(poll_scene_quit(vga))return 0;
        }
        fd2_input_clear_keys(&vga->input);
        if(poll_scene_quit(vga))return 0;
    }
    while (field_state->focus_cell_y != target_y) {
        int dy = field_state->focus_cell_y < target_y ? 1 : -1;
        int rel_y = field_state->focus_cell_y - field_state->camera_cell_y;
        int camera_y = field_state->camera_cell_y;
        if (dy < 0 && rel_y < 2 && camera_y > 0)
            camera_y--;
        else if (dy > 0 && rel_y >= 6 && camera_y < max_cam_y)
            camera_y++;
        int camera_changed = camera_y != field_state->camera_cell_y;
        field_state->focus_cell_y += dy;
        set_camera_cell(field_state, map, field_state->camera_cell_x,
                        camera_y);
        scene_motion_sync(field_state);
        if (!fast) {
            if(display_mode!=0 || camera_changed) {
                if(present_base_frame(vga,terrain,map,sprites,field_state,0)<0)return -1;
                if(scene_render_error)return -1;
        if(scene_host_quit_requested)return 0;
            }
            if(display_mode!=0 && display_mode!=6 && fd2_vga_wait_ticks(vga,1)<0)return -1;
            if(poll_scene_quit(vga))return 0;
        }
        fd2_input_clear_keys(&vga->input);
        if(poll_scene_quit(vga))return 0;
    }

    if (fast)
        if(present_base_frame(vga, terrain, map, sprites, field_state,0)<0)return -1;
    return 0;
}

static int focus_camera_on_actor(fd2_vga *vga,const fd2_terrain_tileset *terrain,
        const fd2_field_map *map,const fd2_map_sprite_bank *sprites,
        fd2_scene_field_state *state,int actor,int fast) {
    if (!state || actor<0 || (size_t)actor>=state->units.count) return 0;
    if(focus_camera_to_cell(vga,terrain,map,sprites,state,state->units.items[actor].x,
        state->units.items[actor].y,fast,0)<0)return -1;
    return 0;
}

static int redraw_dialog(fd2_vga *vga,
                          const fd2_terrain_tileset *terrain,
                          const fd2_field_map *map,
                          const fd2_archive *dato,
                          const fd2_ui_sheet *ui,
                          const fd2_map_sprite_bank *sprites,
                          const fd2_scene_field_state *field_state,
                          fd2_dialog_state *state) {
    (void)terrain;(void)map;(void)ui;(void)sprites;(void)field_state;
    state->is_open = 1;
    if (state->has_portrait &&
        draw_portrait(vga, dato, state->area, state->portrait_id) != 0)
        return -1;

    if (state->area == FD2_DIALOG_TOP) {
        state->x = DIALOG_TOP_TEXT_X;
        state->y = DIALOG_TOP_TEXT_Y;
    } else {
        state->x = DIALOG_BOTTOM_TEXT_X;
        state->y = DIALOG_BOTTOM_TEXT_Y;
    }
    state->line = 0;
    return 0;
}

static int wait_for_dialog_step(fd2_vga *vga, int *skip) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;
    if (!vga || !skip || *skip) return 0;
    if(fd2_vga_wait_ticks(vga,1)<0)return -1;
    if (fd2_input_take_quit(&vga->input)) {
        scene_host_quit_requested = 1;
        *skip = 1;
    }
    return 0;
}

static void dialog_scroll_text_up(fd2_vga *vga,
                                  fd2_dialog_state *state,
                                  int fast) {
    (void)fast;
    if (!vga || !state || !state->is_open) return;
    (void)fd2_dialog_scroll_pixels(vga->framebuffer,sizeof(vga->framebuffer),
        state->area==FD2_DIALOG_TOP?1832u:36887u);
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

/* 仅本次实际空等轮的H16节流；不借用/遗留其它owner的deadline。
 * timed present和wait均可接收HOST，仍在本次调用返回前恢复foreign值。 */
static int scene_present_idle(fd2_vga *vga) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;
    uint64_t saved_deadline=vga->frame_deadline_ns;
    uint64_t saved_interval=vga->frame_interval_ns;
    vga->frame_deadline_ns=0;vga->frame_interval_ns=0;
    int status=fd2_vga_present_timed(vga,16);
    if(status>=0)status=fd2_vga_wait_frame_deadline(vga);
    vga->frame_deadline_ns=saved_deadline;
    vga->frame_interval_ns=saved_interval;
    return status;
}

static int dialog_wait(fd2_vga *vga,
                               const fd2_archive *dato,
                               const fd2_ui_sheet *ui,
                               const fd2_dialog_state *state,
                               const fd2_scene_field_state *field_state,
                               int arrow_mode, int *skip) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;
    if (!vga || !ui || !state || !field_state || !skip ||
        (!*skip && !field_state->runtime.rng) || (arrow_mode!=0 && arrow_mode!=1)) return -1;
    if (fd2_input_take_quit(&vga->input)) {
        scene_host_quit_requested=1;*skip=1;return 0;
    }
    if (*skip) return 0;
    int arrow_x = state->area == FD2_DIALOG_TOP ? 111 : 191;
    int arrow_y = state->area == FD2_DIALOG_TOP ? 71 : 181;
    fd2_dialog_wait_state wait;uint64_t actual;
    if(scene_clock_ns(vga,&actual)<0)return -1;
    if(fd2_dialog_wait_begin(&wait,actual,arrow_mode,
            field_state->runtime.rng,field_state->runtime.rng_userdata)!=0) return -1;
    if(arrow_mode==1)draw_continuation_arrow_frame(vga,ui,18,arrow_x,arrow_y);
    /* dialog_wait_key @0x16c57：入口一次RNG，只有空等键轮推进water与
     * portrait blink，不额外推进1297d或127e0的idle/raw26时钟。 */
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) {
            scene_host_quit_requested = 1;
            *skip = 1;
            return 0;
        }
        fd2_input_event event;
        if (fd2_input_take_key(&vga->input, &event)) break;
        if(fd2_field_water_tick_live(vga)<0)return -1;
        fd2_dialog_wait_visual visual;
        if(scene_clock_ns(vga,&actual)<0)return -1;
        int updated=fd2_dialog_wait_step(&wait,actual,&visual);
        if(updated<0)return -1;
        if(visual.arrow>=0)
            draw_continuation_arrow_frame(vga,ui,visual.arrow,arrow_x,arrow_y);
        if(visual.portrait>=0 && state->has_portrait &&
            draw_portrait_frame(vga,dato,state->area,state->portrait_id,
                (unsigned)visual.portrait)!=0) return -1;
        if(updated) {
            if(visual.portrait==0)wait.countdown=
                field_state->runtime.rng(field_state->runtime.rng_userdata)%30u+2u;
            if(scene_clock_tick(vga,&wait.last_tick)<0)return -1;
        }
        if(scene_present_idle(vga)<0)return -1;
    }
    if(arrow_mode==1)blit_ui_tile(vga,ui,13,arrow_x,arrow_y-5);
    return 0;
}

static int newline_or_scroll(fd2_vga *vga,
                              const fd2_archive *dato,
                              const fd2_ui_sheet *ui,
                              fd2_dialog_state *state,
                              const fd2_scene_field_state *field_state,
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

    if (transition.wait == FD2_DIALOG_FLOW_WAIT_SCROLL && !fast) {
        if(state->has_portrait && draw_portrait(vga,dato,state->area,state->portrait_id)!=0)
            return -1;
        return dialog_wait(vga,dato,ui,state,field_state,1,skip);
    }
    return 0;
}

static int set_dialog_area(fd2_vga *vga,
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
        if (!fast && (draw_portrait(vga,dato,state->area,state->portrait_id)!=0 ||
                dialog_wait(vga,dato,ui,state,field_state,0,skip)!=0)) return -1;
        if (skip && *skip) return 0;
        if(animate_dialog_box_close(vga, terrain, map, ui, sprites, field_state,
                                 state, fast)<0)return -1;
    }

    /* dialog_box_open @0x165ac（原FUN_00013cf4）的a3非零分支：
     * 聚焦前mode0、之后mode1；直接raw说话人即使hidden也走此分支。 */
    int tracks_display = field_state && field_state->ordinary_cursor &&
        speaker_actor_idx >= 0 &&
        (size_t)speaker_actor_idx < field_state->units.count;
    if (tracks_display) {
        field_state->cursor = NULL;
        field_state->dialog_cursor_suppressed = 1;
        if(field_state->motion_owner)field_state->motion_owner->ordinary_display_mode=0;
    }
    if(focus_camera_on_actor(vga, terrain, map, sprites, field_state,
                          speaker_actor_idx, fast)<0)return -1;
    if(scene_render_error)return -1;
    if(scene_host_quit_requested){if(skip)*skip=1;return 0;}
    if (tracks_display) {
        field_state->cursor = field_state->ordinary_cursor;
        if(field_state->motion_owner)field_state->motion_owner->ordinary_display_mode=1;
    }
    state->area = area;
    state->portrait_id = portrait_id;
    state->has_portrait = 1;
    state->speaker_actor_idx = speaker_actor_idx;
    if(animate_dialog_box_open(vga, terrain, map, ui, sprites, field_state,
                            state,speaker_actor_idx, area, fast)<0)return -1;
    if(scene_host_quit_requested) {
        if(state->captured)dialog_snapshot(vga,state,0,1);
        if(skip)*skip=1;
        state->is_open=0;
        return 0;
    }
    if (redraw_dialog(vga, terrain, map, dato, ui, sprites, field_state, state) != 0)
        return -1;
    if(scene_present_copy(vga)<0)return -1;
    return 0;
}

static int scene_dynamic_text(const fd2_scene_field_state *field_state,
    const fd2_font *font, int16_t control, fd2_text_expanded *expanded) {
    if(!field_state || !font || !expanded) return -1;
    if(control==-6)return fd2_text_expand_number(field_state->runtime.text.value,
        font->glyph_count,expanded);
    if((control!=-4 && control!=-5) || !field_state->names) return -1;
    size_t fragment=control==-4?field_state->runtime.text.new_fragment:
        field_state->runtime.text.old_fragment;
    if(fd2_text_expand(field_state->names,font->glyph_count,
            &field_state->runtime.text,fragment,expanded)!=0) return -1;
    /* 正常名称域：1..136/150..178/181..395/441..476原UI0均无控制符。
     * 不能把SAFE_MINUS生成标记重新当原-17 portrait，也不截长Scene正文。 */
    for(size_t i=0;i<expanded->count;i++)
        if(expanded->tokens[i]>=font->glyph_count &&
            expanded->tokens[i]!=FD2_TEXT_SAFE_MINUS) return -1;
    return 0;
}

static int scene_fragment_preflight(const fd2_archive *dato,
    const fd2_ui_sheet *ui, const fd2_font *font, const uint8_t *tokens,
    size_t count, const fd2_scene_field_state *field_state) {
    if(!ui || !font || !font->glyphs || !tokens || !field_state)
        return -1;
    for(size_t i=0;i<count;i++) {
        int16_t token=(int16_t)fd2_text_token_at(tokens,i);
        if(token==-1)return 0;
        if(token==-2 || token==-3)continue;
        if(token>=0) {if((size_t)token>=font->glyph_count)return -1;continue;}
        if(token>=-6 && token<=-4) {
            fd2_text_expanded expanded;
            if(scene_dynamic_text(field_state,font,token,&expanded)!=0)return -1;
            for(size_t j=0;j<expanded.count;j++) if(expanded.tokens[j]==FD2_TEXT_SAFE_MINUS) {
                fd2_image minus={0};
                int valid=ui_sheet_decode_rle(ui,132,&minus)==0 &&
                    minus.width>=1 && minus.width<=16 && minus.height>=1 && minus.height<=16;
                fd2_image_free(&minus);if(!valid)return -1;
            }
            continue;
        }
        if(token<-20 || token>-17 || ++i>=count)return -1;
        if(!dato)return -1; /* 只有真正portrait控制才需要DATO。 */
        uint16_t argument=fd2_text_token_at(tokens,i);
        if((token==-19 || token==-20) && argument>=field_state->units.count)return -1;
        uint16_t portrait=resolve_portrait_control(field_state,token,argument);
        for(unsigned frame=0;frame<4;frame++) {
            fd2_image image={0};
            int valid=fd2_portrait_decode_frame(&image,dato,portrait,frame)==0 &&
                image.width==80 && image.height==80;
            fd2_image_free(&image);
            if(!valid)return -1;
        }
    }
    return -1;
}

static int scene_text_glyph(fd2_vga *vga, const fd2_archive *dato,
    const fd2_ui_sheet *ui, const fd2_font *font, fd2_dialog_state *state,
    uint16_t glyph, int background, int *slow, int *skip,
    fd2_field_audio *audio) {
    if(!vga || fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    int64_t offset=(int64_t)state->y*VGA_STRIDE+state->x;
    uint8_t raw_background=(uint8_t)(background<0?0:background);
    if(glyph==FD2_TEXT_SAFE_MINUS) {
        /* 原负-6在font首LODSW越font owner；这里只保之前的清底前缀，
         * 后画安全符号是有限H，不以glyph10冒充原负索引的字形。 */
        if(fd2_font_draw_glyph_linear(vga->framebuffer,sizeof(vga->framebuffer),font,
                10u,offset,VGA_STRIDE,205,76,raw_background)!=0)return -1;
        fd2_image minus={0};if(ui_sheet_decode_rle(ui,132,&minus)!=0)return -1;
        fd2_map_sprite_blit(vga,&minus,state->x,state->y,0);fd2_image_free(&minus);
    } else if(fd2_font_draw_glyph_linear(vga->framebuffer,sizeof(vga->framebuffer),font,
            glyph,offset,VGA_STRIDE,205,76,raw_background)!=0)return -1;
    state->x+=16;
    if(scene_present_copy(vga)<0)return -1;
    if(scene_host_quit_requested || fd2_input_take_quit(&vga->input)) {
        scene_host_quit_requested=1;*skip=1;return 0;
    }
    if(fd2_input_has_any_key(&vga->input))*slow=0;
    if(*slow) {
        unsigned phase;
        int redraw=fd2_dialog_visual_glyph_step(&vga->dialog_visual,&phase);
        if(redraw<0 || (redraw && state->has_portrait &&
                draw_portrait_frame(vga,dato,state->area,state->portrait_id,phase)!=0))
            return -1;
        if(redraw && state->has_portrait) {
            fd2_vga_upload_current_frame(vga);
            if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
        }
        if(audio && fd2_field_audio_play(audio,FD2_FIELD_SFX_DIALOG_GLYPH)<0)
            return -1;
        if(wait_for_dialog_step(vga,skip)<0)return -1;
    }
    return 0;
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
    if ((!fast && (!field_state || !field_state->runtime.rng)) ||
        fd2_text_entry_get_fragment(text, fragment_idx, &tokens, &token_count) != 0 ||
        scene_fragment_preflight(dato,ui,font,tokens,token_count,field_state)!=0)
        return -1;

    fd2_dialog_state state;
    memset(&state, 0, sizeof(state));
    state.area = FD2_DIALOG_BOTTOM;
    state.speaker_actor_idx = -1;
    /* 15f84消费caller当前VGA；它本身不额外11cac，不把UI背景写回page。 */

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
            if (set_dialog_area(vga, terrain, map, dato, ui, sprites, field_state,
                                &state, area, portrait_id, speaker_actor_idx, fast,
                                &skip) != 0)
                return -1;
            if(scene_render_error)return -1;
            glyph_step_enabled = !fast;
            /* 原版打开新说话人后直接继续 token 循环；不存在额外 250 ms
             * 固定等待。首个字形仍由 glyph-step 的 1 BIOS tick 节奏控制。 */
            continue;
        }

        if (tok == FD2_TEXT_NEWLINE || tok == FD2_TEXT_PAGE) {
            int restores_glyph_step = tok == FD2_TEXT_PAGE;
            if(newline_or_scroll(vga,dato,ui,&state,field_state,
                    tok==FD2_TEXT_PAGE,fast,&skip)!=0)return -1;
            if (restores_glyph_step && !skip) glyph_step_enabled = !fast;
            if(scene_present_copy(vga)<0)return -1;
            continue;
        }

        if (tok>=-6 && tok<=-4) {
            fd2_text_expanded expanded;
            if(scene_dynamic_text(field_state,font,tok,&expanded)!=0)return -1;
            /* -4/-5递归调用15f84强制slow1/bg74；返回不覆盖父a10。
             * -6继承父样式/slow。子串有来源标记，不再走portrait控制parser。 */
            int nested_slow=!fast;
            int *slow=tok==-6?&glyph_step_enabled:&nested_slow;
            for(size_t j=0;j<expanded.count && !skip;j++)
                if(scene_text_glyph(vga,dato,ui,font,&state,expanded.tokens[j],
                        tok==-6?-1:74,slow,&skip,audio)!=0)return -1;
            continue;
        }
        if(tok<0)return -1;
        /* text_dialog_render_tokens @code0 0x5f84 不做按对话框宽度自动折行，
         * 只在 FDTXT token -2/-3 出现时推进一行。此前这里额外自动折行，
         * 会让恰好排到行尾后紧跟 -2 的文本一次换两行。 */
        if(scene_text_glyph(vga,dato,ui,font,&state,(uint16_t)tok,-1,
                &glyph_step_enabled,&skip,audio)!=0)return -1;
    }

    if (!fast && !skip && state.is_open &&
        (draw_portrait(vga,dato,state.area,state.portrait_id)!=0 ||
         dialog_wait(vga,dato,ui,&state,field_state,0,&skip)!=0))return -1;
    if(animate_dialog_box_close(vga, terrain, map, ui, sprites, field_state,
                             &state, fast)<0)return -1;
    state.is_open = 0;
    if (field_state) field_state->dialog_cursor_suppressed = 0;
    return 0;
}

/* DAT_000027d8 中新游戏开场会用到的移动脚本。字节流按
 * field_movement_script_play @0x1366a 的原始格式登记：group_count，
 * 然后是 step_or_mode、actor_count、(actor_id,direction)。数据来自未
 * patch FD2.EXE 的 object3 初始化区与对应 LE relocation。 */

static const uint8_t k_walk_phases[] = {1, 2, 1, 0, 1, 2};

static int opening_footstep_play(fd2_vga *vga,fd2_field_audio *audio,
                                  const fd2_scene_field_state *state,
                                  size_t actor_idx) {
    if (!audio || !state || actor_idx >= state->units.count)
        return 0;

    const fd2_field_unit *actor = &state->units.items[actor_idx];
    (void)fd2_field_audio_play_footstep(
        audio, actor->unit_id, actor->movement_profile, actor->race);
    return (fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)?-1:0;
}

static void opening_move_one(fd2_field_unit *actor, uint8_t direction) {
    if (!actor) return;
    actor->direction = direction;
    switch (actor->direction) {
        case 0: actor->y++; break;
        case 1: actor->x--; break;
        case 3: actor->x++; break;
        default: actor->y--; break;
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
     * field_actor_move_up_follow_camera @0x13185。该函数在角色靠近视窗
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
                if(opening_footstep_play(vga,audio,state,actor_idx)<0)return -1;
                if(fd2_field_water_tick_live(vga)<0)return -1;
                state->units.walk_frames[actor_idx] = k_walk_phases[phase];
                actor->frame_phase = (uint8_t)phase + 1u;
                state->camera_offset_y = camera_dy * (int)(phase + 1u) * 4;
                scene_motion_sync(state);
                if(fd2_field_visual_actor_step_live(vga,&vga->field_visual)<0 ||
                    fd2_field_visual_terrain_step_live(vga,&vga->field_visual)<0)
                    return -1;
                /* 13185先铺上沿13×9 margin，再移动投影源；格camera仅末尾写。 */
                if(!vga->field_page ||
                    fd2_field_background_draw_live(vga,vga->field_page,72,
                        72+camera_dy*(int)(phase+1u)*4,state->camera_cell_x,
                        state->camera_cell_y>0?state->camera_cell_y-1:0,0,
                        state->camera_cell_y>0?6+camera_dy*(int)(phase+1u):0,
                        1)!=0 ||
                    fd2_field_visual_range_step_live(vga,&vga->field_visual,-1)<0 ||
                    fd2_terrain_render_page_block(vga->field_page,terrain,map,
                        72,state->camera_cell_y>0?48:72,13,9,
                        state->camera_cell_x,state->camera_cell_y>0?
                        state->camera_cell_y-1:0,(int)vga->field_visual.terrain_phase,
                        (uint8_t)vga->field_visual.actor_idle_phase,NULL,scene_field_range_lut(state,vga))!=0)
                    return -1;
                if(fd2_field_units_render_page_live(vga,vga->field_page,sprites,&state->units,
                        state->camera_cell_x,state->camera_cell_y,(uint8_t)vga->field_visual.actor_idle_phase,
                        (uint8_t)vga->field_visual.cursor_phase)!=0 ||
                    fd2_field_units_render_page_terrain_overlays(vga->field_page,
                        terrain,map,&state->units,state->camera_cell_x,
                        state->camera_cell_y,(int)vga->field_visual.terrain_phase,NULL,scene_field_range_lut(state,vga))!=0 ||
                    fd2_field_page_project(vga->field_page,72,
                        72+camera_dy*(int)(phase+1u)*4,vga->framebuffer,
                        sizeof(vga->framebuffer))!=0) return -1;
                if(scene_present_copy(vga)<0)return -1;
                int waited=fd2_vga_wait_movement_tick(vga);
                if(waited>0){scene_host_quit_requested=1;return -1;}
                if(waited<0){scene_render_error=1;return -1;}
                if(poll_scene_quit(vga))return -1;

            }
        }

        opening_move_one(actor, 2);
        state->units.walk_frames[actor_idx] = 1;
        actor->frame_phase = 0;
        state->focus_cell_y--;
        set_camera_cell(state, map, state->camera_cell_x,
                        state->camera_cell_y + camera_dy);
        scene_motion_sync(state);
    }
    return 0;
}

static int movement_script_preflight_domain(const fd2_field_units *units,
        const uint8_t *script, size_t script_len, uint8_t idle_phase,size_t *defined_groups) {
    if (!units || !script || !script_len) return -1;
    /* 格式全检；source只校验首未构造身份读之前的完整组及其资源。
     * 此处不写热状态，实际演出按原顺序提交已定义前缀后才停。 */
    fd2_field_units checked = *units;
    int result=defined_groups ? fd2_field_movement_script_project_source(&checked,script,script_len,defined_groups):
        fd2_field_movement_script_apply(&checked,script,script_len);
    if (result<0) return -1;
    size_t check_at=1;
    size_t groups=defined_groups?*defined_groups:script[0];
    for (size_t group=0;group<groups;group++) {
        uint8_t mode=script[check_at++],count=script[check_at++];
        for (size_t i=0;i<count;i++,check_at+=2) {
            size_t actor=script[check_at];uint8_t direction=script[check_at+1u];
            if (mode && direction>3u) {
                fd2_field_units view=*units;
                view.items[actor].direction=direction;
                if (fd2_field_unit_cached_frame_index(&view,actor,idle_phase)==SIZE_MAX) return -1;
            }
        }
    }
    return result;
}

static int movement_script_preflight(const fd2_field_units *units,
        const uint8_t *script,size_t script_len,uint8_t idle_phase) {
    return movement_script_preflight_domain(units,script,script_len,idle_phase,NULL);
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
                                    int fast,int source) {
    if (!state || !script || script_len == 0) return -1;
    size_t defined_groups=script[0];
    if (movement_script_preflight_domain(&state->units,script,script_len,
            (uint8_t)vga->field_visual.actor_idle_phase,source?&defined_groups:NULL)<0) return -1;
    size_t pos = 1;
    uint8_t group_count = script[0];
    int darkness = 1;

    for (uint8_t group = 0; group < group_count; group++) {
        /* 原normal组先32230/1f1a3，首未构造身份前没有DIR/frame写。
         * 保此前完整组；不把截断前缀当完整脚本再补1398a尾刷新。 */
        if (group==defined_groups) {scene_motion_sync(state);return -1;}
        if (pos + 2 > script_len) return -1;
        uint8_t mode = script[pos++];
        uint8_t actor_count = script[pos++];
        if (pos + (size_t)actor_count * 2u > script_len) return -1;
        const uint8_t *pairs = script + pos;
        pos += (size_t)actor_count * 2u;

        for (uint8_t i = 0; i < actor_count; i++) {
            uint8_t actor_id = pairs[i * 2];
            if (mode && actor_id < state->units.count)
                state->units.items[actor_id].direction = pairs[i * 2 + 1];
        }
        /* 原DIR为同一热记录；jump首wait之前也必须交回真实owner。 */
        scene_motion_sync(state);

        if (mode & 0x80u) {
            int frames = mode & 0x7fu;
            if (frames == 0) {
                /* 1366a/1374b：原工作页指针-5472，即所选actor临时上抬12px。
                 * 按raw顺序画全表（不是普通深度排序），位置和frame_phase不写回。 */
                if (!fast) {
                    if(fd2_vga_wait_ticks(vga,1)<0)return -1;
                    if(poll_scene_quit(vga))return -1;
                    int camera_x=state->camera_cell_x,camera_y=state->camera_cell_y;
                    if(fd2_field_visual_terrain_step_live(vga,&vga->field_visual)<0)
                        return -1;
                    if(!vga->field_page ||
                        fd2_field_background_draw_live(vga,vga->field_page,72,72,
                            camera_x,camera_y,0,0,1)!=0 ||
                        fd2_field_visual_range_step_live(vga,&vga->field_visual,-1)<0 ||
                        fd2_terrain_render_page_block(vga->field_page,terrain,map,
                            72,72,13,8,camera_x,camera_y,
                            (int)vga->field_visual.terrain_phase,(uint8_t)vga->field_visual.actor_idle_phase,
                            NULL,scene_field_range_lut(state,vga))!=0) return -1;
                    for (size_t raw=0;raw<state->units.count;raw++) {
                        /* 1366a@13744/13748先筛hidden，1374b才进入direct读钟。 */
                        if(fd2_field_unit_is_hidden(&state->units.items[raw]))continue;
                        int selected=0;
                        for (size_t i=0;i<actor_count;i++) if (pairs[i*2u]==raw) selected=1;
                        if(fd2_field_units_render_page_actor_live(vga,vga->field_page,sprites,
                                &state->units,raw,camera_x,camera_y,(uint8_t)vga->field_visual.actor_idle_phase,
                                (uint8_t)vga->field_visual.cursor_phase,selected?-12:0)!=0) return -1;
                    }
                    if(fd2_field_units_render_page_terrain_overlays(vga->field_page,
                            terrain,map,&state->units,camera_x,camera_y,
                            (int)vga->field_visual.terrain_phase,NULL,scene_field_range_lut(state,vga))!=0 ||
                        fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,
                            sizeof(vga->framebuffer))!=0) return -1;
                    if(scene_present_copy(vga)<0)return -1;
                    if(fd2_vga_wait_ticks(vga,2)<0)return -1;
                    if(poll_scene_quit(vga))return -1;
                    if(present_base_frame(vga,terrain,map,sprites,state,0)<0)return -1;
                    if(scene_render_error || scene_host_quit_requested)return -1;
                    fd2_input_clear_keys(&vga->input);
                    if(poll_scene_quit(vga))return -1;
                }
                continue;
            }
            if (!fast) {
                for (int frame = 0; frame < frames; frame++) {
                    if(present_base_frame(vga,terrain,map,sprites,state,0)<0)return -1;
                    if(scene_render_error || scene_host_quit_requested)return -1;
                    if(fd2_vga_wait_ticks(vga,1)<0)return -1;
                    if(poll_scene_quit(vga))return -1;
                    fd2_input_clear_keys(&vga->input);
                    if(poll_scene_quit(vga))return -1;

                }
            }
            continue;
        }

        for (uint8_t step = 0; step < mode; step++) {
            if (!fast) {
                for (size_t phase = 0; phase < sizeof(k_walk_phases); phase++) {
                    if(actor_count && opening_footstep_play(vga,audio,state,pairs[0])<0)return -1;
                    for (uint8_t i = 0; i < actor_count; i++) {
                        uint8_t actor_id = pairs[i * 2];
                        if (actor_id < state->units.count) {
                            state->units.walk_frames[actor_id] = k_walk_phases[phase];
                            state->units.items[actor_id].frame_phase = (uint8_t)phase + 1u;
                        }
                    }
                    /* 原版只写脚本中 actor 的 record+4；卫兵等静止角色
                     * 不能跟随移动角色一起切换步行动画。 */
                    /* 138b8..138f3：只在53AFB非0/0x40的渐暗相位跳水色。 */
                    int fading=fade_out && darkness!=0 && darkness!=0x40;
                    render_scene_11cac(vga, terrain, map, sprites, state,fading);
                    if(scene_render_error || scene_host_quit_requested)return -1;
                    if (fading) {
                        if (darkness < 0x40) darkness++;
                        fd2_vga_set_brightness(vga, darkness);
                    }
                    /* 138fd wait1后13905是FIFO clear；无vsync/渲染补偿。 */
                    if(scene_present_copy(vga)<0)return -1;
                    if(fd2_vga_wait_ticks(vga,1)<0)return -1;
                    if(scene_render_error || poll_scene_quit(vga))return -1;
                    fd2_input_clear_keys(&vga->input);
                    if(poll_scene_quit(vga))return -1;

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
            scene_motion_sync(state);
            /* phase 6 的 +24 px 与提交后的格坐标画面相同；非 fast 路径
             * 不再重复 present，以免重置下一相位的 deadline。 */
            if (fast)
                if(present_base_frame(vga, terrain, map, sprites, state,1)<0)return -1;
        }
    }
    if(pos!=script_len)return -1;
    /* 1398a..1398c：清record+4后的脚本尾仍11cac(1)，与phase6不是同一写点。 */
    if(present_base_frame(vga,terrain,map,sprites,state,1)<0)return -1;
    return scene_render_error || scene_host_quit_requested?-1:0;
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

static int opening_delay(fd2_vga *vga,
                          const fd2_terrain_tileset *terrain,
                          const fd2_field_map *map,
                          const fd2_map_sprite_bank *sprites,
                          fd2_scene_field_state *state,
                          int ms,
                          int fast) {
    (void)terrain;(void)map;(void)sprites;(void)state;
    if(scene_host_quit_requested || poll_scene_quit(vga))return 0;
    if (!fast) {
        if(fd2_vga_delay_ms(vga,ms)<0)return -1;
        (void)poll_scene_quit(vga);
    }
    return 0;
}

/* 4ebab只跳颜色0；固定工作页LMI叶，不把VGA/UI反馈到App。 */
static int scene_page_lmi(fd2_field_page *page,const fd2_image *image,int x,int y) {
    if(!page || !image || !image->pixels || image->width<=0 || image->height<=0)return -1;
    int left=x<0?-x:0,right=image->width;
    if(right>FD2_FIELD_PAGE_STRIDE-x)right=FD2_FIELD_PAGE_STRIDE-x;
    if(left>=right)return 0;
    uint8_t row[FD2_FIELD_PAGE_STRIDE];
    for(int sy=0;sy<image->height;sy++) {
        int dy=y+sy;if(dy<0 || dy>=FD2_FIELD_PAGE_HEIGHT)continue;
        size_t width=(size_t)(right-left);
        if(fd2_field_page_read(page,x+left,dy,(int)width,1,row,sizeof(row),width)!=0)return -1;
        for(int sx=left;sx<right;sx++) {
            uint8_t color=image->pixels[(size_t)sy*(size_t)image->width+(size_t)sx];
            if(color)row[sx-left]=color;
        }
        if(fd2_field_page_write(page,x+left,dy,(int)width,1,row,sizeof(row),width)!=0)return -1;
    }
    return 0;
}

static int arrival_page_actors(fd2_vga *vga,const fd2_terrain_tileset *terrain,
    const fd2_field_map *map,const fd2_map_sprite_bank *sprites,
    const fd2_field_units *units,const fd2_scene_field_state *state,size_t first,int offset) {
    for(size_t raw=0;raw<units->count;raw++) {
        if(fd2_field_unit_is_hidden(&units->items[raw]))continue;
        if(fd2_field_units_render_page_actor_live(vga,vga->field_page,sprites,units,raw,
                state->camera_cell_x,state->camera_cell_y,
                (uint8_t)vga->field_visual.actor_idle_phase,(uint8_t)vga->field_visual.cursor_phase,
                raw>=first?offset:0)!=0)return -1;
    }
    return fd2_field_units_render_page_terrain_overlays(vga->field_page,terrain,map,units,
        state->camera_cell_x,state->camera_cell_y,(int)vga->field_visual.terrain_phase,NULL,scene_field_range_lut(state,vga));
}

struct fd2_scene_arrival_visual {
    fd2_image images[12];
    fd2_field_page *base;
    fd2_field_page *owner;
    size_t first;
    int started;
};

static int arrival_node_valid(const fd2_static_flow_node *node) {
    if(!node || !node->op || !(node->views&FD2_FLOW_PRESENT))return 0;
    if(!strcmp(node->op,"arrival_frame"))return node->args[0]>=0 && node->args[0]<12;
    if(!strcmp(node->op,"arrival_postbase"))return node->args[0]>=6 && node->args[0]<=8;
    if(!strcmp(node->op,"arrival_wait_ticks"))return node->args[0]==1;
    return !strcmp(node->op,"arrival_cue") || !strcmp(node->op,"arrival_project") ||
        !strcmp(node->op,"arrival_clear") || !strcmp(node->op,"arrival_done");
}

static int arrival_flow_ready(const fd2_static_flow *flow) {
    if(!flow || !flow->count || flow->count>254u)return 0;
    for(size_t i=0;i<flow->count;i++)if(!arrival_node_valid(&flow->nodes[i]))return 0;
    int pc=flow->entry;
    for(unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if(!node)return 0;
        if(!strcmp(node->op,"arrival_done"))return 1;
        pc=fd2_static_flow_edge(flow,node,FD2_FLOW_NEXT);
    }
    return 0;
}

/* 原32999先载95/9，再分配和保存整页；准备不触及Game或App像素。 */
static int arrival_images_open(fd2_scene_arrival_visual *scope,const fd2_archive *fdother) {
    const uint8_t *data;size_t size;
    fd2_pcm_bank bank={0};
    if(!scope || !fdother || fd2_pcm_bank_open(&bank,fdother,95)!=0)return -1;
    const uint8_t *sample;size_t sample_size;
    int valid=fd2_pcm_bank_count(&bank)>0 && fd2_archive_get(&bank.archive,0,&sample,&sample_size)==0 && sample_size>0;
    fd2_pcm_bank_close(&bank);
    if(!valid)return -1;
    if (fd2_archive_get(fdother, 9, &data, &size) != 0 ||
        size < 6 || memcmp(data, "LMI1", 4) != 0)
        return -1;
    uint16_t frame_count = rd_u16_le(data + 4);
    if (frame_count < 12 || 6u + (size_t)frame_count * 4u > size)
        return -1;

    for (uint16_t frame_idx = 0; frame_idx < 12; frame_idx++) {
        uint32_t start = rd_u32_le(data + 6 + (size_t)frame_idx * 4u);
        uint32_t end = frame_idx + 1 < frame_count
                     ? rd_u32_le(data + 6 + (size_t)(frame_idx + 1) * 4u)
                     : (uint32_t)size;
        if (start<6u+(size_t)frame_count*4u || start >= end || end > size ||
            fd2_lmi_decode_image(&scope->images[frame_idx], data + start, end - start) != 0)return -1;
    }
    return 0;
}

void fd2_scene_arrival_visual_close(fd2_scene_arrival_visual **scope) {
    if(!scope || !*scope)return;
    fd2_scene_arrival_visual *owned=*scope;*scope=NULL;
    for(size_t i=0;i<12;i++)fd2_image_free(&owned->images[i]);
    fd2_field_page_destroy(owned->base);free(owned);
}

int fd2_scene_arrival_visual_validate(const fd2_archive *fdother) {
    fd2_scene_arrival_visual *scope=calloc(1,sizeof(*scope));
    if(!scope)return -1;
    int result=arrival_flow_ready(fd2_static_flow_lookup("runtime","runtime.arrival"))?
        arrival_images_open(scope,fdother):-1;
    fd2_scene_arrival_visual_close(&scope);return result;
}

int fd2_scene_arrival_visual_prepare(fd2_scene_arrival_visual **scope,
    fd2_vga *vga,const fd2_archive *fdother,size_t first) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if(!scope || *scope || !vga || !vga->field_page || !fdother || first>FD2_FIELD_MAX_UNITS ||
        !arrival_flow_ready(fd2_static_flow_lookup("runtime","runtime.arrival")))return -1;
    fd2_scene_arrival_visual *owned=calloc(1,sizeof(*owned));
    if(!owned)return -1;
    if(arrival_images_open(owned,fdother)!=0 ||
        (owned->base = fd2_field_page_create()) == NULL ||
        fd2_field_page_copy_pixels(owned->base,vga->field_page)!=0) {
        fd2_scene_arrival_visual_close(&owned);return -1;
    }
    if(poll_scene_quit(vga)) {fd2_scene_arrival_visual_close(&owned);return FD2_SCENE_RESULT_HOST_QUIT;}
    owned->first=first;owned->owner=vga->field_page;*scope=owned;return 0;
}

static int arrival_visual_play_view(fd2_scene_arrival_visual *scope,fd2_vga *vga,
    const fd2_terrain_tileset *terrain,const fd2_field_map *map,
    const fd2_map_sprite_bank *sprites,const fd2_scene_field_state *state,
    fd2_field_audio *audio,int fast) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    const fd2_static_flow *flow=fd2_static_flow_lookup("runtime","runtime.arrival");
    if(!scope || !scope->base || !vga || scope->owner!=vga->field_page || !terrain || !map || !sprites ||
        !state || state->units.count>FD2_FIELD_MAX_UNITS || scope->first>state->units.count || scope->started ||
        !arrival_flow_ready(flow))return -1;
    if(poll_scene_quit(vga))return FD2_SCENE_RESULT_HOST_QUIT;
    scope->started=1;int pc=flow->entry;
    for(unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if(!arrival_node_valid(node))return -1;
        if(poll_scene_quit(vga))return FD2_SCENE_RESULT_HOST_QUIT;
        if(!strcmp(node->op,"arrival_done"))return 0;
        if(!strcmp(node->op,"arrival_cue")) {
            if(audio){(void)fd2_field_audio_play(audio,FD2_FIELD_SFX_ACTOR_GROUP_ARRIVAL);if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;}
        } else if(!strcmp(node->op,"arrival_frame")) {
            unsigned frame_idx=(unsigned)node->args[0];
            if(fd2_field_page_copy_pixels(vga->field_page,scope->base)!=0)return -1;
            for(size_t raw=scope->first;raw<state->units.count;raw++) {
                const fd2_field_unit *u=&state->units.items[raw];
                if((int)u->x<state->camera_cell_x-1 || (int)u->x>state->camera_cell_x+13 ||
                    (int)u->y<state->camera_cell_y || (int)u->y>state->camera_cell_y+9)continue;
                if(scene_page_lmi(vga->field_page,&scope->images[frame_idx],
                    72+((int)u->x-state->camera_cell_x-1)*24,
                    66+((int)u->y-state->camera_cell_y)*24)!=0)return -1;
            }
        } else if(!strcmp(node->op,"arrival_project")) {
            if(fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer))!=0)return -1;
            fd2_vga_upload_current_frame(vga);
            if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
        } else if(!strcmp(node->op,"arrival_postbase")) {
            unsigned frame_idx=(unsigned)node->args[0];
            if(frame_idx==6) {
                if(fd2_field_page_copy_pixels(vga->field_page,scope->base)!=0)return -1;
            } else if(fd2_field_visual_terrain_step_live(vga,&vga->field_visual)<0 ||
                fd2_field_background_draw_live(vga,vga->field_page,72,72,state->camera_cell_x,
                    state->camera_cell_y,0,0,1)!=0 ||
                fd2_field_visual_range_step_live(vga,&vga->field_visual,-1)<0 ||
                fd2_terrain_render_page_block(vga->field_page,terrain,map,72,72,13,8,
                    state->camera_cell_x,state->camera_cell_y,(int)vga->field_visual.terrain_phase,
                    (uint8_t)vga->field_visual.actor_idle_phase,NULL,scene_field_range_lut(state,vga))!=0)return -1;
            if(arrival_page_actors(vga,terrain,map,sprites,&state->units,state,scope->first,
                frame_idx==6?-8:frame_idx==7?-5:0)!=0 ||
                fd2_field_page_copy_pixels(scope->base,vga->field_page)!=0)return -1;
        } else if(!strcmp(node->op,"arrival_clear")) {
            fd2_input_clear_keys(&vga->input);
        } else if(!strcmp(node->op,"arrival_wait_ticks")) {
            if(!fast && fd2_vga_wait_ticks(vga,(uint32_t)node->args[0])<0)return -1;
        }
        if(poll_scene_quit(vga))return FD2_SCENE_RESULT_HOST_QUIT;
        pc=fd2_static_flow_edge(flow,node,FD2_FLOW_NEXT);
    }
    return -1;
}

int fd2_scene_arrival_visual_play(fd2_scene_arrival_visual *scope,fd2_vga *vga,
    const fd2_field_game *game,fd2_field_audio *audio,int fast) {
    if(!game || !game->ready)return -1;
    fd2_scene_field_state state={0};state.units=game->units;
    state.camera_cell_x=game->camera_cell_x;state.camera_cell_y=game->camera_cell_y;
    return arrival_visual_play_view(scope,vga,&game->terrain,&game->map,&game->sprites,&state,audio,fast);
}

static int opening_actor_arrival(fd2_vga *vga,const fd2_archive *fdother,
    const fd2_terrain_tileset *terrain,const fd2_field_map *map,
    const fd2_map_sprite_bank *sprites,fd2_scene_field_state *state,
    fd2_field_audio *audio,size_t first_actor,size_t last_actor,int fast) {
    if(!vga || !fdother || !terrain || !map || !sprites || !state ||
        state->units.count>FD2_FIELD_MAX_UNITS || last_actor>state->units.count || first_actor>last_actor)return -1;
    fd2_scene_arrival_visual *scope=NULL;
    int result=fd2_scene_arrival_visual_prepare(&scope,vga,fdother,first_actor);
    if(result)return result;
    for(size_t raw=first_actor;raw<last_actor;raw++)fd2_field_unit_set_hidden(&state->units.items[raw],0);
    fd2_scene_field_state visible=*state;visible.units.count=last_actor;
    result=arrival_visual_play_view(scope,vga,terrain,map,sprites,&visible,audio,fast);
    fd2_scene_arrival_visual_close(&scope);return result;
}

/* 同一冷流程产生逻辑终态与呈现重放；此解释器只使用原Scene局部状态。 */
typedef struct {
    size_t stage;
    fd2_field_map map;
    fd2_field_metadata metadata;
    fd2_field_placements placements;
    fd2_terrain_tileset terrain;
    fd2_text_entry text;
    fd2_scene_field_state field_state;
} fd2_opening_stage;

typedef struct {
    fd2_image images[2];
    const uint8_t *data;
    size_t offsets[2], ends[2];
    uint16_t x[2], y[2];
} scene_game_over_data;

typedef struct {
    fd2_vga *vga;
    const fd2_archive *fdother, *dato, *field_archive, *shape_archive, *text_archive;
    const fd2_terrain_tileset *terrain;
    const fd2_field_map *map;
    const fd2_map_sprite_bank *sprites;
    const fd2_ui_sheet *ui;
    const fd2_font *font;
    const fd2_text_entry *text;
    const fd2_text_entry *names;
    const fd2_scene_runtime *runtime;
    const fd2_field_metadata *metadata;
    const fd2_field_placements *placements;
    fd2_scene_field_state *state;
    const fd2_field_game *game;
    fd2_field_game *mutable_game;
    const fd2_field_units *projected;
    fd2_bgm_player *bgm;
    fd2_field_audio *audio;
    fd2_opening_stage *opening;
    scene_game_over_data *game_over;
    size_t opening_part;
    unsigned flow_steps, flow_depth;
    int fast;
    int source_flow; /* 显式源入口临时视图；不进入Game/SAV。 */
    fd2_scene_transition_scope *transition;
    fd2_scene_teleport_scope *teleport;
    fd2_scene_cinematic *cinematic;
    fd2_text_entry *source_text; /* 本次Scene拥有的原FDTXT视图，1088d(30)重绑。 */
    fd2_dialog_state cinematic_notice; /* 31bdf原底框局部状态，不进入Game/SAV。 */
} scene_flow_context;

static int scene_source_commit(scene_flow_context *c) {
    if (!c || !c->source_flow || !c->mutable_game || !c->state ||
            c->state->units.count > FD2_FIELD_MAX_UNITS) return -1;
    fd2_field_game *game = c->mutable_game;
    game->units = c->state->units;
    game->camera_cell_x = c->state->camera_cell_x;
    game->camera_cell_y = c->state->camera_cell_y;
    game->camera_pixel_offset_x = c->state->camera_offset_x;
    game->camera_pixel_offset_y = c->state->camera_offset_y;
    game->cursor_cell_x = c->state->focus_cell_x;
    game->cursor_cell_y = c->state->focus_cell_y;
    game->ordinary_display_mode = c->state->cursor ? 1u : 0u;
    return 0;
}

static void scene_source_load(scene_flow_context *c) {
    const fd2_field_game *game = c->game;
    c->state->units = game->units;
    c->state->camera_cell_x = game->camera_cell_x;
    c->state->camera_cell_y = game->camera_cell_y;
    c->state->camera_offset_x = game->camera_pixel_offset_x;
    c->state->camera_offset_y = game->camera_pixel_offset_y;
    c->state->focus_cell_x = game->cursor_cell_x;
    c->state->focus_cell_y = game->cursor_cell_y;
    c->state->persistent = game->roster;
    c->state->persistent_count = game->roster_count;
    c->state->cursor = game->ordinary_display_mode ? c->state->ordinary_cursor : NULL;
}

static int opening_stage_open(fd2_opening_stage *scene,
    const fd2_archive *field, const fd2_archive *fdshap,
    const fd2_archive *fdtxt, size_t stage,const fd2_scene_runtime *runtime);
static int field_event_append_group(fd2_vga *vga, const fd2_archive *fdother,
    const fd2_field_game *game, fd2_scene_field_state *state,
    uint8_t group, int force, int arrival_effect, fd2_field_audio *audio,
    int fast, const fd2_field_units *projected);
static int scene_fixed_ticks(fd2_vga *vga, unsigned ticks, int fast);
static int scene_game_over_frame(fd2_vga *vga, scene_game_over_data *data,
    size_t index, int fast);
static int field_story_validate_units(const fd2_field_game *game,
        const fd2_field_units *units, size_t fragment);
static int scene_common_map_replace(fd2_field_game *game,int32_t entry,int32_t shape);

static const fd2_static_flow *scene_flow_find(const char *id) {
    const fd2_static_flow *flow = fd2_static_flow_lookup("scene", id);
    return flow ? flow : fd2_static_flow_lookup("stage", id);
}

static const fd2_static_flow *scene_stage_flow(const char *prefix, size_t stage) {
    char id[64];
    if (snprintf(id, sizeof(id), "%s.%03zu", prefix, stage) < 0) return NULL;
    return scene_flow_find(id);
}

static int scene_flow_run(scene_flow_context *c, const fd2_static_flow *flow);

static int scene_source_status(int result) {
    if (result==FD2_SCENE_RESULT_HOST_QUIT) scene_host_quit_requested=1;
    return result ? -1:0;
}

/* 22253只把xy写回原96槽allocation；362e8合并state2时29/30可以
 * 位于active尾后。普通focus/绘制/动作读取仍由各自<count门约束。 */
static int scene_source_teleport_actor(const fd2_field_game *game,int32_t selector,size_t *actor) {
    if (!game || !actor || game->units.count>FD2_FIELD_MAX_UNITS) return -1;
    uint32_t raw;
    if (selector>=0) raw=(uint32_t)selector;
    else if (selector>=-3) {
        fd2_field_event_value_source source=selector==-1 ? FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR:
            selector==-2 ? FD2_EVENT_VALUE_STATE16_FIRST_ACTOR:FD2_EVENT_VALUE_STATE16_SECOND_ACTOR;
        if (fd2_field_event_source_value(source,game->turn_number,game->cell_action_completed,&raw)) return -1;
    } else if (selector==-4 && game->units.count) raw=(uint32_t)game->units.count-1u;
    else return -1;
    if (raw>=FD2_FIELD_MAX_UNITS) return -1;
    *actor=raw; return 0;
}

#include "scene_cinematic_flow.h"

static int scene_flow_run_nodes(scene_flow_context *c, const fd2_static_flow *flow) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(c ? c->vga:NULL)<0))return -1;
    if (!c || !flow) return -1;
    /* 两个固定调色子图也供App/准备页使用，不借Game或开放其它source操作。 */
    int palette_flow = flow->id && (!strcmp(flow->id,"stage.source.fade.in") ||
        !strcmp(flow->id,"stage.source.fade.out") || !strcmp(flow->id,"stage.palette.transition.finish"));
    /* 分支读取入口快照，不让呈现中的移动或库存投影重新选择业务路径。 */
    uint8_t hidden[FD2_FIELD_MAX_UNITS] = {0};
    size_t initial_count = c->state ? c->state->units.count : 0;
    if (initial_count > FD2_FIELD_MAX_UNITS) return -1;
    for (size_t i = 0; i < initial_count; i++)
        hidden[i] = (uint8_t)fd2_field_unit_is_hidden(&c->state->units.items[i]);
    int pc = flow->entry;
    while (c->flow_steps++ < FD2_FLOW_STEP_BUDGET) {
        const fd2_static_flow_node *n = fd2_static_flow_at(flow, pc);
        if (!n) return -1;
        if (palette_flow && strcmp(n->op,"palette_subtract") && strcmp(n->op,"palette_add") &&
                strcmp(n->op,"delay") && strcmp(n->op,"end")) return -1;
        fd2_static_flow_node source_literal;
        if (c->game && c->game->runtime_host_quit) scene_host_quit_requested=1;
        if (scene_host_quit_requested || poll_scene_quit(c->vga)) return -1;
        if (c->source_flow) {
            if (scene_source_commit(c) != 0) return -1;
            if (!strcmp(n->op, "text_state_offset")) {
                if (n->args[0] < 0 || n->args[0] > 31 || n->args[1] < 0 || n->args[1] > UINT8_MAX) return -1;
                /* 24f35/24fb6：直接零扩展当前event BYTE后加常量，无消息锁存副写。 */
                source_literal = *n; source_literal.op = "text";
                source_literal.args[0] = c->game->cell_action_completed[n->args[0]] + n->args[1];
                n = &source_literal;
            }
            /* source纯等待没有资源引用；保自己的0..65535ms域，不借
             * common delay_ms的1..1000ms caller域截断31529的2000ms。 */
            if (c->game->event_preflight && (!strcmp(n->op, "text") || !strcmp(n->op, "pan") ||
                    !strcmp(n->op, "move") || !strcmp(n->op, "refresh"))) {
                fd2_static_flow_node literal = *n;
                if (!strcmp(n->op, "text")) literal.op = "story";
                else if (!strcmp(n->op, "refresh")) literal.op = "refresh_map";
                fd2_field_event_resource_view view = {&c->game->units,
                    c->game->roster, c->game->roster_count, -1, -1, 0, c->text};
                if (c->game->event_preflight(c->game, &literal, &view, c->game->event_preflight_userdata) != 0)
                    return -1;
            }
            unsigned port = FD2_FLOW_NEXT;
            int handled = fd2_field_game_stage_source_node(c->mutable_game, n, &port);
            if (handled < 0) {
                /* Game叶失败仍可已到原写点（如1db65逐actor转向）；
                 * 顶层提交只能保该前缀，不能用叶前Scene副本覆盖它。
                 * Scene自身移动/对白失败仍由各自的state前缀交回。 */
                scene_source_load(c);
                return -1;
            }
            if (handled) {
                scene_source_load(c);
                if (!strcmp(n->op, "stage_setup_source") &&
                        fd2_field_background_rebuild(c->vga->field_page, c->fdother, c->game->stage) != 0)
                    return -1;
                pc = fd2_static_flow_edge(flow, n, port);
                continue;
            }
            if (!strcmp(n->op, "end")) return scene_render_error ? -1 : 0;
            if (!(n->views & FD2_FLOW_PRESENT) && strcmp(n->op, "end")) return -1;
        }
        if (!(n->views & FD2_FLOW_PRESENT)) {
            pc = fd2_static_flow_edge(flow, n, FD2_FLOW_NEXT);
            continue;
        }
        const int32_t *a = n->args;
        int field = c->vga && c->terrain && c->map && c->sprites && c->state;
        if (!strcmp(n->op, "end")) return scene_render_error?-1:0;
        unsigned cinematic_port=FD2_FLOW_NEXT;
        int cinematic=scene_cinematic_node(c,n,&cinematic_port);
        if (cinematic<0 || scene_render_error || scene_host_quit_requested) return -1;
        if (cinematic) { pc=fd2_static_flow_edge(flow,n,cinematic_port); continue; }
        if (!strcmp(n->op, "call_teleport") || !strcmp(n->op,"call_transition_finish")) {
            const char *id=!strcmp(n->op,"call_teleport") ? "stage.source.teleport":"stage.palette.transition.finish";
            if (!c->source_flow || scene_flow_run(c,fd2_static_flow_lookup("stage",id))) return -1;
        } else if (!strcmp(n->op,"bank80_validate")) {
            if (!c->source_flow || fd2_scene_transition_bank80(c->fdother,c->audio)) return -1;
        } else if (!strcmp(n->op,"transition_begin")) {
            if (!c->source_flow || !field || scene_source_status(fd2_scene_transition_begin(&c->transition,
                    c->mutable_game,c->vga,c->state->focus_cell_x-c->state->camera_cell_x+a[0],
                    c->state->focus_cell_y-c->state->camera_cell_y+a[1]))) return -1;
        } else if (!strcmp(n->op,"transition_sfx")) {
            if (scene_source_status(fd2_scene_transition_play_sfx(c->transition,c->audio))) return -1;
        } else if (!strcmp(n->op,"transition_frame")) {
            if (scene_source_status(fd2_scene_transition_frame(c->transition,(unsigned)a[0],a[1]))) return -1;
        } else if (!strcmp(n->op,"transition_release")) fd2_scene_transition_release(&c->transition);
        else if (!strcmp(n->op,"teleport_begin")) {
            size_t actor;
            if (!c->source_flow || scene_source_teleport_actor(c->game,a[0],&actor) ||
                    (actor>=c->game->units.count && (a[3]==-1 || a[4]==-1))) return -1;
            int x=a[3]==-1 ? c->game->units.items[actor].x:a[3];
            int y=a[4]==-1 ? c->game->units.items[actor].y:a[4];
            if (scene_source_status(fd2_scene_teleport_begin(&c->teleport,c->mutable_game,c->vga,c->fdother,
                    actor,a[1],a[2],x,y))) return -1;
        } else if (!strcmp(n->op,"teleport_sprite")) {
            if (scene_source_status(fd2_scene_teleport_sprite(c->teleport,(unsigned)a[0]))) return -1;
        } else if (!strcmp(n->op,"teleport_sound")) {
            if (scene_source_status(fd2_scene_teleport_sound(c->teleport,c->audio))) return -1;
        } else if (!strcmp(n->op,"teleport_icon")) {
            if (scene_source_status(fd2_scene_teleport_icon(c->teleport))) return -1;
        } else if (!strcmp(n->op,"teleport_contract")) {
            if (scene_source_status(fd2_scene_teleport_contract(c->teleport,(unsigned)a[0],(unsigned)a[1]))) return -1;
        } else if (!strcmp(n->op,"teleport_commit")) {
            int result=fd2_scene_teleport_commit(c->teleport);
            scene_source_load(c); /* 已到达的真实xy不能被Scene旧视图覆盖。 */
            if (scene_source_status(result)) return -1;
        } else if (!strcmp(n->op,"teleport_top")) {
            int top=fd2_scene_teleport_top(c->teleport);
            if (top<0) return scene_source_status(top);
            pc=fd2_static_flow_edge(flow,n,top ? FD2_FLOW_YES:FD2_FLOW_NO); continue;
        } else if (!strcmp(n->op,"teleport_wipe")) {
            if (scene_source_status(fd2_scene_teleport_wipe(c->teleport,(unsigned)a[0]))) return -1;
        } else if (!strcmp(n->op,"teleport_expand")) {
            if (scene_source_status(fd2_scene_teleport_expand(c->teleport,(unsigned)a[0]))) return -1;
        } else if (!strcmp(n->op,"teleport_release")) fd2_scene_teleport_release(&c->teleport);
        else if (!strcmp(n->op,"credits_setup")) {
            if (!c->source_flow || !c->mutable_game ||
                    fd2_field_background_rebuild(c->vga->field_page,c->fdother,c->game->stage)) return -1;
            int result=fd2_field_game_source_credits_setup(c->mutable_game);
            scene_source_load(c);
            if (result) return -1;
            if (c->source_text) {
                fd2_text_entry_close(c->source_text);
                if (fd2_text_entry_open_entry(c->source_text,&c->game->fdtxt_archive,31)) return -1;
                c->text=c->source_text;
            }
        } else if (!strcmp(n->op,"background_scroll")) {
            if (!c->vga || a[0]<0 || a[0]>255 || fd2_field_background_scroll(c->vga->field_page,(uint8_t)a[0])) return -1;
        } else if (!strcmp(n->op,"background_rebuild")) {
            if (!c->source_flow || !c->vga || fd2_field_background_rebuild(c->vga->field_page,c->fdother,c->game->stage)) return -1;
        } else if (!strcmp(n->op,"map_replace")) {
            if (!c->source_flow || scene_common_map_replace(c->mutable_game,a[0],a[1])) return -1;
        } else if (!strcmp(n->op,"flash_actor")) {
            if (!c->source_flow || a[0]<0 || (size_t)a[0]>=c->game->units.count) return -1;
            /* 250cc真实push(0,19,1,&byte52326)：COUNT1/list{1}；
             * 原arg4未用于绘色，4e127固定200，不把伪C“0..19”当范围。 */
            uint8_t actor=(uint8_t)a[0];
            int result=fd2_field_game_play_actor_group_flash(c->mutable_game,c->vga,c->audio,
                &actor,1,200,!c->fast);
            if (result>0) scene_host_quit_requested=1;
            if (result) return -1;
        } else if (!strcmp(n->op,"focus_cell")) {
            if (!field || a[0]<0 || a[1]<0 || a[0]>=c->map->width || a[1]>=c->map->height) return -1;
            uint32_t mode=c->state->motion_owner ? c->state->motion_owner->ordinary_display_mode :
                c->state->cursor ? 1u : 0u;
            if(focus_camera_to_cell(c->vga,c->terrain,c->map,c->sprites,c->state,a[0],a[1],c->fast,mode)<0)return -1;
        } else if (!strcmp(n->op,"terminal_wait")) {
            /* 25462/25975原无限JMP：不消费普通键、不增stage、不回标题。
             * 宿主泵仅供关闭窗口；不会从这个叶正常返回。 */
            if (!c->source_flow) return -1;
            while (!poll_scene_quit(c->vga)) if(scene_present_idle(c->vga)<0)return -1;
            return -1;
        } else if (!strcmp(n->op, "call_init") || !strcmp(n->op, "call_fade_in") || !strcmp(n->op, "call_fade_out")) {
            const char *id = !strcmp(n->op, "call_init") ? "stage.source.initialize" :
                !strcmp(n->op, "call_fade_in") ? "stage.source.fade.in" : "stage.source.fade.out";
            if (!c->source_flow || scene_flow_run(c, fd2_static_flow_lookup("stage", id)) != 0) return -1;
        } else if (!strcmp(n->op, "palette_subtract") || !strcmp(n->op, "palette_add")) {
            if ((!c->source_flow && !palette_flow) || !c->vga || (!strcmp(n->op, "palette_subtract") ?
                    fd2_vga_palette_subtract(c->vga,a[0],a[1],a[2]) : fd2_vga_palette_add(c->vga,a[0],a[1],a[2])) != 0) return -1;
            /* 旧fast入口不增加中间上传；原source机制夹具的fast行为保留。 */
            if (!c->fast || c->source_flow) {
                fd2_vga_upload_current_frame(c->vga);
                if(fd2_bios_clock_error()<0 || fd2_vga_output_error(c->vga)<0)return -1;
            }
        } else if (!strcmp(n->op, "pan")) {
            if (!field) return -1;
            if(pan_camera_to(c->vga,c->terrain,c->map,c->sprites,c->state,a[0],a[1],c->fast)<0)return -1;
        } else if (!strcmp(n->op, "move")) {
            const uint8_t *bytes; size_t size;
            if (!field || fd2_field_movement_script_get((uint8_t)a[0],&bytes,&size) != 0 ||
                play_opening_move_script(c->vga,c->terrain,c->map,c->sprites,c->state,
                    c->audio,bytes,size,a[1],c->fast,c->source_flow) != 0) return -1;
        } else if (!strcmp(n->op, "text")) {
            if(c->state) {
                if(c->game) {
                    scene_state_borrow_game(c->state,c->game);
                } else {
                    if(c->runtime)c->state->runtime=*c->runtime;
                    c->state->names=c->names;
                }
            }
            if (!field || !c->dato || !c->ui || !c->font || !c->text ||
                opening_text(c->vga,c->terrain,c->map,c->dato,c->ui,c->sprites,c->state,
                    c->font,c->text,c->audio,(size_t)a[0],c->fast) != 0) return -1;
        } else if (!strcmp(n->op, "delay")) {
            if (!field && !palette_flow) return -1;
            if (c->source_flow || palette_flow) {
                if(a[0]<0 || a[0]>65535)return -1;
                /* 原3790a只等待；不能额外推进11cac、水色或actor视觉锚。 */
                if(!c->fast && fd2_vga_delay_ms(c->vga,(uint32_t)a[0])<0)return -1;
                (void)poll_scene_quit(c->vga);
            } else if(opening_delay(c->vga,c->terrain,c->map,c->sprites,c->state,a[0],c->fast)<0)return -1;
        } else if (!strcmp(n->op, "sleep_ms")) {
            if(!c->fast && fd2_vga_delay_ms(c->vga,(uint32_t)a[0])<0)return -1;
        } else if (!strcmp(n->op, "clear_keys")) {
            if (!c->source_flow || !c->vga) return -1;
            fd2_input_clear_keys(&c->vga->input);
        } else if (!strcmp(n->op, "append_group")) {
            if (!field || !c->game || field_event_append_group(c->vga,c->fdother,c->game,
                    c->state,(uint8_t)a[0],a[1],a[2],c->audio,c->fast,c->projected) != 0) return -1;
        } else if (!strcmp(n->op, "reset_directions")) {
            if (!c->state) return -1;
            for (size_t i=0;i<c->state->units.count;i++) c->state->units.items[i].direction=0;
        } else if (!strcmp(n->op, "focus_actor")) {
            if (!field || (size_t)a[0] >= c->state->units.count) return -1;
            if(focus_camera_on_actor(c->vga,c->terrain,c->map,c->sprites,c->state,(size_t)a[0],c->fast)<0)return -1;
        } else if (!strcmp(n->op, "actor_hidden")) {
            if (!c->state || a[0] < 0 || a[1] < a[0] || a[1] >= FD2_FIELD_MAX_UNITS) return -1;
            for (int32_t i=a[0];i<=a[1];i++) fd2_field_unit_set_hidden(&c->state->units.items[i],a[2]);
        } else if (!strcmp(n->op, "ensure_count") || !strcmp(n->op, "limit_count")) {
            if (!c->state || a[0] < 0 || a[0] > FD2_FIELD_MAX_UNITS) return -1;
            if ((!strcmp(n->op,"ensure_count") && c->state->units.count < (size_t)a[0]) ||
                (!strcmp(n->op,"limit_count") && c->state->units.count > (size_t)a[0]))
                c->state->units.count=(size_t)a[0];
        } else if (!strcmp(n->op, "bgm")) {
            if (c->bgm && (a[0] < 0 ? fd2_bgm_stop(c->bgm) : fd2_bgm_play(c->bgm,(unsigned)a[0],0)) != 0) return -1;
            if(fd2_bios_clock_error()<0 || fd2_vga_output_error(c->vga)<0)return -1;
        } else if (!strcmp(n->op, "fade")) {
            if (!c->vga || (a[0] != 0 && a[0] != 1) || (!c->game_over && a[0] != 1) ||
                scene_flow_run(c,fd2_static_flow_lookup("stage",a[0] ?
                    "stage.source.fade.in" : "stage.source.fade.out")) != 0) return -1;
        } else if (!strcmp(n->op, "brightness")) {
            if (!c->vga) return -1;
            fd2_vga_set_brightness(c->vga,a[0]);
        } else if (!strcmp(n->op, "render_base")) {
            if (!field) return -1;
            /* 205da初始化入口：原11cac(1)，不借51A83推断水色。 */
            render_scene_11cac(c->vga,c->terrain,c->map,c->sprites,c->state,1);
        } else if (!strcmp(n->op, "refresh")) {
            if (!field || !c->source_flow || (a[0] != 0 && a[0] != 1)) return -1;
            render_scene_11cac(c->vga,c->terrain,c->map,c->sprites,c->state,a[0]);
            if(scene_render_error)return -1;
            /* sub_11CAC @0x11cac 直接写显存，随后的等待应显示这次重绘。 */
            if(!c->fast)fd2_vga_upload_current_frame(c->vga);
            if(fd2_bios_clock_error()<0 || fd2_vga_output_error(c->vga)<0)return -1;
        } else if (!strcmp(n->op, "present_base")) {
            if (!field) return -1;
            if(present_base_frame(c->vga,c->terrain,c->map,c->sprites,c->state,0)<0)return -1;
        } else if (!strcmp(n->op, "palette_fade_in")) {
            if (!c->vga || scene_flow_run(c,fd2_static_flow_lookup("stage",
                    "stage.source.fade.in")) != 0) return -1;
            /* 开场fast仍交一帧；准备入场已有单独的黑帧，不能多交一帧。 */
            if(c->fast && a[0] && scene_present_copy(c->vga)<0)return -1;
        } else if (!strcmp(n->op, "move_up")) {
            if (!field || opening_actor_move_up_follow_camera(c->vga,c->terrain,c->map,
                    c->sprites,c->state,c->audio,(size_t)a[0],a[1],c->fast) != 0) return -1;
        } else if (!strcmp(n->op, "arrival")) {
            if (!field || opening_actor_arrival(c->vga,c->fdother,c->terrain,c->map,c->sprites,
                    c->state,c->audio,(size_t)a[0],(size_t)a[1],c->fast) != 0) return -1;
        } else if (!strcmp(n->op, "init_player")) {
            uint8_t x, y;
            if (!c->state || fd2_field_unit_deployment_position(c->metadata,
                    c->placements,(size_t)a[3],&x,&y) != 0) return -1;
            if(c->runtime && c->runtime->new_game_roster) {
                /* new_game_opening_play @0x3231b（原同名）327f7..32815：
                 * 四次112a5在现53BF7目标槽写58B，余22B及unused尾仍归App。
                 * Scene仅部署该记录；Game之后消费同一来源，不再重构roster。 */
                fd2_save_manual_state *roster=c->runtime->new_game_roster;
                size_t actor=(size_t)a[0];
                if(a[0]<0 || actor!=c->state->units.count || actor!=roster->unit_count ||
                    actor>=FD2_SAVE_MAX_UNITS || a[1]<0 || a[1]>31)return -1;
                fd2_field_unit unit;
                memcpy(&unit,roster->unit_area+actor*sizeof(unit),sizeof(unit));
                if(fd2_field_unit_persistent_create(&unit,(uint8_t)a[1])!=0)return -1;
                memcpy(roster->unit_area+actor*sizeof(unit),&unit,sizeof(unit));
                roster->unit_count++;
                unit.x=x;unit.y=y;unit.direction=unit.frame_phase=0;unit.side=2;
                ((uint8_t *)&unit)[0x31]=0xff;
                memset((uint8_t *)&unit+0x22,0,6);
                c->state->units.items[actor]=unit;
                c->state->units.walk_frames[actor]=1;
                c->state->units.source_template_indices[actor]=FD2_FIELD_UNIT_NO_TEMPLATE;
                c->state->units.count=actor+1u;
                if(fd2_field_units_sprite_cache_append(&c->state->units,actor)!=0)return -1;
            } else if(opening_actor_set(c->state,(size_t)a[0],(uint16_t)a[1],x,y)!=0)return -1;
        } else if (!strcmp(n->op, "init_templates")) {
            if (!c->state || !c->placements || !c->metadata || a[0] < 0) return -1;
            size_t count;
            if (fd2_field_unit_template_count(c->metadata,&count) != 0 ||
                count > c->placements->count || !c->placements->records) return -1;
            size_t end=a[1] < 0 ? count : (size_t)a[0]+(size_t)a[1];
            if (end > count) return -1;
            for (size_t i=(size_t)a[0];i<end;i++) {
                const fd2_field_placement *p=&c->placements->records[i];
                if (a[3] && !p->x && !p->y) break;
                size_t actor=(size_t)a[2]+i-(size_t)a[0];
                if (actor >= FD2_FIELD_MAX_UNITS) return -1;
                if (opening_actor_set_template(c->state,actor,&c->metadata->unit_templates[i],
                    (uint8_t)p->x,(uint8_t)p->y,i) != 0) return -1;
                if (a[4]) fd2_field_unit_set_hidden(&c->state->units.items[actor],1);
            }
        } else if (!strcmp(n->op, "opening_stage")) {
            if (!c->opening || !c->field_archive || !c->shape_archive || !c->text_archive ||
                opening_stage_open(c->opening,c->field_archive,c->shape_archive,c->text_archive,(size_t)a[0],c->runtime) != 0 ||
                !c->vga || fd2_field_background_rebuild(c->vga->field_page,
                    c->fdother,(size_t)a[0])!=0) return -1;
            c->terrain=&c->opening->terrain; c->map=&c->opening->map;
            c->state=&c->opening->field_state; c->text=&c->opening->text;
            printf("new-game opening: part=%zu stage=%zu text_entry=%zu actors=%zu%s\n",
                c->opening_part++,c->opening->stage,c->opening->stage+1u,
                c->state->units.count,c->fast ? ", fast" : "");
            if (scene_flow_run(c,scene_stage_flow("scene.opening",(size_t)a[0])) != 0) return -1;
        } else if (!strcmp(n->op, "stage_setup")) {
            /* 资源/前态壳已按此节点参数预检与初始化，不再做一次业务提交。 */
            if (!c->game || c->game->stage != (size_t)a[0]) return -1;
        } else if (!strcmp(n->op, "branch_any_hidden")) {
            if (a[0] < 0 || a[1] < a[0] || (size_t)a[1] >= initial_count) return -1;
            int any=0;
            for (int32_t i=a[0];i<=a[1];i++) if (hidden[i]) any=1;
            pc=fd2_static_flow_edge(flow,n,any ? FD2_FLOW_YES : FD2_FLOW_NO);
            continue;
        } else if (!strcmp(n->op, "copy_projected_inventory")) {
            if (!c->state || !c->projected || c->state->units.count > c->projected->count) return -1;
            for (size_t i=0;i<c->state->units.count;i++)
                memcpy((uint8_t *)&c->state->units.items[i]+0x0a,
                    (const uint8_t *)&c->projected->items[i]+0x0a,16);
        } else if (!strcmp(n->op, "commit_view")) {
            if (!c->state || !c->game || !c->vga || c->mutable_game != c->game) return -1;
            const fd2_field_units *expected=c->projected ? c->projected : &c->game->units;
            if (c->state->units.count != expected->count ||
                memcmp(c->state->units.items,expected->items,expected->count*sizeof(expected->items[0]))) return -1;
            fd2_field_game *owner=c->mutable_game;
            owner->camera_cell_x=c->state->camera_cell_x; owner->camera_cell_y=c->state->camera_cell_y;
            owner->camera_pixel_offset_x=owner->camera_pixel_offset_y=0;
            owner->cursor_cell_x=clamp_int(c->state->focus_cell_x,0,owner->map.width-1);
            owner->cursor_cell_y=clamp_int(c->state->focus_cell_y,0,owner->map.height-1);
            owner->idle_phase=(uint8_t)c->vga->field_visual.actor_idle_phase;
        } else if (!strcmp(n->op, "inherit_cursor")) {
            if (!c->state || !c->game || c->game->ordinary_display_mode > 1u) return -1;
            const fd2_image *cursor=fd2_field_cursor_frame(&c->game->visuals,0);
            if (!cursor || cursor->width != 24 || cursor->height != 24) return -1;
            c->state->ordinary_cursor=cursor;
            c->state->cursor=c->game->ordinary_display_mode ? cursor : NULL;
        } else if (!strcmp(n->op, "cursor_visible")) {
            if (!c->state) return -1;
            c->state->cursor=a[0] ? c->state->ordinary_cursor : NULL;
        } else if (!strcmp(n->op, "fixed_ticks") || !strcmp(n->op,"delay_ticks")) {
            if (!c->vga || scene_fixed_ticks(c->vga,(unsigned)a[0],c->fast) != 0) return -1;
        } else if (!strcmp(n->op, "clear_screen")) {
            if (!c->vga) return -1;
            /* sub_2548C @0x2548c 先清显存再等待，不能保留上一张窗口图像。 */
            fd2_vga_clear(c->vga,0);
            if(!c->fast)fd2_vga_upload_current_frame(c->vga);
            if(fd2_bios_clock_error()<0 || fd2_vga_output_error(c->vga)<0)return -1;
        } else if (!strcmp(n->op, "gameover_frame")) {
            if (!c->vga || !c->game_over || scene_game_over_frame(c->vga,c->game_over,(size_t)a[0],c->fast) != 0) return -1;
        } else return -1;
        if (c->source_flow && scene_source_commit(c) != 0) return -1;
        if(scene_render_error)return -1;
        pc=fd2_static_flow_edge(flow,n,FD2_FLOW_NEXT);
    }
    return -1;
}

/* opening_stage只能有界进入子流程，递归与子流程共用推进预算。 */
static int scene_flow_run(scene_flow_context *c, const fd2_static_flow *flow) {
    if (!c || c->flow_depth >= 8u) return -1;
    if (!c->flow_depth) {c->flow_steps=0;scene_render_error=0;}
    c->flow_depth++;
    int result=scene_flow_run_nodes(c,flow);
    c->flow_depth--;
    return result;
}

static int scene_palette_play(fd2_vga *vga, const char *identity, int fast) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if (!vga || (fast != 0 && fast != 1)) return FD2_SCENE_RESULT_ERROR;
    if (scene_host_quit_requested || poll_scene_quit(vga)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (scene_render_error) return FD2_SCENE_RESULT_ERROR;
    scene_flow_context c={0};c.vga=vga;c.fast=fast;
    int result=scene_flow_run(&c,fd2_static_flow_lookup("stage",identity));
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if (scene_render_error) return FD2_SCENE_RESULT_ERROR;
    if (scene_host_quit_requested) return FD2_SCENE_RESULT_HOST_QUIT;
    return result==0 ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
}

int fd2_scene_play_teleport_scope(fd2_vga *vga,const fd2_archive *fdother,
        fd2_field_game *game,fd2_field_audio *audio,int fast,
        fd2_scene_teleport_scope **scope) {
    if (!scope || !*scope || !game || !vga || !fdother) return FD2_SCENE_RESULT_ERROR;
    scene_flow_context c={0}; fd2_scene_field_state state={0};
    state.ordinary_cursor=fd2_field_cursor_frame(&game->visuals,0);
    if (game->ordinary_display_mode && (!state.ordinary_cursor ||
            state.ordinary_cursor->width!=24 || state.ordinary_cursor->height!=24))
        return FD2_SCENE_RESULT_ERROR;
    c.vga=vga; c.fdother=fdother; c.game=c.mutable_game=game;
    c.terrain=&game->terrain; c.map=&game->map; c.sprites=&game->sprites;
    c.state=&state; c.audio=audio; c.fast=fast; c.source_flow=1;
    c.teleport=*scope; *scope=NULL;
    scene_source_load(&c);
    scene_host_quit_requested=scene_render_error=0;
    int result=scene_flow_run(&c,fd2_static_flow_lookup("stage","stage.source.teleport"));
    *scope=c.teleport;
    return scene_render_error ? FD2_SCENE_RESULT_ERROR:
        scene_host_quit_requested ? FD2_SCENE_RESULT_HOST_QUIT:result;
}

int fd2_scene_palette_fade(fd2_vga *vga, int fade_in, int fast) {
    if (fade_in != 0 && fade_in != 1) return FD2_SCENE_RESULT_ERROR;
    return scene_palette_play(vga,fade_in ? "stage.source.fade.in" : "stage.source.fade.out",fast);
}

int fd2_scene_stage_transition_finish(fd2_vga *vga) {
    return scene_palette_play(vga,"stage.palette.transition.finish",0);
}

static int init_opening_field_state(fd2_scene_field_state *state, size_t stage,
        const fd2_field_map *map, const fd2_field_metadata *metadata,
        const fd2_field_placements *placements, const fd2_scene_runtime *runtime) {
    if (!state || !map || !placements || !placements->records || !metadata) return -1;
    memset(state,0,sizeof(*state));
    scene_flow_context c={0};
    c.state=state; c.map=map; c.metadata=metadata; c.placements=placements;c.runtime=runtime;
    return scene_flow_run(&c,scene_stage_flow("scene.init",stage));
}

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
                              size_t stage, const fd2_scene_runtime *runtime) {
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
    if (init_opening_field_state(&scene->field_state, stage,
            &scene->map, &scene->metadata, &scene->placements,runtime) != 0) {
        opening_stage_close(scene);
        return -1;
    }
    return 0;
}

int fd2_scene_play_new_game_prologue_handoff_with_runtime(
        fd2_vga *vga,
        const fd2_archive *fdother,
        fd2_bgm_player *bgm,
        fd2_field_audio *audio,
        int once,
        fd2_field_handoff *handoff,
        const fd2_scene_runtime *runtime) {
    fd2_archive field = {0};
    fd2_archive fdshap = {0};
    fd2_archive fdtxt = {0};
    fd2_archive dato = {0};
    fd2_font font = {0};
    fd2_text_entry names = {0};
    fd2_ui_sheet ui = {0};
    fd2_map_sprite_bank sprites = {0};
    fd2_opening_stage scene = {0};
    int result = -1;

    if (handoff) memset(handoff, 0, sizeof(*handoff));
    if (!vga || !fdother || (!once && (!runtime || !runtime->rng)))
        return FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = 0;
    /* 不在标题→新游戏的宿主边界停止 music。原版 new_game_opening_play
     * 让 track 18 继续覆盖最初两段对白，并在自身 code0 0x223dd 才淡出。 */
    if (fd2_archive_open(&field, fd2_asset_path(FD2_FDFIELD)) != 0 ||
        fd2_archive_open(&fdshap, fd2_asset_path(FD2_FDSHAP)) != 0 ||
        fd2_archive_open(&fdtxt, fd2_asset_path(FD2_FDTXT)) != 0 ||
        fd2_archive_open(&dato, fd2_asset_path(FD2_DATO)) != 0) {
        fprintf(stderr, "cannot open opening archives\n");
        goto done;
    }
    if (fd2_text_entry_open_entry(&names,&fdtxt,0) != 0 ||
        fd2_font_open_fdother(&font, fdother, 4) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0 ||
        fd2_map_sprite_bank_open(&sprites, fd2_asset_path(FD2_FDICON)) != 0) {
        fprintf(stderr, "cannot load opening font/UI/FDICON sprites\n");
        goto done;
    }

    const uint8_t *pal;
    size_t pal_len;
    if (fd2_archive_get(fdother, 0, &pal, &pal_len) != 0 || pal_len < 768) {
        fprintf(stderr, "cannot load opening palette\n");
        goto done;
    }
    fd2_vga_set_palette(vga, pal);
    fd2_vga_set_brightness(vga, 0);

    scene_flow_context context={0};
    context.vga=vga; context.fdother=fdother; context.dato=&dato;
    context.field_archive=&field; context.shape_archive=&fdshap; context.text_archive=&fdtxt;
    context.sprites=&sprites; context.ui=&ui; context.font=&font;
    context.names=&names;context.runtime=runtime;
    context.bgm=bgm; context.audio=audio; context.opening=&scene; context.fast=once;
    if (scene_flow_run(&context,scene_flow_find("scene.new_game")) != 0 ||
        scene_host_quit_requested || poll_scene_quit(vga)) goto done;
    if (handoff) {
        handoff->stage = scene.stage;
        handoff->units = scene.field_state.units;
        handoff->camera_cell_x = scene.field_state.camera_cell_x;
        handoff->camera_cell_y = scene.field_state.camera_cell_y;
        handoff->focus_cell_x = scene.field_state.focus_cell_x;
        handoff->focus_cell_y = scene.field_state.focus_cell_y;
        handoff->idle_phase = (uint8_t)vga->field_visual.actor_idle_phase;
        handoff->valid = 1;
    }
    result = 0;

done:
    opening_stage_close(&scene);
    fd2_map_sprite_bank_close(&sprites);
    fd2_archive_close(&dato);
    fd2_text_entry_close(&names);
    fd2_archive_close(&fdtxt);
    fd2_archive_close(&fdshap);
    fd2_archive_close(&field);
    return scene_finish(vga, result);
}

int fd2_scene_play_new_game_prologue_handoff(fd2_vga *vga,
    const fd2_archive *fdother, fd2_bgm_player *bgm, fd2_field_audio *audio,
    int once, fd2_field_handoff *handoff) {
    return fd2_scene_play_new_game_prologue_handoff_with_runtime(vga,fdother,
        bgm,audio,once,handoff,NULL);
}

static int field_event_append_group(
        fd2_vga *vga,
        const fd2_archive *fdother,
        const fd2_field_game *game,
        fd2_scene_field_state *state,
        uint8_t group,
        int force,
        int arrival_effect,
        fd2_field_audio *audio,
        int fast,
        const fd2_field_units *projected) {
    const fd2_field_units *committed_units = projected ? projected : &game->units;
    size_t first = state->units.count;
    fd2_field_units shadow = state->units;
    if (fd2_field_units_append_group_placed(&shadow, &game->metadata,
            &game->placements, &game->map, group, force) != 0 ||
        shadow.count == first || shadow.count > committed_units->count)
        return -1;
    /* field_unit_stage_template_append @0x10c50（原登记同名）：Game已提交
     * 1b750派生值；Scene仅借同一身份的静态记录，按原placement重放移动。
     * 持久角色可无template来源，不能把呈现模板索引当第二个业务owner。 */
    for (size_t i = first; i < shadow.count; i++) {
        fd2_field_unit initial = shadow.items[i];
        const fd2_field_unit *committed = &committed_units->items[i];
        uint16_t source = committed_units->source_template_indices[i];
        if (initial.unit_id != committed->unit_id ||
            initial.text_id != committed->text_id)
            return -1;
        if (source != FD2_FIELD_UNIT_NO_TEMPLATE) {
            if (source != shadow.source_template_indices[i]) return -1;
        } else {
            size_t p = 0;
            while (p < game->roster_count &&
                   (game->roster[p].unit_id != committed->unit_id ||
                    game->roster[p].text_id != committed->text_id)) p++;
            if (p == game->roster_count) return -1;
        }
        shadow.items[i] = *committed;
        shadow.items[i].x = initial.x;
        shadow.items[i].y = initial.y;
        shadow.items[i].direction = initial.direction;
        shadow.items[i].frame_phase = initial.frame_phase;
        fd2_field_unit_set_hidden(&shadow.items[i],
                                  fd2_field_unit_is_hidden(&initial));
        shadow.source_template_indices[i] = source;
    }
    state->units = shadow;
    if (!arrival_effect) {
        if(present_base_frame(vga, &game->terrain, &game->map,
                           &game->sprites, state,0)<0)return -1;
        return 0;
    }
    for (size_t i = first; i < state->units.count; i++)
        fd2_field_unit_set_hidden(&state->units.items[i], 1);
    return opening_actor_arrival(vga, fdother, &game->terrain, &game->map,
                                 &game->sprites, state, audio, first,
                                 state->units.count, fast);
}

static int field_story_validate_units(const fd2_field_game *game,
        const fd2_field_units *units, size_t fragment);

static int scene_flow_preflight(const fd2_field_game *game,
        const fd2_field_units *projected, const fd2_static_flow *flow) {
    if (!game || !flow || game->units.count > FD2_FIELD_MAX_UNITS) return -1;
    fd2_field_units preview=game->units;
    uint8_t hidden[FD2_FIELD_MAX_UNITS]={0};
    for (size_t i=0;i<game->units.count;i++)
        hidden[i]=(uint8_t)fd2_field_unit_is_hidden(&game->units.items[i]);
    int pc=flow->entry;
    for (unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *n=fd2_static_flow_at(flow,pc);
        if (!n) return -1;
        if (!(n->views & FD2_FLOW_PRESENT)) { pc=fd2_static_flow_edge(flow,n,FD2_FLOW_NEXT); continue; }
        if (!strcmp(n->op,"end")) return 0;
        if (!strcmp(n->op,"branch_any_hidden")) {
            if (n->args[0] < 0 || n->args[1] < n->args[0] || (size_t)n->args[1] >= game->units.count) return -1;
            int any=0;
            for (int32_t i=n->args[0];i<=n->args[1];i++) if (hidden[i]) any=1;
            pc=fd2_static_flow_edge(flow,n,any ? FD2_FLOW_YES : FD2_FLOW_NO);
            continue;
        }
        if (!strcmp(n->op,"inherit_cursor")) {
            const fd2_image *cursor=fd2_field_cursor_frame(&game->visuals,0);
            if (game->ordinary_display_mode > 1u || !cursor || cursor->width != 24 || cursor->height != 24) return -1;
        } else if (!strcmp(n->op,"move")) {
            const uint8_t *bytes; size_t size;
            if (fd2_field_movement_script_get((uint8_t)n->args[0],&bytes,&size) != 0 ||
                movement_script_preflight(&preview,bytes,size,0) != 0) return -1;
        } else if (!strcmp(n->op,"append_group") && projected) {
            size_t first=preview.count;
            if (fd2_field_units_append_group_placed(&preview,&game->metadata,&game->placements,
                    &game->map,(uint8_t)n->args[0],n->args[1]) != 0 ||
                preview.count > projected->count) return -1;
            for (size_t i=first;i<preview.count;i++) {
                if (preview.items[i].unit_id != projected->items[i].unit_id ||
                    preview.items[i].text_id != projected->items[i].text_id) return -1;
                preview.items[i]=projected->items[i];
            }
        } else if (!strcmp(n->op,"text")) {
            if (field_story_validate_units(game,&preview,(size_t)n->args[0]) != 0) return -1;
        }
        pc=fd2_static_flow_edge(flow,n,FD2_FLOW_NEXT);
    }
    return -1;
}

/* 正式source绑定单次逐叶执行；旧field_stage1_entry_play @0x32d18（原sub_32D18）与
 * field_stage2_entry_play @0x32e8c（原sub_32E8C）兼容域保留，
 * 旧域业务已准备终态；此处重放脚本/对白，再只交还动态坐标与镜头。 */
int fd2_scene_play_stage_entry(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast) {
    if (game && fd2_field_stage_source_is_bound(game->stage, "entry")) {
        char id[64];
        int length = snprintf(id, sizeof(id), "stage.source.entry.%03zu", game->stage);
        return length > 0 && (size_t)length < sizeof(id)
            ? fd2_scene_play_stage_source_flow(vga, fdother, game, audio, fast, id)
            : FD2_SCENE_RESULT_ERROR;
    }
    if (!vga || !fdother || !game || !game->ready ||
        game->roster_count > FD2_SAVE_BATTLE_MAX_ROSTER || game->failure != FD2_FIELD_FAILURE_NONE)
        return FD2_SCENE_RESULT_ERROR;
    const fd2_static_flow *flow=scene_stage_flow("stage.entry",game->stage);
    const fd2_static_flow_node *setup=flow ? fd2_static_flow_at(flow,flow->entry) : NULL;
    if (!setup || strcmp(setup->op,"stage_setup") || setup->args[0] != (int32_t)game->stage ||
        game->units.count != (size_t)setup->args[2] ||
        game->metadata.unknown_01 != (uint8_t)setup->args[3] ||
        game->placements.count < (size_t)setup->args[1]) return FD2_SCENE_RESULT_ERROR;
    size_t player_slots=(size_t)setup->args[1];
    fd2_archive fdtxt = {0};
    fd2_text_entry text = {0};
    fd2_ui_sheet ui = {0};
    fd2_scene_field_state state = {0};
    int result = FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = 0;
    if (poll_scene_quit(vga)) goto done;
    if (fd2_archive_open(&fdtxt, fd2_asset_path(FD2_FDTXT)) != 0 ||
        fd2_text_entry_open_entry(&text, &fdtxt, game->stage + 1u) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0)
        goto done;
    if (scene_flow_preflight(game,NULL,flow) != 0) goto done;
    state.units = game->units;
    state.persistent = game->roster;
    state.persistent_count = game->roster_count;
    scene_state_borrow_game(&state,game);
    state.units.count = player_slots;

    for (size_t i = 0; i < player_slots; i++) {
        if (i >= game->roster_count) {
            /* 1088d未部署槽清整80B，仅flags=1；1366a仍会移动此hidden槽。 */
            memset(&state.units.items[i], 0, sizeof(state.units.items[i]));
            state.units.items[i].flags = 1;
            state.units.walk_frames[i] = 1;
            continue;
        }
        const fd2_field_placement *pos = &game->placements.records[
            game->placements.count - player_slots + i];
        state.units.items[i].x = (uint8_t)pos->x;
        state.units.items[i].y = (uint8_t)pos->y;
        state.units.items[i].direction = 0;
        state.units.items[i].frame_phase = 0;
        state.units.walk_frames[i] = 1;
    }
    scene_flow_context context={0};
    context.vga=vga; context.fdother=fdother; context.dato=&game->dato_archive;
    context.terrain=&game->terrain; context.map=&game->map; context.sprites=&game->sprites;
    context.state=&state; context.game=game; context.ui=&ui; context.font=&game->font;
    context.text=&text; context.audio=audio; context.fast=fast;
    if (scene_flow_run(&context,flow) != 0) goto done;

    fd2_field_handoff handoff = {0};
    handoff.stage = game->stage;
    handoff.units = state.units;
    handoff.camera_cell_x = state.camera_cell_x;
    handoff.camera_cell_y = state.camera_cell_y;
    handoff.focus_cell_x = state.focus_cell_x;
    handoff.focus_cell_y = state.focus_cell_y;
    handoff.idle_phase = (uint8_t)vga->field_visual.actor_idle_phase;
    handoff.valid = 1;
    result = fd2_field_game_apply_handoff(game, &handoff);
    if (result == 0) {
        printf("stage%zu entry: units=%zu camera=(%d,%d) cursor=(%d,%d) turn=%u\n",
            game->stage, game->units.count, game->camera_cell_x, game->camera_cell_y,
            game->cursor_cell_x, game->cursor_cell_y, game->turn_number);
        fflush(stdout);
    }
done:
    fd2_text_entry_close(&text);
    fd2_archive_close(&fdtxt);
    return scene_finish(vga, result);
}

/* 完整源caller序列的单次执行；只接受已生成的明确source ID。
 * 复用原Scene临时呈现视图，逐叶和失败/HOST出口交回原Game；不预跑整图。 */
int fd2_scene_play_stage_source_flow(fd2_vga *vga, const fd2_archive *fdother,
        fd2_field_game *game, fd2_field_audio *audio, int fast, const char *source_id) {
    if (!vga || !vga->field_page || !fdother || !source_id ||
            !fd2_field_game_stage_source_is_idle(game) || game->stage >= 30u)
        return FD2_SCENE_RESULT_ERROR;
    char entry_id[64], victory_id[64];
    if (snprintf(entry_id, sizeof(entry_id), "stage.source.entry.%03zu", game->stage) < 0 ||
            snprintf(victory_id, sizeof(victory_id), "stage.source.victory.%03zu", game->stage) < 0 ||
            (strcmp(source_id, entry_id) && strcmp(source_id, victory_id)))
        return FD2_SCENE_RESULT_ERROR;
    const fd2_static_flow *flow = fd2_static_flow_lookup("stage", source_id);
    if (!flow) return FD2_SCENE_RESULT_ERROR;
    fd2_text_entry text = {0}; fd2_ui_sheet ui = {0};
    fd2_scene_field_state state = {0};
    scene_flow_context context = {0};
    int result = FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = scene_render_error = 0;
    if (fd2_text_entry_open_entry(&text, &game->fdtxt_archive, game->stage + 1u) != 0 ||
            ui_sheet_open_fdother(&ui, fdother, 5) != 0) goto done;
    state.ordinary_cursor = fd2_field_cursor_frame(&game->visuals, 0);
    if (!state.ordinary_cursor || state.ordinary_cursor->width != 24 || state.ordinary_cursor->height != 24)
        goto done;
    state.motion_owner = game;
    scene_state_borrow_game(&state, game);
    context.vga = vga; context.fdother = fdother; context.dato = &game->dato_archive;
    context.terrain = &game->terrain; context.map = &game->map; context.sprites = &game->sprites;
    context.state = &state; context.game = context.mutable_game = game;
    context.ui = &ui; context.font = &game->font; context.text = &text; context.source_text=&text;
    context.audio = audio; context.fast = fast; context.source_flow = 1;
    scene_source_load(&context);
    result = scene_flow_run(&context, flow);
    /* 叶内HOST/首错也保留实际移动、对话焦点和已提交业务前缀。 */
    if (scene_source_commit(&context) != 0 || scene_render_error) result = FD2_SCENE_RESULT_ERROR;
done:
    fd2_scene_transition_release(&context.transition);
    fd2_scene_teleport_release(&context.teleport);
    fd2_scene_cinematic_close(&context.cinematic);
    fd2_text_entry_close(&text);
    if (result == FD2_SCENE_RESULT_ERROR && scene_render_error) return result;
    return scene_finish(vga, result);
}

int fd2_scene_play_stage1_entry(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast) {
    if (!game || game->stage != 1u) return FD2_SCENE_RESULT_ERROR;
    return fd2_scene_play_stage_entry(vga, fdother, game, audio, fast);
}

int fd2_scene_play_stage2_entry(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast) {
    if (!game || game->stage != 2u) return FD2_SCENE_RESULT_ERROR;
    return fd2_scene_play_stage_entry(vga, fdother, game, audio, fast);
}

static int field_story_validate_units(const fd2_field_game *game,
        const fd2_field_units *units, size_t fragment) {
    if (!game || !units || !game->ready ||
        !fd2_field_stage_is_supported(game->stage) ||
        units->count > FD2_FIELD_MAX_UNITS || !game->font.glyphs ||
        game->roster_count > FD2_SAVE_BATTLE_MAX_ROSTER)
        return -1;
    fd2_archive archive = {0};
    fd2_text_entry text = {0};
    const uint8_t *tokens;
    size_t count;
    int result = -1;
    fd2_scene_field_state state = {0};
    state.units = *units;
    state.persistent = game->roster;
    state.persistent_count = game->roster_count;
    scene_state_borrow_game(&state,game);
    if (fd2_archive_open(&archive, fd2_asset_path(FD2_FDTXT)) != 0 ||
        fd2_text_entry_open_entry(&text, &archive, game->stage + 1u) != 0 ||
        fd2_text_entry_get_fragment(&text, fragment, &tokens, &count) != 0)
        goto done;
    for (size_t i = 0; i < count; i++) {
        int16_t token = (int16_t)fd2_text_token_at(tokens, i);
        if (token == -1) { result = 0; break; }
        if (token == -2 || token == -3) continue;
        if (token >= 0) {
            if ((unsigned)token >= game->font.glyph_count) goto done;
            continue;
        }
        if(token>=-6 && token<=-4) {
            fd2_text_expanded expanded;
            if(scene_dynamic_text(&state,&game->font,token,&expanded)!=0)goto done;
            for(size_t j=0;j<expanded.count;j++) if(expanded.tokens[j]==FD2_TEXT_SAFE_MINUS) {
                const fd2_image *minus=&game->info_assets.signs[1];
                if(!minus->pixels || minus->width<1 || minus->width>16 ||
                    minus->height<1 || minus->height>16)goto done;
            }
            continue;
        }
        if (token < -20 || token > -17 || ++i >= count) goto done;
        uint16_t argument = fd2_text_token_at(tokens, i);
        if ((token == -19 || token == -20) && argument >= state.units.count)
            goto done;
        unsigned portrait = resolve_portrait_control(&state, token, argument);
        for (unsigned frame = 0; frame < 4; frame++) {
            fd2_image image = {0};
            int valid = fd2_portrait_decode_frame(&image, &game->dato_archive,
                portrait, frame) == 0 && image.width == 80 && image.height == 80;
            fd2_image_free(&image);
            if (!valid) goto done;
        }
    }
done:
    fd2_text_entry_close(&text);
    fd2_archive_close(&archive);
    return result;
}

int fd2_scene_field_story_validate(const fd2_field_game *game, size_t fragment) {
    if (!game || !fd2_field_stage_battle_is_supported(game->stage)) return -1;
    return field_story_validate_units(game, &game->units, fragment);
}

int fd2_scene_stage_victory_validate(const fd2_field_game *game) {
    if (!game) return -1;
    const fd2_static_flow *flow=scene_stage_flow("stage.victory",game->stage);
    fd2_field_units projected;
    return flow && fd2_field_game_settlement_project_units(game,&projected)==0
        ? scene_flow_preflight(game,&projected,flow) : -1;
}

/* sub_22F37 @0x22f37（原登记同名）：村民分支→静默198→group4/14..16。
 * 仅Scene值投影按原时点变化；整段成功后由Play调用settlement_commit。 */
static int scene_play_stage_victory_with_owner(fd2_vga *vga, const fd2_archive *fdother,
        const fd2_field_game *game, fd2_field_audio *audio, int fast, fd2_field_game *mutable_game) {
    if (!vga || !fdother || !game || !game->ready ||
        game->failure != FD2_FIELD_FAILURE_NONE) return FD2_SCENE_RESULT_ERROR;
    const fd2_static_flow *flow=scene_stage_flow("stage.victory",game->stage);
    fd2_field_units projected;
    if (!flow || fd2_field_game_settlement_project_units(game,&projected) != 0 ||
        scene_flow_preflight(game,&projected,flow) != 0) return FD2_SCENE_RESULT_ERROR;
    fd2_archive fdtxt = {0}; fd2_text_entry text = {0}; fd2_ui_sheet ui = {0};
    fd2_scene_field_state state = {0};
    int result = FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = 0;
    if (poll_scene_quit(vga)) goto done;
    if (fd2_archive_open(&fdtxt, fd2_asset_path(FD2_FDTXT)) != 0 ||
        fd2_text_entry_open_entry(&text, &fdtxt, game->stage + 1u) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0) goto done;
    state.units = game->units; state.persistent = game->roster;
    state.persistent_count = game->roster_count;
    state.camera_cell_x = game->camera_cell_x; state.camera_cell_y = game->camera_cell_y;
    state.focus_cell_x = game->cursor_cell_x; state.focus_cell_y = game->cursor_cell_y;
    scene_state_borrow_game(&state,game);
    scene_flow_context context={0};
    context.vga=vga; context.fdother=fdother; context.dato=&game->dato_archive;
    context.terrain=&game->terrain; context.map=&game->map; context.sprites=&game->sprites;
    context.state=&state; context.game=game; context.mutable_game=mutable_game; context.projected=&projected;
    context.ui=&ui; context.font=&game->font; context.text=&text; context.audio=audio; context.fast=fast;
    if (scene_flow_run(&context,flow) != 0) goto done;
    if (state.units.count != projected.count ||
        memcmp(state.units.items, projected.items, projected.count * sizeof(projected.items[0])) != 0)
        goto done;
    result = FD2_SCENE_RESULT_OK;
done:
    fd2_text_entry_close(&text); fd2_archive_close(&fdtxt);
    return scene_finish(vga, result);
}

int fd2_scene_play_stage_victory(fd2_vga *vga, const fd2_archive *fdother,
        fd2_field_game *game, fd2_field_audio *audio, int fast) {
    if (game && fd2_field_stage_source_is_bound(game->stage, "victory")) {
        char id[64];
        int length = snprintf(id, sizeof(id), "stage.source.victory.%03zu", game->stage);
        return length > 0 && (size_t)length < sizeof(id)
            ? fd2_scene_play_stage_source_flow(vga, fdother, game, audio, fast, id)
            : FD2_SCENE_RESULT_ERROR;
    }
    return scene_play_stage_victory_with_owner(vga,fdother,game,audio,fast,game);
}

int fd2_scene_play_stage1_victory(fd2_vga *vga, const fd2_archive *fdother,
        const fd2_field_game *game, fd2_field_audio *audio, int fast) {
    if (!game || game->stage != 1u) return FD2_SCENE_RESULT_ERROR;
    return scene_play_stage_victory_with_owner(vga,fdother,game,audio,fast,NULL);
}

int fd2_scene_play_field_story(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, size_t fragment, fd2_field_audio *audio, int fast) {
    if (!vga || !fdother || !game || !game->ready || !fd2_field_stage_battle_is_supported(game->stage) ||
        game->failure != FD2_FIELD_FAILURE_NONE ||
        fd2_scene_field_story_validate(game, fragment) != 0)
        return FD2_SCENE_RESULT_ERROR;
    fd2_archive fdtxt = {0};
    fd2_text_entry text = {0};
    fd2_ui_sheet ui = {0};
    fd2_scene_field_state state = {0};
    int result = FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = 0;
    if (poll_scene_quit(vga)) goto done;
    if (fd2_archive_open(&fdtxt, fd2_asset_path(FD2_FDTXT)) != 0 ||
        fd2_text_entry_open_entry(&text, &fdtxt, game->stage + 1u) != 0 ||
        ui_sheet_open_fdother(&ui, fdother, 5) != 0 ||
        fragment >= text.fragment_count)
        goto done;
    state.camera_cell_x = game->camera_cell_x;
    state.camera_cell_y = game->camera_cell_y;
    state.focus_cell_x = game->cursor_cell_x;
    state.focus_cell_y = game->cursor_cell_y;
    state.units = game->units;
    state.persistent = game->roster;
    state.persistent_count = game->roster_count;

    scene_state_borrow_game(&state,game);
    if (opening_text(vga, &game->terrain, &game->map, &game->dato_archive,
            &ui, &game->sprites, &state, &game->font, &text, audio,
            fragment, fast) != 0)
        goto done;
    /* 对白不提交单位属性。仅交还原对白产生的镜头、焦点与idle相位。 */
    if (poll_scene_quit(vga)) goto done;
    if (state.units.count != game->units.count ||
        memcmp(state.units.items, game->units.items,
            game->units.count * sizeof(game->units.items[0])) != 0)
        goto done;
    game->camera_cell_x = state.camera_cell_x;
    game->camera_cell_y = state.camera_cell_y;
    game->camera_pixel_offset_x = game->camera_pixel_offset_y = 0;
    game->cursor_cell_x = clamp_int(state.focus_cell_x, 0, game->map.width - 1);
    game->cursor_cell_y = clamp_int(state.focus_cell_y, 0, game->map.height - 1);
    game->idle_phase = (uint8_t)vga->field_visual.actor_idle_phase;
    result = FD2_SCENE_RESULT_OK;
done:
    fd2_text_entry_close(&text);
    fd2_archive_close(&fdtxt);
    return scene_finish(vga, result);
}

/* 已证common冷节点的资源叶；24754这里只替换map/shape，4df4c与10652
 * 均由后续独立冷节点决定，不把重置/重建顺序隐藏进本叶。 */
static int scene_common_map_replace(fd2_field_game *game,int32_t entry,int32_t shape) {
    if(entry<0 || shape<0)return -1;
    fd2_field_map map={0};fd2_terrain_tileset terrain={0};
    if(fd2_field_map_open_entry(&map,&game->field_archive,(size_t)entry)!=0 ||
        fd2_terrain_tileset_open(&terrain,&game->fdshap_archive,(size_t)shape)!=0) {
        fd2_field_map_close(&map);fd2_terrain_tileset_close(&terrain);return -1;
    }
    fd2_field_map_close(&game->map);fd2_terrain_tileset_close(&game->terrain);
    game->map=map;game->terrain=terrain;
    return 0;
}

/* sub_35A0D @0x35a0d→2eb9f的FDOTHER45是FIGANI式part目录。
 * 仅本次op持资源；4e98d的SKIP保VGA、opaque0实写，a6为常色而非透明色。 */
static int scene_common_raw_animation(fd2_vga *vga,const fd2_archive *fdother,
    const int32_t *a,int fast,int validate_only) {
    if(a[0]<0 || a[1]<0 || a[2]<=0 || a[3]<0 || a[6]<-1 || a[6]>255)return -1;
    fd2_figani_clip clip={0};
    if(fd2_figani_open_entry(&clip,fdother,(size_t)a[0])!=0 || clip.empty)return -1;
    size_t first=(size_t)a[1],count=(size_t)a[2];
    if(first>clip.part_count || count>clip.part_count-first)return -1;
    typedef struct {fd2_image image;uint8_t *mask;uint16_t x,y;} scene_raw_part;
    scene_raw_part *parts=calloc(count,sizeof(*parts));if(!parts)return -1;
    int result=-1;
    for(size_t i=0;i<count;i++) {
        fd2_figani_part p={0};
        if(fd2_figani_get_part(&clip,first+i,&p)!=0 ||
            fd2_image_decode_buf(&parts[i].image,p.image_data,p.image_size)!=0)
            goto done;
        size_t pixels=(size_t)p.width*p.height;
        parts[i].mask=malloc(pixels);if(!parts[i].mask)goto done;
        memset(parts[i].mask,255,pixels);
        if(fd2_image_decode_rle_pixels(parts[i].mask,p.width,p.height,
            p.image_data+4,p.image_size-4)!=0)goto done;
        for(size_t j=0;j<pixels;j++)
            parts[i].mask[j]=(uint8_t)(parts[i].mask[j]==parts[i].image.pixels[j]);
        parts[i].x=p.x;parts[i].y=p.y;
    }
    if(validate_only) {result=0;goto done;}
    for(size_t i=0;i<count;i++) {
        if(poll_scene_quit(vga)){result=FD2_SCENE_RESULT_HOST_QUIT;goto done;}
        const scene_raw_part *p=&parts[i];
        for(int y=0;y<p->image.height;y++)for(int x=0;x<p->image.width;x++) {
            int dx=a[4]+p->x+x,dy=a[5]+p->y+y;
            size_t at=(size_t)y*(size_t)p->image.width+(size_t)x;
            if(dx>=0 && dx<VGA_W && dy>=0 && dy<VGA_H && p->mask[at])
                vga->framebuffer[(size_t)dy*VGA_STRIDE+(size_t)dx]=
                    a[6]<0?p->image.pixels[at]:(uint8_t)a[6];
        }
        if(!fast) {
            if(scene_present_copy(vga)<0)goto done;
            if(fd2_vga_wait_ticks(vga,(uint32_t)a[3])<0)goto done;
            if(poll_scene_quit(vga)){result=FD2_SCENE_RESULT_HOST_QUIT;goto done;}
        }
    }
    result=0;
done:
    for(size_t i=0;i<count;i++) {fd2_image_free(&parts[i].image);free(parts[i].mask);}
    free(parts);return result;
}

/* 11eee/12ac6有限phase域：换图提交前核完整地形引用，不推进视觉时钟。 */
static int scene_map_resource_validate(const fd2_field_map *map,
        const fd2_terrain_tileset *terrain) {
    if(!map || !terrain || !map->cells || map->width<=0 || map->height<=0)return -1;
    for(size_t i=0;i<(size_t)map->width*(size_t)map->height;i++) {
        size_t frame;
        for(int phase=0;phase<2;phase++)for(int idle=0;idle<4;idle+=2)
            if(fd2_terrain_base_frame_from_cell_animated(terrain,map->cells[i],phase,idle,&frame)!=0 ||
               fd2_terrain_overlay_frame_from_cell(terrain,map->cells[i],phase,&frame)<0)return -1;
    }
    return 0;
}

/* 24754先改变热stage，旧FDTXT owner仍留给后续对白；25757则显式重开31。
 * 只借用入口，不保存新热状态，也不按stage重新猜测调用者的文本资源。 */
static int scene_story_resource_validate(const fd2_field_game *game,
    const fd2_archive *fdother,const fd2_static_flow_node *node,
    const fd2_field_units *units,const fd2_field_unit *roster,size_t roster_count,
    const fd2_text_entry *borrowed) {
    if(!game || !fdother || !node || !units || units->count>FD2_FIELD_MAX_UNITS ||
        roster_count>FD2_SAVE_BATTLE_MAX_ROSTER || (roster_count && !roster))return -1;
    fd2_text_entry owned={0};fd2_ui_sheet ui={0};const uint8_t *tokens;size_t count;
    fd2_scene_field_state state={0};state.units=*units;
    state.persistent=roster;state.persistent_count=roster_count;
    scene_state_borrow_game(&state,game);
    int result=-1;
    if(node->args[0]<0 || ui_sheet_open_fdother(&ui,fdother,5)!=0)goto done;
    if(!borrowed) {
        if(fd2_text_entry_open_entry(&owned,&game->fdtxt_archive,game->stage+1u)!=0)goto done;
        borrowed=&owned;
    }
    if(fd2_text_entry_get_fragment(borrowed,(size_t)node->args[0],&tokens,&count)==0)
        result=scene_fragment_preflight(&game->dato_archive,&ui,&game->font,tokens,count,&state);
done:
    fd2_text_entry_close(&owned);return result;
}

int fd2_scene_common_step_validate_view(const fd2_field_game *game,
    const fd2_archive *fdother,const fd2_static_flow_node *node,
    const fd2_field_units *units,const fd2_field_unit *roster,size_t roster_count) {
    if(!game || !fdother || !node || !node->op || !units ||
        units->count>FD2_FIELD_MAX_UNITS || roster_count>FD2_SAVE_BATTLE_MAX_ROSTER ||
        (roster_count && !roster))return -1;
    const int32_t *a=node->args;
    if(!strcmp(node->op,"story")) {
        return scene_story_resource_validate(game,fdother,node,units,roster,roster_count,NULL);
    }
    if(!strcmp(node->op,"move")) {
        const uint8_t *bytes;size_t length,groups;
        if(a[0]<0 || a[0]>255 || (a[1]!=0 && a[1]!=1) ||
                fd2_field_movement_script_get((uint8_t)a[0],&bytes,&length)!=0)return -1;
        return movement_script_preflight_domain(units,bytes,length,game->idle_phase,&groups)<0?-1:0;
    }
    if(!strcmp(node->op,"focus_cell"))return a[0]>=0 && a[1]>=0 &&
        a[0]<game->map.width && a[1]<game->map.height && game->map.cells?0:-1;
    if(!strcmp(node->op,"pan"))return a[0]>=0 && a[0]<=255 && a[1]>=0 && a[1]<=255 &&
        game->map.cells && game->map.width>0 && game->map.height>0?0:-1;
    if(!strcmp(node->op,"brightness"))return a[0]>=0 && a[0]<=64?0:-1;
    if(!strcmp(node->op,"refresh_map"))return a[0]==0 || a[0]==1?0:-1;
    if(!strcmp(node->op,"raw_animation"))
        return scene_common_raw_animation(NULL,fdother,a,1,1);
    if(!strcmp(node->op,"map_replace")) {
        fd2_field_map map={0};fd2_terrain_tileset terrain={0};
        int result=a[0]>=0 && a[1]>=0 &&
            fd2_field_map_open_entry(&map,&game->field_archive,(size_t)a[0])==0 &&
            fd2_terrain_tileset_open(&terrain,&game->fdshap_archive,(size_t)a[1])==0?
            scene_map_resource_validate(&map,&terrain):-1;
        fd2_field_map_close(&map);fd2_terrain_tileset_close(&terrain);return result;
    }
    if(!strcmp(node->op,"background_rebuild") || !strcmp(node->op,"background_scroll")) {
        fd2_field_page *page=fd2_field_page_create();if(!page)return -1;
        int result=fd2_field_background_rebuild(page,fdother,game->stage);
        if(!result && !strcmp(node->op,"background_scroll"))
            result=a[0]>=0 && a[0]<=255?fd2_field_background_scroll(page,(uint8_t)a[0]):-1;
        fd2_field_page_destroy(page);return result;
    }
    return -1;
}

int fd2_scene_common_step_validate_resource_view(const fd2_field_game *game,
    const fd2_archive *fdother,const fd2_static_flow_node *node,
    const fd2_field_event_resource_view *view) {
    if(!game || !view || !node || !node->op)return -1;
    if(!strcmp(node->op,"story") && view->text_override)
        return scene_story_resource_validate(game,fdother,node,view->units,view->roster,
            view->roster_count,view->text_override);
    if(!view->has_map_override)return fd2_scene_common_step_validate_view(game,fdother,node,
        view->units,view->roster,view->roster_count);
    fd2_field_map map={0};fd2_terrain_tileset terrain={0};int result=-1;
    if(view->map_entry<0 || view->shape_index<0 ||
        fd2_field_map_open_entry(&map,&game->field_archive,(size_t)view->map_entry)!=0 ||
        fd2_terrain_tileset_open(&terrain,&game->fdshap_archive,(size_t)view->shape_index)!=0)goto done;
    if(scene_map_resource_validate(&map,&terrain)!=0)goto done;
    if(!strcmp(node->op,"pan"))result=node->args[0]>=0 && node->args[0]<=255 &&
        node->args[1]>=0 && node->args[1]<=255?0:-1;
    else result=fd2_scene_common_step_validate_view(game,fdother,node,view->units,view->roster,view->roster_count);
done:
    fd2_field_map_close(&map);fd2_terrain_tileset_close(&terrain);return result;
}

int fd2_scene_common_step_validate(const fd2_field_game *game,
    const fd2_archive *fdother,const fd2_static_flow_node *node) {
    return game?fd2_scene_common_step_validate_view(game,fdother,node,
        &game->units,game->roster,game->roster_count):-1;
}

int fd2_scene_play_common_step(fd2_vga *vga,const fd2_archive *fdother,
    fd2_field_game *game,const fd2_static_flow_node *node,
    fd2_field_audio *audio,int fast) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;
    if(!vga || !vga->field_page || !fdother || !game || !node || !node->op ||
        game->units.count>FD2_FIELD_MAX_UNITS)return FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested=scene_render_error=0;
    if(poll_scene_quit(vga))return FD2_SCENE_RESULT_HOST_QUIT;
    const int32_t *a=node->args;
    if(!strcmp(node->op,"map_replace"))return scene_common_map_replace(game,a[0],a[1]);
    if(!strcmp(node->op,"background_rebuild"))
        return fd2_field_background_rebuild(vga->field_page,fdother,game->stage);
    if(!strcmp(node->op,"background_scroll"))
        return a[0]>=0 && a[0]<=255?fd2_field_background_scroll(vga->field_page,(uint8_t)a[0]):-1;
    if(!strcmp(node->op,"brightness")) {
        if(a[0]<0 || a[0]>64)return -1;
        fd2_vga_set_brightness(vga,a[0]);return 0;
    }
    if(!strcmp(node->op,"raw_animation")) {
        /* 资源预解码失败与播放中的宿主退出分别返回ERROR/HOST，
         * 不借全局host位把真实资源错误重新分类。 */
        return scene_common_raw_animation(vga,fdother,a,fast,0);
    }
    fd2_scene_field_state state={0};
    state.units=game->units;state.persistent=game->roster;
    state.persistent_count=game->roster_count;
    state.camera_cell_x=game->camera_cell_x;state.camera_cell_y=game->camera_cell_y;
    state.camera_offset_x=game->camera_pixel_offset_x;
    state.camera_offset_y=game->camera_pixel_offset_y;
    state.focus_cell_x=game->cursor_cell_x;state.focus_cell_y=game->cursor_cell_y;
    state.motion_owner=game;scene_state_borrow_game(&state,game);
    state.ordinary_cursor=fd2_field_cursor_frame(&game->visuals,0);
    state.cursor=game->ordinary_display_mode?state.ordinary_cursor:NULL;
    int result=-1;
    if(!strcmp(node->op,"focus_cell")) {
        if(a[0]<0 || a[1]<0 || a[0]>=game->map.width || a[1]>=game->map.height)return -1;
        if(focus_camera_to_cell(vga,&game->terrain,&game->map,&game->sprites,&state,a[0],a[1],fast,game->ordinary_display_mode)<0)return -1;
        result=0;
    } else if(!strcmp(node->op,"pan")) {
        if(pan_camera_to(vga,&game->terrain,&game->map,&game->sprites,&state,a[0],a[1],fast)<0)return -1;
        result=0;
    } else if(!strcmp(node->op,"move")) {
        const uint8_t *bytes;size_t length;
        if(a[0]>=0 && a[0]<=255 &&
            fd2_field_movement_script_get((uint8_t)a[0],&bytes,&length)==0)
            result=play_opening_move_script(vga,&game->terrain,&game->map,
                &game->sprites,&state,audio,bytes,length,a[1],fast,1);
    } else if(!strcmp(node->op,"story")) {
        fd2_ui_sheet ui={0};fd2_text_entry text={0};
        if(a[0]>=0 && ui_sheet_open_fdother(&ui,fdother,5)==0 &&
            fd2_text_entry_open_entry(&text,&game->fdtxt_archive,game->stage+1u)==0)
            result=opening_text(vga,&game->terrain,&game->map,&game->dato_archive,
                &ui,&game->sprites,&state,&game->font,&text,audio,(size_t)a[0],fast);
        fd2_text_entry_close(&text);
    } else if(!strcmp(node->op,"refresh_map")) {
        if(a[0]==0 || a[0]==1) {
            /* 11cac@11cbc cmp arg,0；11cc1 jnz越过4e31c：原0才推进水。 */
            render_scene_base_with_water(vga,&game->terrain,&game->map,
                &game->sprites,&state,a[0]==0);
            if(!scene_render_error && scene_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR;
            (void)poll_scene_quit(vga);
            result=0;
        }
    }
    scene_motion_sync(&state);
    game->idle_phase=(uint8_t)vga->field_visual.actor_idle_phase;
    /* 同次11cac已确认的资源/绘制错误优先；尾poll仍保留新到的退出位，
     * 但不能将这次失败变成正常宿主关闭而跳过父图ERROR合同。 */
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0) || scene_render_error)return FD2_SCENE_RESULT_ERROR;
    if(scene_host_quit_requested)return FD2_SCENE_RESULT_HOST_QUIT;
    return result;
}

/* bios_tick_delay @0x17aa9（原登记同名）：start→至少一次比较→独立尾读。
 * 固定停顿不重画、不上传，不让普通按键跳过22e5c的1/9/36 tick；
 * signed低WORD负差加65536，53A2C仅原函数scratch，不新增App owner。 */
static int scene_fixed_ticks(fd2_vga *vga, unsigned ticks, int fast) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;
    if (!vga || ticks>65535u) return FD2_SCENE_RESULT_ERROR;
    if (poll_scene_quit(vga)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (fast) return FD2_SCENE_RESULT_OK;
    uint16_t started;if(scene_clock_low(vga,&started)<0)return FD2_SCENE_RESULT_ERROR;
    for (;;) {
        if (poll_scene_quit(vga)) return FD2_SCENE_RESULT_HOST_QUIT;
        uint16_t now;if(scene_clock_low(vga,&now)<0)return FD2_SCENE_RESULT_ERROR;
        if (fd2_vga_bios_tick_elapsed(started,now)>=(int32_t)ticks) break;
        if(fd2_vga_delay_ms(vga,1)<0)return FD2_SCENE_RESULT_ERROR; /* 仅宿主pump；原回边无VGA/输入消费。 */
    }
    uint16_t tail;if(scene_clock_low(vga,&tail)<0)return FD2_SCENE_RESULT_ERROR; /* 原17ae4 fresh尾读。 */
    return poll_scene_quit(vga) ? FD2_SCENE_RESULT_HOST_QUIT : FD2_SCENE_RESULT_OK;
}

/* 只绘制一张预检后的资源帧；先后顺序、clear/fade/等待来自冷流程。 */
static int scene_game_over_frame(fd2_vga *vga, scene_game_over_data *page,
        size_t index, int fast) {
    if (!vga || !page || index >= 2u) return -1;
    fd2_image *image=&page->images[index];
    for (int row=0;row<image->height;row++)
        memcpy(image->pixels+(size_t)row*(size_t)image->width,
            vga->framebuffer+((size_t)page->y[index]+(size_t)row)*VGA_STRIDE+page->x[index],
            (size_t)image->width);
    if (fd2_image_decode_rle_pixels(image->pixels,image->width,image->height,
            page->data+page->offsets[index]+13,
            page->ends[index]-page->offsets[index]-13) != 0) return -1;
    for (int row=0;row<image->height;row++)
        memcpy(vga->framebuffer+((size_t)page->y[index]+(size_t)row)*VGA_STRIDE+page->x[index],
            image->pixels+(size_t)row*(size_t)image->width,(size_t)image->width);
    if(!fast && scene_present_copy(vga)<0)return -1;
    return poll_scene_quit(vga) ? -1 : 0;
}

int fd2_scene_play_game_over(fd2_vga *vga, const fd2_archive *fdother,
    fd2_bgm_player *bgm, int fast) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if (!vga || !fdother) return FD2_SCENE_RESULT_ERROR;
    scene_host_quit_requested = 0;
    if (poll_scene_quit(vga)) return FD2_SCENE_RESULT_HOST_QUIT;
    fd2_image images[2] = {{0}};
    const uint8_t *data;
    size_t size, offsets[2], ends[2];
    uint16_t x[2], y[2];
    int result = FD2_SCENE_RESULT_ERROR;
    if (fd2_archive_get(fdother, 79, &data, &size) != 0 || size < 16 ||
        rd_u16_le(data) != 2 || rd_u16_le(data + 2) != 2 || rd_u32_le(data + 4) != 0)
        goto done;
    /* sub_2EB9F @0x2eb9f的嵌图从9B头后开始；全部RLE先验，零色/skip不混用。 */
    for (size_t i = 0; i < 2; i++) {
        offsets[i] = rd_u32_le(data + 8 + i * 4u);
        ends[i] = i == 0 ? rd_u32_le(data + 12) : size;
        if (offsets[i] < 16 || ends[i] > size || ends[i] <= offsets[i] ||
            ends[i] - offsets[i] < 13 ||
            fd2_image_decode_buf(&images[i], data + offsets[i] + 9,
                ends[i] - offsets[i] - 9) != 0)
            goto done;
        x[i] = rd_u16_le(data + offsets[i]);
        y[i] = rd_u16_le(data + offsets[i] + 2);
        if ((size_t)x[i] + (size_t)images[i].width > VGA_W ||
            (size_t)y[i] + (size_t)images[i].height > VGA_H)
            goto done;
    }
    scene_game_over_data page={0};
    memcpy(page.images,images,sizeof(images)); page.data=data;
    memcpy(page.offsets,offsets,sizeof(offsets)); memcpy(page.ends,ends,sizeof(ends));
    memcpy(page.x,x,sizeof(x)); memcpy(page.y,y,sizeof(y));
    scene_flow_context context={0};
    context.vga=vga; context.bgm=bgm; context.fast=fast; context.game_over=&page;
    if (scene_flow_run(&context,scene_flow_find("scene.game_over")) != 0) goto done;
    result = FD2_SCENE_RESULT_OK;
done:
    for (size_t i = 0; i < 2; i++) fd2_image_free(&images[i]);
    return scene_finish(vga, result);
}

int fd2_scene_play_new_game_prologue(fd2_vga *vga,
                                     const fd2_archive *fdother,
                                     fd2_bgm_player *bgm,
                                     fd2_field_audio *audio,
                                     int once) {
    return fd2_scene_play_new_game_prologue_handoff(
        vga, fdother, bgm, audio, once, NULL);
}
