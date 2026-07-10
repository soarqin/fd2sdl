#ifndef FD2_VGA_H
#define FD2_VGA_H

#include <stdint.h>
#include <SDL3/SDL.h>

/* 虚拟 VGA 320x200 256色
 *
 * 对应反编译中的硬件抽象：
 *   - 显存 0xa0000 (320x200 索引像素) -> vga.framebuffer
 *   - 调色板寄存器 DAT_00003a65 -> vga.palette
 *   - VGA DAC 端口 0x3c8/0x3c9 (FUN_0003522d) -> vga 纹理上传
 *
 * 反编译依据：
 *   FUN_00035058 @0x35058 = vga_clear(0xa0000, 0, &fill_pattern)
 *   FUN_0000f488 @0xf488   = 调色板/DAC 应用（第三参数为暗度）
 *   FUN_0001db69 @0x1db69  = animation_play(anim_idx, delay, check_input)
 *   FUN_0002b649 @0x2b649  = palette_fade_to(start, end, step, r, g, b)
 *   FUN_0004bac9 @0x4bac9  = vsync_wait()
 */

#define VGA_W 320
#define VGA_H 200
#define VGA_STRIDE 320       /* 0x140, 反编译中每行字节数 */
#define VGA_PALETTE_SIZE 768 /* 256 * 3 */

typedef struct {
    uint8_t  framebuffer[VGA_W * VGA_H]; /* 索引像素，对应 0xa0000 */
    uint8_t  palette[VGA_PALETTE_SIZE];  /* 当前调色板(6-bit)，对应 DAT_00003a65 */
    uint8_t  dac[VGA_PALETTE_SIZE];      /* 当前 DAC 输出(8-bit)，palette_expand 后 */
    SDL_Renderer *renderer;
    SDL_Texture   *texture;              /* ARGB8888 流式纹理 */
    uint64_t frame_start;               /* 帧计时，对应 vsync */
} fd2_vga;

/* 初始化 VGA + SDL 窗口/渲染器 */
int  fd2_vga_init(fd2_vga *vga, SDL_Window *win, SDL_Renderer *ren);

/* 清屏 (对应 FUN_00035058: memset 0xa0000)
 * 复现: FUN_00035058(0xa0000, 0, &pattern) */
void fd2_vga_clear(fd2_vga *vga, uint8_t fill);

/* 设置调色板 (对应 DAT_00003a65 赋值 + FUN_0000f488)
 * pal 指向 768 字节 6-bit RGB 数据
 * 复现: DAT_00003a65 = res_load(...,idx); FUN_0000f488(0,0xff,flag) */
void fd2_vga_set_palette(fd2_vga *vga, const uint8_t pal6[768]);

/* 设置调色板暗度 (对应 FUN_0000f488 @0xf488 的第三参数)
 * brightness=0x40: 全黑, brightness=0: 完整调色板
 * 公式: dac[i] = pal[i] * (0x40 - brightness) / 0x40
 * 用于 FUN_0001cc6d(渐入 0x40->0) 和 FUN_0001cfca(渐出 0->0x3f) */
void fd2_vga_set_brightness(fd2_vga *vga, int brightness);

/* 渐变到目标调色板 (对应 FUN_0002b649 @0x2b649)
 * 从 (r,g,b) 基色渐变到 vga.palette，分 steps 步
 * 复现: FUN_0002b649(start, steps, r, g, b) */
void fd2_vga_palette_fade_to(fd2_vga *vga, int start, int steps,
                               uint8_t r, uint8_t g, uint8_t b);

/* 兼容包装：旧代码误把 FUN_0001db69 当淡入淡出；真实淡入淡出应使用
 * FUN_0002b649 / FUN_0000f488，FUN_0001db69 已确认为动画播放器。 */
void fd2_vga_palette_fade(fd2_vga *vga, int start, int end, int step);

/* vsync 等待 + 呈现 (对应 FUN_0004bac9 @0x4bac9 + thunk_FUN_0003b765)
 * 将 framebuffer+palette 上传到纹理并渲染 */
void fd2_vga_present(fd2_vga *vga);

/* 延时 (对应 thunk_FUN_0003b765 @0x3b765, 毫秒)
 * 原始实现用 int 21h/ah=2Ch 忙等待，参数为毫秒 */
void fd2_delay_ms(uint32_t ms);

/* 等待 BIOS tick (对应 FUN_000151f1 @0x151f1)
 * BIOS tick 频率 18.2Hz，1 tick ≈ 54.9ms
 * 用于帧同步等待 */
void fd2_wait_ticks(uint32_t ticks);

/* 检查输入 (对应 FUN_0000dd68 @0xdd68)
 * 返回: 0=无输入, 非0=有按键(跳过) */
int fd2_input_check(void);

/* 释放资源 */
void fd2_vga_close(fd2_vga *vga);

#endif /* FD2_VGA_H */
