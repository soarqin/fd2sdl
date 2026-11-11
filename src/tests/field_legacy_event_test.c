/* G5 既有34531/34778正式turn入口的业务停点；只显式驱动有限叶。
 * 此无窗口测试不声称PAN/MOVE帧、文本RNG或原生输入；原函数调用/写序
 * 由独立source verifier核，真实Scene/Play取消另测。旧整段walker必须真红。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "static_data.h"

#define CHECK(x) do {if(!(x)){fprintf(stderr,"legacy:%d %s\n",__LINE__,#x);return -1;}}while(0)
static unsigned rolls;
static uint32_t next_roll(void *userdata){(void)userdata;rolls++;return 0;}

static int fixture(fd2_field_game *g,const fd2_archive *other,int stage) {
    int result;
    if(!stage) {
        fd2_vga vga={0};
        result=fd2_field_game_open(g,&vga,other,0);
    }
    else {
        fd2_save_manual_state state={0};state.stage_id=1;state.unit_count=5;
        static const uint8_t ids[]={0,9,4,30,1};
        for(size_t i=0;i<5;i++) {
            fd2_field_unit u={0};CHECK(fd2_field_unit_persistent_create(&u,ids[i])==0);
            memcpy(state.unit_area+i*80,&u,80);
        }
        result=fd2_field_game_prepare_stage1(g,other,&state);
    }
    CHECK(result==0);rolls=0;
    CHECK(fd2_field_game_set_attack_hooks(g,NULL,NULL,NULL,NULL,next_roll,NULL)==0);
    g->turn_number=3;
    int phase_result=fd2_field_game_end_active_phase(g);
    if(phase_result!=0)fprintf(stderr,"legacy fixture stage=%d phase_result=%d failure=%d units=%zu side=%u pc=%u action=%u slot=%zu\n",
        stage,phase_result,g->failure,g->units.count,g->active_side,g->event_resume.pc,
        g->failure_notice.action,g->failure_notice.slot);
    CHECK(phase_result==0);
    return 0;
}

/* 明确的无呈现驱叶，不将ACK当真实用户等待。业务仍由正式Game续态写，
 * 仅PAN/MOVE的headless逻辑采用既有已证纯原语，以抵达要比较的文本边界。 */
static int until_story(fd2_field_game *g,int fragment) {
    for(unsigned i=0;i<256;i++) {
        fd2_static_flow_node n;size_t actor;
        CHECK(fd2_field_game_common_event_peek(g,&n,&actor)==1);
        if(!strcmp(n.op,"story") && n.args[0]==fragment)return 0;
        if(!strcmp(n.op,"pan")) {
            g->camera_cell_x=n.args[0];g->camera_cell_y=n.args[1];g->ordinary_display_mode=0;
        } else if(!strcmp(n.op,"move")) {
            const uint8_t *bytes;size_t count;
            CHECK(fd2_field_movement_script_get((uint8_t)n.args[0],&bytes,&count)==0);
            CHECK(fd2_field_movement_script_apply(&g->units,bytes,count)==0);
            g->ordinary_display_mode=1;
        }
        CHECK(fd2_field_game_common_event_reply(g,!strcmp(n.op,"story")?FD2_FLOW_ACK:FD2_FLOW_NEXT)==0);
    }
    return -1;
}

static int test_original_story_write_boundaries(const fd2_archive *other) {
    for(int stage=0;stage<=1;stage++)for(int ending=0;ending<3;ending++) {
        fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
        CHECK(fixture(g,other,stage)==0);
        /* 旧bug在此失败：旧walker已经14/27且没有父PC，不能靠改最终期望掩盖。 */
        if(!(g->event_resume.active && g->active_side==1 && g->units.count==(stage?21u:13u)))
            fprintf(stderr,"legacy prefix stage=%d active=%u side=%u units=%zu failure=%d\n",
                stage,g->event_resume.active,g->active_side,g->units.count,g->failure);
        CHECK(g->event_resume.active && g->active_side==1 && g->units.count==(stage?21u:13u));
        uint8_t ai_before[12];
        for(size_t i=5;i<=10;i++){ai_before[(i-5)*2]=g->units.items[i].ai_param_35;ai_before[(i-5)*2+1]=g->units.items[i].ai_param_36;}
        CHECK(until_story(g,stage?4:11)==0);
        CHECK(g->units.count==(stage?27u:13u) && !rolls);
        for(size_t i=5;i<=10;i++)CHECK(g->units.items[i].ai_param_35==ai_before[(i-5)*2] &&
            g->units.items[i].ai_param_36==ai_before[(i-5)*2+1]);
        fd2_save_battle_snapshot out,untouched;memset(&out,0x5a,sizeof(out));untouched=out;
        CHECK(fd2_field_game_export_battle_snapshot(g,&out)==-1 && !memcmp(&out,&untouched,sizeof(out)));
        if(ending==1) {
            g->runtime_host_quit=1;fd2_field_game *before=malloc(sizeof(*before));CHECK(before);*before=*g;
            CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1 && !memcmp(g,before,sizeof(*g)) && !rolls);
            free(before);
        } else if(ending==2) {
            CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ERROR)==-1 && g->failure==FD2_FIELD_FAILURE_ACTION);
            CHECK(g->units.count==(stage?27u:13u) && !rolls);
            for(size_t i=5;i<=10;i++)CHECK(g->units.items[i].ai_param_35==ai_before[(i-5)*2] &&
                g->units.items[i].ai_param_36==ai_before[(i-5)*2+1]);
        } else {
            CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0);
            if(stage) {
                CHECK(!g->event_resume.active);
                for(size_t i=5;i<=10;i++)CHECK(g->units.items[i].ai_param_35==26 && g->units.items[i].ai_param_36==15);
            } else {
                CHECK(g->units.count==14 && g->event_resume.active && until_story(g,3)==0);
                CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0);
                fd2_static_flow_node n;size_t actor;
                CHECK(fd2_field_game_common_event_peek(g,&n,&actor)==1 && !strcmp(n.op,"delay_ms") && n.args[0]==20);
                for(size_t i=0;i<g->units.count;i++)CHECK(g->units.items[i].direction==0);
                CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==0 && !g->event_resume.active);
            }
            CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1 && !rolls);
        }
        fd2_field_game_close(g);free(g);
    }
    return 0;
}

int main(void) {
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(test_original_story_write_boundaries(&other)==0);
    fd2_archive_close(&other);puts("legacy event continuation: source-prefix checks ok; no GUI/RNG wait claim");return 0;
}
