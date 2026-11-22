/* 宿主设置。等级、默认值、范围和界面共用同一张表；存档选项仍归游戏。 */
#include "settings.h"
#include "host_files.h"
#include "bindings.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static fd2_settings current;
static bool initialized;
#define INT(group, name, def, lo, hi, choices, grade, label) \
    {group, #name, label, grade, lo, hi, choices, def, &current.name, 0}
#define STR(group, name, def, label) \
    {group, #name, label, 'H', 0, 0, NULL, def, current.name, sizeof(current.name)}
static const fd2_setting items[] = {
    STR("paths", game_dir, "original_game", "游戏数据目录"),
    STR("paths", save_path, "FD2.SAV", "存档路径"),
    INT("launcher", show_on_start, "off", 0, 1, "off|on", 'H', "每次启动时显示设置"),
    INT("video", window_scale, "3", 1, 8, NULL, 'H', "窗口倍数"),
    INT("video", fullscreen, "off", 0, 1, "off|desktop", 'H', "桌面全屏"),
    INT("video", scale_mode, "integer", 0, 2, "integer|letterbox|stretch", 'H', "缩放模式"),
    INT("video", filter, "nearest", 0, 1, "nearest|linear", 'H', "像素滤镜"),
    INT("video", aspect_4_3, "off", 0, 1, "off|on", 'H', "校正到 4:3"),
    STR("video", renderer, "", "渲染驱动"),
    INT("audio", volume, "100", 0, 100, NULL, 'H', "主音量"),
    INT("audio", mute, "off", 0, 1, "off|on", 'H', "静音"),
    STR("audio", device, "", "输出设备"),
    INT("input", gamepad_deadzone, "50", 0, 99, NULL, 'H', "左摇杆死区 (%)"),
    INT("pacing", key_repeat_delay_ms, "500", 1, 5000, NULL, 'P', "首次连发等待 (ms)"),
    INT("pacing", key_repeat_rate_ms, "33", 1, 1000, NULL, 'P', "连发间隔 (ms)"),
    INT("pacing", text_speed, "normal", 0, 2, "normal|fast|instant", 'P', "逐字速度"),
    INT("pacing", animation_speed, "100", 25, 400, NULL, 'P', "动画速度 (%)"),
    INT("pacing", new_game_battle_effect, "full", 0, 1, "full|short", 'P', "新游戏战斗演出"),
    INT("pacing", new_game_info_board, "on", 0, 1, "off|on", 'P', "新游戏信息板"),
};
#undef INT
#undef STR

const fd2_setting *fd2_settings_table(size_t *count) {
    *count = SDL_arraysize(items);
    return items;
}

static bool assign(const fd2_setting *item, const char *value) {
    if (item->capacity) {
        if (strlen(value) >= item->capacity || strpbrk(value, "\r\n") ||
            (!*value && *item->initial)) return false;
        SDL_strlcpy(item->value, value, item->capacity);
    } else if (item->choices) {
        const char *option = item->choices;
        for (int i = 0; option; ++i) {
            size_t n = strcspn(option, "|");
            if (strlen(value) == n && !SDL_strncasecmp(value, option, n)) {
                *(int *)item->value = i;
                return true;
            }
            option = option[n] ? option + n + 1 : NULL;
        }
        return false;
    } else {
        char *end;
        long n = strtol(value, &end, 10);
        if (end == value || *end) return false;
        if (n < item->min || n > item->max)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] %s: %s outside %d..%d",
                        item->section, item->key, value, item->min, item->max);
        *(int *)item->value = (int)SDL_clamp(n, item->min, item->max);
    }
    return true;
}

bool fd2_setting_set(const fd2_setting *item, const char *value) {
    if (assign(item, value)) return true;
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] %s: invalid value, using '%s'",
                item->section, item->key, item->initial);
    assign(item, item->initial);
    return false;
}

const char *fd2_setting_text(const fd2_setting *item, char number[32]) {
    if (item->capacity) return item->value;
    int value = *(const int *)item->value;
    if (item->choices) {
        const char *at = item->choices;
        for (int i = 0; i < value; ++i) at = strchr(at, '|') + 1;
        snprintf(number, 32, "%.*s", (int)strcspn(at, "|"), at);
    } else snprintf(number, 32, "%d", value);
    return number;
}

void fd2_settings_defaults(void) {
    for (size_t i = 0; i < SDL_arraysize(items); ++i) assign(&items[i], items[i].initial);
    fd2_keyboard_defaults();
    fd2_pad_defaults();
    initialized = true;
}
const fd2_settings *fd2_get_settings(void) {
    if (!initialized) fd2_settings_defaults();
    return &current;
}
bool fd2_settings_original(void) {
    char number[32];
    for (size_t i = 0; i < SDL_arraysize(items); ++i)
        if (items[i].grade != 'H' && strcmp(fd2_setting_text(&items[i], number), items[i].initial))
            return false;
    return true;
}
SDL_AudioDeviceID fd2_output_device(const char *name) {
    SDL_AudioDeviceID id = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    if (!*name) return id;
    int count = 0;
    SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&count);
    for (int i = 0; devices && i < count; ++i) {
        const char *found = SDL_GetAudioDeviceName(devices[i]);
        if (found && !strcmp(found, name)) { id = devices[i]; break; }
    }
    SDL_free(devices);
    if (id == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "audio device '%s' unavailable; using default", name);
    return id;
}

const char *fd2_save_path(void) {
    const char *env = SDL_getenv("FD2SDL_SAVE_PATH");
    return env && *env ? env : fd2_get_settings()->save_path;
}

const char *const fd2_asset_names[FD2_ASSET_COUNT] = {
    "ANI.DAT", "BG.DAT", "DATO.DAT", "FDFIELD.DAT", "FDICON.B24", "FDMUS.DAT",
    "FDOTHER.DAT", "FDSHAP.DAT", "FDTXT.DAT", "FIGANI.DAT", "SAMPLE.AD", "TAI.DAT"
};
const char *fd2_asset_path(fd2_asset asset) {
    /* 同一调用可借用两个路径（例如音乐和音色库），各资源保留独立缓冲。 */
    static char paths[FD2_ASSET_COUNT][FD2_PATH_SIZE + 32];
    snprintf(paths[asset], sizeof(paths[asset]), "%s/%s", fd2_get_settings()->game_dir,
             fd2_asset_names[asset]);
    return paths[asset];
}

static char *trim(char *s) {
    s += strspn(s, " \t\r\n");
    size_t end = strlen(s);
    while (end && strchr(" \t\r\n", s[end - 1])) --end;
    s[end] = 0;
    return s;
}

static bool write_item(SDL_IOStream *out, const fd2_setting *item) {
    char number[32];
    return SDL_IOprintf(out, "%s = %s\n", item->key, fd2_setting_text(item, number)) > 0;
}

/* 同一词法用于读取和更新。未知行（包括注释和未知节）按原字节复制。 */
static bool process_text(const char *source, SDL_IOStream *out) {
    char section[64] = "";
    bool seen[SDL_arraysize(items)] = {false};
    const char *line = source ? source : "";
    if (!strncmp(line, "\xef\xbb\xbf", 3)) {
        if (out && SDL_WriteIO(out, line, 3) != 3) return false;
        line += 3;
    }
    bool wrote_keyboard = false, wrote_pad = false;
    while (*line) {
        size_t len = strcspn(line, "\n");
        size_t bytes = len + (line[len] == '\n');
        char *copy = SDL_strndup(line, len);
        if (!copy) return false;
        char *key = trim(copy);
        char *equal = strchr(key, '=');
        const fd2_setting *item = NULL;
        bool binding_line = false, keyboard_header = false, pad_header = false;
        if (*key == '[') {
            size_t n = strlen(key);
            section[0] = 0;
            if (n > 2 && key[n-1] == ']') {
                key[n-1] = 0;
                SDL_strlcpy(section, trim(key + 1), sizeof(section));
                pad_header = !SDL_strcasecmp(section, "gamepad");
                keyboard_header = !SDL_strcasecmp(section, "keyboard");
            }
        } else if (*key && *key != '#' && equal) {
            *equal = 0;
            key = trim(key);
            if (!SDL_strcasecmp(section, "keyboard")) {
                fd2_binding target = fd2_binding_parse(key);
                binding_line = target.key != SDL_SCANCODE_UNKNOWN;
                if (binding_line && !out) fd2_keyboard_read(target, trim(equal + 1));
            } else if (!SDL_strcasecmp(section, "gamepad")) {
                int source_index = fd2_pad_source(key);
                binding_line = source_index >= 0;
                if (binding_line && !out) fd2_pad_read(source_index, trim(equal + 1));
            }
            for (size_t i = 0; i < SDL_arraysize(items); ++i) {
                if (!SDL_strcasecmp(items[i].section, section) && !SDL_strcasecmp(items[i].key, key)) {
                    item = &items[i];
                    seen[i] = true;
                    if (!out) fd2_setting_set(item, trim(equal + 1));
                    break;
                }
            }
        }
        SDL_free(copy);
        if (out && !binding_line && !(item ? write_item(out, item) : SDL_WriteIO(out, line, bytes) == bytes))
            return false;
        if (out && keyboard_header && !wrote_keyboard) {
            if (line[bytes-1] != '\n' && !SDL_WriteU8(out, '\n')) return false;
            if (!fd2_keyboard_write(out)) return false;
            wrote_keyboard = true;
        }
        if (out && pad_header && !wrote_pad) {
            if (line[bytes-1] != '\n' && !SDL_WriteU8(out, '\n')) return false;
            if (!fd2_pad_write(out)) return false;
            wrote_pad = true;
        }
        line += bytes;
    }
    if (out) {
        if (!wrote_keyboard && (!SDL_IOprintf(out, "\n[keyboard]\n") || !fd2_keyboard_write(out)))
            return false;
        if (!wrote_pad && (!SDL_IOprintf(out, "\n[gamepad]\n") || !fd2_pad_write(out)))
            return false;
        const char *group = NULL;
        for (size_t i = 0; i < SDL_arraysize(items); ++i) {
            if (seen[i]) continue;
            if (!group || strcmp(group, items[i].section)) {
                group = items[i].section;
                if (!SDL_IOprintf(out, "\n[%s]\n", group)) return false;
            }
            if (!write_item(out, &items[i])) return false;
        }
    }
    return true;
}

static char *config_path(void) {
    const char *portable = "fd2sdl.cfg";
    if (SDL_GetPathInfo(portable, NULL)) return SDL_strdup(portable);
    char *pref = SDL_GetPrefPath("fd2sdl", "fd2sdl");
    char *path = NULL;
    if (pref) SDL_asprintf(&path, "%s%s", pref, portable);
    SDL_free(pref);
    return path;
}

bool fd2_config_open(fd2_config_file *file, int *argc, char **argv) {
    fd2_settings_defaults();
    *file = (fd2_config_file){0};
    const char *path = SDL_getenv("FD2SDL_CONFIG");
    bool disabled = path && !SDL_strcasecmp(path, "none");
    int keep = 1;
    for (int i = 1; i < *argc; ++i) {
        if (!strcmp(argv[i], "--config")) {
            if (++i >= *argc || !*argv[i] || !strncmp(argv[i], "--", 2))
                return SDL_SetError("--config requires a path");
            path = argv[i];
            disabled = false;
        } else if (!strcmp(argv[i], "--no-config")) disabled = true;
        else if (!strcmp(argv[i], "--setup")) file->setup = true;
        else argv[keep++] = argv[i];
    }
    argv[keep] = NULL;
    *argc = keep;
    if (disabled) return true;
    file->path = path && *path ? SDL_strdup(path) : config_path();
    if (!file->path) return false;
    FILE *f = fd2_open_read(file->path);
    if (!f && errno == ENOENT) return true;
    file->found = true;
    if (f && !fseek(f, 0, SEEK_END)) {
        long size = ftell(f);
        if (size >= 0 && size <= 4 * 1024 * 1024 && !fseek(f, 0, SEEK_SET)) {
            file->source = SDL_malloc((size_t)size + 1);
            if (file->source && fread(file->source, 1, size, f) == (size_t)size &&
                !memchr(file->source, 0, size)) {
                file->source[size] = 0;
                fclose(f);
                return process_text(file->source, NULL);
            }
        }
    }
    if (f) fclose(f);
    file->unreadable = true;
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot read configuration %s; using defaults", file->path);
    return true;
}

bool fd2_config_write(const fd2_config_file *file) {
    if (!file->path || file->unreadable) return SDL_SetError("configuration is read-only");
    SDL_IOStream *out = SDL_IOFromDynamicMem();
    if (!out) return false;
    bool ok = process_text(file->source, out);
    Sint64 size = SDL_GetIOSize(out);
    void *bytes = SDL_GetPointerProperty(SDL_GetIOProperties(out), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
    ok = ok && size >= 0 && bytes && fd2_replace_file(file->path, bytes, (size_t)size, current.game_dir) == 0;
    SDL_CloseIO(out);
    return ok;
}

void fd2_config_close(fd2_config_file *file) {
    SDL_free(file->path);
    SDL_free(file->source);
    *file = (fd2_config_file){0};
}
