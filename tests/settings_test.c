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
    const fd2_settings *s = fd2_get_settings();
    assert(!strcmp(s->game_dir, ".") && !strcmp(fd2_asset_path(FD2_FDOTHER), "./FDOTHER.DAT"));
    assert(fd2_settings_gameplay_is_default() && s->level_up_growth == FD2_GROWTH_RANDOM);
    assert(s->rodman_recruitment == FD2_RODMAN_WITHOUT_MITTY && s->fix_level_30_experience &&
           !s->ai_attack_high_defense && !s->disable_rest_healing);
    char *explicit_args[] = {"fd2sdl", "--field-play", "2", "--config", "chosen.cfg", NULL};
    const char config[] = "# header\n[VIDEO]\nWindow_Scale=900\nfilter=bad\nOther = Keep\r\n"
        "[paths]\ngame_dir = resources\nsave_path = 用户.SAV\n[extra]\nUntouched= value \n"
        "[pacing]\ntext_speed=fast\n[keyboard]\nEnter = Space, J\nShift+F1 = K\nFutureKey = Q\n";
    assert(SDL_SaveFile("chosen.cfg", config, sizeof(config)-1));
    fd2_config_file file = load(5, explicit_args);
    assert(file.found && !strcmp(file.path, "chosen.cfg") && explicit_args[3] == NULL);
    assert(s->window_scale == 8 && s->filter == 0 && s->text_speed == 1);
    assert(s->level_up_growth == FD2_GROWTH_RANDOM);
    assert(!strcmp(fd2_asset_path(FD2_SAMPLE), "resources/SAMPLE.AD"));
    assert(!fd2_settings_gameplay_is_default());
    assert(SDL_setenv_unsafe("FD2SDL_SAVE_PATH", "override.sav", 1) == 0);
    assert(!strcmp(fd2_save_path(), "override.sav") && !strcmp(s->save_path, "用户.SAV"));
    fd2_binding enter = fd2_binding_parse("Enter");
    assert(fd2_keyboard_bind(SDL_SCANCODE_RETURN2, enter));
    assert(fd2_keyboard_bind(SDL_SCANCODE_KP_COMMA, enter));
    assert(fd2_config_write(&file));
    fd2_config_close(&file);
    char *written = SDL_LoadFile("chosen.cfg", NULL);
    assert(written && strstr(written, "Other = Keep\r\n") &&
           strstr(written, "[extra]\nUntouched= value \n") &&
           strstr(written, "[rules]\nlevel_up_growth = random\n") &&
           strstr(written, "rodman_recruitment = without_mitty\n") &&
           strstr(written, "fix_level_30_experience = on\n") &&
           strstr(written, "ai_attack_high_defense = off\n") &&
           strstr(written, "disable_rest_healing = off\n"));
    assert(!strstr(written, "[video]") && strstr(written, "[VIDEO]"));
    SDL_free(written);
    char *args[] = {"fd2sdl", NULL};
    assert(SDL_setenv_unsafe("FD2SDL_CONFIG", "chosen.cfg", 1) == 0);
    file = load(1, args);
    assert(file.found && s->text_speed == 1 && !strcmp(s->save_path, "用户.SAV"));
    assert(fd2_keyboard_binding(SDL_SCANCODE_J).key == SDL_SCANCODE_RETURN);
    assert(fd2_keyboard_binding(SDL_SCANCODE_KP_COMMA).key == enter.key);
    assert(fd2_keyboard_binding(SDL_SCANCODE_RETURN2).key == enter.key);
    assert(fd2_keyboard_binding(SDL_SCANCODE_SPACE).key == SDL_SCANCODE_RETURN);
    assert(fd2_keyboard_binding(SDL_SCANCODE_K).mods == SDL_KMOD_LSHIFT);
    assert(strstr(file.source, "FutureKey = Q"));
    fd2_settings_defaults();
    assert(fd2_settings_gameplay_is_default() && s->window_scale == 3);
    fd2_config_close(&file);
    assert(SDL_setenv_unsafe("FD2SDL_CONFIG", "new.cfg", 1) == 0);
    file = load(1, args);
    assert(!file.found && !strcmp(file.path, "new.cfg") && fd2_config_write(&file));
    fd2_config_close(&file);
    const char rules[] = "[Rules]\nlevel_up_growth=MiN\nrodman_recruitment=AlWaYs\n"
        "fix_level_30_experience=off\nai_attack_high_defense=on\ndisable_rest_healing=on\n";
    assert(SDL_SaveFile("new.cfg", rules, sizeof(rules)-1));
    file = load(1, args);
    assert(file.found && !fd2_settings_gameplay_is_default() && s->level_up_growth == FD2_GROWTH_MIN);
    assert(s->rodman_recruitment == FD2_RODMAN_ALWAYS && !s->fix_level_30_experience &&
           s->ai_attack_high_defense && s->disable_rest_healing);
    fd2_config_close(&file);
    assert(SDL_unsetenv_unsafe("FD2SDL_CONFIG") == 0);
    assert(SDL_SaveFile("fd2sdl.cfg", config, sizeof(config)-1));
    file = load(1, args);
    assert(file.found && !strcmp(file.path, "fd2sdl.cfg"));
    fd2_config_close(&file);
    char *disable[] = {"app", "--no-config", NULL};
    file = load(2, disable);
    assert(!file.path && fd2_settings_gameplay_is_default() && s->level_up_growth == FD2_GROWTH_RANDOM &&
           s->rodman_recruitment == FD2_RODMAN_WITHOUT_MITTY && s->fix_level_30_experience &&
           !s->ai_attack_high_defense && !s->disable_rest_healing &&
           !fd2_config_write(&file));
    fd2_config_close(&file);
    /* 本轮创建的目录别名指向资源树，原文件和新文件都不得覆盖。 */
    assert(fd2_replace_file("alias/new", "bad", 3, "resources") < 0);
    assert(fd2_replace_file("alias/keep", "bad", 3, "resources") < 0);
    assert(fd2_replace_file("original_game/new", "bad", 3, "resources") < 0);
    assert(fd2_replace_file("original_game/new", "bad", 3, ".") < 0);
    char *kept = SDL_LoadFile("resources/keep", NULL);
    assert(kept && !strcmp(kept, "original"));
    SDL_free(kept);
    assert(!SDL_GetPathInfo("resources/new", NULL));
    assert(!fd2_capture_directory("alias", "resources"));
#ifdef _WIN32
    assert(fd2_replace_file("C:relative.cfg", "bad", 3, "resources") < 0);
    assert(fd2_replace_file("chosen.cfg:alternate", "bad", 3, "resources") < 0);
#endif
    const char broken[] = "[video]\nwindow_scale=1\0end";
    assert(SDL_SaveFile("broken.cfg", broken, sizeof(broken)));
    char *broken_args[] = {"app", "--config", "broken.cfg", NULL};
    file = load(3, broken_args);
    assert(file.unreadable && fd2_get_settings()->window_scale == 3 && !fd2_config_write(&file));
    fd2_config_close(&file);
    assert(fd2_replace_file("用户.cfg", "first", 5, "resources") == 0);
    assert(fd2_replace_file("用户.cfg", "second", 6, "resources") == 0);
    FILE *unicode = fd2_open_read("用户.cfg");
    assert(unicode && fgetc(unicode) == 's' && fclose(unicode) == 0);

    assert(SDL_SaveFile("original_game/FDOTHER.DAT", "legacy", 6));
    fd2_settings_defaults();
    assert(!strcmp(s->game_dir, "original_game"));
    assert(SDL_SaveFile("ANI.DAT", "local", 5));
    fd2_settings_defaults();
    assert(!strcmp(s->game_dir, ".") && !SDL_GetPathInfo(fd2_asset_path(FD2_FDOTHER), NULL));
    assert(fd2_replace_file(fd2_asset_path(FD2_ANI), "bad", 3, s->game_dir) < 0);
    kept = SDL_LoadFile("ANI.DAT", NULL);
    assert(kept && !strcmp(kept, "local"));
    SDL_free(kept);
    assert(SDL_unsetenv_unsafe("FD2SDL_SAVE_PATH") == 0);
    assert(fd2_replace_file(fd2_save_path(), "save", 4, s->game_dir) == 0);
    return 0;
}
