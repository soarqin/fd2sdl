#ifndef FD2_SETTINGS_H
#define FD2_SETTINGS_H
#include <SDL3/SDL.h>

#define FD2_PATH_SIZE 4096

typedef struct {
    int window_scale, fullscreen, scale_mode, filter, aspect_4_3;
    int volume, mute, gamepad_deadzone, show_on_start;
    int key_repeat_delay_ms, key_repeat_rate_ms, text_speed, animation_speed;
    int new_game_battle_effect, new_game_info_board;
    char game_dir[FD2_PATH_SIZE], save_path[FD2_PATH_SIZE], renderer[128], device[512];
} fd2_settings;

typedef struct {
    const char *section, *key, *label;
    char grade;
    int min, max;
    const char *choices, *initial; /* choices 以 | 分隔；字符串的 capacity 非零。 */
    void *value;
    size_t capacity;
} fd2_setting;

typedef struct {
    char *path, *source;
    bool found, setup, unreadable;
} fd2_config_file;

const fd2_settings *fd2_get_settings(void);
void fd2_settings_defaults(void);
bool fd2_settings_original(void);
const fd2_setting *fd2_settings_table(size_t *count);
bool fd2_setting_set(const fd2_setting *item, const char *value);
const char *fd2_setting_text(const fd2_setting *item, char number[32]);
bool fd2_config_open(fd2_config_file *file, int *argc, char **argv);
bool fd2_config_write(const fd2_config_file *file);
void fd2_config_close(fd2_config_file *file);
const char *fd2_save_path(void);
SDL_AudioDeviceID fd2_output_device(const char *name);

typedef enum {
    FD2_ANI, FD2_BG, FD2_DATO, FD2_FDFIELD, FD2_FDICON, FD2_FDMUS,
    FD2_FDOTHER, FD2_FDSHAP, FD2_FDTXT, FD2_FIGANI, FD2_SAMPLE, FD2_TAI,
    FD2_ASSET_COUNT
} fd2_asset;
const char *fd2_asset_path(fd2_asset asset);
extern const char *const fd2_asset_names[FD2_ASSET_COUNT];
#endif
