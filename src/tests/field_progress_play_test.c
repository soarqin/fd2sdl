/* B4呈现边界：1e292/1e529/1a866，真实资源，无窗口；不替代原版WGC。 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "field_game.h"
#include "portrait.h"
#include "scene.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static uint64_t now_ms;
static unsigned test_frames, rng_calls, live_images, allocation_calls, fail_allocation;
static unsigned quit_at, key_at, clock_step, arrow_replies;
static int auto_arrows, arrow_held, quit_when_arrow;
static unsigned arrow_draws, arrow_erases;
static fd2_field_game *loot_wait_game;
static int auto_loot_keys, loot_key_sent, quit_when_loot_ready;
static fd2_field_game fixture_game;
static fd2_archive fixture_other;
static uint8_t arrow_pixels[2][140];
static fd2_field_progress_notice script[8];
static unsigned script_count, script_index, acknowledged;
static unsigned resume_calls;
static uint64_t close_started;

static void *test_malloc(size_t size) {
    if (++allocation_calls == fail_allocation) return NULL;
    return malloc(size);
}
static void *test_calloc(size_t count, size_t size) {
    if (++allocation_calls == fail_allocation) return NULL;
    return calloc(count, size);
}
static int test_decode(fd2_image *image, const fd2_archive *archive, size_t id, size_t frame) {
    int result = fd2_portrait_decode_frame(image, archive, id, frame);
    if (result == 0) live_images++;
    return result;
}
static void test_image_free(fd2_image *image) {
    if (image && image->pixels) { CHECK(live_images != 0); live_images--; }
    fd2_image_free(image);
}
static uint64_t test_ticks(void) { return now_ms; }
static void test_delay(uint32_t delay) { now_ms += delay; }
static int test_audio(fd2_field_audio *audio, fd2_field_sfx cue) { (void)audio; (void)cue; return 0; }
static int test_water(fd2_vga *vga, uint64_t now) { (void)vga; (void)now; return 0; }
static int test_message_arrow(fd2_vga *vga, const fd2_archive *other, unsigned frame) {
    if(frame==13)arrow_erases++;else arrow_draws++;
    return fd2_scene_field_message_arrow(vga,other,frame);
}
static void test_render(const fd2_field_game *source, fd2_vga *vga) {
    (void)source; memset(vga->framebuffer, 0x55, sizeof(vga->framebuffer));
}
static int has_arrow(const fd2_vga *vga) {
    for (size_t image = 0; image < 2; image++) {
        int match = 1;
        for (int row = 0; row < 10; row++)
            if (memcmp(vga->framebuffer + (181 + row) * VGA_STRIDE + 191,
                       arrow_pixels[image] + row * 14, 14)) match = 0;
        if (match) return 1;
    }
    return 0;
}
static void test_frame(fd2_vga *vga, unsigned interval) {
    test_frames++;
    now_ms += clock_step ? clock_step : interval;
    fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 0);
    int arrow = auto_arrows && has_arrow(vga);
    if (!arrow) arrow_held = 0;
    if (test_frames == key_at || (arrow && !arrow_held)) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        if (arrow) { arrow_held = 1; arrow_replies++; }
    }
    if (auto_loot_keys && loot_wait_game && loot_wait_game->loot.text_ready && !loot_key_sent) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        loot_key_sent=1;
    }
    fd2_input_begin_test_frame(&vga->input, now_ms);
    if (test_frames == quit_at || (quit_when_arrow && has_arrow(vga))) vga->input.quit_requested = 1;
    if (quit_when_loot_ready && loot_wait_game && loot_wait_game->loot.text_ready)
        vga->input.quit_requested=1;
    CHECK(test_frames < 2000);
}
static void test_present(fd2_vga *vga) { test_frame(vga, 16); }
static void test_present_timed(fd2_vga *vga, uint32_t interval) { test_frame(vga, interval); }
static uint32_t test_rng(void *context) { (void)context; rng_calls++; return 0; }
static int test_peek(const fd2_field_game *source, fd2_field_progress_notice *notice) {
    (void)source;
    if (script_index == script_count) return 0;
    *notice = script[script_index]; return 1;
}
static int test_resume(fd2_field_game *source) { (void)source; resume_calls++; return script_index < script_count; }
static int test_ack(fd2_field_game *source) {
    CHECK(script_index < script_count);
    const fd2_field_progress_notice *notice = &script[script_index];
    if (notice->kind == FD2_FIELD_PROGRESS_GAIN) source->units.items[0].level++;
    if (notice->kind == FD2_FIELD_PROGRESS_LEVEL) CHECK(arrow_replies == 1);
    if (notice->kind == FD2_FIELD_PROGRESS_GROWTH) {
        CHECK(arrow_replies == 2 && source->units.items[0].attack == 10);
        source->units.items[0].attack += (uint16_t)notice->value;
        ((uint8_t *)&source->units.items[0])[0x1c] |= 2; /* LEARN在呈现前OR17。 */
    }
    if (notice->kind == FD2_FIELD_PROGRESS_CLOSE) {
        CHECK(now_ms >= close_started + 11u * 55u);
        CHECK(source->units.items[0].experience == 99 && source->experience_value == 5);
        source->units.items[0].experience = 4;
        source->experience_value = 0;
    }
    script_index++; acknowledged++;
    if (script_index < script_count && script[script_index].kind == FD2_FIELD_PROGRESS_CLOSE)
        close_started = now_ms;
    return 0;
}

#define malloc test_malloc
#define calloc test_calloc
#define fd2_portrait_decode_frame test_decode
#define fd2_image_free test_image_free
#define SDL_GetTicks test_ticks
#define fd2_delay_ms test_delay
#define fd2_field_audio_play test_audio
#define fd2_field_water_tick test_water
#define fd2_scene_field_message_arrow test_message_arrow
#define fd2_field_game_render test_render
#define fd2_vga_present test_present
#define fd2_vga_present_timed test_present_timed
#define fd2_field_game_progress_peek test_peek
#define fd2_field_game_progress_ack test_ack
#define fd2_field_game_progress_resume test_resume
#include "field_play.c"
#undef malloc
#undef calloc
#undef fd2_portrait_decode_frame
#undef fd2_image_free
#undef SDL_GetTicks
#undef fd2_delay_ms
#undef fd2_field_audio_play
#undef fd2_field_water_tick
#undef fd2_scene_field_message_arrow
#undef fd2_field_game_render
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_field_game_progress_peek
#undef fd2_field_game_progress_ack
#undef fd2_field_game_progress_resume

static void reset(fd2_vga *vga) {
    CHECK(live_images == 0);
    now_ms = test_frames = rng_calls = allocation_calls = fail_allocation = 0;
    quit_at = key_at = clock_step = arrow_replies = 0;
    script_count = script_index = acknowledged = resume_calls = 0;
    auto_arrows = arrow_held = quit_when_arrow = 0;
    arrow_draws=arrow_erases=0;
    loot_wait_game=NULL;auto_loot_keys=loot_key_sent=quit_when_loot_ready=0;
    memset(vga, 0, sizeof(*vga));
    memset(vga->framebuffer, 0x55, sizeof(vga->framebuffer));
    fd2_input_init(&vga->input);
}
static void test_resources(void) {
    fd2_field_progress_notice notice = {FD2_FIELD_PROGRESS_GAIN, 0, 488, 630, 0, 0};
    field_progress_text text;
    CHECK(progress_text_prepare(&fixture_game, &notice, &text) == 0);
    CHECK(text.count == 9 && text.tokens[4] == 6 && text.tokens[5] == 3 && text.tokens[6] == 0);
    CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == 0 && live_images == 0 && rng_calls == 0);
    notice.value = UINT32_MAX; CHECK(progress_text_prepare(&fixture_game, &notice, &text) == -1);
    notice = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_LEARN, 0, 587, 0, 17, 0};
    CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == 0);
    notice.magic_id = 36; CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == -1);
    notice.magic_id = 17; fixture_game.units.items[0].unit_id = 255;
    CHECK(progress_resources_preflight(&fixture_game, &notice, &fixture_other) == -1 && live_images == 0);
    fixture_game.units.items[0].unit_id = 0;
    fd2_archive damaged = fixture_other; /* 只借offsets，关闭仍由原owner承担。 */
    damaged.data = malloc(fixture_other.size); CHECK(damaged.data != NULL);
    memcpy(damaged.data, fixture_other.data, fixture_other.size);
    const uint8_t *sheet; size_t sheet_size;
    CHECK(fd2_archive_get(&damaged, 5, &sheet, &sheet_size) == 0 && sheet_size > 86);
    uint32_t last = (uint32_t)sheet[82] | (uint32_t)sheet[83] << 8 |
                    (uint32_t)sheet[84] << 16 | (uint32_t)sheet[85] << 24;
    CHECK(last + 4 < sheet_size);
    damaged.data[(size_t)(sheet - damaged.data) + last] = 13; /* frame19须14×10。 */
    fd2_vga pixels; memset(&pixels, 0xa5, sizeof(pixels));
    CHECK(fd2_scene_field_message_box(&pixels, &damaged) == -1);
    for (size_t i = 0; i < sizeof(pixels.framebuffer); i++) CHECK(pixels.framebuffer[i] == 0xa5);
    CHECK(progress_resources_preflight(&fixture_game, &notice, &damaged) == -1 && live_images == 0);
    free(damaged.data);
}
static void test_box_and_wait(void) {
    fd2_vga vga;
    reset(&vga);
    field_progress_box box = {0};
    CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 0) == 0 && test_frames == 6 && live_images == 4);
    CHECK(!memcmp(vga.framebuffer, box.page->framebuffer, sizeof(vga.framebuffer)));
    for (unsigned i = 0; i < 80; i++)
        CHECK(vga.framebuffer[115 * VGA_STRIDE + 87 - i] == box.portraits[0].pixels[i]);
    test_frames = rng_calls = 0; now_ms = 0; clock_step = 110; key_at = 5;
    CHECK(progress_wait(&box, &fixture_game, &vga, &fixture_other, 0, 1) == 0 && rng_calls == 2);
    test_frames = key_at = 0; now_ms = 0; clock_step = 110;
    CHECK(progress_timed_wait(&vga, 10, 0) == 0 && now_ms >= 550);
    test_frames = 0; now_ms = 0; key_at = 1;
    CHECK(progress_timed_wait(&vga, 11, 0) == 0 && now_ms < 605);
    test_frames = key_at = 0;
    CHECK(progress_box_transition(&box, &vga, 0, 0) == 0 && test_frames == 5);
    progress_box_free(&box); CHECK(live_images == 0 && !box.page && !box.background);
    for (unsigned failure = 1; failure <= 2; failure++) {
        reset(&vga); fail_allocation = failure;
        CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 0) == -1 && live_images == 0);
        CHECK(!box.page && !box.background && test_frames == 0);
    }
    reset(&vga); quit_at = 3;
    CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 0) == FD2_SCENE_RESULT_HOST_QUIT);
    progress_box_free(&box); CHECK(live_images == 0);
    reset(&vga); fixture_game.units.items[0].unit_id = 128;
    CHECK(progress_box_open(&box, &fixture_game, &vga, &fixture_other, 0, 1) == 0);
    CHECK(box.page->framebuffer[13 * VGA_STRIDE + 123] == box.portraits[0].pixels[0]);
    progress_portrait(&box, &vga, 1, 0);
    CHECK(!memcmp(vga.framebuffer + 13 * VGA_STRIDE + 123, box.portraits[1].pixels, 80));
    progress_box_free(&box); fixture_game.units.items[0].unit_id = 0;
}
static void test_script_boundary(void) {
    fd2_vga vga;
    reset(&vga); auto_arrows = 1;
    fixture_game.units.items[0].level = 4; fixture_game.units.items[0].attack = 10;
    fixture_game.units.items[0].experience = 99; fixture_game.experience_value = 5;
    script[0] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_GAIN, 0, 488, 5, 0, 0};
    script[1] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_LEVEL, 0, 489, 5, 0, 0};
    script[2] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_GROWTH, 0, 490, 8, 0, 0};
    script[3] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_LEARN, 0, 587, 0, 17, 0};
    script[4] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_CLOSE, 0, 0, 0, 0, 11};
    script_count = 5;
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == 0);
    CHECK(acknowledged == 5 && fixture_game.units.items[0].level == 5 && fixture_game.units.items[0].attack == 18);
    CHECK(fixture_game.units.items[0].experience == 4 && fixture_game.experience_value == 0 && live_images == 0);
    CHECK(arrow_replies == 2 && rng_calls == 2 && !fd2_input_has_any_key(&vga.input));
    reset(&vga);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, 0);
    CHECK(fd2_input_has_any_key(&vga.input));
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == 0);
    CHECK(fd2_input_has_any_key(&vga.input) && !test_frames && !rng_calls);
    reset(&vga); clock_step = 110;
    script[0] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_STATUS, 0, 487, 4, 0, 10};
    script[1] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_STATUS, 0, 481, 0, 0, 10};
    script_count = 2;
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == 0);
    CHECK(acknowledged == 2 && !rng_calls && !arrow_replies && !live_images);
    CHECK(fixture_game.units.items[0].experience == 4 && fixture_game.experience_value == 0);
    reset(&vga); script_count = 1;
    script[0] = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_STATUS, 0, 487, 4, 0, 10};
    fd2_field_unit before = fixture_game.units.items[0];
    vga.input.quit_requested = 1;
    CHECK(play_field_progress(&fixture_game, &vga, &fixture_other, NULL, 0) == FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!resume_calls && !acknowledged && !test_frames && !rng_calls && !live_images);
    CHECK(!memcmp(&before, &fixture_game.units.items[0], sizeof(before)));
    reset(&vga); quit_at = 1;
    CHECK(progress_scroll(&vga, 0) == FD2_SCENE_RESULT_HOST_QUIT && test_frames == 1);
    CHECK(!acknowledged && !rng_calls && !memcmp(&before, &fixture_game.units.items[0], sizeof(before)));
}
static void test_attack_validation_preserves_quit_result(void) {
    fd2_field_game game={0};fd2_vga vga;reset(&vga);
    uint32_t cells[64]={0};fd2_terrain_attr terrain={0};
    game.ready=1;game.active_side=2;game.selected_unit=-1;game.turn_number=1;
    game.map.width=game.map.height=8;game.map.cells=cells;
    game.terrain.attrs=&terrain;game.terrain.attr_count=1;game.units.count=3;
    memset(game.metadata.turn_events,0xff,sizeof(game.metadata.turn_events));
    memset(game.metadata.cell_lookup,0xff,sizeof(game.metadata.cell_lookup));
    for(size_t i=0;i<3;i++) {
        fd2_field_unit *unit=&game.units.items[i];
        unit->unit_id=i==1?68:0;unit->side=i==1?0:2;
        unit->x=(uint8_t)(3+i);unit->y=i==2?7:3;
        unit->hp=unit->hp_max=100;unit->level=1;unit->movement_profile=1;
        unit->movement_points=3;unit->attack=100;unit->accuracy=1000;
        ((uint8_t*)unit)[0x31]=0xff;
        for(size_t slot=0;slot<8;slot++)((uint8_t*)unit)[10+slot*2]=0x80;
    }
    game.units.items[1].hp=10;
    ((uint8_t*)&game.units.items[0])[10]=0x40;((uint8_t*)&game.units.items[0])[11]=0;
    vga.input.quit_requested=1;
    CHECK(validate_attack_flow(&game,1,&vga,&fixture_other,NULL)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!acknowledged && !test_frames && !rng_calls && !live_images);
    /* 独立数据错误保持-1；不能只因同时有quit就把资源/数据失败洗成退出。 */
    game.map.cells=NULL;
    CHECK(validate_attack_flow(&game,1,&vga,&fixture_other,NULL)==-1);
    CHECK(!acknowledged && !test_frames && !rng_calls && !live_images);
}
/* B5共用同一底框：19953四步展开先吞入场键、No短停不等箭头、
 * 交换的-5不能误取新物品名。这里采样原资源，不启动窗口。 */
static void test_loot_presentation_contract(void) {
    fd2_vga vga;
    reset(&vga);
    CHECK(fd2_field_command_assets_open(&fixture_game.command_assets, &fixture_other) == 0);
    field_progress_text text;
    fd2_field_loot_notice notice = {0};
    notice.kind=FD2_FIELD_LOOT_MESSAGE;notice.fragment=425;
    notice.item_id=192;notice.old_item_id=0;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0 && text.count>0);
    notice.old_item_id=255;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==-1);
    notice.old_item_id=0;notice.fragment=432;
    CHECK(loot_resources_preflight(&fixture_game,&notice,&fixture_other)==0);
    CHECK(!rng_calls && !test_frames && !live_images);
    field_progress_box box={0};
    CHECK(progress_box_open(&box,&fixture_game,&vga,&fixture_other,0,1)==0);
    clock_step=110;key_at=10;
    fd2_field_loot_reply reply=FD2_FIELD_LOOT_CANCEL;
    CHECK(loot_choice(&box,&fixture_game,&vga,&reply)==0 && reply==FD2_FIELD_LOOT_YES);
    CHECK(rng_calls==2 && test_frames>=14);
    CHECK(!fd2_input_has_any_key(&vga.input));
    progress_box_free(&box);
    reset(&vga);clock_step=16;key_at=1;
    CHECK(loot_fixed_wait(&vga,200,0)==0 && now_ms>=200 && test_frames>=13);
    CHECK(!fd2_input_has_any_key(&vga.input) && !rng_calls);
    reset(&vga);clock_step=16;quit_at=2;
    CHECK(loot_fixed_wait(&vga,200,0)==FD2_SCENE_RESULT_HOST_QUIT && now_ms<200);
    fd2_field_command_assets_close(&fixture_game.command_assets);
}
/* 190ac的421/422同框累计；1aa1d/190ac外wait0不画也不擦箭头。
 * 与15f84的-3传1独立，不能用test自动点击箭头掩盖原版无箭头的等待。 */
static void test_loot_accumulated_text_and_no_arrow_wait(void) {
    fd2_vga vga;reset(&vga);
    field_progress_box box={0};field_progress_text text;
    fd2_field_loot_notice notice={0};notice.kind=FD2_FIELD_LOOT_MESSAGE;
    notice.fragment=421;
    CHECK(progress_box_open(&box,&fixture_game,&vga,&fixture_other,0,1)==0);
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0);
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&text,99,127,1)==0);
    uint8_t first_line[17*210];
    for(unsigned y=0;y<17;y++)memcpy(first_line+y*210,vga.framebuffer+(127+y)*320+98,210);
    notice.fragment=422;notice.item_id=192;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0);
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&text,99,146,1)==0);
    for(unsigned y=0;y<17;y++)CHECK(!memcmp(first_line+y*210,vga.framebuffer+(127+y)*320+98,210));
    uint8_t unchanged[14*10];
    for(unsigned y=0;y<10;y++)memcpy(unchanged+y*14,vga.framebuffer+(181+y)*320+191,14);
    clock_step=110;key_at=5;rng_calls=0;test_frames=0;now_ms=0;
    CHECK(progress_wait(&box,&fixture_game,&vga,&fixture_other,0,0)==0 && rng_calls==2);
    CHECK(!arrow_draws && !arrow_erases);
    for(unsigned y=0;y<10;y++)CHECK(!memcmp(unchanged+y*14,vga.framebuffer+(181+y)*320+191,14));
    /* 新框423必须先恢复背景/开框，旧421第一行不能沿用。 */
    memcpy(vga.framebuffer,box.background,sizeof(vga.framebuffer));progress_box_free(&box);
    CHECK(progress_box_open(&box,&fixture_game,&vga,&fixture_other,0,1)==0);
    notice.fragment=423;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==0);
    CHECK(field_text_play(&box,&fixture_game,&vga,&fixture_other,NULL,&text,99,127,1)==0);
    unsigned different=0;
    for(unsigned y=0;y<17;y++)for(unsigned x=0;x<210;x++)
        different+=first_line[y*210+x]!=vga.framebuffer[(127+y)*320+98+x];
    CHECK(different>100);
    /* 晚坏动态名在绘制前拒绝，完整VGA不写，也不消费RNG。 */
    uint8_t before[64000];memcpy(before,vga.framebuffer,sizeof(before));
    unsigned previous_rng=rng_calls;notice.fragment=422;notice.item_id=255;
    CHECK(loot_text_prepare(&fixture_game,&notice,&text)==-1);
    CHECK(!memcmp(before,vga.framebuffer,sizeof(before)) && rng_calls==previous_rng);
    test_frames=key_at=0;quit_at=2;now_ms=0;rng_calls=0;
    CHECK(progress_wait(&box,&fixture_game,&vga,&fixture_other,0,0)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(rng_calls==1 && !arrow_draws && !arrow_erases);
    progress_box_free(&box);CHECK(!live_images && !box.page && !box.background);
}
static int test_adjacent(const fd2_field_unit *a,const fd2_field_unit *b,void *context) {
    (void)context;
    return a->side!=b->side && abs((int)a->x-b->x)+abs((int)a->y-b->y)==1;
}
static void test_loot_ui_commit_boundary(void) {
    for(unsigned mode=0;mode<3;mode++) {
        fd2_vga vga;reset(&vga);
        fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
        CHECK(fd2_field_game_open_resources(game,&fixture_other,0)==0);
        for(size_t i=0;i<3;i++) {
            CHECK(fd2_field_units_set(&game->units,i,i==1?96:0,(uint8_t)i,i==1?0:2,
                i==1?8:i==2?10:7,14)==0);
            fd2_field_unit *u=&game->units.items[i];
            u->hp=u->hp_max=i==1?1:100;u->level=1;u->movement_profile=1;
            u->movement_points=3;u->attack=100;u->accuracy=1000;
            ((uint8_t*)u)[0x31]=i==1?0:0xff;
            ((uint8_t*)u)[0x32]=i==1?192:0;
            for(size_t slot=0;slot<8;slot++)((uint8_t*)u)[10+slot*2]=0x80;
        }
        ((uint8_t*)&game->units.items[0])[10]=0x40;
        game->active_side=2;game->selected_unit=0;game->interaction=FD2_FIELD_INTERACTION_COMMAND;
        CHECK(fd2_field_game_set_attack_hooks(game,test_adjacent,NULL,NULL,NULL,test_rng,NULL)==0);
        CHECK(fd2_field_game_set_loot_preflight(game,loot_resources_preflight,&fixture_other)==0);
        CHECK(fd2_field_game_begin_attack(game)==0);
        CHECK(fd2_field_game_resolve_attack(game)==0);
        CHECK(game->loot.notice.fragment==432 && fd2_field_unit_inventory_count(&game->units.items[0])==1);
        test_frames=0;rng_calls=0;
        loot_wait_game=game;auto_loot_keys=1;
        if(mode==1)quit_at=7; /* 六相位开框之后第一字；尚未text_ready。 */
        if(mode==2)quit_when_loot_ready=1;
        int result=play_field_loot(game,&vga,&fixture_other,NULL,0);
        CHECK(result==(mode?FD2_SCENE_RESULT_HOST_QUIT:0));
        CHECK(game->units.items[1].hp==0); /* 上游攻击不因呈现关闭回滚。 */
        CHECK(fd2_field_unit_inventory_count(&game->units.items[0])==(mode==1?1u:2u));
        if(mode==1)CHECK(!game->loot.text_ready);
        if(mode==2)CHECK(game->loot.text_ready && game->loot.notice.fragment==432);
        CHECK(!live_images);
        CHECK(!arrow_draws && !arrow_erases);
        loot_wait_game=NULL;
        fd2_field_game_close(game);free(game);
    }
}
int main(void) {
    CHECK(fd2_archive_open(&fixture_other, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_archive_open(&fixture_game.fdtxt_archive, "original_game/FDTXT.DAT") == 0);
    CHECK(fd2_archive_open(&fixture_game.dato_archive, "original_game/DATO.DAT") == 0);
    CHECK(fd2_text_entry_open_entry(&fixture_game.ui_text, &fixture_game.fdtxt_archive, 0) == 0);
    CHECK(fd2_font_open_fdother(&fixture_game.font, &fixture_other, 4) == 0);
    fixture_game.ready = 1; fixture_game.units.count = 1; fixture_game.attack_rng = test_rng;
    fixture_game.map.cells = calloc(1, sizeof(*fixture_game.map.cells)); CHECK(fixture_game.map.cells != NULL);
    for (unsigned i = 0; i < 2; i++) {
        fd2_vga page = {0};
        CHECK(fd2_scene_field_message_arrow(&page, &fixture_other, 18 + i) == 0);
        for (int row = 0; row < 10; row++)
            memcpy(arrow_pixels[i] + row * 14, page.framebuffer + (181 + row) * VGA_STRIDE + 191, 14);
    }
    test_resources(); test_box_and_wait(); test_script_boundary();
    test_attack_validation_preserves_quit_result(); test_loot_presentation_contract();
    test_loot_accumulated_text_and_no_arrow_wait();
    test_loot_ui_commit_boundary();
    free(fixture_game.map.cells); fd2_text_entry_close(&fixture_game.ui_text);
    fd2_archive_close(&fixture_game.dato_archive); fd2_archive_close(&fixture_game.fdtxt_archive); fd2_archive_close(&fixture_other);
    puts("field_progress_play_test: ok (resources, wait/RNG, staged ACK, cleanup; no GUI)");
    return 0;
}
