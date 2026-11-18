#ifndef FD2_TILE_H
#define FD2_TILE_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "field.h"
#include "field_page.h"
#include "field_path.h"
#include "image.h"
#include "shape.h"
#include "vga.h"

/* FDSHAP.DAT 战场地形表与 24×24 地图帧。
 *
 * 对照 FUN_00010580 @0x12e38：FDFIELD cell 低 10 位为 terrain_id，
 * 再从 FDSHAP 奇数条目的 4 字节地形表读取属性。地图底图使用
 * FDSHAP 偶数条目的同号帧；FUN_0001020e @0x12ac6 在重绘遮挡格时
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
/* field_view_render_tiles @0x11eee：任意有证13×8/13×9/14×8块。
 * dest为456页坐标、map_x/y为格坐标；不清页、不推进视觉时钟。
 * 先验证整块后写，范围LUT只映实际RLE opaque写，不映SKIP历史。
 * 非NULL range/lut表示调用方已投影原node+3!=ff的有效选择域。 */
int fd2_terrain_render_page_block(fd2_field_page *page,
    const fd2_terrain_tileset *terrain, const fd2_field_map *map,
    int dest_x, int dest_y, int columns, int rows, int map_x, int map_y,
    int terrain_phase, int actor_phase,
    const fd2_field_path_result *range, const uint8_t lut[256]);
/* 11eee/12ac6有限map源：index=y*width+x，-1为4B WH头；
 * 其它须在完整cells内。只供渲染，不改二维path/lookup接口。 */
int fd2_terrain_page_source_cell(const fd2_field_map *map,int x,int y,uint32_t *cell);
/* 12ac6单格遮挡，camera为格坐标；源视口/负y门外或无overlay为no-op。 */
int fd2_terrain_render_page_overlay_cell(fd2_field_page *page,
    const fd2_terrain_tileset *terrain, const fd2_field_map *map,
    int cell_x, int cell_y, int camera_x, int camera_y, int anim_phase,
    const uint8_t lut[256]);
void fd2_terrain_tileset_close(fd2_terrain_tileset *terrain);

void fd2_tileset_blit(fd2_vga *vga, const fd2_image *img,
                      int x, int y, int transparent_index);
void fd2_tileset_blit_lut(fd2_vga *vga, const fd2_image *img,
                          int x, int y, int transparent_index,
                          const uint8_t lut[256]);
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
/* map_tile_blit_visible @0x12ac6（原FUN_0001020e）：只恢复指定格的
 * 透明遮挡帧；lut非NULL时沿原node+7分支映色，不遍历地图或选择actor。 */
void fd2_terrain_render_field_overlay_cell(fd2_vga *vga,
    const fd2_terrain_tileset *terrain, const fd2_field_map *map,
    int cell_x, int cell_y, int camera_x, int camera_y,
    int anim_phase, const uint8_t lut[256]);
void fd2_terrain_render_field_preview(fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int camera_x, int camera_y,
                                      int anim_phase);

/* field_view_render_scaled @0x1f558：以 1/128 pixel 的 fixed-point 步长
 * 重采样 312×192 战场视窗。center_* 使用原版每格 0xc00 的坐标。 */
void fd2_terrain_render_field_scaled_animated(
                                      fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int center_x_fp, int center_y_fp,
                                      int step_fp,
                                      int terrain_phase, int actor_phase);

#endif /* FD2_TILE_H */
