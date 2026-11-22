#ifndef FD2_BINDINGS_H
#define FD2_BINDINGS_H
#include <SDL3/SDL.h>

typedef struct { SDL_Scancode key; SDL_Keymod mods; } fd2_binding;
typedef struct { const char *name; SDL_Scancode key; } fd2_virtual_key;
const fd2_virtual_key *fd2_virtual_keys(size_t *count);
fd2_binding fd2_binding_parse(const char *name);
void fd2_binding_name(fd2_binding binding, char text[64]);
fd2_binding fd2_keyboard_binding(SDL_Scancode source);
bool fd2_keyboard_bind(SDL_Scancode source, fd2_binding target);
void fd2_keyboard_defaults(void);
void fd2_keyboard_read(fd2_binding target, char *sources);
bool fd2_keyboard_list(fd2_binding target, char *buffer, size_t size);
bool fd2_keyboard_write(SDL_IOStream *out);
enum {
    FD2_STICK_UP = SDL_GAMEPAD_BUTTON_COUNT, FD2_STICK_DOWN, FD2_STICK_LEFT, FD2_STICK_RIGHT,
    FD2_PAD_SOURCES
};
const char *fd2_pad_name(int source);
int fd2_pad_source(const char *name);
fd2_binding fd2_pad_binding(int source);
void fd2_pad_bind(int source, fd2_binding target);
void fd2_pad_defaults(void);
void fd2_pad_read(int source, const char *value);
bool fd2_pad_write(SDL_IOStream *out);
int fd2_stick_direction(int x, int y, int previous, int deadzone);
SDL_Gamepad *fd2_first_gamepad(SDL_JoystickID excluded);
#endif
