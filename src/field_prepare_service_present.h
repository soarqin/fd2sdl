/* 准备UI的private有限呈现叶；仅由field_prepare_play.c在prepare_ui及旧
 * helpers之后include。来源：15f84/164e8/16559/16c57、1956b/1974c、
 * 19953/197e5、2670e/26996/26a7a/26b91/26c9b/26ce4/26eda/28b41/2921a。
 * 分支、等待节点、业务写点及取消去向唯一由preparation冷图掌握。
 * 此处不拥有第二份roster/资金/RNG，不在cleanup回滚已经提交的业务。
 * G3规范命名（完整Domain函数/xref/原资源依据，非ABI推断）：
 * field_prepare_money_add_present @0x26a7a（原登记 sub_26A7A）。
 * field_prepare_money_subtract_present @0x26b91（原登记 sub_26B91）。
 * field_prepare_money_strip_draw @0x26c9b（原登记 sub_26C9B）。
 * field_prepare_menu_transition @0x26ce4（原登记 sub_26CE4）。
 * field_prepare_picker_wait_key @0x26eda（原登记 sub_26EDA）。
 * field_prepare_picker_tick_draw @0x27079（原登记 sub_27079）。
 * field_prepare_rows_shift_up @0x27816（原登记 sub_27816）。
 * field_prepare_rows_shift_down @0x278e7（原登记 sub_278E7）。
 * field_prepare_service_animate @0x28b41（原登记 sub_28B41）。
 */
#ifndef FD2_FIELD_PREPARE_SERVICE_PRESENT_H
#define FD2_FIELD_PREPARE_SERVICE_PRESENT_H

#include "field_visual.h"

static int prepare_service_host_quit(prepare_ui *ui) {
    if (ui->service.host_quit || quit(ui)) {ui->service.host_quit=1;return 1;}
    return 0;
}
static int prepare_service_take_key(prepare_ui *ui,fd2_input_event *event) {
    prepare_service *s=&ui->service;
    if(s->deferred_key_ready){*event=s->deferred_key;s->deferred_key_ready=0;return 1;}
    if(!fd2_input_take_key(&ui->vga->input,event))return 0;
    s->input_epoch++;return 1;
}
static void prepare_service_clear_keys(prepare_ui *ui) {
    ui->service.deferred_key_ready=0;drain(ui);
}
static int prepare_service_present(prepare_ui *ui,unsigned ms) {
    if (prepare_service_host_quit(ui)) return 1;
    fd2_vga_present_timed(ui->vga,ms);
    fd2_vga_wait_frame_deadline(ui->vga);
    return prepare_service_host_quit(ui);
}
static int prepare_service_fade(prepare_ui *ui,int in) {
    for (int value=in ? 63:0;in ? value>=0:value<64;value+=in ? -1:1) {
        fd2_vga_set_brightness(ui->vga,value);
        if (prepare_service_present(ui,2)) return 1;
    }
    return 0;
}

/* 1956b初帧总reverse；16559仅36887继续reverse，特殊origin正向。
 * 不用普通image水平翻转后+x定位替代原右端坐标，裁掉原可见页外像素。 */
static int prepare_service_portrait_at(fd2_vga *dst,const fd2_image *portraits,
        unsigned origin,unsigned frame,int opening) {
    if (!portraits) return 0;
    if (frame>3u || !image_size(&portraits[frame],80,80)) return -1;
    const fd2_image *image=&portraits[frame];
    int x=(int)(origin%VGA_STRIDE),y=(int)(origin/VGA_STRIDE);
    int reverse=opening || origin==36887u;
    for (int row=0;row<80;row++) for (int col=0;col<80;col++) {
        int px=reverse ? x-col:x+col;
        if (px>=0 && px<VGA_W && y+row>=0 && y+row<VGA_H)
            dst->framebuffer[(y+row)*VGA_STRIDE+px]=image->pixels[row*80+col];
    }
    return 0;
}
static int prepare_service_portrait(prepare_ui *ui,fd2_vga *dst,unsigned frame,int opening) {
    return prepare_service_portrait_at(dst,ui->service.active_portraits,
        ui->service.notice_origin,frame,opening);
}
static void prepare_service_portraits_free(prepare_service *s) {
    for (size_t i=0;i<4u;i++) fd2_image_free(&s->notice_portraits[i]);
    s->active_portraits=NULL;s->notice_open=0;
}

/* 1974c/26996只滑静态初页，不增5/6音效或战场11cac尾。 */
static int prepare_service_modal_transition(prepare_ui *ui,int opening) {
    prepare_service *s=&ui->service;
    if (!s->page || !s->page_background) return -1;
    for (int phase=opening ? 5:1;opening ? phase>=0:phase<6;phase+=opening ? -1:1) {
        int y=112+phase*13;
        memcpy(ui->vga->framebuffer,s->page_background,VGA_W*VGA_H);
        for (int row=0;row<86 && y+row<VGA_H;row++)
            memcpy(ui->vga->framebuffer+(y+row)*VGA_STRIDE+5,
                s->page->framebuffer+(112+row)*VGA_STRIDE+5,310);
        if (prepare_service_present(ui,16)) return 1;
    }
    if (!opening) memcpy(ui->vga->framebuffer,s->page_background,VGA_W*VGA_H);
    return 0;
}
static int prepare_service_modal_open(prepare_ui *ui,const fd2_vga *page,int page_kind) {
    prepare_service *s=&ui->service;
    if (!page || s->page || s->page_background) return -1;
    uint8_t *background=malloc(VGA_W*VGA_H);fd2_vga *copy=new_frame(page);
    if (!background || !copy) {free(background);free(copy);return -1;}
    memcpy(background,ui->vga->framebuffer,VGA_W*VGA_H);
    s->page_background=background;s->page=copy;s->page_kind=page_kind;
    return prepare_service_modal_transition(ui,1);
}
static int prepare_service_modal_close(prepare_ui *ui) {
    prepare_service *s=&ui->service;int result=0;
    if (s->page && s->page_background) result=prepare_service_modal_transition(ui,0);
    else if (s->page || s->page_background) result=-1;
    free(s->page);free(s->page_background);s->page=NULL;s->page_background=NULL;s->page_kind=0;
    /* 26996只释放三页；53A85/53C67仍由后续26eda列表等待继续借用。 */
    s->notice_open=0;return result;
}
static void prepare_service_cleanup(prepare_ui *ui) {
    if (!ui) return;
    prepare_service *s=&ui->service;
    if (s->promotion_visual) fd2_prepare_promotion_close(s->promotion_visual,1);
    s->promotion_visual=NULL;
    fd2_archive_close(&s->promotion_figani);fd2_archive_close(&s->promotion_backgrounds);
    if (s->page_background) memcpy(ui->vga->framebuffer,s->page_background,VGA_W*VGA_H);
    free(s->page);free(s->page_background);free(s->town_background);
    s->page=NULL;s->page_background=NULL;s->town_background=NULL;s->page_kind=0;
    for(size_t i=0;i<4u;i++)fd2_image_free(&s->equipment_portraits[i]);
    s->equipment_previous_portraits=NULL;
    fd2_image_free(&s->selection_label);fd2_image_free(&s->selection_cursor);
    s->deferred_key_ready=0;
    prepare_service_portraits_free(s);
}

static int prepare_service_notice_open(prepare_ui *ui,int kind) {
    prepare_service *s=&ui->service;unsigned id;fd2_image candidate[4]={0};
    const fd2_image *portraits=NULL;
    if (kind<0 || kind>2 || s->page || s->page_background) return -1;
    if (kind==0) {
        if (!ui->assets->service.ready || s->selector<0 || s->selector>=6) return -1;
        id=k_prepare_town_portraits[s->selector];portraits=ui->assets->service.portraits;
    } else {
        if (kind==2) id=75;
        else {
            if (s->member<0 || (unsigned)s->member>=ui->context->preparation.unit_count) return -1;
            id=ui->context->preparation.unit_area[(size_t)s->member*80u+7u];
        }
        for (size_t i=0;i<4u;i++)
            if (fd2_portrait_decode_frame(&candidate[i],&ui->assets->dato,id,i)!=0 ||
                !image_size(&candidate[i],80,80)) goto fail;
        portraits=candidate;
    }
    for (size_t i=0;i<4u;i++) if(!image_size(&portraits[i],80,80)) goto fail;
    unsigned origin=fd2_portrait_notice_origin(id);
    fd2_vga *page=new_frame(ui->vga);if(!page)goto fail;
    int result=fd2_scene_field_message_box(page,ui->fdother);
    if (!result) result=prepare_service_portrait_at(page,portraits,origin,0,1);
    if (!result) result=prepare_service_modal_open(ui,page,1);
    free(page);
    if(!result) {
        prepare_service_portraits_free(s);
        if(kind==0)s->active_portraits=ui->assets->service.portraits;
        else {memcpy(s->notice_portraits,candidate,sizeof(candidate));memset(candidate,0,sizeof(candidate));s->active_portraits=s->notice_portraits;}
        s->notice_origin=origin;s->notice_open=1;
    }
    for(size_t i=0;i<4u;i++)fd2_image_free(&candidate[i]);
    return result;
fail:
    for(size_t i=0;i<4u;i++)fd2_image_free(&candidate[i]);return -1;
}

/* 16c57准备caller的53A51=0：箭头x143（不是战场x191）。18/19起点181，
 * frame13擦除起点176；raw零色也写。临时借原FDOTHER5目录，无热状态。 */
static int prepare_service_arrow(prepare_ui *ui,unsigned frame) {
    if (frame!=13u && frame!=18u && frame!=19u) return -1;
    const uint8_t *data;size_t size;
    if (fd2_archive_get(ui->fdother,5,&data,&size)!=0 || size<6 || memcmp(data,"LMI1",4)) return -1;
    size_t count=read16(data+4);
    if (count>(size-6u)/4u || frame>=count) return -1;
    size_t start=read32(data+6u+frame*4u),end=frame+1u<count ? read32(data+10u+frame*4u):size;
    if (start<6u+count*4u || end<=start || end>size || end-start<4u) return -1;
    int w=read16(data+start),h=read16(data+start+2u);
    if (w<1 || w>32 || h<1 || h>32 || (size_t)w*(size_t)h!=end-start-4u) return -1;
    fd2_image image={w,h,(uint8_t *)(data+start+4u)};
    fd2_map_sprite_blit(ui->vga,&image,143,frame==13u ? 176:181,-1);return 0;
}
static int prepare_service_wait_event(prepare_ui *ui,fd2_input_event *event,int mode) {
    if (!event || (mode!=0 && mode!=1)) return -1;
    unsigned countdown=fd2_field_rng_next(&ui->context->rng)%30u+2u;
    uint64_t last=SDL_GetTicks();int blink=0;unsigned arrow=18,arrow_ticks=0;
    if (mode==1 && prepare_service_arrow(ui,18)!=0) return -1;
    for (;;) {
        if (prepare_service_host_quit(ui)) return 1;
        if (prepare_service_take_key(ui,event)) break;
        uint64_t now=SDL_GetTicks();(void)fd2_field_water_tick(ui->vga,now);
        if (now-last>=110u) {
            if (mode==1 && ++arrow_ticks==3u) {
                arrow_ticks=0;arrow=arrow==18u ? 19u:18u;
                if (prepare_service_arrow(ui,arrow)!=0) return -1;
            }
            if (blink) {
                if (prepare_service_portrait(ui,ui->vga,0,0)!=0) return -1;
                countdown=fd2_field_rng_next(&ui->context->rng)%30u+2u;blink=0;
            } else if (countdown--==0u) {
                if (prepare_service_portrait(ui,ui->vga,3,0)!=0) return -1;blink=1;
            }
            last=now;
        }
        if (prepare_service_present(ui,16)) return 1;
    }
    return mode==1 ? prepare_service_arrow(ui,13):0;
}
static int prepare_service_wait_ack(prepare_ui *ui,int mode) {
    fd2_input_event event;return prepare_service_wait_event(ui,&event,mode);
}
static int prepare_service_wait_key(prepare_ui *ui,fd2_input_event *event,int mode) {
    return prepare_service_wait_event(ui,event,mode);
}

/* 26eda/27079：入口一次%50+8；highlight初2，与53F72 sprite相位独立。
 * 原仅highlight为奇数时递增sprite；blink后重新%30+2。 */
static int prepare_service_read_key(prepare_ui *ui,fd2_input_event *event,
        int animation_mode,int (*redraw)(prepare_ui *)) {
    if (!event || animation_mode<0 || animation_mode>3 || !redraw) return -1;
    prepare_service *s=&ui->service;uint64_t last=SDL_GetTicks();int blink=0;
    unsigned countdown=fd2_field_rng_next(&ui->context->rng)%50u+8u;
    s->highlight_phase=2;
    if (redraw(ui)!=0 || prepare_service_portrait(ui,ui->vga,0,0)!=0) return -1;
    for (;;) {
        if (prepare_service_host_quit(ui)) return 1;
        if (prepare_service_take_key(ui,event)) return 0;
        uint64_t now=SDL_GetTicks();
        if (now<last || now-last>=110u) {
            s->highlight_phase=(s->highlight_phase+1u)&3u;
            if (s->highlight_phase&1u) s->sprite_phase=(s->sprite_phase+1u)&3u;
            if (redraw(ui)!=0 || prepare_service_portrait(ui,ui->vga,blink ? 3u:0u,0)!=0) return -1;
            if (blink) {
                if (prepare_service_portrait(ui,ui->vga,0,0)!=0) return -1;
                countdown=fd2_field_rng_next(&ui->context->rng)%30u+2u;blink=0;
            } else if (countdown--==0u) {
                if (prepare_service_portrait(ui,ui->vga,3,0)!=0) return -1;blink=1;
            }
            last=now;
        }
        if (prepare_service_present(ui,16)) return 1;
    }
}

/* dialog_text_scroll_up @0x16e24（原登记同名）：仅两种对话origin。
 * 原五次3px和一次4px都是CPU写入，每步只清一底行；无wait/flip/键采样。 */
static int prepare_service_scroll(prepare_ui *ui) {
    unsigned origin=ui->service.notice_origin;
    if(origin!=1832u && origin!=36887u)return 0;
    int x=origin==1832u ? 14:94,y=origin==1832u ? 9:119;
    for (int step=0;step<6;step++) {
        int delta=step<5 ? 3:4;
        for (int row=0;row<72;row++)
            memmove(ui->vga->framebuffer+(y+row)*VGA_STRIDE+x,
                ui->vga->framebuffer+(y+row+delta)*VGA_STRIDE+x,208);
        memset(ui->vga->framebuffer+(y+72)*VGA_STRIDE+x+1,74,208);
    }
    return 0;
}
/* sub_27816 @0x27816 / sub_278E7 @0x278e7：列表窗口(10,115)284x80，
 * 三次6px+10ms，再8px；末步紧接原内容重绘，不额外增加末态等待。 */
static int prepare_service_scroll_rows(prepare_ui *ui,const uint8_t *before,
        const uint8_t *after,int down) {
    if(!before || !after || (down!=0 && down!=1))return -1;
    memcpy(ui->vga->framebuffer,before,VGA_W*VGA_H);
    for(int step=0;step<4;step++) {
        int delta=step<3 ? 6:8;
        if(down) {
            for(int row=0;row<80-delta;row++)memmove(ui->vga->framebuffer+(115+row)*VGA_STRIDE+10,
                ui->vga->framebuffer+(115+row+delta)*VGA_STRIDE+10,284);
            for(int row=80-delta;row<80;row++)memset(ui->vga->framebuffer+(115+row)*VGA_STRIDE+10,73,284);
        } else {
            for(int row=79-delta;row>=0;row--)memmove(ui->vga->framebuffer+(115+row+delta)*VGA_STRIDE+10,
                ui->vga->framebuffer+(115+row)*VGA_STRIDE+10,284);
            for(int row=0;row<delta;row++)memset(ui->vga->framebuffer+(115+row)*VGA_STRIDE+10,73,284);
        }
        if(step<3 && prepare_service_present(ui,10))return 1;
    }
    memcpy(ui->vga->framebuffer,after,VGA_W*VGA_H);return prepare_service_host_quit(ui);
}
/* 提交前的纯文字/立绘读集验证；不呈现、耗键、推进RNG或改message owner。 */
static int prepare_service_text_ready(prepare_ui *ui,size_t fragment,
        const fd2_text_parameters *parameters,int x,int y) {
    fd2_text_expanded expanded;
    if (!ui || !parameters || !ui->assets->font.glyphs || !ui->assets->font.glyph_count ||
        ui->assets->font.glyph_count>ui->assets->font.size/32u ||
        fd2_text_expand(&ui->assets->text,ui->assets->font.glyph_count,parameters,fragment,&expanded)!=0) return -1;
    int origin=x,base_y=y,row=0;
    for(size_t i=0;i<expanded.count;i++) {
        uint16_t token=expanded.tokens[i];
        if(token==0xfffeu || token==0xfffdu) {
            if((ui->service.notice_origin==1832u || ui->service.notice_origin==36887u) && row==3)row--;
            y=base_y+19*++row;x=origin;continue;
        }
        if(x<0 || x+16>VGA_W || y<0 || y+17>VGA_H)return -1;
        if(token==FD2_TEXT_SAFE_MINUS) {
            const fd2_image *minus=&ui->assets->info.signs[1];
            if(!minus->pixels || minus->width<1 || minus->width>16 || minus->height<1 || minus->height>16)return -1;
        }
        x+=16;
    }
    return 0;
}
static int prepare_service_notice_ready(prepare_ui *ui,unsigned portrait_id) {
    if(fd2_scene_field_message_validate(ui->fdother)!=0)return -1;
    for(size_t i=0;i<4u;i++) {
        fd2_image image={0};
        int ready=fd2_portrait_decode_frame(&image,&ui->assets->dato,portrait_id,i)==0 && image_size(&image,80,80);
        fd2_image_free(&image);if(!ready)return -1;
    }
    return 0;
}
static int prepare_service_text(prepare_ui *ui,size_t fragment,int x,int y) {
    prepare_service *s=&ui->service;fd2_text_expanded expanded;
    fd2_text_parameters parameters={ui->context->message_number,s->new_fragment,s->old_fragment};
    if (prepare_service_text_ready(ui,fragment,&parameters,x,y)!=0 ||
        fd2_text_expand(&ui->assets->text,ui->assets->font.glyph_count,&parameters,fragment,&expanded)!=0) return -1;
    int base_x=x,base_y=y,row=0,slow=1;
    for (size_t i=0;i<expanded.count;i++) {
        uint16_t token=expanded.tokens[i];
        if (token==0xfffeu || token==0xfffdu) {
            if ((s->notice_origin==1832u || s->notice_origin==36887u) && row==3) {
                int result=prepare_service_scroll(ui);if(result)return result;row--;
            }
            row++;y=base_y+19*row;x=base_x;
            if (token==0xfffdu) {
                if (prepare_service_portrait(ui,ui->vga,0,0)!=0) return -1;
                int result=prepare_service_wait_ack(ui,1);if(result)return result;slow=1;
            }
            continue;
        }
        if (x<0 || x+16>VGA_W || y<0 || y+17>VGA_H) return -1;
        if (token==FD2_TEXT_SAFE_MINUS) {
            const fd2_image *minus=&ui->assets->info.signs[1];
            if (!minus->pixels || minus->width<1 || minus->width>16 || minus->height<1 || minus->height>16) return -1;
            for (int j=0;j<16;j++) memset(ui->vga->framebuffer+(y+j)*VGA_STRIDE+x,74,16);
            fd2_map_sprite_blit(ui->vga,minus,x+(16-minus->width)/2,y+(16-minus->height)/2,0);
        } else fd2_font_draw_glyph(ui->vga,&ui->assets->font,token,x,y,205,76,74);
        x+=16;
        /* input_check看到入口已排BIOS键；先保存当前脉冲，再让present采样
         * 下一帧，避免首glyph上传清掉快显/随后ACK共用的那个键。 */
        if(slow && !s->deferred_key_ready && fd2_input_has_any_key(&ui->vga->input)) {
            fd2_input_event event;
            if(prepare_service_take_key(ui,&event)){s->deferred_key=event;s->deferred_key_ready=1;slow=0;}
        }
        if(slow)fd2_vga_present(ui->vga);
        if (prepare_service_host_quit(ui)) return 1;
        if(s->deferred_key_ready)slow=0;
        else if(fd2_input_has_any_key(&ui->vga->input)) {
            fd2_input_event event;
            if(prepare_service_take_key(ui,&event)) {s->deferred_key=event;s->deferred_key_ready=1;slow=0;}
        }
        if (slow) {
            if (++s->glyph_counter==2u) {
                s->glyph_counter=0;s->mouth_phase=(s->mouth_phase+1u)&3u;
                if (prepare_service_portrait(ui,ui->vga,s->mouth_phase==3u ? 1u:s->mouth_phase,0)!=0) return -1;
            }
            if (sound(ui,2)!=0) return -1;
            if (prepare_service_present(ui,55)) return 1;
        }
    }
    fd2_vga_present(ui->vga);return prepare_service_host_quit(ui);
}

static int prepare_service_money_redraw(prepare_ui *ui) {
    return fd2_field_info_number_draw(ui->vga,&ui->assets->info,16,99,
        ui->context->preparation.gold_or_flags<=INT32_MAX ? (int32_t)ui->context->preparation.gold_or_flags:0,8,0);
}
static int prepare_service_money_animate(prepare_ui *ui,int subtract) {
    const fd2_image *strip=&ui->assets->service.money_strip;
    if (!image_size(strip,6,99)) return -1;
    char old_text[12],new_text[12];uint8_t digit[8],target[8],valid[8];
    uint32_t old=ui->service.previous_money,new_value=ui->context->preparation.gold_or_flags;
    int64_t old_signed=old<=INT32_MAX ? (int64_t)old:(int64_t)old-INT64_C(4294967296);
    int64_t new_signed=new_value<=INT32_MAX ? (int64_t)new_value:(int64_t)new_value-INT64_C(4294967296);
    /* 原%0.8d是precision8：负号另占一字，不能改成总宽8的%08d。 */
    (void)snprintf(old_text,sizeof(old_text),"%.8lld",(long long)old_signed);
    (void)snprintf(new_text,sizeof(new_text),"%.8lld",(long long)new_signed);
    for (size_t i=0;i<8u;i++) {
        digit[i]=(uint8_t)(old_text[i]-'0');target[i]=(uint8_t)(new_text[i]-'0');
        valid[i]=(uint8_t)(digit[i]<=9u && target[i]<=9u);
        /* 原'-'→253越raw strip且可能永不收敛。仅略该未定义数字切片，
         * 其余7列原算法照常，最终187d6安全末态；业务DWORD已由cold提交。 */
        if (!valid[i]) digit[i]=target[i];
    }
    for (unsigned cycle=0;cycle<10u;cycle++) {
        uint8_t moving[8];int any=0;
        for (size_t i=0;i<8u;i++) {
            moving[i]=(uint8_t)(valid[i] && digit[i]!=target[i]);any|=moving[i];
            if (moving[i] && subtract) digit[i]=digit[i] ? (uint8_t)(digit[i]-1u):9u;
        }
        if (!any) return prepare_service_money_redraw(ui);
        for (unsigned phase=0;phase<9u;phase++) {
            for (size_t i=0;i<8u;i++) if (moving[i]) {
                unsigned line=9u*digit[i]+(subtract ? 8u-phase:phase+1u);
                if (line>90u) return -1;
                for (unsigned row=0;row<9u;row++)
                    memcpy(ui->vga->framebuffer+(98u+row)*VGA_STRIDE+16u+6u*i,
                        strip->pixels+6u*(line+row),6);
            }
            if (prepare_service_present(ui,10)) return 1;
        }
        if (!subtract) for(size_t i=0;i<8u;i++)if(moving[i])digit[i]=(uint8_t)((digit[i]+1u)%10u);
    }
    return -1;
}

static int prepare_service_promotion_prepare(prepare_ui *ui) {
    prepare_service *s=&ui->service;
    if (s->promotion_visual) return 0;
    if (fd2_archive_open(&s->promotion_backgrounds,"original_game/BG.DAT")!=0 ||
        fd2_archive_open(&s->promotion_figani,"original_game/FIGANI.DAT")!=0 ||
        fd2_prepare_promotion_open(&s->promotion_visual,ui->vga,&s->promotion_backgrounds,
            &s->promotion_figani,s->promotion.old_unit_id,s->promotion.target_unit_id)!=0) {
        fd2_archive_close(&s->promotion_figani);fd2_archive_close(&s->promotion_backgrounds);return -1;
    }
    return 0;
}

static int prepare_service_menu_page(prepare_ui *ui,int mode) {
    if (mode<0 || mode>1 || !ui->assets->service.ready) return -1;
    fd2_vga *base=new_frame(ui->vga);if(!base)return -1;
    if(!mode && ui->service.page)memcpy(ui->service.page->framebuffer,ui->vga->framebuffer,VGA_W*VGA_H);
    for (int row=169;row<189;row++) memset(base->framebuffer+row*VGA_STRIDE+201,74,104);
    for (int step=0;step<4;step++) {
        memcpy(ui->vga->framebuffer,base->framebuffer,VGA_W*VGA_H);
        for (size_t i=0;i<4u;i++)
            fd2_map_sprite_blit(ui->vga,&ui->assets->service.icons[2u*i],
                240+(int)k_prepare_menu_slide_offsets[i]/(mode ? step+1:4-step),169,0);
        if (prepare_service_present(ui,16)) {free(base);return 1;}
    }
    if (mode) memcpy(ui->vga->framebuffer,base->framebuffer,VGA_W*VGA_H);
    free(base);return 0;
}

static int prepare_service_animate(prepare_ui *ui) {
    const fd2_field_prepare_service_assets *a=&ui->assets->service;
    size_t count=a->animation_count;int x=0,y=0;
    switch (ui->service.selector) {
        case 1: if(count!=5u)return -1;x=169;y=45;break;
        case 3: if(count!=1u)return -1;x=148;y=39;break;
        case 4: if(count!=9u)return -1;x=147;y=32;break;
        case 5: if(count!=7u)return -1;x=131;y=28;break;
        default:return -1;
    }
    for(size_t i=0;i<count;i++)if(!a->animation_frames[i].pixels)return -1;
    if (ui->service.selector==3 && prepare_service_present(ui,55)) return 1;
    if (ui->service.selector==4) {
        if (prepare_service_portrait(ui,ui->vga,3,0)!=0) return -1;
        if (prepare_service_present(ui,110)) return 1;
    }
    for(size_t i=0;i<count;i++) {
        fd2_map_sprite_blit(ui->vga,&a->animation_frames[i],x,y,0);
        if (prepare_service_present(ui,ui->service.selector==3 ? 440u:110u)) return 1;
    }
    if (ui->service.selector==4) {
        for(int value=0;value<64;value+=2) {
            for(size_t i=0;i<VGA_PALETTE_SIZE;i++) {
                unsigned channel=(unsigned)ui->vga->palette[i]+(unsigned)value;
                if(channel>63u)channel=63u;ui->vga->dac[i]=(uint8_t)((channel<<2)|(channel>>4));
            }
            if(prepare_service_present(ui,4))return 1;
        }
        if(prepare_service_present(ui,550))return 1;
        for(int value=62;value>=0;value-=2) {
            for(size_t i=0;i<VGA_PALETTE_SIZE;i++) {
                unsigned channel=(unsigned)ui->vga->palette[i]+(unsigned)value;
                if(channel>63u)channel=63u;ui->vga->dac[i]=(uint8_t)((channel<<2)|(channel>>4));
            }
            if(prepare_service_present(ui,4))return 1;
        }
        if(prepare_service_present(ui,275))return 1;
    }
    if (ui->service.selector!=5 && prepare_service_portrait(ui,ui->vga,0,0)!=0) return -1;
    prepare_service_clear_keys(ui);return 0;
}

/* sub_2921A @0x2921a：128固定点正域采样；范围外清0，无双线性插值。 */
static int prepare_service_zoom(prepare_ui *ui,int phase) {
    prepare_service *s=&ui->service;
    const uint8_t *record=fd2_field_stage_prepare_record_get(ui->context->preparation.stage_id);
    if (!s->town_background || !record || record[0]>=3u || s->selector<0 || s->selector>=6 || phase<0 || phase>10) return -1;
    int type=record[0],step=128-9*phase;
    int cx=20480+128*(phase*((int)k_prepare_town_x[type][s->selector]-150)/10);
    int cy=12800+128*(phase*((int)k_prepare_town_y[type][s->selector]-100)/10);
    memset(ui->vga->framebuffer,0,VGA_W*VGA_H);
    for (int y=0;y<VGA_H;y++) {
        int sy=cy+(y-100)*step;if(sy<0 || sy>=25600)continue;
        for(int x=0;x<VGA_W;x++) {
            int sx=cx+(x-160)*step;if(sx>=0 && sx<40960)
                ui->vga->framebuffer[y*VGA_STRIDE+x]=s->town_background[(sy/128)*VGA_STRIDE+sx/128];
        }
    }
    fd2_vga_set_brightness(ui->vga,4*phase);return prepare_service_present(ui,16);
}

static int prepare_service_confirm(prepare_ui *ui,unsigned *port) {
    fd2_vga *base=new_frame(ui->vga);if(!base)return -1;
    if(ui->service.page)memcpy(ui->service.page->framebuffer,ui->vga->framebuffer,VGA_W*VGA_H);
    int selected=0,result=0,blink=0;unsigned countdown=fd2_field_rng_next(&ui->context->rng)%30u+2u;
    uint64_t last=SDL_GetTicks();unsigned phase=0;
    for (int step=1;step<=4;step++) {
        memcpy(ui->vga->framebuffer,base->framebuffer,VGA_W*VGA_H);
        fd2_map_sprite_blit(ui->vga,&ui->assets->commands.frames[49],248-step*4,168,0);
        fd2_map_sprite_blit(ui->vga,&ui->assets->commands.frames[51],248+step*4,168,0);
        if(prepare_service_present(ui,16)){result=1;goto done;}
    }
    for (;;) {
        fd2_input_event event;
        if(prepare_service_host_quit(ui)){result=1;goto done;}
        if(prepare_service_take_key(ui,&event)) {
            if(cancel_key(event.key)){*port=FD2_FLOW_CANCEL;break;}
            if(confirm_key(event.key)){*port=selected ? FD2_FLOW_NO:FD2_FLOW_YES;break;}
            if(event.key==FD2_INPUT_KEY_LEFT)selected=0;
            if(event.key==FD2_INPUT_KEY_RIGHT)selected=1;
        }
        uint64_t now=SDL_GetTicks();(void)fd2_field_water_tick(ui->vga,now);
        if(now-last>=110u) {
            phase=(phase+1u)&3u;last=now;
            if(blink){countdown=fd2_field_rng_next(&ui->context->rng)%30u+10u;blink=0;
                if(prepare_service_portrait(ui,base,0,0)!=0){result=-1;goto done;}}
            else if(countdown--==0u){blink=1;if(prepare_service_portrait(ui,base,3,0)!=0){result=-1;goto done;}}
        }
        memcpy(ui->vga->framebuffer,base->framebuffer,VGA_W*VGA_H);
        fd2_map_sprite_blit(ui->vga,&ui->assets->commands.frames[selected==0 && phase/2u ? 49:48],232,168,0);
        fd2_map_sprite_blit(ui->vga,&ui->assets->commands.frames[selected==1 && phase/2u ? 52:51],264,168,0);
        if(prepare_service_present(ui,16)){result=1;goto done;}
    }
    for (int step=1;step<=4;step++) {
        memcpy(ui->vga->framebuffer,base->framebuffer,VGA_W*VGA_H);
        fd2_map_sprite_blit(ui->vga,&ui->assets->commands.frames[48],232+step*4,168,0);
        fd2_map_sprite_blit(ui->vga,&ui->assets->commands.frames[51],264-step*4,168,0);
        if(prepare_service_present(ui,16)){result=1;goto done;}
    }
    memcpy(ui->vga->framebuffer,base->framebuffer,VGA_W*VGA_H);
done:
    free(base);return result;
}

static int prepare_service_ui(prepare_ui *ui,const fd2_static_flow_node *node,unsigned *port) {
    if (!ui || !node || !node->op || !port) return -1;
    prepare_service *s=&ui->service;int result=0;*port=FD2_FLOW_NEXT;
    if (!strcmp(node->op,"notice_open")) result=prepare_service_notice_open(ui,node->args[0]);
    else if (!strcmp(node->op,"close_page")) result=prepare_service_modal_close(ui);
    else if (!strcmp(node->op,"clear_keys")) prepare_service_clear_keys(ui);
    else if (!strcmp(node->op,"wait_key")) {
        fd2_input_event event;result=prepare_service_wait_event(ui,&event,node->args[0]);
        if(!result)*port=cancel_key(event.key) ? FD2_FLOW_CANCEL:FD2_FLOW_ACK;
    } else if (!strcmp(node->op,"confirm")) result=prepare_service_confirm(ui,port);
    else if (!strcmp(node->op,"show_message")) {
        int source=node->args[0],index=node->args[1],layout=node->args[2];size_t fragment;
        if(source<0 || source>3 || index<0 || layout<0 || layout>5 || s->selector<0 || s->selector>=6)goto fatal;
        if(source==0)fragment=(size_t)index;
        else if(source==1){if(index>=4)goto fatal;fragment=k_prepare_buy_messages[index][s->selector];}
        else if(source==2){if(index>=2)goto fatal;fragment=k_prepare_sell_messages[index][s->selector];}
        else fragment=k_prepare_service_return_fragments[s->selector];
        int x=layout<2 ? 12:layout==4 ? 100:95;
        if(layout==3 && s->growth_line==3u) {
            result=prepare_service_scroll(ui);if(result)goto complete;s->growth_line=2;
        }
        /* 2ac7d的末项用1e529返回行直接画；layout5不重复1e529的滚动门。 */
        int y=layout==1 ? 157:(layout==3 || layout==5) ? 119+19*(int)s->growth_line:119;
        result=prepare_service_text(ui,fragment,x,y);
        if(!result && layout!=3 && layout!=5)result=prepare_service_portrait(ui,ui->vga,0,0);
    } else if (!strcmp(node->op,"service_resources")) {
        if(!s->town_background){s->town_background=malloc(VGA_W*VGA_H);if(!s->town_background)goto fatal;
            memcpy(s->town_background,ui->vga->framebuffer,VGA_W*VGA_H);}
        result=fd2_field_prepare_assets_open_service(ui->assets,ui->fdother,s->selector);
    } else if (!strcmp(node->op,"service_transition")) {
        if(node->args[0]!=0)goto fatal;
        for(int phase=1;phase<=10 && !result;phase++)result=prepare_service_zoom(ui,phase);
        if(!result){fd2_vga_set_brightness(ui->vga,64);memset(ui->vga->framebuffer,0,VGA_W*VGA_H);}
    } else if (!strcmp(node->op,"service_enter")) {
        if(!image_size(&ui->assets->service.background,320,200))goto fatal;
        fd2_map_sprite_blit(ui->vga,&ui->assets->service.background,0,0,0);
        result=prepare_service_fade(ui,1);if(!result)result=prepare_service_present(ui,200);
    } else if (!strcmp(node->op,"service_page")) result=prepare_service_menu_page(ui,node->args[0]);
    else if (!strcmp(node->op,"service_exit")) {
        if(!image_size(&ui->assets->service.background,320,200))goto fatal;
        fd2_map_sprite_blit(ui->vga,&ui->assets->service.background,0,0,0);
        result=prepare_service_present(ui,200);if(!result)result=prepare_service_fade(ui,0);
        for(int phase=10;phase>=0 && !result;phase--)result=prepare_service_zoom(ui,phase);
    } else if (!strcmp(node->op,"service_animate")) result=prepare_service_animate(ui);
    else if (!strcmp(node->op,"money_redraw")) {
        if(!image_size(&ui->assets->service.money_panel,63,15))goto fatal;
        fd2_map_sprite_blit(ui->vga,&ui->assets->service.money_panel,5,95,-1);
        result=prepare_service_money_redraw(ui);
        if(!result && s->page_background) {
            for(int row=95;row<110;row++)memcpy(s->page_background+row*VGA_STRIDE+5,
                ui->vga->framebuffer+row*VGA_STRIDE+5,63);
        }
    } else if (!strcmp(node->op,"music")) {
        if(node->args[0]<-1 || node->args[0]>28 || node->args[1]<0 || node->args[1]>1 || sync_audio_options(ui)!=0)goto fatal;
        if(ui->context->bgm)result=node->args[0]==-1 ? fd2_bgm_stop(ui->context->bgm):
            fd2_bgm_play(ui->context->bgm,(size_t)node->args[0],node->args[1]);
    } else if (!strcmp(node->op,"sfx")) {
        /* 原FDOTHER31严格nested目录13项；首offset前另有无效间隙，不能以
         * (62-6)/4上界14当count。schema与consumer同域。
         * 声音关闭/后端不可用沿现App策略静默；有效播放另核实际borrowed bank。 */
        if(node->args[0]<0 || node->args[0]>12 || node->args[1]<1 || node->args[1]>3)goto fatal;
        if(ui->context->preparation.options[3] && ui->audio && ui->audio->player &&
            (!ui->audio->ui_bank || (size_t)node->args[0]>=fd2_pcm_bank_count(ui->audio->ui_bank)))goto fatal;
        result=sound_loop(ui,(size_t)node->args[0],node->args[1]);
    } else if (!strcmp(node->op,"promotion_visual")) {
        if(!s->promotion_visual || node->args[0]<0 || node->args[0]>6 || node->args[1]<0)goto fatal;
        result=fd2_prepare_promotion_step(s->promotion_visual,(fd2_prepare_promotion_phase)node->args[0],(unsigned)node->args[1]);
        if(result==-2){s->host_quit=1;result=1;}
        if(!result && node->args[0]==6){fd2_prepare_promotion_close(s->promotion_visual,0);s->promotion_visual=NULL;
            fd2_archive_close(&s->promotion_figani);fd2_archive_close(&s->promotion_backgrounds);}
    } else if (!strcmp(node->op,"darkness")) {
        if(node->args[0]<0 || node->args[0]>255)goto fatal;
        /* 11d40的byte暗度超63输出黑；不让uint8乘法回绕为亮色。 */
        fd2_vga_set_brightness(ui->vga,node->args[0]);result=prepare_service_present(ui,16);
    } else if (!strcmp(node->op,"empty_screen")) {
        memset(ui->vga->framebuffer,0,VGA_W*VGA_H);result=prepare_service_present(ui,16);
    } else return 0;
complete:
    if(result){*port=FD2_FLOW_ERROR;if(result==1)s->host_quit=1;return -1;}return 1;
fatal:
    *port=FD2_FLOW_ERROR;return -1;
}

#endif
