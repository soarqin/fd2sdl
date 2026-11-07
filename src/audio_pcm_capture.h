/* 测试取证：只观察真实设备callback已成功提交的应用混音，不代表设备输出。 */
#ifndef FD2_AUDIO_PCM_CAPTURE_H
#define FD2_AUDIO_PCM_CAPTURE_H
#include <stddef.h>
typedef struct fd2_audio_pcm_capture fd2_audio_pcm_capture;
fd2_audio_pcm_capture *fd2_audio_pcm_capture_open(const char *directory, int rate, unsigned max_ms, int suppress_device_output);
const float *fd2_audio_pcm_capture_submission(fd2_audio_pcm_capture *, const float *mix, size_t frames);
/* 仅audio_stream_callback成功Put之后调用；无I/O、分配、锁或等待。 */
void fd2_audio_pcm_capture_push(fd2_audio_pcm_capture *, const float *stereo, size_t frames);
/* 仅当前取证窗口内的Put失败：复用首错和quiet门；无I/O、分配或等待。
 * 不改播放器首错/恢复语义；窗口未开始或已封闭时不回写旧summary。 */
void fd2_audio_pcm_capture_put_failed(fd2_audio_pcm_capture *);
/* 必须先销毁/停止原SDL stream，随后关闭本observer。 */
void fd2_audio_pcm_capture_close(fd2_audio_pcm_capture *);
#endif
