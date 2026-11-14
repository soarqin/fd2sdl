#ifndef FD2_SCENE_TELEPORT_H
#define FD2_SCENE_TELEPORT_H

#include "field_game.h"

/* sub_22253 @0x22253（原同名）的五参Scene局部资源。
 * 每个API只处理一个源叶；帧序、等待、坐标提交点由冷图决定。 */
typedef struct fd2_scene_teleport_scope fd2_scene_teleport_scope;
int fd2_scene_teleport_validate(const fd2_archive *fdother);
int fd2_scene_teleport_begin(fd2_scene_teleport_scope **scope,
    fd2_field_game *game, fd2_vga *vga, const fd2_archive *fdother,
    size_t actor, int destination_x, int destination_y, int draw_x, int draw_y);
int fd2_scene_teleport_sprite(fd2_scene_teleport_scope *scope, unsigned frame);
int fd2_scene_teleport_icon(fd2_scene_teleport_scope *scope);
int fd2_scene_teleport_sound(fd2_scene_teleport_scope *scope, fd2_field_audio *audio);
int fd2_scene_teleport_contract(fd2_scene_teleport_scope *scope, unsigned frame, unsigned multiplier);
int fd2_scene_teleport_commit(fd2_scene_teleport_scope *scope);
/* 返回1=top18行、0=24行，负值为Scene错误/HOST。 */
int fd2_scene_teleport_top(fd2_scene_teleport_scope *scope);
/* row==18..23在原dy==0的18行分支不读不写；冷图可共享24个有限叶。 */
int fd2_scene_teleport_wipe(fd2_scene_teleport_scope *scope, unsigned row);
int fd2_scene_teleport_expand(fd2_scene_teleport_scope *scope, unsigned frame);
void fd2_scene_teleport_release(fd2_scene_teleport_scope **scope);

#endif
