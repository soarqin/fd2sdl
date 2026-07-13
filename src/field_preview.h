#ifndef FD2_FIELD_PREVIEW_H
#define FD2_FIELD_PREVIEW_H

#include <stddef.h>

#include "archive.h"
#include "vga.h"

/* 独立战场单位预览。当前正式验收目标是新游戏第一关 stage 0：
 * 从 FDFIELD metadata/placement 建立通用单位表，再用 FDICON.B24 绘制。 */
int fd2_field_preview_run(fd2_vga *vga,
                          const fd2_archive *fdother,
                          size_t stage,
                          int once);

#endif /* FD2_FIELD_PREVIEW_H */
