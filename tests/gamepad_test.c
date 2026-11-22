#include "input.h"
#include "settings.h"
#include <assert.h>

static void update(fd2_input *input, uint64_t now) {
    input->test_now_ms = now;
    SDL_UpdateJoysticks();
    fd2_input_poll_host_events(input);
}
static SDL_JoystickID attach(void) {
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.vendor_id = 0x1209;
    desc.product_id = 0xfd02;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "fd2 virtual pad";
    return SDL_AttachVirtualJoystick(&desc);
}
static void button(SDL_Joystick *j, SDL_GamepadButton b, bool down) {
    assert(SDL_SetJoystickVirtualButton(j, b, down));
}
static void key(fd2_input *input, SDL_Scancode source, bool repeat) {
    fd2_input_event event;
    assert(fd2_input_take_key(input, &event) && event.source == source && event.repeat == repeat);
}

int main(void) {
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, "0x1209/0xfd02");
    assert(SDL_Init(SDL_INIT_GAMEPAD));
    fd2_settings_defaults();
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_begin_test_frame(&input, 0);
    SDL_JoystickID id = attach();
    assert(id);
    update(&input, 0);
    assert(input.gamepad && input.pad_id == id);
    SDL_Joystick *j = SDL_GetGamepadJoystick(input.gamepad);
    SDL_JoystickID second = attach();
    assert(second);
    update(&input, 0);
    assert(input.pad_id == id);
    button(j, SDL_GAMEPAD_BUTTON_SOUTH, true);
    update(&input, 10);
    key(&input, SDL_SCANCODE_RETURN, false);
    update(&input, 510);
    key(&input, SDL_SCANCODE_RETURN, true);
    update(&input, 5510);
    assert(input.queued_count == FD2_INPUT_KEY_QUEUE_CAPACITY);
    for (int i = 0; i < FD2_INPUT_KEY_QUEUE_CAPACITY; ++i) key(&input, SDL_SCANCODE_RETURN, true);
    button(j, SDL_GAMEPAD_BUTTON_SOUTH, false);
    assert(SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, 12000));
    update(&input, 5511);
    assert(!input.repeat_key && !fd2_input_has_any_key(&input));
    assert(SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, -22000));
    update(&input, 5520);
    key(&input, SDL_SCANCODE_LEFT, false);
    assert(SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, -15000));
    button(j, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
    update(&input, 5530);
    assert(input.sources_down[SDL_SCANCODE_LEFT] == 2 && !fd2_input_has_any_key(&input));
    assert(SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, 16000));
    update(&input, 5540);
    assert(input.stick_source == FD2_STICK_LEFT);
    assert(SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, 25000));
    update(&input, 5550);
    key(&input, SDL_SCANCODE_DOWN, false);
    assert(input.sources_down[SDL_SCANCODE_LEFT] == 1);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_LEFT, 1);
    update(&input, 5560);
    assert(input.sources_down[SDL_SCANCODE_LEFT] == 2);
    assert(SDL_DetachVirtualJoystick(id));
    update(&input, 5570);
    assert(input.pad_id == second && !input.sources_down[SDL_SCANCODE_DOWN] &&
           input.sources_down[SDL_SCANCODE_LEFT] == 1 && !input.repeat_key);
    j = SDL_GetGamepadJoystick(input.gamepad);
    fd2_pad_read(SDL_GAMEPAD_BUTTON_SOUTH, "Alt+F1");
    button(j, SDL_GAMEPAD_BUTTON_SOUTH, true);
    update(&input, 5580);
    fd2_input_event e;
    assert(fd2_input_take_key(&input, &e) && e.bios_code == 0x6800);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_LEFT, 0);
    SDL_Event lost = {0}; lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    assert(SDL_PushEvent(&lost));
    update(&input, 5590);
    button(j, SDL_GAMEPAD_BUTTON_EAST, true);
    update(&input, 5600);
    assert(!fd2_input_has_any_key(&input) && !input.chord_mods && !input.pad_down[SDL_GAMEPAD_BUTTON_EAST]);
    fd2_input_close(&input);
    assert(SDL_DetachVirtualJoystick(second));
    SDL_Quit();
    return 0;
}
