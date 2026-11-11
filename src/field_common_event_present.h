/* G4共用事件的正式Play叶消费者：1956b/196cb、15f84、16559、16c57。
 * 完整编排留common_event冷图；本文件只借既有progress_box/Scene领域叶。
 * 在field_play.c的play_field_loot之后包含；不另建业务owner或替身RNG。 */
#ifndef FD2_FIELD_COMMON_EVENT_PRESENT_H
#define FD2_FIELD_COMMON_EVENT_PRESENT_H

static int common_event_message_prepare(const fd2_field_game *game,
        const fd2_static_flow_node *node, field_progress_text *text) {
    if (!game || !node || strcmp(node->op,"message") || !text ||
        !game->font.glyphs || !game->font.glyph_count ||
        game->font.glyph_count>game->font.size/32u || node->args[0]<0 ||
        (node->args[1]!=0 && node->args[1]!=1)) return -1;
    fd2_text_parameters parameters={game->message_number,
        game->message_new_fragment,game->message_old_fragment};
    /* 35854@35916的422名称只读投影；真实53AD9已在cell_item_prepare提交。
     * 起始全图资源检查尚未执行该节点，不能为预检写live owner。 */
    if (!node->args[1] && node->args[0]==422) {
        uint8_t item;
        if (game->event_resume.active && game->event_resume.has_item) item=game->event_resume.item_id;
        else if (fd2_field_event_cell_item(&game->map,game->cursor_cell_x,game->cursor_cell_y,&item)!=0) return -1;
        parameters.new_fragment=181u+item;
    }
    fd2_text_entry primary={0};
    const fd2_text_entry *entry=&game->ui_text;
    if (node->args[1]) {
        if (fd2_text_entry_open_entry(&primary,&game->fdtxt_archive,game->stage+1u)!=0) return -1;
        entry=&primary;
    }
    int result=fd2_text_expand_banked(entry,&game->ui_text,game->font.glyph_count,
        &parameters,(size_t)node->args[0],text);
    fd2_text_entry_close(&primary);
    if (result!=0) return -1;
    for(size_t i=0;i<text->count;i++) if(text->tokens[i]==FIELD_TEXT_SAFE_MINUS) {
        const fd2_image *minus=&game->info_assets.signs[1];
        if(!minus->pixels || minus->width<1 || minus->width>16 || minus->height<1 || minus->height>16) return -1;
    }
    return 0;
}

static int common_event_resources_preflight(const fd2_field_game *game,
        const fd2_static_flow_node *node,const fd2_field_event_resource_view *view,void *userdata) {
    const fd2_archive *fdother=userdata;
    if(!game || !node || !node->op || !fdother) return -1;
    const char *op=node->op;
    if(!strcmp(op,"message")) {
        field_progress_text text;
        return fd2_scene_field_message_validate(fdother)!=0 ? -1:
            common_event_message_prepare(game,node,&text);
    }
    if(!strcmp(op,"notice_actor")) {
        if(node->args[0]<0 || (size_t)node->args[0]>=game->units.count ||
            fd2_scene_field_message_validate(fdother)!=0) return -1;
        for(unsigned frame=0;frame<4;frame++) {
            fd2_image portrait={0};
            int valid=fd2_portrait_decode_frame(&portrait,&game->dato_archive,
                game->units.items[node->args[0]].unit_id,frame)==0 &&
                portrait.width==80 && portrait.height==80;
            fd2_image_free(&portrait);
            if(!valid)return -1;
        }
        return 0;
    }
    if(!strcmp(op,"portrait_frame"))return node->args[0]>=0 && node->args[0]<=3 ? 0:-1;
    if(!strcmp(op,"wait_key"))return node->args[0]==0 || node->args[0]==1 ? 0:-1;
    if(!strcmp(op,"clear_keys") || !strcmp(op,"close_box"))return 0;
    if(!strcmp(op,"delay_ms"))return node->args[0]>0 && node->args[0]<=1000?0:-1;
    if(!strcmp(op,"arrival_prepare") || !strcmp(op,"arrival_frames"))
        return fd2_scene_arrival_visual_validate(fdother);
    if(!strcmp(op,"arrival_close"))return 0;
    return view?fd2_scene_common_step_validate_view(game,fdother,node,view->units,view->roster,view->roster_count):
        fd2_scene_common_step_validate(game,fdother,node);
}

static int play_common_events(fd2_field_game *game,fd2_vga *vga,
        const fd2_archive *fdother,fd2_field_audio *audio,int fast) {
    field_progress_box box={0};
    fd2_scene_arrival_visual *arrival=NULL;
    int result=0;
    while(game->event_resume.active) {
        if((result=play_field_terminal_result(game,vga))!=0)break;
        if(game->event_resume.child_wait) {
            if(box.page){result=-1;break;}
            result=play_field_loot(game,vga,fdother,audio,fast);
            if(result!=0)break;
            continue;
        }
        fd2_static_flow_node node;size_t actor;
        if(fd2_field_game_common_event_peek(game,&node,&actor)!=1){result=-1;break;}
        unsigned port=FD2_FLOW_NEXT;
        if(!strcmp(node.op,"clear_keys"))fd2_input_clear_keys(&vga->input);
        else if(!strcmp(node.op,"delay_ms"))result=loot_fixed_wait(vga,(uint32_t)node.args[0],fast);
        else if(!strcmp(node.op,"arrival_prepare")) {
            if(box.page || arrival){result=FD2_SCENE_RESULT_ERROR;break;}
            result=fd2_scene_arrival_visual_prepare(&arrival,vga,fdother,game->units.count);
        } else if(!strcmp(node.op,"arrival_frames")) {
            if(box.page || !arrival){result=FD2_SCENE_RESULT_ERROR;break;}
            result=fd2_scene_arrival_visual_play(arrival,vga,game,audio,fast);
        } else if(!strcmp(node.op,"arrival_close"))fd2_scene_arrival_visual_close(&arrival);
        else if(!strcmp(node.op,"notice_actor")) {
            size_t selected=node.args[0]==-1 ? actor:(size_t)node.args[0];
            if(box.page || selected>=game->units.count){result=-1;break;}
            fd2_field_game_render(game,vga);
            result=progress_box_open(&box,game,vga,fdother,selected,fast);
        } else if(!strcmp(node.op,"message")) {
            field_progress_text text;
            if(!box.page || common_event_message_prepare(game,&node,&text)!=0){result=-1;break;}
            result=field_text_play(&box,game,vga,fdother,audio,&text,node.args[2],node.args[3],fast);
        } else if(!strcmp(node.op,"portrait_frame")) {
            if(!box.page || node.args[0]<0 || node.args[0]>3){result=-1;break;}
            progress_portrait(&box,vga,(unsigned)node.args[0],0);
        } else if(!strcmp(node.op,"wait_key")) {
            if(!box.page){result=-1;break;}
            result=progress_wait(&box,game,vga,fdother,fast,node.args[0]);port=FD2_FLOW_ACK;
        } else if(!strcmp(node.op,"close_box")) {
            if(!box.page){result=-1;break;}
            result=progress_box_transition(&box,vga,0,fast);
            if(!result){memcpy(vga->framebuffer,box.background,VGA_W*VGA_H);progress_box_free(&box);}
        } else {
            if(box.page || arrival){result=-1;break;}
            result=fd2_scene_play_common_step(vga,fdother,game,&node,audio,fast);
            if(!strcmp(node.op,"story"))port=FD2_FLOW_ACK;
        }
        if(result!=0)break;
        if((result=play_field_terminal_result(game,vga))!=0)break;
        if(fd2_field_game_common_event_reply(game,port)!=0){result=-1;break;}
        if((result=play_field_terminal_result(game,vga))!=0)break;
    }
    /* HOST_QUIT 与 ERROR 都是负值，但关闭窗口不走事件错误边：保留
     * 已提交前缀和当前叶，使用现有 owner 阻止后续回复推进游标。 */
    if(result==FD2_SCENE_RESULT_HOST_QUIT)game->runtime_host_quit=1;
    if(result==FD2_SCENE_RESULT_ERROR && game->event_resume.active && !game->event_resume.child_wait)
        (void)fd2_field_game_common_event_reply(game,FD2_FLOW_ERROR);
    if(!result && box.page)result=-1;
    if(!result && arrival)result=-1;
    fd2_scene_arrival_visual_close(&arrival);
    progress_box_free(&box);
    return result;
}

#endif
