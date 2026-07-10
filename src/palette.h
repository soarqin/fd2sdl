#ifndef FD2_PALETTE_H
#define FD2_PALETTE_H

#include <stdint.h>

/* 原版调色板：256 色，VGA 6-bit（0-63），存于 FDOTHER.DAT[0]（768 字节）。
 * 渲染时需把 6-bit 值扩展为 8-bit。 */

#define FD2_PALETTE_SIZE 768   /* 256 * 3 */

/* 把 6-bit VGA 值转为 8-bit（<<2 | >>4，含低位精度） */
uint8_t fd2_pal_expand6(uint8_t v);

#endif /* FD2_PALETTE_H */
