#include "palette.h"

uint8_t fd2_pal_expand6(uint8_t v) {
    /* VGA 6-bit (0-63) -> 8-bit (0-255)，保留低位精度 */
    return (uint8_t)((v << 2) | (v >> 4));
}
