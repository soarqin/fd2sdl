#ifndef FD2_VGA_H
#define FD2_VGA_H

#include <stdint.h>
#include <SDL3/SDL.h>

#include "input.h"

/* 虚拟 VGA 320x200 256色
 *
 * 对应反编译中的硬件抽象：
 *   - 显存 0xa0000 (320x200 索引像素) -> vga.framebuffer
 *   - 调色板寄存器 DAT_00003a65 -> vga.palette
 *   - VGA DAC 端口 0x3c8/0x3c9 (FUN_0003522d) -> vga 纹理上传
 *
 * 反编译依据：
 *   FUN_00035058 @0x5cb24 = vga_clear(0xa0000, 0, &fill_pattern)
 *   FUN_0000f488 @0x46f54   = 调色板/DAC 应用（第三参数为暗度）
 *   FUN_0001db69 @0x45635  = animation_play(anim_idx, delay, check_input)
 *   FUN_0002b649 @0x53115  = palette_fade_to(start, end, step, r, g, b)
 *   FUN_0004bac9 @0x73595  = vsync_wait()
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
    fd2_input input;                     /* 唯一 SDL 帧输入所有者 */
    uint64_t frame_deadline_ns;          /* 宿主端绝对帧截止时间 */
    uint64_t frame_interval_ns;
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

/* 设置调色板暗度 (对应 FUN_0000f488 @0x46f54 的第三参数)
 * brightness=0x40: 全黑, brightness=0: 完整调色板
 * 公式: dac[i] = pal[i] * (0x40 - brightness) / 0x40
 * 用于 FUN_0001cc6d(渐入 0x40->0) 和 FUN_0001cfca(渐出 0->0x3f) */
void fd2_vga_set_brightness(fd2_vga *vga, int brightness);

/* 渐变到目标调色板 (对应 FUN_0002b649 @0x53115)
 * 从 (r,g,b) 基色渐变到 vga.palette，分 steps 步
 * 复现: FUN_0002b649(start, steps, r, g, b) */
void fd2_vga_palette_fade_to(fd2_vga *vga, int start, int steps,
                               uint8_t r, uint8_t g, uint8_t b);

/* 兼容包装：旧代码误把 FUN_0001db69 当淡入淡出；真实淡入淡出应使用
 * FUN_0002b649 / FUN_0000f488，FUN_0001db69 已确认为动画播放器。 */
void fd2_vga_palette_fade(fd2_vga *vga, int start, int end, int step);

/* 呈现当前 framebuffer，并开始新的输入帧。上一帧未消费按键会先被
 * 清除；各 UI 只消费本帧上下文动作。 */
void fd2_vga_present(fd2_vga *vga);

/* 按绝对 deadline 呈现并等待下一帧，避免把本帧渲染耗时叠加到间隔。 */
void fd2_vga_present_timed(fd2_vga *vga, uint32_t frame_ms);
/* 等待 timed present 已登记的最后一个 deadline，并结束该时间序列；
 * 不重复上传或呈现 framebuffer。 */
void fd2_vga_wait_frame_deadline(fd2_vga *vga);

/* 延时 (对应 thunk_FUN_0003b765 @0x63231, 毫秒)。宿主实现分片等待并
 * 检查退出事件；普通按键由下一输入帧统一读取。 */
void fd2_delay_ms(uint32_t ms);

/* 等待 BIOS tick (对应 FUN_000151f1 @0x3ccbd)
 * BIOS tick 频率 18.2Hz，1 tick ≈ 54.9ms
 * 用于帧同步等待 */
void fd2_wait_ticks(uint32_t ticks);

/* SDL 帧输入下的动画跳过查询：开始新帧并检查该帧是否有按键。
 * 返回: 0=无输入, 非0=本帧有按键（用于跳过片头）。 */
int fd2_input_check(fd2_vga *vga);

/* 释放资源 */
void fd2_vga_close(fd2_vga *vga);

#endif /* FD2_VGA_H */
