/* 炎龙骑士团 2 SDL3 重写 - 战场图形指令菜单
 *
 * field_command_menu_open @0x3c630 从 FDOTHER[2] 取图标，并以四步
 * 上／左／右／下展开；field_command_menu_input @0x3ca10 将四方向直接
 * 映射到 attack/magic/item/wait。其内部 field_command_menu_wait_key
 * @0x3caac 读取键盘并切换高亮相位，field_command_menu_draw @0x3cbe9
 * 绘制四帧；确认或取消后由 field_command_menu_close @0x3c8c8 收起。
 */
#include "field_command.h"

#include <stdlib.h>
#include <string.h>

#include "map_sprite.h"

#define FD2_FIELD_COMMAND_RESOURCE_INDEX 2u

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void fd2_field_command_assets_close(fd2_field_command_assets *assets) {
    if (!assets) return;
    for (size_t i = 0; i < FD2_FIELD_COMMAND_FRAME_COUNT; i++)
        fd2_image_free(&assets->frames[i]);
    memset(assets, 0, sizeof(*assets));
}

int fd2_field_command_assets_open(fd2_field_command_assets *assets,
                                  const fd2_archive *fdother) {
    if (!assets || !fdother) return -1;
    memset(assets, 0, sizeof(*assets));

    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(fdother, FD2_FIELD_COMMAND_RESOURCE_INDEX,
                        &data, &size) != 0 || size < 4)
        return -1;

    uint32_t first_offset = rd_u32_le(data);
    if (first_offset % 4u != 0 || first_offset > size ||
        first_offset / 4u < FD2_FIELD_COMMAND_FRAME_COUNT)
        return -1;
    size_t frame_count = first_offset / 4u;
    if (frame_count > size / 4u) return -1;

    uint32_t previous = first_offset;
    for (size_t i = 0; i < frame_count; i++) {
        uint32_t offset = rd_u32_le(data + i * 4u);
        if (offset < first_offset || offset >= size ||
            (i > 0 && offset <= previous))
            goto fail;
        previous = offset;
    }

    for (size_t i = 0; i < FD2_FIELD_COMMAND_FRAME_COUNT; i++) {
        uint32_t start = rd_u32_le(data + i * 4u);
        uint32_t end = i + 1u < frame_count
            ? rd_u32_le(data + (i + 1u) * 4u)
            : (uint32_t)size;
        size_t pixel_count = (size_t)FD2_FIELD_COMMAND_ICON_WIDTH *
                             FD2_FIELD_COMMAND_ICON_HEIGHT;
        if (end <= start || (size_t)(end - start) != 4u + pixel_count ||
            rd_u16_le(data + start) != FD2_FIELD_COMMAND_ICON_WIDTH ||
            rd_u16_le(data + start + 2u) != FD2_FIELD_COMMAND_ICON_HEIGHT)
            goto fail;
        fd2_image *frame = &assets->frames[i];
        frame->width = FD2_FIELD_COMMAND_ICON_WIDTH;
        frame->height = FD2_FIELD_COMMAND_ICON_HEIGHT;
        frame->pixels = malloc(pixel_count);
        if (!frame->pixels) goto fail;
        memcpy(frame->pixels, data + start + 4u, pixel_count);
    }
    assets->ready = 1;
    return 0;

fail:
    fd2_field_command_assets_close(assets);
    return -1;
}

const fd2_image *fd2_field_command_frame(
        const fd2_field_command_assets *assets,
        fd2_field_command command, int selected, int disabled,
        uint8_t highlight_phase) {
    if (!assets || !assets->ready || command < FD2_FIELD_COMMAND_ATTACK ||
        command > FD2_FIELD_COMMAND_WAIT)
        return NULL;
    size_t frame = (size_t)command * 3u;
    if (disabled)
        frame += 2u;
    else if (selected && (highlight_phase & 1u))
        frame += 1u;
    return &assets->frames[frame];
}

int fd2_field_command_first_enabled(
        const uint8_t disabled[FD2_FIELD_COMMAND_COUNT]) {
    /* field_command_first_enabled_select @0x3c5fb：从索引 0 起找到
     * 第一项 disabled==0 的命令，并写入原版全局选择索引。 */
    if (!disabled) return -1;
    for (size_t i = 0; i < FD2_FIELD_COMMAND_COUNT; i++)
        if (!disabled[i]) return (int)i;
    return -1;
}

int fd2_field_command_select_direction(
        int current, fd2_field_command_direction direction,
        const uint8_t disabled[FD2_FIELD_COMMAND_COUNT]) {
    if (!disabled || direction < FD2_FIELD_COMMAND_DIRECTION_UP ||
        direction > FD2_FIELD_COMMAND_DIRECTION_DOWN)
        return current;
    static const fd2_field_command command_by_direction[] = {
        FD2_FIELD_COMMAND_ATTACK,
        FD2_FIELD_COMMAND_MAGIC,
        FD2_FIELD_COMMAND_ITEM,
        FD2_FIELD_COMMAND_WAIT,
    };
    int candidate = command_by_direction[direction];
    return disabled[candidate] ? current : candidate;
}

void fd2_field_command_draw_animation(
        fd2_vga *vga, const fd2_field_command_assets *assets,
        int focus_x, int focus_y, int selected,
        const uint8_t disabled[FD2_FIELD_COMMAND_COUNT],
        uint8_t highlight_phase, int opening, uint8_t animation_phase) {
    if (!vga || !assets || !assets->ready || !disabled) return;
    if (animation_phase > 4u) animation_phase = 4u;
    const int phase = animation_phase;
    /* field_command_menu_open @0x3c630 从共同的 y=2 起步；close
     * @0x3c8c8 则从 (-20,-24,+24,+24) 的独立初值反向移动。
     * 两者的实际四帧并非同一组坐标的简单倒序。 */
    const int dx[] = {
        0,
        opening ? -6 * phase : -24 + 6 * phase,
        opening ? 6 * phase : 24 - 6 * phase,
        0,
    };
    const int dy[] = {
        opening ? 2 - 5 * phase : -20 + 5 * phase,
        2,
        2,
        opening ? 2 + 5 * phase : 24 - 5 * phase,
    };
    for (size_t i = 0; i < FD2_FIELD_COMMAND_COUNT; i++) {
        const fd2_image *frame = fd2_field_command_frame(
            assets, (fd2_field_command)i, selected == (int)i,
            disabled[i], highlight_phase);
        if (frame)
            fd2_map_sprite_blit(vga, frame, focus_x + dx[i],
                                focus_y + dy[i], 0);
    }
}

void fd2_field_command_draw(
        fd2_vga *vga, const fd2_field_command_assets *assets,
        int focus_x, int focus_y, int selected,
        const uint8_t disabled[FD2_FIELD_COMMAND_COUNT],
        uint8_t highlight_phase) {
    fd2_field_command_draw_animation(
        vga, assets, focus_x, focus_y, selected, disabled,
        highlight_phase, 1, 4u);
}
