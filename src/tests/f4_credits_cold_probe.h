/* C13：真实stage.cinematic.credits与全部子图的有限观察口。
 * 只指定合法持久constructor IDs0/1/2与两个输入计划，不解释/剪切冷图。
 * full-page expected由31c49/2e9a8/311e5原来源与保留原像素叶独立组合。
 * 此私有TU不打开SDL/声音设备；固定tick替身不签物理PIT/绝对时间。 */
#ifndef FD2_F4_CREDITS_COLD_PROBE_H
#define FD2_F4_CREDITS_COLD_PROBE_H
#define FD2_F4_PROTOCOL_ONLY
#include "f4_reader_probe.h"
#undef FD2_F4_PROTOCOL_ONLY

static scene_flow_context *f4_credits_context;
static fd2_field_game *f4_credits_after_setup;
static const fd2_static_flow *f4_credits_flow;
static const fd2_static_flow_node *f4_credits_node;
static unsigned f4_credits_case,f4_credits_frames,f4_credits_rng_calls;
static unsigned f4_credits_actors,f4_credits_outer,f4_credits_card_frame;
static unsigned f4_credits_keys,f4_credits_tai_bound,f4_credits_failed;
static unsigned f4_credits_roll_frames,f4_credits_wait_nodes,f4_credits_wait_ticks;
static unsigned f4_credits_epochs,f4_credits_blink_resets,f4_credits_input_pumps;
static unsigned f4_credits_clock_reads,f4_credits_seen_input[3];
static uint64_t f4_credits_clock_tick;
static int32_t f4_credits_last_copy[8];

static void f4_credits_fail(const char *why) {
    if(!f4_credits_failed)fprintf(stderr,"C13 credits probe: %s\n",why);
    f4_credits_failed=1;scene_render_error=1;
}
static uint32_t f4_credits_random(void *unused) {
    (void)unused;f4_credits_rng_calls++;return 0;
}
static int f4_credits_emit(const char *op,unsigned frame) {
    const size_t n=64000;const uint8_t zeros[768]={0};
    fd2_vga *vga=f4_credits_context->vga;
    if(printf("{\"schema\":\"fd2-f4-reader-v1\",\"status\":0,\"op\":\"%s\","
        "\"a\":%u,\"b\":%u,\"variant\":%u,\"stride\":320,\"height\":200,"
        "\"page_bytes\":64000,\"payload_bytes\":129536,\"owner_guard\":true,"
        "\"pixel_addresses_written\":0,\"palette_addresses_written\":0,"
        "\"history_mask_measured\":false,\"palette_semantics\":\"actualDAC\","
        "\"flow\":\"%s\",\"node\":\"%s\",\"source_op\":\"%s\","
        "\"outer\":%u,\"actor_visits\":%u,\"card_frame\":%u,\"rng\":%u,\"keys\":%u,"
        "\"queued\":%u,\"wait_nodes\":%u,\"wait_ticks\":%u,\"epochs\":%u,"
        "\"blink_resets\":%u,\"clock_reads\":%u,\"input_pumps\":%u,\"blink\":%u,\"idle_part\":%u,\"idle_ticks\":%u,\"last_copy\":[%d,%d,%d,%d,%d,%d,%d,%d]}\n",
        op,f4_credits_case,frame,0u,f4_credits_flow?f4_credits_flow->id:"final",
        f4_credits_node?f4_credits_node->id:"final",f4_credits_node?f4_credits_node->op:"final",
        f4_credits_outer,f4_credits_actors,f4_credits_card_frame,f4_credits_rng_calls,
        f4_credits_keys,(unsigned)vga->input.queued_count,f4_credits_wait_nodes,f4_credits_wait_ticks,
        f4_credits_epochs,f4_credits_blink_resets,f4_credits_clock_reads,f4_credits_input_pumps,
        f4_credits_context->cinematic?(unsigned)f4_credits_context->cinematic->blink_countdown:0u,
        f4_credits_context->cinematic?(unsigned)f4_credits_context->cinematic->idle_part:0u,
        f4_credits_context->cinematic?(unsigned)f4_credits_context->cinematic->idle_ticks:0u,
        f4_credits_last_copy[0],f4_credits_last_copy[1],f4_credits_last_copy[2],f4_credits_last_copy[3],
        f4_credits_last_copy[4],f4_credits_last_copy[5],f4_credits_last_copy[6],f4_credits_last_copy[7])<0 ||
       fwrite(vga->framebuffer,1,n,stdout)!=n || fwrite(vga->dac,1,768,stdout)!=768)return -1;
    for(size_t i=0;i<n/sizeof(zeros);i++)if(fwrite(zeros,1,sizeof(zeros),stdout)!=sizeof(zeros))return -1;
    if(fwrite(zeros,1,n%sizeof(zeros),stdout)!=n%sizeof(zeros) ||
       fwrite(zeros,1,768,stdout)!=768 || fflush(stdout))return -1;
    return 0;
}
static void f4_credits_upload(fd2_vga *vga) {
    if(!f4_credits_enabled)return;
    if(!f4_credits_context || vga!=f4_credits_context->vga || !f4_credits_node || !f4_credits_flow) {
        f4_credits_fail("upload has no actual flow owner");return;
    }
    const char *op=f4_credits_node->op,*flow=f4_credits_flow->id;
    int emit=!strcmp(op,"cin_present") || (!strcmp(op,"palette_subtract") &&
        (!strcmp(flow,"stage.cinematic.card_first") || !strcmp(flow,"stage.source.fade.out")));
    if(emit) {
        if(!strcmp(flow,"stage.cinematic.credits"))f4_credits_roll_frames++;
        if(f4_credits_frames>=3000u || f4_credits_emit(f4_credits_frames?"frame":"start",f4_credits_frames))f4_credits_fail("full page output");
        else f4_credits_frames++;
    }
}
static void f4_credits_input_frame(fd2_input *input) {
    /* No synthetic key per upload. Only the two stated source peek sites inject.
     * The actual same-owner test pump remains visible if the walker requests it. */
    f4_credits_input_pumps++;
    fd2_input_begin_test_frame(input,s5_test_ms);
}
static void f4_credits_before_node(const fd2_static_flow *flow,const fd2_static_flow_node *node) {
    if(!f4_credits_enabled || !flow || !node)return;
    f4_credits_flow=flow;f4_credits_node=node;
    scene_flow_context *c=f4_credits_context;
    if(!c || f4_credits_failed)return;
    if(f4_credits_after_setup && memcmp(f4_credits_after_setup,c->game,sizeof(*c->game))) {
        f4_credits_fail("renderer changed post-setup Game owner");return;
    }
    if(!strcmp(node->op,"cin_credits_blink_reset")) {
        if(f4_credits_blink_resets++ || f4_credits_after_setup) {f4_credits_fail("duplicate initial blink reset");return;}
        f4_credits_after_setup=malloc(sizeof(*c->game));
        if(!f4_credits_after_setup) {f4_credits_fail("post-setup snapshot");return;}
        *f4_credits_after_setup=*c->game;
        if(c->game->roster_count!=3u || c->game->units.count<3u) {f4_credits_fail("source setup count");return;}
        for(unsigned i=0;i<3;i++)if(c->game->units.items[i].unit_id!=i ||
            c->game->units.items[i].text_id!=i || c->game->units.items[i].side!=2u) {
            f4_credits_fail("source setup actor identity");return;
        }
    }
    /* Source loads TAI only after all 500 ROLL windows and freeing that owner.
     * This is the actual still-owned archive buffer, never a reopened handshake. */
    if(c->cinematic && c->cinematic->credits_assets && !f4_credits_tai_bound) {
        if(f4_credits_roll_frames!=500u || c->cinematic->pages[FD2_CINEMATIC_ROLL].data ||
            f4_input_receipt("TAI.DAT",c->cinematic->tai.data,c->cinematic->tai.size)) {
            f4_credits_fail("late TAI owner/input order");return;
        }
        f4_credits_tai_bound=1;
    }
    if(!strcmp(node->op,"cin_copy"))memcpy(f4_credits_last_copy,node->args,sizeof(f4_credits_last_copy));
    if(!strcmp(node->op,"cin_credits_actor")) {
        if(node->args[0]<0 || node->args[0]>2 || f4_credits_actors>=3u) {f4_credits_fail("unexpected actor visit");return;}
        f4_credits_outer=(unsigned)node->args[0];f4_credits_actors++;
        f4_credits_card_frame=0;
    } else if(!strcmp(node->op,"cin_credits_card"))f4_credits_card_frame=(unsigned)node->args[0];
    else if(!strcmp(node->op,"cin_credits_epoch"))f4_credits_epochs++;
    else if(!strcmp(node->op,"delay_ticks") || !strcmp(node->op,"cin_credits_part_wait")) {
        unsigned ticks;
        if(!strcmp(node->op,"delay_ticks"))ticks=(unsigned)node->args[0];
        else {
            size_t count;
            if(f4_credits_private_credits_part_info(c->cinematic,&count,(size_t)node->args[0],&ticks)) {
                f4_credits_fail("actual part wait metadata");return;
            }
        }
        f4_credits_wait_nodes++;f4_credits_wait_ticks+=ticks;
    } else if(!strcmp(node->op,"branch_cin_input") && f4_credits_case==1u) {
        unsigned visit=f4_credits_actors;
        int inject=(visit==1u && f4_credits_outer==2u && f4_credits_card_frame==17u) ||
            (visit==2u && f4_credits_outer==0u && f4_credits_card_frame==218u);
        if(inject) {
            if(f4_credits_seen_input[visit-1u]++ || c->vga->input.queued_count) {
                f4_credits_fail("source input plan duplicate/dirty FIFO");return;
            }
            c->vga->input.queued_head=0;c->vga->input.queued_count=1;
            c->vga->input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
            c->vga->input.queued_keys[0].bios_code=0x1c0d;f4_credits_keys++;
        }
    }
}
static int f4_credits_cold_cli(fd2_archive *other) {
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)return 1;
#endif
    char line[128],op[16];unsigned v[3];int result=-1;
    fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
    fd2_scene_field_state *state=calloc(1,sizeof(*state));scene_flow_context c={0};
    if(!game || !vga || !state || s5_prepare(game,other,26))goto done;
    game->roster_count=3;
    for(unsigned i=0;i<3;i++) {
        fd2_field_unit unit={0};if(fd2_field_unit_persistent_create(&unit,(uint8_t)i))goto done;
        game->roster[i]=unit;
    }
    vga->field_page=fd2_field_page_create();if(!vga->field_page || fd2_field_game_bind_visual(game,vga))goto done;
    const uint8_t *palette;size_t palette_size;
    if(fd2_archive_get(other,0,&palette,&palette_size) || palette_size!=768)goto done;
    fd2_vga_set_palette(vga,palette);
    for(size_t i=0;i<64000;i++)vga->framebuffer[i]=(uint8_t)(i*17u+i/320u+83u);
    /* Same actual resource owners used by setup/render, before reading the sole start scalar. */
    if(f4_input_receipt("FDOTHER.DAT",other->data,other->size) ||
       f4_input_receipt("FDTXT.DAT",game->fdtxt_archive.data,game->fdtxt_archive.size) ||
       f4_input_receipt("FIGANI.DAT",game->figani_archive.data,game->figani_archive.size) ||
       f4_input_receipt("DATO.DAT",game->dato_archive.data,game->dato_archive.size) ||
       f4_input_receipt("FDFIELD.DAT",game->field_archive.data,game->field_archive.size) ||
       f4_input_receipt("FDSHAP.DAT",game->fdshap_archive.data,game->fdshap_archive.size) ||
       f4_input_receipt("FDICON.B24",game->sprites.data,game->sprites.size))goto done;
    if(!fgets(line,sizeof(line),stdin) || !strchr(line,'\n') || f4_parse(line,op,v) ||
        strcmp(op,"start") || v[0]>1u || v[1] || v[2])goto done;
    f4_credits_case=v[0];f4_credits_context=&c;
    state->ordinary_cursor=fd2_field_cursor_frame(&game->visuals,0);
    if(!state->ordinary_cursor)goto done;
    c.vga=vga;c.fdother=other;c.game=c.mutable_game=game;c.state=state;
    c.terrain=&game->terrain;c.map=&game->map;c.sprites=&game->sprites;c.source_flow=1;
    game->attack_rng=f4_credits_random;game->attack_rng_userdata=NULL;
    s5_reset_io(vga);scene_source_load(&c);fd2_bios_clock_reset(0,0);
    f4_credits_clock_tick=100;f4_credits_outer=99;f4_credits_enabled=1;
    result=scene_flow_run(&c,fd2_static_flow_lookup("stage","stage.cinematic.credits"));
    if(f4_credits_failed || !f4_credits_tai_bound || f4_credits_roll_frames!=500u ||
       f4_credits_actors!=3u || f4_credits_epochs!=3u || f4_credits_blink_resets!=1u ||
       f4_credits_keys!=2u*f4_credits_case || vga->input.queued_count ||
       !c.cinematic || c.cinematic->credits_assets || c.cinematic->actor_ready ||
       !f4_credits_after_setup || memcmp(f4_credits_after_setup,game,sizeof(*game)))result=-1;
    if(c.cinematic)for(unsigned slot=1;slot<=4;slot++)if(c.cinematic->pages[slot].data)result=-1;
    if(!result && f4_credits_emit("done",f4_credits_frames))result=-1;
done:
    f4_credits_enabled=0;f4_credits_private_close(&c.cinematic);
    if(game)fd2_field_game_close(game);
    if(vga)fd2_field_page_destroy(vga->field_page);
    free(f4_credits_after_setup);f4_credits_after_setup=NULL;
    free(state);free(vga);free(game);
    if(result){fputs("{\"schema\":\"fd2-f4-reader-v1\",\"status\":1,\"payload_bytes\":0}\n",stdout);fflush(stdout);}
    return result?1:0;
}
#endif
