/* new_game_opening_play @0x3231b：真实 32→31→0 资源的无窗口交接回归。
 * 私有 TU 仅替换呈现/等待和故障注入，不增加生产测试入口。
 * 终态来自 FDFIELD placement 与原版 script 0/1/2/5 的写集，不读取 START.SAV。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene.h"
#include "portrait.h"
#include "static_data.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint64_t test_ms;
static int presents, quit_at_frame, quit_on_preview_delay;
static uint32_t quit_on_delay_ms;
static unsigned water_calls;
static uint8_t *arrival_page_trace;
static uint32_t arrival_terrain_trace[12];
static const uint8_t *arrival_data_override;
static size_t arrival_data_override_size;
static int archive_opens, archive_closes;
static const char *fail_archive_path;
static const fd2_archive *fail_palette_archive;
static int palette_failure, palette_queries;
static int portrait_frame_override;
static size_t portrait_frame_requested;
static int portrait_decode_calls, fail_portrait_decode_at;
static size_t fail_portrait_identity=(size_t)-1;
static int fail_portrait_frame0_at,portrait_frame0_hits,portrait_calls_at_failure;
static int presents_at_portrait_failure;
static size_t failed_portrait_identity;
static fd2_vga *current_vga;
static fd2_field_page *test_page;
static int trace_fragments;
static size_t fragment_log[32], fragment_log_count, last_fragment;
static const fd2_image *tracked_cursor;
static unsigned cursor_by_fragment[16];
static int current_darkness, presented_darkness[32];
static const fd2_static_flow *bound_flow_override;
static int auto_confirm=1;

static const fd2_static_flow *scene_test_flow_lookup(const char *domain, const char *id) {
    if (bound_flow_override && !strcmp(domain,"scene") && !strcmp(id,bound_flow_override->id))
        return bound_flow_override;
    return fd2_static_flow_lookup(domain,id);
}

static void scene_test_brightness(fd2_vga *vga, int darkness) {
    current_darkness=darkness;
    fd2_vga_set_brightness(vga,darkness);
}

static int scene_test_get_fragment(const fd2_text_entry *text, size_t fragment,
        const uint8_t **tokens, size_t *count) {
    if (trace_fragments) {
        if (fragment_log_count < 32) fragment_log[fragment_log_count++] = fragment;
        last_fragment = fragment;
    }
    return fd2_text_entry_get_fragment(text, fragment, tokens, count);
}
static void scene_test_sprite_blit(fd2_vga *vga, const fd2_image *image,
        int x, int y, int transparent) {
    if (image == tracked_cursor && last_fragment < 16) cursor_by_fragment[last_fragment]++;
    fd2_map_sprite_blit(vga, image, x, y, transparent);
}

static int scene_test_page_write(fd2_field_page *page,int x,int y,int w,int h,
    const uint8_t *pixels,size_t size,size_t stride) {
    /* Scene正式cursor在同工作页24² scratch合成；旧VGA blit不再是观察点。 */
    int matched=tracked_cursor && tracked_cursor->pixels && w==24 && h==24 &&
        tracked_cursor->width==24 && tracked_cursor->height==24 && stride>=24 &&
        size>=23*stride+24;
    if(matched)for(size_t row=0;row<24;row++)for(size_t col=0;col<24;col++) {
        uint8_t value=tracked_cursor->pixels[row*24+col];
        if(value && pixels[row*stride+col]!=value)matched=0;
    }
    int result=fd2_field_page_write(page,x,y,w,h,pixels,size,stride);
    if(!result && matched && last_fragment<16)cursor_by_fragment[last_fragment]++;
    return result;
}


static int scene_test_archive_open(fd2_archive *archive, const char *path) {
    if (fail_archive_path && strcmp(path, fail_archive_path) == 0)
        return -1;
    int result = fd2_archive_open(archive, path);
    if (result == 0) archive_opens++;
    return result;
}

static void scene_test_archive_close(fd2_archive *archive) {
    if (archive && archive->data) archive_closes++;
    fd2_archive_close(archive);
}

static int scene_test_archive_get(const fd2_archive *archive, size_t index,
    const uint8_t **data, size_t *size) {
    if(index==9 && arrival_data_override) {
        *data=arrival_data_override;*size=arrival_data_override_size;return 0;
    }
    if (archive == fail_palette_archive && index == 0) {
        palette_queries++;
        if (palette_failure == 1) {
            *data = NULL;
            *size = 0;
            return -1;
        }
        int result = fd2_archive_get(archive, index, data, size);
        if (result == 0 && palette_failure == 2) *size = 767;
        return result;
    }
    return fd2_archive_get(archive, index, data, size);
}

static int scene_test_portrait_decode_frame(fd2_image *image,
                                             const fd2_archive *archive,
                                             size_t identity, size_t frame) {
    portrait_frame_requested = frame;
    portrait_decode_calls++;
    if(identity==fail_portrait_identity && frame==0)portrait_frame0_hits++;
    if (portrait_decode_calls == fail_portrait_decode_at ||
        (identity==fail_portrait_identity && frame==0 &&
         portrait_frame0_hits==fail_portrait_frame0_at)) {
        presents_at_portrait_failure = presents;
        failed_portrait_identity = identity;
        portrait_calls_at_failure=portrait_decode_calls;
        return -1;
    }
    return fd2_portrait_decode_frame(image, archive, identity,
        portrait_frame_override >= 0 ? (size_t)portrait_frame_override : frame);
}

static void scene_test_present(fd2_vga *vga) {
    if (presents < 32) presented_darkness[presents]=current_darkness;
    presents++;
    if(arrival_page_trace && presents<=12) {
        if(fd2_field_page_read(vga->field_page,0,0,456,336,
            arrival_page_trace+(size_t)(presents-1)*FD2_FIELD_PAGE_SIZE,
            FD2_FIELD_PAGE_SIZE,456)!=0)abort();
        arrival_terrain_trace[presents-1]=vga->field_visual.terrain_phase;
    }
    test_ms += 17;
    /* 非 fast 退出用例自动确认所有对白；不创建 SDL 窗口或注入宿主键。 */
    fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN,
                                auto_confirm ? presents % 2 : 0);
    fd2_input_begin_test_frame(&vga->input, test_ms);
    if (presents == quit_at_frame || presents > 100000)
        vga->input.quit_requested = 1;
}

static void scene_test_present_timed(fd2_vga *vga, uint32_t ms) {
    test_ms += ms;
    scene_test_present(vga);
}

static void scene_test_delay(uint32_t ms) {
    test_ms += ms;
    if (((quit_on_preview_delay && ms == 800) || (quit_on_delay_ms && ms==quit_on_delay_ms)) && current_vga)
        current_vga->input.quit_requested = 1;
}

static uint64_t scene_test_ticks(void) { return test_ms; }
static int scene_test_water_tick(fd2_vga *vga,uint64_t now) {
    water_calls++;
    return fd2_field_water_tick(vga,now);
}

/* core 中仍有正式 scene.o；本 TU 的公开入口私有改名，避免重复定义。 */
#define fd2_scene_play_new_game_prologue_handoff scene_test_play_handoff
#define fd2_scene_play_new_game_prologue_handoff_with_runtime scene_test_play_handoff_with_runtime
#define fd2_scene_play_new_game_prologue scene_test_play_prologue
#define fd2_scene_play_field_event scene_test_play_event
#define fd2_scene_play_common_step scene_test_common_step
#define fd2_scene_common_step_validate scene_test_common_validate
#define fd2_scene_play_stage1_entry scene_test_stage1_entry
#define fd2_scene_play_stage2_entry scene_test_stage2_entry
#define fd2_scene_stage_victory_validate scene_test_stage_victory_validate
#define fd2_scene_play_stage_entry scene_test_stage_entry
#define fd2_scene_play_stage_victory scene_test_stage_victory
#define fd2_scene_play_stage1_victory scene_test_stage1_victory
#define fd2_scene_play_field_story scene_test_field_story
#define fd2_scene_field_story_validate scene_test_story_validate
#define fd2_scene_play_game_over scene_test_game_over
#define fd2_scene_field_message_validate scene_test_message_validate
#define fd2_scene_field_notice_page scene_test_notice_page
#define fd2_scene_field_message_box scene_test_message_box
#define fd2_scene_field_message_arrow scene_test_message_arrow
#define fd2_archive_open scene_test_archive_open
#define fd2_archive_close scene_test_archive_close
#define fd2_archive_get scene_test_archive_get
#define fd2_portrait_decode_frame scene_test_portrait_decode_frame
#define fd2_text_entry_get_fragment scene_test_get_fragment
#define fd2_map_sprite_blit scene_test_sprite_blit
#define fd2_field_page_write scene_test_page_write
#define fd2_vga_present scene_test_present
#define fd2_vga_present_timed scene_test_present_timed
#define fd2_vga_set_brightness scene_test_brightness
#define fd2_delay_ms scene_test_delay
#define SDL_GetTicks scene_test_ticks
#define fd2_field_water_tick scene_test_water_tick
#define fd2_static_flow_lookup scene_test_flow_lookup
#include "../scene.c"
#undef fd2_scene_play_new_game_prologue_handoff
#undef fd2_scene_play_new_game_prologue_handoff_with_runtime
#undef fd2_scene_play_new_game_prologue
#undef fd2_scene_play_field_event
#undef fd2_scene_play_common_step
#undef fd2_scene_common_step_validate
#undef fd2_scene_play_stage1_entry
#undef fd2_scene_play_stage2_entry
#undef fd2_scene_stage_victory_validate
#undef fd2_scene_play_stage_entry
#undef fd2_scene_play_stage_victory
#undef fd2_scene_play_stage1_victory
#undef fd2_scene_play_field_story
#undef fd2_scene_field_story_validate
#undef fd2_scene_play_game_over
#undef fd2_scene_field_message_validate
#undef fd2_scene_field_notice_page
#undef fd2_scene_field_message_box
#undef fd2_scene_field_message_arrow
#undef fd2_archive_open
#undef fd2_archive_close
#undef fd2_archive_get
#undef fd2_portrait_decode_frame
#undef fd2_text_entry_get_fragment
#undef fd2_map_sprite_blit
#undef fd2_field_page_write
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_set_brightness
#undef fd2_delay_ms
#undef SDL_GetTicks
#undef fd2_field_water_tick
#undef fd2_static_flow_lookup

static void reset_controls(fd2_vga *vga) {
    fd2_field_page_destroy(test_page);
    test_page=fd2_field_page_create();
    if(!test_page)abort();
    memset(vga, 0, sizeof(*vga));
    vga->field_page=test_page;
    fd2_input_init(&vga->input);
    test_ms = 1000;
    presents = quit_at_frame = quit_on_preview_delay = 0;
    quit_on_delay_ms=0;water_calls=0;
    arrival_page_trace=NULL;arrival_data_override=NULL;arrival_data_override_size=0;
    archive_opens = archive_closes = 0;
    fail_archive_path = NULL;
    fail_palette_archive = NULL;
    palette_failure = palette_queries = 0;
    portrait_frame_override = -1;
    portrait_frame_requested = (size_t)-1;
    portrait_decode_calls = fail_portrait_decode_at = 0;
    fail_portrait_identity=(size_t)-1;
    fail_portrait_frame0_at=portrait_frame0_hits=portrait_calls_at_failure=0;
    presents_at_portrait_failure = -1;
    failed_portrait_identity = (size_t)-1;
    trace_fragments = 0; fragment_log_count = 0; last_fragment = 0;
    tracked_cursor = NULL; memset(cursor_by_fragment, 0, sizeof(cursor_by_fragment));
    current_darkness=0;memset(presented_darkness,-1,sizeof(presented_darkness));
    bound_flow_override=NULL;
    scene_render_error=0;
    auto_confirm=1;
    current_vga = vga;
}

static uint32_t scene_counted_random(void *userdata) {
    unsigned *calls=userdata;(*calls)++;return 0;
}

static int test_scene_actual_dynamic_consumer(const fd2_archive *fdother) {
    /* 原UI0[439]确含-4/-6；这只是Scene共享token机制夹具，不能称为
     * 原stage剧情自然含该片段（原entry1..33无动态码）。 */
    fd2_archive fdtxt={0};fd2_text_entry names={0};fd2_font font={0};fd2_ui_sheet ui={0};
    CHECK(fd2_archive_open(&fdtxt,"original_game/FDTXT.DAT")==0);
    CHECK(fd2_text_entry_open_entry(&names,&fdtxt,0)==0);
    CHECK(fd2_font_open_fdother(&font,fdother,4)==0);
    CHECK(ui_sheet_open_fdother(&ui,fdother,5)==0);
    fd2_vga actual_vga,expected_vga={0};reset_controls(&actual_vga);auto_confirm=0;
    memset(actual_vga.framebuffer,211,sizeof(actual_vga.framebuffer));
    memset(expected_vga.framebuffer,211,sizeof(expected_vga.framebuffer));
    fd2_scene_field_state field={0};unsigned rng_calls=0;
    field.names=&names;
    field.runtime=(fd2_scene_runtime){scene_counted_random,&rng_calls,{0xfffffffbu,1,2}};
    const uint8_t *tokens;size_t count,total_glyphs=0;int x=0;
    CHECK(fd2_text_entry_get_fragment(&names,439,&tokens,&count)==0);
    for(size_t i=0;i<count;i++) {
        int16_t token=(int16_t)fd2_text_token_at(tokens,i);
        if(token==-1)break;
        fd2_text_expanded part={0};int background=-1;
        if(token==-4) {
            CHECK(fd2_text_expand(&names,font.glyph_count,&field.runtime.text,1,&part)==0);
            background=74;
        } else if(token==-6)CHECK(fd2_text_expand_number(field.runtime.text.value,font.glyph_count,&part)==0);
        else {CHECK(token>=0);part.tokens[0]=(uint16_t)token;part.count=1;}
        for(size_t j=0;j<part.count;j++) {
            if(part.tokens[j]==FD2_TEXT_SAFE_MINUS) {
                /* 原132的6×5负号是RLE，独立资源解码形成期望像素。 */
                uint32_t start=rd_u32_le(ui.data+6+132*4),end=rd_u32_le(ui.data+6+133*4);
                fd2_image minus={0};CHECK(fd2_image_decode_buf(&minus,ui.data+start,end-start)==0);
                CHECK(minus.width==6 && minus.height==5);
                fd2_map_sprite_blit(&expected_vga,&minus,x,0,0);fd2_image_free(&minus);
            }
            else fd2_font_draw_glyph(&expected_vga,&font,part.tokens[j],x,0,205,76,background);
            x+=16;total_glyphs++;
        }
    }
    CHECK(play_text_fragment(&actual_vga,NULL,NULL,NULL,&ui,NULL,&field,&font,
        &names,NULL,439,0)==0);
    CHECK(!memcmp(actual_vga.framebuffer,expected_vga.framebuffer,sizeof(actual_vga.framebuffer)));
    CHECK(actual_vga.dialog_visual.glyph_counter==total_glyphs%2u);
    CHECK(actual_vga.dialog_visual.mouth_phase==(total_glyphs/2u)%4u);
    CHECK(rng_calls==0); /* 无本地开框的-1不发明末尾16c57。 */
    fd2_dialog_visual_state before=actual_vga.dialog_visual;
    /* 同一Scene实际-5分支借原UI0旧名称2，跨consumer不复位嘴型。 */
    const uint8_t old_tokens[]={2,0,0xfb,0xff,0xff,0xff};fd2_text_entry old={0};
    CHECK(fd2_text_entry_open_mem(&old,old_tokens,sizeof(old_tokens))==0);
    fd2_text_expanded old_name;
    CHECK(fd2_text_expand(&names,font.glyph_count,&field.runtime.text,2,&old_name)==0);
    CHECK(play_text_fragment(&actual_vga,NULL,NULL,NULL,&ui,NULL,&field,&font,
        &old,NULL,0,0)==0);
    size_t combined=total_glyphs+old_name.count;
    CHECK(actual_vga.dialog_visual.glyph_counter==combined%2u &&
        actual_vga.dialog_visual.mouth_phase==(combined/2u)%4u && !rng_calls);
    CHECK(before.glyph_counter==total_glyphs%2u);
    field.runtime.text.old_fragment=SIZE_MAX;
    unsigned calls_before=(unsigned)presents;
    before=actual_vga.dialog_visual;
    CHECK(play_text_fragment(&actual_vga,NULL,NULL,NULL,&ui,NULL,&field,&font,
        &old,NULL,0,0)==-1);
    CHECK(presents==(int)calls_before && !memcmp(&before,&actual_vga.dialog_visual,sizeof(before)) && !rng_calls);
    fd2_text_entry_close(&old);fd2_text_entry_close(&names);fd2_archive_close(&fdtxt);
    return 0;
}

/* 初始成员/模板两端同源；present-only藏显不改变Game默认战斗状态。 */
static int test_shared_initialization_definition(const fd2_archive *fdother) {
    fd2_field_game *resources = calloc(1, sizeof(*resources)); CHECK(resources);
    CHECK(fd2_field_game_open_resources(resources, fdother, 0) == 0);
    fd2_vga vga;
    reset_controls(&vga); scene_host_quit_requested = 0;
    fd2_scene_field_state state;
    CHECK(init_opening_field_state(&state, 0, &resources->map,
        &resources->metadata, &resources->placements) == 0);
    CHECK(state.units.count == 12 && presents == 0);
    static const uint8_t ids[] = {0, 9, 4, 30};
    for (size_t i = 0; i < state.units.count; i++) {
        const fd2_field_unit *unit = &state.units.items[i];
        const fd2_field_placement *position = &resources->placements.records[
            i < 4 ? resources->placements.count - 4 + i : i - 4];
        CHECK(unit->unit_id == (i < 4 ? ids[i] : resources->metadata.unit_templates[i - 4].bytes[1]));
        CHECK(unit->x == position->x && unit->y == position->y);
        CHECK(unit->direction == 0 && unit->frame_phase == 0);
        CHECK(fd2_field_unit_is_hidden(unit) == (i >= 4));
        CHECK(state.units.walk_frames[i] == 1);
        CHECK(state.units.source_template_indices[i] == (i < 4 ? FD2_FIELD_UNIT_NO_TEMPLATE : i - 4));
    }
    const fd2_static_flow *source = scene_flow_find("scene.init.000"); CHECK(source);
    fd2_static_flow_node *nodes = malloc(source->count * sizeof(*nodes)); CHECK(nodes);
    memcpy(nodes, source->nodes, source->count * sizeof(*nodes));
    fd2_static_flow changed = *source; changed.nodes = nodes;
    int first = fd2_static_flow_label(source, "init_player_01");
    int second = fd2_static_flow_label(source, "init_player_02");
    int templates = fd2_static_flow_label(source, "init_templates_04");
    int hidden = fd2_static_flow_label(source, "hide_opening_templates");
    CHECK(first >= 0 && second >= 0 && templates >= 0 && hidden >= 0);
    CHECK(nodes[first].views == (FD2_FLOW_LOGIC | FD2_FLOW_PRESENT));
    CHECK(nodes[templates].views == (FD2_FLOW_LOGIC | FD2_FLOW_PRESENT));
    CHECK(nodes[hidden].views == FD2_FLOW_PRESENT);
    nodes[first].args[1] = source->nodes[second].args[1];
    nodes[second].args[1] = source->nodes[first].args[1];
    nodes[templates].args[1] = 7;
    nodes[hidden].args[1] = 10;
    bound_flow_override = &changed;
    CHECK(init_opening_field_state(&state, 0, &resources->map,
        &resources->metadata, &resources->placements) == 0);
    bound_flow_override = NULL;
    CHECK(state.units.count == 11 && state.units.items[1].unit_id == 4 && state.units.items[2].unit_id == 9);
    for (size_t i = 4; i < state.units.count; i++) {
        CHECK(state.units.source_template_indices[i] == i - 4);
        CHECK(state.units.items[i].x == resources->placements.records[i - 4].x);
        CHECK(state.units.items[i].y == resources->placements.records[i - 4].y);
        CHECK(fd2_field_unit_is_hidden(&state.units.items[i]));
    }
    CHECK(presents == 0);
    free(nodes); fd2_field_game_close(resources); free(resources); return 0;
}

static int test_natural_stage0_handoff(const fd2_archive *fdother,
                                      int *final_present) {
    static const uint8_t expected[][4] = {
        /* unit_id, x, y, flags.hidden；全部方向应为 0。 */
        {0, 7, 14, 0}, {9, 10, 15, 0}, {4, 8, 16, 0}, {30, 11, 17, 0},
        {96, 1, 4, 0}, {96, 2, 2, 0}, {96, 4, 2, 0}, {96, 6, 1, 0},
        {96, 3, 18, 0}, {96, 4, 23, 1}, {96, 2, 18, 0}, {96, 5, 19, 0}
    };
    fd2_vga vga;
    fd2_field_handoff handoff;
    reset_controls(&vga);
    memset(&handoff, 0xa5, sizeof(handoff));
    CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                  &handoff) == FD2_SCENE_RESULT_OK);
    CHECK(handoff.valid && handoff.stage == 0);
    CHECK(handoff.units.count == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < handoff.units.count; i++) {
        const fd2_field_unit *unit = &handoff.units.items[i];
        CHECK(unit->unit_id == expected[i][0]);
        CHECK(unit->x == expected[i][1] && unit->y == expected[i][2]);
        CHECK(unit->direction == 0 && unit->frame_phase == 0);
        CHECK(fd2_field_unit_is_hidden(unit) == expected[i][3]);
        CHECK(handoff.units.walk_frames[i] == 1);
    }
    /* TEXT2 最后说话人是 raw11(5,19)；原版收尾还须移到 raw0。
     * 保留逐格边缘卷动历史，不能改成按 actor0 直接居中。 */
    CHECK(handoff.focus_cell_x == 7 && handoff.focus_cell_y == 14);
    CHECK(handoff.camera_cell_x == 0 && handoff.camera_cell_y == 13);
    CHECK(archive_opens == 4 && archive_closes == archive_opens);
    CHECK(presents > 3);
    *final_present = presents;
    return 0;
}

static int test_resource_error_has_no_handoff(const fd2_archive *fdother) {
    fd2_vga vga;
    fd2_field_handoff handoff;
    reset_controls(&vga);
    fail_archive_path = "original_game/FDTXT.DAT";
    memset(&handoff, 0xa5, sizeof(handoff));
    CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                  &handoff) == FD2_SCENE_RESULT_ERROR);
    CHECK(!handoff.valid && handoff.units.count == 0);
    CHECK(archive_opens == 2 && archive_closes == archive_opens);
    CHECK(presents == 0);
    /* 字体/UI 无资源时也不能交接；使用真实空 archive 的正式错误出口。 */
    fd2_archive empty = {0};
    reset_controls(&vga);
    memset(&handoff, 0xa5, sizeof(handoff));
    CHECK(scene_test_play_handoff(&vga, &empty, NULL, NULL, 1,
                                  &handoff) == FD2_SCENE_RESULT_ERROR);
    CHECK(!handoff.valid && handoff.units.count == 0);
    CHECK(archive_opens == 4 && archive_closes == archive_opens);
    return 0;
}

static int test_host_quit_has_no_handoff(const fd2_archive *fdother,
                                        int final_present) {
    fd2_vga vga;
    fd2_field_handoff handoff;
    const int frames[] = {1, final_present};
    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        reset_controls(&vga);
        quit_at_frame = frames[i];
        memset(&handoff, 0xa5, sizeof(handoff));
        CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                      &handoff) == FD2_SCENE_RESULT_HOST_QUIT);
        CHECK(!handoff.valid && handoff.units.count == 0);
        CHECK(archive_opens == 4 && archive_closes == archive_opens);
    }
    /* 最后 800 ms 预览也属于可退出流程，不能先导出 valid 再返回 quit。 */
    reset_controls(&vga);
    quit_on_preview_delay = 1;
    memset(&handoff, 0xa5, sizeof(handoff));
    unsigned rng_calls=0;
    fd2_scene_runtime runtime={scene_counted_random,&rng_calls,{0,0,0}};
    CHECK(scene_test_play_handoff_with_runtime(&vga,fdother,NULL,NULL,0,
                                  &handoff,&runtime) == FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!handoff.valid && handoff.units.count == 0);
    CHECK(archive_opens == 4 && archive_closes == archive_opens);
    CHECK(presents < 100000);
    return 0;
}

static int test_required_palette_error_has_no_handoff(const fd2_archive *fdother) {
    /* 只故障 FDOTHER[0]：字体/UI/FDICON 均真实有效，不能由更早的错误假通过。 */
    for (int failure = 1; failure <= 2; failure++) {
        fd2_vga vga;
        fd2_field_handoff handoff;
        reset_controls(&vga);
        fail_palette_archive = fdother;
        palette_failure = failure;
        memset(&handoff, 0xa5, sizeof(handoff));
        CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                      &handoff) == FD2_SCENE_RESULT_ERROR);
        CHECK(palette_queries == 1);
        CHECK(!handoff.valid && handoff.units.count == 0);
        CHECK(presents == 0);
        CHECK(archive_opens == 4 && archive_closes == archive_opens);
    }
    return 0;
}

static int test_real_portrait_orientation(void) {
    static const size_t identities[] = {0, 4, 9, 30, 48, 66, 96};
    static const int frames[] = {0, 3};
    fd2_archive dato = {0};
    CHECK(fd2_archive_open(&dato, "original_game/DATO.DAT") == 0);
    for (size_t id = 0; id < sizeof(identities) / sizeof(identities[0]); id++) {
        for (size_t phase = 0; phase < sizeof(frames) / sizeof(frames[0]); phase++) {
            fd2_image original = {0};
            CHECK(fd2_portrait_decode_frame(&original, &dato, identities[id],
                                            (size_t)frames[phase]) == 0);
            CHECK(original.width == 80 && original.height == 80);
            for (int area = FD2_DIALOG_TOP; area <= FD2_DIALOG_BOTTOM; area++) {
                fd2_vga vga;
                reset_controls(&vga);
                memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
                /* 正式scene目前固定请求frame0；替身喂真实0/3，验证共同方向路径，
                 * 不声称实现了native眨眼计时或RNG。 */
                portrait_frame_override = frames[phase];
                CHECK(draw_portrait(&vga, &dato, (fd2_dialog_area)area,
                                    (uint16_t)identities[id]) == 0);
                CHECK(portrait_frame_requested == 0 && presents == 0);
                int x = area == FD2_DIALOG_TOP ? 232 : 8;
                int y = area == FD2_DIALOG_TOP ? 5 : 115;
                size_t asymmetric = 0;
                for (int row = 0; row < VGA_H; row++) {
                    for (int column = 0; column < VGA_W; column++) {
                        uint8_t expected = 0xa5;
                        if (column >= x && column < x + 80 &&
                            row >= y && row < y + 80) {
                            int sx = column - x;
                            if (area == FD2_DIALOG_BOTTOM) sx = 79 - sx;
                            size_t index = (size_t)(row - y) * 80u + (size_t)sx;
                            expected = original.pixels[index];
                            if (expected != original.pixels[
                                    (size_t)(row - y) * 80u + (size_t)(79 - sx)])
                                asymmetric++;
                        }
                        CHECK(vga.framebuffer[(size_t)row * VGA_STRIDE + (size_t)column] == expected);
                    }
                }
                CHECK(asymmetric > 0);
            }
            fd2_image_free(&original);
        }
    }
    fd2_archive_close(&dato);
    return 0;
}

static int test_portrait_decode_error_has_no_handoff(const fd2_archive *fdother) {
    /* 真正运行开场，只有指定次立绘解码返回错误；其他资源全部真实有效。
     * 首次bottom ID0和第二次top ID48分别覆盖打开／切换说话人的整条传播链。 */
    static const size_t expected_ids[] = {0, 48};
    for (int failure = 1; failure <= 2; failure++) {
        fd2_vga vga;
        fd2_field_handoff handoff;
        reset_controls(&vga);
        fail_portrait_identity=expected_ids[failure-1];
        fail_portrait_frame0_at=2; /* 第1次是四帧预检，第2次才是真开框。 */
        memset(&handoff, 0xa5, sizeof(handoff));
        CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                      &handoff) == FD2_SCENE_RESULT_ERROR);
        CHECK(portrait_frame0_hits==2 && portrait_decode_calls==portrait_calls_at_failure);
        CHECK(failed_portrait_identity == expected_ids[failure - 1]);
        CHECK(portrait_frame_requested == 0);
        CHECK(!scene_host_quit_requested);
        CHECK(!handoff.valid && handoff.units.count == 0);
        CHECK(presents_at_portrait_failure > 0 && presents == presents_at_portrait_failure);
        CHECK(archive_opens == 4 && archive_closes == archive_opens);
    }
    return 0;
}

/* 从真实冷图截取入场前缀；fast帧合同和正常17帧不由C隐藏编排补齐。 */
static int test_scene_entry_prefix(const fd2_field_game *game) {
    const char *ids[]={"stage.entry.001","scene.opening.032"};
    for (size_t kind=0;kind<2;kind++) {
        const fd2_static_flow *source=scene_flow_find(ids[kind]);CHECK(source);
        fd2_static_flow_node *nodes=malloc(source->count*sizeof(*nodes));CHECK(nodes);
        memcpy(nodes,source->nodes,source->count*sizeof(*nodes));
        fd2_static_flow flow=*source;flow.nodes=nodes;
        int entry=fd2_static_flow_label(&flow,kind ? "entry_render" : "entry_dark");CHECK(entry>=0);
        flow.entry=(uint8_t)entry;
        int fade=fd2_static_flow_label(&flow,"entry_fade");
        int end=-1;
        for (size_t i=0;i<flow.count;i++) if (!strcmp(nodes[i].op,"end")) end=(int)i;
        CHECK(fade>=0 && end>=0);
        nodes[fade].edges[FD2_FLOW_NEXT]=(int16_t)end;
        fd2_vga vga;fd2_scene_field_state state={0};state.units=game->units;
        scene_flow_context context={0};context.vga=&vga;context.game=game;
        context.terrain=&game->terrain;context.map=&game->map;
        context.sprites=&game->sprites;context.state=&state;
        for (int fast=0;fast<=1;fast++) {
            reset_controls(&vga);scene_host_quit_requested=0;context.fast=fast;
            CHECK(scene_flow_run(&context,&flow)==0 && current_darkness==0);
            CHECK(presents==(fast ? 1 : kind ? 17 : 18));
            if (fast) CHECK(presented_darkness[0]==(kind ? 0 : 64));
            else for (int i=0;i<17;i++) CHECK(presented_darkness[i+(kind ? 0 : 1)]==64-4*i);
        }
        if (!kind) {
            int present=fd2_static_flow_label(&flow,"entry_present");CHECK(present>=0);
            nodes[flow.entry].edges[FD2_FLOW_NEXT]=(int16_t)fade;
            nodes[fade].edges[FD2_FLOW_NEXT]=(int16_t)present;
            nodes[present].edges[FD2_FLOW_NEXT]=(int16_t)end;
            reset_controls(&vga);scene_host_quit_requested=0;context.fast=1;
            CHECK(scene_flow_run(&context,&flow)==0 && presents==1 && presented_darkness[0]==0);
        }
        free(nodes);
    }
    return 0;
}

static int test_stage1_presentation(const fd2_archive *fdother) {
    fd2_save_manual_state prepared={0};
    prepared.stage_id=1;prepared.unit_count=5;prepared.gold_or_flags=1234;
    const uint8_t ids[]={0,9,4,30,1};
    for(size_t i=0;i<5;i++) {
        fd2_field_unit actor={0};
        CHECK(fd2_field_unit_persistent_create(&actor,ids[i])==0);
        memcpy(prepared.unit_area+i*80,&actor,80);
    }
    fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
    CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
    CHECK(test_scene_entry_prefix(game)==0);
    fd2_field_units expected=game->units;
    fd2_vga vga;reset_controls(&vga);
    CHECK(scene_test_stage1_entry(&vga,fdother,game,NULL,1)==0);
    CHECK(game->units.count==21 && game->turn_number==1 && game->active_side==2);
    CHECK(game->cursor_cell_x==20 && game->cursor_cell_y==14);
    /* B5 C9原版25页后首leader WGC(268,28)，合法Save反推camera9,13；
     * 不沿用业务prepare的近似镜头或最后一次PAN6,12。 */
    CHECK(game->camera_cell_x==9 && game->camera_cell_y==13);
    for(size_t i=0;i<21;i++)CHECK(!memcmp(&game->units.items[i],&expected.items[i],80));
    CHECK(!memcmp(game->roster,prepared.unit_area,sizeof(game->roster)));
    CHECK(archive_opens==archive_closes);
    CHECK(scene_test_story_validate(game,3)==0);
    CHECK(scene_test_story_validate(game,65535)==-1);
    fd2_field_game_close(game);
    prepared.unit_count=4;
    CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
    expected=game->units;
    reset_controls(&vga);
    CHECK(scene_test_stage1_entry(&vga,fdother,game,NULL,1)==0);
    uint8_t absent[80]={0};absent[0]=254;absent[5]=1;
    CHECK(!memcmp(&game->units.items[4],absent,80));
    for(size_t i=0;i<21;i++)CHECK(!memcmp(&game->units.items[i],&expected.items[i],80));
    /* 中途退出不交还半套脚本坐标；属性与持久owner也不被呈现覆盖。 */
    reset_controls(&vga);quit_at_frame=3;
    fd2_field_units before=game->units;
    CHECK(scene_test_stage1_entry(&vga,fdother,game,NULL,0)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!memcmp(&before,&game->units,sizeof(before)) && archive_opens==archive_closes);
    fd2_field_game_close(game);free(game);
    return 0;
}

static int make_stage1_victory(fd2_field_game *game, fd2_vga *vga,
        const fd2_archive *fdother, int missing_villager, int full) {
    fd2_save_manual_state prepared={0};prepared.stage_id=1;prepared.unit_count=5;
    const uint8_t ids[]={0,9,4,30,1};
    for(size_t i=0;i<5;i++) {
        fd2_field_unit unit={0};CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
        memcpy(prepared.unit_area+i*80,&unit,80);
    }
    CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
    game->turn_number=3;CHECK(fd2_field_game_end_active_phase(game)==0);
    CHECK(game->event_log_count==1 && game->units.count==27);
    reset_controls(vga);
    CHECK(scene_test_play_event(vga,fdother,game,&game->event_log[0],NULL,1)==0);
    CHECK(fd2_field_game_progress_resume(game)==0);
    for(size_t i=0;i<game->units.count;i++) {
        if(game->units.items[i].side==0) {game->units.items[i].flags|=1;game->units.items[i].hp=0;}
        if(full && game->units.items[i].side==2) memset((uint8_t*)&game->units.items[i]+10,0,16);
    }
    if(missing_villager)game->units.items[7].flags|=1;
    CHECK(fd2_field_game_battle_result(game)==FD2_FIELD_BATTLE_VICTORY);
    return 0;
}

static int test_stage1_victory_projection(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=calloc(1,sizeof(*before));CHECK(game&&before);
    fd2_vga vga;
    for(int variant=0;variant<3;variant++)for(unsigned display=0;display<=1;display++) {
        CHECK(make_stage1_victory(game,&vga,fdother,variant==1,variant==2)==0);
        game->ordinary_display_mode=(uint8_t)display;*before=*game;
        reset_controls(&vga);trace_fragments=1;
        tracked_cursor=fd2_field_cursor_frame(&game->visuals,0);
        CHECK(scene_test_stage1_victory(&vga,fdother,game,NULL,1)==0);
        CHECK(!memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
        const size_t first=variant==1?7u:6u;
        const size_t expected[]={first,8,9,10,first,8,9,10};
        CHECK(fragment_log_count==8 && !memcmp(fragment_log,expected,sizeof(expected)));
        /* 22f37直接15f84；后者不发明首11cac。165ac先清51A83再12cea
         * 首11cac，最后写1但框只缓存恢复，因此四个fragment均不新画cursor。
         * 另由test_stage1_dialog_display显式caller页验证display0/1的实际写点。 */
        CHECK(!cursor_by_fragment[first]);
        CHECK(!cursor_by_fragment[8]);
        CHECK(!cursor_by_fragment[9] && !cursor_by_fragment[10]);
        int final_present=presents;
        if(variant==0 && display==1) {
            const int quit_frames[]={1,final_present};
            for(size_t i=0;i<2;i++) {
                reset_controls(&vga);quit_at_frame=quit_frames[i];
                CHECK(scene_test_stage1_victory(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_HOST_QUIT);
                CHECK(!memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
            }
            reset_controls(&vga);fail_portrait_decode_at=1;
            CHECK(scene_test_stage1_victory(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_ERROR);
            CHECK(!presents && !memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
        }
        fd2_field_game_close(game);
    }
    free(before);free(game);return 0;
}

/* 165ac/135dd局部显示状态与弹框缓存分开验证；不改真实Game或替换focus逻辑。 */
static int test_stage1_dialog_display(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=calloc(1,sizeof(*before));
    CHECK(game && before);
    fd2_vga vga;CHECK(make_stage1_victory(game,&vga,fdother,0,0)==0);
    *before=*game;
    fd2_archive fdtxt={0};fd2_text_entry text={0};fd2_ui_sheet ui={0};
    CHECK(fd2_archive_open(&fdtxt,"original_game/FDTXT.DAT")==0);
    CHECK(fd2_text_entry_open_entry(&text,&fdtxt,2)==0);
    CHECK(ui_sheet_open_fdother(&ui,fdother,5)==0);
    const fd2_image *normal=fd2_field_cursor_frame(&game->visuals,0);CHECK(normal);
    for(unsigned hidden=0;hidden<=1;hidden++)for(unsigned display=0;display<=1;display++) {
        reset_controls(&vga);scene_host_quit_requested=0;
        tracked_cursor=normal;trace_fragments=1;
        fd2_scene_field_state state={0};
        state.units=game->units;state.persistent=game->roster;state.persistent_count=game->roster_count;
        state.camera_cell_x=game->camera_cell_x;state.camera_cell_y=game->camera_cell_y;
        state.focus_cell_x=game->cursor_cell_x;state.focus_cell_y=game->cursor_cell_y;
        state.ordinary_cursor=normal;state.cursor=display?normal:NULL;
        fd2_field_unit_set_hidden(&state.units.items[6],hidden);
        /* 显式caller先有11cac页，与15f84自己偷偷重画一页区分。 */
        last_fragment=6;
        render_scene_11cac(&vga,&game->terrain,&game->map,&game->sprites,&state,1);
        CHECK(!scene_render_error && cursor_by_fragment[6]==display);
        /* -19,6是直接raw，不因hidden跳过165ac；fast只略宿主等待。 */
        CHECK(opening_text(&vga,&game->terrain,&game->map,&game->dato_archive,&ui,
            &game->sprites,&state,&game->font,&text,NULL,6,1)==0);
        CHECK(state.cursor==normal && !state.dialog_cursor_suppressed);
        CHECK(cursor_by_fragment[6]==display);
        CHECK(state.focus_cell_x==state.units.items[6].x &&
              state.focus_cell_y==state.units.items[6].y);
        unsigned drawn=cursor_by_fragment[6];
        pan_camera_to(&vga,&game->terrain,&game->map,&game->sprites,&state,14,2,1);
        CHECK(!state.cursor && !state.dialog_cursor_suppressed && cursor_by_fragment[6]==drawn);
        /* hidden textID不会聚焦，a3=0保留当前mode，不把普通cursor自动打开。 */
        fd2_dialog_state dialog={0};dialog.speaker_actor_idx=-1;
        CHECK(set_dialog_area(&vga,&game->terrain,&game->map,&game->dato_archive,&ui,
            &game->sprites,&state,&dialog,FD2_DIALOG_TOP,game->units.items[6].unit_id,-1,1,NULL)==0);
        CHECK(!state.cursor && !state.dialog_cursor_suppressed);
        /* 旧开场没有ordinary_cursor借用，零初始化仍始终无选择层。 */
        state.ordinary_cursor=NULL;
        CHECK(opening_text(&vga,&game->terrain,&game->map,&game->dato_archive,&ui,
            &game->sprites,&state,&game->font,&text,NULL,6,1)==0);
        CHECK(!state.cursor && !state.dialog_cursor_suppressed && cursor_by_fragment[6]==drawn);
        CHECK(!memcmp(game,before,sizeof(*game)));
    }
    fd2_text_entry_close(&text);fd2_archive_close(&fdtxt);
    fd2_field_game_close(game);free(before);free(game);return 0;
}

static int test_stage2_presentation(const fd2_archive *fdother) {
    fd2_save_manual_state prepared={0};prepared.stage_id=2;prepared.unit_count=6;
    const uint8_t ids[]={0,9,4,30,1,8};
    for(size_t i=0;i<6;i++) {
        fd2_field_unit unit={0};CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
        memcpy(prepared.unit_area+i*80,&unit,80);
    }
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=calloc(1,sizeof(*before));CHECK(game&&before);
    CHECK(fd2_field_game_prepare_stage2(game,fdother,&prepared)==0);*before=*game;
    fd2_vga vga;reset_controls(&vga);trace_fragments=1;
    CHECK(scene_test_stage2_entry(&vga,fdother,game,NULL,1)==0);
    const size_t expected[]={0,1,2,3,0,1,2,3};
    CHECK(fragment_log_count==8 && !memcmp(fragment_log,expected,sizeof(expected)));
    CHECK(game->units.count==15 && game->stage==2 && game->turn_number==1 && game->active_side==2);
    /* 32e8c fragment3 text8先聚焦(7,24)，12cea滚至map高26-8=18；
     * 最后12d7b Hero(7,22)保rel(4,4)。镜头终态属于完整Scene的handoff，
     * 不是只构造单位表的prepare_stage2或最后PAN(3,17)。 */
    CHECK(game->cursor_cell_x==7 && game->cursor_cell_y==22 &&
          game->camera_cell_x==3 && game->camera_cell_y==18);
    CHECK(!memcmp(game->units.items,before->units.items,15*80) &&
          !memcmp(game->roster,before->roster,sizeof(game->roster)) && archive_opens==archive_closes);
    for(size_t i=0;i<15;i++)CHECK(game->units.items[i].direction==0);
    *before=*game;reset_controls(&vga);quit_at_frame=1;
    CHECK(scene_test_stage2_entry(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
    reset_controls(&vga);fail_portrait_decode_at=1;
    CHECK(scene_test_stage2_entry(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_ERROR);
    CHECK(!presents && !memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
    fd2_field_game_close(game);free(before);free(game);return 0;
}

static int test_turn_event_presentation(const fd2_archive *fdother) {
    /* 34778的fragment4实际有9个等待页；fast仅替换宿主等待，不跳资源/脚本。 */
    fd2_archive fdtxt={0};fd2_text_entry text={0};
    const uint8_t *tokens;size_t token_count;
    CHECK(fd2_archive_open(&fdtxt,"original_game/FDTXT.DAT")==0);
    CHECK(fd2_text_entry_open_entry(&text,&fdtxt,2)==0);
    CHECK(fd2_text_entry_get_fragment(&text,4,&tokens,&token_count)==0);
    CHECK(fd2_dialog_flow_count_waits(tokens,token_count)==9);
    fd2_text_entry_close(&text);fd2_archive_close(&fdtxt);
    for (size_t stage=0;stage<=1;stage++) {
        fd2_field_game *game=calloc(1,sizeof(*game));
        fd2_field_game *before=malloc(sizeof(*before));
        CHECK(game && before);
        fd2_vga vga;reset_controls(&vga);
        if (stage==0) {
            CHECK(fd2_field_game_open(game,&vga,fdother,0)==0);
        } else {
            fd2_save_manual_state prepared={0};
            prepared.stage_id=1;prepared.unit_count=5;
            const uint8_t ids[]={0,9,4,30,1};
            for (size_t i=0;i<5;i++) {
                fd2_field_unit unit={0};
                CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
                memcpy(prepared.unit_area+i*80,&unit,80);
            }
            CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
        }
        game->turn_number=3;
        CHECK(fd2_field_game_end_active_phase(game)==0);
        CHECK(game->event_log_count==1);
        fd2_field_event_notice *notice=&game->event_log[0];
        CHECK(notice->action==(stage ? 6 : 0) && notice->handled &&
              notice->presentation_deferred);
        CHECK(notice->presentation_unit_count==(stage ? 21 : 12));
        CHECK(game->units.count==(stage ? 27 : 14));
        if (!stage) {
            /* strictLE原表：unit1/LV1与unit3/LV3，装备32/AP20 AC95、128/DP2。 */
            CHECK(game->units.items[12].attack==27 && game->units.items[12].defense==6 &&
                  game->units.items[12].accuracy==96 && game->units.items[12].evasion==1);
            CHECK(game->units.items[13].attack==50 && game->units.items[13].defense==17 &&
                  game->units.items[13].accuracy==106 && game->units.items[13].evasion==11);
        }
        if (stage) {
            /* G3 constructor已经统一重算原31/5/98/3；不能再假定模板缺4WORD。
             * 先核真实重放一致，再注入明确的业务后态夹具，锁Scene只借属性。 */
            fd2_field_units replay=game->units;
            replay.count=notice->presentation_unit_count;
            CHECK(fd2_field_units_append_group_placed(&replay,&game->metadata,
                &game->placements,&game->map,3,1)==0);
            const uint8_t *script;size_t script_size;
            CHECK(fd2_field_movement_script_get(13,&script,&script_size)==0);
            CHECK(fd2_field_movement_script_apply(&replay,script,script_size)==0);
            CHECK(!memcmp(replay.items,game->units.items,game->units.count*sizeof(replay.items[0])));
            for(size_t i=21;i<27;i++) {
                CHECK(game->units.items[i].attack==31 && game->units.items[i].defense==5 &&
                    game->units.items[i].accuracy==98 && game->units.items[i].evasion==3);
                game->units.items[i].attack=32;game->units.items[i].defense=6;
                game->units.items[i].accuracy=99;game->units.items[i].evasion=4;
            }
            size_t differences=0;
            for(size_t i=0;i<game->units.count;i++)for(size_t b=0;b<80;b++) {
                if (((uint8_t *)&replay.items[i])[b]==
                    ((uint8_t *)&game->units.items[i])[b]) continue;
                CHECK(i>=21 && (b==0x48 || b==0x4a || b==0x4c || b==0x4e));
                differences++;
            }
            CHECK(differences==24);
        }
        *before=*game;reset_controls(&vga);
        CHECK(scene_test_play_event(&vga,fdother,game,notice,NULL,1)==FD2_SCENE_RESULT_OK);
        CHECK(!memcmp(&game->units,&before->units,sizeof(game->units)));
        CHECK(!memcmp(game->roster,before->roster,sizeof(game->roster)));
        CHECK(!notice->presentation_deferred && game->turn_number==3 && game->active_side==1);
        CHECK(game->experience_value==before->experience_value &&
              game->message_number==before->message_number);
        CHECK(archive_opens==2 && archive_closes==2 && presents>0);
        int final_present=presents;
        if (stage) {
            const fd2_static_flow *shared=fd2_static_flow_lookup("scene","event.action.006");
            const fd2_static_flow *bound=fd2_static_flow_lookup("scene","stage.001.action.006");
            CHECK(shared && bound && shared->nodes==bound->nodes && shared->count==bound->count &&
                  shared->entry==bound->entry);
            /* 仅本私有TU临时绑定另一action；用真实资源/notice重放同一节点数组。 */
            fd2_static_flow rebound=*shared;rebound.id="stage.001.action.042";
            *game=*before;game->event_log[0].action=42;reset_controls(&vga);
            bound_flow_override=&rebound;
            int rebound_result=scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1);
            bound_flow_override=NULL;
            CHECK(rebound_result==FD2_SCENE_RESULT_OK && !game->event_log[0].presentation_deferred);
            CHECK(!memcmp(&game->units,&before->units,sizeof(game->units)) &&
                  !memcmp(game->roster,before->roster,sizeof(game->roster)) &&
                  !fd2_field_stage_battle_is_supported(2));
        }
        if (!stage) {
            /* 同ID的持久角色以Game来源为准，不能强套呈现模板idx9。 */
            *game=*before;game->units.source_template_indices[12]=FD2_FIELD_UNIT_NO_TEMPLATE;
            fd2_field_units expected=game->units;reset_controls(&vga);
            CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==0);
            CHECK(!memcmp(&game->units,&expected,sizeof(expected)));
        }
        for (int failure=0;failure<2;failure++) {
            *game=*before;
            size_t first=game->event_log[0].presentation_unit_count;
            if (failure==0) game->units.items[first].unit_id^=1;
            else game->units.source_template_indices[first]++;
            fd2_scene_field_state state={0};state.units=game->units;
            state.units.count=first;
            fd2_field_units original=state.units;
            reset_controls(&vga);
            CHECK(field_event_append_group(&vga,fdother,game,&state,3,1,0,NULL,1,NULL)==-1);
            CHECK(!memcmp(&state.units,&original,sizeof(original)) && presents==0);
        }
        /* 借用静态属性不等于接受任意终态；脚本坐标不符仍须整80B拒绝。 */
        *game=*before;
        game->units.items[game->event_log[0].presentation_unit_count].x++;
        fd2_field_game *wrong_end=malloc(sizeof(*wrong_end));CHECK(wrong_end);
        *wrong_end=*game;reset_controls(&vga);
        CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==FD2_SCENE_RESULT_ERROR);
        CHECK(!memcmp(game,wrong_end,sizeof(*game)) && archive_opens==archive_closes);
        free(wrong_end);
        const int quit_frames[]={1,final_present};
        for (size_t i=0;i<sizeof(quit_frames)/sizeof(quit_frames[0]);i++) {
            *game=*before;reset_controls(&vga);quit_at_frame=quit_frames[i];
            CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==
                  FD2_SCENE_RESULT_HOST_QUIT);
            CHECK(!memcmp(game,before,sizeof(*game)));
            CHECK(archive_opens==2 && archive_closes==2);
        }
        *game=*before;reset_controls(&vga);fail_archive_path="original_game/DATO.DAT";
        CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==FD2_SCENE_RESULT_ERROR);
        CHECK(!memcmp(game,before,sizeof(*game)) && presents==0);
        CHECK(archive_opens==1 && archive_closes==1);
        fd2_field_game_close(game);free(before);free(game);
    }
    return 0;
}

static int test_dialog_identity_lookup(void) {
    fd2_scene_field_state state={0};
    state.units.count=3;
    state.units.items[0].text_id=1;state.units.items[0].unit_id=1;state.units.items[0].flags=1;
    state.units.items[1].text_id=1;state.units.items[1].unit_id=51;
    state.units.items[2].text_id=1;state.units.items[2].unit_id=50;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==51);
    CHECK(resolve_speaker_actor(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==1);
    state.units.items[1].flags=state.units.items[2].flags=1;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==50);
    CHECK(resolve_speaker_actor(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==-1);
    fd2_field_unit persistent[2]={{0}};
    persistent[0].text_id=persistent[1].text_id=1;
    persistent[0].unit_id=1;persistent[1].unit_id=51;
    state.persistent=persistent;state.persistent_count=2;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==50); /* active hidden仍先于roster */
    state.units.count=0;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==51);
    CHECK(resolve_speaker_actor(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==-1);
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,39)==39);
    CHECK(persistent[1].unit_id==51 && state.persistent_count==2);
    return 0;
}

static int test_game_over_frames(const fd2_archive *fdother) {
    fd2_vga vga;reset_controls(&vga);
    memset(vga.framebuffer,0xa5,sizeof(vga.framebuffer));
    const uint8_t *data;size_t size;
    CHECK(fd2_archive_get(fdother,79,&data,&size)==0 && size>=16);
    uint8_t expected[VGA_W*VGA_H]={0};
    for(size_t i=0;i<2;i++) {
        uint32_t start=rd_u32_le(data+8+4*i);
        uint32_t end=i? (uint32_t)size:rd_u32_le(data+12);
        fd2_image image={0};
        CHECK(fd2_image_decode_buf(&image,data+start+9,end-start-9)==0);
        int x=rd_u16_le(data+start),y=rd_u16_le(data+start+2);
        CHECK(x==(i?130:69) && y==(i?141:61));
        size_t opaque=0;
        for(int row=0;row<image.height;row++)for(int col=0;col<image.width;col++) {
            uint8_t pixel=image.pixels[row*image.width+col];
            if(pixel)opaque++;
            expected[(y+row)*VGA_STRIDE+x+col]=pixel;
        }
        CHECK(opaque==(i?263u:9848u));
        fd2_image_free(&image);
    }
    uint64_t start=test_ms;
    CHECK(scene_test_game_over(&vga,fdother,NULL,0)==0);
    CHECK(!memcmp(vga.framebuffer,expected,sizeof(expected)));
    CHECK(test_ms-start>=46u*55u); /* 普通Enter脉冲不能略过固定停顿。 */
    reset_controls(&vga);quit_at_frame=1;
    memset(vga.framebuffer,0xa5,sizeof(vga.framebuffer));
    CHECK(scene_test_game_over(&vga,fdother,NULL,0)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(vga.framebuffer[0]==0xa5); /* clear0在tick1/fade之后。 */
    return 0;
}

static int test_scene_existing_actor_motion(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
    fd2_vga vga;reset_controls(&vga);
    CHECK(fd2_field_game_open(game,&vga,fdother,0)==0);
    fd2_field_event_notice notice={0};notice.presentation_unit_count=(uint8_t)game->units.count;
    notice.presentation_camera_x=(int16_t)game->camera_cell_x;notice.presentation_camera_y=(int16_t)game->camera_cell_y;
    notice.presentation_focus_x=(int16_t)game->cursor_cell_x;notice.presentation_focus_y=(int16_t)game->cursor_cell_y;
    for (size_t i=0;i<game->units.count;i++) {
        const fd2_field_unit *u=&game->units.items[i];uint8_t *m=notice.presentation_motion[i];
        m[0]=u->x;m[1]=u->y;m[2]=u->direction;m[3]=u->frame_phase;m[4]=game->units.walk_frames[i];
    }
    const uint8_t *script;size_t size;
    CHECK(fd2_field_movement_script_get(0,&script,&size)==0);
    CHECK(fd2_field_movement_script_apply(&game->units,script,size)==0);
    game->units.items[0].side=1;game->units.items[0].ai_param_35=37; /* logic-only后态必须保留。 */
    fd2_static_flow_node nodes[]={
        {"move_existing","move",{0,0},{1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_LOGIC|FD2_FLOW_PRESENT},
        {"done","end",{0},{-1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_LOGIC|FD2_FLOW_PRESENT}
    };
    fd2_static_flow flow={"test.scene.existing_actor",nodes,2,0};
    fd2_scene_field_state state={0};scene_flow_context context={0};
    context.vga=&vga;context.terrain=&game->terrain;context.map=&game->map;context.sprites=&game->sprites;
    context.state=&state;context.game=game;context.fast=1;scene_host_quit_requested=0;
    CHECK(scene_event_state_init(&state,game,&notice)==0 && scene_flow_run(&context,&flow)==0);
    CHECK(state.units.items[0].y!=game->units.items[0].y); /* 未恢复前态的旧路径会双移。 */
    notice.presentation_motion_valid=1;reset_controls(&vga);scene_host_quit_requested=0;
    CHECK(scene_event_state_init(&state,game,&notice)==0 && scene_flow_run(&context,&flow)==0);
    CHECK(!memcmp(state.units.items,game->units.items,game->units.count*sizeof(game->units.items[0])));
    CHECK(!memcmp(state.units.walk_frames,game->units.walk_frames,game->units.count));
    CHECK(state.units.items[0].side==1 && state.units.items[0].ai_param_35==37);
    fd2_field_game_close(game);free(game);return 0;
}

static int test_common_step_live_motion(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
    fd2_vga vga;reset_controls(&vga);
    CHECK(fd2_field_game_open(game,&vga,fdother,0)==0);
    game->camera_cell_x=game->camera_cell_y=0;
    game->cursor_cell_x=game->cursor_cell_y=3;
    game->ordinary_display_mode=1;
    uint16_t hp=game->units.items[0].hp;
    fd2_static_flow_node pan={"pan","pan",{5,5},{0},FD2_FLOW_PRESENT};
    quit_at_frame=presents+1;
    CHECK(scene_test_common_step(&vga,fdother,game,&pan,NULL,0)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(game->camera_cell_x==1 && game->camera_cell_y==0);
    CHECK(game->cursor_cell_x==4 && game->cursor_cell_y==3);
    CHECK(game->ordinary_display_mode==0 && game->units.items[0].hp==hp);
    reset_controls(&vga);
    CHECK(scene_test_common_step(&vga,fdother,game,&pan,NULL,1)==0);
    CHECK(game->camera_cell_x==5 && game->camera_cell_y==5);
    CHECK(game->cursor_cell_x==8 && game->cursor_cell_y==8);
    CHECK(game->units.items[0].hp==hp);
    fd2_static_flow_node refresh={"refresh","refresh_map",{1},{0},FD2_FLOW_PRESENT};
    game->ordinary_display_mode=0;
    /* 原11cbc..11cc8：1跳过4e31c，0调用；时钟门已到，不能靠未到门掩反向。 */
    vga.field_water_phase=5;vga.field_water_last_tick=0;
    uint8_t before_dac[sizeof(vga.dac)];memcpy(before_dac,vga.dac,sizeof(before_dac));
    CHECK(scene_test_common_step(&vga,fdother,game,&refresh,NULL,1)==0);
    CHECK(game->ordinary_display_mode==0 && vga.field_water_phase==5 && !vga.field_water_last_tick);
    CHECK(!memcmp(before_dac,vga.dac,sizeof(before_dac)));
    refresh.args[0]=0;
    CHECK(scene_test_common_step(&vga,fdother,game,&refresh,NULL,1)==0);
    CHECK(game->ordinary_display_mode==0 && vga.field_water_phase==6 && vga.field_water_last_tick>0);
    refresh.args[0]=2;CHECK(scene_test_common_validate(game,fdother,&refresh)==-1);
    uint32_t *old_cells=game->map.cells;fd2_image *old_frames=game->terrain.frames;
    fd2_static_flow_node map={"map","map_replace",{99,23},{0},FD2_FLOW_PRESENT};
    CHECK(scene_test_common_step(&vga,fdother,game,&map,NULL,1)==-1);
    CHECK(game->map.cells==old_cells && game->terrain.frames==old_frames);
    fd2_static_flow_node bad={"bad","unknown",{0},{0},FD2_FLOW_PRESENT};
    int old_presents=presents;
    CHECK(scene_test_common_step(&vga,fdother,game,&bad,NULL,1)==-1 && presents==old_presents);
    CHECK(game->units.items[0].hp==hp);
    /* 35a0d的真实FDOTHER45是59part目录；最终8×2是opaque写，非LMI1。 */
    reset_controls(&vga);memset(vga.framebuffer,211,sizeof(vga.framebuffer));
    fd2_static_flow_node raw={"raw","raw_animation",{45,0,59,2,36,151,-1},{0},FD2_FLOW_PRESENT};
    fd2_field_game *before=malloc(sizeof(*before));CHECK(before);*before=*game;
    fd2_field_visual_state visual_before=vga.field_visual;
    CHECK(scene_test_common_validate(game,fdother,&raw)==0 && !presents);
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(&vga.field_visual,&visual_before,sizeof(visual_before)));
    CHECK(scene_test_common_step(&vga,fdother,game,&raw,NULL,0)==0 && presents==59);
    static const uint8_t final[16]={204,74,43,94,94,43,74,204,202,77,45,44,44,45,77,202};
    for(int y=0;y<200;y++)for(int x=0;x<320;x++) {
        uint8_t expected=x>=36 && x<44 && y>=151 && y<153?
            final[(y-151)*8+x-36]:211;
        CHECK(vga.framebuffer[y*320+x]==expected);
    }
    CHECK(!memcmp(game,before,sizeof(*game)));
    free(before);
    fd2_field_game_close(game);free(game);return 0;
}

/* 改冷节点边会改变路径；业务条件只读入口快照，logic视图不二次呈现。 */
static int test_scene_flow_graph(void) {
    fd2_static_flow_node nodes[] = {
        {"hide","actor_hidden",{0,0,1},{1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT},
        {"branch","branch_any_hidden",{0,0},{-1,2,3,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT},
        {"three","ensure_count",{3},{4,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT},
        {"two","ensure_count",{2},{4,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT},
        {"done","end",{0},{-1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT}
    };
    fd2_static_flow flow={"test.scene.branch",nodes,5,0};
    fd2_scene_field_state state={0}; state.units.count=1;
    scene_flow_context context={0}; context.state=&state; context.fast=1;
    scene_host_quit_requested=0;
    CHECK(scene_flow_run(&context,&flow)==0 && state.units.count==2);
    CHECK(fd2_field_unit_is_hidden(&state.units.items[0]));
    memset(&state,0,sizeof(state));state.units.count=1;
    nodes[1].edges[FD2_FLOW_NO]=2;
    CHECK(scene_flow_run(&context,&flow)==0 && state.units.count==3);
    memset(&state,0,sizeof(state));state.units.count=1;
    nodes[0].views=FD2_FLOW_LOGIC;
    CHECK(scene_flow_run(&context,&flow)==0 && state.units.count==3);
    CHECK(!fd2_field_unit_is_hidden(&state.units.items[0]));
    nodes[0].views=FD2_FLOW_PRESENT;nodes[0].op="unknown";
    fd2_scene_field_state before=state;
    CHECK(scene_flow_run(&context,&flow)==-1 && !memcmp(&before,&state,sizeof(state)));
    nodes[0].op="ensure_count";nodes[0].args[0]=1;nodes[0].edges[FD2_FLOW_NEXT]=0;
    CHECK(scene_flow_run(&context,&flow)==-1); /* 预算耗尽不能被当作正常结束。 */
    /* hidden快照不再以uint64位移，原raw65..95不会与0..31混叠。 */
    nodes[0].op="branch_any_hidden";nodes[0].args[0]=65;nodes[0].args[1]=95;
    nodes[0].edges[FD2_FLOW_YES]=2;nodes[0].edges[FD2_FLOW_NO]=3;
    nodes[2].op=nodes[3].op="limit_count";
    flow.entry=0;state.units.count=96;
    fd2_field_unit_set_hidden(&state.units.items[65],1);
    scene_host_quit_requested=0;context.flow_steps=0;
    CHECK(scene_flow_run(&context,&flow)==0 && state.units.count==3);
    state.units.count=96;fd2_field_unit_set_hidden(&state.units.items[65],0);
    fd2_field_unit_set_hidden(&state.units.items[1],1);
    context.flow_steps=0;
    CHECK(scene_flow_run(&context,&flow)==0 && state.units.count==2);
    return 0;
}

static int test_scene_flow_view_commit(void) {
    fd2_field_game *owner=calloc(1,sizeof(*owner)); CHECK(owner);
    owner->map.width=owner->map.height=10;owner->units.count=1;
    owner->camera_pixel_offset_x=4;owner->camera_pixel_offset_y=5;
    fd2_scene_field_state state={0};state.units=owner->units;
    state.camera_cell_x=2;state.camera_cell_y=3;
    state.focus_cell_x=11;state.focus_cell_y=-2;
    fd2_vga vga={0};vga.field_visual.actor_idle_phase=3;
    fd2_static_flow_node nodes[]={
        {"commit","commit_view",{0},{1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT},
        {"done","end",{0},{-1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT}
    };
    fd2_static_flow flow={"test.scene.commit",nodes,2,0};
    scene_flow_context context={0};context.state=&state;context.game=owner;context.mutable_game=owner;context.vga=&vga;
    scene_host_quit_requested=0;
    CHECK(scene_flow_run(&context,&flow)==0);
    CHECK(owner->camera_cell_x==2 && owner->camera_cell_y==3 && owner->cursor_cell_x==9 && owner->cursor_cell_y==0);
    CHECK(owner->camera_pixel_offset_x==0 && owner->camera_pixel_offset_y==0 && owner->idle_phase==3);
    CHECK(!memcmp(owner->units.items,state.units.items,sizeof(owner->units.items)));
    context.mutable_game=NULL;state.camera_cell_x=4;
    CHECK(scene_flow_run(&context,&flow)==-1 && owner->camera_cell_x==2);
    context.mutable_game=owner;state.units.items[0].hp=1;
    CHECK(scene_flow_run(&context,&flow)==-1 && owner->camera_cell_x==2);
    free(owner);return 0;
}

/* 11cac的0/1是水色门；1366a的相位、等待、脚本尾各保原调用点。
 * 使用真实地形/FDICON，只替换时间与present，不以SDL结果构造原参预期。 */
static int test_scene_water_callers(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=malloc(sizeof(*before));
    uint8_t *saved=malloc(FD2_FIELD_PAGE_SIZE),*actual=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && vga && before && saved && actual);
    CHECK(fd2_field_game_open_resources(game,fdother,0)==0);
    CHECK(fd2_field_units_set(&game->units,0,0,0,2,3,3)==0);
    fd2_scene_field_state state={0};state.units=game->units;
    reset_controls(vga);scene_host_quit_requested=0;
    render_scene_11cac(vga,&game->terrain,&game->map,&game->sprites,&state,1);
    CHECK(!scene_render_error && water_calls==0 && vga->field_water_phase==0);
    render_scene_11cac(vga,&game->terrain,&game->map,&game->sprites,&state,0);
    CHECK(!scene_render_error && water_calls==1 && vga->field_water_phase==1);
    *before=*vga;
    render_scene_11cac(vga,&game->terrain,&game->map,&game->sprites,&state,2);
    CHECK(scene_render_error && !memcmp(vga,before,sizeof(*vga)));

    static const struct {
        uint8_t script[5];int fade;unsigned waters,frames,cursor;
    } cases[]={
        {{1,1,1,0,3},0,6,7,3}, /* six11cac(0), final11cac(1) */
        {{1,1,1,0,3},1,0,7,3}, /* fade phases use1; same final1 */
        {{1,11,1,0,3},1,3,67,3}, /* 53AFB reaches64; last three phases use0 */
        {{1,0x82,1,0,0},0,2,3,3}, /* two waits use0; final1 */
        {{1,0x80,1,0,0},0,1,3,2} /* initial11eee, restore0, final1 */
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        reset_controls(vga);scene_host_quit_requested=0;
        state=(fd2_scene_field_state){0};state.units=game->units;
        CHECK(play_opening_move_script(vga,&game->terrain,&game->map,&game->sprites,
            &state,NULL,cases[i].script,sizeof(cases[i].script),cases[i].fade,0)==0);
        CHECK(water_calls==cases[i].waters && presents==(int)cases[i].frames);
        CHECK(vga->field_visual.cursor_phase==cases[i].cursor);
        CHECK(state.units.items[0].frame_phase==0);
    }
    /* pan每格11cac(0)，entry cold render_base来自205da的11cac(1)。 */
    reset_controls(vga);scene_host_quit_requested=0;
    state=(fd2_scene_field_state){0};state.units=game->units;
    pan_camera_to(vga,&game->terrain,&game->map,&game->sprites,&state,2,1,0);
    CHECK(!scene_render_error && water_calls==3 && presents==3);
    reset_controls(vga);scene_host_quit_requested=0;
    state=(fd2_scene_field_state){0};state.units=game->units;
    fd2_static_flow_node nodes[]={
        {"entry","render_base",{0},{1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT},
        {"end","end",{0},{-1,-1,-1,-1,-1,-1,-1,-1},FD2_FLOW_PRESENT}
    };
    fd2_static_flow flow={"test.scene.entry-water",nodes,2,0};
    scene_flow_context context={0};context.vga=vga;context.terrain=&game->terrain;
    context.map=&game->map;context.sprites=&game->sprites;context.state=&state;
    CHECK(scene_flow_run(&context,&flow)==0 && !water_calls && !presents);
    CHECK(vga->field_visual.cursor_phase==1);

    /* 3231b纯delay不重画；结束或中断均不触摸像素/时钟，不伪造脚本尾。 */
    *before=*vga;
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,saved,FD2_FIELD_PAGE_SIZE,456)==0);
    uint64_t now=test_ms;
    opening_delay(vga,&game->terrain,&game->map,&game->sprites,&state,200,0);
    CHECK(test_ms==now+200 && !water_calls && !presents && !memcmp(vga,before,sizeof(*vga)));
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(!memcmp(saved,actual,FD2_FIELD_PAGE_SIZE));
    quit_on_delay_ms=200;
    opening_delay(vga,&game->terrain,&game->map,&game->sprites,&state,200,0);
    CHECK(scene_host_quit_requested && !water_calls && !presents);
    before->input.quit_requested=1;
    CHECK(!memcmp(vga,before,sizeof(*vga)));
    now=test_ms;
    opening_delay(vga,&game->terrain,&game->map,&game->sprites,&state,200,0);
    CHECK(test_ms==now);

    reset_controls(vga);scene_host_quit_requested=0;quit_at_frame=2;
    state=(fd2_scene_field_state){0};state.units=game->units;
    CHECK(play_opening_move_script(vga,&game->terrain,&game->map,&game->sprites,
        &state,NULL,cases[0].script,5,0,0)==-1);
    CHECK(water_calls==2 && presents==2 && vga->field_visual.cursor_phase==2);
    CHECK(state.units.items[0].x==3 && state.units.items[0].frame_phase==2);
    CHECK(!memcmp(&game->units.items[0],&state.units.items[0],2));
    reset_controls(vga);scene_host_quit_requested=0;
    fd2_field_game_close(game);free(game);free(before);free(saved);free(actual);free(vga);
    return 0;
}

static void arrival_expected_image(uint8_t *page,const fd2_image *image,int x,int y) {
    for(int row=0;row<image->height;row++)for(int col=0;col<image->width;col++) {
        int dx=x+col,dy=y+row;
        uint8_t pixel=image->pixels[(size_t)row*(size_t)image->width+(size_t)col];
        if(pixel && dx>=0 && dx<456 && dy>=0 && dy<336)page[(size_t)dy*456u+(size_t)dx]=pixel;
    }
}

/* 32999原写序：前7张只含旧snapshot+LMI，之后以6/7/8三个边界更新
 * 底页。真实LMI/FDICON及整153216B写集，同时检查原App时钟未变。 */
static int test_arrival_page_and_clocks(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=malloc(sizeof(*before));
    uint8_t *trace=malloc(12u*FD2_FIELD_PAGE_SIZE),*expected=malloc(FD2_FIELD_PAGE_SIZE);
    uint8_t *base=malloc(FD2_FIELD_PAGE_SIZE),*actual=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && vga && before && trace && expected && base && actual);
    CHECK(fd2_field_game_open_resources(game,fdother,0)==0);
    CHECK(fd2_field_units_set(&game->units,0,0,0,2,2,3)==0);
    CHECK(fd2_field_units_set(&game->units,1,1,1,2,5,3)==0);
    game->units.items[0].movement_profile=game->units.items[1].movement_profile=19;
    fd2_field_unit_set_hidden(&game->units.items[1],1);
    fd2_field_units original=game->units;
    fd2_scene_field_state state={0};state.units=original;
    const uint8_t *data;size_t size;
    CHECK(fd2_archive_get(fdother,9,&data,&size)==0);
    fd2_image fx[12]={{0}};
    for(size_t i=0;i<12;i++) {
        uint32_t a=rd_u32_le(data+6+4*i),b=i+1<rd_u16_le(data+4)?rd_u32_le(data+10+4*i):(uint32_t)size;
        CHECK(a<b && b<=size && fd2_lmi_decode_image(&fx[i],data+a,b-a)==0);
    }
    /* FDICON1680帧原扫描无opaque0；此两独立raw帧的零是SKIP，
     * 不用生产缓存getter/arrival绘制叶来生成预期。 */
    fd2_image old_actor={24,24,game->sprites.decoded_frames+2u*576u};
    fd2_image new_actor={24,24,game->sprites.decoded_frames+14u*576u};
    reset_controls(vga);scene_host_quit_requested=0;arrival_page_trace=trace;
    memset(base,85,FD2_FIELD_PAGE_SIZE);
    CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,base,FD2_FIELD_PAGE_SIZE,456)==0);
    vga->field_visual.actor_idle_phase=2;vga->field_visual.actor_idle_tick=7;
    vga->field_visual.cursor_phase=3;
    CHECK(opening_actor_arrival(vga,fdother,&game->terrain,&game->map,&game->sprites,
        &state,NULL,1,2,0)==0);
    CHECK(presents==12 && !water_calls && !scene_render_error);
    CHECK(vga->field_visual.actor_idle_phase==2 && vga->field_visual.actor_idle_tick==7 &&
        vga->field_visual.cursor_phase==3 && !vga->field_water_phase && !vga->field_water_last_tick);
    for(size_t frame=0;frame<=6;frame++) {
        memcpy(expected,base,FD2_FIELD_PAGE_SIZE);
        arrival_expected_image(expected,&fx[frame],72+(5-1)*24,66+3*24);
        CHECK(!memcmp(expected,trace+frame*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    }
    /* i6在VGA投影之后重建，故其产物首次出现在frame7；新actor上抬8px。 */
    arrival_expected_image(base,&old_actor,72+2*24,66+3*24);
    arrival_expected_image(base,&new_actor,72+5*24,66+3*24-8);
    memcpy(expected,base,FD2_FIELD_PAGE_SIZE);
    arrival_expected_image(expected,&fx[7],72+(5-1)*24,66+3*24);
    CHECK(!memcmp(expected,trace+7u*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    fd2_field_page *terrain=fd2_field_page_create();CHECK(terrain);
    for(size_t frame=8;frame<=9;frame++) {
        CHECK(fd2_field_page_write(terrain,0,0,456,336,trace+(frame-1u)*FD2_FIELD_PAGE_SIZE,
            FD2_FIELD_PAGE_SIZE,456)==0);
        CHECK(fd2_terrain_render_page_block(terrain,&game->terrain,&game->map,72,72,13,8,0,0,
            (int)arrival_terrain_trace[frame],2,NULL,NULL)==0);
        CHECK(fd2_field_page_read(terrain,0,0,456,336,base,FD2_FIELD_PAGE_SIZE,456)==0);
        arrival_expected_image(base,&old_actor,72+2*24,66+3*24);
        arrival_expected_image(base,&new_actor,72+5*24,66+3*24-(frame==8?5:0));
        memcpy(expected,base,FD2_FIELD_PAGE_SIZE);
        arrival_expected_image(expected,&fx[frame],72+(5-1)*24,66+3*24);
        CHECK(!memcmp(expected,trace+frame*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    }
    for(size_t frame=10;frame<12;frame++) {
        memcpy(expected,base,FD2_FIELD_PAGE_SIZE);
        arrival_expected_image(expected,&fx[frame],72+(5-1)*24,66+3*24);
        CHECK(!memcmp(expected,trace+frame*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    }
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(!memcmp(actual,trace+11u*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    CHECK(!fd2_field_unit_is_hidden(&state.units.items[1]) && !memcmp(&game->units,&original,sizeof(original)));
    fd2_field_page_destroy(terrain);

    /* late LMI11坏宽度：12帧预检先完成，App/Scene及所有clock均原样。 */
    reset_controls(vga);scene_host_quit_requested=0;state.units=original;
    uint8_t *bad=malloc(size);CHECK(bad);memcpy(bad,data,size);
    uint32_t last=rd_u32_le(bad+6+11*4);bad[last]=bad[last+1]=0;
    arrival_data_override=bad;arrival_data_override_size=size;*before=*vga;
    CHECK(opening_actor_arrival(vga,fdother,&game->terrain,&game->map,&game->sprites,
        &state,NULL,1,2,0)==-1);
    CHECK(!presents && !water_calls && !memcmp(vga,before,sizeof(*vga)));
    CHECK(!memcmp(&state.units,&original,sizeof(original)));
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0);
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)CHECK(actual[i]==0);
    arrival_data_override=NULL;free(bad);

    /* hostquit只保已经投影的第8张LMI，不重建后继底，也没有尾11cac。 */
    reset_controls(vga);scene_host_quit_requested=0;arrival_page_trace=trace;quit_at_frame=8;
    state.units=original;vga->field_visual.actor_idle_phase=2;vga->field_visual.cursor_phase=3;
    CHECK(opening_actor_arrival(vga,fdother,&game->terrain,&game->map,&game->sprites,
        &state,NULL,1,2,0)==-1);
    CHECK(presents==8 && scene_host_quit_requested && !water_calls && vga->field_visual.cursor_phase==3);
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(!memcmp(actual,trace+7u*FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_SIZE));
    CHECK(!memcmp(&game->units,&original,sizeof(original)));
    for(size_t i=0;i<12;i++)fd2_image_free(&fx[i]);
    reset_controls(vga);scene_host_quit_requested=0;
    fd2_field_game_close(game);free(game);free(vga);free(before);
    free(trace);free(expected);free(base);free(actual);return 0;
}

int main(int argc, char **argv) {
    CHECK(test_scene_flow_graph()==0);
    CHECK(test_scene_flow_view_commit()==0);
    fd2_archive fdother = {0};
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(test_scene_water_callers(&fdother)==0);
    CHECK(test_arrival_page_and_clocks(&fdother)==0);
    CHECK(test_scene_actual_dynamic_consumer(&fdother)==0);
    CHECK(test_shared_initialization_definition(&fdother) == 0);
    CHECK(test_scene_existing_actor_motion(&fdother)==0);
    CHECK(test_common_step_live_motion(&fdother)==0);
    if(argc==2 && strcmp(argv[1],"--b5")==0) {
        CHECK(test_dialog_identity_lookup()==0);
        CHECK(test_stage1_presentation(&fdother)==0);
        CHECK(test_stage1_victory_projection(&fdother)==0);
        CHECK(test_stage1_dialog_display(&fdother)==0);
        CHECK(test_stage2_presentation(&fdother)==0);
        CHECK(test_turn_event_presentation(&fdother)==0);
        CHECK(test_game_over_frames(&fdother)==0);
        fd2_archive_close(&fdother);
        puts("scene_transition: stage0/stage1 event identity and committed attributes, host quit, GAME OVER passed (no GUI)");
        return 0;
    }
    int final_present = 0;
    CHECK(test_natural_stage0_handoff(&fdother, &final_present) == 0);
    CHECK(test_resource_error_has_no_handoff(&fdother) == 0);
    CHECK(test_required_palette_error_has_no_handoff(&fdother) == 0);
    CHECK(test_real_portrait_orientation() == 0);
    CHECK(test_portrait_decode_error_has_no_handoff(&fdother) == 0);
    CHECK(test_host_quit_has_no_handoff(&fdother, final_present) == 0);
    fd2_archive_close(&fdother);
    puts("scene_handoff_test: ok");
    return 0;
}
