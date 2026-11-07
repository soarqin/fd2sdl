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
#include "field_battle.h"
#include "field_command.h"
#include "field_system_menu.h"
#include "field_manual_slot.h"
#include "field_handoff.h"
#include "field_info.h"
#include "field_item.h"
#include "field_magic.h"
#include "field_path.h"
#include "field_unit.h"
#include "field_unit_base.h"
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
    FD2_FIELD_INTERACTION_ITEM_TARGETING,
    FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING,
    FD2_FIELD_INTERACTION_ITEM_GIVE_LIST,
    FD2_FIELD_INTERACTION_ITEM_TELEPORTING
} fd2_field_interaction;

/* field_unit_detail_present @0x17aed（原sub_17AED）：装备页任意键后，有法术才显示第二页。 */
typedef enum {
    FD2_FIELD_DETAIL_EQUIPMENT = 0,
    FD2_FIELD_DETAIL_MAGIC
} fd2_field_detail_page;

typedef struct fd2_field_game fd2_field_game;

/* field_unit_experience_reward_execute @0x1e292（原sub_1E292）与
 * field_phase_status_tick @0x1a866（原登记同名）
 * 的B4单份进展续态，不是通用事件队列；NONE为零初始化。 */
typedef enum {
    FD2_FIELD_PROGRESS_NONE = 0,
    FD2_FIELD_PROGRESS_GAIN,
    FD2_FIELD_PROGRESS_LEVEL,
    FD2_FIELD_PROGRESS_GROWTH,
    FD2_FIELD_PROGRESS_LEARN,
    FD2_FIELD_PROGRESS_CLOSE,
    FD2_FIELD_PROGRESS_STATUS
} fd2_field_progress_kind;

typedef struct {
    fd2_field_progress_kind kind;
    size_t actor_index;
    uint16_t fragment; /* 488/489/490..494/587；STATUS为481..487。 */
    uint32_t value;    /* gain、下一等级、成长delta或毒伤；其他0。 */
    uint8_t magic_id;  /* LEARN使用；其他0。 */
    uint8_t wait_ticks; /* CLOSE=11、STATUS=10，其余由文本token决定。 */
} fd2_field_progress_notice;

/* 正式play注册必需资源预检；无present/键消费/RNG/业务写入。
 * core在首动作RNG前枚举可能奖励路径，状态批次亦在写入前枚举。
 * 返回0资源已验证，非0拒绝。未注册的纯core只验证逻辑数据，不宣称UI资源验收。 */
typedef int (*fd2_field_progress_preflight_fn)(
    const fd2_field_game *game, const fd2_field_progress_notice *notice,
    void *userdata);

typedef enum {
    FD2_FIELD_PROGRESS_RESUME_NONE = 0,
    FD2_FIELD_PROGRESS_RESUME_PLAYER,
    FD2_FIELD_PROGRESS_RESUME_AI_DIRECT,
    FD2_FIELD_PROGRESS_RESUME_AI_SCHEDULER,
    FD2_FIELD_PROGRESS_RESUME_PHASE
} fd2_field_progress_resume;

typedef struct {
    fd2_field_progress_notice notice;
    uint32_t total;
    fd2_field_unit_level_growth level;
    uint8_t growth_cursor;
    uint8_t learn_cursor;
    fd2_field_progress_resume resume;
    size_t resume_actor;
    uint8_t ai_common_tail;
    /* STATUS逐raw毒伤→finalize→逐raw/byte计时；不整批提前改写。 */
    uint8_t status_side;
    uint8_t status_pass;
    uint8_t status_byte;
    size_t status_cursor;
} fd2_field_progress;

/* field_turn_cycle_run @0x1a30b 的同步音乐请求；-1只请求异步fade-stop。
 * App持有音频设备，core不等待、不消费RNG，也不由此生成输入事件。 */
typedef enum {
    FD2_FIELD_MUSIC_PHASE_STOP = 0,
    FD2_FIELD_MUSIC_PLAYER,
    FD2_FIELD_MUSIC_ENEMY
} fd2_field_music_reason;
typedef void (*fd2_field_music_request_fn)(size_t stage, int track,
    fd2_field_music_reason reason, void *userdata);

/* field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D） 与field_special_cell_execute @0x190ac（原sub_190AC）的有限业务续态。
 * MESSAGE/CHOICE借同一底框；STORY是当前stage文本，不是FDTXT[0]。
 * wait_ms是已证显式delay，真正等键/开关动画由play完成后reply。 */
typedef enum {
    FD2_FIELD_LOOT_NONE = 0,
    FD2_FIELD_LOOT_MESSAGE,
    FD2_FIELD_LOOT_CHOICE,
    FD2_FIELD_LOOT_INVENTORY,
    FD2_FIELD_LOOT_STORY,
    FD2_FIELD_LOOT_DELAY,
    FD2_FIELD_LOOT_FIELD_EVENT
} fd2_field_loot_kind;
typedef struct {
    fd2_field_loot_kind kind;
    size_t actor_index;
    uint16_t fragment;
    uint32_t value;
    uint16_t wait_ms;
    uint8_t item_id;
    uint8_t old_item_id;
    uint8_t open_box;
    uint8_t append_line;
    uint8_t close_box;
    uint8_t wait_key;
} fd2_field_loot_notice;
typedef enum {
    FD2_FIELD_LOOT_ACK = 0,
    FD2_FIELD_LOOT_YES,
    FD2_FIELD_LOOT_NO,
    FD2_FIELD_LOOT_CANCEL,
    FD2_FIELD_LOOT_SLOT
} fd2_field_loot_reply;
typedef int (*fd2_field_loot_preflight_fn)(const fd2_field_game *game,
    const fd2_field_loot_notice *notice, void *userdata);
/* 固定容量来自原active上限，没有资源/heap所有权；成功Load可直接清零。
 * step/source/resume仅此两原函数续态内部使用，不进入SAV。 */
typedef struct {
    fd2_field_loot_notice notice;
    uint8_t descriptors[FD2_FIELD_MAX_UNITS][3];
    size_t count;
    size_t cursor;
    size_t actor_index;
    size_t action_actor;
    uint64_t raw_reward;
    uint8_t source;
    uint8_t step;
    uint8_t action_kind;
    uint8_t cell_index;
    uint8_t cell_flags;
    fd2_field_progress_resume resume;
    uint8_t ai_common_tail;
    uint8_t ai_after_magic;
    uint8_t text_ready;
} fd2_field_loot;

/* 正式战场 session。
 *
 * 集中管理 stage 资源、单位、镜头、移动查询及后续交互状态。单位记录
 * 本身使用 fd2_field_unit 的原版 0x50 字节布局；移动 profile 与预算已
 * 确认，职业和其他战斗属性仍保留原始字节，不在 session 层猜测解释。
 */
struct fd2_field_game {
    /* 借用App/验证caller的FDOTHER；font等保留其raw视图，须晚于game关闭。 */
    const fd2_archive *fdother;
    fd2_archive field_archive;
    fd2_archive fdshap_archive;
    fd2_archive dato_archive;
    fd2_archive fdtxt_archive;
    fd2_archive figani_archive; /* 费用标记属于逻辑参数；Play借用同一只读归档。 */
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
    /* 原版DS:0x3f56归城镇准备页；sub_4E809 @0x4e809按stage-1取31B行。
     * 当前stage0活动session不绑定准备记录，本旁路值为0，不写SAV。 */
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
     * 玩家物品按各子操作的当前数据支持门开放。 */
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
     * Use/Give/Equip/Drop，disabled由各操作当前支持门计算。 */
    int item_list_selected;
    /* Give满槽列表另选recipient raw槽；source仍由selected_unit/item_list_selected拥有。
     * 53C53只表示本次18890内已转交，最终COMMAND取消仍须提交acted/lookup。
     * 临时索引/标志不写SAV，成功Load与新行动入口清零。 */
    int item_give_recipient;
    int item_give_selected;
    uint8_t item_transfer_committed;
    /* field_cell_selection_execute @0x115b6 的本次target局部轮询，
     * 物理/MAGIC/ITEM_TARGETING互斥借用候选buffer；id只归MAGIC。
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
    /* 借用Play的同步演出service；不拥有context，不写入SAV。 */
    fd2_field_physical_present_fn physical_present;
    void *physical_present_userdata;
    fd2_field_magic_present_fn magic_present;
    void *magic_present_userdata;
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
    /* 205da清完整32B，active Load/Save整块恢复；metadata action仍只有16项。 */
    uint8_t cell_action_completed[FD2_SAVE_BATTLE_CELL_STATE_SIZE];
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
    /* 原51A83普通cursor模式0/1；保留显式END后AI的最终写点供结算继承。
     * 不表示目标overlay的2..6，不进入SAV；Scene只读借用。 */
    uint8_t ordinary_display_mode;
    int info_panel_right;
    fd2_field_battle_result battle_result;
    int ready;
    fd2_field_pending_cell_event pending_cell_event;
    /* field_controller_input @0x117e7 在13565整轮phase之后续行；只保存
     * 原玩家actor，不保存共享pending。waiting为0即无上下文，actor无效。
     * 成功active Load清除，失败Load保留；均不写入SAV。 */
    size_t player_post_actor;
    uint8_t player_post_waiting_phase;
    /* 原53EC8只在已证调用点清/写；不进SAV，成功Load/END保留它。 */
    uint32_t experience_value;
    /* 原53AE1动态数字；GAIN/LEVEL/GROWTH/毒伤/拾取按原写点共用，Load保留。 */
    uint32_t message_number;
    fd2_field_loot loot;
    fd2_field_loot_preflight_fn loot_preflight;
    void *loot_preflight_userdata;
    fd2_field_music_request_fn music_request;
    void *music_request_userdata;
    fd2_field_progress progress;
    fd2_field_progress_preflight_fn progress_preflight;
    void *progress_preflight_userdata;
};

int fd2_field_game_set_music_request(fd2_field_game *game,
    fd2_field_music_request_fn request, void *userdata);
int fd2_field_game_set_loot_preflight(fd2_field_game *game,
    fd2_field_loot_preflight_fn preflight, void *userdata);
/* peek同progress的1/0/-1；reply在完整当前呈现/关闭后调用一次。
 * pending时禁止保存和普通行动；取消库存选择不撤销先前已提交效果。 */
int fd2_field_game_loot_peek(const fd2_field_game *game,
    fd2_field_loot_notice *notice);
/* MESSAGE文字成功且宿主关闭已优先检查后、portrait0/等键之前调用。
 * 432/422/429此时才append/置completed；其他MESSAGE只标记文本已完成。
 * 0成功/-1拒绝，重复拒绝；MESSAGE最终ACK必须先经过此边界。 */
int fd2_field_game_loot_text_ready(fd2_field_game *game);
int fd2_field_game_loot_reply(fd2_field_game *game,
    fd2_field_loot_reply reply, int raw_slot);

/* 0成功/-1拒绝；注册只读资源校验，不变更当前贡献或进展。 */
int fd2_field_game_set_progress_preflight(fd2_field_game *game,
    fd2_field_progress_preflight_fn preflight, void *userdata);
/* peek=1有当前notice、0无、-1数据错误；不改变单位/RNG/输出失败值。
 * ack=0消费当前notice；可能产生下一notice或续原controller/AI/phase。
 * GROWTH ack之后才加WORD；LEARN notice之前已OR位；CLOSE仅在宿主
 * 等11tick并关闭后ack，届时才写EX/清53EC8。STATUS宿主等10tick/关闭后ack。
 * 主循环必须先呈现已有turn事件，再处理progress，pending时不调度/读普通动作。 */
int fd2_field_game_progress_peek(const fd2_field_game *game,
    fd2_field_progress_notice *notice);
int fd2_field_game_progress_ack(fd2_field_game *game);
/* turn演出成功清deferred后调用，才开始本phase的状态写；1忙/0无/-1错。
 * 等待turn时peek为0但progress.resume=PHASE，仍不能调度或保存。 */
int fd2_field_game_progress_resume(fd2_field_game *game);
/* controller117e7选到可见actor时清53EC8；Enter详情调用，F2不调用。
 * 普通行动confirm_cursor会自行调用；不得给Load/END补造清零。 */
int fd2_field_game_begin_player_controller(fd2_field_game *game,
    size_t actor_index);

/* 当前只允许打开已验证初始 roster 的 stage 0。资源容器和渲染接口按
 * 通用 stage session 组织，后续确认其他关卡初始化后可直接扩展。 */
int fd2_field_game_open(fd2_field_game *game,
                        fd2_vga *vga,
                        const fd2_archive *fdother,
                        size_t stage);
/* 1088d/10010共用资源准备；只支持stage0/1/2，不构造单位、推进phase、
 * 写VGA/palette/RNG。输出须是新owner（先close旧Game），失败后可安全close。
 * fdother借用至game关闭；失败释放已取得资源。 */
int fd2_field_game_open_resources(fd2_field_game *game,
    const fd2_archive *fdother, size_t stage);
/* 26152准备owner出阵到32d18：完整persistent表→5部署槽、group1/2和
 * scripts9..12业务终态。无输入/演出；App播放开场后handoff镜头/焦点。
 * stage1只支持1..16 roster部署，超过16所需2af28尚未支持，拒绝而不截断。 */
int fd2_field_game_prepare_stage1(fd2_field_game *game,
    const fd2_archive *fdother, const fd2_save_manual_state *state);
/* field_stage2_entry_play @0x32e8c：仅已确认的stage2专用入口；
 * 不以stage++或stage1部署模板替代，不改传入的准备owner。 */
int fd2_field_game_prepare_stage2(fd2_field_game *game,
    const fd2_archive *fdother, const fd2_save_manual_state *state);
/* field_stage1_victory_play @0x22f37（原sub_22F37）的只读结算投影：
 * 198奖励→group4→scripts14..16。
 * stage0返回当前units；stage1完整预检后才写out，无Game/RNG/归档owner写入。 */
int fd2_field_game_settlement_project_units(const fd2_field_game *game,
    fd2_field_units *out);
/* 准备Load的资源/记录纯预检，count0可显示；不代表可以出阵。 */
int fd2_field_game_validate_manual_state(const fd2_save_manual_state *state);
/* 22ef6/22f37：完整结束前只prepare shadow；结束后commit 11506同步及下一准备stage。
 * out整张0xa00；stage1先追加8号持久成员，不带RNG或53EC8，不改source。 */
int fd2_field_game_settlement_prepare(const fd2_field_game *game,
    fd2_save_manual_state *out);
int fd2_field_game_settlement_commit(fd2_field_game *game,
    fd2_save_manual_state *out);
/* field_completed_chest_tiles_apply @0x12263（原sub_12263）：对base map按完整completed32B重建flags20地形。
 * 先验证所有输入/改变后的帧，失败不写map；不写resources/metadata/completed。 */
int fd2_field_game_apply_completed_cells(const fd2_field_game *resources,
    fd2_field_map *map, const uint8_t completed[FD2_SAVE_BATTLE_CELL_STATE_SIZE]);
void fd2_field_game_close(fd2_field_game *game);

/* 将过场的动态坐标、朝向、隐藏状态和镜头应用到已经从 FDFIELD 建立的
 * 正式单位表；阵营、模板和战斗属性仍保留 session 中的正式记录。 */
int fd2_field_game_apply_handoff(fd2_field_game *game,
                                 const fd2_field_handoff *handoff);

/* 1297d/11eee视觉时钟唯一写口；仅idle/terrain相位及其last锚点四字段。
 * 演出显式借Game调用，不能用普通tick代替而推进AI/菜单/提交业务。 */
int fd2_field_game_tick_visual(fd2_field_game *game, uint64_t now_ms);
void fd2_field_game_tick(fd2_field_game *game, uint64_t now_ms);
/* 16f55/19df7系统notice的同一纯页面：首提示、稳态和结果共用。
 * 原资源全预检；失败不写VGA，成功不改Game/RNG/palette或present。 */
int fd2_field_game_render_system_notice(const fd2_field_game *game,
    fd2_vga *vga, size_t fragment);
void fd2_field_game_render(fd2_field_game *game, fd2_vga *vga);
/* sub_1B1E7 @0x1b1e7（原登记 sub_1B1E7）的地图／actor 背景；
 * 不绘光标、范围或任何UI，也不改变信息面板的左右侧状态。 */
void fd2_field_game_render_background(const fd2_field_game *game, fd2_vga *vga);
/* 原11cac(1)战斗返回页：当前terrain/actor/普通cursor/info，不画旧范围/modal。
 * 纯投影，不提交panel-side、Game、RNG或present；normal/error退出分别由Play服务编排。 */
void fd2_field_game_render_battle_return(const fd2_field_game *game, fd2_vga *vga);
/* 原22046两次LUT之间重画actor/前景；312×192视区相对中心，半径1..4096。
 * 不改Game/owner/RNG/present，非法参数无写入；复用field_visual的唯一几何实现。 */
int fd2_field_game_render_transition_mask(const fd2_field_game *game, fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius);
/* 1cff0/15311的51a83=0页：当前地图/actor/info，无cursor/range/modal；
 * 纯绘制，不另存时钟、不修改Game或RNG。 */
void fd2_field_game_render_effect_background(const fd2_field_game *game, fd2_vga *vga);
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
/* 115b6的Z/小键盘5按raw候选回绕；121占槽但不移动焦点，不消费RNG。 */
int fd2_field_game_cycle_attack_target(fd2_field_game *game);
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
 * 只支持stage0/1与密集已知ID库存；密集门是SDL支持范围，不是原版规则。
 * 正式COMMAND由item_is_available限制效果能力；低层纯session不混此门。
 * 正式菜单按各自能力开放Use/Give/Equip/Drop；confirm借同一
 * ITEM_LIST与command_selected区分mode1/0，不缓存槽ID。
 * list step的1只报告可确认，不提交handler。
 * begin/confirm/end成功0，拒绝-1；方向返回是否改变，拒绝-1；list step
 * 沿用-2/-1/0/1。step不关闭列表，宿主完成关闭动画再end。
 * Use取消回ITEM首可用项，Drop取消亦重算派生后返回；子菜单取消回COMMAND。
 * 均不回滚临时移动、不清pending或消费RNG；COMMAND取消仍由原流程处理。 */
int fd2_field_game_begin_item_menu(fd2_field_game *game);
int fd2_field_game_select_item_menu_direction(
    fd2_field_game *game, fd2_field_command_direction direction);
int fd2_field_game_confirm_item_menu(fd2_field_game *game);
int fd2_field_game_item_list_step(fd2_field_game *game, fd2_field_item_list_key key);
int fd2_field_game_end_item_list(fd2_field_game *game);
int fd2_field_game_end_item_menu(fd2_field_game *game);

/* field_player_item_command_execute @0x1bbdc（原登记同名）的纯Drop提交：
 * 仅ITEM_LIST/Drop3；确认移raw槽，取消由end_item_list分派，二者均按
 * field_unit_combat_stats_recompute @0x1b750（原登记同名）重算shadow后
 * 才交接actor。余项回ITEM首可用项，删空回COMMAND首可用项；重复拒绝。
 * 成功0、拒绝-1。无acted/目标/收尾/RNG；失败actor与UI owner均不变。
 * 宿主须先完整关闭列表；正式入口另重验Drop等待/数据能力。 */
int fd2_field_game_resolve_item_drop(fd2_field_game *game);

/* field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）、
 * field_unit_equipment_slot_set @0x1c142（原sub_1C142）与
 * field_unit_can_equip_item @0x1c1c3（原sub_1C1C3）
 * 的装备应用，支持已确认profile0..28及ITEM_LIST/command_selected2。
 * 兼容项在80B shadow按原分组写flag，再验证dense-known结果并重算后交接；
 * 成功1仍留原页/同raw索引（已装备重选亦为1），不兼容0且不重算，数据拒绝-1。
 * 失败actor/UI无写；无目标/消耗/acted/MP/EX/移动/pending/RNG。
 * EQT取消由end_item_list返回菜单且不重算；宿主复用留页等待/重绘。 */
int fd2_field_game_equip_item(fd2_field_game *game);

/* 正式ITEM能力查询：Use/Give/Equip/Drop任一可用即开放菜单；可见库存须密集
 * 且ID已知，Use支持表中code5..24，Drop/Give须全8槽可完成派生重算。
 * code0武器/装备可看列表，确认仍留页。要求stage0/1可行动玩家及共享RNG，
 * 不套MP/+27/移动后禁法/EX99门。查询不写状态或消费RNG。
 * COMMAND、ITEM_MENU与ITEM_TARGETING正式confirm均重新检查此门；
 * confirm_cursor成功返回原actor index，非法格/拒绝/错误返回-1。 */
int fd2_field_game_item_is_available(const fd2_field_game *game);
int fd2_field_game_item_use_is_available(const fd2_field_game *game);
int fd2_field_game_item_drop_is_available(const fd2_field_game *game);
int fd2_field_game_item_give_is_available(const fd2_field_game *game);

/* field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）的EQT等待查询：已证职业支持门
 * 加共享RNG有效性；不写状态/RNG。正式确认重验此门。 */
int fd2_field_game_item_equip_is_available(const fd2_field_game *game);

/* field_player_item_command_execute @0x1bbdc的Give路径：range最大1/最小1、
 * filter3；目标可HP0/acted，hidden不收。source槽不缓存ID，满槽另选recipient。
 * begin/resolve/cancel成功0、数据拒绝-1；list step沿用-2/-1/0/1。
 * resolve按当前owner完成空槽转交或交换，不acted；只重算sender，保留recipient派生。
 * 任一Give取消也重算sender并回菜单；最终COMMAND取消见53C53合同。 */
int fd2_field_game_begin_item_give(fd2_field_game *game);
int fd2_field_game_item_give_target_is_legal(const fd2_field_game *game);
int fd2_field_game_cycle_item_give_target(fd2_field_game *game);
int fd2_field_game_resolve_item_give(fd2_field_game *game);
int fd2_field_game_item_give_list_step(fd2_field_game *game, fd2_field_item_list_key key);
int fd2_field_game_cancel_item_give(fd2_field_game *game);

/* field_player_item_command_execute @0x1bbdc（原登记同名）的物品
 * 目标session：用record+10/+12/+15，不套AI的+11或MAGIC门。
 * begin/cancel成功0、拒绝-1；legal为1合法/0当前格不合法/-1错误；
 * cycle为1消费一个槽（121只跳过focus）/0无候选/-1错误。
 * 只借move_range与target候选owner，不消费物品/MP/RNG，不提交动作；
 * cancel先focus到使用者，再返回ITEM_LIST0，保留临时移动和pending。 */
int fd2_field_game_begin_item_targeting(fd2_field_game *game);
int fd2_field_game_item_target_is_legal(const fd2_field_game *game);
int fd2_field_game_cycle_item_target(fd2_field_game *game);
int fd2_field_game_cancel_item_targeting(fd2_field_game *game);

/* field_item_use_effect_execute @0x20c6f（原sub_20C6F）的code5..24：
 * 当前cast/area/filter、原槽、数据及尾链先预检，再按分支执行/移槽。
 * code23首次合法确认只进入目的地选择，最终确认才MP u16扣费并移动。
 * 返回0成功/-1预检拒绝或提交后冻结；角色EX不写，不借MAGIC奖励。
 * 仅恢复无未知掉落/必需handler的逻辑；音画/附加RNG仍另行验收。 */
int fd2_field_game_resolve_player_item(fd2_field_game *game);

/* field_player_magic_command_execute @0x1cff0 的普通area0选择；当前只支持
 * ID0/13。begin/cancel成功0、拒绝-1；legal为1合法/0当前格不合法/-1错误；
 * cycle为1消费一个候选槽（121只跳过focus）/0无候选/-1错误。
 * 这些接口不扣MP、不提交效果/acted/controller；方向可离开cast范围。 */
int fd2_field_game_begin_magic_targeting(fd2_field_game *game, uint8_t magic_id);
int fd2_field_game_magic_target_is_legal(const fd2_field_game *game);
int fd2_field_game_cycle_magic_target(fd2_field_game *game);
int fd2_field_game_cancel_magic_targeting(fd2_field_game *game);

/* stage0/1基础法术能力门：列表可含其他已知项，逐项仅开放0(unit9)/13；
 * 不因低MP禁列表；正式COMMAND图标复用此门。resolve只从MAGIC_TARGETING提交，
 * 0成功/-1拒绝或已提交后冻结；拒绝预检不改业务状态/RNG，提交后不退款。
 * 成长奖励由progress分段提交，已知掉落由loot接续；FIGANI附加呈现RNG另验。 */
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
 * 不移动选择；确认分派攻击／基础法术／物品／待机；COMMAND取消无Give
 * 提交时才回退移动，53C53非零则走acted/lookup/controller，不回滚库存。法术真实等待消费另由play负责。 */
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
/* @0x117e7 的玩家返回后置：本stage结果门→13565→当前pending分派/清空。
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
 * 调用方负责FDTXT418提示与输入隔离；复用已恢复stage0/1的phase/event门。 */
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
