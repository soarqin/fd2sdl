#include <stdio.h>
#include <string.h>

#include "field_event.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

static int test_turn_events(void) {
    fd2_field_metadata metadata;
    memset(&metadata, 0xff, sizeof(metadata));
    metadata.turn_events[0] = (fd2_field_turn_event){3, 0, 1};
    metadata.turn_events[1] = (fd2_field_turn_event){3, 4, 0};
    metadata.turn_events[2] = (fd2_field_turn_event){3, 7, 1};

    fd2_field_turn_event_match matches[2];
    CHECK(fd2_field_turn_events_find(&metadata, 3, 1,
                                     matches, 2) == 2);
    CHECK(matches[0].slot == 0 && matches[0].action == 0);
    CHECK(matches[1].slot == 2 && matches[1].action == 7);
    CHECK(fd2_field_turn_events_find(&metadata, 3, 0,
                                     matches, 2) == 1);
    CHECK(matches[0].slot == 1 && matches[0].action == 4);
    CHECK(fd2_field_turn_events_find(&metadata, 256, 1,
                                     matches, 2) == 0);
    return 0;
}

static int test_movement_script(void) {
    fd2_field_units units;
    memset(&units, 0, sizeof(units));
    units.count = 2;
    units.items[0].x = 4;
    units.items[0].y = 5;
    units.items[1].x = 8;
    units.items[1].y = 9;

    const uint8_t script[] = {
        2,
        2, 2, 0, 3, 1, 2,
        0x84, 1, 0, 1,
    };
    CHECK(fd2_field_movement_script_apply(
              &units, script, sizeof(script)) == 0);
    CHECK(units.items[0].x == 6 && units.items[0].y == 5 &&
          units.items[0].direction == 1 && units.items[0].frame_phase == 0);
    CHECK(units.items[1].x == 8 && units.items[1].y == 7 &&
          units.items[1].direction == 2 && units.items[1].frame_phase == 0);
    CHECK(units.walk_frames[0] == 1 && units.walk_frames[1] == 1);

    fd2_field_units saved = units;
    const uint8_t bad_actor[] = {1, 1, 1, 2, 0};
    CHECK(fd2_field_movement_script_apply(
              &units, bad_actor, sizeof(bad_actor)) == -1);
    CHECK(memcmp(&units, &saved, sizeof(units)) == 0);
    const uint8_t truncated[] = {1, 1, 2, 0, 0};
    CHECK(fd2_field_movement_script_apply(
              &units, truncated, sizeof(truncated)) == -1);
    CHECK(memcmp(&units, &saved, sizeof(units)) == 0);
    const uint8_t excessive_groups[] = {0xff};
    CHECK(fd2_field_movement_script_apply(
              &units, excessive_groups, sizeof(excessive_groups)) == -1);
    CHECK(memcmp(&units, &saved, sizeof(units)) == 0);
    const uint8_t invalid_direction[] = {1, 1, 1, 0, 4};
    CHECK(fd2_field_movement_script_apply(
              &units, invalid_direction, sizeof(invalid_direction)) == -1);
    CHECK(memcmp(&units, &saved, sizeof(units)) == 0);
    return 0;
}

static int test_stage0_scripts(void) {
    static const uint8_t ids[] = {3, 4, 6, 7, 8};
    for (size_t i = 0; i < sizeof(ids); i++) {
        const uint8_t *script = NULL;
        size_t size = 0;
        CHECK(fd2_field_stage0_movement_script_get(
                  ids[i], &script, &size) == 0);
        CHECK(script != NULL && size > 3 && script[0] != 0);
    }
    const uint8_t *script = (const uint8_t *)1;
    size_t size = 1;
    CHECK(fd2_field_stage0_movement_script_get(5, &script, &size) == -1);
    CHECK(script == NULL && size == 0);
    /* 全106条独立原源已另由verify_movement_scripts逐字核对；这里检查热执行域。 */
    for (unsigned id=0;id<106;id++) {
        fd2_field_units units={0}; units.count=FD2_FIELD_MAX_UNITS;
        CHECK(fd2_field_movement_script_get((uint8_t)id,&script,&size)==0);
        CHECK(fd2_field_movement_script_apply(&units,script,size)==0);
    }
    fd2_field_units units={0}; units.count=96;
    units.items[1].x=21;units.items[1].y=19;
    CHECK(fd2_field_movement_script_get(24,&script,&size)==0 && size==13);
    CHECK(fd2_field_movement_script_apply(&units,script,size)==0);
    CHECK(units.items[1].direction==5 && units.items[1].x==21 && units.items[1].y==19);
    fd2_field_units before=units;
    const uint8_t bad[]={1,0x80,1,96,0};
    CHECK(fd2_field_movement_script_apply(&units,bad,sizeof(bad))==-1);
    CHECK(!memcmp(&before,&units,sizeof(units)));
    const uint8_t too_many[]={1,0x80,33};
    CHECK(fd2_field_movement_script_apply(&units,too_many,sizeof(too_many))==-1);
    CHECK(!memcmp(&before,&units,sizeof(units)));
    CHECK(fd2_field_movement_script_get(106,&script,&size)==-1 && !script && !size);
    return 0;
}

static int test_cell_events(void) {
    uint32_t cell = 5u | (3u << 16);
    fd2_field_map map = {.width = 1, .height = 1, .cells = &cell};
    fd2_terrain_attr attrs[6] = {{0}};
    fd2_terrain_tileset terrain = {
        .attrs = attrs,
        .attr_count = sizeof(attrs) / sizeof(attrs[0]),
    };
    fd2_field_metadata metadata;
    memset(&metadata, 0xff, sizeof(metadata));
    metadata.cell_lookup[2] = (fd2_field_cell_lookup){9, 1};

    uint8_t code = 0;
    size_t slot = 0;
    CHECK(fd2_field_cell_event_find(&map, &terrain, &metadata,
                                    0, 0, 1, &code, &slot) == 1);
    CHECK(code == 9 && slot == 2);
    CHECK(fd2_field_cell_event_find(&map, &terrain, &metadata,
                                    0, 0, 0, &code, &slot) == 0);

    attrs[5].flags = 0x20;
    CHECK(fd2_field_cell_event_find(&map, &terrain, &metadata,
                                    0, 0, 1, &code, &slot) == 0);
    attrs[5].flags = 0;
    cell = 5u | (17u << 16);
    CHECK(fd2_field_cell_event_find(&map, &terrain, &metadata,
                                    0, 0, 1, &code, &slot) == 0);
    CHECK(fd2_field_cell_event_find(&map, &terrain, &metadata,
                                    1, 0, 1, &code, &slot) == -1);
    return 0;
}

static uint32_t pair_rng(void *userdata) {
    unsigned *calls = userdata;
    (*calls)++;
    return 2;
}

static void empty_inventory(fd2_field_unit *unit) {
    uint8_t *raw = (uint8_t *)unit;
    for (size_t i = 0; i < 8; i++) raw[0x0a + i * 2] = 0x80;
}

static int test_common_predicates_and_writes(void) {
    uint8_t state[32], saved[32];
    for (unsigned i = 0; i < 32; i++) state[i] = (uint8_t)(i + 173u);
    memcpy(saved, state, sizeof(state));
    CHECK(fd2_field_event_state_write(state, 16, 16, 255, 0) == 0);
    CHECK(fd2_field_event_state_write(state, 16, 16, 1, 1) == 0 && state[16] == 0);
    CHECK(!memcmp(state, saved, 16) && !memcmp(state+17, saved+17, 15));
    CHECK(fd2_field_event_state_compare(state, 16, FD2_EVENT_EQ, 0) == 1);
    memcpy(saved, state, sizeof(state));
    CHECK(fd2_field_event_state_write(state, 16, 32, 1, 0) == -1);
    CHECK(fd2_field_event_state_compare(state, 32, FD2_EVENT_EQ, 0) == -1);
    CHECK(!memcmp(saved, state, sizeof(state)));
    CHECK(fd2_field_event_compare_turn(UINT32_MAX, FD2_EVENT_LT, 15) == 1);
    CHECK(fd2_field_event_compare_unsigned(UINT32_MAX, FD2_EVENT_LT, 15) == 0);
    CHECK(fd2_field_event_compare_turn(15, FD2_EVENT_LT, 15) == 0);
    CHECK(fd2_field_event_compare_turn(16, FD2_EVENT_GT, 15) == 1);
    CHECK(fd2_field_event_compare_turn(15, (fd2_field_event_compare)77, 15) == -1);

    fd2_field_metadata meta;
    memset(&meta, 0x9d, sizeof(meta));
    fd2_field_metadata original = meta;
    CHECK(fd2_field_event_schedule_turn(&meta, 2, 255, 2) == 0);
    CHECK(meta.turn_events[2].turn == 1 && meta.turn_events[2].action == 0x9d &&
        meta.turn_events[2].phase == 0x9d);
    original.turn_events[2].turn = 1;
    CHECK(!memcmp(&meta, &original, sizeof(meta)));
    CHECK(fd2_field_event_schedule_turn(&meta, 16, 1, 0) == -1);
    CHECK(fd2_field_event_schedule_turn(&meta, 2, 1, 3) == -1);
    CHECK(!memcmp(&meta, &original, sizeof(meta)));
    CHECK(fd2_field_event_schedule_turn(&meta, 2, UINT32_MAX, 1) == 0 &&
        meta.turn_events[2].turn == 0);

    fd2_field_units units = {0}; units.count = 96;
    units.items[0].flags = 0x85; units.items[0].hp = 7; units.items[0].hp_max = 320;
    units.items[95].flags = 0x80;
    CHECK(fd2_field_event_actor_compare(&units, 0, FD2_EVENT_ACTOR_HP_MAX, FD2_EVENT_LT, 320) == 0);
    CHECK(fd2_field_event_actor_compare(&units, 0, FD2_EVENT_ACTOR_HIDDEN, FD2_EVENT_EQ, 1) == 1);
    CHECK(fd2_field_event_actor_compare(&units, 95, FD2_EVENT_ACTOR_HIDDEN, FD2_EVENT_EQ, 0) == 1);
    CHECK(fd2_field_event_actor_compare(&units, 96, FD2_EVENT_ACTOR_HP, FD2_EVENT_EQ, 0) == -1);
    size_t count = 77;
    CHECK(fd2_field_event_hidden_count(&units, 0, 95, &count) == 0 && count == 1);
    CHECK(fd2_field_event_hidden_count(&units, 95, 96, &count) == -1 && count == 1);
    CHECK(fd2_field_event_state_from_count(state, 21, &units, 3) == 0 && state[21] == 93);
    units.count = 2; memcpy(saved, state, sizeof(state));
    CHECK(fd2_field_event_state_from_count(state, 21, &units, 3) == -1);
    CHECK(!memcmp(saved, state, sizeof(state)));

    fd2_field_unit roster[32] = {0};
    roster[0].text_id = 18; roster[0].flags = 1;
    CHECK(fd2_field_event_roster_has_text(roster, 1, 18) == 1);
    CHECK(fd2_field_event_roster_has_text(roster, 0, 18) == 0);
    CHECK(fd2_field_event_roster_has_text(roster, 33, 18) == -1);
    return 0;
}

static int test_event_inventory_and_rng(void) {
    fd2_field_units units = {0}; units.count = 16;
    for (unsigned i = 0; i < 16; i++) empty_inventory(&units.items[i]);
    CHECK(fd2_field_event_item_append(&units, 0, 209) == 1);
    CHECK(fd2_field_event_item_append(&units, 0, 209) == 1);
    CHECK(fd2_field_event_item_append(&units, 1, 209) == 1);
    CHECK(fd2_field_event_item_append(&units, 2, 210) == 1);
    size_t count = 99;
    CHECK(fd2_field_event_item_occurrences(&units, 0, 15, 209, 214, &count) == 0 && count == 3);
    /* 原循环每actor/item只删一次，raw0同item第二件保留。 */
    units.items[0].attack = 543; units.items[0].defense = 321;
    CHECK(fd2_field_event_item_consume(&units, 0, 15, 209, 214) == 0);
    CHECK(fd2_field_event_item_occurrences(&units, 0, 15, 209, 214, &count) == 0 && count == 1);
    CHECK(units.items[0].attack == 543 && units.items[0].defense == 321);
    fd2_field_units saved = units;
    CHECK(fd2_field_event_item_consume(&units, 0, 16, 209, 214) == -1);
    CHECK(!memcmp(&saved, &units, sizeof(units)));
    /* 不跳过隐藏洞：count=2时原2aedb仍会命中raw0的stale item。 */
    uint8_t *raw = (uint8_t *)&units.items[3];
    raw[0x0b] = 208; raw[0x0c] = 0; raw[0x0d] = 4;
    raw[0x14] = 0; raw[0x15] = 5; raw[0x19] = 0x6a;
    size_t slot = 7;
    CHECK(fd2_field_event_item_find(&units, 3, 208, &slot) == 1 && slot == 0);
    CHECK(fd2_field_event_item_find(&units, 3, 5, &slot) == 0 && slot == 0);
    CHECK(fd2_field_event_item_remove(&units, 3, 208) == 1);
    CHECK(raw[0x0b] == 4 && raw[0x18] == 0x80 && raw[0x19] == 0x6a);
    unsigned calls = 0;
    CHECK(fd2_field_event_mark_random_pair(&units, 13, pair_rng, &calls) == 0 && calls == 1);
    CHECK(units.items[15].flags == 0x80 && units.items[13].flags == 0x80 && units.items[14].flags == 0);
    saved = units;
    CHECK(fd2_field_event_mark_random_pair(&units, 14, pair_rng, &calls) == -1 && calls == 1);
    CHECK(!memcmp(&saved, &units, sizeof(units)));
    return 0;
}

static int test_event_source_tables_and_map_reset(void) {
    const uint8_t *descriptor = NULL;
    CHECK(fd2_field_event_child_descriptor(3,&descriptor)==0 && descriptor[0]==0 &&
        descriptor[1]==11 && descriptor[2]==0);
    const uint8_t *saved=descriptor;
    CHECK(fd2_field_event_child_descriptor(4,&descriptor)==-1 && descriptor==saved);
    uint32_t cells[]={UINT32_C(0x80fffc71),UINT32_C(0x05e25abe)};
    fd2_field_map map={.width=2,.height=1,.cells=cells};
    uint8_t item=0xd3;
    CHECK(fd2_field_event_cell_item(&map,0,0,&item)==-1 && item==0xd3);
    cells[0]=UINT32_C(0x80040071);
    CHECK(fd2_field_event_cell_item(&map,0,0,&item)==0 && item==71);
    CHECK(fd2_field_event_cell_item(&map,2,0,&item)==-1 && item==71);
    CHECK(fd2_field_event_map_reset_cells(&map)==0);
    CHECK(cells[0]==UINT32_C(0xff040071) && cells[1]==UINT32_C(0xff0202be));
    uint32_t before[2];memcpy(before,cells,sizeof(cells));
    map.width=0;
    CHECK(fd2_field_event_map_reset_cells(&map)==-1 && !memcmp(before,cells,sizeof(cells)));
    map.width=257;
    CHECK(fd2_field_event_map_reset_cells(&map)==-1 && !memcmp(before,cells,sizeof(cells)));
    fd2_field_units units={0};units.count=3;
    units.items[0].hp=11;units.items[1].hp=22;units.items[2].hp=33;
    units.items[1].flags=0xa4;units.items[2].flags=0x80;
    CHECK(fd2_field_event_hp_zero_tail(&units,1)==0 && units.items[0].hp==11 &&
        units.items[1].hp==0 && units.items[2].hp==0 && units.items[1].flags==0xa4 && units.items[2].flags==0x80);
    fd2_field_units saved_units=units;
    CHECK(fd2_field_event_hp_zero_tail(&units,18)==0 && !memcmp(&units,&saved_units,sizeof(units)));
    CHECK(fd2_field_event_hp_zero_tail(&units,97)==0 && !memcmp(&units,&saved_units,sizeof(units)));
    CHECK(fd2_field_event_hp_zero_tail(&units,INT32_MAX)==0 && !memcmp(&units,&saved_units,sizeof(units)));
    CHECK(fd2_field_event_hp_zero_tail(&units,SIZE_MAX)==-1 && !memcmp(&units,&saved_units,sizeof(units)));
    return 0;
}

static int test_event_child_continuation(void) {
    fd2_static_flow_node node = {.id="resume", .op="end"};
    fd2_static_flow flow = {"fixture", &node, 1, 0};
    fd2_field_event_resume state = {0};
    state.item_id = 91; state.has_item = 1;
    int pc = -77; size_t actor = 77;
    CHECK(fd2_field_event_child_begin(&state, &flow, 0, 95, 96) == 0);
    fd2_field_event_resume saved = state;
    CHECK(fd2_field_event_child_begin(&state, &flow, 0, 0, 96) == -1);
    CHECK(!memcmp(&state, &saved, sizeof(state)));
    CHECK(fd2_field_event_child_finish(&state, 0, &pc, &actor) == -1);
    CHECK(pc == -77 && actor == 77 && !memcmp(&state, &saved, sizeof(state)));
    CHECK(fd2_field_event_child_finish(&state, 1, &pc, &actor) == 0);
    CHECK(pc == 0 && actor == 95 && !state.child_wait && state.flow == &flow &&
        state.active && state.has_item && state.item_id == 91);
    CHECK(fd2_field_event_child_finish(&state, 1, &pc, &actor) == -1);
    saved = state;
    CHECK(fd2_field_event_child_begin(&state, &flow, 1, 0, 96) == -1);
    CHECK(fd2_field_event_child_begin(&state, &flow, 0, 96, 96) == -1);
    CHECK(!memcmp(&state, &saved, sizeof(state)));
    return 0;
}

static int test_original_dynamic_operands(void) {
    uint8_t state[32]={0};uint32_t value=77;
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_DWORD,256,NULL,&value)==0 && value==256);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_BYTE,256,NULL,&value)==0 && value==0);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_SIGNED_HALF,7,NULL,&value)==0 && value==3);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_SIGNED_HALF,UINT32_MAX,NULL,&value)==0 && value==0);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_SIGNED_HALF,UINT32_C(0xfffffffd),NULL,&value)==0 && value==UINT32_MAX);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_SIGNED_HALF,UINT32_C(0x80000000),NULL,&value)==0 && value==UINT32_C(0xc0000000));
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_PAIR_EVEN_BYTE,13,NULL,&value)==0 && value==254);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_PAIR_ODD_BYTE,13,NULL,&value)==0 && value==255);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_TURN_PAIR_EVEN_BYTE,142,NULL,&value)==0 && value==0);
    state[16]=4;
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16,0,state,&value)==0 && value==4);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR,0,state,&value)==0 && value==20);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16_FIRST_ACTOR,0,state,&value)==0 && value==33);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16_SECOND_ACTOR,0,state,&value)==0 && value==34);
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16_STORY,0,state,&value)==0 && value==6);
    state[16]=255;
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR,0,state,&value)==0 && value==UINT32_C(0xffffff19));
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16_FIRST_ACTOR,0,state,&value)==0 && value==535);
    value=77;
    CHECK(fd2_field_event_source_value(FD2_EVENT_VALUE_STATE16,0,NULL,&value)==-1 && value==77);
    CHECK(fd2_field_event_source_value((fd2_field_event_value_source)10,0,state,&value)==-1 && value==77);
    return 0;
}

int main(void) {
    CHECK(test_turn_events() == 0);
    CHECK(test_movement_script() == 0);
    CHECK(test_stage0_scripts() == 0);
    CHECK(test_cell_events() == 0);
    CHECK(test_common_predicates_and_writes() == 0);
    CHECK(test_event_inventory_and_rng() == 0);
    CHECK(test_event_child_continuation() == 0);
    CHECK(test_event_source_tables_and_map_reset() == 0);
    CHECK(test_original_dynamic_operands() == 0);
    puts("field event tests: ok");
    return 0;
}
