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

int main(void) {
    CHECK(test_turn_events() == 0);
    CHECK(test_movement_script() == 0);
    CHECK(test_stage0_scripts() == 0);
    CHECK(test_cell_events() == 0);
    puts("field event tests: ok");
    return 0;
}
