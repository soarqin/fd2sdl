/* 活动Load sub_10010 @0x10010、field_turn_cycle_run @0x1a30b
 * （原FUN_00017a53）的共同Round横幅。sub_15F0E @0x15f0e保存矩形并
 * 经sub_4EBAB @0x4ebab零透明LMI绘制；sub_15E71 @0x15e71恢复。
 * runtime.round保83..91/数字/等待/下落/尾刷新顺序。本消费者只投影，
 * 业务提交、曲目和312x192原工作页均属于调用方，不建立第二page owner。 */
/* 阵营来源：field_phase_title_enter @0x1f1cc（原sub_1F1CC）、
 * field_phase_title_leave @0x1f30a（原sub_1F30A）、
 * field_phase_title_slide_frame @0x1f42d（原sub_1F42D）及
 * field_phase_mosaic_visible_copy @0x4eb59（原sub_4EB59）。
 * 完整指令与1a30b/两个标题caller核对；不登记Hex-Rays猜测ABI。 */
#include "field_round.h"

#include <stdlib.h>
#include <string.h>

#include "map_sprite.h"
#include "static_data.h"

static int valid_image(const fd2_image *image, int width, int height) {
    return image->pixels && image->width == width && image->height == height;
}

int fd2_field_round_is_ready(const fd2_field_info_assets *assets) {
    if (!assets || !assets->ready) return 0;
    for (size_t i = 0; i < 9u; i++) {
        const fd2_image *image = &assets->round_banner[i];
        if (!image->pixels || image->width < 1 || image->width > VGA_W ||
            image->height < 1 || image->height > VGA_H) return 0;
    }
    for (size_t i = 0; i < 10u; i++)
        if (!valid_image(&assets->digits[1][i], i == 1u ? 5 : 6, 8)) return 0;
    return valid_image(&assets->digit_overflow[1], 18, 8);
}

static int node_valid(const fd2_static_flow_node *node) {
    if (!node || !(node->views & FD2_FLOW_PRESENT)) return 0;
    const int32_t *a = node->args;
    if (!strcmp(node->op, "round_frame"))
        return a[0] >= 83 && a[0] <= 91 && a[1] >= 0 && a[1] < VGA_W && a[2] >= 0 && a[2] < VGA_H;
    if (!strcmp(node->op, "round_number"))
        return a[0] >= 0 && a[0] <= VGA_W - 18 && a[1] >= 0 && a[1] <= VGA_H - 8;
    if (!strcmp(node->op, "round_wait_ms")) return a[0] > 0 && a[0] <= 1000;
    if (!strcmp(node->op, "round_wait_ticks")) return a[0] == 1;
    if (!strcmp(node->op, "round_drop"))
        return a[0] == 91 && a[1] >= 0 && a[1] < VGA_W && a[2] >= 0 && a[2] < VGA_H &&
            a[3] >= 0 && a[3] <= VGA_W - 18 && a[4] >= 0 && a[4] <= VGA_H - 8;
    return !strcmp(node->op, "round_restore_vga") || !strcmp(node->op, "round_map_background") ||
        !strcmp(node->op, "round_refresh") || !strcmp(node->op, "round_finish");
}

/* 确定性无分支显示图先走完资源/有限op/边界门，损坏定义不得画半帧。 */
static int flow_ready(const fd2_static_flow *flow) {
    if (!flow || !flow->count || flow->count > 254u) return 0;
    int pc = flow->entry, map_ready = 0;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, pc);
        if (!node_valid(node)) return 0;
        if (!strcmp(node->op, "round_map_background")) map_ready = 1;
        if (!strcmp(node->op, "round_drop") && !map_ready) return 0;
        if (!strcmp(node->op, "round_finish")) return 1;
        pc = fd2_static_flow_edge(flow, node, FD2_FLOW_NEXT);
        if (pc < 0) return 0;
    }
    return 0;
}

static void visible_copy(uint8_t *dst, const uint8_t *src) {
    for (int y = 4; y < VGA_H - 4; y++)
        memcpy(dst + (size_t)y * VGA_STRIDE + 4u,
               src + (size_t)y * VGA_STRIDE + 4u, VGA_W - 8u);
}

static void restore_border(uint8_t *dst, const uint8_t *saved) {
    for (int y = 0; y < VGA_H; y++) {
        if (y < 4 || y >= VGA_H - 4) memcpy(dst + (size_t)y * VGA_STRIDE, saved + (size_t)y * VGA_STRIDE, VGA_W);
        else {
            memcpy(dst + (size_t)y * VGA_STRIDE, saved + (size_t)y * VGA_STRIDE, 4u);
            memcpy(dst + (size_t)y * VGA_STRIDE + VGA_W - 4u, saved + (size_t)y * VGA_STRIDE + VGA_W - 4u, 4u);
        }
    }
}

int fd2_field_round_play(fd2_vga *vga, const fd2_field_info_assets *assets,
        uint32_t turn, const fd2_field_round_hooks *hooks, void *userdata) {
    const fd2_static_flow *flow = fd2_static_flow_lookup("runtime", "round");
    if (!vga || !fd2_field_round_is_ready(assets) || !hooks || !hooks->wait ||
        !hooks->render_background || !hooks->refresh || !flow_ready(flow)) return -1;
    uint8_t *saved = malloc(2u * VGA_W * VGA_H);
    if (!saved) return -1;
    uint8_t *map = saved + VGA_W * VGA_H;
    memcpy(saved, vga->framebuffer, VGA_W * VGA_H);
    int map_ready = 0, result = -1, pc = flow->entry;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, pc);
        const int32_t *a = node->args;
        if (!strcmp(node->op, "round_frame")) {
            fd2_map_sprite_blit(vga, &assets->round_banner[a[0] - 83], a[1], a[2], 0);
        } else if (!strcmp(node->op, "round_number")) {
            if (fd2_field_info_number_draw(vga, assets, a[0], a[1], (int32_t)turn, 3u, 1u) != 0) break;
        } else if (!strcmp(node->op, "round_wait_ms") || !strcmp(node->op, "round_wait_ticks")) {
            int ticks = !strcmp(node->op, "round_wait_ticks");
            int wait = hooks->wait(vga, ticks ? 0u : (uint32_t)a[0], ticks ? (unsigned)a[0] : 0u, userdata);
            if (wait != 0) { result = wait > 0 ? 1 : -1; break; }
        } else if (!strcmp(node->op, "round_restore_vga")) {
            memcpy(vga->framebuffer, saved, VGA_W * VGA_H);
        } else if (!strcmp(node->op, "round_map_background")) {
            int background = hooks->render_background(vga, userdata);
            if (background != 0) { result = background > 0 ? 1 : -1; break; }
            memcpy(map, vga->framebuffer, VGA_W * VGA_H); map_ready = 1;
        } else if (!strcmp(node->op, "round_drop")) {
            if (!map_ready) break;
            visible_copy(vga->framebuffer, map);
            fd2_map_sprite_blit(vga, &assets->round_banner[a[0] - 83], a[1], a[2], 0);
            if (fd2_field_info_number_draw(vga, assets, a[3], a[4], (int32_t)turn, 3u, 1u) != 0) break;
            /* 原copy只覆盖312x192；零透明sprite不得改变外框。 */
            restore_border(vga->framebuffer, saved);
        } else if (!strcmp(node->op, "round_refresh")) {
            int refresh = hooks->refresh(vga, userdata);
            if (refresh != 0) { result = refresh > 0 ? 1 : -1; break; }
        } else if (!strcmp(node->op, "round_finish")) { result = 0; break; }
        else break;
        pc = fd2_static_flow_edge(flow, node, FD2_FLOW_NEXT);
        if (pc < 0) break;
    }
    free(saved);
    return result;
}

int fd2_field_phase_is_ready(const fd2_field_info_assets *assets) {
    if (!assets || !assets->ready) return 0;
    for (size_t i = 0; i < 3u; i++) {
        const fd2_image *image = &assets->phase_labels[i];
        if (!image->pixels || image->width < 1 || image->width > VGA_W ||
            image->height < 1 || image->height > VGA_H) return 0;
    }
    return 1;
}

static int phase_flow_ready(const fd2_static_flow *flow, int entry) {
    if (!flow || !flow->count || flow->count > 254u) return 0;
    int pc=entry,parameters=0,map=0,captured=0;
    for (unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if (!node || !(node->views & FD2_FLOW_PRESENT)) return 0;
        const int32_t *a=node->args;
        if (!strcmp(node->op,"phase_parameters")) {
            if ((a[0]!=80 && a[0]!=82) || a[1]<1 || a[1]>1000) return 0;
            parameters=1;
        } else if (!strcmp(node->op,"round_map_background")) map=1;
        else if (!strcmp(node->op,"phase_capture_prefix")) captured=1;
        else if (!strcmp(node->op,"phase_slide") || !strcmp(node->op,"phase_mosaic")) {
            int mosaic=!strcmp(node->op,"phase_mosaic");
            if (!parameters || !map || (mosaic && !captured) || a[0]<(mosaic?1:0) ||
                a[0]>(mosaic?17:100) || a[1]<0 || a[1]>=VGA_W || a[2]<0 || a[2]>=VGA_W ||
                a[3]<0 || a[3]>=VGA_H || a[4]!=81) return 0;
        } else if (!strcmp(node->op,"phase_darkness")) {
            if(a[0]!=16 || a[1]!=255 || a[2]<0 || a[2]>16)return 0;
        } else if (!strcmp(node->op,"phase_hold")) { if(!parameters)return 0; }
        else if (!strcmp(node->op,"phase_redraw") || !strcmp(node->op,"phase_clear_keys")) {}
        else if (!strcmp(node->op,"round_wait_ticks")) { if(a[0]!=1)return 0; }
        else if (!strcmp(node->op,"round_finish")) return 1;
        else return 0;
        pc=fd2_static_flow_edge(flow,node,FD2_FLOW_NEXT);
        if(pc<0)return 0;
    }
    return 0;
}

int fd2_field_phase_play(fd2_vga *vga, const fd2_field_info_assets *assets,
        fd2_field_page *page, fd2_field_phase_kind kind,
        const fd2_field_round_hooks *hooks, void *userdata) {
    const fd2_static_flow *flow=fd2_static_flow_lookup("runtime","phase");
    int entry=kind==FD2_FIELD_PHASE_PLAYER?fd2_static_flow_label(flow,"player"):
        kind==FD2_FIELD_PHASE_ENEMY?fd2_static_flow_label(flow,"enemy"):-1;
    if(!vga || !page || !fd2_field_phase_is_ready(assets) || !hooks || !hooks->wait ||
        !hooks->render_background || !hooks->phase_redraw || !hooks->clear_keys ||
        !phase_flow_ready(flow,entry))return -1;
    uint8_t *scope=malloc(3u*VGA_W*VGA_H);
    if(!scope)return -1;
    uint8_t *map=scope+VGA_W*VGA_H,*prefix=map+VGA_W*VGA_H;
    memcpy(scope,vga->framebuffer,VGA_W*VGA_H);
    int pc=entry,result=-1,label=0;uint32_t hold=0;
    for(unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);const int32_t *a=node->args;
        if(!strcmp(node->op,"phase_parameters")) {label=a[0]-80;hold=(uint32_t)a[1];}
        else if(!strcmp(node->op,"round_map_background")) {
            int read=hooks->render_background(vga,userdata);
            if(read){result=read>0?1:-1;break;}
            memcpy(map,vga->framebuffer,VGA_W*VGA_H);
        } else if(!strcmp(node->op,"phase_slide")) {
            visible_copy(vga->framebuffer,map);
            fd2_map_sprite_blit(vga,&assets->phase_labels[label],a[1]-a[0],a[3],0);
            fd2_map_sprite_blit(vga,&assets->phase_labels[a[4]-80],a[2]+a[0],a[3],0);
            restore_border(vga->framebuffer,map);
        } else if(!strcmp(node->op,"phase_capture_prefix")) {
            /* field_phase_title_enter @0x1f1cc（原sub_1F1CC）明确memmove(53a49,saved_VGA,64000)。
             * 此为有证page raw-prefix借用，不是一般UI→工作页反灌规则。 */
            if(fd2_field_page_phase_prefix_write(page,scope,VGA_W*VGA_H)!=0 ||
                fd2_field_page_phase_prefix_read(page,prefix,VGA_W*VGA_H)!=0)break;
        } else if(!strcmp(node->op,"phase_mosaic")) {
            /* field_phase_mosaic_visible_copy @0x4eb59（原sub_4EB59）从320stride源(4,4)以BX块宽复制312x192；
             * DL/DH每满factor才跳源，16bit MUL低字在合法1..17域不溢出。 */
            unsigned factor=(unsigned)a[0];memset(vga->framebuffer,0,VGA_W*VGA_H);
            for(unsigned y=0;y<192u;y++)for(unsigned x=0;x<312u;x++)
                vga->framebuffer[(y+4u)*VGA_STRIDE+x+4u]=
                    prefix[(4u+y/factor*factor)*VGA_STRIDE+4u+x/factor*factor];
            fd2_map_sprite_blit(vga,&assets->phase_labels[label],a[1],a[3],0);
            fd2_map_sprite_blit(vga,&assets->phase_labels[a[4]-80],a[2],a[3],0);
        } else if(!strcmp(node->op,"phase_darkness")) {
            /* dac_apply_darkness @0x11d40：6bit通道减暗度，不是比例缩放。
             * 原只写palette16..255；前16DAC、base palette与App水clock保留。 */
            for(int i=a[0]*3;i<(a[1]+1)*3;i++) {
                uint8_t value=(uint8_t)(vga->palette[i]>a[2]?vga->palette[i]-a[2]:0);
                vga->dac[i]=(uint8_t)((value<<2)|(value>>4));
            }
        } else if(!strcmp(node->op,"round_wait_ticks") || !strcmp(node->op,"phase_hold")) {
            int ticks=!strcmp(node->op,"round_wait_ticks");
            int waited=hooks->wait(vga,ticks?0u:hold,ticks?(unsigned)a[0]:0u,userdata);
            if(waited){result=waited>0?1:-1;break;}
        } else if(!strcmp(node->op,"phase_clear_keys")) {
            int clear=hooks->clear_keys(vga,userdata);if(clear){result=clear>0?1:-1;break;}
        } else if(!strcmp(node->op,"phase_redraw")) {
            int redraw=hooks->phase_redraw(vga,userdata);if(redraw){result=redraw>0?1:-1;break;}
        } else if(!strcmp(node->op,"round_finish")){result=0;break;}
        else break;
        pc=fd2_static_flow_edge(flow,node,FD2_FLOW_NEXT);if(pc<0)break;
    }
    free(scope);return result;
}

/* field_turn_cycle_run @0x1a30b（原FUN_00017a53）：只解释该函数已经证实
 * 的有限领域次序；状态提交、演员循环和原progress续接仍在原Game owner。 */
static int turn_node(const fd2_static_flow_node *node,
        fd2_field_turn_step *step) {
    static const char *const names[] = {
        "turn_recovery_wait", "turn_recovery_highlight", "turn_recovery_cue",
        "turn_recovery_commit", "turn_recovery_project", "turn_refresh",
        "turn_stop_if_different", "turn_enemy_title", "turn_player_title",
        "turn_clear_acted", "turn_enter_phase", "turn_increment",
        "turn_display_mode", "turn_primary_music", "turn_round",
        "turn_focus_reset", "turn_focus_zero", "turn_clear_keys"
    };
    if (!node || !step) return -1;
    for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) {
        if (strcmp(node->op,names[i])) continue;
        int value=node->args[0];
        if (i==FD2_FIELD_TURN_RECOVERY_WAIT || i==FD2_FIELD_TURN_DISPLAY_MODE) {
            if (value<0 || value>1) return -1;
        } else if (i==FD2_FIELD_TURN_ENTER_PHASE) {
            if (value<0 || value>2) return -1;
        } else if (value!=0) return -1;
        *step=(fd2_field_turn_step)i;return 0;
    }
    return -1;
}

static int turn_entry_node(const fd2_static_flow *flow,
        fd2_field_turn_entry entry) {
    static const char *const labels[]={"player_done","side1_done","side0_done","player_ready"};
    if (!flow || (unsigned)entry>=sizeof(labels)/sizeof(labels[0])) return -1;
    for (size_t i=0;i<flow->count;i++)
        if (!strcmp(flow->nodes[i].id,labels[entry])) return (int)i;
    return -1;
}

int fd2_field_turn_run(fd2_field_turn_entry entry,
        fd2_field_turn_step_fn apply, void *userdata) {
    const fd2_static_flow *flow=fd2_static_flow_lookup("runtime","turncycle");
    int first=turn_entry_node(flow,entry);
    if (!apply || first<0 || flow->count>254u) return -1;
    /* 完整入口先验，不因坏的后继定义执行半次HP/回合提交。 */
    int pc=first,ready=0;
    uint8_t seen[254]={0};
    for (unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if (!node || seen[pc]) return -1;
        seen[pc]=1;
        if (!strcmp(node->op,"turn_done")) {ready=1;break;}
        fd2_field_turn_step step;
        if (turn_node(node,&step)!=0) return -1;
        pc=fd2_static_flow_edge(flow,node,FD2_FLOW_NEXT);
    }
    if (!ready) return -1;
    pc=first;
    for (unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if (!strcmp(node->op,"turn_done")) return 0;
        fd2_field_turn_step step;
        if (turn_node(node,&step)!=0) return -1;
        int result=apply(step,node->args[0],userdata);
        if (result) return result>0?1:-1;
        pc=fd2_static_flow_edge(flow,node,FD2_FLOW_NEXT);
    }
    return -1;
}

static int choice_node(const fd2_static_flow_node *node,fd2_field_choice_step *step) {
    static const char *const names[]={"choice_begin","choice_draw","choice_wait_key",
        "choice_read_key","choice_tick","choice_finish","choice_snapshot",
        "choice_has_field","choice_field_redraw","choice_blink","choice_steady"};
    if (!node) return -1;
    for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) if (!strcmp(node->op,names[i])) {
        if (i==FD2_FIELD_CHOICE_DRAW || i==FD2_FIELD_CHOICE_STEADY) {
            if (node->args[0]<0 || node->args[0]>16 || node->args[0]%4) return -1;
            if (i==FD2_FIELD_CHOICE_STEADY && node->args[0]!=16) return -1;
        } else if (node->args[0]) return -1;
        *step=(fd2_field_choice_step)i;return 0;
    }
    return -1;
}

/* field_choice_buttons_wait @0x19953（原sub_19953）和
 * field_choice_buttons_close @0x197e5（原sub_197E5）的共用冷次序。
 * 共享epilogue仍属原函数owner，不据伪C添加新参数或返回类型。 */
int fd2_field_choice_run(fd2_field_choice_step_fn apply,void *userdata) {
    const fd2_static_flow *flow=fd2_static_flow_lookup("runtime","choice");
    if (!flow || !apply || !flow->count || flow->count>254u) return -1;
    for (size_t i=0;i<flow->count;i++) {
        fd2_field_choice_step step;
        if (strcmp(flow->nodes[i].op,"choice_done") && choice_node(&flow->nodes[i],&step)!=0) return -1;
    }
    int pc=flow->entry;
    /* 这里是真正输入等待，不能拿有限事件图的损坏预算当用户超时。各次
     * 等待由host callback采帧/quit；定义边另由构建期完整引用校验。 */
    for (;;) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if (!node) return -1;
        if (!strcmp(node->op,"choice_done")) return 0;
        fd2_field_choice_step step;
        if (choice_node(node,&step)!=0) return -1;
        int result=apply(step,node->args[0],userdata);
        if (result<0 || result==3) return result==3?1:-1;
        unsigned port=FD2_FLOW_NEXT;
        if (step==FD2_FIELD_CHOICE_WAIT_KEY || step==FD2_FIELD_CHOICE_TICK ||
                step==FD2_FIELD_CHOICE_HAS_FIELD) {
            if (result>1) return -1;
            port=result?FD2_FLOW_YES:FD2_FLOW_NO;
        } else if (step==FD2_FIELD_CHOICE_READ_KEY) {
            if (result>2) return -1;
            port=result==1?FD2_FLOW_ACK:result==2?FD2_FLOW_CANCEL:FD2_FLOW_NEXT;
        } else if (result) return -1;
        pc=fd2_static_flow_edge(flow,node,port);
    }
}
