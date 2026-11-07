/* 仅测试：直接复用SDL commit2813f263的原内部removed通知。
 * SDL_audio.c377..443、1474..1490和724..797规定hash/device/stream锁与ref边界。
 * 含真实私有头，不猜struct/ABI；内部头只在此TU启用dynapi_REAL映射。 */
#include "audio/SDL_sysaudio.h"
#include "sdl_local_disconnect.h"
#include <string.h>

typedef struct { SDL_AudioDevice *address; } find_owned_device;

static bool reference_address(SDL_AudioDevice *device, void *userdata) {
    find_owned_device *wanted = userdata;
    if (device != wanted->address) return false;
    /* FindPhysical回调持hash读锁；这里只取ref，不能再取得device锁。 */
    RefPhysicalAudioDevice(device);
    return true;
}

static int owns_stream(SDL_AudioStream *stream, SDL_LogicalAudioDevice *logical,
                       SDL_AudioDevice *physical, SDL_AudioDeviceID id) {
    /* 调用时已按device→stream顺序持锁；不会与SDL render反序。 */
    return stream->bound_device == logical && logical->instance_id == id &&
        SDL_GetAtomicPointer((void **)&logical->physical_device) == physical &&
        logical->simplified && logical->opened_as_default && !physical->recording;
}

int fd2_private_disconnect_owned_stream(SDL_AudioStream *stream,
                                      fd2_private_disconnect_report *report) {
    if (!stream || !report) return -1;
    memset(report, 0, sizeof(*report));
    /* 先仅借地址。解锁后不解引用地址，必须先从hash找到仍存活对象并取ref。 */
    SDL_LockMutex(stream->lock);
    SDL_LogicalAudioDevice *logical = stream->bound_device;
    SDL_AudioDeviceID logical_id = logical ? logical->instance_id : 0;
    find_owned_device wanted = {logical ? SDL_GetAtomicPointer(
        (void **)&logical->physical_device) : NULL};
    SDL_UnlockMutex(stream->lock);
    if (!logical || !logical_id || !wanted.address) return -1;
    SDL_AudioDevice *physical = SDL_FindPhysicalAudioDeviceByCallback(reference_address, &wanted);
    if (!physical) return -1;
    report->ref_acquired = 1;
    report->logical_id = logical_id;
    bool (*old_wait)(SDL_AudioDevice *) = NULL;
    Uint8 *(*old_get)(SDL_AudioDevice *, int *) = NULL;
    bool (*old_play)(SDL_AudioDevice *, const Uint8 *, int) = NULL;
    int ok = 0;

    SDL_LockMutex(physical->lock);
    SDL_LockMutex(stream->lock);
    report->owned_before = owns_stream(stream, logical, physical, logical_id);
    report->physical_id = physical->instance_id;
    report->default_logical = report->owned_before ? logical->opened_as_default : 0;
    report->zombie_before = SDL_GetAtomicInt(&physical->zombie);
    report->shutdown_before = SDL_GetAtomicInt(&physical->shutdown);
    if (report->owned_before && !report->zombie_before && !report->shutdown_before) {
        old_wait = physical->WaitDevice;
        old_get = physical->GetDeviceBuf;
        old_play = physical->PlayDevice;
        ok = old_wait && old_get && old_play;
    }
    SDL_UnlockMutex(stream->lock);
    SDL_UnlockMutex(physical->lock);
    if (!ok) goto done;

    /* 原函数自己取ref和device锁，不能带着任何一个锁进入。 */
    SDL_AudioDeviceDisconnected(physical);
    report->disconnected_called = 1;
    SDL_LockMutex(physical->lock);
    SDL_LockMutex(stream->lock);
    report->owned_after = owns_stream(stream, logical, physical, logical_id);
    report->zombie_after = SDL_GetAtomicInt(&physical->zombie);
    report->shutdown_after = SDL_GetAtomicInt(&physical->shutdown);
    report->wait_function_changed = physical->WaitDevice != old_wait;
    report->get_buffer_function_changed = physical->GetDeviceBuf != old_get;
    report->play_function_changed = physical->PlayDevice != old_play;
    ok = report->owned_after && report->zombie_after == 1 && !report->shutdown_after &&
         report->wait_function_changed && report->get_buffer_function_changed &&
         report->play_function_changed;
    SDL_UnlockMutex(stream->lock);
    SDL_UnlockMutex(physical->lock);
done:
    /* Unref可能销毁对象；必须不持device锁，之后不再借指针。 */
    UnrefPhysicalAudioDevice(physical);
    report->ref_released = 1;
    return ok ? 0 : -1;
}
