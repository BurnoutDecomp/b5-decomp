#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unordered_map>
#include "pc/gcm/renderengine/DepthOnly.h"
#include "pc/gcm/renderengine/ShaderBindings.h"
#include "pc/gcm/renderengine/ShaderConstantCache.h"
#include "pc/gcm/renderengine/ShadowReceiver.h"
#include "pc/gcm/renderengine/renderstates.h"
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "depth_technique.inc"

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement=1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1; }
static int checks, failures, blendBinds, samplerBinds, pixelConstants, fallbacks;
static void Check(bool ok, const char* text) { ++checks; if(!ok){++failures;std::printf("FAIL %s\n",text);} }
static std::vector<void*> allocated;
void* AllocateTestBlend(void* out, const void* descriptor)
{
    const auto* entries=static_cast<const renderengine::ResourceDescriptorEntry*>(descriptor);
    if(entries[0].muSize!=76||entries[0].muAlignment!=4)std::abort();
    void* memory=std::calloc(1,entries[0].muSize); allocated.push_back(memory);
    static_cast<void**>(out)[0]=memory;return out;
}
IDirect3DDevice9* gpD3DDevice=nullptr;
static IDirect3DDevice9* Dev(){return gpD3DDevice;}
static std::unordered_map<const void*,IDirect3DVertexShader9*> sVsCache;
static std::unordered_map<const void*,IDirect3DPixelShader9*> sPsCache;
static bool sbRealProgramsBound,sbRealVsHasWorld;
static const char* spCurrentTechniqueName;
static void *spTechniqueUnit0Texture,*spTechniqueFirstTexture;
static IDirect3DVertexShader9* spRealVs;
static IDirect3DPixelShader9* spRealPs;
static unsigned suRealVsInputMask;
static unsigned suBlendCensusWordMask;
static bool sbA2cRequested=false;
static int siA2cPath=0;
enum {E_A2C_PATH_NVIDIA=1};
static void AlphaCoverage_Reconcile(){} // Factory depth states disable A2C.
static unsigned VertexShaderInputMask(const void*){return 1;}
static bool VertexProgramHasWorldMatrix(const void*){return false;}
static void LogOnce(const char*,const char*){}
namespace CgsDev {
    struct QuietLog {template<class T> QuietLog& operator<<(const T&){return *this;}};
    namespace Message {static unsigned gxMessageFilterFlags=0;}
    namespace Log {static QuietLog stream;static QuietLog* gpDebugPrint=&stream;}
}
namespace renderengine {
    struct ProgramBufferData;
    namespace FrameProfile {enum {MESH_TECHNIQUE};struct DetailScope{explicit DetailScope(int){}};}
    enum {E_WORLD_FALLBACK_NO_PROGRAM,E_WORLD_FALLBACK_NOT_D3D9_BYTECODE,E_WORLD_FALLBACK_CREATE_FAILED};
    static void WorldShader_ReportFallback(const char*,int){++fallbacks;}
    static void WorldShader_ReportTechniqueHasNoPrograms(const char*){++fallbacks;}
    static void WorldShader_ClearRealPrograms(){sbRealProgramsBound=false;spRealPs=nullptr;spRealVs=nullptr;}
    static void WorldFallbackShader_Bind(){}
    static void WorldMaterialSamplers_Bind(const void*){}
}
void D3DDevice_SetPixelShader(IDirect3DDevice9*,void*);
void D3DDevice_SetRenderState_ColorWriteEnable(IDirect3DDevice9*,u32);
void D3DDevice_SetRenderState_AlphaTestEnable(IDirect3DDevice9*,u32);
void D3DDevice_SetRenderState_AlphaRef(IDirect3DDevice9*,u32);
void D3DDevice_SetRenderState_AlphaFunc(IDirect3DDevice9*,u32);
namespace shadow {
    // Unchanged constant/sampler transports are counted boundaries. The native
    // texture is bound by the fixture; the production caller decides whether to bind.
    static void DispatchExternalBlock(const u32*,void* const*,bool pixel){pixelConstants+=pixel;}
    static void DispatchInternalBlock(const u8*,u8,const u32*,bool pixel){pixelConstants+=pixel;}
    static void BindTechniqueSamplers(const u8*,const u8*){++samplerBinds;}
    static void LogFallbackTechniqueOnce(){}
    struct Device {
        inline static bool mbDepthStencilStateLocked=false,mbRasteriserStateLocked=false;
        inline static const renderengine::DepthStencilState* mpDepthStencilState=nullptr;
        inline static const renderengine::RasterizerState* mpRasterizerState=nullptr;
        inline static const renderengine::BlendMaterialState* mpBlendState=nullptr;
        inline static const renderengine::ProgramBufferData *mpVertexProgramShadow=nullptr,*mpPixelProgramShadow=nullptr;
        static void SetVertexProgramInternal(){}
        static bool SetVertexProgram(const renderengine::ProgramBufferData*);
        static bool SetPixelProgram(const renderengine::ProgramBufferData*);
        static void SetMaterialRenderStatesPC(const CgsGraphics::MaterialTechniqueView*,bool);
        static void SetMeshTechniquePC(const CgsGraphics::MaterialTechniqueView*,const void*,void* const*,bool);
        static void Xbox2SetDepthStencilStateLowLevelShadowed(renderengine::DepthStencilState*,bool){}
        static void Xbox2SetRasterizerStateLowLevelShadowed(renderengine::RasterizerState*,bool){}
        static void Xbox2SetStateLowLevelShadowed(renderengine::BlendMaterialState* state,bool)
        {
            ++blendBinds;if(!state)return;
            D3DDevice_SetRenderState_ColorWriteEnable(Dev(),state->maState[4]);
            D3DDevice_SetRenderState_AlphaFunc(Dev(),state->maState[15]);
            D3DDevice_SetRenderState_AlphaTestEnable(Dev(),state->maState[16]);
            D3DDevice_SetRenderState_AlphaRef(Dev(),state->maState[17]);
        }
    };
}
#include "depth_only.inc"

static ID3DBlob* Compile(const char* source,const char* entry,const char* target)
{
    ID3DBlob *code=nullptr,*error=nullptr;
    HRESULT hr=D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,entry,target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);
    if(FAILED(hr))std::printf("compile %08x %s\n",unsigned(hr),error?static_cast<const char*>(error->GetBufferPointer()):"");
    if(error)error->Release();return code;
}
static void* LowMemory()
{
    for(uintptr_t at=0x20000000;at<0x70000000;at+=0x1000000)
        if(void* p=VirtualAlloc(reinterpret_cast<void*>(at),65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE))return p;
    return nullptr;
}
static DWORD Pixel(IDirect3DSurface9* target,IDirect3DSurface9* copy,unsigned x=32)
{
    if(FAILED(Dev()->GetRenderTargetData(target,copy)))return 0xffffffffu;
    D3DLOCKED_RECT lock{};if(FAILED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return 0xffffffffu;
    DWORD colour;std::memcpy(&colour,static_cast<char*>(lock.pBits)+32*lock.Pitch+x*4,4);copy->UnlockRect();
    return colour|0xff000000u; // X8 is undefined; only RGB is part of this target's contract.
}
static HRESULT Quad(float z)
{
    struct Vertex{float x,y,z,u,v;};
    Vertex vertices[]={{-1,1,z,0,0},{1,1,z,1,0},{-1,-1,z,0,1},{1,-1,z,1,1}};
    return Dev()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));
}
int main()
{
    HWND window=CreateWindowA("STATIC","Native depth checks",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9Ex* api=nullptr;IDirect3DDevice9Ex* device=nullptr;
    D3DPRESENT_PARAMETERS p{};p.Windowed=TRUE;p.SwapEffect=D3DSWAPEFFECT_FLIPEX;p.BackBufferCount=2;
    p.BackBufferWidth=p.BackBufferHeight=64;p.BackBufferFormat=D3DFMT_X8R8G8B8;p.hDeviceWindow=window;p.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    if(FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION,&api))||FAILED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&p,nullptr,&device)))return 2;
    gpD3DDevice=device;
    IDirect3DSurface9 *target=nullptr,*depth=nullptr,*readback=nullptr;
    device->CreateRenderTarget(64,64,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr);
    device->CreateDepthStencilSurface(64,64,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&depth,nullptr);
    device->CreateOffscreenPlainSurface(64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr);
    if(!target||!depth||!readback)return 3;
    device->SetRenderTarget(0,target);device->SetDepthStencilSurface(depth);
    const char* source=R"(
struct In {float3 p:POSITION;float2 uv:TEXCOORD0;};
struct Out {float4 p:POSITION;float2 uv:TEXCOORD0;};
Out vs(In v){Out o;o.p=float4(v.p,1);o.uv=v.uv;return o;}
sampler2D image:register(s0);float4 colour:register(c0);
float4 cutout(Out v):COLOR{return tex2D(image,v.uv);}
float4 killed(Out v):COLOR{clip(tex2D(image,v.uv).a-1.1);return colour;}
float4 plain():COLOR{return colour;}
)";
    ID3DBlob *vs=Compile(source,"vs","vs_3_0"),*cutout=Compile(source,"cutout","ps_3_0"),
        *killed=Compile(source,"killed","ps_3_0"),*plain=Compile(source,"plain","ps_3_0");
    if(!vs||!cutout||!killed||!plain)return 4;
    auto* low=static_cast<u8*>(LowMemory());if(!low)return 5;
    std::memcpy(low+0x1000+20,vs->GetBufferPointer(),vs->GetBufferSize());
    std::memcpy(low+0x3000+20,killed->GetBufferPointer(),killed->GetBufferSize());
    auto* shader=reinterpret_cast<u32*>(low+0x5000);shader[0]=u32(uintptr_t(low+0x1000));shader[1]=u32(uintptr_t(low+0x3000));
    auto* material=reinterpret_cast<renderengine::MaterialState*>(low+0x6000);
    CgsBlendStateFactory factory;factory.Construct(nullptr);
    auto* opaque=CgsBlendStateFactory::GetState(7);auto* alpha=CgsBlendStateFactory::GetState(8);
    Check(opaque&&opaque->maState[4]==0&&opaque->maState[16]==0&&opaque->maState[18]==1,"real factory constructs opaque depth state");
    Check(alpha&&alpha->maState[4]==0&&alpha->maState[15]==4&&alpha->maState[16]==1&&alpha->maState[17]==128,"real factory constructs strict greater-than-128 alpha depth state");
    Check(opaque->maState[5]==15&&opaque->maState[6]==15&&opaque->maState[7]==15,"factory preserves the original unused MRT masks");
    material->mpBlendState=CgsBlendStateFactory::GetState(0);
    auto* technique=reinterpret_cast<CgsGraphics::MaterialTechniqueView*>(low+0x7000);
    technique->muShaderTechnique=u32(uintptr_t(shader));technique->muRenderStateGroup=u32(uintptr_t(material));
    void* scratch[8]{};
    D3DVERTEXELEMENT9 layout[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* declaration=nullptr;device->CreateVertexDeclaration(layout,&declaration);if(!declaration)return 6;
    renderengine::PCSetVertexDeclaration(device,declaration);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE,TRUE);device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    IDirect3DTexture9* texture=nullptr;device->CreateTexture(4,1,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr);
    if(!texture)return 7;D3DLOCKED_RECT pixels{};
    if(FAILED(texture->LockRect(0,&pixels,nullptr,D3DLOCK_DISCARD)))return 8;
    const DWORD texels[]={0x7fffffff,0x80ffffff,0x81ffffff,0xffffffff};std::memcpy(pixels.pBits,texels,sizeof(texels));texture->UnlockRect(0);
    device->SetTexture(0,texture);device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    const float green[]={0,1,0,1};device->SetPixelShaderConstantF(0,green,1);
    auto bind=[&](bool z){shadow::Device::SetMeshTechniquePC(technique,nullptr,scratch,z);};
    bind(true);
    Check(sbRealProgramsBound&&spRealPs==sPsCache[renderengine::DepthOnlyPC::KAU_PIXEL_CODE],"opaque technique binds and retains the minimal native pixel program");
    Check(shadow::Device::mpPixelProgramShadow==nullptr&&shadow::Device::mpBlendState==opaque,"logical depth program is null and cached blend is the original factory state");
    Check(samplerBinds==0&&pixelConstants==0,"opaque depth skips material samplers and both pixel constant blocks");
    int before=blendBinds;material->mpBlendState=CgsBlendStateFactory::GetState(1);bind(true);
    Check(blendBinds==before,"distinct opaque materials share the same cached depth blend");
    material->mpBlendState=CgsBlendStateFactory::GetState(0);
    IDirect3DPixelShader9* nativePlain=nullptr;device->CreatePixelShader(static_cast<DWORD*>(plain->GetBufferPointer()),&nativePlain);
    if(!nativePlain)return 9;
    // Match the production renderer's shared native-writer boundary for this
    // fixture pass; a raw setter would bypass the very shadow being tested.
    auto background=[&]{renderengine::PCSetPixelShader(device,nativePlain);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);return Quad(.75f);};
    device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff0000ff,1,0);device->BeginScene();
    HRESULT front=Quad(.25f);HRESULT back=background();device->EndScene();
    Check(SUCCEEDED(front)&&SUCCEEDED(back)&&Pixel(target,readback)==0xff0000ff,"opaque depth ignores a material shader that kills every pixel and blocks the background");
    float constant[4]{};device->GetPixelShaderConstantF(0,constant,1);
    Check(std::memcmp(green,constant,sizeof(green))==0,"shader-local DEF preserves device pixel constants");
    device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff0000ff,1,0);device->BeginScene();background();device->EndScene();
    Check(Pixel(target,readback)==0xff00ff00,"background oracle is visible without foreground depth and retains the green constant");
    // A different program allocation models another loaded technique; the real
    // shader cache is keyed by immutable payload addresses, not edited bytecode.
    std::memcpy(low+0x9000+20,cutout->GetBufferPointer(),cutout->GetBufferSize());shader[1]=u32(uintptr_t(low+0x9000));
    technique->mu16Flags=8;before=blendBinds;bind(true);
    Check(blendBinds==before+1&&shadow::Device::mpBlendState==alpha,"changing only the technique alpha flag selects a different depth state");
    Check(spRealPs==sPsCache[low+0x9000+20]&&samplerBinds==1,"alpha depth retains the material pixel shader and sampler binding");
    DWORD func=0,ref=0;device->GetRenderState(D3DRS_ALPHAFUNC,&func);device->GetRenderState(D3DRS_ALPHAREF,&ref);
    Check(func==D3DCMP_GREATER&&ref==128,"native alpha comparison preserves the original strict threshold");
    device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff0000ff,1,0);device->BeginScene();front=Quad(.25f);back=background();device->EndScene();
    Check(SUCCEEDED(front)&&SUCCEEDED(back),"native alpha depth and background draws succeed");
    Check(Pixel(target,readback,8)==0xff00ff00,"alpha 127 leaves the depth cutout open");
    Check(Pixel(target,readback,24)==0xff00ff00,"alpha 128 leaves the strict-threshold cutout open");
    Check(Pixel(target,readback,40)==0xff0000ff,"alpha 129 writes depth without writing colour");
    Check(Pixel(target,readback,56)==0xff0000ff,"alpha 255 writes depth without writing colour");
    Check(pixelConstants==0,"Z-only transitions never read missing pixel constant scratch blocks");
    bind(false);DWORD mask=0;device->GetRenderState(D3DRS_COLORWRITEENABLE,&mask);
    Check(mask==15&&shadow::Device::mpBlendState==material->mpBlendState&&spRealPs==sPsCache[low+0x9000+20],"colour pass restores material blend and pixel program");
    device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff0000ff,1,0);device->BeginScene();front=Quad(.25f);device->EndScene();
    Check(SUCCEEDED(front)&&Pixel(target,readback,8)==0xffffffffu&&Pixel(target,readback,56)==0xffffffffu,"restored colour pass writes the actual material texture across the whole quad");
    technique->mu16Flags=0;bind(true);auto* saved=spRealPs;renderengine::PCSetPixelShader(device,nativePlain);renderengine::PCSetPixelShader(device,spRealPs);
    IDirect3DPixelShader9* rebound=nullptr;device->GetPixelShader(&rebound);
    Check(rebound==saved&&rebound==sPsCache[renderengine::DepthOnlyPC::KAU_PIXEL_CODE],"the saved real-program pointer reasserts the minimal shader after an intervening pass");if(rebound)rebound->Release();
    const size_t cached=sPsCache.size();for(int i=0;i<50;++i)bind(true);
    Check(sPsCache.size()==cached,"repeated opaque depth binds reuse one native shader");
    shadow::Device::SetMeshTechniquePC(nullptr,nullptr,scratch,true);
    Check(!sbRealProgramsBound&&fallbacks==1,"missing technique preserves the existing fallback path without dereferencing it");
    Check(allocated.size()==9,"real factory allocated its full nine-state table once");
    ID3DBlob *vs2=Compile(source,"vs","vs_2_0"),*ps2=Compile(source,"plain","ps_2_0");
    if(!vs2||!ps2)return 10;
    bool legacy=renderengine::WorldPrograms_Bind(vs2->GetBufferPointer(),ps2->GetBufferPointer(),"SM2 fixture",true);
    Check(legacy&&spRealPs==sPsCache[ps2->GetBufferPointer()],"older valid shader models preserve their material pair instead of mixing VS2 with PS3");
    device->SetRenderState(D3DRS_COLORWRITEENABLE,15);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff0000ff,1,0);device->BeginScene();front=Quad(.25f);device->EndScene();
    Check(legacy&&SUCCEEDED(front)&&Pixel(target,readback)==0xff00ff00,"the retained VS2/PS2 pair executes on the real device and writes its expected colour");
    nativePlain->Release();texture->Release();declaration->Release();
    for(auto& item:sVsCache)item.second->Release();for(auto& item:sPsCache)if(item.second)item.second->Release();
    vs->Release();cutout->Release();killed->Release();plain->Release();
    vs2->Release();ps2->Release();
    readback->Release();depth->Release();target->Release();device->Release();api->Release();DestroyWindow(window);
    VirtualFree(low,0,MEM_RELEASE);for(void* memory:allocated)std::free(memory);
    std::printf("PCDepthOnly: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
