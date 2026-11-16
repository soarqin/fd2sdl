#ifndef FD2_F4_DYNAMIC_PREPARE_PROBE_H
#define FD2_F4_DYNAMIC_PREPARE_PROBE_H
#include "f4_dynamic_text_probe.h"
static void f4_dynamic_prepare_advance(uint32_t ticks) {
    uint64_t at=fd2_bios_clock_deadline_ns(clock_ms*UINT64_C(1000000),ticks);
    clock_ms=(at+999999u)/UINT64_C(1000000);
}
static int f4_dynamic_prepare_line(fd2_field_prepare_assets *a,const fd2_archive *other,
    const fd2_save_file *save,const char *line) {
    f4_dynamic_request r;fd2_image portraits[4]={{0}};int result=-1;
    uint8_t *pixels[2]={malloc(64000),malloc(64000)},*mask=malloc(64000);unsigned written=0;
    fd2_vga *v=calloc(1,sizeof(*v));fd2_field_play_context *ctx=calloc(1,sizeof(*ctx)),*before=malloc(sizeof(*before));
    fd2_text_expanded expanded={0};unsigned ticks=0,pcm=0,rng=0,takes=0,injected=0,queued=0,peeked=0;
    uint32_t rng_before=0,rng_after=0;int consumer_result=0,business=1;uint8_t *save_before=NULL;
    if(f4_dynamic_parse(line,&r) || !v || !ctx || !before || !pixels[0] || !pixels[1] || !mask ||
       f4_dynamic_images(portraits,&a->dato,r.portrait))goto done;
    if(r.fragment==549u) { if(!save)goto done;save_before=malloc(save->size);if(!save_before)goto done;memcpy(save_before,save->data,save->size); }
    for(unsigned pass=0;pass<2;pass++) {
        controls(v);service_exact_time=1;memset(&v->dialog_visual,0,sizeof(v->dialog_visual));
        v->frame_deadline_ns=v->frame_interval_ns=0;memset(v->palette,165,768);
        f4_seed(v->framebuffer,64000,320,0,(int)(pass^r.seed));
        memset(ctx,0,sizeof(*ctx));ctx->has_preparation=1;ctx->preparation.options[3]=1;
        ctx->message_number=r.value;ctx->message_new_fragment=r.name;ctx->message_old_fragment=r.old_name;
        fd2_field_rng_seed(&ctx->rng,0x7a18);
        prepare_ui ui={.vga=v,.assets=a,.fdother=other,.context=ctx};
        fd2_pcm_player player={0};fd2_pcm_bank bank={0};fd2_field_audio audio={.player=&player,.ui_bank=&bank};ui.audio=&audio;
        ui.service.active_portraits=portraits;ui.service.notice_origin=fd2_portrait_notice_origin(r.portrait);ui.service.notice_open=1;
        if(r.fragment!=549u && prepare_service_portrait(&ui,v,0,1))goto done;
        if(f4_dynamic_queue(v,r.track))goto done;
        memset(&f4_dynamic,0,sizeof(f4_dynamic));f4_dynamic.active=1;f4_dynamic.vga=v;f4_dynamic.advance=f4_dynamic_prepare_advance;
        *before=*ctx;fd2_text_parameters params={r.value,r.name,r.old_name};
        if(r.fragment==549u) {
            if(r.value>=4u || !save)goto done;
            consumer_result=fd2_field_prepare_draw_slots(v,a,save,(int)r.value);
            expanded.count=0;
            for(unsigned slot=0;slot<4;slot++) {
                fd2_text_expanded one;fd2_text_parameters np={slot+1u,SIZE_MAX,SIZE_MAX};
                if(fd2_text_expand(&a->text,a->font.glyph_count,&np,549,&one) || expanded.count+one.count>FD2_TEXT_EXPANDED_CAPACITY)goto done;
                memcpy(expanded.tokens+expanded.count,one.tokens,one.count*sizeof(*one.tokens));expanded.count+=one.count;
            }
        } else {
            if(fd2_text_expand(&a->text,a->font.glyph_count,&params,r.fragment,&expanded))goto done;
            consumer_result=prepare_service_text(&ui,r.fragment,(int)r.x,(int)r.y);
        }
        before->rng=ctx->rng;business=business && !memcmp(before,ctx,sizeof(*ctx));
        if(save_before)business=business && !memcmp(save_before,save->data,save->size);
        memcpy(pixels[pass],v->framebuffer,64000);
        if(!pass) {
            ticks=f4_dynamic.ticks;pcm=f4_dynamic.pcm;rng=f4_dynamic.rng;takes=f4_dynamic.takes;injected=f4_dynamic.injected;
            queued=v->input.queued_count;peeked=r.track;rng_before=0x7a18u;rng_after=ctx->rng.state;
        } else if(ticks!=f4_dynamic.ticks || pcm!=f4_dynamic.pcm || rng!=f4_dynamic.rng || takes!=f4_dynamic.takes ||
            injected!=f4_dynamic.injected || queued!=v->input.queued_count || rng_after!=ctx->rng.state)goto done;
        if(!f4_dynamic_palette_unchanged(v) || f4_dynamic.tick_total!=f4_dynamic.ticks || v->frame_deadline_ns || v->frame_interval_ns || ui.service.input_epoch!=takes)goto done;
        f4_dynamic.active=0;
    }
    if(f4_mask(mask,pixels[0],pixels[1],64000,320,0,&written))goto done;
    result=f4_dynamic_emit(&r,pixels[0],mask,written,&expanded,consumer_result,ticks,pcm,rng,takes,injected,queued,
        peeked,rng_before,rng_after,0,0,0,business);
done:
    f4_dynamic.active=0;f4_dynamic_images_close(portraits);free(v);free(ctx);free(before);free(pixels[0]);free(pixels[1]);free(mask);free(save_before);return result;
}
static int f4_dynamic_prepare_run(const char *root) {
    fd2_archive other={0};fd2_field_prepare_assets *a=calloc(1,sizeof(*a));fd2_save_file save={0};
    uint8_t *encrypted=NULL;FILE *file=NULL;char path[1024],line[256];int result=1;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)goto done;
#endif
    if(!a || f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&other,path) ||
       f4_path(path,root,"FDTXT.DAT") || fd2_archive_open(&a->text_archive,path) ||
       f4_path(path,root,"DATO.DAT") || fd2_archive_open(&a->dato,path) ||
       fd2_font_open_fdother(&a->font,&other,4) || fd2_text_entry_open_entry(&a->text,&a->text_archive,0) ||
       fd2_field_info_assets_open(&a->info,&other) || hotel_frame(&a->slots,&other,16,1) ||
       f4_path(path,root,"START.SAV"))goto done;
    encrypted=malloc(FD2_SAVE_FILE_SIZE);file=fopen(path,"rb");if(!encrypted || !file)goto done;
    if(fread(encrypted,1,FD2_SAVE_FILE_SIZE,file)!=FD2_SAVE_FILE_SIZE || fgetc(file)!=EOF || ferror(file))goto done;
    if(fclose(file)){file=NULL;goto done;}file=NULL;
    if(fd2_save_file_open_encrypted(&save,encrypted,FD2_SAVE_FILE_SIZE) ||
       f4_input_receipt("FDOTHER.DAT",other.data,other.size) || f4_input_receipt("FDTXT.DAT",a->text_archive.data,a->text_archive.size) ||
       f4_input_receipt("DATO.DAT",a->dato.data,a->dato.size) || f4_input_receipt("START.SAV",encrypted,FD2_SAVE_FILE_SIZE))goto done;
    a->ready=1;
    while(fgets(line,sizeof(line),stdin))if(!strchr(line,'\n') || f4_dynamic_prepare_line(a,&other,&save,line))goto done;
    result=ferror(stdin)?1:0;
done:
    f4_dynamic.active=0;if(file)fclose(file);free(encrypted);fd2_save_file_close(&save);
    if(a){fd2_field_prepare_assets_close(a);free(a);}fd2_archive_close(&other);return result;
}
#endif
