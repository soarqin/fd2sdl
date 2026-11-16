/* DERIVED完整CAST/part/marker/idle历史；私有stdout probe，无shipping API。
 * 2cf30/30e9d/2e9a8/2f4d4/2f631，给定已定义128000B work-before；
 * 原palette/FIGANI/BG/TAI/summary reader均真实执行，业务提交是明确输入。 */
#ifndef FD2_F4_BATTLE_HISTORY_PROBE_H
#define FD2_F4_BATTLE_HISTORY_PROBE_H
#include <limits.h>
enum { F4_BH_PAYLOAD=302336 };
static fd2_battle_animation *f4_bh_context;
static unsigned f4_bh_case,f4_bh_frame,f4_bh_phase;
static int f4_bh_part,f4_bh_io_error;
static unsigned f4_bh_caster(unsigned id) {
    return id==24?32u:id==28?19u:id==29?34u:id==30?18u:32u;
}
static int f4_bh_parse(char *line,unsigned v[4]) {
    char *p=line,*end;
    for(unsigned i=0;i<4;i++) {
        while(isspace((unsigned char)*p))p++;
        if(*p=='-')return -1;
        errno=0;unsigned long value=strtoul(p,&end,10);
        if(errno || p==end || value>UINT_MAX || (i<3 && !isspace((unsigned char)*end)))return -1;
        v[i]=(unsigned)value;p=end;
    }
    while(isspace((unsigned char)*p))p++;
    return *p || (v[0]!=24 && (v[0]<28 || v[0]>31)) || (v[1]!=0 && v[1]!=2) ||
        v[2]>1 || v[3]>2 || (v[3]==1 && v[0]!=28) || (v[3]==2 && v[0]!=24)?-1:0;
}
static void f4_bh_packet(fd2_vga *vga,const char *kind,int status) {
    fd2_battle_magic_exchange *e=f4_bh_context->magic_exchange;
    if(f4_bh_io_error || !e)return;
    unsigned hp=e->next<e->count?e->units[e->next].hp:0u;
    if(printf("{\"schema\":\"fd2-f4-battle-history-state-v1\",\"case\":%u,\"kind\":\"%s\",\"phase\":%u,\"part\":%d,\"frame\":%u,\"target\":%zu,\"next_part\":%zu,\"markers\":%u,\"idle_part\":%u,\"idle_ticks\":%u,\"shake\":%d,\"tint\":%d,\"caster_mp\":%u,\"active_hp\":%u,\"cast_done\":%s,\"started\":%s,\"failed\":%s,\"status\":%d,\"ticks\":%u,\"delays\":%u,\"cues\":%u,\"last_cue\":%zu,\"pumps\":%u,\"payload_bytes\":%u}\n",
        f4_bh_case,kind,f4_bh_phase,f4_bh_part,f4_bh_frame,e->next,e->next_part,
        e->markers,e->idle_part,e->idle_ticks,e->shake_phase,e->tint_phase,e->caster.mp,hp,
        e->cast_done?"true":"false",e->started?"true":"false",f4_bh_context->failed?"true":"false",status,
        ticks,delays,cues,last_cue,input_pumps,F4_BH_PAYLOAD)<0 ||
       fwrite(e->work,1,172800,stdout)!=172800 || fwrite(vga->framebuffer,1,64000,stdout)!=64000 ||
       fwrite(e->board.framebuffer,1,64000,stdout)!=64000 || fwrite(vga->palette,1,768,stdout)!=768 ||
       fwrite(vga->dac,1,768,stdout)!=768 || fflush(stdout))f4_bh_io_error=1;
    if(!strcmp(kind,"upload"))f4_bh_frame++;
}
static void f4_bh_present(fd2_vga *vga) {f4_bh_packet(vga,"upload",0);}
static int f4_bh_one(const fd2_archive *other,const fd2_archive *figani,
    const fd2_archive *bg,const fd2_archive *tai,const unsigned v[4]) {
    fd2_battle_animation ctx={0};fd2_pcm_player primary={0};
    fd2_battle_magic_exchange *e=calloc(1,sizeof(*e)),*saved=malloc(sizeof(*e));
    fd2_field_game *gb=malloc(sizeof(game));fd2_vga *vb=malloc(sizeof(screen));int status=-1,render_status=0;
    if(!e || !saved || !gb || !vb)goto done;
    reset_observer();input_pumps=0;last_cue=0;fake_ms=1000;f4_bh_frame=f4_bh_phase=0;f4_bh_part=-1;f4_bh_io_error=0;
    ctx.vga=&screen;ctx.fdother=other;ctx.figani=figani;ctx.visual_owner=&game;ctx.primary=&primary;
    ctx.backgrounds=*bg;ctx.terrain_images=*tai;ctx.magic_exchange=e;f4_bh_context=&ctx;
    game.units.count=3;actor(&game.units.items[0],(uint8_t)f4_bh_caster(v[0]),(uint8_t)v[1],1);
    actor(&game.units.items[1],0,v[1]?0:2,2);actor(&game.units.items[2],96,v[1]?0:2,3);
    for(unsigned i=0;i<3;i++) {
        game.units.items[i].hp=1;game.units.items[i].hp_max=42;
        game.units.items[i].mp=game.units.items[i].mp_max=999;
    }
    game.ready=1;game.failure=0;game.stage=0;game.option_values[1]=game.option_values[3]=1;
    game.visual_owner=&screen.field_visual;game.attack_rng=forbidden_rng;game.attack_rng_userdata=&business_rng_calls;
    e->id=(uint8_t)v[0];e->mode=FD2_FIELD_MAGIC_MODE_DERIVED;e->action_kind=FD2_FIELD_MAGIC_ACTION_MAGIC;
    e->count=e->effect_count=2;e->targets[0]=1;e->targets[1]=2;e->caster_index=0;e->sfx_enabled=1;
    e->caster=game.units.items[0];for(unsigned i=0;i<2;i++)e->units[i]=game.units.items[i+1];
    if(clip_open(&e->casting,&ctx,3u*e->caster.unit_id+2u,1) ||
       clip_open_visual(&e->caster_idle,figani,3u*e->caster.unit_id))goto done;
    for(unsigned i=0;i<2;i++)if(clip_open_visual(&e->target_idle[i],figani,3u*e->units[i].unit_id))goto done;
    if(image_entry(&e->bg,bg,v[0]==28?4u:3u) || image_entry(&e->target_bg,bg,4u) ||
       image_entry(&e->terrain,tai,3u))goto done;
    for(unsigned i=0;i<3;i++)if(image_entry(&e->transition[i],bg,i))goto done;
    e->next_part=e->casting.clip.intro_end;e->shake_phase=0;e->tint_phase=-1;
    memset(e->board.framebuffer,0,64000);
    if(magic_summary(e,&game,&e->caster,0) || (v[0]==28 && magic_summary(e,&game,&e->units[0],1)))goto done;
    blit(e->board.framebuffer,320,200,320,&e->bg,0,50,-1);
    f4_phys_seed(e->work,172800,23,640,41,(int)v[2]);
    f4_phys_seed(screen.framebuffer,64000,19,320,61,(int)v[2]);
    const uint8_t *palette;size_t palette_size;
    if(fd2_archive_get(other,0,&palette,&palette_size) || palette_size<768)goto done;
    memcpy(screen.palette,palette,768);
    for(unsigned i=0;i<768;i++)screen.dac[i]=(uint8_t)((palette[i]<<2)|(palette[i]>>4));
    screen.field_page=NULL;screen.frame_deadline_ns=UINT64_C(999999999999);screen.frame_interval_ns=UINT64_C(23000000);
    unsigned cost_markers=0,total_markers=0;
    for(size_t i=0;i<e->casting.clip.part_count;i++)if(e->casting.parts[i].part.effect_marker==1) {
        if(i<e->casting.clip.intro_end)cost_markers++;else total_markers++;
    }
    if(!total_markers)goto done;
    const uint8_t *record=fd2_field_magic_record_get((uint8_t)v[0]);
    if(!record || cost_markers*(unsigned)record[5]>999u)goto done;
    fd2_field_unit initial_caster=game.units.items[0];
    game.units.items[0].mp=(uint16_t)(999u-cost_markers*(unsigned)record[5]);
    fd2_field_magic_present_event event={0};event.magic_id=(uint8_t)v[0];event.actor_index=0;
    event.action_kind=FD2_FIELD_MAGIC_ACTION_MAGIC;event.presentation_mode=FD2_FIELD_MAGIC_MODE_DERIVED;
    event.targets=e->targets;event.target_count=2;event.effect_target_count=2;event.cast_cost_count=(uint8_t)cost_markers;
    event.actor_before=&initial_caster;event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    if(v[3]==1)ctx.skip=1;
    *saved=*e;*vb=screen;*gb=game;expected_primary=&primary;f4_promotion_observe=f4_bh_present;
    if(fd2_battle_animation_magic_present(&game,&event,&ctx) || memcmp(gb,&game,sizeof(game)))goto done;
    f4_bh_packet(&screen,"cast_end",0);
    for(size_t target=0;target<2;target++) {
        fd2_field_unit before=game.units.items[target+1];unsigned markers=0;
        for(size_t part=e->casting.clip.intro_end;part<e->casting.clip.part_count;part++) {
            if(e->casting.parts[part].part.effect_marker==1)markers++;
            fd2_field_unit committed=game.units.items[target+1];
            committed.hp=(uint16_t)(markers==total_markers?0u:1u);
            if(v[3]==2 && !target && markers)committed.hp=32767;
            game.units.items[target+1]=committed;*gb=game;
            event.phase=FD2_FIELD_MAGIC_PRESENT_STEP;event.target_index=target+1;
            event.target_before=&before;event.target_after=&committed;event.part_index=(uint16_t)part;event.marker_count=(uint8_t)markers;
            f4_bh_phase=1;f4_bh_part=(int)part;
            render_status=fd2_battle_animation_magic_present(&game,&event,&ctx);
            if(memcmp(gb,&game,sizeof(game)))goto done;
            f4_bh_packet(&screen,render_status?"part_error":"part_end",render_status);
            if(render_status)goto final_state;
        }
        fd2_field_magic_basic_result result={0};result.hit=1;result.signed_damage=1;
        event.result=&result;event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;
        *gb=game;f4_bh_phase=2;
        if(fd2_battle_animation_magic_present(&game,&event,&ctx) || memcmp(gb,&game,sizeof(game)))goto done;
        f4_bh_packet(&screen,"target_end",0);
    }
final_state:
    f4_promotion_observe=NULL;f4_bh_phase=3;f4_bh_packet(&screen,"final",render_status);
    if(f4_bh_io_error || business_rng_calls || primary_stops || ctx.host_quit || frames!=f4_bh_frame || input_pumps!=frames ||
       (v[3]==2 ? render_status!=-1 || !ctx.failed : render_status || ctx.failed))goto done;
    memcpy(saved->work,e->work,128000);memcpy(saved->board.framebuffer,e->board.framebuffer,64000);
    saved->caster=e->caster;for(unsigned i=0;i<2;i++)saved->units[i]=e->units[i];
    saved->next=e->next;saved->next_part=e->next_part;saved->markers=e->markers;
    saved->idle_part=e->idle_part;saved->idle_ticks=e->idle_ticks;saved->shake_phase=e->shake_phase;saved->tint_phase=e->tint_phase;
    saved->cast_done=e->cast_done;saved->started=e->started;
    memcpy(vb->framebuffer,screen.framebuffer,64000);memcpy(vb->dac,screen.dac,768);
    if(memcmp(saved,e,sizeof(*e)) || memcmp(vb,&screen,sizeof(screen)))goto done;
    if(printf("{\"case_end\":%u,\"frames\":%u,\"status\":%d,\"failed\":%s,\"skip\":%s,\"other_owners_unchanged\":true}\n",
        f4_bh_case,frames,render_status,ctx.failed?"true":"false",ctx.skip?"true":"false")<0 || fflush(stdout))goto done;
    status=0;
done:
    f4_promotion_observe=NULL;
    if(ctx.magic_exchange)magic_close_exchange(&ctx);else free(e);
    free(saved);free(gb);free(vb);f4_bh_context=NULL;return status;
}
/* 两个有限历史入口只共用同buffer加载握手；各自验证固定参数/绘制。 */
static int f4_history_loaded_cli(const char *root,int (*parse)(char *,unsigned[4]),
    int (*one)(const fd2_archive *,const fd2_archive *,const fd2_archive *,const fd2_archive *,const unsigned[4])) {
    fd2_archive bg={0},tai={0};char path[1024],line[128];unsigned v[4];int status=1;
    const char *names[5]={"FDOTHER.DAT","FDTXT.DAT","FIGANI.DAT","BG.DAT","TAI.DAT"};
    fd2_archive *archives[5]={&fdother,&fdtxt,&figani_fixture,&bg,&tai};uint8_t *saved[5]={0};
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)<0)return 1;
#endif
    for(unsigned i=0;i<5;i++) {
        if(f4_path(path,root,names[i]) || fd2_archive_open(archives[i],path) || !(saved[i]=malloc(archives[i]->size)))goto done;
        memcpy(saved[i],archives[i]->data,archives[i]->size);
        if(f4_input_receipt(names[i],archives[i]->data,archives[i]->size))goto done;
    }
    if(fd2_field_info_assets_open(&game.info_assets,&fdother) || fd2_font_open_fdother(&game.font,&fdother,4) ||
       fd2_text_entry_open_entry(&game.ui_text,&fdtxt,0))goto done;
    for(f4_bh_case=0;fgets(line,sizeof(line),stdin);f4_bh_case++)
        if(f4_bh_case>=64 || !strchr(line,'\n') || parse(line,v) || one(&fdother,&figani_fixture,&bg,&tai,v))goto done;
    if(ferror(stdin) || !f4_bh_case)goto done;
    for(unsigned i=0;i<5;i++)if(memcmp(saved[i],archives[i]->data,archives[i]->size))goto done;
    if(printf("{\"end\":%u,\"binding_unchanged\":true}\n",f4_bh_case)<0 || fflush(stdout))goto done;
    status=0;
done:
    fd2_text_entry_close(&game.ui_text);memset(&game.font,0,sizeof(game.font));fd2_field_info_assets_close(&game.info_assets);
    for(unsigned i=0;i<5;i++){free(saved[i]);fd2_archive_close(archives[i]);}
    if(status)fputs("F4 battle history: request/source/output rejected\n",stderr);
    return status;
}
static int f4_battle_history_cli(const char *root) {return f4_history_loaded_cli(root,f4_bh_parse,f4_bh_one);}
#endif
