/* 无窗口设备的界面操作回归，实际 WGC 窗口另由 launcher_window 验证。 */
#include "launcher.c"
#include <assert.h>

static void mouse_click(setup *ui, int x, int y) {
    SDL_Event e = {0};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.windowID = SDL_GetWindowID(ui->window);
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = (float)x;
    e.button.y = (float)y;
    assert(handle_event(ui, &e));
}
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO));
    fd2_settings_defaults();
    fd2_config_file config = {0};
    setup ui = {.config = &config, .stick = -1};
    assert(SDL_CreateWindowAndRenderer("controls", WIDTH, HEIGHT, SDL_WINDOW_HIDDEN, &ui.window, &ui.renderer));
    open_page(&ui, PATHS);
    mouse_click(&ui, 490, ROW_Y + 8);
    assert(ui.editing && ui.selected_all);
    insert_text(&ui, "路径/资料");
    assert(!ui.available && !strcmp(ui.edit, "路径/资料"));
    edit_key(&ui, SDL_SCANCODE_BACKSPACE, 0);
    assert(!strcmp(ui.edit, "路径/资"));
    edit_key(&ui, SDL_SCANCODE_ESCAPE, 0);
    assert(!ui.editing && !strcmp(fd2_get_settings()->game_dir, "original_game"));
    mouse_click(&ui, 40, 88 + VIDEO * 43 + 10);
    mouse_click(&ui, 755, ROW_Y + 10);
    assert(ui.page == VIDEO && fd2_get_settings()->window_scale == 4 && fd2_settings_original());
    mouse_click(&ui, 710, 28);
    assert(ui.english);
    mouse_click(&ui, 40, 88 + SOUND * 43 + 10);
    ui.index = ui.count - 1;
    activate_row(&ui);
    assert(ui.preview);
    stop_preview(&ui);
    mouse_click(&ui, 40, 88 + GAMEPAD * 43 + 10);
    ui.focus = ROWS;
    ui.index = 3; /* 死区、恢复、修饰之后是虚拟 Up。 */
    pad_command(&ui, SDL_GAMEPAD_BUTTON_SOUTH);
    assert(ui.capture);
    pad_command(&ui, SDL_GAMEPAD_BUTTON_NORTH);
    assert(!ui.capture && fd2_pad_binding(SDL_GAMEPAD_BUTTON_NORTH).key == SDL_SCANCODE_UP);
    pad_command(&ui, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(ui.page == KEYBOARD);
    ui.index = 2;
    activate_row(&ui);
    command(&ui, SDL_SCANCODE_J, 0);
    assert(fd2_keyboard_binding(SDL_SCANCODE_J).key == SDL_SCANCODE_UP);
    command(&ui, SDL_SCANCODE_DELETE, 0);
    assert(fd2_keyboard_binding(SDL_SCANCODE_J).key == SDL_SCANCODE_J);
    perform(&ui, RESET);
    assert(fd2_get_settings()->window_scale == 3 && fd2_pad_binding(SDL_GAMEPAD_BUTTON_NORTH).key == SDL_SCANCODE_F1);
    perform(&ui, SAVE);
    assert(!ui.done && !config.path && *ui.status);
    perform(&ui, EXIT);
    assert(ui.done && !ui.result);
    SDL_DestroyRenderer(ui.renderer);
    SDL_DestroyWindow(ui.window);
    SDL_Quit();
    return 0;
}
