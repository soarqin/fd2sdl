#ifndef FD2_SCENE_TRANSITION_H
#define FD2_SCENE_TRANSITION_H

#include "field_game.h"

/* field_stage_transition_effect @0x24618（原同名）：仅本次Scene调用拥有
 * 临时456页与固定屏幕中心；不进入Game/SAV，也不预生成九个终态页。 */
typedef struct fd2_scene_transition_scope fd2_scene_transition_scope;

/* sub_1D4CB @0x1d4cb：确认并借用App持有的FDOTHER80。原前/后均reload，
 * 不stop当前voice；借用同一不可变bank保留其寿命，避免复制原泄漏。 */
int fd2_scene_transition_bank80(const fd2_archive *fdother,
    const fd2_field_audio *audio);

/* relative_cell_*为原四参前二项，不是camera或世界坐标。其余两项radius /
 * step由冷图逐帧展开；本API不保存执行游标。返回Scene的0/-1/-2。
 * begin只创建scope并执行一次11eee；失败/HOST后调用者仍须release。 */
int fd2_scene_transition_begin(fd2_scene_transition_scope **scope,
    fd2_field_game *game, fd2_vga *vga,
    int relative_cell_x, int relative_cell_y);
int fd2_scene_transition_play_sfx(fd2_scene_transition_scope *scope,
    fd2_field_audio *audio);
/* 单叶：整页恢复→22046的mask/actors交错→312×192投影并上传。
 * LUT frame仅1..9，radius沿已证几何域1..4096；不等待、不提交业务后缀。 */
int fd2_scene_transition_frame(fd2_scene_transition_scope *scope,
    unsigned lut_frame, int radius);
/* 成功、错误、HOST均可幂等释放；不回滚App页/视觉时钟或停止音频。 */
void fd2_scene_transition_release(fd2_scene_transition_scope **scope);

#endif
