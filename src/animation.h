#ifndef FD2_ANIMATION_H
#define FD2_ANIMATION_H

#include <stdint.h>

#include "archive.h"
#include "vga.h"

/* 播放 ANI.DAT 中的 AFM 动画。
 *
 * 复现 FUN_0001db69 @0x45635 (animation_play)：
 *   animation_play(anim_idx, frame_delay_ms, check_input)
 * 返回 1 表示普通按键跳过，-2 表示宿主退出请求。
 *
 * 相关字节码执行依据：
 *   FUN_0003471b @0x5c1e7  anim_buffer_init(size, framebuffer, palette_buf)
 *   FUN_0003473c @0x5c208  anim_exec_bytecode(command_count, frame_data)
 *   FUN_0003466c @0x5c138  opcode 6: AFM RLE 写入显存
 *   FUN_000346b1 @0x5c17d  opcode 7: 稀疏单点写入
 *   FUN_000346ca @0x5c196  opcode 8: 稀疏等值 run 写入
 *   FUN_000346f4 @0x5c1c0  opcode 9: 稀疏 literal 写入
 */
int fd2_animation_play(fd2_vga *vga, const fd2_archive *ani,
                       int anim_idx, uint32_t frame_delay_ms,
                       int check_input);

#endif /* FD2_ANIMATION_H */
