#pragma once

#include <Windows.h>
#include <d3d9.h>

// FLAG PC-platform leaf: owner-requested F11 borderless fullscreen and 16:9
// presentation. The console owns its display; Windows window placement and
// letterboxing are host policy. Render size changes at the next frame boundary.
namespace renderengine
{
    inline void EnablePerMonitorDpi()
    {
        // Resolve dynamically for older Windows versions. This must precede
        // CreateWindow: monitor/client sizes must be physical pixels, not DWM's
        // virtualized 96-DPI coordinates on a high-density screen.
        using SetAwareness = BOOL(WINAPI*)(HANDLE);
        const auto lpSetAwareness = reinterpret_cast<SetAwareness>(
            GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext"));
        if (lpSetAwareness)
            lpSetAwareness(reinterpret_cast<HANDLE>(-4)); // PER_MONITOR_AWARE_V2
        else
            SetProcessDPIAware();
    }

    inline RECT FitDisplay16By9(LONG liWidth, LONG liHeight)
    {
        if (liWidth <= 0 || liHeight <= 0)
            return RECT{0, 0, 0, 0};
        LONG liViewWidth = liWidth;
        LONG liViewHeight = static_cast<LONG>(static_cast<LONGLONG>(liWidth) * 9 / 16);
        if (liViewHeight > liHeight)
        {
            liViewHeight = liHeight;
            liViewWidth = static_cast<LONG>(static_cast<LONGLONG>(liHeight) * 16 / 9);
        }
        const LONG liLeft = (liWidth - liViewWidth) / 2;
        const LONG liTop = (liHeight - liViewHeight) / 2;
        return RECT{liLeft, liTop, liLeft + liViewWidth, liTop + liViewHeight};
    }

    class PCWindowMode
    {
        WINDOWPLACEMENT mWindowedPlacement = {sizeof(WINDOWPLACEMENT)};
        LONG_PTR miWindowedStyle = 0;

    public:
        static bool FitMonitor(HWND lhWindow)
        {
            MONITORINFO lMonitor = {sizeof(MONITORINFO)};
            if (!GetMonitorInfo(MonitorFromWindow(lhWindow, MONITOR_DEFAULTTONEAREST), &lMonitor))
                return false;
            const RECT& lrMonitor = lMonitor.rcMonitor;
            return SetWindowPos(lhWindow, HWND_TOP, lrMonitor.left, lrMonitor.top,
                                lrMonitor.right - lrMonitor.left, lrMonitor.bottom - lrMonitor.top,
                                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED) != FALSE;
        }

        bool Toggle(HWND lhWindow, bool& lbFullscreen)
        {
            if (!lbFullscreen)
            {
                if (!GetWindowPlacement(lhWindow, &mWindowedPlacement))
                    return false;
                miWindowedStyle = GetWindowLongPtr(lhWindow, GWL_STYLE);
                SetLastError(0);
                const LONG_PTR liOldStyle = SetWindowLongPtr(lhWindow, GWL_STYLE,
                    (miWindowedStyle & ~(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE | WS_MINIMIZE)) | WS_POPUP);
                if (!liOldStyle && GetLastError() != 0)
                    return false;
                if (!FitMonitor(lhWindow))
                {
                    SetWindowLongPtr(lhWindow, GWL_STYLE, miWindowedStyle);
                    SetWindowPlacement(lhWindow, &mWindowedPlacement);
                    return false;
                }
            }
            else
            {
                SetLastError(0);
                const LONG_PTR liOldStyle = SetWindowLongPtr(lhWindow, GWL_STYLE, miWindowedStyle);
                if (!liOldStyle && GetLastError() != 0)
                    return false;
                if (!SetWindowPlacement(lhWindow, &mWindowedPlacement) ||
                    !SetWindowPos(lhWindow, nullptr, 0, 0, 0, 0,
                                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
                {
                    SetWindowLongPtr(lhWindow, GWL_STYLE, liOldStyle);
                    FitMonitor(lhWindow);
                    return false;
                }
            }
            lbFullscreen = !lbFullscreen;
            return true;
        }
    };

    // Preserve the completed COPY frame (assert overlays depend on it). The
    // window-sized output surface adds black bars around the 16:9 render area;
    // matching source/destination dimensions present at one output pixel per
    // rendered pixel. A retained frame can also be scaled after allocation fails.
    class PCPresentation
    {
    private:
        IDirect3DSwapChain9* mpSwapChain = nullptr;
        LONG miWidth = 0;
        LONG miHeight = 0;
        LONG miFailedWidth = 0;
        LONG miFailedHeight = 0;
        D3DTEXTUREFILTERTYPE meFilter = D3DTEXF_NONE;

    public:
        PCPresentation() = default;
        PCPresentation(const PCPresentation&) = delete;
        PCPresentation& operator=(const PCPresentation&) = delete;
        ~PCPresentation() { Release(); }

        void Release()
        {
            if (mpSwapChain) mpSwapChain->Release();
            mpSwapChain = nullptr;
            miWidth = miHeight = 0;
            miFailedWidth = miFailedHeight = 0;
        }

        HRESULT Prepare(IDirect3DDevice9* lpDevice, HWND lhWindow, bool lbVSync,
                        LONG liWidth, LONG liHeight)
        {
            if (liWidth <= 0 || liHeight <= 0) return D3DERR_INVALIDCALL;
            if (liWidth != miFailedWidth || liHeight != miFailedHeight)
                miFailedWidth = miFailedHeight = 0;
            if (mpSwapChain && miWidth == liWidth && miHeight == liHeight) return S_OK;
            if (liWidth == miFailedWidth && liHeight == miFailedHeight) return E_OUTOFMEMORY;
            D3DPRESENT_PARAMETERS lParameters = {};
            lParameters.Windowed = TRUE;
            lParameters.SwapEffect = D3DSWAPEFFECT_COPY;
            lParameters.BackBufferFormat = D3DFMT_X8R8G8B8;
            lParameters.BackBufferWidth = liWidth;
            lParameters.BackBufferHeight = liHeight;
            lParameters.BackBufferCount = 1;
            lParameters.hDeviceWindow = lhWindow;
            lParameters.PresentationInterval = lbVSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
            IDirect3DSwapChain9* lpPending = nullptr;
            const HRESULT lResult = lpDevice->CreateAdditionalSwapChain(&lParameters, &lpPending);
            if (FAILED(lResult))
            {
                miFailedWidth = liWidth;
                miFailedHeight = liHeight;
                return lResult;
            }
            Release();
            mpSwapChain = lpPending;
            miWidth = liWidth;
            miHeight = liHeight;
            D3DCAPS9 lCaps = {};
            lpDevice->GetDeviceCaps(&lCaps);
            const DWORD luLinearCaps = D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MAGFLINEAR;
            meFilter = (lCaps.StretchRectFilterCaps & luLinearCaps) == luLinearCaps ? D3DTEXF_LINEAR : D3DTEXF_NONE;
            return S_OK;
        }

        // Call after EndScene. No device Reset or render-state/viewport changes.
        HRESULT Present(IDirect3DDevice9* lpDevice, HWND lhWindow, bool lbVSync,
                        IDirect3DSurface9* lpFrame = nullptr)
        {
            RECT lClient;
            if (!GetClientRect(lhWindow, &lClient)) return E_FAIL;
            if (IsIconic(lhWindow) || lClient.right <= 0 || lClient.bottom <= 0) return S_OK;
            const RECT lView = FitDisplay16By9(lClient.right, lClient.bottom);
            if (lView.right <= lView.left || lView.bottom <= lView.top) return S_OK;
            if (!lpFrame && EqualRect(&lView, &lClient))
            {
                Release();
                return lpDevice->Present(nullptr, nullptr, lhWindow, nullptr);
            }

            const HRESULT lPrepare = Prepare(lpDevice, lhWindow, lbVSync, lClient.right, lClient.bottom);
            if (FAILED(lPrepare) && !mpSwapChain) return lPrepare;
            // If output allocation failed, retain the old output and map the
            // client-space bars into it. Present scales the whole result back
            // to the client, so the game still has the correct aspect ratio.
            const RECT lOutputView = {
                static_cast<LONG>(static_cast<LONGLONG>(lView.left) * miWidth / lClient.right),
                static_cast<LONG>(static_cast<LONGLONG>(lView.top) * miHeight / lClient.bottom),
                static_cast<LONG>(static_cast<LONGLONG>(lView.right) * miWidth / lClient.right),
                static_cast<LONG>(static_cast<LONGLONG>(lView.bottom) * miHeight / lClient.bottom)};

            IDirect3DSurface9* lpSource = nullptr;
            IDirect3DSurface9* lpOutput = nullptr;
            HRESULT lResult = S_OK;
            if (lpFrame)
            {
                lpSource = lpFrame;
                lpSource->AddRef();
            }
            else
                lResult = lpDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &lpSource);
            if (SUCCEEDED(lResult)) lResult = mpSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &lpOutput);
            if (SUCCEEDED(lResult)) lResult = lpDevice->ColorFill(lpOutput, nullptr, D3DCOLOR_XRGB(0, 0, 0));
            if (SUCCEEDED(lResult)) lResult = lpDevice->StretchRect(lpSource, nullptr, lpOutput, &lOutputView, meFilter);
            if (SUCCEEDED(lResult)) lResult = mpSwapChain->Present(nullptr, nullptr, lhWindow, nullptr, 0);
            if (lpOutput) lpOutput->Release();
            if (lpSource) lpSource->Release();
            return lResult;
        }
    };
}
