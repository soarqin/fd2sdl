#ifndef FD2_SAVE_H
#define FD2_SAVE_H

#include <stddef.h>
#include <stdint.h>

#define FD2_SAVE_FILE_SIZE 0x59cbu
#define FD2_SAVE_CHECKSUM_SIZE 4u
#define FD2_SAVE_SLOT_BASE 0x312bu
#define FD2_SAVE_SLOT_SIZE 0x0a28u
#define FD2_SAVE_SLOT_COUNT 4u
#define FD2_SAVE_UNIT_TABLE_SIZE 0x0a00u
#define FD2_SAVE_UNIT_RECORD_SIZE 0x50u
#define FD2_SAVE_MAX_UNITS 32u
#define FD2_SAVE_SLOT_META_SIZE 0x28u
#define FD2_SAVE_LAST_SLOT_META_SIZE 0x24u

/* 原版活动战场快照，位于四个手工 slot 之前。战场 save 路径 code0
 * 0x9fe9..0xa102 更新这些范围；启动／恢复路径 code0 0x10..0x44b
 * 原样读回。 */
#define FD2_SAVE_BATTLE_UNIT_BASE 0x12a3u
#define FD2_SAVE_BATTLE_UNIT_AREA_SIZE 0x1e00u
#define FD2_SAVE_BATTLE_MAX_UNITS \
    (FD2_SAVE_BATTLE_UNIT_AREA_SIZE / FD2_SAVE_UNIT_RECORD_SIZE)
#define FD2_SAVE_BATTLE_CELL_STATE_BASE 0x30a3u
#define FD2_SAVE_BATTLE_CELL_STATE_SIZE 0x20u
#define FD2_SAVE_BATTLE_META_BASE 0x30c3u
#define FD2_SAVE_BATTLE_META_SIZE 0x68u
#define FD2_SAVE_BATTLE_META_TURN 0u
#define FD2_SAVE_BATTLE_META_UNIT_COUNT 1u
#define FD2_SAVE_BATTLE_META_STAGE 2u
#define FD2_SAVE_BATTLE_META_CAMERA_X 3u
#define FD2_SAVE_BATTLE_META_CAMERA_Y 4u
#define FD2_SAVE_BATTLE_META_FOCUS_X 5u
#define FD2_SAVE_BATTLE_META_FOCUS_Y 6u
#define FD2_SAVE_BATTLE_META_FOCUS_REL_X 7u
#define FD2_SAVE_BATTLE_META_FOCUS_REL_Y 8u
#define FD2_SAVE_BATTLE_META_UNIT_COUNT_COPY 9u
#define FD2_SAVE_BATTLE_META_RESOURCE_VALUE 10u
#define FD2_SAVE_BATTLE_META_OPTION_EFFECT 14u
#define FD2_SAVE_BATTLE_META_OPTION_CAMERA 15u
#define FD2_SAVE_BATTLE_META_OPTION_MUSIC 16u
#define FD2_SAVE_BATTLE_META_OPTION_SFX 17u

typedef struct {
    uint8_t bytes[FD2_SAVE_UNIT_RECORD_SIZE];
} fd2_save_unit;

typedef struct {
    uint8_t stage_id;
    uint8_t unit_count;
    uint32_t gold_or_flags;
    uint8_t flags[4];
    fd2_save_unit units[FD2_SAVE_MAX_UNITS];
} fd2_save_slot;

/* 原版手工 slot 的完整可编辑区域。调用方先从当前容器读取整块数据，
 * 再只修改已有 owner；这样未知记录和 meta 字节不会被 SDL 猜测覆盖。 */
typedef struct {
    uint8_t unit_area[FD2_SAVE_UNIT_TABLE_SIZE];
    uint8_t meta[FD2_SAVE_SLOT_META_SIZE];
} fd2_save_manual_slot;

/* 四槽 hand_save/hand_load 的运行期字段。unit_area 仍是原始完整 0xa00
 * 字节；这些字段只提供已确认的 meta owner，未知 meta 字节由 codec 保留。 */
typedef struct {
    uint8_t stage_id;
    uint8_t unit_count;
    uint32_t gold_or_flags;
    uint8_t options[4];
    uint8_t unit_area[FD2_SAVE_UNIT_TABLE_SIZE];
} fd2_save_manual_state;

/* 保留式 FD2.SAV 容器。data 始终保存解密后的完整 0x59cb 字节；
 * 更新 slot 时只修改指定范围及末尾 checksum，未知区域保持原样。 */
typedef struct {
    uint8_t *data;
    size_t size;
} fd2_save_file;

typedef struct {
    uint8_t unit_area[FD2_SAVE_BATTLE_UNIT_AREA_SIZE];
    uint8_t cell_state[FD2_SAVE_BATTLE_CELL_STATE_SIZE];
    uint8_t meta[FD2_SAVE_BATTLE_META_SIZE];
} fd2_save_battle_snapshot;

void fd2_save_xor_crypt(uint8_t *data, size_t size);
uint32_t fd2_save_checksum_sum(const uint8_t *data, size_t size);
int fd2_save_validate_plain(const uint8_t *data, size_t size);

int fd2_save_file_open(fd2_save_file *save, const char *path);
/* 原版 hand_save 在 FD2.SAV 不存在时以 0xff 建立完整容器；该函数只
 * 创建明文内存 shadow，调用方仍需经 fd2_save_file_write() 原子写回。 */
int fd2_save_file_create_empty(fd2_save_file *save);
int fd2_save_file_open_encrypted(fd2_save_file *save,
                                 const uint8_t *encrypted, size_t size);
void fd2_save_file_close(fd2_save_file *save);
int fd2_save_file_write(const fd2_save_file *save, const char *path);

int fd2_save_file_get_battle_snapshot(
    const fd2_save_file *save, fd2_save_battle_snapshot *snapshot);
/* 安全前置条件：snapshot 必须由 fd2_save_file_get_battle_snapshot()
 * 从当前格式容器提取，再只修改已确认字段；不得零初始化或自行构造。
 * update 使用完整明文 shadow，校验通过后一次提交到 save->data。 */
int fd2_save_file_update_battle_snapshot(
    fd2_save_file *save, const fd2_save_battle_snapshot *snapshot);

int fd2_save_file_slot_valid(const fd2_save_file *save, size_t slot_index);
int fd2_save_file_get_slot(const fd2_save_file *save, size_t slot_index,
                           fd2_save_slot *slot);
/* 读取原版手工 slot 的整块单位区和 meta。空 slot 只由 stage_id=0xff
 * 判定；unit_count 的语义校验由调用方依据当前 session 再执行。 */
/* 严格读取：除 stage_id 外，unit_count 也必须是可解析的 0..32。 */
int fd2_save_file_get_manual_slot(const fd2_save_file *save,
                                  size_t slot_index,
                                  fd2_save_manual_slot *slot);
/* 允许读取原版 hand_load 的空 slot；仅 stage_id=0xff 表示无效。 */
int fd2_save_file_get_manual_slot_raw(const fd2_save_file *save,
                                      size_t slot_index,
                                      fd2_save_manual_slot *slot);
/* 用完整 shadow 一次更新手工 slot。slot 3 的末 4 字节与文件 checksum
 * 重叠，因此只写 meta 前 0x24 字节，随后重算 checksum。 */
int fd2_save_file_update_manual_slot(fd2_save_file *save, size_t slot_index,
                                     const fd2_save_manual_slot *slot);
int fd2_save_manual_slot_decode(const fd2_save_manual_slot *slot,
                                fd2_save_manual_state *state);
int fd2_save_manual_slot_encode(fd2_save_manual_slot *slot,
                                const fd2_save_manual_state *state);
/* meta_28[1] 必须等于 unit_count。兼容调用只覆盖提供的记录，其余单位
 * 区域保留原容器字节；正式手工 slot writer 使用上面的完整接口。 */
int fd2_save_file_update_slot(fd2_save_file *save, size_t slot_index,
                              const fd2_save_unit *units,
                              size_t unit_count,
                              const uint8_t meta_28[FD2_SAVE_SLOT_META_SIZE]);

/* 兼容旧调用：验证整文件 checksum 后读取一个 slot。 */
int fd2_save_load_slot(fd2_save_slot *slot, const char *path,
                       size_t slot_index);

uint8_t fd2_save_unit_class(const fd2_save_unit *unit);
uint8_t fd2_save_unit_portrait(const fd2_save_unit *unit);
uint8_t fd2_save_unit_name_id(const fd2_save_unit *unit);

#endif /* FD2_SAVE_H */
