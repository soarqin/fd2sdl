/* field_stage_transition_effect @0x24618（原FUN_0004982c）：完整316B、
 * 六caller八调用及S5原码写集。9帧、等待、加白尾和bank80前后借用均由冷图
 * 编排；这里只提供原临时页/单帧叶，不引入新的Game owner或通用脚本状态。 */
#include "scene_transition.h"

#include "scene.h"
#include "bios_clock.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct fd2_scene_transition_scope {
    fd2_field_game *game;
    fd2_vga *vga;
    fd2_field_page *page;
    fd2_field_page *base;
    size_t stage;
    int center_x, center_y;
};

static int transition_owner(const fd2_scene_transition_scope *scope) {
    if (!scope || !scope->game || !scope->game->ready || !scope->vga ||
            !scope->base || scope->game->stage != scope->stage ||
            scope->vga->field_page != scope->page) return FD2_SCENE_RESULT_ERROR;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0)return FD2_SCENE_RESULT_ERROR;
    return fd2_input_take_quit(&scope->vga->input) ?
        FD2_SCENE_RESULT_HOST_QUIT : FD2_SCENE_RESULT_OK;
}

int fd2_scene_transition_bank80(const fd2_archive *fdother,
        const fd2_field_audio *audio) {
    const uint8_t *data, *sample;
    size_t size, sample_size=0;
    fd2_archive bank = {0};
    if (!fdother || fd2_archive_get(fdother,80,&data,&size) != 0 ||
            fd2_archive_open_mem(&bank,data,size) != 0) return -1;
    int result = fd2_archive_get(&bank,11,&sample,&sample_size);
    if (!sample_size) result = -1;
    /* 无音频设备的已声明域仍核真实bank；开启时只能借同一资源。 */
    if (!result && audio && audio->player && (!audio->battle_bank ||
            audio->battle_bank->archive.size != size ||
            !audio->battle_bank->archive.data ||
            memcmp(audio->battle_bank->archive.data,data,size))) result = -1;
    fd2_archive_close(&bank);
    return result;
}

int fd2_scene_transition_begin(fd2_scene_transition_scope **scope,
        fd2_field_game *game, fd2_vga *vga,
        int relative_cell_x, int relative_cell_y) {
    if (!scope || *scope || !game || !game->ready || !vga || !vga->field_page)
        return FD2_SCENE_RESULT_ERROR;
    int64_t x = (int64_t)relative_cell_x * 24 + 12;
    int64_t y = (int64_t)relative_cell_y * 24 + 16;
    if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX)
        return FD2_SCENE_RESULT_ERROR;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    fd2_scene_transition_scope *created = calloc(1,sizeof(*created));
    if (!created) return FD2_SCENE_RESULT_ERROR;
    created->base = fd2_field_page_create();
    if (!created->base) { free(created); return FD2_SCENE_RESULT_ERROR; }
    created->game=game; created->vga=vga; created->page=vga->field_page;
    created->stage=game->stage; created->center_x=(int)x; created->center_y=(int)y;
    *scope=created;
    /* 24673只铺一次临时terrain；App背景/视觉锚仍沿既有owner推进。 */
    if (fd2_field_game_effect_terrain_begin(game,vga,created->base) != 0) {
        fd2_scene_transition_release(scope);
        return FD2_SCENE_RESULT_ERROR;
    }
    int result=transition_owner(created);
    if (result) fd2_scene_transition_release(scope);
    return result;
}

int fd2_scene_transition_play_sfx(fd2_scene_transition_scope *scope,
        fd2_field_audio *audio) {
    int result=transition_owner(scope);
    if (result) return result;
    if (!audio || !audio->player) return FD2_SCENE_RESULT_OK;
    result=fd2_field_audio_play(audio,FD2_FIELD_SFX_STAGE_TRANSITION);
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0)return FD2_SCENE_RESULT_ERROR;
    return result==0 ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
}

int fd2_scene_transition_frame(fd2_scene_transition_scope *scope,
        unsigned lut_frame, int radius) {
    int result=transition_owner(scope);
    if (result) return result;
    if (lut_frame < 1 || lut_frame > 9 || radius < 1 || radius > 4096)
        return FD2_SCENE_RESULT_ERROR;
    const uint8_t *lut=fd2_field_lut_frame(&scope->game->visuals,lut_frame);
    if (!lut || fd2_field_game_effect_mask_frame(scope->game,scope->vga,
            scope->base,lut,scope->center_x,scope->center_y,radius,0,0) != 0)
        return FD2_SCENE_RESULT_ERROR;
    fd2_vga_upload_current_frame(scope->vga);
    return transition_owner(scope);
}

void fd2_scene_transition_release(fd2_scene_transition_scope **scope) {
    if (!scope || !*scope) return;
    fd2_field_page_destroy((*scope)->base);
    free(*scope);
    *scope=NULL;
}
