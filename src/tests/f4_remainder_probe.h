/* F4剩余FDOTHER实际reader，仅测试私有TU；不增加产品reader/decoder。
 * 4ed34+4ed4f=raw零透明；4ebab+4ec66=LMI零透明，4ebff=全像素解码；
 * 2eb9f→4e98d=FIGANI目录/稀疏RLE，26eda/27079/279bc/29300=混合容器。
 * 整个FDOTHER实际借用buffer在首request前回传，不重新打开资源替代握手。
 * prepare输出是返回copy；copy mask不冒充原稀疏decoder的写历史。
 * 这里只签明确初始化页/固定参数；自然调用者/时点/页历史另账。 */
#ifndef FD2_F4_REMAINDER_PROBE_H
#define FD2_F4_REMAINDER_PROBE_H
#include "field_page.h"
#include "map_sprite.h"
static int f4_reader_run(const char *root);
#include "f4_reader_probe.h"

static int f4_remainder_fx(unsigned entry) {
    static const unsigned entries[]={18,19,20,21,22,23,24,25,26,27,28,30,32,33,
        34,37,38,39,43,44,45,54,58,65,66,67,68,79};
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++)if(entry==entries[i])return 1;
    return 0;
}
#ifdef FD2_F4_REMAINDER_BATTLE
#include "field_command.h"
/* 该测试target原本不链接field_command.c；私有TU纳入同一真实loader，
 * 不修改CMake/产品依赖，也不重写目录或raw decoder。 */
#include "../field_command.c"
static int f4_remainder_case(const fd2_archive *archive,
    const fd2_field_command_assets *commands,const char *op,unsigned a,unsigned b,unsigned variant) {
    battle_image image={0};battle_clip clip={0};const battle_image *drawing=NULL;
    fd2_field_page *page=NULL;fd2_vga *vga=NULL,*before=NULL;
    uint8_t *pixels[2]={NULL,NULL},*mask=NULL,palette[768],pmask[768]={0};
    unsigned stride=0,height=0,written=0;size_t size=0;int result=-1;
    int command=!strcmp(op,"command"),map=!strcmp(op,"maplmi"),fx=!strcmp(op,"otherfx");
    int direct=!strcmp(op,"direct");
    uint64_t old_ms=fake_ms;unsigned old_frames=frames,old_ticks=ticks,old_delays=delays,old_cues=cues;
    if(command) {
        if(a>=78u || b || variant || !commands->ready)goto done;
        stride=320;height=200;
    } else if(map) {
        if((a!=6u && a!=9u) || (a==6u && b==125u) || variant>1u || lmi_image(&image,archive,a,b))goto done;
        drawing=&image;
        stride=variant?456u:(unsigned)image.image.width;
        height=variant?336u:(unsigned)image.image.height;
    } else if(fx) {
        if(!f4_remainder_fx(a) || variant>1u || clip_open_visual(&clip,archive,a) ||
            b>=clip.clip.part_count)goto done;
        drawing=&clip.parts[b].drawing;
        stride=variant?320u:(unsigned)drawing->image.width;
        height=variant?200u:(unsigned)drawing->image.height;
    } else if(direct) {
        if(b || variant || (a!=10u && a!=11u && a!=56u && a!=59u && a!=60u && a!=61u && a!=62u))goto done;
        if(a==10u) {
            const uint8_t *bytes;size_t length;
            if(fd2_archive_get(archive,a,&bytes,&length) || fd2_lmi_decode_image(&image.image,bytes,length))goto done;
            /* 返回copy，不把协议全1 mask称作原decoder写历史。 */
        } else if(image_entry(&image,archive,a))goto done;
        drawing=&image;stride=(unsigned)image.image.width;height=(unsigned)image.image.height;
    } else goto done;
    size=(size_t)stride*height;
    if(!stride || !height || size>FD2_FIELD_PAGE_SIZE)goto done;
    pixels[0]=malloc(size);pixels[1]=malloc(size);mask=malloc(size);
    if(!pixels[0] || !pixels[1] || !mask)goto done;
    memset(palette,165,768);
    if((map && !variant) || (direct && a==10u)) {
        memcpy(pixels[0],drawing->image.pixels,size);memset(mask,1,size);written=(unsigned)size;
    } else for(unsigned pass=0;pass<2u;pass++) {
        f4_seed(pixels[pass],size,stride,0,(int)pass);
        if(command) {
            if(!vga) {vga=calloc(1,sizeof(*vga));before=malloc(sizeof(*before));}
            if(!vga || !before)goto done;
            memcpy(vga->framebuffer,pixels[pass],size);memset(vga->palette,165,768);*before=*vga;
            fd2_map_sprite_blit(vga,&commands->frames[a],4,4,0);
            if(f4_vga_guard(before,vga,0))goto done;
            memcpy(pixels[pass],vga->framebuffer,size);
        } else if(map) {
            page=fd2_field_page_create();
            if(!page || fd2_field_page_write(page,0,0,456,336,pixels[pass],size,456) ||
                battle_page_image(page,drawing,72,72,-1) ||
                fd2_field_page_read(page,0,0,456,336,pixels[pass],size,456))goto done;
            fd2_field_page_destroy(page);page=NULL;
        } else {
            int x=fx && variant?clip.parts[b].part.x:0,y=fx && variant?clip.parts[b].part.y:0;
            blit(pixels[pass],(int)stride,(int)height,(int)stride,drawing,x,y,-1);
        }
    }
    if(!((map && !variant) || (direct && a==10u)) &&
        f4_mask(mask,pixels[0],pixels[1],size,stride,0,&written))goto done;
    if(fake_ms!=old_ms || frames!=old_frames || ticks!=old_ticks || delays!=old_delays || cues!=old_cues)goto done;
    result=f4_emit(op,a,b,variant,stride,height,pixels[0],palette,mask,pmask,written,0);
done:
    image_close(&image);clip_close(&clip);fd2_field_page_destroy(page);
    free(pixels[0]);free(pixels[1]);free(mask);free(vga);free(before);return result;
}
static int f4_reader_run(const char *root) {
    fd2_archive archive={0};fd2_field_command_assets commands={0};char path[1024],line[128],op[16];
    unsigned v[3];int result=1;
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&archive,path) ||
       fd2_field_command_assets_open(&commands,&archive) ||
       f4_input_receipt("FDOTHER.DAT",archive.data,archive.size))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        if(!strchr(line,'\n') || f4_parse(line,op,v) ||
           f4_remainder_case(&archive,&commands,op,v[0],v[1],v[2]))goto done;
    }
    result=ferror(stdin)?1:0;
done:
    fd2_field_command_assets_close(&commands);fd2_archive_close(&archive);
    if(result)fputs("F4 remainder battle reader rejected input/output\n",stderr);
    return result;
}
#endif
#ifdef FD2_F4_REMAINDER_PREPARE
/* 仅实际正式typed reader已承担的域。hotel未消费槽保持inventory，不用商店推导。 */
static int f4_prepare_format(unsigned entry,unsigned frame) {
    if(entry==5u)return frame==137u?0:-1;
    if(entry==13u)return frame==0u?0:(frame==16u || (frame>=11u && frame<=14u) || frame==17u)?1:frame>=3u && frame<=10u?2:-1;
    unsigned count=entry==12u?29u:entry==14u?32u:entry==29u?24u:entry==63u?31u:0u;
    if(frame>=count)return -1;
    if(frame==0u || frame>=23u)return 0;
    if(frame==2u)return 4;
    if(frame>=3u && frame<=10u)return 2;
    if(frame==15u)return 3;
    if(entry==14u && frame>=18u && frame<=22u)return -1; /* 教会未消费预览label。 */
    return 1;
}
static int f4_prepare_case(const fd2_archive *archive,const char *op,unsigned a,unsigned b,unsigned variant) {
    fd2_image image={0};uint8_t *mask=NULL,palette[768],pmask[768]={0};int result=-1;
    uint64_t old_ms=clock_ms;unsigned old_reads=clock_reads,old_frames=frame_number;
    size_t old_music=music_count;int old_options=option_sync_count;unsigned old_sound=sound_calls;
    int format=f4_prepare_format(a,b);
    if(strcmp(op,"prepare") || variant || format<0)goto done;
    if((a==13u?hotel_frame(&image,archive,b,format):service_frame(&image,archive,a,b,format)))goto done;
    if(image.width<=0 || image.height<=0 || (size_t)image.width*(size_t)image.height>FD2_FIELD_PAGE_SIZE)goto done;
    size_t size=(size_t)image.width*(size_t)image.height;
    mask=malloc(size);if(!mask)goto done;memset(mask,1,size);memset(palette,165,768);
    if(clock_ms!=old_ms || clock_reads!=old_reads || frame_number!=old_frames || music_count!=old_music ||
        option_sync_count!=old_options || sound_calls!=old_sound)goto done;
    result=f4_emit(op,a,b,variant,(unsigned)image.width,(unsigned)image.height,
        image.pixels,palette,mask,pmask,(unsigned)size,0);
done:
    free(mask);fd2_image_free(&image);return result;
}
static int f4_reader_run(const char *root) {
    fd2_archive archive={0};char path[1024],line[128],op[16];unsigned v[3];int result=1;
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&archive,path) ||
       f4_input_receipt("FDOTHER.DAT",archive.data,archive.size))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        if(!strchr(line,'\n') || f4_parse(line,op,v) || f4_prepare_case(&archive,op,v[0],v[1],v[2]))goto done;
    }
    result=ferror(stdin)?1:0;
done:
    fd2_archive_close(&archive);if(result)fputs("F4 remainder prepare reader rejected input/output\n",stderr);
    return result;
}
#endif
#endif
