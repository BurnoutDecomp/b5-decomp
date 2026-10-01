// Native winding oracle through the real factory, shadow-device setters,
// manager brackets and renderer cascade loop. Target binding and list contents
// are fixture boundaries; neither the cull decision nor the loop is copied here.
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "pc/gcm/renderengine/renderstates.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "GameSource/Graphics/BrnShadowMapRenderManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "shadow_cull_technique.inc"

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement=1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1; }
static int checks, failures, nativeBinds;
static void Check(bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL %s\n",text);}}
static std::vector<void*> allocations;
void* AllocateTestState(void* out,const void* descriptor)
{
    const auto* entries=static_cast<const renderengine::ResourceDescriptorEntry*>(descriptor);
    if((entries[0].muSize!=52&&entries[0].muSize!=76)||entries[0].muAlignment!=4)std::abort();
    void* memory=std::calloc(1,entries[0].muSize);allocations.push_back(memory);
    static_cast<void**>(out)[0]=memory;return out;
}
static IDirect3DDevice9* gpD3DDevice;
static IDirect3DDevice9* Dev(){return gpD3DDevice;}
namespace CgsDev {
struct QuietLog{template<class T>QuietLog& operator<<(const T&){return *this;}};
namespace Message {static unsigned gxMessageFilterFlags=0;}
namespace Log {static QuietLog stream;static QuietLog* gpDebugPrint=&stream;}
}
namespace renderengine {
static u64 drawCalls;
static bool resetEnabled;
static u32 resetIndex;
void WorldDraw_SetPrimitiveReset(bool enabled,u32 index)
{++nativeBinds;resetEnabled=enabled;resetIndex=index;}
static u64 WorldDrawCallCount(){return drawCalls;}
static void ShadowProbe_Begin(u32){}
static void ShadowProbe_End(u32){}
}
namespace shadow {
struct Device {
    inline static bool mbRasteriserStateLocked=false,mbDepthStencilStateLocked=false;
    inline static const renderengine::RasterizerState* mpRasterizerState=nullptr;
    inline static const renderengine::DepthStencilState* mpDepthStencilState=nullptr;
    inline static const renderengine::BlendMaterialState* mpBlendState=nullptr;
    static void* LockRasteriserState();
    static void* UnlockRasteriserState();
    static void SetState(const renderengine::RasterizerState*);
    static void* Xbox2SetRasterizerStateLowLevelShadowed(void*,bool);
    static void SetMaterialRenderStatesPC(const CgsGraphics::MaterialTechniqueView*,bool);
    static void Xbox2SetDepthStencilStateLowLevelShadowed(void*,bool){}
    static void Xbox2SetStateLowLevelShadowed(void*,bool){}
};
}
#include "shadow_cull.inc"

static IDirect3DSurface9 *target,*readback;
static DWORD Pixel(unsigned x)
{
    if(FAILED(Dev()->GetRenderTargetData(target,readback)))return 0;
    D3DLOCKED_RECT lock{};if(FAILED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return 0;
    DWORD colour;std::memcpy(&colour,static_cast<char*>(lock.pBits)+16*lock.Pitch+x*4,4);
    readback->UnlockRect();return colour|0xff000000u;
}
static void DrawAndCheck(u32 mode)
{
    // Left triangle is clockwise in screen coordinates; right is counterclockwise.
    struct Vertex{float x,y,z,rhw;DWORD colour;};
    const Vertex vertices[]={{4,8,.5f,1,0xffffffff},{28,8,.5f,1,0xffffffff},{4,56,.5f,1,0xffffffff},
        {36,8,.5f,1,0xffffffff},{60,56,.5f,1,0xffffffff},{60,8,.5f,1,0xffffffff}};
    Dev()->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
    Dev()->BeginScene();HRESULT hr=Dev()->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,vertices,sizeof(Vertex));Dev()->EndScene();
    Check(SUCCEEDED(hr)&&Pixel(8)==(mode==2?0xff000000u:0xffffffffu)
        &&Pixel(56)==(mode==1?0xff000000u:0xffffffffu),"native opposite-winding pixels match selected cull state");
}
static bool forceFront=true;
static int emptyList=-1;
static std::vector<int> lists,begins,ends;
static renderengine::MaterialState* material;
static CgsGraphics::MaterialTechniqueView technique{};
struct BrnRendererMemory {};
void BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap(s32 index,bool clear,BrnRendererMemory*)
{Check(clear&&!shadow::Device::mbRasteriserStateLocked,"cascade begins clear and unlocked");begins.push_back(index);}
void BrnGraphics::ShadowMapRenderManager::EndRenderShadowMap(s32 index,BrnRendererMemory*)
{
    Check(!shadow::Device::mbRasteriserStateLocked,"cascade resolves unlocked");
    DWORD cull=0;Dev()->GetRenderState(D3DRS_CULLMODE,&cull);
    Check(shadow::Device::mpRasterizerState==CgsRasterizerStateFactory::GetState(0)&&cull==D3DCULL_CW,
          "cascade ends with the restored BACK pointer and native winding");
    ends.push_back(index);
}
struct List {
    int index;
    void DispatchAllMeshesZOnly(void*,void*)
    {
        lists.push_back(index);
        const bool locked=forceFront?(index==2||index==3):(index==0||index==1||index==4);
        Check(shadow::Device::mbRasteriserStateLocked==locked,"assembly-defined caster group owns the lock");
        const int before=nativeBinds;
        if(index!=emptyList)for(int i=0;i<4;++i){
            const u32 mode=(i&1)?2:0;
            material->mpRasterizerState=CgsRasterizerStateFactory::GetState(mode==2?0:2);
            shadow::Device::SetMaterialRenderStatesPC(&technique,true);
            DWORD native=0;Dev()->GetRenderState(D3DRS_CULLMODE,&native);
            Check(native==(locked?D3DCULL_CCW:(mode==2?D3DCULL_CW:D3DCULL_NONE)),"material writer respects the locked native cull state");
            DrawAndCheck(locked?1:mode);++renderengine::drawCalls;
        }
        Check(index==emptyList?nativeBinds==before:(locked?nativeBinds==before:nativeBinds>before),
              "locked materials avoid all native rasterizer writes; unlocked materials still bind");
    }
};
struct Frame {
    List entries[5]={{0},{1},{2},{3},{4}};
    List* GetList(u32 index){if(index>=5)std::abort();return &entries[index];}
};
struct RendererFixture {
    BrnGraphics::ShadowMapRenderManager mShadowMapRenderManager;
    BrnRendererMemory mAllocatedRenderTargets;
    Frame mSingleBufferedDispatchFrame;
    void* mpInterpreter=nullptr;
    void Construct(){
#include "shadow_cull_construct.inc"
    }
    void Render(void* lpContext){
#include "shadow_cull_loop.inc"
    }
};
int main()
{
    HWND window=CreateWindowA("STATIC","Shadow cull checks",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9Ex* api=nullptr;IDirect3DDevice9Ex* device=nullptr;
    D3DPRESENT_PARAMETERS p{};p.Windowed=TRUE;p.SwapEffect=D3DSWAPEFFECT_FLIPEX;p.BackBufferCount=2;
    p.BackBufferWidth=p.BackBufferHeight=64;p.BackBufferFormat=D3DFMT_X8R8G8B8;p.hDeviceWindow=window;p.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    if(FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION,&api))||FAILED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&p,nullptr,&device)))return 2;
    gpD3DDevice=device;
    device->CreateRenderTarget(64,64,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr);
    device->CreateOffscreenPlainSurface(64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr);
    if(!target||!readback)return 3;
    device->SetRenderTarget(0,target);device->SetDepthStencilSurface(nullptr);
    D3DVIEWPORT9 viewport={0,0,64,64,0,1};device->SetViewport(&viewport);RECT rect={0,0,64,64};device->SetScissorRect(&rect);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_ZENABLE,FALSE);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    for(uintptr_t at=0x20000000;at<0x70000000&&!material;at+=0x1000000)
        material=static_cast<renderengine::MaterialState*>(VirtualAlloc(reinterpret_cast<void*>(at),65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!material)return 4;
    technique.muRenderStateGroup=u32(uintptr_t(material));
    CgsRasterizerStateFactory rasterFactory;rasterFactory.Construct(nullptr);
    CgsBlendStateFactory blendFactory;blendFactory.Construct(nullptr);
    for(int slot=0;slot<3;++slot){auto* state=CgsRasterizerStateFactory::GetState(slot);
        Check(state->muCullMode==u32(slot==0?2:slot==1?1:0)&&state->muDepthBias==0&&state->muSlopeScaleDepthBias==0,
              "original factory pins winding with zero depth biases");}
    RendererFixture renderer;renderer.Construct();
    Check(renderer.mShadowMapRenderManager.muWriteBufferIndex==3&&renderer.mShadowMapRenderManager.muReadBufferIndex==2
          &&renderer.mShadowMapRenderManager.mbForceFrontFaceCull,"real renderer call initializes the original manager defaults");
    for(int run=0;run<4;++run){
        forceFront=(run&1)==0;emptyList=run>=2?2:-1;
        renderer.mShadowMapRenderManager.mbForceFrontFaceCull=forceFront;
        lists.clear();begins.clear();ends.clear();const int before=nativeBinds;renderer.Render(nullptr);
        std::printf("force_front=%d empty_list=%d native_rasterizer_applications=%d\n",int(forceFront),emptyList,nativeBinds-before);
        Check(lists==std::vector<int>({0,2,1,3,4}),"production renderer visits every original caster list in order");
        Check(begins==std::vector<int>({0,1,2})&&ends==begins,"production renderer pairs every cascade begin and resolve");
        Check(!shadow::Device::mbRasteriserStateLocked,"pass leaves the rasterizer unlocked, including an empty locked list");
        material->mpRasterizerState=CgsRasterizerStateFactory::GetState(2);
        shadow::Device::SetMaterialRenderStatesPC(&technique,false);DrawAndCheck(0);
        Check(renderengine::resetEnabled&&renderengine::resetIndex==0xffff,"factory primitive-reset state survives the bracket");
    }
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(0));
    renderer.mShadowMapRenderManager.mbForceFrontFaceCull=true;
    renderer.mShadowMapRenderManager.BeginBackFaceCullRender();
    const int before=nativeBinds;shadow::Device::SetState(CgsRasterizerStateFactory::GetState(2));
    Check(nativeBinds==before,"direct state setter also respects the active lock");
    renderer.mShadowMapRenderManager.EndBackFaceCullRender();
    Check(shadow::Device::mpRasterizerState==CgsRasterizerStateFactory::GetState(0),"end restores the original BACK factory pointer after unlocking");
    DrawAndCheck(2);
    VirtualFree(material,0,MEM_RELEASE);readback->Release();target->Release();device->Release();api->Release();DestroyWindow(window);
    for(void* memory:allocations)std::free(memory);
    std::printf("PCShadowCull: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
