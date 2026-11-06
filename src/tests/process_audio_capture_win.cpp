/* Windows进程树声音取证：按Microsoft ApplicationLoopback官方API合同独立实现。
 * https://learn.microsoft.com/zh-cn/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/
 * SDK26100 audioclientactivationparams.h；最低Windows build20348。
 * 只激活VAD\\Process_Loopback/include-tree；无桌面设备回退、窗口、输入或Game callback改动。
 * CLI: PID WAV MAX_MS READY_FILE STOP_FILE。原始PCM16/stereo/44100，真实录音另验。
 */
#ifndef _WIN32
#error This evidence helper requires Windows process-loopback APIs.
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wrl.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <sstream>
#include <string>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};
static HRESULT win_error() {
    DWORD code = GetLastError();
    return HRESULT_FROM_WIN32(code ? code : ERROR_GEN_FAILURE);
}
struct Failure { HRESULT hr; const char *stage; const char *message; };
static void require(HRESULT hr, const char *stage, const char *message) {
    if (FAILED(hr)) throw Failure{hr, stage, message};
}
static std::string utf8(const std::wstring &text) {
    if (text.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!n) return {};
    std::string result(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), &result[0], n, nullptr, nullptr);
    return result;
}
static std::string quoted(const std::string &text) {
    std::string out = "\"";
    const char hex[] = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out + '"';
}
static bool number(const wchar_t *text, uint64_t maximum, uint64_t &out) {
    if (!text || !*text) return false;
    uint64_t value = 0;
    for (; *text; text++) {
        if (*text < L'0' || *text > L'9') return false;
        unsigned digit = static_cast<unsigned>(*text-L'0');
        if (value > maximum/10 || (value == maximum/10 && digit > maximum%10)) return false;
        value = value*10 + digit;
    }
    if (!value) return false;
    out = value; return true;
}
static std::wstring lower(std::wstring text) {
    for (auto &c : text) c = static_cast<wchar_t>(towlower(c));
    return text;
}
/* 解析真实父目录(junction亦解析)，再CREATE_NEW；不写original_game、ADS或设备名。
 * 父目录必须已存在，helper不会递归创建目录，也不覆盖任何已有输出。 */
static fs::path output_path(const wchar_t *argument) {
    fs::path path(argument);
    if (!path.is_absolute() || path.filename().empty()) throw Failure{E_INVALIDARG,"path","输出路径必须是绝对文件路径"};
    std::wstring name = path.filename().wstring(), base = lower(name.substr(0,name.find(L'.')));
    if (name.back()==L'.' || name.back()==L' ' || name.find_first_of(L"<>:\"/\\|?*")!=std::wstring::npos ||
        base==L"con" || base==L"prn" || base==L"aux" || base==L"nul" ||
        (base.size()==4 && (base.substr(0,3)==L"com" || base.substr(0,3)==L"lpt") && base[3]>=L'0' && base[3]<=L'9'))
        throw Failure{E_INVALIDARG,"path","非法输出文件名"};
    for (wchar_t c : name) if (c<32) throw Failure{E_INVALIDARG,"path","输出文件名含控制字符"};
    Handle directory(CreateFileW(path.parent_path().c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr));
    if (!directory) throw Failure{win_error(),"path","无法打开输出父目录"};
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(directory.value,&info)) throw Failure{win_error(),"path","无法检查输出父目录"};
    if (!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) throw Failure{E_INVALIDARG,"path","输出父路径不是目录"};
    DWORD n = GetFinalPathNameByHandleW(directory.value,nullptr,0,FILE_NAME_NORMALIZED);
    if (!n) throw Failure{win_error(),"path","无法解析输出父目录"};
    std::wstring final_dir(n,L'\0');
    DWORD written = GetFinalPathNameByHandleW(directory.value,&final_dir[0],n,FILE_NAME_NORMALIZED);
    if (!written || written>=n) throw Failure{win_error(),"path","输出父目录解析失败"};
    final_dir.resize(written);
    fs::path resolved = fs::path(final_dir)/name;
    for (const auto &component : resolved)
        if (lower(component.wstring())==L"original_game") throw Failure{E_ACCESSDENIED,"path","禁止写入原版资源目录"};
    DWORD attributes = GetFileAttributesW(resolved.c_str());
    if (attributes!=INVALID_FILE_ATTRIBUTES) throw Failure{HRESULT_FROM_WIN32(ERROR_FILE_EXISTS),"path","输出或停止文件已存在"};
    if (GetLastError()!=ERROR_FILE_NOT_FOUND) throw Failure{win_error(),"path","无法检查输出文件"};
    return resolved;
}
static HRESULT write_bytes(HANDLE file, const void *data, DWORD bytes, uint64_t *progress=nullptr) {
    const BYTE *p=static_cast<const BYTE*>(data);
    while (bytes) {
        DWORD wrote=0;
        BOOL ok=WriteFile(file,p,bytes,&wrote,nullptr);
        if (progress) *progress+=wrote;
        if (!ok) return win_error();
        if (!wrote) return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
        p+=wrote; bytes-=wrote;
    }
    return S_OK;
}
struct Wave {
    Handle file;
    uint64_t data_bytes=0;
    static constexpr DWORD rate=44100, align=4;
    static void le(BYTE *p, DWORD value, unsigned bytes) {
        for (unsigned i=0;i<bytes;i++) p[i]=static_cast<BYTE>(value>>(8*i));
    }
    HRESULT header() {
        BYTE h[44]{};
        memcpy(h,"RIFF",4); le(h+4,36+static_cast<DWORD>(data_bytes),4);
        memcpy(h+8,"WAVEfmt ",8); le(h+16,16,4); le(h+20,1,2); le(h+22,2,2);
        le(h+24,rate,4); le(h+28,rate*align,4); le(h+32,align,2); le(h+34,16,2);
        memcpy(h+36,"data",4); le(h+40,static_cast<DWORD>(data_bytes),4);
        LARGE_INTEGER zero{};
        if (!SetFilePointerEx(file.value,zero,nullptr,FILE_BEGIN)) return win_error();
        return write_bytes(file.value,h,sizeof(h));
    }
    HRESULT open(const fs::path &path) {
        file.value=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,
            nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        return file ? header() : win_error();
    }
    HRESULT append(const BYTE *data, UINT32 frames, bool silent) {
        if (frames>MAXDWORD/align || (!silent && !data)) return E_INVALIDARG;
        DWORD bytes=frames*align;
        if (!silent) return write_bytes(file.value,data,bytes,&data_bytes);
        static const BYTE zero[4096]{};
        while (bytes) {
            DWORD n=std::min<DWORD>(bytes,sizeof(zero));
            HRESULT hr=write_bytes(file.value,zero,n,&data_bytes); if (FAILED(hr)) return hr;
            bytes-=n;
        }
        return S_OK;
    }
    HRESULT finish() {
        if (!file) return S_OK;
        data_bytes-=data_bytes%align; /* 失败写入只保留完整PCM帧。 */
        LARGE_INTEGER end{}; end.QuadPart=44+static_cast<LONGLONG>(data_bytes);
        if (!SetFilePointerEx(file.value,end,nullptr,FILE_BEGIN) || !SetEndOfFile(file.value)) return win_error();
        HRESULT hr=header();
        if (SUCCEEDED(hr) && !FlushFileBuffers(file.value)) hr=win_error();
        return hr;
    }
};
class Activation final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, Microsoft::WRL::FtmBase,
    IActivateAudioInterfaceCompletionHandler> {
public:
    Handle done{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    AUDIOCLIENT_ACTIVATION_PARAMS params{}; /* async完成前由handler持有，非临时悬空blob。 */
    HRESULT result=E_PENDING;
    ComPtr<IUnknown> object;
    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation *operation) override {
        HRESULT activation=E_FAIL;
        result=operation->GetActivateResult(&activation,&object);
        if (SUCCEEDED(result)) result=activation;
        SetEvent(done.value);
        return S_OK;
    }
};
struct CaptureStats {
    uint64_t packets=0, discontinuities=0, timestamp_errors=0;
    uint64_t first_qpc_100ns=0, last_qpc_100ns=0;
};
static HRESULT drain(IAudioCaptureClient *capture, Wave &wave, CaptureStats &stats,
                     uint64_t until_ms=0) {
    UINT32 next=0; HRESULT hr=capture->GetNextPacketSize(&next);
    while (SUCCEEDED(hr) && next) {
        if (until_ms && GetTickCount64()>=until_ms) return S_OK;
        BYTE *data=nullptr; UINT32 frames=0; DWORD flags=0; UINT64 device=0,qpc=0;
        hr=capture->GetBuffer(&data,&frames,&flags,&device,&qpc); if (FAILED(hr)) return hr;
        HRESULT wrote=wave.append(data,frames,(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0);
        HRESULT released=capture->ReleaseBuffer(frames); /* 写失败也必须释放本包。 */
        if (FAILED(wrote)) return wrote;
        if (FAILED(released)) return released;
        if (!stats.packets) stats.first_qpc_100ns=qpc;
        stats.last_qpc_100ns=qpc; stats.packets++;
        stats.discontinuities+=(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0;
        stats.timestamp_errors+=(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)!=0;
        hr=capture->GetNextPacketSize(&next);
    }
    return hr;
}
static HRESULT publish_ready(const fs::path &path, const std::string &text) {
    fs::path temporary=path.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
    HRESULT hr=S_OK;
    {
        Handle file(CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if (!file) return win_error();
        hr=write_bytes(file.value,text.data(),static_cast<DWORD>(text.size()));
        if (SUCCEEDED(hr) && !FlushFileBuffers(file.value)) hr=win_error();
    }
    if (SUCCEEDED(hr) && !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) hr=win_error();
    if (FAILED(hr)) DeleteFileW(temporary.c_str()); /* 只清理本helper创建的临时收据。 */
    return hr;
}

enum class CaptureWake { Audio, TargetExited, StopFile, Timeout, Idle };
static CaptureWake capture_wake(HANDLE target, HANDLE audio, const fs::path &stop_path,
                                uint64_t started_ms, uint64_t maximum) {
    /* 退出不能被持续音频、已出现的停止文件或已到期deadline掩盖。 */
    DWORD target_state=WaitForSingleObject(target,0);
    if (target_state==WAIT_OBJECT_0) return CaptureWake::TargetExited;
    if (target_state!=WAIT_TIMEOUT) throw Failure{win_error(),"audio_wait","检查录音目标退出状态失败"};
    DWORD attributes=GetFileAttributesW(stop_path.c_str());
    if (attributes!=INVALID_FILE_ATTRIBUTES) {
        if (attributes&FILE_ATTRIBUTE_DIRECTORY) throw Failure{E_INVALIDARG,"stop_file","停止路径变成了目录"};
        return CaptureWake::StopFile;
    }
    if (GetLastError()!=ERROR_FILE_NOT_FOUND) throw Failure{win_error(),"stop_file","无法检查停止文件"};
    uint64_t elapsed=GetTickCount64()-started_ms;
    if (elapsed>=maximum) return CaptureWake::Timeout;
    DWORD wait_ms=static_cast<DWORD>(std::min<uint64_t>(maximum-elapsed,20));
    HANDLE events[]={target,audio};
    DWORD result=WaitForMultipleObjects(2,events,FALSE,wait_ms);
    if (result==WAIT_OBJECT_0) return CaptureWake::TargetExited;
    if (result==WAIT_OBJECT_0+1) return CaptureWake::Audio;
    if (result==WAIT_TIMEOUT) return CaptureWake::Idle;
    throw Failure{win_error(),"audio_wait","等待进程音频包失败"};
}

int wmain(int argc, wchar_t **argv) {
    HRESULT status=S_OK, stop_hr=S_OK, finalize_hr=S_OK;
    const char *stage="arguments", *message="", *reason="error";
    DWORD target_pid=0, os_build=0; uint64_t maximum=0, started_ms=0, ended_ms=0, creation=0;
    LARGE_INTEGER frequency{},start_qpc{},end_qpc{}; QueryPerformanceFrequency(&frequency);
    fs::path wave_path, ready_path, stop_path;
    Handle target, audio_event;
    Wave wave; CaptureStats stats;
    ComPtr<IAudioClient> client; ComPtr<IAudioCaptureClient> capture;
    ComPtr<Activation> handler; ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    bool com=false, started=false, ready=false;
    try {
        uint64_t pid=0;
        if (argc!=6 || !number(argv[1],MAXDWORD,pid) || !number(argv[3],120000,maximum))
            throw Failure{E_INVALIDARG,"arguments","参数应为PID WAV MAX_MS READY_FILE STOP_FILE，MAX_MS须1到120000"};
        target_pid=static_cast<DWORD>(pid);
        if (target_pid==GetCurrentProcessId()) throw Failure{E_INVALIDARG,"pid","不能把录音helper自身作为目标"};
        wave_path=output_path(argv[2]); ready_path=output_path(argv[4]); stop_path=output_path(argv[5]);
        if (lower(wave_path.extension().wstring())!=L".wav" ||
            lower(wave_path.wstring())==lower(ready_path.wstring()) ||
            lower(wave_path.wstring())==lower(stop_path.wstring()) ||
            lower(ready_path.wstring())==lower(stop_path.wstring()))
            throw Failure{E_INVALIDARG,"path","WAV扩展名或三个隔离文件路径无效"};
        target.value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,target_pid);
        if (!target) throw Failure{win_error(),"pid","无法打开指定进程"};
        FILETIME born{},exit{},kernel{},user{};
        if (!GetProcessTimes(target.value,&born,&exit,&kernel,&user)) throw Failure{win_error(),"pid","无法读取指定进程身份"};
        creation=(static_cast<uint64_t>(born.dwHighDateTime)<<32)|born.dwLowDateTime;
        if (WaitForSingleObject(target.value,0)!=WAIT_TIMEOUT) throw Failure{HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED),"pid","指定进程已退出"};
        using VersionFn=LONG(WINAPI*)(OSVERSIONINFOW*);
        auto version=reinterpret_cast<VersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion"));
        OSVERSIONINFOW os{}; os.dwOSVersionInfoSize=sizeof(os);
        if (!version || version(&os)!=0 || os.dwBuildNumber<20348) throw Failure{E_NOTIMPL,"version","进程loopback要求Windows build20348或更新版本"};
        os_build=os.dwBuildNumber;
        require(wave.open(wave_path),"wave","无法新建隔离WAV文件");
        require(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM","COM初始化失败"); com=true;
        handler=Microsoft::WRL::Make<Activation>();
        if (!handler || !handler->done) throw Failure{E_OUTOFMEMORY,"activation_event","无法创建音频激活完成事件"};
        handler->params.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
        handler->params.ProcessLoopbackParams.TargetProcessId=target_pid;
        handler->params.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
        PROPVARIANT property{}; property.vt=VT_BLOB;
        property.blob.cbSize=sizeof(handler->params);
        property.blob.pBlobData=reinterpret_cast<BYTE*>(&handler->params);
        require(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),
            &property,handler.Get(),&operation),"ActivateAudioInterfaceAsync","指定进程音频激活请求失败");
        DWORD waited=WaitForSingleObject(handler->done.value,10000);
        if (waited!=WAIT_OBJECT_0) throw Failure{waited==WAIT_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_TIMEOUT):win_error(),"activation_wait","等待进程音频激活失败"};
        require(handler->result,"activation_result","指定进程音频激活失败");
        require(handler->object.As(&client),"IAudioClient","无法取得进程音频接口");
        WAVEFORMATEX format{}; format.wFormatTag=WAVE_FORMAT_PCM; format.nChannels=2;
        format.nSamplesPerSec=Wave::rate; format.wBitsPerSample=16; format.nBlockAlign=Wave::align;
        format.nAvgBytesPerSec=Wave::rate*Wave::align;
        require(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
            0,0,&format,nullptr),"Initialize","进程PCM16立体声44100初始化失败，无设备回退");
        audio_event.value=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!audio_event) throw Failure{win_error(),"audio_event","无法创建音频包事件"};
        require(client->SetEventHandle(audio_event.value),"SetEventHandle","绑定音频包事件失败");
        require(client->GetService(IID_PPV_ARGS(&capture)),"GetService","取得进程音频包接口失败");
        if (WaitForSingleObject(target.value,0)!=WAIT_TIMEOUT) throw Failure{HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED),"pid","指定进程在激活期间退出"};
        require(client->Start(),"Start","开始进程录音失败");
        started=true; started_ms=GetTickCount64(); QueryPerformanceCounter(&start_qpc);
        std::ostringstream receipt;
        receipt << "{\"ready\":true,\"target_pid\":" << target_pid
            << ",\"target_creation_filetime\":" << creation
            << ",\"process_tree\":true,\"format\":\"PCM16-stereo-44100\",\"sample_rate\":44100,\"channels\":2,\"bits\":16"
            << ",\"start_qpc\":" << start_qpc.QuadPart << ",\"qpc_frequency\":" << frequency.QuadPart << "}\n";
        require(publish_ready(ready_path,receipt.str()),"ready_receipt","原子发布就绪收据失败"); ready=true;
        for (;;) {
            CaptureWake wake=capture_wake(target.value,audio_event.value,stop_path,started_ms,maximum);
            if (wake==CaptureWake::TargetExited) { reason="target_exited"; throw Failure{HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED),"target_exit","录音目标提前退出"}; }
            if (wake==CaptureWake::StopFile) { reason="stop_file"; break; }
            if (wake==CaptureWake::Timeout) { reason="timeout"; break; }
            if (wake==CaptureWake::Audio) require(drain(capture.Get(),wave,stats,
                std::min<uint64_t>(started_ms+maximum,GetTickCount64()+20)),
                "capture_packet","读取或写入进程音频包失败");
        }
    } catch (const Failure &error) { status=error.hr; stage=error.stage; message=error.message; }
      catch (const std::exception &) { status=E_FAIL; stage="exception"; message="路径或内存操作异常"; }
    if (started) {
        stop_hr=client->Stop(); QueryPerformanceCounter(&end_qpc); ended_ms=GetTickCount64();
        if (SUCCEEDED(stop_hr)) {
            HRESULT remaining=drain(capture.Get(),wave,stats);
            if (SUCCEEDED(status) && FAILED(remaining)) { status=remaining; stage="final_drain"; message="读取末尾音频包失败"; }
        }
        if (SUCCEEDED(status) && FAILED(stop_hr)) { status=stop_hr; stage="Stop"; message="停止进程录音失败"; }
    }
    finalize_hr=wave.finish();
    if (SUCCEEDED(status) && FAILED(finalize_hr)) { status=finalize_hr; stage="wave_finalize"; message="写入WAV尾部或刷新文件失败"; }
    capture.Reset(); client.Reset(); operation.Reset(); handler.Reset();
    if (com) CoUninitialize();
    char hr[16],stop[16],finish[16];
    snprintf(hr,sizeof(hr),"0x%08lX",static_cast<unsigned long>(status));
    snprintf(stop,sizeof(stop),"0x%08lX",static_cast<unsigned long>(stop_hr));
    snprintf(finish,sizeof(finish),"0x%08lX",static_cast<unsigned long>(finalize_hr));
    std::ostringstream result;
    result << "{\"ok\":" << (SUCCEEDED(status)?"true":"false") << ",\"hresult\":" << quoted(hr)
        << ",\"stage\":" << quoted(SUCCEEDED(status)?"finished":stage) << ",\"error\":" << quoted(message)
        << ",\"backend\":\"WASAPI-process-loopback\",\"process_tree\":true,\"desktop_fallback\":false,\"foreground_required\":false"
        << ",\"target_pid\":" << target_pid << ",\"target_creation_filetime\":" << creation
        << ",\"windows_build\":" << os_build << ",\"stream_started\":" << (started?"true":"false")
        << ",\"max_ms\":" << maximum
        << ",\"ready_published\":" << (ready?"true":"false") << ",\"stop_reason\":" << quoted(reason)
        << ",\"sample_rate\":44100,\"channels\":2,\"bits\":16,\"data_bytes\":" << wave.data_bytes
        << ",\"frames\":" << wave.data_bytes/Wave::align << ",\"packets\":" << stats.packets
        << ",\"discontinuities\":" << stats.discontinuities << ",\"timestamp_errors\":" << stats.timestamp_errors
        << ",\"first_packet_qpc_100ns\":" << stats.first_qpc_100ns << ",\"last_packet_qpc_100ns\":" << stats.last_qpc_100ns
        << ",\"start_qpc\":" << start_qpc.QuadPart << ",\"stop_qpc\":" << end_qpc.QuadPart
        << ",\"qpc_frequency\":" << frequency.QuadPart << ",\"elapsed_ms\":" << (ended_ms>=started_ms?ended_ms-started_ms:0)
        << ",\"stop_hresult\":" << quoted(stop) << ",\"finalize_hresult\":" << quoted(finish)
        << ",\"wav\":" << quoted(utf8(wave_path.wstring())) << "}\n";
    std::string output=result.str(); fwrite(output.data(),1,output.size(),stdout); fflush(stdout);
    return SUCCEEDED(status)?0:1;
}
