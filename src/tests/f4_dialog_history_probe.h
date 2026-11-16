/* F4普通对白组合：同一Scene私有TU，实际opening/close/glyph/PAGE/scroll。
 * stdout仅给有限全VGA双底历史；没有生成器expected、窗口、设备或新owner。
 * copy/scroll读旧底，不伪装双底推物理mask；两个64000B页逐一独立比较。 */
#ifndef FD2_F4_DIALOG_HISTORY_PROBE_H
#define FD2_F4_DIALOG_HISTORY_PROBE_H
#define F4_DIALOG_N 64000u
typedef struct {
    char trace[768];unsigned count,uploads,delays,ticks,audio,keys,clears,rng,last_key;
    unsigned capture_at,hit,inject,host_at,host_present,fail_audio_at;
    unsigned host_frames,idle_goal,idle_host,clock_reads;uint64_t own_deadline;
    uint8_t *capture;
    int invalid;
} f4_dialog_observation;
static f4_dialog_observation f4_dialog_seen;
static void f4_dialog_key(fd2_input *input,unsigned code) {
    if(input->queued_count)return;
    input->queued_head=0;input->queued_count=1;
    input->queued_keys[0]=(fd2_input_event){FD2_INPUT_KEY_OTHER,
        code==0x1579?SDL_SCANCODE_Y:SDL_SCANCODE_Z,0,0,(uint16_t)code};
}
static void f4_dialog_observe(fd2_vga *vga,char event,uint32_t value) {
    f4_dialog_observation *s=&f4_dialog_seen;
    if(s->count+1u>=sizeof(s->trace)){s->invalid=1;return;}
    s->trace[s->count++]=event;s->trace[s->count]=0;
    if(event=='U') {
        s->uploads++;
        if(s->uploads==s->capture_at){memcpy(s->capture,vga->framebuffer,F4_DIALOG_N);s->hit=1;}
        if(s->inject)f4_dialog_key(&vga->input,0x2c7a);
        if(s->uploads==s->host_at)vga->input.quit_requested=1;
    } else if(event=='I' && s->host_present)vga->input.quit_requested=1;
    else if(event=='D'){s->delays+=value;if(value!=10u)s->invalid=1;}
    else if(event=='T')s->ticks+=value;
    else if(event=='A'){s->audio++;if(s->audio==s->fail_audio_at)dialog_probe_audio_failure=1;}
    else if(event=='K'){s->keys++;s->last_key=value;}
    else if(event=='C')s->clears++;
    else if(event=='c' && value)s->invalid=1;
}
static int f4_dialog_ready_key(void) {
    return !f4_dialog_seen.idle_host && f4_dialog_seen.host_frames>=f4_dialog_seen.idle_goal;
}
static uint64_t f4_dialog_clock(void) {
    unsigned tick=f4_dialog_seen.clock_reads++?102u+2u*f4_dialog_seen.host_frames:100u;
    return fd2_bios_tick_start_ns(tick);
}
static void f4_dialog_timed(fd2_vga *vga,uint32_t ms) {
    f4_dialog_observation *s=&f4_dialog_seen;
    if(ms!=16u || vga->frame_deadline_ns || vga->frame_interval_ns)s->invalid=1;
    f4_dialog_observe(vga,'H',ms);s->host_frames++;
    scene_test_input_frame(&vga->input);scene_test_upload(vga);
    s->own_deadline=UINT64_C(1000000000)+s->host_frames*UINT64_C(16000000);
    vga->frame_deadline_ns=s->own_deadline;vga->frame_interval_ns=16000000;
    if(s->idle_host)vga->input.quit_requested=1;
}
static void f4_dialog_wait_own(fd2_vga *vga) {
    if(vga->frame_deadline_ns!=f4_dialog_seen.own_deadline || vga->frame_interval_ns!=16000000u)
        f4_dialog_seen.invalid=1;
    f4_dialog_observe(vga,'E',0);vga->frame_deadline_ns=0;vga->frame_interval_ns=0;
}
static uint32_t f4_dialog_random(void *unused) {
    (void)unused;f4_dialog_seen.rng++;
    f4_dialog_observe(current_vga,'R',0);return 7u;
}
static int f4_dialog_emit(const char *op,unsigned a,unsigned b,unsigned v,
        const uint8_t *pages,const f4_dialog_observation *s,int status,int skip,
        unsigned queue,unsigned phase,unsigned counter,unsigned captured,
        unsigned water,int final_restored,const uint8_t palette[768]) {
    const size_t n=2u*F4_DIALOG_N;uint8_t zeros[768]={0};
    if(printf("{\"schema\":\"fd2-f4-reader-v1\",\"status\":0,\"op\":\"%s\","
        "\"a\":%u,\"b\":%u,\"variant\":%u,\"stride\":320,\"height\":400,"
        "\"page_bytes\":%zu,\"payload_bytes\":%zu,\"owner_guard\":true,"
        "\"history_mask_measured\":false,\"palette_semantics\":\"actualDAC\",\"buffer_semantics\":\"two-independent-VGA-pages\","
        "\"trace\":\"%s\",\"source_result\":%d,\"skip\":%d,\"queued\":%u,"
        "\"last_key\":%u,\"rng\":%u,\"clears\":%u,\"delay_ms\":%u,\"ticks\":%u,"
        "\"audio\":%u,\"mouth\":%u,\"glyph_counter\":%u,\"captured\":%u,"
        "\"water\":%u,\"water_phase\":%u,\"water_anchor\":%u,\"final_restored\":%d,\"host_stopped\":%d}\n",op,a,b,v,n,2u*n+1536u,
        s->trace,status,skip,queue,s->last_key,s->rng,s->clears,s->delays,s->ticks,s->audio,
        phase,counter,captured,water,(unsigned)current_vga->field_water_phase,(unsigned)current_vga->field_water_last_tick,final_restored,scene_host_quit_requested!=0)<0)return -1;
    if(fwrite(pages,1,n,stdout)!=n || fwrite(palette,1,768,stdout)!=768)return -1;
    /* 没有物理mask测量，零占位仅维持既有有界transport layout，header明确标false。 */
    for(size_t i=0;i<n/sizeof(zeros);i++)if(fwrite(zeros,1,sizeof(zeros),stdout)!=sizeof(zeros))return -1;
    if(n%sizeof(zeros) && fwrite(zeros,1,n%sizeof(zeros),stdout)!=n%sizeof(zeros))return -1;
    if(fwrite(zeros,1,768,stdout)!=768)return -1;
    return fflush(stdout)?-1:0;
}
static int f4_dialog_case(const fd2_archive *other,const fd2_archive *dato,
        const fd2_archive *texts,const fd2_ui_sheet *ui,const fd2_font *font,
        const char *op,unsigned a,unsigned b,unsigned v) {
    static fd2_vga screen; /* input_init注册的全局owner必须活到进程退出，不能free悬空。 */
    int box=!strcmp(op,"box"),marker=!strcmp(op,"marker"),face=!strcmp(op,"face"),
        scroll=!strcmp(op,"scroll"),page=!strcmp(op,"page"),mouth=!strcmp(op,"mouth"),edge=!strcmp(op,"edge"),idle=!strcmp(op,"idle");
    uint8_t *pages=NULL,*baseline=NULL,*page_before=NULL;uint8_t first_dac[768];fd2_dialog_state *state=NULL;
    fd2_scene_field_state *field=NULL,*field_before=NULL;f4_dialog_observation first={0};
    unsigned queue=0,phase=0,counter=0,captured=0,water=0;int status=0,skip=0,restored=0,result=-1,first_status=0,first_skip=0;
    if(!(box||marker||face||scroll||page||mouth||edge||idle) || a>1u ||
        (box && (v>3u || b>4u || (v==2u && b>3u) || (v==3u && b))) ||
        (marker && (v>6u || b>((v==4u||v==5u)?5u:2u) || ((v==2u||v==3u||v==6u)&&b))) ||
        (face && (b>1u || v>3u)) || (scroll && (b||v)) ||
        (page && (b>3u||v>1u)) || (mouth && (!b||b>8u||v>3u || ((v==2u||v==3u)&&b!=2u))) ||
        (edge && (a||b||v)) || (idle && (b>3u || v>1u || ((b==0u||b==3u)&&v))))return -1;
    pages=malloc(2u*F4_DIALOG_N);baseline=malloc(F4_DIALOG_N);page_before=malloc(FD2_FIELD_PAGE_SIZE);
    state=calloc(1,sizeof(*state));field=calloc(1,sizeof(*field));field_before=malloc(sizeof(*field));
    if(!pages||!baseline||!page_before||!state||!field||!field_before)goto done;
    for(unsigned pass=0;pass<2u;pass++) {
        dialog_probe_observer=NULL;dialog_probe_audio_failure=dialog_probe_feed_on_read=0;
        dialog_probe_timed=NULL;dialog_probe_wait_deadline=NULL;dialog_probe_clock=NULL;dialog_probe_ready_key=NULL;
        status=0;skip=0;reset_controls(&screen);auto_confirm=0;scene_host_quit_requested=0;fd2_bios_clock_reset(0,0);
        memset(state,0,sizeof(*state));memset(field,0,sizeof(*field));
        field->runtime.rng=f4_dialog_random;
        state->area=a?FD2_DIALOG_BOTTOM:FD2_DIALOG_TOP;state->speaker_actor_idx=-1;
        state->is_open=1;state->portrait_id=0;
        state->x=a?95:15;state->y=a?119:9;
        f4_seed(screen.framebuffer,F4_DIALOG_N,320,0,(int)pass);memcpy(baseline,screen.framebuffer,F4_DIALOG_N);
        memset(screen.palette,165,768);memset(screen.dac,165,768);
        screen.frame_deadline_ns=UINT64_C(999999999999);screen.frame_interval_ns=16000000;
        memset(screen.field_page->pixels,0x6b,FD2_FIELD_PAGE_SIZE);memcpy(page_before,screen.field_page->pixels,FD2_FIELD_PAGE_SIZE);
        memset(&f4_dialog_seen,0,sizeof(f4_dialog_seen));f4_dialog_seen.capture=pages+pass*F4_DIALOG_N;
        if(box || marker || page || (mouth && v==1u) || (idle && !b))f4_dialog_key(&screen.input,((page||idle)?0x1579:0x2c7a));
        if(box && v==1u) {
            animate_dialog_box_open(&screen,NULL,NULL,ui,NULL,NULL,state,-1,state->area,1);
            f4_dialog_seen.capture_at=b<4u?b+1u:0;
        }
        if(box && v!=1u){f4_dialog_seen.capture_at=b<4u?b+1u:0;f4_dialog_seen.inject=1;if(v==2u)f4_dialog_seen.host_at=b+1u;}
        if(marker) {
            field->units.count=1;field->units.items[0].x=(v==4u||v==5u)?3u:(v==2u||v==3u)?0u:1u;
            field->units.items[0].y=(v==4u||v==5u)?2u:(v==2u||v==3u)?0u:1u;
            field->focus_cell_x=field->units.items[0].x;field->focus_cell_y=field->units.items[0].y;
            f4_dialog_seen.capture_at=(v==2u||v==3u)?0u:b+1u;
            f4_dialog_seen.inject=1;if(v==6u)f4_dialog_seen.host_at=1;
        }
        if(idle) {
            dialog_probe_timed=f4_dialog_timed;dialog_probe_wait_deadline=f4_dialog_wait_own;
            dialog_probe_clock=f4_dialog_clock;dialog_probe_ready_key=f4_dialog_ready_key;
            screen.field_water_last_tick=100;
            f4_dialog_seen.idle_goal=b==2u?3u:b?1u:0u;
            f4_dialog_seen.idle_host=b==3u;
            f4_dialog_seen.capture_at=(b && !v)?f4_dialog_seen.idle_goal:0u;
        }
        *field_before=*field;dialog_probe_observer=f4_dialog_observe;
        if(idle)status=dialog_wait(&screen,dato,ui,state,field,1,&skip);
        else if(box) {
            if(v==1u)animate_dialog_box_close(&screen,NULL,NULL,ui,NULL,NULL,state,0);
            else animate_dialog_box_open(&screen,NULL,NULL,ui,NULL,NULL,state,-1,state->area,0);
            if(v==3u) {
                f4_dialog_key(&screen.input,0x1579);
                status=dialog_wait(&screen,dato,ui,state,field,0,&skip);
            }
        } else if(marker) {
            animate_dialog_marker(&screen,NULL,NULL,ui,NULL,field,0,state->area,(v==1u||v==3u||v==5u));
        } else if(face)status=draw_portrait_frame(&screen,dato,state->area,(uint16_t)b,v);
        else if(scroll)dialog_scroll_text_up(&screen,state,0);
        else if(page) {
            state->line=(int)b;state->y+=(int)b*19;
            status=newline_or_scroll(&screen,dato,ui,state,field,v==0u,0,&skip);
        } else if(mouth) {
            draw_dialog_box(&screen,ui,state->area);state->has_portrait=1;
            if(draw_portrait(&screen,dato,state->area,0))goto done;
            int slow=1;fd2_field_audio audio={0};
            if(v==2u)f4_dialog_seen.host_present=1;
            if(v==3u)f4_dialog_seen.fail_audio_at=2;
            for(unsigned i=0;i<b && !skip && !status;i++)
                status=scene_text_glyph(&screen,dato,ui,font,state,(uint16_t)i,-1,&slow,&skip,&audio);
        } else {
            fd2_text_entry text={0};
            if(fd2_text_entry_open_entry(&text,texts,23))goto done;
            /* 源23/12只含三个text_id；隐藏typed actor让a3=0，保合法资源/无地图依赖。 */
            field->units.count=3;
            for(unsigned i=0;i<3u;i++) {
                field->units.items[i].text_id=(uint8_t)(i==0?21:i==1?19:18);
                field->units.items[i].unit_id=field->units.items[i].text_id;
                field->units.items[i].flags=FD2_FIELD_UNIT_FLAG_HIDDEN;
            }
            *field_before=*field;dialog_probe_feed_on_read=1;
            status=play_text_fragment(&screen,NULL,NULL,dato,ui,NULL,field,font,&text,NULL,12,0);
            dialog_probe_feed_on_read=0;
        }
        dialog_probe_observer=NULL;
        if(f4_dialog_seen.invalid || memcmp(field,field_before,sizeof(*field)) ||
           memcmp(page_before,screen.field_page->pixels,FD2_FIELD_PAGE_SIZE) ||
           (water_calls && !idle) || clock_leaf_count || screen.frame_deadline_ns!=UINT64_C(999999999999) ||
           screen.frame_interval_ns!=16000000u)goto done;
        for(unsigned i=0;i<768u;i++)if(screen.palette[i]!=165u || (!idle && screen.dac[i]!=165u))goto done;
        if(!f4_dialog_seen.capture_at)memcpy(pages+pass*F4_DIALOG_N,screen.framebuffer,F4_DIALOG_N);
        else if(!f4_dialog_seen.hit)goto done;
        int is_restored=!memcmp(screen.framebuffer,baseline,F4_DIALOG_N);
        if(pass) {
            if(memcmp(first_dac,screen.dac,768) || status!=first_status || skip!=first_skip || strcmp(first.trace,f4_dialog_seen.trace) || first.rng!=f4_dialog_seen.rng || first.clears!=f4_dialog_seen.clears ||
               first.delays!=f4_dialog_seen.delays || first.ticks!=f4_dialog_seen.ticks || first.audio!=f4_dialog_seen.audio ||
               first.last_key!=f4_dialog_seen.last_key || queue!=screen.input.queued_count ||
               phase!=screen.dialog_visual.mouth_phase || counter!=screen.dialog_visual.glyph_counter ||
               captured!=state->captured || water!=water_calls || restored!=is_restored)goto done;
        } else {
            memcpy(first_dac,screen.dac,768);first=f4_dialog_seen;first_status=status;first_skip=skip;queue=screen.input.queued_count;phase=(unsigned)screen.dialog_visual.mouth_phase;
            counter=(unsigned)screen.dialog_visual.glyph_counter;captured=state->captured;water=water_calls;restored=is_restored;
        }
    }
    result=f4_dialog_emit(op,a,b,v,pages,&first,status,skip,queue,phase,counter,captured,water,restored,screen.dac);
done:
    dialog_probe_observer=NULL;dialog_probe_audio_failure=dialog_probe_feed_on_read=0;
    dialog_probe_timed=NULL;dialog_probe_wait_deadline=NULL;dialog_probe_clock=NULL;dialog_probe_ready_key=NULL;
    scene_host_quit_requested=0;free(pages);free(baseline);free(page_before);free(state);free(field);free(field_before);
    (void)other;return result;
}
static int f4_dialog_cli(const char *root) {
    fd2_archive other={0},dato={0},texts={0};fd2_ui_sheet ui={0};fd2_font font={0};
    char path[1024],line[128],op[16];unsigned v[3];int result=1;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)return 1;
#endif
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&other,path) ||
       f4_path(path,root,"DATO.DAT") || fd2_archive_open(&dato,path) ||
       f4_path(path,root,"FDTXT.DAT") || fd2_archive_open(&texts,path) ||
       ui_sheet_open_fdother(&ui,&other,5) || fd2_font_open_fdother(&font,&other,4) ||
       f4_input_receipt("FDOTHER.DAT",other.data,other.size) || f4_input_receipt("DATO.DAT",dato.data,dato.size) ||
       f4_input_receipt("FDTXT.DAT",texts.data,texts.size))goto done;
    while(fgets(line,sizeof(line),stdin))
        if(!strchr(line,'\n') || f4_parse(line,op,v) || f4_dialog_case(&other,&dato,&texts,&ui,&font,op,v[0],v[1],v[2]))goto done;
    result=ferror(stdin)?1:0;
done:
    fd2_archive_close(&other);fd2_archive_close(&dato);fd2_archive_close(&texts);
    fd2_field_page_destroy(test_page);test_page=NULL;current_vga=NULL;
    if(result){fputs("F4 dialog history rejected request/source/output\n",stderr);fputs("{\"schema\":\"fd2-f4-reader-v1\",\"status\":1,\"payload_bytes\":0}\n",stdout);fflush(stdout);}
    return result;
}
#endif
