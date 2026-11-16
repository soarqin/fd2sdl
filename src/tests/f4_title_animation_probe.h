/* A14-C05：真实main wrapper＋同一animation.c，无窗口/设备/原码执行。
 * source依据20421/36ff4/36e57与1f81e/1f894。只mock提交/毫秒等待，
 * 不替身decoder。payload owner=VGA64000+base768，palette字段为实际DAC。 */
#ifndef FD2_F4_TITLE_ANIMATION_PROBE_H
#define FD2_F4_TITLE_ANIMATION_PROBE_H
static fd2_vga *title_ani_vga;
static unsigned title_ani_case,title_ani_frames,title_ani_delays,title_ani_clears;
static unsigned title_ani_failed,title_ani_stream,title_ani_hooks;
static uint8_t title_ani_base[768],title_ani_dac[768];
static uint64_t title_ani_deadline,title_ani_interval;

static int title_ani_emit(const char *op,unsigned frame,int status) {
    const size_t n=64768;const uint8_t zeros[768]={0};fd2_vga *vga=title_ani_vga;
    if(printf("{\"schema\":\"fd2-f4-reader-v1\",\"status\":0,\"op\":\"%s\","
        "\"a\":%u,\"b\":%u,\"variant\":0,\"stride\":1,\"height\":64768,"
        "\"page_bytes\":64768,\"payload_bytes\":131072,\"owner_guard\":true,"
        "\"history_mask_measured\":false,\"pixel_addresses_written\":0,\"palette_addresses_written\":0,"
        "\"buffer_semantics\":\"VGA64000+base768\",\"palette_semantics\":\"actualDAC\","
        "\"actual_main_wrapper\":true,\"actual_ANI_decoder\":true,\"function_returned\":%s,\"source_result\":%d,"
        "\"uploads\":%u,\"delays\":%u,\"clears\":%u,\"queued\":%u,\"host\":%u}\n",
        op,title_ani_case,frame,!strcmp(op,"done")?"true":"false",status,title_ani_frames,title_ani_delays,title_ani_clears,
        (unsigned)vga->input.queued_count,(unsigned)vga->input.quit_requested)<0 ||
       fwrite(vga->framebuffer,1,64000,stdout)!=64000 || fwrite(vga->palette,1,768,stdout)!=768 ||
       fwrite(vga->dac,1,768,stdout)!=768)return -1;
    for(size_t i=0;i<n/sizeof(zeros);i++)if(fwrite(zeros,1,sizeof(zeros),stdout)!=sizeof(zeros))return -1;
    if(fwrite(zeros,1,n%sizeof(zeros),stdout)!=n%sizeof(zeros) ||
       fwrite(zeros,1,768,stdout)!=768 || fflush(stdout))return -1;
    return 0;
}
static void title_ani_upload(fd2_vga *vga) {
    if(vga!=title_ani_vga || memcmp(vga->palette,title_ani_base,768) ||
       memcmp(vga->dac,title_ani_dac,768) || vga->frame_deadline_ns!=title_ani_deadline ||
       vga->frame_interval_ns!=title_ani_interval)title_ani_failed=1;
    if(title_ani_stream && title_ani_emit(title_ani_frames?"frame":"start",title_ani_frames,0))title_ani_failed=1;
    title_ani_frames++;
}
static void title_ani_delay(uint32_t ms) {
    uint32_t expected=title_ani_case==2u?90u:15u;
    if(ms!=expected || title_ani_frames!=title_ani_delays+1u)title_ani_failed=1;
    title_ani_delays++;
    if((title_ani_case==1u && title_ani_frames==6u) || title_ani_case==2u) {
        title_ani_vga->input.queued_head=0;title_ani_vga->input.queued_count=1;
        title_ani_vga->input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
        title_ani_vga->input.queued_keys[0].bios_code=0x1c0d;
    }
    if(title_ani_case==3u && title_ani_frames==6u)title_ani_vga->input.quit_requested=1;
}
static void title_ani_clear(fd2_input *input) {
    title_ani_clears++;fd2_input_clear_keys(input);
}
#define rd_u16_le title_ani_rd_u16_le
#undef fd2_animation_play_hooked
#undef fd2_animation_play
#undef fd2_vga_upload_current_frame
#undef fd2_delay_ms
#define fd2_animation_play_hooked title_ani_private_play_hooked
#define fd2_animation_play title_ani_private_play
#define fd2_vga_upload_current_frame title_ani_upload
#define fd2_delay_ms title_ani_delay
#define fd2_input_clear_keys title_ani_clear
#include "../animation.c"
#undef rd_u16_le
#undef fd2_animation_play_hooked
#undef fd2_animation_play
#undef fd2_vga_upload_current_frame
#undef fd2_delay_ms
#undef fd2_input_clear_keys
static int title_ani_decode(fd2_vga *vga,const fd2_archive *ani,int index,uint32_t delay,
        int check,fd2_animation_frame_hook hook,void *userdata) {
    return title_ani_private_play_hooked(vga,ani,index,delay,check,hook,userdata);
}
static int title_ani_setup(fd2_vga *vga,unsigned which) {
    const uint8_t *raw;size_t size;unsigned palette=which==2u?99u:0u;
    if(which>3u || fd2_archive_get(&g_fdother,palette,&raw,&size) || size!=768)return -1;
    memset(vga,0,sizeof(*vga));vga->input.use_test_key_state=1;
    for(size_t i=0;i<64000;i++)vga->framebuffer[i]=(uint8_t)(i*17u+i/320u+83u);
    fd2_vga_set_palette(vga,raw);
    memcpy(title_ani_base,raw,768);
    for(unsigned i=0;i<768;i++)title_ani_dac[i]=(uint8_t)((raw[i]<<2)|(raw[i]>>4));
    vga->frame_deadline_ns=UINT64_C(999999999999);vga->frame_interval_ns=16000000;
    title_ani_deadline=vga->frame_deadline_ns;title_ani_interval=vga->frame_interval_ns;
    title_ani_vga=vga;title_ani_case=which;
    title_ani_frames=title_ani_delays=title_ani_clears=title_ani_failed=title_ani_hooks=0;
    /* 原入口clear应丢弃这一个queued键；没有held-key或synthetic repeat。 */
    vga->input.queued_head=0;vga->input.queued_count=1;vga->input.queued_keys[0].key=FD2_INPUT_KEY_F1;vga->input.queued_keys[0].bios_code=0x3b00;
    return 0;
}
static int title_ani_run(fd2_vga *vga,unsigned which) {
    if(title_ani_setup(vga,which))return -1;
    int result=which==2u?title_animation_play(vga,3,90,1,NULL,NULL):
        play_intro_animation_with_palette(vga,0,15,0);
    unsigned frames=which==2u?1u:which==3u?6u:96u;
    unsigned clears=which==2u?1u:which==3u?6u:97u;
    int expected=which==2u?1:which==3u?-2:0;
    uint8_t zero[768]={0};const uint8_t *last_dac=which>=2u?title_ani_dac:zero;
    if(result!=expected || title_ani_failed || title_ani_frames!=frames || title_ani_delays!=frames ||
       title_ani_clears!=clears || vga->input.queued_count!=(which==2u) ||
       vga->input.quit_requested!=(which==3u) || memcmp(vga->palette,title_ani_base,768) ||
       memcmp(vga->dac,last_dac,768) || vga->frame_deadline_ns!=title_ani_deadline ||
       vga->frame_interval_ns!=title_ani_interval)return -1;
    return title_ani_stream?title_ani_emit("done",frames,result):0;
}
static void title_ani_chained_hook(void *unused,uint16_t frame) {
    (void)unused;if(frame || memcmp(title_ani_vga->palette,title_ani_base,768) ||
        memcmp(title_ani_vga->dac,title_ani_dac,768))title_ani_failed=1;
    title_ani_hooks++;
}
static int title_ani_error_and_chain(fd2_vga *vga) {
    const uint8_t *entry;size_t length;
    if(fd2_archive_get(&g_ani,0,&entry,&length) || length<0xadu)return -1;
    const size_t payload=772,total=10+0xad+8+payload;
    uint8_t *bad=calloc(total,1);fd2_archive malformed={0},original=g_ani;int result=-1;
    if(!bad)return -1;
    memcpy(bad,"LLLLLL\x0a\0\0\0",10);memcpy(bad+10,entry,0xad);
    bad[10+0xa5]=1;bad[10+0xa6]=0;
    size_t at=10+0xad;bad[at]=(uint8_t)payload;bad[at+1]=(uint8_t)(payload>>8);bad[at+2]=3;
    at+=8;bad[at++]=1;for(unsigned i=0;i<768;i++)bad[at+i]=(uint8_t)((title_ani_base[i]+17u)&63u);at+=768;
    bad[at++]=4;bad[at++]=77;bad[at++]=255;
    if(at!=total || fd2_archive_open_mem(&malformed,bad,total) || title_ani_setup(vga,0))goto done;
    g_ani=malformed;memset(&malformed,0,sizeof(malformed));
    int failed=title_animation_play(vga,0,15,0,NULL,NULL);
    fd2_archive_close(&g_ani);g_ani=original;
    if(failed!=-1 || title_ani_frames || title_ani_delays || title_ani_clears!=1u ||
       memcmp(vga->palette,title_ani_base,768) || memcmp(vga->dac,title_ani_dac,768) ||
       vga->input.queued_count || vga->frame_deadline_ns!=title_ani_deadline || vga->frame_interval_ns!=title_ani_interval)goto done;
    for(size_t i=0;i<64000;i++)if(vga->framebuffer[i]!=77u)goto done;
    if(title_ani_setup(vga,2))goto done;
    /* 共享hook合成只核顺序：frame0恢复palette/DAC后才交音效callback。
     * 普通skip键由原delay之后注入，不声称本测试播放了真实声音。 */
    int skipped=title_animation_play(vga,3,90,1,title_ani_chained_hook,NULL);
    if(skipped!=1 || title_ani_failed || title_ani_hooks!=1u || title_ani_frames!=1u)goto done;
    result=0;
done:
    fd2_archive_close(&malformed);free(bad);return result;
}
static int title_ani_open(const char *root) {
    char path[1024];
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&g_fdother,path) ||
       f4_path(path,root,"ANI.DAT") || fd2_archive_open(&g_ani,path))return -1;
    return 0;
}
static int test_title_animation_palette_owner(void) {
    static fd2_vga vga;int result=1;title_ani_active=1;title_ani_stream=0;
    if(title_ani_open("original_game"))goto done;
    for(unsigned which=0;which<4;which++)if(title_ani_run(&vga,which)) {
        fprintf(stderr,"title ANI owner case %u failed\n",which);goto done;
    }
    if(title_ani_error_and_chain(&vga)) {
        fputs("title ANI owner error/hook case failed\n",stderr);goto done;
    }
    result=0;
done:
    fd2_archive_close(&g_fdother);fd2_archive_close(&g_ani);title_ani_active=0;return result;
}
static int f4_title_ani_cli(const char *root) {
    static fd2_vga vga;char line[128],op[16];unsigned values[3];int result=1;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)return 1;
#endif
    title_ani_active=1;title_ani_stream=1;
    if(title_ani_open(root) || f4_input_receipt("FDOTHER.DAT",g_fdother.data,g_fdother.size) ||
       f4_input_receipt("ANI.DAT",g_ani.data,g_ani.size) ||
       !fgets(line,sizeof(line),stdin) || !strchr(line,'\n') || f4_parse(line,op,values) ||
       strcmp(op,"start") || values[0]>3u || values[1] || values[2] || title_ani_run(&vga,values[0]))goto done;
    result=0;
done:
    fd2_archive_close(&g_fdother);fd2_archive_close(&g_ani);title_ani_active=0;
    if(result){fputs("{\"schema\":\"fd2-f4-reader-v1\",\"status\":1,\"payload_bytes\":0}\n",stdout);fflush(stdout);}
    return result;
}
#endif
