#ifndef FD2_FIELD_PREPARE_PLAY_H
#define FD2_FIELD_PREPARE_PLAY_H

#include "field_play.h"
#include "field_info.h"
#include "field_command.h"
#include "map_sprite.h"

typedef enum {
    FD2_FIELD_PREPARE_ERROR = -1,
    FD2_FIELD_PREPARE_DEPART = 0,
    FD2_FIELD_PREPARE_TITLE = 1,
    FD2_FIELD_PREPARE_HOST_QUIT = 2,
    FD2_FIELD_PREPARE_LEAVE_GAME = 3
} fd2_field_prepare_result;

/* 一次只持有当前服务页；准备owner不在资源对象中复制。 */
typedef struct {
    fd2_image background, money_panel, roster_page, icons[8], portraits[4];
    fd2_image price_icon, preview_labels[4], preview_arrow;
    fd2_image scroll_arrows[5], money_strip; /* frames11..14/17、raw frame2 */
    fd2_image animation_frames[9];
    size_t animation_count; /* 原服务归档23..末尾，提交前完整解码 */
    int selector, ready;
} fd2_field_prepare_service_assets;

/* 城镇／酒店／四槽资源；规范VA及原登记名见实现头。图像拥有内存，font借FDOTHER。 */
typedef struct {
    fd2_image town, town_type1, town_stage2, town_panel, hotel, slots;
    fd2_image hotel_icons[8];
    fd2_image hotel_scroll_arrows[5]; /* 27079：FDOTHER13 frames11..14/17，8x9 LMI。 */
    fd2_image hotel_portraits[4], depart_portraits[4];
    fd2_archive text_archive, dato;
    fd2_font font;
    fd2_text_entry text;
    fd2_map_sprite_bank sprites;
    fd2_field_info_assets info;
    fd2_field_command_assets commands;
    fd2_field_prepare_service_assets service;
    uint8_t sprite_cache_ids[FD2_SAVE_MAX_UNITS];
    size_t sprite_cache_count;
    int sprite_cache_ready; /* 原11019目录身份，仅在显式重建点更新。 */
    int ready;
} fd2_field_prepare_assets;

int fd2_field_prepare_assets_open(fd2_field_prepare_assets *assets,
    const fd2_archive *fdother);
void fd2_field_prepare_assets_close(fd2_field_prepare_assets *assets);
/* 279bc/29daa：仅当前商店1/3/5或教会4；完整候选成功后才替换旧资源。
 * 不换准备owner、音乐、VGA或RNG，失败保留旧服务，FDOTHER只借用。 */
int fd2_field_prepare_assets_open_service(fd2_field_prepare_assets *assets,
    const fd2_archive *fdother, int selector);
int fd2_field_prepare_assets_bind_roster(fd2_field_prepare_assets *assets,
    const fd2_save_manual_state *state);
int fd2_field_prepare_draw_service(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_archive *fdother,
    const fd2_save_manual_state *state, int selected, unsigned highlight,
    unsigned portrait_frame, size_t fragment);
/* 纯绘制：成功0，错误-1且全VGA不变；不写ctx/RNG/动态数字owner。
 * town有App页时提交原265ec已定义320x200区域及面板，再投影内区；
 * 新town临时底在该区域外的原padding来源未证，不据此补零整页。 */
int fd2_field_prepare_draw_town(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    int selected, unsigned phase);
int fd2_field_prepare_draw_hotel(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_archive *fdother,
    int selected, unsigned highlight, unsigned portrait_frame, size_t fragment);
int fd2_field_prepare_draw_slots(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_file *save, int selected);
/* 27d33酒店名单的当前页widget；初开caller先画slots，随后2810b/27079
 * 保留RLE孔，不在此函数重建底板；错误仍全VGA不变。 */
int fd2_field_prepare_draw_roster(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    int selected, size_t first, unsigned phase);

/* 已证31B城镇记录或NoTown入口；不因此开放对应battle或场景绑定。 */
int fd2_field_prepare_stage_supported(size_t stage);
/* 完整持久表预检，16/20名出阵配额与额外成员选择遵循冷图。
 * DEPART不消费准备owner；相应Game+Scene入口完整成功后才清has_preparation。 */
fd2_field_prepare_result fd2_field_prepare_run(fd2_vga *vga,
    const fd2_archive *fdother, fd2_field_play_context *context,
    const char *save_path, fd2_field_audio *audio);
/* 成功只交接准备owner并返回DEPART，调用方随后仍须进入prepare_run；
 * 取消返回TITLE。失败/取消不改context.preparation/has_preparation。 */
fd2_field_prepare_result fd2_field_prepare_load_title(fd2_vga *vga,
    const fd2_archive *fdother, fd2_field_play_context *context,
    const char *save_path, fd2_field_audio *audio);

#endif
