/* R1宿主启动前置：独立合成容器与B24，不读取原版资源、不初始化SDL。
 * 复用main私有检查及正式archive/FDICON解码；坏资源不能误报为缺失。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define TEST_MKDIR(path) _mkdir(path)
#define TEST_RMDIR(path) _rmdir(path)
#define TEST_CHDIR(path) _chdir(path)
#define TEST_GETCWD(path, size) _getcwd(path, size)
#define TEST_PID() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define TEST_MKDIR(path) mkdir(path, 0700)
#define TEST_RMDIR(path) rmdir(path)
#define TEST_CHDIR(path) chdir(path)
#define TEST_GETCWD(path, size) getcwd(path, size)
#define TEST_PID() getpid()
#endif

static unsigned sdl_init_calls;
static bool test_sdl_init(SDL_InitFlags flags) {
    (void)flags;
    sdl_init_calls++;
    return false;
}
#define SDL_Init test_sdl_init
#define main startup_test_app_main
#include "../src/main.c"
#undef main
#undef SDL_Init

static char fixture[128];
static int fixture_created;
static const char *const names[] = {
    "FDOTHER.DAT", "ANI.DAT", "FDFIELD.DAT", "FDSHAP.DAT",
    "DATO.DAT", "FDTXT.DAT", "FDICON.B24", "FIGANI.DAT",
};
static const uint8_t archive_bytes[] = {'L','L','L','L','L','L',10,0,0,0,0};

static int create_fixture(void) {
    for (unsigned attempt = 0; attempt < 32; attempt++) {
        snprintf(fixture, sizeof(fixture), "r1-startup-%lu-%u",
                 (unsigned long)TEST_PID(), attempt);
        if (TEST_MKDIR(fixture) == 0) {
            fixture_created = 1;
            return 0;
        }
        if (errno != EEXIST) return -1;
    }
    return -1;
}

static int write_data(const char *name, const void *data, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", fixture, name);
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    int ok = fwrite(data, 1, size, file) == size;
    return fclose(file) == 0 && ok ? 0 : -1;
}

static int write_valid(const char *name) {
    if (!strcmp(name, "FDICON.B24")) {
        uint8_t bytes[34] = {24,0,24,0,1,0,10,0,0,0};
        memset(bytes + 10, 0xd7, 24); /* 每行24像素SKIP，仍走真实B24解码。 */
        return write_data(name, bytes, sizeof(bytes));
    }
    return write_data(name, archive_bytes, sizeof(archive_bytes));
}

static int remove_data(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", fixture, name);
    return remove(path);
}

static int check_output(unsigned mode, char output[4096]) {
    FILE *errors = tmpfile();
    if (!errors) return -2;
    int result = check_startup_resources(mode, fixture, errors);
    if (fflush(errors) != 0 || fseek(errors, 0, SEEK_SET) != 0) {
        fclose(errors);
        return -2;
    }
    size_t count = fread(output, 1, 4095, errors);
    output[count] = '\0';
    int bad = ferror(errors);
    if (fclose(errors) != 0 || bad) return -2;
    return result;
}

static unsigned occurrences(const char *text, const char *needle) {
    unsigned count = 0;
    for (const char *at = text; (at = strstr(at, needle)) != NULL; at += strlen(needle))
        count++;
    return count;
}

static uint64_t fixture_hash(void) {
    uint64_t value = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", fixture, names[i]);
        FILE *file = fopen(path, "rb");
        if (!file) return 0;
        int c;
        while ((c = fgetc(file)) != EOF) {
            value ^= (unsigned char)c;
            value *= UINT64_C(1099511628211);
        }
        int bad = ferror(file);
        if (fclose(file) != 0 || bad) return 0;
    }
    return value;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    result = 1; goto done; } } while (0)

/* 超大编号乘以 3 会绕回条目 2。该条目及其后两条均可正常解析。 */
static int check_group_indices(void) {
    uint8_t fields[143] = {0,0,1,0,1,0};
    uint32_t field_offsets[] = {0,1,2,10,141};
    fd2_archive field = {.data=fields, .size=sizeof(fields),
        .offsets=field_offsets, .count=5};
    uint8_t shapes[] = {1,0,1,0,1,0,10,0,0,0,0xc0, 0,0,0,0};
    uint32_t shape_offsets[] = {0,11};
    fd2_archive shape = {.data=shapes, .size=sizeof(shapes),
        .offsets=shape_offsets, .count=2};
    size_t stage = SIZE_MAX / 3u + 1u;
    fd2_field_map map = {0};
    fd2_field_metadata meta = {0};
    fd2_field_placements placements = {0};
    fd2_terrain_tileset terrain = {0};
    int result = 0;
    CHECK(fd2_field_map_open_stage(&map, &field, stage) == -1);
    CHECK(fd2_field_metadata_open_stage(&meta, &field, stage) == -1);
    CHECK(fd2_field_placements_open_stage(&placements, &field, stage) == -1);
    CHECK(fd2_terrain_tileset_open_stage(&terrain, &shape, &field, stage) == -1);
    CHECK(fd2_terrain_tileset_open(&terrain, &shape, SIZE_MAX / 2u + 1u) == -1);
    CHECK(!map.cells && !meta.unit_templates && !placements.records && !terrain.frames);
done:
    fd2_field_map_close(&map);
    fd2_field_metadata_close(&meta);
    fd2_field_placements_close(&placements);
    fd2_terrain_tileset_close(&terrain);
    return result;
}

static int check_vga_integer_scale(void) {
    const int scale = 3, edge = 4 * scale;
    const uint8_t palette[VGA_PALETTE_SIZE] = {0, 0, 0, 63, 63, 63};
    int result = 0;
    fd2_vga vga = {0};
    SDL_Renderer *renderer = NULL;
    SDL_Surface *surface = SDL_CreateSurface(VGA_W * scale, VGA_H * scale, SDL_PIXELFORMAT_ARGB8888);
    CHECK(surface);
    renderer = SDL_CreateSoftwareRenderer(surface);
    CHECK(renderer && SDL_SetRenderLogicalPresentation(renderer, VGA_W, VGA_H,
                                                        SDL_LOGICAL_PRESENTATION_INTEGER_SCALE));
    CHECK(fd2_vga_init(&vga, NULL, renderer) == 0);
    /* 内区角落的一个白像素应只覆盖 3×3，四周保持纯黑。 */
    vga.framebuffer[4 * VGA_STRIDE + 4] = 1;
    fd2_vga_set_palette(&vga, palette);
    fd2_vga_upload_current_frame(&vga);
    CHECK(fd2_vga_output_error(&vga) == 0);
    for (int y = edge - 1; y <= edge + scale; y++) {
        for (int x = edge - 1; x <= edge + scale; x++) {
            uint8_t r, g, b;
            uint8_t expected = (x >= edge && x < edge + scale &&
                                y >= edge && y < edge + scale) ? 255 : 0;
            CHECK(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, NULL));
            CHECK(r == expected && g == expected && b == expected);
        }
    }
done:
    fd2_vga_close(&vga);
    if (renderer) SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    return result;
}

int main(void) {
    int result = 0, changed_directory = 0;
    char original_directory[2048], output[4096];
    fd2_config_file config = {0};
    fd2_settings_defaults();
    CHECK(!fd2_setup_requested(&config, false));
    config.path = "first.cfg";
    CHECK(fd2_setup_requested(&config, false) && !fd2_setup_requested(&config, true));
    config.found = true;
    CHECK(!fd2_setup_requested(&config, false));
    size_t count;
    const fd2_setting *items = fd2_settings_table(&count);
    for (size_t i = 0; i < count; ++i)
        if (!strcmp(items[i].key, "show_on_start")) CHECK(fd2_setting_set(&items[i], "on"));
    CHECK(fd2_setup_requested(&config, false) && !fd2_setup_requested(&config, true));
    config.path = NULL;
    CHECK(!fd2_setup_requested(&config, false));
    config.setup = true;
    CHECK(fd2_setup_requested(&config, true));
    fd2_settings_defaults();
    CHECK(check_group_indices() == 0);
    CHECK(check_vga_integer_scale() == 0);
    CHECK(create_fixture() == 0);
    CHECK(check_output(STARTUP_TITLE, output) == -1);
    CHECK(occurrences(output, "缺少文件：") == 8);
    CHECK(strstr(output, "original_game") == NULL); /* 检查使用给定隔离目录。 */
    CHECK(strstr(output, "不需要 FD2.EXE") != NULL);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        CHECK(strstr(output, names[i]) != NULL);
        CHECK(write_valid(names[i]) == 0);
    }
    uint64_t before = fixture_hash();
    CHECK(before != 0 && check_output(STARTUP_TITLE, output) == 0 && !output[0]);
    CHECK(fixture_hash() == before);
    CHECK(remove_data("FDTXT.DAT") == 0);
    CHECK(check_output(STARTUP_TITLE, output) == -1);
    CHECK(occurrences(output, "缺少文件：") == 1 && strstr(output, "FDTXT.DAT"));
    CHECK(write_valid("FDTXT.DAT") == 0);

    CHECK(write_data("FDOTHER.DAT", "broken", 6) == 0);
    CHECK(check_output(STARTUP_TITLE, output) == -1);
    CHECK(strstr(output, "格式损坏：") && strstr(output, "FDOTHER.DAT"));
    CHECK(!strstr(output, "缺少文件："));
    CHECK(write_valid("FDOTHER.DAT") == 0);
    /* 第一帧仅有 23 行；后一帧独立有效，不能借它的首行补齐。 */
    uint8_t overlapping[14 + 23 + 24] = {24,0,24,0,2,0,14,0,0,0,37,0,0,0};
    memset(overlapping + 14, 0xd7, sizeof(overlapping) - 14);
    CHECK(write_data("FDICON.B24", overlapping, sizeof(overlapping)) == 0);
    CHECK(check_output(STARTUP_TITLE, output) == -1 && strstr(output, "FDICON.B24"));
    CHECK(write_data("FDICON.B24", "broken", 6) == 0);
    CHECK(check_output(STARTUP_TITLE, output) == -1);
    CHECK(strstr(output, "损坏：") && strstr(output, "FDICON.B24"));
    CHECK(!strstr(output, "缺少文件："));
    CHECK(remove_data("ANI.DAT") == 0);
    CHECK(check_output(STARTUP_TITLE, output) == -1);
    CHECK(occurrences(output, "缺少文件：") == 1 && strstr(output, "ANI.DAT"));
    CHECK(strstr(output, "无法读取或格式损坏：") && strstr(output, "FDICON.B24"));
    CHECK(write_valid("ANI.DAT") == 0);
    CHECK(write_valid("FDICON.B24") == 0);

    CHECK(remove_data("ANI.DAT") == 0 && check_output(STARTUP_FIELD, output) == 0);
    CHECK(remove_data("FIGANI.DAT") == 0 && check_output(STARTUP_PROLOGUE, output) == 0);
    CHECK(check_output(STARTUP_FIELD, output) == -1 && strstr(output, "FIGANI.DAT"));
    CHECK(remove_data("DATO.DAT") == 0 && remove_data("FDTXT.DAT") == 0 &&
          remove_data("FDICON.B24") == 0 && check_output(STARTUP_MAP, output) == 0);

    /* 真 main 默认在工作目录找资源。缺文件时先返回，齐全时进入 SDL 初始化。 */
    CHECK(TEST_GETCWD(original_directory, sizeof(original_directory)) != NULL);
    CHECK(TEST_CHDIR(fixture) == 0);
    changed_directory = 1;
    char *args[] = {"fd2sdl", "--field-play-once", NULL};
    CHECK(startup_test_app_main(2, args) == 1 && sdl_init_calls == 0);
    CHECK(!g_fdother.data && !g_ani.data);
    char *map_args[] = {"fd2sdl", "--no-config", "--map-preview-once", NULL};
    CHECK(startup_test_app_main(3, map_args) == 1 && sdl_init_calls == 1);

done:
    if (changed_directory && TEST_CHDIR(original_directory) != 0) result = 1;
    if (fixture_created) {
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) remove_data(names[i]);
        if (TEST_RMDIR(fixture) != 0) result = 1;
    }
    if (!result) puts("startup resources: missing/single/corrupt/CLI/read-only/early-exit checks passed");
    return result;
}
