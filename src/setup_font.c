/* 引导专用的 Unifont 15.1.05 子集。字形许可证见 third_party/LICENSE.unifont。 */
#include "setup_font.h"

typedef struct { Uint32 code; int width; Uint16 rows[16]; } glyph;
static const glyph glyphs[] = {
#include "setup_font.inc"
};

SDL_Texture *fd2_setup_font(SDL_Renderer *renderer) {
    int rows = (int)((SDL_arraysize(glyphs) + 15) / 16);
    SDL_Surface *s = SDL_CreateSurface(256, rows * 16, SDL_PIXELFORMAT_RGBA32);
    if (!s) return NULL;
    SDL_FillSurfaceRect(s, NULL, 0);
    Uint32 ink = SDL_MapSurfaceRGBA(s, 255, 255, 255, 255);
    for (size_t i = 0; i < SDL_arraysize(glyphs); ++i) {
        const glyph *g = &glyphs[i];
        int x0 = ((int)i % 16) * 16, y0 = ((int)i / 16) * 16;
        for (int y = 0; y < 16; ++y) {
            Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + (y0 + y) * s->pitch) + x0;
            for (int x = 0; x < g->width; ++x)
                if (g->rows[y] & (1u << (g->width - 1 - x))) row[x] = ink;
        }
    }
    SDL_Texture *atlas = SDL_CreateTextureFromSurface(renderer, s);
    SDL_DestroySurface(s);
    if (atlas && (!SDL_SetTextureScaleMode(atlas, SDL_SCALEMODE_NEAREST) ||
                  !SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND))) {
        SDL_DestroyTexture(atlas);
        return NULL;
    }
    return atlas;
}

static int glyph_index(Uint32 code) {
    int low = 0, high = (int)SDL_arraysize(glyphs);
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (glyphs[mid].code < code) low = mid + 1;
        else high = mid;
    }
    return low < (int)SDL_arraysize(glyphs) && glyphs[low].code == code ? low : '?' - 32;
}

void fd2_setup_text(SDL_Renderer *renderer, SDL_Texture *atlas, int x, int y,
                    int width, int scale, SDL_Color color, const char *text) {
    SDL_SetTextureColorMod(atlas, color.r, color.g, color.b);
    while (*text) {
        int i = glyph_index(SDL_StepUTF8(&text, NULL));
        int w = glyphs[i].width;
        if (w * scale > width) break;
        SDL_FRect src = {(float)(i % 16 * 16), (float)(i / 16 * 16), (float)w, 16};
        SDL_FRect dst = {(float)x, (float)y, (float)(w * scale), (float)(16 * scale)};
        SDL_RenderTexture(renderer, atlas, &src, &dst);
        x += w * scale;
        width -= w * scale;
    }
}
