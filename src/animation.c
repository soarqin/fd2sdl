#include "animation.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int afm_decode_palette_rle(uint8_t dst[VGA_PALETTE_SIZE],
                                  const uint8_t *src, size_t len,
                                  size_t *pos) {
    /* FUN_000345ad：AFM palette RLE。
     * 控制字节高 2 位为 0xc0 时，低 6 位是重复次数，下一字节为值；
     * 否则控制字节本身就是 literal。注意这里没有 image.c RLE 的 +1。 */
    size_t out = 0;
    while (out < VGA_PALETTE_SIZE) {
        if (*pos >= len) return -1;
        uint8_t c = src[(*pos)++];
        if ((c & 0xc0) == 0xc0) {
            size_t count = c & 0x3f;
            if (*pos >= len || out + count > VGA_PALETTE_SIZE) return -1;
            memset(dst + out, src[(*pos)++], count);
            out += count;
        } else {
            dst[out++] = c;
        }
    }
    return 0;
}

static int afm_decode_frame_rle(uint8_t *dst, size_t dst_len,
                                const uint8_t *src, size_t len,
                                size_t *pos) {
    /* FUN_0003466c：AFM framebuffer RLE。
     * 该格式与 FDOTHER 图像 RLE 不同：只有 0xc0-0xff 表示 run，
     * 低 6 位为重复次数；其他字节直接写入显存。 */
    size_t out = 0;
    while (out < dst_len) {
        if (*pos >= len) return -1;
        uint8_t c = src[(*pos)++];
        if ((c & 0xc0) == 0xc0) {
            size_t count = c & 0x3f;
            if (*pos >= len || out + count > dst_len) return -1;
            memset(dst + out, src[(*pos)++], count);
            out += count;
        } else {
            dst[out++] = c;
        }
    }
    return 0;
}

static int afm_exec_frame(fd2_vga *vga, const uint8_t *data, size_t len,
                          uint16_t command_count) {
    size_t pos = 0;
    for (uint16_t cmd = 0; cmd < command_count; cmd++) {
        if (pos >= len) return -1;
        uint8_t op = data[pos++];
        switch (op) {
        case 0:
            /* no-op */
            break;
        case 1:
            /* FUN_0003459f：直接写入 0x300 字节调色板。 */
            if (pos + VGA_PALETTE_SIZE > len) return -1;
            fd2_vga_set_palette(vga, data + pos);
            pos += VGA_PALETTE_SIZE;
            break;
        case 2: {
            uint8_t pal[VGA_PALETTE_SIZE];
            if (afm_decode_palette_rle(pal, data, len, &pos) != 0) return -1;
            fd2_vga_set_palette(vga, pal);
            break;
        }
        case 4:
            /* FUN_00034628：单字节填充整块显存。 */
            if (pos >= len) return -1;
            memset(vga->framebuffer, data[pos++], VGA_W * VGA_H);
            break;
        case 5:
            /* FUN_00034650：直接复制整块显存。启动 ANI 暂未使用，保留实现。 */
            if (pos + VGA_W * VGA_H > len) return -1;
            memcpy(vga->framebuffer, data + pos, VGA_W * VGA_H);
            pos += VGA_W * VGA_H;
            break;
        case 6:
            if (afm_decode_frame_rle(vga->framebuffer, VGA_W * VGA_H,
                                     data, len, &pos) != 0) return -1;
            break;
        case 7: {
            /* FUN_000346b1：count 个 (offset:u16, value:u8)。 */
            if (pos + 2 > len) return -1;
            uint16_t count = rd_u16_le(data + pos);
            pos += 2;
            for (uint16_t i = 0; i < count; i++) {
                if (pos + 3 > len) return -1;
                uint16_t off = rd_u16_le(data + pos);
                uint8_t value = data[pos + 2];
                pos += 3;
                if (off < VGA_W * VGA_H) vga->framebuffer[off] = value;
            }
            break;
        }
        case 8: {
            /* FUN_000346ca：count 个 (offset:u16, run:u8, value:u8)。 */
            if (pos + 2 > len) return -1;
            uint16_t count = rd_u16_le(data + pos);
            pos += 2;
            for (uint16_t i = 0; i < count; i++) {
                if (pos + 4 > len) return -1;
                uint16_t off = rd_u16_le(data + pos);
                uint8_t run = data[pos + 2];
                uint8_t value = data[pos + 3];
                pos += 4;
                if (off < VGA_W * VGA_H) {
                    size_t n = run;
                    if (off + n > VGA_W * VGA_H) n = VGA_W * VGA_H - off;
                    memset(vga->framebuffer + off, value, n);
                }
            }
            break;
        }
        case 9: {
            /* FUN_000346f4：count 个 (offset:u16, len:u8, literal[len])。 */
            if (pos + 2 > len) return -1;
            uint16_t count = rd_u16_le(data + pos);
            pos += 2;
            for (uint16_t i = 0; i < count; i++) {
                if (pos + 3 > len) return -1;
                uint16_t off = rd_u16_le(data + pos);
                uint8_t run = data[pos + 2];
                pos += 3;
                if (pos + run > len) return -1;
                if (off < VGA_W * VGA_H) {
                    size_t n = run;
                    if (off + n > VGA_W * VGA_H) n = VGA_W * VGA_H - off;
                    memcpy(vga->framebuffer + off, data + pos, n);
                }
                pos += run;
            }
            break;
        }
        default:
            return -1;
        }
    }
    return (pos == len) ? 0 : -1;
}

int fd2_animation_play(fd2_vga *vga, const fd2_archive *ani,
                       int anim_idx, uint32_t frame_delay_ms,
                       int check_input) {
    const uint8_t *entry;
    size_t entry_len;
    if (anim_idx < 0 || fd2_archive_get(ani, (size_t)anim_idx, &entry, &entry_len) != 0) {
        return -1;
    }
    if (entry_len < 0xad) return -1;
    if (memcmp(entry, "AFM - Animation File Manager", 28) != 0) return -1;

    uint16_t frame_count = rd_u16_le(entry + 0xa5);
    uint16_t width = rd_u16_le(entry + 0xa7);
    uint16_t height = rd_u16_le(entry + 0xa9);
    uint16_t screen_size = rd_u16_le(entry + 0xab);
    if (width != VGA_W || height != VGA_H || screen_size != VGA_W * VGA_H) {
        return -1;
    }

    size_t pos = 0xad;
    for (uint16_t frame = 0; frame < frame_count; frame++) {
        if (pos + 8 > entry_len) return -1;
        uint16_t data_len = rd_u16_le(entry + pos);
        uint16_t command_count = rd_u16_le(entry + pos + 2);
        pos += 8;
        if (pos + data_len > entry_len) return -1;

        if (afm_exec_frame(vga, entry + pos, data_len, command_count) != 0) {
            return -1;
        }
        pos += data_len;

        fd2_vga_present_timed(vga, frame_delay_ms);
        if (check_input && fd2_input_check(vga)) return 1;
    }
    return 0;
}
