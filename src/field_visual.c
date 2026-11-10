/* 炎龙骑士团 2 SDL3 重写 - 战场选择框、可达范围与水色DAC
 *
 * 逆向依据：field_selection_overlay_draw @0x122dc、
 * field_selection_sprite_blit @0x126f7、field_view_render_tiles @0x11eee
 * 与 shape_blit_palette_lut_24x24 @0x4e016、透明变体 @0x4e0a2。
 * FDOTHER[1] 提供 24×24
 * 选择框帧，FDOTHER[3] 提供 256 字节 palette translation LUT。
 */

#include "field_visual.h"

#include <stdlib.h>
#include <string.h>

#include "shape.h"
#include "static_data/runtime.inc"

_Static_assert(sizeof(k_field_range_sequence)==FD2_FIELD_RANGE_PHASE_COUNT,
               "original range phase count");

static int32_t visual_signed_bits(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits :
        (int32_t)((int64_t)bits - INT64_C(0x100000000));
}

static int32_t visual_bios_tick(uint64_t now_ms) {
    uint32_t low=(uint32_t)(now_ms/55u)&0xffffu;
    return low<0x8000u ? (int32_t)low : (int32_t)low-0x10000;
}

int fd2_field_visual_actor_step(fd2_field_visual_state *state,uint64_t now_ms) {
    if(!state || state->actor_idle_phase>=4 || state->cursor_phase>=4)return -1;
    int32_t tick=visual_bios_tick(now_ms);
    int32_t delta=visual_signed_bits((uint32_t)tick-state->actor_idle_tick);
    if(delta>4 || delta<0) {
        state->actor_idle_phase=(state->actor_idle_phase+1u)&3u;
        state->actor_idle_tick=(uint32_t)tick;
    }
    state->cursor_phase=(state->cursor_phase+1u)&3u;
    return 1; /* cursor在同tick调用也实际推进。 */
}

int fd2_field_visual_tiles_step(fd2_field_visual_state *state,uint64_t now_ms,
        int32_t range_override) {
    if(!state || state->terrain_phase>1 || state->range_phase>=20 ||
        range_override < -1 || range_override>=20)return -1;
    int changed=0;
    int32_t tick=visual_bios_tick(now_ms);
    if((uint32_t)tick!=state->terrain_tick) {
        state->terrain_phase^=1u;
        state->terrain_tick=(uint32_t)tick;
        changed=1;
    }
    if(range_override!=-1) {
        changed|=state->range_phase!=(uint32_t)range_override;
        state->range_phase=(uint32_t)range_override;
    } else if(visual_signed_bits((uint32_t)tick-state->range_tick)>2 ||
              tick<visual_signed_bits(state->range_tick)) {
        state->range_phase++;
        if(state->range_phase==20)state->range_phase=0;
        state->range_tick=(uint32_t)tick;
        changed=1;
    }
    return changed;
}

/* 22046只改53A49内区，VGA几何叶的(4,4)正好对应page(72,72)。 */
static int transition_page(fd2_field_page *page,const uint8_t lut[256],
    int x,int y,int radius,int first_row,int operation) {
    if(!page)return -1;
    fd2_vga scratch={0};
    uint8_t *view=scratch.framebuffer+1284;
    if(fd2_field_page_read(page,72,72,312,192,view,
        sizeof(scratch.framebuffer)-1284,320)!=0)return -1;
    int result=operation==0?fd2_field_transition_lut_before_actors_from(
        &scratch,lut,x,y,radius,first_row):operation==1?
        fd2_field_transition_lut_after_actors_from(&scratch,lut,x,y,radius,first_row):
        fd2_field_transition_lut_circle12(&scratch,lut,x,y,radius);
    return result?result:fd2_field_page_write(page,72,72,312,192,view,
        sizeof(scratch.framebuffer)-1284,320);
}

int fd2_field_transition_page_before_actors_from(fd2_field_page *page,
    const uint8_t lut[256],int x,int y,int radius,int first_row) {
    return transition_page(page,lut,x,y,radius,first_row,0);
}
int fd2_field_transition_page_after_actors_from(fd2_field_page *page,
    const uint8_t lut[256],int x,int y,int radius,int first_row) {
    return transition_page(page,lut,x,y,radius,first_row,1);
}
int fd2_field_transition_page_circle12(fd2_field_page *page,
    const uint8_t lut[256],int x,int y,int radius) {
    return transition_page(page,lut,x,y,radius,0,2);
}

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int load_cursor_frames(fd2_field_visuals *visuals,
                              const fd2_archive *fdother) {
    const uint8_t *data;
    size_t size;
    fd2_shape_sheet sheet = {0};

    if (fd2_archive_get(fdother, 1, &data, &size) != 0 ||
        fd2_shape_sheet_open_mem(&sheet, data, size) != 0)
        return -1;
    if (sheet.width != 24 || sheet.height != 24 ||
        sheet.frame_count != FD2_FIELD_CURSOR_FRAME_COUNT) {
        fd2_shape_sheet_close(&sheet);
        return -1;
    }

    for (size_t i = 0; i < sheet.frame_count; i++) {
        if (fd2_shape_sheet_decode_frame(&visuals->cursor_frames[i],
                                         &sheet, i) != 0) {
            fd2_shape_sheet_close(&sheet);
            return -1;
        }
        visuals->cursor_frame_count++;
    }
    fd2_shape_sheet_close(&sheet);
    return 0;
}

static int load_range_luts(fd2_field_visuals *visuals,
                           const fd2_archive *fdother) {
    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(fdother, 3, &data, &size) != 0 || size < 6 ||
        memcmp(data, "LMI1", 4) != 0)
        return -1;

    uint16_t count = rd_u16_le(data + 4);
    if (count != FD2_FIELD_RANGE_LUT_COUNT ||
        6u + (size_t)count * 4u > size)
        return -1;

    for (size_t i = 0; i < count; i++) {
        uint32_t start = rd_u32_le(data + 6u + i * 4u);
        uint32_t end = i + 1u < count
                     ? rd_u32_le(data + 6u + (i + 1u) * 4u)
                     : (uint32_t)size;
        if (start > size || end > size || end < start || end - start != 256u)
            return -1;
        memcpy(visuals->range_luts[i], data + start, 256u);
        visuals->range_lut_count++;
    }
    return 0;
}

int fd2_field_visuals_open(fd2_field_visuals *visuals,
                           const fd2_archive *fdother) {
    if (!visuals || !fdother) return -1;
    memset(visuals, 0, sizeof(*visuals));
    if (load_cursor_frames(visuals, fdother) != 0 ||
        load_range_luts(visuals, fdother) != 0) {
        fd2_field_visuals_close(visuals);
        return -1;
    }
    return 0;
}

void fd2_field_visuals_close(fd2_field_visuals *visuals) {
    if (!visuals) return;
    for (size_t i = 0; i < FD2_FIELD_CURSOR_FRAME_COUNT; i++)
        fd2_image_free(&visuals->cursor_frames[i]);
    memset(visuals, 0, sizeof(*visuals));
}

const fd2_image *fd2_field_cursor_frame(const fd2_field_visuals *visuals,
                                        size_t frame_index) {
    if (!visuals || frame_index >= visuals->cursor_frame_count)
        return NULL;
    return &visuals->cursor_frames[frame_index];
}

const uint8_t *fd2_field_lut_frame(const fd2_field_visuals *visuals,
                                   size_t frame_index) {
    if (!visuals || frame_index >= visuals->range_lut_count)
        return NULL;
    return visuals->range_luts[frame_index];
}

const uint8_t *fd2_field_range_lut(const fd2_field_visuals *visuals,
                                   size_t phase) {
    /* DS:0x1a97，20 个 phase 对 FDOTHER[3] LUT frame 的选择表。
     * field_view_render_tiles @0x11eee 每 3 个 BIOS tick 前进一步。 */
    if (!visuals || visuals->range_lut_count != FD2_FIELD_RANGE_LUT_COUNT)
        return NULL;
    return fd2_field_lut_frame(
        visuals, k_field_range_sequence[phase % FD2_FIELD_RANGE_PHASE_COUNT]);
}

void fd2_field_apply_palette_lut(fd2_vga *vga,
                                 int x, int y, int width, int height,
                                 const uint8_t lut[256]) {
    if (!vga || !lut || width <= 0 || height <= 0) return;
    int64_t right64 = (int64_t)x + width;
    int64_t bottom64 = (int64_t)y + height;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = right64 > VGA_W ? VGA_W : (int)right64;
    int bottom = bottom64 > VGA_H ? VGA_H : (int)bottom64;
    for (int row = top; row < bottom; row++) {
        uint8_t *dst = vga->framebuffer + (size_t)row * VGA_STRIDE;
        for (int col = left; col < right; col++)
            dst[col] = lut[dst[col]];
    }
}

/* sub_219AD @0x219ad、field_transition_lut_mask @0x22046：原sqrt/CHP读集
 * 与有限机器码向量确认向零截断：scale16用isqrt(64*n/25)，
 * 2189a的scale12用isqrt(36*n/25)，不是nearest。 */
static uint64_t transition_isqrt(uint64_t value) {
    uint64_t root = 0, bit = UINT64_C(1) << 62;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else root >>= 1;
        bit >>= 2;
    }
    return root;
}

static void transition_span(fd2_vga *vga, const uint8_t lut[256],
                             int y, int64_t left, int64_t right) {
    if (left < 0) left = 0;
    if (right > 312) right = 312;
    if (right <= left) return;
    uint8_t *row = vga->framebuffer + (size_t)(y + 4) * VGA_STRIDE + 4;
    for (int64_t x = left; x < right; x++) row[x] = lut[row[x]];
}

static void transition_circle(fd2_vga *vga, const uint8_t lut[256],
    int cx, int cy, int radius, int first_row, unsigned square_scale) {
    int start = first_row < 0 ? 0 : first_row;
    for (int y = start; y < 192; y++) {
        int64_t dy = (int64_t)cy - y;
        if (dy <= -radius || dy >= radius) continue;
        uint64_t square = (uint64_t)((int64_t)radius * radius - dy * dy);
        int64_t half = (int64_t)transition_isqrt(square * square_scale / 25u);
        transition_span(vga, lut, y, (int64_t)cx - half, (int64_t)cx + half);
    }
}

int fd2_field_transition_lut_before_actors(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius) {
    return fd2_field_transition_lut_before_actors_from(vga,lut,center_x,center_y,radius,0);
}

int fd2_field_transition_lut_after_actors(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius) {
    return fd2_field_transition_lut_after_actors_from(vga,lut,center_x,center_y,radius,0);
}

int fd2_field_transition_lut_before_actors_from(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius, int first_row) {
    if (!vga || !lut || radius<1 || radius>4096 || first_row<0 || first_row>192) return -1;
    transition_circle(vga,lut,center_x,center_y,radius,first_row,64);
    return 0;
}

int fd2_field_transition_lut_after_actors_from(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius, int first_row) {
    if (!vga || !lut || radius < 1 || radius > 4096) return -1;
    if (first_row<0 || first_row>192) return -1;
    transition_circle(vga, lut, center_x, center_y, radius, center_y, 64);
    int end = center_y < 192 ? center_y : 192;
    int64_t half = (int64_t)radius * 8 / 5;
    for (int y = first_row; y < end; y++)
        transition_span(vga, lut, y, (int64_t)center_x - half, (int64_t)center_x + half);
    return 0;
}

/* sub_2189A @0x2189a直接调用sub_219AD @0x219ad(scale12)，
 * 不经过22046的actor插入点／第二圆与矩形。 */
int fd2_field_transition_lut_circle12(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius) {
    if (!vga || !lut || radius < 1 || radius > 4096) return -1;
    transition_circle(vga, lut, center_x, center_y, radius, 0, 36);
    return 0;
}

int fd2_field_actor_visual_tick(fd2_vga *vga, uint64_t now_ms) {
    if (!vga || vga->actor_jitter_phase>1u) return -1;
    uint16_t tick=(uint16_t)(now_ms/55u);
    if (tick==vga->actor_jitter_tick) return 0;
    vga->actor_jitter_tick=tick;
    vga->actor_jitter_phase^=1u;
    return 1;
}

/* sub_4E31C @0x4e31c（原登记 sub_4E31C）：DAC224..239固定水色窗口。
 * strict LE object3 canonical0x60003的有效读集93字节=16个RGB6+前15个重复。
 * 这里只保存等价的16色周期；不从FDOTHER初始palette猜色（其分量少1）。 */
static const uint8_t field_water_rgb6[16][3] = {
    {14,21,38}, {13,20,37}, {13,20,37}, {13,20,37},
    {12,19,36}, {12,19,36}, {11,18,35}, {11,18,35},
    {11,18,35}, {11,18,35}, {12,19,36}, {12,19,36},
    {13,20,37}, {14,21,38}, {14,21,38}, {14,21,38}
};

int fd2_field_water_palette_apply(fd2_vga *vga, unsigned phase) {
    if (!vga || phase >= 16) return -1;
    for (unsigned i = 0; i < 16; i++) {
        const uint8_t *color = field_water_rgb6[(phase + i) & 15u];
        for (unsigned channel = 0; channel < 3; channel++) {
            uint8_t value = color[channel];
            vga->dac[(224u + i) * 3u + channel] =
                (uint8_t)((value << 2) | (value >> 4));
        }
    }
    return 0;
}

int fd2_field_water_tick(fd2_vga *vga, uint64_t now_ms) {
    if (!vga) return -1;
    if (fd2_input_take_quit(&vga->input)) return 0;
    if (vga->field_water_phase >= 16) return -1;
    /* sub_4E310 @0x4e310（原登记sub_4E310）实际lodsw读BIOS低16位；
     * 原4e32e是unsigned jb。55ms沿用宿主BIOS近似，不声称精确硬件时钟。
     * 原版长停顿后只更新一次并重取last；不能用while追赶消耗暂停时间。 */
    uint16_t tick = (uint16_t)(now_ms / 55u);
    if ((uint16_t)(tick - vga->field_water_last_tick) < 2u) return 0;
    unsigned phase = ((unsigned)vga->field_water_phase + 1u) & 15u;
    (void)fd2_field_water_palette_apply(vga, phase);
    vga->field_water_phase = (uint8_t)phase;
    vga->field_water_last_tick = tick;
    return 1;
}
