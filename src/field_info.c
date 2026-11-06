/* 炎龙骑士团 2 SDL3 重写 - 战场格子/单位信息面板
 *
 * 逆向依据：field_cell_info_panel_draw_entry @0x1acf3、
 * field_cell_info_panel_draw @0x1acf3、field_unit_detail_open @0x17e0b、
 * field_unit_detail_draw @0x17eef、field_unit_detail_stats_draw @0x17fc0
 * 与 field_unit_detail_equipment_draw @0x184c0。总览依据
 * sub_1B41D @0x1b41d（原登记 sub_1B41D）和人数门
 * sub_1B5F1 @0x1b5f1（原登记 sub_1B5F1）。原版从 FDOTHER[5] 取 frame 130
 * 底板、frame 131/132 正负号及两组 HP 数字，在当前地形上叠加
 * 光标格单位的固定 idle sprite，并显示两项地形修正和当前 HP。
 * 法术列表依据field_unit_magic_list_draw @0x1ceed（原登记sub_1CEED）及其调用者
 * field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）；
 * 本模块只绘制，不接该单键UI步骤／提交。
 * 临时单位概要板依据field_unit_summary_draw @0x18c6d（原登记sub_18C6D）。
 */

#include "field_info.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_item.h"
#include "field_magic.h"
#include "map_sprite.h"
#include "portrait.h"

#define FD2_FIELD_INFO_FDOTHER_ENTRY 5u
#define FD2_FIELD_INFO_REQUIRED_FRAMES 137u
#define FD2_FIELD_INFO_LEFT_X 5
#define FD2_FIELD_INFO_RIGHT_X 246
#define FD2_FIELD_INFO_Y 161

static const int overview_sizes[4][2] = {
    {102, 17}, {170, 117}, {170, 16}, {63, 15}
};

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
    fd2_image_free(&assets->magic_cost);
    for (size_t i = 1; i < 18; i++)
        fd2_image_free(&assets->detail_border[i]);
    fd2_image_free(&assets->detail_top);
    fd2_image_free(&assets->detail_bottom);
    fd2_image_free(&assets->unit_summary);
    for (size_t i = 0; i < 2; i++) {
        fd2_image_free(&assets->affiliation[i]);
    }
    for (size_t i = 0; i < 8; i++)
        fd2_image_free(&assets->bars[i]);
    for (size_t i = 0; i < 3; i++)
        fd2_image_free(&assets->status_icons[i]);
    for (size_t i = 0; i < 9; i++)
        fd2_image_free(&assets->detail_icons[i]);
    for (size_t i = 0; i < 4; i++)
        fd2_image_free(&assets->overview[i]);
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
    if (decode_raw_frame(&assets->magic_cost, data, size, frame_count,
                         FD2_FIELD_INFO_MAGIC_COST_FRAME) != 0 ||
        assets->magic_cost.width != 20 || assets->magic_cost.height != 8 ||
        decode_frame(&assets->two_digit_overflow, data, size,
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
        decode_lmi_frame(&assets->unit_summary, data, size, frame_count, 22) != 0 ||
        assets->unit_summary.width != 149 || assets->unit_summary.height != 42 ||
        decode_raw_frame(&assets->affiliation[0], data, size, frame_count, 53) != 0 ||
        decode_raw_frame(&assets->affiliation[1], data, size, frame_count, 54) != 0 ||
        decode_raw_frame(&assets->status_icons[0], data, size, frame_count, 55) != 0 ||
        decode_raw_frame(&assets->status_icons[1], data, size, frame_count, 56) != 0 ||
        decode_raw_frame(&assets->status_icons[2], data, size, frame_count, 57) != 0)
        goto fail;
    const size_t bar_frames[8] = {23, 26, 24, 25, 27, 28, 29, 30};
    for (size_t i = 0; i < 8; i++) {
        if (decode_raw_frame(&assets->bars[i], data, size, frame_count, bar_frames[i]) != 0 ||
            assets->bars[i].width != 1 || assets->bars[i].height != 5)
            goto fail;
    }
    for (size_t i = 0; i < 9; i++) {
        if (decode_raw_frame(&assets->detail_icons[i], data, size,
                             frame_count, 59u + i) != 0)
            goto fail;
    }
    for (size_t i = 0; i < 4; i++) {
        if (decode_frame(&assets->overview[i], data, size, frame_count,
                         133u + i) != 0 ||
            assets->overview[i].width != overview_sizes[i][0] ||
            assets->overview[i].height != overview_sizes[i][1])
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
                        int x, int y, int value, size_t count,
                        size_t bank) {
    /* ui_number_draw @0x187d6（原登记同名）先把signed负数夹0。 */
    if (value < 0) value = 0;
    if (count == 3 && value > 999) {
        fd2_map_sprite_blit(vga, &assets->digit_overflow[bank], x, y, 0);
        return;
    }
    if (count == 2 && value >= 100) {
        fd2_map_sprite_blit(vga, &assets->two_digit_overflow, x, y, 0);
        return;
    }
    unsigned remaining = (unsigned)value;
    unsigned divisor = count == 3 ? 100u : 10u;
    for (size_t i = 0; i < count; i++) {
        unsigned digit = remaining / divisor;
        remaining %= divisor;
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

static int image_has_size(const fd2_image *image, int width, int height) {
    return image->pixels && image->width == width && image->height == height;
}

static int list_name(const fd2_text_entry *text, const fd2_font *font,
                       size_t fragment, size_t maximum_glyphs,
                       const uint8_t **tokens, size_t *glyph_count) {
    const uint8_t *data;
    size_t count;
    if (fd2_text_entry_get_fragment(text, fragment, &data, &count) != 0)
        return -1;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(data, i);
        if (token == 0xffffu) {
            *tokens = data;
            *glyph_count = i; /* 资源中的合法空名不补猜测文字。 */
            return 0;
        }
        if (token >= 0xff00u || token >= font->glyph_count || i >= maximum_glyphs)
            return -1;
    }
    return -1;
}

int fd2_field_magic_list_draw(fd2_vga *vga,
                              const fd2_field_info_assets *assets,
                              const fd2_font *font,
                              const fd2_text_entry *text,
                              const fd2_field_unit *unit,
                              int selected) {
    if (!vga || !assets || !assets->ready || !font || !font->glyphs ||
        font->glyph_count == 0 || font->glyph_count > font->size / 32u ||
        !text || !unit || selected < -1 ||
        !image_has_size(&assets->detail_bottom, 310, 99) ||
        !image_has_size(&assets->magic_cost, 20, 8))
        return -1;
    for (size_t i = 0; i < 10; i++) {
        if (!image_has_size(&assets->digits[1][i], i == 1 ? 5 : 6, 8))
            return -1;
    }
    uint8_t ids[40];
    size_t count = fd2_field_unit_magic_list_build(unit, ids, sizeof(ids));
    if (count > FD2_FIELD_MAGIC_LIST_MAX_COUNT ||
        (selected >= 0 && (size_t)selected >= count))
        return -1;
    const uint8_t *names[FD2_FIELD_MAGIC_LIST_MAX_COUNT];
    size_t glyph_counts[FD2_FIELD_MAGIC_LIST_MAX_COUNT];
    uint8_t costs[FD2_FIELD_MAGIC_LIST_MAX_COUNT];
    for (size_t i = 0; i < count; i++) {
        const uint8_t *record = fd2_field_magic_record_get(ids[i]);
        /* 名称与x+50费用标签间仅容纳三个16px字形及左下阴影。 */
        if (!record || list_name(text, font, 441u + ids[i], 3u,
                                  &names[i], &glyph_counts[i]) != 0)
            return -1;
        costs[i] = record[5];
    }
    /* 先预检全部行，再进入无分配的绘制段。native1ceed只绘名称／费用；
     * SDL现有详情合并了装备页，故此整合入口先恢复空白底板。
     * 上半姓名／MP／立绘和任何玩法状态均不改。 */
    fd2_map_sprite_blit(vga, &assets->detail_bottom, 5, 94, -1);
    for (size_t i = 0; i < count; i++) {
        int x = 18 + (int)(i / 4u) * 100;
        int y = 103 + (int)(i % 4u) * 22;
        uint8_t foreground = selected == (int)i ? 201u : 205u;
        for (size_t j = 0; j < glyph_counts[i]; j++) {
            fd2_font_draw_glyph(vga, font, fd2_text_token_at(names[i], j),
                                x + (int)j * 16, y, foreground, 76, -1);
        }
        fd2_map_sprite_blit(vga, &assets->magic_cost, x + 50, y + 5, -1);
        draw_digits(vga, assets, x + 73, y + 5, costs[i], 2, 1);
    }
    return 0;
}

/* stage 0 两段条件只有普通字形及结束符；先检查完整文本，避免画完
 * 第一段后才发现第二段缺失。其他控制码留待其实际关卡得到支持。 */
static int overview_fragment(const fd2_text_entry *text, const fd2_font *font,
                              size_t fragment, const uint8_t **tokens,
                              size_t *glyph_count) {
    const uint8_t *data;
    size_t count;
    if (fd2_text_entry_get_fragment(text, fragment, &data, &count) != 0)
        return -1;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(data, i);
        if (token == 0xffffu) {
            *tokens = data;
            *glyph_count = i;
            return 0;
        }
        if (token >= 0xff00u || token >= font->glyph_count ||
            i >= (VGA_W - 80u) / 16u)
            return -1;
    }
    return -1;
}

/* ui_number_draw @0x187d6（原登记 ui_number_draw）的八位分支：
 * signed 负值先归零；超过八位时按格式化结果取前八字符。 */
static void draw_overview_resource(fd2_vga *vga,
                                     const fd2_field_info_assets *assets,
                                     uint32_t value) {
    char digits[11];
    unsigned number = value <= INT32_MAX ? (unsigned)value : 0u;
    (void)snprintf(digits, sizeof(digits), "%08u", number);
    for (size_t i = 0; i < 8; i++)
        fd2_map_sprite_blit(vga, &assets->digits[0][digits[i] - '0'],
                            140 + (int)i * 6, 176, 0);
}

int fd2_field_overview_draw(fd2_vga *vga,
                            const fd2_field_info_assets *assets,
                            const fd2_font *font,
                            const fd2_text_entry *text,
                            const fd2_field_overview *data) {
    if (!vga || !assets || !assets->ready || !font || !font->glyphs ||
        font->glyph_count == 0 || font->glyph_count > font->size / 32u ||
        !data || data->stage > 1u || !data->units ||
        data->units->count > FD2_FIELD_MAX_UNITS)
        return -1;
    for (size_t i = 0; i < 4; i++) {
        if (!image_has_size(&assets->overview[i],
                            overview_sizes[i][0], overview_sizes[i][1]))
            return -1;
    }
    for (size_t bank = 0; bank < 2; bank++) {
        for (size_t digit = 0; digit < 10; digit++) {
            if (!image_has_size(&assets->digits[bank][digit],
                                digit == 1 ? 5 : 6, 8))
                return -1;
        }
    }
    /* sub_1B41D @0x1b41d（原登记同名）：stage+1及597/598+2*stage。
     * 三阵营最多各64人，无两位溢出；回合可超过999。stage16特例未开放。 */
    if (!image_has_size(&assets->digit_overflow[1], 18, 8))
        return -1;
    const uint8_t *conditions[2];
    size_t glyph_counts[2];
    for (size_t i = 0; i < 2; i++) {
        if (overview_fragment(text, font, 597u + data->stage * 2u + i,
                               &conditions[i], &glyph_counts[i]) != 0)
            return -1;
    }

    /* sub_1B5F1 @0x1b5f1：按 active 表计数，不过滤HP、acted或AI门，
     * 也不以 persistent roster／remaining_units 代替本页人数。 */
    unsigned counts[3] = {0};
    for (size_t i = 0; i < data->units->count; i++) {
        const fd2_field_unit *unit = &data->units->items[i];
        if (unit->side < 3 && unit->unit_id != 121 && unit->race != 10 &&
            !(unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN))
            ++counts[unit->side];
    }
    fd2_map_sprite_blit(vga, &assets->overview[0], 109, 19, -1);
    fd2_map_sprite_blit(vga, &assets->overview[1], 75, 37, -1);
    fd2_map_sprite_blit(vga, &assets->overview[2], 75, 155, -1);
    fd2_map_sprite_blit(vga, &assets->overview[3], 129, 172, -1);
    draw_digits(vga, assets, 143, 24, (unsigned)data->stage + 1u, 2, 1);
    unsigned turn = data->turn_number <= INT32_MAX
        ? (unsigned)data->turn_number : 0u;
    draw_digits(vga, assets, 188, 24, turn, 3, 1);
    draw_overview_resource(vga, assets, data->resource_value);
    draw_digits(vga, assets, 120, 159, counts[0], 2, 1);
    draw_digits(vga, assets, 182, 159, counts[2], 2, 1);
    draw_digits(vga, assets, 228, 159, counts[1], 2, 1);
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < glyph_counts[i]; j++) {
            fd2_font_draw_glyph(vga, font,
                                fd2_text_token_at(conditions[i], j),
                                80 + (int)j * 16, i == 0 ? 61 : 116,
                                0xcd, 0x4c, -1);
        }
    }
    return 0;
}

static void draw_detail_number_bank(fd2_vga *vga,
                                    const fd2_field_info_assets *assets,
                                    int x, int y, int value,
                                    size_t count, size_t bank) {
    draw_digits(vga, assets, x, y, value, count, bank);
}

static void draw_detail_number(fd2_vga *vga,
                               const fd2_field_info_assets *assets,
                               int x, int y, int value, size_t count) {
    draw_detail_number_bank(vga, assets, x, y, value, count, 1);
}

static void draw_bar(fd2_vga *vga, const fd2_field_info_assets *assets,
                     size_t bank, int x, int y, int current, int maximum) {
    if (!vga || !assets || maximum == 0) return;
    /* field_stat_bar_draw @0x18795（原sub_18795）signed idiv；
     * field_stat_bar_columns_draw @0x17d6f（原sub_17D6F）负长仍画首cap和
     * x+1尾cap，零长则画101列空条及尾cap。 */
    int length = current == 0 ? 0 : current * 101 / maximum + 1;
    if (length == 0) {
        for (int i = 1; i <= 101; i++)
            fd2_map_sprite_blit(vga, &assets->bars[6], x + i, y, -1);
        fd2_map_sprite_blit(vga, &assets->bars[7], x + 102, y, -1);
        return;
    }
    fd2_map_sprite_blit(vga, &assets->bars[bank], x, y, -1);
    /* 原始WORD可使条越过102列；只裁实际VGA边界，不改有界画面内的长度。 */
    for (int i = 1; i < length && x + i < VGA_W; i++)
        fd2_map_sprite_blit(vga, &assets->bars[2 + bank * 2], x + i, y, -1);
    fd2_map_sprite_blit(vga, &assets->bars[3 + bank * 2],
                        x + (length > 1 ? length : 1), y, -1);
}

static int signed_detail_word(uint16_t value) {
    return value < 0x8000u ? (int)value : (int)value - 0x10000;
}

int fd2_field_unit_summary_draw(fd2_vga *vga,
                                 const fd2_field_info_assets *assets,
                                 const fd2_font *font,
                                 const fd2_text_entry *text,
                                 const fd2_field_unit *unit,
                                 int actor_relative_x) {
    if (!vga || !assets || !assets->ready || !font || !font->glyphs ||
        !font->glyph_count || font->glyph_count > font->size / 32u ||
        !text || !unit || !image_has_size(&assets->unit_summary, 149, 42))
        return -1;
    for (size_t bank = 0; bank < 2; bank++) {
        for (size_t digit = 0; digit < 10; digit++) {
            if (!image_has_size(&assets->digits[bank][digit],
                                digit == 1 ? 5 : 6, 8))
                return -1;
        }
        if (!image_has_size(&assets->digit_overflow[bank], 18, 8)) return -1;
    }
    if (!image_has_size(&assets->two_digit_overflow, 12, 8)) return -1;
    for (size_t i = 0; i < 8; i++)
        if (!image_has_size(&assets->bars[i], 1, 5)) return -1;
    const uint8_t *name;
    size_t glyphs;
    /* 当前已证姓名均为普通字形；最多7字，不能覆盖右侧LV字段。 */
    if (list_name(text, font, (size_t)unit->text_id + 1u, 7u, &name, &glyphs) != 0)
        return -1;

    int x = actor_relative_x >= 7 ? 9 : 160;
    int hp = signed_detail_word(unit->hp), hp_max = signed_detail_word(unit->hp_max);
    int mp = signed_detail_word(unit->mp), mp_max = signed_detail_word(unit->mp_max);
    /* field_unit_summary_draw @0x18c6d先板、条、LV、当前值、姓名；0背景不预清。 */
    fd2_map_sprite_blit(vga, &assets->unit_summary, x, 9, -1);
    draw_bar(vga, assets, 0, x + 21, 31, hp, hp_max);
    draw_bar(vga, assets, 1, x + 21, 40, mp, mp_max);
    draw_digits(vga, assets, x + 132, 13, unit->level, 2, 0);
    draw_digits(vga, assets, x + 126, 30, hp, 3,
                unit->hp == unit->hp_max ? 0u : 1u);
    draw_digits(vga, assets, x + 126, 39, mp, 3,
                unit->mp == unit->mp_max ? 0u : 1u);
    for (size_t i = 0; i < glyphs; i++)
        fd2_font_draw_glyph(vga, font, fd2_text_token_at(name, i),
                            x + 5 + (int)i * 16, 13, 205, 76, -1);
    return 0;
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

typedef struct {
    size_t raw_slot, icon, effect_icon;
    const uint8_t *name;
    size_t glyphs;
    int value, has_effect;
} item_list_row;

static int prepare_item_list(const fd2_field_info_assets *assets,
                              const fd2_font *font,
                              const fd2_text_entry *text,
                              const fd2_field_unit *unit, int selected,
                              item_list_row rows[8], size_t *count) {
    if (!assets || !assets->ready || !font || !font->glyphs ||
        !font->glyph_count || font->glyph_count > font->size / 32u ||
        !text || !unit || selected < -1 || selected >= 8 ||
        !image_has_size(&assets->detail_bottom, 310, 99) ||
        !image_has_size(&assets->digit_overflow[0], 18, 8) ||
        !image_has_size(&assets->digit_overflow[1], 18, 8)) return -1;
    for (size_t i = 0; i < 9; i++)
        if (!image_has_size(&assets->detail_icons[i], i < 5 ? 24 : 21,
                            i < 5 ? 20 : 8)) return -1;
    for (size_t i = 0; i < 10; i++)
        if (!image_has_size(&assets->digits[1][i], i == 1 ? 5 : 6, 8)) return -1;
    const uint8_t *record = (const uint8_t *)unit;
    if (selected >= 0 && (record[0x0au + (size_t)selected * 2u] & 0x80u)) return -1;
    *count = 0;
    for (size_t slot = 0; slot < 8; slot++) {
        uint8_t flags = record[0x0au + slot * 2u];
        if (flags & 0x80u) continue;
        uint8_t id = record[0x0bu + slot * 2u];
        const uint8_t *item = fd2_field_item_record_get(id);
        item_list_row *row = &rows[*count];
        if (!item || list_name(text, font, 181u + id, SIZE_MAX,
                                &row->name, &row->glyphs) != 0) return -1;
        row->raw_slot = slot;
        row->has_effect = item_detail(item, flags, &row->icon, &row->effect_icon, &row->value);
        (*count)++;
    }
    return 0;
}

static void draw_item_list(fd2_vga *vga, const fd2_field_info_assets *assets,
                            const fd2_font *font, const item_list_row rows[8],
                            size_t count, int selected) {
    fd2_map_sprite_blit(vga, &assets->detail_bottom, 5, 94, -1);
    for (size_t i = 0; i < count; i++) {
        const item_list_row *row = &rows[i];
        int x = 42 + (int)(i / 4u) * 150, y = 103 + (int)(i % 4u) * 22;
        /* 1685c→4ed0b逐行memcpy raw59..67，0也必须写；数字RLE的
         * 当前42..51/41/52均已审计没有opaque0，缓存0仍按skip绘制。 */
        fd2_map_sprite_blit(vga, &assets->detail_icons[row->icon], x - 29, y - 2, -1);
        for (size_t j = 0; j < row->glyphs; j++)
            fd2_font_draw_glyph(vga, font, fd2_text_token_at(row->name, j),
                                x + (int)j * 16, y,
                                selected == (int)row->raw_slot ? 201u : 205u, 76, -1);
        if (row->has_effect) {
            fd2_map_sprite_blit(vga, &assets->detail_icons[row->effect_icon], x + 68, y + 4, -1);
            draw_detail_number(vga, assets, x + 93, y + 4,
                               (unsigned)(row->value < 0 ? 0 : row->value), 3);
        } else {
            /* 184c0的已知物品无主属性分支：frame41在属性图位置，不是数值位置。 */
            fd2_map_sprite_blit(vga, &assets->digit_overflow[0], x + 68, y + 4, 0);
        }
    }
}

int fd2_field_item_list_draw(fd2_vga *vga,
                             const fd2_field_info_assets *assets,
                             const fd2_font *font,
                             const fd2_text_entry *text,
                             const fd2_field_unit *unit, int selected_raw_slot) {
    item_list_row rows[8];
    size_t count;
    if (!vga || prepare_item_list(assets, font, text, unit, selected_raw_slot, rows, &count) != 0)
        return -1;
    draw_item_list(vga, assets, font, rows, count, selected_raw_slot);
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
    /* 三个 helper 分别复现 field_unit_detail_transition_left @0x182ad、
     * field_unit_detail_transition_top @0x18312 与
     * field_unit_detail_transition_bottom @0x1839b；组合顺序复现
     * field_unit_detail_transition_frame @0x18409。 */
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
    /* dialog_box_draw_tiles @0x168b6 的 `(x=5,y=7,w=5,h=5)` 调用；
     * field_unit_detail_draw @0x17eef 在 DATO 立绘之前执行该调用。 */
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
    item_list_row rows[8];
    size_t count;
    /* F2 owner先预检完整详情；MAGIC owner仅需上页，并将完整覆盖下页。
     * 因而坏库存不能阻止本void入口绘制上页；下页只在准备成功后绘制。
     * 需要库存错误返回／整VGA原子性的调用者使用item_list_draw。 */
    int items_ready = prepare_item_list(assets, font, text, unit, -1, rows, &count) == 0;
    /* field_unit_detail_draw @0x17eef：先用通用 UI 小块拼 portrait
     * 边框，再绘制 portrait (8,10)、属性框 (92,7) 与装备框 (5,94)。 */
    draw_detail_portrait_border(vga, assets);
    if (portrait) fd2_map_sprite_blit(vga, portrait, 8, 10, -1);
    fd2_map_sprite_blit(vga, &assets->detail_top, 92, 7, -1);

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

    /* field_unit_detail_stats_draw @0x17fc0（原登记同名）的movsx WORD：
     * 详情与1acf3小面板的unsigned HP读取不同，LV/EX/MV仍按byte。 */
    int hp = signed_detail_word(unit->hp), hp_max = signed_detail_word(unit->hp_max);
    int mp = signed_detail_word(unit->mp), mp_max = signed_detail_word(unit->mp_max);
    draw_bar(vga, assets, 0, 198, 33, hp, hp_max);
    draw_bar(vga, assets, 1, 198, 52, mp, mp_max);
    draw_detail_number_bank(vga, assets, 267, 41, hp, 3,
                            unit->hp == unit->hp_max ? 0u : 1u);
    draw_detail_number_bank(vga, assets, 293, 41,
                            hp_max, 3, 0u);
    draw_detail_number_bank(vga, assets, 267, 59, mp, 3,
                            unit->mp == unit->mp_max ? 0u : 1u);
    draw_detail_number_bank(vga, assets, 293, 59,
                            mp_max, 3, 0u);
    draw_detail_number(vga, assets, 157, 33, record[0x21], 2);
    draw_detail_number(vga, assets, 157, 44, record[0x3c], 2);
    draw_detail_number(vga, assets, 117, 55,
                       signed_detail_word(rd_u16_le(record + 0x3e)), 3);
    draw_detail_number(vga, assets, 157, 55, unit->movement_points, 2);
    size_t hit_bank = record[0x24] == 0 ? 1u : 2u;
    draw_detail_number_bank(vga, assets, 117, 67,
                            signed_detail_word(unit->accuracy), 3, hit_bank);
    draw_detail_number_bank(vga, assets, 157, 67,
                            signed_detail_word(unit->attack), 3,
                            record[0x22] == 0 ? 1u : 2u);
    draw_detail_number_bank(vga, assets, 117, 79,
                            signed_detail_word(unit->evasion), 3, hit_bank);
    draw_detail_number_bank(vga, assets, 157, 79,
                            signed_detail_word(unit->defense), 3,
                            record[0x23] == 0 ? 1u : 2u);

    if (items_ready) draw_item_list(vga, assets, font, rows, count, -1);
}
