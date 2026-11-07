/* 私有adapter边界：真实SDL私有struct，仅替换锁/查找/ref/断连函数；无SDL初始化。 */
#include "audio/SDL_sysaudio.h"
#include <stdio.h>
#include <string.h>

static void SDLCALL fake_lock(SDL_Mutex *lock);
static void SDLCALL fake_unlock(SDL_Mutex *lock);
static SDL_AudioDevice *fake_find(bool (*callback)(SDL_AudioDevice *, void *), void *userdata);
static void fake_ref(SDL_AudioDevice *device);
static void fake_unref(SDL_AudioDevice *device);
static void fake_disconnect(SDL_AudioDevice *device);
#undef SDL_LockMutex
#undef SDL_UnlockMutex
#define SDL_LockMutex fake_lock
#define SDL_UnlockMutex fake_unlock
#define SDL_FindPhysicalAudioDeviceByCallback fake_find
#define RefPhysicalAudioDevice fake_ref
#define UnrefPhysicalAudioDevice fake_unref
#define SDL_AudioDeviceDisconnected fake_disconnect
#include "sdl_local_disconnect.c"

static SDL_AudioStream stream;
static SDL_LogicalAudioDevice logical;
static SDL_AudioDevice physical, another;
static unsigned char stream_lock, device_lock;
static int held_stream, held_device, in_hash, refs, disconnects, invalid;
static int absent, migrate, unbind_after_find, no_switch;

static bool wait_old(SDL_AudioDevice *p) { (void)p; return true; }
static bool wait_new(SDL_AudioDevice *p) { (void)p; return true; }
static Uint8 *get_old(SDL_AudioDevice *p, int *n) { (void)p; (void)n; return NULL; }
static Uint8 *get_new(SDL_AudioDevice *p, int *n) { (void)p; (void)n; return NULL; }
static bool play_old(SDL_AudioDevice *p, const Uint8 *b, int n) { (void)p; (void)b; (void)n; return true; }
static bool play_new(SDL_AudioDevice *p, const Uint8 *b, int n) { (void)p; (void)b; (void)n; return true; }

static void SDLCALL fake_lock(SDL_Mutex *lock) {
    if (in_hash) invalid++;
    if (lock == stream.lock) held_stream++;
    else if (lock == physical.lock) {
        if (held_stream) invalid++; /* 禁止stream→device反序。 */
        held_device++;
    } else invalid++;
}
static void SDLCALL fake_unlock(SDL_Mutex *lock) {
    if (lock == stream.lock) held_stream--;
    else if (lock == physical.lock) held_device--;
    else invalid++;
    if (held_stream < 0 || held_device < 0) invalid++;
}
static SDL_AudioDevice *fake_find(bool (*callback)(SDL_AudioDevice *, void *), void *userdata) {
    if (held_stream || held_device) invalid++;
    in_hash = 1;
    int found = !absent && callback(&physical, userdata);
    in_hash = 0;
    if (migrate) logical.physical_device = &another;
    if (unbind_after_find) stream.bound_device = NULL;
    return found ? &physical : NULL;
}
static void fake_ref(SDL_AudioDevice *device) {
    if (device != &physical || !in_hash || held_stream || held_device) invalid++;
    refs++;
}
static void fake_unref(SDL_AudioDevice *device) {
    if (device != &physical || in_hash || held_stream || held_device) invalid++;
    refs--;
}
static void fake_disconnect(SDL_AudioDevice *device) {
    if (device != &physical || in_hash || held_stream || held_device || refs != 1) invalid++;
    disconnects++;
    SDL_SetAtomicInt(&device->zombie, 1);
    if (!no_switch) {
        device->WaitDevice = wait_new;
        device->GetDeviceBuf = get_new;
        device->PlayDevice = play_new;
    }
}
static void reset(void) {
    memset(&stream, 0, sizeof(stream)); memset(&logical, 0, sizeof(logical));
    memset(&physical, 0, sizeof(physical)); memset(&another, 0, sizeof(another));
    stream.lock = (SDL_Mutex *)&stream_lock; stream.bound_device = &logical;
    logical.instance_id = 101; logical.physical_device = &physical;
    logical.simplified = true; logical.opened_as_default = true;
    physical.lock = (SDL_Mutex *)&device_lock; physical.instance_id = 103;
    physical.WaitDevice = wait_old; physical.GetDeviceBuf = get_old; physical.PlayDevice = play_old;
    held_stream = held_device = in_hash = refs = disconnects = invalid = absent = migrate = unbind_after_find = no_switch = 0;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "failed %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
int main(void) {
    fd2_private_disconnect_report row;
    reset(); CHECK(fd2_private_disconnect_owned_stream(&stream, &row) == 0);
    CHECK(row.owned_before && row.owned_after && row.ref_acquired && row.ref_released);
    CHECK(row.zombie_after == 1 && row.wait_function_changed && row.get_buffer_function_changed && row.play_function_changed);
    CHECK(disconnects == 1 && !refs && !invalid && !held_stream && !held_device);
    reset(); stream.bound_device = NULL; CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(!disconnects && !refs && !invalid);
    reset(); absent = 1; CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(!disconnects && !row.ref_acquired && !refs && !invalid);
    reset(); migrate = 1; CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(!row.owned_before && !row.default_logical && !disconnects);
    CHECK(row.ref_acquired && row.ref_released && !refs && !invalid && !held_stream && !held_device);
    reset(); unbind_after_find = 1; CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(!stream.bound_device && !row.owned_before && !row.default_logical && !disconnects);
    CHECK(row.ref_acquired && row.ref_released && !refs && !invalid && !held_stream && !held_device);
    reset(); logical.opened_as_default = false; CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(!disconnects && !refs && !invalid);
    reset(); SDL_SetAtomicInt(&physical.zombie, 1); CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(!disconnects && !refs && !invalid);
    reset(); SDL_SetAtomicInt(&physical.shutdown, 1); CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(!disconnects && !refs && !invalid);
    reset(); no_switch = 1; CHECK(fd2_private_disconnect_owned_stream(&stream, &row) < 0);
    CHECK(disconnects == 1 && !row.wait_function_changed && !refs && !invalid);
    puts("private SDL local disconnect adapter: 9 pure groups; no SDL_Init/device/window");
    return 0;
}
