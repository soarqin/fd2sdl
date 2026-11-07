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
#include "../main.c"
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

int main(void) {
    int result = 0, changed_directory = 0;
    char original_directory[2048], output[4096];
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

    /* 真main缺数据分支必须先返回，替身SDL_Init也不应被调用。 */
    CHECK(TEST_GETCWD(original_directory, sizeof(original_directory)) != NULL);
    CHECK(TEST_CHDIR(fixture) == 0);
    changed_directory = 1;
    char *args[] = {"fd2sdl", "--field-play-once", NULL};
    CHECK(startup_test_app_main(2, args) == 1 && sdl_init_calls == 0);
    CHECK(!g_fdother.data && !g_ani.data);

done:
    if (changed_directory && TEST_CHDIR(original_directory) != 0) result = 1;
    if (fixture_created) {
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) remove_data(names[i]);
        if (TEST_RMDIR(fixture) != 0) result = 1;
    }
    if (!result) puts("startup resources: missing/single/corrupt/CLI/read-only/early-exit checks passed");
    return result;
}
