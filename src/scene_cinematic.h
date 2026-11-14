#ifndef FD2_SCENE_CINEMATIC_H
#define FD2_SCENE_CINEMATIC_H

/* sub_24336 / sub_2189A / sub_24B4D / sub_31529 / sub_31C49：
 * 一次Scene调用内的有限页、资源和绘制叶。PC、顺序、循环、音乐、等待、
 * 输入skip及terminal hold均由冷图/caller决定；本对象不保存Game/SAV副本。 */
#include "archive.h"
#include "field_game.h"
#include "field_audio.h"
#include "vga.h"

typedef struct fd2_scene_cinematic fd2_scene_cinematic;

enum {
    FD2_CINEMATIC_VGA = 0,
    FD2_CINEMATIC_WORK = 1,
    FD2_CINEMATIC_BASE = 2,
    FD2_CINEMATIC_CARD = 3,
    FD2_CINEMATIC_ROLL = 4,
    FD2_CINEMATIC_APP_PREFIX = 5,
    FD2_CINEMATIC_PAGE_COUNT = 6,
    FD2_CINEMATIC_SHEET_COUNT = 3
};

int fd2_scene_cinematic_open(fd2_scene_cinematic **out,fd2_vga *vga,
    fd2_field_game *game,const fd2_archive *fdother);
/* 仅释放临时资源；不回滚已到达的像素、DAC、Game或RNG前缀。 */
void fd2_scene_cinematic_close(fd2_scene_cinematic **scope);

/* 固定源容量64000/128000/224000，slot1..4；VGA不转移所有权。
 * 首次安全清零不声明原malloc未写字节的视觉等价。 */
int fd2_scene_cinematic_page_alloc(fd2_scene_cinematic *scope,int slot,size_t bytes);
int fd2_scene_cinematic_page_release(fd2_scene_cinematic *scope,int slot);
int fd2_scene_cinematic_page_clear(fd2_scene_cinematic *scope,int slot,uint8_t value);
int fd2_scene_cinematic_work_clear(fd2_scene_cinematic *scope,uint8_t value);
int fd2_scene_cinematic_copy(fd2_scene_cinematic *scope,int source,int destination,
    size_t source_offset,size_t destination_offset,size_t width,size_t height,
    size_t source_stride,size_t destination_stride);
int fd2_scene_cinematic_pixels(const fd2_scene_cinematic *scope,int slot,
    const uint8_t **pixels,size_t *size);

/* FIGANI式part+9 RLE目录；完整预检后提交，SKIP与opaque0分别保留。
 * slot0..2只借App归档。绘制不present/等待/切曲/清键。 */
int fd2_scene_cinematic_sheet_open(fd2_scene_cinematic *scope,int slot,size_t entry);
int fd2_scene_cinematic_sheet_close(fd2_scene_cinematic *scope,int slot);
int fd2_scene_cinematic_sheet_info(const fd2_scene_cinematic *scope,int slot,
    size_t *count,size_t *intro_count);
int fd2_scene_cinematic_sheet_duration(const fd2_scene_cinematic *scope,int slot,
    size_t frame,unsigned *ticks);
int fd2_scene_cinematic_sheet_blit(fd2_scene_cinematic *scope,int slot,size_t frame,
    int destination,int x,int y,size_t stride);
int fd2_scene_cinematic_image_blit(fd2_scene_cinematic *scope,size_t entry,
    int destination,int x,int y,size_t stride);

/* 原53A65的临时借用：只改基色，不隐式应用DAC；restore必须由冷图调用。 */
int fd2_scene_cinematic_palette_save(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_palette_validate(const fd2_scene_cinematic *scope,size_t entry);
int fd2_scene_cinematic_palette_load(fd2_scene_cinematic *scope,size_t entry);
int fd2_scene_cinematic_palette_restore(fd2_scene_cinematic *scope);
/* entry24的88借用与1d4f6/1a80a共享free切片；primary stop先于close。
 * App FDOTHER原字节仍晚于全部PCM引用退休，不重载默认bank80。 */
int fd2_scene_cinematic_audio_open(fd2_scene_cinematic *scope,size_t entry);
int fd2_scene_cinematic_audio_play(fd2_scene_cinematic *scope,fd2_field_audio *audio,int sample);
int fd2_scene_cinematic_audio_close(fd2_scene_cinematic *scope,fd2_field_audio *audio);

/* 2189a：terrain快照一次；每帧circle12→127a9→投影，10次/半径步长在冷图。
 * 24b4d：13×9首地形与11cac(0)后，冷图逐次选行0/1并等待20ms。 */
int fd2_scene_cinematic_circle_prepare(fd2_scene_cinematic *scope,size_t actor);
int fd2_scene_cinematic_circle_frame(fd2_scene_cinematic *scope,size_t actor,
    int radius,size_t lut_frame);
int fd2_scene_cinematic_circle_close(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_shake_prepare(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_shake_frame(fd2_scene_cinematic *scope,unsigned row);

/* credits临时actor/资源游标，不改Game单位/持久roster。outer_index由冷图给出，
 * 原j=0/1互换；idx<count且映射raw存在才接受。intro8..0、20帧、FIGANI
 * intro部分及220/440卡片帧的循环/等待/输入仍由冷图逐次调用。 */
int fd2_scene_cinematic_credits_actor(fd2_scene_cinematic *scope,size_t outer_index);
int fd2_scene_cinematic_credits_assets_open(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_assets_close(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_intro(fd2_scene_cinematic *scope,unsigned phase);
int fd2_scene_cinematic_credits_intro_finish(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_idle_reset(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_idle(fd2_scene_cinematic *scope,int destination);
int fd2_scene_cinematic_credits_part(fd2_scene_cinematic *scope,size_t part);
int fd2_scene_cinematic_credits_part_info(const fd2_scene_cinematic *scope,
    size_t *count,size_t part,unsigned *ticks);
int fd2_scene_cinematic_credits_part_count(const fd2_scene_cinematic *scope,
    size_t *count);
int fd2_scene_cinematic_credits_card_prepare(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_blink_reset(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_card(fd2_scene_cinematic *scope,unsigned frame);
int fd2_scene_cinematic_credits_actor_close(fd2_scene_cinematic *scope);
/* 原outer j=1的本卡局部标记；只由冷图set/query，不轮询输入或跳PC。
 * actor_close保留它，供冷图在本卡结束后决定下一卡。 */
int fd2_scene_cinematic_credits_skip_set(fd2_scene_cinematic *scope,int skip);
int fd2_scene_cinematic_credits_skip_get(const fd2_scene_cinematic *scope,int *skip);
/* 原FDTXT[31]44长页，fd2_font唯一字模绘制器；不等待或自动滚页。 */
int fd2_scene_cinematic_credits_roll_text(fd2_scene_cinematic *scope);

/* 完整31529双caller的montage资源/单帧口；只准hot stage26/30。
 * 原marker HP63680由冷图现unit_field_write提交；RNG仅显式branch调用。
 * 原54133非0使25a96在解引用bank前返回，cue不触设备或另载bank。 */
int fd2_scene_cinematic_montage_open(fd2_scene_cinematic *scope,int first,int second,int scene);
int fd2_scene_cinematic_montage_close(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_montage_intro(fd2_scene_cinematic *scope,unsigned phase);
int fd2_scene_cinematic_montage_intro_finish(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_montage_half(fd2_scene_cinematic *scope,unsigned half);
int fd2_scene_cinematic_montage_current_half(const fd2_scene_cinematic *scope,unsigned *half);
int fd2_scene_cinematic_montage_part_count(const fd2_scene_cinematic *scope,size_t *count);
int fd2_scene_cinematic_montage_part_info(const fd2_scene_cinematic *scope,size_t part,
    uint8_t *marker,uint8_t *ticks,int *last_marker);
int fd2_scene_cinematic_montage_rng(fd2_scene_cinematic *scope,unsigned threshold,int *yes);
int fd2_scene_cinematic_montage_hit(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_montage_cue(fd2_scene_cinematic *scope,size_t part);
int fd2_scene_cinematic_montage_frame(fd2_scene_cinematic *scope,size_t part);

#endif
