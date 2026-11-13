#ifndef FD2_FIELD_UNIT_H
#define FD2_FIELD_UNIT_H

#include <stddef.h>
#include <stdint.h>

#include "field.h"
#include "field_path.h"
#include "map_sprite.h"
#include "tile.h"
#include "vga.h"

/* 战场单位运行时记录。
 *
 * 逆向依据：DAT_00003a45 是由 0x50 字节记录组成的战场单位表；
 * map_scene_render_actors @0x1cb94、map_actor_blit_24x24 @0x1da16、
 * field_visible_actor_find_by_text_id @0x12c60 分别读取坐标、精灵、
 * 隐藏标志及文本编号。FDFIELD stage metadata 的 26 字节模板与
 * placement 按有效模板数（metadata+2）和相同索引读取；原版 stage constructor
 * 将 26 字节模板重排为身份、装备和 AI 字段，再按等级派生基础属性。
 * field_ai_unit_execute @0x13a9f 进一步确认 record +0x34 低半字节为
 * behavior selector，并在入口读取 +0x35/+0x36 作为模式参数。
 */
#define FD2_FIELD_UNIT_RECORD_SIZE 0x50
#define FD2_FIELD_MAX_UNITS 96
#define FD2_FIELD_UNIT_NO_TEMPLATE UINT16_MAX
#define FD2_FIELD_UNIT_FLAG_HIDDEN 0x01u
#define FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE 0x04u /* AI entry flags &0x05 gate */
#define FD2_FIELD_UNIT_FLAG_ACTED 0x80u

typedef struct {
    /* 原版 DAT_00003a45 的 0x50 字节布局。 */
    uint8_t x;                  /* 0x00: 地图格 X */
    uint8_t y;                  /* 0x01: 地图格 Y */
    uint8_t sprite_cache_class; /* 0x02: FD2.TMP 运行期 cache class */
    uint8_t direction;          /* 0x03: 普通0下1左2上3右；mode80保留已证raw5 */
    uint8_t frame_phase;        /* 0x04: 移动相位 1..6；0 为静止 */
    uint8_t flags;              /* 0x05: bit0 隐藏，bit2 AI 不可执行，bit7 acted */
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
    uint8_t data_3d;            /* 0x3d: behavior 5 的 0-based cell action slot */
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

/* field_roster_has_text @0x33499（原sub_33499）：独立roster的+8 BYTE与
 * 完整DWORD比较，不过滤hidden/HP/side；仅借用已有32条容量owner。 */
static inline int fd2_field_roster_has_text(const fd2_field_unit *roster,
        size_t count, uint32_t text_id) {
    if (!roster || count > 32u) return -1;
    for (size_t i = 0; i < count; i++)
        if ((uint32_t)roster[i].text_id == text_id) return 1;
    return 0;
}

typedef struct {
    /* items 保持原版 0x50 字节步长；SDL 派生状态必须放在旁路数组中。 */
    fd2_field_unit items[FD2_FIELD_MAX_UNITS];
    uint8_t walk_frames[FD2_FIELD_MAX_UNITS];
    uint16_t source_template_indices[FD2_FIELD_MAX_UNITS];
    /* 11042原cache去重顺序的资源投影；不写入80B单位记录或SAV扩展。 */
    uint8_t sprite_cache_ids[FD2_FIELD_MAX_UNITS];
    uint8_t sprite_cache_classes[FD2_FIELD_MAX_UNITS];
    uint8_t sprite_cache_exact; /* 仅有证构造/Load顺序才能用于跨class方向。 */
    size_t sprite_cache_count;
    size_t count;
} fd2_field_units;

/* fdicon_cache_append_unit @0x11019：登记已构造actor；同ID复用class。
 * restore复现10010的Load：按当前actor/ID顺序重新建cache，仅写旁路，
 * 不信存档raw+2、不尝试找旧TMP，全部80B保留。未知目标class拒绝绘制。 */
int fd2_field_units_sprite_cache_append(fd2_field_units *units, size_t actor);
int fd2_field_units_sprite_cache_restore(fd2_field_units *units);
size_t fd2_field_unit_cached_frame_index(const fd2_field_units *units,
    size_t actor, uint8_t idle_phase);
/* 信息板固定朝向帧也读原cache身份，不能随hot unit_id写提前换sprite。 */
size_t fd2_field_unit_cached_sprite_id(const fd2_field_units *units, size_t actor);

void fd2_field_units_clear(fd2_field_units *units);

int fd2_field_unit_init(fd2_field_unit *unit,
                        uint8_t unit_id, uint8_t text_id, uint8_t side,
                        int x, int y);
int fd2_field_units_set(fd2_field_units *units, size_t index,
                        uint8_t unit_id, uint8_t text_id, uint8_t side,
                        int x, int y);

/* 1088d/10b4e的有效模板前缀；资源尾和未知字节保留，不能按资源长度遍历。
 * 失败-1不写count；成功0。 */
int fd2_field_unit_template_count(const fd2_field_metadata *meta, size_t *count);
/* 10a5b从有效模板数后开始取玩家出阵位置；仅读坐标低byte。 */
int fd2_field_unit_deployment_position(const fd2_field_metadata *meta,
    const fd2_field_placements *placements, size_t ordinal, uint8_t *x, uint8_t *y);

/* 10c85按相同模板索引读取placement坐标低byte，不按第三WORD的ID匹配。 */
int fd2_field_units_append_template(fd2_field_units *units,
                                    const fd2_field_metadata *meta,
                                    const fd2_field_placements *placements,
                                    size_t template_index);
int fd2_field_units_append_group(fd2_field_units *units,
                                 const fd2_field_metadata *meta,
                                 const fd2_field_placements *placements,
                                 uint8_t group);
/* 仅供对白资源预检的借用副本：投影身份/坐标/source索引，不作基础/装备派生
 * 或cache登记，不可当作可提交的真实单位。原handler仍在自己的写点完整构造。 */
int fd2_field_units_project_group_identity(fd2_field_units *projection,
    const fd2_field_metadata *meta, const fd2_field_placements *placements,
    uint8_t group);

/* 53AFA强制原placement；否则nearest free占格搜索，不读terrain成本。
 * 原样shadow提交；Scene与业务共用，未知模板/无空格失败不写units。 */
int fd2_field_units_append_group_placed(fd2_field_units *units,
    const fd2_field_metadata *meta, const fd2_field_placements *placements,
    const fd2_field_map *map, uint8_t group, int force);

int fd2_field_unit_is_hidden(const fd2_field_unit *unit);
void fd2_field_unit_set_hidden(fd2_field_unit *unit, int hidden);
int fd2_field_unit_has_acted(const fd2_field_unit *unit);
void fd2_field_unit_set_acted(fd2_field_unit *unit, int acted);
int fd2_field_units_find_visible_text_id(const fd2_field_units *units,
                                         uint8_t text_id);
/* 已确认事件写点的有限typed字段。SET/AND/OR只操作指定字段，
 * 不接收offset/地址；重算、败亡、掉落和呈现由原owner的冷顺序调用。 */
typedef enum {
    FD2_UNIT_X, FD2_UNIT_Y, FD2_UNIT_DIRECTION, FD2_UNIT_FRAME_PHASE,
    FD2_UNIT_FLAGS, FD2_UNIT_SIDE, FD2_UNIT_ID, FD2_UNIT_TEXT_ID,
    FD2_UNIT_ATTACK_STATUS, FD2_UNIT_DEFENSE_STATUS, FD2_UNIT_HIT_STATUS,
    FD2_UNIT_STATUS_25, FD2_UNIT_STATUS_26, FD2_UNIT_STATUS_27,
    FD2_UNIT_DESCRIPTOR, FD2_UNIT_AI_RAW, FD2_UNIT_AI_PARAM_35, FD2_UNIT_AI_PARAM_36,
    FD2_UNIT_HP, FD2_UNIT_MP
} fd2_field_unit_field;
typedef enum { FD2_UNIT_SET, FD2_UNIT_AND, FD2_UNIT_OR } fd2_field_unit_write;
int fd2_field_units_field_write(fd2_field_units *units, size_t first, size_t last,
    fd2_field_unit_field field, fd2_field_unit_write operation, uint16_t value);
/* 344f2: AI34 = arg | (old & f0)，arg是原BYTE，不擅自截断到低4位。 */
int fd2_field_units_ai_behavior_write(fd2_field_units *units,
    size_t first, size_t last, uint8_t value);
size_t fd2_field_unit_frame_index(const fd2_field_unit *unit,
                                  uint8_t walk_frame,
                                  uint8_t idle_phase);

/* 用 FDICON.B24 直接按 unit_id 绘制，避开会随 stage 重建的 FD2.TMP
 * cache class。camera_x/y 为世界像素坐标。 */
void fd2_field_unit_render(fd2_vga *vga,
                           const fd2_map_sprite_bank *sprites,
                           const fd2_field_unit *unit, uint8_t walk_frame,
                           int camera_x, int camera_y, uint8_t idle_phase);
void fd2_field_units_render(fd2_vga *vga,
                            const fd2_map_sprite_bank *sprites,
                            const fd2_field_units *units,
                            int camera_x, int camera_y,
                            uint8_t idle_phase);
/* 单actor路径也使用同一原cache映射；offset_y仅是当前演出的工作页偏移。 */
void fd2_field_units_render_actor(fd2_vga *vga,
    const fd2_map_sprite_bank *sprites, const fd2_field_units *units,
    size_t actor, int camera_x, int camera_y, uint8_t idle_phase, int offset_y);

/* sub_127E0/127A9的原工作页版本。camera为格；raw顺序不按y/x排序。
 * idle/cursor/jitter均借App；移动帧取53C07，不能用局部六步数组重置。
 * offset_y只用于有证演出。
 * 不画前景/选择/UI、不投影、不改unit/cache/时钟。错误返回-1。 */
int fd2_field_units_render_page_actor(fd2_field_page *page,
    const fd2_map_sprite_bank *sprites, const fd2_field_units *units,
    size_t actor, int camera_x, int camera_y, uint8_t idle_phase,
    uint8_t cursor_phase, uint8_t jitter_phase, int offset_y);
/* map_actor_blit_24x24 @0x1da16（原同名）：原1a30b/静止恢复专用叶。
 * 不读hidden/acted/raw26；frame_phase非0选共享53C07，否则53C0B，3→1。
 * mode0原色，mode2按原stride低BYTE填200；不等待/投影/推进任何phase。 */
int fd2_field_map_action_page_actor(fd2_field_page *page,
    const fd2_map_sprite_bank *sprites, const fd2_field_units *units,
    size_t actor, int camera_x, int camera_y, uint8_t idle_phase,
    uint8_t cursor_phase, unsigned mode);
/* field_actor_group_flash @0x1c2da：固定direction0，idle3→2，solid200；
 * 不套hidden/acted/raw26/frame_phase。只画显式传入raw，不投影/推进clock。 */
int fd2_field_flash_page_actor(fd2_field_page *page,
    const fd2_map_sprite_bank *sprites,const fd2_field_units *units,
    size_t actor,int camera_x,int camera_y,uint8_t idle_phase);
/* 1cb94/1cd17/1c2da共同固定dir0、idle3→2叶；hidden由caller按各源筛，
 * lut=NULL原色，非NULL只映真实RLE写点，不画FG或推进jitter。 */
int fd2_field_fixed_page_actor(fd2_field_page *page,
    const fd2_map_sprite_bank *sprites,const fd2_field_units *units,
    size_t actor,int camera_x,int camera_y,uint8_t idle_phase,const uint8_t lut[256]);
int fd2_field_units_render_page(fd2_field_page *page,
    const fd2_map_sprite_bank *sprites, const fd2_field_units *units,
    int camera_x, int camera_y, uint8_t idle_phase, uint8_t cursor_phase,
    uint8_t jitter_phase);
int fd2_field_units_render_page_terrain_overlays(fd2_field_page *page,
    const fd2_terrain_tileset *terrain, const fd2_field_map *map,
    const fd2_field_units *units, int camera_x, int camera_y, int anim_phase,
    const fd2_field_path_result *range, const uint8_t lut[256]);

/* field_actor_terrain_overlays_draw @0x129ec（原sub_129EC）：全表actor
 * 绘完后，只恢复非hidden/非terrain-immune的本格、上格及行走附加格。
 * range/lut只控制这些格的映色，不追加范围外的前景；不改units或range。 */
void fd2_field_units_render_terrain_overlays(fd2_vga *vga,
    const fd2_terrain_tileset *terrain, const fd2_field_map *map,
    const fd2_field_units *units, int camera_x, int camera_y, int anim_phase,
    const fd2_field_path_result *range, const uint8_t lut[256]);

#endif /* FD2_FIELD_UNIT_H */
