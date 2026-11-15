#ifndef FD2_FIELD_DEFEAT_H
#define FD2_FIELD_DEFEAT_H

#include "archive.h"
#include "vga.h"

struct fd2_field_game;
typedef struct fd2_field_defeat_visual fd2_field_defeat_visual;

/* field_defeated_units_finalize @0x1db65（原登记同名）的effect-local资源。
 * 只读预检12张LMI、旋转用sprite及临时页；成功前不改输出指针、Game、
 * App像素或任何clock。当前C仅存30个FX锚；正常参数域由F1取证，
 * 31+锚像素差异归F4，不按局部数组长度判未定义。
 * Game仍对完整active表旋转/提交flags，不据此缩减队伍或拒绝动作。 */
int fd2_field_defeat_prepare(fd2_field_defeat_visual **visual,
    const struct fd2_field_game *game,const fd2_vga *vga,const fd2_archive *fdother);
/* 三个有限绘制叶，不写方向/flags/HP/roster/RNG；分支、13+12帧顺序、
 * 等待、cue及尾11cac(0)由runtime.defeat冷图与原Game/Play owner负责。
 * rotate只11eee，Game逐raw写方向后actor只127e0，project作129ec+project；
 * base在flags1后画临时无死者页；
 * frame68..73累积写App，74..79先恢复临时底。返回0/1hostquit/-1。 */
int fd2_field_defeat_rotate(fd2_field_defeat_visual *visual,
    const struct fd2_field_game *game,fd2_vga *vga);
int fd2_field_defeat_actor(fd2_field_defeat_visual *visual,
    const struct fd2_field_game *game,fd2_vga *vga,size_t raw);
int fd2_field_defeat_project(fd2_field_defeat_visual *visual,
    const struct fd2_field_game *game,fd2_vga *vga);
int fd2_field_defeat_base(fd2_field_defeat_visual *visual,
    const struct fd2_field_game *game,fd2_vga *vga);
int fd2_field_defeat_frame(fd2_field_defeat_visual *visual,
    const struct fd2_field_game *game,fd2_vga *vga,unsigned frame);
/* 任何完成/错误/取消均调用；可重复，不恢复App/clock/业务，不含refresh。 */
void fd2_field_defeat_close(fd2_field_defeat_visual **visual);

#endif
