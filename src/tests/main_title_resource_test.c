/* boot_intro_title_menu @0x1f894：标题必需资源错误出口，无窗口私有TU。
 * 所有后续资源与菜单替身均成功，避免错误被另一个资源失败掩盖。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "archive.h"
#include "app_flow.h"
#include "animation.h"
#include "image.h"
#include "input.h"
#include "title.h"
#include "vga.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static int palette_fault, base_fault, test_animation_result, menu_quit;
static int palette_reads, title_reads, base_decodes, menu_decodes;
static int animation_calls, archive_opens, archive_closes, image_live;
static int menu_frames, action_reads, presents, unexpected;
static uint8_t test_palette_data[768], archive_token;
static int intro_enabled, intro_resource_fault, intro_image_fault;
static int intro_animation_fault, intro_animation_code, intro_skip_scroll;
static size_t last_resource;
static int intro_resource_reads[128], intro_image_decodes[128];
static int intro_animation_calls[9];

static int title_test_archive_get(const fd2_archive *ar, size_t index,
                                  const uint8_t **data, size_t *length) {
    (void)ar;
    last_resource = index;
    if (index < 128) intro_resource_reads[index]++;
    if (index == 8) {
        palette_reads++;
        if (palette_fault == 1) return -1;
        *data = test_palette_data;
        *length = palette_fault == 2 ? 767 : sizeof(test_palette_data);
        return 0;
    }
    if (index == 7) {
        title_reads++;
        *data = &archive_token;
        *length = 1;
        return 0;
    }
    if (intro_enabled) {
        if (index == 0 || index == 0x4c || index == 99 ||
            index == 0x65 || index == 0x66) {
            palette_reads++;
            if ((int)index == intro_resource_fault) return -1;
            *data = test_palette_data;
            *length = sizeof(test_palette_data);
            return 0;
        }
        if ((index >= 0x45 && index <= 0x4b) || index == 100) {
            *data = &archive_token;
            *length = 1;
            return 0;
        }
    }
    unexpected++;
    return -1;
}

static int title_test_archive_open_mem(fd2_archive *ar,
                                       const uint8_t *data, size_t length) {
    if (data != &archive_token || length != 1) unexpected++;
    memset(ar, 0, sizeof(*ar));
    ar->count = 7;
    archive_opens++;
    return 0;
}

static void title_test_archive_close(fd2_archive *ar) {
    memset(ar, 0, sizeof(*ar));
    archive_closes++;
}

static int title_test_image_decode_entry(fd2_image *image,
                                         const fd2_archive *ar, size_t index) {
    (void)ar;
    memset(image, 0, sizeof(*image));
    if (index == 0) {
        base_decodes++;
        if (base_fault) return -1;
    } else if (index < 7) {
        menu_decodes++;
    } else {
        unexpected++;
        return -1;
    }
    image->pixels = malloc(1);
    if (!image->pixels) abort();
    image->width = image->height = 1;
    image->pixels[0] = 1;
    image_live++;
    return 0;
}

static int title_test_image_decode_buf(fd2_image *image,
                                       const uint8_t *data, size_t length) {
    memset(image, 0, sizeof(*image));
    if (data != &archive_token || length != 1 || last_resource >= 128) {
        unexpected++;
        return -1;
    }
    intro_image_decodes[last_resource]++;
    if ((int)last_resource == intro_image_fault) return -1;
    image->width = 320;
    image->height = last_resource >= 0x45 && last_resource <= 0x49 ? 147 : 200;
    image->pixels = malloc((size_t)image->width * image->height);
    if (!image->pixels) abort();
    memset(image->pixels, 1, (size_t)image->width * image->height);
    image_live++;
    return 0;
}

static void title_test_image_free(fd2_image *image) {
    if (image->pixels) {
        free(image->pixels);
        image_live--;
    }
    memset(image, 0, sizeof(*image));
}

static int title_test_animation(fd2_vga *vga, const fd2_archive *ani,
                                 int index, uint32_t delay, int check_input,
                                 fd2_animation_frame_hook hook, void *userdata) {
    (void)vga;
    (void)ani;
    if (index != 1 || delay != FD2_TITLE_ANIM_DELAY_FAST_MS || !check_input)
        unexpected++;
    animation_calls++;
    if (test_animation_result >= 0 && hook) hook(userdata, 0);
    return test_animation_result;
}

static int title_test_intro_animation(fd2_vga *vga, const fd2_archive *ani,
                                       int index, uint32_t delay, int check_input) {
    (void)vga; (void)ani;
    if (index < 0 || index >= 9 || index == 1 || index == 2) {
        unexpected++;
        return -1;
    }
    uint32_t expected_delay = index == 0 ? FD2_TITLE_ANIM_DELAY_FAST_MS
        : index == 5 || index == 7 ? FD2_TITLE_ANIM_DELAY_MEDIUM_MS
                                  : FD2_TITLE_ANIM_DELAY_SLOW_MS;
    if (delay != expected_delay || check_input != (index == 3)) unexpected++;
    intro_animation_calls[index]++;
    return index == intro_animation_fault ? intro_animation_code : 0;
}

static void title_test_clear(fd2_vga *vga, uint8_t fill) {
    memset(vga->framebuffer, fill, sizeof(vga->framebuffer));
}
static void title_test_palette(fd2_vga *vga, const uint8_t *data) {
    (void)vga;
    if (data != test_palette_data) unexpected++;
}
static void title_test_brightness(fd2_vga *vga, int brightness) {
    (void)vga;
    (void)brightness;
}
static void title_test_fade(fd2_vga *vga, int start, int steps,
                            uint8_t r, uint8_t g, uint8_t b) {
    (void)vga; (void)start; (void)steps; (void)r; (void)g; (void)b;
}
static void title_test_present(fd2_vga *vga) { (void)vga; presents++; }
static void title_test_delay(uint32_t delay) { (void)delay; }
static int title_test_observe(fd2_input *input) {
    (void)input;
    return intro_skip_scroll;
}
static int title_test_quit(fd2_input *input) {
    (void)input;
    return menu_quit && menu_frames > 0;
}
static int title_test_action(fd2_input *input, fd2_input_context context,
                              fd2_input_action *action) {
    (void)input;
    if (context != FD2_INPUT_CONTEXT_TITLE || action_reads++) unexpected++;
    *action = FD2_INPUT_ACTION_CONFIRM;
    return 1;
}
static int title_test_menu(int count, int selected, int highlighted,
                            fd2_title_draw_item_fn draw, void *userdata) {
    menu_frames++;
    return fd2_title_draw_menu_frame(count, selected, highlighted, draw, userdata);
}

/* 只在此测试TU替换I/O；包含完整main，未增加生产hook或复制标题逻辑。 */
#define main fd2_program_main
#define fd2_archive_get title_test_archive_get
#define fd2_archive_open_mem title_test_archive_open_mem
#define fd2_archive_close title_test_archive_close
#define fd2_image_decode_entry title_test_image_decode_entry
#define fd2_image_decode_buf title_test_image_decode_buf
#define fd2_image_free title_test_image_free
#define fd2_animation_play_hooked title_test_animation
#define fd2_animation_play title_test_intro_animation
#define fd2_vga_clear title_test_clear
#define fd2_vga_set_palette title_test_palette
#define fd2_vga_set_brightness title_test_brightness
#define fd2_vga_palette_fade_to title_test_fade
#define fd2_vga_present title_test_present
#define fd2_delay_ms title_test_delay
#define fd2_wait_ticks title_test_delay
#define fd2_input_take_quit title_test_quit
#define fd2_input_take_action title_test_action
#define fd2_input_observe_any_key title_test_observe
#define fd2_title_draw_menu_frame title_test_menu
#ifndef FD2_TITLE_TEST_MAIN
#define FD2_TITLE_TEST_MAIN "main.c"
#endif
#include FD2_TITLE_TEST_MAIN
#undef main

static void reset_test(void) {
    palette_fault = base_fault = test_animation_result = menu_quit = 0;
    palette_reads = title_reads = base_decodes = menu_decodes = 0;
    animation_calls = archive_opens = archive_closes = image_live = 0;
    menu_frames = action_reads = presents = unexpected = 0;
    intro_enabled = intro_skip_scroll = 0;
    intro_resource_fault = intro_image_fault = intro_animation_fault = -1;
    intro_animation_code = 0;
    memset(intro_resource_reads, 0, sizeof(intro_resource_reads));
    memset(intro_image_decodes, 0, sizeof(intro_image_decodes));
    memset(intro_animation_calls, 0, sizeof(intro_animation_calls));
}

static int run_case(const char *name) {
    reset_test();
    fd2_title_action expected = FD2_TITLE_ACTION_ERROR;
    if (!strcmp(name, "palette-missing")) palette_fault = 1;
    else if (!strcmp(name, "palette-short")) palette_fault = 2;
    else if (!strcmp(name, "base-error")) base_fault = 1;
    else if (!strcmp(name, "animation-error")) test_animation_result = -1;
    else if (!strcmp(name, "animation-quit")) {
        test_animation_result = -2;
        expected = FD2_TITLE_ACTION_HOST_QUIT;
    } else if (!strcmp(name, "menu-quit")) {
        menu_quit = 1;
        expected = FD2_TITLE_ACTION_HOST_QUIT;
    } else if (!strcmp(name, "normal") || !strcmp(name, "animation-skip")) {
        test_animation_result = !strcmp(name, "animation-skip");
        expected = FD2_TITLE_ACTION_NEW_GAME;
    } else {
        fprintf(stderr, "unknown case: %s\n", name);
        return 1;
    }
    fd2_vga vga = {0};
    CHECK(boot_intro_title(&vga, NULL, NULL, 0, 1) == expected);
    CHECK(palette_reads == 1 && unexpected == 0 && image_live == 0);
    CHECK(archive_opens == archive_closes);
    if (palette_fault) {
        CHECK(!animation_calls && !title_reads && !base_decodes && !presents);
    } else {
        CHECK(animation_calls == 1);
    }
    if (expected == FD2_TITLE_ACTION_ERROR || test_animation_result == -2) {
        CHECK(!menu_decodes && !menu_frames && !action_reads);
        if (test_animation_result < 0) CHECK(!title_reads && !base_decodes && !presents);
        if (base_fault) CHECK(title_reads == 1 && base_decodes == 1 && !presents);
    } else {
        CHECK(title_reads == 2 && base_decodes == 1 && menu_decodes == 6);
        CHECK(menu_frames == (menu_quit ? 1 : 1 + FD2_TITLE_CONFIRM_FLASH_FRAMES));
        CHECK(action_reads == (menu_quit ? 0 : 1) && presents > 0);
    }
    return 0;
}

typedef struct {
    const char *name;
    int resource, image, animation, animation_code, skip;
    fd2_title_action expected;
} intro_case;

static const intro_case intro_cases[] = {
    {"intro-palette76", 76,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette99", 99,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette101",101,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette102",102,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette0",   0,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image74",   -1,74,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image69",   -1,69,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image100",  -1,100,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image75",   -1,75,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani3", -1,-1,3,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani4", -1,-1,4,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani5", -1,-1,5,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani6", -1,-1,6,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani7", -1,-1,7,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani8", -1,-1,8,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani0", -1,-1,0,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-normal", -1,-1,-1,0,0,FD2_TITLE_ACTION_NEW_GAME},
    {"intro-scroll-skip", -1,-1,-1,0,1,FD2_TITLE_ACTION_NEW_GAME},
    {"intro-ani3-skip", -1,-1,3,1,0,FD2_TITLE_ACTION_NEW_GAME},
    {"intro-ani3-quit", -1,-1,3,-2,0,FD2_TITLE_ACTION_HOST_QUIT},
    {"intro-ani4-quit", -1,-1,4,-2,0,FD2_TITLE_ACTION_HOST_QUIT},
};

static int run_intro_case(const intro_case *test) {
    reset_test();
    intro_enabled = 1;
    intro_resource_fault = test->resource;
    intro_image_fault = test->image;
    intro_animation_fault = test->animation;
    intro_animation_code = test->animation_code;
    intro_skip_scroll = test->skip;
    fd2_vga vga = {0};
    CHECK(boot_intro_title(&vga, NULL, NULL, 1, 1) == test->expected);
    CHECK(!unexpected && !image_live && archive_opens == archive_closes);
    if (test->expected != FD2_TITLE_ACTION_NEW_GAME) {
        CHECK(!title_reads && !base_decodes && !menu_decodes &&
              !menu_frames && !action_reads && !animation_calls);
        if (test->resource >= 0) CHECK(intro_resource_reads[test->resource] == 1);
        if (test->image >= 0) CHECK(intro_image_decodes[test->image] == 1);
        if (test->animation >= 0) {
            const int order[] = {3,4,5,6,7,8,0};
            int after_failure = 0;
            for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
                if (after_failure) CHECK(!intro_animation_calls[order[i]]);
                if (order[i] == test->animation) after_failure = 1;
            }
            CHECK(intro_animation_calls[test->animation] == 1);
        }
        if (test->resource == 0) CHECK(!intro_animation_calls[5]);
    } else {
        CHECK(animation_calls == 1 && menu_decodes == 6 && action_reads == 1);
        CHECK(intro_animation_calls[3] == 1);
        const int rest[] = {4,5,6,7,8,0};
        for (size_t i = 0; i < sizeof(rest) / sizeof(rest[0]); i++)
            CHECK(intro_animation_calls[rest[i]] == !test->skip);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        for (size_t i = 0; i < sizeof(intro_cases) / sizeof(intro_cases[0]); i++)
            if (!strcmp(argv[1], intro_cases[i].name)) return run_intro_case(&intro_cases[i]);
        return run_case(argv[1]);
    }
    const char *cases[] = {"palette-missing", "palette-short", "base-error",
                          "animation-error", "animation-quit", "menu-quit",
                          "normal", "animation-skip"};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        if (run_case(cases[i])) return 1;
    for (size_t i = 0; i < sizeof(intro_cases) / sizeof(intro_cases[0]); i++)
        if (run_intro_case(&intro_cases[i])) return 1;
    puts("main_title_resource: 29 cases passed; no SDL initialization/window");
    return 0;
}
