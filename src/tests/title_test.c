#include <stdio.h>

#include "title.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct {
    int count;
    int item[3];
    int image[3];
} draw_capture;

static void capture_draw(void *userdata, int item_index, int image_index) {
    draw_capture *capture = userdata;
    capture->item[capture->count] = item_index;
    capture->image[capture->count] = image_index;
    capture->count++;
}

int main(void) {
    static const struct {
        fd2_title_sfx cue;
        fd2_title_sfx_handle handle;
        size_t sample;
    } cases[] = {
        {FD2_TITLE_SFX_FLIGHT, FD2_TITLE_SFX_HANDLE_PRIMARY, 0},
        {FD2_TITLE_SFX_ENTER, FD2_TITLE_SFX_HANDLE_SECONDARY, 3},
        {FD2_TITLE_SFX_MOVE, FD2_TITLE_SFX_HANDLE_PRIMARY, 2},
        {FD2_TITLE_SFX_CONFIRM, FD2_TITLE_SFX_HANDLE_PRIMARY, 1},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_title_sfx_handle handle;
        size_t sample;
        CHECK(fd2_title_sfx_resolve(cases[i].cue, &handle, &sample) == 0);
        CHECK(handle == cases[i].handle && sample == cases[i].sample);
    }
    fd2_title_sfx_handle handle;
    size_t sample;
    CHECK(FD2_TITLE_SFX_BANK == 77);
    CHECK(FD2_TITLE_LETTER_FLY_SFX_BANK == 78);
    CHECK(FD2_TITLE_LETTER_FLY_SFX_SAMPLE == 0);
    CHECK(fd2_title_sfx_resolve((fd2_title_sfx)99, &handle, &sample) != 0);

    static const int flight_trigger_y[FD2_TITLE_FLIGHT_TRIGGER_COUNT] = {
        520, 430, 410, 340, 310, 300, 240,
        180, 150, 130, 110, 87, 64, 22,
    };
    int trigger_count = 0;
    for (int scroll_y = 0x217; scroll_y >= 0; scroll_y--) {
        int expected = 0;
        for (size_t i = 0; i < sizeof(flight_trigger_y) /
                                      sizeof(flight_trigger_y[0]); i++)
            expected |= scroll_y == flight_trigger_y[i];
        CHECK(fd2_title_flight_sfx_for_scroll_y(scroll_y) == expected);
        trigger_count += expected;
    }
    CHECK(trigger_count == FD2_TITLE_FLIGHT_TRIGGER_COUNT);
    CHECK(!fd2_title_flight_sfx_for_scroll_y(1000));

    static const int lightning_y[FD2_TITLE_LIGHTNING_FLASH_COUNT] = {
        520, 430, 410, 340, 310, 300, 240, 180, 150, 130, 87,
    };
    int lightning_count = 0;
    for (int scroll_y = 0x217; scroll_y >= 0; scroll_y--) {
        int expected = 0;
        for (size_t i = 0; i < sizeof(lightning_y) /
                                      sizeof(lightning_y[0]); i++)
            expected |= scroll_y == lightning_y[i];
        CHECK(fd2_title_lightning_flash_for_scroll_y(scroll_y) == expected);
        lightning_count += expected;
    }
    CHECK(lightning_count == FD2_TITLE_LIGHTNING_FLASH_COUNT);
    CHECK(!fd2_title_lightning_flash_for_scroll_y(110));
    CHECK(!fd2_title_lightning_flash_for_scroll_y(64));
    CHECK(!fd2_title_lightning_flash_for_scroll_y(22));

    CHECK(FD2_TITLE_CONFIRM_FLASH_FRAMES == 8);
    CHECK(FD2_TITLE_CONFIRM_FLASH_DELAY_MS == 80);
    for (int frame = 0; frame < FD2_TITLE_CONFIRM_FLASH_FRAMES; frame++)
        CHECK(fd2_title_confirm_highlight_for_frame(frame) == (frame & 1));
    CHECK(fd2_title_confirm_highlight_for_frame(-1) == -1);
    CHECK(fd2_title_confirm_highlight_for_frame(
              FD2_TITLE_CONFIRM_FLASH_FRAMES) == -1);

    for (int menu_count = 2; menu_count <= 3; menu_count++) {
        for (int selection = 0; selection < menu_count; selection++) {
            draw_capture normal = {0};
            draw_capture highlighted = {0};
            CHECK(fd2_title_draw_menu_frame(menu_count, selection, 0,
                                            capture_draw, &normal) ==
                  menu_count);
            CHECK(fd2_title_draw_menu_frame(menu_count, selection, 1,
                                            capture_draw, &highlighted) ==
                  menu_count);
            for (int item = 0; item < menu_count; item++) {
                CHECK(normal.item[item] == item);
                CHECK(normal.image[item] == item * 2);
                CHECK(highlighted.image[item] ==
                      item * 2 + (item == selection));
            }
        }
    }
    draw_capture normal = {0};
    CHECK(fd2_title_draw_menu_frame(0, 0, 1,
                                    capture_draw, &normal) == -1);
    CHECK(fd2_title_draw_menu_frame(2, 2, 1,
                                    capture_draw, &normal) == -1);

    puts("title_test: ok");
    return 0;
}
