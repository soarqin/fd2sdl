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
#include "field_system_menu.h"
#include "field_manual_slot.h"
#include "field_handoff.h"
#include "field_info.h"
#include "field_item.h"
#include "field_magic.h"
#include "field_path.h"
#include "field_unit.h"
#include "field_visual.h"
#include "font.h"
#include "map_sprite.h"
#include "save.h"
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

/* field_cell_event_lookup @0x13a44 的51A8F旁路owner；source_slot仅供
 * SDL诊断，handler仍在dispatch时读取actor最终坐标。不进入存档。 */
typedef struct {
    uint8_t valid;
    uint8_t action;
    size_t source_slot;
} fd2_field_pending_cell_event;

/* SDL session 的首个不可恢复错误，不写入原版存档或单位记录。 */
typedef enum {
    FD2_FIELD_FAILURE_NONE = 0,
    FD2_FIELD_FAILURE_EVENT_CAPACITY,
    FD2_FIELD_FAILURE_EVENT_UNSUPPORTED,
    FD2_FIELD_FAILURE_EVENT_EXECUTION,
    FD2_FIELD_FAILURE_EVENT_DATA,
    FD2_FIELD_FAILURE_ACTION
} fd2_field_failure;

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
    FD2_FIELD_INTERACTION_TARGETING,
    FD2_FIELD_INTERACTION_SYSTEM_MENU,
    FD2_FIELD_INTERACTION_MANUAL_SLOT,
    FD2_FIELD_INTERACTION_AUXILIARY,
    FD2_FIELD_INTERACTION_MAGIC_LIST,
    FD2_FIELD_INTERACTION_MAGIC_TARGETING,
    FD2_FIELD_INTERACTION_ITEM_MENU,
    FD2_FIELD_INTERACTION_ITEM_LIST,
    FD2_FIELD_INTERACTION_ITEM_TARGETING
} fd2_field_interaction;

/* field_unit_detail_present @0x17aed（原sub_17AED）：装备页任意键后，有法术才显示第二页。 */
typedef enum {
    FD2_FIELD_DETAIL_EQUIPMENT = 0,
    FD2_FIELD_DETAIL_MAGIC
} fd2_field_detail_page;

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
    fd2_field_system_menu system_menu;
    fd2_field_manual_slot_picker manual_slot_picker;
    /* 活动快照 meta[14..17] 的持久配置 owner；菜单打开时复制到
     * system_menu，toggle 后同步写回。 */
    uint8_t option_values[FD2_FIELD_SYSTEM_MENU_COUNT];
    /* 原版 DS:0x3bf3；手工 slot meta +0x02 的 32 位资源值 owner。
     * 当前战斗系统尚未消费该值，但 Save/Load 必须无损往返。 */
    uint32_t resource_value;
    fd2_field_system_action last_system_action;
    /* Save/Load 后端结果的短暂展示状态；只保存已确认 FDTXT fragment，
     * 不写入原版活动快照。 */
    uint16_t system_result_fragment;
    uint64_t system_result_until_ms;
    /* 手工 slot Save/Load 的 FDTXT 结果；picker 关闭后由 field_play
     * 在下一帧显示，避免把结果误并入活动快照 confirmation。 */
    uint16_t manual_slot_result_fragment;
    uint64_t manual_slot_result_until_ms;
    /* code0 0x9df7 的 selector 2 资源门；field_play 按宿主提供的
     * FD2.SAV 路径刷新，测试／宿主后端也可显式覆盖。 */
    int save_resource_available;
    /* 原版 DS:0x3f56：stage-indexed 31-byte runtime table pointer 的
     * SDL 旁路 owner。当前只支持 stage 0，保存其确认的 stage table
     * entry offset，不把未知字段解释成 gameplay 状态。 */
    uint32_t stage_runtime_entry_offset;
    fd2_font font;
    fd2_text_entry ui_text;
    fd2_field_units units;
    /* sub_112A5 @0x112a5 / sub_10010 @0x10010 的独立持久 roster。
     * raw 记录包含未部署角色；未使用尾也整块归 session。
     * count 不等于 active actor 或 side 2 数。 */
    fd2_field_unit roster[FD2_SAVE_BATTLE_MAX_ROSTER];
    size_t roster_count;

    size_t stage;
    int camera_cell_x;
    int camera_cell_y;
    int cursor_cell_x;
    int cursor_cell_y;
    int selected_unit;
    int detail_unit;
    int detail_visible;
    fd2_field_detail_page detail_page;
    int detail_acknowledged_unit;
    fd2_image detail_portrait;

    /* field_player_command_execute @0x18d8c 的四向图形菜单旁路状态；
     * 0..3 固定为 attack/magic/item/wait。玩家法术仅开放已验证基础分支；
     * M7的AI物品事务不等于玩家入口，ITEM继续禁用。 */
    int command_selected;
    uint8_t command_disabled[FD2_FIELD_COMMAND_COUNT];
    uint8_t command_highlight_phase;
    uint8_t command_animation_phase;
    uint8_t command_animation_opening;
    uint64_t last_command_highlight_ms;
    /* field_player_magic_command_execute @0x1cff0 的53C57等价索引；
     * 只归列表交互，不写入原版单位或存档。 */
    int magic_list_selected;
    /* 1b932/1b9de的raw槽索引，ITEM_LIST/TARGETING共用；memset零态及成功Load
     * 均归0，不写SAV。ITEM_MENU复用上述command四向owner，语义为
     * Use/Give/Equip/Drop；SDL目前仅Use可选，非原版能力表。 */
    int item_list_selected;
    /* field_cell_selection_execute @0x115b6 的本次target局部轮询，
     * MAGIC/ITEM_TARGETING互斥借用候选buffer；id只归MAGIC。
     * 不复用browse的focus_cycle_unit，不写SAV。 */
    uint8_t magic_target_id;
    uint8_t magic_target_candidates[FD2_FIELD_MAX_UNITS];
    size_t magic_target_count;
    size_t magic_target_cycle;

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
    /* field controller @code0 0x17e7 的 DS:0x3ae9 等价物；Esc、Z、
     * keypad 5 轮换可聚焦的 side 2 actor，并按 raw actor index 回绕。 */
    size_t focus_cycle_unit;
    uint8_t phase_ai_pass;      /* side 0: 0=magic/item gate, 1=normal pass */
    uint8_t cell_action_completed[FD2_FIELD_EVENT_SLOTS];
    uint64_t phase_next_action_ms;

    fd2_field_event_notice event_log[FD2_FIELD_EVENT_LOG_CAPACITY];
    size_t event_log_count;
    uint32_t event_total_count;
    uint32_t unhandled_event_count;
    uint32_t dropped_event_count;
    fd2_field_failure failure;
    fd2_field_event_notice failure_notice;

    uint8_t idle_phase;
    uint64_t last_idle_ms;
    uint8_t terrain_visual_phase;
    uint64_t last_terrain_visual_ms;
    uint8_t range_visual_phase;
    uint64_t last_range_visual_ms;
    int info_panel_right;
    fd2_field_battle_result battle_result;
    int ready;
    fd2_field_pending_cell_event pending_cell_event;
    /* field_controller_input @0x117e7 在13565整轮phase之后续行；只保存
     * 原玩家actor，不保存共享pending。waiting为0即无上下文，actor无效。
     * 成功active Load清除，失败Load保留；均不写入SAV。 */
    size_t player_post_actor;
    uint8_t player_post_waiting_phase;
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
/* sub_1B1E7 @0x1b1e7（原登记 sub_1B1E7）的地图／actor 背景；
 * 不绘光标、范围或任何UI，也不改变信息面板的左右侧状态。 */
void fd2_field_game_render_background(fd2_field_game *game, fd2_vga *vga);
int fd2_field_game_move_camera(fd2_field_game *game, int dx, int dy);
int fd2_field_game_move_cursor(fd2_field_game *game, int dx, int dy);
/* 复现 field_controller_input @code0 0x17e7 的焦点轮换分支：跳过
 * hidden／AI-ineligible／acted／非 side 2 actor；成功时移动到 actor 格。 */
int fd2_field_game_cycle_focus(fd2_field_game *game);
int fd2_field_game_unit_at(const fd2_field_game *game, int x, int y);
int fd2_field_game_confirm_cursor(fd2_field_game *game);
int fd2_field_game_cancel_selection(fd2_field_game *game);

/* M8.1 系统菜单骨架：仅在 browse、玩家回合且焦点没有可见 actor
 * 时进入独立 SYSTEM_MENU 上下文；不复用单位 COMMAND 状态。 */
int fd2_field_game_open_system_menu(fd2_field_game *game);
int fd2_field_game_select_system_menu_direction(
    fd2_field_game *game, fd2_field_command_direction direction);
fd2_field_system_action fd2_field_game_confirm_system_menu(
    fd2_field_game *game);
fd2_field_system_action fd2_field_game_cancel_system_menu(
    fd2_field_game *game);
void fd2_field_game_set_save_resource_available(fd2_field_game *game,
                                                int available);
int fd2_field_game_animate_system_menu(fd2_field_game *game, fd2_vga *vga,
                                       int opening,
                                       uint32_t frame_delay_ms);

/* 原版四槽 hand_save/hand_load picker @code0 0x19ab2/0x19bcb。
 * 这是独立 interaction，不与四向 system menu 选择状态混用。当前只接
 * picker 生命周期；确认 slot 后的磁盘提交／runtime restore 由下一层
 * owner 执行，不能把 active snapshot API 用来冒充手工 slot。 */
int fd2_field_game_open_manual_slot_picker(
    fd2_field_game *game, const char *path,
    fd2_field_manual_slot_mode mode, size_t initial_slot);
int fd2_field_game_move_manual_slot_picker(fd2_field_game *game, int delta);
int fd2_field_game_confirm_manual_slot_picker(fd2_field_game *game);
int fd2_field_game_cancel_manual_slot_picker(fd2_field_game *game);

/* code0 0x9f7a..0xa136 / 0x10..0x44b 的活动战场快照 codec。
 * snapshot 必须来自完整 save 容器，以保留未知字节。原版 loader 返回
 * 玩家 side 2 控制器边界；快照不保存 RNG，SDL 也不伪造 checkpoint。 */
int fd2_field_game_export_battle_snapshot(
    const fd2_field_game *game, fd2_save_battle_snapshot *snapshot);
int fd2_field_game_import_battle_snapshot(
    fd2_field_game *game, const fd2_save_battle_snapshot *snapshot);
int fd2_field_game_save_resource_available(const fd2_field_game *game,
                                           const char *path);
int fd2_field_game_save_active(fd2_field_game *game, const char *path);
int fd2_field_game_load_active(fd2_field_game *game, const char *path);
/* 四槽 Save 写入独立持久 roster，保留未知 meta。Load 的原版 stage
 * 重建路径尚未恢复，目前显式拒绝且无副作用，不能把 slot 当 active 表。 */
int fd2_field_game_save_manual_slot(fd2_field_game *game, const char *path,
                                    size_t slot_index);
int fd2_field_game_load_manual_slot(fd2_field_game *game, const char *path,
                                    size_t slot_index);
/* 执行已由 confirmation 页确认的 SAVE/LOAD。返回 1=成功、0=后端失败、
 * -1=不是存储动作或参数无效；result_fragment 写入 0 表示无已确认片段。 */
int fd2_field_game_execute_storage_action(
    fd2_field_game *game, fd2_field_system_action action,
    const char *path, size_t *result_fragment);

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

/* 1cff0／1d51d 的列表状态边界。begin/end成功0、拒绝-1；step沿用
 * 纯选择函数的-2/-1/0/1。step不关闭列表，宿主先完成关闭动画再end或
 * 进入目标选择。end只回COMMAND，不回滚临时移动或清pending。
 * 正式MAGIC由下述基础目标／提交能力门控制。 */
int fd2_field_game_begin_magic_list(fd2_field_game *game);
int fd2_field_game_magic_list_step(fd2_field_game *game,
                                   fd2_field_magic_list_key key,
                                   uint8_t *confirmed_id);
int fd2_field_game_end_magic_list(fd2_field_game *game);

/* field_player_item_command_execute @0x1bbdc（原登记同名）、
 * field_unit_inventory_select @0x1b932（原sub_1B932）的纯session边界。
 * 只支持stage0与密集已知ID库存；密集门是SDL支持范围，不是原版规则。
 * 正式COMMAND尚未分派此入口，ITEM仍禁用；菜单仅Use可选，Give/Equip/Drop
 * 明确禁用。confirm仅开列表0；list step的1只报告可确认，不提交handler。
 * begin/confirm/end成功0，拒绝-1；方向返回是否改变，拒绝-1；list step
 * 沿用-2/-1/0/1。step不关闭列表，宿主完成关闭动画再end。
 * 列表取消回ITEM_MENU0，子菜单取消回COMMAND首可用项，均不回滚临时
 * 移动、不清pending、不改单位/资源或RNG。COMMAND取消仍由原流程处理。 */
int fd2_field_game_begin_item_menu(fd2_field_game *game);
int fd2_field_game_select_item_menu_direction(
    fd2_field_game *game, fd2_field_command_direction direction);
int fd2_field_game_confirm_item_menu(fd2_field_game *game);
int fd2_field_game_item_list_step(fd2_field_game *game, fd2_field_item_list_key key);
int fd2_field_game_end_item_list(fd2_field_game *game);
int fd2_field_game_end_item_menu(fd2_field_game *game);

/* field_player_item_command_execute @0x1bbdc（原登记同名）的草药192
 * 普通area0目标session：用record+10/+12/+15，不套AI的+11或MAGIC门。
 * begin/cancel成功0、拒绝-1；legal为1合法/0当前格不合法/-1错误；
 * cycle为1消费一个槽（121只跳过focus）/0无候选/-1错误。
 * 只借move_range与target候选owner，不消费物品/MP/RNG，不提交动作；
 * cancel先focus到使用者，再返回ITEM_LIST0，保留临时移动和pending。 */
int fd2_field_game_begin_item_targeting(fd2_field_game *game);
int fd2_field_game_item_target_is_legal(const fd2_field_game *game);
int fd2_field_game_cycle_item_target(fd2_field_game *game);
int fd2_field_game_cancel_item_targeting(fd2_field_game *game);

/* field_player_magic_command_execute @0x1cff0 的普通area0选择；当前只支持
 * ID0/13。begin/cancel成功0、拒绝-1；legal为1合法/0当前格不合法/-1错误；
 * cycle为1消费一个候选槽（121只跳过focus）/0无候选/-1错误。
 * 这些接口不扣MP、不提交效果/acted/controller；方向可离开cast范围。 */
int fd2_field_game_begin_magic_targeting(fd2_field_game *game, uint8_t magic_id);
int fd2_field_game_magic_target_is_legal(const fd2_field_game *game);
int fd2_field_game_cycle_magic_target(fd2_field_game *game);
int fd2_field_game_cancel_magic_targeting(fd2_field_game *game);

/* stage0基础法术能力门：所有列表项都须为已支持的0(unit9)/13；
 * 不因低MP禁列表；正式COMMAND图标复用此门。resolve只从MAGIC_TARGETING提交，
 * 0成功/-1拒绝或已提交后冻结；拒绝预检不改业务状态/RNG，提交后不退款。
 * 当前只恢复无掉落、非升级逻辑，FIGANI/奖励提示/附加呈现RNG另行验收。 */
int fd2_field_game_magic_is_available(const fd2_field_game *game);
int fd2_field_game_resolve_player_magic(fd2_field_game *game);

/* M7.3 显式物理提交事务：重新计算 actor 当前移动范围及最优物理候选，
 * 仅当与传入 plan 完全一致时重建路径、提交移动，再复用
 * field_physical_exchange @0x1548e 的确定性逻辑核心。vga 可为空；非空时
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

/* field_command_menu_input @code0 0x77fc：四方向直接选择固定图标，禁用项
 * 不移动选择；确认分派已接入的攻击／基础法术／待机；COMMAND取消才
 * 复用移动快照回退，本身不消费RNG。法术真实等待消费另由play负责。 */
int fd2_field_game_refresh_commands(fd2_field_game *game);
int fd2_field_game_select_command_direction(
    fd2_field_game *game, fd2_field_command_direction direction);
int fd2_field_game_confirm_command(fd2_field_game *game);
int fd2_field_game_animate_command(fd2_field_game *game, fd2_vga *vga,
                                   int opening, uint32_t frame_delay_ms);

int fd2_field_game_open_detail(fd2_field_game *game, size_t unit_index);
void fd2_field_game_close_detail(fd2_field_game *game);
/* 上页借用资源的纯预检，供详情与法术列表共同使用；不画屏、不耗RNG。 */
int fd2_field_game_detail_upper_is_ready(
    const fd2_field_game *game, const fd2_field_unit *unit);
typedef void (*fd2_field_detail_phase_fn)(void *userdata,
                                           int opening, int phase);
int fd2_field_game_animate_detail(fd2_field_game *game, fd2_vga *vga,
                                  int opening,
                                  fd2_field_detail_phase_fn phase_callback,
                                  void *phase_userdata);
/* 17aed装备→未选中法术页的bottom-only过渡；两动画接口均为
 * 0完成/-1资源错误/1宿主退出。只改详情页，不提交玩法或controller。
 * 16c57等待／眨眼共享RNG的完整时序尚未恢复。 */
int fd2_field_game_animate_detail_magic_page(
    fd2_field_game *game, fd2_vga *vga,
    fd2_field_detail_phase_fn phase_callback, void *phase_userdata);
int fd2_field_game_update_move_preview(fd2_field_game *game);
int fd2_field_game_execute_move(fd2_field_game *game,
                                fd2_vga *vga,
                                uint32_t frame_delay_ms);
int fd2_field_game_finish_unit_action(fd2_field_game *game);
/* @0x117e7 的玩家返回后置：stage0结果门→13565→当前pending分派/清空。
 * complete用于成功command、selection取消与Enter详情（F2不调用）。
 * resume返回1=phase尚未返回，0=无续行或已完成，-1=首错冻结。 */
int fd2_field_game_complete_player_controller(fd2_field_game *game,
                                               size_t actor_index);
int fd2_field_game_resume_player_controller(fd2_field_game *game);
/* 无演出快速回归用：将当前阵营全部标记 acted 后跳过该 phase。
 * 正式玩家菜单使用 end_player_turn，不以本接口模拟原版恢复门。 */
int fd2_field_game_end_active_phase(fd2_field_game *game);
/* 玩家确认结束回合：执行已确认的未行动恢复，再进入 side 1。 */
int fd2_field_game_end_player_turn(fd2_field_game *game);
/* field_empty_focus_menu_execute @0x16f55（原登记同名）的批量行军。
 * 固定调用时的绝对focus anchor，raw顺序逐actor提交，不保存/回滚批次。
 * 0=全部提交并进入side1；1=宿主关闭（保留已提交事实）；-1=失败冻结。
 * 调用方负责FDTXT418提示与输入隔离；这里只开放正式stage0。 */
int fd2_field_game_march(fd2_field_game *game, fd2_vga *vga,
                         uint32_t frame_delay_ms);
int fd2_field_game_process_automatic_action(fd2_field_game *game);
/* clean_background只由March宿主续行传1；普通END/AI调用传0。
 * 它只选择本次呈现，不进入session、不改变共享pending或动作策略。 */
int fd2_field_game_process_automatic_action_visual(
    fd2_field_game *game, fd2_vga *vga, uint32_t frame_delay_ms,
    int clean_background);
size_t fd2_field_game_remaining_units(const fd2_field_game *game,
                                      uint8_t side);
fd2_field_battle_result fd2_field_game_battle_result(
    const fd2_field_game *game);
int fd2_field_game_compute_move_range(const fd2_field_game *game,
                                      size_t mover_index,
                                      fd2_field_path_result *result);
size_t fd2_field_game_group_count(const fd2_field_game *game, uint8_t group);

/* 战场全屏效果：分别复现 field_actor_group_flash @0x1c2da、
 * field_earthquake_effect @0x21527 与 field_stage_transition_effect
 * @0x24618。prepare 与 play 分离，保证先完整构造再提交演出。
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
