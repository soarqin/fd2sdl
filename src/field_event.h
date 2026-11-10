#ifndef FD2_FIELD_EVENT_H
#define FD2_FIELD_EVENT_H

#include <stddef.h>
#include <stdint.h>

#include "field.h"
#include "field_unit.h"
#include "tile.h"
#include "static_flow.h"

typedef struct {
    size_t slot;
    uint8_t action;
    uint8_t phase;
} fd2_field_turn_event_match;

/* 复现 field_turn_event_check @0x1a813 的纯查询部分：只返回
 * turn==turn_number 且 phase 匹配的记录，不执行回调表。 */
size_t fd2_field_turn_events_find(
    const fd2_field_metadata *metadata,
    uint32_t turn_number,
    uint8_t phase,
    fd2_field_turn_event_match *out,
    size_t capacity);

/* 复现 field_cell_event_lookup @0x13a44：地图 cell 第 2 字节低 5 位
 * 是 1-based lookup ID；地形 attr.flags 的 0x60 位会禁止触发。
 * 玩家移动每步查询 match_arg 0；玩家行动及 AI common tail 查询 1。
 * 返回 1 表示命中，0 表示无事件，-1 表示输入数据无效。 */
int fd2_field_cell_event_find(
    const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,
    const fd2_field_metadata *metadata,
    int x, int y,
    uint8_t match_arg,
    uint8_t *event_code,
    size_t *slot);

/* 复现 field_movement_script_play @0x1366a 的无演出状态提交部分。
 * 先完整校验脚本与 actor 索引，失败时不修改单位表。mode 最高位为 1
 * 时只提交朝向；否则按低 7 位步数移动，并将步态复位为静止。 */
int fd2_field_movement_script_apply(fd2_field_units *units,
                                    const uint8_t *script,
                                    size_t script_size);

/* 原0x627d8的全106条计数流。该getter不解除任何stage/事件支持门。
 * mode80/raw5保留原朝向与cache跨class语义，不规范化为普通四方向。
 * 返回0并提供只读字节流，
 * 未知 script_id 返回 -1，不推进演出。 */
int fd2_field_movement_script_get(uint8_t script_id,
    const uint8_t **script, size_t *script_size);
int fd2_field_stage0_movement_script_get(uint8_t script_id,
                                         const uint8_t **script,
                                         size_t *script_size);

/* G4有限事件谓词与写点。只借原owner；分支、ACK及先后顺序由冷图。
 * cmp不是可执行表达式；每个调用只读一个已确认字段/有界记录范围。 */
#define FD2_FIELD_EVENT_STATE_SIZE 32u
typedef enum {
    FD2_EVENT_EQ, FD2_EVENT_NE, FD2_EVENT_LT,
    FD2_EVENT_LE, FD2_EVENT_GT, FD2_EVENT_GE
} fd2_field_event_compare;
typedef enum {
    FD2_EVENT_ACTOR_HIDDEN, FD2_EVENT_ACTOR_SIDE, FD2_EVENT_ACTOR_TEXT,
    FD2_EVENT_ACTOR_ID, FD2_EVENT_ACTOR_HP, FD2_EVENT_ACTOR_HP_MAX,
    FD2_EVENT_ACTOR_ACTED, FD2_EVENT_ACTOR_INVENTORY_COUNT
} fd2_field_event_actor_field;
int fd2_field_event_compare_unsigned(uint32_t value,
    fd2_field_event_compare comparison, uint32_t expected);
/* 20765/24754等原cmp DWORD+signed Jcc；不把turn BYTE查询的无符号义
 * 推广到这些条件。按DWORD位型显式转有符号，避免实现定义的C转换。 */
int fd2_field_event_compare_turn(uint32_t value,
    fd2_field_event_compare comparison, int32_t expected);
int fd2_field_event_state_compare(const uint8_t state[FD2_FIELD_EVENT_STATE_SIZE],
    size_t index, fd2_field_event_compare comparison, uint8_t expected);
/* SET或BYTE模加；仅写first..last，不清整个32B或未知尾。 */
int fd2_field_event_state_write(uint8_t state[FD2_FIELD_EVENT_STATE_SIZE],
    size_t first, size_t last, uint8_t value, int add);
int fd2_field_event_state_from_count(uint8_t state[FD2_FIELD_EVENT_STATE_SIZE],
    size_t index, const fd2_field_units *units, uint8_t subtract);
/* 34dd0/35bee/...只写目标slot的turn BYTE，action/phase原样保留。 */
int fd2_field_event_schedule_turn(fd2_field_metadata *metadata, size_t slot,
    uint32_t now, uint8_t delta);
int fd2_field_event_actor_compare(const fd2_field_units *units, size_t actor,
    fd2_field_event_actor_field field, fd2_field_event_compare comparison,
    uint32_t expected);
int fd2_field_event_hidden_count(const fd2_field_units *units,
    size_t first, size_t last, size_t *count);
/* 33499/24bde扫描完整持久raw前缀，包括raw0/hidden，不按已部署成员筛选。 */
int fd2_field_event_roster_has_text(const fd2_field_unit *roster,
    size_t count, uint8_t text_id);
/* 2aedb：先数8槽flag80，再读raw[0..count-1]的ID；不跳洞、不归一stale。
 * 1找到/0未找到/-1坏actor，只有找到才改slot。 */
int fd2_field_event_item_find(const fd2_field_units *units, size_t actor,
    uint8_t item, size_t *slot);
int fd2_field_event_item_append(fd2_field_units *units, size_t actor, uint8_t item);
int fd2_field_event_item_remove(fd2_field_units *units, size_t actor, uint8_t item);
/* 240fa的item-major/actor-minor规则：每种item在每actor最多计一次，
 * 相同item可跨actor重复计；consume每对只移第一个prefix命中，不重算装备。 */
int fd2_field_event_item_occurrences(const fd2_field_units *units,
    size_t first_actor, size_t last_actor, uint8_t first_item, uint8_t last_item,
    size_t *count);
int fd2_field_event_item_consume(fd2_field_units *units,
    size_t first_actor, size_t last_actor, uint8_t first_item, uint8_t last_item);
/* 35854读取map cell完整byte2作为5项caller局部索引，不截为低5bit。
 * 4条nested descriptor是352ca/35346/355f0/35730的只读原3B局部。 */
int fd2_field_event_cell_item(const fd2_field_map *map, int x, int y, uint8_t *item);
int fd2_field_event_child_descriptor(size_t index, const uint8_t **descriptor);
/* 4df4c原地图初始化写集：terrain低10bit/lookup低5bit，byte3=ff。
 * 只处理已经归当前owner的cell数组；不读资源、不做背景/镜头或完成格处理。 */
int fd2_field_event_map_reset_cells(fd2_field_map *map);
/* 35f10的HP写循环；first>=count时为空循环。统一败亡整理必须在下一
 * 冷节点调用，不能仅隐藏actor、在此偷偷发放掉落或重算战斗尾。 */
int fd2_field_event_hp_zero_tail(fd2_field_units *units, size_t first);
/* 已证动态来源形式，不组合表达式、不拥有变量；返回原DWORD位型。
 * 35638是AL内减/加并MOVZX，356b3/35833直接传完整DWORD，不能混淆。 */
typedef enum {
    FD2_EVENT_VALUE_TURN_DWORD=0,
    FD2_EVENT_VALUE_TURN_BYTE,
    FD2_EVENT_VALUE_TURN_SIGNED_HALF,
    FD2_EVENT_VALUE_TURN_PAIR_EVEN_BYTE,
    FD2_EVENT_VALUE_TURN_PAIR_ODD_BYTE,
    FD2_EVENT_VALUE_STATE16,
    FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR,
    FD2_EVENT_VALUE_STATE16_FIRST_ACTOR,
    FD2_EVENT_VALUE_STATE16_SECOND_ACTOR,
    FD2_EVENT_VALUE_STATE16_STORY
} fd2_field_event_value_source;
int fd2_field_event_source_value(fd2_field_event_value_source source, uint32_t turn,
    const uint8_t state[FD2_FIELD_EVENT_STATE_SIZE], uint32_t *value);
/* 3623c仅一次共享RNG；先验证base..base+2，坏输入不得调用RNG或写flags。 */
typedef uint32_t (*fd2_field_event_rng_fn)(void *userdata);
int fd2_field_event_mark_random_pair(fd2_field_units *units, size_t base,
    fd2_field_event_rng_fn rng, void *userdata);

/* 仅作为原event/loot owner内的有界续态字段，不独立拥有session或SAV。
 * 最多一个父callback、一个子loot；二次压入拒绝，子失败不倒回已提交写点。 */
typedef struct {
    const fd2_static_flow *flow;
    int16_t pc;
    uint8_t actor;
    uint8_t active;
    uint8_t child_wait;
    uint8_t item_id; /* 35854原caller局部；不得借可能被子loot覆盖的notice。 */
    uint8_t has_item;
    uint8_t from_loot; /* 当前callback返回后仅续父descriptor PC，不重启entry。 */
    /* 原callback返回点：控制器/turn/loot继续原调用者，不是事件操作码栈。 */
    uint8_t completion;
    uint8_t next_slot;
    uint8_t waiting_legacy; /* 前条旧Scene notice呈现完才执行当前entry。 */
    int16_t march_anchor_x, march_anchor_y;
    uint32_t march_frame_ms;
    void *march_vga; /* 仅借本次Game宿主，Load/close先清续态，不拥有设备。 */
} fd2_field_event_resume;
enum {
    FD2_EVENT_COMPLETE_NONE=0,
    FD2_EVENT_COMPLETE_TURN,
    FD2_EVENT_COMPLETE_PLAYER_PENDING,
    FD2_EVENT_COMPLETE_AI_PENDING,
    FD2_EVENT_COMPLETE_LOOT_PENDING,
    FD2_EVENT_COMPLETE_MARCH
};
int fd2_field_event_child_begin(fd2_field_event_resume *continuation,
    const fd2_static_flow *flow, int parent_pc, size_t actor, size_t actor_count);
int fd2_field_event_child_finish(fd2_field_event_resume *continuation,
    int succeeded, int *parent_pc, size_t *actor);

#endif /* FD2_FIELD_EVENT_H */
