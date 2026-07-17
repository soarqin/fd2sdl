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
    int sample_rate;       /* 默认 48000 Hz */
    int open_device;       /* 0=null/offline，非 0=尝试 SDL playback device */
    int allow_null;        /* device 打开失败时是否保留 null backend */
} fd2_audio_config;

typedef struct fd2_audio fd2_audio;

fd2_audio *fd2_audio_create(const fd2_audio_config *config);
void fd2_audio_destroy(fd2_audio *audio);
int fd2_audio_has_device(const fd2_audio *audio);
int fd2_audio_sample_rate(const fd2_audio *audio);

/* 单生产者命令入口。render/retire 可能在实时音频线程执行，必须有界且禁止
 * 文件 I/O、动态分配、锁等待和阻塞。source 及 userdata 必须保持有效，直至
 * retire 通知；retire 只能发布可回收状态，不能直接释放可能仍被其他线程观察
 * 的对象。retire 可为空，表示 userdata 在整个 audio 生命周期内保持有效。 */
int fd2_audio_play_source(fd2_audio *audio, fd2_audio_bus bus,
                          fd2_audio_source source, float gain);
int fd2_audio_stop_bus(fd2_audio *audio, fd2_audio_bus bus);
int fd2_audio_set_bus_gain(fd2_audio *audio, fd2_audio_bus bus, float gain);
/* MUSIC bus uses one replaceable source, matching music_track_play's
 * single active XMIDI sequence. */
int fd2_audio_play_music_source(fd2_audio *audio, fd2_audio_source source,
                                float gain);
int fd2_audio_stop_music(fd2_audio *audio);
int fd2_audio_set_master_gain(fd2_audio *audio, float gain);

/* 仅用于 null backend 的确定性测试／离线 capture。输出为交错 float32 stereo。
 * 所有公开 API 均要求 fd2_audio 生命周期由同一主线程管理；offline render、
 * producer API 和 destroy 不得互相并发。 */
size_t fd2_audio_render_offline(fd2_audio *audio, float *stereo,
                                size_t frames);

#ifdef __cplusplus
}
#endif

#endif /* FD2_AUDIO_H */
