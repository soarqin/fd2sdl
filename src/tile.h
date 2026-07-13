#ifndef FD2_TILE_H
#define FD2_TILE_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "field.h"
#include "image.h"
#include "shape.h"
#include "vga.h"

/* TAI.DAT 图像读取
 *
 * TAI.DAT 的每个条目可按通用 RLE 图像解码，尺寸多为 154/155×39/42，
 * 使用 FDOTHER.DAT[0] 调色板显示。它不是战场网格底图的直接来源。
 */
typedef struct {
    fd2_image *images;
    size_t count;
} fd2_tileset;

int  fd2_tileset_load(fd2_tileset *tiles, const fd2_archive *tai);
const fd2_image *fd2_tileset_get(const fd2_tileset *tiles, size_t idx);
void fd2_tileset_close(fd2_tileset *tiles);

/* FDSHAP.DAT 战场地形表与 24×24 地图帧。
 *
 * 对照 FUN_00010580 @0x3804c：FDFIELD cell 低 10 位为 terrain_id，
 * 再从 FDSHAP 奇数条目的 4 字节地形表读取属性。地图底图使用
 * FDSHAP 偶数条目的同号帧；FUN_0001020e @0x37cda 在重绘遮挡格时
 * 会检查 flags 的 0x08/0x80 位并绘制后一帧。
 */
typedef struct {
    uint8_t flags;
    uint8_t movement_cost_class; /* profile[该值] 为进入本格的移动消耗 */
    uint8_t attr2;
    uint8_t attr3;
} fd2_terrain_attr;

typedef struct {
    size_t shape_index;       /* FDSHAP 成对资源编号；实际条目为 index*2 / index*2+1 */
    fd2_shape_sheet sheet;
    fd2_image *frames;        /* 已解码的 24×24 帧缓存 */
    fd2_terrain_attr *attrs;  /* 奇数条目的 4 字节地形表 */
    size_t attr_count;
} fd2_terrain_tileset;

int  fd2_terrain_tileset_open(fd2_terrain_tileset *terrain,
                              const fd2_archive *fdshap,
                              size_t shape_index);
int  fd2_terrain_tileset_open_stage(fd2_terrain_tileset *terrain,
                                    const fd2_archive *fdshap,
                                    const fd2_archive *field,
                                    size_t stage_idx);
const fd2_image *fd2_terrain_frame(const fd2_terrain_tileset *terrain,
                                   size_t frame_idx);
const fd2_terrain_attr *fd2_terrain_attr_get(const fd2_terrain_tileset *terrain,
                                             uint16_t terrain_id);
int  fd2_terrain_base_frame_from_cell(const fd2_terrain_tileset *terrain,
                                      uint32_t cell,
                                      size_t *out_frame_idx);
int  fd2_terrain_base_frame_from_cell_animated(
                                      const fd2_terrain_tileset *terrain,
                                      uint32_t cell,
                                      int terrain_phase,
                                      int actor_phase,
                                      size_t *out_frame_idx);
int  fd2_terrain_overlay_frame_from_cell(const fd2_terrain_tileset *terrain,
                                         uint32_t cell,
                                         int anim_phase,
                                         size_t *out_frame_idx);
void fd2_terrain_tileset_close(fd2_terrain_tileset *terrain);

void fd2_tileset_blit(fd2_vga *vga, const fd2_image *img,
                      int x, int y, int transparent_index);
void fd2_tileset_blit_lut(fd2_vga *vga, const fd2_image *img,
                          int x, int y, int transparent_index,
                          const uint8_t lut[256]);
void fd2_terrain_render_field_base(fd2_vga *vga,
                                   const fd2_terrain_tileset *terrain,
                                   const fd2_field_map *map,
                                   int camera_x, int camera_y);
void fd2_terrain_render_field_base_animated(
                                   fd2_vga *vga,
                                   const fd2_terrain_tileset *terrain,
                                   const fd2_field_map *map,
                                   int camera_x, int camera_y,
                                   int terrain_phase, int actor_phase);
void fd2_terrain_render_field_overlay(fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int camera_x, int camera_y,
                                      int anim_phase);
void fd2_terrain_render_field_preview(fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int camera_x, int camera_y,
                                      int anim_phase);

/* field_view_render_scaled @0x44776：以 1/128 pixel 的 fixed-point 步长
 * 重采样 312×192 战场视窗。center_* 使用原版每格 0xc00 的坐标。 */
void fd2_terrain_render_field_scaled_animated(
                                      fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int center_x_fp, int center_y_fp,
                                      int step_fp,
                                      int terrain_phase, int actor_phase);

/* 兼容旧调试代码的临时 TAI 预览映射。正式战场底图见 fd2_terrain_*。 */
size_t fd2_tileset_preview_index_from_cell(uint32_t cell, size_t tile_count);
void fd2_tileset_render_field_preview(fd2_vga *vga,
                                      const fd2_tileset *tiles,
                                      const fd2_field_map *map,
                                      int camera_x, int camera_y);

#endif /* FD2_TILE_H */
