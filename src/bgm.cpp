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
#include "bios_clock.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "third_party/nukedopl3.h"
#include "third_party/opl_models.h"

namespace {

constexpr size_t kChannels = 18;
constexpr size_t kLogicalVoices = 20;
constexpr uint8_t kUnassigned = 0xff;
constexpr size_t kMidiChannels = 16;
constexpr size_t kMaxInstruments = 256;

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
    uint8_t key_bits = 0, dirty = 0; /* 原1561／1539，同一个Voice。 */
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
    const uint8_t *rbrn_chunk = nullptr; /* 原S+0c，只借同resource。 */
    const uint8_t *timbre_chunk = nullptr;size_t timbre_available=0;
    const uint8_t *timbre_pairs = nullptr; /* 借同一immutable FDMUS，不另复制资源。 */
    uint16_t timbre_count = 0;
    bool install_timbres = false; /* 443d0非空TIMB在503前先flush同driver旧消息。 */
};
struct NativeEvent {
    uint8_t status = 0, a = 0, b = 0, meta = 0;
    uint32_t duration = 0,meta_size=0;
    unsigned duration_vlq_bytes=0,length_vlq_bytes=0;
    size_t next = 0;
    uint32_t meta_value = 0; /* 51h原BE24；58h复用a/b的BYTE参数。 */
};

struct BgmVoice;
struct fd2_bgm_player_impl;

/* 唯一MDI runtime：App线程按共享AIL IRQ服务、渲染及退休。
 * SDL设备只消费已生成PCM；sequence预检失败不写driver。 */
struct InstrumentSlot {
    uint8_t flags = 0, patch = 0, bank = 0;
    uint16_t offset = 0;
    std::array<uint8_t,14> payload{}; /* 原heap的唯一runtime14B；cold bank只作输入。 */
    uint32_t touched = 0;
};

struct AilDriver {
    fd2_audio *audio = nullptr;
    bool source_live = false; /* 同Audio的常驻chip借用，非CPU/source模式。 */
    int sample_rate = 48000;
    size_t voice_count = kChannels;
    std::array<AilInstrument, kMaxInstruments> instruments{}; /* immutable decoded bank */
    std::array<InstrumentSlot, 192> instrument_cache{}; /* 原133b/11bb/127b/103b */
    std::array<int16_t, 128> drum_instruments{}; /* 原19b9；FF表示未resolve。 */
    uint32_t instrument_clock = 0; /* 原D37/D39，仅注册与note-on有源推进。 */
    uint16_t instrument_bytes = 0; /* 原1401，固定heap4096。 */
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
    /* ail_mdi_message_queue_push/flush @0x422c0/0x42270（原sub_422C0/sub_42270）：
     * 原driver+100h的512B及D+1a8/1ac；同driver唯一待提交消息，不是第二sequencer。 */
    std::array<uint8_t,512> message_bytes{};
    uint32_t queued_messages = 0, queued_bytes = 0;
    /* sub_43AD0 @0x43ad0（原同名登记）：共享AIL请求120Hz，逻辑fade每次
     * 加8333us。IRQ相位归唯一bios_clock，不由设备sample计数重建。 */
    fd2_ail_timer *timer = nullptr;
    uint32_t timer_handle = FD2_AIL_TIMER_INVALID;
};

struct BgmVoice {
    /* 原53ED0唯一S；host诊断属于同owner，不是第二cache／sequencer。 */
    fd2_bgm_player_impl *owner = nullptr;
    fd2_bgm_event_state event_state = FD2_BGM_EVENTS_NO_VALID_INIT;
    bool event_field_produced = false, resource_present = false, barrier_reached = false;
    fd2_bgm_gap_reason first_gap_reason = FD2_BGM_GAP_NONE;
    int first_gap_track = -1, first_event_track = -1;
    uint32_t first_gap_pc = 0;
    bool first_event_read_blocked = false;
    AilDriver *driver = nullptr; /* 借用 player；audio 先销毁。 */
    NativeSequence sequence;
    std::array<int16_t, kMidiChannels> pedal_cache{};
    std::array<int16_t, kMidiChannels> priority_cache{};
    bool music_enabled = true;
    std::array<int16_t, kMidiChannels> volume_cache{}; /* 43160：-1 表示尚无原 CC7。 */
    std::array<uint8_t, kMidiChannels> channel_map{};  /* 43160：初始恒等映射。 */
    int sequence_volume = 127;
    int target_volume = 127;
    uint32_t fade_accumulator_us = 0;
    uint32_t fade_interval_us = 0;
    uint32_t timer_ticks = 0;
    bool start_fade = false;
    /* 原同S+44..54：tempo百分比不由meta51改变；无第二服务时钟。 */
    uint32_t tempo_percent=100,target_tempo=100;
    uint32_t tempo_fade_accumulator_us=0,tempo_fade_interval_us=0,tempo_accumulator=0;
    /* 同S+58..6c按DWORD位型保存；比较按原signed指令，算术按32bit回绕。 */
    uint32_t beat=0,measure=UINT32_MAX,beats_per_measure=4;
    uint32_t beat_increment=0,beat_phase=0,beat_denominator=8000000;
};

struct fd2_bgm_player_impl {
    fd2_audio *audio = nullptr;
    fd2_archive fdmus{};
    AilDriver driver;
    BgmVoice sequence;
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
        } else if (!std::memcmp(data + at, "TIMB", 4)) {
            /* 正式15曲均有非空TIMB；同一immutable span，不读取下一资源或重建音色。 */
            if (size >= 2 && rd16(data + at + 8) != 0) {
                const uint16_t count=rd16(data+at+8);
                if(out.install_timbres || size-2u < size_t(count)*2u)return false;
                out.install_timbres=true;out.timbre_chunk=data+at;out.timbre_available=end-at;
                out.timbre_pairs=data+at+10;out.timbre_count=count;
            }
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
static bool native_vlq(const uint8_t *data, size_t size, size_t &at, uint32_t &value,unsigned *bytes=nullptr) {
    value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (at >= size) return false;
        uint8_t byte = data[at++]; value = (value << 7) | (byte & 127u);
        if (!(byte & 128u)) {if(bytes)*bytes=i+1;return true;}
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
        if (kind == 9 && !native_vlq(seq.evnt, seq.size, at, e.duration,&e.duration_vlq_bytes)) return false;
        if (kind == 11 && e.a != 1 && e.a != 7 && e.a != 64) return false;
    } else if (kind == 12) {
        if (at == seq.size || (e.a = seq.evnt[at++]) >= 128) return false;
    } else if (e.status == 0xff) {
        if (at == seq.size) return false;
        e.meta = seq.evnt[at++]; uint32_t size = 0;
        if (!native_vlq(seq.evnt, seq.size, at, size,&e.length_vlq_bytes) || size > seq.size - at) return false;
        if ((e.meta == 0x51 && size < 3) || (e.meta == 0x58 && size < 2) ||
            (e.meta == 0x2f && size != 0)) return false;
        if(e.meta==0x51)e.meta_value=(uint32_t(seq.evnt[at])<<16) |
            (uint32_t(seq.evnt[at+1])<<8) | seq.evnt[at+2];
        if(e.meta==0x58){e.a=seq.evnt[at];e.b=seq.evnt[at+1];}
        e.meta_size=size;at += size; /* 唯一parser另给栈上原VLQ字节数，不重解析。 */
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

/* 2fa1/2fab真实heap14B的只读局部投影；不保第二decoded热音色。 */
static AilInstrument decode_instrument(const uint8_t *raw) {
    AilInstrument inst;inst.present=true;inst.note_offset=static_cast<int8_t>(raw[2]);
    inst.mod_20=raw[3];inst.mod_40=raw[4];inst.mod_60=raw[5];inst.mod_80=raw[6];inst.mod_e0=raw[7];
    inst.feedback=raw[8];inst.car_20=raw[9];inst.car_40=raw[10];inst.car_60=raw[11];
    inst.car_80=raw[12];inst.car_e0=raw[13];return inst;
}
static std::array<uint8_t,14> instrument_payload(const AilInstrument &i) {
    return {{14,0,uint8_t(i.note_offset),i.mod_20,i.mod_40,i.mod_60,i.mod_80,i.mod_e0,
             i.feedback,i.car_20,i.car_40,i.car_60,i.car_80,i.car_e0}};
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
    std::vector<uint8_t> data;
    if(static_cast<size_t>(length)>data.max_size()){std::fclose(file);return false;}
    try{data.resize(static_cast<size_t>(length));}
    catch(const std::bad_alloc&){std::fclose(file);return false;}
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
        out[static_cast<size_t>(index)]=decode_instrument(raw);
        count++;
    }
    return count >= 128;
}

/* 原2ca6按物理slot顺序扫描；绝不用key或cold目录顺序充当slot。 */
static int find_cached_instrument(const AilDriver *driver,uint8_t patch,uint8_t bank) {

    for(size_t i=0;i<driver->instrument_cache.size();i++) {
        const InstrumentSlot &slot=driver->instrument_cache[i];

        if(slot.flags&0x80u) {

            if(slot.bank==bank) {

                if(slot.patch==patch) {
                    return static_cast<int>(i);
                }
            }
        }

    }

    return -1;
}
static bool register_instrument(AilDriver *driver,uint8_t patch,uint8_t bank) {
    int slot=find_cached_instrument(driver,patch,bank);
    if(slot<0) {
        const int cold=instrument_index(patch,bank);
        if(cold<0||!driver->instruments[size_t(cold)].present)return false;
        size_t at=0;
        while(at<driver->instrument_cache.size()&&(driver->instrument_cache[at].flags&0x80u))++at;
        if(at==driver->instrument_cache.size()||driver->instrument_bytes>4096u-14u)return false;
        const uint16_t old_bytes=driver->instrument_bytes;
        driver->instrument_bytes=uint16_t(old_bytes+14u);slot=int(at);
        InstrumentSlot &record=driver->instrument_cache[at];
        record.patch=patch;record.bank=bank;record.flags=0x80;
        record.touched=driver->instrument_clock++;record.offset=old_bytes;
        record.payload=instrument_payload(driver->instruments[size_t(cold)]);
    }
    for(size_t channel=0;channel<kMidiChannels;++channel)
        if(driver->patches[channel]==patch&&driver->banks[channel]==bank)
            driver->selected_instruments[channel]=int16_t(slot);
    return true;
}
static void flush_driver_messages(AilDriver *);
static bool install_sequence_timbres(AilDriver *driver,const NativeSequence &sequence) {
    if(sequence.timbre_count&&!sequence.timbre_pairs)return false;
    flush_driver_messages(driver);
    for(size_t i=0;i<sequence.timbre_count;++i) {
        const uint8_t patch=sequence.timbre_pairs[2*i],bank=sequence.timbre_pairs[2*i+1];
        if(find_cached_instrument(driver,patch,bank)<0&&!register_instrument(driver,patch,bank))return false;
    }
    return true;
}

static AilInstrument voice_instrument(const AilDriver *voice,
                                             const Voice &slot) {
    return decode_instrument(voice->instrument_cache[slot.instrument].payload.data());
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
    /* 低16位折回沿原整数算式；合成频率仍由同AIL表提供。 */

    const uint16_t difference=uint16_t(bend-8192u);
    const int signed_difference=difference<0x8000u?int(difference):int(difference)-65536;
    int pitch=int(uint16_t(sar(signed_difference,5)*range));
    int note=int(slot.tone)+slot.note_offset-24;
    do { note+=12; } while(note<0);
    note+=12;
    do { note-=12; } while(note>95);

    uint16_t ax=uint16_t((unsigned(pitch)&0xffu)|
        ((((unsigned(pitch)>>8)+unsigned(note))&0xffu)<<8));
    ax=uint16_t(ax+8u);
    pitch=sar(ax<0x8000u?int(ax):int(ax)-65536,4)-192;
    do { pitch+=192; } while(pitch<0);
    pitch+=192;
    do { pitch-=192; } while(pitch>1535);

    uint32_t unused=0;
    uint16_t value=oplModel_ailFreq((pitch<192?pitch+192:pitch)/16.0+12.0,&unused);
    if(pitch<192) {
        if(value&0x1c00u)value=uint16_t(value-0x400u);
        else value=uint16_t((value&0x3ffu)>>1);
    }
    return value;
}

/* 原SBPRO2.MDI3184每个正常寄存器写的host FillUp接点。
 * 先完成当前Audio片段，再写同一Nuked实例；不从写入次数重建音乐时间。 */
#ifndef FD2_BGM_DRIVER_WRITE_TRACE
#define FD2_BGM_DRIVER_WRITE_TRACE(driver,reg,value) ((void)(driver),(void)(reg),(void)(value))
#endif

static void normal_write_reg(AilDriver *driver,uint16_t reg,uint8_t value) {
    if(driver->audio&&fd2_audio_is_original_mixer(driver->audio))
        (void)fd2_audio_original_fill_up(driver->audio);
    OPL3_WriteReg(&driver->opl,reg,value);
    FD2_BGM_DRIVER_WRITE_TRACE(driver,reg,value);
}

static void normal_operator_write(AilDriver *driver,uint16_t reg,uint8_t value) {

    normal_write_reg(driver,reg,value);
}
static void normal_channel_write(AilDriver *driver,uint16_t reg,uint8_t value) {

    normal_write_reg(driver,reg,value);
}

static void opl_update_voice(AilDriver *voice, Voice &slot, uint8_t dirty) {
    if(dirty)slot.dirty=uint8_t(slot.dirty|dirty); /* 原caller的dirty写点。 */

    if(slot.opl_channel==kUnassigned) {

        return;
    }
    dirty=slot.dirty;

    /* 固定SAMPLE.AD的162项均14B：原type0，4op mask保持初始化的0。
     * 不借这个共享two-op路径开放25B、四算子或未知probe后状态。 */

    const AilInstrument &inst=voice_instrument(voice,slot);
    const uint8_t channel=slot.midi_channel;
    const uint16_t op=operator_register(0,slot.opl_channel);

    if(dirty&0x80u) {

        uint8_t vibrato=0x40;
        if(voice->modulations[channel]<64) {
            vibrato=0;
        }

        normal_operator_write(voice,0x20u+op,inst.mod_20|vibrato);

        normal_operator_write(voice,0x23u+op,inst.car_20|vibrato);
        slot.dirty=uint8_t(slot.dirty&0x7fu);

    }

    if(dirty&0x40u) {

        normal_operator_write(voice,0x40u+op,scaled_level(inst.mod_40,slot.velocity,
            voice->volumes[channel],voice->expressions[channel],(inst.feedback&1u)!=0));

        normal_operator_write(voice,0x43u+op,scaled_level(inst.car_40,slot.velocity,
            voice->volumes[channel],voice->expressions[channel],true));
        slot.dirty=uint8_t(slot.dirty&0xbfu);

    }

    if(dirty&0x20u) {

        normal_operator_write(voice,0x60u+op,inst.mod_60);
        normal_operator_write(voice,0x63u+op,inst.car_60);
        normal_operator_write(voice,0x80u+op,inst.mod_80);
        normal_operator_write(voice,0x83u+op,inst.car_80);
        slot.dirty=uint8_t(slot.dirty&0xdfu);

    }

    if(dirty&0x10u) {

        normal_operator_write(voice,0xe3u+op,inst.car_e0);
        normal_operator_write(voice,0xe0u+op,inst.mod_e0);
        slot.dirty=uint8_t(slot.dirty&0xefu);

    }

    if(dirty&0x08u) {

        uint8_t routing=uint8_t((inst.feedback&0x0fu)|0x30u);
        if(voice->pans[channel]<=27) {
            routing&=0xef;
        } else {

            if(voice->pans[channel]>=100) {
                routing&=0xdf;
            }
        }

        normal_channel_write(voice,channel_register(0xc0,slot.opl_channel),routing);
        slot.dirty=uint8_t(slot.dirty&0xf7u);

    }

    if(dirty&0x01u) {

        if(!(slot.key_bits&0x20u)) {

            normal_channel_write(voice,channel_register(0xb0,slot.opl_channel),uint8_t(slot.b0&0xdfu));

        } else {
            const uint16_t value=ail_frequency(slot,voice->bends[channel],voice->pitch_ranges[channel]);

            normal_channel_write(voice,channel_register(0xa0,slot.opl_channel),uint8_t(value));
            slot.b0=uint8_t(((value>>8)&0x1fu)|slot.key_bits);

            normal_channel_write(voice,channel_register(0xb0,slot.opl_channel),slot.b0);

        }
        slot.dirty=uint8_t(slot.dirty&0xfeu);

    }

}

/* 3127：未分配硬件时直接返回；39cc自己的active清除另在caller。 */
static void release_voice(AilDriver *voice, Voice &slot) {

    if(slot.opl_channel!=kUnassigned) {
        slot.key_bits=uint8_t(slot.key_bits&0xdfu);
        slot.dirty|=1;
        opl_update_voice(voice,slot,0);
        --voice->assigned_counts[slot.midi_channel];

        voice->hardware_channels[slot.opl_channel]=kUnassigned;
        slot.opl_channel=kUnassigned;

        slot.active=false;
    }

}

/* 36f6：score只快照一次。普通14B都是type0；平分取末槽。 */
static void steal_hardware(AilDriver *voice) {

    std::array<uint16_t,kLogicalVoices> scores{};
    size_t budget=0;
    for(size_t i=0;;i++) {
        if(i==voice->voices.size())break;
        const Voice &candidate=voice->voices[i];
        if(!candidate.active)continue;
        ++budget;
        unsigned base=0xffffu;
        if(voice->priorities[candidate.midi_channel]<64) {
            base=0x7fffu;
        }

        const unsigned count=voice->assigned_counts[candidate.midi_channel];
        if(base<count){base=count;}
        scores[i]=uint16_t(base-count);
    }
    for(;;) {

        int incoming=-1,victim=-1;uint16_t maximum=0,minimum=0xffff;
        for(size_t i=0;;i++) {
            if(i==voice->voices.size())break;
            const Voice &candidate=voice->voices[i];
            if(!candidate.active)continue;

            if(candidate.opl_channel==kUnassigned) {

                if(scores[i]>=maximum) {
                    maximum=scores[i];incoming=int(i);
                }
            } else {

                if(scores[i]<=minimum) {
                    minimum=scores[i];victim=int(i);
                }
            }
        }

        if(maximum<minimum){break;}
        if(!maximum)break;
        /* maximum>0及minimum<FFFF由原循环保证两个局部已有producer。 */

        uint8_t hardware=voice->voices[size_t(victim)].opl_channel;
        release_voice(voice,voice->voices[size_t(victim)]);
        Voice &selected=voice->voices[size_t(incoming)];
        selected.opl_channel=hardware;

        ++voice->assigned_counts[selected.midi_channel];
        voice->hardware_channels[hardware]=selected.midi_channel;

        selected.dirty=0xf9;
        opl_update_voice(voice,selected,0);
        if(--budget==0)break;

    }

}
static void allocate_hardware(AilDriver *voice, Voice &slot) {

    for(size_t n=0;;n++) {

        if(n==voice->voice_count) {
            steal_hardware(voice);
            return;
        }
        int hardware=voice->last_free_channel+1;
        if(hardware==int(voice->voice_count)){hardware=0;}
        voice->last_free_channel=hardware;

        if(voice->hardware_channels[size_t(hardware)]!=kUnassigned)continue;
        slot.opl_channel=uint8_t(hardware);

        ++voice->assigned_counts[slot.midi_channel];
        voice->hardware_channels[size_t(hardware)]=slot.midi_channel;
        slot.dirty=0xf9;
        opl_update_voice(voice,slot,0);
        return;
    }
}
static void driver_note_off(void *userdata,uint8_t channel,uint8_t note) {
    AilDriver *voice=static_cast<AilDriver *>(userdata);if(channel>=kMidiChannels)return;

    for(size_t i=0;;i++) {
        if(i==voice->voices.size())break;
        Voice &slot=voice->voices[i];
        if(!slot.active)continue;
        if(slot.note!=note)continue;
        if(slot.midi_channel!=channel)continue;

        if(voice->pedals[channel]>=64) {
            slot.deferred=true;

        } else {

            release_voice(voice,slot);
            slot.active=false;

        }

    }

}
static void driver_note_on(void *userdata,uint8_t channel,uint8_t note,uint8_t velocity) {
    AilDriver *voice=static_cast<AilDriver *>(userdata);
    if(channel>=kMidiChannels || note>=128 || velocity>=128)return;
    if(!velocity){driver_note_off(userdata,channel,note);return;} /* 3b94在caller转发。 */

    int index=voice->selected_instruments[channel];
    if(channel==9) {
        index=voice->drum_instruments[note];
        if(index<0) {
            index=find_cached_instrument(voice,note,0x7f);
            voice->drum_instruments[note]=int16_t(index);
        }
    }

    if(index<0) {return;}
    const AilInstrument inst=decode_instrument(voice->instrument_cache[size_t(index)].payload.data());

    const uint32_t old_clock=voice->instrument_clock;
    voice->instrument_clock=(old_clock&0xffff0000u)|uint16_t(old_clock+1u);
    voice->instrument_clock=old_clock+1u;
    InstrumentSlot &instrument=voice->instrument_cache[size_t(index)];
    instrument.touched=(instrument.touched&0xffff0000u)|(voice->instrument_clock&0xffffu);
    instrument.touched=voice->instrument_clock;

    for(size_t i=0;i<voice->voices.size();i++) {
        Voice &slot=voice->voices[i];
        if(slot.active){continue;}
        slot.midi_channel=channel;
        slot.note=note;

        slot.tone=channel==9?uint8_t(inst.note_offset):note;
        slot.note_offset=channel==9?0:inst.note_offset;
        slot.velocity=velocity;
        slot.instrument=uint16_t(index);
        slot.active=true;
        slot.deferred=false;

        slot.key_bits=0x20;

        slot.dirty=0xf9;

        slot.opl_channel=kUnassigned;
        allocate_hardware(voice,slot);

        return;
    }

}
/* 3b1e/3c9a按sounding note重入39cc，不改为逐实例强制释放。 */
static void flush_deferred(AilDriver *voice,uint8_t channel) {

    for(Voice &slot:voice->voices) {

        if(slot.active) {

            if(slot.midi_channel==channel) {

                if(slot.deferred) {
                    driver_note_off(voice,channel,slot.tone);

                }
            }
        }

    }

}
static void update_channel_voices(AilDriver *voice,uint8_t channel,uint8_t dirty) {

    for(Voice &slot:voice->voices) {

        if(slot.active) {

            if(slot.midi_channel==channel) {
                slot.dirty|=dirty;
                opl_update_voice(voice,slot,0);

            }
        }

    }

}
static void driver_controller(void *userdata,uint8_t channel,uint8_t type,uint8_t value) {
    AilDriver *voice=static_cast<AilDriver *>(userdata);if(channel>=kMidiChannels || value>=128)return;

    if(type==114){voice->banks[channel]=value;return;}

    if(type==112){voice->priorities[channel]=value;return;}

    if(type==113) {

        const int selected=voice->selected_instruments[channel];if(selected<0)return;

        InstrumentSlot &slot=voice->instrument_cache[size_t(selected)];slot.flags=uint8_t((slot.flags&0xbfu)|(value>=64?0x40u:0u));
        return;
    }

    if(type==1){voice->modulations[channel]=value;update_channel_voices(voice,channel,0x80);return;}

    if(type==7){voice->volumes[channel]=value;update_channel_voices(voice,channel,0x40);return;}

    if(type==11){voice->expressions[channel]=value;update_channel_voices(voice,channel,0x40);return;}

    if(type==10){voice->pans[channel]=value;update_channel_voices(voice,channel,8);return;}

    if(type==64) {
        voice->pedals[channel]=value;

        if(value>=64){return;}
        flush_deferred(voice,channel);
        return;
    }

    if(type==121) {

        voice->pedals[channel]=0;
        flush_deferred(voice,channel);

        voice->modulations[channel]=0;
        voice->expressions[channel]=127;
        voice->bends[channel]&=0xff80u;
        voice->bends[channel]=8192;
        update_channel_voices(voice,channel,0xc1);return;
    }

    if(type==123) {

        for(Voice &slot:voice->voices) {

            if(slot.active) {

                if(slot.midi_channel==channel) {
                    driver_note_off(voice,channel,slot.tone);

                }
            }

        }
        return;
    }

    if(type==6){voice->pitch_ranges[channel]=value;return;}

}
static void driver_patch(void *userdata,uint8_t channel,uint8_t patch) {
    AilDriver *voice=static_cast<AilDriver *>(userdata);if(channel>=kMidiChannels || patch>=128)return;
    voice->patches[channel]=patch;
    const int selected=find_cached_instrument(voice,patch,voice->banks[channel]);
    voice->selected_instruments[channel]=int16_t(selected);
}
static void driver_bend(void *userdata,uint8_t channel,uint8_t msb,uint8_t lsb) {
    AilDriver *voice=static_cast<AilDriver *>(userdata);if(channel>=kMidiChannels || msb>=128 || lsb>=128)return;
    voice->bends[channel]=uint16_t((voice->bends[channel]&0xff80u)|lsb);
    voice->bends[channel]=uint16_t((msb<<7)|lsb);
    update_channel_voices(voice,channel,1);
}

static void reset_driver_state(AilDriver *voice) {
    voice->active_sequence = nullptr;
    voice->queued_messages = voice->queued_bytes = 0; /* 43f18/43f22：新driver初始化。 */
    voice->voices = {};
    voice->hardware_channels.fill(kUnassigned);
    voice->assigned_counts.fill(0);
    voice->last_free_channel = -1;
    voice->instrument_cache={};voice->instrument_clock=0;voice->instrument_bytes=0;
    voice->drum_instruments.fill(-1);
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
}
static void initialize_driver_registers(AilDriver *voice) {
    /* 2c47：OPL3 enable/4op clear 后按原两个 bank 初值清寄存器。 */
    normal_write_reg(voice, 0x105, 1);
    normal_write_reg(voice, 0x104, 0);
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
            normal_write_reg(voice, static_cast<uint16_t>((bank << 8) | reg), value);
        }
    }
}
static void reset_driver(AilDriver *voice) { reset_driver_state(voice);initialize_driver_registers(voice); }

/* ail_sequence_event_state_reset @43160（原sub_43160）：只写原reset字段。
 * S+10 EVNT指针、+14 cursor、+34/+38 volume以及note/remaining不在此写集。
 * beat按原写序重置，tempo字段不在reset写集；loop-stack／全对象布局另有资格门。 */
static void reset_sequence_event_state(BgmVoice *voice) {
    voice->pedal_cache.fill(-1);voice->priority_cache.fill(-1);voice->volume_cache.fill(-1);
    for(size_t i=0;i<kMidiChannels;i++)voice->channel_map[i]=static_cast<uint8_t>(i);
    for(DurationSlot &slot:voice->sequence.pending)slot.channel=-1;
    voice->sequence.pending_count=0;voice->sequence.delay=0;voice->sequence.loop_remaining=1;
    voice->sequence.queue_overflow=false;
    voice->beat=0;voice->measure=UINT32_MAX;voice->beat_phase=0;
    voice->beat_increment=0;voice->beats_per_measure=4;voice->beat_denominator=8000000;
    voice->timer_ticks=0;
}
/* 443d0成功init的已证producer：volume/target与fade初态另写，再共用43160。
 * 调用者须提供已验证EVNT；纯领域夹具也以此显式建立给定source资格。
 * failed-format绝不调用它，不把C nullptr或旧size当原合法界。 */
static void prepare_sequence(BgmVoice *voice,bool start_fade=false) {
    voice->sequence.status=2;voice->sequence.cursor=0;
    voice->sequence_volume=voice->target_volume=127;
    voice->fade_accumulator_us=voice->fade_interval_us=0;voice->start_fade=start_fade;
    reset_sequence_event_state(voice);
    voice->tempo_percent=100;voice->target_tempo=100;
    voice->tempo_fade_interval_us=0;voice->tempo_fade_accumulator_us=0;voice->tempo_accumulator=0;
    voice->event_state=FD2_BGM_EVENTS_QUALIFIED;voice->event_field_produced=true;
    voice->barrier_reached=false;
}

static void set_sequence_volume(BgmVoice *voice, int value, uint32_t milliseconds);

static bool begin_sequence(BgmVoice *voice,bool apply_volume=true) {
    if (!voice->driver || (voice->driver->active_sequence &&
                          voice->driver->active_sequence != voice)) return false;
    voice->driver->active_sequence = voice;
    voice->sequence.status = 4;
    if(!apply_volume)return true;
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
#ifndef FD2_BGM_MESSAGE_BATCH_TRACE
#define FD2_BGM_MESSAGE_BATCH_TRACE(driver, bytes, used, count) ((void)0)
#endif
static unsigned driver_message_size(uint8_t status) {
    switch (status & 0xf0u) {
    case 0x80: case 0x90: case 0xa0: case 0xb0: case 0xe0: return 3;
    case 0xc0: case 0xd0: return 2;
    default: return 0; /* 现正式preflight不产生其它语法；不复刻原unknown size0追加。 */
    }
}
static void dispatch_driver_message(AilDriver *driver, uint8_t status, uint8_t a, uint8_t b) {
    const uint8_t channel = status & 15u;
    switch (status & 0xf0u) {
    case 0x80: driver_note_off(driver, channel, a); break;
    case 0x90: driver_note_on(driver, channel, a, b); break;
    case 0xb0: driver_controller(driver, channel, a, b); break;
    case 0xc0: driver_patch(driver, channel, a); break;
    case 0xe0: driver_bend(driver, channel, b, a); break;
    default: break; /* 原3b54忽略A0/D0；仍占本包的正确字节数。 */
    }
}
/* ail_mdi_message_queue_flush @0x42270（原sub_42270）/43270@43946：
 * 原502读取count条消息后才清两个计数，unused bytes保留。这里只接提交序；
 * 原CLI/DPMI/port/PIC成本仍待共享有限续行，不把同步flush签为原时刻。 */
static void flush_driver_messages(AilDriver *driver) {
    if (!driver || driver->queued_messages == 0) return;
    FD2_BGM_MESSAGE_BATCH_TRACE(driver,driver->message_bytes.data(),driver->queued_bytes,driver->queued_messages);
    size_t at = 0;
    const uint32_t count = driver->queued_messages;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t status = driver->message_bytes[at++];
        const uint8_t a = driver->message_bytes[at++];
        const uint8_t b = driver_message_size(status) == 3 ? driver->message_bytes[at++] : 0;
        dispatch_driver_message(driver,status,a,b);
    }
    driver->queued_messages = 0;
    driver->queued_bytes = 0;
}
static void queue_driver_message(AilDriver *driver, uint8_t status, uint8_t a, uint8_t b) {
    const unsigned size = driver_message_size(status);
    if (!size) return; /* 私有unsupported H；正式五种status均在preflight内。 */
    /* 原42329 unsigned >512：正好512不flush；满包先提交，后追加新消息。 */
    if (driver->queued_bytes + size > driver->message_bytes.size() && driver->queued_messages != 0)
        flush_driver_messages(driver);
    driver->message_bytes[driver->queued_bytes++] = status;
    driver->message_bytes[driver->queued_bytes++] = a;
    if (size == 3) driver->message_bytes[driver->queued_bytes++] = b;
    ++driver->queued_messages;
}
static void post_driver_message(BgmVoice *voice, uint8_t status, uint8_t a, uint8_t b) {
    FD2_BGM_NATIVE_TRACE(voice, status, a, b); /* 原逻辑消息观测，不冒充硬件OUT。 */
    queue_driver_message(voice->driver,status,a,b);
}
/* sub_43AD0 @0x43ad0（原同名）：44000..44144每channel15条/44B，
 * 4410f逐channel flush；program0也必须提交，不能仅留下patch ff。
 * 这里保固定pref12/15的控制序；channel0/4/8/12的3次retrace时间另待续行。 */
static void initialize_driver_controls(AilDriver *driver,uint8_t volume,uint8_t pitch_range) {
    for (uint8_t channel = 0; channel < 16; ++channel) {
        const uint8_t control = static_cast<uint8_t>(0xb0u | channel);
        queue_driver_message(driver,control,114,0);
        queue_driver_message(driver,static_cast<uint8_t>(0xc0u | channel),0,0);
        queue_driver_message(driver,static_cast<uint8_t>(0xe0u | channel),0,64);
        queue_driver_message(driver,control,112,0);
        queue_driver_message(driver,control,1,0);
        queue_driver_message(driver,control,7,volume);
        queue_driver_message(driver,control,10,64);
        queue_driver_message(driver,control,11,127);
        queue_driver_message(driver,control,64,0);
        queue_driver_message(driver,control,91,40);
        queue_driver_message(driver,control,93,0);
        queue_driver_message(driver,control,100,0);
        queue_driver_message(driver,control,101,0);
        queue_driver_message(driver,control,38,0);
        queue_driver_message(driver,control,6,pitch_range);
        flush_driver_messages(driver);
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
    /* 42e38..42e6e：旧queued消息与32槽note-off先flush；447d0后半
     * 再queue pedal/priority reset，stop返回不擅加第二次flush。 */
    flush_driver_messages(voice->driver);
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
/* 同S DWORD的signed比较不借C++溢出或实现定义右移。原SHL/SAR只取CL低5位。 */
static int64_t signed_sequence_word(uint32_t value) {
    return value<UINT32_C(0x80000000)?int64_t(value):int64_t(value)-INT64_C(0x100000000);
}
static uint32_t sequence_beat_increment(uint8_t exponent) {
    /* 4353a..43577：固定正常MDI preference54334=120，IDIV(16000000,120)。
     * 这是source参数，不是测得IRQ相位；meta51不是tempo百分比。 */
    uint32_t value=16000000u/120u;
    int shift=int(exponent)-2;
    unsigned count=unsigned(shift<0?-shift:shift)&31u;
    if(!count)return value;
    if(shift>=0)return value<<count;
    return (value>>count) | ((value&UINT32_C(0x80000000))?(UINT32_MAX<<(32u-count)):0u);
}
static void sequence_meta_time_signature(BgmVoice *voice,uint8_t numerator,uint8_t exponent) {
    /* 43524->43577->4357f/86->43599/9c；measure在phase-=increment之前更新。 */
    voice->beats_per_measure=numerator;
    voice->beat_increment=sequence_beat_increment(exponent);
    voice->beat_phase=0;voice->beat=0;++voice->measure;
    voice->beat_phase-=voice->beat_increment;
}
static void sequence_beat_step(BgmVoice *voice) {
    voice->beat_phase+=voice->beat_increment;
    /* 43827/43840都是signed JL，且只有单if；不可追赶while。 */
    if(signed_sequence_word(voice->beat_phase)>=signed_sequence_word(voice->beat_denominator)) {
        voice->beat_phase-=voice->beat_denominator;++voice->beat;
        if(signed_sequence_word(voice->beat)>=signed_sequence_word(voice->beats_per_measure)) {
            voice->beat=0;++voice->measure;
        }
    }
}
static void advance_sequence_tempo_fade(BgmVoice *voice) {
    if(voice->tempo_percent==voice->target_tempo)return;
    voice->tempo_fade_accumulator_us+=1000000u/120u; /* 原D+10固定正常8333us */
    while(signed_sequence_word(voice->tempo_fade_accumulator_us)>=
          signed_sequence_word(voice->tempo_fade_interval_us) && voice->tempo_percent!=voice->target_tempo) {
        voice->tempo_fade_accumulator_us-=voice->tempo_fade_interval_us;
        voice->tempo_percent+=signed_sequence_word(voice->target_tempo)>
            signed_sequence_word(voice->tempo_percent)?1u:UINT32_MAX;
    }
}
static bool sequence_events_playable(const BgmVoice *voice) {
    return voice->event_state==FD2_BGM_EVENTS_QUALIFIED ||
        voice->event_state==FD2_BGM_EVENTS_COMPAT_REPLAY;
}

/* ail_mdi_sequences_timer_service @0x43270（原登记sub_43270）：callback计数一次，
 * tempo accumulator每满100执行duration->delay/EVNT->beat；末尾非EOT才volume->tempo fade。
 * 正常15曲tempo默认100，meta51只改变beat denominator；不是新的音频时间真值。 */
static bool advance_native_sequence(BgmVoice *voice) {
    NativeSequence &seq=voice->sequence;
    if(voice->barrier_reached && !sequence_events_playable(voice))return false;
    if(seq.status!=4)return true;
    ++voice->timer_ticks;voice->tempo_accumulator+=voice->tempo_percent;
    bool eot=false;
    while(signed_sequence_word(voice->tempo_accumulator)>=100) {
        voice->tempo_accumulator-=100;
        for(DurationSlot &slot:seq.pending) {
            if(slot.channel<0)continue;
            --slot.remaining;
            if(slot.remaining==0 || (slot.remaining&0x80000000u)) {
                seq_note_off(voice,static_cast<uint8_t>(slot.channel),slot.note);
                slot.channel=-1;--seq.pending_count;
            }
        }
        --seq.delay;
        if(signed_sequence_word(seq.delay)<=0 && !eot) {
            if(!sequence_events_playable(voice)) {
                voice->barrier_reached=true;
                if(!voice->first_event_read_blocked) {
                    voice->first_event_read_blocked=true;
                    voice->first_event_track=voice->owner?voice->owner->current_track:-1;
                }
                return false; /* unknown不读旧size，不到43946共同tail */
            }
            while(seq.cursor<seq.size) {
                NativeEvent event;
                if(!decode_native_event(seq,seq.cursor,event)) {
                    stop_sequence_events(voice);seq.status=2;return true;
                }
                if(event.status<128){seq.cursor=event.next;seq.delay=event.status;break;}
                if(event.status==0xff) {
                    if(event.meta==0x2f) {
                        eot=true;
                        if(seq.loop_remaining && --seq.loop_remaining==0) {
                            stop_sequence_events(voice);seq.status=2;seq.cursor=event.next;
                        } else {
                            voice->beat=0;voice->measure=UINT32_MAX;voice->beat_phase=0;
                            seq.cursor=0;
                        }
                        /* 原43490回433b0会peek一BYTE，但433c9/437e3由EOT门截断，
                         * 不消费新头meta/MIDI或delay；C只投影状态，不额外读未知尾。
                         * 原peek实际读资格和分支成本另账，不签全对象或strict PIC。 */
                        break;
                    }
                    if(event.meta==0x51)voice->beat_denominator=event.meta_value<<4;
                    else if(event.meta==0x58)sequence_meta_time_signature(voice,event.a,event.b);
                }
                uint8_t channel=event.status&15u;
                switch(event.status&0xf0u) {
                case 0x80:seq_note_off(voice,channel,event.a);break;
                case 0x90:
                    seq_note_on(voice,channel,event.a,event.b,event.duration);
                    if(seq.queue_overflow)return true;
                    break;
                case 0xb0:seq_controller(voice,channel,event.a,event.b);break;
                case 0xc0:seq_patch(voice,channel,event.a);break;
                case 0xe0:seq_bend(voice,channel,event.b,event.a);break;
                }
                seq.cursor=event.next;
            }
        }
        if(!eot)sequence_beat_step(voice);
    }
    if(!eot){advance_sequence_fade(voice);advance_sequence_tempo_fade(voice);}
    return true;
}

/* 一次完整43270服务：早退/EOT也到共同tail；不是每个逻辑消息即时OUT。 */
static bool native_sequence_tick(BgmVoice *voice) {
    if(!advance_native_sequence(voice))return false; /* unknown不是43946共同tail */
    flush_driver_messages(voice->driver);return true;
}
static void mdi_timer_service(void *user) {
    AilDriver *driver=static_cast<AilDriver *>(user);
    if (!driver) return;
    if (driver->active_sequence) native_sequence_tick(driver->active_sequence);
    else flush_driver_messages(driver); /* 432b4->43946：无playing sequence也清待提交包。 */
}

static void render_opl_frames(AilDriver *driver, float *output, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        int16_t sample[2] = {0, 0};
        OPL3_GenerateResampled(&driver->opl, sample);
        output[i * 2u] = sample[0] / 32768.0f;
        output[i * 2u + 1u] = sample[1] / 32768.0f;
    }
}

static size_t bgm_render(void *userdata, float *stereo, size_t frames) {
    AilDriver *driver = static_cast<AilDriver *>(userdata);
    if (!driver || !driver->source_live) return 0;
    render_opl_frames(driver, stereo, frames);
    /* 固定host的OPL channel独立于AIL sequence生命周期；包括新曲TIMB
     * preflush与start之间也必须推进chip，不能把未started误当FINISHED。 */
    return frames;
}

/* 同chip的显式S停止，不再是每曲Audio回调／RT状态机。 */
static void bgm_retire(void *userdata, fd2_audio_retire_reason reason) {
    (void)reason;BgmVoice *voice=static_cast<BgmVoice *>(userdata);
    if(voice)end_sequence(voice);
}
static void chip_retire(void *userdata, fd2_audio_retire_reason reason) {
    AilDriver *driver=static_cast<AilDriver *>(userdata);
    if(!driver || !driver->source_live)return;
    driver->source_live=false;
    if(driver->active_sequence)bgm_retire(driver->active_sequence,reason);
}
#ifndef FD2_BGM_REQUEST_PREFIX_TRACE
#define FD2_BGM_REQUEST_PREFIX_TRACE(player,pc) ((void)0)
#endif
static void record_event_source_gap(BgmVoice *voice) {
    /* music_track_play @0x25977（原FUN_0004ab8b）/
     * ail_sequence_playback_start @0x44790（原sub_44790）：
     * 3B空条目初始化失败后，原caller仍reset并从旧score开头开始。
     * 2026-11-18给定Music1 Town G1/G2录音确认重现旧乐句。C沿同player长期持有
     * 的整DAT安全重播已解码score；这是表现兼容，不冒原free后owner等价。 */
    const bool replay=voice->event_field_produced && voice->sequence.evnt && voice->sequence.size;
    voice->event_state=replay?FD2_BGM_EVENTS_COMPAT_REPLAY:FD2_BGM_EVENTS_SOURCE_UNQUALIFIED;
    voice->barrier_reached=!replay;
    if(voice->first_gap_reason==FD2_BGM_GAP_NONE) {
        voice->first_gap_reason=voice->event_field_produced?
            FD2_BGM_GAP_EVNT_OWNER_AFTER_RESOURCE_REPLACE:FD2_BGM_GAP_INITIAL_EVNT_FIELD_PRODUCER_UNPROVEN;
        voice->first_gap_track=voice->owner?voice->owner->current_track:-1;
        voice->first_gap_pc=0x447ae;
    }
}

} // namespace

struct fd2_bgm_player : fd2_bgm_player_impl {};

#ifndef FD2_BGM_STARTUP_OWNER_TRACE
#define FD2_BGM_STARTUP_OWNER_TRACE(driver,phase) ((void)0)
#endif

fd2_bgm_player *fd2_bgm_create(fd2_audio *audio,
                               const fd2_bgm_config *config) {
    if (!audio || !config || !config->fdmus_path || !config->ail_bank_path)
        return nullptr;
    fd2_bgm_player *player = new(std::nothrow) fd2_bgm_player;
    if (!player) return nullptr;
    player->audio = audio;
    player->driver.audio = audio;
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
    player->sequence.driver=&player->driver;player->sequence.owner=player;
    reset_driver_state(&player->driver);
    /* 同chip在OPL_Reset后、首个初始化写入前即由Audio借用；
     * 换曲或sequence结束只更新事件，不退休合成器及其尾音。 */
    player->driver.source_live=true;
    fd2_audio_source source={bgm_render,chip_retire,&player->driver};
    auto discard=[&]() -> fd2_bgm_player * {
        (void)fd2_audio_abandon_source(audio,source);
        fd2_archive_close(&player->fdmus);delete player;return nullptr;
    };
    if(fd2_audio_play_music_source(audio,source,1.0f)!=0 || !player->driver.source_live)
        return discard();
    FD2_BGM_STARTUP_OWNER_TRACE(&player->driver,1);
    initialize_driver_registers(&player->driver);
    initialize_driver_controls(&player->driver,127,2); /* 原pref12/15，不随换曲重置。 */
    FD2_BGM_STARTUP_OWNER_TRACE(&player->driver,2);
    if(fd2_audio_sync(audio)!=0)return discard();
    uint64_t now=fd2_audio_now_ns(audio);
    player->driver.timer=fd2_bios_clock_ail_owner_after_irqs(now);
    player->driver.timer_handle=fd2_bios_clock_install_mdi(now,mdi_timer_service,&player->driver);
    if(!player->driver.timer || player->driver.timer_handle==FD2_AIL_TIMER_INVALID)
        return discard();
    return player;
}

void fd2_bgm_destroy(fd2_bgm_player *player) {
    if (!player) return;
    /* App先close clock并销毁audio/retire，才释放userdata。release只写status，
     * 不清pending；关闭后的clock不能再调用这些已结束生命周期的借用。 */
    if(player->driver.timer && player->driver.timer_handle!=FD2_AIL_TIMER_INVALID)
        (void)fd2_ail_timer_release(player->driver.timer,player->driver.timer_handle);
    fd2_archive_close(&player->fdmus);
    delete player;
}

int fd2_bgm_play(fd2_bgm_player *player, size_t track, int loop_count) {
    if(!player || !player->driver.source_live || loop_count<0 || track>=player->fdmus.count || track>INT32_MAX)return -1;
    if(player->current_track==static_cast<int>(track))return 0;
    const uint8_t *data=nullptr;size_t size=0;
    if(fd2_archive_get(&player->fdmus,track,&data,&size)!=0)return -1;
    const bool recognized_failure=size==3 && data[0]==0x20 && data[1]==0x0d && data[2]==0x0a;
    NativeSequence prepared;
    if(!recognized_failure && !prepare_native_sequence(prepared,data,size))return -1;
    /* 25997缓存先于device/stop/res_load；host真错负返但不回滚已达cache。
     * 非recognized坏格式/参数是此接口H预检，不借本族把全部失败吞成0。 */
    player->current_track=static_cast<int>(track);FD2_BGM_REQUEST_PREFIX_TRACE(player,0x25997);
    if(fd2_audio_sync(player->audio)!=0 || fd2_audio_original_fill_up(player->audio)<0)return -1;
    BgmVoice *voice=&player->sequence;
    if(voice->resource_present) {
        FD2_BGM_REQUEST_PREFIX_TRACE(player,0x259d8);end_sequence(voice);
    }
    /* C整DAT由同player持有；空条目保留旧score，原内部owner差异另记诊断。 */
    voice->resource_present=true;voice->sequence.status=2;
    if(recognized_failure)record_event_source_gap(voice);
    FD2_BGM_REQUEST_PREFIX_TRACE(player,0x443ef);
    if(!recognized_failure) {
        voice->sequence.evnt=prepared.evnt;voice->sequence.size=prepared.size;
        voice->sequence.install_timbres=prepared.install_timbres;
        voice->sequence.timbre_chunk=prepared.timbre_chunk;voice->sequence.timbre_available=prepared.timbre_available;
        voice->sequence.timbre_pairs=prepared.timbre_pairs;voice->sequence.timbre_count=prepared.timbre_count;
        prepare_sequence(voice,track!=16 && track!=17);
        FD2_BGM_REQUEST_PREFIX_TRACE(player,0x44550);
        if(prepared.install_timbres&&!install_sequence_timbres(&player->driver,prepared))return -1;
    }
    /* 447a0再次stop在status2无写；447a9仅43160 reset，失败init保volume旧值。 */
    reset_sequence_event_state(voice);FD2_BGM_REQUEST_PREFIX_TRACE(player,0x447a9);
    FD2_BGM_REQUEST_PREFIX_TRACE(player,0x447ae);
    voice->music_enabled=player->music_enabled;
    voice->start_fade=track!=16 && track!=17;
    voice->sequence.cursor=0; /* 原447be从score开头；首次空条目没有score时不读事件。 */
    if(!begin_sequence(voice,false))return -1;
    FD2_BGM_REQUEST_PREFIX_TRACE(player,voice->event_field_produced?0x447be:0);
    if(player->music_enabled) {
        if(voice->start_fade) {
            set_sequence_volume(voice,0,0);FD2_BGM_REQUEST_PREFIX_TRACE(player,0x25a5b);
            set_sequence_volume(voice,127,2000);
        } else set_sequence_volume(voice,127,0);
    } else set_sequence_volume(voice,0,0);
    FD2_BGM_REQUEST_PREFIX_TRACE(player,0x25a7a);
    voice->sequence.loop_remaining=static_cast<uint32_t>(loop_count);
    FD2_BGM_REQUEST_PREFIX_TRACE(player,0x25a8c);
    return 0; /* 空条目重播资格见COMPAT_REPLAY；首次无score仍保UI与明确缺口。 */
}

/* field_options_menu_execute @0x1728c（原同名登记）：只在真实toggle时请求1秒渐变。
 * Load只恢复配置；新曲由music_track_play @0x25977（原登记FUN_0004ab8b）应用。
 * 每次命令先关闭已到达区间，再同步登记sequence请求；不合并为未来render的最新值。 */
int fd2_bgm_set_music_enabled(fd2_bgm_player *player, int enabled, int fade_current) {
    if (!player || (enabled != 0 && enabled != 1) ||
        (fade_current != 0 && fade_current != 1)) return -1;
    if(fd2_audio_sync(player->audio)!=0)return -1;
    player->music_enabled = enabled != 0;
    if (fade_current) {
        if(player->driver.active_sequence)
            set_sequence_volume(player->driver.active_sequence,enabled?127:0,1000u);
    }
    return 0;
}

int fd2_bgm_stop(fd2_bgm_player *player) {
    if (!player || !player->driver.source_live) return -1;
    player->current_track = -1;
    if(fd2_audio_sync(player->audio)!=0)return -1;
    /* music_track_play @0x25977（原登记 FUN_0004ab8b）的 -1 请求：
     * 原版 4000 ms sequence-volume 渐变，不承诺硬件已静音或 source 已退休。 */
    if(player->driver.active_sequence)set_sequence_volume(player->driver.active_sequence,0,4000u);
    return 0;
}

int fd2_bgm_get_qualification(const fd2_bgm_player *player,fd2_bgm_qualification *out) {
    if(!player || !out)return -1;
    const BgmVoice &s=player->sequence;
    *out=fd2_bgm_qualification{s.event_state,player->current_track,s.barrier_reached?1:0,
        s.first_gap_reason,s.first_gap_track,s.first_gap_pc,s.first_event_read_blocked?1:0,
        s.first_event_track,s.first_event_read_blocked?0x433b0u:0u};
    return 0; /* 纯同owner读取；不采clock、不读Audio/EVNT、不清历史。 */
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
