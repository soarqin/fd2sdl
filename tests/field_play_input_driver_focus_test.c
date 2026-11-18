/* HWND后台输入防护：依据Win32 WS_EX_NOACTIVATE和原driver投递协议。
 * 仅宏替换本TU所调用的Win32函数，不创建窗口／进程或发送真实消息。 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

static HWND test_window = (HWND)(uintptr_t)0x1234, foreground;
static LONG_PTR exstyle;
static DWORD fake_owner_pid;
static int set_calls, down_count, up_count, closed, sleep150, sleep250;
static int set_error, readback_missing, change_owner, focus_on_down, focus_on_hold;
static int fail_up, exited, open_error;
static int up_attempts, fail_first_up, wait_error;
static int hold_focus, hold_owner, hold_exit;
static WORD down_keys[2], up_keys[2];
static ULONGLONG ticks;
static LONGLONG qpc_clock, fake_qpc_frequency, qpc_step, qpc_last_sample;
static LONGLONG posted_down_qpc, posted_up_qpc;
static int qpc_calls, frequency_calls, frequency_error, qpc_fail_at, qpc_back_at, qpc_frozen;
static unsigned positive_hold_sleeps, zero_sleeps;
static char last_receipt[4096];

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
    (void)handle; (void)ms;
    if(wait_error && down_count){SetLastError(5);return WAIT_FAILED;}
    return exited ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
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
        if(!down_count)posted_down_qpc=qpc_last_sample;
        if (down_count < 2) down_keys[down_count] = (WORD)symbol;
        ++down_count;
        if (focus_on_down) foreground = test_window;
    } else if (message == WM_KEYUP) {
        ++up_attempts;
        if (fail_up || (fail_first_up && up_attempts==1)) { SetLastError(5); return FALSE; }
        if (up_count < 2) up_keys[up_count] = (WORD)symbol;
        ++up_count;
        posted_up_qpc=qpc_last_sample;
    } else return FALSE;
    return TRUE;
}
static VOID WINAPI fake_sleep(DWORD ms) {
    if (ms == 150) { ++sleep150; if (focus_on_hold) foreground = test_window; }
    if (ms == 250) ++sleep250;
    if(ms>0 && ms<=10u) {
        positive_hold_sleeps++;
        if(hold_focus)foreground=test_window;
        if(hold_owner)fake_owner_pid=99;
        if(hold_exit)exited=1;
    }
    if(!ms)zero_sleeps++;
    ticks += ms;
    if(!qpc_frozen)qpc_clock+=(LONGLONG)ms*(fake_qpc_frequency/1000);
    else if(!ms)ticks++; /* 坏QPC仍有独立粗钟，避免模拟无限轮询。 */
}
static ULONGLONG WINAPI fake_ticks(void) { return ticks; }
static BOOL WINAPI fake_frequency(LARGE_INTEGER *value) {
    frequency_calls++;value->QuadPart=fake_qpc_frequency;return !frequency_error;
}
static BOOL WINAPI fake_qpc(LARGE_INTEGER *value) {
    qpc_calls++;
    if(qpc_calls==qpc_fail_at)return FALSE;
    value->QuadPart=qpc_clock-(qpc_calls==qpc_back_at ? 10000:0);
    qpc_last_sample=value->QuadPart;
    if(!qpc_frozen){qpc_clock+=qpc_step;ticks=(ULONGLONG)(qpc_clock/1000);}
    return TRUE;
}
static int fake_fprintf(FILE *stream,const char *format,...) {
    va_list arguments;va_start(arguments,format);
    int result=!strncmp(format,"fd2 input: ",11) ?
        vsnprintf(last_receipt,sizeof(last_receipt),format,arguments):vfprintf(stream,format,arguments);
    va_end(arguments);return result;
}

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
#define QueryPerformanceFrequency fake_frequency
#define QueryPerformanceCounter fake_qpc
#define fprintf fake_fprintf
#define main field_play_input_driver_main
#include "field_play_input_driver.c"
#undef main
#undef fprintf

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
    up_attempts=fail_first_up=wait_error=hold_focus=hold_owner=hold_exit=0;
    qpc_clock=1000000;fake_qpc_frequency=1000000;qpc_step=100;qpc_last_sample=0;
    posted_down_qpc=posted_up_qpc=0;
    qpc_calls=frequency_calls=frequency_error=qpc_fail_at=qpc_back_at=qpc_frozen=0;
    positive_hold_sleeps=zero_sleeps=0;last_receipt[0]=0;
    memset(down_keys, 0, sizeof(down_keys)); memset(up_keys, 0, sizeof(up_keys));
    SetLastError(0);
}
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "focus guard test:%d: %s\n", __LINE__, #expr); return 1; } } while (0)

/* 只模拟宿主时钟与Post；精度门仍由独立runner按QPC收据验，不签目标repeat。 */
static int precise_hold_contract(InputTarget *target) {
    WORD key=VK_DOWN;
    reset();
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,1176u)==0);
    CHECK(down_count==1 && up_count==1 && closed==1 && !sleep150);
    CHECK(frequency_calls==1 && qpc_calls>10 && positive_hold_sleeps && zero_sleeps);
    CHECK(posted_up_qpc-posted_down_qpc>=1176000 && posted_up_qpc-posted_down_qpc<1177000);
    int release_query=qpc_calls;
    char expected[192];
    snprintf(expected,sizeof(expected),"\"down_qpc\":%llu,\"up_qpc\":%llu,\"qpc_frequency\":1000000,\"qpc_valid\":true",
        (unsigned long long)posted_down_qpc,(unsigned long long)posted_up_qpc);
    CHECK(strstr(last_receipt,expected) && strstr(last_receipt,"\"down_tick_ms\":") &&
        strstr(last_receipt,"\"up_tick_ms\":"));

    reset();const WORD chord[]={VK_SHIFT,VK_F1};
    CHECK(tap_keys_duration(target,chord,2u,0,0,0,166u)==0 && down_count==2 && up_count==2);
    CHECK(down_keys[0]==VK_SHIFT && down_keys[1]==VK_F1 && up_keys[0]==VK_F1 && up_keys[1]==VK_SHIFT);
    CHECK(posted_up_qpc-posted_down_qpc>=166000 && strstr(last_receipt,"\"qpc_valid\":true"));

    for(unsigned i=0;i<4u;i++) {
        reset();
        if(i==0u)frequency_error=1;
        else fake_qpc_frequency=i==1u?0:i==2u?-1:INT64_MAX;
        CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && !down_count && !up_count && closed==1);
        CHECK(strstr(last_receipt,"\"qpc_valid\":false"));
    }
    reset();qpc_fail_at=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && !down_count && !up_count && closed==1);
    reset();qpc_fail_at=2;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && up_count==1 && closed==1);
    CHECK(strstr(last_receipt,"\"qpc_valid\":false"));
    reset();qpc_back_at=2;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && up_count==1);
    CHECK(strstr(last_receipt,"\"qpc_valid\":false"));
    reset();qpc_fail_at=release_query;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,1176u)==-1 && down_count==1 && up_count==1);
    CHECK(strstr(last_receipt,"\"up_qpc\":0,") && strstr(last_receipt,"\"qpc_valid\":false"));
    reset();qpc_frozen=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && up_count==1);
    CHECK(ticks<1800 && strstr(last_receipt,"\"qpc_valid\":false"));

    reset();hold_focus=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && up_count==1);
    CHECK(strstr(last_receipt,"\"foreground_guard_ok\":false"));
    reset();hold_owner=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && !up_count);
    CHECK(fake_owner_pid==99 && strstr(last_receipt,"\"key_up_sent\":false"));
    reset();hold_exit=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==0 && down_count==1 && !up_count);
    CHECK(strstr(last_receipt,"\"up_qpc\":0,") && strstr(last_receipt,"\"release_not_needed\":true") &&
        strstr(last_receipt,"\"qpc_valid\":false"));
    reset();wait_error=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && up_count==1);
    reset();fail_first_up=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && up_count==1 && up_attempts==2);
    snprintf(expected,sizeof(expected),"\"up_qpc\":%llu,",(unsigned long long)posted_up_qpc);
    CHECK(strstr(last_receipt,expected) && strstr(last_receipt,"\"release_retries\":1"));
    reset();fail_up=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,600u)==-1 && down_count==1 && !up_count && up_attempts==2);
    CHECK(strstr(last_receipt,"\"up_qpc\":0,") && strstr(last_receipt,"\"qpc_valid\":false"));

    reset();frequency_error=1;qpc_fail_at=1;
    CHECK(tap_keys_duration(target,&key,1u,0,0,0,150u)==0 && down_count==1 && up_count==1 && sleep150==1);
    CHECK(!frequency_calls && !qpc_calls && strstr(last_receipt,"\"qpc_frequency\":0,\"qpc_valid\":false"));
    return 0;
}

int main(void) {
    InputTarget target = { (HWND)(uintptr_t)0x1234, 12345, 1 };
    LPARAM down, up;
    reset();
    WORD held=VK_DOWN;
    CHECK(tap_keys_duration(&target,&held,1u,0,0,0,149u)==-1 && !down_count && closed);
    reset();
    CHECK(tap_keys_duration(&target,&held,1u,0,0,0,3001u)==-1 && !down_count && closed);
    reset();
    CHECK(precise_hold_contract(&target)==0);
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

    /* 新modifier组合直接走同一真实driver函数；这里只替Win32调用，不发键。
     * 特别核实DOWN顺序和逆序UP，以及仅modifier发出后的失败释放。 */
    const char *chord_names[] = {"SHIFT_F1", "CTRL_F2", "ALT_F3"};
    const WORD modifier_keys[] = {VK_SHIFT, VK_CONTROL, VK_MENU};
    for (unsigned i=0;i<3u;i++) {
        fd2_driver_key_chord chord;
        CHECK(fd2_driver_key_chord_parse(chord_names[i], &chord) == 0);
        const WORD symbols[] = {chord.modifier_vk, chord.function_vk};
        reset();
        CHECK(tap_keys(&target, symbols, 2u, 250000u, 0, chord.bios_scan_expected) == 0);
        CHECK(down_count == 2 && up_count == 2 && set_calls == 1 && closed == 1);
        CHECK(down_keys[0] == modifier_keys[i] && down_keys[1] == VK_F1+i);
        CHECK(up_keys[0] == VK_F1+i && up_keys[1] == modifier_keys[i]);
        CHECK(foreground != test_window && sleep150 == 1 && sleep250 == 1);
        reset(); focus_on_down = 1;
        CHECK(tap_keys(&target, symbols, 2u, 250000u, 0, chord.bios_scan_expected) == -1);
        CHECK(down_count == 1 && up_count == 1 && down_keys[0] == modifier_keys[i] && up_keys[0] == modifier_keys[i]);
        reset(); fake_owner_pid = 99;
        CHECK(tap_keys(&target, symbols, 2u, 250000u, 0, chord.bios_scan_expected) == -1);
        CHECK(down_count == 0 && up_count == 0 && closed == 1);
    }

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
