/* B4 field_unit_experience_reward_execute @0x1e292（原sub_1E292）、
 * field_growth_stat_present_apply @0x1e529（原sub_1E529）及1a866逐条写序。
 * 仅headless core ACK，不能代替真实提示/输入/RNG/原版运行验收。 */
#include <stdio.h>
#include <string.h>
#include "field_game.h"
#include "field_ai.h"
#include "test_temp.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static uint32_t cells[24*24];
static fd2_terrain_attr terrain;
static unsigned calls;
static uint32_t rng(void *unused) { (void)unused; calls++; return 0; }
static void init(fd2_field_game *g) {
    memset(g,0,sizeof(*g)); memset(cells,0,sizeof(cells)); memset(&terrain,0,sizeof(terrain)); calls=0;
    g->ready=1;g->turn_number=1;g->active_side=2;g->selected_unit=-1;g->detail_unit=-1;
    g->map.width=g->map.height=24;g->map.cells=cells;g->terrain.attrs=&terrain;g->terrain.attr_count=1;
    g->units.count=3;g->attack_rng=rng;g->cursor_cell_x=7;g->cursor_cell_y=14;
    memset(g->metadata.turn_events,0xff,sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup,0xff,sizeof(g->metadata.cell_lookup));
    for(size_t i=0;i<3;i++) {
        fd2_field_unit *u=&g->units.items[i];u->unit_id=i==0?9:i==1?0:96;
        u->side=i==2?0:2;u->x=(uint8_t)(7+i);u->y=14;u->level=4;u->movement_profile=1;
        u->hp=20;u->hp_max=28;u->mp=5;u->mp_max=8;u->direction=3;
        for(size_t j=0;j<8;j++)((uint8_t*)u)[10+j*2]=0x80;
        ((uint8_t*)u)[0x31]=0xff;
        ((uint8_t*)u)[0x37]=10;((uint8_t*)u)[0x39]=10;((uint8_t*)u)[0x3e]=10;
    }
}
static int drain(fd2_field_game *g) {
    fd2_field_progress_notice n;
    for(unsigned i=0;i<512;i++) {
        int pending=fd2_field_game_progress_peek(g,&n);CHECK(pending>=0);
        if(!pending) { CHECK(fd2_field_game_progress_resume(g)>=0); pending=fd2_field_game_progress_peek(g,&n); }
        if(!pending)return 0;
        CHECK(fd2_field_game_progress_ack(g)==0);
    }
    CHECK(0);return 1;
}
static int test_segmented_growth_learning_and_owner(void) {
    fd2_field_game g;init(&g);fd2_field_unit *u=&g.units.items[0];
    u->experience=99;g.experience_value=5;
    fd2_field_unit before=*u, expected;
    CHECK(fd2_field_unit_reward_preview(u,5,NULL,NULL,&expected)==1);
    CHECK(fd2_field_game_complete_player_controller(&g,0)==0);
    fd2_field_progress_notice n;
    CHECK(fd2_field_game_progress_peek(&g,&n)==1 && n.kind==FD2_FIELD_PROGRESS_GAIN && n.value==5);
    CHECK(memcmp(u,&before,80)==0 && g.experience_value==5);
    fd2_field_game frozen=g;
    CHECK(fd2_field_game_move_cursor(&g,1,0)==0);
    CHECK(fd2_field_game_open_system_menu(&g)==-1);
    CHECK(fd2_field_game_end_player_turn(&g)==-1);
    CHECK(memcmp(&frozen,&g,sizeof(g))==0);
    CHECK(fd2_field_game_progress_ack(&g)==0 && u->level==5 && u->experience==99);
    CHECK(u->hp_max==28 && u->mp_max==8 && g.progress.notice.fragment==489);
    CHECK(fd2_field_game_progress_ack(&g)==0);
    unsigned growth=0,learn=0;
    while(g.progress.notice.kind!=FD2_FIELD_PROGRESS_CLOSE) {
        n=g.progress.notice;
        if(n.kind==FD2_FIELD_PROGRESS_GROWTH) {
            static const size_t offsets[]={0x37,0x39,0x3e,0x42,0x46};
            size_t at=offsets[n.fragment-490];uint8_t *raw=(uint8_t*)u;
            uint16_t value=(uint16_t)(raw[at]|((uint16_t)raw[at+1]<<8));
            CHECK(fd2_field_game_progress_ack(&g)==0);
            CHECK((uint16_t)(raw[at]|((uint16_t)raw[at+1]<<8))==(uint16_t)(value+n.value));growth++;
        } else { CHECK(n.kind==FD2_FIELD_PROGRESS_LEARN && n.magic_id==17);
            CHECK(((uint8_t*)u)[0x1c]&2);CHECK(fd2_field_game_progress_ack(&g)==0);learn++; }
        CHECK(u->experience==99 && g.experience_value==5);
    }
    CHECK(growth>0 && learn==1 && g.progress.notice.wait_ticks==11);
    CHECK(u->hp==20 && u->mp==5 && u->direction==3);
    CHECK(fd2_field_game_progress_ack(&g)==0 && u->experience==4 && g.experience_value==0);
    CHECK(memcmp(u,&expected,80)==0 && calls==0 && !g.progress.resume);
    return 0;
}
static int test_raw_cap_early_return_and_multi_level(void) {
    fd2_field_game g;init(&g);g.units.items[0].experience=0;g.experience_value=630;
    /* AI1548e没有player cap99；本测试构造其实际续态起点，不使用player complete。 */
    g.progress.notice=(fd2_field_progress_notice){FD2_FIELD_PROGRESS_GAIN,0,488,630,0,0};
    g.progress.total=630;
    CHECK(drain(&g)==0 && g.units.items[0].level==10 && g.units.items[0].experience==30);
    init(&g);g.experience_value=630;CHECK(fd2_field_game_complete_player_controller(&g,0)==0);
    CHECK(g.progress.notice.value==99 && g.experience_value==99);CHECK(drain(&g)==0 && g.units.items[0].experience==99);
    for(unsigned which=0;which<3;which++) {
        init(&g);g.experience_value=which==2?0:33;
        if(which==0)g.units.items[0].flags=1;else if(which==1)g.units.items[0].level=40;
        CHECK(fd2_field_game_complete_player_controller(&g,0)==0);
        CHECK(!g.progress.resume && g.progress.notice.kind==FD2_FIELD_PROGRESS_NONE);
        CHECK(g.experience_value==(which==2?0:33));
    }
    return 0;
}
typedef struct { unsigned calls; uint16_t reject; } preflight;
static int reject_notice(const fd2_field_game *g,const fd2_field_progress_notice *n,void *data) {
    preflight *p=data;(void)g;p->calls++;return n->fragment==p->reject?-1:0;
}
static int test_late_resource_preflight_before_any_growth(void) {
    fd2_field_game g;init(&g);g.units.items[0].experience=99;g.experience_value=5;
    preflight p={0,587};CHECK(fd2_field_game_set_progress_preflight(&g,reject_notice,&p)==0);
    fd2_field_units before=g.units;
    CHECK(fd2_field_game_complete_player_controller(&g,0)==-1);
    CHECK(g.failure==FD2_FIELD_FAILURE_ACTION && p.calls>3 && !calls);
    CHECK(memcmp(&g.units,&before,sizeof(before))==0 && g.experience_value==5 && !g.progress.resume);
    return 0;
}
static int test_turn_deferred_then_poison_six_byte_sequence(void) {
    fd2_field_game g;init(&g);fd2_field_unit *u=&g.units.items[2];u->side=1;
    u->hp=40;u->hp_max=50;memset(&u->attack_status,1,6);g.experience_value=77;
    g.event_log_count=1;g.event_log[0].kind=FD2_FIELD_EVENT_TURN;g.event_log[0].handled=1;g.event_log[0].presentation_deferred=1;
    CHECK(fd2_field_game_end_player_turn(&g)==0 && g.active_side==1);
    CHECK(u->hp==40 && u->attack_status==1 && !g.progress.notice.kind && g.progress.resume==FD2_FIELD_PROGRESS_RESUME_PHASE);
    CHECK(fd2_field_game_process_automatic_action(&g)==0 && u->hp==40);
    g.event_log[0].presentation_deferred=0;
    CHECK(fd2_field_game_progress_resume(&g)==1 && u->hp==35);
    CHECK(g.progress.notice.fragment==487 && g.progress.notice.value==5 && g.progress.notice.wait_ticks==10);
    CHECK(u->attack_status==1 && u->detail_status[0]==1);
    CHECK(fd2_field_game_progress_ack(&g)==0);
    for(unsigned i=0;i<6;i++) {
        CHECK(g.progress.notice.kind==FD2_FIELD_PROGRESS_STATUS && g.progress.notice.fragment==481+i);
        for(unsigned j=0;j<6;j++)CHECK((&u->attack_status)[j]==(j<=i?0:1));
        CHECK(fd2_field_game_progress_ack(&g)==0);
    }
    CHECK(!g.progress.resume && u->hp==35 && g.experience_value==77 && !calls);
    return 0;
}
static int test_controller_clear_and_learning_does_not_disable_basic(void) {
    fd2_field_game g;init(&g);g.experience_value=44;
    CHECK(fd2_field_game_begin_player_controller(&g,0)==0 && g.experience_value==0);
    g.selected_unit=0;g.interaction=FD2_FIELD_INTERACTION_COMMAND;
    ((uint8_t*)&g.units.items[0])[0x1a]=1;((uint8_t*)&g.units.items[0])[0x1b]=0x20;((uint8_t*)&g.units.items[0])[0x1c]=2;
    CHECK(fd2_field_game_magic_is_available(&g)==1);
    CHECK(fd2_field_game_begin_magic_list(&g)==0);g.magic_list_selected=2;
    uint8_t output=0xa5;
    CHECK(fd2_field_game_magic_list_step(&g,FD2_FIELD_MAGIC_LIST_KEY_CONFIRM,&output)==0 && output==0xa5);
    CHECK(g.interaction==FD2_FIELD_INTERACTION_MAGIC_LIST && g.magic_list_selected==2);
    g.magic_list_selected=1;
    CHECK(fd2_field_game_magic_list_step(&g,FD2_FIELD_MAGIC_LIST_KEY_CONFIRM,&output)==1 && output==13);
    return 0;
}
static int test_actual_ai_counter_reward_and_two_hit_preflight(void) {
    fd2_field_game g;init(&g);g.active_side=0;
    fd2_field_unit *hero=&g.units.items[0],*enemy=&g.units.items[2];
    hero->unit_id=hero->text_id=0;hero->level=1;hero->hp=hero->hp_max=42;
    hero->attack=100;hero->accuracy=100;hero->defense=hero->evasion=0;
    ((uint8_t*)hero)[10]=0x40;((uint8_t*)hero)[11]=0;
    g.units.items[1].x=18;g.units.items[1].y=18;
    enemy->x=8;enemy->y=14;enemy->level=30;enemy->hp=enemy->hp_max=10;
    enemy->attack=3;enemy->defense=enemy->evasion=0;enemy->accuracy=100;enemy->movement_points=1;
    ((uint8_t*)enemy)[10]=0x40;((uint8_t*)enemy)[11]=71;
    fd2_field_path_result range=FD2_FIELD_PATH_RESULT_INITIALIZER;
    fd2_field_ai_physical_candidate plan;
    CHECK(fd2_field_game_compute_move_range(&g,2,&range)==0);
    CHECK(fd2_field_ai_choose_physical_candidate(&g.map,&g.terrain,&g.units,2,0,&range,&plan)==0);
    fd2_field_path_close(&range);
    CHECK(plan.unit_index==0);
    CHECK(fd2_field_game_commit_ai_physical(&g,2,0,&plan,NULL,0)==0);
    CHECK(hero->hp==38 && enemy->hp==0 && enemy->flags==1);
    CHECK(g.progress.notice.kind==FD2_FIELD_PROGRESS_GAIN && g.progress.notice.value==630);
    CHECK(g.progress.notice.actor_index==0 && hero->level==1 && hero->experience==0);
    CHECK(!fd2_field_unit_has_acted(enemy)); /* AI尾在奖励close之后。 */
    CHECK(drain(&g)==0 && hero->level==7 && hero->experience==30);
    CHECK(fd2_field_unit_has_acted(enemy) && g.experience_value==0 && hero->hp==38);
    init(&g);g.selected_unit=0;g.interaction=FD2_FIELD_INTERACTION_COMMAND;
    hero=&g.units.items[0];enemy=&g.units.items[2];
    hero->attack=10;hero->accuracy=100;hero->defense=0;
    ((uint8_t*)hero)[10]=0x40;((uint8_t*)hero)[11]=71;
    enemy->x=8;enemy->y=14;enemy->hp=15;enemy->hp_max=15;enemy->defense=0;enemy->evasion=0;
    ((uint8_t*)enemy)[0x31]=4; /* 单击最大9，双击18会触未知loot。 */
    CHECK(fd2_field_game_begin_attack(&g)==0);g.cursor_cell_x=8;g.cursor_cell_y=14;
    fd2_field_game before=g;calls=0;
    CHECK(fd2_field_game_resolve_attack(&g)==-1 && !calls && memcmp(&g,&before,sizeof(g))==0);
    fd2_field_path_close(&g.move_range);
    return 0;
}

/* C4真实START派生字段：不把MOV0改成1来绕过上层范围门。
 * 通过实际资源open/文件Load/END/完整scheduler/逐条ACK/Save验证逻辑，
 * ACK不代替原版提示、输入、RNG时序或WGC验收。 */
static int test_loaded_zero_movement_counter_scheduler(void) {
    fd2_archive other = {0};
    fd2_save_file fixture = {0}, saved = {0};
    fd2_field_game g = {0};
    fd2_vga vga = {0};
    fd2_save_battle_snapshot initial, final;
    char path[FD2_TEST_PATH_MAX] = {0};
    int result = 1;
#define LOADED_CHECK(x) do { if (!(x)) { \
    fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); goto done; \
} } while (0)
    LOADED_CHECK(fd2_save_file_open(&fixture, "original_game/START.SAV") == 0);
    LOADED_CHECK(fd2_save_file_get_battle_snapshot(&fixture, &initial) == 0);
    LOADED_CHECK(initial.meta[FD2_SAVE_BATTLE_META_STAGE] == 0 &&
                 initial.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == 12);
    fd2_field_unit units[12];
    memcpy(units, initial.unit_area, sizeof(units));
    for (size_t i = 0; i < 12; i++) if (units[i].side != 2) units[i].flags |= 4;
    fd2_field_unit *enemy = &units[11], *hero = &units[0];
    LOADED_CHECK(enemy->unit_id == 96 && enemy->ai_behavior_raw == 0 &&
                 ((uint8_t *)enemy)[0x31] == 255);
    enemy->x = 8; enemy->y = 14; enemy->level = 30;
    enemy->hp = 1; enemy->defense = enemy->evasion = 0;
    enemy->flags &= (uint8_t)~4u;
    ((uint8_t *)enemy)[0x0b] = 71;
    enemy->movement_points = 0;
    enemy->attack = 3; enemy->accuracy = 100;
    hero->accuracy = 200; hero->defense = hero->evasion = 0;
    memcpy(initial.unit_area, units, sizeof(units));
    LOADED_CHECK(fd2_save_file_update_battle_snapshot(&fixture, &initial) == 0);
    LOADED_CHECK(fd2_test_temp_path(path) == 0 && fd2_save_file_write(&fixture, path) == 0);
    LOADED_CHECK(fd2_archive_open(&other, "original_game/FDOTHER.DAT") == 0);
    LOADED_CHECK(fd2_field_game_open(&g, &vga, &other, 0) == 0);
    calls = 0;
    LOADED_CHECK(fd2_field_game_set_attack_hooks(&g, NULL, NULL, NULL, NULL, rng, NULL) == 0);
    LOADED_CHECK(fd2_field_game_load_active(&g, path) == 0);
    LOADED_CHECK(g.units.items[11].movement_points == 0 && calls == 0);
    LOADED_CHECK(fd2_field_game_end_player_turn(&g) == 0);
    unsigned gains = 0, levels = 0, growths = 0, closes = 0, steps;
    for (steps = 0; steps < 256; steps++) {
        fd2_field_progress_notice notice;
        int pending = fd2_field_game_progress_peek(&g, &notice);
        LOADED_CHECK(pending >= 0);
        if (pending) {
            LOADED_CHECK(notice.actor_index == 0);
            if (notice.kind == FD2_FIELD_PROGRESS_GAIN) {
                LOADED_CHECK(notice.value == 630 && g.units.items[0].hp == 38);
                gains++;
            } else if (notice.kind == FD2_FIELD_PROGRESS_LEVEL) levels++;
            else if (notice.kind == FD2_FIELD_PROGRESS_GROWTH) growths++;
            else if (notice.kind == FD2_FIELD_PROGRESS_CLOSE) closes++;
            else LOADED_CHECK(0);
            LOADED_CHECK(fd2_field_game_progress_ack(&g) == 0);
            continue;
        }
        /* 纯逻辑测试显式跳过必需场景演出边界，再允许STATUS/phase继续。 */
        for (size_t i = 0; i < g.event_log_count; i++) g.event_log[i].presentation_deferred = 0;
        LOADED_CHECK(fd2_field_game_progress_resume(&g) >= 0);
        LOADED_CHECK(fd2_field_game_resume_player_controller(&g) >= 0);
        if (g.progress.notice.kind) continue;
        if (g.turn_number == 2 && g.active_side == 2) break;
        LOADED_CHECK(fd2_field_game_process_automatic_action_visual(&g, NULL, 0, 0) >= 0);
    }
    LOADED_CHECK(steps < 256 && gains == 1 && levels == 6 && growths == 24 && closes == 1);
    LOADED_CHECK(g.units.items[0].hp == 38 && g.units.items[0].hp_max == 114 &&
                 g.units.items[0].level == 7 && g.units.items[0].experience == 30);
    LOADED_CHECK(g.units.items[11].hp == 0 && g.units.items[11].flags == 1 &&
                 g.units.items[11].movement_points == 0 && calls > 0);
    LOADED_CHECK(g.experience_value == 0 && !g.progress.resume &&
                 g.progress.notice.kind == FD2_FIELD_PROGRESS_NONE);
    LOADED_CHECK(fd2_field_game_save_active(&g, path) == 0);
    LOADED_CHECK(fd2_save_file_open(&saved, path) == 0 &&
                 fd2_save_file_get_battle_snapshot(&saved, &final) == 0);
    fd2_field_unit persisted;
    memcpy(&persisted, final.unit_area, sizeof(persisted));
    LOADED_CHECK(persisted.hp == 38 && persisted.hp_max == 114 &&
                 persisted.level == 7 && persisted.experience == 30);
    LOADED_CHECK(memcmp(final.unit_area, g.units.items, 12 * sizeof(fd2_field_unit)) == 0);
    LOADED_CHECK(final.meta[FD2_SAVE_BATTLE_META_TURN] == 2 &&
                 final.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == 12);
    LOADED_CHECK(memcmp(final.roster, initial.roster, sizeof(initial.roster)) == 0);
    LOADED_CHECK(memcmp(final.metadata, initial.metadata, sizeof(initial.metadata)) == 0);
    LOADED_CHECK(memcmp(final.unit_area + 12 * 80, initial.unit_area + 12 * 80,
                 sizeof(initial.unit_area) - 12 * 80) == 0);
    LOADED_CHECK(memcmp(saved.data + FD2_SAVE_SLOT_BASE, fixture.data + FD2_SAVE_SLOT_BASE,
                 FD2_SAVE_FILE_SIZE - FD2_SAVE_SLOT_BASE - FD2_SAVE_CHECKSUM_SIZE) == 0);
    result = 0;
done:
    fd2_save_file_close(&saved); fd2_save_file_close(&fixture);
    fd2_field_game_close(&g); fd2_archive_close(&other);
    if (path[0]) (void)remove(path);
#undef LOADED_CHECK
    return result;
}
typedef struct { unsigned index; } physical_rng;
static uint32_t physical_roll(void *userdata) {
    physical_rng *r=userdata;static const uint32_t values[]={50,0,99,0};
    return values[(r->index++)%4];
}
static int test_physical_level_weight_not_magic_normalization(void) {
    for(unsigned which=0;which<4;which++) {
        fd2_field_game g;init(&g);g.selected_unit=0;g.interaction=FD2_FIELD_INTERACTION_COMMAND;
        fd2_field_unit *a=&g.units.items[0],*t=&g.units.items[2];
        g.units.items[1].x=18;g.units.items[1].y=18;
        a->level=which==3?226:2;a->movement_profile=which==1||which==3?9:1;
        a->text_id=which==2?28:9;a->attack=10;a->accuracy=100;
        ((uint8_t*)a)[10]=0x40;((uint8_t*)a)[11]=0;
        t->x=8;t->y=14;t->unit_id=68;t->level=20;t->hp=t->hp_max=100;t->defense=0;t->evasion=0;
        physical_rng r={0};g.attack_rng=physical_roll;g.attack_rng_userdata=&r;
        CHECK(fd2_field_game_begin_attack(&g)==0);g.cursor_cell_x=8;g.cursor_cell_y=14;
        fd2_field_game before=g;
        if(which==3) { CHECK(fd2_field_game_resolve_attack(&g)==-1 && !r.index);
            CHECK(memcmp(&before,&g,sizeof(g))==0); }
        else { CHECK(fd2_field_game_resolve_attack(&g)==0);
            CHECK(t->hp==91 && g.experience_value==(which==0?27:1));
            CHECK(g.progress.notice.value==(which==0?27:1));
            CHECK(drain(&g)==0 && a->experience==(which==0?27:1)); }
        fd2_field_path_close(&g.move_range);
    }
    return 0;
}
static int test_status_recompute_native_double_truncation(void) {
    const int16_t values[]={0,10,12,16,20,40,60,100,-100,-32768,32767};
    const int16_t expected[]={0,11,13,18,23,46,69,114,-114,27853,-27854};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++) {
        fd2_field_game g;init(&g);fd2_field_unit *u=&g.units.items[2];u->side=1;
        u->attack_status=u->defense_status=2;u->hit_evasion_status=1;
        uint8_t *raw=(uint8_t*)u;uint16_t v=(uint16_t)values[i];
        raw[0x37]=raw[0x39]=(uint8_t)v;raw[0x38]=raw[0x3a]=(uint8_t)(v>>8);
        CHECK(fd2_field_game_end_player_turn(&g)==0);
        CHECK(g.progress.notice.fragment==483);CHECK(drain(&g)==0);
        CHECK(u->attack==(uint16_t)expected[i] && u->defense==(uint16_t)expected[i]);
        CHECK(!calls);
    }
    return 0;
}
int main(void) {
    CHECK(test_status_recompute_native_double_truncation()==0);
    CHECK(test_physical_level_weight_not_magic_normalization()==0);
    CHECK(test_actual_ai_counter_reward_and_two_hit_preflight()==0);
    CHECK(test_loaded_zero_movement_counter_scheduler()==0);
    CHECK(test_segmented_growth_learning_and_owner()==0);
    CHECK(test_raw_cap_early_return_and_multi_level()==0);
    CHECK(test_late_resource_preflight_before_any_growth()==0);
    CHECK(test_turn_deferred_then_poison_six_byte_sequence()==0);
    CHECK(test_controller_clear_and_learning_does_not_disable_basic()==0);
    puts("field progress tests: ok (headless ACK only)");return 0;
}
