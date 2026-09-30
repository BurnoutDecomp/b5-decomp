// Native D3D9 oracle for the shared APT/FLAPT command stream. Both the buffered
// and existing immediate consumers are compiled from production translation units.
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"
#include "GameShared/GameClasses/Gui/PC/CgsAptRenderBackendPC.h"
#include "pc/gcm/renderengine/device.h"
#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/AssertFramePCLeaf.h"

namespace renderengine {
IDirect3DDevice9* gDevice = nullptr;
s32 gDisplayWidth = 320, gDisplayHeight = 180;
u32 guPresentCount = 1, guDiagImBatches = 0, guDiagImFullBlack = 0;
bool gbDiagLastPresentBlack = false;
u32 FrameDumpEvery() { return 1; }
HRESULT PCGetBackBuffer(IDirect3DSurface9** out) {
    return gDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, out);
}
}
namespace CgsGui { const renderengine::BlendState* gpGuiBlendStateAdditive =
    reinterpret_cast<const renderengine::BlendState*>(1); }
namespace CgsDev {
namespace Log { void WriteToLog(const char*) {} }
namespace Assert {
unsigned gAssertions = 0;
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
}
using namespace CgsGraphics;
static int checks, failures;
static void Check(bool good, const char* label) {
    ++checks;
    if (!good) { ++failures; std::printf("FAIL %s\n", label); }
}
static Im2dTransform ScreenTransform() {
    Im2dTransform t = {};
    t.mOriginXYZ.x = -1; t.mOriginXYZ.y = 1;
    t.mRightUp.x = 1.0f / 640; t.mRightUp.w = -1.0f / 360;
    t.mColourScale.x = t.mColourScale.y = t.mColourScale.z = t.mColourScale.w = 1;
    return t;
}
static void Quad(Basic2dColouredTexturedVertex* v, float x0, float y0,
                 float x1, float y1, RGBA8 colour) {
    const float x[] = {x0,x1,x0,x1}, y[] = {y0,y0,y1,y1};
    for (int i = 0; i < 4; ++i) {
        v[i].mv2Pos.x = x[i]; v[i].mv2Pos.y = y[i]; v[i].mv4Colour = colour;
        v[i].mv2Tex0UV.x = (i & 1) ? 1.0f : 0.0f;
        v[i].mv2Tex0UV.y = (i & 2) ? 1.0f : 0.0f;
    }
}
static std::vector<DWORD> Pixels(IDirect3DSurface9* target) {
    D3DSURFACE_DESC desc = {}; target->GetDesc(&desc);
    IDirect3DSurface9* copy = nullptr;
    std::vector<DWORD> result(desc.Width * desc.Height, 0xdeaddead);
    if (SUCCEEDED(renderengine::gDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height,
        desc.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr))) {
        D3DLOCKED_RECT locked = {};
        if (SUCCEEDED(renderengine::gDevice->GetRenderTargetData(target, copy)) &&
            SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
            for (UINT y = 0; y < desc.Height; ++y)
                std::memcpy(result.data() + y * desc.Width,
                    static_cast<const char*>(locked.pBits) + y * locked.Pitch, desc.Width * 4);
            copy->UnlockRect();
        }
        copy->Release();
    }
    return result;
}
static DWORD At(const std::vector<DWORD>& pixels, int x, int y) {
    return pixels[(y * renderengine::gDisplayHeight / 720) * renderengine::gDisplayWidth
                  + x * renderengine::gDisplayWidth / 1280] & 0xffffff;
}
static bool Close(DWORD a, DWORD b, int tolerance = 1) {
    for (int lane = 0; lane < 3; ++lane)
        if (std::abs(int((a >> (lane * 8)) & 255) - int((b >> (lane * 8)) & 255)) > tolerance)
            return false;
    return true;
}
struct FailingAllocator : rw::LinearResourceAllocator {
    unsigned calls = 0, frees = 0, failAt = 0;
    rw::Resource DoAllocate(const rw::ResourceDescriptor& descriptor, const char* name) override {
        if (++calls == failAt) return rw::Resource{};
        return rw::LinearResourceAllocator::DoAllocate(descriptor,name);
    }
    void DoFree(const rw::Resource&) override { ++frees; }
};
static void CheckAssertFrame(IDirect3DDevice9* device, IDirect3DSurface9* scene, unsigned width, unsigned height) {
    IDirect3DSurface9* modal=nullptr;
    IDirect3DSurface9* depth=nullptr;
    const HRESULT mr=device->CreateRenderTarget(width,height,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&modal,nullptr);
    const HRESULT dr=device->CreateDepthStencilSurface(width,height,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&depth,nullptr);
    Check(SUCCEEDED(mr) && SUCCEEDED(dr),"native assertion targets prepare");
    if (!modal || !depth) { if(modal)modal->Release(); if(depth)depth->Release(); return; }
    device->SetRenderTarget(0,scene); device->SetDepthStencilSurface(depth);
    const D3DVIEWPORT9 before={7,9,width/2,height/2,.2f,.8f};
    device->SetViewport(&before);
    device->SetRenderState(D3DRS_ZENABLE,TRUE);
    const float oldValue[4]={1,2,3,4}, newValue[4]={5,6,7,8};
    renderengine::PCSetVertexShaderConstantF(device,17,oldValue,1);
    renderengine::PCSetSamplerState(device,3,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    Check(SUCCEEDED(device->BeginScene()),"assertion interrupts an open scene");
    {
        renderengine::PCAssertFrame saved(device,modal);
        Check(saved.IsReady(),"modal frame can preserve the interrupted state");
        IDirect3DSurface9* bound=nullptr; device->GetRenderTarget(0,&bound);
        Check(bound==modal,"assertion binds its visible target instead of the offscreen scene");
        if(bound)bound->Release();
        device->BeginScene();
        device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff00ff00,1,0);
        renderengine::PCSetVertexShaderConstantF(device,17,newValue,1);
        renderengine::PCSetSamplerState(device,3,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        device->SetRenderState(D3DRS_ZENABLE,FALSE);
        device->EndScene();
    }
    Check(FAILED(device->BeginScene()),"resume restores the caller's open scene bracket");
    device->EndScene();
    IDirect3DSurface9* bound=nullptr; IDirect3DSurface9* restoredDepth=nullptr;
    device->GetRenderTarget(0,&bound); device->GetDepthStencilSurface(&restoredDepth);
    Check(bound==scene && restoredDepth==depth,"resume restores colour and depth targets");
    if(bound)bound->Release(); if(restoredDepth)restoredDepth->Release();
    D3DVIEWPORT9 viewport={}; DWORD z=0,address=0; float values[4]={};
    device->GetViewport(&viewport); device->GetRenderState(D3DRS_ZENABLE,&z);
    device->GetVertexShaderConstantF(17,values,1);
    device->GetSamplerState(3,D3DSAMP_ADDRESSU,&address);
    Check(!std::memcmp(&viewport,&before,sizeof(before)) && z==TRUE && address==D3DTADDRESS_WRAP
          && !std::memcmp(values,oldValue,sizeof(values)),"resume restores pipeline, viewport and shader state");
    renderengine::PCSetVertexShaderConstantF(device,17,newValue,1);
    renderengine::PCSetSamplerState(device,3,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    device->GetVertexShaderConstantF(17,values,1);
    device->GetSamplerState(3,D3DSAMP_ADDRESSU,&address);
    Check(!std::memcmp(values,newValue,sizeof(values)) && address==D3DTADDRESS_CLAMP,
          "native caches cannot skip the first upload after context restoration");
    const auto pixels=Pixels(modal);
    Check(!pixels.empty() && (pixels[0]&0xffffff)==0x00ff00,"modal rendering reached the visible surface");
    {
        renderengine::PCAssertFrame saved(device,modal);
        device->BeginScene(); device->EndScene();
    }
    Check(SUCCEEDED(device->BeginScene()),"an assertion between frames leaves the scene closed");
    device->EndScene();
    device->SetDepthStencilSurface(nullptr);
    depth->Release(); modal->Release();
}
int main() {
    HWND window = CreateWindowA("STATIC", "Buffered 2D regression", WS_OVERLAPPEDWINDOW,
        0,0,320,180,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    auto d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = 320; pp.BackBufferHeight = 180;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.hDeviceWindow = window;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (!d3d || FAILED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&renderengine::gDevice))) return 2;
    auto device = renderengine::gDevice;
    // The real Prepare path carves two independent command/vertex banks.
    std::vector<unsigned char> storage(2u * 1024u * 1024u);
    rw::LinearResourceAllocator allocator;
    rw::Resource resource = {}; resource.m_baseResources[0] = storage.data();
    rw::ResourceDescriptor capacity = {};
    capacity.m_baseResourceDescriptors[0].m_size = static_cast<u32>(storage.size());
    capacity.m_baseResourceDescriptors[0].m_alignment = 128;
    allocator.Initialize(resource, capacity);
    for (unsigned bank = 1; bank <= 4; ++bank) {
        FailingAllocator failing;
        failing.Initialize(resource,capacity); failing.failAt = bank;
        Im2dRenderBuffer rejected; rejected.Construct();
        Check(!rejected.Prepare(65536,262144,&failing,false), "each failed allocation rejects preparation");
        Check(!rejected.IsPreparedPC() && rejected.GetFirstCommand() == nullptr,
              "failed bank cannot be published or consumed");
        Check(failing.frees == 3, "partial allocation returns every successful bank");
        rejected.Dispatch(); // Must be a safe empty consumer even with a live D3D device.
    }
    Im2dRenderBuffer buffer;
    buffer.Construct();
    Check(!buffer.IsPreparedPC(), "constructed buffer is not ready before allocation");
    Check(!buffer.Prepare(65536,262144,nullptr,false), "missing allocator rejects preparation");
    Check(buffer.Prepare(65536,262144,&allocator,false), "production double buffer prepares");
    Im2d immediate = {};
    const auto topology = static_cast<renderengine::PrimitiveType>(6);
    Basic2dColouredTexturedVertex background[4], overlay[4];
    Quad(background,0,0,1280,720,{255,0,0,255});
    Quad(overlay,320,180,960,540,{0,255,0,255});
    const Im2dTransform transform = ScreenTransform();
    const int widths[] = {320,1280,2560}, heights[] = {180,720,1440};
    for (int resolution = 0; resolution < 3; ++resolution) {
        renderengine::gDisplayWidth = widths[resolution];
        renderengine::gDisplayHeight = heights[resolution];
        IDirect3DSurface9* target = nullptr;
        Check(SUCCEEDED(device->CreateRenderTarget(widths[resolution],heights[resolution],
            D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr)), "native target created");
        if (!target) break;
        device->SetRenderTarget(0,target);
        D3DVIEWPORT9 viewport = {0,0,UINT(widths[resolution]),UINT(heights[resolution]),0,1};
        device->SetViewport(&viewport);
        device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
        // APT appends logical commands; FLAPT appends through the native Im2d
        // interface. No device is available during recording.
        renderengine::gDevice = nullptr;
        Im2dColouredTexturedRenderBuffer& apt = buffer;
        apt.BeginRendering(); apt.SetTexture(nullptr); apt.Render(topology,background,4); apt.EndRendering();
        buffer.BeginRendering();
        buffer.BatchTransformTextureBlendRenderStatic(transform,nullptr,nullptr,topology,overlay,4,3);
        buffer.SetTransform(transform);
        buffer.SetTexture(nullptr);
        auto glyph = buffer.RenderStart(4);
        Quad(glyph,560,260,720,460,{255,255,0,255});
        buffer.RenderEnd(topology,glyph,4); buffer.EndRendering();
        renderengine::gDevice = device;
        Check(At(Pixels(target),640,360) == 0, "recording does not draw on the GPU");
        CgsGui::PublishAptIm2dRenderBufferPC(&buffer);
        // Fill the other bank while the published stream remains available.
        Basic2dColouredTexturedVertex next[4];
        Quad(next,0,0,1280,720,{0,0,255,255});
        buffer.BeginRendering(); buffer.SetTexture(nullptr);
        buffer.Render(topology,next,4); buffer.EndRendering();
        std::memset(next,0,sizeof(next));
        device->BeginScene(); CgsGui::DispatchAptIm2dRenderBufferPC(&buffer); device->EndScene();
        const auto published = Pixels(target);
        Check(At(published,160,90) == 0xff0000, "APT background remains behind FLAPT");
        Check(At(published,400,360) == 0x00ff00, "combined FLAPT record draws at correct resolution");
        Check(At(published,640,360) == 0xffff00, "reserved text vertices follow their transform");
        device->BeginScene(); CgsGui::DispatchAptIm2dRenderBufferPC(&buffer); device->EndScene();
        Check(At(Pixels(target),640,360) == 0xffff00,
              "consuming a published GUI frame never swaps the producer bank");
        buffer.Swap(); buffer.Clear();
        device->BeginScene(); buffer.Dispatch(&immediate); device->EndScene();
        Check(At(Pixels(target),640,360) == 0x0000ff, "next bank owns copied dynamic vertices");

        // A real dark texture makes a pre-texture colour shift observably wrong.
        renderengine::Texture texture = {};
        IDirect3DTexture9* raster = nullptr;
        device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&raster,nullptr);
        D3DLOCKED_RECT locked = {}; raster->LockRect(0,&locked,nullptr,0);
        *static_cast<DWORD*>(locked.pBits) = 0xff201008; raster->UnlockRect(0);
        texture.mpD3DTexture = raster;
        Im2dTransform colour = transform;
        colour.mColourScale.x = .5f; colour.mColourScale.y = .25f; colour.mColourScale.z = 0;
        colour.mColourShift.x = .4f; colour.mColourShift.y = .1f; colour.mColourShift.z = .2f;
        Basic2dColouredTexturedVertex white[4]; Quad(white,0,0,1280,720,{255,255,255,255});
        buffer.BeginRendering();
        buffer.BatchTransformTextureBlendRenderStatic(colour,&texture,nullptr,topology,white,4,3);
        buffer.EndRendering(); buffer.Swap(); buffer.Clear();
        device->BeginScene(); buffer.Dispatch(&immediate); device->EndScene();
        const DWORD buffered = At(Pixels(target),640,360);
        Check(Close(buffered,0x761e33,2), "colour shift follows texture multiplication");
        device->BeginScene(); immediate.BeginRendering();
        immediate.BatchTransformTextureBlendRenderStatic(colour,&texture,nullptr,topology,white,4,3);
        immediate.EndRendering(); device->EndScene();
        Check(Close(buffered,At(Pixels(target),640,360)), "buffered colour matches existing immediate pixels");

        // A following movie/debug block must not inherit the preceding tint.
        buffer.BeginRendering(); buffer.SetTexture(nullptr); buffer.Render(topology,white,4);
        buffer.EndRendering(); buffer.Swap(); buffer.Clear();
        device->BeginScene(); buffer.Dispatch(&immediate); device->EndScene();
        Check(At(Pixels(target),640,360) == 0xffffff, "new block restores logical position and colour identity");

        // Mask coordinates are frozen with the frame, and PopMask must restore
        // the enclosing draw state before later HUD / debug records execute.
        buffer.BeginRendering(); buffer.SetTexture(nullptr); buffer.Render(topology,background,4);
        Basic2dColouredTexturedVertex corners[2] = {};
        corners[0].mv2Pos.x = 480; corners[0].mv2Pos.y = 270;
        corners[1].mv2Pos.x = 800; corners[1].mv2Pos.y = 450;
        corners[1].mv2Tex0UV.x = corners[1].mv2Tex0UV.y = 1;
        buffer.PushMask(static_cast<renderengine::Texture*>(nullptr),corners);
        buffer.SetTexture(nullptr); buffer.Render(topology,overlay,4); buffer.PopMask();
        buffer.EndRendering(); buffer.Swap(); buffer.Clear();
        device->BeginScene(); buffer.Dispatch(&immediate); device->EndScene();
        const auto masked = Pixels(target);
        Check(At(masked,640,360) == 0x00ff00, "mask preserves the inside pixels");
        Check(At(masked,400,360) == 0xff0000, "mask clips the outside pixels");
        DWORD scissor = TRUE; device->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor);
        Check(scissor == FALSE, "PopMask clears clipping after the recorded block");

        // The two batch flags are independent. A missing blend flag must keep
        // the explicitly selected additive state; a set bit must replace it.
        buffer.BeginRendering(); buffer.SetTexture(nullptr); buffer.Render(topology,background,4);
        buffer.SetState(CgsGui::gpGuiBlendStateAdditive);
        buffer.BatchTransformTextureBlendRenderStatic(transform,nullptr,nullptr,topology,overlay,4,1);
        buffer.EndRendering(); buffer.Swap(); buffer.Clear();
        device->BeginScene(); buffer.Dispatch(&immediate); device->EndScene();
        Check(At(Pixels(target),640,360) == 0xffff00, "unset blend flag preserves additive blending");
        buffer.BeginRendering(); buffer.SetTexture(&texture);
        buffer.BatchTransformTextureBlendRenderStatic(transform,nullptr,nullptr,topology,white,4,2);
        buffer.EndRendering(); buffer.Swap(); buffer.Clear();
        device->BeginScene(); buffer.Dispatch(&immediate); device->EndScene();
        Check(Close(At(Pixels(target),640,360),0x201008), "unset texture flag preserves the bound texture");
        DWORD blend = 0; device->GetRenderState(D3DRS_DESTBLEND,&blend);
        Check(blend == D3DBLEND_INVSRCALPHA, "set blend flag binds standard blending");
        CheckAssertFrame(device,target,widths[resolution],heights[resolution]);
        raster->Release(); target->Release();
    }
    Check(CgsDev::Assert::gAssertions == 0, "production command writers raised no assertions");
    buffer.Release();
    Check(!buffer.IsPreparedPC(), "released buffer cannot be consumed");
    device->Release(); d3d->Release(); DestroyWindow(window);
    std::printf("PCIm2dBuffer: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
