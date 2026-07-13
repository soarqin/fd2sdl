#include "tile.h"

#include <stdlib.h>
#include <string.h>

#define FD2_FIELD_TILE_STEP 24 /* 反编译地图绘制中横/纵格步长 0x18 */

int fd2_tileset_load(fd2_tileset *tiles, const fd2_archive *tai) {
    memset(tiles, 0, sizeof(*tiles));
    if (!tai || tai->count == 0) return -1;

    fd2_image *images = calloc(tai->count, sizeof(*images));
    if (!images) return -1;

    for (size_t i = 0; i < tai->count; i++) {
        if (fd2_image_decode_entry(&images[i], tai, i) != 0) {
            for (size_t j = 0; j < i; j++) fd2_image_free(&images[j]);
            free(images);
            return -1;
        }
    }

    tiles->images = images;
    tiles->count = tai->count;
    return 0;
}

const fd2_image *fd2_tileset_get(const fd2_tileset *tiles, size_t idx) {
    if (!tiles || !tiles->images || idx >= tiles->count) return NULL;
    return &tiles->images[idx];
}

void fd2_tileset_close(fd2_tileset *tiles) {
    if (tiles && tiles->images) {
        for (size_t i = 0; i < tiles->count; i++) fd2_image_free(&tiles->images[i]);
        free(tiles->images);
    }
    if (tiles) memset(tiles, 0, sizeof(*tiles));
}

static int terrain_load_attrs(fd2_terrain_tileset *terrain,
                              const uint8_t *data, size_t size) {
    if (!data || size == 0 || (size % 4) != 0) return -1;

    size_t count = size / 4;
    fd2_terrain_attr *attrs = calloc(count, sizeof(*attrs));
    if (!attrs) return -1;

    for (size_t i = 0; i < count; i++) {
        attrs[i].flags = data[i * 4 + 0];
        attrs[i].movement_cost_class = data[i * 4 + 1];
        attrs[i].attr2 = data[i * 4 + 2];
        attrs[i].attr3 = data[i * 4 + 3];
    }

    terrain->attrs = attrs;
    terrain->attr_count = count;
    return 0;
}

int fd2_terrain_tileset_open(fd2_terrain_tileset *terrain,
                              const fd2_archive *fdshap,
                              size_t shape_index) {
    memset(terrain, 0, sizeof(*terrain));
    if (!fdshap) return -1;

    size_t sheet_entry = shape_index * 2;
    size_t attr_entry = sheet_entry + 1;
    if (attr_entry >= fdshap->count) return -1;

    if (fd2_shape_sheet_open_entry(&terrain->sheet, fdshap, sheet_entry) != 0)
        goto fail;

    const uint8_t *attr_data;
    size_t attr_size;
    if (fd2_archive_get(fdshap, attr_entry, &attr_data, &attr_size) != 0)
        goto fail;
    if (terrain_load_attrs(terrain, attr_data, attr_size) != 0)
        goto fail;

    terrain->frames = calloc(terrain->sheet.frame_count, sizeof(*terrain->frames));
    if (!terrain->frames) goto fail;

    for (size_t i = 0; i < terrain->sheet.frame_count; i++) {
        if (fd2_shape_sheet_decode_frame(&terrain->frames[i], &terrain->sheet, i) != 0)
            goto fail;
    }

    terrain->shape_index = shape_index;
    return 0;

fail:
    fd2_terrain_tileset_close(terrain);
    return -1;
}

int fd2_terrain_tileset_open_stage(fd2_terrain_tileset *terrain,
                                    const fd2_archive *fdshap,
                                    const fd2_archive *field,
                                    size_t stage_idx) {
    const uint8_t *meta;
    size_t meta_len;

    /* FDFIELD.DAT 每 3 条一组；组内第 1 条首字节即 FDSHAP 地形包编号。
     * 反编译中 `res_load(&DAT_00001a65, ..., *LAB_00003a55 * 2)` 以此编号
     * 加载 FDSHAP 偶数条目，奇数条目则是 FUN_00010580 使用的地形表。 */
    if (!field || fd2_archive_get(field, stage_idx * 3 + 1, &meta, &meta_len) != 0)
        return -1;
    if (meta_len == 0) return -1;

    return fd2_terrain_tileset_open(terrain, fdshap, (size_t)meta[0]);
}

const fd2_image *fd2_terrain_frame(const fd2_terrain_tileset *terrain,
                                   size_t frame_idx) {
    if (!terrain || !terrain->frames || frame_idx >= terrain->sheet.frame_count)
        return NULL;
    return &terrain->frames[frame_idx];
}

const fd2_terrain_attr *fd2_terrain_attr_get(const fd2_terrain_tileset *terrain,
                                             uint16_t terrain_id) {
    if (!terrain || !terrain->attrs || terrain_id >= terrain->attr_count)
        return NULL;
    return &terrain->attrs[terrain_id];
}

int fd2_terrain_base_frame_from_cell_animated(
                                      const fd2_terrain_tileset *terrain,
                                      uint32_t cell,
                                      int terrain_phase,
                                      int actor_phase,
                                      size_t *out_frame_idx) {
    uint16_t terrain_id = fd2_field_cell_terrain(cell);
    const fd2_terrain_attr *attr = fd2_terrain_attr_get(terrain, terrain_id);
    if (!terrain || !attr || terrain_id >= terrain->sheet.frame_count)
        return -1;
    if (terrain_phase < 0) terrain_phase = 0;
    if (actor_phase < 0) actor_phase = 0;
    size_t frame_index = terrain_id;
    /* field_view_render_tiles @0x3710c：0x08 动画跨过配套遮挡帧，
     * 0x10 使用单位 idle phase 的一半，0x04 使用逐 BIOS tick 相位。 */
    if ((attr->flags & 0x08u) != 0)
        frame_index += (size_t)(terrain_phase & 1) * 2u;
    else if ((attr->flags & 0x10u) != 0)
        frame_index += (size_t)((actor_phase & 3) / 2);
    else if ((attr->flags & 0x04u) != 0)
        frame_index += (size_t)(terrain_phase & 1);
    if (frame_index >= terrain->sheet.frame_count) return -1;
    if (out_frame_idx) *out_frame_idx = frame_index;
    return 0;
}

int fd2_terrain_base_frame_from_cell(const fd2_terrain_tileset *terrain,
                                      uint32_t cell,
                                      size_t *out_frame_idx) {
    return fd2_terrain_base_frame_from_cell_animated(
        terrain, cell, 0, 0, out_frame_idx);
}

int fd2_terrain_overlay_frame_from_cell(const fd2_terrain_tileset *terrain,
                                         uint32_t cell,
                                         int anim_phase,
                                         size_t *out_frame_idx) {
    uint16_t terrain_id = fd2_field_cell_terrain(cell);
    const fd2_terrain_attr *attr = fd2_terrain_attr_get(terrain, terrain_id);
    if (!terrain || !attr) return -1;
    if ((attr->flags & 0x80u) == 0) return 0;

    size_t frame_idx = terrain_id;
    if ((attr->flags & 0x08u) != 0) {
        if (anim_phase < 0) anim_phase = 0;
        frame_idx += (size_t)anim_phase * 2u;
    }
    frame_idx += 1u; /* FUN_0001020e 使用 offset_table[terrain_id + 1] 绘制遮挡层。 */
    if (frame_idx >= terrain->sheet.frame_count) return -1;

    if (out_frame_idx) *out_frame_idx = frame_idx;
    return 1;
}

void fd2_terrain_tileset_close(fd2_terrain_tileset *terrain) {
    if (!terrain) return;
    if (terrain->frames) {
        for (size_t i = 0; i < terrain->sheet.frame_count; i++)
            fd2_image_free(&terrain->frames[i]);
        free(terrain->frames);
    }
    free(terrain->attrs);
    fd2_shape_sheet_close(&terrain->sheet);
    memset(terrain, 0, sizeof(*terrain));
}

size_t fd2_tileset_preview_index_from_cell(uint32_t cell, size_t tile_count) {
    if (tile_count == 0) return 0;
    uint32_t idx = cell & 0xffu;
    if (idx >= tile_count) idx %= (uint32_t)tile_count;
    return (size_t)idx;
}

void fd2_tileset_blit(fd2_vga *vga, const fd2_image *img,
                      int x, int y, int transparent_index) {
    if (!vga || !img || !img->pixels) return;

    for (int row = 0; row < img->height; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= VGA_H) continue;

        for (int col = 0; col < img->width; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= VGA_W) continue;
            uint8_t px = img->pixels[(size_t)row * (size_t)img->width + (size_t)col];
            if (transparent_index >= 0 && px == (uint8_t)transparent_index)
                continue;
            vga->framebuffer[(size_t)dy * VGA_STRIDE + (size_t)dx] = px;
        }
    }
}

void fd2_tileset_blit_lut(fd2_vga *vga, const fd2_image *img,
                          int x, int y, int transparent_index,
                          const uint8_t lut[256]) {
    if (!vga || !img || !img->pixels || !lut) return;
    for (int row = 0; row < img->height; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= VGA_H) continue;
        for (int col = 0; col < img->width; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= VGA_W) continue;
            uint8_t px = img->pixels[(size_t)row * (size_t)img->width +
                                     (size_t)col];
            if (transparent_index >= 0 &&
                px == (uint8_t)transparent_index)
                continue;
            vga->framebuffer[(size_t)dy * VGA_STRIDE + (size_t)dx] = lut[px];
        }
    }
}

void fd2_terrain_render_field_base_animated(
                                   fd2_vga *vga,
                                   const fd2_terrain_tileset *terrain,
                                   const fd2_field_map *map,
                                   int camera_x, int camera_y,
                                   int terrain_phase, int actor_phase) {
    if (!vga || !terrain || !map) return;
    fd2_vga_clear(vga, 0);

    /* 战场底图：对照 field_view_render_tiles @0x3710c，按 terrain_id
     * 与 flags 0x04/0x08/0x10 选择 FDSHAP 24×24 动画帧。 */
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            uint32_t cell = fd2_field_map_cell(map, x, y);
            size_t frame_idx;
            if (fd2_terrain_base_frame_from_cell_animated(
                    terrain, cell, terrain_phase, actor_phase,
                    &frame_idx) != 0)
                continue;
            const fd2_image *img = fd2_terrain_frame(terrain, frame_idx);
            int sx = x * FD2_FIELD_TILE_STEP - camera_x;
            int sy = y * FD2_FIELD_TILE_STEP - camera_y;
            fd2_tileset_blit(vga, img, sx, sy, 0);
        }
    }
}

void fd2_terrain_render_field_base(fd2_vga *vga,
                                   const fd2_terrain_tileset *terrain,
                                   const fd2_field_map *map,
                                   int camera_x, int camera_y) {
    fd2_terrain_render_field_base_animated(vga, terrain, map,
                                           camera_x, camera_y, 0, 0);
}

void fd2_terrain_render_field_overlay(fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int camera_x, int camera_y,
                                      int anim_phase) {
    if (!vga || !terrain || !map) return;

    /* 遮挡层：复现 FUN_0001020e @0x37cda 对地形表 flags 的处理。
     * 原版在 actor 绘制后重绘附近遮挡格，使树冠/建筑位于角色上方。 */
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            uint32_t cell = fd2_field_map_cell(map, x, y);
            size_t frame_idx;
            int r = fd2_terrain_overlay_frame_from_cell(terrain, cell, anim_phase, &frame_idx);
            if (r <= 0) continue;
            const fd2_image *img = fd2_terrain_frame(terrain, frame_idx);
            int sx = x * FD2_FIELD_TILE_STEP - camera_x;
            int sy = y * FD2_FIELD_TILE_STEP - camera_y;
            fd2_tileset_blit(vga, img, sx, sy, 0);
        }
    }
}

void fd2_terrain_render_field_preview(fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int camera_x, int camera_y,
                                      int anim_phase) {
    fd2_terrain_render_field_base(vga, terrain, map, camera_x, camera_y);
    fd2_terrain_render_field_overlay(vga, terrain, map, camera_x, camera_y,
                                     anim_phase);
}

static int64_t fixed_cell_floor(int64_t value, int64_t *remainder) {
    int64_t cell = value / 0xc00;
    int64_t rem = value % 0xc00;
    if (rem < 0) {
        rem += 0xc00;
        cell--;
    }
    if (remainder) *remainder = rem;
    return cell;
}

void fd2_terrain_render_field_scaled_animated(
                                      fd2_vga *vga,
                                      const fd2_terrain_tileset *terrain,
                                      const fd2_field_map *map,
                                      int center_x_fp, int center_y_fp,
                                      int step_fp,
                                      int terrain_phase, int actor_phase) {
    if (!vga || !terrain || !map || step_fp <= 0) return;
    /* field_view_render_scaled @0x44776 以视窗中心 (156,96) 回推
     * 起点，tile fixed-point 宽度为 0xc00，单像素为 0x80。 */
    int64_t start_x = (int64_t)center_x_fp - (int64_t)step_fp * 156;
    int64_t source_y = (int64_t)center_y_fp - (int64_t)step_fp * 96;
    for (int y = 0; y < 192; y++, source_y += step_fp) {
        int64_t rem_y;
        int64_t cell_y = fixed_cell_floor(source_y, &rem_y);
        int pixel_y = (int)(rem_y / 0x80);
        int64_t source_x = start_x;
        for (int x = 0; x < 312; x++, source_x += step_fp) {
            uint8_t pixel = 0;
            int64_t rem_x;
            int64_t cell_x = fixed_cell_floor(source_x, &rem_x);
            if (cell_x >= 0 && cell_x < map->width &&
                cell_y >= 0 && cell_y < map->height) {
                uint32_t cell = fd2_field_map_cell(map, cell_x, cell_y);
                size_t frame_index;
                if (fd2_terrain_base_frame_from_cell_animated(
                        terrain, cell, terrain_phase, actor_phase,
                        &frame_index) == 0) {
                    const fd2_image *image = fd2_terrain_frame(
                        terrain, frame_index);
                    int pixel_x = (int)(rem_x / 0x80);
                    if (image && image->pixels && pixel_x < image->width &&
                        pixel_y < image->height)
                        pixel = image->pixels[(size_t)pixel_y *
                                              (size_t)image->width +
                                              (size_t)pixel_x];
                }
            }
            vga->framebuffer[(size_t)(y + 4) * VGA_STRIDE +
                             (size_t)(x + 4)] = pixel;
        }
    }
}

void fd2_tileset_render_field_preview(fd2_vga *vga,
                                      const fd2_tileset *tiles,
                                      const fd2_field_map *map,
                                      int camera_x, int camera_y) {
    if (!vga || !tiles || !map) return;

    fd2_vga_clear(vga, 0);
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            uint32_t cell = fd2_field_map_cell(map, x, y);
            size_t idx = fd2_tileset_preview_index_from_cell(cell, tiles->count);
            const fd2_image *img = fd2_tileset_get(tiles, idx);
            int sx = x * FD2_FIELD_TILE_STEP - camera_x;
            int sy = y * FD2_FIELD_TILE_STEP - camera_y;
            fd2_tileset_blit(vga, img, sx, sy, 0);
        }
    }
}
