#ifndef FD2_SCENE_H
#define FD2_SCENE_H

#include "archive.h"
#include "vga.h"

/* 完整新游戏初始过场预览。
 *
 * 按 new_game_opening_play @0x2fa63 播放 stage 32、31、0 的全部
 * 开场镜头、移动、对白和登场特效，在第一关正式交还控制前结束。
 * 资源来自 FDFIELD/FDSHAP、FDICON.B24、DATO、FDTXT 与 FDOTHER。
 */
int fd2_scene_play_new_game_prologue(fd2_vga *vga,
                                     const fd2_archive *fdother,
                                     int once);

#endif /* FD2_SCENE_H */
