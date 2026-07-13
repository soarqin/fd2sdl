/* 炎龙骑士团 2 SDL3 重写 - 正式战场单位预览入口
 *
 * 战场资源、单位和镜头状态由 fd2_field_game 统一管理；本文件仅保留
 * 独立预览的 SDL 事件循环。后续正式玩法入口将复用同一 session。
 */

#include "field_preview.h"

#include <stdio.h>

#include <SDL3/SDL.h>

#include "field_game.h"
#include "input.h"

int fd2_field_preview_run(fd2_vga *vga,
                          const fd2_archive *fdother,
                          size_t stage,
                          int once) {
    fd2_field_game game;
    if (!vga || !fdother) return -1;
    if (stage != 0) {
        fprintf(stderr,
                "field preview currently supports the verified stage 0 roster only\n");
        return -1;
    }
    if (fd2_field_game_open(&game, vga, fdother, stage) != 0) {
        fprintf(stderr, "cannot load stage %zu field session\n", stage);
        return -1;
    }

    printf("field preview: stage=%zu, map=%dx%d, units=%zu",
           game.stage, game.map.width, game.map.height, game.units.count);
    for (uint8_t group = 1; group <= 2; group++) {
        printf(", group%u=%zu", group,
               fd2_field_game_group_count(&game, group));
    }
    printf("%s\n", once ? ", fast" : "");

    int running = 1;
    while (running) {
        fd2_field_game_tick(&game, SDL_GetTicks());
        fd2_field_game_render(&game, vga);
        fd2_vga_present(vga);
        if (once) break;

        if (fd2_input_take_quit(&vga->input)) running = 0;
        fd2_input_action action;
        while (running &&
               fd2_input_take_action(&vga->input, FD2_INPUT_CONTEXT_PREVIEW,
                                     &action)) {
            switch (action) {
                case FD2_INPUT_ACTION_EXIT:
                    running = 0;
                    break;
                case FD2_INPUT_ACTION_LEFT:
                    fd2_field_game_move_camera(&game, -1, 0);
                    break;
                case FD2_INPUT_ACTION_RIGHT:
                    fd2_field_game_move_camera(&game, 1, 0);
                    break;
                case FD2_INPUT_ACTION_UP:
                    fd2_field_game_move_camera(&game, 0, -1);
                    break;
                case FD2_INPUT_ACTION_DOWN:
                    fd2_field_game_move_camera(&game, 0, 1);
                    break;
                default:
                    break;
            }
        }
        fd2_delay_ms(16);
    }

    fd2_field_game_close(&game);
    return 0;
}
