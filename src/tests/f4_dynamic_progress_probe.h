#ifndef FD2_F4_DYNAMIC_PROGRESS_PROBE_H
#define FD2_F4_DYNAMIC_PROGRESS_PROBE_H
#include "f4_dynamic_text_probe.h"
static void f4_dynamic_progress_advance(uint32_t ticks) {
    uint64_t at=fd2_bios_clock_deadline_ns(now_ms*UINT64_C(1000000),ticks);
    now_ms=(at+999999u)/UINT64_C(1000000);
}
static int f4_dynamic_progress_line(fd2_field_game *g,const fd2_archive *other,const char *line) {
    f4_dynamic_request r;field_progress_box box={0};field_progress_text text={0};int result=-1,consumer_result=0;
    uint8_t *pixels[2]={malloc(64000),malloc(64000)},*mask=malloc(64000);unsigned written=0;
    fd2_vga *v=calloc(1,sizeof(*v));fd2_field_game *before=malloc(sizeof(*before));
    unsigned ticks=0,pcm=0,rng=0,takes=0,injected=0,queued=0;int business=1,prefix=0;
    if(f4_dynamic_parse(line,&r) || !v || !before || !pixels[0] || !pixels[1] || !mask ||
       f4_dynamic_images(box.portraits,&g->dato_archive,r.portrait))goto done;
    box.portrait_origin=(int)fd2_portrait_notice_origin(r.portrait);box.row=2;
    for(unsigned pass=0;pass<2;pass++) {
        reset(v);finite_probe=1;memset(&v->dialog_visual,0,sizeof(v->dialog_visual));
        v->frame_deadline_ns=v->frame_interval_ns=0;memset(v->palette,165,768);
        f4_seed(v->framebuffer,64000,320,0,(int)pass);progress_portrait(&box,v,0,1);
        g->message_new_fragment=r.name;g->message_old_fragment=r.old_name;
        if(f4_dynamic_queue(v,r.track))goto done;
        memset(&f4_dynamic,0,sizeof(f4_dynamic));f4_dynamic.active=1;f4_dynamic.vga=v;
        f4_dynamic.negative=r.value>INT32_MAX;f4_dynamic.advance=f4_dynamic_progress_advance;
        *before=*g;
        if((r.fragment>=481u && r.fragment<=494u) || r.fragment==587u) {
            fd2_field_progress_notice notice={0};notice.fragment=(uint16_t)r.fragment;notice.value=r.value;
            notice.kind=r.fragment==487u ? FD2_FIELD_PROGRESS_STATUS:r.fragment==488u ? FD2_FIELD_PROGRESS_GAIN:
                r.fragment==587u ? FD2_FIELD_PROGRESS_LEARN:FD2_FIELD_PROGRESS_GROWTH;
            if(r.fragment==587u) {if(r.name<441u || r.name>=477u)goto done;notice.magic_id=(uint8_t)(r.name-441u);}
            if(progress_text_prepare(g,&notice,&text))goto done;
        } else {
            fd2_field_loot_notice notice={0};notice.kind=FD2_FIELD_LOOT_MESSAGE;notice.fragment=(uint16_t)r.fragment;notice.value=r.value;
            if(r.name<181u || r.name>395u || r.old_name<181u || r.old_name>395u)goto done;
            notice.item_id=(uint8_t)(r.name-181u);notice.old_item_id=(uint8_t)(r.old_name-181u);
            if(loot_text_prepare(g,&notice,&text))goto done;
        }
        fd2_field_audio audio={0};
        consumer_result=field_text_play(&box,g,v,other,&audio,&text,(int)r.x,(int)r.y,0);
        business=business && !memcmp(before,g,sizeof(*g));
        if(f4_dynamic.negative) {
            if(!f4_dynamic.prefix_captured)goto done;
            memcpy(pixels[pass],f4_dynamic.prefix,64000);prefix=1;
        } else memcpy(pixels[pass],v->framebuffer,64000);
        unsigned t=prefix?f4_dynamic.prefix_ticks:f4_dynamic.ticks,p=prefix?f4_dynamic.prefix_pcm:f4_dynamic.pcm;
        unsigned n=prefix?f4_dynamic.prefix_rng:f4_dynamic.rng,q=prefix?f4_dynamic.prefix_queue:v->input.queued_count;
        if(!pass){ticks=t;pcm=p;rng=n;queued=q;takes=f4_dynamic.takes;injected=f4_dynamic.injected;}
        else if(ticks!=t || pcm!=p || rng!=n || queued!=q || takes!=f4_dynamic.takes || injected!=f4_dynamic.injected)goto done;
        if(!f4_dynamic_palette_unchanged(v) || f4_dynamic.tick_total!=f4_dynamic.ticks || v->frame_deadline_ns || v->frame_interval_ns)goto done;
        f4_dynamic.active=0;
    }
    if(f4_mask(mask,pixels[0],pixels[1],64000,320,0,&written))goto done;
    result=f4_dynamic_emit(&r,pixels[0],mask,written,&text,consumer_result,ticks,pcm,rng,takes,injected,queued,
        r.track,0,0,0,0,prefix,business);
done:
    f4_dynamic.active=0;f4_dynamic_images_close(box.portraits);free(v);free(before);free(pixels[0]);free(pixels[1]);free(mask);return result;
}
static int f4_dynamic_progress_run(const char *root) {
    fd2_archive other={0};fd2_field_game *g=calloc(1,sizeof(*g));char path[1024],line[256];int result=1;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)goto done;
#endif
    if(!g || f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&other,path) ||
       f4_path(path,root,"FDTXT.DAT") || fd2_archive_open(&g->fdtxt_archive,path) ||
       f4_path(path,root,"DATO.DAT") || fd2_archive_open(&g->dato_archive,path) ||
       fd2_font_open_fdother(&g->font,&other,4) || fd2_text_entry_open_entry(&g->ui_text,&g->fdtxt_archive,0) ||
       fd2_field_info_assets_open(&g->info_assets,&other) || f4_input_receipt("FDOTHER.DAT",other.data,other.size) ||
       f4_input_receipt("FDTXT.DAT",g->fdtxt_archive.data,g->fdtxt_archive.size) ||
       f4_input_receipt("DATO.DAT",g->dato_archive.data,g->dato_archive.size))goto done;
    g->ready=1;g->units.count=1;g->attack_rng=test_rng;g->fdother=&other;
    while(fgets(line,sizeof(line),stdin))if(!strchr(line,'\n') || f4_dynamic_progress_line(g,&other,line))goto done;
    result=ferror(stdin)?1:0;
done:
    if(g){fd2_field_info_assets_close(&g->info_assets);fd2_text_entry_close(&g->ui_text);fd2_archive_close(&g->fdtxt_archive);fd2_archive_close(&g->dato_archive);free(g);}
    fd2_archive_close(&other);return result;
}
#endif
