/* S5转场/传送受控资源夹具：104地形帧+一actor，真实Game渲染；
 * 只读原LUT/bank，临时页未写域另由各TU明确seed。非自然关卡。 */
#ifndef FD2_S5_EFFECT_FIXTURE_H
#define FD2_S5_EFFECT_FIXTURE_H

static uint32_t forbidden_rng(void *userdata) {
    (void)userdata;rng_calls++;return 0;
}
typedef struct {
    fd2_field_game *game,*before;
    fd2_vga *vga;
    uint8_t *terrain,*page_bytes;
    fd2_archive fdother;
    fd2_pcm_bank bank;
    fd2_field_audio audio;
    uint8_t actor[48];
    uint32_t actor_offsets[12],terrain_offsets[104],cells[20*19];
    fd2_terrain_attr attrs[104];
} fixture;

static int fixture_open(fixture *f) {
    memset(f,0,sizeof(*f));reset_spies();
    f->game=calloc(1,sizeof(*f->game));f->before=malloc(sizeof(*f->before));
    f->vga=calloc(1,sizeof(*f->vga));f->terrain=malloc(104*600);
    f->page_bytes=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(f->game && f->before && f->vga && f->terrain && f->page_bytes);
    CHECK(fd2_archive_open(&f->fdother,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_pcm_bank_open(&f->bank,&f->fdother,80)==0);
    fd2_field_game *g=f->game;fd2_vga *v=f->vga;
    g->fdother=&f->fdother;g->ready=1;g->stage=0;
    /* F2正式live入口借唯一App视觉owner；手工资源夹具也须满足该合同。
     * attrs无动画位且12个actor frame同源，像素不依赖实际clock相位。 */
    g->visual_owner=&v->field_visual;
    g->camera_cell_x=7;g->camera_cell_y=11;g->cursor_cell_x=11;g->cursor_cell_y=14;
    g->attack_rng=forbidden_rng;
    memset(g->roster,0x5a,sizeof(g->roster));g->roster_count=7;g->resource_value=0x12345678;
    g->map.width=20;g->map.height=19;g->map.cells=f->cells;
    for(int ty=0;ty<8;ty++)for(int tx=0;tx<13;tx++) {
        unsigned frame=(unsigned)(ty*13+tx);f->terrain_offsets[frame]=frame*600;
        f->cells[(ty+11)*20+tx+7]=frame;
        for(int y=0;y<24;y++) {
            size_t at=frame*600+(size_t)y*25;f->terrain[at++]=0x97;
            for(int x=0;x<24;x++)f->terrain[at++]=(uint8_t)(tx*24+x+3*(ty*24+y));
        }
    }
    g->terrain.sheet=(fd2_shape_sheet){24,24,104,f->terrain,104*600,f->terrain_offsets};
    g->terrain.attrs=f->attrs;g->terrain.attr_count=104;
    for(unsigned y=0;y<24;y++) {f->actor[y*2]=23;f->actor[y*2+1]=177;}
    g->sprites.data=f->actor;g->sprites.size=sizeof(f->actor);
    g->sprites.frame_count=12;g->sprites.offsets=f->actor_offsets;
    g->units.count=1;g->units.items[0].x=12;g->units.items[0].y=15;
    g->units.items[0].hp=93;g->units.items[0].side=2;
    CHECK(fd2_field_units_sprite_cache_restore(&g->units)==0);
    CHECK(fd2_field_visuals_open(&g->visuals,&f->fdother)==0);
    v->field_page=fd2_field_page_create();CHECK(v->field_page);
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)f->page_bytes[i]=(uint8_t)(i*17u+23u);
    CHECK(fd2_field_page_write(v->field_page,0,0,456,336,f->page_bytes,FD2_FIELD_PAGE_SIZE,456)==0);
    memset(v->framebuffer,0xa5,64000);
    for(unsigned i=0;i<768;i++)v->palette[i]=(uint8_t)((i*5u+13u)&63u);
    memset(v->dac,17,sizeof(v->dac));
    v->input.queued_count=2;v->input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
    v->input.queued_keys[1].key=FD2_INPUT_KEY_ESCAPE;
    f->audio.battle_bank=&f->bank;
    f->audio.player=(fd2_pcm_player *)(uintptr_t)1; /* 设备I/O仅私有spy。 */
    *f->before=*g;return 0;
}
static int logic_unchanged(fixture *f) {
    /* 11eee的terrain/range投影是明确写集；其余整个Game相同。 */
    f->before->last_terrain_visual_ms=f->game->last_terrain_visual_ms;
    f->before->terrain_visual_phase=f->game->terrain_visual_phase;
    f->before->range_visual_phase=f->game->range_visual_phase;
    CHECK(!memcmp(f->before,f->game,sizeof(*f->game)) && rng_calls==0);
    CHECK(f->vga->input.queued_count==2 && f->vga->input.queued_keys[0].key==FD2_INPUT_KEY_ENTER &&
        f->vga->input.queued_keys[1].key==FD2_INPUT_KEY_ESCAPE);
    for(unsigned i=0;i<768;i++)CHECK(f->vga->palette[i]==((i*5u+13u)&63u) && f->vga->dac[i]==17);
    return 0;
}
static void fixture_close(fixture *f) {
    if(f->game)fd2_field_visuals_close(&f->game->visuals);
    if(f->vga)fd2_field_page_destroy(f->vga->field_page);
    fd2_pcm_bank_close(&f->bank);fd2_archive_close(&f->fdother);
    free(f->game);free(f->before);free(f->vga);free(f->terrain);free(f->page_bytes);
}

#endif
