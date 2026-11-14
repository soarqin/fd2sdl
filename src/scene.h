#ifndef FD2_SCENE_H
#define FD2_SCENE_H

#include "archive.h"
#include "bgm.h"
#include "field_audio.h"
#include "field_game.h"
#include "field_handoff.h"
#include "vga.h"
#include "text.h"
#include "scene_teleport.h"

/* App/Game借用：Scene不自seed、不另存消息值或视觉phase。-4/-5实际查UI0，
 * -6使用message_number；new/old/value均是原App DWORD的当前投影。 */
typedef struct {
    fd2_field_combat_rng_fn rng;
    void *rng_userdata;
    fd2_text_parameters text;
} fd2_scene_runtime;

typedef enum {
    FD2_SCENE_RESULT_OK = 0,
    FD2_SCENE_RESULT_ERROR = -1,
    FD2_SCENE_RESULT_HOST_QUIT = -2
} fd2_scene_result;

/* palette_fade_in_light @0x1f525 / palette_fade_out_dark @0x1f882：
 * 复用唯一减法冷子图；fast只保终态，不等待/上传中间帧。
 * HOST/错误保留已到达的DAC前缀，不改base palette、Game、SAV或RNG。 */
int fd2_scene_palette_fade(fd2_vga *vga, int fade_in, int fast);
/* field_stage_transition_effect @0x24618（原登记同名）的500ms/32级加白尾。
 * 只消费固定冷子图，返回Scene状态；不重放转场帧或修改Game。 */
int fd2_scene_stage_transition_finish(fd2_vga *vga);
/* callback82与stage源共用同一个有限传送冷子图；scope由当前Play调用拥有。 */
int fd2_scene_play_teleport_scope(fd2_vga *vga,const fd2_archive *fdother,
    fd2_field_game *game,fd2_field_audio *audio,int fast,fd2_scene_teleport_scope **scope);

/* 完整新游戏初始过场预览。
 *
 * 按 new_game_opening_play @code0 0x2231b 播放 stage 32、31、0 的全部
 * 开场镜头、移动、对白和登场特效，在第一关正式交还控制前结束。
 * 资源来自 FDFIELD/FDSHAP、FDICON.B24、DATO、FDTXT 与 FDOTHER。
 */
int fd2_scene_play_new_game_prologue(fd2_vga *vga,
                                     const fd2_archive *fdother,
                                     fd2_bgm_player *bgm,
                                     fd2_field_audio *audio,
                                     int once);

/* 与预览入口播放相同过场，并在成功结束时导出 stage 0 动态状态。 */
int fd2_scene_play_new_game_prologue_handoff(
        fd2_vga *vga,
        const fd2_archive *fdother,
        fd2_bgm_player *bgm,
        fd2_field_audio *audio,
        int once,
        fd2_field_handoff *handoff);
int fd2_scene_play_new_game_prologue_handoff_with_runtime(
    fd2_vga *vga, const fd2_archive *fdother, fd2_bgm_player *bgm,
    fd2_field_audio *audio, int once, fd2_field_handoff *handoff,
    const fd2_scene_runtime *runtime);

/* common_event冷图的一次有限呈现op；PC/分支/ACK归原Game续态。
 * pan/move/story逐已发生写点交还原motion owner，失败/退出不伪造终态。
 * 不解释整事件handler，不执行message/choice或任何业务交易。 */
int fd2_scene_play_common_step(fd2_vga *vga,const fd2_archive *fdother,
    fd2_field_game *game,const fd2_static_flow_node *node,
    fd2_field_audio *audio,int fast);
/* common领域的只读叶预检；不套关卡绑定支持门，不推进视觉/输入/RNG。 */
int fd2_scene_common_step_validate(const fd2_field_game *game,
    const fd2_archive *fdother,const fd2_static_flow_node *node);
int fd2_scene_common_step_validate_view(const fd2_field_game *game,
    const fd2_archive *fdother,const fd2_static_flow_node *node,
    const fd2_field_units *units,const fd2_field_unit *roster,size_t roster_count);
int fd2_scene_common_step_validate_resource_view(const fd2_field_game *game,
    const fd2_archive *fdother,const fd2_static_flow_node *node,
    const fd2_field_event_resource_view *view);

/* field_actor_group_arrival_effect @0x32999（原sub_32999）：资源／App底页
 * 先于原Game追加；12帧顺序、clear和wait由runtime.arrival决定。scope仅视觉。 */
typedef struct fd2_scene_arrival_visual fd2_scene_arrival_visual;
int fd2_scene_arrival_visual_validate(const fd2_archive *fdother);
int fd2_scene_arrival_visual_prepare(fd2_scene_arrival_visual **scope,
    fd2_vga *vga,const fd2_archive *fdother,size_t first);
int fd2_scene_arrival_visual_play(fd2_scene_arrival_visual *scope,fd2_vga *vga,
    const fd2_field_game *game,fd2_field_audio *audio,int fast);
void fd2_scene_arrival_visual_close(fd2_scene_arrival_visual **scope);

/* 旧预投影胜利图的只读提前检查，不呈现或切曲。
 * 单次source图不预跑终态，沿实际Scene叶晚验，不能用此接口预签该域。 */
int fd2_scene_stage_victory_validate(const fd2_field_game *game);

/* 按当前stage的同源冷流程执行，缺少已支持绑定明确失败。 */
int fd2_scene_play_stage_entry(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast);
int fd2_scene_play_stage_victory(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast);
/* 明确source.entry/victory冷图的逐叶机制入口；不自动绑定关卡。
 * 错误/宿主退出保留已到达前缀，已有App入口仍用上面的正式支持门。 */
int fd2_scene_play_stage_source_flow(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast, const char *source_id);

/* field_stage1_entry_play @0x32d18（原sub_32D18）：五个persistent部署槽的stage1开场；只交接动态
 * 坐标/镜头，不再次构造或结算单位。game须先由prepare_stage1预检。 */
int fd2_scene_play_stage1_entry(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast);
/* field_stage2_entry_play @0x32e8c（原sub_32E8C）：六部署槽、group1及17..19脚本；
 * 必须先由prepare_stage2完成本入口专用预检，错误/退出不交还部分动态状态。 */
int fd2_scene_play_stage2_entry(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, fd2_field_audio *audio, int fast);
/* sub_22F37 @0x22f37（原登记同名）：只呈现Game只读结算投影；
 * 首段继承Game ordinary_display_mode即本次51A83=0/1，fragment8之后清选择层。
 * 成功/失败/宿主退出均不改Game、roster或准备owner；Play完整成功后再commit。 */
int fd2_scene_play_stage1_victory(fd2_vga *vga, const fd2_archive *fdother,
    const fd2_field_game *game, fd2_field_audio *audio, int fast);
/* 22ef6/loot mode3借当前战场的完整FDTXT对白，不冒充UI0底框短消息。 */
int fd2_scene_play_field_story(fd2_vga *vga, const fd2_archive *fdother,
    fd2_field_game *game, size_t fragment, fd2_field_audio *audio, int fast);
/* 当前单位身份解析所有立绘、字模和控制码，只读，不呈现/耗键/RNG。 */
int fd2_scene_field_story_validate(const fd2_field_game *game, size_t fragment);
/* field_defeat_game_over_present @0x22e5c（原sub_22E5C）：FDOTHER79两整帧与已证1/9/36 tick。 */
int fd2_scene_play_game_over(fd2_vga *vga, const fd2_archive *fdother,
    fd2_bgm_player *bgm, int fast);

/* B4的1956b滑入框复用168b6布局；纯绘制／资源校验，不采样输入或改game。
 * arrow_frame仅接受18/19，13表示按16c57擦除；正式战场map已载入。 */
int fd2_scene_field_message_validate(const fd2_archive *fdother);
/* 1956b静态底框+原立绘初帧；资源预检后才绘，不改palette/输入或Game。 */
int fd2_scene_field_notice_page(fd2_vga *vga, const fd2_archive *fdother,
    const fd2_archive *dato, uint16_t portrait_id);
int fd2_scene_field_message_box(fd2_vga *vga, const fd2_archive *fdother);
int fd2_scene_field_message_arrow(fd2_vga *vga, const fd2_archive *fdother,
                                  unsigned arrow_frame);

#endif /* FD2_SCENE_H */
