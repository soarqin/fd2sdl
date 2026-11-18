/* owned HWND driver的有限modifier+F1..F10协议；不读/改用户全局键态。
 * BIOS预期scan仅是IBM兼容键表映射，不是DOSBox实际消费证据。
 * 原stage行6238d前3B由2670e比较扩展scan；实测另留收据。 */
#ifndef FD2_FIELD_PLAY_KEY_CHORD_H
#define FD2_FIELD_PLAY_KEY_CHORD_H

#include <string.h>

typedef struct {
    unsigned short modifier_vk;
    unsigned short function_vk;
    unsigned bios_scan_expected;
} fd2_driver_key_chord;

static int fd2_driver_key_chord_parse(const char *name, fd2_driver_key_chord *out) {
    unsigned short modifier;
    unsigned base;
    const char *function;
    if (!name || !out) return -1;
    if (!strncmp(name, "SHIFT_", 6)) {
        modifier = 0x10; base = 84; function = name + 6;
    } else if (!strncmp(name, "CTRL_", 5)) {
        modifier = 0x11; base = 94; function = name + 5;
    } else if (!strncmp(name, "ALT_", 4)) {
        modifier = 0x12; base = 104; function = name + 4;
    } else return -1;
    unsigned number;
    if (function[0] != 'F') return -1;
    if (function[1] >= '1' && function[1] <= '9' && !function[2])
        number = (unsigned)(function[1] - '0');
    else if (function[1] == '1' && function[2] == '0' && !function[3])
        number = 10;
    else return -1;
    fd2_driver_key_chord result = {modifier, (unsigned short)(0x70u + number - 1u), base + number - 1u};
    *out = result;
    return 0;
}

#endif
