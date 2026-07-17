#include "title.h"

int fd2_title_sfx_resolve(fd2_title_sfx cue,
                          fd2_title_sfx_bank *bank,
                          size_t *sample_index) {
    if (!bank || !sample_index) return -1;
    switch (cue) {
        case FD2_TITLE_SFX_ENTER:
            *bank = FD2_TITLE_SFX_BANK_ENTRY;
            *sample_index = 3;
            return 0;
        case FD2_TITLE_SFX_MOVE:
            *bank = FD2_TITLE_SFX_BANK_UI;
            *sample_index = 2;
            return 0;
        case FD2_TITLE_SFX_CONFIRM:
            *bank = FD2_TITLE_SFX_BANK_UI;
            *sample_index = 1;
            return 0;
    }
    return -1;
}

int fd2_title_confirm_highlight_for_frame(int frame) {
    if (frame < 0 || frame >= FD2_TITLE_CONFIRM_FLASH_FRAMES) return -1;
    return frame & 1;
}

int fd2_title_draw_menu_frame(int menu_count,
                              int selection,
                              int highlight_selected,
                              fd2_title_draw_item_fn draw,
                              void *userdata) {
    if (menu_count < 1 || menu_count > 3 || selection < 0 ||
        selection >= menu_count || !draw)
        return -1;
    for (int item = 0; item < menu_count; item++) {
        int highlighted = highlight_selected && item == selection;
        draw(userdata, item, item * 2 + highlighted);
    }
    return menu_count;
}
