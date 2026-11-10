#ifndef FD2_FIELD_PREPARE_VIEW_H
#define FD2_FIELD_PREPARE_VIEW_H

#include "field_prepare_play.h"

/* 26152/11019首次ID去重目录，2825b/选阵按raw序号而非record+2索引。
 * 1有已初始化sprite并写ID；0超已初始化目录（仅省未知绘制，输出不写）；
 * -1坏参数。只读重建，不保存cache owner，不以unitID+1猜缺失条目。 */
int fd2_field_prepare_cached_sprite_id(const fd2_save_manual_state *state,
    size_t raw_index, uint8_t *sprite_id);

/* 27738底页(5,112)+272d0商品／压缩库存六格，first为原偶数滚动偏移，
 * selected是同一个数字rank，不按raw flag或装备40改高亮／分类图。
 * selling非零按3/4价。成功0；失败-1全VGA不变；不处理输入/等待/RNG。 */
int fd2_field_prepare_draw_items(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const uint8_t *ids, size_t count,
    size_t first, int selected, int selling);

/* 27f4a底页+2825b三行装备收件预览；candidate_raws来自原职业候选，
 * first逐行滚动，phase3映1。preview只借28632结果，不改roster／钱／派生。
 * 缺已初始化cache条目只省sprite，其它真实坏资源仍原子拒绝。 */
int fd2_field_prepare_draw_equipment_recipients(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    const uint8_t *candidate_raws, size_t count, size_t first, int selected,
    uint8_t item_id, unsigned phase);

/* 27d33/2810b成员2x3；first为偶数，末页5/6行，selected=-1或当前页raw。
 * 2a0c2复活三行的raws为领域候选原序，selected为候选rank；包含原费用，
 * 不处理支付/清flags/HP写点。两页仅重建service.roster_page所在底页。 */
int fd2_field_prepare_draw_members(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    size_t first, int selected, unsigned phase);
int fd2_field_prepare_draw_revive_candidates(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    const uint8_t *candidate_raws, size_t count, size_t first, int selected,
    unsigned phase);
/* 2a694/2a857三行转职候选：姓名、旧职业、593、plan目标职业。
 * 不显示复活费用、不提前耗物或写身份。其余原子/游标合同同revive。 */
int fd2_field_prepare_draw_promotion_candidates(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    const uint8_t *candidate_raws, size_t count, size_t first, int selected,
    unsigned phase);

/* 2af28/2b4fb选阵整页；quota为同伴名额15/19，cursor为同伴0..count-2。
 * selection_label借FDOTHER[5] frame137(86x86)，cursor_image借FDOTHER[1]
 * frame0(24x24)；其它底板复用info。调用者持有图像，不转移owner。
 * phase由外部视觉时钟供给；仅原始31位临时bitmap，不重排/修改队伍。
 * 32人容器末同伴超200行的sprite只裁掉视口外部分；不读写VGA外字节。
 * 全部新页成功0，错误-1且整个VGA不变；不输入/等待/present/RNG。 */
int fd2_field_prepare_draw_deployment(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    const uint8_t selected[FD2_SAVE_MAX_UNITS - 1u], size_t quota, size_t cursor,
    unsigned phase, const fd2_image *selection_label, const fd2_image *cursor_image);

#endif
