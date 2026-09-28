// Exercise the production window procedure and presentation leaf on real Win32/D3D9.
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include "pc/gcm/renderengine/WindowPresentationPCLeaf.h"
namespace renderengine { bool gFullscreen = false; }
struct NullLog { template<class T> NullLog& operator<<(const T&) { return *this; } };
namespace CgsDev { namespace Log { NullLog* gpDebugPrint = nullptr; } }
namespace CgsSystem { struct HardwareInit { static void RequestShutdown() {} }; }
#include "pc_fullscreen_window.inc"
static int checks, failures;
static void Check(bool good, const char* what) {
    ++checks;
    if (!good) { ++failures; std::printf("FAIL %s\n", what); }
}
static bool SurfaceIs(IDirect3DDevice9* device, IDirect3DSurface9* surface, const RECT& view) {
    D3DSURFACE_DESC desc = {};
    surface->GetDesc(&desc);
    IDirect3DSurface9* copy = nullptr;
    if (FAILED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr))) return false;
    HRESULT result = device->GetRenderTargetData(surface, copy);
    D3DLOCKED_RECT locked = {};
    bool matches = SUCCEEDED(result) && SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    if (matches) {
        for (UINT y = 0; y < desc.Height; ++y) {
            const DWORD* row = reinterpret_cast<const DWORD*>(static_cast<const char*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < desc.Width; ++x) {
                const bool inside = x >= static_cast<UINT>(view.left) && x < static_cast<UINT>(view.right) &&
                                    y >= static_cast<UINT>(view.top) && y < static_cast<UINT>(view.bottom);
                if ((row[x] & 0xffffff) != (inside ? 0x4997d1 : 0)) matches = false;
            }
        }
        copy->UnlockRect();
    }
    copy->Release();
    return matches;
}
int main() {
    WNDCLASSA cls = {};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = GetModuleHandle(nullptr);
    cls.lpszClassName = "BurnoutFullscreenRegression";
    RegisterClassA(&cls);
    HWND window = CreateWindowA(cls.lpszClassName, "Display regression", WS_OVERLAPPEDWINDOW,
                               70, 90, 400, 300, nullptr, nullptr, cls.hInstance, nullptr);
    if (!window) return 2;
    RECT initial;
    GetWindowRect(window, &initial);
    MONITORINFO monitor = {sizeof(MONITORINFO)};
    GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
    for (int cycle = 0; cycle < 2; ++cycle) {
        SendMessage(window, WM_KEYDOWN, VK_F11, 1);
        RECT actual;
        GetWindowRect(window, &actual);
        Check(renderengine::gFullscreen && EqualRect(&actual, &monitor.rcMonitor), "F11 fills the current monitor");
        Check((GetWindowLongPtr(window, GWL_STYLE) & WS_CAPTION) == 0, "fullscreen has no title bar");
        SendMessage(window, WM_KEYDOWN, VK_F11, 1LL << 30);
        Check(renderengine::gFullscreen, "held F11 does not toggle repeatedly");
        SendMessage(window, WM_SYSKEYDOWN, VK_F11, 1);
        GetWindowRect(window, &actual);
        Check(!renderengine::gFullscreen && EqualRect(&actual, &initial), "F11 restores exact window position and size");
        Check((GetWindowLongPtr(window, GWL_STYLE) & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW, "window frame restored");
    }
    const struct { LONG w,h; RECT view; } sizes[] = {
        {1920,1080,{0,0,1920,1080}}, {1920,1200,{0,60,1920,1140}},
        {3440,1440,{440,0,3000,1440}}, {1024,768,{0,96,1024,672}},
        {1080,1920,{0,656,1080,1263}}, {0,0,{0,0,0,0}}
    };
    for (const auto& s : sizes) {
        const RECT actual = renderengine::FitDisplay16By9(s.w,s.h);
        Check(EqualRect(&actual,&s.view), "16:9 fit: standard, tall, ultrawide, 4:3, portrait, minimized");
    }
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.BackBufferWidth = 320; pp.BackBufferHeight = 180;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.BackBufferCount = 1;
    pp.hDeviceWindow = window; pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT result = d3d ? d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device) : E_FAIL;
    Check(SUCCEEDED(result), "real D3D9 device created");
    if (device) {
        renderengine::PCPresentation presenter;
        IDirect3DSurface9* source = nullptr;
        device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&source);
        device->ColorFill(source,nullptr,0xff4997d1);
        const struct { int w,h; RECT view; } clients[] = {
            {400,300,{0,37,400,262}}, {800,300,{133,0,666,300}}, {320,180,{0,0,320,180}}
        };
        for (const auto& c : clients) {
            RECT outer = {0,0,c.w,c.h};
            AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE);
            SetWindowPos(window,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            Check(SUCCEEDED(presenter.Present(device,window,false)), "presentation succeeds after resizing");
            IDirect3DSurface9* output = nullptr;
            if (presenter.mpSwapChain) presenter.mpSwapChain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&output);
            else device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&output);
            Check(output && SurfaceIs(device,output,c.view), "real output pixels: centered game image and solid black bars");
            if (output) output->Release();
            const RECT whole = {0,0,320,180};
            Check(SurfaceIs(device,source,whole), "original render buffer preserved for overlay rendering");
        }
        presenter.Release();
        source->Release();
        device->Release();
    }
    if (d3d) d3d->Release();
    DestroyWindow(window);
    UnregisterClassA(cls.lpszClassName,cls.hInstance);
    std::printf("PCFullscreen: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
