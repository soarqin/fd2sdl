/* 城镇／酒店准备界面：field_town_prepare_execute @0x26152（原sub_26152）、
 * field_town_view_render @0x265ec（原sub_265EC）、field_town_selection_execute
 * @0x2670e（原sub_2670E）、field_hotel_menu_execute @0x29300（原field_manual_slot_dispatch）、
 * field_manual_slot_save @0x2968d（原sub_2968D）、field_manual_slot_load @0x2986f（原登记同名）、
 * field_manual_slot_rows_draw @0x29ab2（原sub_29AB2）与field_manual_slot_picker @0x29bcb（原登记同名）。
 * 只借App准备owner；四槽绝不恢复为active。原版资源和布局来自已发布
 * canonical伪C及独立Domain/raw读集，不声称完整原版时序/RNG等价。
 * G1新增静态依据（尚未实现服务）：field_shop_menu_execute @0x279bc（原sub_279BC）、
 * field_shop_buy_execute @0x2872b（原sub_2872B）、field_shop_sell_execute @0x28cbd（原sub_28CBD）、
 * field_prepare_item_give_execute @0x28f65（原sub_28F65）、field_church_menu_execute
 * @0x29daa（原sub_29DAA）、field_church_revive_execute @0x2a43e（原sub_2A43E）、
 * field_church_promote_execute @0x2aa00（原sub_2AA00）、field_deployment_select_execute
 * @0x2af28（原sub_2AF28）及persistent_roster_selection_reorder @0x2b777（原sub_2B777）。
 * 取消/排序/物品和资金的分段写点见common-preparation-semantics.md，不能套统一无写取消。 */
#include "field_prepare_play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"
#include "field_magic.h"
#include "field_prepare.h"
#include "field_prepare_item.h"
#include "field_prepare_view.h"
#include "battle_animation.h"
#include "static_data.h"
#include "portrait.h"
#include "scene.h"

/* 原表由准备领域和页面各自只读消费，不维护第二份冷数据。 */
#include "static_data/preparation.inc"

static uint16_t read16(const uint8_t *p) {
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int image_size(const fd2_image *image, int w, int h) {
    return image->pixels && image->width == w && image->height == h;
}
static int hotel_frame(fd2_image *out, const fd2_archive *fdother,
        size_t index, int format) {
    const uint8_t *data; size_t size;
    if (fd2_archive_get(fdother, 13, &data, &size) != 0 || size < 6 ||
        memcmp(data, "LMI1", 4) != 0) return -1;
    size_t count = read16(data + 4);
    if (index >= count || count > (size - 6u) / 4u) return -1;
    size_t start = read32(data + 6u + index * 4u);
    size_t end = index + 1u < count ? read32(data + 10u + index * 4u) : size;
    if (start < 6u + count * 4u || end <= start || end > size) return -1;
    if (format == 0) return fd2_image_decode_buf(out, data + start, end - start);
    if (format == 1) return fd2_lmi_decode_image(out, data + start, end - start);
    if (end - start != 484u || read16(data + start) != 24 || read16(data + start + 2) != 20)
        return -1;
    out->pixels = malloc(480);
    if (!out->pixels) return -1;
    out->width = 24; out->height = 20;
    memcpy(out->pixels, data + start + 4, 480);
    return 0;
}

/* 279bc/29daa消费相同+6目录；12/63是嵌套DAT，14/29是LMI1。
 * 两种容器均用真实边界，不用首offset反推LMI1的显式count。 */
static int service_frame(fd2_image *out, const fd2_archive *fdother,
        size_t resource, size_t index, int format) {
    const uint8_t *data, *part; size_t size, length;
    if (fd2_archive_get(fdother, resource, &data, &size) != 0 || size < 10u) return -1;
    if (!memcmp(data, FD2_ARCHIVE_MAGIC, FD2_ARCHIVE_MAGIC_LEN)) {
        fd2_archive nested = {0};
        if (fd2_archive_open_mem(&nested, data, size) != 0) return -1;
        int result = fd2_archive_get(&nested, index, &part, &length);
        fd2_archive_close(&nested);
        if (result != 0) return -1;
    } else {
        if (memcmp(data, "LMI1", 4)) return -1;
        size_t count = read16(data + 4);
        if (index >= count || count > (size - 6u) / 4u) return -1;
        size_t start = read32(data + 6u + index * 4u);
        size_t end = index + 1u < count ? read32(data + 10u + index * 4u) : size;
        if (start < 6u + count * 4u || end <= start || end > size) return -1;
        part = data + start; length = end - start;
    }
    if (!format) return fd2_image_decode_buf(out, part, length);
    if (format == 1) return fd2_lmi_decode_image(out, part, length);
    int width = format == 2 ? 24 : 6, height = format == 2 ? 20 : format == 4 ? 99 : 8;
    size_t pixels = (size_t)width * (size_t)height;
    if ((format != 2 && format != 3 && format != 4) || length != pixels + 4u ||
        read16(part) != width || read16(part + 2) != height) return -1;
    out->pixels = malloc(pixels);
    if (!out->pixels) return -1;
    out->width = width; out->height = height;
    memcpy(out->pixels, part + 4, pixels);
    return 0;
}

static void service_assets_close(fd2_field_prepare_service_assets *service) {
    fd2_image_free(&service->background); fd2_image_free(&service->money_panel);
    fd2_image_free(&service->roster_page);
    fd2_image_free(&service->price_icon); fd2_image_free(&service->preview_arrow);
    fd2_image_free(&service->money_strip);
    for (size_t i = 0; i < 5; i++) fd2_image_free(&service->scroll_arrows[i]);
    for (size_t i = 0; i < 9; i++) fd2_image_free(&service->animation_frames[i]);
    for (size_t i = 0; i < 4; i++) fd2_image_free(&service->preview_labels[i]);
    for (size_t i = 0; i < 8; i++) fd2_image_free(&service->icons[i]);
    for (size_t i = 0; i < 4; i++) fd2_image_free(&service->portraits[i]);
    memset(service, 0, sizeof(*service));
}

int fd2_field_prepare_assets_open_service(fd2_field_prepare_assets *a,
        const fd2_archive *fdother, int selector) {
    if (!a || !a->ready || !fdother ||
        (selector != 1 && selector != 3 && selector != 4 && selector != 5)) return -1;
    fd2_field_prepare_service_assets shadow = {0};
    size_t resource = k_prepare_town_service_entries[selector];
    if (resource == 255u ||
        service_frame(&shadow.background, fdother, resource, 0, 0) != 0 ||
        service_frame(&shadow.money_panel, fdother, resource, 1, 1) != 0 ||
        service_frame(&shadow.roster_page, fdother, resource, 16, 1) != 0 ||
        !image_size(&shadow.background, 320, 200) ||
        !image_size(&shadow.money_panel, 63, 15) ||
        !image_size(&shadow.roster_page, 310, 86)) goto fail;
    for (size_t i = 0; i < 8; i++)
        if (service_frame(&shadow.icons[i], fdother, resource, i + 3u, 2) != 0) goto fail;
    for (size_t i = 0; i < 4; i++)
        if (fd2_portrait_decode_frame(&shadow.portraits[i], &a->dato,
                k_prepare_town_portraits[selector], i) != 0 ||
                !image_size(&shadow.portraits[i], 80, 80)) goto fail;
    /* 1685c ->4ed0b：frame15是6x8 raw；18..22由2825b用LMI读取。 */
    if (service_frame(&shadow.price_icon, fdother, resource, 15, 3) != 0) goto fail;
    if (service_frame(&shadow.money_strip, fdother, resource, 2, 4) != 0) goto fail;
    for (size_t i = 0; i < 5; i++)
        if (service_frame(&shadow.scroll_arrows[i], fdother, resource,
                i < 4u ? 11u + i : 17u, 1) != 0) goto fail;
    if (selector != 4) {
        for (size_t i = 0; i < 4; i++)
            if (service_frame(&shadow.preview_labels[i], fdother, resource, 18u + i, 1) != 0) goto fail;
        if (service_frame(&shadow.preview_arrow, fdother, resource, 22, 1) != 0) goto fail;
    }
    const uint8_t *sheet; size_t sheet_size, count;
    if (fd2_archive_get(fdother, resource, &sheet, &sheet_size) != 0) goto fail;
    if (!memcmp(sheet, FD2_ARCHIVE_MAGIC, FD2_ARCHIVE_MAGIC_LEN)) {
        fd2_archive nested = {0};
        if (fd2_archive_open_mem(&nested, sheet, sheet_size) != 0) goto fail;
        count = nested.count; fd2_archive_close(&nested);
    } else count = read16(sheet + 4);
    if (count < 24u || count > 32u) goto fail;
    shadow.animation_count = count - 23u;
    for (size_t i = 0; i < shadow.animation_count; i++)
        if (service_frame(&shadow.animation_frames[i], fdother, resource, 23u + i, 0) != 0) goto fail;
    shadow.selector = selector; shadow.ready = 1;
    service_assets_close(&a->service); a->service = shadow;
    return 0;
fail:
    service_assets_close(&shadow);
    return -1;
}

int fd2_field_prepare_assets_bind_roster(fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state) {
    if (!a || !a->ready || !state || state->unit_count > FD2_SAVE_MAX_UNITS) return -1;
    uint8_t ids[FD2_SAVE_MAX_UNITS]; size_t count = 0;
    for (size_t i = 0; i < state->unit_count; i++) {
        uint8_t id = state->unit_area[i * FD2_SAVE_UNIT_RECORD_SIZE + 7u];
        if ((size_t)id * 12u + 11u >= a->sprites.frame_count) return -1;
        size_t j = 0; while (j < count && ids[j] != id) j++;
        if (j == count) ids[count++] = id;
    }
    memcpy(a->sprite_cache_ids, ids, count); a->sprite_cache_count = count;
    a->sprite_cache_ready = 1;
    return 0;
}

void fd2_field_prepare_assets_close(fd2_field_prepare_assets *a) {
    if (!a) return;
    service_assets_close(&a->service);
    fd2_image_free(&a->town); fd2_image_free(&a->town_stage2); fd2_image_free(&a->town_panel);
    fd2_image_free(&a->town_type1);
    fd2_image_free(&a->hotel); fd2_image_free(&a->slots);
    for (size_t i = 0; i < 8; i++) fd2_image_free(&a->hotel_icons[i]);
    for (size_t i = 0; i < 4; i++) {
        fd2_image_free(&a->hotel_portraits[i]); fd2_image_free(&a->depart_portraits[i]);
    }
    fd2_field_command_assets_close(&a->commands);
    fd2_field_info_assets_close(&a->info);
    fd2_map_sprite_bank_close(&a->sprites);
    fd2_text_entry_close(&a->text);
    fd2_archive_close(&a->text_archive); fd2_archive_close(&a->dato);
    memset(a, 0, sizeof(*a));
}

int fd2_field_prepare_assets_open(fd2_field_prepare_assets *a, const fd2_archive *fdother) {
    if (!a || !fdother) return -1;
    memset(a, 0, sizeof(*a));
    const uint8_t *panel; size_t panel_size;
    if (fd2_image_decode_entry(&a->town, fdother, 11) != 0 ||
        fd2_archive_get(fdother, 10, &panel, &panel_size) != 0 ||
        fd2_lmi_decode_image(&a->town_panel, panel, panel_size) != 0 ||
        hotel_frame(&a->hotel, fdother, 0, 0) != 0 || hotel_frame(&a->slots, fdother, 16, 1) != 0 ||
        !image_size(&a->town, 320, 200) || !image_size(&a->town_panel, 62, 26) ||
        !image_size(&a->hotel, 320, 200) || !image_size(&a->slots, 310, 86) ||
        fd2_archive_open(&a->text_archive, "original_game/FDTXT.DAT") != 0 ||
        fd2_archive_open(&a->dato, "original_game/DATO.DAT") != 0 ||
        fd2_text_entry_open_entry(&a->text, &a->text_archive, 0) != 0 ||
        fd2_font_open_fdother(&a->font, fdother, 4) != 0 ||
        fd2_map_sprite_bank_open(&a->sprites, "original_game/FDICON.B24") != 0 ||
        fd2_field_info_assets_open(&a->info, fdother) != 0 ||
        fd2_field_command_assets_open(&a->commands, fdother) != 0 ||
        fd2_scene_field_message_validate(fdother) != 0) goto fail;
    for (size_t i = 0; i < 8; i++)
        if (hotel_frame(&a->hotel_icons[i], fdother, i + 3u, 2) != 0) goto fail;
    for (size_t i = 0; i < 4; i++)
        if (fd2_portrait_decode_frame(&a->hotel_portraits[i], &a->dato, 129, i) != 0 ||
            fd2_portrait_decode_frame(&a->depart_portraits[i], &a->dato, 75, i) != 0 ||
            !image_size(&a->hotel_portraits[i], 80, 80) ||
            !image_size(&a->depart_portraits[i], 80, 80)) goto fail;
    a->ready = 1;
    return 0;
fail:
    fd2_field_prepare_assets_close(a);
    return -1;
}

/* 26152/265ec：准备能力独立于战场handler；NoTown先于普通31byte行。 */
static int prepare_is_notown(size_t stage) {
    return stage<sizeof(k_prepare_no_town) && k_prepare_no_town[stage]!=0;
}
static int prepare_stage_valid(size_t stage) {
    return stage>0u && stage<sizeof(k_prepare_no_town) &&
        (prepare_is_notown(stage) || fd2_field_stage_prepare_record_get(stage));
}
static int town_type(size_t stage) {
    const uint8_t *row = fd2_field_stage_prepare_record_get(stage);
    return !prepare_is_notown(stage) && row && row[0]<=2u ? row[0] : -1;
}

static int prepare_town(fd2_field_prepare_assets *a, const fd2_archive *fdother, size_t stage) {
    if (!a || !a->ready || !fdother) return -1;
    if (prepare_is_notown(stage)) return 0;
    int type = town_type(stage);
    if (type == 0) return image_size(&a->town, 320, 200) ? 0 : -1;
    if (type != 1 && type != 2) return -1;
    fd2_image *target=type==1 ? &a->town_type1:&a->town_stage2;
    if (target->pixels) return image_size(target, 320, 200) ? 0 : -1;
    fd2_image shadow = {0};
    if (fd2_image_decode_entry(&shadow, fdother, k_prepare_town_background_entries[type]) != 0 || !image_size(&shadow, 320, 200)) {
        fd2_image_free(&shadow); return -1;
    }
    *target = shadow;
    return 0;
}

/* 15f84：片段展开复用战场纯能力；-3等待由冷流程owner处理，纯页不得吞掉。 */
static int text_draw_parameters(fd2_vga *vga, const fd2_field_prepare_assets *a,
        size_t fragment, int x, int y, uint8_t fg, int bg,
        const fd2_text_parameters *parameters) {
    int origin = x; fd2_text_expanded expanded;
    if (!a->font.glyphs || a->font.glyph_count > a->font.size / 32u ||
        fd2_text_expand(&a->text, a->font.glyph_count, parameters, fragment, &expanded) != 0) return -1;
    for (size_t i = 0; i < expanded.count; i++) {
        uint16_t token = expanded.tokens[i];
        if (token == 0xfffeu) { y += 19; x = origin; continue; }
        if (token == 0xfffdu || x < 0 || x + 16 > VGA_W || y < 0 || y + 17 > VGA_H) return -1;
        if (token == FD2_TEXT_SAFE_MINUS) {
            const fd2_image *minus = &a->info.signs[1];
            if (!minus->pixels || minus->width < 1 || minus->width > 16 ||
                minus->height < 1 || minus->height > 16) return -1;
            fd2_map_sprite_blit(vga, minus, x, y, 0);
        } else fd2_font_draw_glyph(vga, &a->font, token, x, y, fg, 76, bg);
        x += 16;
    }
    return 0;
}

static int text_draw(fd2_vga *vga, const fd2_field_prepare_assets *a, size_t fragment,
        int x, int y, uint8_t fg, int bg, uint32_t number) {
    fd2_text_parameters parameters = {number, SIZE_MAX, SIZE_MAX};
    return text_draw_parameters(vga, a, fragment, x, y, fg, bg, &parameters);
}

static fd2_vga *new_frame(const fd2_vga *source) {
    fd2_vga *frame = calloc(1, sizeof(*frame));
    if (frame && source) memcpy(frame->framebuffer, source->framebuffer, VGA_W * VGA_H);
    return frame;
}
static int finish_frame(fd2_vga *vga, fd2_vga *frame, int result) {
    if (result == 0) memcpy(vga->framebuffer, frame->framebuffer, VGA_W * VGA_H);
    free(frame); return result;
}

int fd2_field_prepare_draw_town(fd2_vga *vga, const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, int selected, unsigned phase) {
    if (!vga || !a || !a->ready || !state ||
        state->unit_count > FD2_SAVE_MAX_UNITS || selected < 0 || selected > 5 || phase > 3 ||
        !image_size(&a->town_panel, 62, 26)) return -1;
    int type = town_type(state->stage_id);
    if (type < 0 || type > 2) return -1;
    const fd2_image *town = type == 0 ? &a->town : type==1 ? &a->town_type1:&a->town_stage2;
    const uint8_t *x=k_prepare_town_x[type],*y=k_prepare_town_y[type];
    if (!image_size(town, 320, 200)) return -1;
    if (state->unit_count && (size_t)state->unit_area[7] >= a->sprites.frame_count / 12u) return -1;
    fd2_vga *frame = new_frame(NULL); if (!frame) return -1;
    for (size_t row = 0; row < 192; row++)
        memcpy(frame->framebuffer + (row + 4u) * VGA_STRIDE + 4,
               town->pixels + row * VGA_STRIDE, 312);
    fd2_map_sprite_blit(frame, &a->town_panel, 248, 166, -1);
    int result = text_draw(frame, a, (size_t)selected + 495u, 256, 172, 205, 74, 0);
    if (state->unit_count) fd2_map_sprite_blit_frame(frame, &a->sprites,
        (size_t)state->unit_area[7] * 12u + (phase == 3 ? 1u : phase), x[selected] + 4, y[selected] + 4, 0);
    return finish_frame(vga, frame, result);
}

int fd2_field_prepare_draw_hotel(fd2_vga *vga, const fd2_field_prepare_assets *a,
        const fd2_archive *fdother, int selected, unsigned highlight, unsigned portrait, size_t fragment) {
    if (!vga || !a || !a->ready || !fdother || selected < -1 || selected > 3 ||
        highlight > 1 || portrait > 3 || !image_size(&a->hotel, 320, 200) ||
        !image_size(&a->hotel_portraits[portrait], 80, 80)) return -1;
    fd2_vga *frame = new_frame(NULL); if (!frame) return -1;
    fd2_map_sprite_blit(frame, &a->hotel, 0, 0, 0);
    /* special portrait不在bottom slide复制区；16559实际forward(107,5)。 */
    fd2_map_sprite_blit(frame, &a->hotel_portraits[portrait], 107, 5, -1);
    int result = fd2_scene_field_message_box(frame, fdother);
    if (result == 0) result = text_draw(frame, a, fragment, 12, 119, 205, 74, 0);
    if (result == 0 && selected >= 0) {
        for (int row = 169; row < 189; row++) memset(frame->framebuffer + row * VGA_STRIDE + 201, 74, 104);
        for (int i = 0; i < 4; i++) {
            size_t index = (size_t)i * 2u + (i == selected && highlight ? 1u : 0u);
            if (!image_size(&a->hotel_icons[index], 24, 20)) { result = -1; break; }
            fd2_map_sprite_blit(frame, &a->hotel_icons[index], 201 + i * 26, 169, 0);
        }
    }
    return finish_frame(vga, frame, result);
}

int fd2_field_prepare_draw_service(fd2_vga *vga, const fd2_field_prepare_assets *a,
        const fd2_archive *fdother, const fd2_save_manual_state *state,
        int selected, unsigned highlight, unsigned portrait, size_t fragment) {
    if (!vga || !a || !a->ready || !fdother || !state ||
        state->unit_count > FD2_SAVE_MAX_UNITS || !a->service.ready ||
        (a->service.selector != 1 && a->service.selector != 3 &&
         a->service.selector != 4 && a->service.selector != 5) ||
        selected < -1 || selected > 3 || highlight > 1u || portrait > 3u ||
        !image_size(&a->service.background, 320, 200) ||
        !image_size(&a->service.money_panel, 63, 15) ||
        !image_size(&a->service.portraits[portrait], 80, 80)) return -1;
    fd2_vga *frame = new_frame(NULL); if (!frame) return -1;
    fd2_map_sprite_blit(frame, &a->service.background, 0, 0, 0);
    fd2_map_sprite_blit(frame, &a->service.money_panel, 5, 95, -1);
    /* 279bc/29daa ->187d6：signed负值画0，至少8位，超过8位取前8。
     * 这只是末态数字；26a7a/26b91的金额滚动另由服务流程消费。 */
    char digits[11];
    unsigned value = state->gold_or_flags <= INT32_MAX ? state->gold_or_flags : 0u;
    (void)snprintf(digits, sizeof(digits), "%08u", value);
    for (size_t i = 0; i < 8; i++)
        fd2_map_sprite_blit(frame, &a->info.digits[0][digits[i] - '0'], 16 + (int)i * 6, 99, 0);
    unsigned origin = fd2_portrait_notice_origin(k_prepare_town_portraits[a->service.selector]);
    fd2_map_sprite_blit(frame, &a->service.portraits[portrait], (int)(origin % VGA_STRIDE), (int)(origin / VGA_STRIDE), -1);
    int result = fd2_scene_field_message_box(frame, fdother);
    if (!result) result = text_draw(frame, a, fragment, 12, 119, 205, 74, 0);
    if (!result && selected >= 0) {
        for (int row = 169; row < 189; row++) memset(frame->framebuffer + row * VGA_STRIDE + 201, 74, 104);
        for (int i = 0; i < 4; i++) {
            size_t index = (size_t)i * 2u + (i == selected && highlight ? 1u : 0u);
            if (!image_size(&a->service.icons[index], 24, 20)) { result = -1; break; }
            fd2_map_sprite_blit(frame, &a->service.icons[index], 201 + i * 26, 169, 0);
        }
    }
    return finish_frame(vga, frame, result);
}

int fd2_field_prepare_draw_slots(fd2_vga *vga, const fd2_field_prepare_assets *a,
        const fd2_save_file *save, int selected) {
    if (!vga || !a || !a->ready || !save || selected < 0 || selected >= (int)FD2_SAVE_SLOT_COUNT ||
        !image_size(&a->slots, 310, 86) || fd2_save_validate_plain(save->data, save->size) != 0) return -1;
    fd2_vga *frame = new_frame(vga); if (!frame) return -1;
    fd2_map_sprite_blit(frame, &a->slots, 5, 112, -1);
    int result = 0;
    for (size_t i = 0; result == 0 && i < FD2_SAVE_SLOT_COUNT; i++) {
        fd2_save_manual_slot slot;
        if (fd2_save_file_get_manual_slot_raw(save, i, &slot) != 0) { result = -1; break; }
        int row = 119 + (int)i * 19; uint8_t fg = (int)i == selected ? 201 : 205;
        result = text_draw(frame, a, 549, 10, row, fg, -1, (uint32_t)i + 1u);
        if (result == 0 && slot.meta[0] == 0xffu) result = text_draw(frame, a, 514, 88, row, fg, -1, 0);
        else if (result == 0) {
            result = text_draw(frame, a, 514u + slot.meta[0], 40, row, fg, -1, 0);
            if (result == 0) result = text_draw(frame, a, 550u + slot.meta[0], 130, row, fg, -1, 0);
        }
    }
    return finish_frame(vga, frame, result);
}

int fd2_field_prepare_draw_roster(fd2_vga *vga, const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, int selected, size_t first, unsigned phase) {
    if (!vga || !a || !a->ready || !state || state->unit_count > FD2_SAVE_MAX_UNITS ||
        selected < 0 || (size_t)selected >= state->unit_count || first > (size_t)selected ||
        (size_t)selected - first >= 6u || phase > 3 || !image_size(&a->slots, 310, 86)) return -1;
    fd2_vga *frame = new_frame(vga); if (!frame) return -1;
    fd2_map_sprite_blit(frame, &a->slots, 5, 112, -1);
    size_t count = state->unit_count - first; if (count > 6u) count = 6u;
    int result = 0;
    for (size_t i = 0; i < count; i++) {
        const uint8_t *unit = state->unit_area + (i + first) * FD2_SAVE_UNIT_RECORD_SIZE;
        uint8_t sprite=0;int cached;
        if(a->sprite_cache_ready) {
            if(a->sprite_cache_count>FD2_SAVE_MAX_UNITS){result=-1;break;}
            cached=i+first<a->sprite_cache_count;
            if(cached)sprite=a->sprite_cache_ids[i+first];
        } else cached=fd2_field_prepare_cached_sprite_id(state,i+first,&sprite);
        if(cached<0 || (cached && (size_t)sprite>=a->sprites.frame_count/12u)){result=-1;break;}
        int col = (int)(i % 2u) * 132, row = (int)(i / 2u) * 26;
        if(cached)fd2_map_sprite_blit_frame(frame, &a->sprites, (size_t)sprite * 12u + (phase == 3 ? 1u : phase),
            14 + col, 117 + row, 0);
        result = text_draw(frame, a, (size_t)unit[8] + 1u, 40 + col, 121 + row,
            i + first == (size_t)selected ? 201 : 205, -1, 0);
        if (result != 0) break;
    }
    return finish_frame(vga, frame, result);
}

typedef struct {
    /* 原prepare UI的临时选择/图游标与视觉资源，不复制真实roster/资金/RNG。 */
    int selector, selected_command, product_index, product_first;
    uint8_t stock[12], candidates[FD2_SAVE_MAX_UNITS], visible_items[8];
    uint8_t selection[FD2_SAVE_MAX_UNITS - 1u];
    size_t stock_count, candidate_count, item_count, first;
    int member, receiver, raw_slot, member_kind, item_kind, cursor;
    uint8_t product_id, captured_item, required_text;
    uint32_t price, previous_money;
    size_t new_fragment, old_fragment, quota;
    fd2_field_prepare_promotion promotion;
    unsigned growth_index, growth_line;
    uint16_t growth_delta;
    int notown_mode, done, result, host_quit, irreversible, running;
    uint64_t input_epoch;
    fd2_input_event deferred_key;
    int deferred_key_ready;
    uint8_t *page_background, *town_background;
    fd2_vga *page;
    fd2_image notice_portraits[4], selection_label, selection_cursor;
    const fd2_image *active_portraits; /* 借service assets，或本结构的notice图。 */
    unsigned notice_origin, sprite_phase, highlight_phase, glyph_counter, mouth_phase;
    int notice_open, page_kind, picker_mode, equipment_selected;
    fd2_image equipment_portraits[4];
    const fd2_image *equipment_previous_portraits;
    unsigned equipment_previous_origin;
    fd2_archive promotion_backgrounds, promotion_figani;
    fd2_prepare_promotion *promotion_visual;
} prepare_service;

typedef struct {
    fd2_vga *vga;
    const fd2_archive *fdother;
    fd2_field_prepare_assets *assets;
    fd2_field_play_context *context;
    fd2_field_audio *audio;
    const char *save_path;
    int menu_selected; /* -1非hotel四图标等待，不是持久owner。 */
    size_t roster_first;
    int roster_selected;
    prepare_service service;
} prepare_ui;

static int quit(prepare_ui *ui) { return fd2_input_take_quit(&ui->vga->input); }
static void drain(prepare_ui *ui) {
    while (fd2_input_take_key(&ui->vga->input, NULL)) {}
}
static int sound_loop(prepare_ui *ui,size_t sample,int loop_count) {
    if(loop_count<1 || loop_count>3)return -1;
    if (ui->context->has_preparation && !ui->context->preparation.options[3]) return 0;
    if (!ui->audio || !ui->audio->player) return 0;
    if (!ui->audio->ui_bank) return -1;
    return fd2_pcm_play_replace(ui->audio->player, ui->audio->ui_bank, sample, loop_count, FD2_GAME_SFX_DEFAULT_GAIN);
}
static int sound(prepare_ui *ui,size_t sample){return sound_loop(ui,sample,1);}
/* 2af28切换同伴用secondary7，紧接右移primary0，不能在同一voice互相替掉。 */
static int prepare_service_selection_sound(prepare_ui *ui,size_t sample,int secondary) {
    if(!secondary)return sound(ui,sample);
    if(ui->context->has_preparation && !ui->context->preparation.options[3])return 0;
    if(!ui->audio || !ui->audio->secondary_player)return 0;
    if(!ui->audio->ui_bank)return -1;
    return fd2_pcm_play_replace(ui->audio->secondary_player,ui->audio->ui_bank,
        sample,1,FD2_GAME_SFX_DEFAULT_GAIN);
}
static int sync_audio_options(prepare_ui *ui) {
    if (ui->context->has_preparation) {
        ui->context->music_enabled = ui->context->preparation.options[2];
        ui->context->sfx_enabled = ui->context->preparation.options[3];
        if (ui->context->bgm && fd2_bgm_set_music_enabled(ui->context->bgm,
                ui->context->preparation.options[2] != 0, 0) != 0) return -1;
        if (ui->audio && ui->audio->player && ui->audio->player->audio && fd2_pcm_player_set_enabled(ui->audio->player,
                ui->context->preparation.options[3] != 0) != 0) return -1;
        if (ui->audio && ui->audio->secondary_player && ui->audio->secondary_player->audio && fd2_pcm_player_set_enabled(ui->audio->secondary_player,
                ui->context->preparation.options[3] != 0) != 0) return -1;
    }
    return 0;
}
static int music(prepare_ui *ui, int track, const char *reason) {
    if (sync_audio_options(ui) != 0) return -1;
    size_t stage = ui->context->has_preparation ? ui->context->preparation.stage_id : 1u;
    return fd2_field_play_request_music(ui->context, stage, track, reason);
}
static int present(prepare_ui *ui) {
    if (quit(ui)) return 1;
    fd2_vga_present_timed(ui->vga, 16);
    return quit(ui) ? 1 : 0;
}

/* 26eda/16c57均借App共享RNG；这里只恢复已证输入等待和blink，不把
 * SDL 16ms主机采样称作BIOS tick逐时刻一致。标题slot读取不耗等待RNG。 */
typedef struct { uint64_t last; int countdown, portrait, phase, initialized; } prepare_wait;
static int read_key_state(prepare_ui *ui, fd2_input_event *event, int wait_rng,
        int hotel_portrait, prepare_wait *state) {
    if (!state->initialized) {
        state->last = SDL_GetTicks(); state->phase = 2; state->portrait = 0;
        state->countdown = !wait_rng ? 0 : wait_rng == 2 ?
            (int)(fd2_field_rng_next(&ui->context->rng) % 50u) + 8 :
            (int)(fd2_field_rng_next(&ui->context->rng) % 30u) + 2;
        state->initialized = 1;
    }
    for (;;) {
        if (quit(ui)) return 1;
        if (fd2_input_take_key(&ui->vga->input, event)) {
            drain(ui); fd2_vga_wait_frame_deadline(ui->vga); return 0;
        }
        if (wait_rng && SDL_GetTicks() - state->last >= 110u) {
            state->last = SDL_GetTicks();
            state->phase = (state->phase + 1) & 3;
            if (state->portrait == 3) {
                state->portrait = 0;
                state->countdown = (int)(fd2_field_rng_next(&ui->context->rng) % 30u) + 2;
            } else if (state->countdown-- == 0) {
                state->portrait = 3;
            }
            if (hotel_portrait) fd2_map_sprite_blit(ui->vga,
                &ui->assets->hotel_portraits[state->portrait], 107, 5, -1);
            if (ui->menu_selected >= 0) for (int i = 0; i < 4; i++)
                fd2_map_sprite_blit(ui->vga, &ui->assets->hotel_icons[i * 2 +
                    (i == ui->menu_selected && state->phase / 2 ? 1 : 0)], 201 + i * 26, 169, 0);
            if (ui->roster_selected >= 0 && fd2_field_prepare_draw_roster(ui->vga, ui->assets,
                    &ui->context->preparation, ui->roster_selected, ui->roster_first,
                    (unsigned)state->phase) != 0) return -1;
        }
        if (present(ui)) return 1;
    }
}
static int read_key(prepare_ui *ui, fd2_input_event *event, int wait_rng, int hotel_portrait) {
    prepare_wait state = {0};
    return read_key_state(ui, event, wait_rng, hotel_portrait, &state);
}

static int confirm_key(fd2_input_key key) {
    return key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE || key == FD2_INPUT_KEY_KEYPAD_CONFIRM;
}
static int cancel_key(fd2_input_key key) {
    return key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL;
}

static int bottom_slide(prepare_ui *ui, const uint8_t *background,
        const uint8_t *page, int opening) {
    if (sound(ui, opening ? 5u : 6u) != 0) return -1;
    int first = opening ? 5 : 1, last = opening ? 0 : 5, step = opening ? -1 : 1;
    for (int phase = first;; phase += step) {
        if (quit(ui)) return 1;
        memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
        int y = 112 + phase * 13;
        int rows = 200 - y; if (rows > 86) rows = 86;
        for (int row = 0; row < rows; row++)
            memcpy(ui->vga->framebuffer + (y + row) * VGA_STRIDE + 5,
                   page + (112 + row) * VGA_STRIDE + 5, 310);
        if (present(ui)) return 1;
        if (phase == last) break;
    }
    if (!opening) {
        memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
        if (present(ui)) return 1;
    }
    return 0;
}

static int notice(prepare_ui *ui, size_t fragment, int wait, int second_line) {
    /* 585/586/478/479/660的hotel bottom已开；不清掉上半店员画像。 */
    if (quit(ui)) return 1;
    int result = fd2_scene_field_message_box(ui->vga, ui->fdother);
    if (result == 0) result = text_draw(ui->vga, ui->assets, fragment,
        12, second_line ? 138 : 119, 205, 74, ui->context->message_number);
    if (result != 0) return -1;
    fd2_map_sprite_blit(ui->vga, &ui->assets->hotel_portraits[0], 107, 5, -1);
    drain(ui);
    if (present(ui)) return 1;
    if (wait) { fd2_input_event event; return read_key(ui, &event, 1, 1); }
    return 0;
}

static int notice_ready(prepare_ui *ui, size_t fragment) {
    fd2_vga *checked = new_frame(NULL);
    if (!checked) return 0;
    int valid = text_draw(checked, ui->assets, fragment, 12, 119, 205, 74,
        ui->context->message_number) == 0;
    free(checked);
    return valid;
}

static int choice(prepare_ui *ui, int depart) {
    fd2_vga *page = new_frame(ui->vga);
    if (!page) return -1;
    int result = fd2_scene_field_message_box(page, ui->fdother);
    if (depart) {
        const fd2_image *portrait = &ui->assets->depart_portraits[0];
        /* 1956b默认36887是右端，实际镜像覆盖x8..87。 */
        for (int y = 0; y < 80; y++) for (int x = 0; x < 80; x++)
            page->framebuffer[(115 + y) * VGA_STRIDE + 8 + x] = portrait->pixels[y * 80 + 79 - x];
    }
    if (!result) result = text_draw(page, ui->assets, depart ? 513u : 415u,
        depart ? 95 : 12, 119, 205, 74, ui->context->message_number);
    if (result != 0) { free(page); return -1; }
    int selected = 0;
    prepare_wait waiting = {0};
    drain(ui);
    for (int step = 1; step <= 4; step++) {
        memcpy(ui->vga->framebuffer, page->framebuffer, VGA_W * VGA_H);
        fd2_map_sprite_blit(ui->vga, &ui->assets->commands.frames[49], 248 - step * 4, 168, 0);
        fd2_map_sprite_blit(ui->vga, &ui->assets->commands.frames[51], 248 + step * 4, 168, 0);
        if (present(ui)) { free(page); return 1; }
    }
    for (;;) {
        if (quit(ui)) { result = 1; break; }
        fd2_input_event event;
        int read = read_key_state(ui, &event, 1, !depart, &waiting);
        if (read) { result = read; break; }
        if (event.key == FD2_INPUT_KEY_LEFT) selected = 0;
        else if (event.key == FD2_INPUT_KEY_RIGHT) selected = 1;
        else if (cancel_key(event.key)) { result = 2; break; }
        else if (confirm_key(event.key)) { result = selected == 0 ? 0 : 2; break; }
        memcpy(ui->vga->framebuffer, page->framebuffer, VGA_W * VGA_H);
        fd2_map_sprite_blit(ui->vga, &ui->assets->commands.frames[selected == 0 ? 49 : 48], 232, 168, 0);
        fd2_map_sprite_blit(ui->vga, &ui->assets->commands.frames[selected == 1 ? 52 : 51], 264, 168, 0);
        if (present(ui)) { result = 1; break; }
    }
    if (result == 0 || result == 2) {
        for (int step = 1; step <= 4; step++) {
            memcpy(ui->vga->framebuffer, page->framebuffer, VGA_W * VGA_H);
            fd2_map_sprite_blit(ui->vga, &ui->assets->commands.frames[48], 232 + step * 4, 168, 0);
            fd2_map_sprite_blit(ui->vga, &ui->assets->commands.frames[51], 264 - step * 4, 168, 0);
            if (present(ui)) { result = 1; break; }
        }
        if (result != 1) {
            memcpy(ui->vga->framebuffer, page->framebuffer, VGA_W * VGA_H);
            if (present(ui)) result = 1;
            drain(ui);
        }
    }
    free(page);
    return result; /* 0 yes /2 no or cancel /1 hostquit /-1 resource */
}

enum { PREPARE_SLOT_HOTEL, PREPARE_SLOT_TITLE, PREPARE_SLOT_NOTOWN_SAVE };

/* 26152 @2632f传0给2968d：NoTown的29bcb直接BIOS读键，且297f7
 * 跳过660/16c57反馈；酒店29300传1。调用来源由cold/manual入口给出，
 * 不按当前stage重建分支。返回0退出picker，2成功Load，1宿主退出，-1错。 */
static int slots_run(prepare_ui *ui, int save_mode, int consumer) {
    if ((consumer!=PREPARE_SLOT_HOTEL && consumer!=PREPARE_SLOT_TITLE && consumer!=PREPARE_SLOT_NOTOWN_SAVE) ||
        (consumer==PREPARE_SLOT_TITLE && save_mode) || (consumer==PREPARE_SLOT_NOTOWN_SAVE && !save_mode)) return -1;
    int title_mode=consumer==PREPARE_SLOT_TITLE;
    int hotel_wait=consumer==PREPARE_SLOT_HOTEL;
    fd2_save_file save = {0};
    int opened = save_mode ? fd2_save_file_open_for_update(&save, ui->save_path) :
                            fd2_save_file_open(&save, ui->save_path);
    if (opened != 0) return -1; /* 坏档绝不作空档；Load不新建。 */
    uint8_t *background = malloc(VGA_W * VGA_H);
    fd2_vga *page = new_frame(ui->vga);
    if (!background || !page) { free(background); free(page); fd2_save_file_close(&save); return -1; }
    memcpy(background, ui->vga->framebuffer, VGA_W * VGA_H);
    int selected = 0, result = -1;
    drain(ui);
    for (;;) {
        memcpy(page->framebuffer, background, VGA_W * VGA_H);
        if (fd2_field_prepare_draw_slots(page, ui->assets, &save, selected) != 0) { result = -1; break; }
        /* 29ab2明确逐行写53AE1，画完值4；纯绘制API自身不写它。 */
        ui->context->message_number = 4;
        result = bottom_slide(ui, background, page->framebuffer, 1);
        if (result != 0) break;
        for (;;) {
            fd2_input_event event;
            result = read_key(ui, &event, hotel_wait, hotel_wait);
            if (result != 0) break;
            if (event.key == FD2_INPUT_KEY_UP && selected > 0) selected--;
            else if (event.key == FD2_INPUT_KEY_DOWN && selected < 3) selected++;
            else if (cancel_key(event.key)) { result = 0; goto done; }
            else if (confirm_key(event.key)) break;
            else continue;
            if (sound(ui, 7) != 0) { result = -1; break; }
            memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
            if (fd2_field_prepare_draw_slots(ui->vga, ui->assets, &save, selected) != 0) { result = -1; break; }
            memcpy(page->framebuffer, ui->vga->framebuffer, VGA_W * VGA_H);
            if (present(ui)) { result = 1; break; }
        }
        if (result != 0) break;
        if (save_mode) {
            if (hotel_wait && !notice_ready(ui, 660)) { result = -1; break; }
            /* field_manual_slot_save @0x2968d（原sub_2968D）在29bcb确认后
             * 先写完整SAV，297f7之后才26996收框。收框退出/失败不撤销写入，
             * 也不再次提交；酒店660资源仍在不可逆写入前预检。 */
            if (fd2_save_manual_state_save(ui->save_path, (size_t)selected,
                    &ui->context->preparation) != 0) { result = -1; break; }
            fd2_save_file_close(&save);
            if (fd2_save_file_open(&save, ui->save_path) != 0) { result = -1; break; }
            printf("prepare save: slot=%d stage=%u roster=%u\n", selected,
                ui->context->preparation.stage_id, ui->context->preparation.unit_count); fflush(stdout);
            result = bottom_slide(ui, background, page->framebuffer, 0);
            if (result != 0) break;
            if (quit(ui)) { result = 1; break; }
            if (hotel_wait) {
                result = notice(ui, 660, 1, 0);
                if (result != 0) break;
            }
            memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
        } else {
            result = bottom_slide(ui, background, page->framebuffer, 0);
            if (result != 0) break;
            if (quit(ui)) { result = 1; break; }
            fd2_save_manual_state loaded;
            if (fd2_save_manual_state_load(ui->save_path, (size_t)selected, &loaded) != 0) {
                /* 空槽是正常未提交，格式错误由严格后端拒绝而不改owner。 */
                if (fd2_save_file_slot_valid(&save, (size_t)selected)) { result = -1; break; }
                memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
                continue;
            }
            if (fd2_field_game_validate_manual_state(&loaded) != 0 ||
                (!title_mode && prepare_is_notown(loaded.stage_id))) {
                if (title_mode) { result = -1; break; }
                result = notice(ui, 479, 1, 0);
                if (result != 0) break;
                memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
                continue;
            }
            if (prepare_town(ui->assets, ui->fdother, loaded.stage_id) != 0) { result = -1; break; }
            if (!title_mode && !notice_ready(ui, 478)) { result = -1; break; }
            /* owner只在整场资源/数据预检成功且宿主未退出时交接。 */
            if (quit(ui)) { result = 1; break; }
            if(fd2_field_prepare_assets_bind_roster(ui->assets,&loaded)!=0){result=-1;break;}
            ui->context->preparation = loaded;
            ui->context->has_preparation = 1;
            /* field_manual_slot_load @0x2986f（原同名登记）只恢复flag；
             * 酒店13不重开/不变音量，未来cue立即服从新flag。 */
            if (sync_audio_options(ui) != 0) { result = -1; break; }
            printf("prepare load: slot=%d stage=%u roster=%u title=%d\n", selected,
                loaded.stage_id, loaded.unit_count, title_mode); fflush(stdout);
            if (!title_mode) { result = notice(ui, 478, 1, 0); if (result != 0) break; }
            result = 2; break;
        }
    }
done:
    if (result == 0) {
        int close = bottom_slide(ui, background, page->framebuffer, 0);
        if (close != 0) result = close;
    }
    fd2_vga_wait_frame_deadline(ui->vga);
    free(page); free(background); fd2_save_file_close(&save);
    return result;
}

static int roster_step(int key, size_t count, int *selected, size_t *first) {
    if (!count || count > FD2_SAVE_MAX_UNITS || !selected || !first ||
        *selected < 0 || (size_t)*selected >= count || *first > (size_t)*selected ||
        (*first & 1u) || (size_t)*selected - *first >= 6u) return -1;
    int next = *selected;
    if (key == FD2_INPUT_KEY_LEFT && next > 0) next--;
    else if (key == FD2_INPUT_KEY_RIGHT && (size_t)(next + 1) < count) next++;
    else if (key == FD2_INPUT_KEY_UP && next >= 2) next -= 2;
    else if (key == FD2_INPUT_KEY_DOWN && (size_t)(next + 2) < count) next += 2;
    if ((size_t)next < *first) *first -= 2u;
    else if ((size_t)next - *first >= 6u) *first += 2u;
    *selected = next;
    return 0;
}

static void detail_bottom(uint8_t *dst, const uint8_t *page,
        const uint8_t *background, int phase) {
    memcpy(dst, background, VGA_W * VGA_H);
    for (int row = 7; row < 93; row++) memcpy(dst + row * VGA_STRIDE + 5,
        page + row * VGA_STRIDE + 5, 310);
    if (phase == 7) return;
    int y = 94 + phase * 16, rows = 200 - y; if (rows > 102) rows = 102;
    for (int row = 0; row < rows; row++) memcpy(dst + (y + row) * VGA_STRIDE + 5,
        page + (94 + row) * VGA_STRIDE + 5, 310);
}

static int prepare_service_wait_key(prepare_ui *ui,fd2_input_event *event,int mode);

static int member_detail(prepare_ui *ui, size_t index) {
    if (index >= ui->context->preparation.unit_count) return -1;
    fd2_field_unit unit;
    memcpy(&unit, ui->context->preparation.unit_area + index * sizeof(unit), sizeof(unit));
    uint8_t *background = malloc(VGA_W * VGA_H);
    fd2_vga *page = new_frame(ui->vga);
    fd2_image portraits[4] = {{0}};
    const fd2_image *previous_portraits=ui->service.active_portraits;
    unsigned previous_origin=ui->service.notice_origin;
    if (!background || !page) { free(background); free(page); return -1; }
    memcpy(background, ui->vga->framebuffer, VGA_W * VGA_H);
    int result = -1;
    if (!fd2_field_detail_upper_is_ready(&ui->assets->info,&ui->assets->font,&ui->assets->text,&unit) ||
        fd2_field_item_list_draw(page, &ui->assets->info, &ui->assets->font,
            &ui->assets->text, &unit, -1) != 0) goto done;
    for(size_t frame=0;frame<4u;frame++)
        if(fd2_portrait_decode_frame(&portraits[frame],&ui->assets->dato,unit.unit_id,frame)!=0 ||
            !image_size(&portraits[frame],80,80))goto done;
    size_t magic_count = fd2_field_unit_magic_list_build(&unit, NULL, 0);
    if (magic_count && fd2_field_magic_list_draw(page, &ui->assets->info,
        &ui->assets->font, &ui->assets->text, &unit, -1) != 0) goto done;
    fd2_field_detail_draw(page, &ui->assets->info, &ui->assets->font,
        &ui->assets->text, &unit, &portraits[0]);
    ui->service.active_portraits=portraits;ui->service.notice_origin=3208u;
    drain(ui);
    for (int phase = 11; phase >= 0; phase--) {
        int cue=fd2_field_detail_sfx_for_phase(1,phase);
        if(cue>=0 && sound(ui,(size_t)cue)!=0)goto done;
        fd2_field_detail_transition_frame(ui->vga->framebuffer, page->framebuffer, background, phase);
        if (present(ui)) { result = 1; goto done; }
    }
    fd2_input_event event;
    result = prepare_service_wait_key(ui,&event,0);
    if (result != 0) goto done;
    if (magic_count) {
        if (sound(ui, 6) != 0) { result = -1; goto done; }
        for (int phase = 0; phase <= 7; phase++) {
            detail_bottom(ui->vga->framebuffer, page->framebuffer, background, phase);
            if (present(ui)) { result = 1; goto done; }
        }
        if (fd2_field_magic_list_draw(page, &ui->assets->info, &ui->assets->font,
            &ui->assets->text, &unit, -1) != 0 || sound(ui, 5) != 0) { result = -1; goto done; }
        for (int phase = 6; phase >= 0; phase--) {
            detail_bottom(ui->vga->framebuffer, page->framebuffer, background, phase);
            if (present(ui)) { result = 1; goto done; }
        }
        result = prepare_service_wait_key(ui,&event,0);
        if (result != 0) goto done;
    }
    for (int phase = 0; phase < 12; phase++) {
        int cue=fd2_field_detail_sfx_for_phase(0,phase);
        if(cue>=0 && sound(ui,(size_t)cue)!=0){result=-1;goto done;}
        fd2_field_detail_transition_frame(ui->vga->framebuffer, page->framebuffer, background, phase);
        if (present(ui)) { result = 1; goto done; }
    }
    memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
    result = present(ui);
done:
    fd2_vga_wait_frame_deadline(ui->vga);
    ui->service.active_portraits=previous_portraits;ui->service.notice_origin=previous_origin;
    free(page); free(background);
    for(size_t frame=0;frame<4u;frame++)fd2_image_free(&portraits[frame]);
    return result;
}

static int roster_run(prepare_ui *ui) {
    if (ui->context->preparation.unit_count == 0) return 0;
    uint8_t *background = malloc(VGA_W * VGA_H);
    fd2_vga *page = new_frame(ui->vga);
    if (!background || !page) { free(background); free(page); return -1; }
    memcpy(background, ui->vga->framebuffer, VGA_W * VGA_H);
    int result = -1;
    for (;;) {
        int selected = 0; size_t first = 0;
        memcpy(page->framebuffer, background, VGA_W * VGA_H);
        if (fd2_field_prepare_draw_roster(page, ui->assets, &ui->context->preparation, selected, first, 0) != 0) { result = -1; break; }
        result = bottom_slide(ui, background, page->framebuffer, 1);
        if (result != 0) break;
        for (;;) {
            fd2_input_event event;
            ui->roster_first = first; ui->roster_selected = selected;
            result = read_key(ui, &event, 2, 1);
            ui->roster_selected = -1;
            if (result != 0) goto done;
            if (cancel_key(event.key)) { result = 0; goto close; }
            if (confirm_key(event.key)) break;
            int old = selected;
            if (roster_step(event.key, ui->context->preparation.unit_count, &selected, &first) != 0) { result = -1; goto done; }
            if (old != selected && sound(ui, 0) != 0) { result = -1; goto done; }
            memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
            if (fd2_field_prepare_draw_roster(ui->vga, ui->assets, &ui->context->preparation,
                    selected, first, 0) != 0) { result = -1; goto done; }
            memcpy(page->framebuffer, ui->vga->framebuffer, VGA_W * VGA_H);
            if (present(ui)) { result = 1; goto done; }
        }
        result = bottom_slide(ui, background, page->framebuffer, 0);
        if (result != 0) break;
        result = member_detail(ui, (size_t)selected);
        if (result != 0) break;
    }
    goto done;
close:
    result = bottom_slide(ui, background, page->framebuffer, 0);
done:
    fd2_vga_wait_frame_deadline(ui->vga);
    free(page); free(background);
    return result;
}

static int hotel_menu_animation(prepare_ui *ui, int opening) {
    uint8_t *background = malloc(VGA_W * VGA_H);
    if (!background) return -1;
    memcpy(background, ui->vga->framebuffer, VGA_W * VGA_H);
    for (int row = 169; row < 189; row++) memset(background + row * VGA_STRIDE + 201, 74, 104);
    static const int offsets[4] = {-39, -13, 13, 39};
    int result = 0;
    for (int j = 0; j < 4; j++) {
        memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
        for (int i = 0; i < 4; i++) fd2_map_sprite_blit(ui->vga,
            &ui->assets->hotel_icons[i * 2], 240 + offsets[i] / (opening ? 4 - j : j + 1), 169, 0);
        if (present(ui)) { result = 1; break; }
    }
    if (!opening && !result) {
        memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
        result = present(ui);
    }
    free(background);
    return result;
}

static int hotel_run(prepare_ui *ui) {
    if (music(ui, 13, "hotel") != 0) return -1;
    if (fd2_field_prepare_draw_hotel(ui->vga, ui->assets, ui->fdother, -1, 0, 0, 585) != 0) return -1;
    int selected = 0, result = hotel_menu_animation(ui, 1);
    if (result != 0) return result;
    size_t fragment = 585;
    drain(ui);
    for (;;) {
        if (fd2_field_prepare_draw_hotel(ui->vga, ui->assets, ui->fdother,
                selected, 1, 0, fragment) != 0) return -1;
        if (present(ui)) return 1;
        ui->menu_selected = selected;
        fd2_input_event event;
        result = read_key(ui, &event, 2, 1);
        ui->menu_selected = -1;
        if (result != 0) return result;
        if (event.key == FD2_INPUT_KEY_LEFT || event.key == FD2_INPUT_KEY_RIGHT) {
            selected = (selected + (event.key == FD2_INPUT_KEY_LEFT ? 3 : 1)) % 4;
            if (sound(ui, 0) != 0) return -1;
            continue;
        }
        if (!cancel_key(event.key) && !confirm_key(event.key)) continue;
        result = hotel_menu_animation(ui, 0);
        if (result != 0) return result;
        if (cancel_key(event.key)) return 0;
        if (selected == 0) result = roster_run(ui);
        else if (selected == 1 || selected == 2) result = slots_run(ui, selected == 1, PREPARE_SLOT_HOTEL);
        else {
            result = choice(ui, 0);
            if (result == 0) {
                if (text_draw(ui->vga, ui->assets, 416, 12, 138, 205, 74, ui->context->message_number) != 0) return -1;
                uint64_t until = SDL_GetTicks() + 200u;
                while (SDL_GetTicks() < until) if (present(ui)) return 1;
                return 3;
            }
            if (result == 2) result = 0;
        }
        if (result == 1 || result < 0) return result;
        fragment = 586;
        if (fd2_field_prepare_draw_hotel(ui->vga, ui->assets, ui->fdother, -1, 0, 0, fragment) != 0) return -1;
        result = hotel_menu_animation(ui, 1);
        if (result != 0) return result;
    }
}

#include "field_prepare_service_present.h"
#include "field_prepare_service_pick.h"
#include "field_prepare_service_flow.h"

/* 279bc/29daa/2af28只消费prepare领域有限图；每次真实输入重开损坏图预算。
 * UI资源全属本次调用，roster/资金/数字/RNG始终沿现App owner。 */
static int prepare_service_execute(prepare_ui *ui,const char *flow_id,
        const char *entry,int selector) {
    const fd2_static_flow *flow=fd2_static_flow_lookup("preparation",flow_id);
    int pc=flow && entry ? fd2_static_flow_label(flow,entry):flow ? flow->entry:-1;
    if(!flow || pc<0)return -1;
    memset(&ui->service,0,sizeof(ui->service));
    prepare_service *s=&ui->service;
    s->selector=selector;s->member=-1;s->receiver=-1;s->running=1;
    s->new_fragment=SIZE_MAX;s->old_fragment=SIZE_MAX;
    unsigned steps=0;uint64_t epoch=0;int result=-1;
    while(steps++<FD2_FLOW_STEP_BUDGET) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if(!node)break;
        if(quit(ui)){s->host_quit=1;break;}
        unsigned port=FD2_FLOW_NEXT;
        int handled=prepare_service_domain(ui,node,&port);
        if(!handled)handled=prepare_service_pick(ui,node,&port);
        if(!handled)handled=prepare_service_ui(ui,node,&port);
        if(handled<0) {
            if(!s->host_quit)fprintf(stderr,"准备流程失败：%s/%s op=%s port=%u committed=%d\n",
                flow_id,node->id,node->op,port,s->irreversible);
            break;
        }
        if(!handled) {
            fprintf(stderr,"未知准备操作：%s/%s/%s\n",flow_id,node->id,node->op);break;
        }
        if(s->done){result=s->result;break;}
        if(s->input_epoch!=epoch){epoch=s->input_epoch;steps=0;}
        pc=fd2_static_flow_edge(flow,node,port);
        if(pc<0){fprintf(stderr,"准备流程缺少出口：%s/%s port=%u\n",flow_id,node->id,port);break;}
    }
    if(s->host_quit)result=-2;
    prepare_service_cleanup(ui);
    s->running=0;
    return result;
}

int fd2_field_prepare_stage_supported(size_t stage) { return prepare_stage_valid(stage); }

fd2_field_prepare_result fd2_field_prepare_run(fd2_vga *vga, const fd2_archive *fdother,
        fd2_field_play_context *context, const char *save_path, fd2_field_audio *audio) {
    if (!vga || !fdother || !context || !context->initialized || !save_path ||
        !context->has_preparation ||
        !prepare_stage_valid(context->preparation.stage_id) ||
        context->preparation.unit_count > FD2_SAVE_MAX_UNITS) return FD2_FIELD_PREPARE_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_FIELD_PREPARE_HOST_QUIT;
    if (fd2_field_game_validate_manual_state(&context->preparation) != 0)
        return FD2_FIELD_PREPARE_ERROR;
    fd2_field_prepare_assets *assets = calloc(1, sizeof(*assets));
    if (!assets) return FD2_FIELD_PREPARE_ERROR;
    if (fd2_field_prepare_assets_open(assets, fdother) != 0) { free(assets); return FD2_FIELD_PREPARE_ERROR; }
    if (prepare_town(assets, fdother, context->preparation.stage_id) != 0 ||
        fd2_field_prepare_assets_bind_roster(assets,&context->preparation)!=0) {
        fd2_field_prepare_assets_close(assets); free(assets); return FD2_FIELD_PREPARE_ERROR;
    }
    prepare_ui ui = {vga, fdother, assets, context, audio, save_path, -1, 0, -1};
    fd2_field_prepare_result result = FD2_FIELD_PREPARE_ERROR;
    if(prepare_is_notown(context->preparation.stage_id)) {
        int subresult=prepare_service_execute(&ui,"deployment","notown.mode",2);
        result=subresult==1 ? FD2_FIELD_PREPARE_DEPART:subresult==-2 ?
            FD2_FIELD_PREPARE_HOST_QUIT:FD2_FIELD_PREPARE_ERROR;
        goto done;
    }
    if (music(&ui, 10, "town") != 0) goto done;
    int selected = 0,preview_track=0; unsigned phase = 0; uint64_t last = SDL_GetTicks();
    drain(&ui);
    for (;;) {
        if (fd2_field_prepare_draw_town(vga, assets, &context->preparation, selected, phase) != 0) break;
        if (present(&ui)) { result = FD2_FIELD_PREPARE_HOST_QUIT; break; }
        if (SDL_GetTicks() - last >= 220u) { phase = (phase + 1u) & 3u; last = SDL_GetTicks(); }
        fd2_input_event event;
        if (!fd2_input_take_key(&vga->input, &event)) continue;
        drain(&ui);
        if (event.key == FD2_INPUT_KEY_RIGHT || event.key == FD2_INPUT_KEY_LEFT) {
            selected = event.key==FD2_INPUT_KEY_RIGHT ? (selected<=0 ? 4:selected-1):
                (selected>=4 ? 0:selected+1);
            if (sound(&ui, 0) != 0) break;
        } else {
            if(event.key==FD2_INPUT_KEY_G) {
                preview_track=(preview_track+1)%10;
                if(music(&ui,preview_track,"town-preview")!=0)break;
            } else {
                const uint8_t *record=fd2_field_stage_prepare_record_get(context->preparation.stage_id);
                if(record && record[1]==selected && record[2]==fd2_input_function_scan(&event))selected=5;
            }
            if(!confirm_key(event.key))continue;
            int subresult;
            if (selected == 0) {
                if(sound_loop(&ui,1,3)!=0)break;
                if (music(&ui,-1,"town-enter")!=0)break;
                subresult = hotel_run(&ui);
                if (subresult == 3) { result = FD2_FIELD_PREPARE_LEAVE_GAME; break; }
                if(subresult==1)subresult=-2;
            } else if(selected==2) {
                if (music(&ui,-1,"town-enter")!=0)break;
                subresult=prepare_service_execute(&ui,"deployment",NULL,2);
                if(subresult==1){result=FD2_FIELD_PREPARE_DEPART;break;}
            } else {
                subresult=prepare_service_execute(&ui,"services",NULL,selected);
            }
            if (subresult == -2) { result = FD2_FIELD_PREPARE_HOST_QUIT; break; }
            if (subresult < 0) break;
            /* services的退出冷图已请求10/0，不能在这里重启同一曲。 */
            if ((selected==0 || selected==2) && music(&ui, 10, "town-return") != 0) break;
        }
    }
done:
    fd2_vga_wait_frame_deadline(vga);
    fd2_field_prepare_assets_close(assets); free(assets);
    return result;
}

fd2_field_prepare_result fd2_field_prepare_load_title(fd2_vga *vga,
        const fd2_archive *fdother, fd2_field_play_context *context,
        const char *save_path, fd2_field_audio *audio) {
    if (!vga || !fdother || !context || !context->initialized || !save_path) return FD2_FIELD_PREPARE_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_FIELD_PREPARE_HOST_QUIT;
    const uint8_t *palette; size_t palette_size;
    if (fd2_archive_get(fdother, 0, &palette, &palette_size) != 0 || palette_size < VGA_PALETTE_SIZE)
        return FD2_FIELD_PREPARE_ERROR;
    for (size_t i = 0; i < VGA_PALETTE_SIZE; i++) if (palette[i] > 63u) return FD2_FIELD_PREPARE_ERROR;
    fd2_field_prepare_assets *assets = calloc(1, sizeof(*assets));
    if (!assets) return FD2_FIELD_PREPARE_ERROR;
    if (fd2_field_prepare_assets_open(assets, fdother) != 0) { free(assets); return FD2_FIELD_PREPARE_ERROR; }
    prepare_ui ui = {vga, fdother, assets, context, audio, save_path, -1, 0, -1};
    /* title_action_dispatch @0x25ebb（原登记同名）：LOAD先fade-out，
     * 再换FDOTHER0、清VGA0并恢复normal DAC；不能借标题底图/标题palette。
     * 容器与完整四行页面均先在局部预检，失败不动live VGA/准备owner。 */
    fd2_save_file checked_save = {0};
    fd2_vga *checked_page = new_frame(NULL);
    int ready = checked_page && fd2_save_file_open(&checked_save, save_path) == 0 &&
        fd2_field_prepare_draw_slots(checked_page, assets, &checked_save, 0) == 0;
    free(checked_page); fd2_save_file_close(&checked_save);
    if (!ready) { fd2_field_prepare_assets_close(assets); free(assets); return FD2_FIELD_PREPARE_ERROR; }
    for (int brightness = 0; brightness < 64; brightness++) {
        if (quit(&ui)) { fd2_field_prepare_assets_close(assets); free(assets); return FD2_FIELD_PREPARE_HOST_QUIT; }
        fd2_vga_set_brightness(vga, brightness);
        fd2_vga_present_timed(vga, 2); /* 沿宿主既有title fade节奏，不声称BIOS时刻一致。 */
        if (quit(&ui)) { fd2_field_prepare_assets_close(assets); free(assets); return FD2_FIELD_PREPARE_HOST_QUIT; }
    }
    fd2_vga_set_palette(vga, palette);
    fd2_vga_clear(vga, 0);
    fd2_vga_set_brightness(vga, 0);
    int result = slots_run(&ui, 0, PREPARE_SLOT_TITLE);
    fd2_field_prepare_assets_close(assets); free(assets);
    return result == 2 ? FD2_FIELD_PREPARE_DEPART : result == 1 ? FD2_FIELD_PREPARE_HOST_QUIT :
        result == 0 ? FD2_FIELD_PREPARE_TITLE : FD2_FIELD_PREPARE_ERROR;
}
