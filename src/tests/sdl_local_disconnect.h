/* 仅本私有探针：依据pinned SDL_sysaudio.h/SDL_audio.c，不进入产品API。 */
#ifndef FD2_PRIVATE_SDL_LOCAL_DISCONNECT_H
#define FD2_PRIVATE_SDL_LOCAL_DISCONNECT_H
#include <SDL3/SDL.h>

typedef struct {
    SDL_AudioDeviceID logical_id, physical_id;
    int ref_acquired, ref_released, owned_before, owned_after;
    int default_logical, disconnected_called, zombie_before, zombie_after;
    int shutdown_before, shutdown_after;
    int wait_function_changed, get_buffer_function_changed, play_function_changed;
} fd2_private_disconnect_report;

/* 只操作调用者独占stream在本进程的SDL对象；不请求Windows断开端点。 */
int fd2_private_disconnect_owned_stream(SDL_AudioStream *stream,
                                      fd2_private_disconnect_report *report);
#endif
