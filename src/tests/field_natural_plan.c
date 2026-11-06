/* B5自然第一关只读策略工具：读取真实活动SAV，用现SDL合法移动/目标查询
 * 选择下一条玩家动作。它不是原版oracle，不写SAV，不推测原版RNG状态。
 * --simulate只运行独立内存Game并ACK其业务提示，排除音画/等待RNG和WGC；
 * 原版仍须逐键资源ACK、每轮真实Save重规划，不能拿预计伤害验收原版。
 * 操作依据沿已实现18890/18d8c、14818、20c6f及117e7/1a30b接口。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "field_game.h"
#include "field_attack.h"
#include "field_move.h"
#include "field_move_profile.h"
#include "field_rng.h"
#include "field_event.h"

enum { PLAN_WAIT, PLAN_ATTACK, PLAN_MAGIC, PLAN_HERB, PLAN_END };
typedef struct {
    int kind, actor, target, x, y, magic, slot, score, estimate;
} natural_action;
typedef struct { natural_action actions[8]; size_t count; } natural_choices;

static int distance(int x, int y, int tx, int ty) { return abs(x-tx)+abs(y-ty); }
static int living(const fd2_field_unit *u) { return !(u->flags & 1u) && u->hp != 0; }
static int actionable(const fd2_field_unit *u) {
    return living(u) && u->side == 2 && !(u->flags & 0x80u) && !u->detail_status[1] &&
        u->unit_id != 121 && u->race != 10;
}
static int herb_slot(const fd2_field_unit *u) {
    const uint8_t *raw = (const uint8_t *)u;
    for (int i=0;i<8;i++) if (!(raw[10+i*2]&0x80u) && raw[11+i*2]==192) return i;
    return -1;
}
static int physical_access(const fd2_field_game *g, size_t target) {
    const fd2_field_unit *u=&g->units.items[target];
    static const int dx[]={0,-1,0,1},dy[]={1,0,-1,0};
    for(int d=0;d<4;d++) {
        int x=u->x+dx[d],y=u->y+dy[d],occupied=0;
        if(x<0 || y<0 || x>=g->map.width || y>=g->map.height)continue;
        for(size_t i=0;i<g->units.count;i++)if(i!=target && living(&g->units.items[i]) &&
            g->units.items[i].x==x && g->units.items[i].y==y && g->units.items[i].side!=2)occupied=1;
        if(!occupied)return 1;
    }
    return 0;
}
static int incoming(const fd2_field_game *g, const fd2_field_unit *u, int x, int y, int excluded) {
    int sum=0;
    for(size_t i=0;i<g->units.count;i++) {
        const fd2_field_unit *enemy=&g->units.items[i];
        if((int)i==excluded || !living(enemy) || enemy->side!=0 || (enemy->flags&4u) || enemy->detail_status[1])continue;
        uint8_t minimum=0,maximum=1;
        (void)fd2_field_attack_weapon_range(enemy,&minimum,&maximum);
        if(distance(x,y,enemy->x,enemy->y)>(int)enemy->movement_points+maximum)continue;
        int hit=(int)enemy->attack-(int)u->defense;
        if(hit>0)sum+=hit*9/10;
    }
    return sum;
}

/* 仅规划影子：stage0原34531/3460b/34673/346cd的缺失group和脚本。
 * future位置只构成保守危险包络，不是原版坐标/伤害oracle，不消耗RNG。 */
static int potential_units(const fd2_field_game *g, fd2_field_units *out) {
    static const struct {size_t before,after;uint8_t group,script;} steps[]={
        {12,13,3,7},{13,14,7,8},{14,18,4,3},{18,23,5,4},{23,27,6,6}
    };
    if(!g || !out || g->stage>1 || g->units.count>27)return -1;
    *out=g->units;
    if(g->stage==1) {
        if(out->count!=21 && out->count!=27)return -1;
        if(out->count==21) {
            const uint8_t *script;size_t size;
            if(fd2_field_units_append_group_placed(out,&g->metadata,&g->placements,&g->map,3,1)!=0 ||
                out->count!=27 || fd2_field_movement_script_get(13,&script,&size)!=0 ||
                fd2_field_movement_script_apply(out,script,size)!=0)return -1;
        }
        return 0;
    }
    if(out->count<12)return -1;
    for(size_t i=0;i<sizeof(steps)/sizeof(steps[0]);i++) {
        if(out->count>=steps[i].after)continue;
        if(out->count!=steps[i].before)return -1;
        const uint8_t *script;size_t size;
        if(fd2_field_units_append_group_placed(out,&g->metadata,&g->placements,&g->map,steps[i].group,0)!=0 ||
            out->count!=steps[i].after || fd2_field_stage0_movement_script_get(steps[i].script,&script,&size)!=0 ||
            fd2_field_movement_script_apply(out,script,size)!=0)return -1;
    }
    return 0;
}
static int threat_coverage(const fd2_field_units *units,int x,int y) {
    int count=0;
    for(size_t i=0;i<units->count;i++) {
        const fd2_field_unit *enemy=&units->items[i];
        if(!living(enemy) || enemy->side!=0 || (enemy->flags&4u) || enemy->detail_status[1]>1)continue;
        uint8_t minimum=0,maximum=0;int reach=0;
        if(fd2_field_attack_weapon_range(enemy,&minimum,&maximum)==0)reach=maximum;
        uint8_t spells[40];size_t n=fd2_field_unit_magic_list_build(enemy,spells,sizeof(spells));
        if(!enemy->detail_status[2])for(size_t j=0;j<n && j<sizeof(spells);j++) {
            const uint8_t *record=fd2_field_magic_record_get(spells[j]);
            if(!record || enemy->mp<record[5])continue;
            int radius=(record[3]>=16?record[3]-16:record[3])+record[4];
            if(radius>reach)reach=radius;
        }
        if(reach && distance(x,y,enemy->x,enemy->y)<=enemy->movement_points+reach)count++;
    }
    return count;
}
static void project_primary_move(fd2_field_game *shadow,const natural_action *primary) {
    if(primary->actor>=0) {
        shadow->units.items[primary->actor].x=(uint8_t)primary->x;
        shadow->units.items[primary->actor].y=(uint8_t)primary->y;
    }
}
static int plan_rest(const fd2_field_game *query,size_t raw,natural_action *rest) {
    const fd2_field_unit *u=&query->units.items[raw];
    *rest=(natural_action){PLAN_WAIT,(int)raw,-1,u->x,u->y,-1,-1,0,0};
    if(raw!=0 || u->unit_id!=0 || u->text_id!=0)return 0;
    fd2_field_units potential;if(potential_units(query,&potential)!=0)return -1;
    int coverage=threat_coverage(&potential,u->x,u->y);
    if(!coverage)return 0;
    int damage=incoming(query,u,u->x,u->y,-1);uint32_t cost=0;
    fd2_field_path_result range=FD2_FIELD_PATH_RESULT_INITIALIZER;
    if(fd2_field_game_compute_move_range(query,raw,&range)!=0)return -1;
    for(int y=0;y<query->map.height;y++)for(int x=0;x<query->map.width;x++) {
        if(!fd2_field_path_is_destination(&range,x,y))continue;
        int next=threat_coverage(&potential,x,y),hit=incoming(query,u,x,y,-1);
        uint32_t length=fd2_field_path_distance(&range,x,y);
        if(next<coverage || (next==coverage && (hit<damage || (hit==damage && length<cost)))) {
            coverage=next;damage=hit;cost=length;rest->x=x;rest->y=y;
        }
    }
    fd2_field_path_close(&range);return 0;
}
static int physical_estimate(const fd2_field_game *g, const fd2_field_unit *a,
        const fd2_field_unit *d, int x, int y) {
    uint32_t attack=a->attack, defense=d->defense;
    int32_t ap,dp;
    uint16_t at=fd2_field_cell_terrain(fd2_field_map_cell(&g->map,x,y));
    uint16_t dt=fd2_field_cell_terrain(fd2_field_map_cell(&g->map,d->x,d->y));
    if(at>=g->terrain.attr_count || dt>=g->terrain.attr_count)return 0;
    if(!fd2_field_attack_unit_ignores_terrain_modifier(a) &&
        fd2_field_attack_terrain_modifiers(g->terrain.attrs[at].movement_cost_class,&ap,&dp)==0)
        (void)fd2_field_attack_apply_terrain_modifier(attack,ap,&attack);
    if(!fd2_field_attack_unit_ignores_terrain_modifier(d) &&
        fd2_field_attack_terrain_modifiers(g->terrain.attrs[dt].movement_cost_class,&ap,&dp)==0)
        (void)fd2_field_attack_apply_terrain_modifier(defense,dp,&defense);
    return attack>defense?(int)((attack-defense)*9u/10u):0;
}
static void consider(natural_choices *choices, natural_action action) {
    size_t at=0;
    while(at<choices->count && choices->actions[at].score>=action.score)at++;
    if(at>=8)return;
    size_t count=choices->count<8?choices->count+1:8;
    for(size_t i=count-1;i>at;i--)choices->actions[i]=choices->actions[i-1];
    choices->actions[at]=action;choices->count=count;
}

static int choose(const fd2_field_game *g, natural_choices *choices) {
    memset(choices,0,sizeof(*choices));
    if(g->stage>1 || g->active_side!=2 || g->failure || g->interaction!=FD2_FIELD_INTERACTION_BROWSE ||
        g->loot.source || g->progress.resume || g->progress.notice.kind || g->player_post_waiting_phase)return -1;
    fd2_field_units potential;if(potential_units(g,&potential)!=0)return -1;
    int tank=-1; unsigned highest_dp=0;
    for(size_t i=0;i<g->units.count;i++)if(living(&g->units.items[i]) && g->units.items[i].side==2 &&
        (tank<0 || g->units.items[i].defense>highest_dp)){tank=(int)i;highest_dp=g->units.items[i].defense;}
    for(size_t raw=0;raw<g->units.count;raw++) {
        const fd2_field_unit *u=&g->units.items[raw];if(!actionable(u))continue;
        fd2_field_path_result range=FD2_FIELD_PATH_RESULT_INITIALIZER;
        if(fd2_field_game_compute_move_range(g,raw,&range)!=0)return -1;
        int hero_min=INT_MAX;
        if(raw==0)for(int y=0;y<g->map.height;y++)for(int x=0;x<g->map.width;x++)
            if(fd2_field_path_is_destination(&range,x,y)) {
                int count=threat_coverage(&potential,x,y);if(count<hero_min)hero_min=count;
            }
        /* 长程前驱只选本轮可达路标，避免在墙边按Manhattan来回移动；
         * 不模拟原版AI，也不把其路线当原版实际输入/伤害oracle。 */
        int waypoint_x=u->x,waypoint_y=u->y;
        int has_waypoint=0;
        int fire_position=u->unit_id==9 && (((const uint8_t *)u)[0x1a]&1u) && u->mp>=2 && !u->detail_status[2];
        fd2_field_path_result long_range=FD2_FIELD_PATH_RESULT_INITIALIZER;
        const uint8_t *profile=fd2_field_movement_profile_get(u->movement_profile);
        if(profile && fd2_field_move_range_compute(&long_range,&g->map,&g->terrain,&g->units,
                raw,profile,FD2_FIELD_MOVEMENT_COST_CLASS_COUNT,28)==0) {
            uint32_t best=UINT32_MAX;int gx=-1,gy=-1;
            for(int py=0;py<g->map.height;py++)for(int px=0;px<g->map.width;px++) {
                if(!fd2_field_path_is_destination(&long_range,px,py))continue;
                uint32_t cost=fd2_field_path_distance(&long_range,px,py);
                if(cost>=best)continue;
                if(fire_position) {
                    fd2_field_path_result cast=FD2_FIELD_PATH_RESULT_INITIALIZER;
                    if(fd2_field_target_range_compute(&cast,&g->map,&g->terrain,px,py,0,5)==0)
                        for(size_t t=0;t<g->units.count;t++)if(living(&g->units.items[t]) && g->units.items[t].side==0 &&
                            fd2_field_path_is_destination(&cast,g->units.items[t].x,g->units.items[t].y)) {best=cost;gx=px;gy=py;break;}
                    fd2_field_path_close(&cast);
                } else for(size_t t=0;t<g->units.count;t++)if(living(&g->units.items[t]) && g->units.items[t].side==0 &&
                    distance(px,py,g->units.items[t].x,g->units.items[t].y)==1) {best=cost;gx=px;gy=py;break;}
            }
            if(gx>=0) {
                has_waypoint=1;
                uint8_t route[65536];int n=fd2_field_path_build(&long_range,gx,gy,route,sizeof(route));
                int px=u->x,py=u->y;static const int dx[]={0,-1,0,1},dy[]={1,0,-1,0};
                for(int i=0;i<n;i++) {px+=dx[route[i]];py+=dy[route[i]];
                    if(fd2_field_path_is_destination(&range,px,py)){waypoint_x=px;waypoint_y=py;}}
            }
        }
        fd2_field_path_close(&long_range);
        int nearest=INT_MAX;
        for(size_t t=0;t<g->units.count;t++)if(living(&g->units.items[t]) && g->units.items[t].side==0) {
            int d=distance(u->x,u->y,g->units.items[t].x,g->units.items[t].y);if(d<nearest)nearest=d;
        }
        int slot=herb_slot(u); uint8_t min_range=0,max_range=0;
        int has_weapon=fd2_field_attack_weapon_range(u,&min_range,&max_range)==0;
        for(int y=0;y<g->map.height;y++)for(int x=0;x<g->map.width;x++) {
            if(!fd2_field_path_is_destination(&range,x,y))continue;
            if(raw==0 && threat_coverage(&potential,x,y)!=hero_min)continue;
            int cost=(int)fd2_field_path_distance(&range,x,y),threat=incoming(g,u,x,y,-1);
            int remaining=u->hp-(threat>0?threat:0);
            int danger=(int)raw==tank?threat/2:threat*2;
            if(remaining<8)danger+=120;
            if(u->text_id==0 && remaining<15)danger+=150;
            natural_action wait={PLAN_WAIT,(int)raw,-1,x,y,-1,-1,-8-cost-danger,0};
            /* 主角存活是硬目标；不靠预计普通伤害赌暴击/双击。危险时
             * 优先撤到低威胁合法格，HP/敌坐标仍只来自本次真实snapshot。 */
            if(u->text_id==0) {
                int origin_threat=incoming(g,u,u->x,u->y,-1);
                if(origin_threat*2 >= (int)u->hp && threat<origin_threat)
                    wait.score+=300+(origin_threat-threat)*3;
            }
            int next_distance=INT_MAX;
            for(size_t t=0;t<g->units.count;t++)if(living(&g->units.items[t]) && g->units.items[t].side==0) {
                int d=distance(x,y,g->units.items[t].x,g->units.items[t].y);if(d<next_distance)next_distance=d;
            }
            if(has_waypoint && nearest!=INT_MAX && next_distance!=INT_MAX) {
                wait.score+=(nearest-next_distance)*6;
                if((int)raw==tank && cost && next_distance<=4)wait.score+=60;
            }
            if(cost && x==waypoint_x && y==waypoint_y &&
                has_waypoint && incoming(g,&g->units.items[0],g->units.items[0].x,g->units.items[0].y,-1)==0)
                wait.score+=fire_position?125:65;
            if(!cost && u->hp<u->hp_max)wait.score+=((int)u->hp_max-u->hp)*2;
            uint32_t cell=fd2_field_map_cell(&g->map,x,y);
            uint16_t tid=fd2_field_cell_terrain(cell); size_t index=(cell>>16)&31u;
            if(tid<g->terrain.attr_count && (g->terrain.attrs[tid].flags&0x60u) && index<16u &&
                !g->cell_action_completed[index] && g->metadata.cell_actions[index].mode==0 &&
                g->metadata.cell_actions[index].param==192 && slot<0)wait.score+=35;
            consider(choices,wait);
            for(size_t target=0;target<g->units.count;target++) {
                const fd2_field_unit *v=&g->units.items[target];if(!living(v))continue;
                int md=distance(x,y,v->x,v->y);
                if(slot>=0 && v->side!=0 && v->hp<v->hp_max && md<=1) {
                    /* 主角败北即结束：自身受伤或仍在敌方威胁内时，保留草药自救，
                     * 不为替同伴恢复而耗尽唯一资源或走入危险格。仅改变候选意图。 */
                    if(u->text_id==0 && target!=raw &&
                       (u->hp<u->hp_max || threat>0 ||
                        incoming(g,u,u->x,u->y,-1)>0))continue;
                    fd2_field_path_result cast=FD2_FIELD_PATH_RESULT_INITIALIZER;
                    int legal=fd2_field_target_range_compute(&cast,&g->map,&g->terrain,x,y,0,1)==0 &&
                        fd2_field_path_is_destination(&cast,v->x,v->y);
                    fd2_field_path_close(&cast);
                    if(legal) {
                        int missing=(int)v->hp_max-v->hp;
                        int healing_danger = (int)target == (int)raw ? threat * 2 : danger;
                        natural_action heal={PLAN_HERB,(int)raw,(int)target,x,y,-1,slot,
                            40+missing*3-cost-healing_danger,missing<36?missing:36};
                        if(v->hp<v->hp_max/2)heal.score+=60;
                        if(v->text_id==0 && v->hp<=incoming(g,v,v->x,v->y,-1)+5)heal.score+=220;
                        if(missing<12)heal.score-=100;
                        consider(choices,heal);
                    }
                }
                if(!has_weapon || v->side!=0 || md<min_range || md>max_range)continue;
                fd2_field_units units=g->units;units.items[raw].x=(uint8_t)x;units.items[raw].y=(uint8_t)y;
                if(!fd2_field_attack_target_is_legal(&g->map,&g->terrain,&units,raw,target))continue;
                int hit=physical_estimate(g,u,v,x,y);
                natural_action attack={PLAN_ATTACK,(int)raw,(int)target,x,y,-1,-1,
                    10+hit*2-cost-danger,hit};
                if(hit>=v->hp)attack.score+=80;
                if(distance(v->x,v->y,g->units.items[0].x,g->units.items[0].y)<=v->movement_points+1)attack.score+=65;
                consider(choices,attack);
            }
        }
        fd2_field_path_close(&range);
        uint8_t spells[40];size_t spell_count=fd2_field_unit_magic_list_build(u,spells,sizeof(spells));
        for(size_t s=0;s<spell_count && s<sizeof(spells);s++) {
            if(raw==0 && threat_coverage(&potential,u->x,u->y)!=hero_min)continue;
            uint8_t id=spells[s];if(id!=13 && !(id==0 && u->unit_id==9))continue;
            const uint8_t *record=fd2_field_magic_record_get(id);if(!record || u->mp<record[5] || u->detail_status[2])continue;
            fd2_field_path_result cast=FD2_FIELD_PATH_RESULT_INITIALIZER;
            if(fd2_field_target_range_compute(&cast,&g->map,&g->terrain,u->x,u->y,0,record[3])!=0)return -1;
            for(size_t target=0;target<g->units.count;target++) {
                const fd2_field_unit *v=&g->units.items[target];
                if(!living(v) || !fd2_field_path_is_destination(&cast,v->x,v->y))continue;
                if(id==0 && v->side==0) {
                    /* 自然援军可能堵满最后敌人的近战邻格；保留一次真实Fire，
                     * 只在无近战入口或主角确有威胁时使用最后2MP，不重写任何记录。 */
                    if(u->mp==record[5] && physical_access(g,target) &&
                        incoming(g,&g->units.items[0],g->units.items[0].x,g->units.items[0].y,-1)==0)continue;
                    uint32_t scale=0;if(fd2_field_magic_damage_profile_scale(v->movement_profile,&scale)!=0)continue;
                    int hit=(int)((uint32_t)fd2_field_magic_u16(record,0)*scale/10u*9u/10u);
                    natural_action a={PLAN_MAGIC,(int)raw,(int)target,u->x,u->y,id,-1,
                        10+hit*2+(hit>=v->hp?80:0)-incoming(g,u,u->x,u->y,-1)*2,hit};
                    if(distance(v->x,v->y,g->units.items[0].x,g->units.items[0].y)<=v->movement_points+1) {
                        a.score+=65;
                    }
                    consider(choices,a);
                } else if(id==13 && v->side!=0 && v->hp<v->hp_max) {
                    int missing=(int)v->hp_max-v->hp;
                    natural_action a={PLAN_MAGIC,(int)raw,(int)target,u->x,u->y,id,-1,
                        70+missing*3+(v->hp<v->hp_max/2?60:0),missing};consider(choices,a);
                }
            }
            fd2_field_path_close(&cast);
        }
    }
    if(!choices->count)consider(choices,(natural_action){PLAN_END,-1,-1,0,0,-1,-1,0,0});
    return 0;
}

static void hex_record(const fd2_field_unit *u) {
    const uint8_t *raw=(const uint8_t*)u;putchar('"');for(size_t i=0;i<80;i++)printf("%02x",raw[i]);putchar('"');
}
static const char *kind_name(int kind) {
    static const char *names[]={"wait","attack","magic","herb","end_turn"};
    return kind>=0 && kind<=PLAN_END?names[kind]:"invalid";
}
static void print_action(const fd2_field_game *g, const fd2_field_game *query, const natural_action *a, int phase_prediction) {
    printf("{\"kind\":\"%s\",\"actor_raw\":%d,\"target_raw\":%d,\"destination\":[%d,%d],\"magic_id\":%d,\"item_slot\":%d,\"heuristic_score\":%d,\"heuristic_damage_or_heal_not_oracle\":%d",
        kind_name(a->kind),a->actor,a->target,a->x,a->y,a->magic,a->slot,a->score,a->estimate);
    if(a->actor>=0) {
        printf(",\"actor_record_hex\":");hex_record(&g->units.items[a->actor]);
        fd2_field_path_result range=FD2_FIELD_PATH_RESULT_INITIALIZER;uint8_t path[65536];int n=-1;
        if(fd2_field_game_compute_move_range(query,(size_t)a->actor,&range)==0)
            n=fd2_field_path_build(&range,a->x,a->y,path,sizeof(path));
        printf(",\"reachable_path_directions\":[");for(int i=0;i<n;i++)printf("%s%u",i?",":"",path[i]);printf("]");
        fd2_field_path_close(&range);
        fd2_field_game command=*query;
        command.units.items[a->actor].x=(uint8_t)a->x;command.units.items[a->actor].y=(uint8_t)a->y;
        command.selected_unit=a->actor;command.interaction=FD2_FIELD_INTERACTION_COMMAND;
        command.move_path_length=n>0?(size_t)n:0;
        if(fd2_field_game_refresh_commands(&command)==0)
            printf(",\"command_disabled\":[%u,%u,%u,%u],\"command_first_enabled\":%d",
                command.command_disabled[0],command.command_disabled[1],command.command_disabled[2],command.command_disabled[3],command.command_selected);
        else printf(",\"command_capability_query_error\":true");
        printf(",\"execute_by_cursor_destination\":true,\"path_is_query_intent_not_original_trace\":true,\"preconditions\":{\"phase\":2,\"interaction\":\"browse\",\"actor_can_act\":true,\"target_record_from_this_saved_turn\":true,\"fresh_save_required_next_turn\":true}");
        if(a->kind==PLAN_MAGIC)printf(",\"require_zero_movement\":true");
        if(a->kind==PLAN_WAIT) {
            uint32_t cell=fd2_field_map_cell(&g->map,a->x,a->y);uint16_t terrain=fd2_field_cell_terrain(cell);
            size_t index=(cell>>16)&31u;
            if(terrain<g->terrain.attr_count && (g->terrain.attrs[terrain].flags&0x60u) && index<16u && !g->cell_action_completed[index])
                printf(",\"wait_cell_action\":{\"index\":%zu,\"mode\":%u,\"parameter\":%u,\"initial_choice\":\"yes\"}",
                    index,g->metadata.cell_actions[index].mode,g->metadata.cell_actions[index].param);
        }
    }
    if(phase_prediction>=0 && a->actor==0 &&
        (a->x!=g->units.items[0].x || a->y!=g->units.items[0].y))
        printf(",\"hero_safety_reposition\":true");
    if(a->target>=0) { printf(",\"target_record_hex\":");hex_record(&g->units.items[a->target]); }
    if(a->actor>=0) {
        const fd2_field_unit *u=&g->units.items[a->actor];
        printf(",\"visible_health_and_threat\":{\"hp\":%u,\"hp_max\":%u,\"mp\":%u,\"origin_incoming_estimate_not_oracle\":%d,\"destination_incoming_estimate_not_oracle\":%d}",
            u->hp,u->hp_max,u->mp,incoming(g,u,u->x,u->y,-1),incoming(g,u,a->x,a->y,-1));
        if(a->kind==PLAN_ATTACK) {
            uint8_t minimum=0,maximum=0;
            if(fd2_field_attack_weapon_range(u,&minimum,&maximum)==0)
                printf(",\"target_query\":{\"physical_range_min\":%u,\"physical_range_max\":%u,\"cursor_resource_frame\":0}",minimum,maximum);
        } else if(a->kind==PLAN_MAGIC) {
            const uint8_t *record=fd2_field_magic_record_get((uint8_t)a->magic);
            if(record)printf(",\"target_query\":{\"cast\":%u,\"minimum\":0,\"area\":%u,\"filter\":%u,\"cursor_resource_frame\":1}",record[3],record[4],record[6]);
        } else if(a->kind==PLAN_HERB) {
            const uint8_t *record=fd2_field_item_record_get(192);
            if(record)printf(",\"target_query\":{\"cast\":%u,\"minimum\":0,\"area\":%u,\"filter\":%u,\"cursor_resource_frame\":1}",record[0x10],record[0x12],record[0x15]);
        }
    }
    if(phase_prediction>=0)printf(",\"predicted_phase_advance_after_wait\":%s,\"phase_prediction_assumes_unchanged_roster\":true",phase_prediction?"true":"false");
    printf("}");
}

/* 无窗口模拟仅ACK已实现业务通知；不伪造WGC、用户输入或原版等待RNG。 */
static int settle(fd2_field_game *g) {
    for(unsigned guard=0;guard<20000;guard++) {
        if(g->failure)return -1;
        for(size_t i=0;i<g->event_log_count;i++)g->event_log[i].presentation_deferred=0;
        if(g->loot.notice.kind) {
            fd2_field_loot_notice n=g->loot.notice;
            if(n.kind==FD2_FIELD_LOOT_MESSAGE && !g->loot.text_ready &&
                fd2_field_game_loot_text_ready(g)<0)return -1;
            fd2_field_loot_reply reply=n.kind==FD2_FIELD_LOOT_CHOICE?
                (n.fragment==421 || n.fragment==428?FD2_FIELD_LOOT_YES:FD2_FIELD_LOOT_NO):
                n.kind==FD2_FIELD_LOOT_INVENTORY?FD2_FIELD_LOOT_CANCEL:FD2_FIELD_LOOT_ACK;
            if(fd2_field_game_loot_reply(g,reply,-1)<0){fprintf(stderr,"loot reply refused: kind=%d fragment=%u source=%u step=%u\n",n.kind,n.fragment,g->loot.source,g->loot.step);return -1;}
            continue;
        }
        if(g->progress.notice.kind) { if(fd2_field_game_progress_ack(g)<0){fprintf(stderr,"progress ACK refused\n");return -1;}continue; }
        if(g->progress.resume) { if(fd2_field_game_progress_resume(g)<0){fprintf(stderr,"progress resume refused\n");return -1;}continue; }
        if(g->player_post_waiting_phase) {
            int r=fd2_field_game_resume_player_controller(g);if(r<0)return -1;
            if(r==0)continue;
        }
        if(fd2_field_game_battle_result(g)!=FD2_FIELD_BATTLE_ONGOING)return 0;
        if(g->active_side==2 && !g->loot.source)return 0;
        if(fd2_field_game_process_automatic_action(g)<0){fprintf(stderr,"automatic refused: side=%u cursor=%zu\n",g->active_side,g->phase_unit_cursor);return -1;}
    }
    return -1;
}
static int apply(fd2_field_game *g,const natural_action *a) {
    if(a->kind==PLAN_END)return fd2_field_game_end_player_turn(g);
    fd2_field_unit *u=&g->units.items[a->actor];g->cursor_cell_x=u->x;g->cursor_cell_y=u->y;
    if(fd2_field_game_confirm_cursor(g)!=a->actor)return -1;
    g->cursor_cell_x=a->x;g->cursor_cell_y=a->y;
    if(fd2_field_game_confirm_cursor(g)!=a->actor || fd2_field_game_execute_move(g,NULL,0)!=0)return -1;
    if(a->kind==PLAN_WAIT)return fd2_field_game_finish_unit_action(g);
    if(a->kind==PLAN_ATTACK) {
        if(fd2_field_game_begin_attack(g)!=0)return -1;
        g->cursor_cell_x=g->units.items[a->target].x;g->cursor_cell_y=g->units.items[a->target].y;
        return fd2_field_game_resolve_attack(g);
    }
    if(a->kind==PLAN_MAGIC) {
        if(fd2_field_game_begin_magic_list(g)!=0)return -1;
        uint8_t ids[40];size_t count=fd2_field_unit_magic_list_build(u,ids,sizeof(ids)),wanted=count;
        for(size_t i=0;i<count;i++)if(ids[i]==a->magic){wanted=i;break;}
        for(size_t i=0;i<wanted && i<count;i++) {uint8_t out=255;if(fd2_field_game_magic_list_step(g,FD2_FIELD_MAGIC_LIST_KEY_DOWN,&out)<0)return -1;}
        if(wanted>=count || fd2_field_game_begin_magic_targeting(g,(uint8_t)a->magic)!=0)return -1;
        g->cursor_cell_x=g->units.items[a->target].x;g->cursor_cell_y=g->units.items[a->target].y;
        int r=fd2_field_game_resolve_player_magic(g);
        if(r<0)fprintf(stderr,"magic resolve refused: actor=%d target=%d id=%d mp=%u count=%zu gain=%u\n",a->actor,a->target,a->magic,u->mp,g->units.count,g->experience_value);
        return r;
    }
    if(a->kind==PLAN_HERB) {
        if(fd2_field_game_begin_item_menu(g)!=0 || g->command_selected!=0 || fd2_field_game_confirm_item_menu(g)!=0)return -1;
        for(int i=0;i<a->slot;i++)if(fd2_field_game_item_list_step(g,FD2_FIELD_ITEM_LIST_KEY_DOWN)<0)return -1;
        if(fd2_field_game_begin_item_targeting(g)!=0)return -1;
        g->cursor_cell_x=g->units.items[a->target].x;g->cursor_cell_y=g->units.items[a->target].y;
        return fd2_field_game_resolve_player_item(g);
    }
    return -1;
}

static int parse_number(const char *text,unsigned maximum,unsigned *out) {
    if(!text || !*text || *text=='-')return -1;
    char *end;errno=0;unsigned long n=strtoul(text,&end,0);
    if(errno || *end || n>maximum)return -1;
    *out=(unsigned)n;return 0;
}

int main(int argc,char **argv) {
    const char *path=NULL;int simulate=0;unsigned seed=0x7a18,max_turns=100;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--save") && i+1<argc)path=argv[++i];
        else if(!strcmp(argv[i],"--simulate"))simulate=1;
        else if(!strcmp(argv[i],"--seed") && i+1<argc) {if(parse_number(argv[++i],65535,&seed)!=0){fprintf(stderr,"种子参数无效：要求0..65535。\n");return 2;}}
        else if(!strcmp(argv[i],"--max-turns") && i+1<argc) {if(parse_number(argv[++i],1000,&max_turns)!=0){fprintf(stderr,"回合上限参数无效：要求1..1000。\n");return 2;}}
        else {fprintf(stderr,"未知参数或缺值：%s\n",argv[i]);return 2;}
    }
    if(!path || !max_turns || max_turns>1000u) {fprintf(stderr,"用法：field_natural_plan --save PATH [--simulate --seed N --max-turns N]\n");return 2;}
    fd2_save_file save={0};fd2_save_battle_snapshot snapshot;fd2_archive fdother={0};fd2_field_game *g=calloc(1,sizeof(*g));
    int rc=1;
    if(!g || fd2_save_file_open(&save,path)!=0 || fd2_save_file_get_battle_snapshot(&save,&snapshot)!=0 ||
        snapshot.meta[2]>1 || fd2_archive_open(&fdother,"original_game/FDOTHER.DAT")!=0 ||
        fd2_field_game_open_resources(g,&fdother,snapshot.meta[2])!=0 || fd2_field_game_import_battle_snapshot(g,&snapshot)!=0) {
        fprintf(stderr,"活动存档或已支持的关卡资源无效，未规划/未写入。\n");goto done;
    }
    fd2_field_rng rng;fd2_field_rng_seed(&rng,(uint16_t)seed);
    if(fd2_field_game_set_attack_hooks(g,NULL,NULL,NULL,NULL,fd2_field_rng_next,&rng)!=0)goto done;
    natural_choices choices;
    if(!simulate) {
        int terminal=fd2_field_game_battle_result(g);
        if(terminal!=FD2_FIELD_BATTLE_ONGOING) {
            printf("{\"planning_only\":true,\"original_acceptance\":false,\"next_action\":{\"kind\":\"terminal_checkpoint\",\"battle_result\":%d}}\n",terminal);rc=0;goto done;
        }
        fd2_field_game before=*g;uint16_t state=rng.state;
        if(choose(g,&choices)!=0 || memcmp(g,&before,sizeof(*g)) || rng.state!=state)goto done;
        printf("{\"planning_only\":true,\"original_acceptance\":false,\"source_turn\":%u,\"source_active_count\":%zu,\"source_roster_count\":%zu,\"query_side_effects\":false,\"rng_or_contribution_observed\":false,\"heuristic_not_guaranteed\":true,\"fire_reserve_reason\":\"retain final 2MP when enemy has a free melee neighbor and hero has no estimated incoming threat; allied reinforcements can otherwise block the last enemy\",\"next_action\":",g->turn_number,g->units.count,g->roster_count);
        print_action(g,g,&choices.actions[0],-1);printf(",\"ordered_candidates\":[");
        for(size_t i=0;i<choices.count;i++){if(i)putchar(',');print_action(g,g,&choices.actions[i],-1);}
        printf("],\"rest_tail\":[");int rest_count=0;
        fd2_field_game rest_query=*g;project_primary_move(&rest_query,&choices.actions[0]);
        int phase_remaining=0;
        for(size_t i=0;i<g->units.count;i++)if((g->units.items[i].flags&0x81u)==0 &&
            g->units.items[i].side==2 && !g->units.items[i].detail_status[1] && (int)i!=choices.actions[0].actor)phase_remaining++;
        for(size_t i=0;i<g->units.count;i++)if((int)i!=choices.actions[0].actor && actionable(&g->units.items[i])) {
            natural_action rest;if(plan_rest(&rest_query,i,&rest)!=0)goto done;
            if(rest_count++)putchar(',');print_action(g,&rest_query,&rest,--phase_remaining==0);
        }
        printf("],\"rest_tail_policy\":\"Hero0 only may move WAIT to minimal known+pending-wave coverage, then damage heuristic/path cost; primary movement occupancy only, no assumed kill; all other REST stationary; skip END after phase; no MP recovery assumed\"}\n");
        if(memcmp(g,&before,sizeof(*g)) || rng.state!=state)goto done;
        rc=0;
    } else {
        printf("{\"simulation_only\":true,\"original_acceptance\":false,\"presentation_ack_without_render\":true,\"local_sdl_seed_not_original_rng\":%u,\"actions\":[",seed);
        unsigned count=0;int failed=0;
        while(g->turn_number<=max_turns && fd2_field_game_battle_result(g)==FD2_FIELD_BATTLE_ONGOING) {
            if(choose(g,&choices)!=0){failed=1;break;}
            natural_action action=choices.actions[0];
            uint32_t source_turn=g->turn_number;
            fd2_field_units source_units=g->units;
            fd2_field_game source_game=*g,rest_query=*g;project_primary_move(&rest_query,&action);
            natural_action rests[FD2_FIELD_MAX_UNITS];
            for(size_t i=0;i<source_units.count;i++)if((int)i!=action.actor && actionable(&source_units.items[i]))
                if(plan_rest(&rest_query,i,&rests[i])!=0){failed=1;break;}
            if(failed)break;
            if(count++)putchar(',');printf("{\"turn\":%u,\"action\":",g->turn_number);print_action(g,g,&action,-1);printf("}");
            if(apply(g,&action)<0){fprintf(stderr,"apply failed: turn=%u kind=%s interaction=%d actor=%d target=%d\n",g->turn_number,kind_name(action.kind),g->interaction,action.actor,action.target);failed=1;break;}
            if(settle(g)<0){failed=1;break;}
            if(fd2_field_game_battle_result(g)!=FD2_FIELD_BATTLE_ONGOING)break;
            for(size_t i=0;i<source_units.count && g->turn_number==source_turn && g->active_side==2;i++) {
                if((int)i==action.actor || !actionable(&source_units.items[i]) || !actionable(&g->units.items[i]))continue;
                if(g->units.items[i].unit_id!=source_units.items[i].unit_id || g->units.items[i].x!=source_units.items[i].x ||
                    g->units.items[i].y!=source_units.items[i].y){failed=1;break;}
                natural_action rest=rests[i];
                if(count++)putchar(',');printf("{\"turn\":%u,\"role\":\"rest_tail\",\"action\":",g->turn_number);print_action(&source_game,&rest_query,&rest,-1);printf("}");
                if(apply(g,&rest)<0 || settle(g)<0){failed=1;break;}
                if(fd2_field_game_battle_result(g)!=FD2_FIELD_BATTLE_ONGOING)break;
            }
            if(failed || fd2_field_game_battle_result(g)!=FD2_FIELD_BATTLE_ONGOING)break;
            if(g->turn_number==source_turn && g->active_side==2 && fd2_field_game_end_player_turn(g)<0){failed=1;break;}
            if(settle(g)<0){failed=1;break;}
        }
        int result=fd2_field_game_battle_result(g);
        printf("],\"final_turn\":%u,\"battle_result\":%d,\"failure\":%d,\"simulation_error\":%s,\"active_count\":%zu,\"roster_count\":%zu,\"hero_hp\":%u,\"hero_hidden\":%s,\"final_visible_records\":[",
            g->turn_number,result,g->failure,failed?"true":"false",g->units.count,g->roster_count,g->units.items[0].hp,(g->units.items[0].flags&1u)?"true":"false");
        int records=0;
        for(size_t i=0;i<g->units.count;i++)if(living(&g->units.items[i])) {
            if(records++)putchar(',');printf("{\"raw\":%zu,\"record_hex\":",i);hex_record(&g->units.items[i]);printf("}");
        }
        printf("]}\n");
        rc=!failed && result==FD2_FIELD_BATTLE_VICTORY?0:1;
    }
done:
    if(g){fd2_field_game_close(g);free(g);}fd2_archive_close(&fdother);fd2_save_file_close(&save);
    return rc;
}
