/* 炎龙骑士团 2 SDL3 重写 - Miles XMIDI / AIL AdLib BGM source
 *
 * 逆向依据：music_track_play @0x25977（原登记 FUN_0004ab8b）从 FDMUS.DAT 取 XMIDI，
 * 初始化单一 sequence；loop_count 0 表示循环，track -1 表示停止。
 * new_game_opening_play @0x3231b（原登记 FUN_0002fa63）内
 * canonical 0x32413..0x32417 明确调用 track 11。
 *
 * XMIDI 时序按已确认原EVNT/32 duration槽执行，原资源只读借用。
 * 转换后的 MIDI note-off 不能保留原32槽/同tick EOT与fade顺序，故仅替换事件调度；
 * BW/XMIDI converter 保留为历史依赖，不进入本运行路径。
 * OPL 寄存器合成沿用 Nuked OPL3（保留其原作者和许可）。
 * 本文件实现已配置 SBPRO2.MDI 的 SAMPLE14 双算子分支；音色字节直接读取
 * 原版 SAMPLE.AD，不生成或提交转换资产。
 *
 * driver 依据为 SBPRO2.MDI 的 16 位文件偏移，不是 FD2 VA：
 * SHA256: 2aa48568783f6c41df8dfb7e699181299e306fa330415a178fc48b23036c9a77
 * 3085/36f6 分配，3184 寄存器，3b54 控制分派，39cc/3b1e 延迟释放。
 * FD2 ail_sequence_volume_fade_set @0x449e0（原登记 sub_449E0） / ail_sequence_volume_controls_refresh @0x43230（原登记 sub_43230） / ail_sequence_midi_message_dispatch @0x42980（原登记 sub_42980）
 * / ail_mdi_sequences_timer_service @0x43270（原登记 sub_43270）：整数 sequence volume 与原 CC7 重发；不做后混渐变。
 * 只复现本 bank 全部 14B kind0；25B/四算子不按常识补齐。
 * 静态包：ida-domain-a7-voice-readonly-m_v_tyt4；普通算子表更正：
 * a7-sbpro2-parameter-correction-20261107-c1。实际 loaded-driver/OPL/WAV 待验。
 */

#include "bgm.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "third_party/nukedopl3.h"
#include "third_party/opl_models.h"

namespace {

static_assert(ATOMIC_INT_LOCK_FREE == 2,
              "BGM audio-thread state requires lock-free atomics");

constexpr size_t kChannels = 18;
constexpr size_t kLogicalVoices = 20;
constexpr uint8_t kUnassigned = 0xff;
constexpr size_t kMidiChannels = 16;
constexpr size_t kMaxInstruments = 256;
constexpr uint32_t kMagic = 0x46444247u; /* FDBG */

static const uint8_t kOperatorOffsets[9] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12,
};

struct AilInstrument {
    bool present = false;
    int8_t note_offset = 0;
    uint8_t mod_20 = 0;
    uint8_t mod_40 = 0;
    uint8_t mod_60 = 0;
    uint8_t mod_80 = 0;
    uint8_t mod_e0 = 0;
    uint8_t car_20 = 0;
    uint8_t car_40 = 0;
    uint8_t car_60 = 0;
    uint8_t car_80 = 0;
    uint8_t car_e0 = 0;
    uint8_t feedback = 0;
};

struct Voice {
    bool active = false;
    bool deferred = false;
    uint8_t midi_channel = 0;
    uint8_t note = 0;       /* 14e9：原 MIDI note，用于 note-off 匹配。 */
    uint8_t tone = 0;       /* 14d5：打击乐为音色内固定 note。 */
    int8_t note_offset = 0;
    uint8_t velocity = 0;
    uint16_t instrument = 0;
    uint8_t opl_channel = kUnassigned;
    uint8_t b0 = 0;
};

/* ail_mdi_sequences_timer_service @0x43270（原登记 sub_43270）：32 duration 槽是唯一到期真值。
 * 保留槽顺序/重复key；不从转换后的MIDI note-off重建。 */
struct DurationSlot {
    int channel = -1;
    uint8_t note = 0;
    uint32_t remaining = 0;
};
struct NativeSequence {
    const uint8_t *evnt = nullptr; /* 借用player的immutable FDMUS。 */
    size_t size = 0;
    size_t cursor = 0;
    uint32_t delay = 0;
    uint32_t loop_remaining = 1;
    uint32_t pending_count = 0;
    std::array<DurationSlot, 32> pending{};
    uint8_t status = 2; /* 原2 ended、4 playing、8 stopped。 */
    bool queue_overflow = false;
};
struct NativeEvent {
    uint8_t status = 0, a = 0, b = 0, meta = 0;
    uint32_t duration = 0;
    size_t next = 0;
};

struct BgmVoice;

/* 唯一 MDI runtime：创建时安装，随后只由音频执行/退休路径访问。
 * sequence 的 prepare/load/失败提交均不得写这里。 */
struct AilDriver {
    int sample_rate = 48000;
    size_t voice_count = kChannels;
    std::array<AilInstrument, kMaxInstruments> instruments{};
    std::array<Voice, kLogicalVoices> voices{};
    std::array<uint8_t, kChannels> hardware_channels{};
    std::array<uint8_t, kMidiChannels> assigned_counts{};
    int last_free_channel = -1;
    std::array<int16_t, kMidiChannels> selected_instruments{};
    std::array<uint8_t, kMidiChannels> pedals{};
    std::array<uint8_t, kMidiChannels> priorities{};
    std::array<uint8_t, kMidiChannels> modulations{};
    std::array<uint8_t, kMidiChannels> pitch_ranges{};
    std::array<uint8_t, kMidiChannels> patches{};
    std::array<uint8_t, kMidiChannels> banks{};
    std::array<uint8_t, kMidiChannels> volumes{};
    std::array<uint8_t, kMidiChannels> expressions{};
    std::array<uint8_t, kMidiChannels> pans{};
    std::array<uint16_t, kMidiChannels> bends{};
    opl3_chip opl{};
    BgmVoice *active_sequence = nullptr;
    /* sub_43AD0 @0x43ad0（原同名登记）：请求120Hz；每callback给fade加
     * floor(1000000/120) us。sample相位与整数fade余量分别保留。 */
    uint64_t timer_sample_units = 0;
};

struct BgmVoice {
    std::atomic<int> state{0}; /* 0 free, 1 active, 2 retired */
    uint32_t magic = kMagic;
    int track = -1;
    int loop_count = 0;
    int sample_rate = 48000;
    AilDriver *driver = nullptr; /* 借用 player；audio 先销毁。 */
    NativeSequence sequence;
    std::array<int16_t, kMidiChannels> pedal_cache{};
    std::array<int16_t, kMidiChannels> priority_cache{};
    bool started = false;
    std::atomic<int> fade_request{-1}; /* main发布target|(duration_ms<<8)，audio独占执行。 */
    bool music_enabled = true;
    std::array<int16_t, kMidiChannels> volume_cache{}; /* 43160：-1 表示尚无原 CC7。 */
    std::array<uint8_t, kMidiChannels> channel_map{};  /* 43160：初始恒等映射。 */
    int sequence_volume = 127;
    int target_volume = 127;
    uint32_t fade_accumulator_us = 0;
    uint32_t fade_interval_us = 0;
    uint32_t timer_ticks = 0;
    bool start_fade = false;
};

struct fd2_bgm_player_impl {
    fd2_audio *audio = nullptr;
    fd2_archive fdmus{};
    AilDriver driver;
    BgmVoice voices[2];
    int current_track = -1;
    bool music_enabled = true; /* 51E61配置投影；不从PCM bus反推。 */
};

static uint16_t rd16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static uint32_t be32(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
static bool find_evnt(const uint8_t *data, size_t begin, size_t end,
                      unsigned depth, NativeSequence &out, unsigned &found) {
    if (depth > 8) return false;
    size_t at = begin;
    while (at < end) {
        if (end - at < 8) return false;
        uint32_t size = be32(data + at + 4);
        if (size > end - at - 8) return false;
        size_t stop = at + 8 + size;
        if (!std::memcmp(data + at, "EVNT", 4)) {
            if (++found != 1) return false;
            out.evnt = data + at + 8; out.size = size;
        } else if (!std::memcmp(data + at, "FORM", 4) || !std::memcmp(data + at, "CAT ", 4) ||
                   !std::memcmp(data + at, "LIST", 4)) {
            if (size < 4 || !find_evnt(data, at + 12, stop, depth + 1, out, found)) return false;
        }
        at = stop + (size & 1u);
        if (at > end) return false;
    }
    return at == end;
}
/* ail_xmidi_vlq_read @0x424b0（原登记 sub_424B0）的原VLQ。当前资源至多四字节；
 * 更长编码主线程明确拒绝，不猜回绕后继续。 */
static bool native_vlq(const uint8_t *data, size_t size, size_t &at, uint32_t &value) {
    value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (at >= size) return false;
        uint8_t byte = data[at++]; value = (value << 7) | (byte & 127u);
        if (!(byte & 128u)) return true;
    }
    return false;
}
static bool decode_native_event(const NativeSequence &seq, size_t at, NativeEvent &out) {
    if (at >= seq.size) return false;
    NativeEvent e; e.status = seq.evnt[at++];
    unsigned kind = e.status >> 4;
    if (e.status < 128) { e.next = at; out = e; return true; }
    if (kind == 8 || kind == 9 || kind == 11 || kind == 14) {
        if (seq.size - at < 2) return false;
        e.a = seq.evnt[at++]; e.b = seq.evnt[at++];
        if (e.a >= 128 || e.b >= 128) return false;
        if (kind == 9 && !native_vlq(seq.evnt, seq.size, at, e.duration)) return false;
        if (kind == 11 && e.a != 1 && e.a != 7 && e.a != 64) return false;
    } else if (kind == 12) {
        if (at == seq.size || (e.a = seq.evnt[at++]) >= 128) return false;
    } else if (e.status == 0xff) {
        if (at == seq.size) return false;
        e.meta = seq.evnt[at++]; uint32_t size = 0;
        if (!native_vlq(seq.evnt, seq.size, at, size) || size > seq.size - at) return false;
        if ((e.meta == 0x51 && size < 3) || (e.meta == 0x58 && size < 2) ||
            (e.meta == 0x2f && size != 0)) return false;
        at += size; /* 非EOT meta无音符/渐变副作用，保留原字节。 */
    } else return false; /* 未证语法不落BW fallback。 */
    e.next = at; out = e; return true;
}
static bool prepare_native_sequence(NativeSequence &out, const uint8_t *data, size_t size) {
    NativeSequence seq; unsigned found = 0;
    if (size < 12 || std::memcmp(data, "FORM", 4) ||
        !find_evnt(data, 0, size, 0, seq, found) || found != 1 || !seq.size) return false;
    size_t at = 0; bool eot = false;
    while (at < seq.size) {
        NativeEvent e;
        if (!decode_native_event(seq, at, e)) return false;
        at = e.next; eot = e.status == 0xff && e.meta == 0x2f;
        if (eot) break; /* 原EOT后的span/尾字节仍借用保留，但不解释执行。 */
    }
    if (!eot) return false;
    out = seq; return true;
}

static int instrument_index(uint8_t patch, uint8_t bank) {
    if (bank == 0) return patch;
    if (bank == 0x7f && patch >= 35 && patch <= 75)
        return 128 + patch - 35;
    return -1;
}

static bool load_ail_bank(const char *path,
                          std::array<AilInstrument, kMaxInstruments> &out) {
    FILE *file = path ? std::fopen(path, "rb") : nullptr;
    if (!file) return false;
    std::fseek(file, 0, SEEK_END);
    long length = std::ftell(file);
    std::rewind(file);
    if (length <= 0) {
        std::fclose(file);
        return false;
    }
    std::vector<uint8_t> data(static_cast<size_t>(length));
    bool ok = std::fread(data.data(), 1, data.size(), file) == data.size();
    std::fclose(file);
    if (!ok) return false;

    size_t pos = 0;
    size_t count = 0;
    while (pos + 6 <= data.size()) {
        uint8_t patch = data[pos];
        uint8_t bank = data[pos + 1];
        uint32_t offset = rd32(data.data() + pos + 2);
        pos += 6;
        if (patch == 0xff || bank == 0xff) break;
        if (offset > data.size() || data.size() - offset < 14 || rd16(data.data() + offset) != 14)
            return false;
        int index = instrument_index(patch, bank);
        if (index < 0 || static_cast<size_t>(index) >= out.size()) continue;
        const uint8_t *raw = data.data() + offset;
        AilInstrument &inst = out[static_cast<size_t>(index)];
        inst.present = true;
        inst.note_offset = static_cast<int8_t>(raw[2]);
        /* Miles AIL payload: len,note, then 20/40/60/80/E0/C0,
         * skipped byte, then carrier 20/40/60/80/E0. */
        inst.mod_20 = raw[3]; inst.mod_40 = raw[4];
        inst.mod_60 = raw[5]; inst.mod_80 = raw[6]; inst.mod_e0 = raw[7];
        inst.feedback = raw[8];
        inst.car_20 = raw[9]; inst.car_40 = raw[10];
        inst.car_60 = raw[11]; inst.car_80 = raw[12]; inst.car_e0 = raw[13];
        count++;
    }
    return count >= 128;
}

static const AilInstrument &voice_instrument(const AilDriver *voice,
                                             const Voice &slot) {
    return voice->instruments[slot.instrument];
}

/* C37/C49 -> C5B/C7F、CA3/CB5；CEB/CFD 是四算子恢复表，不能代替。 */
static uint16_t channel_register(uint16_t reg, uint8_t channel) {
    return static_cast<uint16_t>(reg + channel % 9 + (channel >= 9 ? 0x100 : 0));
}

static uint16_t operator_register(uint16_t reg, uint8_t channel) {
    return static_cast<uint16_t>(reg + kOperatorOffsets[channel % 9] +
                                 (channel >= 9 ? 0x100 : 0));
}

static uint8_t scaled_level(uint8_t reg40, uint8_t velocity,
                            uint8_t volume, uint8_t expression,
                            bool scale_operator) {
    OPLVolume_t context{};
    context.vel = velocity;
    context.chVol = volume;
    context.chExpr = expression;
    context.masterVolume = 127;
    context.voiceMode = OPLVoice_MODE_2op_FM;
    context.tlMod = reg40 & 0x3fu;
    context.tlCar = reg40 & 0x3fu;
    context.doMod = scale_operator ? 1u : 0u;
    context.doCar = scale_operator ? 1u : 0u;
    oplModel_ailVolume(&context);
    return static_cast<uint8_t>((reg40 & 0xc0u) |
        (scale_operator ? context.tlCar : (reg40 & 0x3fu)));
}

/* x86 SAR，显式保留负值向负无穷取整，不依赖宿主有符号右移。 */
static int sar(int value, int bits) {
    return value >= 0 ? value / (1 << bits)
                      : -((-value + (1 << bits) - 1) / (1 << bits));
}

/* 35fa..36a9：AX 低 16 位回绕；音符与 bend 均按原循环折回硬件范围。 */
static uint16_t ail_frequency(const Voice &slot, uint16_t bend, uint8_t range) {
    int pitch = static_cast<int16_t>(sar(static_cast<int>(bend) - 8192, 5) * range);
    int note = static_cast<int>(slot.tone) + slot.note_offset - 12;
    while (note < 0) note += 12;
    while (note > 95) note -= 12;
    uint16_t ax = static_cast<uint16_t>(pitch);
    ax = static_cast<uint16_t>((ax & 0xffu) |
         (((static_cast<unsigned>(ax >> 8) + note) & 0xffu) << 8));
    pitch = sar(static_cast<int16_t>(static_cast<uint16_t>(ax + 8)), 4);
    while (pitch < 0) pitch += 192;
    while (pitch > 1535) pitch -= 192;
    /* 复用项目现有 AIL 表。模型最低折回到八度 1；原 driver 允许 0..11，
     * 因此只在该区间还原原 3689..369b 的一次半频/八度步骤。 */
    uint32_t unused = 0;
    uint16_t value = oplModel_ailFreq((pitch < 192 ? pitch + 192 : pitch) / 16.0 + 12.0,
                                     &unused);
    if (pitch < 192) {
        if (value & 0x1c00u) value = static_cast<uint16_t>(value - 0x400u);
        else value = static_cast<uint16_t>((value & 0x3ffu) >> 1);
    }
    return value;
}

static void opl_update_voice(AilDriver *voice, Voice &slot, uint8_t dirty) {
    if (slot.opl_channel == kUnassigned) return;
    const AilInstrument &inst = voice_instrument(voice, slot);
    const uint8_t channel = slot.midi_channel;
    const uint16_t op = operator_register(0, slot.opl_channel);
    if (dirty & 0x80u) {
        uint8_t vibrato = voice->modulations[channel] >= 64 ? 0x40 : 0;
        OPL3_WriteReg(&voice->opl, 0x20u + op, inst.mod_20 | vibrato);
        OPL3_WriteReg(&voice->opl, 0x23u + op, inst.car_20 | vibrato);
    }
    if (dirty & 0x40u) {
        OPL3_WriteReg(&voice->opl, 0x40u + op,
            scaled_level(inst.mod_40, slot.velocity, voice->volumes[channel],
                         voice->expressions[channel], (inst.feedback & 1u) != 0));
        OPL3_WriteReg(&voice->opl, 0x43u + op,
            scaled_level(inst.car_40, slot.velocity, voice->volumes[channel],
                         voice->expressions[channel], true));
    }
    if (dirty & 0x20u) {
        OPL3_WriteReg(&voice->opl, 0x60u + op, inst.mod_60);
        OPL3_WriteReg(&voice->opl, 0x63u + op, inst.car_60);
        OPL3_WriteReg(&voice->opl, 0x80u + op, inst.mod_80);
        OPL3_WriteReg(&voice->opl, 0x83u + op, inst.car_80);
    }
    if (dirty & 0x10u) {
        OPL3_WriteReg(&voice->opl, 0xe3u + op, inst.car_e0);
        OPL3_WriteReg(&voice->opl, 0xe0u + op, inst.mod_e0);
    }
    if (dirty & 0x08u) {
        uint8_t routing = static_cast<uint8_t>((inst.feedback & 0x0fu) | 0x30u);
        if (voice->pans[channel] <= 27) routing &= 0xef;
        else if (voice->pans[channel] >= 100) routing &= 0xdf;
        OPL3_WriteReg(&voice->opl, channel_register(0xc0, slot.opl_channel), routing);
    }
    if (dirty & 0x01u) {
        uint16_t value = ail_frequency(slot, voice->bends[channel], voice->pitch_ranges[channel]);
        OPL3_WriteReg(&voice->opl, channel_register(0xa0, slot.opl_channel),
                      static_cast<uint8_t>(value));
        slot.b0 = static_cast<uint8_t>(((value >> 8) & 0x1fu) | 0x20u);
        OPL3_WriteReg(&voice->opl, channel_register(0xb0, slot.opl_channel), slot.b0);
    }
}

/* 3127：未分配硬件时直接返回，不能顺便清掉原逻辑槽。 */
static void release_voice(AilDriver *voice, Voice &slot) {
    if (slot.opl_channel == kUnassigned) return;
    OPL3_WriteReg(&voice->opl, channel_register(0xb0, slot.opl_channel), slot.b0 & 0xdfu);
    voice->assigned_counts[slot.midi_channel]--;
    voice->hardware_channels[slot.opl_channel] = kUnassigned;
    slot.opl_channel = kUnassigned;
    slot.active = false;
}

static void assign_voice(AilDriver *voice, Voice &slot, uint8_t hardware) {
    slot.opl_channel = hardware;
    voice->hardware_channels[hardware] = slot.midi_channel;
    voice->assigned_counts[slot.midi_channel]++;
    opl_update_voice(voice, slot, 0xf9);
}

static void allocate_hardware(AilDriver *voice, Voice &slot) {
    for (size_t n = 0; n < voice->voice_count; n++) {
        voice->last_free_channel = (voice->last_free_channel + 1) %
                                   static_cast<int>(voice->voice_count);
        if (voice->hardware_channels[voice->last_free_channel] == kUnassigned) {
            assign_voice(voice, slot, static_cast<uint8_t>(voice->last_free_channel));
            return;
        }
    }
    /* 36f6：快照只算一次；平分取最后一个逻辑槽，不按 age 抢最旧。 */
    std::array<uint16_t, kLogicalVoices> scores{};
    size_t budget = 0;
    for (size_t i = 0; i < voice->voices.size(); i++) {
        const Voice &candidate = voice->voices[i];
        if (!candidate.active) continue;
        budget++;
        uint16_t base = voice->priorities[candidate.midi_channel] >= 64 ? 0xffff : 0x7fff;
        scores[i] = static_cast<uint16_t>(base - voice->assigned_counts[candidate.midi_channel]);
    }
    while (budget-- > 0) {
        int incoming = -1, victim = -1;
        uint16_t maximum = 0, minimum = 0xffff;
        for (size_t i = 0; i < voice->voices.size(); i++) {
            const Voice &candidate = voice->voices[i];
            if (!candidate.active) continue;
            if (candidate.opl_channel == kUnassigned && scores[i] >= maximum) {
                maximum = scores[i]; incoming = static_cast<int>(i);
            } else if (candidate.opl_channel != kUnassigned && scores[i] <= minimum) {
                minimum = scores[i]; victim = static_cast<int>(i);
            }
        }
        if (incoming < 0 || victim < 0 || maximum == 0 || maximum < minimum) return;
        uint8_t hardware = voice->voices[victim].opl_channel;
        release_voice(voice, voice->voices[victim]);
        assign_voice(voice, voice->voices[incoming], hardware);
    }
}

static void driver_note_off(void *userdata, uint8_t channel, uint8_t note) {
    AilDriver *voice = static_cast<AilDriver *>(userdata);
    if (channel >= kMidiChannels) return;
    for (Voice &slot : voice->voices) {
        if (!slot.active || slot.midi_channel != channel || slot.note != note) continue;
        if (voice->pedals[channel] >= 64) slot.deferred = true;
        else release_voice(voice, slot);
    }
}

static void driver_note_on(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity) {
    AilDriver *voice = static_cast<AilDriver *>(userdata);
    if (channel >= kMidiChannels || note >= 128 || velocity >= 128) return;
    if (velocity == 0) { driver_note_off(userdata, channel, note); return; }
    int index = channel == 9 ? instrument_index(note, 0x7f) : voice->selected_instruments[channel];
    if (index < 0 || !voice->instruments[static_cast<size_t>(index)].present) return;
    for (Voice &slot : voice->voices) {
        if (slot.active) continue;
        slot = Voice{}; /* 3ae8：复用槽必须清 deferred。 */
        slot.active = true;
        slot.midi_channel = channel;
        slot.note = note;
        slot.instrument = static_cast<uint16_t>(index);
        slot.velocity = velocity;
        const AilInstrument &inst = voice_instrument(voice, slot);
        slot.tone = channel == 9 ? static_cast<uint8_t>(inst.note_offset) : note;
        slot.note_offset = channel == 9 ? 0 : inst.note_offset;
        allocate_hardware(voice, slot);
        return;
    }
}

/* 3b1e/3c9a：重入 note-off 时传 sounding note，不改为逐实例释放。
 * 打击乐 raw note 与固定 note 不同的原行为也保留。 */
static void flush_deferred(AilDriver *voice, uint8_t channel) {
    for (Voice &slot : voice->voices)
        if (slot.active && slot.midi_channel == channel && slot.deferred)
            driver_note_off(voice, channel, slot.tone);
}

static void driver_controller(void *userdata, uint8_t channel, uint8_t type, uint8_t value) {
    AilDriver *voice = static_cast<AilDriver *>(userdata);
    if (channel >= kMidiChannels || value >= 128) return;
    uint8_t dirty = 0;
    switch (type) {
    case 1: voice->modulations[channel] = value; dirty = 0x80; break;
    case 6: voice->pitch_ranges[channel] = value; break; /* 原写值后不立即重调。 */
    case 7: voice->volumes[channel] = value; dirty = 0x40; break;
    case 10: voice->pans[channel] = value; dirty = 0x08; break;
    case 11: voice->expressions[channel] = value; dirty = 0x40; break;
    case 64:
        voice->pedals[channel] = value;
        if (value < 64) flush_deferred(voice, channel);
        break;
    case 112: voice->priorities[channel] = value; break;
    case 114: voice->banks[channel] = value; break;
    case 121:
        voice->pedals[channel] = 0;
        flush_deferred(voice, channel);
        voice->modulations[channel] = 0;
        voice->expressions[channel] = 127;
        voice->bends[channel] = 8192;
        dirty = 0xc1;
        break;
    case 123:
        for (Voice &slot : voice->voices)
            if (slot.active && slot.midi_channel == channel)
                driver_note_off(voice, channel, slot.tone);
        break;
    default: break; /* 原3b54分派忽略CC0/32/120，不补GM控制行为。 */
    }
    if (dirty)
        for (Voice &slot : voice->voices)
            if (slot.active && slot.midi_channel == channel) opl_update_voice(voice, slot, dirty);
}

static void driver_patch(void *userdata, uint8_t channel, uint8_t patch) {
    AilDriver *voice = static_cast<AilDriver *>(userdata);
    if (channel >= kMidiChannels || patch >= 128) return;
    voice->patches[channel] = patch;
    int index = instrument_index(patch, voice->banks[channel]);
    voice->selected_instruments[channel] = static_cast<int16_t>(index >= 0 &&
        voice->instruments[static_cast<size_t>(index)].present ? index : -1);
}

static void driver_bend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb) {
    AilDriver *voice = static_cast<AilDriver *>(userdata);
    if (channel >= kMidiChannels || msb >= 128 || lsb >= 128) return;
    voice->bends[channel] = static_cast<uint16_t>((msb << 7) | lsb);
    for (Voice &slot : voice->voices)
        if (slot.active && slot.midi_channel == channel) opl_update_voice(voice, slot, 1);
}

static void reset_driver(AilDriver *voice) {
    voice->active_sequence = nullptr;
    voice->timer_sample_units = 0;
    voice->voices = {};
    voice->hardware_channels.fill(kUnassigned);
    voice->assigned_counts.fill(0);
    voice->last_free_channel = -1;
    voice->selected_instruments.fill(-1);
    voice->patches.fill(0xff);
    voice->banks.fill(0);
    voice->pedals.fill(0);
    voice->priorities.fill(0);
    voice->modulations.fill(0);
    /* sub_3FCA9 @0x3fca9 经 prefs12=127/prefs15=2；sub_43AD0
     * @0x43ad0 安装时发 CC7/10/6=127/64/2。换曲不再调用此函数。 */
    voice->volumes.fill(127);
    voice->expressions.fill(127);
    voice->pans.fill(64);
    voice->pitch_ranges.fill(2);
    voice->bends.fill(8192);
    OPL3_Reset(&voice->opl, static_cast<uint32_t>(voice->sample_rate));
    /* 2c47：OPL3 enable/4op clear 后按原两个 bank 初值清寄存器。 */
    OPL3_WriteReg(&voice->opl, 0x105, 1);
    OPL3_WriteReg(&voice->opl, 0x104, 0);
    for (unsigned bank = 0; bank < 2; bank++) {
        for (unsigned reg = 1; reg <= 0xf5; reg++) {
            uint8_t value = 0;
            if (reg >= 0x20 && reg <= 0x35) value = 1;
            else if (reg >= 0x40 && reg <= 0x55) value = 0x3f;
            else if (reg >= 0x60 && reg <= 0x75) value = 0xff;
            else if (reg >= 0x80 && reg <= 0x95) value = 0x0f;
            else if (bank == 0 && reg == 1) value = 0x20;
            else if (bank == 0 && reg == 4) value = 0x60;
            else if (bank == 0 && reg == 0xbd) value = 0xc0;
            else if (bank == 1 && reg == 5) value = 1;
            OPL3_WriteReg(&voice->opl, static_cast<uint16_t>((bank << 8) | reg), value);
        }
    }
}

/* 主线程只清自己的解析/事件缓存，不读写正在播放的 driver。 */
static void prepare_sequence(BgmVoice *voice) {
    voice->pedal_cache.fill(-1);
    voice->priority_cache.fill(-1);
    voice->volume_cache.fill(-1);
    for (size_t i = 0; i < kMidiChannels; ++i)
        voice->channel_map[i] = static_cast<uint8_t>(i);
    voice->sequence_volume = voice->target_volume = 127;
    voice->fade_accumulator_us = voice->fade_interval_us = 0;
    voice->timer_ticks = 0;
    voice->start_fade = voice->track >= 0 && voice->track != 16 && voice->track != 17;
    voice->sequence = NativeSequence{};
    voice->started = false;
}

static void set_sequence_volume(BgmVoice *voice, int value, uint32_t milliseconds);

static bool begin_sequence(BgmVoice *voice) {
    if (!voice->driver || (voice->driver->active_sequence &&
                          voice->driver->active_sequence != voice)) return false;
    voice->driver->active_sequence = voice;
    voice->started = true;
    voice->sequence.status = 4;
    if (!voice->music_enabled) {
        set_sequence_volume(voice, 0, 0);
    } else if (voice->start_fade) {
        set_sequence_volume(voice, 0, 0);
        set_sequence_volume(voice, 127, 2000);
    }
    return true;
}

static bool sequence_owns_driver(const BgmVoice *voice) {
    return voice && voice->driver && voice->driver->active_sequence == voice;
}

#ifndef FD2_BGM_NATIVE_TRACE
#define FD2_BGM_NATIVE_TRACE(voice, status, a, b) ((void)0)
#endif
static void post_driver_message(BgmVoice *voice, uint8_t status, uint8_t a, uint8_t b) {
    FD2_BGM_NATIVE_TRACE(voice, status, a, b);
    uint8_t channel = status & 15u;
    switch (status & 0xf0u) {
    case 0x80: driver_note_off(voice->driver, channel, a); break;
    case 0x90: driver_note_on(voice->driver, channel, a, b); break;
    case 0xb0: driver_controller(voice->driver, channel, a, b); break;
    case 0xc0: driver_patch(voice->driver, channel, a); break;
    case 0xe0: driver_bend(voice->driver, channel, b, a); break;
    }
}
static void stop_sequence_events(BgmVoice *voice);
static bool register_duration(BgmVoice *voice, uint8_t channel, uint8_t note, uint32_t duration);

/* ail_sequence_midi_message_dispatch @0x42980（原登记 sub_42980）：原 CC7 先缓存，再乘 sequence volume，
 * 经整数除 127 后转发到映射的 MDI channel，不改变缓存原值。 */
static uint8_t scaled_sequence_volume(const BgmVoice *voice, int raw) {
    int value = raw * voice->sequence_volume / 127;
    return static_cast<uint8_t>(std::max(0, std::min(127, value)));
}

/* ail_sequence_volume_controls_refresh @0x43230（原登记 sub_43230）：只重发已缓存的逻辑 channel；
 * -1 跳过，不给未见 CC7 的 channel 合成默认控制消息。 */
static void resend_sequence_volume(BgmVoice *voice) {
    if (!sequence_owns_driver(voice)) return;
    for (size_t logical = 0; logical < kMidiChannels; ++logical) {
        int raw = voice->volume_cache[logical];
        if (raw >= 0)
            post_driver_message(voice, uint8_t(0xb0u | voice->channel_map[logical]), 7,
                                scaled_sequence_volume(voice, raw));
    }
}

/* ail_sequence_volume_fade_set @0x449e0（原登记 sub_449E0）：按整数微秒计算每级间隔；
 * 非零 duration 请求也先重发当前音量，不从线性 PCM 增益推导。 */
static void set_sequence_volume(BgmVoice *voice, int value, uint32_t milliseconds) {
    voice->target_volume = value;
    if (voice->sequence_volume == value) return;
    if (milliseconds) {
        unsigned delta = static_cast<unsigned>(std::abs(voice->sequence_volume - value));
        voice->fade_accumulator_us = 0;
        voice->fade_interval_us = milliseconds * 1000u / delta;
    } else {
        voice->sequence_volume = value;
    }
    resend_sequence_volume(voice);
}

/* ail_mdi_sequences_timer_service @0x43270（原登记 sub_43270），43866..438c7：每整数阈值 ±1；
 * 仅进入渐变分支且 tick 可被 8 整除时重发，到目标不补最终消息、不停曲。 */
static void advance_sequence_fade(BgmVoice *voice) {
    if (voice->sequence_volume == voice->target_volume) return;
    voice->fade_accumulator_us += 1000000u / 120u;
    while (voice->fade_accumulator_us >= voice->fade_interval_us &&
           voice->sequence_volume != voice->target_volume) {
        voice->fade_accumulator_us -= voice->fade_interval_us;
        voice->sequence_volume += voice->target_volume > voice->sequence_volume ? 1 : -1;
    }
    if ((voice->timer_ticks & 7u) == 0) resend_sequence_volume(voice);
}

static void seq_note_off(void *userdata, uint8_t channel, uint8_t note) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (!sequence_owns_driver(voice) || channel >= 16 || note >= 128) return;
    post_driver_message(voice, uint8_t(0x80u | voice->channel_map[channel]), note, 0);
}

static void seq_note_on(BgmVoice *voice, uint8_t channel, uint8_t note,
                         uint8_t velocity, uint32_t duration) {
    if (!sequence_owns_driver(voice) || channel >= 16 || note >= 128 || velocity >= 128) return;
    post_driver_message(voice, uint8_t(0x90u | voice->channel_map[channel]), note, velocity);
    register_duration(voice, channel, note, duration); /* 原先发note-on，后检查空槽。 */
}

static void seq_controller(void *userdata, uint8_t channel, uint8_t type, uint8_t value) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (!sequence_owns_driver(voice) || channel >= 16 || value >= 128) return;
    if (type == 64) voice->pedal_cache[channel] = value;
    if (type == 112) voice->priority_cache[channel] = value;
    if (type == 7) {
        voice->volume_cache[channel] = value;
        value = scaled_sequence_volume(voice, value);
    }
    post_driver_message(voice, uint8_t(0xb0u | voice->channel_map[channel]), type, value);
}
static void seq_patch(void *userdata, uint8_t channel, uint8_t patch) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (sequence_owns_driver(voice) && channel < 16)
        post_driver_message(voice, uint8_t(0xc0u | voice->channel_map[channel]), patch, 0);
}
static void seq_bend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (sequence_owns_driver(voice) && channel < 16)
        post_driver_message(voice, uint8_t(0xe0u | voice->channel_map[channel]), lsb, msb);
}

/* ail_sequence_playback_stop @0x447d0（原登记 sub_447D0） / ail_sequence_pending_notes_release @0x42dd0（原登记 sub_42DD0）：先按32槽
 * 升序收音符，再按本sequence缓存清控制；ended/stopped不二次提交。 */
static void stop_sequence_events(BgmVoice *voice) {
    NativeSequence &seq = voice->sequence;
    if (seq.status != 4) return;
    seq.status = 8;
    for (DurationSlot &slot : seq.pending) {
        if (slot.channel >= 0) {
            seq_note_off(voice, static_cast<uint8_t>(slot.channel), slot.note);
            slot.channel = -1;
        }
    }
    seq.pending_count = 0;
    for (uint8_t channel = 0; channel < 16; ++channel) {
        if (voice->pedal_cache[channel] >= 64)
            post_driver_message(voice, uint8_t(0xb0u | voice->channel_map[channel]), 64, 0);
        if (voice->priority_cache[channel] >= 64)
            post_driver_message(voice, uint8_t(0xb0u | voice->channel_map[channel]), 112, 0);
    }
}
static void end_sequence(BgmVoice *voice) {
    if (!sequence_owns_driver(voice)) return;
    stop_sequence_events(voice);
    voice->driver->active_sequence = nullptr;
}
static bool register_duration(BgmVoice *voice, uint8_t channel, uint8_t note, uint32_t duration) {
    for (DurationSlot &slot : voice->sequence.pending) {
        if (slot.channel < 0) {
            slot.channel = channel; slot.note = note; slot.remaining = duration;
            ++voice->sequence.pending_count; return true;
        }
    }
    stop_sequence_events(voice);
    voice->sequence.queue_overflow = true;
    voice->sequence.status = 2;
    return false;
}
/* ail_mdi_sequences_timer_service @0x43270（原登记 sub_43270）：全部实际曲只使用默认100% tempo。
 * 一次120Hz服务：到期槽升序→delay/原EVNT→非EOT才fade；duration0次tick到期。 */
static void native_sequence_tick(BgmVoice *voice) {
    NativeSequence &seq = voice->sequence;
    if (seq.status != 4) return;
    ++voice->timer_ticks;
    for (DurationSlot &slot : seq.pending) {
        if (slot.channel < 0) continue;
        --slot.remaining;
        if (slot.remaining == 0 || (slot.remaining & 0x80000000u)) {
            seq_note_off(voice, static_cast<uint8_t>(slot.channel), slot.note);
            slot.channel = -1; --seq.pending_count;
        }
    }
    --seq.delay;
    bool eot = false;
    if (seq.delay == 0 || (seq.delay & 0x80000000u)) {
        while (seq.cursor < seq.size) {
            NativeEvent event;
            if (!decode_native_event(seq, seq.cursor, event)) {
                stop_sequence_events(voice); seq.status = 2; return;
            }
            if (event.status < 128) { seq.cursor = event.next; seq.delay = event.status; break; }
            if (event.status == 0xff && event.meta == 0x2f) {
                eot = true;
                if (seq.loop_remaining && --seq.loop_remaining == 0) {
                    stop_sequence_events(voice); seq.status = 2; seq.cursor = event.next;
                } else seq.cursor = 0;
                break;
            }
            uint8_t channel = event.status & 15u;
            switch (event.status & 0xf0u) {
            case 0x80: seq_note_off(voice, channel, event.a); break;
            case 0x90:
                seq_note_on(voice, channel, event.a, event.b, event.duration);
                if (seq.queue_overflow) return; /* 原满表不前移当前note-on的指针。 */
                break;
            case 0xb0: seq_controller(voice, channel, event.a, event.b); break;
            case 0xc0: seq_patch(voice, channel, event.a); break;
            case 0xe0: seq_bend(voice, channel, event.b, event.a); break;
            }
            seq.cursor = event.next;
        }
    }
    if (!eot && seq.status == 4) advance_sequence_fade(voice);
}

static void render_opl_frames(BgmVoice *voice, float *output, size_t frames) {
    const uint64_t period = static_cast<unsigned>(voice->sample_rate);
    for (size_t i = 0; i < frames; i++) {
        while (voice->driver->timer_sample_units >= period) {
            voice->driver->timer_sample_units -= period;
            native_sequence_tick(voice);
        }
        int16_t sample[2] = {0, 0};
        OPL3_GenerateResampled(&voice->driver->opl, sample);
        output[i * 2u] = sample[0] / 32768.0f;
        output[i * 2u + 1u] = sample[1] / 32768.0f;
        voice->driver->timer_sample_units += 120u;
    }
}

static size_t bgm_render(void *userdata, float *stereo, size_t frames) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (!voice || voice->state.load(std::memory_order_acquire) != 1) return 0;
    if (!voice->started && !begin_sequence(voice)) return 0;
    if (!sequence_owns_driver(voice)) return 0;
    int fade = voice->fade_request.exchange(-1, std::memory_order_acq_rel);
    if (fade >= 0) set_sequence_volume(voice, fade & 255, static_cast<uint32_t>(fade >> 8));
    render_opl_frames(voice, stereo, frames);
    /* 原status2只停事件服务；同chip release继续，不猜固定tail秒数。 */
    return frames;
}

static void bgm_retire(void *userdata, fd2_audio_retire_reason reason) {
    (void)reason;
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (voice && voice->magic == kMagic) {
        end_sequence(voice);
        voice->state.store(2, std::memory_order_release);
    }
}

static BgmVoice *claim_voice(fd2_bgm_player_impl *player) {
    for (BgmVoice &voice : player->voices) {
        int state = voice.state.load(std::memory_order_acquire);
        if (state == 2) {
            voice.sequence = NativeSequence{};
            voice.state.store(0, std::memory_order_release);
            state = 0;
        }
        if (state == 0) return &voice;
    }
    return nullptr;
}

} // namespace

struct fd2_bgm_player : fd2_bgm_player_impl {};

fd2_bgm_player *fd2_bgm_create(fd2_audio *audio,
                               const fd2_bgm_config *config) {
    if (!audio || !config || !config->fdmus_path || !config->ail_bank_path)
        return nullptr;
    fd2_bgm_player *player = new(std::nothrow) fd2_bgm_player;
    if (!player) return nullptr;
    player->audio = audio;
    if (fd2_archive_open(&player->fdmus, config->fdmus_path) != 0 ||
        !load_ail_bank(config->ail_bank_path, player->driver.instruments)) {
        fd2_archive_close(&player->fdmus);
        delete player;
        return nullptr;
    }
    int rate = config->sample_rate > 0 ? config->sample_rate
                                      : fd2_audio_sample_rate(audio);
    player->driver.sample_rate = rate;
    player->driver.voice_count = config->voice_count > 0
                        ? std::min<size_t>(kChannels,
                              static_cast<size_t>(config->voice_count))
                        : kChannels;
    for (BgmVoice &voice : player->voices) {
        voice.sample_rate = rate;
        voice.driver = &player->driver;
    }
    reset_driver(&player->driver);
    return player;
}

void fd2_bgm_destroy(fd2_bgm_player *player) {
    if (!player) return;
    /* audio 必须先于 player 销毁，main 的 cleanup 顺序负责触发 retire。 */
    fd2_archive_close(&player->fdmus);
    delete player;
}

int fd2_bgm_play(fd2_bgm_player *player, size_t track, int loop_count) {
    if (!player || loop_count < 0 || !fd2_bgm_track_valid(player, track)) return -1;
    if (player->current_track == static_cast<int>(track)) return 0;
    const uint8_t *data = nullptr;
    size_t size = 0;
    if (fd2_archive_get(&player->fdmus, track, &data, &size) != 0) return -1;
    NativeSequence prepared;
    if (!prepare_native_sequence(prepared, data, size)) return -1;
    BgmVoice *voice = claim_voice(player);
    if (!voice) return -1;
    voice->track = static_cast<int>(track);
    voice->loop_count = loop_count;
    prepare_sequence(voice);
    voice->fade_request.store(-1, std::memory_order_release);
    voice->music_enabled = player->music_enabled;
    /* music_track_play @0x25977（原登记 FUN_0004ab8b）：音频激活时普通曲
     * 设置 0→127/2000ms；16/17 直接 127，准备阶段不写共享 driver。 */
    voice->sequence = prepared;
    voice->sequence.loop_remaining = static_cast<uint32_t>(loop_count);
    voice->state.store(1, std::memory_order_release);
    fd2_audio_source source = {bgm_render, bgm_retire, voice};
    if (fd2_audio_play_music_source(player->audio, source, 1.0f) != 0) {
        voice->state.store(0, std::memory_order_release);
        return -1;
    }
    player->current_track = static_cast<int>(track);
    return 0;
}

/* field_options_menu_execute @0x1728c（原同名登记）：只在真实toggle时请求1秒渐变。
 * Load只恢复配置；新曲由music_track_play @0x25977（原登记FUN_0004ab8b）应用。
 * 复用既有source命令槽；多个producer调用落同render块的中间时序不作硬件等价承诺。 */
int fd2_bgm_set_music_enabled(fd2_bgm_player *player, int enabled, int fade_current) {
    if (!player || (enabled != 0 && enabled != 1) ||
        (fade_current != 0 && fade_current != 1)) return -1;
    player->music_enabled = enabled != 0;
    if (fade_current) {
        for (BgmVoice &voice : player->voices) {
            if (voice.state.load(std::memory_order_acquire) == 1)
                voice.fade_request.store((1000 << 8) | (enabled ? 127 : 0), std::memory_order_release);
        }
    }
    return 0;
}

int fd2_bgm_stop(fd2_bgm_player *player) {
    if (!player) return -1;
    player->current_track = -1;
    /* music_track_play @0x25977（原登记 FUN_0004ab8b）的 -1 请求：
     * 原版 4000 ms sequence-volume 渐变，不承诺硬件已静音或 source 已退休。 */
    for (BgmVoice &voice : player->voices) {
        if (voice.state.load(std::memory_order_acquire) == 1)
            voice.fade_request.store(4000 << 8, std::memory_order_release);
    }
    return 0;
}

int fd2_bgm_current_track(const fd2_bgm_player *player) {
    return player ? player->current_track : -1;
}

size_t fd2_bgm_track_count(const fd2_bgm_player *player) {
    return player ? player->fdmus.count : 0;
}

int fd2_bgm_track_valid(const fd2_bgm_player *player, size_t track) {
    if (!player) return 0;
    static const uint8_t xmid[] = {'X', 'M', 'I', 'D'};
    const uint8_t *data = nullptr;
    size_t size = 0;
    return fd2_archive_get(&player->fdmus, track, &data, &size) == 0 &&
           size >= 16 && std::memcmp(data, "FORM", 4) == 0 &&
           std::search(data, data + std::min<size_t>(size, 96),
                       xmid, xmid + sizeof(xmid)) !=
               data + std::min<size_t>(size, 96);
}
