/* 真正的 X11 窗口／XTest 输入烟测；只在本轮隔离的 Xvfb 中运行。 */
#include <SDL3/SDL.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>

#include "input.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #expr, SDL_GetError()); \
    goto done; \
} } while (0)

int main(void) {
    int status = 1;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    XImage *frame = NULL;
    fd2_input input;
    fd2_input_init(&input);
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    window = SDL_CreateWindow("fd2sdl native smoke", 320, 200, 0);
    CHECK(window && SDL_SyncWindow(window));
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    Display *display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window id = (Window)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    CHECK(display && id);

    renderer = SDL_CreateRenderer(window, "software");
    CHECK(renderer && SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255));
    CHECK(SDL_RenderClear(renderer) && SDL_RenderPresent(renderer));
    frame = XGetImage(display, id, 160, 100, 1, 1, AllPlanes, ZPixmap);
    CHECK(frame && frame->red_mask && XGetPixel(frame, 0, 0) == frame->red_mask);

    XSetInputFocus(display, id, RevertToParent, CurrentTime);
    XSync(display, False);
    fd2_input_poll_host_events(&input);
    KeyCode key = XKeysymToKeycode(display, XK_Return);
    CHECK(key && XTestFakeKeyEvent(display, key, True, CurrentTime));
    CHECK(XTestFakeKeyEvent(display, key, False, CurrentTime));
    XSync(display, False);
    fd2_input_event event = {0};
    for (unsigned attempt = 0; attempt < 200; attempt++) {
        fd2_input_poll_host_events(&input);
        if (fd2_input_take_key(&input, &event)) break;
        SDL_Delay(5);
    }
    CHECK(event.key == FD2_INPUT_KEY_ENTER && event.bios_code == 0x1c0d);
    CHECK(!fd2_input_take_key(&input, NULL));
    key = XKeysymToKeycode(display, XK_Pause);
    CHECK(key && XTestFakeKeyEvent(display, key, True, CurrentTime));
    CHECK(XTestFakeKeyEvent(display, key, False, CurrentTime));
    XSync(display, False);
    fd2_input_poll_host_events(&input);
    CHECK(fd2_input_take_quit(&input));
    status = 0;
done:
    if (frame) XDestroyImage(frame);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
