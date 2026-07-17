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
    fd2_input_init(&vga->input);
    vga->renderer = ren;
    vga->texture = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     VGA_W, VGA_H);
    if (!vga->texture) return -1;
    return 0;
}

void fd2_vga_clear(fd2_vga *vga, uint8_t fill) {
    /* 复现 FUN_00035058 @0x5cb24: 清屏(0xa0000, fill, pattern)
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
    /* 复现 FUN_0002b649 @0x53115: palette_fade_to(start, steps, r, g, b)
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
    /* 兼容包装：真实 FUN_0001db69 @0x45635 已确认是 animation_play，
     * 调色板插值实际对应 FUN_0002b649 @0x53115。 */
    fd2_vga_palette_fade_to(vga, start, step, 0, 0, 0);
    (void)end;
}

static void fd2_vga_present_impl(fd2_vga *vga) {
    /* input.c 是唯一调用 SDL_PollEvent 的位置。原版动画的 input_check
     * 只窥视 BIOS 缓冲，因此这里先将宿主事件写入 FIFO，随后由各 UI 决定
     * 是否消费。 */
    fd2_input_pump(&vga->input);

    uint32_t colors[256];
    for (int i = 0; i < 256; i++) {
        colors[i] = 0xff000000u |
                    (uint32_t)vga->dac[i * 3] << 16 |
                    (uint32_t)vga->dac[i * 3 + 1] << 8 |
                    vga->dac[i * 3 + 2];
    }

    uint8_t *pixels;
    int pitch;
    if (!SDL_LockTexture(vga->texture, NULL, (void **)&pixels, &pitch)) return;
    for (int y = 0; y < VGA_H; y++) {
        uint32_t *dst = (uint32_t *)(pixels + (size_t)y * (size_t)pitch);
        const uint8_t *src = vga->framebuffer + (size_t)y * VGA_STRIDE;
        for (int x = 0; x < VGA_W; x++)
            dst[x] = colors[src[x]];
    }
    SDL_UnlockTexture(vga->texture);
    SDL_RenderTexture(vga->renderer, vga->texture, NULL, NULL);
    SDL_RenderPresent(vga->renderer);
}

static void fd2_delay_until(uint64_t deadline_ns) {
    const uint64_t ns_per_ms = 1000000u;
    for (;;) {
        fd2_input_poll_host_events();
        /* 所有场景和动画的等待都经过此处。宿主中断或窗口关闭一旦到达
         * 就立即结束当前 deadline，随后 present／状态机从统一 input
         * 服务取得持久 quit 请求。 */
        if (fd2_input_host_quit_requested()) break;
        uint64_t now = SDL_GetTicksNS();
        if (now >= deadline_ns) break;
        uint64_t remain = deadline_ns - now;
        if (remain > 2u * ns_per_ms) {
            uint64_t sleep_ms = remain / ns_per_ms - 1u;
            if (sleep_ms > 8u) sleep_ms = 8u;
            SDL_Delay((uint32_t)sleep_ms);
        } else {
            SDL_DelayPrecise(remain);
        }
    }
    fd2_input_poll_host_events();
}

void fd2_vga_present(fd2_vga *vga) {
    /* 固定帧率动画切到普通呈现时，先让上一帧显示满其目标间隔，再重置
     * deadline。这样最后一个步态相位不会被紧随其后的对话立即覆盖。 */
    if (vga->frame_deadline_ns != 0 &&
        SDL_GetTicksNS() < vga->frame_deadline_ns)
        fd2_delay_until(vga->frame_deadline_ns);
    fd2_vga_present_impl(vga);
    vga->frame_deadline_ns = 0;
    vga->frame_interval_ns = 0;
}

void fd2_vga_wait_frame_deadline(fd2_vga *vga) {
    if (!vga) return;
    if (vga->frame_deadline_ns != 0 &&
        SDL_GetTicksNS() < vga->frame_deadline_ns)
        fd2_delay_until(vga->frame_deadline_ns);
    vga->frame_deadline_ns = 0;
    vga->frame_interval_ns = 0;
}

void fd2_vga_present_timed(fd2_vga *vga, uint32_t frame_ms) {
    if (frame_ms == 0) {
        fd2_vga_present(vga);
        return;
    }

    uint64_t interval = (uint64_t)frame_ms * 1000000u;
    uint64_t now = SDL_GetTicksNS();
    uint64_t target = vga->frame_deadline_ns;
    int continue_schedule = target != 0 &&
                            vga->frame_interval_ns == interval &&
                            now <= target + interval;

    /* 在呈现当前帧之前等到 deadline：下一帧的场景合成、sprite blit 和
     * 纹理准备耗时都包含在帧预算内，而不是额外叠加在固定 delay 后。 */
    if (target != 0 && now < target)
        fd2_delay_until(target);
    fd2_vga_present_impl(vga);

    now = SDL_GetTicksNS();
    if (continue_schedule)
        vga->frame_deadline_ns = target + interval;
    else
        vga->frame_deadline_ns = now + interval;
    vga->frame_interval_ns = interval;
}

void fd2_delay_ms(uint32_t ms) {
    /* 复现 thunk_FUN_0003b765 @0x63231: delay_ms(N)。DOS 原版忙等待；
     * SDL 版分成至多 8 ms 的片段并持续泵送事件，但不从队列取走按键。 */
    if (ms == 0) {
        SDL_PumpEvents();
        return;
    }
    fd2_delay_until(SDL_GetTicksNS() + (uint64_t)ms * 1000000u);
}

void fd2_wait_ticks(uint32_t ticks) {
    /* 复现 FUN_000151f1 @0x3ccbd: wait_ticks(N)
     * 等待 N 个 BIOS tick (18.2Hz, 1 tick ≈ 54.9ms)。 */
    fd2_delay_ms(ticks * 55);
}

int fd2_input_check(fd2_vga *vga) {
    /* 复现 input_check @0x35834：原版只比较 BDA 0x041a/0x041c，不读取
     * 键盘缓冲。ANI/片头跳过后，同一按键仍应留给后续标题或菜单。
     * 宿主端在长延时中仍须收集窗口事件；收集由 input.c 完成，不消费 FIFO。 */
    if (!vga) return 0;
    fd2_input_pump(&vga->input);
    return fd2_input_has_any_key(&vga->input);
}

void fd2_vga_close(fd2_vga *vga) {
    if (vga->texture) SDL_DestroyTexture(vga->texture);
    vga->texture = NULL;
}
