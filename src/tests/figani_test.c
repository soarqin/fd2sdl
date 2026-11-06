/* FIGANI 结构回归：sub_2EB9F @0x2eb9f 的目录与 part+9 图像、
 * sub_2EBE1 @0x2ebe1 / sub_30E9D @0x30e9d 的原始字段读集。
 * 原版 408 条目/2118 parts 来自独立 Domain 原指令及全资源分类，
 * 不把 part 数量、音效标记或时长解释成战斗命中数。 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "figani.h"
#include "image.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

static void put_u16_le(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32_le(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(value >> (8u * i));
}

static uint64_t hash_bytes(const uint8_t *data, size_t size) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void make_clip(uint8_t data[48]) {
    memset(data, 0, 48);
    data[0] = 2;
    data[1] = 1;
    data[2] = 18; /* 结构层允许 prefix 超出目录；调用方另验播放范围。 */
    data[3] = 0xa3;
    data[4] = 0xe2;
    data[5] = 0x11;
    data[6] = 0x22;
    data[7] = 0x33;
    put_u32_le(data + 8, 16);
    put_u32_le(data + 12, 32);
    put_u16_le(data + 16, 0xf123);
    put_u16_le(data + 18, 0xf456);
    data[20] = 0xd4;
    data[21] = 0xe5;
    data[22] = 0; /* 合法零时长，不能在解析层归一或拒绝。 */
    data[23] = 0xf7;
    data[24] = 0xa8;
    put_u16_le(data + 25, 2);
    put_u16_le(data + 27, 1);
    data[29] = 0x81;
    data[30] = 1;
    data[31] = 2;
    put_u16_le(data + 32, 7);
    put_u16_le(data + 34, 8);
    data[38] = 3;
    put_u16_le(data + 41, 2);
    put_u16_le(data + 43, 1);
    data[45] = 0x01;
    data[46] = 3;
}

static int rejects_without_output_write(const uint8_t *data, size_t size) {
    fd2_figani_clip out;
    unsigned char before[sizeof(out)];
    memset(&out, 0x5a, sizeof(out));
    memcpy(before, &out, sizeof(out));
    CHECK(fd2_figani_parse(&out, data, size) == -1);
    CHECK(memcmp(before, &out, sizeof(out)) == 0);
    return 0;
}

static int test_fields_and_borrowing(void) {
    uint8_t data[48], before[48];
    make_clip(data);
    memcpy(before, data, sizeof(data));
    fd2_figani_clip clip;
    fd2_figani_part part;
    CHECK(fd2_figani_parse(&clip, data, 47) == 0);
    CHECK(clip.data == data && clip.size == 47 && !clip.empty);
    CHECK(clip.part_count == 2 && clip.split_mode == 1 && clip.intro_end == 18);
    CHECK(clip.reserved3 == 0xa3 && clip.bank_selector == 0xe2);
    CHECK(memcmp(clip.reserved_tail, data + 5, 3) == 0);
    CHECK(fd2_figani_get_part(&clip, 0, &part) == 0);
    CHECK(part.x == 0xf123 && part.y == 0xf456);
    CHECK(part.effect_marker == 0xd4 && part.cue == 0xe5 && part.duration == 0);
    CHECK(part.layer_flags == 0xf7 && part.reserved == 0xa8);
    CHECK(part.width == 2 && part.height == 1);
    CHECK(part.image_data == data + 25 && part.image_size == 7);
    fd2_image image;
    CHECK(fd2_image_decode_buf(&image, part.image_data, part.image_size) == 0);
    CHECK(image.pixels[0] == 1 && image.pixels[1] == 2);
    fd2_image_free(&image);
    CHECK(fd2_figani_get_part(&clip, 1, &part) == 0);
    CHECK(part.x == 7 && part.y == 8 && part.duration == 3);
    CHECK(part.image_data == data + 41 && part.image_size == 6);
    CHECK(memcmp(before, data, sizeof(data)) == 0);

    uint8_t archive_data[61] = "LLLLLL";
    put_u32_le(archive_data + 6, 14);
    put_u32_le(archive_data + 10, sizeof(archive_data));
    memcpy(archive_data + 14, data, 47);
    fd2_archive archive;
    CHECK(fd2_archive_open_mem(&archive, archive_data, sizeof(archive_data)) == 0);
    CHECK(archive.count == 1);
    CHECK(fd2_figani_open_entry(&clip, &archive, 0) == 0);
    CHECK(clip.data == archive_data + 14 && clip.part_count == 2);
    unsigned char saved[sizeof(clip)];
    memcpy(saved, &clip, sizeof(clip));
    CHECK(fd2_figani_open_entry(&clip, &archive, 1) == -1);
    CHECK(memcmp(saved, &clip, sizeof(clip)) == 0);
    CHECK(fd2_figani_open_entry(&clip, NULL, 0) == -1);
    CHECK(memcmp(saved, &clip, sizeof(clip)) == 0);
    fd2_archive_close(&archive);
    return 0;
}

static int test_empty_and_error_atomicity(void) {
    static const uint8_t empty[] = {0, 0, 10};
    fd2_figani_clip clip;
    CHECK(fd2_figani_parse(&clip, empty, sizeof(empty)) == 0);
    CHECK(clip.empty && !clip.part_count && clip.data == empty && clip.size == 3);
    fd2_figani_part part;
    unsigned char saved[sizeof(part)];
    memset(&part, 0x5a, sizeof(part));
    memcpy(saved, &part, sizeof(part));
    CHECK(fd2_figani_get_part(&clip, 0, &part) == -1);
    CHECK(memcmp(saved, &part, sizeof(part)) == 0);
    CHECK(rejects_without_output_write(NULL, 3) == 0);
    CHECK(rejects_without_output_write(empty, 2) == 0);
    uint8_t bad_empty[8] = {0, 0, 10};
    CHECK(rejects_without_output_write(bad_empty, 4) == 0);
    CHECK(rejects_without_output_write(bad_empty, 8) == 0);
    bad_empty[2] = 9;
    CHECK(rejects_without_output_write(bad_empty, 3) == 0);
    CHECK(fd2_figani_parse(NULL, empty, sizeof(empty)) == -1);
    CHECK(fd2_figani_get_part(NULL, 0, &part) == -1);
    CHECK(memcmp(saved, &part, sizeof(part)) == 0);

    uint8_t data[48];
    make_clip(data);
    CHECK(rejects_without_output_write(data, 7) == 0);
    data[0] = 255;
    CHECK(rejects_without_output_write(data, 47) == 0);
    static const uint32_t bad_offsets[][2] = {
        {15, 32}, {17, 32}, {16, 16}, {32, 16}, {16, 15},
        {16, 28}, {16, 48}, {UINT32_MAX, 32}, {16, UINT32_MAX}
    };
    for (size_t i = 0; i < sizeof(bad_offsets) / sizeof(bad_offsets[0]); i++) {
        make_clip(data);
        put_u32_le(data + 8, bad_offsets[i][0]);
        put_u32_le(data + 12, bad_offsets[i][1]);
        CHECK(rejects_without_output_write(data, 47) == 0);
    }
    make_clip(data);
    CHECK(fd2_figani_parse(&clip, data, 47) == 0);
    CHECK(fd2_figani_get_part(&clip, SIZE_MAX, &part) == -1);
    CHECK(memcmp(saved, &part, sizeof(part)) == 0);
    CHECK(fd2_figani_get_part(&clip, 2, &part) == -1);
    CHECK(memcmp(saved, &part, sizeof(part)) == 0);
    CHECK(fd2_figani_get_part(&clip, 0, NULL) == -1);
    /* 即使调用方违约改坏借用目录，也不得越界或部分写入输出。 */
    put_u32_le(data + 12, UINT32_MAX);
    CHECK(fd2_figani_get_part(&clip, 0, &part) == -1);
    CHECK(memcmp(saved, &part, sizeof(part)) == 0);
    return 0;
}

static int test_rle_and_image_boundaries(void) {
    uint8_t data[48];
    static const uint8_t bad_commands[] = {0x02, 0x82, 0x41, 0xc2, 0x81, 0x00};
    for (size_t i = 0; i < sizeof(bad_commands); i++) {
        make_clip(data);
        data[45] = bad_commands[i];
        CHECK(rejects_without_output_write(data, 47) == 0);
    }
    make_clip(data);
    CHECK(rejects_without_output_write(data, 46) == 0); /* 末 part 缺 RUN 值。 */
    CHECK(rejects_without_output_write(data, 44) == 0); /* 末 part 图像头截断。 */
    CHECK(rejects_without_output_write(data, 48) == 0); /* 不吞掉 RLE 尾部垃圾。 */
    static const uint16_t bad_sizes[][2] = {{0, 1}, {2, 0}, {4097, 1}, {2, 2}};
    for (size_t i = 0; i < sizeof(bad_sizes) / sizeof(bad_sizes[0]); i++) {
        make_clip(data);
        put_u16_le(data + 41, bad_sizes[i][0]);
        put_u16_le(data + 43, bad_sizes[i][1]);
        CHECK(rejects_without_output_write(data, 47) == 0);
    }

    /* 保留原流，显式颜色 0 写入，而 SKIP/stride-2 空位不覆盖背景。 */
    uint8_t transparency[30] = {1};
    put_u32_le(transparency + 8, 12);
    put_u16_le(transparency + 21, 4);
    put_u16_le(transparency + 23, 1);
    static const uint8_t rle[] = {0x00, 0, 0xc0, 0x40, 7};
    memcpy(transparency + 25, rle, sizeof(rle));
    fd2_figani_clip clip;
    fd2_figani_part part;
    CHECK(fd2_figani_parse(&clip, transparency, sizeof(transparency)) == 0);
    CHECK(fd2_figani_get_part(&clip, 0, &part) == 0);
    CHECK(part.image_data == transparency + 21 && part.image_size == 9);
    uint8_t pixels[4] = {0xa5, 0xa5, 0xa5, 0xa5};
    static const uint8_t expected[] = {0, 0xa5, 0xa5, 7};
    CHECK(fd2_image_decode_rle_pixels(pixels, part.width, part.height,
              part.image_data + 4, part.image_size - 4) == 0);
    CHECK(memcmp(pixels, expected, sizeof(pixels)) == 0);
    return 0;
}

static int test_actual_figani(const char *path) {
    fd2_archive archive;
    CHECK(fd2_archive_open(&archive, path) == 0);
    CHECK(archive.count == 408 && archive.size == 15310188);
    uint64_t before = hash_bytes(archive.data, archive.size);
    size_t empty_count = 0, part_count = 0, markers = 0, layers = 0;
    size_t zero_duration = 0, three_duration = 0;
    for (size_t i = 0; i < archive.count; i++) {
        fd2_figani_clip clip;
        CHECK(fd2_figani_open_entry(&clip, &archive, i) == 0);
        if (clip.empty) {
            static const uint8_t sentinel[] = {0, 0, 10};
            CHECK(clip.size == 3 && memcmp(clip.data, sentinel, 3) == 0);
            empty_count++;
            continue;
        }
        CHECK(clip.part_count > 0 && clip.part_count <= 35);
        CHECK(clip.split_mode <= 1 && clip.bank_selector <= 6);
        CHECK(clip.reserved_tail[0] == 0 && clip.reserved_tail[1] == 0 &&
              clip.reserved_tail[2] == 0);
        CHECK(clip.reserved3 == (i == 13 ? 3 : 0));
        if (i == 355) CHECK(clip.part_count == 10 && clip.intro_end == 18);
        for (size_t j = 0; j < clip.part_count; j++) {
            fd2_figani_part part;
            CHECK(fd2_figani_get_part(&clip, j, &part) == 0);
            CHECK(part.width >= 3 && part.width <= 320);
            CHECK(part.height >= 3 && part.height <= 200);
            CHECK(part.x <= 289 && part.y <= 142);
            CHECK((unsigned)part.x + part.width <= 320 &&
                  (unsigned)part.y + part.height <= 200);
            CHECK(part.effect_marker <= 1 && part.layer_flags <= 1);
            CHECK(part.cue <= 6 && part.reserved == 0);
            markers += part.effect_marker;
            layers += part.layer_flags;
            if (part.duration == 0) {
                CHECK(i == 337 && j == 19);
                zero_duration++;
            }
            if (part.duration == 3) {
                CHECK(i == 370 && j == 8);
                three_duration++;
            }
            if (i == 0 && j == 0) {
                CHECK(part.x == 167 && part.y == 75);
                CHECK(part.width == 103 && part.height == 117);
                CHECK(part.duration == 2 && part.image_data == clip.data + 33);
                CHECK(part.image_size == 5232);
            }
            part_count++;
        }
    }
    CHECK(empty_count == 144 && part_count == 2118);
    CHECK(markers == 210 && layers == 123);
    CHECK(zero_duration == 1 && three_duration == 1);
    CHECK(hash_bytes(archive.data, archive.size) == before);
    printf("FIGANI actual: 408 entries, 144 empty, 2118 parts; bytes unchanged\n");
    fd2_archive_close(&archive);
    return 0;
}

/* 独立Domain对68个player完整目录读集：1954de8e...a2965c。
 * slot2 empty才退slot1；零/多标记是合法玩法参数，不是损坏容器。 */
static int test_actual_magic_cast_plans(const char *path) {
    fd2_archive archive = {0};
    CHECK(fd2_archive_open(&archive, path) == 0);
    unsigned counts[5] = {0}, empty = 0;
    for (unsigned id = 0; id < 68; id++) {
        fd2_figani_magic_cast plan;
        CHECK(fd2_figani_magic_cast_plan(&plan, &archive, (uint8_t)id) == 0);
        int no_clip = id == 48 || id == 57 || id == 66 || id == 67;
        CHECK(plan.clip.empty == no_clip);
        if (no_clip) { CHECK(plan.cost_marker_count == 0); empty++; continue; }
        unsigned count = id == 7 || id == 39 ? 4u :
            id == 12 || id == 28 || id == 49 ? 2u :
            id == 8 || id == 13 || id == 23 || id == 40 ||
            id == 45 || id == 58 || id == 63 ? 0u : 1u;
        CHECK(plan.cost_marker_count == count);
        CHECK(plan.clip.intro_end <= plan.clip.part_count);
        CHECK(plan.selected_entry == id * 3u + 1u || plan.selected_entry == id * 3u + 2u);
        counts[count]++;
        if (id == 9) CHECK(plan.selected_entry == 28 && plan.clip.intro_end == 11);
    }
    CHECK(empty == 4 && counts[0] == 7 && counts[1] == 52 && counts[2] == 3 && counts[4] == 2);
    fd2_figani_magic_cast plan, untouched;
    memset(&plan, 0xa5, sizeof(plan)); untouched = plan;
    CHECK(fd2_figani_magic_cast_plan(&plan, &archive, 255) == -1);
    CHECK(!memcmp(&plan, &untouched, sizeof(plan)));
    CHECK(fd2_figani_magic_cast_plan(&plan, NULL, 9) == -1);
    CHECK(!memcmp(&plan, &untouched, sizeof(plan)));
    CHECK(fd2_figani_magic_cast_plan(NULL, &archive, 9) == -1);
    fd2_figani_magic_cast good;
    CHECK(fd2_figani_magic_cast_plan(&good, &archive, 9) == 0);
    size_t entry = good.selected_entry;
    uint8_t *header = archive.data + archive.offsets[entry];
    uint8_t original_intro = header[2];
    header[2] = 255; /* 只改本测试拥有的RAM归档，不写文件。 */
    CHECK(fd2_figani_magic_cast_plan(&plan, &archive, 9) == -1);
    CHECK(!memcmp(&plan, &untouched, sizeof(plan)));
    header[2] = original_intro;
    fd2_archive_close(&archive);
    return 0;
}

int main(int argc, char **argv) {
    /* 原版数据检查是本 target 的必需项；缺资源必须失败，不悄悄 skip。 */
    if (argc > 2) {
        fprintf(stderr, "usage: figani_test [FIGANI.DAT]\n");
        return 1;
    }
    const char *path = argc == 2 ? argv[1] : "original_game/FIGANI.DAT";
    if (test_fields_and_borrowing() != 0 || test_empty_and_error_atomicity() != 0 ||
        test_rle_and_image_boundaries() != 0 || test_actual_figani(path) != 0 ||
        test_actual_magic_cast_plans(path) != 0)
        return 1;
    puts("FIGANI parser: 5 groups passed");
    return 0;
}
