/* prepare_ui私有输入叶：275e6/27d33/27f4a/2a29d/2a857/2b67f/1b9de。
 * 只取得选择与显示临时页面，业务分支/提交顺序由preparation冷图决定。
 * 本文件在field_prepare_play.c的旧UI helper之后包含，不建立新owner。
 * G3规范命名（完整Domain函数/xref/原资源依据，非ABI推断）：
 * field_prepare_menu_select @0x26e38（原登记 sub_26E38）。
 * field_shop_item_select @0x275e6（原登记 sub_275E6）。
 * field_shop_item_page_open @0x27738（原登记 sub_27738）。
 * field_prepare_member_select @0x27d33（原登记 sub_27D33）。
 * field_shop_equipment_recipient_select @0x27f4a（原登记 sub_27F4A）。
 * field_revive_candidate_select @0x2a29d（原登记 sub_2A29D）。
 * field_promotion_candidate_select @0x2a857（原登记 sub_2A857）。
 * field_deployment_selection_wait_key @0x2b67f（原登记 sub_2B67F）。
 */
#ifndef FD2_FIELD_PREPARE_SERVICE_PICK_H
#define FD2_FIELD_PREPARE_SERVICE_PICK_H

enum { PREPARE_PICK_PRODUCT=1, PREPARE_PICK_ITEM, PREPARE_PICK_MEMBER,
    PREPARE_PICK_BUY_MEMBER, PREPARE_PICK_REVIVE, PREPARE_PICK_PROMOTION,
    PREPARE_PICK_DEPLOYMENT, PREPARE_PICK_MENU };

static int prepare_service_picker_widgets(prepare_ui *ui,int arrows) {
    prepare_service *s=&ui->service;
    const fd2_save_manual_state *state=&ui->context->preparation;
    /* field_prepare_picker_tick_draw @0x27079（原sub_27079）及27d33导航
     * 只覆盖各自成员/列表/图标窗口；保留16559已画的画像和其它VGA历史。
     * 初次开页/关闭仍由modal snapshot负责，不能每tick恢复整页再补portrait0。
     * field_deployment_selection_draw @0x2b4fb（原sub_2B4FB）另有显式
     * memmove(53C63,base,64000)，仅该owner保留全页基底重建。 */
    if (s->picker_mode==PREPARE_PICK_DEPLOYMENT && s->page_background)
        memcpy(ui->vga->framebuffer,s->page_background,VGA_W*VGA_H);
    int result=-1; size_t first=s->first,count=0,visible=6;
    switch (s->picker_mode) {
        case PREPARE_PICK_PRODUCT:
            first=(size_t)s->product_first;count=s->stock_count;
            result=fd2_field_prepare_draw_items(ui->vga,ui->assets,s->stock,count,first,s->product_index,0);break;
        case PREPARE_PICK_ITEM:
            count=s->item_count;
            /* 28cbd/28f65都给27738/275e6第三参1：出售及转交库存显示3/4价。 */
            result=fd2_field_prepare_draw_items(ui->vga,ui->assets,s->visible_items,count,first,s->raw_slot,1);break;
        case PREPARE_PICK_MEMBER:
            count=state->unit_count;
            result=fd2_field_prepare_draw_members(ui->vga,ui->assets,state,first,s->cursor,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_BUY_MEMBER:
            visible=3;count=s->candidate_count;
            result=fd2_field_prepare_draw_equipment_recipients(ui->vga,ui->assets,state,
                s->candidates,count,first,s->cursor,s->product_id,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_REVIVE:
            visible=3;count=s->candidate_count;
            result=fd2_field_prepare_draw_revive_candidates(ui->vga,ui->assets,state,
                s->candidates,count,first,s->cursor,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_PROMOTION:
            visible=3;count=s->candidate_count;
            result=fd2_field_prepare_draw_promotion_candidates(ui->vga,ui->assets,state,
                s->candidates,count,first,s->cursor,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_DEPLOYMENT: {
            /* 2b4fb调用1297d，共享App idle/cursor；不另持5tick sprite相位。 */
            if(fd2_field_visual_actor_step_live(&ui->vga->field_visual)<0)return -1;
            result=fd2_field_prepare_draw_deployment(ui->vga,ui->assets,state,s->selection,
                s->quota-1u,(size_t)s->cursor,ui->vga->field_visual.actor_idle_phase,
                &s->selection_label,&s->selection_cursor);
            break;
        }
        case PREPARE_PICK_MENU:
            for (int i=0;i<4;i++) fd2_map_sprite_blit(ui->vga,
                &ui->assets->service.icons[i*2+(i==s->selected_command ? ui->vga->prepare_highlight_phase/2u:0u)],201+i*26,169,0);
            result=0;break;
        default:return -1;
    }
    if(!result && count && arrows)result=prepare_picker_arrows(ui->vga,ui->assets->service.scroll_arrows,
        count,first,visible,ui->vga->prepare_highlight_phase);
    return result;
}
static int prepare_service_picker_draw(prepare_ui *ui) {return prepare_service_picker_widgets(ui,1);}
/* 27079真正的空等写集：mode1只箭头，mode2先箭头再3个sprite，
 * mode3成员sprite/name后箭头。导航的完整widget与初开底板另由caller负责。 */
static int prepare_service_picker_tick_draw(prepare_ui *ui) {
    prepare_service *s=&ui->service;size_t count,first=s->first,visible=6;
    switch(s->picker_mode) {
        case PREPARE_PICK_MENU:
            if(s->selected_command<0 || s->selected_command>=4)return -1;
            fd2_map_sprite_blit(ui->vga,&ui->assets->service.icons[2*s->selected_command+
                ui->vga->prepare_highlight_phase/2u],201+26*s->selected_command,169,0);return 0;
        case PREPARE_PICK_PRODUCT:count=s->stock_count;first=(size_t)s->product_first;break;
        case PREPARE_PICK_ITEM:count=s->item_count;break;
        case PREPARE_PICK_MEMBER:
            return prepare_service_picker_draw(ui);
        case PREPARE_PICK_BUY_MEMBER:case PREPARE_PICK_REVIVE:case PREPARE_PICK_PROMOTION:
            count=s->candidate_count;visible=3;
            if(prepare_picker_arrows(ui->vga,ui->assets->service.scroll_arrows,count,first,visible,
                    ui->vga->prepare_highlight_phase)!=0)return -1;
            return fd2_field_prepare_draw_candidate_sprites(ui->vga,ui->assets,&ui->context->preparation,
                s->candidates,count,first,ui->vga->prepare_sprite_phase);
        default:return -1;
    }
    return prepare_picker_arrows(ui->vga,ui->assets->service.scroll_arrows,count,first,visible,
        ui->vga->prepare_highlight_phase);
}

static int prepare_service_picker_open(prepare_ui *ui) {
    prepare_service *s=&ui->service;
    if (s->page || s->page_background) return -1;
    fd2_vga *page=new_frame(ui->vga);if(!page)return -1;
    if(s->picker_mode<PREPARE_PICK_PRODUCT || s->picker_mode>PREPARE_PICK_PROMOTION ||
        !image_size(&ui->assets->service.roster_page,310,86)){free(page);return -1;}
    /* 27738/27d33/27f4a/2a29d/2a857初开唯一LMI底板；尚未到26eda箭头。 */
    fd2_map_sprite_blit(page,&ui->assets->service.roster_page,5,112,-1);
    /* 原选择页在底框出现前先完整预检绘制，失败不展示空白占位。 */
    fd2_vga *live=ui->vga;ui->vga=page;
    int result=prepare_service_picker_widgets(ui,0);ui->vga=live;
    if (!result) result=prepare_service_modal_open(ui,page,2);
    free(page);return result;
}

static int prepare_service_picker_key(prepare_ui *ui,int *selected,size_t count,
        size_t *first,size_t columns,fd2_input_key key,int move_sound) {
    if (!count || !selected || !first || *selected<0 || (size_t)*selected>=count) return -1;
    int previous=*selected,next=previous;
    if (columns==2u) {
        if (roster_step(key,count,&next,first)!=0) return -1;
    } else {
        if (key==FD2_INPUT_KEY_UP && next>0) next--;
        else if (key==FD2_INPUT_KEY_DOWN && (size_t)next+1u<count) next++;
        if ((size_t)next<*first) *first=(size_t)next;
        else if ((size_t)next>=*first+3u) *first=(size_t)next-2u;
    }
    *selected=next;
    if (move_sound && previous!=next && sound(ui,0)!=0) return -1;
    return 0;
}

static int prepare_service_pick_list(prepare_ui *ui,unsigned *port) {
    prepare_service *s=&ui->service; const fd2_save_manual_state *state=&ui->context->preparation;
    size_t count=s->picker_mode==PREPARE_PICK_PRODUCT ? s->stock_count:
        s->picker_mode==PREPARE_PICK_ITEM ? s->item_count:
        s->picker_mode==PREPARE_PICK_MEMBER ? state->unit_count:s->candidate_count;
    size_t columns=s->picker_mode>=PREPARE_PICK_BUY_MEMBER ? 1u:2u;
    int *selected=s->picker_mode==PREPARE_PICK_PRODUCT ? &s->product_index:
        s->picker_mode==PREPARE_PICK_ITEM ? &s->raw_slot:&s->cursor;
    if (!count) { *port=FD2_FLOW_CANCEL;return 1; }
    if (*selected<0 || (size_t)*selected>=count) *selected=0;
    int opened=prepare_service_picker_open(ui);
    if(opened){if(opened==1)s->host_quit=1;*port=FD2_FLOW_ERROR;return -1;}
    /* 275e6/27d33/27f4a/2a29d/2a857及27738/1974c开页均无4e381。 */
    uint64_t host_deadline=0;
    for (;;) {
        fd2_input_event event;
        int result=prepare_service_read_key(ui,&event,columns==1u ? 2: s->picker_mode==PREPARE_PICK_MEMBER ? 3:1,
            prepare_service_picker_tick_draw);
        prepare_host_release(ui,&host_deadline);
        if(result){if(result==1)s->host_quit=1;*port=FD2_FLOW_ERROR;return -1;}
        if(cancel_key(event.key)){*port=FD2_FLOW_CANCEL;return 1;}
        if(confirm_key(event.key)) {
            if(s->picker_mode==PREPARE_PICK_PRODUCT)s->product_id=s->stock[s->product_index];
            *port=FD2_FLOW_SLOT;return 1;
        }
        size_t first=s->picker_mode==PREPARE_PICK_PRODUCT ? (size_t)s->product_first:s->first;
        size_t previous_first=first;
        int move_sound=s->picker_mode!=PREPARE_PICK_REVIVE && s->picker_mode!=PREPARE_PICK_PROMOTION;
        if(prepare_service_picker_key(ui,selected,count,&first,columns,event.key,move_sound)!=0)return -1;
        uint8_t *before=NULL;
        if(first!=previous_first) {
            before=malloc(VGA_W*VGA_H);if(!before)return -1;
            memcpy(before,ui->vga->framebuffer,VGA_W*VGA_H);
        }
        if(s->picker_mode==PREPARE_PICK_PRODUCT)s->product_first=(int)first;else s->first=first;
        fd2_vga *next=before ? new_frame(ui->vga):NULL;
        if(before && !next){free(before);return -1;}
        fd2_vga *live=ui->vga;
        if(next) {
            if(prepare_picker_shift_rows(next->framebuffer,first>previous_first,26)!=0){free(next);free(before);return -1;}
            ui->vga=next;
        }
        int drawn=prepare_service_picker_widgets(ui,0);ui->vga=live;
        if(drawn){free(next);free(before);return -1;}
        if(before) {
            int scrolling=prepare_service_scroll_rows(ui,before,next->framebuffer,first>previous_first);
            free(next);free(before);
            if(scrolling){if(scrolling==1)s->host_quit=1;return -1;}
        }
        if(prepare_host_present(ui,&host_deadline)){s->host_quit=1;return prepare_host_stop(ui,&host_deadline,-1);}
    }
}

static int prepare_service_selection_resources(prepare_ui *ui) {
    prepare_service *s=&ui->service;
    if(!s->selection_label.pixels && service_frame(&s->selection_label,ui->fdother,5,137,0)!=0)return -1;
    if(!s->selection_cursor.pixels) {
        fd2_shape_sheet sheet={0};
        if(fd2_shape_sheet_open_entry(&sheet,ui->fdother,1)!=0)return -1;
        int result=fd2_shape_sheet_decode_frame(&s->selection_cursor,&sheet,0);
        fd2_shape_sheet_close(&sheet);if(result)return -1;
    }
    return image_size(&s->selection_label,86,86) && image_size(&s->selection_cursor,24,24) ? 0:-1;
}

static int prepare_service_selection_open(prepare_ui *ui) {
    prepare_service *s=&ui->service;
    if(s->page || s->page_background || prepare_service_selection_resources(ui)!=0)return -1;
    s->page_background=malloc(VGA_W*VGA_H);s->page=new_frame(ui->vga);
    if(!s->page_background || !s->page)return -1;
    memcpy(s->page_background,ui->vga->framebuffer,VGA_W*VGA_H);
    s->page_kind=3;s->picker_mode=PREPARE_PICK_DEPLOYMENT;
    if(prepare_service_picker_draw(ui)!=0)return -1;
    memcpy(s->page->framebuffer,ui->vga->framebuffer,VGA_W*VGA_H);
    for(int phase=11;phase>=0;phase--) {
        int cue=fd2_field_detail_sfx_for_phase(1,phase);if(cue>=0 && sound(ui,(size_t)cue)!=0)return -1;
        fd2_field_detail_transition_frame(ui->vga->framebuffer,s->page->framebuffer,s->page_background,phase);
        if(present_copy(ui)){s->host_quit=1;return -1;}
    }
    /* 2af28打开12帧后明确4e381，后继2b67f只消费一键。 */
    return prepare_service_clear_keys(ui) ? -1 : 0;
}

static int prepare_service_selection_close(prepare_ui *ui) {
    prepare_service *s=&ui->service;
    if(!s->page || !s->page_background || s->page_kind!=3)return -1;
    if(prepare_service_picker_draw(ui)!=0)return -1;
    memcpy(s->page->framebuffer,ui->vga->framebuffer,VGA_W*VGA_H);
    for(int phase=0;phase<12;phase++) {
        int cue=fd2_field_detail_sfx_for_phase(0,phase);if(cue>=0 && sound(ui,(size_t)cue)!=0)return -1;
        fd2_field_detail_transition_frame(ui->vga->framebuffer,s->page->framebuffer,s->page_background,phase);
        if(present_copy(ui)){s->host_quit=1;return -1;}
    }
    memcpy(ui->vga->framebuffer,s->page_background,VGA_W*VGA_H);
    if(present_copy(ui)){s->host_quit=1;return -1;}
    free(s->page_background);free(s->page);s->page_background=NULL;s->page=NULL;s->page_kind=0;
    return 0;
}

static int prepare_service_equipment_draw(prepare_ui *ui) {
    prepare_service *s=&ui->service;fd2_field_unit unit;
    const fd2_save_manual_state *state=&ui->context->preparation;
    if(s->member<0 || (size_t)s->member>=state->unit_count)return -1;
    memcpy(&unit,state->unit_area+(size_t)s->member*80u,80);
    if(fd2_field_detail_draw(ui->vga,&ui->assets->info,&ui->assets->font,&ui->assets->text,&unit,&s->equipment_portraits[0])!=0)return -1;
    if(fd2_field_item_list_draw(ui->vga,&ui->assets->info,&ui->assets->font,&ui->assets->text,
            &unit,s->equipment_selected)!=0)return -1;
    return 0;
}

/* 1b9de的53C57是DWORD；空EQT的导航也保原signed比较／DWORD加减。
 * 显式解码二补码，不让C signed overflow或size_t(count-1)改原轨迹。 */
static int prepare_equipment_cursor_add(int cursor,int delta) {
    uint32_t bits=(uint32_t)cursor+(uint32_t)delta;
    return bits<=INT32_MAX ? (int)bits:-1-(int)(UINT32_MAX-bits);
}

static int prepare_service_pick(prepare_ui *ui,const fd2_static_flow_node *node,unsigned *port) {
    prepare_service *s=&ui->service;const char *op=node->op;
    *port=FD2_FLOW_NEXT;
    if(!strcmp(op,"menu_pick")) {
        s->picker_mode=PREPARE_PICK_MENU; /* 26e38无入口清队。 */
        for(;;) {
            fd2_input_event event;
            int result=prepare_service_read_key(ui,&event,0,prepare_service_picker_tick_draw);
            if(result){if(result==1)s->host_quit=1;return -1;}
            if(cancel_key(event.key)){*port=FD2_FLOW_CANCEL;return 1;}
            if(confirm_key(event.key)){*port=FD2_FLOW_SLOT;return 1;}
            if(event.key==FD2_INPUT_KEY_LEFT || event.key==FD2_INPUT_KEY_RIGHT) {
                s->selected_command=(s->selected_command+(event.key==FD2_INPUT_KEY_LEFT ? 3:1))%4;
                if(sound(ui,0)!=0)return -1;
            }
        }
    }
    if(!strcmp(op,"pick_product")) {s->picker_mode=PREPARE_PICK_PRODUCT;return prepare_service_pick_list(ui,port);}
    if(!strcmp(op,"pick_item")) {
        s->item_kind=node->args[0];s->raw_slot=0;s->first=0;s->picker_mode=PREPARE_PICK_ITEM;
        return prepare_service_pick_list(ui,port);
    }
    if(!strcmp(op,"pick_member")) {
        s->member_kind=node->args[0];s->cursor=0;s->first=0;
        const uint8_t *item=fd2_field_item_record_get(s->product_id);
        s->picker_mode=s->member_kind==3 ? PREPARE_PICK_REVIVE:s->member_kind==4 ? PREPARE_PICK_PROMOTION:
            s->member_kind==1 && item && item[0]<32u ? PREPARE_PICK_BUY_MEMBER:PREPARE_PICK_MEMBER;
        int result=prepare_service_pick_list(ui,port);
        if(result==1 && *port==FD2_FLOW_SLOT) {
            int actor=s->picker_mode==PREPARE_PICK_MEMBER ? s->cursor:s->candidates[s->cursor];
            if(s->member_kind==2)s->receiver=actor;else s->member=actor;
            if(s->member_kind==4 && fd2_field_prepare_promotion_plan(&ui->context->preparation,
                    (size_t)actor,&s->promotion)!=0)return -1;
        }
        return result;
    }
    if(!strcmp(op,"selection_open"))return prepare_service_selection_open(ui)==0 ? 1:-1;
    if(!strcmp(op,"selection_close"))return prepare_service_selection_close(ui)==0 ? 1:-1;
    if(!strcmp(op,"pick_companion")) {
        uint64_t host_deadline=0;
        for(;;) {
            if(quit(ui)){s->host_quit=1;return prepare_host_stop(ui,&host_deadline,-1);}
            fd2_input_event event;
            if(fd2_input_take_key(&ui->vga->input,&event)) {
                s->input_epoch++;
                prepare_host_release(ui,&host_deadline); /* 2b67f读一，不持有H帧或吞余队。 */
                if(cancel_key(event.key)){*port=FD2_FLOW_CANCEL;return prepare_host_stop(ui,&host_deadline,1);}
                if(confirm_key(event.key) || (event.bios_code&0xffu)==0x20u){*port=FD2_FLOW_SLOT;return prepare_host_stop(ui,&host_deadline,1);}
                fd2_field_prepare_select_key key;
                if(event.key==FD2_INPUT_KEY_LEFT)key=FD2_FIELD_PREPARE_SELECT_LEFT;
                else if(event.key==FD2_INPUT_KEY_RIGHT)key=FD2_FIELD_PREPARE_SELECT_RIGHT;
                else if(event.key==FD2_INPUT_KEY_UP)key=FD2_FIELD_PREPARE_SELECT_UP;
                else if(event.key==FD2_INPUT_KEY_DOWN)key=FD2_FIELD_PREPARE_SELECT_DOWN;
                else continue;
                int moved=fd2_field_prepare_selection_step(ui->context->preparation.unit_count,s->selection,&s->cursor,key);
                if(moved<0 || (moved && sound(ui,0)!=0))return prepare_host_stop(ui,&host_deadline,-1);
                if(prepare_service_picker_draw(ui)!=0)return prepare_host_stop(ui,&host_deadline,-1);
            }
            uint32_t tick=(uint32_t)fd2_bios_tick_signed_ns(SDL_GetTicksNS());
            if(tick!=ui->vga->prepare_pick_tick) {
                ui->vga->prepare_pick_tick=(uint32_t)fd2_bios_tick_signed_ns(SDL_GetTicksNS());
                /* 2b67f先写53F46再call2b4fb；下游失败保已到达的App写点。 */
                if(prepare_service_picker_draw(ui)!=0)return prepare_host_stop(ui,&host_deadline,-1);
            }
            if(prepare_host_present(ui,&host_deadline)){s->host_quit=1;return prepare_host_stop(ui,&host_deadline,-1);}
        }
    }
    if(!strcmp(op,"equipment_open")) {
        const fd2_save_manual_state *state=&ui->context->preparation;
        if(s->page || s->page_background || s->member<0 || (size_t)s->member>=state->unit_count)return -1;
        uint8_t id=state->unit_area[(size_t)s->member*80u+7u];
        for(size_t frame=0;frame<4u;frame++)
            if(fd2_portrait_decode_frame(&s->equipment_portraits[frame],&ui->assets->dato,id,frame)!=0 ||
                !image_size(&s->equipment_portraits[frame],80,80))return -1;
        s->equipment_previous_portraits=s->active_portraits;s->equipment_previous_origin=s->notice_origin;
        s->active_portraits=s->equipment_portraits;s->notice_origin=3208u;
        s->equipment_selected=-1;s->page_background=malloc(VGA_W*VGA_H);s->page=new_frame(ui->vga);
        if(!s->page_background || !s->page)return -1;
        memcpy(s->page_background,ui->vga->framebuffer,VGA_W*VGA_H);s->page_kind=4;
        if(prepare_service_equipment_draw(ui)!=0)return -1;
        memcpy(s->page->framebuffer,ui->vga->framebuffer,VGA_W*VGA_H);
        for(int phase=11;phase>=0;phase--) {
            int cue=fd2_field_detail_sfx_for_phase(1,phase);if(cue>=0 && sound(ui,(size_t)cue)!=0)return -1;
            fd2_field_detail_transition_frame(ui->vga->framebuffer,s->page->framebuffer,s->page_background,phase);
            if(present_copy(ui)){s->host_quit=1;return -1;}
        }
        if(prepare_service_clear_keys(ui))return -1; /* 17e0b尾17ee8。 */
        s->equipment_selected=0;return 1;
    }
    if(!strcmp(op,"pick_equipment_slot")) {
        fd2_field_unit unit;const fd2_save_manual_state *state=&ui->context->preparation;
        memcpy(&unit,state->unit_area+(size_t)s->member*80u,80);
        size_t count=fd2_field_unit_inventory_count(&unit);
        if(count && (s->equipment_selected<0 || (size_t)s->equipment_selected>=count))s->equipment_selected=0;
        for(;;) {
            memcpy(&unit,state->unit_area+(size_t)s->member*80u,80);
            if(fd2_field_item_list_draw(ui->vga,&ui->assets->info,&ui->assets->font,&ui->assets->text,
                    &unit,count?s->equipment_selected:-1)!=0)return -1;
            fd2_input_event event;
            int result=prepare_service_wait_key(ui,&event,0); /* 16c57，保共享RNG及按键 */
            if(result){if(result==1)s->host_quit=1;return -1;}
            if(cancel_key(event.key)){*port=FD2_FLOW_CANCEL;return 1;}
            if(confirm_key(event.key)){s->raw_slot=s->equipment_selected;*port=FD2_FLOW_SLOT;return 1;}
            int old=s->equipment_selected;
            if(event.key==FD2_INPUT_KEY_UP)s->equipment_selected=old ? prepare_equipment_cursor_add(old,-1):(int)count-1;
            else if(event.key==FD2_INPUT_KEY_DOWN)s->equipment_selected=old==(int)count-1 ? 0:prepare_equipment_cursor_add(old,1);
            else if(event.key==FD2_INPUT_KEY_LEFT && old>=4)s->equipment_selected=prepare_equipment_cursor_add(old,-4);
            else if(event.key==FD2_INPUT_KEY_RIGHT && old<=3 && old<(int)count-4)s->equipment_selected=prepare_equipment_cursor_add(old,4);
            if((s->equipment_selected!=old || event.key==FD2_INPUT_KEY_UP || event.key==FD2_INPUT_KEY_DOWN) && sound(ui,0)!=0)return -1;
        }
    }
    if(!strcmp(op,"equipment_redraw"))return prepare_service_equipment_draw(ui)==0 ? 1:-1;
    if(!strcmp(op,"equipment_close")) {
        if(!s->page || !s->page_background || s->page_kind!=4)return -1;
        for(int phase=0;phase<12;phase++) {
            fd2_field_detail_transition_frame(ui->vga->framebuffer,s->page->framebuffer,s->page_background,phase);
            if(present_copy(ui)){s->host_quit=1;return -1;}
        }
        memcpy(ui->vga->framebuffer,s->page_background,VGA_W*VGA_H);
        if(present_copy(ui)){s->host_quit=1;return -1;}
        free(s->page_background);free(s->page);s->page_background=NULL;s->page=NULL;s->page_kind=0;
        s->active_portraits=s->equipment_previous_portraits;s->notice_origin=s->equipment_previous_origin;
        s->equipment_previous_portraits=NULL;
        for(size_t frame=0;frame<4u;frame++)fd2_image_free(&s->equipment_portraits[frame]);
        return 1;
    }
    if(!strcmp(op,"detail_member")) {
        if(s->member<0)return -1;
        int result=member_detail(ui,(size_t)s->member);if(result==1)s->host_quit=1;return result==0 ? 1:-1;
    }
    if(!strcmp(op,"manual_save")) {
        /* caller_kind来自冷图：0=26152 NoTown直键/无660，1=29300酒店。 */
        if(node->args[0]<0 || node->args[0]>1){*port=FD2_FLOW_ERROR;return -1;}
        int result=slots_run(ui,1,node->args[0] ? PREPARE_SLOT_HOTEL:PREPARE_SLOT_NOTOWN_SAVE);
        if(result==1)s->host_quit=1;return result==0 ? 1:-1;
    }
    return 0;
}
#endif
