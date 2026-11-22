/* Scene私有S5桥；在scene_flow_context/scene_source_*声明后include。
 * 仅固定cinematic资源/绘制和六个已命名冷子图；不接受任意flowid或指针。 */
#ifndef FD2_SCENE_CINEMATIC_FLOW_H
#define FD2_SCENE_CINEMATIC_FLOW_H
#include "settings.h"
#include "animation.h"

/* 20421给AFM的52766是单独malloc768临时区；完整36fd3/36ff4及十个
 * opcode不写53A65或DAC。复用旧ANI字节解码时，保留本caller的基色/DAC
 * owner；只在原present前还原，不回滚已解出的像素/输入/时间前缀。 */
typedef struct { fd2_vga *vga;uint8_t palette[768],dac[768]; } cinematic_ani_palette;
static void cinematic_animation_palette(void *userdata,uint16_t frame) {
    cinematic_ani_palette *owner=userdata;(void)frame;
    memcpy(owner->vga->palette,owner->palette,768);memcpy(owner->vga->dac,owner->dac,768);
}

/* 1956b/26996的两页借现Scene dialog局部存储；并非第二Game/page owner。 */
_Static_assert(sizeof(((fd2_dialog_state *)0)->backgrounds)>=128000u,
    "cinematic notice reuses two complete VGA snapshots");
static uint8_t *cinematic_notice_background(fd2_dialog_state *notice) {
    return (uint8_t *)(void *)&notice->backgrounds;
}
static int cinematic_notice_open(scene_flow_context *c,int32_t portrait) {
    fd2_dialog_state *notice=&c->cinematic_notice;
    if(notice->is_open || portrait<0 || portrait>=128 || !c->dato || !c->ui || !c->font)
        return -1;
    /* 此固定ending族只调用普通origin36887，不借此开放128..132特殊布局。 */
    for(size_t frame=0;frame<4;frame++) {
        fd2_image image={0};
        int status=fd2_portrait_decode_frame(&image,c->dato,(size_t)portrait,frame);
        int valid=status==0 && image.width==80 && image.height==80;
        fd2_image_free(&image);if(!valid)return -1;
    }
    fd2_vga *page=calloc(1,sizeof(*page));if(!page)return -1;
    memcpy(page->framebuffer,c->vga->framebuffer,64000);
    int result=fd2_scene_field_notice_page(page,c->fdother,c->dato,(uint16_t)portrait);
    if(!result) {
        memset(notice,0,sizeof(*notice));
        uint8_t *background=cinematic_notice_background(notice);
        memcpy(background,c->vga->framebuffer,64000);
        memcpy(background+64000,page->framebuffer,64000);
        notice->area=FD2_DIALOG_BOTTOM;notice->portrait_id=(uint16_t)portrait;
        notice->has_portrait=notice->is_open=1;notice->speaker_actor_idx=-1;
        notice->x=84;notice->y=119;
    }
    free(page);return result;
}
static int cinematic_notice_slide(scene_flow_context *c,int32_t phase) {
    fd2_dialog_state *notice=&c->cinematic_notice;
    if(!notice->is_open || phase<0 || phase>5)return -1;
    const uint8_t *background=cinematic_notice_background(notice),*page=background+64000;
    memcpy(c->vga->framebuffer,background,64000);
    int y=112+13*phase;
    for(int row=0;row<86 && y+row<200;row++)
        memcpy(c->vga->framebuffer+(size_t)(y+row)*320+5,
            page+(size_t)(112+row)*320+5,310);
    return 0;
}
static int cinematic_notice_wait(scene_flow_context *c,int32_t mode) {
    if(!c->cinematic_notice.is_open || (mode!=0 && mode!=1))return -1;
    int skip=c->fast;
    return dialog_wait(c->vga,c->dato,c->ui,&c->cinematic_notice,c->state,mode,&skip);
}
static int cinematic_notice_text(scene_flow_context *c,int32_t fragment) {
    fd2_dialog_state *notice=&c->cinematic_notice;
    const uint8_t *tokens;size_t count;
    if(!notice->is_open || !c->text || fragment<0 ||
        fd2_text_entry_get_fragment(c->text,(size_t)fragment,&tokens,&count)!=0 ||
        scene_fragment_preflight(c->dato,c->ui,c->font,tokens,count,c->state)!=0)return -1;
    /* 31bdf当前域只有字形/换行/continuation及动态文本；不能套普通story
     * 的说话人变化、闭框和95px origin，也不把不支持控制码静默跳过。 */
    for(size_t i=0;i<count;i++) {
        int16_t token=(int16_t)fd2_text_token_at(tokens,i);
        if(token<0 && token!=-1 && token!=-2 && token!=-3 && token!=-4 && token!=-5 && token!=-6)
            return -1;
    }
    int slow=!c->fast,skip=0;
    for(size_t i=0;i<count && !skip;i++) {
        int16_t token=(int16_t)fd2_text_token_at(tokens,i);
        if(token==-1)break;
        if(token==-2 || token==-3) {
            if(newline_or_scroll(c->vga,c->dato,c->ui,notice,c->state,
                token==-3,c->fast,&skip)!=0)return -1;
            notice->x=84;
            if(token==-3)slow=!c->fast;
        } else if(token>=-6 && token<=-4) {
            fd2_text_expanded expanded;
            if(scene_dynamic_text(c->state,c->font,token,&expanded)!=0)return -1;
            int nested_slow=!c->fast;int *step=token==-6?&slow:&nested_slow;
            for(size_t j=0;j<expanded.count && !skip;j++)
                if(scene_text_glyph(c->vga,c->dato,c->ui,c->font,notice,
                    expanded.tokens[j],token==-6?-1:74,step,&skip,c->audio)!=0)return -1;
        } else if(scene_text_glyph(c->vga,c->dato,c->ui,c->font,notice,
            (uint16_t)token,74,&slow,&skip,c->audio)!=0)return -1;
    }
    return scene_host_quit_requested?-1:0;
}

static int scene_cinematic_call(scene_flow_context *c,const char *identity) {
    if(!c || !c->source_flow || !c->mutable_game || !c->state)return -1;
    unsigned parent_steps=c->flow_steps;
    c->flow_steps=0;
    int result=scene_flow_run(c,fd2_static_flow_lookup("stage",identity));
    c->flow_steps=parent_steps;
    return result;
}

/* 1处理、0非本域、-1错误；分支仅返回port，PC仍归唯一Scene walker。 */
static int scene_cinematic_node(scene_flow_context *c,
        const fd2_static_flow_node *n,unsigned *port) {
    if(!c || !n || !n->op || !port)return -1;
    const char *op=n->op;const int32_t *a=n->args;
    if(!strcmp(op,"call_cinematic_ending") || !strcmp(op,"call_cinematic_credits") ||
        !strcmp(op,"call_cinematic_card_normal") || !strcmp(op,"call_cinematic_card_last") ||
        !strcmp(op,"call_cinematic_card_first") || !strcmp(op,"call_cinematic_montage_round")) {
        const char *identity=!strcmp(op,"call_cinematic_ending")?"stage.cinematic.ending":
            !strcmp(op,"call_cinematic_credits")?"stage.cinematic.credits":
            !strcmp(op,"call_cinematic_card_normal")?"stage.cinematic.card_normal":
            !strcmp(op,"call_cinematic_card_last")?"stage.cinematic.card_last":
            !strcmp(op,"call_cinematic_card_first")?"stage.cinematic.card_first":
            "stage.cinematic.montage_round";
        *port=FD2_FLOW_NEXT;
        return scene_cinematic_call(c,identity)==0?1:-1;
    }
    if(strncmp(op,"cin_",4) && strncmp(op,"branch_cin_",11))return 0;
    if(!c->source_flow || !c->mutable_game || !c->state || !c->vga || !c->fdother ||
        scene_host_quit_requested)return -1;
    *port=FD2_FLOW_NEXT;
    if(!strcmp(op,"cin_close")) {fd2_scene_cinematic_close(&c->cinematic);return 1;}
    if(!c->cinematic && fd2_scene_cinematic_open(&c->cinematic,c->vga,
        c->mutable_game,c->fdother)!=0)return -1;
    fd2_scene_cinematic *scope=c->cinematic;int result=-1;
    if(!strcmp(op,"cin_montage_open")) {
        result=fd2_scene_cinematic_montage_open(scope,a[0],a[1],a[2]);
    } else if(!strcmp(op,"cin_montage_close")) {
        result=fd2_scene_cinematic_montage_close(scope);
    } else if(!strcmp(op,"cin_montage_intro")) {
        if(a[0]<0 || a[0]>8)return -1;
        result=fd2_scene_cinematic_montage_intro(scope,(unsigned)a[0]);
    } else if(!strcmp(op,"cin_montage_intro_finish")) {
        result=fd2_scene_cinematic_montage_intro_finish(scope);
    } else if(!strcmp(op,"cin_montage_half")) {
        if(a[0]<0 || a[0]>1)return -1;
        result=fd2_scene_cinematic_montage_half(scope,(unsigned)a[0]);
    } else if(!strcmp(op,"branch_cin_montage_half")) {
        unsigned half;
        if(a[0]<0 || a[0]>1 || fd2_scene_cinematic_montage_current_half(scope,&half)!=0)return -1;
        *port=half==(unsigned)a[0]?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"branch_cin_montage_rng")) {
        int yes;
        if(a[0]<0 || a[0]>100 || fd2_scene_cinematic_montage_rng(scope,(unsigned)a[0],&yes)!=0)return -1;
        *port=yes?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"branch_cin_montage_part")) {
        size_t count;
        if(a[0]<0 || a[0]>21 || fd2_scene_cinematic_montage_part_count(scope,&count)!=0)return -1;
        *port=(size_t)a[0]<count?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"branch_cin_montage_tick") || !strcmp(op,"branch_cin_montage_marker") ||
              !strcmp(op,"branch_cin_montage_last_marker")) {
        uint8_t marker,ticks;int last;
        if(a[0]<0 || a[0]>21 || fd2_scene_cinematic_montage_part_info(scope,(size_t)a[0],&marker,&ticks,&last)!=0)
            return -1;
        int yes=last;
        if(!strcmp(op,"branch_cin_montage_tick")) {
            if(a[1]<0 || a[1]>9)return -1;
            yes=(unsigned)a[1]<ticks;
        } else if(!strcmp(op,"branch_cin_montage_marker"))yes=marker!=0;
        *port=yes?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"cin_montage_hit")) {
        result=fd2_scene_cinematic_montage_hit(scope);
    } else if(!strcmp(op,"cin_montage_cue") || !strcmp(op,"cin_montage_frame")) {
        if(a[0]<0 || a[0]>21)return -1;
        result=!strcmp(op,"cin_montage_cue")?fd2_scene_cinematic_montage_cue(scope,(size_t)a[0]):
            fd2_scene_cinematic_montage_frame(scope,(size_t)a[0]);
    } else if(!strcmp(op,"cin_page_alloc")) {
        if(a[1]<0)return -1;
        result=fd2_scene_cinematic_page_alloc(scope,a[0],(size_t)a[1]);
    } else if(!strcmp(op,"cin_page_free")) {
        result=fd2_scene_cinematic_page_release(scope,a[0]);
    } else if(!strcmp(op,"cin_page_clear")) {
        if(a[1]<0 || a[1]>255)return -1;
        result=fd2_scene_cinematic_page_clear(scope,a[0],(uint8_t)a[1]);
    } else if(!strcmp(op,"cin_work_clear")) {
        if(a[0]<0 || a[0]>255)return -1;
        result=fd2_scene_cinematic_work_clear(scope,(uint8_t)a[0]);
    } else if(!strcmp(op,"cin_audio_open")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_audio_open(scope,(size_t)a[0]);
    } else if(!strcmp(op,"cin_audio_play")) {
        result=fd2_scene_cinematic_audio_play(scope,c->audio,a[0]);
    } else if(!strcmp(op,"cin_audio_close")) {
        result=fd2_scene_cinematic_audio_close(scope,c->audio);
    } else if(!strcmp(op,"cin_copy")) {
        for(size_t i=2;i<8;i++)if(a[i]<0)return -1;
        result=fd2_scene_cinematic_copy(scope,a[0],a[1],(size_t)a[2],(size_t)a[3],
            (size_t)a[4],(size_t)a[5],(size_t)a[6],(size_t)a[7]);
    } else if(!strcmp(op,"cin_sheet_open")) {
        if(a[1]<0)return -1;
        result=fd2_scene_cinematic_sheet_open(scope,a[0],(size_t)a[1]);
    } else if(!strcmp(op,"cin_sheet_close")) {
        result=fd2_scene_cinematic_sheet_close(scope,a[0]);
    } else if(!strcmp(op,"cin_sheet_blit")) {
        if(a[1]<0 || a[5]<0)return -1;
        result=fd2_scene_cinematic_sheet_blit(scope,a[0],(size_t)a[1],a[2],a[3],a[4],(size_t)a[5]);
    } else if(!strcmp(op,"cin_image_blit")) {
        if(a[0]<0 || a[4]<0)return -1;
        result=fd2_scene_cinematic_image_blit(scope,(size_t)a[0],a[1],a[2],a[3],(size_t)a[4]);
    } else if(!strcmp(op,"cin_palette_save")) {
        result=fd2_scene_cinematic_palette_save(scope);
    } else if(!strcmp(op,"cin_palette_validate")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_palette_validate(scope,(size_t)a[0]);
    } else if(!strcmp(op,"cin_palette_load")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_palette_load(scope,(size_t)a[0]);
    } else if(!strcmp(op,"cin_palette_restore")) {
        result=fd2_scene_cinematic_palette_restore(scope);
    } else if(!strcmp(op,"cin_circle_prepare")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_circle_prepare(scope,(size_t)a[0]);
    } else if(!strcmp(op,"cin_circle_frame")) {
        if(a[0]<0 || a[2]<0)return -1;
        result=fd2_scene_cinematic_circle_frame(scope,(size_t)a[0],a[1],(size_t)a[2]);
    } else if(!strcmp(op,"cin_circle_close")) {
        result=fd2_scene_cinematic_circle_close(scope);
    } else if(!strcmp(op,"cin_shake_prepare")) {
        result=fd2_scene_cinematic_shake_prepare(scope);
    } else if(!strcmp(op,"cin_shake_frame")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_shake_frame(scope,(unsigned)a[0]);
    } else if(!strcmp(op,"cin_present")) {
        if(!c->fast)fd2_vga_upload_current_frame(c->vga);
        if(fd2_bios_clock_error()<0 || fd2_vga_output_error(c->vga)<0)return -1;
        result=0;
    } else if(!strcmp(op,"cin_water")) {
        /* sub_4E31C @0x4e31c（原登记同名）：比较与DAC写后取锚
         * 各自读4e310；本共享op不能预取宿主ms或复用固定时间。 */
        result=fd2_field_water_tick_live(c->vga)<0?-1:0;
    } else if(!strcmp(op,"cin_animation")) {
        if((a[0]!=0 && a[0]!=2) || a[1]<0 || (a[2]!=0 && a[2]!=1))return -1;
        fd2_archive archive={0};
        if(fd2_archive_open(&archive,fd2_asset_path(FD2_ANI))!=0)return -1;
        cinematic_ani_palette owner={0};owner.vga=c->vga;
        memcpy(owner.palette,c->vga->palette,768);memcpy(owner.dac,c->vga->dac,768);
        int status=fd2_animation_play_hooked(c->vga,&archive,a[0],
            c->fast?0u:(uint32_t)a[1],c->fast?0:a[2],cinematic_animation_palette,&owner);
        /* 错误/HOST同样不把借用的AFM临时palette提升为永久base。 */
        cinematic_animation_palette(&owner,0);
        fd2_archive_close(&archive);
        result=status<0?scene_source_status(status):0;
    } else if(!strcmp(op,"branch_cin_stage")) {
        if(a[0]<0)return -1;
        *port=c->game->stage==(size_t)a[0]?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"cin_notice_open")) {
        result=cinematic_notice_open(c,a[0]);
    } else if(!strcmp(op,"cin_notice_slide")) {
        result=cinematic_notice_slide(c,a[0]);
    } else if(!strcmp(op,"cin_notice_text")) {
        result=cinematic_notice_text(c,a[0]);
    } else if(!strcmp(op,"cin_notice_portrait")) {
        if(!c->cinematic_notice.is_open || a[0]<0 || a[0]>3)return -1;
        result=draw_portrait_frame(c->vga,c->dato,FD2_DIALOG_BOTTOM,
            c->cinematic_notice.portrait_id,(unsigned)a[0]);
    } else if(!strcmp(op,"cin_notice_wait")) {
        result=cinematic_notice_wait(c,a[0]);
    } else if(!strcmp(op,"cin_notice_close")) {
        if(!c->cinematic_notice.is_open)return -1;
        memcpy(c->vga->framebuffer,cinematic_notice_background(&c->cinematic_notice),64000);
        c->cinematic_notice.is_open=0;result=0;
    } else if(!strcmp(op,"cin_credits_assets_open")) {
        result=fd2_scene_cinematic_credits_assets_open(scope);
    } else if(!strcmp(op,"cin_credits_assets_close")) {
        result=fd2_scene_cinematic_credits_assets_close(scope);
    } else if(!strcmp(op,"cin_credits_actor")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_credits_actor(scope,(size_t)a[0]);
    } else if(!strcmp(op,"cin_credits_actor_close")) {
        result=fd2_scene_cinematic_credits_actor_close(scope);
    } else if(!strcmp(op,"cin_credits_intro")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_credits_intro(scope,(unsigned)a[0]);
    } else if(!strcmp(op,"cin_credits_intro_finish")) {
        result=fd2_scene_cinematic_credits_intro_finish(scope);
    } else if(!strcmp(op,"cin_credits_idle_reset")) {
        result=fd2_scene_cinematic_credits_idle_reset(scope);
    } else if(!strcmp(op,"cin_credits_idle")) {
        result=fd2_scene_cinematic_credits_idle(scope,a[0]);
    } else if(!strcmp(op,"cin_credits_part")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_credits_part(scope,(size_t)a[0]);
    } else if(!strcmp(op,"cin_credits_part_wait")) {
        size_t count;unsigned ticks;
        if(a[0]<0 || fd2_scene_cinematic_credits_part_info(scope,&count,(size_t)a[0],&ticks)!=0)
            return -1;
        result=scene_source_status(scene_fixed_ticks(c->vga,ticks,c->fast));
    } else if(!strcmp(op,"branch_cin_credits_part")) {
        size_t count;
        if(a[0]<0 || fd2_scene_cinematic_credits_part_count(scope,&count)!=0)return -1;
        *port=(size_t)a[0]<count?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"cin_credits_blink_reset")) {
        result=fd2_scene_cinematic_credits_blink_reset(scope);
    } else if(!strcmp(op,"cin_credits_card_prepare")) {
        result=fd2_scene_cinematic_credits_card_prepare(scope);
    } else if(!strcmp(op,"cin_credits_card")) {
        if(a[0]<0)return -1;
        result=fd2_scene_cinematic_credits_card(scope,(unsigned)a[0]);
    } else if(!strcmp(op,"cin_credits_roll_text")) {
        result=fd2_scene_cinematic_credits_roll_text(scope);
    } else if(!strcmp(op,"cin_credits_skip_set")) {
        result=fd2_scene_cinematic_credits_skip_set(scope,a[0]);
    } else if(!strcmp(op,"branch_cin_credits_skip")) {
        int skip;
        if(fd2_scene_cinematic_credits_skip_get(scope,&skip)!=0)return -1;
        *port=skip?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"branch_cin_input")) {
        *port=fd2_input_check(c->vga)?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"branch_cin_roster_count")) {
        if(a[0]<0 || a[0]>32)return -1;
        *port=c->game->roster_count>(size_t)a[0]?FD2_FLOW_YES:FD2_FLOW_NO;result=0;
    } else if(!strcmp(op,"cin_credits_epoch")) {
        /* 仅固定credits真实原outer边界使用；generator限制此op的所属图。
         * last-card反复输入可以继续其原循环，不将预算耗尽伪装成功。 */
        c->flow_steps=0;result=0;
    } else return -1;
    if(result)return -1;
    scene_source_load(c);
    return 1;
}

#endif
