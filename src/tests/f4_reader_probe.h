/* F4只读资源探针，限现有测试私有TU；不是shipping API或另一个decoder。
 * 原依据：4e22a/4e1a6、4ebff/4ec31、4ed7a、2eb9f→4e98d、36fd3/36ff4。
 * 基础op expected来自独立原码执行；extra字节op允许source-derived数学合同，
 * 资格由runner逐op记载。LUT返回copy mask不代表原LUT/RMW写集。
 * 两底图法仅适用这些不读旧目标值的叶：相等为已写，不等须仍各等原底图。
 * ANI的两份scratch每帧重置，实际连续历史另存；不以历史相等误作未写。
 * stdin: op a b variant\n；stdout: JSON一行，然后page,palette,mask,pal-mask。
 * 输出只供tools/verify_f4_resource_pixels.py流式读取，失败不输出部分payload。 */
#ifndef FD2_F4_READER_PROBE_H
#define FD2_F4_READER_PROBE_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static void f4_seed(uint8_t *dst,size_t n,unsigned stride,int uniform,int complement) {
    for(size_t i=0;i<n;i++) {
        uint8_t value=uniform?83u:(uint8_t)(i*17u+i/stride+83u);
        dst[i]=complement?(uint8_t)(value^255u):value;
    }
}
static int f4_mask(uint8_t *mask,const uint8_t *a,const uint8_t *b,
        size_t n,unsigned stride,int uniform,unsigned *count) {
    *count=0;
    for(size_t i=0;i<n;i++) {
        uint8_t seed=uniform?83u:(uint8_t)(i*17u+i/stride+83u);
        mask[i]=(uint8_t)(a[i]==b[i]);
        if(!mask[i] && (a[i]!=seed || b[i]!=(uint8_t)(seed^255u)))return -1;
        *count+=mask[i];
    }
    return 0;
}
static int f4_palette_mask(uint8_t mask[768],const uint8_t a[768],
        const uint8_t b[768],unsigned *count) {
    *count=0;
    for(unsigned i=0;i<768;i++) {
        mask[i]=(uint8_t)(a[i]==b[i]);
        if(!mask[i] && (a[i]!=165u || b[i]!=90u))return -1;
        *count+=mask[i];
    }
    return 0;
}
static int f4_vga_guard(const fd2_vga *before,const fd2_vga *after,int afm) {
    fd2_vga normalized=*after;
    memcpy(normalized.framebuffer,before->framebuffer,sizeof(normalized.framebuffer));
    if(afm) {
        /* 实际C setter同时投影DAC；原AFM临时palette叶无DAC I/O。
         * 只比较base palette字节；DAC资格留给真实caller，不伪造原mask。 */
        memcpy(normalized.palette,before->palette,768);
        memcpy(normalized.dac,before->dac,768);
    }
    return memcmp(before,&normalized,sizeof(normalized))?-1:0;
}
static int f4_emit(const char *op,unsigned a,unsigned b,unsigned variant,
        unsigned stride,unsigned height,const uint8_t *pixels,const uint8_t palette[768],
        const uint8_t *mask,const uint8_t palette_mask[768],unsigned written,unsigned pal_written) {
    size_t n=(size_t)stride*height;
    if(printf("{\"schema\":\"fd2-f4-reader-v1\",\"status\":0,\"op\":\"%s\","
        "\"a\":%u,\"b\":%u,\"variant\":%u,\"stride\":%u,\"height\":%u,"
        "\"page_bytes\":%zu,\"payload_bytes\":%zu,\"pixel_addresses_written\":%u,"
        "\"palette_addresses_written\":%u,\"owner_guard\":true}\n",
        op,a,b,variant,stride,height,n,n*2u+1536u,written,pal_written)<0)return -1;
    if(fwrite(pixels,1,n,stdout)!=n || fwrite(palette,1,768,stdout)!=768 ||
       fwrite(mask,1,n,stdout)!=n || fwrite(palette_mask,1,768,stdout)!=768)return -1;
    return fflush(stdout)?-1:0;
}
static int f4_parse(char *line,char op[16],unsigned values[3]) {
    char *p=line,*end;size_t n=0;
    while(isspace((unsigned char)*p))p++;
    while(isalpha((unsigned char)*p)) {if(n>=15u)return -1;op[n++]=*p++;}
    op[n]=0;if(!n || !isspace((unsigned char)*p))return -1;
    for(unsigned i=0;i<3;i++) {
        while(isspace((unsigned char)*p))p++;
        if(!isdigit((unsigned char)*p))return -1;
        errno=0;unsigned long value=strtoul(p,&end,10);
        if(errno || value>65535u || end==p)return -1;
        values[i]=(unsigned)value;p=end;
        if(i<2 && !isspace((unsigned char)*p))return -1;
    }
    while(isspace((unsigned char)*p))p++;
    return *p?-1:0;
}
static int f4_path(char out[1024],const char *root,const char *name) {
    int n=snprintf(out,1024,"%s/%s",root,name);return n<0 || n>=1024?-1:0;
}
/* 从实际reader载入并继续借用的同一buffer回传；Python在首request前核
 * 已pin的冻结bytes。只走stdout，不落资源dump，不能由晚文件guard替代。 */
static int f4_input_receipt(const char *name,const uint8_t *data,size_t size) {
    if(!data || !size || size>32u*1024u*1024u ||
        printf("{\"schema\":\"fd2-f4-input-v1\",\"input_name\":\"%s\",\"payload_bytes\":%zu}\n",name,size)<0 ||
        fwrite(data,1,size,stdout)!=size)return -1;
    return fflush(stdout)?-1:0;
}

#ifdef FD2_F4_READER_SCENE
#include "figani.h"
#include "font.h"
#include "shape.h"
#include "scene_cinematic.h"
#include "field_visual.h"
#include "field_background.h"
#include "field_page_private.h" /* 仅私有probe观察既有background reader返回owner。 */
#include "portrait.h"

typedef struct {
    fd2_archive shape,dato,figani,fdother,bg,tai,title;
    fd2_map_sprite_bank icons;
} f4_resources;
static int f4_scene_open(f4_resources *r,const char *root) {
    char path[1024];
#define F4_OPEN(field,name) do {if(f4_path(path,root,name) || fd2_archive_open(&r->field,path))return -1;} while(0)
    F4_OPEN(shape,"FDSHAP.DAT");F4_OPEN(dato,"DATO.DAT");
    F4_OPEN(figani,"FIGANI.DAT");F4_OPEN(fdother,"FDOTHER.DAT");
    F4_OPEN(bg,"BG.DAT");F4_OPEN(tai,"TAI.DAT");F4_OPEN(title,"TITLE.DAT");
#undef F4_OPEN
    if(f4_path(path,root,"FDICON.B24") || fd2_map_sprite_bank_open(&r->icons,path))return -1;
    return f4_input_receipt("FDSHAP.DAT",r->shape.data,r->shape.size) ||
        f4_input_receipt("DATO.DAT",r->dato.data,r->dato.size) ||
        f4_input_receipt("FIGANI.DAT",r->figani.data,r->figani.size) ||
        f4_input_receipt("FDOTHER.DAT",r->fdother.data,r->fdother.size) ||
        f4_input_receipt("FDICON.B24",r->icons.data,r->icons.size) ||
        f4_input_receipt("BG.DAT",r->bg.data,r->bg.size) ||
        f4_input_receipt("TAI.DAT",r->tai.data,r->tai.size) ||
        f4_input_receipt("TITLE.DAT",r->title.data,r->title.size)?-1:0;
}
static void f4_scene_close(f4_resources *r) {
    fd2_archive_close(&r->shape);fd2_archive_close(&r->dato);
    fd2_archive_close(&r->figani);fd2_archive_close(&r->fdother);
    fd2_archive_close(&r->bg);fd2_archive_close(&r->tai);fd2_archive_close(&r->title);
    fd2_map_sprite_bank_close(&r->icons);
}
static int f4_page_draw(f4_resources *r,const char *op,unsigned a,unsigned b,
        unsigned variant,uint8_t *pixels,unsigned stride,unsigned height) {
    fd2_field_page *page=fd2_field_page_create();int result=-1;
    if(!page)return -1;
    if(stride!=456u || height!=336u || fd2_field_page_write(page,0,0,456,336,
        pixels,FD2_FIELD_PAGE_SIZE,456))goto done;
    if(!strcmp(op,"shape")) {
        fd2_shape_sheet sheet={0};const uint8_t *src;size_t size;
        if(a>=33u || variant || fd2_shape_sheet_open_entry(&sheet,&r->shape,a*2u))goto done;
        result=fd2_shape_sheet_get_frame(&sheet,b,&src,&size);
        if(!result)result=fd2_field_page_blit_rle(page,72,72,sheet.width,sheet.height,src,size,NULL);
        fd2_shape_sheet_close(&sheet);
    } else {
        fd2_field_units units={0},before;
        if(a>=r->icons.frame_count || b || variant>1u)goto done;
        units.count=1;units.items[0].unit_id=(uint8_t)(a/12u);
        units.items[0].direction=(uint8_t)((a%12u)/3u);
        units.items[0].flags=variant?0x80u:0u;before=units;
        /* 正式127e0叶负责acted映色；不在probe复制(i&7)+24。 */
        result=fd2_field_units_render_page_actor(page,&r->icons,&units,0,0,0,
            (uint8_t)(a%3u),0,0,6);
        if(memcmp(&before,&units,sizeof(units)))result=-1;
    }
    if(!result)result=fd2_field_page_read(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456);
done:
    fd2_field_page_destroy(page);return result;
}

/* 扩展资源reader；仍复用同一f4协议/owner。图像项没有C expected。
 * LUT只回实际reader载入的256B，不把copy mask称作LUT/RMW应用证明。
 * rawUI依1685c/4ed0b，wave源依10652；palette仅比较资源基色bytes，
 * 不把原源指针改绑说成768B原内存写事件或DAC时点。 */
static int f4_ui_codec(unsigned frame) {
    if(frame<20u || (frame>=23u && frame<=30u) || (frame>=53u && frame<=57u) ||
       (frame>=59u && frame<=67u) || frame==92u)return 0;
    if((frame>=31u && frame<=52u) || frame==93u || (frame>=119u && frame<=136u))return 1;
    if((frame>=20u && frame<=22u) || (frame>=68u && frame<=91u) ||
       (frame>=94u && frame<=118u))return 2;
    return -1; /* 58/137没有本候选的codec/正式绑定资格。 */
}
static int f4_extra_scene_case(f4_resources *r,const char *op,unsigned a,unsigned b,unsigned variant) {
    int raw_ui=!strcmp(op,"uiraw"), lut=!strcmp(op,"lut"), pal=!strcmp(op,"palette");
    int wave=!strcmp(op,"waveraw"), cursor=!strcmp(op,"cursor");
    int ui_rle=!strcmp(op,"uirle"),ui_lmi=!strcmp(op,"uilmi");
    int bg=!strcmp(op,"bg"),tai=!strcmp(op,"tai"),title=!strcmp(op,"title");
    int nested=!strcmp(op,"nested"),other=!strcmp(op,"other");
    if(!raw_ui && !lut && !pal && !wave && !cursor && !ui_rle && !ui_lmi &&
       !bg && !tai && !title && !nested && !other)return -2;
    if(b || variant)return -1;
    uint64_t old_ms=test_ms;int old_presents=presents;
    unsigned old_water=water_calls,old_clock=clock_leaf_count,old_clears=motion_clears;
    uint8_t palette[768],pmask[768]={0};memset(palette,165,768);
    const uint8_t *data=NULL;size_t length=0,size=0;unsigned stride=0,height=0,written=0;
    uint8_t *pixels[2]={NULL,NULL},*mask=NULL;fd2_vga *vga=NULL,*before=NULL;
    fd2_field_page *page=NULL;fd2_shape_sheet shape={0};fd2_image image={0};
    fd2_field_visuals visuals={0};fd2_ui_sheet ui={0};fd2_archive child={0};int result=-1;
    if(lut) {
        if(a>=23u || fd2_field_visuals_open(&visuals,&r->fdother))goto done;
        data=fd2_field_lut_frame(&visuals,a);if(!data)goto done;
        stride=256;height=1;size=256;
    } else if(wave) {
        if(a!=9u && a!=28u)goto done;
        page=fd2_field_page_create();
        if(!page || fd2_field_background_rebuild(page,&r->fdother,a) ||
           page->background.kind!=1u || page->background.source_size!=64000u ||
           page->background.width!=320 || page->background.height!=200)goto done;
        data=page->background.source;stride=320;height=200;size=64000;
    } else if(pal) {
        if(a!=0u && a!=8u && a!=57u && a!=76u && a!=99u && a!=101u && a!=102u)goto done;
        if(fd2_archive_get(&r->fdother,a,&data,&length) || length!=768u)goto done;
        for(size_t i=0;i<length;i++)if(data[i]>63u)goto done;
        stride=320;height=200;size=64000;
    } else if(raw_ui || ui_rle || ui_lmi) {
        if(f4_ui_codec(a)!=(raw_ui?0:ui_rle?1:2) || ui_sheet_open_fdother(&ui,&r->fdother,5))goto done;
        uint32_t start=rd_u32_le(ui.data+6u+a*4u);
        uint32_t end=a+1u<ui.count?rd_u32_le(ui.data+10u+a*4u):(uint32_t)ui.size;
        if(start>=end || end>ui.size || end-start<4u)goto done;
        data=ui.data+start;length=end-start;
        if(raw_ui) {
            int width_px,height_px;
            if(ui_sheet_get_tile(&ui,(uint16_t)a,NULL,&width_px,&height_px) ||
               width_px+4>320 || height_px+4>200)goto done;
            stride=320;height=200;size=64000;
        }
    } else if(cursor) {
        if(a>=20u || fd2_shape_sheet_open_entry(&shape,&r->fdother,1) ||
            shape.width!=24 || shape.height!=24 || shape.frame_count!=20u ||
            fd2_shape_sheet_get_frame(&shape,a,&data,&length))goto done;
        stride=height=24;size=576;
    } else {
        const fd2_archive *archive=bg?&r->bg:tai?&r->tai:title?&r->title:&r->fdother;
        if(other && a!=16u && a!=17u && a!=35u && a!=36u && a!=40u && a!=41u &&
           a!=42u && a!=46u && a!=47u && a!=69u && a!=70u && a!=71u && a!=72u &&
           a!=73u && a!=74u && a!=75u && a!=100u)goto done;
        if(nested) {
            const uint8_t *bytes;size_t bytes_count;
            if(fd2_archive_get(&r->fdother,7,&bytes,&bytes_count) ||
               fd2_archive_open_mem(&child,bytes,bytes_count))goto done;
            archive=&child;
        }
        if(fd2_archive_get(archive,a,&data,&length))goto done;
    }
    if(!size) {
        if(length<4u)goto done;
        stride=rd_u16_le(data);height=rd_u16_le(data+2);
        if(!stride || !height || stride>4096u || height>4096u || height>FD2_FIELD_PAGE_SIZE/stride)goto done;
        size=(size_t)stride*height;
        if(ui_lmi && (fd2_lmi_decode_image(&image,data,length) ||
            image.width!=(int)stride || image.height!=(int)height))goto done;
    }
    pixels[0]=malloc(size);pixels[1]=malloc(size);mask=malloc(size);
    if(!pixels[0] || !pixels[1] || !mask)goto done;
    if(lut || wave || ui_lmi) {
        memcpy(pixels[0],ui_lmi?image.pixels:data,size);memset(mask,1,size);written=(unsigned)size;
    } else if(pal || raw_ui) {
        vga=calloc(1,sizeof(*vga));before=malloc(sizeof(*before));if(!vga || !before)goto done;
        for(unsigned pass=0;pass<2;pass++) {
            f4_seed(vga->framebuffer,64000,320,0,(int)pass);
            memset(vga->palette,165,768);memset(vga->dac,0,768);*before=*vga;
            if(pal)fd2_vga_set_palette(vga,data);
            else blit_ui_tile_mode(vga,&ui,(uint16_t)a,4,4,-1);
            if(f4_vga_guard(before,vga,pal))goto done;
            memcpy(pixels[pass],vga->framebuffer,64000);
        }
        if(f4_mask(mask,pixels[0],pixels[1],size,320,0,&written))goto done;
        if(pal) {memcpy(palette,vga->palette,768);memset(pmask,1,768);}
    } else {
        for(unsigned pass=0;pass<2;pass++) {
            f4_seed(pixels[pass],size,stride,0,(int)pass);
            if(fd2_image_decode_rle_pixels(pixels[pass],(int)stride,(int)height,
                cursor?data:data+4,cursor?length:length-4))goto done;
        }
        if(f4_mask(mask,pixels[0],pixels[1],size,stride,0,&written))goto done;
    }
    if(test_ms!=old_ms || presents!=old_presents || water_calls!=old_water ||
       clock_leaf_count!=old_clock || motion_clears!=old_clears)goto done;
    result=f4_emit(op,a,b,variant,stride,height,pixels[0],palette,mask,pmask,written,pal?768u:0u);
 done:
    fd2_archive_close(&child);fd2_shape_sheet_close(&shape);fd2_image_free(&image);
    fd2_field_visuals_close(&visuals);fd2_field_page_destroy(page);
    free(vga);free(before);free(pixels[0]);free(pixels[1]);free(mask);return result;
}

static int f4_scene_case(f4_resources *r,const char *op,unsigned a,unsigned b,unsigned variant) {
    int extra=f4_extra_scene_case(r,op,a,b,variant);if(extra!=-2)return extra;
    unsigned stride=320,height=200,written=0;int result=-1;
    uint64_t old_ms=test_ms;int old_presents=presents;
    unsigned old_water=water_calls,old_clock=clock_leaf_count,old_clears=motion_clears;
    fd2_figani_clip clip={0};fd2_figani_part part={0};
    if(!strcmp(op,"shape") || !strcmp(op,"icon")) {stride=456;height=336;}
    else if(!strcmp(op,"figani")) {
        if(variant>1u || fd2_figani_open_entry(&clip,&r->figani,a) ||
                fd2_figani_get_part(&clip,b,&part))return -1;
        if(!variant) {stride=part.width;height=part.height;}
        else if((size_t)part.x+part.width>320u || (size_t)part.y+part.height>200u)return -1;
    } else if(strcmp(op,"portrait") && strcmp(op,"glyph"))return -1;
    size_t size=(size_t)stride*height;
    if(!size || size>FD2_FIELD_PAGE_SIZE)return -1;
    uint8_t *pixels[2]={malloc(size),malloc(size)},*mask=malloc(size);
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=malloc(sizeof(*before));
    uint8_t palette[768],pal_mask[768]={0};memset(palette,165,768);
    if(!pixels[0] || !pixels[1] || !mask || !vga || !before)goto done;
    for(unsigned pass=0;pass<2;pass++) {
        f4_seed(pixels[pass],size,stride,0,(int)pass);
        if(!strcmp(op,"shape") || !strcmp(op,"icon")) {
            if(f4_page_draw(r,op,a,b,variant,pixels[pass],stride,height))goto done;
            continue;
        }
        if(!strcmp(op,"figani") && !variant) {
            if(fd2_image_decode_rle_pixels(pixels[pass],part.width,part.height,
                part.image_data+4,part.image_size-4))goto done;
            continue;
        }
        memcpy(vga->framebuffer,pixels[pass],64000);memcpy(vga->palette,palette,768);*before=*vga;
        if(!strcmp(op,"portrait")) {
            if(variant>1u || draw_portrait_frame(vga,&r->dato,
                variant?FD2_DIALOG_BOTTOM:FD2_DIALOG_TOP,(uint16_t)a,b))goto done;
        } else if(!strcmp(op,"glyph")) {
            fd2_font font;
            if(b || (variant!=0u && variant!=74u) ||
                fd2_font_open_fdother(&font,&r->fdother,4) || a>=font.glyph_count)goto done;
            fd2_font_draw_glyph(vga,&font,(uint16_t)a,8,4,205,74,variant?(int)variant:-1);
        } else {
            fd2_scene_cinematic *scope=NULL;
            int status=fd2_scene_cinematic_open(&scope,vga,NULL,&r->figani);
            if(!status)status=fd2_scene_cinematic_sheet_open(scope,0,a);
            if(!status)status=fd2_scene_cinematic_sheet_blit(scope,0,b,FD2_CINEMATIC_VGA,0,0,320);
            fd2_scene_cinematic_close(&scope);if(status)goto done;
        }
        if(f4_vga_guard(before,vga,0))goto done;
        memcpy(pixels[pass],vga->framebuffer,64000);
    }
    if(f4_mask(mask,pixels[0],pixels[1],size,stride,0,&written) ||
        test_ms!=old_ms || presents!=old_presents || water_calls!=old_water ||
        clock_leaf_count!=old_clock || motion_clears!=old_clears)goto done;
    result=f4_emit(op,a,b,variant,stride,height,pixels[0],palette,mask,pal_mask,written,0);
done:
    free(pixels[0]);free(pixels[1]);free(mask);free(vga);free(before);return result;
}
static int f4_reader_run(const char *root) {
    f4_resources resources={0};char line[128],op[16];unsigned v[3];int result=1;
    /* 默认测试observer可替portrait frame；CLI必须消费请求中的真实frame。 */
    portrait_frame_override=-1;
    if(f4_scene_open(&resources,root))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        if(!strchr(line,'\n') || f4_parse(line,op,v) ||
            f4_scene_case(&resources,op,v[0],v[1],v[2]))goto done;
    }
    result=ferror(stdin)?1:0;
done:
    f4_scene_close(&resources);
    if(result)fprintf(stderr,"F4 scene reader rejected request/resource/output\n");
    return result;
}
#endif

#ifdef FD2_F4_READER_ANI
static int f4_reader_run(const char *root) {
    fd2_archive archive={0};char path[1024],line[128],op[16];unsigned v[3];
    fd2_vga *history=calloc(1,sizeof(*history)),*scratch=calloc(2,sizeof(*scratch));
    fd2_vga *before=malloc(sizeof(*before));uint8_t *mask=malloc(64000),pal_mask[768];
    unsigned entry_id=UINT_MAX,next_frame=0;size_t pos=0;int result=1;
    if(!history || !scratch || !before || !mask || f4_path(path,root,"ANI.DAT") ||
        fd2_archive_open(&archive,path))goto done;
    if(f4_input_receipt("ANI.DAT",archive.data,archive.size))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        const uint8_t *entry;size_t len;unsigned written=0,pal_written=0;
        uint64_t old_ms=now_ms;int old_frames=frames,old_clears=clears;
        if(!strchr(line,'\n') || f4_parse(line,op,v) || strcmp(op,"ani") || v[2] ||
            fd2_archive_get(&archive,v[0],&entry,&len) || len<0xadu ||
            memcmp(entry,"AFM - Animation File Manager",28) ||
            rd_u16_le(entry+0xa7)!=320u || rd_u16_le(entry+0xa9)!=200u ||
            rd_u16_le(entry+0xab)!=64000u || v[1]>=rd_u16_le(entry+0xa5))goto done;
        if(!v[1]) {
            memset(history,0,sizeof(*history));memset(history->framebuffer,83,64000);
            memset(history->palette,165,768);memset(history->dac,7,768);
            entry_id=v[0];next_frame=0;pos=0xad;
        }
        if(entry_id!=v[0] || next_frame!=v[1] || pos+8u>len)goto done;
        size_t bytes=rd_u16_le(entry+pos);uint16_t commands=rd_u16_le(entry+pos+2);
        pos+=8u;if(bytes>len-pos)goto done;
        if(next_frame+1u==rd_u16_le(entry+0xa5) && pos+bytes!=len)goto done;
        *before=*history;
        if(afm_exec_frame(history,entry+pos,bytes,commands) || f4_vga_guard(before,history,1))goto done;
        for(unsigned pass=0;pass<2;pass++) {
            memset(&scratch[pass],0,sizeof(scratch[pass]));
            f4_seed(scratch[pass].framebuffer,64000,320,1,(int)pass);
            memset(scratch[pass].palette,pass?90:165,768);*before=scratch[pass];
            if(afm_exec_frame(&scratch[pass],entry+pos,bytes,commands) ||
                f4_vga_guard(before,&scratch[pass],1))goto done;
        }
        if(f4_mask(mask,scratch[0].framebuffer,scratch[1].framebuffer,64000,320,1,&written) ||
            f4_palette_mask(pal_mask,scratch[0].palette,scratch[1].palette,&pal_written))goto done;
        if(now_ms!=old_ms || frames!=old_frames || clears!=old_clears)goto done;
        if(f4_emit(op,v[0],v[1],0,320,200,history->framebuffer,history->palette,
            mask,pal_mask,written,pal_written))goto done;
        pos+=bytes;next_frame++;
    }
    result=ferror(stdin)?1:0;
done:
    fd2_archive_close(&archive);free(history);free(scratch);free(before);free(mask);
    if(result)fprintf(stderr,"F4 ANI reader rejected request/history/resource/output\n");
    return result;
}
#endif

#ifndef FD2_F4_PROTOCOL_ONLY
static int f4_reader_cli(const char *root) {
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)return 1;
#endif
    int result=f4_reader_run(root);
    if(result) {fputs("{\"schema\":\"fd2-f4-reader-v1\",\"status\":1,\"payload_bytes\":0}\n",stdout);fflush(stdout);}
    return result;
}
#endif /* FD2_F4_PROTOCOL_ONLY */
#endif
