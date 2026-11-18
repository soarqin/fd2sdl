/* VGA投影／调色及bios_tick_delay @0x17aa9（原FUN_000151f1）、
 * field_movement_tick_edge_wait @0x13460（原sub_13460）的共享等待边界。 */
#include "vga.h"
#include "bios_clock.h"
#include <string.h>

/* 固定DOSBox-X b6abbd5的VGA_DAC_SendColor/dacexpand在6-bit模式使用
 * (v<<2)|(v>>4)；原11d40/11df2只写DAC分量，不执行这个宿主RGB扩展。
 * 正常palette组件域0..63；外来8-bit DAC模式不借此公式扩签。 */
static inline uint8_t pal_expand6(uint8_t v) {
    return (uint8_t)((v << 2) | (v >> 4));
}

int fd2_vga_init(fd2_vga *vga, SDL_Window *win, SDL_Renderer *ren) {
    (void)win;
    memset(vga, 0, sizeof(*vga));
    fd2_input_init(&vga->input);
    vga->renderer = ren;
    vga->texture = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     VGA_W, VGA_H);
    /* 整数缩放保留 VGA 像素边界，避免地图颜色混进黑边。 */
    if (!vga->texture || !SDL_SetTextureScaleMode(vga->texture, SDL_SCALEMODE_NEAREST)) {
        fd2_vga_close(vga);
        vga->output_error = FD2_VGA_OUTPUT_CREATE;
        return -1;
    }
    vga->field_page=fd2_field_page_create();
    if(!vga->field_page) {
        fd2_vga_close(vga);
        return -1;
    }
    return 0;
}

void fd2_vga_clear(fd2_vga *vga, uint8_t fill) {
    /* 复现 FUN_00035058 @0x37910: 清屏(0xa0000, fill, pattern)
     * FUN_00035058(0xa0000, 0, &DAT_0000fa00) = 用 0 填充显存 */
    memset(vga->framebuffer, fill, sizeof(vga->framebuffer));
}

void fd2_vga_set_palette(fd2_vga *vga, const uint8_t pal6[768]) {
    /* 复现: DAT_00003a65 = pal; FUN_0000f488(0, 0xff, flag)
     * FUN_0000f488 遍历调色板，通过 out(0x3c8,idx)+out(0x3c9,r/g/b) 写 DAC */
    memcpy(vga->palette, pal6, VGA_PALETTE_SIZE);
    for (int i = 0; i < 256; i++) {
        vga->dac[i*3]   = pal_expand6(pal6[i*3]);
        vga->dac[i*3+1] = pal_expand6(pal6[i*3+1]);
        vga->dac[i*3+2] = pal_expand6(pal6[i*3+2]);
    }
}

void fd2_vga_set_brightness(fd2_vga *vga, int brightness) {
    /* 旧void/clamp接口兼容；合法6bit域统一复用dac_apply_darkness @0x11d40
     * （原登记同名）。原“暗度”是逐分量减法，不是乘法亮度比例。 */
    if (brightness < 0) brightness = 0;
    if (brightness > 0x40) brightness = 0x40;
    (void)fd2_vga_palette_subtract(vga,0,255,brightness);
}

int fd2_vga_palette_add(fd2_vga *vga, int first, int last, int32_t raw_add) {
    /* pal_partial_set @0x11df2（原登记同名），包括1951f共享返回尾。
     * source的有符号DWORD加法在0..255偏移／6bit基色内无溢出。 */
    if (!vga || first < 0 || first > 255 || last < 0 || last > 255 || raw_add < 0 || raw_add > 255) return -1;
    for (int i = first*3; i < (last+1)*3; i++) if (vga->palette[i] > 63) return -1;
    for (int i = first*3; i < (last+1)*3; i++) {
        int32_t value = (int32_t)vga->palette[i] + raw_add;
        if (value > 63) value = 63;
        vga->dac[i] = pal_expand6((uint8_t)value);
    }
    return 0;
}

int fd2_vga_palette_subtract(fd2_vga *vga, int first, int last, int32_t darkness) {
    /* dac_apply_darkness @0x11d40（原登记同名）及1951f只读返回尾。
     * 原1f882/1f525在完整冷序列中分别传0..63/64..0；不改base palette。 */
    if (!vga || first < 0 || first > 255 || last < 0 || last > 255 || darkness < 0 || darkness > 64) return -1;
    for (int i = first * 3; i < (last + 1) * 3; i++) if (vga->palette[i] > 63) return -1;
    for (int i = first * 3; i < (last + 1) * 3; i++) {
        int32_t value = (int32_t)vga->palette[i] - darkness;
        if (value < 0) value = 0;
        vga->dac[i] = pal_expand6((uint8_t)value);
    }
    return 0;
}

int32_t fd2_vga_bios_tick_elapsed(uint16_t start, uint16_t now) {
    int32_t first = start < 0x8000u ? (int32_t)start : (int32_t)start - 65536;
    int32_t current = now < 0x8000u ? (int32_t)now : (int32_t)now - 65536;
    int32_t delta = current - first;
    return delta < 0 ? delta + 65536 : delta;
}

void fd2_vga_palette_fade_to(fd2_vga *vga, int start, int steps,
                               uint8_t r, uint8_t g, uint8_t b) {
    /* 复现 FUN_0002b649 @0x2df01: palette_fade_to(start, steps, r, g, b)
     * 从基色(r,g,b)渐变到 vga->palette，分 0x28=40 步
     * 每个分量: dac = (pal[i] - base) * steps / 0x28 + base
     * 注意: pal 是 6-bit，base 也是 6-bit */
    const int total = 0x28; /* 40 步 */
    for (int i = start; i < 256; i++) {
        int idx = i * 3;
        uint8_t dr = (uint8_t)(((vga->palette[idx]   - r) * steps) / total + r);
        uint8_t dg = (uint8_t)(((vga->palette[idx+1] - g) * steps) / total + g);
        uint8_t db = (uint8_t)(((vga->palette[idx+2] - b) * steps) / total + b);
        vga->dac[idx]   = pal_expand6(dr);
        vga->dac[idx+1] = pal_expand6(dg);
        vga->dac[idx+2] = pal_expand6(db);
    }
}

/* 真实SDL输出首错只归当前VGA owner；不改游戏时钟/RNG/Input。 */
static void vga_output_fail(fd2_vga *vga,int error) {
    if(vga&&!vga->output_error)vga->output_error=error;
}
void fd2_vga_upload_current_frame(fd2_vga *vga) {
    /* 离屏对象没有终端；普通App在同一renderer/texture上传当前逻辑页。 */
    if(!vga || !vga->renderer || !vga->texture || vga->output_error)return;
    uint32_t colors[256];
    for(int i=0;i<256;++i)colors[i]=0xff000000u |
        (uint32_t)vga->dac[i*3]<<16 | (uint32_t)vga->dac[i*3+1]<<8 | vga->dac[i*3+2];
    uint8_t *pixels;int pitch;
    if(!SDL_LockTexture(vga->texture,NULL,(void **)&pixels,&pitch)) {
        vga_output_fail(vga,FD2_VGA_OUTPUT_LOCK);return;
    }
    if(!pixels || pitch<VGA_W*(int)sizeof(uint32_t)) {
        vga_output_fail(vga,FD2_VGA_OUTPUT_PITCH);SDL_UnlockTexture(vga->texture);return;
    }
    for(int y=0;y<VGA_H;++y) {
        uint32_t *dst=(uint32_t *)(pixels+(size_t)y*(size_t)pitch);
        const uint8_t *src=vga->framebuffer+(size_t)y*VGA_STRIDE;
        for(int x=0;x<VGA_W;++x)dst[x]=colors[src[x]];
    }
    SDL_UnlockTexture(vga->texture);
    if(!SDL_RenderTexture(vga->renderer,vga->texture,NULL,NULL)) {
        vga_output_fail(vga,FD2_VGA_OUTPUT_RENDER);return;
    }
    if(!SDL_RenderPresent(vga->renderer))vga_output_fail(vga,FD2_VGA_OUTPUT_PRESENT);
}
int fd2_vga_output_error(const fd2_vga *vga) {return vga?vga->output_error:0;}

static int vga_observe_checked(const fd2_vga *vga,uint64_t now,uint64_t *actual) {
    if(!actual || fd2_bios_clock_error()<0)return -1;
    if(fd2_vga_output_error(vga)<0)return -1;
    uint64_t completed;
    int status=fd2_bios_clock_observe_checked(now,&completed);
    if(status<0)return status;
    if(fd2_vga_output_error(vga)<0)return -1;
    *actual=completed;return 0;
}

int fd2_vga_observe_checked(const fd2_vga *vga,uint64_t now,uint64_t *actual) {
    if(!vga)return -1;
    return vga_observe_checked(vga,now,actual);
}

static int fd2_vga_present_impl(fd2_vga *vga) {
    if(fd2_vga_output_error(vga)<0)return -1;
    uint64_t actual;
    if(vga_observe_checked(vga,SDL_GetTicksNS(),&actual)<0)return -1;
    /* 完成同一时钟观察后才泵输入；输出失败保已经发生的前缀。 */
    fd2_input_begin_frame(&vga->input);
    fd2_vga_upload_current_frame(vga);
    return fd2_vga_output_error(vga)<0?-1:0;
}

static int fd2_delay_until(fd2_vga *vga,uint64_t deadline_ns) {
    const uint64_t ns_per_ms=1000000u;
    if(fd2_bios_clock_error()<0)return -1;
    if(vga&&fd2_vga_output_error(vga)<0)return -1;
    for(;;) {
        fd2_input_poll_host_events(NULL);
        if(fd2_input_host_quit_requested())break;
        uint64_t now;
        if(vga_observe_checked(vga,SDL_GetTicksNS(),&now)<0)return -1;
        if(now>=deadline_ns)break;
        uint64_t remain=deadline_ns-now;
        if(remain>2u*ns_per_ms) {
            uint64_t sleep_ms=remain/ns_per_ms-1u;
            if(sleep_ms>8u)sleep_ms=8u;
            SDL_Delay((uint32_t)sleep_ms);
        } else SDL_DelayPrecise(remain);
    }
    fd2_input_poll_host_events(NULL);
    return 0;
}

int fd2_vga_present(fd2_vga *vga) {
    if(!vga || fd2_bios_clock_error()<0)return -1;
    if(fd2_vga_output_error(vga)<0)return -1;
    if(vga->frame_deadline_ns && SDL_GetTicksNS()<vga->frame_deadline_ns &&
        fd2_delay_until(vga,vga->frame_deadline_ns)<0)return -1;
    if(fd2_vga_present_impl(vga)<0)return -1;
    vga->frame_deadline_ns=0;vga->frame_interval_ns=0;
    return 0;
}

int fd2_vga_wait_frame_deadline(fd2_vga *vga) {
    if(!vga || fd2_bios_clock_error()<0)return -1;
    if(fd2_vga_output_error(vga)<0)return -1;
    if(vga->frame_deadline_ns && SDL_GetTicksNS()<vga->frame_deadline_ns &&
        fd2_delay_until(vga,vga->frame_deadline_ns)<0)return -1;
    vga->frame_deadline_ns=0;vga->frame_interval_ns=0;
    return 0;
}

int fd2_vga_present_timed(fd2_vga *vga,uint32_t frame_ms) {
    if(!vga || fd2_bios_clock_error()<0)return -1;
    if(fd2_vga_output_error(vga)<0)return -1;
    if(frame_ms==0)return fd2_vga_present(vga);
    uint64_t interval=(uint64_t)frame_ms*1000000u;
    uint64_t now=SDL_GetTicksNS(),target=vga->frame_deadline_ns;
    int continue_schedule=target && vga->frame_interval_ns==interval && now<=target+interval;
    if(target && now<target && fd2_delay_until(vga,target)<0)return -1;
    if(fd2_vga_present_impl(vga)<0)return -1;
    now=SDL_GetTicksNS();
    vga->frame_deadline_ns=continue_schedule?target+interval:now+interval;
    vga->frame_interval_ns=interval;
    return 0;
}

static int vga_delay_ms(fd2_vga *vga,uint32_t ms) {
    if(fd2_bios_clock_error()<0)return -1;
    if(fd2_vga_output_error(vga)<0)return -1;
    if(ms==0) {
        uint64_t actual;
        if(vga_observe_checked(vga,SDL_GetTicksNS(),&actual)<0)return -1;
        fd2_input_poll_host_events(NULL);return 0;
    }
    return fd2_delay_until(vga,SDL_GetTicksNS()+(uint64_t)ms*1000000u);
}

static int vga_wait_ticks(fd2_vga *vga,uint32_t ticks) {
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    uint64_t actual;
    if(vga_observe_checked(vga,SDL_GetTicksNS(),&actual)<0)return -1;
    uint16_t started=fd2_bios_tick_low_ns(actual);
    for(;;) {
        fd2_input_poll_host_events(NULL);
        if(fd2_input_host_quit_requested())return 0;
        uint64_t now=SDL_GetTicksNS();
        if(vga_observe_checked(vga,now,&actual)<0)return -1;
        if(fd2_vga_bios_tick_elapsed(started,fd2_bios_tick_low_ns(actual))>=(int64_t)ticks)break;
        uint64_t deadline=fd2_bios_clock_deadline_ns(now,1);
        /* 无有效期限时不能等待MAX并绕过外层BDA比较。 */
        if(deadline==UINT64_MAX)return -1;
        if(fd2_delay_until(vga,deadline)<0)return -1;
    }
    /* 17ae4源尾独立读点；失败不能当等待成功。 */
    if(vga_observe_checked(vga,SDL_GetTicksNS(),&actual)<0)return -1;
    return 0;
}

/* 无VGA入口仅供独立时钟诊断；正式App调用下列显式owner接口。 */
int fd2_delay_ms(uint32_t ms){return vga_delay_ms(NULL,ms);}
int fd2_wait_ticks(uint32_t ticks){return vga_wait_ticks(NULL,ticks);}
int fd2_vga_delay_ms(fd2_vga *vga,uint32_t ms){
    return vga ? vga_delay_ms(vga,ms) : -1;
}
int fd2_vga_wait_ticks(fd2_vga *vga,uint32_t ticks){
    return vga ? vga_wait_ticks(vga,ticks) : -1;
}

int fd2_vga_wait_movement_tick(fd2_vga *vga) {
    if(!vga || fd2_bios_clock_error()<0)return -1;
    if(fd2_vga_output_error(vga)<0)return -1;
    for(;;) {
        fd2_input_poll_host_events(NULL);
        if(fd2_input_take_quit(&vga->input))return 1;
        uint64_t now=SDL_GetTicksNS(),actual;
        if(vga_observe_checked(vga,now,&actual)<0)return -1;
        if(fd2_bios_tick_signed_ns(actual)!=vga->movement_tick)break;
        uint64_t deadline=fd2_bios_clock_deadline_ns(now,1);
        /* 无有效期限时不能等待MAX并绕过外层BDA比较。 */
        if(deadline==UINT64_MAX)return -1;
        if(fd2_delay_until(vga,deadline)<0)return -1;
    }
    uint64_t actual;
    if(vga_observe_checked(vga,SDL_GetTicksNS(),&actual)<0)return -1;
    vga->movement_tick=fd2_bios_tick_signed_ns(actual);
    return 0;
}

int fd2_input_check(fd2_vga *vga) {
    /* 对应10620，只peek现input owner；不泵事件、不消费或清余队。 */
    return vga && fd2_input_has_any_key(&vga->input);
}

void fd2_vga_close(fd2_vga *vga) {
    if(!vga)return;
    fd2_field_page_destroy(vga->field_page);vga->field_page=NULL;
    if (vga->texture) SDL_DestroyTexture(vga->texture);
    vga->texture = NULL;
}
