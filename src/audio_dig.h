/* 完整404c0/49690/49891与SB16.DIG normal401/402；固定U8 mono22050。
 * 有限控制核，显式external SB provider；无输出renderer、DMA理想时钟或FIFO。 */
#ifndef FD2_AUDIO_DIG_H
#define FD2_AUDIO_DIG_H
#include <stdint.h>
#include "bios_clock.h"
#ifdef __cplusplus
extern "C" {
#endif

enum { FD2_DIG_HANDLES=2, FD2_DIG_HALF=2048, FD2_DIG_DEFAULT_U8_SCALE=202 };
enum { FD2_DIG_OK=0, FD2_DIG_H_ARGUMENT=-1, FD2_DIG_H_SB16_TIMELINE=-2,
       FD2_DIG_H_CLOCK=-3, FD2_DIG_H_SB16_READ=-4,
       FD2_DIG_H_SB16_START=-5, FD2_DIG_H_SB16_PAUSE=-6,
       FD2_DIG_H_SHARED_DRIVER_REQUIRED=-7, FD2_DIG_H_PROFILE=-8 };
typedef struct fd2_dig_sample {
    const uint8_t *data;
    uint32_t length, position, loop_count, status;
} fd2_dig_sample;
/* 三叶只表示原正常调用完成或一次half读取。0为完整完成；负值为宿主H。
 * start完成fresh/resume，read_half给-1/0/1，pause请求下次SB IRQ停止。
 * 提供者负责唯一SB DMA/IRQ时间域；不能从host frames直接猜half。
 * 这些回调/非NULL指针本身不是原timeline已资格的证据。 */
typedef struct fd2_dig_external_sb16 {
    void *user;
    int (*start)(void *user,const uint8_t *dma,uint32_t total_bytes);
    int (*read_half)(void *user,int16_t *half);
    int (*request_pause)(void *user);
} fd2_dig_external_sb16;
typedef struct fd2_audio_dig {
    fd2_dig_sample samples[FD2_DIG_HANDLES];
    int32_t build[FD2_DIG_HALF];
    uint8_t dma[FD2_DIG_HANDLES][FD2_DIG_HALF];
    uint32_t playing, empty_services, entered_active;
    int16_t last_half;
    int enabled, attached, closed, first_error;
    fd2_ail_timer *timer;
    uint32_t timer_handle;
    fd2_dig_external_sb16 sb16;
} fd2_audio_dig;

void fd2_audio_dig_init(fd2_audio_dig *dig);
/* parent在MDI安装完成之后调用；借同一bios owner安装DIG200。NULL/缺叶H
 * 不安装假timer。正式PCM从Audio的唯一SB provider取得此借用；测试可显式
 * 提供有限替身，其资格与正式设备/PIC分账。 */
int fd2_audio_dig_attach_external(fd2_audio_dig *dig,
    const fd2_dig_external_sb16 *provider,uint64_t now_ns);
/* 先在现Audio owner关闭旧前缀，再调用这些同步控制；不自读宿主时间。 */
int fd2_audio_dig_stop(fd2_audio_dig *dig,unsigned handle);
int fd2_audio_dig_start(fd2_audio_dig *dig,unsigned handle,
    const uint8_t *data,uint32_t length,uint32_t loop_count);
/* 生产由唯一DIG timer调用；公开有限叶也用于显式external状态向量。 */
int fd2_audio_dig_service(fd2_audio_dig *dig);
void fd2_audio_dig_close(fd2_audio_dig *dig);
#ifdef __cplusplus
}
#endif
#endif
