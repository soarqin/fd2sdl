#ifndef FD2_VGA_H
#define FD2_VGA_H

#include <stdint.h>
#include <SDL3/SDL.h>

#include "input.h"
#include "dialog_flow.h"
#include "field_page.h"
#include "field_visual_state.h"

/* 虚拟 VGA 320x200 256色
 *
 * 对应反编译中的硬件抽象：
 *   - 显存 0xa0000 (320x200 索引像素) -> vga.framebuffer
 *   - 调色板寄存器 DAT_00003a65 -> vga.palette
 *   - VGA DAC 端口 0x3c8/0x3c9 (FUN_0003522d) -> vga 纹理上传
 *
 * 反编译依据：
 *   memset @0x37910             = Watcom runtime 的通用 memset（历史上被
 *                                 误记为专用 vga_clear；调用点可指向任意缓冲）
 *   dac_apply_darkness @0x11d40 = 调色板/DAC 应用（第三参数为暗度）
 *   animation_play @0x20421     = animation_play(anim_idx, delay, check_input)
 *   palette_fade_to @0x2df01    = palette_fade_to(start, end, step, r, g, b)
 *   bios_keyboard_buffer_clear @0x4e381 = 清BDA待取键，不是vsync
 */

#define VGA_W 320
#define VGA_H 200
#define VGA_STRIDE 320       /* 0x140, 反编译中每行字节数 */
#define VGA_PALETTE_SIZE 768 /* 256 * 3 */

enum {FD2_VGA_OUTPUT_CREATE=-1,FD2_VGA_OUTPUT_LOCK=-2,FD2_VGA_OUTPUT_PITCH=-3,
      FD2_VGA_OUTPUT_RENDER=-4,FD2_VGA_OUTPUT_PRESENT=-5};
typedef struct {
    uint8_t  framebuffer[VGA_W * VGA_H]; /* 索引像素，对应 0xa0000 */
    uint8_t  palette[VGA_PALETTE_SIZE];  /* 当前调色板(6-bit)，对应 DAT_00003a65 */
    uint8_t  dac[VGA_PALETTE_SIZE];      /* 当前 DAC 输出(8-bit)，palette_expand 后 */
    SDL_Renderer *renderer;
    SDL_Texture   *texture;              /* ARGB8888 流式纹理 */
    int output_error; /* 此真实SDL输出owner的首错；离屏对象不借终端。 */
    fd2_input input;                     /* 唯一 SDL 键缓存/held/repeat owner */
    uint64_t frame_deadline_ns;          /* 宿主端绝对帧截止时间 */
    uint64_t frame_interval_ns;
    /* sub_4E31C @0x4e31c：app级DAC视觉owner；活动Load/新session不清零。
     * 原版object3 60000/60002，仅VGA初始化归零，不写SAV。
     * 11cac(0)调用本门，11cac(1)跳过；独立步态/等键按各自有证调用点推进。 */
    uint16_t field_water_last_tick;
    uint8_t field_water_phase;
    /* sub_127E0 @0x127e0：共享BIOS tick锚与raw26水平0/1px相位，不进SAV。 */
    uint16_t actor_jitter_tick;
    uint8_t actor_jitter_phase;
    /* 164e8的两个App级DWORD：换框/Load/闭嘴不重置，不进SAV。 */
    fd2_dialog_visual_state dialog_visual;
    /* 原51AAC的临时门取反；零初始化对应原初值1，不是用户选项。 */
    uint8_t info_panel_suppressed;
    uint32_t menu_highlight_phase, menu_highlight_tick;
    /* 26152/265ec/26eda/27079共享53F52，27079独立推进53F72；
     * App初始化为0，切服务/详情/Load不重置，不进SAV。 */
    uint32_t prepare_highlight_phase, prepare_sprite_phase;
    uint32_t prepare_pick_tick; /* 53F46：2b67f持久signed低WORD锚；不入SAV/Load。 */
    int32_t movement_tick; /* 53A0C：13460共享tick-edge锚；装载初态0，不入SAV。 */
    uint16_t browse_input_tick; /* 539F2：11aa8空等刷新锚，App初态0，不随Load重置。 */
    fd2_field_page *field_page; /* 原53A49的App视觉owner，跨Scene/Game保留。 */
    fd2_field_visual_state field_visual;
    uint8_t info_panel_right; /* 原51A0C横偏移1/242的bool投影：0=左，禁显时保留。 */
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

/* 历史宿主暗度缩放接口；并非11d40逐分量减法的精确投影。
 * brightness=0x40: 全黑, brightness=0: 完整调色板
 * 公式: dac[i] = pal[i] * (0x40 - brightness) / 0x40
 * 用于 FUN_0001cc6d(渐入 0x40->0) 和 FUN_0001cfca(渐出 0->0x3f) */
void fd2_vga_set_brightness(fd2_vga *vga, int brightness);

/* pal_partial_set @0x11df2：基色加原DWORD正偏移，上限63，只改选中DAC。
 * 当前已证source域first/last0..255、raw_add0..255、基色6bit；逆区间为空。
 * 坏域返回-1且不写；不更改palette或framebuffer。 */
int fd2_vga_palette_add(fd2_vga *vga, int first, int last, int32_t raw_add);

/* dac_apply_darkness @0x11d40：6bit基色减原darkness并夹0，只改所选DAC。
 * S2已证fade域darkness0..64；坏参数/基色先拒绝，不改palette/framebuffer。 */
int fd2_vga_palette_subtract(fd2_vga *vga, int first, int last, int32_t darkness);

/* bios_tick_delay @0x17aa9：signed WORD之差，负值加65536。
 * 与1e5c0的signed倒退提前结束合同不同。 */
int32_t fd2_vga_bios_tick_elapsed(uint16_t start, uint16_t now);

/* 渐变到目标调色板 (对应 FUN_0002b649 @0x2df01)
 * 从 (r,g,b) 基色渐变到 vga.palette，分 steps 步
 * 复现: FUN_0002b649(start, steps, r, g, b) */
void fd2_vga_palette_fade_to(fd2_vga *vga, int start, int steps,
                               uint8_t r, uint8_t g, uint8_t b);

/* 下列有时钟/等待的int接口0保旧行为，负数为失败；HOST仍由input owner读，
 * 不把时钟/输出错误翻译为HOST。movement的既有HOST=1合同单列。 */
/* 呈现当前 framebuffer，由同input owner归档到达键并采样repeat；
 * 不清除跨帧未读项。 */
int fd2_vga_present(fd2_vga *vga);
/* 同一游戏帧内刷新已改的glyph/mouth像素；不采键、推进时钟或新增等待。 */
void fd2_vga_upload_current_frame(fd2_vga *vga);

/* 按绝对 deadline 呈现并等待下一帧，避免把本帧渲染耗时叠加到间隔。 */
int fd2_vga_present_timed(fd2_vga *vga, uint32_t frame_ms);
/* 等待 timed present 已登记的最后一个 deadline，并结束该时间序列；
 * 不重复上传或呈现 framebuffer。 */
int fd2_vga_wait_frame_deadline(fd2_vga *vga);
/* field_movement_tick_edge_wait @0x13460（原sub_13460）：
 * 只等当前signed低WORD不同于App旧锚，再重新读取并写锚。
 * render期间已跨tick时立即返回；不清普通FIFO，HOST返回1，坏owner为-1。 */
int fd2_vga_wait_movement_tick(fd2_vga *vga);

/* 延时 (对应 thunk_FUN_0003b765 @0x3e01d, 毫秒)。宿主实现分片等待并
 * 检查退出事件并向同input owner归档普通键。 */
int fd2_delay_ms(uint32_t ms);
/* 正式App等待借同VGA输出owner，OS首错不改变游戏时钟、不消费后继输入。 */
int fd2_vga_delay_ms(fd2_vga *vga,uint32_t ms);
int fd2_vga_wait_ticks(fd2_vga *vga,uint32_t ticks);
/* 原一次checked观察前后查询同输出首错；失败不改actual。 */
int fd2_vga_observe_checked(const fd2_vga *vga,uint64_t now,uint64_t *actual);
/* 只读既有输出首错；NULL表示纯离屏对象没有输出owner。 */
int fd2_vga_output_error(const fd2_vga *vga);

/* 等待 BIOS tick (对应 FUN_000151f1 @0x17aa9)
 * BIOS tick 频率 18.2Hz，1 tick ≈ 54.9ms
 * 用于帧同步等待 */
int fd2_wait_ticks(uint32_t ticks);

/* 非消费式查询同input owner的待取键，不重新泵送或采样。 */
int fd2_input_check(fd2_vga *vga);

/* 释放资源 */
void fd2_vga_close(fd2_vga *vga);

#endif /* FD2_VGA_H */
