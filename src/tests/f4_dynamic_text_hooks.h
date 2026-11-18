/* F4动态UI私有观察器：只替身host wait/设备I/O和受控ACK投递。
 * 字模、formatter、portrait、正式Prepare/Play consumer全部调用真实函数。
 * 负-6不调用原越界glyph；仅保C SAFE_MINUS已清底后的原定义前缀。 */
#ifndef FD2_F4_DYNAMIC_TEXT_HOOKS_H
#define FD2_F4_DYNAMIC_TEXT_HOOKS_H
#include "font.h"
#include "text.h"
#include <inttypes.h>
static struct {
    int active,negative,prefix_captured;
    unsigned ticks,tick_total,takes,injected,pcm,rng;
    unsigned prefix_ticks,prefix_pcm,prefix_rng,prefix_queue;
    void (*advance)(uint32_t);
    fd2_vga *vga;
    uint8_t prefix[64000];
} f4_dynamic;
static int f4_dynamic_font_linear(uint8_t *p,size_t n,const fd2_font *font,
    uint32_t glyph,int64_t at,uint32_t stride,uint8_t fg,uint8_t shadow,uint8_t bg) {
    int result=fd2_font_draw_glyph_linear(p,n,font,glyph,at,stride,fg,shadow,bg);
    if(!result && f4_dynamic.active && f4_dynamic.negative && !f4_dynamic.prefix_captured &&
       glyph==10u && bg==74u && f4_dynamic.vga && p==f4_dynamic.vga->framebuffer && n==64000u) {
        memcpy(f4_dynamic.prefix,p,64000);f4_dynamic.prefix_captured=1;
        f4_dynamic.prefix_ticks=f4_dynamic.ticks;f4_dynamic.prefix_pcm=f4_dynamic.pcm;
        f4_dynamic.prefix_rng=f4_dynamic.rng;f4_dynamic.prefix_queue=f4_dynamic.vga->input.queued_count;
    }
    return result;
}
static int f4_dynamic_wait_ticks(uint32_t ticks) {
    if(f4_dynamic.active) {
        f4_dynamic.ticks++;f4_dynamic.tick_total+=ticks;
        if(f4_dynamic.advance)f4_dynamic.advance(ticks);
        return 0;
    }
    fd2_wait_ticks(ticks);
return 0;
}
static int f4_dynamic_take_key(fd2_input *input,fd2_input_event *event) {
    if(f4_dynamic.active) {
        f4_dynamic.takes++;
        if(!input->queued_count) {
            input->queued_head=0;input->queued_count=1;
            input->queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_ENTER,.bios_code=0x1c0d};
            f4_dynamic.injected++;
        }
    }
    return fd2_input_take_key(input,event);
}
static void f4_dynamic_upload(fd2_vga *vga) {
    if(!f4_dynamic.active)fd2_vga_upload_current_frame(vga);
}
#endif
