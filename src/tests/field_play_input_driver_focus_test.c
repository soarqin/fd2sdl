/* HWND后台输入防护：依据Win32 WS_EX_NOACTIVATE和原driver投递协议。
 * 仅宏替换本TU所调用的Win32函数，不创建窗口／进程或发送真实消息。 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static HWND test_window = (HWND)(uintptr_t)0x1234, foreground;
static LONG_PTR exstyle;
static DWORD fake_owner_pid;
static int set_calls, down_count, up_count, closed, sleep150, sleep250;
static int set_error, readback_missing, change_owner, focus_on_down, focus_on_hold;
static int fail_up, exited, open_error;
static WORD down_keys[2], up_keys[2];
static ULONGLONG ticks;

static HWND WINAPI fake_foreground(void) { return foreground; }
static BOOL WINAPI fake_iswindow(HWND window) { return window == test_window; }
static DWORD WINAPI fake_owner(HWND window, LPDWORD pid) {
    if (window != test_window) return 0;
    *pid = fake_owner_pid;
    return 123;
}
static LONG_PTR WINAPI fake_get_style(HWND window, int index) {
    if (window != test_window || index != GWL_EXSTYLE) { SetLastError(5); return 0; }
    return readback_missing && set_calls ? exstyle & ~WS_EX_NOACTIVATE : exstyle;
}
static LONG_PTR WINAPI fake_set_style(HWND window, int index, LONG_PTR value) {
    LONG_PTR previous = exstyle;
    ++set_calls;
    if (window != test_window || index != GWL_EXSTYLE || set_error) {
        SetLastError(5); return 0;
    }
    exstyle = value;
    if (change_owner) fake_owner_pid = 99;
    return previous;
}
static HANDLE WINAPI fake_open(DWORD access, BOOL inherit, DWORD pid) {
    (void)access; (void)inherit;
    if (pid != 12345 || open_error) { SetLastError(5); return NULL; }
    return (HANDLE)(uintptr_t)0x5555;
}
static BOOL WINAPI fake_close(HANDLE handle) { (void)handle; ++closed; return TRUE; }
static DWORD WINAPI fake_wait(HANDLE handle, DWORD ms) {
    (void)handle; (void)ms; return exited ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}
static BOOL WINAPI fake_exit(HANDLE handle, LPDWORD code) { (void)handle; *code = 0; return TRUE; }
static BOOL WINAPI fake_path(HANDLE handle, DWORD flags, LPWSTR path, PDWORD count) {
    const wchar_t name[] = L"C:\\owned\\dosbox-x.exe";
    (void)handle; (void)flags;
    if (*count < sizeof(name) / sizeof(*name)) return FALSE;
    memcpy(path, name, sizeof(name)); *count = (DWORD)wcslen(name); return TRUE;
}
static BOOL WINAPI fake_post(HWND window, UINT message, WPARAM symbol, LPARAM parameter) {
    (void)parameter;
    if (window != test_window) { SetLastError(5); return FALSE; }
    if (message == WM_KEYDOWN) {
        if (down_count < 2) down_keys[down_count] = (WORD)symbol;
        ++down_count;
        if (focus_on_down) foreground = test_window;
    } else if (message == WM_KEYUP) {
        if (fail_up) { SetLastError(5); return FALSE; }
        if (up_count < 2) up_keys[up_count] = (WORD)symbol;
        ++up_count;
    } else return FALSE;
    return TRUE;
}
static VOID WINAPI fake_sleep(DWORD ms) {
    if (ms == 150) { ++sleep150; if (focus_on_hold) foreground = test_window; }
    if (ms == 250) ++sleep250;
    ticks += ms;
}
static ULONGLONG WINAPI fake_ticks(void) { return ++ticks; }

#define GetForegroundWindow fake_foreground
#define IsWindow fake_iswindow
#define GetWindowThreadProcessId fake_owner
#define GetWindowLongPtrW fake_get_style
#define SetWindowLongPtrW fake_set_style
#define OpenProcess fake_open
#define CloseHandle fake_close
#define WaitForSingleObject fake_wait
#define GetExitCodeProcess fake_exit
#define QueryFullProcessImageNameW fake_path
#define PostMessageW fake_post
#define Sleep fake_sleep
#define GetTickCount64 fake_ticks
#define main field_play_input_driver_main
#include "field_play_input_driver.c"
#undef main

int fd2_capture_window(HWND window, const char *path) { (void)window; (void)path; return -1; }
int fd2_capture_window_series(HWND window, DWORD pid, const wchar_t *directory,
                              unsigned long ms, unsigned long interval) {
    (void)window; (void)pid; (void)directory; (void)ms; (void)interval; return -1;
}

static void reset(void) {
    foreground = (HWND)(uintptr_t)0xabcd;
    exstyle = WS_EX_APPWINDOW | WS_EX_LAYERED;
    fake_owner_pid = 12345; ticks = 1000;
    set_calls = down_count = up_count = closed = sleep150 = sleep250 = 0;
    set_error = readback_missing = change_owner = focus_on_down = focus_on_hold = 0;
    fail_up = exited = open_error = 0;
    memset(down_keys, 0, sizeof(down_keys)); memset(up_keys, 0, sizeof(up_keys));
    SetLastError(0);
}
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "focus guard test:%d: %s\n", __LINE__, #expr); return 1; } } while (0)

int main(void) {
    InputTarget target = { (HWND)(uintptr_t)0x1234, 12345, 1 };
    LPARAM down, up;
    reset();
    CHECK(tap(&target, VK_RETURN, 250000) == 0);
    CHECK(exstyle == (WS_EX_APPWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE) && set_calls == 1);
    CHECK(tap(&target, VK_RETURN, 250000) == 0 && set_calls == 1);
    reset(); exstyle = 0;
    CHECK(tap(&target, VK_RETURN, 250000) == 0 && exstyle == WS_EX_NOACTIVATE);

    reset(); set_error = 1;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && down_count == 0 && up_count == 0 && closed == 1);
    reset(); readback_missing = 1;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && down_count == 0);
    reset(); change_owner = 1;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && down_count == 0);
    reset(); fake_owner_pid = 99;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && set_calls == 0 && down_count == 0);
    reset(); target.windows_found = 2;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && set_calls == 0 && down_count == 0);
    target.windows_found = 1;

    reset(); foreground = test_window;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && set_calls == 0 && down_count == 0 && up_count == 0);
    reset(); focus_on_hold = 1;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && down_count == 1 && up_count == 1);
    CHECK(down_keys[0] == VK_RETURN && up_keys[0] == VK_RETURN && sleep150 == 1 && sleep250 == 1);
    reset(); focus_on_down = 1;
    CHECK(tap_native_wave_record(&target) == -1 && down_count == 1 && up_count == 1);
    CHECK(down_keys[0] == VK_F11 && up_keys[0] == VK_F11);
    reset(); focus_on_hold = 1;
    CHECK(tap_native_wave_record(&target) == -1 && down_count == 2 && up_count == 2);
    CHECK(up_keys[0] == 'W' && up_keys[1] == VK_F11);

    reset(); fail_up = 1;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && down_count == 1 && up_count == 0);
    reset(); exited = 1;
    CHECK(tap(&target, VK_RETURN, 250000) == 0 && down_count == 1 && up_count == 0);
    reset(); focus_on_hold = exited = 1;
    CHECK(tap(&target, VK_RETURN, 250000) == -1 && down_count == 1 && up_count == 0);

    reset();
    CHECK(tap(&target, VK_RETURN, 250000) == 0 && down_count == 1 && up_count == 1);
    CHECK(sleep150 == 1 && sleep250 == 1 && closed == 1);
    CHECK(exstyle == (WS_EX_APPWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE));
    CHECK(key_message_lparam(VK_RETURN, 1, &down) == 0 && key_message_lparam(VK_RETURN, 0, &up) == 0);
    CHECK(((DWORD)down & (3u << 30)) == 0 && ((DWORD)up & (3u << 30)) == (3u << 30));
    puts("focus guard checks passed (no windows or real messages)");
    return 0;
}
