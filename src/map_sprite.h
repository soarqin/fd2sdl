#ifndef FD2_MAP_SPRITE_H
#define FD2_MAP_SPRITE_H

#include <stddef.h>
#include <stdint.h>

#include "image.h"
#include "vga.h"

/* FDICON.B24 完整帧源 / FD2.TMP 运行期帧缓存
 *
 * 逆向依据：fdicon_cache_append_unit @0x4622d 从 FDICON.B24 复制单位
 * 12 帧到 FD2.TMP；fd2tmp_map_sprite_load @0x53b6f 读入缓存；
 * map_actor_blit_24x24 @0x42c34 使用
 * `(direction*3 + cache_class*0x0c + frame) * 4` 查偏移表。
 */
typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t *offsets;
    uint8_t *decoded_frames; /* frame_count × 24 × 24，避免逐帧重复解码/分配 */
    size_t frame_count;
} fd2_map_sprite_bank;

int  fd2_map_sprite_bank_open(fd2_map_sprite_bank *bank, const char *path);
int  fd2_map_sprite_decode_frame(fd2_image *img,
                                 const fd2_map_sprite_bank *bank,
                                 size_t frame_idx);
void fd2_map_sprite_bank_close(fd2_map_sprite_bank *bank);

void fd2_map_sprite_blit_frame(fd2_vga *vga,
                               const fd2_map_sprite_bank *bank,
                               size_t frame_idx,
                               int x, int y, int transparent_index);
void fd2_map_sprite_blit(fd2_vga *vga, const fd2_image *img,
                         int x, int y, int transparent_index);

#endif /* FD2_MAP_SPRITE_H */
