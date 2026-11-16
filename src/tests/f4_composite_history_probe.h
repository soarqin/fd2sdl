/* field_composite_magic_full_execute @0x2d80d（原sub_2D80D）：CAST到MAP组前。
 * 原37part/slot1/summary实际读取；回图11cac用既有明确phase3输入页。
 * 原malloc的新App-before显式提供，不把C初次calloc或保留页当原默认值。 */
#ifndef FD2_F4_COMPOSITE_HISTORY_PROBE_H
#define FD2_F4_COMPOSITE_HISTORY_PROBE_H
enum { F4_CH_PAYLOAD=455552 };
static fd2_battle_animation *f4_ch_context;
static fd2_field_page *f4_ch_page;
static unsigned f4_ch_frame;
static int f4_ch_io_error;
static int f4_ch_parse(char *line,unsigned v[4]) {
    char *p=line,*end;
    for(unsigned i=0;i<4;i++) {
        while(isspace((unsigned char)*p))p++;
        if(*p=='-')return -1;
        errno=0;unsigned long value=strtoul(p,&end,10);
        if(errno || p==end || value>UINT_MAX || (i<3 && !isspace((unsigned char)*end)))return -1;
        v[i]=(unsigned)value;p=end;
    }
    while(isspace((unsigned char)*p))p++;
    return *p || v[0]<32 || v[0]>35 || (v[1]!=0 && v[1]!=2) || v[2]>1 || v[3]>3 ||
        (v[3] && v[0]!=34)?-1:0;
}
static void f4_ch_packet(fd2_vga *vga,const char *kind,int status) {
    fd2_battle_magic_exchange *e=f4_ch_context->magic_exchange;
    if(f4_ch_io_error || !e)return;
    if(printf("{\"schema\":\"fd2-f4-battle-history-state-v1\",\"case\":%u,\"kind\":\"%s\",\"phase\":0,\"part\":-1,\"frame\":%u,\"target\":0,\"next_part\":%zu,\"markers\":%u,\"idle_part\":%u,\"idle_ticks\":%u,\"shake\":%d,\"tint\":%d,\"caster_mp\":%u,\"active_hp\":%u,\"cast_done\":%s,\"started\":%s,\"failed\":%s,\"status\":%d,\"ticks\":%u,\"delays\":%u,\"cues\":%u,\"last_cue\":%zu,\"pumps\":%u,\"payload_bytes\":%u,\"secondary_cues\":%u,\"stops\":%u,\"returns\":%u,\"cast_bank_open\":%s,\"map_bank_open\":%s,\"host\":%s}\n",
        f4_bh_case,kind,f4_ch_frame,e->next_part,e->markers,e->idle_part,e->idle_ticks,e->shake_phase,e->tint_phase,
        e->caster.mp,e->units[0].hp,e->cast_done?"true":"false",e->started?"true":"false",f4_ch_context->failed?"true":"false",status,
        ticks,delays,cues,last_cue,input_pumps,F4_CH_PAYLOAD,secondary_cues,primary_stops,runtime_calls,
        e->sound.archive.data?"true":"false",e->map_sound.archive.data?"true":"false",f4_ch_context->host_quit?"true":"false")<0 ||
       fwrite(e->work,1,172800,stdout)!=172800 || fwrite(vga->framebuffer,1,64000,stdout)!=64000 ||
       fwrite(e->board.framebuffer,1,64000,stdout)!=64000 || fwrite(f4_ch_page->pixels,1,FD2_FIELD_PAGE_SIZE,stdout)!=FD2_FIELD_PAGE_SIZE ||
       fwrite(vga->palette,1,768,stdout)!=768 || fwrite(vga->dac,1,768,stdout)!=768 || fflush(stdout))f4_ch_io_error=1;
    if(!strcmp(kind,"upload"))f4_ch_frame++;
}
static void f4_ch_present(fd2_vga *vga) {f4_ch_packet(vga,"upload",0);}
static int f4_ch_one(const fd2_archive *other,const fd2_archive *figani,
    const fd2_archive *bg,const fd2_archive *tai,const unsigned v[4]) {
    fd2_battle_animation ctx={0};fd2_pcm_player primary={0},secondary={0};
    fd2_battle_magic_exchange *e=calloc(1,sizeof(*e)),*saved=malloc(sizeof(*e));
    fd2_field_game *gb=malloc(sizeof(game));fd2_vga *vb=malloc(sizeof(screen));
    fd2_field_page *page=fd2_field_page_create(),*pb=malloc(sizeof(*pb));int status=-1,render_status=0;
    if(!e || !saved || !gb || !vb || !page || !pb)goto done;
    if(fd2_field_visuals_open(&game.visuals,other))goto done;
    reset_observer();input_pumps=0;last_cue=0;fake_ms=1000;f4_ch_frame=0;f4_ch_io_error=0;
    ctx.vga=&screen;ctx.fdother=other;ctx.figani=figani;ctx.visual_owner=&game;ctx.primary=&primary;ctx.secondary=&secondary;
    ctx.backgrounds=*bg;ctx.terrain_images=*tai;ctx.magic_exchange=e;f4_ch_context=&ctx;f4_ch_page=page;
    game.units.count=3;actor(&game.units.items[0],9,(uint8_t)v[1],1);
    actor(&game.units.items[1],0,v[1]?0:2,2);actor(&game.units.items[2],96,v[1]?0:2,3);
    for(unsigned i=0;i<3;i++){game.units.items[i].hp=1;game.units.items[i].hp_max=42;game.units.items[i].mp=game.units.items[i].mp_max=999;}
    game.ready=1;game.failure=0;game.stage=0;game.option_values[1]=game.option_values[3]=1;
    game.visual_owner=&screen.field_visual;game.attack_rng=forbidden_rng;game.attack_rng_userdata=&business_rng_calls;
    e->id=(uint8_t)v[0];e->mode=FD2_FIELD_MAGIC_MODE_COMPOSITE;e->action_kind=FD2_FIELD_MAGIC_ACTION_MAGIC;
    e->count=e->effect_count=2;e->targets[0]=1;e->targets[1]=2;e->caster_index=0;e->sfx_enabled=1;e->caster=game.units.items[0];
    for(unsigned i=0;i<2;i++)e->units[i]=game.units.items[i+1];
    e->fx_parameters.return_lut_base=(uint8_t)(v[0]<=33?19:15);
    if(clip_open_visual(&e->casting,figani,28) || clip_open_visual(&e->caster_idle,figani,27) ||
       clip_open_visual(&e->target_idle[0],figani,0) || clip_open_visual(&e->target_idle[1],figani,288) ||
       clip_open_visual(&e->particles,other,v[0]+33u) || image_entry(&e->bg,bg,3) || image_entry(&e->terrain,tai,3) ||
       fd2_pcm_bank_open(&e->sound,other,k_battle_composite_banks[v[0]-32u]) || fd2_pcm_bank_open(&e->map_sound,other,80))goto done;
    e->shake_phase=0;e->tint_phase=-1;memset(e->board.framebuffer,0,64000);
    if(magic_summary(e,&game,&e->caster,0))goto done;
    blit(e->board.framebuffer,320,200,320,&e->bg,0,50,-1);
    f4_phys_seed(e->work,172800,23,640,41,(int)v[2]);f4_phys_seed(screen.framebuffer,64000,19,320,61,(int)v[2]);
    f4_phys_seed(page->pixels,FD2_FIELD_PAGE_SIZE,17,456,83,(int)v[2]);
    const uint8_t *palette;size_t palette_size;
    if(fd2_archive_get(other,0,&palette,&palette_size) || palette_size<768)goto done;
    memcpy(screen.palette,palette,768);for(unsigned i=0;i<768;i++)screen.dac[i]=(uint8_t)((palette[i]<<2)|(palette[i]>>4));
    screen.field_page=page;screen.frame_deadline_ns=UINT64_C(999999999999);screen.frame_interval_ns=UINT64_C(23000000);
    unsigned cost_markers=0;for(size_t i=0;i<e->casting.clip.intro_end;i++)cost_markers+=e->casting.parts[i].part.effect_marker==1;
    const uint8_t *record=fd2_field_magic_record_get((uint8_t)v[0]);if(!record || !cost_markers || cost_markers*(unsigned)record[5]>999u)goto done;
    fd2_field_unit initial_caster=game.units.items[0];game.units.items[0].mp=(uint16_t)(999u-cost_markers*(unsigned)record[5]);
    fd2_field_magic_present_event event={0};event.magic_id=(uint8_t)v[0];event.actor_index=0;
    event.action_kind=FD2_FIELD_MAGIC_ACTION_MAGIC;event.presentation_mode=FD2_FIELD_MAGIC_MODE_COMPOSITE;event.group_pass=UINT8_MAX;
    event.targets=e->targets;event.target_count=event.effect_target_count=2;event.cast_cost_count=(uint8_t)cost_markers;
    event.actor_before=&initial_caster;event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    if(v[3]==1)ctx.skip=1;if(v[3]==2)runtime_failure=1;
    if(v[3]==3){composite_capture=1;composite_quit_after_return=1;}
    expected_primary=&primary;expected_secondary=&secondary;*saved=*e;*vb=screen;*gb=game;*pb=*page;
    f4_promotion_observe=f4_ch_present;render_status=fd2_battle_animation_magic_present(&game,&event,&ctx);
    f4_promotion_observe=NULL;f4_ch_packet(&screen,"cast_end",render_status);
    if(f4_ch_io_error || memcmp(gb,&game,sizeof(game)) || business_rng_calls || wrong_stop || frames!=f4_ch_frame || input_pumps!=frames ||
       (v[3]==2?render_status!=-1 || !ctx.failed:render_status || ctx.failed) || (v[3]==3)!=(ctx.host_quit!=0))goto done;
    memcpy(saved->work,e->work,128000);memcpy(saved->board.framebuffer,e->board.framebuffer,64000);saved->caster=e->caster;
    saved->cast_done=e->cast_done;saved->started=e->started;saved->sound=e->sound;
    memcpy(vb->framebuffer,screen.framebuffer,64000);memcpy(vb->dac,screen.dac,768);if(v[3]==3)vb->input=screen.input;
    memcpy(pb->pixels,page->pixels,FD2_FIELD_PAGE_SIZE);
    if(memcmp(saved,e,sizeof(*e)) || memcmp(vb,&screen,sizeof(screen)) || memcmp(pb,page,sizeof(*page)))goto done;
    if(printf("{\"case_end\":%u,\"frames\":%u,\"status\":%d,\"failed\":%s,\"skip\":%s,\"other_owners_unchanged\":true}\n",
        f4_bh_case,frames,render_status,ctx.failed?"true":"false",ctx.skip?"true":"false")<0 || fflush(stdout))goto done;
    status=0;
done:
    f4_promotion_observe=NULL;composite_capture=composite_quit_after_return=0;runtime_failure=0;
    if(ctx.magic_exchange)magic_close_exchange(&ctx);else free(e);
    fd2_field_visuals_close(&game.visuals);fd2_field_page_destroy(page);screen.field_page=NULL;
    free(pb);free(saved);free(gb);free(vb);f4_ch_context=NULL;f4_ch_page=NULL;return status;
}
static int f4_composite_history_cli(const char *root) {return f4_history_loaded_cli(root,f4_ch_parse,f4_ch_one);}
#endif
