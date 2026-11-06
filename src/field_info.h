#ifndef FD2_FIELD_INFO_H
#define FD2_FIELD_INFO_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "font.h"
#include "image.h"
#include "text.h"
#include "field_unit.h"
#include "vga.h"

#define FD2_FIELD_INFO_PANEL_FRAME 130u
#define FD2_FIELD_INFO_PLUS_FRAME 131u
#define FD2_FIELD_INFO_MINUS_FRAME 132u
#define FD2_FIELD_INFO_DIGIT_FRAME 31u
#define FD2_FIELD_INFO_MAGIC_COST_FRAME 92u
/* 当前SDL完整可见布局门，不是原版40bit位图或分页数量上限。 */
#define FD2_FIELD_MAGIC_LIST_MAX_COUNT 12u

typedef struct {
    fd2_image panel;
    fd2_image signs[2]; /* 0=正号，1=负号 */
    fd2_image digits[3][10]; /* frame 31..40、42..51、119..128 */
    fd2_image digit_overflow[3]; /* frame 41/52/129 */
    fd2_image magic_cost; /* frame 92: 20×8 raw，零像素也绘制 */
    fd2_image two_digit_overflow; /* frame 93 */
    fd2_image detail_border[18]; /* frame 1..17：通用 UI 边框小块 */
    fd2_image detail_top;    /* frame 20: 223×86 */
    fd2_image detail_bottom; /* frame 21: 310×99 */
    fd2_image unit_summary;  /* frame 22: 149×42，移动选择入口的临时概要板 */
    fd2_image affiliation[2];/* frame 53/54 */
    fd2_image status_icons[3]; /* frame 55..57 */
    fd2_image bars[8];       /* 首cap23/26保索引0/1；其后24/25/27/28/29/30，均1×5 */
    fd2_image detail_icons[9]; /* frame 59..67 */
    fd2_image overview[4]; /* frame 133..136：回合、条件、人数、资源底板 */
    int ready;
} fd2_field_info_assets;

typedef struct {
    size_t stage;
    uint32_t turn_number;
    uint32_t resource_value;
    const fd2_field_units *units;
} fd2_field_overview;

/* FDOTHER[5] 战场格子/单位信息面板资源。
 * field_cell_info_panel_draw @0x1acf3（Watcom __chkstk body 在 +0xa，
 * 不是独立函数）使用 frame 130
 * 作为 69×34 底板，frame 131/132 为正负号，frame 31..40 与 42..51
 * 分别为满 HP 和受伤状态的数字。 */
/* assets 首次调用前必须清零；已 ready 的对象可直接重新打开。 */
int fd2_field_info_assets_open(fd2_field_info_assets *assets,
                               const fd2_archive *fdother);
int fd2_field_info_assets_open_mem(fd2_field_info_assets *assets,
                                   const uint8_t *data, size_t size);
void fd2_field_info_assets_close(fd2_field_info_assets *assets);

/* panel_right=0 时逻辑坐标为 (5,161)，非 0 时为 (246,161)。terrain 和
 * unit 均绘制到左侧 24×24 格；unit 可为空。 */
void fd2_field_info_draw(fd2_vga *vga,
                         const fd2_field_info_assets *assets,
                         int panel_right,
                         const fd2_image *terrain,
                         const fd2_image *unit,
                         uint16_t hp,
                         uint16_t hp_max,
                         int attack_modifier,
                         int defense_modifier);

/* field_unit_summary_draw @0x18c6d（原登记sub_18C6D）：姓名、LV、signed HP/MP与条形。
 * actor_relative_x>=7画(9,9)，否则画(160,9)。完整预检后绘制，返回0；
 * 资源／普通姓名不受支持时返回-1，整个VGA不变，unit与所有owner均只读。
 * 仅纯绘制；18890→18b84首个未消费键前的出现／消失由play owner管理，
 * 不在UNIT_SELECTED常驻，也不处理输入／RNG／present。 */
int fd2_field_unit_summary_draw(fd2_vga *vga,
                                 const fd2_field_info_assets *assets,
                                 const fd2_font *font,
                                 const fd2_text_entry *text,
                                 const fd2_field_unit *unit,
                                 int actor_relative_x);

/* sub_2FACD @0x2facd 调用 field_unit_summary_draw @0x18c6d（原sub_18C6D）：
 * 战斗页按阵营选(171,4)/(0,154)。这里只提供同一原概要板的坐标入口；
 * 所有资源与149x42范围完整预检，失败整个VGA不变，单位只读。 */
int fd2_field_unit_summary_draw_at(fd2_vga *vga,
                                 const fd2_field_info_assets *assets,
                                 const fd2_font *font,
                                 const fd2_text_entry *text,
                                 const fd2_field_unit *unit,
                                 int x, int y);

/* 战况／胜败条件总览：sub_1B41D @0x1b41d、sub_1B5F1 @0x1b5f1。
 * 当前支持stage0/1的FDTXT[0]597/598、599/600普通字形条件。成功返回0；
 * 资源、文本或输入不受支持时返回 -1，framebuffer 保持不变。
 * 只叠加4块页面，不绘制地图、不提交玩法，也不播放开合或处理输入。 */
int fd2_field_overview_draw(fd2_vga *vga,
                            const fd2_field_info_assets *assets,
                            const fd2_font *font,
                            const fd2_text_entry *text,
                            const fd2_field_overview *data);

/* field_unit_magic_list_draw @0x1ceed（原登记sub_1CEED）的法术列表绘制；
 * field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）
 * 的单键UI步骤使用同一selected索引；本函数不执行该步骤。
 * 接受0..12个已知ID；selected=-1为未选，其余须为有效索引。
 * 完整预检后恢复(5,94)的310×99底板并叠列表，不重建上半详情。
 * 成功返回0；错误返回-1且framebuffer不变。不处理输入／MP消耗／RNG。 */
int fd2_field_magic_list_draw(fd2_vga *vga,
                              const fd2_field_info_assets *assets,
                              const fd2_font *font,
                              const fd2_text_entry *text,
                              const fd2_field_unit *unit,
                              int selected);

/* field_unit_detail_equipment_draw @0x184c0（原登记同名）：显示压行，
 * selected_raw_slot仍是0..7原始槽；-1未选，选中空槽或未知ID拒绝。
 * 合法空库存+未选恢复空底板，隐藏槽的stale ID不参与绘制／验证。
 * 完整预检后恢复(5,94)的310×99底板，再绘名称、分类／属性与数值；
 * native184c0自身只叠列表，恢复底板是SDL合并详情的整合约定。
 * 返回0成功/-1错误，失败全VGA不变。不处理输入、Use门、库存或RNG。 */
int fd2_field_item_list_draw(fd2_vga *vga,
                             const fd2_field_info_assets *assets,
                             const fd2_font *font,
                             const fd2_text_entry *text,
                             const fd2_field_unit *unit,
                             int selected_raw_slot);

/* 全屏角色详情页主体：复现 field_unit_detail_draw @0x17eef 与
 * field_unit_detail_stats_draw @0x17fc0。portrait 为 DATO 当前单位首帧。 */
void fd2_field_detail_draw(fd2_vga *vga,
                           const fd2_field_info_assets *assets,
                           const fd2_font *font,
                           const fd2_text_entry *text,
                           const fd2_field_unit *unit,
                           const fd2_image *portrait);

/* field_unit_detail_transition_frame @0x18409：phase 11→0 打开，
 * 0→11 关闭；每帧从 background 重建三段开合画面。 */
void fd2_field_detail_transition_frame(uint8_t *dst,
                                       const uint8_t *detail,
                                       const uint8_t *background,
                                       int phase);

/* field_unit_detail_open @0x17e0b：打开 phase 11/5 播放 SFX 5；
 * 调用方关闭循环 phase 0/7 播放 SFX 6。其他 phase 返回 -1。 */
int fd2_field_detail_sfx_for_phase(int opening, int phase);

#endif /* FD2_FIELD_INFO_H */
