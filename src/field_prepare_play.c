/* 城镇／酒店准备界面：field_town_prepare_execute @0x26152（原sub_26152）、
 * field_town_view_render @0x265ec（原sub_265EC）、field_town_selection_execute
 * @0x2670e（原sub_2670E）、field_hotel_menu_execute @0x29300（原field_manual_slot_dispatch）、
 * field_manual_slot_save @0x2968d（原sub_2968D）、field_manual_slot_load @0x2986f（原登记同名）、
 * field_manual_slot_rows_draw @0x29ab2（原sub_29AB2）与field_manual_slot_picker @0x29bcb（原登记同名）。
 * 只借App准备owner；四槽绝不恢复为active。原版资源和布局来自已发布
 * canonical伪C及独立Domain/raw读集，不声称完整原版时序/RNG等价。 */
#include "field_prepare_play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"
#include "field_magic.h"
#include "portrait.h"
#include "scene.h"

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

void fd2_field_prepare_assets_close(fd2_field_prepare_assets *a) {
    if (!a) return;
    fd2_image_free(&a->town); fd2_image_free(&a->town_panel);
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

/* 15f84：此准备页读集仅普通glyph/-2/-6/-1；数字是FONT[0..9]，
 * 不是ui_number_draw的位图bank。额外控制码必须另有证据，不能跳过。 */
static int text_draw(fd2_vga *vga, const fd2_field_prepare_assets *a, size_t fragment,
        int x, int y, uint8_t fg, int bg, uint32_t number) {
    const uint8_t *tokens; size_t count; int origin = x;
    if (!a->font.glyphs || a->font.glyph_count > a->font.size / 32u ||
        fd2_text_entry_get_fragment(&a->text, fragment, &tokens, &count) != 0) return -1;
    for (size_t i = 0; i < count; i++) {
        int16_t token = (int16_t)fd2_text_token_at(tokens, i);
        if (token == -1) return 0;
        if (token == -2) { y += 19; x = origin; continue; }
        if (token == -6) {
            char digits[12]; int n = snprintf(digits, sizeof(digits), "%d", (int32_t)number);
            if (n < 0 || n >= (int)sizeof(digits)) return -1;
            for (int j = 0; j < n; j++) {
                if (digits[j] < '0' || digits[j] > '9' || x < 0 || x + 16 > VGA_W || y < 0 || y + 17 > VGA_H)
                    return -1;
                fd2_font_draw_glyph(vga, &a->font, (uint16_t)(digits[j] - '0'), x, y, fg, 76, bg); x += 16;
            }
        } else {
            if (token < 0 || (size_t)token >= a->font.glyph_count || x < 0 || x + 16 > VGA_W || y < 0 || y + 17 > VGA_H)
                return -1;
            fd2_font_draw_glyph(vga, &a->font, (uint16_t)token, x, y, fg, 76, bg); x += 16;
        }
    }
    return count == 0 ? 0 : -1;
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
    static const uint8_t x[5] = {29, 41, 59, 154, 182};
    static const uint8_t y[5] = {46, 109, 163, 139, 65};
    if (!vga || !a || !a->ready || !state || state->stage_id != 1 ||
        state->unit_count > FD2_SAVE_MAX_UNITS || selected < 0 || selected > 4 || phase > 3 ||
        !image_size(&a->town, 320, 200) || !image_size(&a->town_panel, 62, 26)) return -1;
    if (state->unit_count && (size_t)state->unit_area[7] >= a->sprites.frame_count / 12u) return -1;
    fd2_vga *frame = new_frame(NULL); if (!frame) return -1;
    for (size_t row = 0; row < 192; row++)
        memcpy(frame->framebuffer + (row + 4u) * VGA_STRIDE + 4,
               a->town.pixels + row * VGA_STRIDE, 312);
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
        if ((size_t)unit[7] >= a->sprites.frame_count / 12u) { result = -1; break; }
        int col = (int)(i % 2u) * 132, row = (int)(i / 2u) * 26;
        fd2_map_sprite_blit_frame(frame, &a->sprites, (size_t)unit[7] * 12u + (phase == 3 ? 1u : phase),
            14 + col, 117 + row, 0);
        result = text_draw(frame, a, (size_t)unit[8] + 1u, 40 + col, 121 + row,
            i + first == (size_t)selected ? 201 : 205, -1, 0);
        if (result != 0) break;
    }
    return finish_frame(vga, frame, result);
}

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
} prepare_ui;

static int quit(prepare_ui *ui) { return fd2_input_take_quit(&ui->vga->input); }
static void drain(prepare_ui *ui) {
    while (fd2_input_take_key(&ui->vga->input, NULL)) {}
}
static int sound(prepare_ui *ui, size_t sample) {
    if (ui->context->has_preparation && !ui->context->preparation.options[3]) return 0;
    if (!ui->audio || !ui->audio->player) return 0;
    if (!ui->audio->ui_bank) return -1;
    return fd2_pcm_play_replace(ui->audio->player, ui->audio->ui_bank, sample, 1, 1.0f);
}
static int music(prepare_ui *ui, int track, const char *reason) {
    if (ui->audio && ui->audio->player && ui->audio->player->audio && ui->context->has_preparation) {
        if (fd2_audio_set_bus_gain(ui->audio->player->audio, FD2_AUDIO_BUS_MUSIC,
                ui->context->preparation.options[2] ? 1.0f : 0.0f) != 0 ||
            fd2_audio_set_bus_gain(ui->audio->player->audio, FD2_AUDIO_BUS_SFX,
                ui->context->preparation.options[3] ? 1.0f : 0.0f) != 0) return -1;
    }
    return fd2_field_play_request_music(ui->context, 1, track, reason);
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

/* 返回0退出picker，2成功Load，1宿主退出，-1错；Save成功继续同一slot。 */
static int slots_run(prepare_ui *ui, int save_mode, int title_mode) {
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
            result = read_key(ui, &event, !title_mode, !title_mode);
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
        result = bottom_slide(ui, background, page->framebuffer, 0);
        if (result != 0) break;
        if (quit(ui)) { result = 1; break; }
        if (save_mode) {
            if (!notice_ready(ui, 660)) { result = -1; break; }
            if (fd2_save_manual_state_save(ui->save_path, (size_t)selected,
                    &ui->context->preparation) != 0) { result = -1; break; }
            fd2_save_file_close(&save);
            if (fd2_save_file_open(&save, ui->save_path) != 0) { result = -1; break; }
            printf("prepare save: slot=%d stage=%u roster=%u\n", selected,
                ui->context->preparation.stage_id, ui->context->preparation.unit_count); fflush(stdout);
            result = notice(ui, 660, 1, 0);
            if (result != 0) break;
            memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
        } else {
            fd2_save_manual_state loaded;
            if (fd2_save_manual_state_load(ui->save_path, (size_t)selected, &loaded) != 0) {
                /* 空槽是正常未提交，格式错误由严格后端拒绝而不改owner。 */
                if (fd2_save_file_slot_valid(&save, (size_t)selected)) { result = -1; break; }
                memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
                continue;
            }
            if (fd2_field_game_validate_manual_state(&loaded) != 0) {
                if (title_mode) { result = -1; break; }
                result = notice(ui, 479, 1, 0);
                if (result != 0) break;
                memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
                continue;
            }
            if (!title_mode && !notice_ready(ui, 478)) { result = -1; break; }
            /* owner只在整场资源/数据预检成功且宿主未退出时交接。 */
            if (quit(ui)) { result = 1; break; }
            ui->context->preparation = loaded;
            ui->context->has_preparation = 1;
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

static int member_detail(prepare_ui *ui, size_t index) {
    if (index >= ui->context->preparation.unit_count) return -1;
    fd2_field_unit unit;
    memcpy(&unit, ui->context->preparation.unit_area + index * sizeof(unit), sizeof(unit));
    uint8_t *background = malloc(VGA_W * VGA_H);
    fd2_vga *page = new_frame(ui->vga);
    fd2_image portrait = {0};
    if (!background || !page) { free(background); free(page); return -1; }
    memcpy(background, ui->vga->framebuffer, VGA_W * VGA_H);
    fd2_field_game borrowed = {0}; /* 只借纯绘制资源；禁止close此结构。 */
    borrowed.ready = 1;
    borrowed.info_assets = ui->assets->info; borrowed.font = ui->assets->font;
    borrowed.ui_text = ui->assets->text;
    int result = -1;
    if (!fd2_field_game_detail_upper_is_ready(&borrowed, &unit) ||
        fd2_portrait_decode_frame(&portrait, &ui->assets->dato, unit.unit_id, 0) != 0 ||
        !image_size(&portrait, 80, 80) ||
        fd2_field_item_list_draw(page, &ui->assets->info, &ui->assets->font,
            &ui->assets->text, &unit, -1) != 0) goto done;
    size_t magic_count = fd2_field_unit_magic_list_build(&unit, NULL, 0);
    if (magic_count && fd2_field_magic_list_draw(page, &ui->assets->info,
        &ui->assets->font, &ui->assets->text, &unit, -1) != 0) goto done;
    fd2_field_detail_draw(page, &ui->assets->info, &ui->assets->font,
        &ui->assets->text, &unit, &portrait);
    if (sound(ui, 5) != 0) goto done;
    drain(ui);
    for (int phase = 11; phase >= 0; phase--) {
        fd2_field_detail_transition_frame(ui->vga->framebuffer, page->framebuffer, background, phase);
        if (present(ui)) { result = 1; goto done; }
    }
    fd2_input_event event;
    result = read_key(ui, &event, 1, 0);
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
        result = read_key(ui, &event, 1, 0);
        if (result != 0) goto done;
    }
    if (sound(ui, 6) != 0) { result = -1; goto done; }
    for (int phase = 0; phase < 12; phase++) {
        fd2_field_detail_transition_frame(ui->vga->framebuffer, page->framebuffer, background, phase);
        if (present(ui)) { result = 1; goto done; }
    }
    memcpy(ui->vga->framebuffer, background, VGA_W * VGA_H);
    result = present(ui);
done:
    fd2_vga_wait_frame_deadline(ui->vga);
    free(page); free(background); fd2_image_free(&portrait);
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
        else if (selected == 1 || selected == 2) result = slots_run(ui, selected == 1, 0);
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

fd2_field_prepare_result fd2_field_prepare_run(fd2_vga *vga, const fd2_archive *fdother,
        fd2_field_play_context *context, const char *save_path, fd2_field_audio *audio) {
    if (!vga || !fdother || !context || !context->initialized || !save_path ||
        !context->has_preparation || context->preparation.stage_id != 1 ||
        context->preparation.unit_count > FD2_SAVE_MAX_UNITS) return FD2_FIELD_PREPARE_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_FIELD_PREPARE_HOST_QUIT;
    fd2_field_prepare_assets *assets = calloc(1, sizeof(*assets));
    if (!assets) return FD2_FIELD_PREPARE_ERROR;
    if (fd2_field_prepare_assets_open(assets, fdother) != 0) { free(assets); return FD2_FIELD_PREPARE_ERROR; }
    prepare_ui ui = {vga, fdother, assets, context, audio, save_path, -1, 0, -1};
    fd2_field_prepare_result result = FD2_FIELD_PREPARE_ERROR;
    if (music(&ui, 10, "town") != 0) goto done;
    int selected = 0; unsigned phase = 0; uint64_t last = SDL_GetTicks();
    drain(&ui);
    for (;;) {
        if (fd2_field_prepare_draw_town(vga, assets, &context->preparation, selected, phase) != 0) break;
        if (present(&ui)) { result = FD2_FIELD_PREPARE_HOST_QUIT; break; }
        if (SDL_GetTicks() - last >= 220u) { phase = (phase + 1u) & 3u; last = SDL_GetTicks(); }
        fd2_input_event event;
        if (!fd2_input_take_key(&vga->input, &event)) continue;
        drain(&ui);
        if (event.key == FD2_INPUT_KEY_RIGHT || event.key == FD2_INPUT_KEY_LEFT) {
            selected = (selected + (event.key == FD2_INPUT_KEY_RIGHT ? 4 : 1)) % 5;
            if (sound(&ui, 0) != 0) break;
        } else if (confirm_key(event.key)) {
            if (selected != 0 && selected != 2) {
                fprintf(stderr, "当前准备页尚未开放此商店：selector=%d，保留城镇选择。\n", selected);
                continue;
            }
            if (music(&ui, -1, "town-enter") != 0) break;
            int subresult;
            if (selected == 0) {
                subresult = hotel_run(&ui);
                if (subresult == 3) { result = FD2_FIELD_PREPARE_LEAVE_GAME; break; }
            } else {
                subresult = choice(&ui, 1);
                if (subresult == 0) {
                    if (!context->preparation.unit_count || context->preparation.unit_count > 16u) {
                        fprintf(stderr, "当前出阵尚不支持此队伍人数：%u；保留完整持久表。\n", context->preparation.unit_count);
                    } else { result = FD2_FIELD_PREPARE_DEPART; break; }
                }
                if (subresult == 2) subresult = 0;
            }
            if (subresult == 1) { result = FD2_FIELD_PREPARE_HOST_QUIT; break; }
            if (subresult < 0) break;
            if (music(&ui, 10, "town-return") != 0) break;
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
    int result = slots_run(&ui, 0, 1);
    fd2_field_prepare_assets_close(assets); free(assets);
    return result == 2 ? FD2_FIELD_PREPARE_DEPART : result == 1 ? FD2_FIELD_PREPARE_HOST_QUIT :
        result == 0 ? FD2_FIELD_PREPARE_TITLE : FD2_FIELD_PREPARE_ERROR;
}
