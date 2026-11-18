/* animation_play @0x20421：入口clear、delay后peek skip、仅续播帧尾clear。
 * 使用合成AFM与本地只读ANI0/2及真实唯一input owner；仅窗口/计时替身。 */
#include <stdio.h>
#include <string.h>
#include "animation.h"
#include "input.h"

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)

static fd2_vga *screen;
static uint64_t now_ms;
static int frames, inject, inject_quit, clears, quit_on_clear, foreign_presents;
static void animation_clear(fd2_input *input) {
    fd2_input_clear_keys(input);
    if (++clears==quit_on_clear) input->quit_requested=1;
}

static int animation_present(fd2_vga *vga) {
    foreign_presents++;
    vga->frame_deadline_ns=vga->frame_interval_ns=0;
return 0;
}
static void animation_upload(fd2_vga *vga) {
    (void)vga;
    frames++;
}
static int animation_delay(uint32_t ms) {
    now_ms += ms;
    if (frames == 1 && inject) {
        fd2_input_set_test_key_state(&screen->input,SDL_SCANCODE_RIGHT,1);
        fd2_input_begin_test_frame(&screen->input,++now_ms);
        fd2_input_set_test_key_state(&screen->input,SDL_SCANCODE_RIGHT,0);
        fd2_input_set_test_key_state(&screen->input,SDL_SCANCODE_RETURN,1);
        fd2_input_begin_test_frame(&screen->input,++now_ms);
        fd2_input_set_test_key_state(&screen->input,SDL_SCANCODE_RETURN,0);
        fd2_input_begin_test_frame(&screen->input,++now_ms);
        if (inject_quit) screen->input.quit_requested = 1;
    }
return 0;
}

#define fd2_vga_present animation_present
#define fd2_vga_upload_current_frame animation_upload
#define fd2_delay_ms animation_delay
#define fd2_vga_delay_ms(vga,value) animation_delay(value)
#define fd2_input_clear_keys animation_clear
#define fd2_animation_play_hooked input_test_animation_play_hooked
#define fd2_animation_play input_test_animation_play
#include "../animation.c"
#undef fd2_vga_present
#undef fd2_vga_upload_current_frame
#undef fd2_delay_ms
#undef fd2_vga_delay_ms
#undef fd2_input_clear_keys
#undef fd2_animation_play_hooked
#undef fd2_animation_play

static void word(uint8_t *p,unsigned value) { p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8); }
static void reset(fd2_vga *vga) {
    memset(vga,0,sizeof(*vga));fd2_input_init(&vga->input);
    screen=vga;now_ms=0;frames=inject=inject_quit=clears=quit_on_clear=foreign_presents=0;
    vga->frame_deadline_ns=UINT64_C(999999999999);vga->frame_interval_ns=16000000;
}

/* 原20421/36ff4实际ANI0=96帧、ANI2=26帧；仅present/delay替身，
 * 文件、目录和每帧decoder真实执行，不以Scene的ANI成功spy代替。 */
static unsigned real_frames;
static uint8_t real_first[VGA_W*VGA_H];
static void real_frame(void *userdata,uint16_t frame) {
    fd2_vga *vga=userdata;
    if(!frame)memcpy(real_first,vga->framebuffer,sizeof(real_first));
    if(frame!=real_frames)real_frames=UINT16_MAX;
    else real_frames++;
}
static int real_ani_checks(fd2_vga *vga) {
    fd2_archive archive={0};CHECK(fd2_archive_open(&archive,"original_game/ANI.DAT")==0);
    const int ids[2]={0,2},counts[2]={96,26};const uint32_t delays[2]={15,100};
    for(unsigned i=0;i<2;i++) {
        reset(vga);real_frames=0;
        int status=input_test_animation_play_hooked(vga,&archive,ids[i],delays[i],0,real_frame,vga);
        if(status)fprintf(stderr,"actual ANI%d failed status=%d after %u decoded/%d presented frames\n",
            ids[i],status,real_frames,frames);
        CHECK(status==0 && real_frames==(unsigned)counts[i] && frames==counts[i]);
        CHECK(now_ms==(uint64_t)counts[i]*delays[i] && !fd2_input_has_any_key(&vga->input));
        uint8_t first[VGA_W*VGA_H];memcpy(first,real_first,sizeof(first));
        reset(vga);real_frames=0;inject=inject_quit=1;
        CHECK(input_test_animation_play_hooked(vga,&archive,ids[i],delays[i],0,real_frame,vga)==-2);
        CHECK(real_frames==1 && frames==1 && !memcmp(first,vga->framebuffer,sizeof(first)));
        CHECK(fd2_input_has_any_key(&vga->input) && vga->input.quit_requested);
    }
    fd2_archive_close(&archive);return 0;
}

#define FD2_F4_READER_ANI
#include "f4_reader_probe.h"
#undef FD2_F4_READER_ANI

int main(int argc,char **argv) {
    if(argc>1 && !strcmp(argv[1],"--f4-readers"))return argc==3?f4_reader_cli(argv[2]):1;
    if(argc!=1)return 1;
    uint8_t blob[10+173+3*10] = {0};
    memcpy(blob,"LLLLLL",6);blob[6]=10;
    uint8_t *entry=blob+10;memcpy(entry,"AFM - Animation File Manager",28);
    word(entry+0xa5,3);word(entry+0xa7,320);word(entry+0xa9,200);word(entry+0xab,64000);
    for (unsigned i=0;i<3;i++) {
        uint8_t *part=entry+173+i*10;word(part,2);word(part+2,1);part[8]=4;part[9]=(uint8_t)(37+i);
    }
    fd2_archive archive={0};CHECK(fd2_archive_open_mem(&archive,blob,sizeof(blob))==0);
    fd2_vga vga;reset(&vga);
    fd2_input_set_test_key_state(&vga.input,SDL_SCANCODE_DOWN,1);
    fd2_input_begin_test_frame(&vga.input,0);
    uint64_t repeat=vga.input.next_repeat_ms[SDL_SCANCODE_DOWN];
    CHECK(input_test_animation_play(&vga,&archive,0,5,1)==0 && frames==3);
    CHECK(!foreign_presents && vga.frame_deadline_ns==UINT64_C(999999999999) &&
          vga.frame_interval_ns==16000000 && now_ms==15);
    CHECK(!fd2_input_has_any_key(&vga.input) && vga.input.previous_down[SDL_SCANCODE_DOWN] &&
          vga.input.next_repeat_ms[SDL_SCANCODE_DOWN]==repeat);
    CHECK(vga.framebuffer[0]==39);

    reset(&vga);inject=1;
    CHECK(input_test_animation_play(&vga,&archive,0,5,1)==1 && frames==1 && vga.framebuffer[0]==37);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&vga.input,&event) && event.key==FD2_INPUT_KEY_RIGHT);
    CHECK(fd2_input_take_key(&vga.input,&event) && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!fd2_input_take_key(&vga.input,&event));

    reset(&vga);inject=1;
    CHECK(input_test_animation_play(&vga,&archive,0,5,0)==0 && frames==3 && !fd2_input_has_any_key(&vga.input));
    reset(&vga);inject=inject_quit=1;
    CHECK(input_test_animation_play(&vga,&archive,0,5,1)==-2 && frames==1 && vga.input.quit_requested);
    CHECK(!foreign_presents && vga.frame_deadline_ns==UINT64_C(999999999999) &&
          vga.frame_interval_ns==16000000);
    CHECK(fd2_input_has_any_key(&vga.input));
    reset(&vga);quit_on_clear=1;memset(vga.framebuffer,0x88,sizeof(vga.framebuffer));
    CHECK(input_test_animation_play(&vga,&archive,0,5,1)==-2 && !frames && clears==1);
    for(size_t i=0;i<sizeof(vga.framebuffer);i++)CHECK(vga.framebuffer[i]==0x88);
    reset(&vga);quit_on_clear=2;
    CHECK(input_test_animation_play(&vga,&archive,0,5,0)==-2 && frames==1 && clears==2 && vga.framebuffer[0]==37);
    fd2_archive_close(&archive);
    CHECK(real_ani_checks(&vga)==0);
    puts("animation input: entry/continuation clear, skip peek and quit passed");
    return 0;
}
