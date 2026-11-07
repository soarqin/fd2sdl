/* 仅离线Title18寄存器观察；同TU复用真实BGM/Audio/Nuked路径，不改正式callback。
 * 寄存器原语依据bgm.cpp中的SBPRO2 file-offset实现；本文件不声明新FD2 ABI。
 * 宏wrapper原样调用Nuked，固定预分配记录；文件输出仅在main完成render/retire后。
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "audio_probe_output.h"
#include "../third_party/nukedopl3.h"

static void observed_write(opl3_chip *, uint16_t, uint8_t);
static void observed_generate(opl3_chip *, int16_t *);
#ifndef FD2_OPL_PROBE_UNHOOKED
#define OPL3_WriteReg observed_write
#define OPL3_GenerateResampled observed_generate
#endif
#include "../bgm.cpp"
#ifndef FD2_OPL_PROBE_UNHOOKED
#undef OPL3_WriteReg
#undef OPL3_GenerateResampled
#endif

namespace {
constexpr size_t kTraceCapacity = 262144;
constexpr uint64_t kHashBasis = UINT64_C(14695981039346656037);
struct Row { uint64_t sample; uint32_t tick; uint16_t reg; uint8_t value, phase; };
struct Trace {
    Row rows[kTraceCapacity];
    size_t count = 0, limit = kTraceCapacity;
    uint64_t generated = 0;
    bool overflow = false, bad_chip = false;
    uint8_t phase = 0;
    opl3_chip *chip = nullptr;
    fd2_bgm_player *player = nullptr;
};
static Trace trace;
static const char *phases[] = {"install", "render", "retire"};
struct Result {
    uint64_t frames = 0, generated = 0, pcm_hash = kHashBasis, trace_hash = kHashBasis;
    size_t writes = 0, phase_counts[3] = {0, 0, 0};
    uint32_t timer_ticks = 0;
    int sequence_status = 0, sequence_volume = 0;
    bool overflow = false, bad_chip = false, finite = true;
    float peak = 0;
};
static void fold(uint64_t &hash, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        hash ^= static_cast<uint8_t>(value >> (8 * i));
        hash *= UINT64_C(1099511628211);
    }
}
static bool number(const char *value, uint64_t &out) {
    if (!value || !*value || *value == '-') return false;
    char *end = nullptr;
    auto parsed = std::strtoull(value, &end, 10);
    if (!end || *end) return false;
    out = parsed;
    return true;
}
static bool render_case(const char *original, uint32_t rate, uint64_t frames,
                        size_t block, Result &out, size_t limit = kTraceCapacity) {
    trace.count = 0; trace.generated = 0; trace.overflow = false;
    trace.bad_chip = false; trace.phase = 0; trace.chip = nullptr;
    trace.player = nullptr; trace.limit = limit;
    fd2_audio_config config{};
    config.sample_rate = static_cast<int>(rate);
    config.open_device = 0; config.allow_null = 1;
    fd2_audio *audio = fd2_audio_create(&config);
    if (!audio) return false;
    std::string music = std::string(original) + "/FDMUS.DAT";
    std::string bank = std::string(original) + "/SAMPLE.AD";
    fd2_bgm_config bgm{};
    bgm.fdmus_path = music.c_str(); bgm.ail_bank_path = bank.c_str();
    bgm.sample_rate = static_cast<int>(rate);
    fd2_bgm_player *player = fd2_bgm_create(audio, &bgm);
    trace.player = player;
    bool ok = player && !fd2_audio_has_device(audio) && fd2_bgm_play(player, 18, 0) == 0;
    float output[512 * 2];
    trace.phase = 1;
    for (uint64_t done = 0; ok && done < frames;) {
        size_t take = static_cast<size_t>(std::min<uint64_t>(block, frames - done));
        if (fd2_audio_render_offline(audio, output, take) != take) { ok = false; break; }
#ifdef FD2_OPL_PROBE_UNHOOKED
        trace.generated += take; // 控制组不插Nuked wrapper，只计实际offline返回量。
#endif
        for (size_t i = 0; i < take * 2; ++i) {
            uint32_t bits;
            std::memcpy(&bits, output + i, sizeof(bits));
            fold(out.pcm_hash, bits, 4);
            out.finite &= std::isfinite(output[i]);
            out.peak = std::max(out.peak, std::fabs(output[i]));
        }
        done += take; out.frames += take;
    }
    if (player && player->driver.active_sequence) {
        const BgmVoice *active = player->driver.active_sequence;
        out.timer_ticks = active->timer_ticks;
        out.sequence_status = active->sequence.status;
        out.sequence_volume = active->sequence_volume;
    }
    fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
    ok &= fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_NONE;
    trace.phase = 2;
    fd2_audio_destroy(audio); // 原owner顺序：先retire，再释放BGM/archive。
    fd2_bgm_destroy(player);
    trace.player = nullptr;
    out.generated = trace.generated; out.writes = trace.count;
    out.overflow = trace.overflow; out.bad_chip = trace.bad_chip;
    for (size_t i = 0; i < trace.count; ++i) {
        const Row &row = trace.rows[i];
        ++out.phase_counts[row.phase];
        fold(out.trace_hash, row.sample, 8); fold(out.trace_hash, row.tick, 4);
        fold(out.trace_hash, row.reg, 2); fold(out.trace_hash, row.value, 1);
        fold(out.trace_hash, row.phase, 1);
    }
    return ok && out.finite && !out.bad_chip && out.generated == frames && out.frames == frames;
}
static bool outputs(const fd2_probe_outputs *paths, uint32_t rate,
                    size_t block, const Result &result) {
    FILE *stream = fd2_probe_output_open(paths, 0);
    if (!stream) return false;
    bool ok = true;
    for (size_t i = 0; i < trace.count; ++i) {
        const Row &row = trace.rows[i];
        if (std::fprintf(stream,
                "{\"ordinal\":%llu,\"sample\":%llu,\"phase\":\"%s\",\"timer_tick\":%u,\"register\":%u,\"value\":%u}\n",
                static_cast<unsigned long long>(i), static_cast<unsigned long long>(row.sample),
                phases[row.phase], row.tick, static_cast<unsigned>(row.reg),
                static_cast<unsigned>(row.value)) < 0) ok = false;
    }
    if (std::fclose(stream)) ok = false;
    stream = fd2_probe_output_open(paths, 1);
    if (!stream) return false;
    bool hooked = true;
#ifdef FD2_OPL_PROBE_UNHOOKED
    hooked = false;
#endif
    if (std::fprintf(stream,
        "{\"schema\":\"fd2-title18-offline-opl-v1\",\"track\":18,\"sample_rate\":%u,"
        "\"frames\":%llu,\"generated_samples\":%llu,\"block_frames\":%zu,\"hook_enabled\":%s,"
        "\"writes\":%zu,\"overflow\":%s,\"trace_capacity\":%zu,\"timer_ticks\":%u,"
        "\"sequence_status_before_retire\":%d,\"sequence_volume_before_retire\":%d,"
        "\"phase_counts\":{\"install\":%zu,\"render\":%zu,\"retire\":%zu},"
        "\"pcm_fnv1a64\":\"%016llx\",\"trace_fnv1a64\":\"%016llx\",\"peak\":%.9g,"
        "\"device_opened\":false,\"PCM_file_written\":false,\"native_clock_aligned\":false}\n",
        rate, static_cast<unsigned long long>(result.frames),
        static_cast<unsigned long long>(result.generated), block, hooked ? "true" : "false",
        result.writes, result.overflow ? "true" : "false", kTraceCapacity, result.timer_ticks,
        result.sequence_status, result.sequence_volume, result.phase_counts[0], result.phase_counts[1],
        result.phase_counts[2], static_cast<unsigned long long>(result.pcm_hash),
        static_cast<unsigned long long>(result.trace_hash), static_cast<double>(result.peak)) < 0) ok = false;
    if (std::fclose(stream)) ok = false;
    return ok;
}
static bool self_test(const char *original) {
    Result one, block;
    if (!render_case(original, 44100, 22050, 1, one) ||
        !render_case(original, 44100, 22050, 512, block)) return false;
    if (one.pcm_hash != block.pcm_hash || one.trace_hash != block.trace_hash ||
        one.writes != block.writes || one.timer_ticks != block.timer_ticks || one.peak <= 0) return false;
#ifndef FD2_OPL_PROBE_UNHOOKED
    Result bounded;
    if (!render_case(original, 44100, 22050, 511, bounded, 1) || !bounded.overflow ||
        bounded.writes != 1 || bounded.pcm_hash != one.pcm_hash) return false;
#endif
    std::printf("same-runtime block1/512 exact PCM=%016llx trace=%016llx frames=%llu writes=%zu ticks=%u; no device/GUI\n",
        static_cast<unsigned long long>(one.pcm_hash), static_cast<unsigned long long>(one.trace_hash),
        static_cast<unsigned long long>(one.frames), one.writes, one.timer_ticks);
    return true;
}
} // namespace

static void observed_write(opl3_chip *chip, uint16_t reg, uint8_t value) {
    if (!trace.chip) trace.chip = chip;
    if (trace.chip != chip || reg > 0x1ff) trace.bad_chip = true;
    if (trace.count < trace.limit) {
        uint32_t tick = 0;
        if (trace.player && trace.player->driver.active_sequence)
            tick = trace.player->driver.active_sequence->timer_ticks;
        trace.rows[trace.count++] = {trace.generated, tick, reg, value, trace.phase};
    } else trace.overflow = true;
    OPL3_WriteReg(chip, reg, value); // 参数/调用顺序原样，不因trace满而漏真实写。
}
static void observed_generate(opl3_chip *chip, int16_t *output) {
    if (chip != trace.chip) trace.bad_chip = true;
    OPL3_GenerateResampled(chip, output);
    ++trace.generated;
}

int main(int argc, char **argv) {
    const char *original = nullptr, *events = nullptr, *metadata = nullptr;
    uint64_t rate = 44100, frames = 0, block = 512;
    bool test = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--self-test")) test = true;
        else if (!std::strcmp(argv[i], "--original") && i + 1 < argc) original = argv[++i];
        else if (!std::strcmp(argv[i], "--trace") && i + 1 < argc) events = argv[++i];
        else if (!std::strcmp(argv[i], "--metadata") && i + 1 < argc) metadata = argv[++i];
        else if (!std::strcmp(argv[i], "--sample-rate") && i + 1 < argc && number(argv[i + 1], rate)) ++i;
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc && number(argv[i + 1], frames)) ++i;
        else if (!std::strcmp(argv[i], "--block-frames") && i + 1 < argc && number(argv[i + 1], block)) ++i;
        else return 2;
    }
    if (!original || !rate || rate > 192000 || !block || block > 512 ||
        frames > rate * 120 || (test && (events || metadata)) || (!test && (!frames || !events || !metadata))) return 2;
    if (test) return self_test(original) ? 0 : 1;
    const char *paths[] = {events, metadata};
    fd2_probe_outputs *destination = fd2_probe_outputs_prepare(original, paths, 2);
    if (!destination) return 2;
    Result result;
    int status = 1;
    if (render_case(original, static_cast<uint32_t>(rate), frames, static_cast<size_t>(block), result) && !result.overflow)
        status = outputs(destination, static_cast<uint32_t>(rate), static_cast<size_t>(block), result) ? 0 : 1;
    fd2_probe_outputs_close(destination);
    return status;
}
