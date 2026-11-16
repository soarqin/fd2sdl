/* 私有MAP/Item整历史；magic_map_frames真实执行，terrain/refresh是给定页输入。
 * 这两个adapter不冒11cac/11eee实时读钟；原透明LMI/FDICON与页写者照常调用。
 * 只stdout；真实FDOTHER/FDICON加载buffer在握手及末尾逐byte绑定。 */
#ifndef FD2_F4_MAP_ITEM_HISTORY_PROBE_H
#define FD2_F4_MAP_ITEM_HISTORY_PROBE_H
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "../field_page_private.h"
enum { F4_MAP_HISTORY_PAGE=153216, F4_MAP_HISTORY_PAYLOAD=608768 };
static unsigned f4_maph_case,f4_maph_frame,f4_maph_refreshes;
static int f4_maph_io_error;
static fd2_battle_animation *f4_maph_context;
static fd2_field_page *f4_maph_terrain;
static int f4_maph_parse(char *line,unsigned v[6]) {
    char *p=line,*end;
    for(unsigned i=0;i<6;i++) {
        while(isspace((unsigned char)*p))p++;
        if(*p=='-')return -1;
        errno=0;unsigned long value=strtoul(p,&end,10);
        if(errno || p==end || value>UINT_MAX || (i<5 && !isspace((unsigned char)*end)))return -1;
        v[i]=(unsigned)value;p=end;
    }
    while(isspace((unsigned char)*p))p++;
    if(*p || v[0]>32 || v[0]==23 || v[2]>3 || v[3]>4 || v[4]>1 || v[5]>4)return -1;
    if(v[1] && !((v[1]==20 && v[0]==4) || (v[1]==21 && v[0]==0) || (v[1]==24 && v[0]==11)))return -1;
    return 0;
}
static int f4_maph_refresh(fd2_field_game *current,fd2_vga *vga) {
    if(current!=&game || vga!=&screen || !vga->field_page || !f4_maph_terrain)return -1;
    f4_maph_refreshes++;
    uint8_t row[312];
    for(unsigned y=0;y<192;y++) {
        for(unsigned x=0;x<312;x++)row[x]=(uint8_t)(40u+(x*7u+y*11u+f4_maph_refreshes*3u)%173u);
        if(fd2_field_page_write(vga->field_page,72,72+(int)y,312,1,row,sizeof(row),312))return -1;
    }
    if(fd2_field_page_copy_pixels(f4_maph_terrain,vga->field_page))return -1;
    return fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,64000);
}
static int f4_maph_terrain_input(fd2_field_game *current,fd2_vga *vga,fd2_field_page *page) {
    if(current!=&game || vga!=&screen || !page || !f4_maph_terrain)return -1;
    return fd2_field_page_copy_pixels(page,f4_maph_terrain);
}
static void f4_maph_packet(fd2_vga *vga,int final) {
    fd2_battle_magic_exchange *e=f4_maph_context->magic_exchange;
    if(f4_maph_io_error || !e || !vga->field_page || !e->map_base)return;
    if(printf("{\"schema\":\"fd2-f4-map-history-state-v1\",\"case\":%u,\"final\":%s,\"frame\":%u,\"ticks\":%u,\"delays\":%u,\"cues\":%u,\"last_cue\":%zu,\"pumps\":%u,\"refreshes\":%u,\"payload_bytes\":%u}\n",
        f4_maph_case,final?"true":"false",f4_maph_frame,ticks,delays,cues,last_cue,input_pumps,
        f4_maph_refreshes,F4_MAP_HISTORY_PAYLOAD)<0 ||
       fwrite(vga->field_page->pixels,1,F4_MAP_HISTORY_PAGE,stdout)!=F4_MAP_HISTORY_PAGE ||
       fwrite(vga->framebuffer,1,64000,stdout)!=64000 || fwrite(e->board.framebuffer,1,64000,stdout)!=64000 ||
       fwrite(e->work,1,172800,stdout)!=172800 ||
       fwrite(e->map_base->pixels,1,F4_MAP_HISTORY_PAGE,stdout)!=F4_MAP_HISTORY_PAGE ||
       fwrite(vga->palette,1,768,stdout)!=768 || fwrite(vga->dac,1,768,stdout)!=768 || fflush(stdout))f4_maph_io_error=1;
    if(!final)f4_maph_frame++;
}
static void f4_maph_present(fd2_vga *vga) {f4_maph_packet(vga,0);}
static void f4_maph_units(unsigned layout) {
    static const int positions[5][3][2]={
        {{4,3},{6,4},{8,4}},{{0,0},{-1,-1},{0,0}},
        {{12,8},{13,9},{12,8}},{{6,4},{6,4},{6,4}},{{-2,0},{6,4},{8,4}}};
    static const uint8_t ids[3]={0,30,96};
    memset(&game.units,0,sizeof(game.units));game.units.count=3;
    game.camera_cell_x=game.camera_cell_y=8;
    for(unsigned i=0;i<3;i++) {
        actor(&game.units.items[i],ids[i],i?2:0,8+positions[layout][i][0]);
        game.units.items[i].y=(uint8_t)(8+positions[layout][i][1]);
        game.units.items[i].direction=3;game.units.items[i].frame_phase=6;
        game.units.items[i].flags=FD2_FIELD_UNIT_FLAG_ACTED;
    }
    if(layout==4)game.units.items[2].flags|=FD2_FIELD_UNIT_FLAG_HIDDEN;
}
static int f4_maph_one(const fd2_archive *other,const unsigned v[6]) {
    fd2_battle_animation ctx={0};fd2_pcm_player primary={0};
    fd2_battle_magic_exchange *e=calloc(1,sizeof(*e)),*ebefore=malloc(sizeof(*e));
    fd2_field_game *gbefore=malloc(sizeof(game));fd2_vga *vbefore=malloc(sizeof(screen));
    fd2_field_page *page=fd2_field_page_create();int status=-1;
    if(!e || !ebefore || !gbefore || !vbefore || !page)goto done;
    reset_observer();input_pumps=0;last_cue=0;fake_ms=1000;
    f4_maph_context=&ctx;f4_maph_refreshes=f4_maph_frame=0;f4_maph_io_error=0;
    f4_maph_terrain=fd2_field_page_create();if(!f4_maph_terrain)goto done;
    f4_maph_units(v[3]);if(fd2_field_units_sprite_cache_restore(&game.units))goto done;
    for(size_t i=0;i<game.units.count;i++)game.units.items[i].sprite_cache_class=game.units.sprite_cache_classes[i];
    game.ready=1;game.failure=0;game.idle_phase=(uint8_t)v[2];game.visual_owner=&screen.field_visual;
    game.option_values[1]=game.option_values[3]=1; /* 同enabled source fixture，保原raw owner。 */
    game.attack_rng=forbidden_rng;game.attack_rng_userdata=&business_rng_calls;
    for(size_t i=0;i<F4_MAP_HISTORY_PAGE;i++)page->pixels[i]=(uint8_t)((i*17u+i/456u+83u)^(v[4]?255u:0u));
    f4_phys_seed(screen.framebuffer,64000,19,320,61,(int)v[4]);
    f4_phys_seed(e->board.framebuffer,64000,13,320,31,(int)v[4]);
    f4_phys_seed(e->work,172800,23,640,41,(int)v[4]);
    for(unsigned i=0;i<768;i++){screen.palette[i]=(uint8_t)(i*7u%64u);screen.dac[i]=(uint8_t)(i*11u+29u);}
    screen.field_page=page;screen.input.quit_requested=0;
    screen.frame_deadline_ns=UINT64_C(999999999999);screen.frame_interval_ns=UINT64_C(23000000);
    ctx.vga=&screen;ctx.visual_owner=&game;ctx.fdother=other;ctx.primary=&primary;ctx.magic_exchange=e;
    e->mode=FD2_FIELD_MAGIC_MODE_MAP;e->id=e->effect_id=(uint8_t)v[0];
    e->action_kind=v[1]?FD2_FIELD_MAGIC_ACTION_ITEM:FD2_FIELD_MAGIC_ACTION_MAGIC;e->item_code=(uint8_t)v[1];
    e->count=2;e->targets[0]=2;e->targets[1]=1;e->sfx_enabled=1;
    for(unsigned i=0;i<2;i++) {
        e->units[i]=game.units.items[e->targets[i]];
        e->sprite_ids[i]=(uint8_t)fd2_field_unit_cached_sprite_id(&game.units,e->targets[i]);
    }
    for(unsigned i=0;i<k_battle_map_fx_count[v[0]];i++) {
        size_t frame=(size_t)k_battle_map_fx_start[v[0]]+i;
        if(frame>=256 || lmi_image(&e->map_fx[frame],other,6,frame))goto done;
    }
    if(magic_map_actor_style(e,(uint8_t)v[0])==MAP_ACTORS_DUOTONE)
        for(size_t i=74;i<=75;i++)if(!e->map_fx[i].image.pixels && lmi_image(&e->map_fx[i],other,6,i))goto done;
    if(fd2_pcm_bank_open(&e->map_sound,other,80))goto done;
    *ebefore=*e;*gbefore=game;*vbefore=screen;
    expected_primary=&primary;
    if(v[5]==1)quit_frame=1;
    if(v[5]==4)quit_frame=(unsigned)k_battle_map_fx_count[v[0]]+2u;
    if(v[5]==2)ctx.skip=1;
    if(v[5]==3)screen.input.quit_requested=1;
    f4_map_refresh_override=f4_maph_refresh;f4_map_terrain_override=f4_maph_terrain_input;
    f4_promotion_observe=f4_maph_present;
    if(magic_map_frames(&game,&ctx,(uint8_t)v[0]) || f4_maph_io_error)goto done;
    f4_promotion_observe=NULL;f4_maph_packet(&screen,1);
    if(f4_maph_io_error || memcmp(&game,gbefore,sizeof(game)) || business_rng_calls || primary_stops ||
       ctx.failed || frames!=f4_maph_frame || input_pumps!=frames ||
       (v[5]==1 && (!ctx.host_quit || frames!=1)) || (v[5]==2 && (ctx.host_quit || frames)) ||
       (v[5]==3 && (!ctx.host_quit || frames)) ||
       (v[5]==4 && (!ctx.host_quit || frames!=(unsigned)k_battle_map_fx_count[v[0]]+2u)) ||
       (!v[5] && ctx.host_quit))goto done;
    memcpy(ebefore->board.framebuffer,e->board.framebuffer,64000);
    memcpy(ebefore->work,e->work,64000);ebefore->map_base=e->map_base;
    memcpy(vbefore->framebuffer,screen.framebuffer,64000);vbefore->input.quit_requested=screen.input.quit_requested;
    if(memcmp(ebefore,e,sizeof(*e)) || memcmp(vbefore,&screen,sizeof(screen)) ||
       memcmp(&page->background,&f4_maph_terrain->background,sizeof(page->background)))goto done;
    if(printf("{\"case_end\":%u,\"frames\":%u,\"host\":%s,\"skip\":%s,\"ticks\":%u,\"delays\":%u,\"cues\":%u,\"pumps\":%u,\"refreshes\":%u,\"other_owners_unchanged\":true}\n",
        f4_maph_case,frames,ctx.host_quit?"true":"false",ctx.skip?"true":"false",ticks,delays,cues,input_pumps,
        f4_maph_refreshes)<0 || fflush(stdout))goto done;
    status=0;
done:
    f4_promotion_observe=NULL;f4_map_refresh_override=NULL;f4_map_terrain_override=NULL;
    if(ctx.magic_exchange)magic_close_exchange(&ctx);else free(e);
    fd2_field_page_destroy(f4_maph_terrain);f4_maph_terrain=NULL;screen.field_page=NULL;
    fd2_field_page_destroy(page);free(ebefore);free(gbefore);free(vbefore);f4_maph_context=NULL;
    return status;
}
static int f4_map_history_cli(const char *root) {
    fd2_archive other={0};uint8_t *old_other=NULL,*old_icon=NULL;char path[1024],line[128];unsigned v[6];int status=1;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)<0)return 1;
#endif
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&other,path) ||
       f4_path(path,root,"FDICON.B24") || fd2_map_sprite_bank_open(&game.sprites,path) ||
       !(old_other=malloc(other.size)) || !(old_icon=malloc(game.sprites.size)))goto done;
    memcpy(old_other,other.data,other.size);memcpy(old_icon,game.sprites.data,game.sprites.size);
    if(f4_input_receipt("FDOTHER.DAT",other.data,other.size) ||
       f4_input_receipt("FDICON.B24",game.sprites.data,game.sprites.size))goto done;
    for(f4_maph_case=0;fgets(line,sizeof(line),stdin);f4_maph_case++)
        if(f4_maph_case>=128 || !strchr(line,'\n') || f4_maph_parse(line,v) || f4_maph_one(&other,v))goto done;
    if(ferror(stdin) || !f4_maph_case || memcmp(other.data,old_other,other.size) ||
       memcmp(game.sprites.data,old_icon,game.sprites.size))goto done;
    if(printf("{\"end\":%u,\"binding_unchanged\":true}\n",f4_maph_case)<0 || fflush(stdout))goto done;
    status=0;
done:
    free(old_other);free(old_icon);fd2_archive_close(&other);fd2_map_sprite_bank_close(&game.sprites);
    if(status)fputs("F4 map history: request/source/output rejected\n",stderr);
    return status;
}
#endif
