/* 正式 SDL 战场输入回归的 X11/XTest 驱动。
 * 只发送真实按键，不调用 field_game 状态 API；由 field_play_input_test.sh
 * 启动 --field-play 0 并检查生产循环日志。
 */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int tap(Display *display, KeySym symbol, useconds_t delay_us) {
    KeyCode code = XKeysymToKeycode(display, symbol);
    if (code == 0) return -1;
    XTestFakeKeyEvent(display, code, True, CurrentTime);
    XFlush(display);
    usleep(50000);
    XTestFakeKeyEvent(display, code, False, CurrentTime);
    XFlush(display);
    usleep(delay_us);
    return 0;
}

static int tap_many(Display *display, KeySym symbol, size_t count,
                    useconds_t delay_us) {
    for (size_t i = 0; i < count; i++) {
        if (tap(display, symbol, delay_us) != 0) return -1;
    }
    return 0;
}

static int finish_wait(Display *display, int nonzero_down) {
    if (tap(display, XK_Return, 350000) != 0) return -1;
    if (nonzero_down && tap(display, XK_Down, 180000) != 0) return -1;
    if (tap(display, XK_Return, nonzero_down ? 850000 : 500000) != 0 ||
        tap(display, XK_Down, 250000) != 0 ||
        tap(display, XK_Return, 650000) != 0)
        return -1;
    return 0;
}

static int drive_wait_round(Display *display) {
    /* Z 先按 actor 表聚焦 unit 0；(7,20) → (7,21) 非零移动后 wait。 */
    if (tap(display, XK_z, 250000) != 0 ||
        finish_wait(display, 1) != 0 ||
        tap(display, XK_Return, 700000) != 0 ||
        tap(display, XK_Return, 700000) != 0)
        return -1;

    /* focus-cycle 会跳过 acted 单位，依次聚焦其余三名玩家。 */
    if (tap(display, XK_z, 250000) != 0 ||
        finish_wait(display, 0) != 0 ||
        tap(display, XK_z, 250000) != 0 ||
        finish_wait(display, 0) != 0 ||
        tap(display, XK_z, 250000) != 0 ||
        finish_wait(display, 0) != 0)
        return -1;
    return 0;
}

static int drive_attack(Display *display) {
    /* Z 连按三次聚焦 unit 2；从 (8,22) 移到 (5,22)，攻击 (3,22)。 */
    if (tap_many(display, XK_z, 3, 180000) != 0 ||
        tap(display, XK_Return, 250000) != 0 ||
        tap_many(display, XK_Left, 3, 160000) != 0 ||
        tap(display, XK_Return, 1000000) != 0 ||
        tap(display, XK_Up, 250000) != 0 ||
        tap(display, XK_Return, 350000) != 0 ||
        tap_many(display, XK_Left, 2, 180000) != 0 ||
        tap(display, XK_Return, 1200000) != 0)
        return -1;

    /* 攻击后回到攻击者格再次确认，只能触发 acted gate／详情。 */
    if (tap_many(display, XK_Right, 2, 180000) != 0 ||
        tap(display, XK_Return, 900000) != 0)
        return -1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2 ||
        (strcmp(argv[1], "wait") != 0 && strcmp(argv[1], "attack") != 0)) {
        fprintf(stderr, "usage: %s wait|attack\n", argv[0]);
        return 2;
    }
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "cannot open X display\n");
        return 1;
    }
    int result = strcmp(argv[1], "wait") == 0
        ? drive_wait_round(display) : drive_attack(display);
    XCloseDisplay(display);
    return result == 0 ? 0 : 1;
}
