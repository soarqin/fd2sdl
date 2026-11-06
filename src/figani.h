#ifndef FD2_FIGANI_H
#define FD2_FIGANI_H

/* FIGANI 结构依据 sub_2EB9F @0x2eb9f、sub_2EBE1 @0x2ebe1 与
 * sub_30E9D @0x30e9d；图像流复用 blit_image_clipped @0x4e98d
 * （原登记 FUN_0004c0d5）的 RLE。此接口不执行动作、音效或播放循环。 */
#include <stddef.h>
#include <stdint.h>

#include "archive.h"

typedef struct {
    const uint8_t *data;
    size_t size;
    uint8_t part_count;
    uint8_t split_mode;
    uint8_t intro_end;
    uint8_t reserved3;
    uint8_t bank_selector;
    uint8_t reserved_tail[3];
    int empty; /* 仅精确的三字节 00 00 0a 哨兵。 */
} fd2_figani_clip;

typedef struct {
    uint16_t x, y; /* part+0/+2 是无符号绘制位置，不是图像宽高。 */
    uint8_t effect_marker;
    uint8_t cue;
    uint8_t duration;
    uint8_t layer_flags;
    uint8_t reserved;
    uint16_t width, height;
    const uint8_t *image_data; /* part+9：保留宽高头和原始 RLE。 */
    size_t image_size;
} fd2_figani_part;

/* 完整验证一个条目的目录、各 part 和 RLE，成功返回 0，失败返回 -1。
 * 仅借用输入，调用方须保证其存活且不再修改；无需关闭 clip。
 * 保留所有元数据，不把 intro_end > part_count、duration == 0 或未知
 * selector/标记当结构损坏；播放方另行检查自身循环和资源准入。
 * 所有接口失败均不改写输出结构。 */
int fd2_figani_parse(fd2_figani_clip *out, const uint8_t *data, size_t size);
int fd2_figani_open_entry(fd2_figani_clip *out, const fd2_archive *archive,
                          size_t index);

/* 从已验证且源数据未变的 clip 取出 part；不复制图像，也不推断透明色。
 * image_data 可交给现有 image 解码接口；SKIP 与显式颜色 0 仍有区别。 */
int fd2_figani_get_part(const fd2_figani_clip *clip, size_t index,
                        fd2_figani_part *out);

typedef struct {
    fd2_figani_clip clip; /* 借用archive；empty是合法哨兵，不能伪造为有part。 */
    size_t selected_entry;
    uint8_t cost_marker_count;
} fd2_figani_magic_cast;

/* 2ff01先取unit_id*3+2，empty才退+1；30e9d仅在intro_end内part+4==1
 * 调用费用。非empty的intro越界拒绝；空的最终选择仍返回成功/empty供caller
 * 区分合法容器与未支持播放。无分配所有权、RNG、费用写入；失败输出不变。 */
int fd2_figani_magic_cast_plan(fd2_figani_magic_cast *out,
    const fd2_archive *archive, uint8_t unit_id);

#endif /* FD2_FIGANI_H */
