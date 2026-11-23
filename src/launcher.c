/* 独立的 SDL 启动窗口。关闭后才建立游戏 VGA、BIOS 时钟和音频 owner。 */
#include "launcher.h"
#include "bindings.h"
#include "input.h"
#include "setup_font.h"
#include "version.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { WIDTH = 800, HEIGHT = 540, ROW_Y = 125, ROW_H = 40, VISIBLE = 6 };
enum page { PATHS, VIDEO, SOUND, KEYBOARD, GAMEPAD, PACING, PAGE_COUNT };
enum focus { NAV, ROWS, FOOT };
enum kind { SETTING, PREVIEW, MODIFIERS, BIND, RESET_BINDINGS };
enum action { START, SAVE, RESET, EXIT, ACTION_COUNT };
typedef struct { enum kind kind; const fd2_setting *setting; const fd2_virtual_key *key; } row;
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *font;
    SDL_Gamepad *pad;
    SDL_AudioStream *preview;
    const fd2_config_file *config;
    enum page page;
    enum focus focus;
    enum action action;
    row rows[64];
    int count, index, top, chord, result;
    bool traditional, done, capture, editing, selected_all;
    char edit[FD2_PATH_SIZE], status[256], missing[128];
    size_t caret;
    int available, stick, x, y, pad_char;
} setup;

static const char *const pages[][2] = {
    {"路径", "路徑"}, {"画面", "畫面"}, {"声音", "聲音"},
    {"键盘", "鍵盤"}, {"手柄", "手把"}, {"节奏与规则", "節奏與規則"}
};
static const char *const buttons[][2] = {
    {"启动游戏", "啟動遊戲"}, {"仅保存", "僅儲存"}, {"恢复默认", "恢復預設"}, {"退出", "離開"}
};
static const char *const window_titles[] = {
    "FD2-SDL " FD2_VERSION " 启动设置",
    "FD2-SDL " FD2_VERSION " 啟動設定"
};
static const char pad_chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/\\._- ";
static const SDL_Color canvas = {24, 31, 37, 255}, panel = {35, 44, 51, 255},
    rule = {74, 88, 92, 255}, highlight = {49, 66, 70, 255}, ink = {236, 231, 211, 255}, muted = {174, 187, 180, 255},
    gold = {238, 195, 118, 255}, mint = {128, 199, 169, 255}, alert = {246, 169, 128, 255};

static bool system_uses_simplified_chinese(void) {
    SDL_Locale **locales = SDL_GetPreferredLocales(NULL);
    bool simplified = false;
    if (locales && locales[0]->language) {
        const SDL_Locale *locale = locales[0];
        const char *variant = NULL;
        if (!SDL_strcasecmp(locale->language, "zh")) variant = locale->country;
        else if (!SDL_strncasecmp(locale->language, "zh-", 3)) variant = locale->language + 3;
        if (variant) {
            size_t len = strcspn(variant, "-");
            simplified = (len == 4 && !SDL_strncasecmp(variant, "Hans", 4)) ||
                (len == 2 && (!SDL_strncasecmp(variant, "CN", 2) ||
                              !SDL_strncasecmp(variant, "SG", 2) ||
                              !SDL_strncasecmp(variant, "MY", 2)));
        }
    }
    SDL_free(locales);
    return simplified;
}
static const char *tr(const setup *ui, const char *simplified, const char *traditional) {
    return ui->traditional ? traditional : simplified;
}
static void toggle_language(setup *ui) {
    ui->traditional = !ui->traditional;
    SDL_SetWindowTitle(ui->window, window_titles[ui->traditional]);
    ui->status[0] = 0;
}
static void message(setup *ui, const char *simplified, const char *traditional) {
    SDL_strlcpy(ui->status, tr(ui, simplified, traditional), sizeof(ui->status));
}
static void fill(setup *ui, int x, int y, int w, int h, SDL_Color color) {
    SDL_FRect r = {(float)x, (float)y, (float)w, (float)h};
    SDL_SetRenderDrawColor(ui->renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(ui->renderer, &r);
}
static void text(setup *ui, int x, int y, int width, SDL_Color color, const char *s) {
    fd2_setup_text(ui->renderer, ui->font, x, y, width, 1, color, s);
}

static void inspect_assets(setup *ui, const char *directory) {
    ui->available = 0;
    ui->missing[0] = 0;
    for (int i = 0; i < FD2_ASSET_COUNT; ++i) {
        char path[FD2_PATH_SIZE + 32];
        SDL_snprintf(path, sizeof(path), "%s/%s", directory, fd2_asset_names[i]);
        SDL_PathInfo info;
        if (*directory && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE && info.size)
            ++ui->available;
        else if (!*ui->missing) SDL_strlcpy(ui->missing, fd2_asset_names[i], sizeof(ui->missing));
    }
}
static void visibility(setup *ui) {
    ui->index = SDL_clamp(ui->index, 0, ui->count - 1);
    ui->top = SDL_clamp(ui->top, ui->index - VISIBLE + 1, ui->index);
    ui->top = SDL_clamp(ui->top, 0, SDL_max(0, ui->count - VISIBLE));
}
static void open_page(setup *ui, enum page page) {
    ui->page = page;
    ui->index = ui->top = ui->count = 0;
    ui->status[0] = 0;
    static const char *const sections[] = {"paths", "video", "audio", "keyboard", "input", "pacing"};
    size_t count;
    const fd2_setting *settings = fd2_settings_table(&count);
    for (size_t i = 0; i < count; ++i) {
        if (!strcmp(settings[i].section, sections[page]) ||
            (page == PATHS && !strcmp(settings[i].section, "launcher")) ||
            (page == PACING && !strcmp(settings[i].section, "rules")))
            ui->rows[ui->count++] = (row){SETTING, &settings[i], NULL};
    }
    if (page == SOUND) ui->rows[ui->count++] = (row){PREVIEW};
    if (page == KEYBOARD || page == GAMEPAD) {
        ui->rows[ui->count++] = (row){RESET_BINDINGS};
        ui->rows[ui->count++] = (row){MODIFIERS};
        const fd2_virtual_key *keys = fd2_virtual_keys(&count);
        for (size_t i = 0; i < count; ++i) ui->rows[ui->count++] = (row){BIND, NULL, &keys[i]};
    }
    if (page == PATHS) inspect_assets(ui, fd2_get_settings()->game_dir);
}
static fd2_binding row_binding(const setup *ui, const row *r) {
    SDL_Keymod mods = 0;
    if (ui->chord & 1) mods |= SDL_KMOD_LSHIFT;
    if (ui->chord & 2) mods |= SDL_KMOD_LCTRL;
    if (ui->chord & 4) mods |= SDL_KMOD_LALT;
    return (fd2_binding){r->key->key, mods};
}
static void pad_list(fd2_binding target, char *value, size_t size) {
    value[0] = 0;
    for (int i = 0; i < FD2_PAD_SOURCES; ++i) {
        fd2_binding b = fd2_pad_binding(i);
        if (b.key == target.key && b.mods == target.mods) {
            if (*value) SDL_strlcat(value, ", ", size);
            SDL_strlcat(value, fd2_pad_name(i), size);
        }
    }
}
static const char *choice_label(const setup *ui, const char *value) {
    static const char *const labels[][3] = {
        {"off", "关闭", "關閉"}, {"on", "开启", "開啟"}, {"desktop", "桌面全屏", "桌面全螢幕"},
        {"integer", "整数倍", "整數倍"}, {"letterbox", "等比留边", "等比留邊"}, {"stretch", "拉伸", "拉伸"},
        {"nearest", "最近邻", "最鄰近"}, {"linear", "线性", "線性"}, {"normal", "正常", "正常"},
        {"fast", "快速", "快速"}, {"instant", "即时", "即時"}, {"full", "完整", "完整"}, {"short", "简短", "簡短"},
        {"random", "原版随机", "原版隨機"}, {"max", "最大成长+1", "最大成長+1"},
        {"max_minus_one", "最大成长", "最大成長"}, {"min", "最小成长", "最小成長"}
    };
    for (size_t i = 0; i < SDL_arraysize(labels); ++i)
        if (!strcmp(labels[i][0], value)) return tr(ui, labels[i][1], labels[i][2]);
    return value;
}
static bool device_row(const row *r) {
    return r->kind == SETTING && !strcmp(r->setting->key, "device");
}
static bool adjustable(const row *r) {
    return r->kind == MODIFIERS || (r->kind == SETTING && (!r->setting->capacity || device_row(r)));
}
static void row_text(setup *ui, const row *r, char label[96], char *value, size_t size) {
    value[0] = 0;
    switch (r->kind) {
    case SETTING: {
        const fd2_setting *s = r->setting;
        SDL_strlcpy(label, tr(ui, s->label_simplified, s->label_traditional), 96);
        char number[32];
        const char *v = fd2_setting_text(s, number);
        SDL_strlcpy(value, *v ? (s->choices ? choice_label(ui, v) : v) :
                    tr(ui, "系统默认", "系統預設"), size);
        break;
    }
    case PREVIEW: SDL_strlcpy(label, tr(ui, "试听", "試聽"), 96); break;
    case RESET_BINDINGS:
        SDL_strlcpy(label, tr(ui, "恢复本页绑定", "恢復本頁綁定"), 96);
        break;
    case MODIFIERS: {
        static const char *const mods[] = {"-", "Shift", "Ctrl", "Shift+Ctrl", "Alt", "Shift+Alt",
                                           "Ctrl+Alt", "Shift+Ctrl+Alt"};
        SDL_strlcpy(label, tr(ui, "虚拟键修饰", "虛擬鍵修飾"), 96);
        SDL_strlcpy(value, mods[ui->chord], size);
        break;
    }
    case BIND: {
        fd2_binding target = row_binding(ui, r);
        fd2_binding_name(target, label);
        if (ui->page == KEYBOARD) fd2_keyboard_list(target, value, size);
        else pad_list(target, value, size);
        if (!*value) SDL_strlcpy(value, tr(ui, "未绑定", "未綁定"), size);
        break;
    }
    }
}
static const char *edit_tail(const setup *ui, int *caret_x) {
    const char *end = ui->edit + ui->caret, *at = end;
    int width = 0;
    while (at > ui->edit) {
        const char *prev = at;
        Uint32 cp = SDL_StepBackUTF8(ui->edit, &prev);
        int next = width + fd2_setup_glyph_width(cp);
        if (next > 226) break;
        width = next;
        at = prev;
    }
    *caret_x = width;
    return at;
}
static void draw(setup *ui) {
    fill(ui, 0, 0, WIDTH, HEIGHT, canvas);
    const char *heading = tr(ui, "启动设置", "啟動設定");
    fd2_setup_text(ui->renderer, ui->font, 22, 20, 176, 2, ink, heading);
    text(ui, 202, 27, 200, muted, "FD2-SDL  /  v" FD2_VERSION);
    bool original = fd2_settings_original();
    text(ui, 465, 27, 194, original ? mint : alert, original ?
         tr(ui, "原版一致", "原版一致") : tr(ui, "非原版设置", "非原版設定"));
    fill(ui, 685, 17, 94, 35, panel);
    text(ui, 700, 26, 64, ink, tr(ui, "繁體中文", "简体中文"));
    fill(ui, 20, 70, 760, 1, rule);
    for (int i = 0; i < PAGE_COUNT; ++i) {
        int y = 88 + i * 43;
        if ((int)ui->page == i) {
            fill(ui, 20, y, 150, 36, highlight);
            fill(ui, 20, y, 3, 36, ui->focus == NAV ? gold : mint);
        }
        text(ui, 37, y + 10, 128, (int)ui->page == i ? ink : muted, pages[i][ui->traditional]);
    }
    fill(ui, 185, 88, 1, 356, panel);
    text(ui, 206, 93, 330, gold, pages[ui->page][ui->traditional]);
    if (ui->count > VISIBLE) {
        char range[64];
        SDL_snprintf(range, sizeof(range), "%d-%d / %d", ui->top + 1,
                     SDL_min(ui->count, ui->top + VISIBLE), ui->count);
        text(ui, 675, 93, 100, muted, range);
    }
    for (int i = ui->top; i < ui->count && i < ui->top + VISIBLE; ++i) {
        int y = ROW_Y + (i - ui->top) * ROW_H;
        row *r = &ui->rows[i];
        bool focus = ui->focus == ROWS && ui->index == i;
        fill(ui, 202, y, 577, 36, focus ? highlight : panel);
        if (focus) fill(ui, 202, y, 3, 36, gold);
        char label[96], value[1024];
        row_text(ui, r, label, value, sizeof(value));
        text(ui, 214, y + 10, 238, focus ? gold : ink, label);
        if (focus && ui->editing) {
            int caret_x;
            const char *value_start = edit_tail(ui, &caret_x);
            text(ui, 464, y + 10, 302, ui->selected_all ? gold : ink, value_start);
            fill(ui, 464 + caret_x, y + 9, 1, 18, gold);
        } else if (focus && ui->capture) {
            text(ui, 464, y + 10, 300, gold, tr(ui, "等待输入……", "等待輸入……"));
        } else {
            text(ui, 464, y + 10, adjustable(r) ? 249 : 307, muted, value);
            if (adjustable(r)) {
                text(ui, 721, y + 10, 16, gold, "-");
                text(ui, 755, y + 10, 16, gold, "+");
            }
        }
    }
    char detail[240] = "";
    if (ui->page == PATHS) {
        if (ui->available == FD2_ASSET_COUNT)
            SDL_snprintf(detail, sizeof(detail), tr(ui, "资源齐全  %d/%d", "資源齊全  %d/%d"),
                         ui->available, FD2_ASSET_COUNT);
        else SDL_snprintf(detail, sizeof(detail), tr(ui, "已找到 %d/%d  缺少 %s", "已找到 %d/%d  缺少 %s"),
                          ui->available, FD2_ASSET_COUNT, ui->missing);
    } else if (ui->page == GAMEPAD) {
        const char *name = ui->pad ? SDL_GetGamepadName(ui->pad) : NULL;
        SDL_snprintf(detail, sizeof(detail), "%s  X %d  Y %d", name ? name :
                     tr(ui, "未连接手柄", "未連接手把"), ui->x, ui->y);
        fill(ui, 733, 369, 40, 40, panel);
        fill(ui, 750 + ui->x / 2250, 386 + ui->y / 2250, 5, 5, mint);
    } else if (ui->page == PACING) SDL_strlcpy(detail,
        tr(ui, "原版随机关闭成长补丁。固定档位适用于升级。",
               "原版隨機關閉成長補丁。固定檔位適用於升級。"), sizeof(detail));
    else if (ui->page == KEYBOARD) SDL_strlcpy(detail,
        tr(ui, "选择虚拟键后按下物理键。Delete 清除该行绑定。",
               "選擇虛擬鍵後按下實體鍵。Delete 清除該列綁定。"), sizeof(detail));
    text(ui, 206, 380, ui->page == GAMEPAD ? 510 : 568, mint, detail);
    text(ui, 206, 415, 568, gold, ui->status);
    fill(ui, 20, 449, 760, 1, rule);
    text(ui, 22, 460, 757, muted, ui->editing ?
        tr(ui, "Enter 确定   Esc 取消   Ctrl+V 粘贴", "Enter 確定   Esc 取消   Ctrl+V 貼上") :
        tr(ui, "Tab 切换区域   方向键选择或调整   Enter 确定   PgUp/PgDn 切页",
               "Tab 切換區域   方向鍵選擇或調整   Enter 確定   PgUp/PgDn 換頁"));
    for (int i = 0; i < ACTION_COUNT; ++i) {
        int x = 20 + i * 192;
        bool focus = ui->focus == FOOT && (int)ui->action == i;
        fill(ui, x, 489, 184, 36, i == START ? mint : focus ? highlight : panel);
        if (focus) fill(ui, x, 522, 184, 3, gold);
        text(ui, x + 14, 499, 162, i == START ? canvas : ink, buttons[i][ui->traditional]);
    }
    SDL_RenderPresent(ui->renderer);
}

static void stop_preview(setup *ui) {
    if (ui->preview) SDL_DestroyAudioStream(ui->preview);
    ui->preview = NULL;
}
static void preview_sound(setup *ui) {
    stop_preview(ui);
    const fd2_settings *s = fd2_get_settings();
    const char *quiet = SDL_getenv("FD2SDL_TEST_AUDIO_QUIET");
    if (s->mute || !s->volume || (quiet && !strcmp(quiet, "1"))) {
        message(ui, "当前输出已静音", "目前輸出已靜音");
        return;
    }
    SDL_AudioSpec spec = {.freq = 48000, .format = SDL_AUDIO_F32, .channels = 1};
    ui->preview = SDL_OpenAudioDeviceStream(fd2_output_device(s->device), &spec, NULL, NULL);
    float tone[9600];
    for (size_t i = 0; i < SDL_arraysize(tone); ++i) {
        float t = (float)i / (float)SDL_arraysize(tone);
        tone[i] = 0.15f * sinf(6.2831853f * 440.0f * (float)i / 48000.0f) *
                  SDL_min(t * 40.0f, 1.0f) * (1.0f - t);
    }
    if (!ui->preview || !SDL_SetAudioStreamGain(ui->preview, (float)s->volume / 100.0f) ||
        !SDL_PutAudioStreamData(ui->preview, tone, sizeof(tone)) ||
        !SDL_FlushAudioStream(ui->preview) || !SDL_ResumeAudioStreamDevice(ui->preview)) {
        stop_preview(ui);
        message(ui, "无法试听，请检查输出设备", "無法試聽，請檢查輸出裝置");
    } else message(ui, "已播放试听音", "已播放試聽音");
}
static void cycle_device(const fd2_setting *setting, int step) {
    int count = 0;
    SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&count);
    int selected = 0;
    for (int i = 0; devices && i < count; ++i) {
        const char *name = SDL_GetAudioDeviceName(devices[i]);
        if (name && !strcmp(name, setting->value)) selected = i + 1;
    }
    selected = (selected + step + count + 1) % (count + 1);
    const char *name = selected ? SDL_GetAudioDeviceName(devices[selected - 1]) : "";
    if (name) fd2_setting_set(setting, name);
    SDL_free(devices);
}
static void adjust(setup *ui, int direction) {
    const row *r = &ui->rows[ui->index];
    ui->status[0] = 0;
    if (r->kind == MODIFIERS) ui->chord = (ui->chord + direction + 8) % 8;
    else if (device_row(r)) { stop_preview(ui); cycle_device(r->setting, direction); }
    else if (r->kind == SETTING && !r->setting->capacity) {
        const fd2_setting *s = r->setting;
        int value = *(int *)s->value;
        if (s->choices) value = (value + direction + s->max + 1) % (s->max + 1);
        else value = SDL_clamp(value + direction, s->min, s->max);
        *(int *)s->value = value;
        if (ui->page == SOUND) stop_preview(ui);
    }
}

static void check_edit_path(setup *ui) {
    if (!strcmp(ui->rows[ui->index].setting->key, "game_dir")) inspect_assets(ui, ui->edit);
}
static void insert_text(setup *ui, const char *value) {
    size_t n = strlen(value), len = ui->selected_all ? 0 : strlen(ui->edit);
    const fd2_setting *s = ui->rows[ui->index].setting;
    size_t capacity = s->capacity ? s->capacity : 32;
    if (strpbrk(value, "\r\n") || n >= capacity - len) {
        message(ui, "内容过长或含换行", "內容過長或含換行");
        return;
    }
    if (ui->selected_all) { ui->edit[0] = 0; ui->caret = 0; ui->selected_all = false; }
    memmove(ui->edit + ui->caret + n, ui->edit + ui->caret, len - ui->caret + 1);
    memcpy(ui->edit + ui->caret, value, n);
    ui->caret += n;
    check_edit_path(ui);
}
static void erase_text(setup *ui, bool back) {
    if (ui->selected_all) { insert_text(ui, ""); return; }
    const char *a = ui->edit + ui->caret, *b = a;
    if (back && ui->caret) SDL_StepBackUTF8(ui->edit, &a);
    else if (!back && *b) SDL_StepUTF8(&b, NULL);
    memmove(ui->edit + (a - ui->edit), b, strlen(b) + 1);
    ui->caret = (size_t)(a - ui->edit);
    check_edit_path(ui);
}
static void end_edit(setup *ui, bool accept) {
    const fd2_setting *s = ui->rows[ui->index].setting;
    if (accept) {
        char old[FD2_PATH_SIZE], number[32];
        SDL_strlcpy(old, fd2_setting_text(s, number), sizeof(old));
        if (!fd2_setting_set(s, ui->edit)) {
            fd2_setting_set(s, old);
            message(ui, "值无效，请重新输入", "數值無效，請重新輸入");
            return;
        }
    }
    SDL_StopTextInput(ui->window);
    ui->editing = false;
    ui->status[0] = 0;
    if (ui->page == PATHS) inspect_assets(ui, fd2_get_settings()->game_dir);
}
static void edit_key(setup *ui, SDL_Scancode key, SDL_Keymod mods) {
    if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER) end_edit(ui, true);
    else if (key == SDL_SCANCODE_ESCAPE) end_edit(ui, false);
    else if ((mods & SDL_KMOD_CTRL) && key == SDL_SCANCODE_A) ui->selected_all = true;
    else if ((mods & SDL_KMOD_CTRL) && key == SDL_SCANCODE_V) {
        char *clip = SDL_GetClipboardText();
        if (clip) insert_text(ui, clip);
        SDL_free(clip);
    } else if (key == SDL_SCANCODE_BACKSPACE || key == SDL_SCANCODE_DELETE) {
        erase_text(ui, key == SDL_SCANCODE_BACKSPACE);
    } else {
        const char *at = ui->edit + ui->caret;
        if (key == SDL_SCANCODE_HOME) at = ui->edit;
        else if (key == SDL_SCANCODE_END) at = ui->edit + strlen(ui->edit);
        else if (key == SDL_SCANCODE_LEFT && at != ui->edit) SDL_StepBackUTF8(ui->edit, &at);
        else if (key == SDL_SCANCODE_RIGHT && *at) SDL_StepUTF8(&at, NULL);
        else return;
        ui->selected_all = false;
        ui->caret = (size_t)(at - ui->edit);
    }
}
static void activate_row(setup *ui) {
    const row *r = &ui->rows[ui->index];
    ui->status[0] = 0;
    if (r->kind == PREVIEW) preview_sound(ui);
    else if (r->kind == RESET_BINDINGS) {
        if (ui->page == KEYBOARD) fd2_keyboard_defaults(); else fd2_pad_defaults();
        message(ui, "已恢复本页绑定", "已恢復本頁綁定");
    } else if (r->kind == BIND) {
        ui->capture = true;
        message(ui, "按键或推动摇杆录入，Esc 取消", "按鍵或推動搖桿設定，Esc 取消");
    } else if (r->kind == MODIFIERS || r->setting->choices || device_row(r)) adjust(ui, 1);
    else {
        char number[32];
        SDL_strlcpy(ui->edit, fd2_setting_text(r->setting, number), sizeof(ui->edit));
        ui->caret = strlen(ui->edit);
        ui->selected_all = true;
        ui->editing = SDL_StartTextInput(ui->window);
        if (!ui->editing) message(ui, "无法启动文字输入", "無法啟動文字輸入");
    }
}
static void clear_binding(setup *ui) {
    row *r = &ui->rows[ui->index];
    if (r->kind != BIND) return;
    fd2_binding target = row_binding(ui, r);
    if (ui->page == KEYBOARD) {
        char empty[] = "";
        fd2_keyboard_read(target, empty);
    } else {
        for (int i = 0; i < FD2_PAD_SOURCES; ++i) {
            fd2_binding b = fd2_pad_binding(i);
            if (b.key == target.key && b.mods == target.mods) fd2_pad_bind(i, (fd2_binding){0});
        }
    }
    message(ui, "已清除自定义绑定", "已清除自訂綁定");
}
static void perform(setup *ui, enum action action) {
    if (action == EXIT) { ui->done = true; return; }
    if (action == RESET) {
        fd2_settings_defaults();
        stop_preview(ui);
        inspect_assets(ui, fd2_get_settings()->game_dir);
        ui->chord = 0;
        message(ui, "已恢复默认值", "已恢復預設值");
        return;
    }
    if (!ui->config->path) {
        if (action == SAVE) {
            message(ui, "无配置模式不可保存", "未載入設定檔，無法儲存");
            return;
        }
        fd2_settings_defaults();
    }
    if (action == START) {
        inspect_assets(ui, fd2_get_settings()->game_dir);
        if (ui->available != FD2_ASSET_COUNT) {
            open_page(ui, PATHS);
            ui->focus = ROWS;
            message(ui, "请检查游戏数据目录", "請檢查遊戲資料目錄");
            return;
        }
    }
    if (ui->config->path && !fd2_config_write(ui->config)) {
        SDL_Log("Cannot save %s: %s", ui->config->path, SDL_GetError());
        message(ui, "保存失败，请检查配置文件路径和权限", "儲存失敗，請檢查設定檔路徑和權限");
        return;
    }
    ui->done = true;
    ui->result = action == START;
}
static void command(setup *ui, SDL_Scancode key, SDL_Keymod mods) {
    if (ui->editing) { edit_key(ui, key, mods); return; }
    if (ui->capture) {
        if (key == SDL_SCANCODE_ESCAPE) { ui->capture = false; ui->status[0] = 0; }
        else if (ui->page == KEYBOARD) {
            if (fd2_keyboard_bind(key, row_binding(ui, &ui->rows[ui->index]))) {
                ui->capture = false;
                message(ui, "已录入键位", "已設定按鍵");
            } else message(ui, "请选择普通键", "請選擇一般按鍵");
        }
        return;
    }
    if (key == SDL_SCANCODE_F2) { toggle_language(ui); return; }
    if (key == SDL_SCANCODE_ESCAPE) { perform(ui, EXIT); return; }
    if (key == SDL_SCANCODE_TAB) {
        ui->focus = (enum focus)((ui->focus + ((mods & SDL_KMOD_SHIFT) ? 2 : 1)) % 3);
        return;
    }
    if (key == SDL_SCANCODE_PAGEUP || key == SDL_SCANCODE_PAGEDOWN) {
        open_page(ui, (enum page)((ui->page + (key == SDL_SCANCODE_PAGEUP ? -1 : 1) + PAGE_COUNT) % PAGE_COUNT));
        return;
    }
    if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER || key == SDL_SCANCODE_SPACE) {
        if (ui->focus == NAV) ui->focus = ROWS;
        else if (ui->focus == ROWS) activate_row(ui);
        else perform(ui, ui->action);
        return;
    }
    int d = key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_UP ? -1 : 1;
    if (key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN) {
        if (ui->focus == NAV) open_page(ui, (enum page)((ui->page + d + PAGE_COUNT) % PAGE_COUNT));
        else if (ui->focus == ROWS) { ui->index += d; visibility(ui); }
        else ui->focus = ROWS;
    } else if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT) {
        if (ui->focus == ROWS) adjust(ui, d);
        else if (ui->focus == FOOT) ui->action = (enum action)((ui->action + d + ACTION_COUNT) % ACTION_COUNT);
        else ui->focus = ROWS;
    } else if (ui->focus == ROWS && (key == SDL_SCANCODE_HOME || key == SDL_SCANCODE_END)) {
        ui->index = key == SDL_SCANCODE_HOME ? 0 : ui->count - 1;
        visibility(ui);
    } else if (ui->focus == ROWS && key == SDL_SCANCODE_DELETE) clear_binding(ui);
}

static void click(setup *ui, int x, int y) {
    if (ui->editing) {
        end_edit(ui, true);
        if (ui->editing) return;
    }
    ui->capture = false;
    ui->status[0] = 0;
    if (x >= 685 && x < 779 && y >= 17 && y < 52) toggle_language(ui);
    else if (x >= 20 && x < 170 && y >= 88 && y < 88 + PAGE_COUNT * 43) {
        if ((y - 88) % 43 < 36) {
            open_page(ui, (enum page)((y - 88) / 43));
            ui->focus = NAV;
        }
    } else if (x >= 202 && x < 779 && y >= ROW_Y && y < ROW_Y + VISIBLE * ROW_H) {
        int i = ui->top + (y - ROW_Y) / ROW_H;
        if (i >= ui->count || (y - ROW_Y) % ROW_H >= 36) return;
        ui->focus = ROWS;
        ui->index = i;
        if (adjustable(&ui->rows[i]) && x >= 710) adjust(ui, x < 742 ? -1 : 1);
        else activate_row(ui);
    } else if (x >= 20 && x < 780 && y >= 489 && y < 525 && (x - 20) % 192 < 184) {
        ui->focus = FOOT;
        ui->action = (enum action)((x - 20) / 192);
        perform(ui, ui->action);
    }
}
static void pad_command(setup *ui, int source) {
    if (ui->capture && ui->page == GAMEPAD) {
        fd2_pad_bind(source, row_binding(ui, &ui->rows[ui->index]));
        ui->capture = false;
        message(ui, "已录入手柄绑定", "已設定手把綁定");
        return;
    }
    if (ui->editing) {
        if (source == SDL_GAMEPAD_BUTTON_SOUTH) {
            char c[2] = {pad_chars[ui->pad_char], 0};
            insert_text(ui, c);
        } else if (source == SDL_GAMEPAD_BUTTON_EAST) end_edit(ui, false);
        else if (source == SDL_GAMEPAD_BUTTON_NORTH) end_edit(ui, true);
        else if (source == SDL_GAMEPAD_BUTTON_WEST) erase_text(ui, true);
        else {
            int step = source == SDL_GAMEPAD_BUTTON_DPAD_LEFT || source == FD2_STICK_LEFT ? -1 :
                       source == SDL_GAMEPAD_BUTTON_DPAD_RIGHT || source == FD2_STICK_RIGHT ? 1 :
                       source == SDL_GAMEPAD_BUTTON_DPAD_UP || source == FD2_STICK_UP ? -8 :
                       source == SDL_GAMEPAD_BUTTON_DPAD_DOWN || source == FD2_STICK_DOWN ? 8 : 0;
            int n = (int)strlen(pad_chars);
            ui->pad_char = (ui->pad_char + n + step) % n;
        }
        if (ui->editing) SDL_snprintf(ui->status, sizeof(ui->status),
            tr(ui, "字符 [%c]    A 输入   X 删除   Y 确定   B 取消",
               "字元 [%c]    A 輸入   X 刪除   Y 確定   B 取消"), pad_chars[ui->pad_char]);
        return;
    }
    SDL_Scancode key;
    switch (source) {
    case SDL_GAMEPAD_BUTTON_SOUTH: key = SDL_SCANCODE_RETURN; break;
    case SDL_GAMEPAD_BUTTON_EAST: key = SDL_SCANCODE_ESCAPE; break;
    case SDL_GAMEPAD_BUTTON_BACK: case SDL_GAMEPAD_BUTTON_WEST: key = SDL_SCANCODE_TAB; break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: key = SDL_SCANCODE_PAGEUP; break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: key = SDL_SCANCODE_PAGEDOWN; break;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: case FD2_STICK_UP: key = SDL_SCANCODE_UP; break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: case FD2_STICK_DOWN: key = SDL_SCANCODE_DOWN; break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: case FD2_STICK_LEFT: key = SDL_SCANCODE_LEFT; break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: case FD2_STICK_RIGHT: key = SDL_SCANCODE_RIGHT; break;
    case SDL_GAMEPAD_BUTTON_START:
        if (!ui->capture) { ui->focus = FOOT; ui->action = START; }
        return;
    default: return;
    }
    command(ui, key, 0);
}
static bool handle_event(setup *ui, SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_QUIT: case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        ui->done = true; break;
    case SDL_EVENT_KEY_DOWN:
        if (!e->key.repeat || (!ui->capture && (ui->editing ||
            (e->key.scancode >= SDL_SCANCODE_RIGHT && e->key.scancode <= SDL_SCANCODE_UP)) ))
            command(ui, e->key.scancode, e->key.mod);
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (ui->editing) insert_text(ui, e->text.text);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.button == SDL_BUTTON_LEFT && SDL_ConvertEventToRenderCoordinates(ui->renderer, e))
            click(ui, (int)e->button.x, (int)e->button.y);
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (!ui->editing && !ui->capture && e->wheel.y) {
            ui->focus = ROWS;
            ui->index += e->wheel.y > 0 ? -1 : 1;
            visibility(ui);
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!ui->pad) ui->pad = fd2_first_gamepad(0);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (ui->pad && e->gdevice.which == SDL_GetGamepadID(ui->pad)) {
            SDL_CloseGamepad(ui->pad);
            ui->pad = fd2_first_gamepad(e->gdevice.which);
            ui->stick = -1; ui->x = ui->y = 0;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (ui->pad && e->gbutton.which == SDL_GetGamepadID(ui->pad)) pad_command(ui, e->gbutton.button);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        if (!ui->pad || e->gaxis.which != SDL_GetGamepadID(ui->pad)) return false;
        if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) ui->x = e->gaxis.value;
        else if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) ui->y = e->gaxis.value;
        else return false;
        int direction = fd2_stick_direction(ui->x, ui->y, ui->stick, fd2_get_settings()->gamepad_deadzone);
        if (direction != ui->stick && direction >= 0) pad_command(ui, direction);
        ui->stick = direction;
        break;
    }
    case SDL_EVENT_WINDOW_EXPOSED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: break;
    default: return false;
    }
    return true;
}

bool fd2_setup_requested(const fd2_config_file *config, bool development_mode) {
    return config->setup || (!development_mode && config->path &&
                             (!config->found || fd2_get_settings()->show_on_start));
}
static bool confirm_held(const setup *ui) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    return keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER] || keys[SDL_SCANCODE_SPACE] ||
           (ui->pad && SDL_GetGamepadButton(ui->pad, SDL_GAMEPAD_BUTTON_SOUTH));
}
int fd2_launcher_run(const fd2_config_file *config) {
    setup ui = {.config = config, .stick = -1};
    if (!SDL_Init(SDL_INIT_VIDEO)) return -1;
    ui.traditional = !system_uses_simplified_chinese();
    (void)SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    (void)SDL_InitSubSystem(SDL_INIT_AUDIO);
    ui.pad = fd2_first_gamepad(0);
    if (!SDL_CreateWindowAndRenderer(window_titles[ui.traditional], WIDTH, HEIGHT,
            SDL_WINDOW_RESIZABLE, &ui.window, &ui.renderer) ||
        !SDL_SetRenderLogicalPresentation(ui.renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX) ||
        !(ui.font = fd2_setup_font(ui.renderer))) {
        ui.result = -1;
        goto done;
    }
    SDL_SetWindowMinimumSize(ui.window, WIDTH / 2, HEIGHT / 2);
    open_page(&ui, PATHS);
    if (!config->path) message(&ui, "无配置模式，启动时使用默认值", "未載入設定檔，啟動時使用預設值");
    draw(&ui);
    while (!ui.done && !fd2_input_host_quit_requested()) {
        SDL_Event event;
        if (!SDL_WaitEventTimeout(&event, 100)) continue;
        bool redraw = handle_event(&ui, &event);
        while (!ui.done && SDL_PollEvent(&event)) redraw |= handle_event(&ui, &event);
        if (redraw && !ui.done) draw(&ui);
    }
    /* 保留引导窗口直至松开确认键，避免启动者的物理按键进入游戏队列。 */
    while (ui.result == 1 && confirm_held(&ui)) {
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 100) &&
            (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED))
            ui.result = 0;
        if (fd2_input_host_quit_requested()) ui.result = 0;
    }
done:
    if (ui.result < 0) SDL_Log("launcher: %s", SDL_GetError());
    stop_preview(&ui);
    if (ui.pad) SDL_CloseGamepad(ui.pad);
    SDL_DestroyTexture(ui.font);
    SDL_DestroyRenderer(ui.renderer);
    SDL_DestroyWindow(ui.window);
    SDL_Quit();
    return ui.result;
}
