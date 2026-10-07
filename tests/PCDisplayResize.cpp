#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <initializer_list>
#include "pc/gcm/renderengine/device.h"
#include "pc/gcm/renderengine/DisplayResizePCLeaf.h"
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"
#include "pc/gcm/renderengine/WindowPresentationPCLeaf.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"
#include "GameSource/Graphics/BrnRendererMemory.h"

// Cgs allocation/format setters are fixture boundaries. The extracted real
// creators determine dimensions; the real native transaction realizes surfaces.
class CgsRenderTarget {
public:
    rw::graphics::postfx::RenderTarget* mpTarget = nullptr;
    u32 muWidth = 0, muHeight = 0;
    bool mbConstructed = false;
    rw::graphics::postfx::RenderTarget* GetRenderTarget() { return mpTarget; }
    void SetDimensions(u32 w,u32 h) { muWidth=w; muHeight=h; }
    void ClearColourTargetInUse() {}
    void SetNumMipMaps(u32) {}
    void SetMultisampleFormat(u32) {}
    void SetUseDepthStencilAsTexture(bool) {}
    void SetColourTargetInUse(u32,bool) {}
    void SetColourTargetBufferFormat(u32,u32) {}
    void SetColourTargetTextureFormat(u32,u32) {}
    void SetColourTargetBaseEDRAM(u32,u32) {}
    void SetDepthTargetInUse(bool) {}
    void Construct(rw::IResourceAllocator*) { mbConstructed=true; }
};
static const u32 KU_WORK_BUFFER_WIDTH=320, KU_WORK_BUFFER_HEIGHT=180;
static const u32 KU_BLOOM_BUFFER_WIDTH=320, KU_BLOOM_BUFFER_HEIGHT=180;
static const u32 KU_DEPTH_OF_FIELD_BUFFER_WIDTH=320, KU_DEPTH_OF_FIELD_BUFFER_HEIGHT=180;
static const u32 KU_COLOUR_SURFACE_BUFFER_FORMAT=0x18280186;

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
    gDevice->Clear(0,nullptr,D3DCLEAR_TARGET|(state->mpDepthSurface?D3DCLEAR_ZBUFFER:0),0,1,0);
}
static int checks, failures;
static void Check(bool good,const char* message) {
    ++checks; if (!good) { ++failures; std::printf("FAIL %s\n",message); }
}
struct TargetFixture {
    rw::graphics::postfx::RenderTarget target = {};
    renderengine::RenderTargetState state = {};
    renderengine::Texture colour = {}, depth = {};
    bool Create(UINT w,UINT h,bool msaa,bool wantDepth=true) {
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
        if (!wantDepth) return true;
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
struct TargetSnapshot {
    TargetFixture* fixture;
    IDirect3DSurface9* colourSurface;
    IDirect3DSurface9* depthSurface;
    IDirect3DBaseTexture9* colourTexture;
    IDirect3DBaseTexture9* depthTexture;
    UINT width, height;
    TargetSnapshot(TargetFixture& value)
        : fixture(&value), colourSurface(value.state.mpColourSurface), depthSurface(value.state.mpDepthSurface),
          colourTexture(value.colour.mpD3DTexture), depthTexture(value.depth.mpD3DTexture),
          width(value.target.muWidth), height(value.target.muHeight) {}
    bool Unchanged() const {
        return fixture->state.mpColourSurface==colourSurface && fixture->state.mpDepthSurface==depthSurface &&
               fixture->colour.mpD3DTexture==colourTexture && fixture->depth.mpD3DTexture==depthTexture &&
               fixture->target.muWidth==width && fixture->target.muHeight==height &&
               fixture->state.muWidth==width && fixture->state.muHeight==height;
    }
};
static bool ResizePool(TargetFixture& scene,TargetFixture& down,TargetFixture& particle,
                       TargetFixture& bloom,TargetFixture& dof,TargetFixture& work,UINT w,UINT h) {
#if PC_QUARTER_TARGETS
    return renderengine::PCResizeDisplayTargets(&scene.target,&down.target,&particle.target,w,h,
                                                &bloom.target,&dof.target,&work.target);
#else
    return renderengine::PCResizeDisplayTargets(&scene.target,&down.target,&particle.target,w,h);
#endif
}
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
        BrnRendererMemory creation = {};
        const UINT creationSizes[][4]={{1280,720,320,180},{2560,1440,640,360},
                                      {3840,2160,960,540},{1001,563,250,140}};
        for (const auto& size : creationSizes) {
            creation.mu32ScreenWidth=size[0]; creation.mu32ScreenHeight=size[1];
            creation.CreateBloomBuffer(nullptr); creation.CreateDepthOfFieldBuffer(nullptr);
            creation.CreateWorkBuffer(nullptr);
            for (auto* buffer : {creation.GetBloomBuffer(),creation.GetDepthOfFieldBuffer(),creation.GetWorkBuffer()}) {
                Check(buffer && buffer->mbConstructed && buffer->muWidth==size[2] && buffer->muHeight==size[3],
                      "real creator retains original 720p size and native PC quarter dimensions");
                delete buffer;
            }
        }
        TargetFixture scene, down, particle, bloom, dof, work;
        if (!scene.Create(640,360,true) || !down.Create(640,360,false) || !particle.Create(320,180,false)
            || !bloom.Create(320,180,false,false) || !dof.Create(320,180,false,false)
            || !work.Create(320,180,false,false)) return 3;
        gaDepthTargetRecords[0].mpTarget = &down.target;
        TargetFixture* quarters[]={&bloom,&dof,&work};
        const UINT sizes[][2] = {{1280,720},{1920,1080},{2560,1440},{3840,2160},{1001,563},{640,360}};
        for (const auto& size : sizes) {
            const UINT w=size[0], h=size[1];
            Check(ResizePool(scene,down,particle,bloom,dof,work,w,h),"production pool transaction succeeds");
            Check(renderengine::gDisplayWidth==(s32)w && renderengine::gDisplayHeight==(s32)h && down.target.muWidth==w && particle.target.muHeight==h/2,"display, scene and particle metadata updated together");
            Check(down.target.maColourTargets[0].mpTexture==&down.colour && down.target.mDepthTarget.mpTexture==&down.depth && down.target.mapSectionState[0]==&down.state,"wrapper and state identities survive resize");
            Check(down.colour.muWidth==w && down.depth.muHeight==h && gaDepthTargetRecords[0].muWidth==w,"texture and depth registry dimensions updated");
            renderengine::Device::SetState(&scene.state);
            Check(SUCCEEDED(renderengine::gDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,0.6f,0)),"resized MSAA colour and depth can be cleared together");
            Check(SUCCEEDED(renderengine::gDevice->StretchRect(scene.state.mpColourSurface,nullptr,down.state.mpColourSurface,nullptr,D3DTEXF_NONE)) && BottomRightIs(down.state.mpColourSurface,0xff123456),"MSAA resolve writes bottom-right pixel beyond 720p");
            for (auto* quarter : quarters) {
                D3DSURFACE_DESC desc={}; quarter->state.mpColourSurface->GetDesc(&desc);
                Check(quarter->target.muWidth==w/4 && quarter->target.muHeight==h/4 &&
                      quarter->state.muWidth==w/4 && quarter->state.muHeight==h/4 &&
                      quarter->colour.muWidth==w/4 && quarter->colour.muHeight==h/4 &&
                      desc.Width==w/4 && desc.Height==h/4,"quarter target, texture, state and native surface agree");
                Check(!quarter->state.mpDepthSurface && !quarter->target.mDepthTarget.mpTexture &&
                      quarter->target.maColourTargets[0].mpTexture==&quarter->colour &&
                      quarter->target.mapSectionState[0]==&quarter->state,"colour-only quarter target retains original identities");
                renderengine::Device::SetState(&quarter->state);
                Check(SUCCEEDED(renderengine::gDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff418dc7,1,0)) &&
                      BottomRightIs(quarter->state.mpColourSurface,0xff418dc7),"quarter surface renders through its new bottom-right pixel");
            }
        }
        TargetSnapshot before[]={scene,down,particle,bloom,dof,work};
        auto unchanged=[&]() {
            for (const auto& value : before) if (!value.Unchanged()) return false;
            return renderengine::gDisplayWidth==640 && renderengine::gDisplayHeight==360;
        };
        // Restore the control after a deliberately failing assertion, so one old
        // transaction publishing prematurely cannot cause unrelated later failures.
        auto recover=[&]() {
            if (renderengine::gDisplayWidth!=640 || renderengine::gDisplayHeight!=360)
                ResizePool(scene,down,particle,bloom,dof,work,640,360);
            for (auto& value : before) value=TargetSnapshot(*value.fixture);
        };
        Check(!ResizePool(scene,down,particle,bloom,dof,work,0,0),"invalid resize fails");
        Check(unchanged(),"failed transaction keeps all old GPU handles and dimensions");
        D3DCAPS9 caps = {}; renderengine::gDevice->GetDeviceCaps(&caps);
        Check(!ResizePool(scene,down,particle,bloom,dof,work,caps.MaxTextureWidth+1,360),"unsupported GPU extent fails");
        Check(unchanged(),"scene allocation failure preserves all six targets and dimensions");
        Check(!ResizePool(scene,down,particle,bloom,dof,work,2,2),
              "zero quarter extent fails after native scene and particle allocations");
        Check(unchanged(),"late quarter allocation failure preserves all six targets and dimensions");
        recover();
        RECT outer = {0,0,800,450}; AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE);
        SetWindowPos(renderengine::hWnd,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOZORDER|SWP_NOACTIVATE);
        gPresentation.miFailedWidth=800; gPresentation.miFailedHeight=450;
        Check(!ResizePool(scene,down,particle,bloom,dof,work,800,450) && unchanged(),
              "output allocation failure occurs before publishing new scene targets");
        gPresentation.miFailedWidth=gPresentation.miFailedHeight=0;
        auto* workState=work.target.mapSectionState[0];
        work.target.mapSectionState[0]=work.target.mpSection0State=nullptr;
        Check(!ResizePool(scene,down,particle,bloom,dof,work,800,450) && unchanged(),
              "missing final work state prevents partial publication of all prepared targets");
        work.target.mapSectionState[0]=work.target.mpSection0State=workState;
        recover();
        BrnRendererMemory pool = {};
        CgsRenderTarget incomplete;
        pool.mapRenderTarget[BrnRendererMemory::E_RENDER_TARGET_PARTICLE] = &incomplete;
        pool.PCResizeDisplay();
        Check(renderengine::gDisplayWidth==640,"incomplete Cgs target prevents resize without a null dereference");
        pool.mapRenderTarget[BrnRendererMemory::E_RENDER_TARGET_PARTICLE] = nullptr;
        pool.mapRenderTarget[BrnRendererMemory::E_RENDER_TARGET_WORK] = &incomplete;
        pool.PCResizeDisplay();
        Check(renderengine::gDisplayWidth==640,"incomplete quarter target prevents Cgs metadata publication");
        pool.mapRenderTarget[BrnRendererMemory::E_RENDER_TARGET_WORK] = nullptr;
        recover();
        pool.PCResizeDisplay();
        Check(renderengine::gDisplayWidth==800 && renderengine::gDisplayHeight==450,"pre-pool frame can resize while targets are absent");
        TargetFixture* fixtures[]={&scene,&down,&particle,&bloom,&dof,&work};
        const BrnRendererMemory::ERenderTargetSlot slots[]={BrnRendererMemory::E_RENDER_TARGET_ANTI_ALIAS,
            BrnRendererMemory::E_RENDER_TARGET_DOWN_SAMPLE,BrnRendererMemory::E_RENDER_TARGET_PARTICLE,
            BrnRendererMemory::E_RENDER_TARGET_BLOOM,BrnRendererMemory::E_RENDER_TARGET_DEPTH_OF_FIELD,
            BrnRendererMemory::E_RENDER_TARGET_WORK};
        const UINT divisors[]={1,1,2,4,4,4};
        CgsRenderTarget wrappers[6];
        for (UINT index=0;index<6;++index) {
            wrappers[index].mpTarget=&fixtures[index]->target;
            pool.mapRenderTarget[slots[index]]=&wrappers[index];
        }
        outer={0,0,960,540}; AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE);
        SetWindowPos(renderengine::hWnd,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOZORDER|SWP_NOACTIVATE);
        pool.PCResizeDisplay();
        Check(renderengine::gDisplayWidth==960 && renderengine::gDisplayHeight==540 &&
              pool.mu32ScreenWidth==960 && pool.mu32ScreenHeight==540,"real Cgs pool publishes native display dimensions");
        for (UINT index=0;index<6;++index) {
            Check(wrappers[index].mpTarget==&fixtures[index]->target &&
                  wrappers[index].muWidth==960/divisors[index] && wrappers[index].muHeight==540/divisors[index] &&
                  fixtures[index]->state.muWidth==wrappers[index].muWidth &&
                  fixtures[index]->state.muHeight==wrappers[index].muHeight,
                  "real Cgs resize publishes coherent dimensions without replacing wrappers");
        }
    }
    gFrameBuffer.Release();
    gPresentation.Release();
    renderengine::gDevice->Release(); d3d->Release(); DestroyWindow(renderengine::hWnd);
    std::printf("PCDisplayResize: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
