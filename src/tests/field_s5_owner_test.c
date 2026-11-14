/* sub_25089 / sub_1088D(30) @0x25089/0x1088d：直接owner消费门。
 * 25089 expected按完整原码的3个写点；资源30字段/31槽来自独立DAT，
 * 不把本测试当作credits可达/原版画面或完整1088d原码差分。 */
#include "field_game.h"
#include "field_prepare.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return -1;} }while(0)
static unsigned rng_calls;
static uint32_t rng(void *unused) { (void)unused; rng_calls++;return 17; }

static int restore(void) {
    fd2_field_game *g=calloc(1,sizeof(*g)),*expected=malloc(sizeof(*expected)); CHECK(g && expected);
    g->ready=1; g->stage=26; g->roster_count=31; g->attack_rng=rng;
    for(size_t i=0;i<sizeof(g->roster);i++)((uint8_t *)g->roster)[i]=(uint8_t)(i*29u+11u);
    *expected=*g;
    for(size_t i=0;i<31;i++) {
        uint8_t *raw=(uint8_t *)&expected->roster[i];raw[5]=0;
        memcpy(raw+64,raw+66,2);memcpy(raw+68,raw+70,2);
    }
    fd2_static_flow_node node={0};node.op="roster_restore";unsigned port=99;
    CHECK(fd2_field_game_stage_source_node(g,&node,&port)==1 && port==FD2_FLOW_NEXT);
    CHECK(!memcmp(g,expected,sizeof(*g)) && !rng_calls);
    CHECK(fd2_field_game_stage_source_node(g,&node,&port)==1 && !memcmp(g,expected,sizeof(*g)));
    g->roster_count=FD2_SAVE_BATTLE_MAX_ROSTER+1;*expected=*g;
    CHECK(fd2_field_game_stage_source_node(g,&node,&port)==-1 && !memcmp(g,expected,sizeof(*g)));
    free(g);free(expected);return 0;
}

static int resource30(const fd2_archive *other,size_t hot_stage) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_field_game_open_resources(g,other,20)==0);
    g->stage=hot_stage;g->turn_number=0x11223344;g->camera_cell_x=7;g->camera_cell_y=9;
    g->cursor_cell_x=12;g->cursor_cell_y=13;g->ordinary_display_mode=0;
    g->battle_result=FD2_FIELD_BATTLE_VICTORY;g->resource_value=0x24681357;
    g->roster_count=31;g->attack_rng=rng;
    for(size_t i=0;i<31;i++)CHECK(fd2_field_unit_persistent_create(&g->roster[i],(uint8_t)i)==0);
    memset(g->cell_action_completed,0x3d,sizeof(g->cell_action_completed));
    fd2_field_unit original[FD2_SAVE_BATTLE_MAX_ROSTER];memcpy(original,g->roster,sizeof(original));
    CHECK(fd2_field_game_source_credits_setup(g)==0 && g->stage==hot_stage);
    const uint8_t *meta;size_t size;
    CHECK(fd2_archive_get(&g->field_archive,30*3+1,&meta,&size)==0 && size>=131 && meta[1]==31);
    CHECK(g->metadata.unknown_01==meta[1] && g->metadata.unknown_02==meta[2] && g->units.count==31);
    CHECK(!memcmp(g->roster,original,sizeof(original)) && !rng_calls);
    CHECK(g->turn_number==0x11223344 && g->camera_cell_x==7 && g->camera_cell_y==9 &&
        g->cursor_cell_x==12 && g->cursor_cell_y==13 && !g->ordinary_display_mode &&
        g->battle_result==FD2_FIELD_BATTLE_VICTORY && g->resource_value==0x24681357);
    for(size_t i=0;i<sizeof(g->cell_action_completed);i++)CHECK(g->cell_action_completed[i]==0x3d);
    for(size_t i=0;i<31;i++) {
        uint8_t x,y;CHECK(fd2_field_unit_deployment_position(&g->metadata,&g->placements,i,&x,&y)==0);
        const fd2_field_unit *u=&g->units.items[i];
        CHECK(u->x==x && u->y==y && u->text_id==original[i].text_id && u->side==2 &&
            u->direction==0 && u->frame_phase==0 && ((const uint8_t *)u)[49]==255);
        CHECK(fd2_field_unit_cached_frame_index(&g->units,i,0)<g->sprites.frame_count);
    }
    g->stage=27;fd2_field_units before=g->units;
    CHECK(fd2_field_game_source_credits_setup(g)==-1 && !memcmp(&before,&g->units,sizeof(before)));
    fd2_field_game_close(g);free(g);return 0;
}

/* 原生代表开窗前只读校验完整夹具：真实资源/import/export，不查结果或驱动事件。
 * 夹具由独立DAT/原SAV模型产生；未知洞及未用尾逐字节保留，不给生成图作oracle。 */
static int snapshot_fixture(const char *path) {
    fd2_save_file save={0};fd2_save_battle_snapshot before,after;
    fd2_archive other={0};fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_save_file_open(&save,path)==0 &&
        fd2_save_file_get_battle_snapshot(&save,&before)==0);
    uint8_t *plain=malloc(save.size);CHECK(plain);memcpy(plain,save.data,save.size);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0 &&
        fd2_field_game_open_resources(g,&other,before.meta[FD2_SAVE_BATTLE_META_STAGE])==0);
    g->attack_rng=rng;rng_calls=0;
    CHECK(fd2_field_game_import_battle_snapshot(g,&before)==0 && !rng_calls);
    CHECK(g->units.count==before.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] &&
        !memcmp(g->units.items,before.unit_area,g->units.count*FD2_FIELD_UNIT_RECORD_SIZE) &&
        !memcmp(g->roster,before.roster,sizeof(before.roster)) &&
        !memcmp(g->cell_action_completed,before.cell_state,sizeof(before.cell_state)));
    after=before;
    CHECK(fd2_field_game_export_battle_snapshot(g,&after)==0 && !memcmp(&before,&after,sizeof(before)) &&
        fd2_save_file_update_battle_snapshot(&save,&after)==0 &&
        !memcmp(plain,save.data,save.size) && !rng_calls);
    printf("S5 snapshot fixture: stage=%zu active=%zu roster=%zu complete_container_preserved=true RNG=0\n",
        g->stage,g->units.count,g->roster_count);
    fd2_field_game_close(g);fd2_archive_close(&other);fd2_save_file_close(&save);
    free(plain);free(g);return 0;
}

int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"--s5-snapshot"))return snapshot_fixture(argv[2]);
    CHECK(argc==1);
    fd2_archive other={0};
    CHECK(restore()==0 && fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(resource30(&other,26)==0 && resource30(&other,30)==0);
    CHECK(!fd2_field_stage_is_supported(30) && !fd2_field_stage_battle_is_supported(30));
    for(size_t stage=27;stage<=29;stage++) {
        CHECK(!fd2_field_stage_prepare_record_get(stage));
        CHECK(fd2_field_prepare_stage_is_notown(stage) && fd2_field_prepare_stage_is_valid(stage));
    }
    CHECK(!fd2_field_prepare_stage_is_valid(0) && !fd2_field_prepare_stage_is_valid(30) &&
        !fd2_field_prepare_stage_is_valid(SIZE_MAX));
    fd2_archive_close(&other);puts("S5 owner: restore write-set / resource30 distinct from hot26,30 / no RNG: passed");
    return 0;
}
