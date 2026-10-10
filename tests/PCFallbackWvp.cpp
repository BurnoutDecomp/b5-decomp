#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "pc/gcm/renderengine/ShaderConstantCache.h"
#include "pc/gcm/renderengine/ShaderBindings.h"
#include "pc/gcm/renderengine/SamplerStateCache.h"
using u32=unsigned;using u64=unsigned long long;using f32=float;
extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement=1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1; }
static int checks,failures;
static void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
static IDirect3DDevice9* device;
static IDirect3DDevice9* Dev(){return device;}
static IDirect3DVertexShader9 *spFallbackVs,*spFallbackTexVs,*spRealVs,*spMeshVertexShader;
static IDirect3DPixelShader9 *spFallbackPs,*spFallbackTexPs,*spFallbackUvDebugPs,*spRealPs;
static IDirect3DBaseTexture9 *spTechniqueUnit0Texture,*spTechniqueFirstTexture;
static bool sbHaveLastWvp,sbForceFallbackNextMesh,sbRealVsHasWorld,sbRealProgramsBound;
static bool sbMaterialTextureBound,sbLastDeclHasTexcoord0;
static float safLastWvp[16];
static const u32 KU_FALLBACK_WVP_REGISTER=240;
static u64 suRealVsInputMask=1,suLastDeclUsageMask=1;
static u32 suLastDeclSourceStride=20;
static const char* spCurrentTechniqueName="native fixture";
static bool CompileFallbackShaders(IDirect3DDevice9*){return spFallbackVs&&spFallbackPs;}
static void LogOnce(const char*,const char*){}
namespace CgsDev {namespace Log {void WriteToLog(const char*){}}}
namespace renderengine {
    enum {E_WORLD_FALLBACK_INSTANCED_NO_WORLD=3,E_WORLD_FALLBACK_DECLARATION_SHORT=4};
    static void WorldShader_ReportFallback(const char*,int){}
}
#include "fallback_wvp.inc"

static ID3DBlob* Compile(const char* text,const char* target)
{
    ID3DBlob *code=nullptr,*error=nullptr;
    HRESULT hr=D3DCompile(text,std::strlen(text),nullptr,nullptr,nullptr,"main",target,0,0,&code,&error);
    if(FAILED(hr))std::printf("compile %08x %s\n",unsigned(hr),error?static_cast<const char*>(error->GetBufferPointer()):"");
    if(error)error->Release();return code;
}
static bool MakeVertex(const char* text,IDirect3DVertexShader9** shader)
{auto* code=Compile(text,"vs_3_0");if(!code)return false;HRESULT hr=device->CreateVertexShader(static_cast<DWORD*>(code->GetBufferPointer()),shader);code->Release();return SUCCEEDED(hr);}
static bool MakePixel(const char* text,IDirect3DPixelShader9** shader)
{auto* code=Compile(text,"ps_3_0");if(!code)return false;HRESULT hr=device->CreatePixelShader(static_cast<DWORD*>(code->GetBufferPointer()),shader);code->Release();return SUCCEEDED(hr);}
static void Matrix(float* values,float x)
{std::memset(values,0,16*sizeof(float));values[0]=values[5]=values[10]=values[15]=1;values[12]=x;}
static bool NativeMatrix(const float* expected)
{float actual[16]{};return SUCCEEDED(device->GetVertexShaderConstantF(240,actual,4))&&!std::memcmp(actual,expected,sizeof(actual));}
static HRESULT Draw()
{
    const float vertices[]={-.3f,-.3f,.5f,0,0, .3f,-.3f,.5f,1,0, 0,.3f,.5f,.5f,1};
    return device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,vertices,5*sizeof(float));
}
static DWORD Pixel(unsigned x)
{
    IDirect3DSurface9 *back=nullptr,*copy=nullptr;DWORD value=0xffffffff;
    if(SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))&&
       SUCCEEDED(device->CreateOffscreenPlainSurface(64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,nullptr))&&
       SUCCEEDED(device->GetRenderTargetData(back,copy))){
        D3DLOCKED_RECT lock{};
        if(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY))){
            std::memcpy(&value,static_cast<char*>(lock.pBits)+32*lock.Pitch+x*4,4);copy->UnlockRect();
        }
    }
    if(copy)copy->Release();if(back)back->Release();return value&0xffffff;
}
static bool DrawAt(unsigned expected,unsigned empty)
{
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device->BeginScene();HRESULT hr=Draw();device->EndScene();
    DWORD pixel=Pixel(expected);return SUCCEEDED(hr)&&pixel!=0&&pixel!=0xffffff&&Pixel(empty)==0;
}
int main()
{
    using namespace renderengine;
    HWND window=CreateWindowA("STATIC","Fallback WVP",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    auto* api=Direct3DCreate9(D3D_SDK_VERSION);D3DPRESENT_PARAMETERS pp{};
    pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    if(!api||FAILED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device)))return 2;
    if(!MakeVertex(KPC_FALLBACK_VS,&spFallbackVs)||!MakeVertex(KPC_FALLBACK_TEX_VS,&spFallbackTexVs)
       ||!MakePixel(KPC_FALLBACK_PS,&spFallbackPs)||!MakePixel(KPC_FALLBACK_TEX_PS,&spFallbackTexPs)
       ||!MakeVertex("float4 main(float4 pos:POSITION):POSITION{return float4(pos.xyz,1);}",&spRealVs)
       ||!MakePixel("float4 main():COLOR{return float4(0,1,0,1);}",&spRealPs))return 3;
    D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* declaration=nullptr;if(FAILED(device->CreateVertexDeclaration(elements,&declaration)))return 4;
    device->SetVertexDeclaration(declaration);device->SetRenderState(D3DRS_ZENABLE,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    float left[16],right[16],outside[16];Matrix(left,-.5f);Matrix(right,.5f);Matrix(outside,8);
    PCSetVertexShaderConstantF(device,240,outside,4);
    const auto initial=gPCShaderConstantCache.mStatistics.muRequests;
    Check(WorldFallbackShader_Bind()&&gPCShaderConstantCache.mStatistics.muRequests==initial,"binding without a published matrix does not invent a constant value");
    sbRealProgramsBound=true;sbRealVsHasWorld=true;WorldFallbackShader_SelectForMesh();
    WorldFallbackShader_SetWvp(left);
    Check(gPCShaderConstantCache.mStatistics.muRequests==initial&&NativeMatrix(outside),"real meshes retain CPU WVP without an unused native upload");
    Check(sbHaveLastWvp&&!std::memcmp(safLastWvp,left,sizeof(left)),"latest CPU matrix remains available to late fallback and diagnostics");
    Check(DrawAt(32,16),"real shader geometry is unaffected by the deferred fallback matrix");
    Check(WorldFallbackShader_Bind()&&NativeMatrix(left),"explicit fallback bind publishes the latest deferred matrix");
    Check(DrawAt(16,48),"production flat fallback shader draws at the deferred left position");
    WorldFallbackShader_SetWvp(right);
    Check(NativeMatrix(right)&&DrawAt(48,16),"matrix changes while fallback is active update its native draw immediately");
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device->BeginScene();
    WorldFallbackShader_SetWvp(left);HRESULT first=Draw();WorldFallbackShader_SetWvp(right);HRESULT second=Draw();device->EndScene();
    const DWORD leftPixel=Pixel(16),rightPixel=Pixel(48);
    Check(SUCCEEDED(first)&&SUCCEEDED(second)&&leftPixel!=0&&leftPixel!=0xffffff&&rightPixel!=0&&rightPixel!=0xffffff,
          "scalar instances retain distinct transforms without reselecting the shader");
    WorldFallbackShader_SelectForMesh();const auto before=gPCShaderConstantCache.mStatistics.muRequests;
    WorldFallbackShader_SetWvp(left);
    Check(gPCShaderConstantCache.mStatistics.muRequests==before,"returning to a real technique stops unnecessary matrix submissions");
    suLastDeclUsageMask=0;WorldFallbackShader_SelectForMesh();
    Check(spMeshVertexShader==spFallbackVs&&NativeMatrix(left)&&DrawAt(16,48),"a late declaration fallback restores the current matrix before drawing");
    suLastDeclUsageMask=1;WorldFallbackShader_SelectForMesh();WorldFallbackShader_SetWvp(right);
    IDirect3DTexture9* texture=nullptr;device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr);
    if(!texture)return 5;D3DLOCKED_RECT lock{};texture->LockRect(0,&lock,nullptr,0);*static_cast<DWORD*>(lock.pBits)=0xffff0000;texture->UnlockRect(0);
    spTechniqueFirstTexture=texture;sbLastDeclHasTexcoord0=true;suLastDeclUsageMask=0;WorldFallbackShader_SelectForMesh();
    Check(spMeshVertexShader==spFallbackTexVs&&NativeMatrix(right)&&DrawAt(48,16),"textured fallback also publishes the current matrix and retains its diffuse sampler");
    WorldFallbackShader_SetWvp(left);
    Check(NativeMatrix(left)&&DrawAt(16,48),"active textured fallback follows the next object's transform");
    suLastDeclUsageMask=1;sbRealVsHasWorld=false;sbForceFallbackNextMesh=true;WorldFallbackShader_SetWvp(right);WorldFallbackShader_SelectForMesh();
    Check(spMeshVertexShader==spFallbackTexVs&&NativeMatrix(right),"console instancing without a world uniform retains the fallback transform");
    auto* saved=device;device=nullptr;WorldFallbackShader_SetWvp(left);
    Check(!WorldFallbackShader_Bind()&&sbHaveLastWvp,"a missing device retains the CPU matrix without attempting a native bind");
    device=saved;gPCShaderConstantCache.Invalidate();
    Check(WorldFallbackShader_Bind()&&NativeMatrix(left),"device recovery and cache invalidation republish the retained matrix");
    PCSetVertexShaderConstantF(device,240,outside,4);WorldFallbackShader_ApplyWvp();
    Check(NativeMatrix(left),"explicit diagnostic materialization restores the reserved registers after an intervening write");
    sbRealVsHasWorld=true;WorldFallbackShader_SelectForMesh();
    Check(spMeshVertexShader==spRealVs&&DrawAt(32,16),"real geometry returns correctly after all fallback transitions");
    texture->Release();declaration->Release();spRealVs->Release();spRealPs->Release();
    spFallbackVs->Release();spFallbackPs->Release();spFallbackTexVs->Release();spFallbackTexPs->Release();
    gPCShaderConstantCache.Invalidate();gPCSamplerStateCache.Invalidate();device->Release();api->Release();DestroyWindow(window);
    std::printf("PCFallbackWvp: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
