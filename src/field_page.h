#ifndef FD2_FIELD_PAGE_H
#define FD2_FIELD_PAGE_H

#include <stddef.h>
#include <stdint.h>

/* field_view_render @0x11cac / field_view_render_tiles @0x11eee 的53A49：
 * App唯一456×336工作页，不是320×200显示页，也不进入SAV。
 * 只在App初始化时分配；透明SKIP和视窗外写入一直保留到下次实际覆盖。 */
enum {
    FD2_FIELD_PAGE_STRIDE = 456,
    FD2_FIELD_PAGE_HEIGHT = 336,
    FD2_FIELD_PAGE_SIZE = 456 * 336,
    FD2_FIELD_PAGE_VIEW_X = 72,
    FD2_FIELD_PAGE_VIEW_Y = 72,
    FD2_FIELD_PAGE_VIEW_W = 312,
    FD2_FIELD_PAGE_VIEW_H = 192
};

typedef struct fd2_field_page fd2_field_page;

/* calloc仅为首次原未定义owner字节提供安全初值；不是任何后续透明像素的依据。 */
fd2_field_page *fd2_field_page_create(void);
void fd2_field_page_destroy(fd2_field_page *page);
/* 仅resource-only PREPARE/隔离预览的临时视觉快照，背景缓冲也深拷贝。
 * 调用结束必须destroy，不挂成第二持久owner，不把副本写回App。 */
fd2_field_page *fd2_field_page_clone(const fd2_field_page *page);
/* 1c2da/21eb1/24618的固定153216B像素memmove；不复制53AFF等背景资源
 * 或任何clock。仅effect-local临时页与App页之间使用，不是UI反馈入口。 */
int fd2_field_page_copy_pixels(fd2_field_page *destination,const fd2_field_page *source);

/* 4e22a/4e1a6 RLE：完整验证再写。SKIP/隔点RUN的空位不写；opaque0也写。
 * LUT只映实际写入像素。严格消耗src_len；出owner部分安全裁剪，不回绕。
 * validate不写page，可在一组领域绘制之前统一预检。 */
int fd2_field_page_validate_rle(int width, int height,
    const uint8_t *src, size_t src_len);
int fd2_field_page_blit_rle(fd2_field_page *page, int x, int y,
    int width, int height, const uint8_t *src, size_t src_len,
    const uint8_t lut[256]);
/* shape_blit_palette_lut_24x24 @0x4e016（原同名）：base范围格的C0 SKIP
 * 映射既有dst；与4e0a2 overlay不同。隔点RUN空位仍保留，不推进clock。 */
int fd2_field_page_blit_base_lut_24(fd2_field_page *page,int x,int y,
    const uint8_t *src,size_t src_len,const uint8_t lut[256]);

/* 同一工作页与隔离scratch之间的有限矩形桥接。坐标必须完整位于owner；
 * 调用方buffer至少(stride*(height-1)+width)字节，失败不作部分复制。
 * buffer不得与page重叠；普通地图/UI不得回灌显示页。下方19953、1f1cc
 * 及1f558固定有证例外必须走各自窄入口，不能拿此通用矩形口补造正常透明底图。 */
int fd2_field_page_read(const fd2_field_page *page, int x, int y,
    int width, int height, uint8_t *pixels, size_t size, size_t stride);
int fd2_field_page_write(fd2_field_page *page, int x, int y,
    int width, int height, const uint8_t *pixels, size_t size, size_t stride);

/* 只复制原312×192视窗到320×200的(4,4)，不清边框、不推进时钟或owner。
 * source可随原camera pan移至页内其它位置，不限正常的(72,72)。 */
int fd2_field_page_project(const fd2_field_page *page,
    int source_x, int source_y, uint8_t *pixels, size_t size);

/* sub_19953 @0x19953（原同名）：原整VGA snapshot写到page(68,68)，
 * 或只把VGA(5,112)310×86写到page(73,180)。这是确认框特定调用合同，
 * 不开放普通地图/对白回灌；完整输入至少64000B，失败不部分写入。 */
int fd2_field_page_choice_snapshot_write(fd2_field_page *page,
    const uint8_t *pixels,size_t size);
int fd2_field_page_choice_bottom_write(fd2_field_page *page,
    const uint8_t *pixels,size_t size);
/* 21548重定向1f558的64000B scaled scratch后，只复制(4,4)312×192
 * 到实际456页(72,72)；不把其它UI/边框或background metadata写入。 */
int fd2_field_page_scaled_view_write(fd2_field_page *page,
    const uint8_t *pixels,size_t size);

/* sub_1F1CC/sub_1F30A @0x1f1cc/0x1f30a的有证例外：原1f1cc把保存的
 * VGA64000字节写到53A49 raw偏移0，随后按320stride作mosaic源。
 * 固定prefix接口仅供这两个caller，不是普通地图/对话页回灌入口。 */
int fd2_field_page_phase_prefix_read(const fd2_field_page *page,
    uint8_t *pixels, size_t size);
int fd2_field_page_phase_prefix_write(fd2_field_page *page,
    const uint8_t *pixels, size_t size);

#endif
