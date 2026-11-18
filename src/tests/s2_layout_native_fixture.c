/* S2非shipping胜利2机制：sub_230F2 @0x230f2 / sub_233C6 @0x233c6。
 * 生产stage2资源与公开SAV导入；ENTER显式消费唯一source冷图，不开放stage绑定。
 * s2_layout_game只供owned PDB读取；完成后保持现帧，不运行stage3/复读原版旧owner。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_rng.h"
#include "save.h"
#include "scene.h"
#include "static_data.h"

fd2_field_game s2_layout_game;
volatile unsigned s2_layout_phase;

static int load(const fd2_archive *other,const char *path) {
    fd2_save_file file={0};fd2_save_battle_snapshot snapshot;
    fd2_static_flow_node story={0};story.op="story";story.args[0]=7;
    int result=-1;
    if(fd2_save_file_open(&file,path)!=0 || fd2_save_file_get_battle_snapshot(&file,&snapshot)!=0)goto done;
    if(snapshot.meta[FD2_SAVE_BATTLE_META_STAGE]!=2 || snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT]!=15)goto done;
    for(size_t raw=0;raw<15;raw++) {
        const uint8_t *unit=snapshot.unit_area+raw*80;
        if((!(unit[5]&1))!=(raw==0 || raw==6))goto done;
        if((raw==0 || raw==6) && (unit[6]!=2 || !(unit[64] || unit[65])))goto done;
    }
    if(fd2_field_game_open_resources(&s2_layout_game,other,2)!=0 ||
       fd2_field_game_import_battle_snapshot(&s2_layout_game,&snapshot)!=0 ||
       !fd2_static_flow_lookup("stage","stage.source.victory.002") ||
       fd2_scene_common_step_validate(&s2_layout_game,other,&story)!=0)goto done;
    result=0;
done:
    fd2_save_file_close(&file);return result;
}

int main(int argc,char **argv) {
    int check=argc==3 && !strcmp(argv[1],"--check");
    if(argc!=3 || (!check && strcmp(argv[1],"--save"))) {
        fprintf(stderr,"usage: s2_layout_native_fixture --check|--save controlled.SAV\n");return 1;
    }
    fd2_archive other={0};SDL_Window *window=NULL;SDL_Renderer *renderer=NULL;fd2_vga *vga=NULL;fd2_field_rng rng;int result=1;
    if(fd2_archive_open(&other,"original_game/FDOTHER.DAT")!=0 || load(&other,argv[2])!=0)goto done;
    /* 明确机制seed；文本mouth/shared phase借现Game RNG，不签原版RNG整轨。 */
    fd2_field_rng_seed(&rng,0x7a18);
    if(fd2_field_game_set_attack_hooks(&s2_layout_game,NULL,NULL,NULL,NULL,fd2_field_rng_next,&rng)!=0)goto done;
    if(check) {
        printf("{\"scope\":\"source-only victory2 layout\",\"stage\":2,\"count\":15,\"roster\":%zu,\"story7_resources_validated\":true,\"window_started\":false}\n",s2_layout_game.roster_count);
        result=0;goto done;
    }
    if(!SDL_Init(SDL_INIT_VIDEO))goto done;
    window=SDL_CreateWindow("FD2 S2 Layout Source",960,600,0);if(!window)goto done;
    renderer=SDL_CreateRenderer(window,NULL);if(!renderer)goto done;
    (void)SDL_SetRenderVSync(renderer,0);
    (void)SDL_SetRenderLogicalPresentation(renderer,VGA_W,VGA_H,SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    vga=calloc(1,sizeof(*vga));if(!vga || fd2_vga_init(vga,window,renderer)!=0 || fd2_field_game_bind_visual(&s2_layout_game,vga)!=0)goto done;
    const uint8_t *palette;size_t size;
    if(fd2_archive_get(&other,0,&palette,&size)!=0 || size<768)goto done;
    fd2_vga_set_palette(vga,palette);fd2_field_game_render(&s2_layout_game,vga);
    printf("S2 layout source ready: stage2/raw15; ENTER explicitly starts stage.source.victory.002; no stage binding/audio claim\n");fflush(stdout);
    for(;;) {
        if(fd2_input_take_quit(&vga->input)){result=0;break;}
        fd2_input_event event;
        if(!s2_layout_phase && fd2_input_take_key(&vga->input,&event) && event.key==FD2_INPUT_KEY_ENTER) {
            s2_layout_phase=1;
            int played=fd2_scene_play_stage_source_flow(vga,&other,&s2_layout_game,NULL,0,"stage.source.victory.002");
            if(played==FD2_SCENE_RESULT_HOST_QUIT){result=0;break;}
            if(played!=0 || s2_layout_game.stage!=3 || s2_layout_game.units.count!=15 || s2_layout_game.roster_count!=5)goto done;
            s2_layout_phase=2;
            printf("S2 layout source done: stage3/roster5; no stage3 controller or owner handoff executed\n");fflush(stdout);
        }
        if(!s2_layout_phase)fd2_field_game_render(&s2_layout_game,vga);
        fd2_vga_present_timed(vga,16);
    }
done:
    if(result)fprintf(stderr,"S2 layout source fixture failed: %s\n",SDL_GetError());
    fd2_field_game_close(&s2_layout_game);
    if(vga){fd2_vga_close(vga);free(vga);}
    fd2_archive_close(&other);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
