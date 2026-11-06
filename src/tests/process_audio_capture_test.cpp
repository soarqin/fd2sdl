/* Windows进程树声音helper纯合同回归。
 * 依据：process_audio_capture_win.cpp的Microsoft process-loopback协议。
 * 仅调用参数/文件/PCM封包与模拟IAudioCaptureClient；不激活真实声音接口。 */
#define wmain unused_capture_cli_main
#include "process_audio_capture_win.cpp"
#undef wmain
#include <fstream>
#include <vector>
#include <stdexcept>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"pure CHECK line%d: %s\n",__LINE__,#e);return 1;} } while(0)
static DWORD rd32(const BYTE *p) { return p[0]|(DWORD(p[1])<<8)|(DWORD(p[2])<<16)|(DWORD(p[3])<<24); }
class ProbeCapture final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,IAudioCaptureClient> {
public:
    unsigned buffers=0;
    STDMETHOD(GetNextPacketSize)(UINT32 *n) override { *n=1;return S_OK; }
    STDMETHOD(GetBuffer)(BYTE **,UINT32 *,DWORD *,UINT64 *,UINT64 *) override { buffers++;return E_ABORT; }
    STDMETHOD(ReleaseBuffer)(UINT32) override { return S_OK; }
};
/* 路径只由本次成功创建的目录持有；返回/异常都先析构文件再清理。 */
struct OwnedDirectory {
    fs::path parent=fs::weakly_canonical(fs::temp_directory_path());
    fs::path path;
    explicit OwnedDirectory(const wchar_t *prefix) {
        path=parent/(std::wstring(prefix)+std::to_wstring(GetCurrentProcessId())+
            L"-"+std::to_wstring(GetTickCount64()));
        if(!fs::create_directory(path))throw std::runtime_error("cannot create owned test directory");
    }
    ~OwnedDirectory() {
        std::error_code error;
        if(path.parent_path()==parent)fs::remove_all(path,error);
    }
};
/* 真正的Win32等待对象：不启动目标进程，不激活COM或音频接口。 */
static int wait_tests(const fs::path &directory) {
    int failures=0;
    auto expect=[&](const char *name, CaptureWake actual, CaptureWake wanted) {
        if(actual!=wanted) {
            fprintf(stderr,"wake CHECK %s: got=%d expected=%d\n",name,int(actual),int(wanted));
            failures++;
        }
    };
    const fs::path absent=directory/L"no-stop.signal";
    for(bool manual : {false,true}) {
        Handle target(CreateEventW(nullptr,TRUE,TRUE,nullptr));
        Handle audio(CreateEventW(nullptr,manual,TRUE,nullptr));
        CHECK(target && audio);
        expect(manual ? "both-signaled-persistent-audio" : "both-signaled-auto-audio",
            capture_wake(target.value,audio.value,absent,GetTickCount64(),1000),CaptureWake::TargetExited);
    }
    Handle target(CreateEventW(nullptr,TRUE,TRUE,nullptr));
    Handle audio(CreateEventW(nullptr,FALSE,FALSE,nullptr));
    CHECK(target && audio);
    const fs::path stop=directory/L"requested-stop.signal";
    { std::ofstream file(stop); CHECK(bool(file)); file << "stop"; }
    expect("target-before-stop-file",capture_wake(target.value,audio.value,stop,
        GetTickCount64(),1000),CaptureWake::TargetExited);
    expect("target-before-expired-deadline",capture_wake(target.value,audio.value,absent,
        GetTickCount64()-1,1),CaptureWake::TargetExited);
    CHECK(SetEvent(audio.value));
    expect("both-signaled-before-stop-file",capture_wake(target.value,audio.value,stop,
        GetTickCount64(),1000),CaptureWake::TargetExited);
    CHECK(SetEvent(audio.value));
    expect("both-signaled-before-expired-deadline",capture_wake(target.value,audio.value,absent,
        GetTickCount64()-1,1),CaptureWake::TargetExited);
    CHECK(ResetEvent(target.value));
    expect("live-target-stop-file",capture_wake(target.value,audio.value,stop,
        GetTickCount64(),1000),CaptureWake::StopFile);
    expect("live-target-deadline",capture_wake(target.value,audio.value,absent,
        GetTickCount64()-1,1),CaptureWake::Timeout);
    CHECK(SetEvent(audio.value));
    expect("live-target-audio",capture_wake(target.value,audio.value,absent,
        GetTickCount64(),1000),CaptureWake::Audio);
    expect("live-target-idle",capture_wake(target.value,audio.value,absent,
        GetTickCount64(),1000),CaptureWake::Idle);
    CHECK(SetEvent(target.value));
    expect("target-only",capture_wake(target.value,audio.value,absent,
        GetTickCount64(),1000),CaptureWake::TargetExited);
    bool rejected=false;
    try { (void)capture_wake(nullptr,audio.value,stop,GetTickCount64(),1000); }
    catch(const Failure &error) { rejected=!strcmp(error.stage,"audio_wait"); }
    if(!rejected) { fprintf(stderr,"wake CHECK invalid-target must fail before stop\n"); failures++; }
    return failures ? 1 : 0;
}
static int run_tests() {
    uint64_t n=7;
    CHECK(number(L"1",120000,n) && n==1);
    CHECK(number(L"120000",120000,n) && n==120000);
    for (const auto *bad : {L"",L"0",L"-1",L"+1",L"120001",L"18446744073709551616",L"12x"})
        CHECK(!number(bad,120000,n));
    OwnedDirectory owned(L"fd2-process-audio-pure-");
    const fs::path &directory=owned.path;
    fs::path forbidden=directory/L"original_game";
    CHECK(fs::create_directory(forbidden));
    for (const auto &path : {fs::path(L"relative.wav"),forbidden/L"never.wav",directory/L"NUL.wav",directory/L"x:ads.wav",directory/L"dot.wav."}) {
        bool rejected=false;
        try { (void)output_path(path.c_str()); } catch (const Failure &) { rejected=true; }
        CHECK(rejected);
    }
    fs::path wave_path=output_path((directory/L"valid.wav").c_str());
    BYTE data[8]={0,1,2,3,4,5,6,7};
    {
        Wave wave; CHECK(SUCCEEDED(wave.open(wave_path)));
        CHECK(SUCCEEDED(wave.append(data,2,false)));
        CHECK(SUCCEEDED(wave.append(nullptr,3,true)));
        CHECK(wave.data_bytes==20 && SUCCEEDED(wave.finish()));
    }
    std::ifstream input(wave_path,std::ios::binary);
    std::vector<BYTE> bytes((std::istreambuf_iterator<char>(input)),{});
    CHECK(bytes.size()==64 && !memcmp(bytes.data(),"RIFF",4) && rd32(bytes.data()+4)==56);
    CHECK(!memcmp(bytes.data()+8,"WAVEfmt ",8) && rd32(bytes.data()+24)==44100);
    CHECK(bytes[20]==1 && bytes[22]==2 && bytes[32]==4 && bytes[34]==16 && rd32(bytes.data()+40)==20);
    CHECK(!memcmp(bytes.data()+44,data,8));
    for(size_t i=52;i<64;i++) CHECK(!bytes[i]);
    Wave duplicate; CHECK(FAILED(duplicate.open(wave_path)) && !duplicate.file);
    bool rejected=false;
    try { (void)output_path(wave_path.c_str()); } catch(const Failure &) { rejected=true; }
    CHECK(rejected);
    Wave read_only;
    read_only.file.value=CreateFileW(wave_path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    CHECK(read_only.file && FAILED(read_only.append(data,2,false)) && read_only.data_bytes==0);
    CHECK(FAILED(read_only.finish()));
    fs::path ready=output_path((directory/L"ready.json").c_str());
    CHECK(SUCCEEDED(publish_ready(ready,"{\"ready\":true}\n")));
    CHECK(FAILED(publish_ready(ready,"must not overwrite")));
    std::ifstream ready_input(ready,std::ios::binary);
    std::string ready_text((std::istreambuf_iterator<char>(ready_input)),{});
    CHECK(ready_text=="{\"ready\":true}\n");
    CHECK(quoted("\"\\\n")=="\"\\\"\\\\\\u000a\"");
    auto probe=Microsoft::WRL::Make<ProbeCapture>();
    Wave untouched;CaptureStats stats;
    CHECK(SUCCEEDED(drain(probe.Get(),untouched,stats,1)) && !probe->buffers && !stats.packets);
    CHECK(wait_tests(directory)==0);
    puts("process audio pure: 9 groups passed; real Win32 wait handles; system COM activation/capture not invoked");
    return 0;
}

int main() {
    try { return run_tests(); }
    catch(const Failure &error) {
        fprintf(stderr,"process audio pure exception %s HRESULT=0x%08lx\n",error.stage,(unsigned long)error.hr);
    } catch(const std::exception &error) { fprintf(stderr,"process audio pure: %s\n",error.what()); }
    return 1;
}
