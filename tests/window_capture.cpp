/* SDL 宿主图形回归：Windows.Graphics.Capture 按 HWND 捕获独立窗口。
 * 依据 Microsoft CreateForWindow / CreateFreeThreaded / D3D11 interop 文档；
 * 不读取桌面 DC，不依赖前台或遮挡状态，不作为原版行为等价证据。
 * 本机2026-11-07 WER记录关闭后GraphicsCapture.dll_unloaded的execute AV；
 * 系统内部异步线程晚于COM卸载是时序推断，未取得dump完整栈。
 * 依GetModuleHandleExW PIN合同固定该系统模块到短命helper退出，仍正常Close/CoUninitialize。
 * https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
/* 本工具不使用协程；兼容 SDK C++17 投影与新 MSVC 的弃用检查。 */
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <set>
#include <stdexcept>
#include <vector>

using namespace winrt::Windows::Graphics::Capture;
using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
using winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
using Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;

namespace {

struct Apartment {
    Apartment() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~Apartment() { winrt::uninit_apartment(); }
};

using SystemDirectoryFn = UINT (WINAPI *)(LPWSTR, UINT);
using ModulePinFn = BOOL (WINAPI *)(DWORD, LPCWSTR, HMODULE *);

/* CreateFreeThreaded有内部线程。拿到activation factory后，仅固定已加载的
 * System32模块；不另行加载DLL、不延时、不跳过session/pool或COM的清理。
 * 两个局部函数参数仅供无WGC的fake回归，不导出测试接口。 */
void pin_capture_module(SystemDirectoryFn system_directory = GetSystemDirectoryW,
                        ModulePinFn pin_module = GetModuleHandleExW) {
    wchar_t directory[MAX_PATH + 1]{};
    const UINT capacity = static_cast<UINT>(sizeof(directory) / sizeof(directory[0]));
    const UINT length = system_directory(directory, capacity);
    if (!length || length >= capacity)
        throw std::runtime_error("cannot resolve System32 for GraphicsCapture.dll lifetime");
    const std::wstring path = std::wstring(directory, length) + L"\\GraphicsCapture.dll";
    HMODULE module = nullptr;
    if (!pin_module(GET_MODULE_HANDLE_EX_FLAG_PIN, path.c_str(), &module) || !module)
        throw std::runtime_error("cannot pin already-loaded System32 GraphicsCapture.dll until helper exit");
    /* PIN依据API合同保持到进程退出，不调用FreeLibrary；不是COM/session owner。 */
}

struct Capture {
    Direct3D11CaptureFramePool pool{nullptr};
    GraphicsCaptureSession session{nullptr};
    ~Capture() {
        try {
            if (session) session.Close();
            if (pool) pool.Close();
        } catch (...) { /* 错误路径仍释放 COM 引用。 */ }
    }
};

struct TextureMapGuard {
    ID3D11DeviceContext *context;
    ID3D11Resource *texture;
    ~TextureMapGuard() { if (context) context->Unmap(texture, 0); }
};

struct Geometry {
    RECT bounds{};
    RECT client{};
    POINT origin{};
};

Geometry geometry(HWND window) {
    if (!IsWindow(window) || !IsWindowVisible(window) || IsIconic(window))
        throw std::runtime_error("capture requires an existing, visible, non-minimized window");
    Geometry result;
    winrt::check_hresult(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
                                              &result.bounds, sizeof(result.bounds)));
    if (!GetClientRect(window, &result.client) ||
        !ClientToScreen(window, &result.origin))
        throw std::runtime_error("cannot obtain window client geometry");
    return result;
}

bool same_geometry(const Geometry &a, const Geometry &b) {
    return EqualRect(&a.bounds, &b.bounds) && EqualRect(&a.client, &b.client) &&
           a.origin.x == b.origin.x && a.origin.y == b.origin.y;
}

void write_bitmap(const char *path, int width, int height,
                  const std::vector<unsigned char> &pixels) {
    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER image_header{};
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(image_header);
    file_header.bfSize = file_header.bfOffBits + static_cast<DWORD>(pixels.size());
    image_header.biSize = sizeof(image_header);
    image_header.biWidth = width;
    image_header.biHeight = height;
    image_header.biPlanes = 1;
    image_header.biBitCount = 24;
    image_header.biSizeImage = static_cast<DWORD>(pixels.size());
    FILE *file = std::fopen(path, "wb");
    if (!file) throw std::runtime_error("cannot create capture BMP");
    bool ok = std::fwrite(&file_header, sizeof(file_header), 1, file) == 1 &&
              std::fwrite(&image_header, sizeof(image_header), 1, file) == 1 &&
              std::fwrite(pixels.data(), pixels.size(), 1, file) == 1;
    if (std::fclose(file) != 0) ok = false;
    if (!ok) {
        std::remove(path);
        throw std::runtime_error("cannot finish capture BMP");
    }
}

} // namespace

extern "C" int fd2_capture_window(HWND window, const char *path) {
    try {
        Apartment apartment;
        if (!GraphicsCaptureSession::IsSupported())
            throw std::runtime_error("Windows.Graphics.Capture is not supported on this device");
        Geometry before = geometry(window);
        auto factory = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        pin_capture_module();
        GraphicsCaptureItem item{nullptr};
        winrt::check_hresult(factory->CreateForWindow(window, winrt::guid_of<GraphicsCaptureItem>(),
                                                      winrt::put_abi(item)));
        auto size = item.Size();
        if (size.Width <= 0 || size.Height <= 0 || size.Width > 8192 || size.Height > 8192)
            throw std::runtime_error("invalid capture item size");

        winrt::com_ptr<ID3D11Device> device;
        winrt::com_ptr<ID3D11DeviceContext> context;
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
            device.put(), nullptr, context.put()));
        auto dxgi = device.as<IDXGIDevice>();
        winrt::com_ptr<IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put()));
        auto graphics_device = inspectable.as<IDirect3DDevice>();
        Capture capture;
        capture.pool = Direct3D11CaptureFramePool::CreateFreeThreaded(
            graphics_device, DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, size);
        capture.session = capture.pool.CreateCaptureSession(item);
        if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
                L"Windows.Graphics.Capture.GraphicsCaptureSession", L"IsCursorCaptureEnabled"))
            capture.session.IsCursorCaptureEnabled(false);
        capture.session.StartCapture();
        Direct3D11CaptureFrame frame{nullptr};
        ULONGLONG deadline = GetTickCount64() + 5000;
        while (!frame && GetTickCount64() < deadline) {
            frame = capture.pool.TryGetNextFrame();
            if (!frame) Sleep(10);
        }
        if (!frame) throw std::runtime_error("window capture timed out waiting for a frame (5s)");
        auto content = frame.ContentSize();
        Geometry after = geometry(window);
        if (!same_geometry(before, after) || content.Width != size.Width || content.Height != size.Height)
            throw std::runtime_error("window geometry changed during capture; retry at a stable size");
        if (content.Width != before.bounds.right - before.bounds.left ||
            content.Height != before.bounds.bottom - before.bounds.top) {
            std::fprintf(stderr, "capture size=%dx%d DWM bounds=%ldx%ld\n", content.Width, content.Height,
                         before.bounds.right - before.bounds.left, before.bounds.bottom - before.bounds.top);
            throw std::runtime_error("capture frame does not match DWM window bounds; refusing guessed crop");
        }
        int width = before.client.right, height = before.client.bottom;
        int left = before.origin.x - before.bounds.left;
        int top = before.origin.y - before.bounds.top;
        if (width <= 0 || height <= 0 || left < 0 || top < 0 ||
            left + width > content.Width || top + height > content.Height)
            throw std::runtime_error("client rectangle is not contained in captured window");

        auto access = frame.Surface().as<IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<ID3D11Texture2D> texture;
        winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM ||
            desc.Width < static_cast<UINT>(content.Width) || desc.Height < static_cast<UINT>(content.Height))
            throw std::runtime_error("unexpected capture texture format or size");
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        winrt::com_ptr<ID3D11Texture2D> staging;
        winrt::check_hresult(device->CreateTexture2D(&desc, nullptr, staging.put()));
        context->CopyResource(staging.get(), texture.get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        winrt::check_hresult(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
        TextureMapGuard map_guard{context.get(), staging.get()};
        size_t stride = (static_cast<size_t>(width) * 3 + 3) & ~size_t(3);
        std::vector<unsigned char> pixels(stride * height);
        std::set<unsigned int> colors;
        for (int y = 0; y < height; y++) {
            const auto *source = static_cast<const unsigned char *>(mapped.pData) +
                                 static_cast<size_t>(top + y) * mapped.RowPitch + left * 4;
            auto *dest = pixels.data() + static_cast<size_t>(height - 1 - y) * stride;
            for (int x = 0; x < width; x++) {
                dest[x * 3] = source[x * 4];
                dest[x * 3 + 1] = source[x * 4 + 1];
                dest[x * 3 + 2] = source[x * 4 + 2];
                if (colors.size() < 8)
                    colors.insert(source[x * 4] | (source[x * 4 + 1] << 8) | (source[x * 4 + 2] << 16));
            }
        }
        context->Unmap(staging.get(), 0);
        map_guard.context = nullptr;
        frame.Close();
        if (colors.size() < 8) throw std::runtime_error("blank or nearly uniform captured client area");
        write_bitmap(path, width, height, pixels);
        return 0;
    } catch (const winrt::hresult_error &error) {
        std::fprintf(stderr, "Windows.Graphics.Capture failed (0x%08lx): %s\n",
                     static_cast<unsigned long>(error.code().value), winrt::to_string(error.message()).c_str());
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Windows.Graphics.Capture failed: %s\n", error.what());
    }
    return -1;
}

/* B6连续取证：一组HWND/item/device/framepool/session复用到stop；PNG保留
 * client原始BGRA，不用旧单帧的低色拒绝门（渐黑/闪白也是合法原始帧）。
 * INTERVAL_MS是实际WGC时间戳的最小间距，不合成固定帧率或重复旧图。
 * 只输出取证，不在此声称帧等价。旧fd2_capture_window实现保持原样。 */
namespace {
namespace fs = std::filesystem;
struct SeriesHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~SeriesHandle() { if (value && value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct SeriesFrame {
    Direct3D11CaptureFrame value{nullptr};
    ~SeriesFrame() { try { if (value) value.Close(); } catch (...) {} }
    void close() { value.Close(); value=nullptr; }
};
void series_check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
void series_limits(unsigned long max_ms, unsigned long interval_ms) {
    series_check(max_ms>=1 && max_ms<=120000 && interval_ms>=1 && interval_ms<=120000,
                 "MAX_MS and INTERVAL_MS must be1..120000");
}
std::wstring series_lower(std::wstring value) {
    for (auto &c:value) c=static_cast<wchar_t>(towlower(c));
    return value;
}
fs::path series_directory(const wchar_t *argument) {
    series_check(argument && *argument,"missing OUTDIR");
    fs::path path(argument);
    series_check(path.is_absolute() && !path.filename().empty(),"OUTDIR must be an absolute new directory");
    auto name=path.filename().wstring();
    auto base=series_lower(name.substr(0,name.find(L'.')));
    series_check(name.back()!=L'.' && name.back()!=L' ' &&
        name.find_first_of(L"<>:\"/\\|?*")==std::wstring::npos &&
        base!=L"con" && base!=L"prn" && base!=L"aux" && base!=L"nul" &&
        !(base.size()==4 && (base.substr(0,3)==L"com" || base.substr(0,3)==L"lpt") && base[3]>=L'0' && base[3]<=L'9'),
        "invalid OUTDIR name");
    for (wchar_t c:name) series_check(c>=32,"OUTDIR contains control characters");
    SeriesHandle parent{CreateFileW(path.parent_path().c_str(),FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr)};
    if (parent.value==INVALID_HANDLE_VALUE) winrt::throw_last_error();
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(parent.value,&info)) winrt::throw_last_error();
    series_check((info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0,"OUTDIR parent is not a directory");
    DWORD count=GetFinalPathNameByHandleW(parent.value,nullptr,0,FILE_NAME_NORMALIZED);
    if (!count) winrt::throw_last_error();
    std::wstring resolved(count,L'\0');
    DWORD written=GetFinalPathNameByHandleW(parent.value,&resolved[0],count,FILE_NAME_NORMALIZED);
    if (!written || written>=count) winrt::throw_last_error();
    resolved.resize(written);
    fs::path result=fs::path(resolved)/name;
    for (const auto &component:result)
        series_check(series_lower(component.wstring())!=L"original_game","OUTDIR must not be inside original_game");
    series_check(GetFileAttributesW(result.c_str())==INVALID_FILE_ATTRIBUTES,"OUTDIR already exists");
    if (GetLastError()!=ERROR_FILE_NOT_FOUND) winrt::throw_last_error();
    return result;
}
void series_write(HANDLE file,const void *data,DWORD size) {
    const auto *p=static_cast<const BYTE*>(data);
    while (size) {
        DWORD written=0;
        if (!WriteFile(file,p,size,&written,nullptr)) winrt::throw_last_error();
        series_check(written!=0,"zero-byte file write");
        p+=written;size-=written;
    }
}
void series_text(const fs::path &path,const std::string &text) {
    SeriesHandle file{CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if (file.value==INVALID_HANDLE_VALUE) winrt::throw_last_error();
    series_write(file.value,text.data(),static_cast<DWORD>(text.size()));
    if (!FlushFileBuffers(file.value)) winrt::throw_last_error();
}
void series_ready(const fs::path &directory,const std::string &text) {
    fs::path temporary=directory/L"ready.tmp";
    series_text(temporary,text);
    if (!MoveFileExW(temporary.c_str(),(directory/L"ready.json").c_str(),MOVEFILE_WRITE_THROUGH)) winrt::throw_last_error();
}
std::string series_quote(const std::string &text) {
    std::string result="\"";
    const char hex[]="0123456789abcdef";
    for (unsigned char c:text) {
        if (c=='"' || c=='\\') {result+='\\';result+=static_cast<char>(c);}
        else if (c<32) {result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
        else result+=static_cast<char>(c);
    }
    return result+'"';
}
void series_owner(HWND window,DWORD pid) {
    DWORD actual=0;
    series_check(pid && IsWindow(window) && IsWindowVisible(window) && !IsIconic(window) &&
        GetWindowThreadProcessId(window,&actual) && actual==pid,"capture-series PID/HWND ownership or visibility changed");
}
bool series_timestamp(int64_t timestamp,int64_t &last_seen) {
    series_check(timestamp>=0 && timestamp>=last_seen,"WGC timestamp moved backwards");
    if (timestamp==last_seen) return false;
    last_seen=timestamp;return true;
}
/* 仅已保存帧的观测：不改变WGC/像素/采样选择，不把空poll或PNG耗时当成丢帧数。 */
struct SeriesFrameMetrics {
    int64_t poll_started_qpc,poll_finished_qpc,arrival_qpc,copy_finished_qpc,png_finished_qpc;
    unsigned long long empty_polls_before_frame;
};
std::string series_metrics_json(const SeriesFrameMetrics &m) {
    series_check(m.poll_started_qpc>=0 && m.poll_started_qpc<=m.poll_finished_qpc &&
        m.poll_finished_qpc<=m.arrival_qpc && m.arrival_qpc<=m.copy_finished_qpc &&
        m.copy_finished_qpc<=m.png_finished_qpc,"capture metrics QPC order invalid");
    std::ostringstream out;
    out << ",\"poll_started_qpc\":" << m.poll_started_qpc
        << ",\"poll_finished_qpc\":" << m.poll_finished_qpc
        << ",\"copy_finished_qpc\":" << m.copy_finished_qpc
        << ",\"png_finished_qpc\":" << m.png_finished_qpc
        << ",\"empty_polls_before_frame\":" << m.empty_polls_before_frame;
    return out.str();
}
void series_png(IWICImagingFactory *factory,const fs::path &path,
                UINT width,UINT height,const std::vector<BYTE> &bgra) {
    series_check(width && height && width<=8192 && height<=8192 &&
        bgra.size()==static_cast<size_t>(width)*height*4,"invalid PNG BGRA buffer");
    winrt::com_ptr<IStream> memory;
    winrt::check_hresult(CreateStreamOnHGlobal(nullptr,TRUE,memory.put()));
    winrt::com_ptr<IWICBitmapEncoder> encoder;
    winrt::check_hresult(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put()));
    winrt::check_hresult(encoder->Initialize(memory.get(),WICBitmapEncoderNoCache));
    winrt::com_ptr<IWICBitmapFrameEncode> image;
    winrt::check_hresult(encoder->CreateNewFrame(image.put(),nullptr));
    winrt::check_hresult(image->Initialize(nullptr));
    winrt::check_hresult(image->SetSize(width,height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    winrt::check_hresult(image->SetPixelFormat(&format));
    series_check(IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA),"PNG codec changed requested BGRA format");
    winrt::check_hresult(image->WritePixels(height,width*4,static_cast<UINT>(bgra.size()),const_cast<BYTE*>(bgra.data())));
    winrt::check_hresult(image->Commit());winrt::check_hresult(encoder->Commit());
    LARGE_INTEGER zero{};winrt::check_hresult(memory->Seek(zero,STREAM_SEEK_SET,nullptr));
    SeriesHandle file{CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if (file.value==INVALID_HANDLE_VALUE) winrt::throw_last_error();
    BYTE bytes[65536];
    for (;;) {
        ULONG count=0;winrt::check_hresult(memory->Read(bytes,sizeof(bytes),&count));
        if (!count) break;
        series_write(file.value,bytes,count);
    }
}
} // namespace

extern "C" int fd2_capture_window_series(HWND window,DWORD pid,const wchar_t *outdir,
                                          unsigned long max_ms,unsigned long interval_ms) {
    fs::path directory;
    bool created=false,started=false,ready=false;
    unsigned frames=0,duplicates=0,interval_skips=0;
    unsigned long long empty_polls=0,empty_polls_before_frame=0;
    int64_t last_seen=-1,last_saved=-1,first_saved=-1;
    ULONGLONG begin=GetTickCount64();
    HRESULT status=S_OK;
    std::string error;
    const char *reason="error";
    std::ostringstream handle_text;handle_text << "0x" << std::hex << reinterpret_cast<UINT_PTR>(window);
    const std::string window_id=handle_text.str();
    LARGE_INTEGER qpc_frequency{};QueryPerformanceFrequency(&qpc_frequency);
    try {
        series_limits(max_ms,interval_ms);
        directory=series_directory(outdir);
        series_owner(window,pid);
        Geometry initial=geometry(window);
        series_check(CreateDirectoryW(directory.c_str(),nullptr)!=0,"cannot create new OUTDIR");created=true;
        Apartment apartment;
        series_check(GraphicsCaptureSession::IsSupported(),"Windows.Graphics.Capture is unavailable");
        auto factory=winrt::get_activation_factory<GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
        pin_capture_module();
        GraphicsCaptureItem item{nullptr};
        winrt::check_hresult(factory->CreateForWindow(window,winrt::guid_of<GraphicsCaptureItem>(),winrt::put_abi(item)));
        auto size=item.Size();
        series_check(size.Width>0 && size.Height>0 && size.Width<=8192 && size.Height<=8192,"invalid initial WGC size");
        winrt::com_ptr<ID3D11Device> device;winrt::com_ptr<ID3D11DeviceContext> context;
        winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,device.put(),nullptr,context.put()));
        auto dxgi=device.as<IDXGIDevice>();winrt::com_ptr<IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(),inspectable.put()));
        auto graphics_device=inspectable.as<IDirect3DDevice>();
        winrt::com_ptr<IWICImagingFactory> codec;
        winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(codec.put())));
        Capture capture;
        capture.pool=Direct3D11CaptureFramePool::CreateFreeThreaded(graphics_device,DirectXPixelFormat::B8G8R8A8UIntNormalized,3,size);
        capture.session=capture.pool.CreateCaptureSession(item);
        if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
            L"Windows.Graphics.Capture.GraphicsCaptureSession",L"IsCursorCaptureEnabled")) capture.session.IsCursorCaptureEnabled(false);
        SeriesHandle index{CreateFileW((directory/L"frames.jsonl").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if (index.value==INVALID_HANDLE_VALUE) winrt::throw_last_error();
        winrt::com_ptr<ID3D11Texture2D> staging;
        D3D11_TEXTURE2D_DESC staging_desc{};
        capture.session.StartCapture();started=true;begin=GetTickCount64();
        while (frames<600) {
            DWORD stop_attributes=GetFileAttributesW((directory/L"stop").c_str());
            if (stop_attributes!=INVALID_FILE_ATTRIBUTES) {
                series_check(!(stop_attributes&FILE_ATTRIBUTE_DIRECTORY),"stop is a directory");reason="stop";break;
            }
            if (GetLastError()!=ERROR_FILE_NOT_FOUND) winrt::throw_last_error();
            if (GetTickCount64()-begin>=max_ms) {reason="time_limit";break;}
            series_owner(window,pid);
            LARGE_INTEGER poll_started{},poll_finished{};
            QueryPerformanceCounter(&poll_started);
            SeriesFrame frame{capture.pool.TryGetNextFrame()};
            QueryPerformanceCounter(&poll_finished);
            if (!frame.value) {
                empty_polls++;empty_polls_before_frame++;
                series_check(frames || GetTickCount64()-begin<5000,"WGC timed out before the first frame");
                Sleep(2);continue;
            }
            ULONGLONG arrival_tick=GetTickCount64();
            LARGE_INTEGER arrival_qpc{};QueryPerformanceCounter(&arrival_qpc);
            Geometry before=geometry(window);auto content=frame.value.ContentSize();
            series_check(content.Width==size.Width && content.Height==size.Height &&
                before.client.right==initial.client.right && before.client.bottom==initial.client.bottom,
                "window size changed during capture-series");
            series_check(content.Width==before.bounds.right-before.bounds.left &&
                content.Height==before.bounds.bottom-before.bounds.top,"WGC content does not match DWM window bounds");
            int width=before.client.right,height=before.client.bottom;
            int left=before.origin.x-before.bounds.left,top=before.origin.y-before.bounds.top;
            series_check(width>0 && height>0 && left>=0 && top>=0 &&
                left+width<=content.Width && top+height<=content.Height,"client ROI outside WGC content");
            int64_t timestamp=frame.value.SystemRelativeTime().count();
            if (!series_timestamp(timestamp,last_seen)) {duplicates++;continue;}
            if (last_saved>=0 && timestamp-last_saved<static_cast<int64_t>(interval_ms)*10000) {interval_skips++;continue;}
            auto access=frame.value.Surface().as<IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> texture;
            winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D),texture.put_void()));
            D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
            series_check(desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM && desc.Width>=static_cast<UINT>(content.Width) &&
                desc.Height>=static_cast<UINT>(content.Height),"unexpected WGC texture format/size");
            if (!staging) {
                staging_desc=desc;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
                winrt::check_hresult(device->CreateTexture2D(&desc,nullptr,staging.put()));
            } else series_check(desc.Width==staging_desc.Width && desc.Height==staging_desc.Height,"WGC backing texture size changed");
            context->CopyResource(staging.get(),texture.get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            winrt::check_hresult(context->Map(staging.get(),0,D3D11_MAP_READ,0,&mapped));
            TextureMapGuard guard{context.get(),staging.get()};
            series_check(mapped.pData && mapped.RowPitch>=desc.Width*4,"invalid mapped WGC row pitch");
            std::vector<BYTE> pixels(static_cast<size_t>(width)*height*4);
            for (int y=0;y<height;y++) memcpy(pixels.data()+static_cast<size_t>(y)*width*4,
                static_cast<const BYTE*>(mapped.pData)+static_cast<size_t>(top+y)*mapped.RowPitch+left*4,static_cast<size_t>(width)*4);
            context->Unmap(staging.get(),0);guard.context=nullptr;
            series_owner(window,pid);
            series_check(same_geometry(before,geometry(window)),"window geometry changed while copying a frame");
            frame.close(); /* 先归还WGC缓冲，再做PNG压缩和文件IO。 */
            LARGE_INTEGER copy_finished{},png_finished{};
            QueryPerformanceCounter(&copy_finished);
            wchar_t name[40];swprintf_s(name,L"frame%05u.png",frames);
            series_png(codec.get(),directory/name,static_cast<UINT>(width),static_cast<UINT>(height),pixels);
            QueryPerformanceCounter(&png_finished);
            std::ostringstream row;
            row << "{\"index\":" << frames << ",\"file\":\"frame";
            char number[16];snprintf(number,sizeof(number),"%05u",frames);
            row << number << ".png\",\"wgc_timestamp_100ns\":" << timestamp
                << ",\"content_width\":" << content.Width << ",\"content_height\":" << content.Height
                << ",\"client_width\":" << width << ",\"client_height\":" << height
                << ",\"crop_x\":" << left << ",\"crop_y\":" << top << ",\"pid\":" << pid
                << ",\"hwnd\":" << series_quote(window_id)
                << ",\"arrival_tick_ms\":" << arrival_tick << ",\"arrival_qpc\":" << arrival_qpc.QuadPart
                << series_metrics_json({poll_started.QuadPart,poll_finished.QuadPart,arrival_qpc.QuadPart,
                    copy_finished.QuadPart,png_finished.QuadPart,empty_polls_before_frame}) << "}\n";
            auto line=row.str();series_write(index.value,line.data(),static_cast<DWORD>(line.size()));
            last_saved=timestamp;if (first_saved<0) first_saved=timestamp;frames++;
            empty_polls_before_frame=0; /* 只有确实写入一条rawrow才重置；跳重/interval不重置。 */
            if (!ready) {
                if (!FlushFileBuffers(index.value)) winrt::throw_last_error();
                std::ostringstream text;text << "{\"ready\":true,\"pid\":" << pid
                    << ",\"hwnd\":" << series_quote(window_id)
                    << ",\"first_index\":0,\"wgc_timestamp_100ns\":" << timestamp
                    << ",\"client_width\":" << width << ",\"client_height\":" << height << "}\n";
                series_ready(directory,text.str());ready=true;
            }
        }
        if (frames==600) reason="frame_limit";
        series_check(frames || std::string(reason)=="stop","capture-series ended without a valid frame");
        if (!FlushFileBuffers(index.value)) winrt::throw_last_error();
    } catch (const winrt::hresult_error &e) {status=e.code().value;error=winrt::to_string(e.message());}
      catch (const std::exception &e) {status=E_FAIL;error=e.what();}
    auto summary_text=[&]() {
    std::ostringstream summary;
    summary << "{\"ok\":" << (SUCCEEDED(status)?"true":"false") << ",\"backend\":\"WGC-HWND-session\",\"pid\":" << pid
        << ",\"hwnd\":" << series_quote(window_id) << ",\"qpc_frequency\":" << qpc_frequency.QuadPart
        << ",\"session_started\":" << (started?"true":"false") << ",\"ready_published\":" << (ready?"true":"false")
        << ",\"frames\":" << frames << ",\"frame_limit\":600,\"max_ms\":" << max_ms << ",\"interval_ms\":" << interval_ms
        << ",\"duplicate_timestamps_skipped\":" << duplicates << ",\"interval_frames_skipped\":" << interval_skips
        << ",\"empty_polls\":" << empty_polls << ",\"empty_polls_after_last_saved_frame\":" << empty_polls_before_frame
        << ",\"first_wgc_timestamp_100ns\":" << first_saved << ",\"last_wgc_timestamp_100ns\":" << last_saved
        << ",\"elapsed_ms\":" << GetTickCount64()-begin << ",\"stop_reason\":" << series_quote(reason)
        << ",\"pixel_format\":\"BGRA8-lossless-PNG\",\"color_filter\":false,\"frame_equivalence_claimed\":false"
        << ",\"hresult\":" << static_cast<unsigned long>(status) << ",\"error\":" << series_quote(error) << "}\n";
    return summary.str();
    };
    std::string text=summary_text();
    if (created) {
        try {series_text(directory/L"summary.json",text);}
        catch (...) {status=E_FAIL;error="capture-series could not create summary.json";text=summary_text();}
    }
    std::fwrite(text.data(),1,text.size(),stdout);std::fflush(stdout);
    if (FAILED(status)) std::fprintf(stderr,"capture-series failed: %s\n",error.c_str());
    return SUCCEEDED(status)?0:1;
}
