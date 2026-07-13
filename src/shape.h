#ifndef FD2_SHAPE_H
#define FD2_SHAPE_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "image.h"

/* 炎龙骑士团 2 FDSHAP 帧包读取（战场地形/精灵 shape sheet）
 *
 * 已由 original_game/FDSHAP.DAT 实测确认：偶数条目是 24x24 帧包，
 * 包头为 width/height/frame_count + u32 帧偏移表；每帧使用与
 * FUN_0004c0d5 @0x73ba1 (blit_image_clipped) 相同的 RLE 控制字节，
 * 但帧流本身不含 4 字节宽高头。战场底图使用 FDSHAP 偶数条目的
 * 同号 terrain_id 帧，属性表在配套奇数条目中。
 */

typedef struct {
    int      width;       /* 单帧宽度（通常 24） */
    int      height;      /* 单帧高度（通常 24） */
    size_t   frame_count; /* 帧数 */

    const uint8_t *data;  /* 所属归档条目内存，不由本结构拥有 */
    size_t   size;
    uint32_t *offsets;    /* 帧偏移表，单位：相对 data 起点的字节偏移 */
} fd2_shape_sheet;

/* 从内存条目打开精灵包。不拷贝 data，调用者需保证其生命周期。 */
int  fd2_shape_sheet_open_mem(fd2_shape_sheet *sheet,
                              const uint8_t *data, size_t size);

/* 从 .DAT 归档条目打开精灵包。不拷贝条目数据，归档需保持打开。 */
int  fd2_shape_sheet_open_entry(fd2_shape_sheet *sheet,
                                const fd2_archive *ar, size_t idx);

/* 获取原始帧 RLE 数据（不含宽高头）。 */
int  fd2_shape_sheet_get_frame(const fd2_shape_sheet *sheet, size_t frame_idx,
                               const uint8_t **out_ptr, size_t *out_len);

/* 解码指定帧为 8bpp 索引图。透明 SKIP 区域保留为 0。 */
int  fd2_shape_sheet_decode_frame(fd2_image *img,
                                  const fd2_shape_sheet *sheet,
                                  size_t frame_idx);

void fd2_shape_sheet_close(fd2_shape_sheet *sheet);

#endif /* FD2_SHAPE_H */
