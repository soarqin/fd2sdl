/* 炎龙骑士团 2 SDL3 重写 - Miles XMIDI / AIL AdLib BGM source
 *
 * 逆向依据：music_track_play @0x4ab8b 从 FDMUS.DAT 取一项 XMIDI，
 * 初始化单一 sequence；loop_count 0 表示循环，track -1 表示停止。
 * new_game_opening_play @code0 0x22413..0x22417 明确调用 track 11。
 *
 * XMIDI 时序使用 BW MIDI Sequencer（MIT；其中 XMI 转换为 LGPL-2.1+）。
 * OPL 寄存器合成器由
 * 本文件实现最小的 Miles AIL 2-op voice allocator；音色字节直接读取
 * 原版 SAMPLE.AD，不生成或提交转换资产。
 */

#include "bgm.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

#include "third_party/midiseq/midi_sequencer_impl.hpp"
#include "third_party/nukedopl3.h"
#include "third_party/opl_models.h"

namespace {

static_assert(ATOMIC_BOOL_LOCK_FREE == 2 && ATOMIC_INT_LOCK_FREE == 2,
              "BGM audio-thread state requires lock-free atomics");

constexpr size_t kChannels = 9;
constexpr size_t kMidiChannels = 16;
constexpr size_t kMaxInstruments = 256;
constexpr uint32_t kMagic = 0x46444247u; /* FDBG */

static const uint8_t kOperatorOffsets[kChannels] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12,
};

struct AilInstrument {
    bool present = false;
    bool fixed_note = false;
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
    uint8_t midi_channel = 0;
    uint8_t note = 0;
    uint8_t tone = 0;
    uint8_t velocity = 0;
    uint8_t patch = 0;
    uint8_t opl_channel = 0;
    uint32_t age = 0;
};

struct BgmVoice {
    std::atomic<int> state{0}; /* 0 free, 1 active, 2 retired */
    uint32_t magic = kMagic;
    int track = -1;
    int loop_count = 0;
    int sample_rate = 48000;
    size_t voice_count = kChannels;
    BW_MidiSequencer sequence;
    BW_MidiRtInterface midi_interface{};
    std::array<AilInstrument, kMaxInstruments> instruments{};
    std::array<Voice, kChannels> voices{};
    std::array<uint8_t, kMidiChannels> patches{};
    std::array<uint8_t, kMidiChannels> banks{};
    std::array<uint8_t, kMidiChannels> volumes{};
    std::array<uint8_t, kMidiChannels> expressions{};
    std::array<uint8_t, kMidiChannels> pans{};
    std::array<uint16_t, kMidiChannels> bends{};
    uint32_t next_age = 1;
    opl3_chip opl{};
    float *render_output = nullptr;
    std::atomic<bool> stop_requested{false};
    float sequence_gain = 1.0f;
    float gain_step = 0.0f;
    uint32_t gain_frames_left = 0;
    bool stopped = false;
};

struct fd2_bgm_player_impl {
    fd2_audio *audio = nullptr;
    fd2_archive fdmus{};
    std::array<AilInstrument, kMaxInstruments> instruments{};
    BgmVoice voices[2];
    size_t voice_count = kChannels;
    int current_track = -1;
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
        if (offset + 14 > data.size() || rd16(data.data() + offset) < 14)
            return false;
        int index = instrument_index(patch, bank);
        if (index < 0 || static_cast<size_t>(index) >= out.size()) continue;
        const uint8_t *raw = data.data() + offset;
        AilInstrument &inst = out[static_cast<size_t>(index)];
        inst.present = true;
        inst.fixed_note = bank == 0x7f;
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

static Voice *alloc_voice(BgmVoice *voice) {
    for (size_t i = 0; i < voice->voice_count; i++)
        if (!voice->voices[i].active) return &voice->voices[i];
    return &*std::min_element(voice->voices.begin(),
        voice->voices.begin() + voice->voice_count,
        [](const Voice &a, const Voice &b) { return a.age < b.age; });
}

static const AilInstrument &voice_instrument(const BgmVoice *voice,
                                             const Voice &slot) {
    int index = slot.midi_channel == 9
              ? instrument_index(slot.note, 0x7f)
              : instrument_index(slot.patch, voice->banks[slot.midi_channel]);
    if (index < 0 || static_cast<size_t>(index) >= voice->instruments.size() ||
        !voice->instruments[static_cast<size_t>(index)].present)
        index = slot.patch;
    return voice->instruments[static_cast<size_t>(index)];
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

static void opl_key_off(BgmVoice *voice, Voice &slot) {
    OPL3_WriteReg(&voice->opl, static_cast<uint16_t>(0xb0u + slot.opl_channel), 0);
    slot.active = false;
}

static void opl_set_pitch(BgmVoice *voice, Voice &slot,
                          const AilInstrument &inst, bool key_on) {
    double bend = (static_cast<int>(voice->bends[slot.midi_channel]) - 8192) /
                  8192.0 * 2.0;
    int midi_note = static_cast<int>(slot.tone);
    if (!inst.fixed_note) midi_note += static_cast<int>(inst.note_offset);
    midi_note = std::max(0, std::min(127, midi_note));
    uint32_t multiplier_offset = 0;
    uint16_t value = oplModel_ailFreq(midi_note + bend, &multiplier_offset);
    (void)multiplier_offset; /* SAMPLE.AD 当前音域不触发高音倍频补偿。 */
    OPL3_WriteReg(&voice->opl,
                  static_cast<uint16_t>(0xa0u + slot.opl_channel),
                  static_cast<uint8_t>(value & 0xffu));
    OPL3_WriteReg(&voice->opl,
                  static_cast<uint16_t>(0xb0u + slot.opl_channel),
                  static_cast<uint8_t>(((value >> 8) & 0x1fu) |
                                       (key_on ? 0x20u : 0u)));
}

static void opl_program_voice(BgmVoice *voice, Voice &slot,
                              const AilInstrument &inst) {
    uint8_t op = kOperatorOffsets[slot.opl_channel];
    bool additive = (inst.feedback & 1u) != 0;
    uint8_t mod_level = scaled_level(inst.mod_40, slot.velocity,
                                     voice->volumes[slot.midi_channel],
                                     voice->expressions[slot.midi_channel],
                                     additive);
    uint8_t car_level = scaled_level(inst.car_40, slot.velocity,
                                     voice->volumes[slot.midi_channel],
                                     voice->expressions[slot.midi_channel],
                                     true);
    OPL3_WriteReg(&voice->opl, 0x20u + op, inst.mod_20);
    OPL3_WriteReg(&voice->opl, 0x40u + op, mod_level);
    OPL3_WriteReg(&voice->opl, 0x60u + op, inst.mod_60);
    OPL3_WriteReg(&voice->opl, 0x80u + op, inst.mod_80);
    OPL3_WriteReg(&voice->opl, 0xe0u + op, inst.mod_e0);
    OPL3_WriteReg(&voice->opl, 0x23u + op, inst.car_20);
    OPL3_WriteReg(&voice->opl, 0x43u + op, car_level);
    OPL3_WriteReg(&voice->opl, 0x63u + op, inst.car_60);
    OPL3_WriteReg(&voice->opl, 0x83u + op, inst.car_80);
    OPL3_WriteReg(&voice->opl, 0xe3u + op, inst.car_e0);
    /* AIL bank 的 feedback/connection 位在低四位；OPL3 需同时打开 L/R。 */
    OPL3_WriteReg(&voice->opl,
                  static_cast<uint16_t>(0xc0u + slot.opl_channel),
                  static_cast<uint8_t>((inst.feedback & 0x0fu) | 0x30u));
    opl_set_pitch(voice, slot, inst, true);
}

static void seq_note_on(void *userdata, uint8_t channel,
                        uint8_t note, uint8_t velocity) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (channel >= kMidiChannels || velocity == 0) return;
    Voice *slot = alloc_voice(voice);
    uint8_t opl_channel = slot->opl_channel;
    if (slot->active) opl_key_off(voice, *slot);
    *slot = Voice{};
    slot->active = true;
    slot->opl_channel = opl_channel;
    slot->midi_channel = channel;
    slot->note = note;
    slot->velocity = velocity;
    slot->patch = voice->patches[channel];
    slot->age = voice->next_age++;
    const AilInstrument &inst = voice_instrument(voice, *slot);
    slot->tone = inst.fixed_note ? static_cast<uint8_t>(inst.note_offset) : note;
    opl_program_voice(voice, *slot, inst);
}

static void seq_note_off(void *userdata, uint8_t channel, uint8_t note) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    for (Voice &slot : voice->voices)
        if (slot.active && slot.midi_channel == channel && slot.note == note)
            opl_key_off(voice, slot);
}

static void seq_note_off_vel(void *userdata, uint8_t channel,
                             uint8_t note, uint8_t velocity) {
    (void)velocity;
    seq_note_off(userdata, channel, note);
}

static void seq_note_touch(void *, uint8_t, uint8_t, uint8_t) {}
static void seq_channel_touch(void *, uint8_t, uint8_t) {}
static void seq_sysex(void *, const uint8_t *, size_t) {}

static void seq_controller(void *userdata, uint8_t channel,
                           uint8_t type, uint8_t value) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (channel >= kMidiChannels) return;
    if (type == 7) voice->volumes[channel] = value;
    else if (type == 10) voice->pans[channel] = value;
    else if (type == 11) voice->expressions[channel] = value;
    else if (type == 0 || type == 32 || type == 114) voice->banks[channel] = value;
    else if (type == 120 || type == 123) {
        for (Voice &slot : voice->voices)
            if (slot.active && slot.midi_channel == channel) opl_key_off(voice, slot);
    }
    if (type == 7 || type == 11) {
        for (Voice &slot : voice->voices)
            if (slot.active && slot.midi_channel == channel)
                opl_program_voice(voice, slot, voice_instrument(voice, slot));
    }
}

static void seq_patch(void *userdata, uint8_t channel, uint8_t patch) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (channel < kMidiChannels) voice->patches[channel] = patch;
}

static void seq_bend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (channel >= kMidiChannels) return;
    voice->bends[channel] = static_cast<uint16_t>((msb << 7) | lsb);
    for (Voice &slot : voice->voices)
        if (slot.active && slot.midi_channel == channel)
            opl_set_pitch(voice, slot, voice_instrument(voice, slot), true);
}

static void seq_pcm_render(void *userdata, uint8_t *stream, size_t length) {
    (void)stream;
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    size_t frames = length / (sizeof(float) * 2u);
    float *output = voice->render_output;
    for (size_t i = 0; i < frames; i++) {
        int16_t sample[2] = {0, 0};
        OPL3_GenerateResampled(&voice->opl, sample);
        output[i * 2u] = sample[0] / 32768.0f;
        output[i * 2u + 1u] = sample[1] / 32768.0f;
    }
    voice->render_output += frames * 2u;
}

static size_t bgm_render(void *userdata, float *stereo, size_t frames) {
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (!voice || voice->state.load(std::memory_order_acquire) != 1 ||
        voice->stopped) return 0;
    if (voice->stop_requested.exchange(false, std::memory_order_acq_rel)) {
        voice->gain_frames_left = static_cast<uint32_t>(voice->sample_rate * 4);
        voice->gain_step = -voice->sequence_gain / voice->gain_frames_left;
    }
    voice->render_output = stereo;
    int bytes = voice->sequence.playStream(
        reinterpret_cast<uint8_t *>(stereo), frames * sizeof(float) * 2u);
    voice->render_output = nullptr;
    size_t produced = bytes > 0
                    ? static_cast<size_t>(bytes) / (sizeof(float) * 2u) : 0;
    for (size_t i = 0; i < produced; i++) {
        float gain = voice->sequence_gain;
        stereo[i * 2u] *= gain;
        stereo[i * 2u + 1u] *= gain;
        if (voice->gain_frames_left > 0) {
            voice->sequence_gain += voice->gain_step;
            voice->gain_frames_left--;
            if (voice->gain_frames_left == 0) {
                voice->sequence_gain = voice->gain_step < 0.0f ? 0.0f : 1.0f;
                if (voice->gain_step < 0.0f) voice->stopped = true;
            }
        }
    }
    return produced;
}

static void bgm_retire(void *userdata, fd2_audio_retire_reason reason) {
    (void)reason;
    BgmVoice *voice = static_cast<BgmVoice *>(userdata);
    if (voice && voice->magic == kMagic)
        voice->state.store(2, std::memory_order_release);
}

static BgmVoice *claim_voice(fd2_bgm_player_impl *player) {
    for (BgmVoice &voice : player->voices) {
        int state = voice.state.load(std::memory_order_acquire);
        if (state == 2) {
            voice.sequence = BW_MidiSequencer();
            voice.state.store(0, std::memory_order_release);
            state = 0;
        }
        if (state == 0) return &voice;
    }
    return nullptr;
}

static void init_interface(BgmVoice *voice) {
    voice->midi_interface = BW_MidiRtInterface{};
    voice->midi_interface.pcmSampleRate = static_cast<uint32_t>(voice->sample_rate);
    voice->midi_interface.pcmFrameSize = static_cast<uint32_t>(sizeof(float) * 2u);
    voice->midi_interface.onPcmRender = seq_pcm_render;
    voice->midi_interface.onPcmRender_userData = voice;
    voice->midi_interface.rtUserData = voice;
    voice->midi_interface.rt_noteOn = seq_note_on;
    voice->midi_interface.rt_noteOff = seq_note_off;
    voice->midi_interface.rt_noteOffVel = seq_note_off_vel;
    voice->midi_interface.rt_noteAfterTouch = seq_note_touch;
    voice->midi_interface.rt_channelAfterTouch = seq_channel_touch;
    voice->midi_interface.rt_controllerChange = seq_controller;
    voice->midi_interface.rt_patchChange = seq_patch;
    voice->midi_interface.rt_pitchBend = seq_bend;
    voice->midi_interface.rt_systemExclusive = seq_sysex;
    voice->sequence.setInterface(&voice->midi_interface);
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
        !load_ail_bank(config->ail_bank_path, player->instruments)) {
        fd2_archive_close(&player->fdmus);
        delete player;
        return nullptr;
    }
    int rate = config->sample_rate > 0 ? config->sample_rate
                                      : fd2_audio_sample_rate(audio);
    player->voice_count = config->voice_count > 0
                        ? std::min<size_t>(kChannels,
                              static_cast<size_t>(config->voice_count))
                        : kChannels;
    for (BgmVoice &voice : player->voices) {
        voice.sample_rate = rate;
        voice.voice_count = player->voice_count;
        voice.instruments = player->instruments;
    }
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
    BgmVoice *voice = claim_voice(player);
    if (!voice) return -1;
    voice->sequence = BW_MidiSequencer();
    voice->track = static_cast<int>(track);
    voice->loop_count = loop_count;
    voice->voices = {};
    for (size_t i = 0; i < voice->voices.size(); i++)
        voice->voices[i].opl_channel = static_cast<uint8_t>(i);
    OPL3_Reset(&voice->opl, static_cast<uint32_t>(voice->sample_rate));
    /* 原版 MDI.INI 选择 SBLASTER.MDI；音乐使用其 OPL2 九声道路径。 */
    OPL3_WriteReg(&voice->opl, 0x105, 0x00);
    OPL3_WriteReg(&voice->opl, 0x01, 0x20);
    voice->patches.fill(0);
    voice->banks.fill(0);
    /* General MIDI / Miles 默认 channel volume 为 100，CC7 再覆盖。 */
    voice->volumes.fill(100);
    voice->expressions.fill(127);
    voice->pans.fill(64);
    voice->bends.fill(8192);
    voice->next_age = 1;
    voice->stop_requested.store(false, std::memory_order_release);
    voice->stopped = false;
    /* music_track_play @code0 0x15977：普通曲从 0 在 2000 ms 内升到
     * 127；track 16/17 为演出曲，立即满音量。 */
    voice->sequence_gain = (track == 16 || track == 17) ? 1.0f : 0.0f;
    voice->gain_frames_left = voice->sequence_gain == 0.0f
                            ? static_cast<uint32_t>(voice->sample_rate * 2) : 0;
    voice->gain_step = voice->gain_frames_left > 0
                     ? 1.0f / voice->gain_frames_left : 0.0f;
    init_interface(voice);
    /* 循环次数必须在 loadMIDI 前设置；解析完成时 sequencer 才会把
     * m_loopCount 复制进当前 loop state。BW API 接受总播放次数。 */
    voice->sequence.setLoopEnabled(loop_count == 0 || loop_count > 1);
    voice->sequence.setLoopsCount(loop_count == 0 ? -1 : loop_count);
    if (!voice->sequence.loadMIDI(data, size)) return -1;
    voice->state.store(1, std::memory_order_release);
    fd2_audio_source source = {bgm_render, bgm_retire, voice};
    if (fd2_audio_play_music_source(player->audio, source, 1.0f) != 0) {
        voice->state.store(0, std::memory_order_release);
        return -1;
    }
    player->current_track = static_cast<int>(track);
    return 0;
}

int fd2_bgm_stop(fd2_bgm_player *player) {
    if (!player) return -1;
    player->current_track = -1;
    /* music_track_play(-1, ...) @code0 0x159a3：不立即移除 sequence，
     * 而是在 4000 ms 内把 sequence volume 降到 0。 */
    for (BgmVoice &voice : player->voices) {
        if (voice.state.load(std::memory_order_acquire) == 1)
            voice.stop_requested.store(true, std::memory_order_release);
    }
    return 0;
}

int fd2_bgm_stop_immediate(fd2_bgm_player *player) {
    if (!player) return -1;
    player->current_track = -1;
    /* 场景所有权切换不能让上一 UI 的曲目继续可听。STOP_BUS 与下一次
     * PLAY 命令使用同一 command FIFO；同时清空 SDL stream 中已排队的
     * 旧场景采样，避免硬停后仍听到设备前缓冲里的标题曲。 */
    if (fd2_audio_stop_music(player->audio) != 0) return -1;
    return fd2_audio_flush_output(player->audio);
}

int fd2_bgm_current_track(const fd2_bgm_player *player) {
    return player ? player->current_track : -1;
}

size_t fd2_bgm_track_count(const fd2_bgm_player *player) {
    return player ? player->fdmus.count : 0;
}

int fd2_bgm_track_valid(const fd2_bgm_player *player, size_t track) {
    if (!player) return 0;
    const uint8_t *data = nullptr;
    size_t size = 0;
    return fd2_archive_get(&player->fdmus, track, &data, &size) == 0 &&
           size >= 16 && std::memcmp(data, "FORM", 4) == 0 &&
           std::search(data, data + std::min<size_t>(size, 96),
                       reinterpret_cast<const uint8_t *>("XMID"),
                       reinterpret_cast<const uint8_t *>("XMID") + 4) !=
               data + std::min<size_t>(size, 96);
}
