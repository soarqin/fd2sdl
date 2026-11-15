#ifndef FD2_FIELD_BACKGROUND_H
#define FD2_FIELD_BACKGROUND_H

#include "archive.h"
#include "field_page.h"

/* sub_10652 @0x10652：按唯一冷dispatch载入原53AFF/53B03背景资源。
 * 全资源预检成功才swap；不改page像素/539F8/539FC/51A10，不推进业务。
 * 首次安全初始化51A10为原byte1；后续Load/换stage保留App视觉锚。 */
int fd2_field_background_rebuild(fd2_field_page *page,
    const fd2_archive *fdother, size_t stage);

/* field_view_render_tiles @0x11eee铺tiles之前的有限背景叶。
 * dest为已经包含原53AED/53AF1/53AF5的实际page原点；camera为本次11eee
 * map格参数，parallax_*对应原53B07/53B0B（非camera像素位移）。
 * advance=1沿真实11eee推进一次539F8/539FC；0仅借当前资源/状态画图。
 * 不做stage分支，不推进terrain/idle/water/RNG。kind0为成功no-op。 */
int fd2_field_background_draw(fd2_field_page *page,
    int dest_x, int dest_y, int camera_x, int camera_y,
    int parallax_x, int parallax_y, uint64_t now_ns, int advance);
/* 21eb1/24618的临时terrain目标仍借同一App背景资源/539F8，不能通过
 * clone目的页造第二个真实资源clock。PREPARE传临时owner，实际传App。 */
int fd2_field_background_draw_from(fd2_field_page *owner,fd2_field_page *target,
    int dest_x,int dest_y,int camera_x,int camera_y,int parallax_x,int parallax_y,
    uint64_t now_ns,int advance);

/* live只在wave/scroll的advance门实际读取共享BIOS；行更新后重取last。
 * none/parallax、advance=0及前检拒绝不读钟；更新失败不重读/写锚。
 * 不另起背景clock owner。 */
int fd2_field_background_draw_live(fd2_field_page *page,
    int dest_x,int dest_y,int camera_x,int camera_y,int parallax_x,int parallax_y,int advance);
int fd2_field_background_draw_from_live(fd2_field_page *owner,fd2_field_page *target,
    int dest_x,int dest_y,int camera_x,int camera_y,int parallax_x,int parallax_y,int advance);

/* sub_24D22 @0x24d22：非0只写BYTE速度，0按当前速度立即轮转背景；
 * 不触碰539F8，调用者保留原控制/等待顺序。仅有源312×192滚动资源可轮转。 */
int fd2_field_background_scroll(fd2_field_page *page, uint8_t amount);

#endif
