/* 转职2fb2c／2fe14完整有限phase：真实step、BG／idle reader与上传路径。
 * stdout只到parent独占的新证据流；原资源只读，未创建窗口或设备。
 * BEGIN尚未初始化的work不读取／输出；首次源clear后的完整work均核。 */
#ifndef FD2_F4_PROMOTION_COMPOSITION_PROBE_H
#define FD2_F4_PROMOTION_COMPOSITION_PROBE_H
#include <limits.h>
static fd2_prepare_promotion *f4_prom_scope;
static unsigned f4_prom_case,f4_prom_phase,f4_prom_frame;
static int f4_prom_io_error;
static int f4_prom_parse(char *line,unsigned values[3]) {
    char *at=line,*end;
    for(unsigned i=0;i<3;i++) {
        while(isspace((unsigned char)*at))at++;
        errno=0;unsigned long value=strtoul(at,&end,10);
        if(errno || at==end || value>UINT_MAX || (i<2 && !isspace((unsigned char)*end)))return -1;
        values[i]=(unsigned)value;at=end;
    }
    while(isspace((unsigned char)*at))at++;
    return *at || values[0]>=136u || values[1]>=136u ||
        (values[2]!=3u && values[2]!=6u) ? -1:0;
}
static void f4_prom_emit(fd2_vga *vga) {
    unsigned work=f4_prom_phase?128000u:0u;
    if(f4_prom_io_error)return;
    if(printf("{\"schema\":\"fd2-f4-promotion-frame-v1\",\"case\":%u,\"phase\":%u,\"frame\":%u,\"ticks\":%u,\"delays\":%u,\"pumps\":%u,\"work_bytes\":%u}\n",
        f4_prom_case,f4_prom_phase,f4_prom_frame++,ticks,delays,input_pumps,work)<0 ||
        (work && fwrite(f4_prom_scope->work,1,work,stdout)!=work) ||
        fwrite(vga->framebuffer,1,64000,stdout)!=64000 ||
        fwrite(vga->palette,1,768,stdout)!=768 || fwrite(vga->dac,1,768,stdout)!=768 || fflush(stdout))
        f4_prom_io_error=1;
}
static int f4_prom_one(const fd2_archive *bg,const fd2_archive *figani,
        unsigned old_id,unsigned new_id,unsigned last_phase) {
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=malloc(sizeof(*before));
    int status=-1;if(!vga || !before)goto done;
    for(size_t i=0;i<64000;i++)vga->framebuffer[i]=(uint8_t)(i*13u+i/320u+31u);
    for(size_t i=0;i<768;i++){vga->palette[i]=(uint8_t)(i*7u%64u);vga->dac[i]=(uint8_t)(i*11u+29u);}
    vga->frame_deadline_ns=UINT64_C(999999999999);vga->frame_interval_ns=UINT64_C(23000000);
    *before=*vga;reset_observer();input_pumps=0;f4_prom_frame=0;f4_prom_io_error=0;
    if(fd2_prepare_promotion_open(&f4_prom_scope,vga,bg,figani,(uint8_t)old_id,(uint8_t)new_id))goto done;
    f4_promotion_observe=f4_prom_emit;
    static const unsigned values[7]={1,9,16,20,200,24,0};
    for(f4_prom_phase=0;f4_prom_phase<=last_phase;f4_prom_phase++)
        if(fd2_prepare_promotion_step(f4_prom_scope,(fd2_prepare_promotion_phase)f4_prom_phase,
            values[f4_prom_phase]) || f4_prom_io_error)goto done;
    f4_promotion_observe=NULL;
    if(cues || primary_stops || business_rng_calls || frames!=f4_prom_frame ||
       vga->frame_deadline_ns!=before->frame_deadline_ns ||
       vga->frame_interval_ns!=before->frame_interval_ns ||
       memcmp(vga->palette,before->palette,768))goto done;
    if(last_phase==6u && memcmp(vga->framebuffer,before->framebuffer,64000))goto done;
    memcpy(before->framebuffer,vga->framebuffer,64000);memcpy(before->dac,vga->dac,768);
    if(memcmp(before,vga,sizeof(*vga)))goto done;
    if(printf("{\"case_end\":%u,\"frames\":%u,\"ticks\":%u,\"delays\":%u,\"pumps\":%u,\"other_owners_unchanged\":true}\n",
        f4_prom_case,frames,ticks,delays,input_pumps)<0 || fflush(stdout))goto done;
    status=0;
done:
    f4_promotion_observe=NULL;
    fd2_prepare_promotion_close(f4_prom_scope,0);f4_prom_scope=NULL;
    free(before);free(vga);return status;
}
static int f4_promotion_cli(const char *root) {
    fd2_archive bg={0},figani={0};char path[1024],line[128];
    uint8_t *bg_before=NULL,*fig_before=NULL;unsigned values[3];
    int status=1;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)<0)return 1;
#endif
    if(f4_path(path,root,"BG.DAT") || fd2_archive_open(&bg,path) ||
       f4_path(path,root,"FIGANI.DAT") || fd2_archive_open(&figani,path) ||
       !(bg_before=malloc(bg.size)) || !(fig_before=malloc(figani.size)))goto done;
    memcpy(bg_before,bg.data,bg.size);memcpy(fig_before,figani.data,figani.size);
    if(f4_input_receipt("BG.DAT",bg.data,bg.size) || f4_input_receipt("FIGANI.DAT",figani.data,figani.size))goto done;
    for(f4_prom_case=0;fgets(line,sizeof(line),stdin);f4_prom_case++) {
        if(f4_prom_case>=64u || !strchr(line,'\n') ||
           f4_prom_parse(line,values) || f4_prom_one(&bg,&figani,values[0],values[1],values[2]))goto done;
    }
    if(ferror(stdin) || !f4_prom_case || memcmp(bg_before,bg.data,bg.size) ||
       memcmp(fig_before,figani.data,figani.size))goto done;
    if(printf("{\"end\":%u,\"binding_unchanged\":true}\n",f4_prom_case)<0 || fflush(stdout))goto done;
    status=0;
done:
    free(fig_before);free(bg_before);fd2_archive_close(&figani);fd2_archive_close(&bg);
    if(status)fputs("F4 promotion composition: request/source/output rejected\n",stderr);
    return status;
}
#endif
