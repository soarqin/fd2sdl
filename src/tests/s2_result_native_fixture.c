/* S2非shipping20bf5结果机制窗口：source-only Game+真实common Play/Scene。
 * 只读受控SAV；ENTER显式启动源图，不模拟自然stage29/controller调度。
 * 单一Game符号仅供PDB只读owner观察，不新增生产热状态/存档字段。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_play.c"

int fd2_s2_result_native_open(fd2_field_game *,const fd2_archive *,const char *);
fd2_field_game s2_result_game;

static int source_begin(fd2_field_game *game,const fd2_archive *other,const char *flow) {
    game->event_preflight=common_event_resources_preflight;
    game->event_preflight_userdata=(void *)other;
    return fd2_field_game_common_event_begin(game,flow,0);
}

int main(int argc,char **argv) {
    int check=argc>=3 && !strcmp(argv[1],"--check");
    const char *flow=argc==5 && !strcmp(argv[3],"--flow")?argv[4]:"common.result.source_020bf5";
    if((argc!=3 && argc!=5) || (argc==5 && strcmp(argv[3],"--flow")) || (!check && strcmp(argv[1],"--save")) ||
       (strcmp(flow,"common.result.source_020bf5") && strcmp(flow,"common.callback.047"))) {
        fprintf(stderr,"usage: s2_result_native_fixture --check|--save controlled.SAV [--flow common.callback.047]\n");return 1;
    }
    fd2_archive other={0};fd2_vga *vga=NULL;SDL_Window *window=NULL;SDL_Renderer *renderer=NULL;
    fd2_field_rng rng;int result=1;
    if(fd2_archive_open(&other,"original_game/FDOTHER.DAT")!=0 ||
       fd2_s2_result_native_open(&s2_result_game,&other,argv[2])!=0)goto done;
    if((s2_result_game.stage==29)!=!strcmp(flow,"common.result.source_020bf5"))goto done;
    fd2_field_rng_seed(&rng,0x7a18);
    if(fd2_field_game_set_attack_hooks(&s2_result_game,NULL,NULL,NULL,NULL,fd2_field_rng_next,&rng)!=0)goto done;
    if(check) {
        fd2_static_flow_node node;size_t actor;
        if(source_begin(&s2_result_game,&other,flow)!=0 ||
           fd2_field_game_common_event_peek(&s2_result_game,&node,&actor)!=1 ||
           actor!=0 || s2_result_game.battle_result!=0 ||
           (s2_result_game.stage==29 && (strcmp(node.op,"story") || node.args[0]!=7)))goto done;
        printf("{\"scope\":\"source-only mechanism\",\"stage\":%zu,\"count\":%zu,\"first_leaf\":\"%s\",\"first_arg\":%d,\"latch_at_leaf\":0,\"resources_validated\":true,\"window_started\":false}\n",
            s2_result_game.stage,s2_result_game.units.count,node.op,node.args[0]);
        result=0;goto done;
    }
    if(!SDL_Init(SDL_INIT_VIDEO))goto done;
    window=SDL_CreateWindow("FD2 S2 Result Source",960,600,0);if(!window)goto done;
    renderer=SDL_CreateRenderer(window,NULL);if(!renderer)goto done;
    (void)SDL_SetRenderVSync(renderer,0);
    (void)SDL_SetRenderLogicalPresentation(renderer,VGA_W,VGA_H,SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    vga=calloc(1,sizeof(*vga));if(!vga || fd2_vga_init(vga,window,renderer)!=0)goto done;
    if(fd2_field_game_bind_visual(&s2_result_game,vga)!=0)goto done;
    const uint8_t *palette;size_t size;
    if(fd2_archive_get(&other,0,&palette,&size)!=0 || size<768)goto done;
    fd2_vga_set_palette(vga,palette);
    field_runtime_presentation runtime={vga,NULL,0,0,NULL};
    s2_result_game.runtime_present=play_runtime_step;s2_result_game.runtime_present_userdata=&runtime;
    fd2_field_game_render(&s2_result_game,vga);
    printf("S2 result source ready: stage%zu/raw%zu; ENTER explicitly starts%s; no stage binding/audio comparison\n",
        s2_result_game.stage,s2_result_game.units.count,flow);fflush(stdout);
    int started=0;
    for(;;) {
        if(fd2_input_take_quit(&vga->input)){result=0;break;}
        fd2_input_event event;
        if(!started && fd2_input_take_key(&vga->input,&event) && event.key==FD2_INPUT_KEY_ENTER) {
            started=1;
            if(source_begin(&s2_result_game,&other,flow)!=0)goto done;
            int played=play_common_events(&s2_result_game,vga,&other,NULL,0);
            if(played==FD2_SCENE_RESULT_HOST_QUIT){result=0;break;}
            if(played!=0 || (s2_result_game.stage==29 && s2_result_game.battle_result!=FD2_FIELD_BATTLE_DEFEAT))goto done;
            printf("S2 result source done: result=%d; source-only graph, no result dispatcher\n",s2_result_game.battle_result);fflush(stdout);
        }
        fd2_field_game_render(&s2_result_game,vga);
        fd2_vga_present_timed(vga,16);
    }
done:
    if(result)fprintf(stderr,"S2 result source fixture failed: %s\n",SDL_GetError());
    fd2_field_game_close(&s2_result_game);
    if(vga){fd2_vga_close(vga);free(vga);}
    fd2_archive_close(&other);
    if(renderer)SDL_DestroyRenderer(renderer);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
