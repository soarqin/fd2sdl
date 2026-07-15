#ifndef FD2_FIELD_GAME_H
#define FD2_FIELD_GAME_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "field.h"
#include "field_effect.h"
#include "field_event.h"
#include "field_ai.h"
#include "field_combat.h"
#include "field_command.h"
#include "field_handoff.h"
#include "field_info.h"
#include "field_path.h"
#include "field_unit.h"
#include "field_visual.h"
#include "font.h"
#include "map_sprite.h"
#include "text.h"
#include "tile.h"
#include "vga.h"

#define FD2_FIELD_VIEW_CELLS_X 13
#define FD2_FIELD_VIEW_CELLS_Y 8
#define FD2_FIELD_VIEW_BORDER 4
#define FD2_FIELD_EVENT_LOG_CAPACITY 64

typedef enum {
    FD2_FIELD_EVENT_TURN = 0,
    FD2_FIELD_EVENT_CELL
} fd2_field_event_kind;

typedef struct {
    fd2_field_event_kind kind;
    uint32_t turn_number;
    uint8_t phase;
    uint8_t action;
    uint8_t unit_index;
    uint8_t x;
    uint8_t y;
    uint8_t handled;
    uint8_t presentation_deferred;
    uint8_t presentation_unit_count;
    int16_t presentation_camera_x;
    int16_t presentation_camera_y;
    int16_t presentation_focus_x;
    int16_t presentation_focus_y;
    size_t slot;
} fd2_field_event_notice;

/* M6 战斗外围允许测试覆盖范围判定；玩家 side 2 → 目标 side 0、存活
 * 与可见性仍由 session 强制检查。正式 session 默认使用已确认的武器
 * record +0x0b/+0x0c 范围。 */
typedef int (*fd2_field_attack_target_fn)(
    const fd2_field_unit *attacker, const fd2_field_unit *defender,
    void *userdata);
typedef int (*fd2_field_critical_base_fn)(
    const fd2_field_unit *attacker, void *userdata, uint8_t *base_chance);

typedef enum {
    FD2_FIELD_BATTLE_ONGOING = 0,
    FD2_FIELD_BATTLE_VICTORY,
    FD2_FIELD_BATTLE_DEFEAT
} fd2_field_battle_result;

typedef enum {
    FD2_FIELD_INTERACTION_BROWSE = 0,
    FD2_FIELD_INTERACTION_UNIT_SELECTED,
    FD2_FIELD_INTERACTION_MOVING,
    FD2_FIELD_INTERACTION_COMMAND,
    FD2_FIELD_INTERACTION_TARGETING
} fd2_field_interaction;

/* 正式战场 session。
 *
 * 集中管理 stage 资源、单位、镜头、移动查询及后续交互状态。单位记录
 * 本身使用 fd2_field_unit 的原版 0x50 字节布局；移动 profile 与预算已
 * 确认，职业和其他战斗属性仍保留原始字节，不在 session 层猜测解释。
 */
typedef struct {
    fd2_archive field_archive;
    fd2_archive fdshap_archive;
    fd2_archive dato_archive;
    fd2_archive fdtxt_archive;
    fd2_field_map map;
    fd2_field_metadata metadata;
    fd2_field_placements placements;
    fd2_terrain_tileset terrain;
    fd2_map_sprite_bank sprites;
    fd2_field_visuals visuals;
    fd2_field_info_assets info_assets;
    fd2_field_command_assets command_assets;
    fd2_font font;
    fd2_text_entry ui_text;
    fd2_field_units units;

    size_t stage;
    int camera_cell_x;
    int camera_cell_y;
    int cursor_cell_x;
    int cursor_cell_y;
    int selected_unit;
    int detail_unit;
    int detail_visible;
    int detail_acknowledged_unit;
    fd2_image detail_portrait;

    /* field_player_command_execute @0x3dfa0 的四向图形菜单旁路状态；
     * 0..3 固定为 attack/magic/item/wait。M7 已有 AI 法术／物品事务，
     * 玩家选择状态机仍未实现，因此玩家菜单继续禁用两项。 */
    int command_selected;
    uint8_t command_disabled[FD2_FIELD_COMMAND_COUNT];
    uint8_t command_highlight_phase;
    uint8_t command_animation_phase;
    uint8_t command_animation_opening;
    uint64_t last_command_highlight_ms;

    /* M6 无演出攻击的 SDL 旁路状态；不改变原版 0x50 字节记录布局。 */
    int attack_target;
    fd2_field_attack_target_fn attack_target_allowed;
    void *attack_target_userdata;
    fd2_field_combat_rng_fn attack_rng;
    void *attack_rng_userdata;
    fd2_field_critical_base_fn attack_critical_base;
    void *attack_critical_base_userdata;
    uint8_t last_attack_valid;
    uint8_t last_attack_strikes;
    uint8_t last_counterattack_valid;
    uint8_t last_counterattack_strikes;
    fd2_field_attack_result last_attack;
    fd2_field_attack_result last_counterattack;

    /* 移动选择与执行状态。move_range/path 是 SDL 旁路数据，不进入
     * 原版 0x50 字节单位记录。 */
    fd2_field_path_result move_range;
    uint8_t *move_path;
    size_t move_path_capacity;
    size_t move_path_length;
    int move_preview_valid;
    int move_origin_x;
    int move_origin_y;
    uint8_t move_origin_direction;
    int move_origin_camera_x;
    int move_origin_camera_y;
    int camera_pixel_offset_x;
    int camera_pixel_offset_y;

    uint32_t turn_number;
    uint8_t active_side;
    fd2_field_interaction interaction;
    size_t phase_unit_cursor;
    uint8_t phase_ai_pass;      /* side 0: 0=magic/item gate, 1=normal pass */
    uint8_t cell_action_completed[FD2_FIELD_EVENT_SLOTS];
    uint64_t phase_next_action_ms;

    fd2_field_event_notice event_log[FD2_FIELD_EVENT_LOG_CAPACITY];
    size_t event_log_count;
    uint32_t event_total_count;
    uint32_t unhandled_event_count;
    uint32_t dropped_event_count;

    uint8_t idle_phase;
    uint64_t last_idle_ms;
    uint8_t terrain_visual_phase;
    uint64_t last_terrain_visual_ms;
    uint8_t range_visual_phase;
    uint64_t last_range_visual_ms;
    int info_panel_right;
    fd2_field_battle_result battle_result;
    int ready;
} fd2_field_game;

/* 当前只允许打开已验证初始 roster 的 stage 0。资源容器和渲染接口按
 * 通用 stage session 组织，后续确认其他关卡初始化后可直接扩展。 */
int fd2_field_game_open(fd2_field_game *game,
                        fd2_vga *vga,
                        const fd2_archive *fdother,
                        size_t stage);
void fd2_field_game_close(fd2_field_game *game);

/* 将过场的动态坐标、朝向、隐藏状态和镜头应用到已经从 FDFIELD 建立的
 * 正式单位表；阵营、模板和战斗属性仍保留 session 中的正式记录。 */
int fd2_field_game_apply_handoff(fd2_field_game *game,
                                 const fd2_field_handoff *handoff);

void fd2_field_game_tick(fd2_field_game *game, uint64_t now_ms);
void fd2_field_game_render(fd2_field_game *game, fd2_vga *vga);
int fd2_field_game_move_camera(fd2_field_game *game, int dx, int dy);
int fd2_field_game_move_cursor(fd2_field_game *game, int dx, int dy);
int fd2_field_game_unit_at(const fd2_field_game *game, int x, int y);
int fd2_field_game_confirm_cursor(fd2_field_game *game);
int fd2_field_game_cancel_selection(fd2_field_game *game);

/* M6：target_allowed 非 NULL 时覆盖正式武器范围判定，供状态机测试；
 * side 2 → side 0、必须持有武器等不变量不会被覆盖。critical_base 非
 * NULL 时按当前进攻者覆盖正式表，供测试使用；正式 session 默认读取
 * DS:0x24a8 的已捕获表。反击交换双方后会重新查询。 */
int fd2_field_game_set_attack_hooks(
    fd2_field_game *game, fd2_field_attack_target_fn target_allowed,
    void *target_userdata, fd2_field_critical_base_fn critical_base,
    void *critical_base_userdata,
    fd2_field_combat_rng_fn rng, void *rng_userdata);
int fd2_field_game_attack_target_is_legal(const fd2_field_game *game,
                                          size_t target_index);
int fd2_field_game_attack_is_available(const fd2_field_game *game);
int fd2_field_game_begin_attack(fd2_field_game *game);
int fd2_field_game_resolve_attack(fd2_field_game *game);
int fd2_field_game_cancel_attack(fd2_field_game *game);

/* M7.3 显式物理提交事务：重新计算 actor 当前移动范围及最优物理候选，
 * 仅当与传入 plan 完全一致时重建路径、提交移动，再复用
 * field_physical_exchange @0x3a6a2 的确定性逻辑核心。vga 可为空；非空时
 * 复用六相位步态。该 API 不负责选择 actor，也不接入自动 phase；法术／
 * 物品 handler 未确认时不得退化为物理或待机。 */
int fd2_field_game_commit_ai_physical(
    fd2_field_game *game,
    size_t actor_index,
    uint8_t side_selector,
    const fd2_field_ai_physical_candidate *plan,
    fd2_vga *vga,
    uint32_t frame_delay_ms);
int fd2_field_game_commit_ai_magic(
    fd2_field_game *game,
    size_t actor_index,
    uint8_t selector_mode,
    const fd2_field_ai_magic_candidate *plan);
int fd2_field_game_commit_ai_item(
    fd2_field_game *game,
    size_t actor_index,
    uint8_t selector_mode,
    const fd2_field_ai_item_candidate *plan);

/* field_command_menu_input @0x3ca10：四方向直接选择固定图标，禁用项
 * 不移动选择；确认只分派已接入的普通攻击和待机，返回操作复用移动
 * 快照回退，不消费 RNG。 */
int fd2_field_game_refresh_commands(fd2_field_game *game);
int fd2_field_game_select_command_direction(
    fd2_field_game *game, fd2_field_command_direction direction);
int fd2_field_game_confirm_command(fd2_field_game *game);
int fd2_field_game_animate_command(fd2_field_game *game, fd2_vga *vga,
                                   int opening, uint32_t frame_delay_ms);

int fd2_field_game_open_detail(fd2_field_game *game, size_t unit_index);
void fd2_field_game_close_detail(fd2_field_game *game);
typedef void (*fd2_field_detail_phase_fn)(void *userdata,
                                           int opening, int phase);
int fd2_field_game_animate_detail(fd2_field_game *game, fd2_vga *vga,
                                  int opening,
                                  fd2_field_detail_phase_fn phase_callback,
                                  void *phase_userdata);
int fd2_field_game_update_move_preview(fd2_field_game *game);
int fd2_field_game_execute_move(fd2_field_game *game,
                                fd2_vga *vga,
                                uint32_t frame_delay_ms);
int fd2_field_game_finish_unit_action(fd2_field_game *game);
int fd2_field_game_end_active_phase(fd2_field_game *game);
int fd2_field_game_process_automatic_action(fd2_field_game *game);
int fd2_field_game_process_automatic_action_visual(
    fd2_field_game *game, fd2_vga *vga, uint32_t frame_delay_ms);
size_t fd2_field_game_remaining_units(const fd2_field_game *game,
                                      uint8_t side);
fd2_field_battle_result fd2_field_game_battle_result(
    const fd2_field_game *game);
int fd2_field_game_compute_move_range(const fd2_field_game *game,
                                      size_t mover_index,
                                      fd2_field_path_result *result);
size_t fd2_field_game_group_count(const fd2_field_game *game, uint8_t group);

/* 战场全屏效果：分别复现 field_actor_group_flash @0x414ee、
 * field_earthquake_effect @0x4673b 与 field_stage_transition_effect
 * @0x4982c。prepare 与 play 分离，保证先完整构造再提交演出。
 * stage transition 的 center_screen_* 是 312×192 战场内区坐标，
 * `(0,0)` 对应 VGA `(4,4)`；允许中心在边界外，由 mask 安全裁剪。 */
int fd2_field_game_prepare_actor_group_flash(
        fd2_field_game *game, fd2_vga *vga,
        const uint8_t *unit_indices, size_t unit_count,
        uint8_t palette_index, fd2_field_effect_frames *frames);
int fd2_field_game_prepare_earthquake(fd2_field_game *game, fd2_vga *vga,
                                      fd2_field_effect_frames *frames);
int fd2_field_game_prepare_stage_transition(
        fd2_field_game *game, fd2_vga *vga,
        int center_screen_x, int center_screen_y,
        int radius, int radius_step,
        fd2_field_effect_frames *frames);
int fd2_field_game_play_actor_group_flash(
        fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
        const uint8_t *unit_indices, size_t unit_count,
        uint8_t palette_index, int timed);
int fd2_field_game_play_earthquake(fd2_field_game *game, fd2_vga *vga,
                                   fd2_field_audio *audio, int timed);
int fd2_field_game_play_stage_transition(
        fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
        int center_screen_x, int center_screen_y,
        int radius, int radius_step, int timed);

#endif /* FD2_FIELD_GAME_H */
