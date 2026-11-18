/* text_draw_parameters @15f84正式纯页适配器；summary @18c6d的456页API。
 * 只传已初始化font/text/page/record，normal producer与自然caller另账。
 * 实际加载FDOTHER/FDTXT同buffer在任何request前握手，不重开假身份。 */
#ifndef FD2_F4_TEXT_PREPARE_PROBE_H
#define FD2_F4_TEXT_PREPARE_PROBE_H
#include "f4_text_probe_common.h"
static int f4_text_prepare_case(fd2_field_prepare_assets *a,const char *op,unsigned fragment,unsigned style,unsigned variant) {
    int plain=!strcmp(op,"plain"),summary=!strcmp(op,"summary"),result=-1;
    unsigned stride=summary?456u:320u,height=summary?336u:200u,written=0;
    size_t size=(size_t)stride*height;
    uint8_t *pixels[2]={NULL,NULL},*mask=NULL,palette[768],pmask[768]={0};
    fd2_vga *vga=NULL,*before=NULL;fd2_field_page *page=NULL;
    uint64_t old_ms=clock_ms;unsigned old_reads=clock_reads,old_frames=frame_number,old_sound=sound_calls;
    size_t old_music=music_count;int old_options=option_sync_count;
    if((!plain && !summary) || variant || (plain && style>1u) ||
       (summary && (style || fragment<1u || fragment>136u)))goto done;
    pixels[0]=malloc(size);pixels[1]=malloc(size);mask=malloc(size);
    if(!pixels[0] || !pixels[1] || !mask)goto done;
    vga=calloc(1,sizeof(*vga));before=malloc(sizeof(*before));if(!vga || !before)goto done;
    if(summary){page=fd2_field_page_create();if(!page)goto done;}
    memset(palette,165,768);
    for(unsigned pass=0;pass<2u;pass++) {
        f4_seed(pixels[pass],size,stride,0,(int)pass);
        if(plain) {
            memcpy(vga->framebuffer,pixels[pass],size);memset(vga->palette,165,768);*before=*vga;
            fd2_text_parameters parameters={0,SIZE_MAX,SIZE_MAX};
            if(text_draw_parameters(vga,a,fragment,40,121,style?205:201,-1,&parameters) ||
               f4_vga_guard(before,vga,0))goto done;
            memcpy(pixels[pass],vga->framebuffer,size);
        } else {
            fd2_field_unit unit={0},old_unit;
            unit.text_id=(uint8_t)(fragment-1u);unit.level=1;unit.hp_max=unit.mp_max=1;old_unit=unit;
            fd2_field_background_state background=page->background;
            if(fd2_field_page_write(page,0,0,456,336,pixels[pass],size,456) ||
               fd2_field_unit_summary_draw_page(page,&a->info,&a->font,&a->text,&unit,77,77) ||
               fd2_field_page_read(page,0,0,456,336,pixels[pass],size,456) ||
               memcmp(&unit,&old_unit,sizeof(unit)) || memcmp(&page->background,&background,sizeof(background)))goto done;
        }
    }
    if(f4_mask(mask,pixels[0],pixels[1],size,stride,0,&written) || clock_ms!=old_ms || clock_reads!=old_reads ||
        frame_number!=old_frames || sound_calls!=old_sound || music_count!=old_music || option_sync_count!=old_options)goto done;
    result=f4_emit(op,fragment,style,variant,stride,height,pixels[0],palette,mask,pmask,written,0);
done:
    free(pixels[0]);free(pixels[1]);free(mask);free(vga);free(before);fd2_field_page_destroy(page);return result;
}
static int f4_text_run(const char *root) {
    fd2_archive other={0};fd2_field_prepare_assets *a=calloc(1,sizeof(*a));
    char path[1024],line[128],op[16];unsigned v[3];int result=1;
    if(!a || f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&other,path) ||
       f4_path(path,root,"FDTXT.DAT") || fd2_archive_open(&a->text_archive,path) ||
       fd2_font_open_fdother(&a->font,&other,4) || fd2_text_entry_open_entry(&a->text,&a->text_archive,0) ||
       fd2_field_info_assets_open(&a->info,&other) ||
       f4_input_receipt("FDOTHER.DAT",other.data,other.size) ||
       f4_input_receipt("FDTXT.DAT",a->text_archive.data,a->text_archive.size))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        if(!strchr(line,'\n') || f4_parse(line,op,v) || f4_text_prepare_case(a,op,v[0],v[1],v[2]))goto done;
    }
    result=ferror(stdin)?1:0;
done:
    if(a){fd2_field_prepare_assets_close(a);free(a);}fd2_archive_close(&other);
    if(result)fputs("F4 text prepare reader rejected input/output\n",stderr);
    return result;
}
#endif
