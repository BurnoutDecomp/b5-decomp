// Unchanged native particle writer + actual Spark/Simple stream-bind blocks.
// The fixture supplies the stream lifetime/declaration and transform only;
// shader bytes, native draw helpers and packed producer bytes are production.
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "types.hpp"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/GeometryBindings.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"
#include "GameSource/Effects/Particles/Native/BrnNativeParticleVertex.h"

static unsigned suChecks,suFailures,suStride;
static IDirect3DDevice9* gpD3DDevice;
static void Check(bool good,const char* label)
{
    ++suChecks;
    if(!good){++suFailures;std::printf("FAIL %s\n",label);}
}
namespace CgsDev::Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++suFailures;return 0;}
void* EndAssert(){return nullptr;}
}
namespace renderengine {
extern const u8 gauIm3dVertexProgramPC[],gauIm3dPixelProgramPC[];
}
using BrnParticle::NativeParticleVertex;
struct NativeBuffer {const u8* mpBytes;};
// The platform's stream-set boundary is observed, not replaced with a constant.
static void D3DDevice_SetStreamSource(IDirect3DDevice9*,u32,NativeBuffer*,u32,u32 stride,u32)
{suStride=stride;}
#include "native_particle_stride.inc"

static void Require(HRESULT hr,const char* label)
{
    if(FAILED(hr)){std::printf("NATIVE FAILURE %s %08X\n",label,unsigned(hr));std::exit(2);}
}
static std::vector<DWORD> Draw(IDirect3DSurface9* target,IDirect3DSurface9* read,
    const u8* stream,u32 stride,bool spark)
{
    const u16 indices[]={0,1,2,0,2,3};
    Require(gpD3DDevice->SetRenderTarget(0,target),"target");
    Require(gpD3DDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xFF091119,1,0),"clear");
    Require(gpD3DDevice->BeginScene(),"begin");
    if(spark)Require(renderengine::GeometryBindingsPC::DrawPrimitiveUP(
        gpD3DDevice,D3DPT_TRIANGLESTRIP,2,stream,stride),"real spark native consumer");
    else Require(renderengine::GeometryBindingsPC::DrawIndexedPrimitiveUP(
        gpD3DDevice,D3DPT_TRIANGLELIST,0,4,2,indices,D3DFMT_INDEX16,stream,stride),
        "real simple quad native consumer");
    Require(gpD3DDevice->EndScene(),"end");
    Require(gpD3DDevice->GetRenderTargetData(target,read),"pixels");
    D3DLOCKED_RECT lock={};Require(read->LockRect(&lock,nullptr,D3DLOCK_READONLY),"read lock");
    std::vector<DWORD> pixels(96*64);
    for(u32 y=0;y<64;++y)std::memcpy(pixels.data()+96*y,
        static_cast<const u8*>(lock.pBits)+lock.Pitch*y,96*4);
    Require(read->UnlockRect(),"unlock");return pixels;
}
int main()
{
    Check(sizeof(CgsGraphics::BasicColouredTexturedVertex)==32,"canonical CPU ABI remains32");
    Check(NativeParticleVertex::GetStride()==24,"canonical GPU writer stride24");
    alignas(16) u8 allocation[1024]={};u8* bytes=allocation+16;
    NativeParticleVertex::VertexIterator writer;
    writer.SetBaseAddress(bytes);writer.SetCurrentAddress(bytes-4);
    writer.SetTopAddress(bytes+960);writer.SetStride(NativeParticleVertex::GetStride());
    const rw::math::vpu::Vector4 positions[8]={
        {-.7f,-.5f,.3f,0},{-.7f,.5f,.3f,0},{.7f,.5f,.3f,0},{.7f,-.5f,.3f,0},
        {-.3f,-.7f,.6f,0},{-.3f,.7f,.6f,0},{.3f,.7f,.6f,0},{.3f,-.7f,.6f,0}};
    const u32 colours[8]={0xFF2040C0,0xFF2040C0,0xFF2040C0,0xFF2040C0,
        0xFFB07030,0xFFB07030,0xFFB07030,0xFFB07030};
    const float uv[4][2]={{0,0},{0,1},{1,1},{1,0}};
    for(u32 i=0;i<8;++i)writer.Write(positions[i],reinterpret_cast<const int*>(&colours[i]),uv[i%4]);
    Check(writer.GetCurrentAddress()==bytes+8*24-4,"real writer advances exactly192bytes");
    NativeBuffer buffer={bytes};u32 strides[2];
    BindSpark(&buffer);strides[0]=suStride;
    BindSimple(&buffer);strides[1]=suStride;
    for(u32 consumer=0;consumer<2;++consumer)
    {
        std::printf("%s producer24 consumer%u\n",consumer==0?"spark":"simple",strides[consumer]);
        Check(strides[consumer]==NativeParticleVertex::GetStride(),"selected stride agrees with real writer");
        for(u32 i=0;i<8;++i)
        {
            const u8* vertex=bytes+i*strides[consumer];
            Check(!std::memcmp(vertex,&positions[i],12),"native position consumed from matching vertex");
            Check(!std::memcmp(vertex+12,&colours[i],4),"native colour consumed from matching vertex");
            Check(!std::memcmp(vertex+16,uv[i%4],8),"native atlas UV consumed from matching vertex");
        }
    }
#if !NATIVE_STRIDE_CPU_ONLY
    HWND window=CreateWindowExA(0,"STATIC","native particle stride",WS_POPUP,0,0,96,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api)return 2;
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=96;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    Require(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,&gpD3DDevice),"device");
    IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
    Require(gpD3DDevice->CreateVertexShader(reinterpret_cast<const DWORD*>(renderengine::gauIm3dVertexProgramPC+20),&vs),"real particle-compatible VS");
    Require(gpD3DDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(renderengine::gauIm3dPixelProgramPC+20),&ps),"real particle-compatible PS");
    D3DVERTEXELEMENT9 elements[]={
        {0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_UBYTE4N,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0},
        {0,16,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* decl=nullptr;Require(gpD3DDevice->CreateVertexDeclaration(elements,&decl),"canonical native24 declaration");
    Require(gpD3DDevice->SetVertexDeclaration(decl),"declaration");Require(gpD3DDevice->SetVertexShader(vs),"vs");Require(gpD3DDevice->SetPixelShader(ps),"ps");
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};Require(gpD3DDevice->SetVertexShaderConstantF(0,identity,4),"projection");
    gpD3DDevice->SetRenderState(D3DRS_ZENABLE,FALSE);gpD3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);gpD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    IDirect3DTexture9* white=nullptr;Require(gpD3DDevice->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&white,nullptr),"white");
    D3DLOCKED_RECT wl={};Require(white->LockRect(0,&wl,nullptr,0),"white lock");*static_cast<DWORD*>(wl.pBits)=0xFFFFFFFF;Require(white->UnlockRect(0),"white unlock");Require(gpD3DDevice->SetTexture(0,white),"white bind");
    gpD3DDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);gpD3DDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    IDirect3DSurface9* target=nullptr;IDirect3DSurface9* read=nullptr;
    Require(gpD3DDevice->CreateRenderTarget(96,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr),"render target");
    Require(gpD3DDevice->CreateOffscreenPlainSurface(96,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr),"read surface");
    for(u32 consumer=0;consumer<2;++consumer)for(u32 first:{0u,4u})
    {
        auto expected=Draw(target,read,bytes+first*24,24,consumer==0);
        auto actual=Draw(target,read,bytes+first*strides[consumer],strides[consumer],consumer==0);
        u32 different=0,covered=0;for(u32 i=0;i<expected.size();++i){different+=actual[i]!=expected[i];covered+=expected[i]!=0xFF091119;}
        std::printf("native %s first%u coverage%u differingPixels%u\n",consumer==0?"spark":"simple",first,covered,different);
        Check(covered>500 && different==0,"native rendered coverage/colour matches packed producer and original stride");
    }
    read->Release();target->Release();white->Release();decl->Release();vs->Release();ps->Release();gpD3DDevice->Release();api->Release();DestroyWindow(window);
#endif
    std::printf("PCNativeParticleStride: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
