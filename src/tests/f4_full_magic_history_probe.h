/* FULL six finite histories: original2ff01/31266, explicit FX tape and jitter.
 * Wholeowners use existing loadedbuffer/stdout protocol. FINISH frees exchange;
 * final packet is named retired last-live snapshot, never a dangling read. */
#ifndef FD2_F4_FULL_MAGIC_HISTORY_PROBE_H
#define FD2_F4_FULL_MAGIC_HISTORY_PROBE_H
enum { F4_FH_PAYLOAD=455552 };
static fd2_battle_animation *f4_fh_ctx;
static fd2_battle_magic_exchange *f4_fh_last;
static fd2_field_page *f4_fh_page;
static unsigned f4_fh_frame,f4_fh_phase,f4_fh_target;
static int f4_fh_io_error;
static int f4_fh_parse(char *line,unsigned v[4]) {
    char *p=line,*end;
    for(unsigned i=0;i<4;i++) {
        while(isspace((unsigned char)*p))p++;if(*p=='-')return -1;
        errno=0;unsigned long n=strtoul(p,&end,10);
        if(errno || p==end || n>UINT_MAX || (i<3 && !isspace((unsigned char)*end)))return -1;v[i]=(unsigned)n;p=end;
    }
    while(isspace((unsigned char)*p))p++;
    return *p || !((v[0]==0 && v[1]==2 && !v[2] && v[3]==2) || (v[0]==2 && !v[1] && v[2]==1 && v[3]==2) ||
        (v[0]==4 && v[1]==2 && !v[2] && v[3]==2) || (v[0]==7 && !v[1] && v[2]==1 && v[3]==2) ||
        (v[0]==8 && v[1]==2 && !v[2] && v[3]==3) || (v[0]==9 && !v[1] && v[2]==1 && v[3]==1))?-1:0;
}
static int f4_fh_segment(unsigned phase,uint32_t tape[512],size_t *n,int8_t jitter[96],size_t *impact) {
    char line[8192],*p,*end;unsigned v[3];
    if(!fgets(line,sizeof(line),stdin) || !strchr(line,'\n'))return -1;p=line;
    for(unsigned i=0;i<3;i++) {
        while(isspace((unsigned char)*p))p++;if(*p=='-')return -1;errno=0;
        unsigned long x=strtoul(p,&end,10);if(errno || p==end || !isspace((unsigned char)*end) || x>UINT_MAX)return -1;
        v[i]=(unsigned)x;p=end;
    }
    if(v[0]!=phase || v[1]>512 || v[2]>96 || (phase!=1 && v[2]))return -1;
    *n=v[1];*impact=v[2];
    for(size_t i=0;i<*n+*impact;i++) {
        while(isspace((unsigned char)*p))p++;errno=0;long x=strtol(p,&end,10);
        if(errno || p==end || (*end && !isspace((unsigned char)*end)))return -1;
        if(i<*n){if(x<0 || x>65535)return -1;tape[i]=(uint32_t)x;}
        else {if(x< -1 || x>1)return -1;jitter[i-*n]=(int8_t)x;}p=end;
    }
    while(isspace((unsigned char)*p))p++;return *p?-1:0;
}
static void f4_fh_packet(fd2_vga *vga,const char *kind,int status) {
    int live=f4_fh_ctx->magic_exchange!=NULL;
    const fd2_battle_magic_exchange *e=live?f4_fh_ctx->magic_exchange:f4_fh_last;
    if(f4_fh_io_error || !e)return;
    if(printf("{\"schema\":\"fd2-f4-battle-history-state-v1\",\"case\":%u,\"kind\":\"%s\",\"phase\":%u,\"part\":-1,\"frame\":%u,\"target\":%zu,\"next_part\":%zu,\"markers\":%u,\"idle_part\":%u,\"idle_ticks\":%u,\"shake\":%d,\"tint\":%d,\"caster_mp\":%u,\"active_hp\":%u,\"cast_done\":%s,\"started\":%s,\"failed\":%s,\"status\":%d,\"ticks\":%u,\"delays\":%u,\"cues\":%u,\"last_cue\":%zu,\"pumps\":%u,\"payload_bytes\":%u,\"shake_sign\":%d,\"background_flip\":%u,\"secondary_cues\":%u,\"stops\":%u,\"returns\":%u,\"owner_live\":%s}\n",
       f4_bh_case,kind,f4_fh_phase,f4_fh_frame,e->next,e->next_part,e->markers,e->idle_part,e->idle_ticks,e->shake_phase,e->tint_phase,
       e->caster.mp,e->units[f4_fh_target].hp,e->cast_done?"true":"false",e->started?"true":"false",f4_fh_ctx->failed?"true":"false",status,
       ticks,delays,cues,last_cue,input_pumps,F4_FH_PAYLOAD,e->shake_sign,e->background_flip,secondary_cues,primary_stops,runtime_calls,live?"true":"false")<0 ||
       fwrite(e->work,1,172800,stdout)!=172800 || fwrite(vga->framebuffer,1,64000,stdout)!=64000 ||
       fwrite(e->board.framebuffer,1,64000,stdout)!=64000 || fwrite(f4_fh_page->pixels,1,FD2_FIELD_PAGE_SIZE,stdout)!=FD2_FIELD_PAGE_SIZE ||
       fwrite(vga->palette,1,768,stdout)!=768 || fwrite(vga->dac,1,768,stdout)!=768 || fflush(stdout))f4_fh_io_error=1;
    if(!strcmp(kind,"upload")){if(live)*f4_fh_last=*e;f4_fh_frame++;}
}
static void f4_fh_present(fd2_vga *vga){f4_fh_packet(vga,"upload",0);}
static int f4_fh_one(const fd2_archive *other,const fd2_archive *figani,const fd2_archive *bg,const fd2_archive *tai,const unsigned v[4]) {
    fd2_battle_animation ctx={0};fd2_pcm_player primary={0},secondary={0};
    fd2_battle_magic_exchange *e=calloc(1,sizeof(*e)),*last=calloc(1,sizeof(*last));
    fd2_vga *vb=malloc(sizeof(screen));fd2_field_game *gb=malloc(sizeof(game));
    fd2_field_page *page=fd2_field_page_create(),*pb=malloc(sizeof(*pb));int status=-1;
    if(!e || !last || !vb || !gb || !page || !pb)goto done;
    if(fd2_field_visuals_open(&game.visuals,other))goto done;
    reset_observer();input_pumps=0;last_cue=0;fake_ms=1000;f4_fh_frame=f4_fh_phase=f4_fh_target=0;f4_fh_io_error=0;
    ctx.vga=&screen;ctx.visual_owner=&game;ctx.fdother=other;ctx.figani=figani;ctx.primary=&primary;ctx.secondary=&secondary;
    ctx.backgrounds=*bg;ctx.terrain_images=*tai;ctx.magic_exchange=e;f4_fh_ctx=&ctx;f4_fh_last=last;f4_fh_page=page;
    game.units.count=v[3]+1;actor(&game.units.items[0],9,(uint8_t)v[1],1);
    static const uint8_t ids[3]={0,96,30};
    for(unsigned i=0;i<v[3];i++)actor(&game.units.items[i+1],ids[i],v[1]?0:2,(int)i+2);
    for(unsigned i=0;i<=v[3];i++){game.units.items[i].hp=game.units.items[i].hp_max=42;game.units.items[i].mp=game.units.items[i].mp_max=999;}
    game.ready=1;game.failure=0;game.stage=0;game.option_values[1]=game.option_values[3]=1;game.ordinary_display_mode=0;
    game.visual_owner=&screen.field_visual;game.attack_rng=forbidden_rng;game.attack_rng_userdata=&business_rng_calls;
    e->id=(uint8_t)v[0];e->mode=FD2_FIELD_MAGIC_MODE_FULL;e->action_kind=FD2_FIELD_MAGIC_ACTION_MAGIC;
    e->count=e->effect_count=v[3];e->caster_index=0;e->caster=game.units.items[0];e->sfx_enabled=1;
    if(fd2_magic_fx_parameters_get(e->id,e->caster.side,&e->fx_parameters) || clip_open_visual(&e->casting,figani,28) ||
       clip_open_visual(&e->caster_idle,figani,27) || clip_open_visual(&e->particles,other,e->fx_parameters.effect_entry) ||
       fd2_pcm_bank_open(&e->sound,other,e->fx_parameters.sfx_bank) || image_entry(&e->bg,bg,3) || image_entry(&e->terrain,tai,3))goto done;
    for(size_t i=0;i<e->particles.clip.part_count;i++)e->part_durations[i]=e->particles.parts[i].part.duration;
    fd2_magic_fx_parts fp={e->part_durations,e->particles.clip.part_count};if(fd2_magic_fx_init(&e->fx,e->id,e->caster.side,fp))goto done;
    for(unsigned i=0;i<v[3];i++){e->targets[i]=(uint8_t)(i+1);e->units[i]=game.units.items[i+1];if(clip_open_visual(&e->target_idle[i],figani,3u*ids[i]))goto done;}
    e->shake_phase=3;e->shake_sign=-1;e->tint_phase=8;memset(e->board.framebuffer,0,64000);
    if(magic_summary(e,&game,&e->caster,0) || magic_summary(e,&game,&e->units[0],1))goto done;
    blit(e->board.framebuffer,320,200,320,&e->bg,0,50,-1);
    f4_phys_seed(e->work,172800,23,640,41,(int)v[2]);f4_phys_seed(screen.framebuffer,64000,19,320,61,(int)v[2]);
    f4_phys_seed(page->pixels,FD2_FIELD_PAGE_SIZE,17,456,83,(int)v[2]);
    const uint8_t *palette;size_t ps;if(fd2_archive_get(other,0,&palette,&ps) || ps<768)goto done;
    memcpy(screen.palette,palette,768);for(unsigned i=0;i<768;i++)screen.dac[i]=(uint8_t)((palette[i]<<2)|(palette[i]>>4));
    memcpy(e->map,screen.framebuffer,64000);memcpy(e->palette,screen.palette,768);memcpy(e->dac,screen.dac,768);
    screen.field_page=page;screen.frame_deadline_ns=UINT64_C(999999999999);screen.frame_interval_ns=UINT64_C(23000000);
    unsigned markers=0;for(size_t i=0;i<e->casting.clip.intro_end;i++)markers+=e->casting.parts[i].part.effect_marker==1;
    const uint8_t *record=fd2_field_magic_record_get(e->id);if(!record || markers!=1)goto done;
    fd2_field_unit caster=game.units.items[0];game.units.items[0].mp=(uint16_t)(999u-record[5]);
    fd2_field_magic_present_event event={0};event.magic_id=e->id;event.actor_index=0;event.action_kind=FD2_FIELD_MAGIC_ACTION_MAGIC;
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_FULL;event.targets=e->targets;event.target_count=event.effect_target_count=v[3];event.cast_cost_count=1;
    event.actor_before=&caster;expected_primary=&primary;expected_secondary=&secondary;*gb=game;*vb=screen;*pb=*page;
    f4_promotion_observe=f4_fh_present;
    for(unsigned segment=0;segment<v[3]+2;segment++) {
        uint32_t tape[512];int8_t jitter[96];size_t count,impacts;unsigned phase=segment?segment<=v[3]?1u:2u:0u;
        if(f4_fh_segment(phase,tape,&count,jitter,&impacts))goto done;
        event.fx_random=tape;event.fx_random_count=count;event.impact_count=impacts;
        for(size_t i=0;i<impacts;i++)event.impact_jitter[i]=jitter[i];
        f4_fh_phase=phase;
        fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=42;result.hp_after=28;result.raw_damage=14;result.signed_damage=14;
        if(!phase)event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        else if(phase==1){f4_fh_target=segment-1;event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=segment;event.target_before=&game.units.items[segment];event.result=&result;}
        else {event.phase=FD2_FIELD_MAGIC_PRESENT_FINISH;event.target_before=NULL;event.result=NULL;}
        if(fd2_battle_animation_magic_present(&game,&event,&ctx) || memcmp(gb,&game,sizeof(game)))goto done;
        f4_fh_packet(&screen,phase==0?"cast_end":phase==1?"target_end":"finish_end",0);
    }
    if(ctx.magic_exchange || ctx.failed || ctx.host_quit || ctx.skip || business_rng_calls || f4_fh_io_error ||
       frames!=f4_fh_frame || input_pumps!=frames || primary_stops!=1 || wrong_stop)goto done;
    memcpy(vb->framebuffer,screen.framebuffer,64000);memcpy(vb->palette,screen.palette,768);memcpy(vb->dac,screen.dac,768);
    memcpy(pb->pixels,page->pixels,FD2_FIELD_PAGE_SIZE);
    if(memcmp(vb,&screen,sizeof(screen)) || memcmp(pb,page,sizeof(*page)))goto done;
    if(printf("{\"case_end\":%u,\"frames\":%u,\"status\":0,\"failed\":false,\"skip\":false,\"other_owners_unchanged\":true}\n",f4_bh_case,frames)<0 || fflush(stdout))goto done;
    status=0;
done:
    f4_promotion_observe=NULL;if(ctx.magic_exchange)magic_close_exchange(&ctx);else if(!ctx.vga)free(e);
    fd2_field_visuals_close(&game.visuals);screen.field_page=NULL;fd2_field_page_destroy(page);
    free(pb);free(vb);free(gb);free(last);f4_fh_ctx=NULL;f4_fh_last=NULL;f4_fh_page=NULL;return status;
}
static int f4_full_magic_history_cli(const char *root){return f4_history_loaded_cli(root,f4_fh_parse,f4_fh_one);}
#endif
