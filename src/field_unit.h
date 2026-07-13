#ifndef FD2_FIELD_UNIT_H
#define FD2_FIELD_UNIT_H

#include <stddef.h>
#include <stdint.h>

#include "field.h"
#include "map_sprite.h"
#include "vga.h"

/* 战场单位运行时记录。
 *
 * 逆向依据：DAT_00003a45 是由 0x50 字节记录组成的战场单位表；
 * map_scene_render_actors @0x41db2、map_actor_blit_24x24 @0x42c34、
 * field_visible_actor_find_by_text_id @0x37e74 分别读取坐标、精灵、
 * 隐藏标志及文本编号。FDFIELD stage metadata 的 26 字节模板与
 * placement 已确认按 unit ID 和出现次序匹配；原版 stage constructor
 * 将 26 字节模板重排为身份、装备和 AI 字段，再按等级派生基础属性。
 * field_ai_unit_execute @0x38cb3 进一步确认 record +0x34 低半字节为
 * behavior selector，并在入口读取 +0x35/+0x36 作为模式参数。
 */
#define FD2_FIELD_UNIT_RECORD_SIZE 0x50
#define FD2_FIELD_MAX_UNITS 64
#define FD2_FIELD_UNIT_NO_TEMPLATE UINT16_MAX
#define FD2_FIELD_UNIT_FLAG_HIDDEN 0x01u
#define FD2_FIELD_UNIT_FLAG_ACTED 0x80u

typedef struct {
    /* 原版 DAT_00003a45 的 0x50 字节布局。 */
    uint8_t x;                  /* 0x00: 地图格 X */
    uint8_t y;                  /* 0x01: 地图格 Y */
    uint8_t sprite_cache_class; /* 0x02: FD2.TMP 运行期 cache class */
    uint8_t direction;          /* 0x03: 0下、1左、2上、3右 */
    uint8_t frame_phase;        /* 0x04: 移动相位 1..6；0 为静止 */
    uint8_t flags;              /* 0x05: bit0 隐藏/失效 */
    uint8_t side;               /* 0x06: 阵营 */
    uint8_t unit_id;            /* 0x07: FDICON/DATO 稳定单位编号 */
    uint8_t text_id;            /* 0x08: FDTXT 说话角色查找编号 */
    uint8_t data_09_1e[0x16];   /* 含 0x0a..0x19 的 8 个装备槽 */
    uint8_t race;               /* 0x1f: 详情页种族 token 索引 */
    uint8_t movement_profile;   /* 0x20: 移动 profile 与详情页职业索引 */
    uint8_t level;              /* 0x21 */
    uint8_t attack_status;      /* 0x22: AP 状态颜色/缩放触发 */
    uint8_t defense_status;     /* 0x23: DP 状态颜色/缩放触发 */
    uint8_t hit_evasion_status; /* 0x24: HIT/EV 状态颜色与 +15 触发 */
    uint8_t detail_status[3];   /* 0x25..0x27: 详情页 frame 55..57 */
    uint8_t data_28_33[0x0c];
    uint8_t ai_behavior_raw;    /* 0x34: behavior 0..5、7..11；6 未确认 */
    uint8_t ai_param_35;        /* 0x35: behavior 参数，具体语义按模式解释 */
    uint8_t ai_param_36;        /* 0x36: behavior 参数，具体语义按模式解释 */
    uint8_t base_attack_le[2];  /* 0x37 */
    uint8_t base_defense_le[2]; /* 0x39 */
    uint8_t movement_points;    /* 0x3b: 每次移动的范围预算 */
    uint8_t experience;         /* 0x3c: 详情页 EX */
    uint8_t data_3d;
    uint8_t dexterity_le[2];    /* 0x3e: 详情页 DX / 基础命中回避 */
    uint16_t hp;                /* 0x40 */
    uint16_t hp_max;            /* 0x42 */
    uint16_t mp;                /* 0x44 */
    uint16_t mp_max;            /* 0x46 */
    uint16_t attack;            /* 0x48: 物理攻击，含角色/装备派生值 */
    uint16_t defense;           /* 0x4a: 物理防御，含角色/装备派生值 */
    uint16_t accuracy;          /* 0x4c: 普通攻击命中值 */
    uint16_t evasion;           /* 0x4e: 普通攻击回避值 */
} fd2_field_unit;

typedef struct {
    /* items 保持原版 0x50 字节步长；SDL 派生状态必须放在旁路数组中。 */
    fd2_field_unit items[FD2_FIELD_MAX_UNITS];
    uint8_t walk_frames[FD2_FIELD_MAX_UNITS];
    uint16_t source_template_indices[FD2_FIELD_MAX_UNITS];
    size_t count;
} fd2_field_units;

void fd2_field_units_clear(fd2_field_units *units);

int fd2_field_unit_init(fd2_field_unit *unit,
                        uint8_t unit_id, uint8_t text_id, uint8_t side,
                        int x, int y);
int fd2_field_units_set(fd2_field_units *units, size_t index,
                        uint8_t unit_id, uint8_t text_id, uint8_t side,
                        int x, int y);

/* 按模板序号定位同序号 unit_id 的 placement，并复现原版模板重排与
 * 基础属性派生。相同 unit_id 按出现次序一一匹配。 */
int fd2_field_units_append_template(fd2_field_units *units,
                                    const fd2_field_metadata *meta,
                                    const fd2_field_placements *placements,
                                    size_t template_index);
int fd2_field_units_append_group(fd2_field_units *units,
                                 const fd2_field_metadata *meta,
                                 const fd2_field_placements *placements,
                                 uint8_t group);

int fd2_field_unit_is_hidden(const fd2_field_unit *unit);
void fd2_field_unit_set_hidden(fd2_field_unit *unit, int hidden);
int fd2_field_unit_has_acted(const fd2_field_unit *unit);
void fd2_field_unit_set_acted(fd2_field_unit *unit, int acted);
int fd2_field_units_find_visible_text_id(const fd2_field_units *units,
                                         uint8_t text_id);
size_t fd2_field_unit_frame_index(const fd2_field_unit *unit,
                                  uint8_t walk_frame,
                                  uint8_t idle_phase);

/* 用 FDICON.B24 直接按 unit_id 绘制，避开会随 stage 重建的 FD2.TMP
 * cache class。camera_x/y 为世界像素坐标。 */
void fd2_field_units_render(fd2_vga *vga,
                            const fd2_map_sprite_bank *sprites,
                            const fd2_field_units *units,
                            int camera_x, int camera_y,
                            uint8_t idle_phase);

#endif /* FD2_FIELD_UNIT_H */
