#ifndef FD2_FIELD_PAGE_PRIVATE_H
#define FD2_FIELD_PAGE_PRIVATE_H

#include "field_page.h"

/* 仅page/background及info的有限summary painter共享；App唯一owner。
 * painter只在当前调用借用像素，禁止Game复制/保存这些指针。 */
typedef struct {
    uint8_t *source, *work;
    size_t source_size, work_size;
    uint32_t last_tick;
    uint8_t kind, phase, scroll_speed, initialized;
    int width, height;
} fd2_field_background_state;

struct fd2_field_page {
    uint8_t pixels[FD2_FIELD_PAGE_SIZE];
    fd2_field_background_state background;
};

#endif
