/* S2：沿冷图枚举有界身份/资源前态；复用10b4e/33499/typed writer合同。
 * 只借原Game，局部投影不拥有资源、不执行RNG/呈现、不回写或回滚热状态。
 * 未知业务后的谓词查两边；新构造actor只确认身份，不伪造其HP/库存。 */
#ifndef FD2_FIELD_COMMON_EVENT_PROJECTION_H
#define FD2_FIELD_COMMON_EVENT_PROJECTION_H

typedef struct {
    fd2_field_units units;
    fd2_field_unit roster[FD2_SAVE_BATTLE_MAX_ROSTER];
    uint8_t state[FD2_FIELD_EVENT_STATE_SIZE];
    uint8_t known[FD2_FIELD_MAX_UNITS];
    size_t roster_count;
    int pc, map_entry, shape_index, uncertain;
} common_event_projection;

static int common_event_project_query_unknown(const common_event_projection *p,
        const fd2_static_flow_node *n,size_t caller) {
    const int32_t *a=n->args;
    /* 战斗/奖励只使对应动态字段未知；不能因此把原caller、回合、
     * 完成位、roster身份或count的已知分支也分叉成不可达资源路径。 */
    if(p->uncertain && (!strcmp(n->op,"result_leader_dead") ||
        !strcmp(n->op,"result_side_present") || !strcmp(n->op,"result_all_hidden") ||
        !strcmp(n->op,"branch_hidden_count") || !strcmp(n->op,"branch_item") ||
        !strcmp(n->op,"branch_item_occurrences") ||
        (!strcmp(n->op,"branch_actor") && a[1]!=FD2_EVENT_ACTOR_ID &&
         a[1]!=FD2_EVENT_ACTOR_TEXT && a[1]!=FD2_EVENT_ACTOR_SIDE)))return 1;
    if(!strcmp(n->op,"result_leader_dead")) {
        size_t actor=(size_t)a[0];
        return actor<p->units.count && (!(p->known[actor]&1u) ||
            (a[1] && !fd2_field_unit_is_hidden(&p->units.items[actor]) && !(p->known[actor]&16u)));
    }
    if(!strcmp(n->op,"result_side_present")) {
        for(size_t i=0;i<p->units.count;i++)
            if(!(p->known[i]&3u) || (a[1] && p->units.items[i].side==a[0] &&
                !fd2_field_unit_is_hidden(&p->units.items[i]) && !(p->known[i]&16u)))return 1;
    }
    if(!strcmp(n->op,"branch_actor")) {
        size_t actor=a[0]==-1?caller:(size_t)a[0];
        return actor<p->units.count && a[1]>=0 && a[1]<8 && !(p->known[actor]&(1u<<a[1]));
    }
    if(!strcmp(n->op,"branch_item")) {
        size_t actor=a[0]==-1?caller:(size_t)a[0];
        return actor<p->units.count && !(p->known[actor]&128u);
    }
    if(!strcmp(n->op,"branch_item_occurrences"))
        for(int32_t i=a[0];i>=0 && i<=a[1] && (size_t)i<p->units.count;i++)
            if(!(p->known[i]&128u))return 1;
    return 0;
}

static int common_event_project_write(const fd2_field_game *game,
        common_event_projection *p,const fd2_static_flow_node *n,unsigned *port,size_t actor) {
    const int32_t *a=n->args;const char *op=n->op;
    if(!strcmp(op,"append_group") || !strcmp(op,"append_group_source")) {
        uint32_t group=(uint32_t)a[0];
        if(!strcmp(op,"append_group_source") && fd2_field_event_source_value(
            (fd2_field_event_value_source)a[0],game->turn_number,p->state,&group)!=0)return -1;
        if(a[1]!=0 && a[1]!=1)return -1;
        size_t first=p->units.count;
        if(group<=UINT8_MAX && fd2_field_units_project_group_identity(&p->units,
            &game->metadata,&game->placements,(uint8_t)group)!=0)return -1;
        for(size_t i=first;i<p->units.count;i++) {
            if(fd2_field_units_sprite_cache_append(&p->units,i)!=0)return -1;
            p->known[i]=0x4fu; /* hidden/side/text/ID/acted；不猜HP/最大HP/库存。 */
        }
    } else if(!strcmp(op,"roster_append")) {
        if(p->roster_count>=FD2_SAVE_BATTLE_MAX_ROSTER || a[0]<0 || a[0]>31)return -1;
        if(fd2_field_unit_init(&p->roster[p->roster_count++],(uint8_t)a[0],(uint8_t)a[0],2,0,0)!=0)return -1;
    } else if(!strcmp(op,"unit_field_write") || !strcmp(op,"unit_ai_write")) {
        if(a[0]<0 || a[1]<a[0] || (size_t)a[1]>=p->units.count)return -1;
        if((!strcmp(op,"unit_ai_write") && (a[2]<0 || a[2]>255)) ||
           (!strcmp(op,"unit_field_write") && (a[4]<0 || a[4]>UINT16_MAX)))return -1;
        int result=!strcmp(op,"unit_ai_write")?
            fd2_field_units_ai_behavior_write(&p->units,(size_t)a[0],(size_t)a[1],(uint8_t)a[2]):
            fd2_field_units_field_write(&p->units,(size_t)a[0],(size_t)a[1],(fd2_field_unit_field)a[2],
                (fd2_field_unit_write)a[3],(uint16_t)a[4]);
        if(result!=0)return -1;
        if(!strcmp(op,"unit_field_write") && a[3]==FD2_UNIT_SET) {
            unsigned bits=a[2]==FD2_UNIT_ID?8u:a[2]==FD2_UNIT_TEXT_ID?4u:
                a[2]==FD2_UNIT_SIDE?2u:a[2]==FD2_UNIT_FLAGS?65u:a[2]==FD2_UNIT_HP?16u:0u;
            for(size_t i=(size_t)a[0];i<=(size_t)a[1];i++)p->known[i]|=(uint8_t)bits;
        }
    } else if(!strcmp(op,"event_byte_write")) {
        if(a[0]<0 || a[1]<a[0] || a[1]>31 || a[2]<0 || a[2]>255 || a[3]<0 || a[3]>1 ||
            fd2_field_event_state_write(p->state,(size_t)a[0],(size_t)a[1],(uint8_t)a[2],a[3])!=0)return -1;
    } else if(!strcmp(op,"event_byte_from_count")) {
        if(a[0]<0 || a[0]>31 || a[1]<0 || (size_t)a[1]>p->units.count)return -1;
        p->state[a[0]]=(uint8_t)(p->units.count-(size_t)a[1]);
    } else if(!strcmp(op,"unit_hp_zero_tail")) {
        if(a[0]<0 || fd2_field_event_hp_zero_tail(&p->units,(size_t)a[0])!=0)return -1;
        for(size_t i=(size_t)a[0];i<p->units.count;i++)p->known[i]|=16u;
    } else if(!strcmp(op,"directions_reset")) {
        for(size_t i=0;i<p->units.count;i++)p->units.items[i].direction=0;
    } else if(!strcmp(op,"event_item_append") || !strcmp(op,"event_item_remove")) {
        size_t at=a[0]==-1?actor:(size_t)a[0];
        if(a[0]<-1 || at>=p->units.count)return -1;
        if(a[1]<0 || !(p->known[at]&128u)){p->uncertain=1;return 0;}
        int result=!strcmp(op,"event_item_append")?fd2_field_event_item_append(&p->units,at,(uint8_t)a[1]):
            fd2_field_event_item_remove(&p->units,at,(uint8_t)a[1]);
        if(result<0)return -1;*port=result?FD2_FLOW_YES:FD2_FLOW_NO;
    } else if(!strcmp(op,"ai_anchor")) {
        if(a[0]<0 || a[1]<a[0] || (size_t)a[1]>=p->units.count)return -1;
    } else if(!strcmp(op,"physical_exchange") || !strcmp(op,"finalize_defeated") ||
              !strcmp(op,"loot_child") || !strcmp(op,"grant_party_item") ||
              !strcmp(op,"event_item_consume") || !strcmp(op,"mark_random_pair")) {
        p->uncertain=1; /* 不预执行真实战斗、child/RNG或推导未知派生值。 */
    }
    return 0;
}

static int common_event_preflight_identity(const fd2_field_game *game,
        const fd2_static_flow *flow,size_t actor) {
    enum { MAX_PATHS=64 };
    if(game->roster_count>FD2_SAVE_BATTLE_MAX_ROSTER)return -1;
    common_event_projection *pending=calloc(MAX_PATHS,sizeof(*pending));
    if(!pending)return -1;
    pending[0].units=game->units;pending[0].roster_count=game->roster_count;
    memcpy(pending[0].roster,game->roster,sizeof(game->roster));
    memcpy(pending[0].state,game->cell_action_completed,sizeof(pending[0].state));
    memset(pending[0].known,255,sizeof(pending[0].known));
    pending[0].pc=flow->entry;pending[0].map_entry=pending[0].shape_index=-1;
    size_t used=1;unsigned steps=0;int result=1;
    while(used && result>0) {
        common_event_projection p=pending[--used];
        while(result>0) {
            if(++steps>FD2_FLOW_STEP_BUDGET){result=-1;break;}
            const fd2_static_flow_node *source=fd2_static_flow_at(flow,p.pc);
            if(!source){result=-1;break;}
            if(!strcmp(source->op,"end") || !strcmp(source->op,"fail"))break;
            fd2_static_flow_node n=*source;
            if(!strcmp(n.op,"story_source")) {
                uint32_t value;
                if(fd2_field_event_source_value((fd2_field_event_value_source)n.args[0],game->turn_number,p.state,&value)!=0 || value>UINT16_MAX){result=-1;break;}
                n.op="story";n.args[0]=(int32_t)value;
            }
            unsigned port=FD2_FLOW_NEXT;
            int query=common_event_query_values(&p.units,p.state,game->turn_number,p.roster,p.roster_count,actor,&n,&port);
            if(query<0){result=-1;break;}
            if(query) {
                if(common_event_project_query_unknown(&p,&n,actor)) {
                    if(used==MAX_PATHS){result=-1;break;}
                    pending[used]=p;pending[used++].pc=fd2_static_flow_edge(flow,source,FD2_FLOW_NO);
                    port=FD2_FLOW_YES;
                }
            } else if(common_event_leaf(n.op)) {
                fd2_field_event_resource_view view={&p.units,p.roster,p.roster_count,p.map_entry,p.shape_index,p.map_entry>=0};
                if(!strcmp(n.op,"move")) {
                    const uint8_t *bytes;size_t length;
                    if(n.args[0]<0 || n.args[0]>255 || fd2_field_movement_script_get((uint8_t)n.args[0],&bytes,&length)!=0 ||
                        fd2_field_movement_script_apply(&p.units,bytes,length)!=0){result=-1;break;}
                }
                if(common_event_preflight_leaf_view(game,&n,actor,&view)!=0){result=-1;break;}
                if(!strcmp(n.op,"map_replace")){p.map_entry=n.args[0];p.shape_index=n.args[1];}
                port=!strcmp(n.op,"story") || !strcmp(n.op,"wait_key")?FD2_FLOW_ACK:FD2_FLOW_NEXT;
            } else {
                if(common_event_project_write(game,&p,&n,&port,actor)!=0){result=-1;break;}
                if(!strcmp(n.op,"loot_child"))port=FD2_FLOW_ACK;
                if((!strcmp(n.op,"event_item_append") || !strcmp(n.op,"event_item_remove")) && p.uncertain) {
                    if(used==MAX_PATHS){result=-1;break;}
                    pending[used]=p;pending[used++].pc=fd2_static_flow_edge(flow,source,FD2_FLOW_NO);port=FD2_FLOW_YES;
                }
            }
            p.pc=fd2_static_flow_edge(flow,source,port);
            if(p.pc<0){result=-1;break;}
        }
    }
    free(pending);return result;
}
#endif
