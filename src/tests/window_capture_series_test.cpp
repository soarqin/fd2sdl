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
static int run_tests() {
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
    printf("capture series pure: limits/timestamps/paths/atomic-ready/BGRA-PNG passed; no WGC/D3D/HWND invoked\n");
    return 0;
}

int main() {
    try { return run_tests(); }
    catch(const winrt::hresult_error &error) {
        fprintf(stderr,"capture series pure HRESULT=0x%08lx\n",(unsigned long)error.code());
    } catch(const std::exception &error) { fprintf(stderr,"capture series pure: %s\n",error.what()); }
    return 1;
}
