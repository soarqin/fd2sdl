/* WGC持续采帧helper的无窗口合同回归。
 * 依据：window_capture.cpp的时间戳/原像素PNG/文件协议。
 * 不调用capture入口、D3D、WGC或创建HWND；WIC仅作离线PNG编解码。 */
#include "window_capture.cpp"
#include <fstream>
#include <stdexcept>
#define CHECK(e) do { if (!(e)) {fprintf(stderr,"pure CHECK line%d %s\n",__LINE__,#e);return 1;} } while(0)
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

/* 只验证生产helper的API合同；不加载DLL、不调用真实Windows API或WGC。
 * fake必须收到系统绝对路径和PIN，不能误变成普通AddRef后再FreeLibrary。 */
struct ModulePinFixture {
    unsigned directory_calls=0,pin_calls=0;
    UINT directory_result=19;
    BOOL pin_result=TRUE;
    HMODULE module=reinterpret_cast<HMODULE>(static_cast<UINT_PTR>(1));
    DWORD flags=0;
    std::wstring path;
};
static ModulePinFixture *pin_fixture=nullptr;
static UINT WINAPI fake_system_directory(LPWSTR buffer,UINT capacity) {
    ++pin_fixture->directory_calls;
    constexpr wchar_t directory[]=L"C:\\Windows\\System32";
    if (capacity>=sizeof(directory)/sizeof(directory[0]))
        memcpy(buffer,directory,sizeof(directory));
    return pin_fixture->directory_result;
}
static BOOL WINAPI fake_pin_module(DWORD flags,LPCWSTR path,HMODULE *module) {
    ++pin_fixture->pin_calls;pin_fixture->flags=flags;pin_fixture->path=path;
    *module=pin_fixture->module;
    return pin_fixture->pin_result;
}
static int module_lifetime_tests() {
    ModulePinFixture fixture;pin_fixture=&fixture;
    pin_capture_module(fake_system_directory,fake_pin_module);
    CHECK(fixture.directory_calls==1 && fixture.pin_calls==1);
    CHECK(fixture.flags==GET_MODULE_HANDLE_EX_FLAG_PIN);
    CHECK(fixture.path==L"C:\\Windows\\System32\\GraphicsCapture.dll");
    for (UINT result:{0u,static_cast<UINT>(MAX_PATH+1)}) {
        fixture={};fixture.directory_result=result;
        bool rejected=false;
        try {pin_capture_module(fake_system_directory,fake_pin_module);}
        catch(const std::runtime_error &) {rejected=true;}
        CHECK(rejected && fixture.directory_calls==1 && fixture.pin_calls==0);
    }
    for (bool missing_handle:{false,true}) {
        fixture={};
        if (missing_handle) fixture.module=nullptr;
        else fixture.pin_result=FALSE;
        bool rejected=false;
        try {pin_capture_module(fake_system_directory,fake_pin_module);}
        catch(const std::runtime_error &) {rejected=true;}
        CHECK(rejected && fixture.directory_calls==1 && fixture.pin_calls==1);
    }
    pin_fixture=nullptr;
    return 0;
}
static int run_tests() {
    CHECK(module_lifetime_tests()==0);
    bool rejected=false;
    series_limits(1,1);series_limits(120000,120000);
    for (auto pair:{std::pair<unsigned long,unsigned long>{0,1},{120001,1},{1,0},{1,120001}}) {
        rejected=false;try {series_limits(pair.first,pair.second);}catch(...){rejected=true;}CHECK(rejected);
    }
    int64_t last=-1;
    CHECK(series_timestamp(0,last) && last==0);
    CHECK(!series_timestamp(0,last) && last==0);
    CHECK(series_timestamp(550000,last) && last==550000);
    CHECK(series_quote("\"\\\n")=="\"\\\"\\\\\\u000a\"");
    rejected=false;try{series_timestamp(549999,last);}catch(...){rejected=true;}CHECK(rejected && last==550000);
    /* 用真实serializer验证只追加数值合同及时间序；不运行WGC或模拟采集吞吐。 */
    const SeriesFrameMetrics timing{100,110,120,150,200,7};
    const std::string fields=series_metrics_json(timing);
    CHECK(fields.find("\"poll_started_qpc\":100")!=std::string::npos);
    CHECK(fields.find("\"poll_finished_qpc\":110")!=std::string::npos);
    CHECK(fields.find("\"copy_finished_qpc\":150")!=std::string::npos);
    CHECK(fields.find("\"png_finished_qpc\":200")!=std::string::npos);
    CHECK(fields.find("\"empty_polls_before_frame\":7")!=std::string::npos);
    CHECK(series_metrics_json({0,0,0,0,0,0}).find("\"empty_polls_before_frame\":0")!=std::string::npos);
    for (const auto &bad:{SeriesFrameMetrics{-1,110,120,150,200,7},
        SeriesFrameMetrics{111,110,120,150,200,7},SeriesFrameMetrics{100,121,120,150,200,7},
        SeriesFrameMetrics{100,110,151,150,200,7},SeriesFrameMetrics{100,110,120,201,200,7}}) {
        rejected=false;try{(void)series_metrics_json(bad);}catch(...){rejected=true;}CHECK(rejected);
    }
    OwnedDirectory owned(L"fd2-capture-series-pure-");
    const fs::path &root=owned.path;
    CHECK(CreateDirectoryW((root/L"original_game").c_str(),nullptr));
    CHECK(CreateDirectoryW((root/L"exists").c_str(),nullptr));
    for (const auto &path:{fs::path(L"relative"),root/L"NUL",root/L"bad:ads",root/L"dot.",root/L"exists",root/L"original_game/new"}) {
        rejected=false;try{(void)series_directory(path.c_str());}catch(...){rejected=true;}CHECK(rejected);
    }
    fs::path directory=series_directory((root/L"series").c_str());
    CHECK(CreateDirectoryW(directory.c_str(),nullptr));
    series_ready(directory,"{\"ready\":true}\n");
    CHECK(GetFileAttributesW((directory/L"ready.json").c_str())!=INVALID_FILE_ATTRIBUTES);
    rejected=false;try{series_ready(directory,"replacement");}catch(...){rejected=true;}CHECK(rejected);
    std::ifstream ready(directory/L"ready.json",std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(ready)),{});
    CHECK(text=="{\"ready\":true}\n");
    Apartment apartment;
    winrt::com_ptr<IWICImagingFactory> factory;
    winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.put())));
    std::vector<BYTE> original={0,1,255,0, 254,128,5,255, 42,43,44,128, 80,90,100,17};
    fs::path png=directory/L"known.png";
    series_png(factory.get(),png,2,2,original);
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::check_hresult(factory->CreateDecoderFromFilename(png.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,decoder.put()));
    winrt::com_ptr<IWICBitmapFrameDecode> frame;winrt::check_hresult(decoder->GetFrame(0,frame.put()));
    winrt::com_ptr<IWICFormatConverter> converter;winrt::check_hresult(factory->CreateFormatConverter(converter.put()));
    winrt::check_hresult(converter->Initialize(frame.get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    std::vector<BYTE> decoded(16);
    winrt::check_hresult(converter->CopyPixels(nullptr,8,16,decoded.data()));
    CHECK(decoded==original); /* 包含alpha0下的非零RGB，不能量化、乘alpha或颜色归一。 */
    rejected=false;try{series_png(factory.get(),png,2,2,original);}catch(...){rejected=true;}CHECK(rejected);
    rejected=false;try{series_png(factory.get(),directory/L"bad.png",2,3,original);}catch(...){rejected=true;}CHECK(rejected);
    printf("capture series pure: module-PIN/limits/timestamps/paths/atomic-ready/BGRA-PNG passed; no WGC/D3D/HWND invoked\n");
    return 0;
}

int main() {
    try { return run_tests(); }
    catch(const winrt::hresult_error &error) {
        fprintf(stderr,"capture series pure HRESULT=0x%08lx\n",(unsigned long)error.code());
    } catch(const std::exception &error) { fprintf(stderr,"capture series pure: %s\n",error.what()); }
    return 1;
}
