#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include "pc/gcm/renderengine/device.h"
#include "pc/gcm/renderengine/DisplayResizePCLeaf.h"
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"
#include "pc/gcm/renderengine/WindowPresentationPCLeaf.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"
#include "GameSource/Graphics/BrnRendererMemory.h"

// Only the Cgs wrapper's accessors are needed to exercise the production
// BrnRendererMemory resize body, including a failed inner-target initialization.
class CgsRenderTarget {
public:
    rw::graphics::postfx::RenderTarget* mpTarget = nullptr;
    rw::graphics::postfx::RenderTarget* GetRenderTarget() { return mpTarget; }
    void SetDimensions(u32,u32) {}
};

namespace renderengine {
    IDirect3DDevice9* gDevice = nullptr;
    HWND hWnd = nullptr;
    s32 gDisplayWidth = 640, gDisplayHeight = 360, gVSync = 0;
}
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
void shadow::Device::ResetShadowing() {}
renderengine::PCFrameBuffer gFrameBuffer;
renderengine::PCPresentation gPresentation;
#include "pc_display_resize.inc"

void renderengine::Device::SetState(const RenderTargetState* state) {
    gDevice->SetDepthStencilSurface(nullptr);
    gDevice->SetRenderTarget(0,state->mpColourSurface);
    gDevice->SetDepthStencilSurface(state->mpDepthSurface);
}
void renderengine::PCBringUpClearRenderTargetState(const RenderTargetState* state) {
    Device::SetState(state);
    gDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0);
}
static int checks, failures;
static void Check(bool good,const char* message) {
    ++checks; if (!good) { ++failures; std::printf("FAIL %s\n",message); }
}
struct TargetFixture {
    rw::graphics::postfx::RenderTarget target = {};
    renderengine::RenderTargetState state = {};
    renderengine::Texture colour = {}, depth = {};
    bool Create(UINT w,UINT h,bool msaa) {
        auto device = renderengine::gDevice;
        target.muWidth = state.muWidth = w; target.muHeight = state.muHeight = h;
        target.mapSectionState[0] = target.mpSection0State = &state;
        if (msaa) {
            if (FAILED(device->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_2_SAMPLES,0,FALSE,&state.mpColourSurface,nullptr))) return false;
            return SUCCEEDED(device->CreateDepthStencilSurface(w,h,D3DFMT_D24S8,D3DMULTISAMPLE_2_SAMPLES,0,FALSE,&state.mpDepthSurface,nullptr));
        }
        IDirect3DTexture9* c = nullptr; IDirect3DTexture9* d = nullptr;
        if (FAILED(device->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&c,nullptr))) return false;
        c->GetSurfaceLevel(0,&state.mpColourSurface);
        colour.mpD3DTexture = c; colour.muWidth = (u16)w; colour.muHeight = (u16)h;
        target.maColourTargets[0].mpTexture = &colour;
        if (FAILED(device->CreateTexture(w,h,1,D3DUSAGE_DEPTHSTENCIL,(D3DFORMAT)MAKEFOURCC('I','N','T','Z'),D3DPOOL_DEFAULT,&d,nullptr))) return false;
        d->GetSurfaceLevel(0,&state.mpDepthSurface);
        depth.mpD3DTexture = d; depth.muWidth = (u16)w; depth.muHeight = (u16)h;
        target.mDepthTarget.mpTexture = &depth;
        return true;
    }
    ~TargetFixture() {
        if (state.mpColourSurface) state.mpColourSurface->Release();
        if (state.mpDepthSurface) state.mpDepthSurface->Release();
        if (colour.mpD3DTexture) colour.mpD3DTexture->Release();
        if (depth.mpD3DTexture) depth.mpD3DTexture->Release();
    }
};
static bool BottomRightIs(IDirect3DSurface9* surface,DWORD expected) {
    D3DSURFACE_DESC desc = {}; surface->GetDesc(&desc);
    IDirect3DSurface9* readback = nullptr;
    auto device = renderengine::gDevice;
    if (FAILED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&readback,nullptr))) return false;
    D3DLOCKED_RECT lock = {}; bool okay = SUCCEEDED(device->GetRenderTargetData(surface,readback)) && SUCCEEDED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));
    if (okay) {
        const DWORD* row = reinterpret_cast<const DWORD*>(static_cast<const char*>(lock.pBits)+(desc.Height-1)*lock.Pitch);
        okay = row[desc.Width-1] == expected;
        readback->UnlockRect();
    }
    readback->Release(); return okay;
}
int main() {
    renderengine::EnablePerMonitorDpi();
    renderengine::hWnd = CreateWindowA("STATIC","Resize pool regression",WS_OVERLAPPEDWINDOW,0,0,640,360,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    auto d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_COPY; pp.BackBufferWidth=640; pp.BackBufferHeight=360;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8; pp.EnableAutoDepthStencil=TRUE; pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    pp.hDeviceWindow=renderengine::hWnd;
    if (!d3d || FAILED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&renderengine::gDevice))) return 2;
    renderengine::PCInstallDefaultRenderTargetState(640,360);
    {
        TargetFixture scene, down, particle;
        if (!scene.Create(640,360,true) || !down.Create(640,360,false) || !particle.Create(320,180,false)) return 3;
        gaDepthTargetRecords[0].mpTarget = &down.target;
        const UINT sizes[][2] = {{1920,1080},{2560,1440},{3840,2160},{1001,563},{640,360}};
        for (const auto& size : sizes) {
            const UINT w=size[0], h=size[1];
            Check(renderengine::PCResizeDisplayTargets(&scene.target,&down.target,&particle.target,w,h),"production pool transaction succeeds");
            Check(renderengine::gDisplayWidth==(s32)w && renderengine::gDisplayHeight==(s32)h && down.target.muWidth==w && particle.target.muHeight==h/2,"display, scene and particle metadata updated together");
            Check(down.target.maColourTargets[0].mpTexture==&down.colour && down.target.mDepthTarget.mpTexture==&down.depth && down.target.mapSectionState[0]==&down.state,"wrapper and state identities survive resize");
            Check(down.colour.muWidth==w && down.depth.muHeight==h && gaDepthTargetRecords[0].muWidth==w,"texture and depth registry dimensions updated");
            renderengine::Device::SetState(&scene.state);
            Check(SUCCEEDED(renderengine::gDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,0.6f,0)),"resized MSAA colour and depth can be cleared together");
            Check(SUCCEEDED(renderengine::gDevice->StretchRect(scene.state.mpColourSurface,nullptr,down.state.mpColourSurface,nullptr,D3DTEXF_NONE)) && BottomRightIs(down.state.mpColourSurface,0xff123456),"MSAA resolve writes bottom-right pixel beyond 720p");
        }
        auto oldColour = down.colour.mpD3DTexture;
        auto oldDepth = scene.state.mpDepthSurface;
        Check(!renderengine::PCResizeDisplayTargets(&scene.target,&down.target,&particle.target,0,0),"invalid resize fails");
        Check(oldColour==down.colour.mpD3DTexture && oldDepth==scene.state.mpDepthSurface && renderengine::gDisplayWidth==640,"failed transaction keeps all old GPU handles and dimensions");
        D3DCAPS9 caps = {}; renderengine::gDevice->GetDeviceCaps(&caps);
        Check(!renderengine::PCResizeDisplayTargets(&scene.target,&down.target,&particle.target,caps.MaxTextureWidth+1,360),"unsupported GPU extent fails");
        Check(oldColour==down.colour.mpD3DTexture && renderengine::gDisplayWidth==640,"allocation failure publishes no partial resize");
        RECT outer = {0,0,800,450}; AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE);
        SetWindowPos(renderengine::hWnd,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOZORDER|SWP_NOACTIVATE);
        gPresentation.miFailedWidth=800; gPresentation.miFailedHeight=450;
        Check(!renderengine::PCResizeDisplayTargets(&scene.target,&down.target,&particle.target,800,450)
              && oldColour==down.colour.mpD3DTexture && renderengine::gDisplayWidth==640,
              "output allocation failure occurs before publishing new scene targets");
        gPresentation.miFailedWidth=gPresentation.miFailedHeight=0;
        BrnRendererMemory pool = {};
        CgsRenderTarget incomplete;
        pool.mapRenderTarget[BrnRendererMemory::E_RENDER_TARGET_PARTICLE] = &incomplete;
        pool.PCResizeDisplay();
        Check(renderengine::gDisplayWidth==640,"incomplete Cgs target prevents resize without a null dereference");
        pool.mapRenderTarget[BrnRendererMemory::E_RENDER_TARGET_PARTICLE] = nullptr;
        pool.PCResizeDisplay();
        Check(renderengine::gDisplayWidth==800 && renderengine::gDisplayHeight==450,"pre-pool frame can resize while targets are absent");
    }
    gFrameBuffer.Release();
    gPresentation.Release();
    renderengine::gDevice->Release(); d3d->Release(); DestroyWindow(renderengine::hWnd);
    std::printf("PCDisplayResize: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
