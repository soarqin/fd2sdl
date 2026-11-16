/* field_defeated_units_finalize @0x1db65（原登记同名）：完整857B（0x359）。
 * runtime.defeat决定13旋转／12消散／等待与提交顺序；Game只写方向/flags，
 * Play借现runtime callback拥有本次临时视觉页，不增加热PC或SAV状态。 */
#ifndef FD2_FIELD_DEFEAT_FLOW_H
#define FD2_FIELD_DEFEAT_FLOW_H

static int defeat_has_visible(const fd2_field_game *game) {
    for(size_t i=0;i<game->units.count;i++) {
        const fd2_field_unit *u=&game->units.items[i];
        if(!u->hp && !fd2_field_unit_is_hidden(u) &&
            u->x>=(int64_t)game->camera_cell_x-1 &&
            u->x<=(int64_t)game->camera_cell_x+FD2_FIELD_VIEW_CELLS_X &&
            u->y>=(int64_t)game->camera_cell_y-1 &&
            u->y<=(int64_t)game->camera_cell_y+FD2_FIELD_VIEW_CELLS_Y+1)return 1;
    }
    return 0;
}

static int finalize_zero_hp_units(fd2_field_game *game) {
    if(!game || game->units.count>FD2_FIELD_MAX_UNITS)return -1;
    if(game->runtime_host_quit)return 1;
    const fd2_static_flow *flow=fd2_static_flow_lookup("runtime","runtime.defeat");
    if(!flow)return -1;
    int pc=flow->entry,result=-1,opened=0;
    for(unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if(!node || !(node->views&FD2_FLOW_LOGIC))break;
        const char *op=node->op;unsigned port=FD2_FLOW_NEXT;
        int step=-1,value=0;
        if(!strcmp(op,"defeat_has_visible"))port=defeat_has_visible(game)?FD2_FLOW_YES:FD2_FLOW_NO;
        else if(!strcmp(op,"defeat_rotate_actors")) {
            if(node->args[0]<0 || node->args[0]>3)break;
            for(size_t i=0;i<game->units.count;i++) {
                fd2_field_unit *u=&game->units.items[i];
                if(fd2_field_unit_is_hidden(u))continue;
                if(!u->hp)u->direction=(uint8_t)node->args[0];
                int shown=turn_present(game,FD2_FIELD_RUNTIME_DEFEAT_ROTATE_ACTOR,(int)i);
                if(shown<0)goto cleanup;
                if(shown>0) {result=1;goto cleanup;}
            }
        } else if(!strcmp(op,"defeat_mark_hidden")) {
            for(size_t i=0;i<game->units.count;i++)
                if(!game->units.items[i].hp)game->units.items[i].flags=1;
        } else if(!strcmp(op,"defeat_begin")) {opened=1;step=FD2_FIELD_RUNTIME_DEFEAT_BEGIN;}
        else if(!strcmp(op,"defeat_rotate_page"))step=FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PAGE;
        else if(!strcmp(op,"defeat_rotate_project"))step=FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PROJECT;
        else if(!strcmp(op,"defeat_base"))step=FD2_FIELD_RUNTIME_DEFEAT_BASE;
        else if(!strcmp(op,"defeat_cue"))step=FD2_FIELD_RUNTIME_DEFEAT_CUE;
        else if(!strcmp(op,"defeat_frame")) {
            if(node->args[0]<68 || node->args[0]>79)break;
            step=FD2_FIELD_RUNTIME_DEFEAT_FRAME;value=node->args[0];
        } else if(!strcmp(op,"defeat_wait")) {
            if(node->args[0]!=1)break;
            step=FD2_FIELD_RUNTIME_DEFEAT_WAIT;value=1;
        } else if(!strcmp(op,"defeat_close"))step=FD2_FIELD_RUNTIME_DEFEAT_CLOSE;
        else if(!strcmp(op,"turn_refresh"))step=FD2_FIELD_RUNTIME_REFRESH;
        else if(!strcmp(op,"defeat_finish")) {result=0;break;}
        else if(!strcmp(op,"defeat_fail"))break;
        else break;
        if(step>=0) {
            int shown=turn_present(game,(fd2_field_runtime_present_step)step,value);
            if(step==FD2_FIELD_RUNTIME_DEFEAT_CLOSE)opened=0;
            if(shown<0)port=FD2_FLOW_ERROR;
            else if(shown>0) {result=1;break;}
        }
        pc=fd2_static_flow_edge(flow,node,port);
    }
    /* 退出不回滚已提交方向/flags；CLOSE必须绕过quit短路释放同一视觉scope。 */
cleanup:
    if(opened && game->runtime_present) {
        int closed=game->runtime_present(game,FD2_FIELD_RUNTIME_DEFEAT_CLOSE,0,game->runtime_present_userdata);
        if(closed<0)result=-1;
    }
    return result;
}
#endif
