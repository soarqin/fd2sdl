/* S3批量接入：原main25bf4的entry分派经正式API消费唯一S2冷图。
 * 原字节/CFG期望由来源账与独立原码门另验；本测试只签真实资源和消费者集成，
 * fast无窗口，不冒称正式按键、自然历史或WGC。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "scene.h"
#include "field_page.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return -1; } } while (0)

static int make_preparation(fd2_save_manual_state *state, size_t stage) {
    static const uint8_t ids[] = {0,9,4,30,1,8};
    memset(state,0,sizeof(*state));
    state->stage_id=(uint8_t)stage;state->unit_count=sizeof(ids);
    state->gold_or_flags=1234;state->options[0]=state->options[2]=state->options[3]=1;
    for (size_t i=0;i<sizeof(ids);i++) {
        fd2_field_unit unit={0};
        CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
        memcpy(state->unit_area+i*sizeof(unit),&unit,sizeof(unit));
    }
    /* 未用持久尾是原owner数据，不由source setup重建或清零。 */
    state->unit_area[FD2_SAVE_BATTLE_MAX_ROSTER*sizeof(fd2_field_unit)-1]=0xa7;
    return 0;
}

static int batch(const fd2_archive *other, fd2_vga *vga, int source_inventory) {
    for (size_t stage=2;stage<=20;stage++) {
        fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
        fd2_save_manual_state preparation;CHECK(make_preparation(&preparation,stage)==0);
        char id[64];snprintf(id,sizeof(id),"stage.source.entry.%03zu",stage);
        CHECK((source_inventory ? fd2_field_game_prepare_stage_source_flow(game,other,&preparation,id)
            : fd2_field_game_prepare_stage(game,other,&preparation))==0);
        CHECK(game->ready && !game->units.count && game->stage==stage);
        CHECK(game->roster_count==preparation.unit_count);
        CHECK(fd2_field_game_bind_visual(game,vga)==0);
        int result=source_inventory ? fd2_scene_play_stage_source_flow(vga,other,game,NULL,1,id)
            : fd2_scene_play_stage_entry(vga,other,game,NULL,1);
        if (result!=FD2_SCENE_RESULT_OK) fprintf(stderr,"S3 entry failed stage=%zu count=%zu result=%d\n",stage,game->units.count,result);
        CHECK(result==FD2_SCENE_RESULT_OK && game->failure==FD2_FIELD_FAILURE_NONE);
        CHECK(game->stage==stage && game->units.count && game->units.count<=FD2_FIELD_MAX_UNITS);
        CHECK(((uint8_t *)game->roster)[sizeof(game->roster)-1]==0xa7);
        CHECK(game->resource_value==1234 && !game->event_resume.active && !game->loot.source);
        if (!source_inventory) {
            CHECK(fd2_field_stage_is_supported(stage));
            CHECK(fd2_field_stage_source_is_bound(stage,"entry"));
            CHECK(fd2_field_stage_battle_is_supported(stage)==(stage<20));
            fd2_save_battle_snapshot snapshot={0};
            CHECK(fd2_field_game_export_battle_snapshot(game,&snapshot)==0);
            CHECK(fd2_field_game_import_battle_snapshot(game,&snapshot)==0);
            CHECK(game->units.count==snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT]);
            if (stage==20) {
                fd2_field_units prior=game->units;
                uint32_t turn=game->turn_number;
                CHECK(fd2_field_game_end_player_turn(game)==-1 && game->turn_number==turn &&
                    !memcmp(&prior,&game->units,sizeof(prior)));
                CHECK(fd2_field_game_begin_player_controller(game,0)==-1 &&
                    !memcmp(&prior,&game->units,sizeof(prior)));
            }
        }
        printf("S3 entry stage=%zu active=%zu roster=%zu mode=%s\n",stage,game->units.count,
            game->roster_count,source_inventory?"source-inventory":"shipping-integration");fflush(stdout);
        if (!source_inventory && stage==16) {
            fd2_field_game_close(game);
            fd2_field_unit recruited={0};CHECK(fd2_field_unit_persistent_create(&recruited,18)==0);
            memcpy(preparation.unit_area+5*sizeof(recruited),&recruited,sizeof(recruited));
            CHECK(fd2_field_game_prepare_stage(game,other,&preparation)==0 && !game->units.count);
            CHECK(fd2_field_game_bind_visual(game,vga)==0 &&
                fd2_scene_play_stage_entry(vga,other,game,NULL,1)==FD2_SCENE_RESULT_OK);
            CHECK(game->units.count==52 && game->roster[5].text_id==18);
            fd2_save_battle_snapshot snapshot={0};
            CHECK(fd2_field_game_export_battle_snapshot(game,&snapshot)==0 &&
                fd2_field_game_import_battle_snapshot(game,&snapshot)==0 && game->units.count==52);
            puts("S3 entry stage=16 roster-text18 early branch active=52: snapshot preserved");
        }
        fd2_field_game_close(game);free(game);
    }
    return 0;
}

int main(int argc,char **argv) {
    int inventory=argc==2 && !strcmp(argv[1],"--source-inventory");
    if (argc>1 && !inventory) return 2;
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    fd2_vga *vga=calloc(1,sizeof(*vga));CHECK(vga);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    fd2_input_init(&vga->input);
    const uint8_t *palette;size_t size;
    CHECK(fd2_archive_get(&other,0,&palette,&size)==0 && size>=768);
    fd2_vga_set_palette(vga,palette);
    int result=batch(&other,vga,inventory);
    fd2_field_page_destroy(vga->field_page);free(vga);fd2_archive_close(&other);
    return result ? 1 : 0;
}
