/* 正式 SDL 战场输入回归的宿主驱动：Windows HWND消息／Linux XTest。
 * 序列沿用原有 stage 0 输入回归，不调用玩法 API。Windows只向本PID
 * 的窗口投递键盘消息，不激活窗口、不发送全局输入；截图仍为HWND WGC。
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#else
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
typedef struct { HWND window; DWORD pid; unsigned windows_found; } InputTarget;
typedef WORD KeySym;
#define XK_Return VK_RETURN
#define XK_Down VK_DOWN
#define XK_Up VK_UP
#define XK_Left VK_LEFT
#define XK_Right VK_RIGHT
#define XK_z 'Z'

/* Win32 Job 是本轮命令树的 owner；句柄不继承，宿主意外退出也会回收子孙。
 * 必须先挂入 Job 再恢复主线程，不能让 Python／CTest 先生成未受约束的窗口。 */
static int append_job_argument(wchar_t *line, size_t *length, const char *argument) {
    int count = MultiByteToWideChar(CP_ACP, 0, argument, -1, NULL, 0);
    if (!count) return -1;
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    if (!wide || !MultiByteToWideChar(CP_ACP, 0, argument, -1, wide, count)) {
        free(wide);
        return -1;
    }
    size_t used = *length;
#define APPEND_JOB_CHAR(c) do { if (used >= 32766u) goto too_long; line[used++] = (c); } while (0)
    if (used) APPEND_JOB_CHAR(L' ');
    APPEND_JOB_CHAR(L'"');
    for (const wchar_t *part = wide; ; ) {
        size_t slashes = 0;
        while (*part == L'\\') { slashes++; part++; }
        size_t escaped = (*part == L'"' || !*part) ? slashes * 2u : slashes;
        for (size_t i = 0; i < escaped; i++) APPEND_JOB_CHAR(L'\\');
        if (!*part) break;
        if (*part == L'"') APPEND_JOB_CHAR(L'\\');
        APPEND_JOB_CHAR(*part++);
    }
    APPEND_JOB_CHAR(L'"');
    line[used] = 0;
    *length = used;
    free(wide);
    return 0;
too_long:
    free(wide);
    return -1;
#undef APPEND_JOB_CHAR
}

static int job_active_processes(HANDLE job, DWORD *count) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting;
    if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                   &accounting, sizeof(accounting), NULL))
        return -1;
    *count = accounting.ActiveProcesses;
    return 0;
}

static int wait_job_empty(HANDLE job, DWORD timeout) {
    ULONGLONG deadline = GetTickCount64() + timeout;
    do {
        DWORD active = 0;
        if (job_active_processes(job, &active) != 0) return -1;
        if (!active) return 1;
        Sleep(25);
    } while (GetTickCount64() < deadline);
    return 0;
}

static BOOL CALLBACK close_job_window(HWND window, LPARAM parameter) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
        BOOL member = FALSE;
        if (IsProcessInJob(process, (HANDLE)parameter, &member) && member)
            PostMessageW(window, WM_CLOSE, 0, 0);
        CloseHandle(process);
    }
    return TRUE;
}

static int run_job(int argc, char **argv) {
    if (argc < 4) return 2;
    char *end = NULL;
    errno = 0;
    unsigned long timeout = strtoul(argv[2], &end, 10);
    if (errno || !timeout || !end || *end || timeout >= INFINITE) return 2;
    wchar_t *command = calloc(32768u, sizeof(*command));
    size_t length = 0;
    if (!command) return 126;
    for (int i = 3; i < argc; i++) {
        if (append_job_argument(command, &length, argv[i]) != 0) {
            fprintf(stderr, "job command line invalid or too long\n");
            free(command);
            return 126;
        }
    }
    HANDLE job = CreateJobObjectW(NULL, NULL); /* NULL security: non-inheritable. */
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                         &limits, sizeof(limits))) {
        fprintf(stderr, "cannot create process-tree job: error=%lu\n", GetLastError());
        if (job) CloseHandle(job);
        free(command);
        return 126;
    }
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    BOOL created = CreateProcessW(NULL, command, NULL, NULL, TRUE,
                                  CREATE_SUSPENDED | CREATE_NO_WINDOW,
                                  NULL, NULL, &startup, &child);
    free(command);
    if (!created) {
        fprintf(stderr, "cannot create job worker: error=%lu\n", GetLastError());
        CloseHandle(job);
        return 126;
    }
    if (!AssignProcessToJobObject(job, child.hProcess)) {
        fprintf(stderr, "cannot assign suspended worker to job: error=%lu\n", GetLastError());
        TerminateProcess(child.hProcess, 126);
        WaitForSingleObject(child.hProcess, 5000);
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        CloseHandle(job);
        return 126;
    }
    int result = 126, timed_out = 0, residual = 0, forced = 0, cleaned = 0;
    DWORD worker_exit = STILL_ACTIVE, active = 0;
    if (ResumeThread(child.hThread) == (DWORD)-1) {
        fprintf(stderr, "cannot resume job worker: error=%lu\n", GetLastError());
    } else {
        DWORD waited = WaitForSingleObject(child.hProcess, (DWORD)timeout);
        if (waited == WAIT_TIMEOUT) {
            timed_out = 1;
            result = 124;
        } else if (waited == WAIT_OBJECT_0 && GetExitCodeProcess(child.hProcess, &worker_exit)) {
            result = (int)worker_exit;
        }
    }
    CloseHandle(child.hThread);
    /* 进程句柄先变为 signaled，Job 记账可能稍后归零；先等自然收尾，
     * 仍存活的子孙才算残留。等待结束后仍须重新查询并确认整树已空。 */
    if (worker_exit != STILL_ACTIVE)
        (void)wait_job_empty(job, 250);
    if (job_active_processes(job, &active) != 0) {
        fprintf(stderr, "cannot inspect job membership: error=%lu\n", GetLastError());
        result = 126;
    } else if (active && !timed_out && worker_exit != STILL_ACTIVE) {
        residual = 1;
        result = 125; /* worker finished but left live descendants: never pass. */
    }
    if (active || result == 126) {
        EnumWindows(close_job_window, (LPARAM)job);
        cleaned = wait_job_empty(job, 5000) == 1;
        if (!cleaned) {
            forced = 1;
            if (!TerminateJobObject(job, 137))
                fprintf(stderr, "cannot terminate owned job: error=%lu\n", GetLastError());
            cleaned = wait_job_empty(job, 5000) == 1;
        }
    } else {
        cleaned = 1;
    }
    if (!cleaned || (forced && result == 0)) result = 126;
    GetExitCodeProcess(child.hProcess, &worker_exit);
    fprintf(stderr, "fd2 job: root=%lu worker_exit=%lu timed_out=%d residual=%d forced=%d cleaned=%d result=%d\n",
            child.dwProcessId, worker_exit, timed_out, residual, forced, cleaned, result);
    fflush(stderr);
    CloseHandle(child.hProcess);
    CloseHandle(job); /* 最后兜底：所有未结束的本轮成员随句柄关闭终止。 */
    return result;
}

static BOOL CALLBACK find_window(HWND window, LPARAM param) {
    InputTarget *target = (InputTarget *)param;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == target->pid && IsWindowVisible(window) &&
        GetWindow(window, GW_OWNER) == NULL) {
        if (!target->windows_found) target->window = window;
        ++target->windows_found;
    }
    return TRUE;
}

/* 定向消息只进入本PID所属HWND，不激活窗口或改变系统键盘状态。
 * lParam保留真实扫描码／extended及up的previous/transition位；普通
 * WM_KEYDOWN/UP供SDL2/3消息后端接收，不能把Post成功当资源ACK。 */
static int key_message_lparam(WORD symbol, int down, LPARAM *parameter) {
    UINT scan = MapVirtualKeyW(symbol, MAPVK_VK_TO_VSC);
    if (!scan || !parameter) return -1;
    DWORD value = 1u | ((scan & 0xffu) << 16);
    if (symbol == VK_UP || symbol == VK_DOWN || symbol == VK_LEFT ||
        symbol == VK_RIGHT || symbol == VK_HOME || symbol == VK_DELETE ||
        symbol == VK_INSERT || symbol == VK_PRIOR || symbol == VK_NEXT)
        value |= 1u << 24;
    if (!down) value |= 3u << 30;
    *parameter = (LPARAM)(LONG)value;
    return 0;
}

static int post_owned_key(const InputTarget *target, WORD symbol, int down,
                          LPARAM parameter) {
    DWORD owner = 0;
    if (!IsWindow(target->window) ||
        !GetWindowThreadProcessId(target->window, &owner) || owner != target->pid) {
        SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return -1;
    }
    return PostMessageW(target->window, down ? WM_KEYDOWN : WM_KEYUP,
                         (WPARAM)symbol, parameter) ? 0 : -1;
}

static int tap_keys(InputTarget *target, const WORD *symbols, unsigned count,
                     unsigned long delay_us, int wave) {
    HWND foreground_before = GetForegroundWindow();
    ULONGLONG started = GetTickCount64(), down_tick = 0, up_tick = 0;
    unsigned sent = 0, released = 0, release_retries = 0;
    DWORD error = 0, target_exit_code = STILL_ACTIVE;
    int release_not_needed = 0;
    LPARAM down[2], up[2];
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                                  FALSE, target->pid);
    if (!process) { error = GetLastError(); goto done; }
    if (!symbols || !count || count > 2u) {
        error = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (wave) {
        wchar_t path[32768]; DWORD length = 32768;
        if (!QueryFullProcessImageNameW(process, 0, path, &length)) {
            error = GetLastError(); goto done;
        }
        const wchar_t *name = wcsrchr(path, L'\\');
        if (_wcsicmp(name ? name + 1 : path, L"dosbox-x.exe") != 0) {
            error = ERROR_INVALID_NAME; goto done;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        if (key_message_lparam(symbols[i], 1, &down[i]) != 0 ||
            key_message_lparam(symbols[i], 0, &up[i]) != 0) {
            error = ERROR_INVALID_PARAMETER; goto done;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        ULONGLONG tick = GetTickCount64();
        if (post_owned_key(target, symbols[i], 1, down[i]) != 0) {
            error = GetLastError();
            if (!error) error = ERROR_GEN_FAILURE;
            break;
        }
        if (!sent) down_tick = tick;
        ++sent;
    }
    /* 保持生产逐帧键态可见，仍低于500ms repeat门；不追加DOWN重试。 */
    if (sent == count) Sleep(150);
    for (unsigned i = sent; i > 0; --i) {
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) break;
        up_tick = GetTickCount64();
        if (post_owned_key(target, symbols[i - 1], 0, up[i - 1]) == 0) {
            ++released;
        } else {
            if (!error) {
                error = GetLastError();
                if (!error) error = ERROR_GEN_FAILURE;
            }
            /* 释放失败仅重试本轮已DOWN的同一PID/HWND；不触碰全局键态。 */
            ++release_retries;
            if (WaitForSingleObject(process, 0) != WAIT_OBJECT_0 &&
                post_owned_key(target, symbols[i - 1], 0, up[i - 1]) == 0)
                ++released;
        }
    }
    if (sent == count && released != count &&
        WaitForSingleObject(process, 0) == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process, &target_exit_code)) {
        release_not_needed = 1;
        up_tick = GetTickCount64(); /* 观察到退出／免release，不伪称已投递UP。 */
    }
    Sleep((DWORD)((delay_us + 999u) / 1000u));
 done:
    if (process) CloseHandle(process);
    int result = !error && sent == count && count != 0u &&
        (released == count || release_not_needed) ? 0 : -1;
    fprintf(stderr, "fd2 input: {\"backend\":\"PostMessageW HWND\",\"pid\":%lu,"
        "\"hwnd\":\"0x%llx\",\"kind\":\"%s\",\"keys\":%u,"
        "\"key_down_sent\":%s,\"key_up_sent\":%s,\"sent\":%u,\"release\":%u,"
        "\"foreground_before\":\"0x%llx\",\"foreground_after\":\"0x%llx\","
        "\"down_tick_ms\":%llu,\"up_tick_ms\":%llu,\"elapsed_ms\":%llu,"
        "\"release_retries\":%u,\"target_exited_before_release\":%s,"
        "\"release_not_needed\":%s,\"target_exit_code\":%lu,\"error\":%lu,\"ok\":%s}\n",
        (unsigned long)target->pid, (unsigned long long)(uintptr_t)target->window,
        wave ? "wave" : "key", count,
        sent == count && count ? "true" : "false", released == count && count ? "true" : "false",
        sent, released, (unsigned long long)(uintptr_t)foreground_before,
        (unsigned long long)(uintptr_t)GetForegroundWindow(),
        (unsigned long long)down_tick, (unsigned long long)up_tick,
        (unsigned long long)(GetTickCount64() - started), release_retries,
        release_not_needed ? "true" : "false", release_not_needed ? "true" : "false",
        (unsigned long)target_exit_code, (unsigned long)error, result == 0 ? "true" : "false");
    fflush(stderr);
    return result;
}

static int tap(InputTarget *target, KeySym symbol, unsigned long delay_us) {
    return tap_keys(target, &symbol, 1u, delay_us, 0);
}

extern int fd2_capture_window(HWND window, const char *path);

/* DOSBox-X 2026.10.01：F11(host)+W，仅投递到其窗口消息队列；
 * 始终W先松、F11后松，不改变用户全局键态或转移焦点。 */
static int tap_native_wave_record(InputTarget *target) {
    const WORD symbols[] = {VK_F11, 'W'};
    return tap_keys(target, symbols, 2u, 100000u, 1);
}

static int capture(InputTarget *target, const char *path) {
    DWORD pid = 0;
    GetWindowThreadProcessId(target->window, &pid);
    if (pid != target->pid) return -1;
    return fd2_capture_window(target->window, path);
}

#else
typedef Display InputTarget;
static int tap(InputTarget *display, KeySym symbol, unsigned long delay_us) {
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
#endif

static int tap_many(InputTarget *display, KeySym symbol, size_t count,
                    unsigned long delay_us) {
    for (size_t i = 0; i < count; i++) {
        if (tap(display, symbol, delay_us) != 0) return -1;
    }
    return 0;
}

static int finish_wait(InputTarget *display, int nonzero_down) {
    if (tap(display, XK_Return, 350000) != 0) return -1;
    if (nonzero_down && tap(display, XK_Down, 180000) != 0) return -1;
    if (tap(display, XK_Return, nonzero_down ? 850000 : 500000) != 0 ||
        tap(display, XK_Down, 250000) != 0 ||
        tap(display, XK_Return, 650000) != 0)
        return -1;
    return 0;
}

static int drive_wait_round(InputTarget *display, int already_focused) {
    /* Z 先按 actor 表聚焦 unit 0；(7,20) → (7,21) 非零移动后 wait。 */
    if ((!already_focused && tap(display, XK_z, 250000) != 0) ||
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

static int drive_attack(InputTarget *display, int already_focused) {
    /* Z 连按三次聚焦 unit 2；从 (8,22) 移到 (5,22)，攻击 (3,22)。 */
    if ((!already_focused && tap_many(display, XK_z, 3, 180000) != 0) ||
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
#ifdef _WIN32
    if (argc > 1 && strcmp(argv[1], "run-job") == 0) return run_job(argc, argv);
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s wait|attack|wait-focused|attack-focused|close PID, or key|capture PID KEY|PATH\n", argv[0]);
        return 2;
    }
    char *end = NULL;
    unsigned long pid = strtoul(argv[2], &end, 10);
    if (!pid || !end || *end) return 2;
    SetProcessDPIAware();
    InputTarget target = {NULL, (DWORD)pid, 0};
    EnumWindows(find_window, (LPARAM)&target);
    if (target.windows_found > 1u) {
        fprintf(stderr, "ambiguous visible windows for PID %lu: count=%u\n",
                pid, target.windows_found);
        target.window = NULL;
    }
    /* key失败也输出定向投递收据，窗口不存在时不改用其它对象。 */
    if (strcmp(argv[1], "key") == 0 && argc == 4) {
        if (strcmp(argv[3], "HOST_W") == 0)
            return tap_native_wave_record(&target) == 0 ? 0 : 1;
        const struct { const char *name; WORD key; } keys[] = {
            {"ENTER", VK_RETURN}, {"ESC", VK_ESCAPE}, {"SPACE", VK_SPACE},
            {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT},
            {"RIGHT", VK_RIGHT}, {"Z", 'Z'}, {"KP5", VK_NUMPAD5}, {"G", 'G'}, {"A", 'A'},
            {"F1", VK_F1}, {"F2", VK_F2}, {"HOME", VK_HOME},
            {"INSERT", VK_INSERT}, {"DELETE", VK_DELETE},
            {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT}
        };
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
            if (strcmp(argv[3], keys[i].name) == 0)
                return tap(&target, keys[i].key, 250000) == 0 ? 0 : 1;
        return 2;
    }
    if (!target.window) {
        fprintf(stderr, "no visible window for PID %lu\n", pid);
        return 1;
    }
    if (strcmp(argv[1], "close") == 0) {
        DWORD owner = 0;
        GetWindowThreadProcessId(target.window, &owner);
        return owner == target.pid && PostMessageW(target.window, WM_CLOSE, 0, 0) ? 0 : 1;
    }
    if (strcmp(argv[1], "capture") == 0 && argc == 4)
        return capture(&target, argv[3]) == 0 ? 0 : 1;
    int wait_mode = strcmp(argv[1], "wait") == 0 || strcmp(argv[1], "wait-focused") == 0;
    int attack_mode = strcmp(argv[1], "attack") == 0 || strcmp(argv[1], "attack-focused") == 0;
    if (!wait_mode && !attack_mode)
        return 2;
    int already_focused = strcmp(argv[1], "wait-focused") == 0 ||
                          strcmp(argv[1], "attack-focused") == 0;
    int result = wait_mode
        ? drive_wait_round(&target, already_focused)
        : drive_attack(&target, already_focused);
    return result == 0 ? 0 : 1;
#else
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
        ? drive_wait_round(display, 0) : drive_attack(display, 0);
    XCloseDisplay(display);
    return result == 0 ? 0 : 1;
#endif
}
