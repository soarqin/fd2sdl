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
#endif
