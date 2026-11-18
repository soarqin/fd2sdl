#ifndef FD2_BGM_H
#define FD2_BGM_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "audio.h"

/* 原版 music_track_play @0x25977 的单 sequence 播放器。
 * 同Audio的120Hz服务推进事件、循环和整数渐变，Nuked实例跨换曲保留。
 * FDMUS.DAT 继续作为 XMIDI 数据源，SAMPLE.AD 继续作为 AIL/AdLib
 * 音色表；SDL 只接收最终 float32 stereo。 */
typedef struct fd2_bgm_player fd2_bgm_player;
/* current事件资格与历史首gap分开；canonical PC是来源边界标签，不是guest采样。
 * COMPAT_REPLAY表示空条目重播同S已解码score的表现兼容，非原内部owner证明。
 * UNKNOWN仅用于首次无score；保UI与已有chip，不凭空产生曲谱。 */
typedef enum { FD2_BGM_EVENTS_NO_VALID_INIT,FD2_BGM_EVENTS_QUALIFIED,
    FD2_BGM_EVENTS_SOURCE_UNQUALIFIED,FD2_BGM_EVENTS_COMPAT_REPLAY } fd2_bgm_event_state;
typedef enum { FD2_BGM_GAP_NONE,FD2_BGM_GAP_EVNT_OWNER_AFTER_RESOURCE_REPLACE,
    FD2_BGM_GAP_INITIAL_EVNT_FIELD_PRODUCER_UNPROVEN } fd2_bgm_gap_reason;
typedef struct fd2_bgm_qualification {
    fd2_bgm_event_state current_events;
    int current_track,current_barrier_reached;
    fd2_bgm_gap_reason first_gap_reason;
    int first_gap_track;
    uint32_t first_gap_pc;
    int first_event_read_blocked,first_event_track;
    uint32_t first_event_read_pc;
} fd2_bgm_qualification;


typedef struct {
    const char *fdmus_path;
    const char *ail_bank_path;
    int sample_rate;
    int voice_count; /* 0=已配置 SBPRO2.MDI 的 18 个 OPL3 硬件 voice */
} fd2_bgm_config;

#ifdef __cplusplus
extern "C" {
#endif

/* 同App线程，bios_clock已attach；创建在该owner安装MDI服务，后装DIG。
 * 销毁前先close App clock、AudioDestroy退休source，再释放本driver借用。 */
fd2_bgm_player *fd2_bgm_create(fd2_audio *audio,
                               const fd2_bgm_config *config);
void fd2_bgm_destroy(fd2_bgm_player *player);

/* loop_count 遵循 AIL sequence 语义：0=无限循环，正数=总播放次数。
 * 同一cache请求早返；不同合法请求先写唯一cache，host错误保已达前缀。
 * recognized 20 0d 0a空条目继续UI；有上一份已解码score时从其开头重播，
 * 同S/同DAT owner不新增曲目副本；首次无score仍显名unknown，其它坏资源仍负。
 * current_track是请求缓存；COMPAT_REPLAY音符来自上一score。valid恢复不清历史gap。
 * 唯一chip source在创建后常驻，直到AudioDestroy才退休。 */
int fd2_bgm_play(fd2_bgm_player *player, size_t track, int loop_count);
/* music_track_play @0x25977（原登记 FUN_0004ab8b）的 -1 分支：
 * 请求原版4秒 sequence-volume 渐变；不承诺 PCM 全零或 sequence 卸载。 */
int fd2_bgm_stop(fd2_bgm_player *player);
/* 51E61：fade_current=0仅同步Load配置，=1复现1728c的1秒toggle渐变。
 * 新曲按当时flag初始化；同曲play仍早退，不借bus硬静音。主线程调用。 */
int fd2_bgm_set_music_enabled(fd2_bgm_player *player, int enabled, int fade_current);
/* 同App owner纯copy，坏参数无写。可在Audio退休后、BgmDestroy前读取。
 * 首provenance gap447ae与后来被阻止的event读433b0分账；不称实际guestPC。 */
int fd2_bgm_get_qualification(const fd2_bgm_player *player,fd2_bgm_qualification *out);
int fd2_bgm_current_track(const fd2_bgm_player *player);

/* 测试和资源验证接口。 */
size_t fd2_bgm_track_count(const fd2_bgm_player *player);
int fd2_bgm_track_valid(const fd2_bgm_player *player, size_t track);

#ifdef __cplusplus
}
#endif

#endif /* FD2_BGM_H */
