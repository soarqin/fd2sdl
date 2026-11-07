/* 仅离线测试probe的输出路径门；不进入播放器或音频callback。 */
#ifndef FD2_AUDIO_PROBE_OUTPUT_H
#define FD2_AUDIO_PROBE_OUTPUT_H
#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct fd2_probe_outputs fd2_probe_outputs;
/* 全部输出先共同校验；原目录/实际parent必须存在，不创建目录。 */
fd2_probe_outputs *fd2_probe_outputs_prepare(const char *original,
                                            const char *const *paths, size_t count);
/* 使用已解析parent的全路径独占创建；FILE由调用者fclose。 */
FILE *fd2_probe_output_open(const fd2_probe_outputs *outputs, size_t index);
void fd2_probe_outputs_close(fd2_probe_outputs *outputs);
#ifdef __cplusplus
}
#endif
#endif
