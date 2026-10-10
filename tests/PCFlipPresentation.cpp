#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <vector>
#include <thread>
#include <cstdio>
template<class T> HRESULT InspectAndPresent(T& flip);
static HRESULT InspectAndPresentLegacy(IDirect3DSwapChain9* chain, HWND window);
#include "pc/gcm/renderengine/WindowPresentation.h"
#include "pc/gcm/renderengine/DisplayResize.h"
static int checks,failures;
static void Check(bool good,const char* label){++checks;if(!good){++failures;std::printf("FAIL %s\n",label);}}
struct ResetDevice
{
    ULONG references=1;unsigned resets=0,presents=0,buffers=0;
    std::vector<HRESULT> outcomes;
    std::vector<D3DPRESENT_PARAMETERS> requests;
    HRESULT presentResult=S_OK;
    ULONG AddRef(){return ++references;} ULONG Release(){return --references;}
    HRESULT ResetEx(D3DPRESENT_PARAMETERS* p,D3DDISPLAYMODEEX*)
    {
        ++resets;requests.push_back(*p);
        HRESULT result=S_OK;if(!outcomes.empty()){result=outcomes.front();outcomes.erase(outcomes.begin());}
        p->BackBufferWidth=p->BackBufferHeight=p->BackBufferCount=0;
        return result;
    }
    HRESULT PresentEx(const RECT*,const RECT*,HWND,const RGNDATA*,DWORD){++presents;return presentResult;}
    HRESULT GetBackBuffer(UINT,UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface9**){++buffers;return S_OK;}
};
static void Protocol()
{
    ResetDevice device;renderengine::BasicFlipSwapChainPC<ResetDevice> flip;
    D3DPRESENT_PARAMETERS p{};p.BackBufferWidth=320;p.BackBufferHeight=180;p.BackBufferCount=2;
    p.Windowed=TRUE;p.SwapEffect=D3DSWAPEFFECT_FLIPEX;p.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    p.hDeviceWindow=reinterpret_cast<HWND>(uintptr_t(1));flip.Configure(&device,p);
    Check(SUCCEEDED(flip.Prepare(p.hDeviceWindow,false,320,180))&&!device.resets,"unchanged primary output does not reset");
    Check(SUCCEEDED(flip.Prepare(p.hDeviceWindow,false,640,360))&&flip.Width()==640&&flip.Height()==360,
        "successful reset retains requested dimensions after ResetEx clears its parameter block");
    device.outcomes={E_OUTOFMEMORY,S_OK};
    Check(FAILED(flip.Prepare(p.hDeviceWindow,false,800,600))&&flip.Ready()&&flip.Width()==640&&device.requests.back().BackBufferWidth==640,
        "failed resize restores the previous output before rendering resumes");
    const unsigned cached=device.resets;flip.Prepare(p.hDeviceWindow,false,800,600);
    Check(device.resets==cached,"a failed size is not retried on every healthy frame");
    device.outcomes={D3DERR_DEVICELOST,D3DERR_DEVICELOST};flip.Prepare(p.hDeviceWindow,false,900,600);
    const unsigned calls=device.presents+device.buffers;
    Check(!flip.Ready()&&FAILED(flip.Present())&&FAILED(flip.GetBackBuffer(nullptr))&&device.presents+device.buffers==calls,
        "unrecovered device loss prevents all output access");
    Check(SUCCEEDED(flip.Prepare(p.hDeviceWindow,false,900,600))&&flip.Ready(),"a later successful reset recovers the frame gate");
    const unsigned before=device.resets;HRESULT workerResult=S_OK;
    std::thread worker([&]{workerResult=flip.Prepare(p.hDeviceWindow,false,1000,700);});worker.join();
    Check(workerResult==E_PENDING&&device.resets==before,"render-thread resize never calls ResetEx");
    Check(flip.Prepare(p.hDeviceWindow,false,1000,700,false)==E_PENDING&&device.resets==before,
        "assert/modal presentation cannot reset outside the joined frame boundary");
    device.presentResult=S_PRESENT_MODE_CHANGED;flip.Present();
    Check(!flip.Ready()&&SUCCEEDED(flip.Prepare(p.hDeviceWindow,false,900,600)),"display-mode changes request same-size recovery");
    flip.Release();Check(device.references==1,"presentation owns and releases exactly one device reference");
}
static HWND testWindow=nullptr;
static DWORD colour=0xFF4997D1;
static bool captureScaled=false;
static std::vector<DWORD> capturedPixels;
static void Capture(IDirect3DSurface9* surface)
{
    capturedPixels.clear();if(!surface)return;
    IDirect3DDevice9* device=nullptr;surface->GetDevice(&device);
    D3DSURFACE_DESC desc{};surface->GetDesc(&desc);IDirect3DSurface9* staging=nullptr;
    HRESULT hr=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&staging,nullptr);
    if(SUCCEEDED(hr))hr=device->GetRenderTargetData(surface,staging);
    D3DLOCKED_RECT lock{};
    if(SUCCEEDED(hr)&&SUCCEEDED(staging->LockRect(&lock,nullptr,D3DLOCK_READONLY))){
        for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x)
            capturedPixels.push_back(reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]&0xFFFFFF);
        staging->UnlockRect();}
    if(staging)staging->Release();device->Release();
}
static bool SurfaceMatches(IDirect3DSurface9* surface,const RECT& view,DWORD expected)
{
    if(!surface)return false;
    IDirect3DDevice9* device=nullptr;surface->GetDevice(&device);
    D3DSURFACE_DESC desc{};surface->GetDesc(&desc);IDirect3DSurface9* staging=nullptr;
    HRESULT hr=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&staging,nullptr);
    if(SUCCEEDED(hr))hr=device->GetRenderTargetData(surface,staging);
    D3DLOCKED_RECT lock{};bool good=SUCCEEDED(hr)&&SUCCEEDED(staging->LockRect(&lock,nullptr,D3DLOCK_READONLY));
    if(good){for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x){
        bool inside=LONG(x)>=view.left&&LONG(x)<view.right&&LONG(y)>=view.top&&LONG(y)<view.bottom;
        good&=(reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]&0xFFFFFF)==(inside?(expected&0xFFFFFF):0);}
        staging->UnlockRect();}
    if(staging)staging->Release();device->Release();return good;
}
template<class T> HRESULT InspectAndPresent(T& flip)
{
    IDirect3DSurface9* output=nullptr;HRESULT hr=flip.GetBackBuffer(&output);
    RECT client{};GetClientRect(testWindow,&client);const RECT fitted=renderengine::FitDisplay16By9(client.right,client.bottom);
    RECT view{LONG(static_cast<LONGLONG>(fitted.left)*flip.Width()/client.right),LONG(static_cast<LONGLONG>(fitted.top)*flip.Height()/client.bottom),
        LONG(static_cast<LONGLONG>(fitted.right)*flip.Width()/client.right),LONG(static_cast<LONGLONG>(fitted.bottom)*flip.Height()/client.bottom)};
    if(captureScaled)Capture(output);
    else Check(SUCCEEDED(hr)&&SurfaceMatches(output,view,colour),"actual flip buffer has the complete image and exact black bars before presentation");
    if(output)output->Release();return flip.Present();
}
static HRESULT InspectAndPresentLegacy(IDirect3DSwapChain9* chain,HWND window)
{
    IDirect3DSurface9* output=nullptr;chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&output);
    Capture(output);if(output)output->Release();return chain->Present(nullptr,nullptr,window,nullptr,0);
}
static std::vector<DWORD> ScaledOutput(bool extended)
{
    testWindow=CreateWindowA("STATIC","Scaled presentation checks",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    RECT outer{0,0,321,181};AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE);
    SetWindowPos(testWindow,HWND_TOP,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOACTIVATE);
    ShowWindow(testWindow,SW_SHOWNOACTIVATE);
    IDirect3D9* api=nullptr;IDirect3D9Ex* exApi=nullptr;
    IDirect3DDevice9* device=nullptr;IDirect3DDevice9Ex* exDevice=nullptr;
    D3DPRESENT_PARAMETERS p{};p.Windowed=TRUE;p.SwapEffect=extended?D3DSWAPEFFECT_FLIPEX:D3DSWAPEFFECT_COPY;
    p.BackBufferCount=extended?2:1;p.BackBufferFormat=D3DFMT_X8R8G8B8;p.BackBufferWidth=321;p.BackBufferHeight=181;
    p.hDeviceWindow=testWindow;p.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    const D3DPRESENT_PARAMETERS original=p;HRESULT hr;
    if(extended){hr=Direct3DCreate9Ex(D3D_SDK_VERSION,&exApi);api=exApi;
        if(SUCCEEDED(hr))hr=exApi->CreateDeviceEx(0,D3DDEVTYPE_HAL,testWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&p,nullptr,&exDevice);device=exDevice;}
    else{api=Direct3DCreate9(D3D_SDK_VERSION);hr=api?api->CreateDevice(0,D3DDEVTYPE_HAL,testWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&p,&device):E_FAIL;}
    Check(SUCCEEDED(hr)&&device,"native device for scaled-source comparison created");
    if(!device){if(api)api->Release();DestroyWindow(testWindow);return {};}
    capturedPixels.clear();captureScaled=true;
    {
        renderengine::PCPresentation presenter;if(extended)presenter.ConfigureFlip(exDevice,original);
        IDirect3DSurface9* source=nullptr;
        hr=device->CreateRenderTarget(4,4,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&source,nullptr);
        if(SUCCEEDED(hr)){
            for(LONG y=0;y<4;++y)for(LONG x=0;x<4;++x){RECT cell{x,y,x+1,y+1};device->ColorFill(source,&cell,(x+y)%2?0xFFFFFFFF:0xFF000000);}
            hr=presenter.Present(device,testWindow,false,source);source->Release();}
        Check(SUCCEEDED(hr)&&presenter.LastPresentSucceeded()&&capturedPixels.size()==321*181,
            "scaled-source comparison captures an actually presented complete output");
    }
    captureScaled=false;device->Release();api->Release();DestroyWindow(testWindow);
    return capturedPixels;
}
static void Native()
{
    renderengine::EnablePerMonitorDpi();
    testWindow=CreateWindowA("STATIC","Flip presentation checks",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    ShowWindow(testWindow,SW_SHOWNOACTIVATE);
    IDirect3D9Ex* api=nullptr;IDirect3DDevice9Ex* device=nullptr;
    D3DPRESENT_PARAMETERS p{};RECT client{};GetClientRect(testWindow,&client);
    p.Windowed=TRUE;p.SwapEffect=D3DSWAPEFFECT_FLIPEX;p.BackBufferCount=2;p.BackBufferFormat=D3DFMT_X8R8G8B8;
    p.BackBufferWidth=client.right;p.BackBufferHeight=client.bottom;p.hDeviceWindow=testWindow;p.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    const D3DPRESENT_PARAMETERS original=p;
    HRESULT hr=Direct3DCreate9Ex(D3D_SDK_VERSION,&api);
    if(SUCCEEDED(hr))hr=api->CreateDeviceEx(0,D3DDEVTYPE_HAL,testWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&p,nullptr,&device);
    Check(SUCCEEDED(hr)&&device,"native primary FLIPEX device created");if(!device)return;
    {
        renderengine::PCPresentation presenter;presenter.ConfigureFlip(device,original);
        D3DCAPS9 caps{};device->GetDeviceCaps(&caps);
        const DWORD linearCaps=D3DPTFILTERCAPS_MINFLINEAR|D3DPTFILTERCAPS_MAGFLINEAR;
        Check(presenter.meFilter==((caps.StretchRectFilterCaps&linearCaps)==linearCaps?D3DTEXF_LINEAR:D3DTEXF_NONE),
            "flip scaling explicitly selects the same capability-based filter as legacy");
        renderengine::PCFrameBuffer source;
        Check(source.Resize(device,testWindow,320,180,false)&&device->GetNumberOfSwapChains()==1,
            "retained scene frame is an offscreen surface, never an additional flip chain");
        source.Bind(device);IDirect3DSurface9* retained=nullptr;source.GetBackBuffer(device,&retained);
        const int sizes[][2]={{400,300},{801,351},{320,180}};
        for(const auto& size:sizes){RECT outer{0,0,size[0],size[1]};AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE);
            SetWindowPos(testWindow,HWND_TOP,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOACTIVATE);
            GetClientRect(testWindow,&client);
            Check(SUCCEEDED(presenter.Prepare(device,testWindow,false,client.right,client.bottom,true)),"joined resize configures the primary output");
            colour^=0x00FF5500;device->ColorFill(retained,nullptr,colour);
            Check(SUCCEEDED(presenter.Present(device,testWindow,false,retained)),"native flip presentation succeeds after aspect changes");
            RECT all{0,0,320,180};Check(SurfaceMatches(retained,all,colour),"flip rotation preserves the frame used by assert overlays");
        }
        renderengine::PCWindowMode mode;bool fullscreen=false;
        for(unsigned i=0;i<2;++i){const bool toggled=mode.Toggle(testWindow,fullscreen);GetClientRect(testWindow,&client);
            Check(toggled&&SUCCEEDED(presenter.Prepare(device,testWindow,false,client.right,client.bottom,true)),"fullscreen toggle preserves flip output lifetime");
            Check(SUCCEEDED(presenter.Present(device,testWindow,false,retained)),"fullscreen/windowed flip presents retained content");}
        Check(FAILED(presenter.Present(device,testWindow,false))&&!presenter.LastPresentSucceeded(),
            "a missing retained source never counts as a successful presentation");
        ShowWindow(testWindow,SW_MINIMIZE);
        Check(SUCCEEDED(presenter.Present(device,testWindow,false,retained))&&!presenter.LastPresentSucceeded(),
            "a minimized frame does not increment the successful presentation count");
        ShowWindow(testWindow,SW_SHOWNOACTIVATE);
        retained->Release();
    }
    device->Release();api->Release();DestroyWindow(testWindow);
}
int main(){Protocol();Native();
    const auto legacy=ScaledOutput(false),flip=ScaledOutput(true);
    Check(!legacy.empty()&&legacy==flip,"nonuniform retained sources use the same scaled pixels in legacy and flip outputs");
    bool interpolated=false;for(DWORD pixel:legacy)interpolated|=pixel!=0&&pixel!=0xFFFFFF;
    Check(interpolated,"scaled checkerboard contains interpolated pixels");
    std::printf("PCFlipPresentation: %d checks, %d failures\n",checks,failures);return failures?1:0;}
