#include "vga.h"
#include "field_play.h"
#include <assert.h>
#include <string.h>

static unsigned delay, observed_ticks;
#define fd2_vga_delay_ms(v, n) ((void)(v), delay = (n), 0)
#define fd2_vga_wait_ticks(v, n) ((void)(v), observed_ticks = (n), 0)
#include "pacing.h"

int main(void) {
    size_t count;
    fd2_settings_defaults();
    const fd2_setting *items = fd2_settings_table(&count);
    assert(fd2_text_wait(NULL) == 0 && observed_ticks == 1);
    assert(fd2_animation_wait_ticks(NULL, 3) == 0 && observed_ticks == 3);
    for (size_t i = 0; i < count; ++i) {
        const fd2_setting *s = &items[i];
        if (!strcmp(s->key, "text_speed")) {
            assert(fd2_setting_set(s, "fast") && fd2_text_wait(NULL) == 0 && delay == 15);
            assert(fd2_setting_set(s, "instant") && fd2_text_wait(NULL) == 0 && delay == 0);
        } else if (!strcmp(s->key, "animation_speed")) assert(fd2_setting_set(s, "200"));
        else if (!strcmp(s->key, "key_repeat_delay_ms")) assert(fd2_setting_set(s, "10"));
        else if (!strcmp(s->key, "key_repeat_rate_ms")) assert(fd2_setting_set(s, "4"));
        else if (!strcmp(s->key, "new_game_battle_effect")) assert(fd2_setting_set(s, "short"));
        else if (!strcmp(s->key, "new_game_info_board")) assert(fd2_setting_set(s, "off"));
    }
    assert(!fd2_settings_original());
    assert(fd2_animation_wait_ms(NULL, 90) == 0 && delay == 45);
    assert(fd2_animation_wait_ticks(NULL, 1) == 0 && delay == 28);
    fd2_field_play_context context;
    fd2_field_play_context_init(&context, NULL);
    assert(context.effect_option == 1 && context.camera_option == 0);
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_G, 1);
    fd2_input_begin_test_frame(&input, 0);
    fd2_input_event key;
    assert(fd2_input_take_key(&input, &key) && !key.repeat);
    fd2_input_begin_test_frame(&input, 9);
    assert(!fd2_input_has_any_key(&input));
    fd2_input_begin_test_frame(&input, 14);
    for (int i = 0; i < 2; ++i)
        assert(fd2_input_take_key(&input, &key) && key.repeat && key.bios_code == 0x2267);
    assert(!fd2_input_has_any_key(&input));
    return 0;
}
