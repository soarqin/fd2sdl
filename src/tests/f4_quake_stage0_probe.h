/* 私有21548实际三页入口：给定合法stage0 Load与idle，stdout-only。
 * 原1f558临时owner是64000B；snapshot App viewport与最终VGA另按原copy连接。
 * 不包含expected、不建窗口、不写Save、不改产品clock或等待。 */
#ifndef FD2_F4_QUAKE_STAGE0_PROBE_H
#define FD2_F4_QUAKE_STAGE0_PROBE_H
#include "save.h"

static uint32_t f4_quake_rng(void *data) {(*(unsigned *)data)++;return 0;}
static int f4_quake_blob(const char *key,const uint8_t *data,size_t size) {
    if(printf(",\"%s\":\"",key)<0 || f4_page_writer_hex(data,size)!=0 ||
            fputc('"',stdout)==EOF)return -1;
    return 0;
}
static int f4_quake_stage0_probe(const char *fixture) {
    fd2_archive other={0};fd2_save_file save={0};fd2_save_battle_snapshot snapshot;
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*vga_before=malloc(sizeof(*vga_before));
    fd2_field_effect_frames effects={0};unsigned rng=0;int status=1;
    uint8_t *page_before=malloc(FD2_FIELD_PAGE_SIZE),*view=malloc(312u*192u);
    uint8_t *saved_field=NULL,*saved_shape=NULL,*saved_icon=NULL,*saved_plain=NULL;
    uint32_t saved_cells[24u*24u];
    if(!game || !before || !vga || !vga_before || !page_before || !view ||
            fd2_archive_open(&other,"original_game/FDOTHER.DAT")!=0 ||
            fd2_save_file_open(&save,fixture)!=0 ||
            fd2_save_file_get_battle_snapshot(&save,&snapshot)!=0 ||
            fd2_field_game_open_resources(game,&other,0)!=0 ||
            fd2_field_game_import_battle_snapshot(game,&snapshot)!=0)goto done;
    if(game->stage || game->map.width!=24 || game->map.height!=24 || game->units.count!=12 ||
            game->camera_cell_x!=5 || game->camera_cell_y!=11 || save.size!=FD2_SAVE_FILE_SIZE)goto done;
    for(size_t raw=0;raw<12u;raw++) {
        const fd2_field_unit *u=&game->units.items[raw];
        if(u->direction || u->frame_phase || u->attack_status || u->defense_status ||
                u->hit_evasion_status || u->detail_status[0] || u->detail_status[1] || u->detail_status[2])goto done;
    }
    const uint8_t *palette;size_t palette_size;
    if(fd2_archive_get(&other,0,&palette,&palette_size)!=0 || palette_size!=768u)goto done;
    fd2_vga_set_palette(vga,palette);
    vga->field_page=fd2_field_page_create();fd2_input_init(&vga->input);
    if(!vga->field_page || fd2_field_game_bind_visual(game,vga)!=0)goto done;
    game->attack_rng=f4_quake_rng;game->attack_rng_userdata=&rng;
    saved_field=malloc(game->field_archive.size);saved_shape=malloc(game->terrain.sheet.size);
    saved_icon=malloc(game->sprites.size);saved_plain=malloc(save.size);
    if(!saved_field || !saved_shape || !saved_icon || !saved_plain)goto done;
    memcpy(saved_field,game->field_archive.data,game->field_archive.size);
    memcpy(saved_shape,game->terrain.sheet.data,game->terrain.sheet.size);
    memcpy(saved_icon,game->sprites.data,game->sprites.size);memcpy(saved_plain,save.data,save.size);
    memcpy(saved_cells,game->map.cells,sizeof(saved_cells));
    /* 同一已加载plain取得snapshot并import；不是打印另一次打开的文件冒充实际输入。 */
    if(fputs("{\"schema\":\"fd2-f4-quake-stage0-v1\"",stdout)==EOF ||
            f4_quake_blob("fixture_plain",save.data,save.size)!=0 ||
            f4_quake_blob("field",game->field_archive.data,game->field_archive.size)!=0 ||
            f4_quake_blob("shape",game->terrain.sheet.data,game->terrain.sheet.size)!=0 ||
            f4_quake_blob("icon",game->sprites.data,game->sprites.size)!=0 ||
            fputs("}\n",stdout)==EOF)goto done;
    for(unsigned idle=0;idle<3u;idle++) {
        for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)page_before[i]=(uint8_t)(53u+i*13u+i/456u);
        if(fd2_field_page_write(vga->field_page,0,0,456,336,page_before,FD2_FIELD_PAGE_SIZE,456)!=0)goto done;
        for(size_t i=0;i<64000u;i++)vga->framebuffer[i]=(uint8_t)(91u+i*17u+i/320u);
        game->idle_phase=(uint8_t)idle;vga->field_visual.actor_idle_phase=idle;
        *before=*game;*vga_before=*vga;
        /* Shipping Battle magic_map_quake也使用此begin入口；不调用play/上传/等待。 */
        if(fd2_field_game_begin_earthquake(game,vga,&effects)!=0 || rng ||
                memcmp(game,before,sizeof(*game)) || memcmp(game->map.cells,saved_cells,sizeof(saved_cells)))goto done;
        fd2_vga observed=*vga;
        observed.actor_jitter_tick=vga_before->actor_jitter_tick;
        observed.actor_jitter_phase=vga_before->actor_jitter_phase;
        if(memcmp(&observed,vga_before,sizeof(observed)) ||
                !effects.page_snapshots[0] || !effects.page_snapshots[1] || !effects.page_snapshots[2] ||
                memcmp(vga->field_page->pixels,effects.page_snapshots[0]->pixels,FD2_FIELD_PAGE_SIZE))goto done;
        for(unsigned q=0;q<3u;q++) {
            if(fd2_field_page_read(effects.page_snapshots[q],72,72,312,192,view,312u*192u,312)!=0 ||
                    printf("{\"case\":%u,\"q\":%u,\"idle\":%u,\"rng\":%u,\"game_unchanged\":true,\"input_palette_vga_unchanged\":true,\"jitter_only\":true,\"App0_committed\":true",
                        idle*3u+q,q,idle,rng)<0 ||
                    f4_quake_blob("vga",effects.snapshots[q],64000)!=0 ||
                    f4_quake_blob("app_view",view,312u*192u)!=0 ||
                    f4_quake_blob("palette",vga->palette,768)!=0 ||
                    f4_quake_blob("dac",vga->dac,768)!=0 ||
                    (!q && f4_quake_blob("App0",vga->field_page->pixels,FD2_FIELD_PAGE_SIZE)!=0) ||
                    fputs("}\n",stdout)==EOF)goto done;
        }
        fd2_field_effect_frames_close(&effects);
    }
    if(memcmp(saved_field,game->field_archive.data,game->field_archive.size) ||
            memcmp(saved_shape,game->terrain.sheet.data,game->terrain.sheet.size) ||
            memcmp(saved_icon,game->sprites.data,game->sprites.size) || memcmp(saved_plain,save.data,save.size) ||
            fputs("{\"end\":9,\"loaded_binding_unchanged\":true}\n",stdout)==EOF || fflush(stdout) || ferror(stdout))goto done;
    status=0;
done:
    if(status)fputs("quake-stage0 actual probe failed\n",stderr);
    fd2_field_effect_frames_close(&effects);fd2_field_game_close(game);
    if(vga)fd2_field_page_destroy(vga->field_page);
    fd2_save_file_close(&save);fd2_archive_close(&other);
    free(saved_field);free(saved_shape);free(saved_icon);free(saved_plain);
    free(view);free(page_before);free(vga_before);free(vga);free(before);free(game);return status;
}
#endif
