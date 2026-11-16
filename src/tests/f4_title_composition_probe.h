/* C14：同一main私有TU的正常title caller，有限有序stdout请求。
 * 保真实资源decoder/页copy/菜单/插入点；ANI仅污染callee边界，不签其内部。
 * fade endpoint借真实Scene fast仅去宿主等待；其完整65/64序列沿原A15另签。
 * 不开窗口/设备/Save；没有第二标题调度器或资源decoder。 */
#ifndef FD2_F4_TITLE_COMPOSITION_PROBE_H
#define FD2_F4_TITLE_COMPOSITION_PROBE_H
#define FD2_F4_PROTOCOL_ONLY
#include "f4_reader_probe.h"
#undef FD2_F4_PROTOCOL_ONLY
static fd2_vga *title_comp_vga;
static char title_comp_op[16];static unsigned title_comp_values[3];static int title_comp_pending;
static int title_comp_full,title_comp_count,title_comp_selection,title_comp_selected;
static int title_comp_confirmed,title_comp_confirm_frame,title_comp_menu_pending;
static int title_comp_first_sent,title_comp_scroll_y,title_comp_pending_fade,title_comp_fade_kind;
static int title_comp_tail_sent,title_comp_fade_steps,title_comp_cut_pending;
static unsigned title_comp_ani_in_boot;
static int title_comp_next(void) {
    char line[128];
    if(!fgets(line,sizeof(line),stdin)) {
        title_comp_pending=0;if(ferror(stdin))title_comp_bad=1;return 0;
    }
    if(!strchr(line,'\n') || f4_parse(line,title_comp_op,title_comp_values)) {
        title_comp_bad=1;title_comp_pending=0;return -1;
    }
    title_comp_pending=1;return 1;
}
static void title_comp_emit(const char *op,unsigned a,unsigned b,unsigned v,fd2_vga *vga) {
    uint8_t zero[768]={0};
    if(title_comp_bad)return;
    if(!title_comp_pending || strcmp(title_comp_op,op) || title_comp_values[0]!=a ||
       title_comp_values[1]!=b || title_comp_values[2]!=v ||
       vga->frame_deadline_ns!=UINT64_C(999999999999) || vga->frame_interval_ns!=16000000u) {
        title_comp_bad=1;return;
    }
    if(printf("{\"schema\":\"fd2-f4-reader-v1\",\"status\":0,\"op\":\"%s\",\"a\":%u,"
        "\"b\":%u,\"variant\":%u,\"stride\":320,\"height\":200,\"page_bytes\":64000,"
        "\"payload_bytes\":129536,\"owner_guard\":true,\"history_mask_measured\":false,"
        "\"palette_semantics\":\"actualDAC\",\"actual_main_caller\":true,"
        "\"ANI_interiors_executed\":false,\"source_delays_executed\":false}\n",op,a,b,v)<0 ||
       fwrite(vga->framebuffer,1,64000,stdout)!=64000 || fwrite(vga->dac,1,768,stdout)!=768) {
        title_comp_bad=1;return;
    }
    for(unsigned i=0;i<83u;i++)if(fwrite(zero,1,768,stdout)!=768){title_comp_bad=1;return;}
    if(fwrite(zero,1,256,stdout)!=256 || fwrite(zero,1,768,stdout)!=768 || fflush(stdout)) {
        title_comp_bad=1;return;
    }
    (void)title_comp_next();
}
static void title_comp_upload(fd2_vga *vga) {
    if(title_comp_pending_fade>=0) {
        if(title_comp_full)title_comp_emit(title_comp_fade_kind?"red":"fade",(unsigned)title_comp_pending_fade,0,0,vga);
        if(!title_comp_fade_kind)title_comp_fade_steps++;
        title_comp_pending_fade=-1;
    } else if(title_comp_fade_steps==41 && !title_comp_tail_sent) {
        if(title_comp_full)title_comp_emit("tail",0,0,0,vga);
        title_comp_tail_sent=1;
    } else if(title_comp_menu_pending && !title_comp_confirmed && title_comp_selected==title_comp_selection) {
        title_comp_emit("menu",(unsigned)title_comp_count,(unsigned)title_comp_selected,1,vga);
        title_comp_menu_pending=0;
    }
}
static void title_comp_delay(uint32_t ms) {
    if(ms==30u && title_comp_full) {
        if(title_comp_scroll_y<0){title_comp_bad=1;return;}
        title_comp_emit("scroll",(unsigned)title_comp_scroll_y,0,0,title_comp_vga);title_comp_scroll_y--;
    } else if(ms==80u && title_comp_confirmed) {
        if(title_comp_confirm_frame>=8){title_comp_bad=1;return;}
        title_comp_emit("confirm",(unsigned)title_comp_count,(unsigned)title_comp_selected,
            (unsigned)title_comp_confirm_frame++,title_comp_vga);
    } else if(ms!=8u && ms!=100u && ms!=1000u)title_comp_bad=1;
}
static void title_comp_wait_ticks(uint32_t ticks) {
    if(ticks==1u && title_comp_cut_pending) {
        unsigned index=(unsigned)title_comp_cut_pending;
        title_comp_emit("cut",index,(unsigned)title_comp_scroll_y,0,title_comp_vga);title_comp_cut_pending=0;
    }
    if(ticks!=1u && ticks!=6u && ticks!=30u)title_comp_bad=1;
}
static void title_comp_before_fade(fd2_vga *vga,int start,int step,uint8_t r,uint8_t g,uint8_t b) {
    if(start || step<0 || step>40){title_comp_bad=1;return;}
    if(r==56u && g==60u && b==63u) {
        if(step==0 && title_comp_full)title_comp_emit("white",0,0,0,vga);
    } else if(r!=63u || g || b)title_comp_bad=1;
}
static void title_comp_after_fade(int start,int step,uint8_t r,uint8_t g,uint8_t b) {
    (void)start;(void)g;(void)b;title_comp_pending_fade=step;title_comp_fade_kind=r==63u;
}
static void title_comp_fade_endpoint(fd2_vga *vga,int fade_in) {
    if(fade_in && title_comp_full && !title_comp_first_sent && title_comp_last_resource==74) {
        title_comp_emit("still",74,0,0,vga);title_comp_first_sent=1;
    }
    if(fade_in && title_comp_full && (title_comp_last_resource==75 || title_comp_last_resource==100))
        title_comp_cut_pending=title_comp_last_resource;
}
static void title_comp_ani_boundary(fd2_vga *vga,int index) {
    /* 两端独立原ANI另账；这里故意污染完整callee output，随后真实main必须
     * 从真实scroll/title owner恢复，而非继承mock图作expected。 */
    static const int order[8]={3,4,5,6,7,8,0,1};
    int expected_palette=index==1?8:(index==3||index==4||index==6||index==8)?99:0;
    if((title_comp_full && (title_comp_ani_in_boot>=8u || index!=order[title_comp_ani_in_boot])) ||
       (!title_comp_full && (title_comp_ani_in_boot || index!=1)) || title_comp_last_resource!=expected_palette)
        title_comp_bad=1;
    uint8_t p[768];title_comp_ani_in_boot++;
    for(size_t i=0;i<64000;i++)vga->framebuffer[i]=(uint8_t)(i*7u+i/320u+(unsigned)index*31u);
    for(unsigned i=0;i<768u;i++)p[i]=(uint8_t)((i+(unsigned)index*3u)&63u);
    fd2_vga_set_palette(vga,p);
}
static int title_comp_read_action(fd2_input_action *action) {
    if(title_comp_bad){title_comp_vga->input.quit_requested=1;return 0;}
    if(title_comp_selected!=title_comp_selection)*action=FD2_INPUT_ACTION_DOWN;
    else {*action=FD2_INPUT_ACTION_CONFIRM;title_comp_confirmed=1;}
    return 1;
}
static void title_comp_menu_draw(int count,int selected,int highlighted) {
    if(count!=title_comp_count || selected<0 || selected>=count || (highlighted!=0&&highlighted!=1))title_comp_bad=1;
    if(title_comp_confirmed && highlighted!=(title_comp_confirm_frame&1))title_comp_bad=1;
    title_comp_selected=selected;title_comp_menu_pending=1;
}
static int f4_title_cli(const char *root) {
    static fd2_vga vga;char path[1024];int result=1;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)return 1;
#endif
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&g_fdother,path))goto done;
    archive_opens++;title_comp_active=1;
    if(f4_input_receipt("FDOTHER.DAT",g_fdother.data,g_fdother.size) || title_comp_next()!=1)goto done;
    while(title_comp_pending) {
        title_comp_full=!strcmp(title_comp_op,"still");
        if(title_comp_full) {
            if(title_comp_values[0]!=74u || title_comp_values[1] || title_comp_values[2]){title_comp_bad=1;goto done;}
            title_comp_count=3;title_comp_selection=0;
        } else {
            if(strcmp(title_comp_op,"menu") || (title_comp_values[0]!=2u&&title_comp_values[0]!=3u) ||
                title_comp_values[1]>=title_comp_values[0] || title_comp_values[2]!=1u){title_comp_bad=1;goto done;}
            title_comp_count=(int)title_comp_values[0];title_comp_selection=(int)title_comp_values[1];
        }
        memset(&vga,0,sizeof(vga));vga.input.use_test_key_state=1;
        vga.frame_deadline_ns=UINT64_C(999999999999);vga.frame_interval_ns=16000000;
        title_comp_vga=&vga;title_comp_selected=title_comp_confirmed=title_comp_confirm_frame=0;
        title_comp_first_sent=title_comp_menu_pending=title_comp_tail_sent=title_comp_fade_steps=title_comp_cut_pending=0;
        title_comp_pending_fade=-1;title_comp_scroll_y=535;title_comp_ani_in_boot=0;
        fd2_title_action action=boot_intro_title(&vga,NULL,NULL,title_comp_full,title_comp_count==3);
        fd2_title_action expected=title_comp_selection==0?FD2_TITLE_ACTION_NEW_GAME:
            title_comp_selection==1?FD2_TITLE_ACTION_LOAD:FD2_TITLE_ACTION_CONTINUE;
        if(title_comp_bad || action!=expected || image_live || title_comp_confirm_frame!=8 || title_comp_ani_in_boot!=(title_comp_full?8u:1u) ||
           vga.frame_deadline_ns!=UINT64_C(999999999999) || vga.frame_interval_ns!=16000000u ||
           (title_comp_full && title_comp_scroll_y!=-1))goto done;
    }
    result=ferror(stdin)?1:0;
done:
    if(g_fdother.data)fd2_archive_close(&g_fdother);
    if(archive_opens!=archive_closes || image_live)result=1;
    title_comp_active=0;
    if(result){fputs("C14 title caller rejected protocol/owner/resource/output\n",stderr);fputs("{\"schema\":\"fd2-f4-reader-v1\",\"status\":1,\"payload_bytes\":0}\n",stdout);fflush(stdout);}
    return result;
}
#endif
