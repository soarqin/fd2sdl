#ifndef FD2_FIELD_VISUAL_STATE_H
#define FD2_FIELD_VISUAL_STATE_H

#include <stdint.h>

/* field_animation_phase_update @0x1297d（原同名登记）及
 * field_view_render_tiles @0x11eee（原同名登记）的App级DWORD；不入SAV。
 * BIOS计时均为movsx WORD[46c]，各caller只推进自己调用的门，不补追时间。 */
typedef struct {
    uint32_t actor_idle_phase; /* 53C0B */
    uint32_t actor_idle_tick;  /* 53C0F：signed低WORD的DWORD位型 */
    uint32_t cursor_phase;    /* 53C07：每次1297d递增 */
    uint32_t terrain_phase;   /* 53A40：11eee只xor低BYTE */
    uint32_t terrain_tick;    /* 53A00 */
    uint32_t range_phase;     /* 53C1F */
    uint32_t range_tick;      /* 539F4 */
} fd2_field_visual_state;

/* now_ns经共享PIT时钟投影BIOS低WORD；返回1表示phase改变，0保持，-1拒绝非法相位。
 * actor步骤不触碰water/jitter/terrain；tiles步骤不触碰actor/water/jitter。
 * range_override=-1对应原51A93自动门，0..19对应有界固定phase。 */
int fd2_field_visual_actor_step(fd2_field_visual_state *state, uint64_t now_ns);
int fd2_field_visual_tiles_step(fd2_field_visual_state *state, uint64_t now_ns,
    int32_t range_override);

#endif
