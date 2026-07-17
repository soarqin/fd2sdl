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
        fd2_title_sfx_bank bank;
        size_t sample;
    } cases[] = {
        {FD2_TITLE_SFX_ENTER, FD2_TITLE_SFX_BANK_ENTRY, 3},
        {FD2_TITLE_SFX_MOVE, FD2_TITLE_SFX_BANK_UI, 2},
        {FD2_TITLE_SFX_CONFIRM, FD2_TITLE_SFX_BANK_UI, 1},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_title_sfx_bank bank;
        size_t sample;
        CHECK(fd2_title_sfx_resolve(cases[i].cue, &bank, &sample) == 0);
        CHECK(bank == cases[i].bank && sample == cases[i].sample);
    }
    fd2_title_sfx_bank bank;
    size_t sample;
    CHECK(fd2_title_sfx_resolve((fd2_title_sfx)99, &bank, &sample) != 0);

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
