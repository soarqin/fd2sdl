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

/* 城镇／酒店／四槽资源；规范VA及原登记名见实现头。图像拥有内存，font借FDOTHER。 */
typedef struct {
    fd2_image town, town_stage2, town_panel, hotel, slots;
    fd2_image hotel_icons[8];
    fd2_image hotel_portraits[4], depart_portraits[4];
    fd2_archive text_archive, dato;
    fd2_font font;
    fd2_text_entry text;
    fd2_map_sprite_bank sprites;
    fd2_field_info_assets info;
    fd2_field_command_assets commands;
    int ready;
} fd2_field_prepare_assets;

int fd2_field_prepare_assets_open(fd2_field_prepare_assets *assets,
    const fd2_archive *fdother);
void fd2_field_prepare_assets_close(fd2_field_prepare_assets *assets);
/* 纯绘制：成功0，错误-1且全VGA不变；不写ctx/RNG/动态数字owner。 */
int fd2_field_prepare_draw_town(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    int selected, unsigned phase);
int fd2_field_prepare_draw_hotel(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_archive *fdother,
    int selected, unsigned highlight, unsigned portrait_frame, size_t fragment);
int fd2_field_prepare_draw_slots(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_file *save, int selected);
int fd2_field_prepare_draw_roster(fd2_vga *vga,
    const fd2_field_prepare_assets *assets, const fd2_save_manual_state *state,
    int selected, size_t first, unsigned phase);

/* 仅stage1/2准备状态；先做对应资源/整roster预检，1..16才可请求出阵。
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
