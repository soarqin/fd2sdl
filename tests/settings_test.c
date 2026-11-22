#include "settings.h"
#include "bindings.h"
#include "host_files.h"
#include <assert.h>
#include <string.h>

static fd2_config_file load(int argc, char **argv) {
    fd2_config_file file;
    assert(fd2_config_open(&file, &argc, argv));
    return file;
}

int main(void) {
    char *explicit_args[] = {"fd2sdl", "--field-play", "2", "--config", "chosen.cfg", NULL};
    const char config[] = "# header\n[VIDEO]\nWindow_Scale=900\nfilter=bad\nOther = Keep\r\n"
        "[paths]\ngame_dir = resources\nsave_path = 用户.SAV\n[extra]\nUntouched= value \n"
        "[pacing]\ntext_speed=fast\n[keyboard]\nEnter = Space, J\nShift+F1 = K\nFutureKey = Q\n";
    assert(SDL_SaveFile("chosen.cfg", config, sizeof(config)-1));
    fd2_config_file file = load(5, explicit_args);
    assert(file.found && !strcmp(file.path, "chosen.cfg") && explicit_args[3] == NULL);
    const fd2_settings *s = fd2_get_settings();
    assert(s->window_scale == 8 && s->filter == 0 && s->text_speed == 1);
    assert(!strcmp(fd2_asset_path(FD2_SAMPLE), "resources/SAMPLE.AD"));
    assert(!fd2_settings_original());
    assert(SDL_setenv_unsafe("FD2SDL_SAVE_PATH", "override.sav", 1) == 0);
    assert(!strcmp(fd2_save_path(), "override.sav") && !strcmp(s->save_path, "用户.SAV"));
    assert(fd2_config_write(&file));
    fd2_config_close(&file);
    char *written = SDL_LoadFile("chosen.cfg", NULL);
    assert(written && strstr(written, "Other = Keep\r\n") &&
           strstr(written, "[extra]\nUntouched= value \n"));
    SDL_free(written);
    char *args[] = {"fd2sdl", NULL};
    assert(SDL_setenv_unsafe("FD2SDL_CONFIG", "chosen.cfg", 1) == 0);
    file = load(1, args);
    assert(file.found && s->text_speed == 1 && !strcmp(s->save_path, "用户.SAV"));
    assert(fd2_keyboard_binding(SDL_SCANCODE_J).key == SDL_SCANCODE_RETURN);
    assert(fd2_keyboard_binding(SDL_SCANCODE_SPACE).key == SDL_SCANCODE_RETURN);
    assert(fd2_keyboard_binding(SDL_SCANCODE_K).mods == SDL_KMOD_LSHIFT);
    assert(strstr(file.source, "FutureKey = Q"));
    fd2_settings_defaults();
    assert(fd2_settings_original() && s->window_scale == 3);
    fd2_config_close(&file);
    assert(SDL_setenv_unsafe("FD2SDL_CONFIG", "new.cfg", 1) == 0);
    file = load(1, args);
    assert(!file.found && !strcmp(file.path, "new.cfg") && fd2_config_write(&file));
    fd2_config_close(&file);
    assert(SDL_unsetenv_unsafe("FD2SDL_CONFIG") == 0);
    assert(SDL_SaveFile("fd2sdl.cfg", config, sizeof(config)-1));
    file = load(1, args);
    assert(file.found && !strcmp(file.path, "fd2sdl.cfg"));
    fd2_config_close(&file);
    char *disable[] = {"app", "--no-config", NULL};
    file = load(2, disable);
    assert(!file.path && fd2_settings_original() && !fd2_config_write(&file));
    fd2_config_close(&file);
    /* 本轮创建的目录别名指向资源树，原文件和新文件都不得覆盖。 */
    assert(fd2_replace_file("alias/new", "bad", 3, "resources") < 0);
    assert(fd2_replace_file("alias/keep", "bad", 3, "resources") < 0);
    assert(fd2_replace_file("original_game/new", "bad", 3, "resources") < 0);
    char *kept = SDL_LoadFile("resources/keep", NULL);
    assert(kept && !strcmp(kept, "original"));
    SDL_free(kept);
    assert(!SDL_GetPathInfo("resources/new", NULL));
    assert(fd2_replace_file("用户.cfg", "first", 5, "resources") == 0);
    assert(fd2_replace_file("用户.cfg", "second", 6, "resources") == 0);
    FILE *unicode = fd2_open_read("用户.cfg");
    assert(unicode && fgetc(unicode) == 's' && fclose(unicode) == 0);
    return 0;
}
