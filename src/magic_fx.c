/* B6普通FULL法术FX：0x524c6十入口的纯状态翻译。
 * 依据：sub_2B996 @0x2b996及表内2bb33/2bd6c/2bfd9/2c217/2c441/2c67d，
 * sub_2CAFC @0x2cafc、sub_2CCF4 @0x2ccf4、sub_2CE1A @0x2ce1a；
 * secondary cue=sfx_play_secondary @0x25b45（原登记pal_mask_set）。
 * IDA现将1..6入口归入2b996 owner，本模块不改原函数边界或ABI。
 * 所有表取已验relocated原字节；只发绘制/声音意图，不调用任何宿主服务。 */
#include "magic_fx.h"
#include <math.h>
#include <string.h>

#include "static_data/magic_fx.inc"

int fd2_magic_fx_parameters_get(uint8_t id, uint8_t side,
        fd2_magic_fx_parameters *out) {
    if (!out || id >= 10u || (side && id == 9u)) return -1;
    if (k_fx_hit_frame_bases[id]>UINT8_MAX || k_fx_return_lut_bases[id]>UINT8_MAX) return -1;
    fd2_magic_fx_parameters value = {side ? k_fx_friendly_clips[id] : k_fx_hostile_clips[id],
        k_fx_banks[id], k_fx_hp_denominators[id], k_fx_sample_counts[id], (uint8_t)k_fx_hit_frame_bases[id],
        (uint8_t)k_fx_return_lut_bases[id], k_fx_minimum_parts[id]};
    *out = value;
    return 0;
}

int fd2_magic_fx_init(fd2_magic_fx *fx, uint8_t id, uint8_t side,
        fd2_magic_fx_parts parts) {
    fd2_magic_fx_parameters parameters;
    if (!fx || fd2_magic_fx_parameters_get(id, side, &parameters) != 0 ||
        !parts.part_durations || parts.part_count < parameters.minimum_part_count)
        return -1;
    fd2_magic_fx value;
    memset(&value, 0, sizeof(value));
    value.id = id; value.side = side; value.parts = parts;
    *fx = value;
    return 0;
}

typedef struct {
    fd2_magic_fx *fx;
    uint8_t phase;
    fd2_magic_fx_rng_fn rng;
    void *rng_userdata;
    fd2_magic_fx_emit_fn emit;
    void *emit_userdata;
} fx_call;

static int output(fx_call *c, fd2_magic_fx_emit_kind kind,
        int32_t index, int32_t x, int32_t y) {
    if (index < 0 || index > UINT16_MAX ||
        (kind == FD2_MAGIC_FX_PART && (size_t)index >= c->fx->parts.part_count))
        return -1;
    fd2_magic_fx_emission event = {kind, c->phase, (uint16_t)index, x, y};
    return c->emit && c->emit(&event, c->emit_userdata) != 0 ? -1 : 0;
}
#define DRAW(part, x, y) do { if (output(c, FD2_MAGIC_FX_PART, (part), (x), (y))) return -1; } while (0)
#define PRIMARY(sample) do { if (output(c, FD2_MAGIC_FX_PRIMARY_SAMPLE, (sample), 0, 0)) return -1; } while (0)
#define SECONDARY(sample) do { if (output(c, FD2_MAGIC_FX_SECONDARY_SAMPLE, (sample), 0, 0)) return -1; } while (0)

static int random_variant(fx_call *c, uint8_t scale, uint8_t *out) {
    uint32_t value = c->rng(c->rng_userdata);
    if (value > UINT16_MAX) return -1;
    *out = (uint8_t)((value % 2u) * scale);
    return 0;
}
static int32_t next_age(int32_t age) {
    return (int32_t)((uint32_t)age + 1u);
}

static int rain(fx_call *c, int *out) {
    fd2_magic_fx *fx = c->fx;
    int32_t *age = fx->state.rain.age;
    *out = 0;
    if (c->phase == 3u) {
        for (size_t i = 0; i < 8u; i++) age[i] = -(int32_t)(2u * i);
        *out = fx->id ? 31 : 28;
    } else if (c->phase == 4u || c->phase == 5u) {
        size_t count = fx->id ? 8u : 7u;
        for (size_t i = 0; i < count; i++) {
            if (!fx->id && c->phase == 4u && age[i] == 3) PRIMARY(1);
            if (age[i] >= 0 && age[i] < (fx->id ? 15 : 16)) {
                if (!fx->id) {
                    if (k_fx_rain0_front[i] == (c->phase == 4u))
                        DRAW(age[i], k_fx_rain0_x[i] + (fx->side ? 0 : 148), k_fx_rain0_y[i]);
                } else {
                    int opposite = (c->phase == 4u ? i >= 4u : i < 4u);
                    size_t position = opposite ? (i + 4u) % 8u : i;
                    DRAW(age[i] + (opposite ? 15 : 0),
                        k_fx_rain1_x[position] + 80 + (fx->side ? 0 : 148), k_fx_rain1_y[position]);
                }
            }
        }
        if (c->phase == 5u) {
            for (size_t i = 0; i < count; i++) {
                age[i] = next_age(age[i]);
                if (age[i] == 9) *out = 1;
                if (fx->id && age[i] == 5) PRIMARY(1);
            }
        }
    }
    return 0;
}

/* loc_2BF83：先画当前part，再以原u8 duration推进；0不是默认1。 */
static int wheel_part(fx_call *c) {
    fd2_magic_fx *fx = c->fx;
    uint8_t frame = fx->state.wheel.frame;
    DRAW(frame, 0, 0);
    fx->state.wheel.timer++;
    if (fx->state.wheel.timer == fx->parts.part_durations[frame]) {
        fx->state.wheel.timer = 0;
        fx->state.wheel.frame++;
    }
    return 0;
}
static int wheel(fx_call *c, int *out) {
    fd2_magic_fx *fx = c->fx;
    *out = 0;
    switch (c->phase) {
    case 0: fx->state.wheel.frame = fx->state.wheel.timer = 0; *out = 29; break;
    case 3: fx->state.wheel.frame = 16; *out = 12; break;
    case 6: PRIMARY(3); fx->state.wheel.frame = 10; *out = 10; break;
    case 1: case 7:
        if (fx->side) {
            if (fx->state.wheel.frame == 10u && c->phase == 1u) fx->state.wheel.frame = 15;
            if (wheel_part(c)) return -1;
        }
        break;
    case 2: case 8:
        if (fx->state.wheel.frame == 7u) PRIMARY(1);
        if (!fx->side) {
            if (fx->state.wheel.frame == 10u && c->phase == 2u) fx->state.wheel.frame = 15;
            if (wheel_part(c)) return -1;
        }
        if (fx->state.wheel.frame == 16u) DRAW(16, 0, 0);
        break;
    case 4: if (fx->side) DRAW(15, 1, -1); break;
    case 5:
        if (!fx->side) DRAW(15, -1, -1);
        DRAW(fx->state.wheel.frame, 1, -1);
        fx->state.wheel.frame++;
        if (fx->state.wheel.frame == 17u) { PRIMARY(2); *out = 1; }
        else if (fx->state.wheel.frame == 18u) fx->state.wheel.frame = 16;
        break;
    }
    return 0;
}

static int shower(fx_call *c, int *out) {
    fd2_magic_fx *fx = c->fx;
    int32_t *age = fx->state.particles.age, *slot = fx->state.particles.slot;
    *out = 0;
    if (c->phase == 0u) {
        for (size_t i = 0; i < 12u; i++) { age[i] = -(int32_t)(2u*i); slot[i] = (int32_t)i; }
        fx->state.particles.next = 12; fx->state.particles.stop = fx->state.particles.flip = 0;
        *out = 2;
    } else if (c->phase == 3u) *out = 40;
    else if (c->phase == 6u) { fx->state.particles.stop = 1; *out = 20; }
    else if (c->phase == 2u || c->phase == 5u || c->phase == 8u) {
        fx->state.particles.flip = (uint8_t)((fx->state.particles.flip + 1u) % 2u);
        for (size_t i = 0; i < 12u; i++) {
            int32_t at = slot[i]; if (at < 0 || at >= 12) return -1;
            if (age[i] >= 0 && age[i] < 11) DRAW(age[i] + k_fx_shower_parts[at], k_fx_shower_x[at] + (fx->side ? 0 : 20), -(int32_t)k_fx_shower_y[at]);
            if (!fx->state.particles.flip) {
                if (!age[i] && k_fx_shower_parts[at]) PRIMARY(2);
                age[i] = next_age(age[i]);
                if (age[i] == 3) { if (!k_fx_shower_parts[at]) SECONDARY(1); *out = 1; }
                if (age[i] == 11 && !fx->state.particles.stop) {
                    fx->state.particles.next = (uint8_t)((fx->state.particles.next + 1u) % 12u);
                    slot[i] = fx->state.particles.next; age[i] = 0;
                }
            }
        }
    }
    return 0;
}

static int dust(fx_call *c, int *out) {
    fd2_magic_fx *fx = c->fx;
    int32_t *age = fx->state.particles.age, *slot = fx->state.particles.slot;
    int fifth = fx->id == 5u;
    *out = 0;
    if (c->phase == 0u) {
        for (size_t i = 0; i < 6u; i++) {
            age[i] = -(int32_t)(2u*i); slot[i] = (int32_t)i;
            if (random_variant(c, fifth ? 6 : 7, &fx->state.particles.variant[i])) return -1;
        }
        fx->state.particles.next = 6; fx->state.particles.stop = 0; *out = fifth ? 1 : 2;
    } else if (c->phase == 3u) *out = 12;
    else if (c->phase == 6u) { fx->state.particles.stop = 1; *out = 8; }
    else if (c->phase == 2u || c->phase == 5u || c->phase == 8u) {
        for (size_t i = 0; i < 6u; i++) {
            int32_t at = slot[i]; if (at < 0 || at >= 10) return -1;
            if (age[i] >= 0 && age[i] < (fifth ? 6 : 7))
                DRAW(age[i] + fx->state.particles.variant[i], k_fx_dust_x[at] + (fx->side ? 0 : 143), 0);
            if (!age[i]) {
                if (!fifth || !i) PRIMARY(1);
                else if (i == 3u) SECONDARY(1);
            }
            age[i] = next_age(age[i]);
            if (age[i] == (fifth ? 2 : 3)) *out = 1;
            if (age[i] == (fifth ? 7 : 8) && !fx->state.particles.stop) {
                fx->state.particles.next = (uint8_t)((fx->state.particles.next + 1u) % 10u);
                slot[i] = fx->state.particles.next; age[i] = 0;
                if (random_variant(c, fifth ? 6 : 7, &fx->state.particles.variant[i])) return -1;
            }
        }
    }
    return 0;
}

/* sub_2B996 @0x2b996 的selector6内部入口0x2c67d：原cos/sin先返回qword再乘半径。
 * 固定Windows DOSBox参照/启动CW127f的五角、两数学分支、80坐标已用
 * 独立普通DOS数值程序及有理界核对；不把其它CW扫描或首越work后的帧签原像素。 */
static int orbit(fx_call *c, int *out) {
    fd2_magic_fx *fx = c->fx;
    *out = 0;
    if (c->phase == 0u) {
        PRIMARY(2); fx->state.orbit.radius = fx->state.orbit.started = 0; *out = 7;
    } else if (c->phase == 3u) {
        if (!fx->state.orbit.started) {
            for (size_t i = 0; i < 5u; i++) { fx->state.orbit.age[i] = -(int32_t)i; fx->state.orbit.spark[i] = 0; }
            int32_t old = fx->state.orbit.x[2];
            fx->state.orbit.x[2] = fx->state.orbit.x[4]; fx->state.orbit.x[4] = old;
            fx->state.orbit.y[2] = fx->state.orbit.y[4];
            fx->state.orbit.y[4] = fx->state.orbit.radius; /* 原2c76d..2c77e，并非另一对称swap。 */
            fx->state.orbit.started = 1;
        }
        *out = 12;
    } else if (c->phase == 6u) { PRIMARY(3); fx->state.orbit.radius = 42; *out = 7; }
    else if (c->phase == 1u || c->phase == 2u || c->phase == 7u || c->phase == 8u) {
        for (size_t i = 0; i < 5u; i++) {
            double angle = (double)(72u*i) * (double)0x1.1df45ap-6f;
            volatile double px = cos(angle) * fx->state.orbit.radius;
            volatile double py = sin(angle) * fx->state.orbit.radius;
            px += fx->side ? 30.0 : 90.0; py *= 1.2; py += 30.0;
            fx->state.orbit.x[i] = (int32_t)px; fx->state.orbit.y[i] = (int32_t)py;
            int late = c->phase == 2u || c->phase == 8u;
            if ((!fx->side && late) || (fx->side && ((late && i > 1u) || (!late && i < 2u))))
                DRAW(4, fx->state.orbit.x[i], fx->state.orbit.y[i]);
        }
        if (c->phase == 2u) fx->state.orbit.radius = (uint8_t)(fx->state.orbit.radius + 6u);
        if (c->phase == 8u) fx->state.orbit.radius = (uint8_t)(fx->state.orbit.radius - 6u);
    } else if (c->phase == 4u || c->phase == 5u) {
        for (size_t i = 0; i < 5u; i++) {
            fx->state.orbit.radius = fx->state.orbit.age[i] < 0 ? 4 : (uint8_t)fx->state.orbit.age[i];
            uint8_t part = fx->state.orbit.radius;
            if (part >= 5u) return -1;
            if (part == 1u) fx->state.orbit.spark[i] = 5;
            int late = c->phase == 5u;
            if ((!fx->side && late) || (fx->side && ((late && i > 1u) || (!late && i < 2u)))) {
                int32_t ox = !fx->side && part < 3u ? -k_fx_orbit_dx[part] : k_fx_orbit_dx[part];
                int32_t oy = !fx->side && part < 3u ? 0 : k_fx_orbit_dy[part];
                DRAW(part, fx->state.orbit.x[i] + ox, fx->state.orbit.y[i] + oy);
            }
        }
        if (c->phase == 5u) {
            for (size_t i = 0; i < 5u; i++) {
                if (fx->state.orbit.spark[i]) {
                    DRAW(fx->state.orbit.spark[i], fx->state.orbit.x[i] - 60, fx->state.orbit.y[i] - 20);
                    if (++fx->state.orbit.spark[i] == 10u) fx->state.orbit.spark[i] = 0;
                }
                if (!fx->state.orbit.age[i]) {
                    if (i == 0u || i == 2u) PRIMARY(1); else SECONDARY(1);
                }
                fx->state.orbit.age[i] = next_age(fx->state.orbit.age[i]);
                if (fx->state.orbit.age[i] == 5) fx->state.orbit.age[i] = 0;
                if (fx->state.orbit.age[i] == 2) *out = 1;
            }
        }
    }
    return 0;
}

static int surge(fx_call *c, int *out) {
    fd2_magic_fx *fx = c->fx;
    int32_t *age = fx->state.particles.age, *slot = fx->state.particles.slot;
    *out = 0;
    if (c->phase == 0u) {
        for (size_t i = 0; i < 4u; i++) { age[i] = -(int32_t)(3u*i); slot[i] = (int32_t)i; }
        fx->state.particles.next = 4; fx->state.particles.stop = fx->state.particles.flip = 0; *out = 2;
    } else if (c->phase == 3u) *out = 32;
    else if (c->phase == 6u) { fx->state.particles.stop = 1; *out = 16; }
    else if (c->phase == 2u || c->phase == 5u || c->phase == 8u) {
        fx->state.particles.flip = (uint8_t)((fx->state.particles.flip + 1u) % 2u);
        for (size_t i = 0; i < 3u; i++) {
            int32_t at = slot[i]; if (at < 0 || at >= 10) return -1;
            if (age[i] >= 0 && age[i] <= 4) DRAW(age[i], k_fx_surge_x[at] + (fx->side ? 0 : 130), 0);
            if (!fx->state.particles.flip) {
                if (age[i] == 1) { if (!i) PRIMARY(1); else if (i == 1u) SECONDARY(1); }
                age[i] = next_age(age[i]);
                if (age[i] == 2) *out = 1;
                if (age[i] == 7 && !fx->state.particles.stop) {
                    fx->state.particles.next = (uint8_t)((fx->state.particles.next + 1u) % 10u);
                    slot[i] = fx->state.particles.next; age[i] = 0;
                }
            }
        }
    }
    return 0;
}
static int burst(fx_call *c, int *out) {
    int32_t *age = c->fx->state.burst.age;
    *out = 0;
    if (c->phase == 0u) { for (size_t i = 0; i < 16u; i++) age[i] = -(int32_t)(2u*i); *out = 3; }
    else if (c->phase == 3u) *out = 34;
    else if (c->phase == 6u) *out = 2;
    else if (c->phase == 2u || c->phase == 5u) {
        for (size_t i = 0; i < 16u; i++) {
            if (age[i] >= 0 && age[i] < 8) DRAW(age[i] + k_fx_burst_offsets[i], 0, 0);
            if (!age[i]) PRIMARY(1);
            if (age[i] == 4) SECONDARY(2);
            age[i] = next_age(age[i]);
            if (age[i] == 4) *out = 1;
        }
    }
    return 0;
}
static int beam(fx_call *c, int *out) {
    fd2_magic_fx *fx = c->fx;
    *out = 0;
    switch (c->phase) {
    case 0: fx->state.beam.frame = 1; fx->state.beam.blink = 0; *out = 20; break;
    case 1: case 7:
        if (!fx->state.beam.blink) DRAW(0, 0, 0);
        fx->state.beam.blink ^= 1u; break;
    case 3: *out = 60; break;
    case 4: DRAW(0, 0, 0); break;
    case 5:
        /* sub_2CE1A @0x2ce1a / 31266：BYTE跨target续增。
         * FDOTHER44 part31的目录读仍在资源内，但offset30产生y12546，
         * 原首RUN1写work+8048320，越过172800B分配及allocator slack。
         * 从该首越owner点省绘属H，非原像素等值；保cue/回绕/impact。
         * part0..30的源/目标extent另核；其它有效DRAW错误仍传播。 */
        if ((size_t)(fx->state.beam.frame >> 1) < fx->parts.part_count)
            DRAW(fx->state.beam.frame >> 1, 0, 0);
        if (fx->state.beam.frame == 6u) PRIMARY(1);
        else if (fx->state.beam.frame == 36u) SECONDARY(2);
        fx->state.beam.frame++;
        *out = fx->state.beam.frame < 44u && fx->state.beam.frame > 16u;
        break;
    case 6: *out = 20; break;
    }
    return 0;
}

int fd2_magic_fx_step(fd2_magic_fx *fx, uint8_t phase,
        fd2_magic_fx_rng_fn rng, void *rng_userdata,
        fd2_magic_fx_emit_fn emit, void *emit_userdata, int *out) {
    fd2_magic_fx_parameters parameters;
    if (!fx || !out || phase > 8u ||
        fd2_magic_fx_parameters_get(fx->id, fx->side, &parameters) != 0 ||
        !fx->parts.part_durations || fx->parts.part_count < parameters.minimum_part_count ||
        ((fx->id == 4u || fx->id == 5u) && !rng)) return -1;
    fx_call call = {fx,phase,rng,rng_userdata,emit,emit_userdata};
    int result = 0, status;
    switch (fx->id) {
    case 0: case 1: status = rain(&call, &result); break;
    case 2: status = wheel(&call, &result); break;
    case 3: status = shower(&call, &result); break;
    case 4: case 5: status = dust(&call, &result); break;
    case 6: status = orbit(&call, &result); break;
    case 7: status = surge(&call, &result); break;
    case 8: status = burst(&call, &result); break;
    case 9: status = beam(&call, &result); break;
    default: return -1;
    }
    if (status != 0) return -1;
    *out = result;
    return 0;
}
