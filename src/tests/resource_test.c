/* 资源边界回归：合成 .DAT/RLE 夹具及可选的原版资源兼容检查。 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "archive.h"
#include "image.h"
#include "shape.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

static void put_u32le(uint8_t *p, uint32_t value) {
    for (size_t i = 0; i < 4; i++) p[i] = (uint8_t)(value >> (i * 8u));
}

static int test_archive_bounds(void) {
    /* 无间隙的嵌套归档；首条目内容恰好也是一个合法的递增偏移值。 */
    uint8_t data[24] = "LLLLLL";
    put_u32le(data + 6, 10);
    put_u32le(data + 10, 20);
    fd2_archive ar;
    CHECK(fd2_archive_open_mem(&ar, data, sizeof(data)) == 0);
    CHECK(ar.count == 1 && ar.data == data && !ar.owns_data);
    const uint8_t *entry;
    size_t length;
    CHECK(fd2_archive_get(&ar, 0, &entry, &length) == 0);
    CHECK(entry == data + 10 && length == 14);
    CHECK(fd2_archive_get(&ar, 1, &entry, &length) == -1);
    fd2_archive_close(&ar);
    fd2_archive_close(&ar);
    CHECK(ar.data == NULL && ar.offsets == NULL && ar.count == 0);

    /* 超过原实现初始容量的偏移表，带原版顶层归档的四字节终止间隙。 */
    uint8_t many[6 + 20 * 4 + 4 + 20] = "LLLLLL";
    for (size_t i = 0; i < 20; i++) put_u32le(many + 6 + i * 4, 90 + (uint32_t)i);
    CHECK(fd2_archive_open_mem(&ar, many, sizeof(many)) == 0);
    CHECK(ar.count == 20);
    CHECK(fd2_archive_get(&ar, 19, &entry, &length) == 0);
    CHECK(entry == many + 109 && length == 1);
    fd2_archive_close(&ar);

    CHECK(fd2_archive_open_mem(&ar, NULL, sizeof(data)) == -1);
    CHECK(errno == EINVAL && ar.data == NULL && ar.offsets == NULL);
    CHECK(fd2_archive_open_mem(&ar, data, 6) == -1);
    put_u32le(data + 6, 6); /* 偏移不能指向自身表项。 */
    CHECK(fd2_archive_open_mem(&ar, data, sizeof(data)) == -1);
    put_u32le(data + 6, sizeof(data));
    CHECK(fd2_archive_open_mem(&ar, data, sizeof(data)) == -1);
#if SIZE_MAX > UINT32_MAX
    CHECK(fd2_archive_open_mem(&ar, data, (size_t)UINT32_MAX + 1u) == -1);
#endif
    CHECK(fd2_archive_open(NULL, "missing.dat") == -1);
    CHECK(fd2_archive_open(&ar, NULL) == -1);
    CHECK(fd2_archive_get(NULL, 0, &entry, &length) == -1);
    return 0;
}

static int test_rle_bounds(void) {
    static const uint8_t encoded[] = {0x81, 3, 4, 0x01, 5, 0xc1, 0x40, 7, 0x07, 9};
    static const uint8_t expected[] = {3, 4, 5, 5, 0, 0, 0, 7, 9, 9, 9, 9, 9, 9, 9, 9};
    uint8_t pixels[16] = {0};
    CHECK(fd2_image_decode_rle_pixels(pixels, 8, 2, encoded, sizeof(encoded)) == 0);
    CHECK(memcmp(pixels, expected, sizeof(pixels)) == 0);

    static const struct {
        uint8_t data[4];
        size_t length;
    } invalid[] = {
        {{0x02, 7}, 2},       /* RUN 超过当前行。 */
        {{0x82, 4, 5}, 3},    /* LITERAL 计数超行，缺失字节不能被裁剪掩盖。 */
        {{0x41, 7}, 2},       /* stride-2 计数超行。 */
        {{0xc2}, 1},          /* SKIP 超行。 */
        {{0x81, 4}, 2},       /* LITERAL 截断。 */
        {{0x01}, 1},          /* RUN 缺少像素值。 */
        {{0x00, 7}, 2},       /* 行未完成。 */
        {{0x01, 7, 0xc0}, 3}, /* 完整行后有多余数据。 */
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        CHECK(fd2_image_decode_rle_pixels(pixels, 2, 1,
                  invalid[i].data, invalid[i].length) == -1);
    }
    CHECK(fd2_image_decode_rle_pixels(NULL, 2, 1, encoded, sizeof(encoded)) == -1);
    CHECK(fd2_image_decode_rle_pixels(pixels, 2, 1, NULL, 0) == -1);
    CHECK(fd2_image_decode_rle_pixels(pixels, 2, 0, encoded, 0) == -1);
    CHECK(fd2_image_decode_rle_pixels(pixels, 0, 1, encoded, 0) == -1);

    fd2_image image;
    static const uint8_t truncated[] = {2, 0, 1, 0, 0x81, 4};
    CHECK(fd2_image_decode_buf(&image, truncated, sizeof(truncated)) == -1);
    CHECK(image.pixels == NULL && image.width == 0 && image.height == 0);
    CHECK(fd2_image_decode_buf(&image, NULL, 4) == -1);
    fd2_image_free(&image);
    return 0;
}

static int test_original_resources_if_available(void) {
    static const struct {
        const char *name;
        size_t count;
    } archives[] = {
        {"TITLE", 7}, {"TAI", 56}, {"FDTXT", 34}, {"BG", 56},
        {"FDMUS", 20}, {"FDFIELD", 99}, {"FDOTHER", 103},
        {"FDSHAP", 66}, {"DATO", 136}, {"ANI", 9}, {"FIGANI", 408},
    };
    for (size_t i = 0; i < sizeof(archives) / sizeof(archives[0]); i++) {
        char path[64];
        snprintf(path, sizeof(path), "original_game/%s.DAT", archives[i].name);
        FILE *probe = fopen(path, "rb");
        if (!probe) continue;
        fclose(probe);
        fd2_archive ar;
        CHECK(fd2_archive_open(&ar, path) == 0);
        CHECK(ar.owns_data && ar.count == archives[i].count);
        for (size_t index = 0; index < ar.count; index++) {
            const uint8_t *entry;
            size_t length;
            CHECK(fd2_archive_get(&ar, index, &entry, &length) == 0);
            CHECK(entry >= ar.data && length > 0 && entry + length <= ar.data + ar.size);
            if (strcmp(archives[i].name, "TITLE") == 0 ||
                (strcmp(archives[i].name, "FDOTHER") == 0 && index >= 0x45 && index <= 0x49)) {
                fd2_image image;
                CHECK(fd2_image_decode_buf(&image, entry, length) == 0);
                fd2_image_free(&image);
            } else if (strcmp(archives[i].name, "FDOTHER") == 0 && index == 7) {
                fd2_archive nested;
                CHECK(fd2_archive_open_mem(&nested, entry, length) == 0);
                CHECK(nested.count == 7);
                for (size_t frame = 0; frame < nested.count; frame++) {
                    fd2_image image;
                    CHECK(fd2_image_decode_entry(&image, &nested, frame) == 0);
                    fd2_image_free(&image);
                }
                fd2_archive_close(&nested);
            } else if (strcmp(archives[i].name, "FDSHAP") == 0 && index % 2u == 0) {
                fd2_shape_sheet sheet;
                CHECK(fd2_shape_sheet_open_mem(&sheet, entry, length) == 0);
                for (size_t frame = 0; frame < sheet.frame_count; frame++) {
                    fd2_image image;
                    CHECK(fd2_shape_sheet_decode_frame(&image, &sheet, frame) == 0);
                    fd2_image_free(&image);
                }
                fd2_shape_sheet_close(&sheet);
            }
        }
        fd2_archive_close(&ar);
    }
    return 0;
}

int main(void) {
    CHECK(test_archive_bounds() == 0);
    CHECK(test_rle_bounds() == 0);
    CHECK(test_original_resources_if_available() == 0);
    puts("resource_test: ok");
    return 0;
}
