#ifndef FD2_SETUP_FONT_H
#define FD2_SETUP_FONT_H
#include <SDL3/SDL.h>
SDL_Texture *fd2_setup_font(SDL_Renderer *renderer);
int fd2_setup_glyph_width(Uint32 code);
void fd2_setup_text(SDL_Renderer *renderer, SDL_Texture *atlas, int x, int y,
                    int width, int scale, SDL_Color color, const char *text);
#endif
