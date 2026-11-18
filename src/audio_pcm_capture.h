/* 测试取证：只观察主线程向真实stream成功Put后的应用混音，不代表设备输出。 */
#ifndef FD2_AUDIO_PCM_CAPTURE_H
#define FD2_AUDIO_PCM_CAPTURE_H
#include <stddef.h>
typedef struct fd2_audio_pcm_capture fd2_audio_pcm_capture;
fd2_audio_pcm_capture *fd2_audio_pcm_capture_open(const char *directory, int rate, unsigned max_ms, int suppress_device_output);
const float *fd2_audio_pcm_capture_submission(fd2_audio_pcm_capture *, const float *mix, size_t frames);
/* 仅主线程成功Put之后调用；v2时点不代表实际可听；无I/O、分配、锁或等待。 */
void fd2_audio_pcm_capture_push(fd2_audio_pcm_capture *, const float *stereo, size_t frames);
/* 仅当前取证窗口内的Put失败：复用首错和quiet门；无I/O、分配或等待。
 * 不改播放器首错/恢复语义；窗口未开始或已封闭时不回写旧summary。 */
void fd2_audio_pcm_capture_put_failed(fd2_audio_pcm_capture *);
/* H队列上限丢新chunk；仅当前窗口记backlog，不伪称Put失败。 */
void fd2_audio_pcm_capture_output_dropped(fd2_audio_pcm_capture *);
/* 必须先销毁/停止原SDL stream，随后关闭本observer。 */
void fd2_audio_pcm_capture_close(fd2_audio_pcm_capture *);
#endif
