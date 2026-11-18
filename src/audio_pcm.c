/* 炎龙骑士团2 - 两sample共享DIG正常PCM adapter
 *
 * 逆向依据：sfx_play @0x25a96 从嵌套 LLLLLL bank 取得无头样本地址和
 * 字节长度；ail_init_sample @0x39521 默认 11025 Hz、8-bit mono。
 */

#include "audio_pcm.h"

#include <limits.h>
#include <string.h>

int fd2_pcm_bank_open_mem(fd2_pcm_bank *bank, const uint8_t *data,
                          size_t size) {
    if (!bank) return -1;
    memset(bank, 0, sizeof(*bank));
    if (!data) return -1;
    return fd2_archive_open_mem(&bank->archive, data, size);
}

int fd2_pcm_bank_open(fd2_pcm_bank *bank, const fd2_archive *fdother,
                      size_t entry_index) {
    if (!bank) return -1;
    memset(bank, 0, sizeof(*bank));
    if (!fdother) return -1;
    const uint8_t *data = NULL;
    size_t size = 0;
    if (fd2_archive_get(fdother, entry_index, &data, &size) != 0) return -1;
    return fd2_pcm_bank_open_mem(bank, data, size);
}

void fd2_pcm_bank_close(fd2_pcm_bank *bank) {
    if (!bank) return;
    fd2_archive_close(&bank->archive);
    memset(bank, 0, sizeof(*bank));
}

size_t fd2_pcm_bank_count(const fd2_pcm_bank *bank) {
    return bank ? bank->archive.count : 0;
}

static int shared_valid(const fd2_pcm_player *p) {
    return p && p->driver && p->driver->audio && !p->driver->dig.closed &&
        p->driver->dig.attached && !p->driver->quiesced && p->handle<FD2_DIG_HANDLES;
}
int fd2_pcm_driver_init_external(fd2_pcm_driver *d,fd2_audio *audio,
                                const fd2_dig_external_sb16 *external) {
    if(!d||!audio||!external)return FD2_DIG_H_ARGUMENT;
    if(fd2_audio_sync(audio)!=0)return FD2_DIG_H_CLOCK;
    memset(d,0,sizeof(*d));fd2_audio_dig_init(&d->dig);d->audio=audio;
    return fd2_audio_dig_attach_external(&d->dig,external,fd2_audio_now_ns(audio));
}
int fd2_pcm_driver_init(fd2_pcm_driver *d,fd2_audio *audio) {
    if(!d||!audio||!fd2_audio_is_original_mixer(audio))return FD2_DIG_H_PROFILE;
    if(fd2_audio_sync(audio)!=0)return FD2_DIG_H_CLOCK;
    memset(d,0,sizeof(*d));fd2_audio_dig_init(&d->dig);d->audio=audio;
    fd2_dig_external_sb16 external;
    int result=fd2_audio_bind_dig(audio,&d->dig,&external);
    if(result<0) {fd2_audio_dig_close(&d->dig);d->audio=NULL;return result;}
    d->normal_bound=1; /* Audio借用已建立，即使后续timer安装失败仍由本owner收尾。 */
    result=fd2_audio_dig_attach_external(&d->dig,&external,fd2_audio_now_ns(audio));
    if(result<0)fd2_audio_dig_close(&d->dig); /* 不终结整个Audio，保既有BGM。 */
    return result;
}
int fd2_pcm_driver_quiesce(fd2_pcm_driver *d) {
    if(!d||!d->audio||d->quiesced)return 0;
    if(!d->normal_bound)return FD2_DIG_H_SB16_TIMELINE;
    int result=fd2_audio_quiesce_dig(d->audio);d->quiesced=1;return result;
}
void fd2_pcm_driver_close(fd2_pcm_driver *d) {
    if(!d)return;
    fd2_audio_dig_close(&d->dig);d->audio=NULL;
}
int fd2_pcm_player_init_shared(fd2_pcm_player *p,fd2_pcm_driver *d,
                               unsigned handle,int rate) {
    if(!p||!d||!d->audio||!d->dig.attached||d->dig.closed||handle>=FD2_DIG_HANDLES)
        return FD2_DIG_H_ARGUMENT;
    if(rate!=11025)return FD2_DIG_H_PROFILE;
    memset(p,0,sizeof(*p));p->driver=d;p->handle=handle;p->source_rate=rate;
    return 0;
}
static int shared_play(fd2_pcm_player *p,const fd2_pcm_bank *bank,size_t index,int loops,float gain) {
    if(!shared_valid(p))return FD2_DIG_H_ARGUMENT;
    if(!p->driver->dig.enabled)return 0;
    if(!bank||loops<0)return FD2_DIG_H_ARGUMENT;
    if(gain!=FD2_DIG_DEFAULT_U8_SCALE/256.0f)return FD2_DIG_H_PROFILE;
    if(fd2_audio_sync(p->driver->audio)!=0)return FD2_DIG_H_CLOCK;
    int result=fd2_audio_dig_stop(&p->driver->dig,p->handle);if(result<0)return result;
    const uint8_t *data=NULL;size_t length=0;
    /* 原25a96先stop8，再取source；错误保stop及已有DMA，不能倒序claim voice。 */
    if(fd2_archive_get(&bank->archive,index,&data,&length)!=0||!length||length>UINT32_MAX)
        return FD2_DIG_H_ARGUMENT;
    return fd2_audio_dig_start(&p->driver->dig,p->handle,data,(uint32_t)length,(uint32_t)loops);
}

int fd2_pcm_player_init(fd2_pcm_player *p,fd2_audio *audio,int source_rate) {
    (void)p;(void)audio;(void)source_rate;return FD2_DIG_H_SHARED_DRIVER_REQUIRED;
}
int fd2_pcm_player_set_enabled(fd2_pcm_player *p,int enabled) {
    if(!shared_valid(p)||(enabled!=0&&enabled!=1))return FD2_DIG_H_ARGUMENT;
    p->driver->dig.enabled=enabled;return 0;
}
int fd2_pcm_play(fd2_pcm_player *p,const fd2_pcm_bank *bank,size_t index,int loops,float gain) {
    return shared_play(p,bank,index,loops,gain);
}
int fd2_pcm_play_replace(fd2_pcm_player *p,const fd2_pcm_bank *bank,size_t index,int loops,float gain) {
    return shared_play(p,bank,index,loops,gain);
}
int fd2_pcm_stop(fd2_pcm_player *p) {
    if(!shared_valid(p))return FD2_DIG_H_ARGUMENT;
    if(!p->driver->dig.enabled)return 0;
    if(fd2_audio_sync(p->driver->audio)!=0)return FD2_DIG_H_CLOCK;
    return fd2_audio_dig_stop(&p->driver->dig,p->handle);
}
void fd2_pcm_player_close(fd2_pcm_player *p) {if(p)memset(p,0,sizeof(*p));}
