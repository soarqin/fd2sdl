/* B6普通FULL法术0..9纯FX推进。
 * 依据：field_magic_battle_animation @0x2ff01的0x524c6 dispatch、
 * sub_2B996 @0x2b996（IDA现owner含1..6入口）、sub_2CAFC @0x2cafc、
 * sub_2CCF4 @0x2ccf4、sub_2CE1A @0x2ce1a；不声明新的原ABI。
 * 无归档、Game、VGA或音频owner；HP/MP/XP及命中后jitter由caller提交。 */
#ifndef FD2_MAGIC_FX_H
#define FD2_MAGIC_FX_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t effect_entry; /* FDOTHER原图包；不是SFX bank。 */
    uint8_t sfx_bank;
    uint8_t hp_denominator; /* 526bc；不等同phase5返回1的次数。 */
    uint8_t minimum_sample_count; /* FX cue最高index+1；不含未知caster额外cue。 */
    uint8_t hit_frame_base; /* 2ff01的32/176。 */
    uint8_t return_lut_base; /* 11/15/19；回场景实际取base+1..3。 */
    uint16_t minimum_part_count;
} fd2_magic_fx_parameters;
/* side0+ID9为44；side!=0的ID9原9B拷贝没有第10项，明确拒绝，
 * 不用邻表首字节20补默认。错误保持out原值。 */
int fd2_magic_fx_parameters_get(uint8_t id, uint8_t side,
    fd2_magic_fx_parameters *out);

typedef struct {
    const uint8_t *part_durations; /* 原part+6，借用覆盖完整cast。 */
    size_t part_count;
} fd2_magic_fx_parts;

typedef enum {
    FD2_MAGIC_FX_PART = 0,
    FD2_MAGIC_FX_PRIMARY_SAMPLE = 1,
    FD2_MAGIC_FX_SECONDARY_SAMPLE = 2
} fd2_magic_fx_emit_kind;
typedef struct {
    fd2_magic_fx_emit_kind kind;
    uint8_t phase;
    uint16_t index; /* PART为part index，其余为sample index。 */
    int32_t x, y; /* 相对当前caller工作页origin；cue固定0,0。 */
} fd2_magic_fx_emission;
typedef uint32_t (*fd2_magic_fx_rng_fn)(void *userdata);
typedef int (*fd2_magic_fx_emit_fn)(const fd2_magic_fx_emission *event,
    void *userdata);

typedef struct {
    uint8_t id, side;
    fd2_magic_fx_parts parts;
    union {
        struct { int32_t age[8]; } rain;
        struct { uint8_t frame, timer; } wheel;
        struct {
            int32_t age[12], slot[12];
            uint8_t variant[6], next, stop, flip;
        } particles;
        struct {
            int32_t x[5], y[5], age[5];
            uint8_t spark[5], radius, started;
        } orbit;
        struct { int32_t age[16]; } burst;
        struct { uint8_t frame, blink; } beam;
    } state;
} fd2_magic_fx;

/* 只准备纯owner，不执行原phase0，不消费RNG或emit；失败保持fx不变。
 * 每次cast只init一次，state贯穿intro/所有target/9tick过渡/tail。 */
int fd2_magic_fx_init(fd2_magic_fx *fx, uint8_t id, uint8_t side,
    fd2_magic_fx_parts parts);
/* phase为原0..8；成功0并写原返回值，错误-1且out未写。
 * emit=NULL照常推进/消费全部FX RNG。4/5需要rng返回0..65535；
 * emit错误或RNG契约错误发生后不回滚已消费RNG/已发event，caller锁存错误。
 * 不执行effect、HP插值或impact jitter；phase5返回1由caller按原条件处理。 */
int fd2_magic_fx_step(fd2_magic_fx *fx, uint8_t phase,
    fd2_magic_fx_rng_fn rng, void *rng_userdata,
    fd2_magic_fx_emit_fn emit, void *emit_userdata, int *out);
#endif
