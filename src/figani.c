/* FIGANI：复现 sub_2EB9F @0x2eb9f 的 entry+8 DWORD 目录及 part+9
 * 图像位置；sub_2EBE1 @0x2ebe1、sub_30E9D @0x30e9d 分别消费头部
 * 与 part 的播放字段。原登记名均保留中性名称，不据此猜测原函数 ABI。 */
#include "figani.h"

#include <string.h>

#include "image.h"

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int part_bounds(const fd2_figani_clip *clip, size_t index,
                        size_t *start, size_t *end) {
    if (!clip || !clip->data || clip->empty || !clip->part_count ||
        index >= clip->part_count)
        return -1;
    size_t directory_end = 8u + (size_t)clip->part_count * 4u;
    if (directory_end > clip->size) return -1;
    size_t first = rd_u32_le(clip->data + 8);
    size_t a = rd_u32_le(clip->data + 8u + index * 4u);
    size_t b = index + 1u < clip->part_count
        ? rd_u32_le(clip->data + 8u + (index + 1u) * 4u) : clip->size;
    if (first != directory_end || a < directory_end || a > clip->size ||
        b > clip->size || b < a || b - a < 13u)
        return -1;
    *start = a;
    *end = b;
    return 0;
}

int fd2_figani_parse(fd2_figani_clip *out, const uint8_t *data, size_t size) {
    if (!out || !data) return -1;
    fd2_figani_clip parsed;
    memset(&parsed, 0, sizeof(parsed));
    parsed.data = data;
    parsed.size = size;
    if (size == 3u && data[0] == 0 && data[1] == 0 && data[2] == 10) {
        parsed.empty = 1;
        *out = parsed;
        return 0;
    }
    if (size < 8u || !data[0]) return -1;
    parsed.part_count = data[0];
    parsed.split_mode = data[1];
    parsed.intro_end = data[2];
    parsed.reserved3 = data[3];
    parsed.bank_selector = data[4];
    memcpy(parsed.reserved_tail, data + 5, sizeof(parsed.reserved_tail));
    for (size_t i = 0; i < parsed.part_count; i++) {
        size_t start, end;
        fd2_image image;
        if (part_bounds(&parsed, i, &start, &end) != 0 ||
            fd2_image_decode_buf(&image, data + start + 9u,
                                  end - start - 9u) != 0)
            return -1;
        fd2_image_free(&image);
    }
    *out = parsed;
    return 0;
}

int fd2_figani_open_entry(fd2_figani_clip *out, const fd2_archive *archive,
                          size_t index) {
    const uint8_t *data;
    size_t size;
    if (!out || fd2_archive_get(archive, index, &data, &size) != 0) return -1;
    return fd2_figani_parse(out, data, size);
}

int fd2_figani_get_part(const fd2_figani_clip *clip, size_t index,
                        fd2_figani_part *out) {
    size_t start, end;
    if (!out || part_bounds(clip, index, &start, &end) != 0) return -1;
    const uint8_t *p = clip->data + start;
    fd2_figani_part part;
    memset(&part, 0, sizeof(part));
    part.x = rd_u16_le(p);
    part.y = rd_u16_le(p + 2);
    part.effect_marker = p[4];
    part.cue = p[5];
    part.duration = p[6];
    part.layer_flags = p[7];
    part.reserved = p[8];
    part.width = rd_u16_le(p + 9);
    part.height = rd_u16_le(p + 11);
    part.image_data = p + 9;
    part.image_size = end - start - 9u;
    *out = part;
    return 0;
}

/* field_magic_battle_animation @0x2ff01 / sub_30E9D @0x30e9d（原登记同名）：
 * 从真实条目导出费用标记，不能把unit9的一次费用当所有caster默认值。 */
int fd2_figani_magic_cast_plan(fd2_figani_magic_cast *out,
    const fd2_archive *archive, uint8_t unit_id) {
    if (!out || !archive) return -1;
    fd2_figani_magic_cast plan = {0};
    plan.selected_entry = (size_t)unit_id * 3u + 2u;
    if (fd2_figani_open_entry(&plan.clip, archive, plan.selected_entry) != 0)
        return -1;
    if (plan.clip.empty) {
        plan.selected_entry--;
        if (fd2_figani_open_entry(&plan.clip, archive, plan.selected_entry) != 0)
            return -1;
    }
    if (!plan.clip.empty) {
        if (plan.clip.intro_end > plan.clip.part_count) return -1;
        for (size_t i = 0; i < plan.clip.intro_end; i++) {
            fd2_figani_part part;
            if (fd2_figani_get_part(&plan.clip, i, &part) != 0) return -1;
            if (part.effect_marker == 1u) plan.cost_marker_count++;
        }
    }
    *out = plan;
    return 0;
}
