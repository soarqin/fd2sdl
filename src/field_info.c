/* 炎龙骑士团 2 SDL3 重写 - 战场格子/单位信息面板
 *
 * 逆向依据：field_cell_info_panel_draw_entry @0x3ff07、
 * field_cell_info_panel_draw @0x3ff11、field_unit_detail_open @0x3d01f、
 * field_unit_detail_draw @0x3d103、field_unit_detail_stats_draw @0x3d1de
 * 与 field_unit_detail_equipment_draw @0x3d6d4。原版从 FDOTHER[5] 取 frame 130
 * 底板、frame 131/132 正负号及两组 HP 数字，在当前地形上叠加
 * 光标格单位的固定 idle sprite，并显示两项地形修正和当前 HP。
 */

#include "field_info.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "field_item.h"
#include "map_sprite.h"
#include "portrait.h"

#define FD2_FIELD_INFO_FDOTHER_ENTRY 5u
#define FD2_FIELD_INFO_REQUIRED_FRAMES 133u
#define FD2_FIELD_INFO_LEFT_X 5
#define FD2_FIELD_INFO_RIGHT_X 246
#define FD2_FIELD_INFO_Y 161

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int frame_bounds(const uint8_t *data, size_t size,
                        size_t frame_count, size_t frame_index,
                        uint32_t *start, uint32_t *end) {
    if (!data || frame_index >= frame_count || !start || !end) return -1;
    size_t table_end = 6u + frame_count * 4u;
    *start = rd_u32_le(data + 6u + frame_index * 4u);
    *end = frame_index + 1u < frame_count
        ? rd_u32_le(data + 6u + (frame_index + 1u) * 4u)
        : (uint32_t)size;
    return *start >= table_end && *start < *end && *end <= size ? 0 : -1;
}

static int decode_frame(fd2_image *image,
                        const uint8_t *data, size_t size,
                        size_t frame_count, size_t frame_index) {
    if (!image || !data || frame_index >= frame_count) return -1;
    uint32_t start, end;
    if (frame_bounds(data, size, frame_count, frame_index,
                     &start, &end) != 0) return -1;
    return fd2_image_decode_buf(image, data + start, (size_t)(end - start));
}

static int decode_lmi_frame(fd2_image *image,
                            const uint8_t *data, size_t size,
                            size_t frame_count, size_t frame_index) {
    uint32_t start, end;
    if (frame_bounds(data, size, frame_count, frame_index,
                     &start, &end) != 0) return -1;
    return fd2_lmi_decode_image(image, data + start, (size_t)(end - start));
}

static int decode_raw_frame(fd2_image *image,
                            const uint8_t *data, size_t size,
                            size_t frame_count, size_t frame_index) {
    uint32_t start, end;
    if (!image || frame_bounds(data, size, frame_count, frame_index,
                               &start, &end) != 0 || end - start < 4)
        return -1;
    int width = rd_u16_le(data + start);
    int height = rd_u16_le(data + start + 2);
    size_t pixels = (size_t)width * (size_t)height;
    if (width <= 0 || height <= 0 || pixels != (size_t)(end - start - 4))
        return -1;
    image->pixels = malloc(pixels);
    if (!image->pixels) return -1;
    image->width = width;
    image->height = height;
    memcpy(image->pixels, data + start + 4, pixels);
    return 0;
}

void fd2_field_info_assets_close(fd2_field_info_assets *assets) {
    if (!assets) return;
    fd2_image_free(&assets->panel);
    for (size_t i = 0; i < 2; i++) fd2_image_free(&assets->signs[i]);
    for (size_t bank = 0; bank < 3; bank++) {
        for (size_t i = 0; i < 10; i++)
            fd2_image_free(&assets->digits[bank][i]);
        fd2_image_free(&assets->digit_overflow[bank]);
    }
    fd2_image_free(&assets->two_digit_overflow);
    for (size_t i = 1; i < 18; i++)
        fd2_image_free(&assets->detail_border[i]);
    fd2_image_free(&assets->detail_top);
    fd2_image_free(&assets->detail_bottom);
    for (size_t i = 0; i < 2; i++) {
        fd2_image_free(&assets->affiliation[i]);
        fd2_image_free(&assets->bars[i]);
    }
    for (size_t i = 0; i < 3; i++)
        fd2_image_free(&assets->status_icons[i]);
    for (size_t i = 0; i < 9; i++)
        fd2_image_free(&assets->detail_icons[i]);
    memset(assets, 0, sizeof(*assets));
}

int fd2_field_info_assets_open_mem(fd2_field_info_assets *assets,
                                   const uint8_t *data, size_t size) {
    if (!assets) return -1;
    if (assets->ready) fd2_field_info_assets_close(assets);
    else memset(assets, 0, sizeof(*assets));
    if (!data || size < 6 || memcmp(data, "LMI1", 4) != 0) return -1;
    size_t frame_count = rd_u16_le(data + 4);
    if (frame_count < FD2_FIELD_INFO_REQUIRED_FRAMES ||
        6u + frame_count * 4u > size)
        return -1;

    if (decode_frame(&assets->panel, data, size, frame_count,
                     FD2_FIELD_INFO_PANEL_FRAME) != 0 ||
        assets->panel.width != 69 || assets->panel.height != 34 ||
        decode_frame(&assets->signs[0], data, size, frame_count,
                     FD2_FIELD_INFO_PLUS_FRAME) != 0 ||
        decode_frame(&assets->signs[1], data, size, frame_count,
                     FD2_FIELD_INFO_MINUS_FRAME) != 0)
        goto fail;
    for (size_t bank = 0; bank < 3; bank++) {
        size_t first_frame = bank == 0 ? 31u : bank == 1 ? 42u : 119u;
        for (size_t i = 0; i < 10; i++) {
            if (decode_frame(&assets->digits[bank][i], data, size,
                             frame_count, first_frame + i) != 0)
                goto fail;
        }
        if (decode_frame(&assets->digit_overflow[bank], data, size,
                         frame_count, first_frame + 10u) != 0)
            goto fail;
    }
    if (decode_frame(&assets->two_digit_overflow, data, size,
                     frame_count, 93u) != 0)
        goto fail;
    for (size_t i = 1; i < 18; i++) {
        if (decode_raw_frame(&assets->detail_border[i], data, size,
                             frame_count, i) != 0)
            goto fail;
    }
    if (decode_lmi_frame(&assets->detail_top, data, size, frame_count, 20) != 0 ||
        decode_lmi_frame(&assets->detail_bottom, data, size, frame_count, 21) != 0 ||
        assets->detail_top.width != 223 || assets->detail_top.height != 86 ||
        assets->detail_bottom.width != 310 || assets->detail_bottom.height != 99 ||
        decode_raw_frame(&assets->affiliation[0], data, size, frame_count, 53) != 0 ||
        decode_raw_frame(&assets->affiliation[1], data, size, frame_count, 54) != 0 ||
        decode_raw_frame(&assets->status_icons[0], data, size, frame_count, 55) != 0 ||
        decode_raw_frame(&assets->status_icons[1], data, size, frame_count, 56) != 0 ||
        decode_raw_frame(&assets->status_icons[2], data, size, frame_count, 57) != 0 ||
        decode_raw_frame(&assets->bars[0], data, size, frame_count, 23) != 0 ||
        decode_raw_frame(&assets->bars[1], data, size, frame_count, 26) != 0)
        goto fail;
    for (size_t i = 0; i < 9; i++) {
        if (decode_raw_frame(&assets->detail_icons[i], data, size,
                             frame_count, 59u + i) != 0)
            goto fail;
    }
    assets->ready = 1;
    return 0;

fail:
    fd2_field_info_assets_close(assets);
    return -1;
}

int fd2_field_info_assets_open(fd2_field_info_assets *assets,
                               const fd2_archive *fdother) {
    const uint8_t *data;
    size_t size;
    if (!fdother ||
        fd2_archive_get(fdother, FD2_FIELD_INFO_FDOTHER_ENTRY,
                        &data, &size) != 0)
        return -1;
    return fd2_field_info_assets_open_mem(assets, data, size);
}

static void draw_digits(fd2_vga *vga,
                        const fd2_field_info_assets *assets,
                        int x, int y, unsigned value, size_t count,
                        size_t bank) {
    if (count == 3 && value > 999u) {
        fd2_map_sprite_blit(vga, &assets->digit_overflow[bank], x, y, 0);
        return;
    }
    if (count == 2 && value >= 100u) {
        fd2_map_sprite_blit(vga, &assets->two_digit_overflow, x, y, 0);
        return;
    }
    unsigned divisor = count == 3 ? 100u : 10u;
    for (size_t i = 0; i < count; i++) {
        unsigned digit = value / divisor;
        value %= divisor;
        divisor /= 10u;
        fd2_map_sprite_blit(vga, &assets->digits[bank][digit], x, y, 0);
        x += 6;
    }
}

static void draw_modifier(fd2_vga *vga,
                          const fd2_field_info_assets *assets,
                          int x, int y, int value) {
    size_t sign = value < 0 ? 1u : 0u;
    uint64_t magnitude64 = value < 0
        ? (uint64_t)(-(int64_t)value) : (uint64_t)value;
    unsigned magnitude = (unsigned)(magnitude64 > 99u ? 99u : magnitude64);
    fd2_map_sprite_blit(vga, &assets->signs[sign], x, y, 0);
    draw_digits(vga, assets, x + 8, y, magnitude, 2, 0);
}

void fd2_field_info_draw(fd2_vga *vga,
                         const fd2_field_info_assets *assets,
                         int panel_right,
                         const fd2_image *terrain,
                         const fd2_image *unit,
                         uint16_t hp,
                         uint16_t hp_max,
                         int attack_modifier,
                         int defense_modifier) {
    if (!vga || !assets || !assets->ready) return;
    int x = panel_right ? FD2_FIELD_INFO_RIGHT_X : FD2_FIELD_INFO_LEFT_X;
    int y = FD2_FIELD_INFO_Y;
    fd2_map_sprite_blit(vga, &assets->panel, x, y, -1);
    if (terrain) fd2_map_sprite_blit(vga, terrain, x + 6, y + 5, 0);
    draw_modifier(vga, assets, x + 43, y + 8, attack_modifier);
    draw_modifier(vga, assets, x + 43, y + 19, defense_modifier);
    if (unit) {
        fd2_map_sprite_blit(vga, unit, x + 6, y + 5, 0);
        draw_digits(vga, assets, x + 9, y + 21, hp, 3,
                    hp == hp_max ? 0u : 1u);
    }
}

static void draw_fragment(fd2_vga *vga, const fd2_font *font,
                          const fd2_text_entry *text, size_t fragment,
                          int x, int y) {
    const uint8_t *tokens;
    size_t count;
    if (!vga || !font || !text ||
        fd2_text_entry_get_fragment(text, fragment, &tokens, &count) != 0)
        return;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(tokens, i);
        if (token >= 0xff00u) break;
        fd2_font_draw_glyph(vga, font, token, x, y, 0xcd, 0x4c, -1);
        x += 16;
    }
}

static void draw_detail_number_bank(fd2_vga *vga,
                                    const fd2_field_info_assets *assets,
                                    int x, int y, unsigned value,
                                    size_t count, size_t bank) {
    draw_digits(vga, assets, x, y, value, count, bank);
}

static void draw_detail_number(fd2_vga *vga,
                               const fd2_field_info_assets *assets,
                               int x, int y, unsigned value, size_t count) {
    draw_detail_number_bank(vga, assets, x, y, value, count, 1);
}

static void draw_bar(fd2_vga *vga, const fd2_image *bar,
                     int x, int y, unsigned current, unsigned maximum) {
    if (!vga || !bar || maximum == 0) return;
    unsigned width = current == 0 ? 0u :
        (unsigned)((uint64_t)current * 101u / maximum) + 1u;
    if (width > 102u) width = 102u;
    for (unsigned i = 0; i < width; i++)
        fd2_map_sprite_blit(vga, bar, x + (int)i, y, -1);
}

static int item_detail(const uint8_t *item, uint8_t flags,
                       size_t *icon_index, size_t *effect_icon,
                       int *effect_value) {
    if (!item || !icon_index || !effect_icon || !effect_value) return 0;
    uint8_t category = item[0];
    *icon_index = category < 0x15u ? 0u : category < 0x20u ? 1u : 2u;
    if ((flags & 0x40u) != 0) *icon_index += 3u;
    if (category < 0x15u) {
        *effect_icon = 5u;
        *effect_value = fd2_field_item_i16(item, 1);
        return 1;
    }
    if (category < 0x20u) {
        *effect_icon = 6u;
        *effect_value = fd2_field_item_i16(item, 5);
        return 1;
    }
    if (category == 0x20u && item[0x0d] == 5u) {
        *effect_icon = 7u;
        *effect_value = fd2_field_item_i16(item, 0x0e);
        return 1;
    }
    if (category == 0x20u && item[0x0d] == 0x0bu) {
        *effect_icon = 8u;
        *effect_value = fd2_field_item_i16(item, 0x0e);
        return 1;
    }
    return 0;
}

static void detail_transition_copy_rect(uint8_t *dst, const uint8_t *src,
                                        int dst_x, int dst_y,
                                        int src_x, int src_y,
                                        int width, int height) {
    if (!dst || !src || width <= 0 || height <= 0) return;
    for (int row = 0; row < height; row++)
        memcpy(dst + (size_t)(dst_y + row) * VGA_STRIDE + dst_x,
               src + (size_t)(src_y + row) * VGA_STRIDE + src_x,
               (size_t)width);
}

void fd2_field_detail_transition_frame(uint8_t *dst,
                                       const uint8_t *detail,
                                       const uint8_t *background,
                                       int phase) {
    if (!dst || !detail || !background || phase < 0 || phase > 11) return;
    /* 三个 helper 分别复现 field_unit_detail_transition_left @0x3d4c1、
     * field_unit_detail_transition_top @0x3d526 与
     * field_unit_detail_transition_bottom @0x3d5af；组合顺序复现
     * field_unit_detail_transition_frame @0x3d61d。 */
    memcpy(dst, background, VGA_W * VGA_H);

    int left = phase >= 6 ? 101 - phase * 16 : 5;
    if (left < 0)
        detail_transition_copy_rect(dst, detail, 0, 7,
                                    5 - left, 7, 86 + left, 86);
    else
        detail_transition_copy_rect(dst, detail, left, 7,
                                    5, 7, 86, 86);

    if (phase < 9) {
        int top = phase > 2 ? 55 - phase * 16 : 7;
        if (top < 0)
            detail_transition_copy_rect(dst, detail, 92, 0,
                                        92, 7 - top, 223, 86 + top);
        else
            detail_transition_copy_rect(dst, detail, 92, top,
                                        92, 7, 223, 86);
    }

    if (phase < 6) {
        int bottom = phase * 16 + 94;
        int height = 200 - bottom;
        if (height > 102) height = 102;
        detail_transition_copy_rect(dst, detail, 5, bottom,
                                    5, 94, 310, height);
    }
}

int fd2_field_detail_sfx_for_phase(int opening, int phase) {
    if (opening) return (phase == 11 || phase == 5) ? 5 : -1;
    return (phase == 0 || phase == 7) ? 6 : -1;
}

static void draw_detail_portrait_border(
    fd2_vga *vga, const fd2_field_info_assets *assets) {
    /* dialog_box_draw_tiles @0x3baca 的 `(x=5,y=7,w=5,h=5)` 调用；
     * field_unit_detail_draw @0x3d103 在 DATO 立绘之前执行该调用。 */
    const int x = 5, y = 7, tile_w = 5, tile_h = 5;
    const int inner_w = tile_w - 2;
    const int right_x = x + 3 + tile_w * 16;
    const int bottom_y = y + 3 + tile_h * 16;
#define BORDER_TILE(frame, tx, ty) \
    fd2_map_sprite_blit(vga, &assets->detail_border[(frame)], \
                        (tx), (ty), -1)
    BORDER_TILE(1, x, y);
    BORDER_TILE(2, right_x, y);
    BORDER_TILE(3, x, bottom_y);
    BORDER_TILE(4, right_x, bottom_y);
    BORDER_TILE(5, x + 3, y);
    BORDER_TILE(6, x + 0x13 + inner_w * 16, y);
    BORDER_TILE(7, x + 3, bottom_y);
    BORDER_TILE(8, x + 0x13 + inner_w * 16, bottom_y);
    BORDER_TILE(0x0e, x, y + 3);
    BORDER_TILE(0x0f, x + 0x23 + inner_w * 16, y + 3);
    BORDER_TILE(0x10, x, y + 3 + 16 * (tile_h - 1));
    BORDER_TILE(0x11, x + 0x23 + inner_w * 16,
                y + 3 + 16 * (tile_h - 1));
    for (int i = 0; i < inner_w; i++) {
        int tx = x + 0x13 + i * 16;
        BORDER_TILE(9, tx, y);
        BORDER_TILE(0x0c, tx, bottom_y);
    }
    for (int j = 1; j < tile_h - 1; j++) {
        int ty = y + 3 + j * 16;
        BORDER_TILE(0x0a, x, ty);
        BORDER_TILE(0x0b, right_x, ty);
    }
    for (int row = 0; row < tile_h; row++) {
        for (int col = 0; col < tile_w; col++) {
            BORDER_TILE(0x0d, x + 3 + col * 16,
                        y + 3 + row * 16);
        }
    }
#undef BORDER_TILE
}

void fd2_field_detail_draw(fd2_vga *vga,
                           const fd2_field_info_assets *assets,
                           const fd2_font *font,
                           const fd2_text_entry *text,
                           const fd2_field_unit *unit,
                           const fd2_image *portrait) {
    if (!vga || !assets || !assets->ready || !font || !text || !unit)
        return;
    /* field_unit_detail_draw @0x3d103：先用通用 UI 小块拼 portrait
     * 边框，再绘制 portrait (8,10)、属性框 (92,7) 与装备框 (5,94)。 */
    draw_detail_portrait_border(vga, assets);
    if (portrait) fd2_map_sprite_blit(vga, portrait, 8, 10, -1);
    fd2_map_sprite_blit(vga, &assets->detail_top, 92, 7, -1);
    fd2_map_sprite_blit(vga, &assets->detail_bottom, 5, 94, -1);

    const uint8_t *record = (const uint8_t *)unit;
    draw_fragment(vga, font, text, (size_t)unit->text_id + 1u, 99, 13);
    draw_fragment(vga, font, text, (size_t)record[0x1f] + 0x8cu, 211, 13);
    draw_fragment(vga, font, text,
                  (size_t)unit->movement_profile + 0x96u, 251, 13);
    fd2_map_sprite_blit(vga, &assets->affiliation[unit->side == 0],
                        101, 30, 0);
    for (size_t i = 0; i < 3; i++) {
        if (record[0x25 + i] != 0)
            fd2_map_sprite_blit(vga, &assets->status_icons[i],
                                194 + (int)i * 35, 68, 0);
    }

    draw_bar(vga, &assets->bars[0], 198, 33, unit->hp, unit->hp_max);
    draw_bar(vga, &assets->bars[1], 198, 52, unit->mp, unit->mp_max);
    draw_detail_number_bank(vga, assets, 267, 41, unit->hp, 3,
                            unit->hp == unit->hp_max ? 0u : 1u);
    draw_detail_number_bank(vga, assets, 293, 41,
                            unit->hp_max, 3, 0u);
    draw_detail_number_bank(vga, assets, 267, 59, unit->mp, 3,
                            unit->mp == unit->mp_max ? 0u : 1u);
    draw_detail_number_bank(vga, assets, 293, 59,
                            unit->mp_max, 3, 0u);
    draw_detail_number(vga, assets, 157, 33, record[0x21], 2);
    draw_detail_number(vga, assets, 157, 44, record[0x3c], 2);
    draw_detail_number(vga, assets, 117, 55,
                       (uint16_t)(record[0x3e] | (record[0x3f] << 8)), 3);
    draw_detail_number(vga, assets, 157, 55, unit->movement_points, 2);
    size_t hit_bank = record[0x24] == 0 ? 1u : 2u;
    draw_detail_number_bank(vga, assets, 117, 67,
                            unit->accuracy, 3, hit_bank);
    draw_detail_number_bank(vga, assets, 157, 67,
                            unit->attack, 3,
                            record[0x22] == 0 ? 1u : 2u);
    draw_detail_number_bank(vga, assets, 117, 79,
                            unit->evasion, 3, hit_bank);
    draw_detail_number_bank(vga, assets, 157, 79,
                            unit->defense, 3,
                            record[0x23] == 0 ? 1u : 2u);

    size_t visible = 0;
    for (size_t slot = 0; slot < 8; slot++) {
        uint8_t flags = record[0x0a + slot * 2u];
        uint8_t item_id = record[0x0b + slot * 2u];
        if ((flags & 0x80u) != 0) continue;
        size_t column = visible / 4u;
        size_t row = visible % 4u;
        int x = 13 + (int)column * 150;
        int y = 101 + (int)row * 22;
        size_t icon = 2;
        size_t effect_icon = 0;
        int effect_value = 0;
        const uint8_t *item = fd2_field_item_record_get(item_id);
        int known = item_detail(item, flags, &icon, &effect_icon,
                                &effect_value);
        fd2_map_sprite_blit(vga, &assets->detail_icons[icon], x, y, 0);
        draw_fragment(vga, font, text, (size_t)item_id + 0xb5u,
                      x + 29, y + 2);
        if (known) {
            fd2_map_sprite_blit(vga, &assets->detail_icons[effect_icon],
                                x + 95, y + 4, 0);
            draw_detail_number(vga, assets, x + 117, y + 4,
                               (unsigned)(effect_value < 0 ? 0 : effect_value),
                               3);
        }
        visible++;
    }
}
