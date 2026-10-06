#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"
#include "pc/gcm/renderengine/device.h"
#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/ImWhiteTexturePCLeaf.h"

namespace renderengine {
IDirect3DDevice9* gDevice = nullptr;
s32 gDisplayWidth=160,gDisplayHeight=96;
u32 guPresentCount=0,guDiagImBatches=0,guDiagImFullBlack=0;
bool gbDiagLastPresentBlack=false;
u32 FrameDumpEvery() { return 0; }
HRESULT PCGetBackBuffer(IDirect3DSurface9** out) { return gDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,out); }
extern const u8 gauIm3dVertexProgramPC[], gauIm3dPixelProgramPC[];
void Device::SetState(const RenderTargetState*) {}
}
namespace CgsGui {
const renderengine::BlendState* gpGuiBlendStateStandard=nullptr;
const renderengine::BlendState* gpGuiBlendStateAdditive=nullptr;
const renderengine::DepthStencilState* gpBillboardDepthStencilState=nullptr;
const renderengine::RasterizerState* gpGuiRasterizerStateCullNone=nullptr;
extern renderengine::Texture* gpGuiWhiteTexture;
}
static IDirect3DVertexShader9* spVertexShader;
static IDirect3DPixelShader9* spPixelShader;
static IDirect3DVertexDeclaration9* spDeclaration;
static float saMatrix[16];
static unsigned suChecks, suFailures, suDraws, suAssertions;
static unsigned suCount, suStride;
static std::vector<unsigned char> saPacked;
static std::vector<std::vector<unsigned char>> saRuns;
static void Check(bool good, const char* label) {
    ++suChecks;
    if (!good) { ++suFailures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* msg, const char*, int) { ++suAssertions; std::printf("ASSERT %s\n",msg); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { void WriteToLog(const char*) {} } }
namespace CgsGraphics {
ImRendererBase* ImRendererBase::mgpActiveRenderer = nullptr;
void* ImRendererBase::mgpDevice = nullptr;
// The fixture's 2D seams are unreachable; any accidental redirection is visible.
template<> Basic2dColouredTexturedVertex* ImRenderer<Basic2dColouredTexturedVertex>::RenderStart(u32) { ++suAssertions; return nullptr; }
template<> void ImRenderer<Basic2dColouredTexturedVertex>::RenderEnd(renderengine::PrimitiveType, const Basic2dColouredTexturedVertex*,u32) { ++suAssertions; }
void ImRendererBase::SetState(const renderengine::TextureState*) { ++suAssertions; }
static const u32 KU_PRIMITIVE_TRIANGLE_STRIP = 6;
static const float KF_DROPSHADOW_OFFSET_X=2, KF_DROPSHADOW_OFFSET_Y=3;
static unsigned guFontDiagShadowVerts;
}
void* RenderEngineDeviceBeginShaderStates(void*, void** out) { *out=saMatrix; return saMatrix; }
namespace shadow {
void Device::SetState(const renderengine::BlendMaterialState*) {}
void Device::SetState(const renderengine::DepthStencilState*) {}
void Device::SetState(const renderengine::RasterizerState*) {}
void Device::ResetShadowing() {}
void DeviceSetVertexProgramInternal(renderengine::ProgramBuffer*) { renderengine::gDevice->SetVertexShader(spVertexShader); }
void DeviceSetPixelProgram(renderengine::ProgramBuffer*) { renderengine::gDevice->SetPixelShader(spPixelShader); }
bool Device::SetVertexDescriptor(const renderengine::VertexDescriptorData*) {
    return SUCCEEDED(renderengine::gDevice->SetVertexDeclaration(spDeclaration));
}
void Device::FlushVertexProgramState() { renderengine::gDevice->SetVertexShaderConstantF(0,saMatrix,4); }
void* Device::SetState(const renderengine::TextureState* state,u32 unit) {
    renderengine::gDevice->SetTexture(unit,state ? state->mpRaster->mpD3DTexture : nullptr); return nullptr;
}
void* Device::SetState(void*,u32) { return nullptr; }
void* Device::SetResource(void* texture,u32 unit) {
    auto* t=static_cast<renderengine::Texture*>(texture);
    renderengine::gDevice->SetTexture(unit,t ? t->mpD3DTexture : nullptr); return nullptr;
}
}
void ImDeviceSetBlendState(void*) {}
void ImDeviceSetDepthStencilState(void*) {}
void ImDeviceSetRasterizerState(void*) {}
extern "C" void D3DDevice_SetRenderState_StencilRef(IDirect3DDevice9* device,u32 value) { device->SetRenderState(D3DRS_STENCILREF,value); }
extern "C" void D3DDevice_InsertFence(void*) {}
extern "C" void* D3DDevice_BeginVertices(void*,u32 primitive,u32 count,u32 stride) {
    Check(primitive==6,"original Xenos triangle-strip opcode reaches the device");
    suCount=count; suStride=stride; saPacked.assign(size_t(count)*stride,0xCD); return saPacked.data();
}
extern "C" void D3DDevice_EndVertices(void*) {
    saRuns.push_back(saPacked);
    Check(suStride==24,"CPU32 is packed into GPU24");
    Check(SUCCEEDED(renderengine::gDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,
        suCount>2 ? suCount-2 : 0,saPacked.data(),suStride)),"real native3D draw accepted"); ++suDraws;
}
#include "im3d_buffer.inc"
using namespace CgsGraphics;
static Matrix44 Identity() {
    Matrix44 m={}; m.xAxis.x=m.yAxis.y=m.zAxis.z=m.wAxis.w=1; return m;
}
static std::vector<DWORD> Pixels(IDirect3DSurface9* target) {
    IDirect3DSurface9* copy=nullptr; std::vector<DWORD> pixels(160*96);
    Check(SUCCEEDED(renderengine::gDevice->CreateOffscreenPlainSurface(160,96,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,nullptr)),"readback surface");
    if (!copy) return pixels;
    Check(SUCCEEDED(renderengine::gDevice->GetRenderTargetData(target,copy)),"native pixels read back");
    D3DLOCKED_RECT lock={};
    if (SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY))) {
        for (unsigned y=0;y<96;++y) std::memcpy(pixels.data()+160*y,static_cast<char*>(lock.pBits)+y*lock.Pitch,640);
        copy->UnlockRect();
    }
    copy->Release(); return pixels;
}
static void AllocationBudget() {
    Check(EnsureWorldDispatchAllocator(),"production joined allocator prepares");
    // Actual seven bank-pairs, with original3D capacities independent of the
    // production budget expression. This reproduces the runtime allocation order.
    ImRenderBuffer<Basic2dColouredTexturedVertex> main2d,debug2d,modal2d;
    for(auto* b : {&main2d,&debug2d,&modal2d}) b->Construct();
    Check(main2d.Prepare(512*1024,512*1024,&sWorldDispatchAllocator,false),"main2D banks fit");
    Check(debug2d.Prepare(160*1024,2*1024*1024,&sWorldDispatchAllocator,true),"debug2D banks fit");
    Check(modal2d.Prepare(160*1024,2*1024*1024,&sWorldDispatchAllocator,true),"modal2D banks fit");
    ImRenderBuffer<Im3dVertex> main3d,debug3d,race3d,menus3d;
    for(auto* b : {&main3d,&debug3d,&race3d,&menus3d}) b->Construct();
    Check(main3d.Prepare(0x400,0x8000,&sWorldDispatchAllocator,false),"original main3D banks fit");
    Check(debug3d.Prepare(0x100000,0x100000,&sWorldDispatchAllocator,true),"original debug3D banks fit");
    Check(race3d.Prepare(0x20000,0x20000,&sWorldDispatchAllocator,false),"original RacePosition banks fit");
    Check(menus3d.Prepare(512,4,&sWorldDispatchAllocator,false),"original static MenusHud banks fit");
    CgsGraphics::DispatchFrame primary={}, gdlRead={}, gdlWrite={}, nextMeshes={}, absent={};
    primary.GetBin().Construct(12*1024*1024,&sWorldDispatchAllocator);
    gdlRead.GetBin().Construct(8*1024*1024,&sWorldDispatchAllocator);
    gdlWrite.GetBin().Construct(8*1024*1024,&sWorldDispatchAllocator);
    nextMeshes.GetBin().Construct(12*1024*1024,&sWorldDispatchAllocator);
    Check(DispatchStorageAvailablePC(&primary),"primary mesh bank survives immediate allocations");
    Check(DispatchStorageAvailablePC(&gdlRead) && DispatchStorageAvailablePC(&gdlWrite),"both GDL banks survive immediate allocations");
    Check(DispatchStorageAvailablePC(&nextMeshes),"next12MiB mesh bank has actual backing before Reset");
    Check(!DispatchStorageAvailablePC(nullptr) && !DispatchStorageAvailablePC(&absent),"failed storage is rejected before key allocation");
    rw::ResourceDescriptor deferred={}; deferred.m_baseResourceDescriptors[0].m_size=192*1024; deferred.m_baseResourceDescriptors[0].m_alignment=128;
    const auto remaining=sWorldDispatchAllocator.DoAllocate(deferred,nullptr);
    Check(remaining.m_baseResources[0]!=nullptr,"deferred render-engine objects retain the existing192KiB capacity");
}
int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    AllocationBudget();
    if(IM3D_BUDGET_ONLY) {
        std::printf("PCIm3dBuffer: %u checks, %u failures\n",suChecks,suFailures);
        return suFailures ? 1 : 0;
    }
    HWND window=CreateWindowA("STATIC","im3d buffer regression",WS_OVERLAPPEDWINDOW,0,0,160,96,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp={}; pp.Windowed=TRUE; pp.hDeviceWindow=window; pp.BackBufferWidth=160; pp.BackBufferHeight=96; pp.BackBufferFormat=D3DFMT_X8R8G8B8; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    if (!api || FAILED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&renderengine::gDevice))) return 2;
    auto* device=renderengine::gDevice;
    PrepareIm3dStateLibraryPC();
    Check(CgsGui::gpGuiBlendStateStandard && CgsGui::gpGuiBlendStateAdditive &&
          CgsGui::gpGuiRasterizerStateCullNone && CgsGui::gpBillboardDepthStencilState,"GUI shares complete native state objects");
    const auto* zOn=GetIm3dDepthStencilZBufferOnPC();
    Check(zOn->muZEnable==1 && zOn->muZWriteEnable==1 && zOn->muZFunc==3 && zOn->muInitialised==1,"original ZBufferOn enables depth test/write LESSEQUAL");
    auto* white=GetImWhiteTexturePC();
    Check(white && white==CgsGui::gpGuiWhiteTexture && white==renderengine::GetImmediateWhiteTexturePC(device),"banking and masks share one real white texture home");
    Check(white && white->muWidth==4 && white->muHeight==4 && white->muNumMipLevels==1,"original White827F1AE8 dimensions4x4/one level");
    if(white) {
        D3DLOCKED_BOX pixels={};
        Check(SUCCEEDED(renderengine::TextureUploadPC::Lock(white->mpD3DTexture,0,0,D3DLOCK_READONLY,pixels)),"real white raster can be read");
        if(pixels.pBits) {
            bool allWhite=true;
            for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x)
                allWhite &= reinterpret_cast<const DWORD*>(static_cast<const char*>(pixels.pBits)+y*pixels.RowPitch)[x]==0xffffffff;
            Check(allWhite,"all16 original white pixels are opaque white");
            renderengine::TextureUploadPC::Unlock(white->mpD3DTexture,0,0);
        }
    }
    std::puts("[im3d-test] native device ready");
    // Actual production PROGRAM0 bytecode, skipping its ProgramBufferData20-byte wrapper.
    Check(SUCCEEDED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(renderengine::gauIm3dVertexProgramPC+20),&spVertexShader)),"actual im3d vertex program");
    Check(SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(renderengine::gauIm3dPixelProgramPC+20),&spPixelShader)),"actual im3d pixel program");
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_UBYTE4N,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0},
        {0,16,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    Check(SUCCEEDED(device->CreateVertexDeclaration(elements,&spDeclaration)),"native24-byte descriptor");
    IDirect3DTexture9* atlas=nullptr; device->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&atlas,nullptr);
    D3DLOCKED_RECT lock={}; atlas->LockRect(0,&lock,nullptr,0);
    for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]=x ? 0xff00ff00 : 0xff0000ff;
    atlas->UnlockRect(0);
    renderengine::Texture texture={}; texture.mpD3DTexture=atlas;
    renderengine::TextureState textureState={}; textureState.mpRaster=&texture;
    device->SetRenderState(D3DRS_ZENABLE,FALSE); device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE); device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT); device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT); device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
    device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP); device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    alignas(128) static unsigned char memory[1024*1024];
    rw::Resource resource={}; resource.m_baseResources[0]=memory;
    rw::ResourceDescriptor capacity={}; capacity.m_baseResourceDescriptors[0].m_size=sizeof(memory);
    rw::LinearResourceAllocator allocator; allocator.Initialize(resource,capacity);
    Im3dRenderBuffer buffer;
    std::puts("[im3d-test] buffer preparing");
    Check(buffer.Prepare(0x20000,0x20000,&allocator,false),"real3D command and vertex banks prepare");
    Check(sizeof(Im3dVertex)==32 && offsetof(Im3dVertex,mv4Colour)==16 && offsetof(Im3dVertex,mv2Tex0UV)==20,"ARTIST CPU layout");
    Im3d renderer={}; renderer.mapVertexProgramBuffer[0]=reinterpret_cast<renderengine::ProgramBuffer*>(1);
    renderer.mapPixelProgramBuffer[0]=reinterpret_cast<renderengine::ProgramBuffer*>(1); renderer.mpVertexDescriptor=reinterpret_cast<renderengine::VertexDescriptor*>(1);
    buffer.BeginRendering();
    Matrix44 model=Identity(), vp=Identity(); model.wAxis.x=.4f; model.wAxis.y=.1f; vp.xAxis.x=.5f;
    buffer.SetTransform(model,vp);
    TextRenderer font; font.Construct(); font.mpIm3dRenderBuffer=&buffer;
    auto* glyph=font.RenderBufferRenderStart(4,TextRenderer::EImRenderingType_Buffered);
    const float xy[4][2]={{-.2f,.2f},{.2f,.2f},{-.2f,-.2f},{.2f,-.2f}};
    for(unsigned i=0;i<4;++i) { glyph[i].mv2Pos={xy[i][0],xy[i][1]}; glyph[i].mv4Colour={255,255,255,255}; glyph[i].mv2Tex0UV={float(i&1),float((i>>1)&1)}; }
    font.mapaVertices[0]=glyph; font.mauVertexCount[0]=4; font.RenderBufferSetTextureState(&textureState,TextRenderer::EImRenderingType_Buffered);
    const RGBA shadowColour=0xff404040;
    font.RenderDropShadow(&shadowColour,TextRenderer::EImRenderingType_Buffered);
    font.RenderBufferRenderEnd(6,glyph,4,TextRenderer::EImRenderingType_Buffered);
    std::puts("[im3d-test] font commands queued");
    Check(!font.mabTempVerticesInUse[0] && buffer.miNumRendersStarted==0,"3D main and shadow reservations close");
    const auto* vertices=reinterpret_cast<const Im3dVertex*>(buffer.mpWriteBuffer->mpu8VertexBuffer);
    Check(std::fabs(vertices[0].mv3Pos.z-.001f)<1e-7f && std::fabs(vertices[0].mv3Pos.x-(-.17f))<1e-7f,"actual3D shadow X/Y .03 and Z .001 constants");
    Check(vertices[4].mv3Pos.x==-.2f && vertices[4].mv3Pos.z==0 && vertices[4].mv3Pos.w==0,"main glyph scratch widens into source32");
    buffer.EndRendering(); buffer.Swap(); buffer.Clear();
    glyph[0].mv2Pos.x=10000;
    Check(buffer.mpDispatchBuffer->muVertexBufferWritePos==8*32,"following producer cannot move frozen glyphs");
    IDirect3DSurface9* target=nullptr; device->GetRenderTarget(0,&target);
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0); device->BeginScene(); buffer.Dispatch(&renderer); device->EndScene();
    std::puts("[im3d-test] native draw done");
    Check(suDraws==2 && ImRendererBase::mgpActiveRenderer==nullptr,"two original3D passes dispatch and release active renderer");
    Check(saRuns.size()==2 && saRuns[1].size()==4*24,"native main run has four packed vertices");
    if(saRuns.size()==2) {
        const float* first=reinterpret_cast<const float*>(saRuns[1].data());
        Check(first[0]==-.2f && first[1]==.2f && first[2]==0,"packing reads CPU32 position");
        Check(*reinterpret_cast<const DWORD*>(saRuns[1].data()+12)==0xffffffff,"packing reads colour at source16");
        Check(first[4]==0 && first[5]==0,"packing reads UV at source20/24");
        const float* next=reinterpret_cast<const float*>(saRuns[1].data()+24);
        Check(next[0]==.2f && next[4]==1,"packing advances CPU32 for second vertex");
    }
    float native[16]={}; device->GetVertexShaderConstantF(0,native,4);
    Check(native[0]==.5f && native[12]==.2f && native[13]==.1f && native[15]==1,"model/view projection reaches actual GPU constants in row-vector order");
    auto pixels=Pixels(target);
    Check((pixels[43*160+90]&0xffffff)==0x0000ff,"projected glyph left samples actual atlas blue");
    Check((pixels[43*160+101]&0xffffff)==0x00ff00,"projected glyph right samples actual atlas green");
    Check((pixels[48*160+80]&0xffffff)==0,"world projection moves glyph away from old screen centre");
    Check(suAssertions==0,"no silent2D fallback or original assertion");
    target->Release(); atlas->Release(); spDeclaration->Release(); spVertexShader->Release(); spPixelShader->Release(); device->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCIm3dBuffer: %u checks, %u failures\n",suChecks,suFailures); return suFailures ? 1 : 0;
}
