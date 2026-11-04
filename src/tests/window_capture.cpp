/* SDL 宿主图形回归：Windows.Graphics.Capture 按 HWND 捕获独立窗口。
 * 依据 Microsoft CreateForWindow / CreateFreeThreaded / D3D11 interop 文档；
 * 不读取桌面 DC，不依赖前台或遮挡状态，不作为原版行为等价证据。
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
/* 本工具不使用协程；兼容 SDK C++17 投影与新 MSVC 的弃用检查。 */
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <cstdio>
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
