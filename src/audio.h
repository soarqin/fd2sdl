#ifndef FD2_AUDIO_H
#define FD2_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SDL 音频核心。资源解码器只需实现 render source；设备、bus 增益、
 * voice 调度和离线验证由本层统一管理。 */

typedef enum {
    FD2_AUDIO_BUS_MUSIC = 0,
    FD2_AUDIO_BUS_SFX = 1,
    FD2_AUDIO_BUS_COUNT = 2,
} fd2_audio_bus;

typedef size_t (*fd2_audio_source_render_fn)(void *userdata,
                                              float *stereo,
                                              size_t frames);

typedef enum {
    FD2_AUDIO_RETIRE_FINISHED,
    FD2_AUDIO_RETIRE_STOPPED,
    FD2_AUDIO_RETIRE_REPLACED,
    FD2_AUDIO_RETIRE_STOLEN,
    FD2_AUDIO_RETIRE_DROPPED,
    FD2_AUDIO_RETIRE_DESTROYED,
} fd2_audio_retire_reason;

typedef void (*fd2_audio_source_retire_fn)(void *userdata,
                                            fd2_audio_retire_reason reason);

typedef struct {
    fd2_audio_source_render_fn render;
    fd2_audio_source_retire_fn retire;
    void *userdata;
} fd2_audio_source;

typedef struct {
    int sample_rate;       /* 默认48000；H：整数ns投影要求<=1e9 Hz */
    int open_device;       /* 0=null/offline，非 0=尝试 SDL playback device */
    int allow_null;        /* device 打开失败时是否保留 null backend */
    int original_mixer;    /* 1=Main固定48000共同混音；0保普通diagnostic路径 */
} fd2_audio_config;

typedef struct fd2_audio fd2_audio;

typedef enum {
    FD2_AUDIO_ERROR_NONE,
    FD2_AUDIO_ERROR_OPEN,
    FD2_AUDIO_ERROR_RESUME,
    FD2_AUDIO_ERROR_PUT,
    FD2_AUDIO_ERROR_CLOCK,
    FD2_AUDIO_ERROR_BACKLOG,
    FD2_AUDIO_ERROR_SOURCE,
    FD2_AUDIO_ERROR_OUTPUT, /* 输出接收失败。 */
} fd2_audio_error;

/* 单App主线程owner。live沿已初始化bios_clock epoch，不reset；offline显式
 * 新scope reset(0,0)，两Audio不得并存。Open/Resume失败且allow_null非零时，
 * 保源逻辑但不提交设备，不自动重开。设备迁移仍由SDL负责。
 * sample0创建锚与500ms SDL输入队列上限是H策略，不是原硬件时序结论。 */
fd2_audio *fd2_audio_create(const fd2_audio_config *config);
/* 固定原profile接点；FillUp只用当前共享edge，不sync或推进BIOS。 */
int fd2_audio_is_original_mixer(const fd2_audio *audio);
int fd2_audio_original_fill_up(fd2_audio *audio);
struct fd2_audio_dig;
struct fd2_dig_external_sb16;
int fd2_audio_bind_dig(fd2_audio *audio, struct fd2_audio_dig *dig,
                       struct fd2_dig_external_sb16 *external);
/* 提交旧声音前缀并停止缓冲，Main在clock_close之前调用。 */
int fd2_audio_quiesce_dig(fd2_audio *audio);
void fd2_audio_destroy(fd2_audio *audio);
/* 仅表示持有stream，不保证设备当前可听或不存在历史写流错误。 */
int fd2_audio_has_device(const fd2_audio *audio);
int fd2_audio_sample_rate(const fd2_audio *audio);
/* 读取生命周期首个设备/输出/clock错误，不清除、不关闭或重放source。
 * 返回0；参数无效返回-1且不修改输出。严格create失败仍返回NULL。 */
int fd2_audio_get_first_error(const fd2_audio *audio, fd2_audio_error *error);
/* 同Audio整个生命周期的传输观察；不代表可听、OS真实队列峰或capture窗口。
 * queue_samples仅计GetQueued>=0，peak仅为这些Put之前实际查询的字节峰；
 * successful_puts包括quiet等长零提交；drop只计500ms积压丢新chunk。
 * 首错沿唯一owner保留；getter纯copy，不sync/查询SDL/推进clock，坏参数无写。
 * 同App线程，可在quiesce/clock_close后、AudioDestroy前读取。 */
typedef struct fd2_audio_transport_stats {
    fd2_audio_error first_error;
    uint64_t queue_samples, queue_peak_bytes, successful_puts, backlog_drop_chunks;
} fd2_audio_transport_stats;
int fd2_audio_get_transport_stats(const fd2_audio *audio,fd2_audio_transport_stats *stats);

/* sync沿共享BIOS关闭每个精确IRQ前与最终now的音频前缀，不渲染未来。
 * 0=clock推进成功；输出故障另查首错且不阻源逻辑，-1=owner/clock输入失败。
 * now是当前live SDL输入或offline已到虚拟输入，只读，不推进clock。
 * 仅主线程调用；timer/source callback不得重入控制或sync。 */
int fd2_audio_sync(fd2_audio *audio);
uint64_t fd2_audio_now_ns(const fd2_audio *audio);

/* 参数合法后先sync关闭旧前缀，再同步提交。render/retire均在本主线程，
 * 有界且不可重入Audio控制。source/userdata有效至retire，retire可为空。
 * null、quiet及输出错误不停止render或AIL；500ms积压丢新chunk且记BACKLOG，
 * 保已提交队列，不追补；需后续实测峰值/零丢弃才能签可听反馈。 */
int fd2_audio_play_source(fd2_audio *audio, fd2_audio_bus bus,
                          fd2_audio_source source, float gain);
int fd2_audio_stop_bus(fd2_audio *audio, fd2_audio_bus bus);
/* bus增益同样先关闭旧前缀再同步写入，不另存无时间戳命令。 */
int fd2_audio_set_bus_gain(fd2_audio *audio, fd2_audio_bus bus, float gain);
/* 只读已接受目标，不表示设备已应用。无效参数返回-1且不修改输出。 */
int fd2_audio_get_bus_gain_target(const fd2_audio *audio, fd2_audio_bus bus,
                                  float *gain);
/* MUSIC bus uses one replaceable source, matching music_track_play's
 * single active XMIDI sequence. */
int fd2_audio_play_music_source(fd2_audio *audio, fd2_audio_source source,
                                float gain);
/* 安装失败后的借用撤销：同owner按render/retire/userdata完整身份移除，
 * 退休原因为DROPPED；不advance，不依赖已失败的clock，不清首错。
 * 仅关已到达输出前缀，不是普通stop或整bus取消。返回移除数，非法输入-1。 */
int fd2_audio_abandon_source(fd2_audio *audio,fd2_audio_source source);
int fd2_audio_stop_music(fd2_audio *audio);
int fd2_audio_set_master_gain(fd2_audio *audio, float gain);

/* 仅open_device=0的确定性离线输出，交错float32 stereo。
 * 以同一bios_clock external虚拟输入推进；不读取wall time、不阻塞等设备。
 * 所有公开 API 均要求 fd2_audio 生命周期由同一主线程管理；offline render、
 * producer API 和 destroy 不得互相并发。 */
/* original_mixer离线H工具界：仅ms边界的48N帧，非法量子无副作用；
 * 不缩live、真实partial FillUp或最终原WAV停录域，普通diagnostic不受此限。 */
size_t fd2_audio_render_offline(fd2_audio *audio, float *stereo,
                                size_t frames);

#ifdef __cplusplus
}
#endif

#endif /* FD2_AUDIO_H */
