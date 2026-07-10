#include "vga.h"
#include <string.h>

/* 6-bit VGA -> 8-bit 扩展: (v << 2) | (v >> 4)
 * 源自反编译 FUN_0000f488 中的 DAC 写入逻辑 */
static inline uint8_t pal_expand6(uint8_t v) {
    return (uint8_t)((v << 2) | (v >> 4));
}

/* FUN_0003522d @0x3522d: out(port, val) - 写 VGA DAC 端口
 * 在 SDL3 中映射为更新 dac 数组 */
static void dac_out(fd2_vga *vga, uint16_t port, uint8_t val) {
    (void)port; (void)vga; (void)val;
    /* 实际 DAC 更新在 fd2_vga_set_palette / fade 中处理 */
}

int fd2_vga_init(fd2_vga *vga, SDL_Window *win, SDL_Renderer *ren) {
    (void)win;
    memset(vga, 0, sizeof(*vga));
    vga->renderer = ren;
    vga->texture = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     VGA_W, VGA_H);
    if (!vga->texture) return -1;
    return 0;
}

void fd2_vga_clear(fd2_vga *vga, uint8_t fill) {
    /* 复现 FUN_00035058 @0x35058: 清屏(0xa0000, fill, pattern)
     * FUN_00035058(0xa0000, 0, &DAT_0000fa00) = 用 0 填充显存 */
    memset(vga->framebuffer, fill, sizeof(vga->framebuffer));
}

void fd2_vga_set_palette(fd2_vga *vga, const uint8_t pal6[768]) {
    /* 复现: DAT_00003a65 = pal; FUN_0000f488(0, 0xff, flag)
     * FUN_0000f488 遍历调色板，通过 out(0x3c8,idx)+out(0x3c9,r/g/b) 写 DAC */
    memcpy(vga->palette, pal6, VGA_PALETTE_SIZE);
    for (int i = 0; i < 256; i++) {
        vga->dac[i*3]   = pal_expand6(pal6[i*3]);
        vga->dac[i*3+1] = pal_expand6(pal6[i*3+1]);
        vga->dac[i*3+2] = pal_expand6(pal6[i*3+2]);
    }
}

void fd2_vga_set_brightness(fd2_vga *vga, int brightness) {
    /* 复现启动流程中 FUN_0000f488(0, 0xff, brightness) 的可见效果。
     * 注意第三参数是“暗度”而不是亮度：
     *   brightness=0x40 -> DAC 全黑
     *   brightness=0    -> 当前调色板完整亮度
     * 证据：FUN_0001cc6d @0x1cc77 从 0x40 递减到 0，是淡入；
     *       FUN_0001cfca @0x1cfd4/0x1cc4b 从 0 递增到 0x3f，是淡出。 */
    if (brightness < 0) brightness = 0;
    if (brightness > 0x40) brightness = 0x40;
    int level = 0x40 - brightness;
    for (int i = 0; i < 256; i++) {
        int idx = i * 3;
        uint8_t r = (uint8_t)((vga->palette[idx]   * level) / 0x40);
        uint8_t g = (uint8_t)((vga->palette[idx+1] * level) / 0x40);
        uint8_t b = (uint8_t)((vga->palette[idx+2] * level) / 0x40);
        vga->dac[idx]   = pal_expand6(r);
        vga->dac[idx+1] = pal_expand6(g);
        vga->dac[idx+2] = pal_expand6(b);
    }
}

void fd2_vga_palette_fade_to(fd2_vga *vga, int start, int steps,
                               uint8_t r, uint8_t g, uint8_t b) {
    /* 复现 FUN_0002b649 @0x2b649: palette_fade_to(start, steps, r, g, b)
     * 从基色(r,g,b)渐变到 vga->palette，分 0x28=40 步
     * 每个分量: dac = (pal[i] - base) * steps / 0x28 + base
     * 注意: pal 是 6-bit，base 也是 6-bit */
    const int total = 0x28; /* 40 步 */
    for (int i = start; i < 256; i++) {
        int idx = i * 3;
        uint8_t dr = (uint8_t)(((vga->palette[idx]   - r) * steps) / total + r);
        uint8_t dg = (uint8_t)(((vga->palette[idx+1] - g) * steps) / total + g);
        uint8_t db = (uint8_t)(((vga->palette[idx+2] - b) * steps) / total + b);
        vga->dac[idx]   = pal_expand6(dr);
        vga->dac[idx+1] = pal_expand6(dg);
        vga->dac[idx+2] = pal_expand6(db);
    }
    (void)b; /* b used above via vga->palette */
}

void fd2_vga_palette_fade(fd2_vga *vga, int start, int end, int step) {
    /* 兼容包装：真实 FUN_0001db69 @0x1db69 已确认是 animation_play，
     * 调色板插值实际对应 FUN_0002b649 @0x2b649。 */
    fd2_vga_palette_fade_to(vga, start, step, 0, 0, 0);
    (void)end;
}

void fd2_vga_present(fd2_vga *vga) {
    /* 复现 FUN_0004bac9 @0x4bac9 (vsync_wait) + 帧呈现
     * 将 framebuffer(索引) + dac(调色板) 转为 ARGB8888 上传 */
    uint8_t *pixels;
    int pitch;
    if (!SDL_LockTexture(vga->texture, NULL, (void **)&pixels, &pitch)) return;
    for (int i = 0; i < VGA_W * VGA_H; i++) {
        uint8_t idx = vga->framebuffer[i];
        pixels[i*4+0] = vga->dac[idx*3+2]; /* B */
        pixels[i*4+1] = vga->dac[idx*3+1]; /* G */
        pixels[i*4+2] = vga->dac[idx*3];   /* R */
        pixels[i*4+3] = 0xFF;               /* A */
    }
    SDL_UnlockTexture(vga->texture);
    SDL_RenderTexture(vga->renderer, vga->texture, NULL, NULL);
    SDL_RenderPresent(vga->renderer);
}

void fd2_delay_ms(uint32_t ms) {
    /* 复现 thunk_FUN_0003b765 @0x3b765: delay_ms(N)
     * 原始: loops = (N * DAT_000041b0 + 500) / 1000, 忙等待 int 21h/ah=2Ch
     * DAT_000041b0 运行时设置，参数语义为毫秒 */
    SDL_Delay(ms);
}

void fd2_wait_ticks(uint32_t ticks) {
    /* 复现 FUN_000151f1 @0x151f1: wait_ticks(N)
     * 等待 N 个 BIOS tick (18.2Hz, 1 tick ≈ 54.9ms) */
    SDL_Delay(ticks * 55);
}

static int g_input_flag = 0;

int fd2_input_check(void) {
    /* 复现 FUN_0000dd68 @0xdd68: 检查是否有按键
     * 返回非0表示有输入(用于跳过片头) */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) return 1;
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) return 1;
            g_input_flag = 1;
        }
    }
    int r = g_input_flag;
    g_input_flag = 0;
    return r;
}

void fd2_vga_close(fd2_vga *vga) {
    if (vga->texture) SDL_DestroyTexture(vga->texture);
    vga->texture = NULL;
}
