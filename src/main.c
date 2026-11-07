/* 炎龙骑士团 2 SDL3 重写 - 主程序
 *
 * 复现 boot_intro_title_menu @0x1f894：片头 + 标题 + action 菜单同一个函数
 * （旧档案曾按 boot_intro_title / boot_intro_title_entry / title_action_menu
 * 三个名字分别登记；`push 0x88; call watcom_stack_check` 是它的序言，
 * body 在 +0xa，不是独立函数）
 *
 * 反编译依据见 docs/generated/ida-canonical/fd2-ida-decomp-all.c 和
 * docs/reverse-engineering/function-names.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <SDL3/SDL.h>

#include "archive.h"
#include "app_flow.h"
#include "bgm.h"
#include "image.h"
#include "input.h"
#include "field.h"
#include "field_play.h"
#include "field_prepare_play.h"
#include "field_preview.h"
#include "tile.h"
#include "title.h"
#include "scene.h"
#include "animation.h"
#include "vga.h"

#define WINDOW_W 960
#define WINDOW_H 600

_Static_assert((int)FD2_FIELD_PLAY_RETURN_COMPLETE == (int)FD2_APP_FIELD_COMPLETE &&
               (int)FD2_FIELD_PLAY_RETURN_LEAVE_GAME == (int)FD2_APP_FIELD_LEAVE_GAME &&
               (int)FD2_FIELD_PLAY_RETURN_HOST_QUIT == (int)FD2_APP_FIELD_HOST_QUIT &&
               (int)FD2_FIELD_PLAY_RETURN_ERROR == (int)FD2_APP_FIELD_ERROR &&
               (int)FD2_FIELD_PLAY_RETURN_VICTORY == (int)FD2_APP_FIELD_VICTORY &&
               (int)FD2_FIELD_PLAY_RETURN_DEFEAT == (int)FD2_APP_FIELD_DEFEAT,
               "field/app flow result mapping changed");

_Static_assert((int)FD2_FIELD_PREPARE_ERROR == (int)FD2_APP_PREPARE_ERROR &&
               (int)FD2_FIELD_PREPARE_DEPART == (int)FD2_APP_PREPARE_DEPART &&
               (int)FD2_FIELD_PREPARE_TITLE == (int)FD2_APP_PREPARE_TITLE &&
               (int)FD2_FIELD_PREPARE_HOST_QUIT == (int)FD2_APP_PREPARE_HOST_QUIT &&
               (int)FD2_FIELD_PREPARE_LEAVE_GAME == (int)FD2_APP_PREPARE_LEAVE_GAME,
               "prepare/app result ABI changed");

/* 独立战场CLI不带App准备owner；只报告结果，不冒充已执行跨关链。 */
static int field_result_status(fd2_field_play_result result, size_t stage) {
    switch (fd2_app_flow_from_field((fd2_app_field_result)result)) {
        case FD2_APP_FLOW_BATTLE_VICTORY:
            fprintf(stderr, "独立战场到达 victory（stage=%zu），未执行App跨关链。\n", stage);
            return 2;
        case FD2_APP_FLOW_BATTLE_DEFEAT:
            fprintf(stderr, "独立战场到达 defeat（stage=%zu），未执行App败北返回标题流程。\n", stage);
            return 2;
        case FD2_APP_FLOW_LEAVE_GAME:
        case FD2_APP_FLOW_EXIT:
            return 0;
        default:
            return -1;
    }
}

/* main @0x25bf4：战斗资源由play关闭后才把persistent交准备页；
 * 同一App context贯穿，START不重置运行期贡献、动态数字或随机流。 */
static fd2_app_flow run_field_chain(fd2_vga *vga, const fd2_archive *fdother,
        fd2_field_audio *audio, const char *save_path, fd2_field_play_context *context,
        fd2_app_flow flow, size_t stage, const fd2_field_handoff *handoff, int load_active) {
    while (flow == FD2_APP_FLOW_BATTLE || flow == FD2_APP_FLOW_PREPARE) {
        if (flow == FD2_APP_FLOW_PREPARE) {
            int result = fd2_field_prepare_run(vga, fdother, context, save_path, audio);
            flow = fd2_app_flow_from_prepare((fd2_app_prepare_result)result);
            if (flow != FD2_APP_FLOW_BATTLE) break;
            if (!context->has_preparation ||
                (context->preparation.stage_id != 1u && context->preparation.stage_id != 2u))
                return FD2_APP_FLOW_ERROR;
            stage = context->preparation.stage_id; handoff = NULL; load_active = 0;
        }
        if (stage != 0u && stage != 1u && stage != 2u) return FD2_APP_FLOW_ERROR;
        fd2_field_play_result result = fd2_field_play_run_context(vga, fdother,
            stage, 0, handoff, audio, save_path, load_active, context);
        flow = fd2_app_flow_after_battle((fd2_app_field_result)result,
            context->has_preparation ? context->preparation.stage_id : -1);
        handoff = NULL; load_active = 0;
    }
    return flow;
}

/* FDOTHER.DAT 句柄（对应反编译 &DAT_00001a4d 全局槽） */
static fd2_archive g_fdother;

/* ANI.DAT 句柄（FUN_0001db69 @0x20421 打开的 AFM 动画包） */
static fd2_archive g_ani;

/* title_action_menu @code0 0xf8d6..0xf8e6 加载 FDOTHER[77]，用于
 * 片头滚动、移动、确认和标题 action 入场。animation_play @code0
 * 0x1043f..0x1059a 另为 ANI[1] 加载 FDOTHER[78]，首帧前播放 SFX 0，
 * 结束时停止。primary／secondary AIL handle 仍相互独立。 */
typedef struct {
    fd2_pcm_player *primary_player;
    fd2_pcm_player *secondary_player;
    const fd2_pcm_bank *bank;
    const fd2_pcm_bank *letter_fly_bank;
} fd2_title_audio;

static int title_audio_play(fd2_title_audio *audio, fd2_title_sfx cue) {
    if (!audio || !audio->bank) return -1;
    fd2_title_sfx_handle handle;
    size_t sample_index;
    if (fd2_title_sfx_resolve(cue, &handle, &sample_index) != 0) return -1;
    fd2_pcm_player *player =
        handle == FD2_TITLE_SFX_HANDLE_PRIMARY
        ? audio->primary_player : audio->secondary_player;
    if (!player) return -1;
    /* 原版 primary／secondary sample handle 可同时存在；同一 handle 的
     * sfx_play 则先停止上一声再播放。 */
    return fd2_pcm_play_replace(player, audio->bank, sample_index, 1, FD2_GAME_SFX_DEFAULT_GAIN);
}

static void title_letter_fly_audio_on_frame(void *userdata,
                                             uint16_t frame_index) {
    fd2_title_audio *audio = userdata;
    if (frame_index != 0 || !audio || !audio->primary_player ||
        !audio->letter_fly_bank)
        return;
    /* animation_play @code0 0x1053b..0x10552：ANI[1] frame 0 字节码
     * 已解码、首个 delay 尚未开始时，以 primary handle 播放
     * FDOTHER[78] SFX 0。 */
    (void)fd2_pcm_play_replace(audio->primary_player,
                               audio->letter_fly_bank,
                               FD2_TITLE_LETTER_FLY_SFX_SAMPLE,
                               1, FD2_GAME_SFX_DEFAULT_GAIN);
}

static void title_letter_fly_audio_stop(fd2_title_audio *audio) {
    if (!audio || !audio->primary_player) return;
    /* animation_play @code0 0x10586..0x1059a：ANI[1] 正常结束或被按键
     * 跳过后，使用同一 primary handle 的 index -1 停止飞入声。 */
    (void)fd2_pcm_stop(audio->primary_player);
}

/* res_load @0x111ba：从 FDOTHER[idx] 加载 768 字节 6-bit RGB 调色板。 */
static const uint8_t *res_load_palette(size_t idx) {
    const uint8_t *palette;
    size_t len;
    if (fd2_archive_get(&g_fdother, idx, &palette, &len) != 0 || len < 768)
        return NULL;
    return palette;
}

/* 从 FDOTHER.DAT[idx] 加载图像并解码（标准 RLE，4字节 w/h 头） */
static int res_load_image(fd2_image *img, size_t idx) {
    const uint8_t *ptr; size_t len;
    if (fd2_archive_get(&g_fdother, idx, &ptr, &len) != 0) return -1;
    return fd2_image_decode_buf(img, ptr, len);
}

/* 从 FDOTHER.DAT[idx] 加载嵌套归档中的 sub[0] 图像 */
static int res_load_nested_image(fd2_image *img, size_t idx, size_t sub) {
    const uint8_t *ptr; size_t len;
    if (fd2_archive_get(&g_fdother, idx, &ptr, &len) != 0) return -1;
    fd2_archive sub_ar;
    if (fd2_archive_open_mem(&sub_ar, ptr, len) != 0) return -1;
    int r = fd2_image_decode_entry(img, &sub_ar, sub);
    fd2_archive_close(&sub_ar);
    return r;
}

/* 将图像 blit 到 VGA framebuffer（对应 FUN_0004c0d5 @0x4e98d）
 * blit_image_clipped(src, x, y, dest, stride, 0xffffffff)
 * 简化版：无透明色，直接拷贝 */
static void blit_image(fd2_vga *vga, const fd2_image *img, int x, int y) {
    for (int row = 0; row < img->height; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= VGA_H) continue;
        int copy_w = img->width;
        if (x + copy_w > VGA_W) copy_w = VGA_W - x;
        if (x < 0) { copy_w += x; }
        if (copy_w <= 0) continue;
        int src_x = (x < 0) ? -x : 0;
        memcpy(vga->framebuffer + dy * VGA_STRIDE + (x < 0 ? 0 : x),
               img->pixels + row * img->width + src_x,
               (size_t)copy_w);
    }
}

typedef struct {
    fd2_vga *vga;
    fd2_image *menu_items;
    int menu_item_count;
    const int *y_positions;
} fd2_title_draw_context;

static void draw_title_menu_item(void *userdata,
                                 int item_index,
                                 int image_index) {
    fd2_title_draw_context *context = userdata;
    if (!context || image_index < 0 ||
        image_index >= context->menu_item_count)
        return;
    blit_image(context->vga, &context->menu_items[image_index],
               129, context->y_positions[item_index]);
}

static void fade_in_light(fd2_vga *vga) {
    for (int b = 0x40; b >= 0; b--) {
        fd2_vga_set_brightness(vga, b);
        fd2_vga_present(vga);
        fd2_delay_ms(2);
    }
}

static void fade_out_dark(fd2_vga *vga) {
    for (int b = 0; b < 0x40; b++) {
        fd2_vga_set_brightness(vga, b);
        fd2_vga_present(vga);
        fd2_delay_ms(2);
    }
}

static void restore_scroll_frame(fd2_vga *vga, const uint8_t *offscreen,
                                 int scroll_total_h, int scroll_y) {
    int src_start = scroll_y * VGA_STRIDE;
    int copy_rows = (scroll_y + VGA_H <= scroll_total_h) ? VGA_H
                  : (scroll_total_h - scroll_y);
    if (copy_rows > 0) {
        memcpy(vga->framebuffer, offscreen + src_start,
               (size_t)copy_rows * VGA_STRIDE);
    }
    if (copy_rows < VGA_H) {
        memset(vga->framebuffer + copy_rows * VGA_STRIDE, 0,
               (size_t)(VGA_H - copy_rows) * VGA_STRIDE);
    }
}

static int play_intro_animation_with_palette(fd2_vga *vga,
                                             int anim_idx,
                                             uint32_t delay_ms,
                                             int pal_idx) {
    /* 复现 FUN_0001cf66 @0x1f81e：可选清屏/换调色板，
     * 播放 FUN_0001db69(animation_play)，随后调用 FUN_0001cfca 渐出。 */
    if (pal_idx >= 0) {
        fd2_vga_clear(vga, 0);
        const uint8_t *pal = res_load_palette((size_t)pal_idx);
        if (!pal) return -1;
        fd2_vga_set_palette(vga, pal);
    }
    fd2_vga_set_brightness(vga, 0);
    int result = fd2_animation_play(vga, &g_ani, anim_idx, delay_ms, 0);
    if (result < 0) return result;
    fade_out_dark(vga);
    return result;
}

/* 复现 boot_intro_title_menu @0x1f894：片头 + 标题 + action 菜单。
 * `push 0x88; call watcom_stack_check` 是该函数序言，body 在 +0xa。 */
static fd2_title_action boot_intro_title(fd2_vga *vga,
                                         fd2_bgm_player *bgm,
                                         fd2_title_audio *title_audio,
                                         int play_intro,
                                          int continue_available) {
    fd2_image menu_items[6] = {0};
    int n_menu = 0;
    fd2_title_action result = FD2_TITLE_ACTION_ERROR;
    /* main @0x25bf4 体内 0x25db1: music_track_play(18, 0) 紧接着调用
     * title_action_dispatch @0x25ebb，后者第一步进入 boot_intro_title_menu。
     * 片头和标题共用循环 track 18，标题入口不切曲。 */
    if (bgm && fd2_bgm_play(bgm, 18, 0) != 0)
        fprintf(stderr, "cannot play startup BGM track 18\n");
    if (!play_intro) goto title_screen;
    /* === 阶段1: 片头初始画面 === */
    /* 反编译精确顺序:
     *   1. vga_clear
     *   2. palette = [0x4c]
     *   3. FUN_0000f488(0,0xff,0x40)  暗度=0x40（DAC全黑）
     *   4. blit img[0x4a] (显存有内容, 但DAC=黑, 不可见)
     *   5. FUN_0001cc6d() = 渐入(0x40->0, 暗->亮) 画面出现!
     *   6. wait 1+30 ticks (画面停留 ~1.7s)
     *   7. FUN_0001cfca() = 渐出(0->0x40, 亮->暗) 画面消失!
     *   8. palette = [99] + vga_clear + brightness=0(完整亮度)
     *   9. FUN_0001db69(3,0x5a,1) 播放 ANI.DAT[3]
     *  10. FUN_0001cfca() = 渐出(0->0x40, 但画面是黑的, 不可见)
     *
     * 注意: FUN_0001cc6d(0x40->0) = 渐入(暗->亮)
     *       FUN_0001cfca(0->0x40) = 渐出(亮->暗)
     * 6-bit 亮度最大 0x3F, 但 scale 是 0x40 */
    fd2_vga_clear(vga, 0);
    const uint8_t *pal = res_load_palette(0x4c);
    if (!pal) goto resource_error;
    fd2_vga_set_palette(vga, pal);
    fd2_vga_set_brightness(vga, 0x40);  /* 暗度=0x40: DAC全黑 */

    fd2_image img_intro;
    if (res_load_image(&img_intro, 0x4a) == 0) {
        blit_image(vga, &img_intro, 0, 0);  /* blit (不可见, DAC=黑) */
        fd2_image_free(&img_intro);
    } else {
        goto resource_error;
    }

    /* FUN_0001cc6d(): 渐入 (brightness 0x40->0, 暗->亮, 65步x2ms) */
    fade_in_light(vga);

    /* FUN_000151f1(1) + FUN_000151f1(0x1e): 等 1+30 ticks (~1.7s) */
    fd2_wait_ticks(1);
    fd2_wait_ticks(0x1e);

    /* FUN_0001cfca(): 渐出 (brightness 0->0x40, 亮->暗, 64步x2ms) */
    fade_out_dark(vga);

    /* 切调色板[99] + 清屏 + brightness=0。
     * 原版此处为 FUN_0000f488(0,0xff,0)：DAC 完整亮度，
     * 画面仍为黑色是因为 framebuffer 已清零。 */
    pal = res_load_palette(99);
    if (!pal) goto resource_error;
    fd2_vga_set_palette(vga, pal);
    fd2_vga_clear(vga, 0);
    fd2_vga_set_brightness(vga, 0);
    fd2_vga_present(vga);  /* 黑屏 */

    /* FUN_0001db69(3,0x5a,1)：播放 ANI.DAT[3]，这是首屏后的第二段片头。 */
    int animation_result = fd2_animation_play(
        vga, &g_ani, 3, FD2_TITLE_ANIM_DELAY_SLOW_MS, 1);
    if (animation_result == -2) goto host_quit;
    if (animation_result < 0) goto resource_error;
    /* 动画只采样当前键态，不保留 skip 事件；若物理键仍按住，后续
     * 交互还要等待 typematic 初始 deadline，不能立即确认标题菜单。 */

    /* FUN_0001cfca(): 渐出 (brightness 0->0x40) */
    fade_out_dark(vga);

    /* === 阶段3: 5帧片头滚动入场 === */
    /* 反编译:
     *   16. FUN_00035058(0xa0000,0,pattern)  vga_clear
     *   17. DAT_00003a65 = res_load(0x65)   palette=[0x65]
     *   18. FUN_0000f488(0,0xff,0x40)        暗度=0x40（全黑）
     *   19. alloc offscreen
     *   20. clear offscreen
     *   21. for 5 frames: load + blit to offscreen */
    fd2_vga_clear(vga, 0);                      /* 步骤16 */
    pal = res_load_palette(0x65);
    if (!pal) goto resource_error;
    fd2_vga_set_palette(vga, pal);             /* 步骤17 */
    fd2_vga_set_brightness(vga, 0x40);           /* 步骤18 */

    /* boot_intro_title_menu @0x1f894：malloc(0x396c0) 分配 320×735 滚动缓冲，
     * 5 帧各 320×147，按 y=0x93*i 拼接；另一次 malloc(0xa0) 不属于此缓冲。 */
    int scroll_total_h = 147 * 5;  /* 0x93 * 5 = 735 */
    uint8_t *offscreen = calloc(VGA_W * scroll_total_h, 1);
    if (!offscreen) goto resource_error;
    {
        for (int i = 0; i < 5; i++) {
            fd2_image frame = {0};
            if (res_load_image(&frame, 0x45 + i) != 0 ||
                fd2_title_copy_scroll_frame(offscreen + i * 0x93 * VGA_W, &frame) != 0) {
                fd2_image_free(&frame);
                free(offscreen);
                goto resource_error;
            }
            fd2_image_free(&frame);
        }

        /* 滚动循环: iVar6 from 0x217(535) down to 0
         * 每帧: memcpy(0xa0000, offscreen + iVar6*0x140, 200行)
         * 即从 offscreen 的 y=iVar6 位置拷贝 200 行到显存 */
        fd2_title_lightning_state lightning;
        fd2_title_lightning_state_init(&lightning);
        int lightning_palette_active = 0;
        for (int scroll_y = 0x217; scroll_y >= 0; scroll_y--) {
            /* 拷贝 offscreen[scroll_y..scroll_y+199] 到 framebuffer */
            int src_start = scroll_y * VGA_STRIDE;
            int copy_rows = (scroll_y + VGA_H <= scroll_total_h) ? VGA_H
                         : (scroll_total_h - scroll_y);
            if (copy_rows > 0) {
                memcpy(vga->framebuffer,
                       offscreen + src_start,
                       (size_t)copy_rows * VGA_STRIDE);
            }
            if (copy_rows < VGA_H) {
                memset(vga->framebuffer + copy_rows * VGA_STRIDE, 0,
                       (size_t)(VGA_H - copy_rows) * VGA_STRIDE);
            }

            /* 原版在首个滚动帧 iVar6==0x217 后调用 FUN_0001cc6d，
             * 将 DAC 暗度从 0x40 渐到 0，滚动画面由黑渐入。 */
            if (scroll_y == 0x217) {
                fade_in_light(vga);
            }

            /* 调色板/动画切换（对应 FUN_0001cf66 各分支）。
             * 原版不是单纯换调色板，而是在滚动中插入 ANI.DAT 动画：
             *   0x14a: #4(90ms,pal99) -> #5(50ms,pal0)
             *   0x0d2: #6(90ms,pal99) -> #7(50ms,pal0)
             *   0x06e: #8(90ms,pal99)
             *   0x019: #0(15ms,pal0)
             * 每段动画结束后恢复滚动缓冲、重载 pal[0x65] 并淡入。 */
            if (scroll_y == 0x14a || scroll_y == 0xd2 ||
                scroll_y == 0x6e || scroll_y == 0x19) {
                if (scroll_y != 0x19) fade_out_dark(vga);

                if (scroll_y == 0x14a) {
                    animation_result = play_intro_animation_with_palette(
                        vga, 4, FD2_TITLE_ANIM_DELAY_SLOW_MS, 99);
                    if (animation_result >= 0)
                        animation_result = play_intro_animation_with_palette(
                            vga, 5, FD2_TITLE_ANIM_DELAY_MEDIUM_MS, 0);
                } else if (scroll_y == 0xd2) {
                    animation_result = play_intro_animation_with_palette(
                        vga, 6, FD2_TITLE_ANIM_DELAY_SLOW_MS, 99);
                    if (animation_result >= 0)
                        animation_result = play_intro_animation_with_palette(
                            vga, 7, FD2_TITLE_ANIM_DELAY_MEDIUM_MS, 0);
                } else if (scroll_y == 0x6e) {
                    animation_result = play_intro_animation_with_palette(
                        vga, 8, FD2_TITLE_ANIM_DELAY_SLOW_MS, 99);
                } else { /* scroll_y == 0x19 */
                    animation_result = play_intro_animation_with_palette(
                        vga, 0, FD2_TITLE_ANIM_DELAY_FAST_MS, 0);
                }
                if (animation_result < 0) {
                    free(offscreen);
                    if (animation_result == -2) goto host_quit;
                    goto resource_error;
                }

                restore_scroll_frame(vga, offscreen, scroll_total_h, scroll_y);
                const uint8_t *rp = res_load_palette(0x65);
                if (!rp) { free(offscreen); goto resource_error; }
                fd2_vga_set_palette(vga, rp);
                fade_in_light(vga);
            }

            /* 切入动画（对应 FUN_0001ce87）
             * FUN_0001ce87(img_idx, pal_idx, buffer, scroll_y):
             *   渐出黑 -> 清屏 -> 加载调色板[pal] -> blit图[img] -> 渐入
             *   等 1+6 tick -> 渐出黑 -> 恢复滚动调色板 -> 恢复滚动画面 -> 渐入
             * iVar6==0x1c2(450): img=[100], pal=[99]  (最后切入，伪3D转场)
             * iVar6==10:         img=[75],  pal=[76]  (战斗画面切入) */
            if (scroll_y == 0x1c2 || scroll_y == 10) {
                int img_idx = (scroll_y == 0x1c2) ? 100 : 0x4b;
                int pal_idx = (scroll_y == 0x1c2) ? 99 : 0x4c;
                /* 渐出到黑 */
                fd2_vga_palette_fade_to(vga, 0, 0, 0, 0, 0);
                fd2_vga_present(vga);
                /* 清屏 + 加载切入调色板和图像 */
                fd2_vga_clear(vga, 0);
                const uint8_t *cp = res_load_palette(pal_idx);
                if (!cp) { free(offscreen); goto resource_error; }
                fd2_vga_set_palette(vga, cp);
                fd2_image cut_img;
                if (res_load_image(&cut_img, img_idx) == 0) {
                    blit_image(vga, &cut_img, 0, 0);
                    fd2_image_free(&cut_img);
                } else {
                    free(offscreen);
                    goto resource_error;
                }
                /* 渐入 */
                for (int s = 0; s <= 0x40; s += 4) {
                    fd2_vga_palette_fade_to(vga, 0, s, 0, 0, 0);
                    fd2_vga_present(vga);
                    fd2_delay_ms(2);
                }
                fd2_vga_set_palette(vga, cp);
                fd2_vga_present(vga);
                /* 等 1+6 tick (FUN_000151f1) */
                fd2_wait_ticks(1);
                fd2_wait_ticks(6);
                /* 渐出到黑 */
                fd2_vga_palette_fade_to(vga, 0, 0, 0, 0, 0);
                fd2_vga_present(vga);
                /* 恢复滚动调色板[0x65]和滚动画面 */
                const uint8_t *rp = res_load_palette(0x65);
                if (!rp) { free(offscreen); goto resource_error; }
                fd2_vga_set_palette(vga, rp);
                int src_start2 = scroll_y * VGA_STRIDE;
                int copy_rows2 = (scroll_y + VGA_H <= scroll_total_h) ? VGA_H
                             : (scroll_total_h - scroll_y);
                if (copy_rows2 > 0)
                    memcpy(vga->framebuffer, offscreen + src_start2,
                           (size_t)copy_rows2 * VGA_STRIDE);
                /* 渐入回亮 */
                for (int s = 0; s <= 0x40; s += 4) {
                    fd2_vga_palette_fade_to(vga, 0, s, 0, 0, 0);
                    fd2_vga_present(vga);
                    fd2_delay_ms(2);
                }
                fd2_vga_set_palette(vga, rp);
            }

            /* title_action_menu @code0 0xfbb7..0xfbd6：所有当前滚动位置
             * 对应的 ANI／cutaway 处理结束后，再比较 object2 DS:0x204e
             * 阈值并以 primary handle 重启 FDOTHER[77] SFX 0。该顺序在
             * scroll_y==110 时尤其重要：原版先播放 ANI[8]，恢复滚动画面
             * 后才重新开始飞行声。 */
            if (fd2_title_flight_sfx_for_scroll_y(scroll_y))
                (void)title_audio_play(title_audio, FD2_TITLE_SFX_FLIGHT);

            /* code0 0xfbd9..0xfc37：全部 14 个有效滚动音效阈值开始闪电
             * 状态，local_4c 从 0 逐帧递增至 11 才恢复 FDOTHER[101]。
             * 因此每次闪光完整持续 11 个滚动帧（约 330 ms），而不是
             * 只显示触发的一个 30 ms 帧。 */
            int lightning_active =
                fd2_title_lightning_state_advance(&lightning, scroll_y);
            if (lightning_active != lightning_palette_active) {
                const uint8_t *scroll_pal = res_load_palette(
                    lightning_active ? 0x66u : 0x65u);
                if (!scroll_pal) { free(offscreen); goto resource_error; }
                fd2_vga_set_palette(vga, scroll_pal);
                lightning_palette_active = lightning_active;
            }

            fd2_vga_present(vga);
            fd2_delay_ms(FD2_TITLE_SCROLL_DELAY_MS); /* 原版 delay_ms(30) */

            /* iVar6==0 时额外等待 1000ms (反编译 L135) */
            if (scroll_y == 0) fd2_delay_ms(1000);

            /* 在原版查询点采样当前键态；宿主退出不能降级为标题跳过。 */
            if (fd2_input_take_quit(&vga->input)) {
                free(offscreen);
                goto host_quit;
            }
            if (fd2_input_observe_any_key(&vga->input)) {
                free(offscreen);
                goto title_screen;
            }
        }
        free(offscreen);
    }

    /* 循环结束后按原版 FUN_0002b649(0,0xff,step,0x3f,0,0)
     * 从当前调色板渐变到红色基色。 */
    for (int step = 0x28; step >= 0; step--) {
        fd2_vga_palette_fade_to(vga, 0, step, 0x3f, 0, 0);
        fd2_vga_present(vga);
        fd2_delay_ms(8);
    }
    fd2_delay_ms(100);

title_screen:
    /* === 阶段4: 标题画面 === */
    /* 反编译精确流程:
     *   uVar9 = res_load(7);                       // 标题图（嵌套.DAT）
     *   DAT_00003a65 = res_load(8);                // 调色板[8]
     *   FUN_00035058(0xa0000,0,&pattern);          // 清屏
     *   FUN_0000f488(0,0xff,0);                     // 完整亮度
     *   FUN_0001db69(1,0xf,1);                     // 标题前 ANI.DAT[1]
     *   FUN_0002328d(local_18,3,1);
     *   FUN_0000f53a(0,0xff,0x40);                  // 标题图先置为黑
     *   FUN_00013fce(0xa0000,0x140,title,0);        // blit标题图
     *   for(i=0;i<0x29;i++) FUN_0002b649(0,0xff,i,0x38,0x3c,0x3f);  // 从基色渐变
     *
     * 注意: FUN_0002b649 用的是插值渐变(不是brightness)
     * 基色(0x38,0x3c,0x3f)接近全亮, 0x29步渐变到完整调色板 */
    fd2_vga_clear(vga, 0);
    pal = res_load_palette(8);
    if (pal) {
        fd2_vga_set_palette(vga, pal);
        fd2_vga_set_brightness(vga, 0);  /* FUN_0000f488(...,0)：完整亮度 */

        /* FUN_0001db69(1,0xf,1)：标题上方文字飞入动画。
         * animation_play 在 frame 0 解码后、首个 15 ms delay 前播放
         * FDOTHER[78] SFX 0，并在动画结束／跳过时停止。AFM 帧内会临时
         * 写动画调色板，动画结束后恢复标题调色板 [8]。 */
        animation_result = fd2_animation_play_hooked(
            vga, &g_ani, 1, FD2_TITLE_ANIM_DELAY_FAST_MS, 1,
            title_letter_fly_audio_on_frame, title_audio);
        title_letter_fly_audio_stop(title_audio);
        if (animation_result == -2) goto host_quit;
        if (animation_result < 0) goto resource_error;
        /* skip 不作为预输入保存；持续按住受 typematic deadline 约束。 */
        /* title_action_menu @code0 0xfd1c：标题 ANI 结束后，以 secondary
         * sample handle 播放同一 FDOTHER[77] 的 SFX 3，再开始渐变。 */
        (void)title_audio_play(title_audio, FD2_TITLE_SFX_ENTER);
        fd2_vga_set_palette(vga, pal);

        /* FUN_0000f53a(0,0xff,0x40)：标题图绘制前先把 DAC 置暗。 */
        fd2_vga_set_brightness(vga, 0x40);

        /* 加载标题图 [7] -> sub[0] */
        fd2_image img_title;
        if (res_load_nested_image(&img_title, 7, 0) == 0) {
            blit_image(vga, &img_title, 0, 0);  /* blit (不可见, DAC=黑) */

            /* FUN_0002b649: 从基色(0x38,0x3c,0x3f)渐变到完整调色板, 0x29步
             * dac = (pal - base) * step / 0x28 + base
             * step=0: 全部=base(接近全亮)
             * step=0x28: 全部=pal(完整) */
            for (int step = 0; step < 0x29; step++) {
                fd2_vga_palette_fade_to(vga, 0, step, 0x38, 0x3c, 0x3f);
                fd2_vga_present(vga);
                fd2_delay_ms(8);
            }
            /* 确保完整调色板 */
            fd2_vga_set_palette(vga, pal);
            fd2_vga_present(vga);
            fd2_image_free(&img_title);
        } else {
            goto resource_error;
        }
    } else {
        goto resource_error;
    }

    /* 标题菜单（对应 FUN_0001d6c1 + 菜单循环）
     * 菜单项位置: y=164/173/182, x=129。
     * 原版比较 INT 16h/AH=10h 的 Up/Down 扫描码；确认接受
     * Enter、Space 与数字小键盘 0。详见 docs/systems/input.md。
     * 菜单项图: FDOTHER[7] sub[1-6] */
    int selection = 0;
    /* 原版 title @code0 0xfd81..0xfe0e 先验证完整 FD2.SAV checksum 与
     * active meta stage，再把菜单项数从 2 扩为 3。 */
    int menu_count = continue_available ? 3 : 2;
    /* 加载菜单项图 */
    const uint8_t *buf7b; size_t len7b;
    if (fd2_archive_get(&g_fdother, 7, &buf7b, &len7b) != 0)
        goto resource_error;
    fd2_archive ar7b;
    if (fd2_archive_open_mem(&ar7b, buf7b, len7b) != 0)
        goto resource_error;
    for (int i = 1; i < 7; i++) {
        if (fd2_image_decode_entry(&menu_items[n_menu], &ar7b, i) != 0) {
            fd2_archive_close(&ar7b);
            goto resource_error;
        }
        n_menu++;
    }
    fd2_archive_close(&ar7b);

    /* 菜单只在交互循环中按需读取当前键态；动画期间按下又松开的键
     * 不会形成预输入。若仍保持按下，则受同一物理键的 typematic
     * deadline 约束，不会在状态切换时立即再次触发。 */

    const int y_pos[3] = {164, 173, 182};
    fd2_title_draw_context title_draw = {
        .vga = vga,
        .menu_items = menu_items,
        .menu_item_count = n_menu,
        .y_positions = y_pos,
    };

    int menu_done = 0;
    while (!menu_done) {
        (void)fd2_title_draw_menu_frame(menu_count, selection, 1,
                                        draw_title_menu_item, &title_draw);
        fd2_vga_present(vga);

        if (fd2_input_take_quit(&vga->input)) {
            selection = FD2_TITLE_ACTION_HOST_QUIT;
            break;
        }
        fd2_input_action action;
        while (!menu_done &&
               fd2_input_take_action(&vga->input, FD2_INPUT_CONTEXT_TITLE,
                                     &action)) {
            switch (action) {
                case FD2_INPUT_ACTION_UP:
                    (void)title_audio_play(title_audio, FD2_TITLE_SFX_MOVE);
                    selection = selection > 0 ? selection - 1 : menu_count - 1;
                    break;
                case FD2_INPUT_ACTION_DOWN:
                    (void)title_audio_play(title_audio, FD2_TITLE_SFX_MOVE);
                    selection = selection + 1 < menu_count ? selection + 1 : 0;
                    break;
                case FD2_INPUT_ACTION_CONFIRM:
                    (void)title_audio_play(title_audio, FD2_TITLE_SFX_CONFIRM);
                    menu_done = 1;
                    break;
                default:
                    break;
            }
        }
        fd2_delay_ms(50);
    }

    /* title_action_menu @code0 0xfef0..0xff30：选中项 normal 与
     * highlight 各显示 80 ms，交替 4 轮。旧实现只重复 present 同一
     * highlight framebuffer，实际没有闪动。宿主退出不播放确认动画。 */
    if (selection != FD2_TITLE_ACTION_HOST_QUIT) {
        for (int frame = 0; frame < FD2_TITLE_CONFIRM_FLASH_FRAMES; frame++) {
            int highlight = fd2_title_confirm_highlight_for_frame(frame);
            (void)fd2_title_draw_menu_frame(menu_count, selection, highlight,
                                            draw_title_menu_item, &title_draw);
            fd2_vga_present(vga);
            fd2_delay_ms(FD2_TITLE_CONFIRM_FLASH_DELAY_MS);
            if (fd2_input_take_quit(&vga->input)) {
                selection = FD2_TITLE_ACTION_HOST_QUIT;
                break;
            }
        }
    }

    result = fd2_title_action_from_selection(selection, continue_available);
    goto done;

resource_error:
    fprintf(stderr, "cannot load title resources\n");
    goto done;
host_quit:
    result = FD2_TITLE_ACTION_HOST_QUIT;
done:
    for (int i = 0; i < n_menu; i++) fd2_image_free(&menu_items[i]);
    return result;
}

/* 阶段 2 地图预览：读取 FDFIELD.DAT[stage*3] + FDSHAP.DAT 成对地形资源，
 * 用 FDOTHER.DAT[0] 调色板绘制战场底图。
 *
 * 逆向依据：FUN_00010580 @0x12e38 从 cell 读 terrain_id 与地形表属性；
 * field_view_render_tiles @0x11eee 按 terrain_id 取 FDSHAP 24×24 帧组成底图；
 * FUN_0001020e @0x12ac6 对 flags 0x80 的遮挡格重绘后一帧。 */
static int run_map_preview(fd2_vga *vga, int once, size_t stage) {
    fd2_archive fdshap = {0};
    fd2_archive field = {0};
    fd2_terrain_tileset terrain = {0};
    fd2_field_map map = {0};
    int result = -1;

    if (fd2_archive_open(&fdshap, "original_game/FDSHAP.DAT") != 0) {
        fprintf(stderr, "cannot open FDSHAP.DAT\n");
        goto done;
    }
    if (fd2_archive_open(&field, "original_game/FDFIELD.DAT") != 0) {
        fprintf(stderr, "cannot open FDFIELD.DAT\n");
        goto done;
    }
    if (fd2_field_map_open_stage(&map, &field, stage) != 0) {
        fprintf(stderr, "cannot decode FDFIELD.DAT stage %zu\n", stage);
        goto done;
    }
    if (fd2_terrain_tileset_open_stage(&terrain, &fdshap, &field, stage) != 0) {
        fprintf(stderr, "cannot decode FDSHAP.DAT terrain sheet for stage %zu\n", stage);
        goto done;
    }

    const uint8_t *pal = res_load_palette(0); /* 战场/地形调色板 */
    if (pal) fd2_vga_set_palette(vga, pal);
    fd2_vga_set_brightness(vga, 0);

    int camera_x = 0;
    int camera_y = 0;
    int anim_phase = 0;
    int running = 1;
    printf("map preview: stage=%zu, %dx%d cells, FDSHAP terrain #%zu, %zu frames, %zu attrs\n",
           stage, map.width, map.height, terrain.shape_index,
           terrain.sheet.frame_count, terrain.attr_count);

    while (running) {
        fd2_terrain_render_field_preview(vga, &terrain, &map,
                                         camera_x, camera_y, anim_phase);
        fd2_vga_present(vga);
        if (once) break;

        if (fd2_input_take_quit(&vga->input)) running = 0;
        fd2_input_action action;
        while (running &&
               fd2_input_take_action(&vga->input, FD2_INPUT_CONTEXT_PREVIEW,
                                     &action)) {
            switch (action) {
                case FD2_INPUT_ACTION_EXIT:
                    running = 0;
                    break;
                case FD2_INPUT_ACTION_LEFT:
                    if (camera_x >= 24) camera_x -= 24;
                    break;
                case FD2_INPUT_ACTION_RIGHT:
                    camera_x += 24;
                    break;
                case FD2_INPUT_ACTION_UP:
                    if (camera_y >= 24) camera_y -= 24;
                    break;
                case FD2_INPUT_ACTION_DOWN:
                    camera_y += 24;
                    break;
                default:
                    break;
            }
        }
        fd2_delay_ms(33);
    }
    result = 0;

done:
    fd2_field_map_close(&map);
    fd2_terrain_tileset_close(&terrain);
    fd2_archive_close(&field);
    fd2_archive_close(&fdshap);
    return result;
}

/* 宿主启动前置，不新增资源owner；正常加载仍负责条目与业务语义检查。 */
enum {
    STARTUP_TITLE = 1u,
    STARTUP_MAP = 2u,
    STARTUP_FIELD = 4u,
    STARTUP_PROLOGUE = 8u,
};

static int check_startup_resources(unsigned mode, const char *directory,
                                   FILE *errors) {
    static const struct { const char *name; unsigned modes; int sprite; } files[] = {
        {"FDOTHER.DAT", 15u, 0},
        {"ANI.DAT", STARTUP_TITLE, 0},
        {"FDFIELD.DAT", 15u, 0},
        {"FDSHAP.DAT", 15u, 0},
        {"DATO.DAT", STARTUP_TITLE | STARTUP_FIELD | STARTUP_PROLOGUE, 0},
        {"FDTXT.DAT", STARTUP_TITLE | STARTUP_FIELD | STARTUP_PROLOGUE, 0},
        {"FDICON.B24", STARTUP_TITLE | STARTUP_FIELD | STARTUP_PROLOGUE, 1},
        {"FIGANI.DAT", STARTUP_TITLE | STARTUP_FIELD, 0},
    };
    if (!directory || !errors || !mode || (mode & ~15u)) return -1;
    int failed = 0;
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        if (!(files[i].modes & mode)) continue;
        char path[1024];
        int written = snprintf(path, sizeof(path), "%s/%s", directory, files[i].name);
        int opened = -1, error = ENAMETOOLONG;
        if (written >= 0 && (size_t)written < sizeof(path)) {
            /* FDICON旧接口未对每种格式错误设置errno，不能继承上一文件的ENOENT。 */
            errno = 0;
            if (files[i].sprite) {
                fd2_map_sprite_bank bank = {0};
                opened = fd2_map_sprite_bank_open(&bank, path);
                error = errno;
                fd2_map_sprite_bank_close(&bank);
            } else {
                fd2_archive archive = {0};
                opened = fd2_archive_open(&archive, path);
                error = errno;
                fd2_archive_close(&archive);
            }
        }
        if (opened == 0) continue;
        if (!failed) fputs("无法启动：原版数据检查失败。\n", errors);
        failed = 1;
        if (error == ENOENT || error == ENOTDIR)
            fprintf(errors, "  缺少文件：%s/%s\n", directory, files[i].name);
        else if (error == EINVAL)
            fprintf(errors, "  格式损坏：%s/%s\n", directory, files[i].name);
        else
            fprintf(errors, "  无法读取或格式损坏：%s/%s\n", directory, files[i].name);
    }
    if (failed) {
        fprintf(errors, "请将合法取得的原版游戏数据放入 %s/，并从其上级目录启动。\n",
                directory);
        fputs("不需要 FD2.EXE；程序不会下载或修改原版数据。\n", errors);
    }
    return failed ? -1 : 0;
}

int main(int argc, char **argv) {
    int map_preview = (argc > 1 && strcmp(argv[1], "--map-preview") == 0);
    int map_preview_once = (argc > 1 && strcmp(argv[1], "--map-preview-once") == 0);
    int prologue_preview = (argc > 1 && strcmp(argv[1], "--prologue-preview") == 0);
    int prologue_preview_once = (argc > 1 && strcmp(argv[1], "--prologue-preview-once") == 0);
    int field_preview = (argc > 1 && strcmp(argv[1], "--field-preview") == 0);
    int field_preview_once = (argc > 1 && strcmp(argv[1], "--field-preview-once") == 0);
    int field_play = (argc > 1 && strcmp(argv[1], "--field-play") == 0);
    int field_play_once = (argc > 1 && strcmp(argv[1], "--field-play-once") == 0);
    int field_effect_play =
        (argc > 1 && strcmp(argv[1], "--field-effect-play") == 0);
    int new_game_play = (argc > 1 && strcmp(argv[1], "--new-game-play") == 0);
    int new_game_play_once =
        (argc > 1 && strcmp(argv[1], "--new-game-play-once") == 0);
    size_t preview_stage = (argc > 2) ? (size_t)strtoul(argv[2], NULL, 0) : 0;
    const char *save_path = getenv("FD2SDL_SAVE_PATH");
    if (!save_path || save_path[0] == '\0')
        save_path = "FD2.SAV";

    unsigned startup_mode = map_preview || map_preview_once ? STARTUP_MAP
        : field_preview || field_preview_once || field_effect_play ||
          field_play || field_play_once || new_game_play || new_game_play_once
            ? STARTUP_FIELD
        : prologue_preview || prologue_preview_once ? STARTUP_PROLOGUE
        : STARTUP_TITLE;
    if (check_startup_resources(startup_mode, "original_game", stderr) != 0)
        return 1;

    fd2_input_install_interrupt_handlers();
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("FD2-SDL", WINDOW_W, WINDOW_H, 0);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, NULL);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    /* 游戏自身按原版毫秒/tick 时序调度；显式关闭 backend 隐式 vsync，
     * 避免 RenderPresent 阻塞与固定 delay 在不同刷新率下重复叠加。 */
    SDL_SetRenderVSync(ren, 0);
    SDL_SetRenderLogicalPresentation(ren, VGA_W, VGA_H,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    /* 打开 FDOTHER.DAT（对应 &DAT_00001a4d 全局槽初始化） */
    if (fd2_archive_open(&g_fdother, "original_game/FDOTHER.DAT") != 0) {
        fprintf(stderr, "cannot open FDOTHER.DAT\n");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    printf("FDOTHER.DAT: %zu entries\n", g_fdother.count);

    /* 初始化虚拟 VGA */
    fd2_vga vga;
    if (fd2_vga_init(&vga, win, ren) != 0) {
        fprintf(stderr, "VGA init failed\n");
        fd2_archive_close(&g_fdother);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    fd2_audio *audio = NULL;
    fd2_bgm_player *bgm = NULL;
    fd2_pcm_bank ui_sfx_bank = {0};
    fd2_pcm_bank title_sfx_bank = {0};
    fd2_pcm_bank title_letter_fly_sfx_bank = {0};
    fd2_pcm_bank battle_sfx_bank = {0};
    fd2_pcm_bank arrival_sfx_bank = {0};
    fd2_pcm_player pcm_player = {0};
    fd2_pcm_player secondary_pcm_player = {0};
    fd2_field_audio field_audio = {0};
    fd2_title_audio title_audio = {0};
    fd2_field_audio *field_audio_ptr = NULL;
    fd2_title_audio *title_audio_ptr = NULL;
    (void)SDL_InitSubSystem(SDL_INIT_AUDIO);
    fd2_audio_config audio_config = {
        .sample_rate = 48000,
        /* 即使 subsystem 初始化失败也尝试打开；失败路径由 audio 层明确
         * 降级为 discard null backend，而不是无人消费的 offline backend。 */
        .open_device = 1,
        .allow_null = 1,
    };
    audio = fd2_audio_create(&audio_config);
    if (audio) {
        fd2_bgm_config bgm_config = {
            .fdmus_path = "original_game/FDMUS.DAT",
            .ail_bank_path = "original_game/SAMPLE.AD",
            .sample_rate = 48000,
        };
        bgm = fd2_bgm_create(audio, &bgm_config);
        if (!bgm)
            fprintf(stderr, "BGM: cannot open FDMUS.DAT/SAMPLE.AD\n");
    }
    if (audio && fd2_pcm_bank_open(&ui_sfx_bank, &g_fdother, 31) == 0 &&
        fd2_pcm_bank_open(&title_sfx_bank, &g_fdother,
                          FD2_TITLE_SFX_BANK) == 0 &&
        fd2_pcm_bank_open(&title_letter_fly_sfx_bank, &g_fdother,
                          FD2_TITLE_LETTER_FLY_SFX_BANK) == 0 &&
        fd2_pcm_bank_open(&battle_sfx_bank, &g_fdother, 80) == 0 &&
        fd2_pcm_bank_open(&arrival_sfx_bank, &g_fdother,
                          FD2_FIELD_SFX_BANK_ARRIVAL) == 0 &&
        fd2_pcm_player_init(&pcm_player, audio, 11025) == 0 &&
        fd2_pcm_player_init(&secondary_pcm_player, audio, 11025) == 0) {
        fd2_field_audio_init(&field_audio, &pcm_player,
                             &ui_sfx_bank, &battle_sfx_bank,
                             &arrival_sfx_bank);
        title_audio.primary_player = &pcm_player;
        field_audio.secondary_player = &secondary_pcm_player;
        title_audio.secondary_player = &secondary_pcm_player;
        title_audio.bank = &title_sfx_bank;
        title_audio.letter_fly_bank = &title_letter_fly_sfx_bank;
        field_audio_ptr = &field_audio;
        title_audio_ptr = &title_audio;
        printf("audio: %s, 48000 Hz, FDOTHER[31]=%zu, [77]=%zu, [78]=%zu, [80]=%zu, [95]=%zu samples\n",
               fd2_audio_has_device(audio) ? "SDL device" : "null backend",
               fd2_pcm_bank_count(&ui_sfx_bank),
               fd2_pcm_bank_count(&title_sfx_bank),
               fd2_pcm_bank_count(&title_letter_fly_sfx_bank),
               fd2_pcm_bank_count(&battle_sfx_bank),
               fd2_pcm_bank_count(&arrival_sfx_bank));
    } else {
        fd2_pcm_bank_close(&arrival_sfx_bank);
        fd2_pcm_bank_close(&battle_sfx_bank);
        fd2_pcm_bank_close(&title_letter_fly_sfx_bank);
        fd2_pcm_bank_close(&title_sfx_bank);
        fd2_pcm_bank_close(&ui_sfx_bank);
        fprintf(stderr, "audio: unavailable; continuing without sound\n");
    }

    fd2_field_play_context app_context;
    fd2_field_play_context_init(&app_context, bgm);
    int rc = 0;
    if (map_preview || map_preview_once) {
        rc = run_map_preview(&vga, map_preview_once, preview_stage);
    } else if (field_preview || field_preview_once) {
        rc = fd2_field_preview_run(&vga, &g_fdother, preview_stage,
                                   field_preview_once);
    } else if (field_effect_play) {
        rc = fd2_field_effect_play_run(
            &vga, &g_fdother, preview_stage, field_audio_ptr);
    } else if (field_play || field_play_once) {
        fd2_field_play_result play_result = fd2_field_play_run(
            &vga, &g_fdother, preview_stage, field_play_once, NULL,
            field_audio_ptr, save_path, 0);
        rc = field_result_status(play_result, preview_stage);
    } else if (new_game_play || new_game_play_once) {
        fd2_field_handoff handoff;
        rc = fd2_scene_play_new_game_prologue_handoff(
            &vga, &g_fdother, bgm, field_audio_ptr,
            new_game_play_once, &handoff);
        if (rc == FD2_SCENE_RESULT_HOST_QUIT)
            rc = 0;
        else if (rc == 0) {
            if (new_game_play_once) {
                rc = field_result_status(fd2_field_play_run(&vga, &g_fdother, 0,
                    1, &handoff, field_audio_ptr, save_path, 0), 0);
            } else {
                fd2_app_flow flow = run_field_chain(&vga, &g_fdother, field_audio_ptr,
                    save_path, &app_context, FD2_APP_FLOW_BATTLE, 0, &handoff, 0);
                /* 原自然START链若败北，回实际标题；CLI不能把返回标题当作退出。 */
                if (flow == FD2_APP_FLOW_TITLE) goto application_title;
                rc = flow == FD2_APP_FLOW_EXIT || flow == FD2_APP_FLOW_LEAVE_GAME ? 0 : -1;
            }
        }
    } else if (prologue_preview || prologue_preview_once) {
        rc = fd2_scene_play_new_game_prologue(
            &vga, &g_fdother, bgm, field_audio_ptr,
            prologue_preview_once);
        if (rc == FD2_SCENE_RESULT_HOST_QUIT) rc = 0;
    } else {
application_title:
        /* 打开 ANI.DAT（对应 FUN_0001db69 @0x20421） */
        if (fd2_archive_open(&g_ani, "original_game/ANI.DAT") != 0) {
            fprintf(stderr, "cannot open ANI.DAT\n");
            rc = -1;
            goto cleanup;
        }
        printf("ANI.DAT: %zu entries\n", g_ani.count);

        /* title_action_dispatch @0x25ebb：START直接自然开场，LOAD恢复准备，
         * CONTINUE只恢复stage0/1活动快照；败北返回标题而不重播片头。 */
        int first_title = !new_game_play;
        while (rc == 0) {
            fd2_save_file title_save = {0};
            fd2_save_battle_snapshot title_snapshot;
            int title_continue_available =
                fd2_save_file_open(&title_save, save_path) == 0 &&
                fd2_save_file_get_battle_snapshot(&title_save, &title_snapshot) == 0 &&
                (title_snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] == 0u ||
                 title_snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] == 1u ||
                 title_snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] == 2u);
            fd2_save_file_close(&title_save);
            fd2_title_action title_action = boot_intro_title(
                &vga, bgm, title_audio_ptr, first_title, title_continue_available);
            first_title = 0;
            fd2_app_flow flow = fd2_app_flow_from_title(title_action);
            if (flow == FD2_APP_FLOW_EXIT) break;
            if (flow == FD2_APP_FLOW_ERROR) { rc = -1; break; }
            size_t stage = 0;
            int load_active = 0;
            fd2_field_handoff handoff;
            const fd2_field_handoff *opening = NULL;
            if (flow == FD2_APP_FLOW_LOAD_GAME) {
                int result = fd2_field_prepare_load_title(&vga, &g_fdother,
                    &app_context, save_path, field_audio_ptr);
                flow = fd2_app_flow_from_prepare((fd2_app_prepare_result)result);
                if (flow == FD2_APP_FLOW_BATTLE) flow = FD2_APP_FLOW_PREPARE;
            } else if (flow == FD2_APP_FLOW_START_NEW_GAME) {
                app_context.has_preparation = 0; /* 只废弃旧准备owner，不重置53EC8/53AE1/RNG。 */
                int result = fd2_scene_play_new_game_prologue_handoff(
                    &vga, &g_fdother, bgm, field_audio_ptr, 0, &handoff);
                flow = result == FD2_SCENE_RESULT_OK ? FD2_APP_FLOW_BATTLE :
                    result == FD2_SCENE_RESULT_HOST_QUIT ? FD2_APP_FLOW_EXIT : FD2_APP_FLOW_ERROR;
                if (result == FD2_SCENE_RESULT_OK) opening = &handoff;
            } else if (flow == FD2_APP_FLOW_CONTINUE_GAME) {
                fd2_save_file save = {0}; fd2_save_battle_snapshot snapshot;
                if (fd2_save_file_open(&save, save_path) != 0 ||
                    fd2_save_file_get_battle_snapshot(&save, &snapshot) != 0 ||
                    (snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] != 0u &&
                     snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] != 1u &&
                     snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] != 2u)) {
                    fprintf(stderr, "活动快照无法恢复：%s\n", save_path);
                    flow = FD2_APP_FLOW_ERROR;
                } else {
                    stage = snapshot.meta[FD2_SAVE_BATTLE_META_STAGE];
                    load_active = 1;
                    flow = FD2_APP_FLOW_BATTLE;
                }
                fd2_save_file_close(&save);
            }
            flow = run_field_chain(&vga, &g_fdother, field_audio_ptr, save_path,
                &app_context, flow, stage, opening, load_active);
            if (flow == FD2_APP_FLOW_TITLE) continue;
            if (flow != FD2_APP_FLOW_EXIT && flow != FD2_APP_FLOW_LEAVE_GAME) rc = -1;
            break;
        }
        fd2_archive_close(&g_ani);
    }

cleanup:
    /* BGM/audio source 借用 FDMUS/FDOTHER 数据。先停止 device 并 retire，
     * 再释放 sequence、bank 和 archive。 */
    fd2_audio_destroy(audio);
    fd2_bgm_destroy(bgm);
    fd2_pcm_player_close(&secondary_pcm_player);
    fd2_pcm_player_close(&pcm_player);
    fd2_pcm_bank_close(&arrival_sfx_bank);
    fd2_pcm_bank_close(&battle_sfx_bank);
    fd2_pcm_bank_close(&title_letter_fly_sfx_bank);
    fd2_pcm_bank_close(&title_sfx_bank);
    fd2_pcm_bank_close(&ui_sfx_bank);
    fd2_vga_close(&vga);
    fd2_archive_close(&g_fdother);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return rc == 0 ? 0 : rc == 2 ? 2 : 1;
}
