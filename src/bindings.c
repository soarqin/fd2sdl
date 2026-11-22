/* 设置文件和引导窗口共用物理源到 BIOS 虚拟键的映射，运行时按键生命周期归 input。 */
#include "bindings.h"
#include <string.h>

#define KEY(name) {#name, SDL_SCANCODE_##name}
static const fd2_virtual_key targets[] = {
    {"Up", SDL_SCANCODE_UP}, {"Down", SDL_SCANCODE_DOWN},
    {"Left", SDL_SCANCODE_LEFT}, {"Right", SDL_SCANCODE_RIGHT},
    {"Enter", SDL_SCANCODE_RETURN}, {"Space", SDL_SCANCODE_SPACE},
    {"Esc", SDL_SCANCODE_ESCAPE}, {"Delete", SDL_SCANCODE_DELETE},
    {"Insert", SDL_SCANCODE_INSERT}, {"Home", SDL_SCANCODE_HOME},
    {"PageUp", SDL_SCANCODE_PAGEUP}, {"Keypad5", SDL_SCANCODE_KP_5},
    KEY(G), KEY(Z), KEY(F1), KEY(F2), KEY(F3), KEY(F4), KEY(F5), KEY(F6),
    KEY(F7), KEY(F8), KEY(F9), KEY(F10), KEY(F11), KEY(F12)
};
#undef KEY
static fd2_binding keyboard[SDL_SCANCODE_COUNT];
static const SDL_Keymod mod_bits[] = {SDL_KMOD_LSHIFT, SDL_KMOD_LCTRL, SDL_KMOD_LALT};
static const char *const mod_names[] = {"Shift+", "Ctrl+", "Alt+"};

const fd2_virtual_key *fd2_virtual_keys(size_t *count) {
    *count = SDL_arraysize(targets);
    return targets;
}
fd2_binding fd2_binding_parse(const char *name) {
    SDL_Keymod mods = 0;
    while (strchr(name, '+')) {
        size_t i = 0;
        for (; i < SDL_arraysize(mod_bits); ++i) {
            size_t n = strlen(mod_names[i]);
            if (!SDL_strncasecmp(name, mod_names[i], n) && !(mods & mod_bits[i])) {
                mods |= mod_bits[i]; name += n; break;
            }
        }
        if (i == SDL_arraysize(mod_bits)) return (fd2_binding){0};
    }
    for (size_t i = 0; i < SDL_arraysize(targets); ++i)
        if (!SDL_strcasecmp(name, targets[i].name)) return (fd2_binding){targets[i].key, mods};
    return (fd2_binding){0};
}
void fd2_binding_name(fd2_binding binding, char text[64]) {
    text[0] = 0;
    for (size_t i = 0; i < SDL_arraysize(mod_bits); ++i)
        if (binding.mods & mod_bits[i]) SDL_strlcat(text, mod_names[i], 64);
    for (size_t i = 0; i < SDL_arraysize(targets); ++i)
        if (targets[i].key == binding.key) { SDL_strlcat(text, targets[i].name, 64); return; }
}

static bool bindable(SDL_Scancode s) {
    return s > SDL_SCANCODE_UNKNOWN && s < SDL_SCANCODE_COUNT &&
        (s < SDL_SCANCODE_LCTRL || s > SDL_SCANCODE_RGUI) &&
        s != SDL_SCANCODE_CAPSLOCK && s != SDL_SCANCODE_NUMLOCKCLEAR &&
        s != SDL_SCANCODE_SCROLLLOCK && s != SDL_SCANCODE_PAUSE && *SDL_GetScancodeName(s);
}
fd2_binding fd2_keyboard_binding(SDL_Scancode source) {
    return keyboard[source].key ? keyboard[source] : (fd2_binding){source, 0};
}
bool fd2_keyboard_bind(SDL_Scancode source, fd2_binding target) {
    if (!bindable(source)) return false;
    keyboard[source] = target;
    return true;
}
void fd2_keyboard_defaults(void) {
    memset(keyboard, 0, sizeof(keyboard));
}
static bool same(fd2_binding a, fd2_binding b) {
    return a.key == b.key && a.mods == b.mods;
}

void fd2_keyboard_read(fd2_binding target, char *sources) {
    bool selected[SDL_SCANCODE_COUNT] = {false};
    char *at = sources;
    bool valid = true;
    while (*at) {
        char *next = strchr(at, ',');
        if (next) *next = 0;
        at += strspn(at, " \t");
        size_t n = strlen(at);
        while (n && (at[n-1] == ' ' || at[n-1] == '\t')) at[--n] = 0;
        SDL_Scancode key = !SDL_strcasecmp(at, "Comma") ? SDL_SCANCODE_COMMA :
                           SDL_GetScancodeFromName(at);
        if (!bindable(key)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Invalid keyboard source '%s'; using passthrough", at);
            valid = false;
            break;
        }
        selected[key] = true;
        if (!next) break;
        at = next + 1;
    }
    for (int i = 1; i < SDL_SCANCODE_COUNT; ++i) {
        if (valid && selected[i]) keyboard[i] = target;
        else if (same(keyboard[i], target)) keyboard[i] = (fd2_binding){0};
    }
}

bool fd2_keyboard_list(fd2_binding target, char *buffer, size_t size) {
    size_t used = 0;
    buffer[0] = 0;
    for (int i = 1; i < SDL_SCANCODE_COUNT; ++i) {
        if (!same(fd2_keyboard_binding((SDL_Scancode)i), target)) continue;
        const char *name = i == SDL_SCANCODE_COMMA ? "Comma" : SDL_GetScancodeName((SDL_Scancode)i);
        int written = SDL_snprintf(buffer + used, size - used, "%s%s", used ? ", " : "", name);
        if (written < 0 || (size_t)written >= size - used) return false;
        used += (size_t)written;
    }
    return true;
}
bool fd2_keyboard_write(SDL_IOStream *out) {
    char names[8192], target_name[64];
    for (unsigned bits = 0; bits < 8; ++bits) {
        SDL_Keymod mods = 0;
        for (unsigned b = 0; b < SDL_arraysize(mod_bits); ++b)
            if (bits & (1u << b)) mods |= mod_bits[b];
        for (size_t i = 0; i < SDL_arraysize(targets); ++i) {
            fd2_binding target = {targets[i].key, mods};
            if (!fd2_keyboard_list(target, names, sizeof(names))) return false;
            if (mods && !*names) continue;
            fd2_binding_name(target, target_name);
            if (!SDL_IOprintf(out, "%s = %s\n", target_name, names)) return false;
        }
    }
    return true;
}

static const SDL_Scancode pad_defaults[FD2_PAD_SOURCES] = {
    [SDL_GAMEPAD_BUTTON_DPAD_UP] = SDL_SCANCODE_UP,
    [SDL_GAMEPAD_BUTTON_DPAD_DOWN] = SDL_SCANCODE_DOWN,
    [SDL_GAMEPAD_BUTTON_DPAD_LEFT] = SDL_SCANCODE_LEFT,
    [SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = SDL_SCANCODE_RIGHT,
    [SDL_GAMEPAD_BUTTON_SOUTH] = SDL_SCANCODE_RETURN,
    [SDL_GAMEPAD_BUTTON_EAST] = SDL_SCANCODE_ESCAPE,
    [SDL_GAMEPAD_BUTTON_WEST] = SDL_SCANCODE_F2,
    [SDL_GAMEPAD_BUTTON_NORTH] = SDL_SCANCODE_F1,
    [SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = SDL_SCANCODE_Z,
    [SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = SDL_SCANCODE_Z,
    [SDL_GAMEPAD_BUTTON_BACK] = SDL_SCANCODE_G,
    [SDL_GAMEPAD_BUTTON_START] = SDL_SCANCODE_SPACE,
    [FD2_STICK_UP] = SDL_SCANCODE_UP, [FD2_STICK_DOWN] = SDL_SCANCODE_DOWN,
    [FD2_STICK_LEFT] = SDL_SCANCODE_LEFT, [FD2_STICK_RIGHT] = SDL_SCANCODE_RIGHT,
};
static fd2_binding pad[FD2_PAD_SOURCES];

void fd2_pad_defaults(void) {
    for (int i = 0; i < FD2_PAD_SOURCES; ++i) pad[i] = (fd2_binding){pad_defaults[i], 0};
}
fd2_binding fd2_pad_binding(int source) { return pad[source]; }
void fd2_pad_bind(int source, fd2_binding target) { pad[source] = target; }
const char *fd2_pad_name(int source) {
    static const char *const stick[] = {"stick_up", "stick_down", "stick_left", "stick_right"};
    return source < FD2_STICK_UP ? SDL_GetGamepadStringForButton((SDL_GamepadButton)source)
                                : stick[source - FD2_STICK_UP];
}
int fd2_pad_source(const char *name) {
    for (int i = 0; i < FD2_PAD_SOURCES; ++i)
        if (!SDL_strcasecmp(fd2_pad_name(i), name)) return i;
    return -1;
}
void fd2_pad_read(int source, const char *value) {
    fd2_binding target = fd2_binding_parse(value);
    if (!target.key && *value && SDL_strcasecmp(value, "none")) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Invalid gamepad binding %s = %s", fd2_pad_name(source), value);
        target.key = pad_defaults[source];
    }
    fd2_pad_bind(source, target);
}
bool fd2_pad_write(SDL_IOStream *out) {
    for (int i = 0; i < FD2_PAD_SOURCES; ++i) {
        char value[64];
        fd2_binding_name(pad[i], value);
        if (!SDL_IOprintf(out, "%s = %s\n", fd2_pad_name(i), *value ? value : "none")) return false;
    }
    return true;
}

/* 四向只保留一个主轴。退出死区及切轴各有 8% 回差，中心总是释放。 */
int fd2_stick_direction(int x, int y, int previous, int deadzone) {
    int level = 32767 * deadzone / 100;
    const int margin = 32767 * 8 / 100;
    int ax = SDL_abs(x), ay = SDL_abs(y);
    int horizontal = x < 0 ? FD2_STICK_LEFT : FD2_STICK_RIGHT;
    int vertical = y < 0 ? FD2_STICK_UP : FD2_STICK_DOWN;
    int release = SDL_max(1, level - margin);
    if (previous == horizontal && ax >= release && ay < ax + margin) return horizontal;
    if (previous == vertical && ay >= release && ax < ay + margin) return vertical;
    if (SDL_max(ax, ay) < SDL_max(1, level)) return -1;
    return ax >= ay ? horizontal : vertical;
}
SDL_Gamepad *fd2_first_gamepad(SDL_JoystickID excluded) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    SDL_Gamepad *pad_handle = NULL;
    for (int i = 0; ids && i < count && !pad_handle; ++i)
        if (ids[i] != excluded) pad_handle = SDL_OpenGamepad(ids[i]);
    SDL_free(ids);
    return pad_handle;
}
