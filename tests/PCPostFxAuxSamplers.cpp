// The actual production auxiliary bind block and shadow/no-op leaf run here.
// Geometry, scene/depth textures and the native texture-wrapper seam are fixtures;
// both composite pixel programs and every observed native sampler/pixel are real.
#include <Windows.h>
#include <d3d9.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"

static IDirect3DDevice9* spDevice;
static u32 suChecks, suFailures;
static bool gbPostFxSourceSamplerApplied;
static const u32 KU_RAW_DEPTH_MAX_SAMPLER_UNITS = 16;
static const u32 KU_POSTFX_SOURCE_FILTER_ANISO = 4;
static const u32 KU_POSTFX_SOURCE_FILTER_LINEAR = 1;
static const u32 KU_SAMPLER_FILTER_LINEAR = 1;
static const u32 KU_SAMPLER_BLOOM = 1;
static const u32 KU_SAMPLER_DOF = 2;
static u8 saLinearSampler[32];
static void* mpSamplerState_Linear = saLinearSampler;
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++suFailures; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace shadow {
IDirect3DDevice9* gpD3DDevice;
void* Device::mapSamplerState[KU_MAX_TEXTURE_STATES] = {};
void* Device::mapSamplerTexture[KU_MAX_TEXTURE_STATES] = {};
const renderengine::TextureState* Device::mapTextureState[KU_MAX_TEXTURE_STATES] = {};
}
namespace renderengine {
class Texture;
extern const u8 gauPostFxCompositePixelProgramPC[];
extern const u8 gauPostFxCompositeDofPixelProgramPC[];
void PCSetSamplerState(IDirect3DDevice9* device, u32 unit, D3DSAMPLERSTATETYPE state, DWORD value)
{ device->SetSamplerState(unit, state, value); }
void PostFxSourceSampler_ApplyState(u32, u32, u32);
}
unsigned int D3DDevice_SetTexture(IDirect3DDevice9* device, u32 unit, void* texture, u32)
{
    // Texture-wrapper boundary only: the extracted native sampler bind is unchanged.
    return device->SetTexture(unit, static_cast<IDirect3DBaseTexture9*>(texture));
}
#include "postfx_aux_samplers.inc"
void renderengine::PostFxSourceSampler_ApplyState(u32 unit, u32 filter, u32 maxAnisotropy)
{ ApplyPostFxSourceSamplerState(spDevice, unit, filter, maxAnisotropy); }

static void Check(bool value, const char* name)
{
    ++suChecks;
    if (!value) { ++suFailures; std::printf("FAIL %s\n", name); }
}
static void Require(HRESULT result, const char* name)
{
    if (FAILED(result)) {
        std::printf("GPU SETUP/DRAW FAILURE %s hr=%08X\n", name, unsigned(result));
        std::exit(2);
    }
}
static DWORD Sampler(u32 unit, D3DSAMPLERSTATETYPE state)
{
    DWORD value = 0;
    Require(spDevice->GetSamplerState(unit, state, &value), "sampler readback");
    return value;
}
static IDirect3DTexture9* Texture(DWORD left, DWORD right)
{
    IDirect3DTexture9* texture = nullptr;
    Require(spDevice->CreateTexture(2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                    &texture, nullptr), "texture");
    D3DLOCKED_RECT lock = {};
    Require(texture->LockRect(0, &lock, nullptr, 0), "texture lock");
    for (u32 y=0; y<2; ++y) {
        DWORD* row = reinterpret_cast<DWORD*>(static_cast<u8*>(lock.pBits)+y*lock.Pitch);
        row[0]=left; row[1]=right;
    }
    Require(texture->UnlockRect(0), "texture unlock");
    return texture;
}
static void PriorWorldSampler(u32 unit, bool linear)
{
    Require(spDevice->SetSamplerState(unit, D3DSAMP_MINFILTER, linear ? D3DTEXF_LINEAR : D3DTEXF_POINT), "prior min");
    Require(spDevice->SetSamplerState(unit, D3DSAMP_MAGFILTER, linear ? D3DTEXF_LINEAR : D3DTEXF_POINT), "prior mag");
    Require(spDevice->SetSamplerState(unit, D3DSAMP_MIPFILTER, D3DTEXF_NONE), "prior mip");
    Require(spDevice->SetSamplerState(unit, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP), "prior addressU");
    Require(spDevice->SetSamplerState(unit, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP), "prior addressV");
}
static void VerifyAuxiliarySampler(u32 unit)
{
    Check(Sampler(unit,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR &&
          Sampler(unit,D3DSAMP_MAGFILTER)==D3DTEXF_LINEAR, "auxiliary render target uses original bilinear filter");
    Check(Sampler(unit,D3DSAMP_ADDRESSU)==D3DTADDRESS_CLAMP &&
          Sampler(unit,D3DSAMP_ADDRESSV)==D3DTADDRESS_CLAMP, "auxiliary render target clamps original edge samples");
    Check(shadow::Device::mapSamplerState[unit]==mpSamplerState_Linear, "original sampler shadow key retained");
}
struct Vertex { float x,y,z,rhw,u,v,s,t; };
static DWORD DrawPixel(IDirect3DPixelShader9* shader, IDirect3DSurface9* target,
                       IDirect3DSurface9* read, float u)
{
    const Vertex vertices[] = {
        {-0.5f,-0.5f,0,1,u,0.5f,0,0}, {-0.5f,63.5f,0,1,u,0.5f,0,0},
        {63.5f,-0.5f,0,1,u,0.5f,0,0}, {63.5f,63.5f,0,1,u,0.5f,0,0} };
    Require(spDevice->SetPixelShader(shader), "composite program");
    Require(spDevice->BeginScene(), "begin composite");
    Require(spDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)), "composite draw");
    Require(spDevice->EndScene(), "end composite");
    Require(spDevice->GetRenderTargetData(target,read), "pixels");
    D3DLOCKED_RECT lock = {};
    Require(read->LockRect(&lock,nullptr,D3DLOCK_READONLY), "pixel lock");
    DWORD colour=0;
    std::memcpy(&colour,static_cast<u8*>(lock.pBits)+32*lock.Pitch+32*4,4);
    Require(read->UnlockRect(), "pixel unlock");
    return colour;
}
int main()
{
    HWND window=CreateWindowA("STATIC","post-fx auxiliary sampler regression",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);
    if (!window || !api) return 2;
    D3DPRESENT_PARAMETERS present={};
    present.Windowed=TRUE; present.hDeviceWindow=window; present.SwapEffect=D3DSWAPEFFECT_DISCARD;
    Require(api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,&present,&spDevice), "device");
    shadow::gpD3DDevice=spDevice;
    IDirect3DSurface9 *target=nullptr,*read=nullptr;
    Require(spDevice->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr), "target");
    Require(spDevice->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr), "readback surface");
    Require(spDevice->SetRenderTarget(0,target), "bind target");
    Require(spDevice->SetDepthStencilSurface(nullptr), "no depth attachment");
    Require(spDevice->SetRenderState(D3DRS_ZENABLE,FALSE), "depth off");
    Require(spDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE), "cull none");
    Require(spDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE), "blend off");
    Require(spDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE4(0)), "quad declaration");
    IDirect3DPixelShader9 *bloomShader=nullptr,*dofShader=nullptr;
    // ProgramBuffer's native package has a20-byte header before D3D9 bytecode.
    Require(spDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(renderengine::gauPostFxCompositePixelProgramPC+20),&bloomShader), "production bloom composite");
    Require(spDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(renderengine::gauPostFxCompositeDofPixelProgramPC+20),&dofShader), "production DoF composite");
    IDirect3DTexture9* black=Texture(0xFF000000,0xFF000000);
    IDirect3DTexture9* edge=Texture(0xFF000000,0xFFFFFFFF);
    IDirect3DTexture9* farDepth=Texture(0xFFFFFFFF,0xFFFFFFFF);
    Require(spDevice->SetTexture(0,black), "source black");
    Require(spDevice->SetTexture(4,farDepth), "source depth");
    PriorWorldSampler(0,false); PriorWorldSampler(4,false);
    for (int cacheHit=0; cacheHit<2; ++cacheHit) {
        // Second iteration modifies real device state while keeping the same engine
        // sampler shadow: SetState's actual skip behavior must not mask this test.
        PriorWorldSampler(1,cacheHit!=0); PriorWorldSampler(2,cacheHit!=0);
        BindAuxiliarySamplers(reinterpret_cast<renderengine::Texture*>(edge),
                              reinterpret_cast<renderengine::Texture*>(edge));
        VerifyAuxiliarySampler(1); VerifyAuxiliarySampler(2);
        Check(Sampler(0,D3DSAMP_MINFILTER)==D3DTEXF_POINT &&
              Sampler(0,D3DSAMP_ADDRESSU)==D3DTADDRESS_WRAP, "auxiliary bind leaves source sampler untouched");
        for (int dof=0; dof<2; ++dof) {
            float constants[7][4]={};
            constants[0][0]=1;
            if (!dof) {
                constants[1][0]=constants[1][1]=constants[1][2]=1;
                for (int i=0;i<3;++i) constants[2][i]=constants[3][i]=1;
            } else {
                constants[1][2]=0.5f; constants[1][3]=0.75f;
                constants[2][0]=1; constants[2][2]=4;
                for (int i=0;i<3;++i) constants[4][i]=constants[5][i]=1;
            }
            Require(spDevice->SetPixelShaderConstantF(0,&constants[0][0],7), "production composite constants");
            for (float u : {-0.1f,0.25f,0.5f,0.75f,1.1f}) {
                const DWORD colour=DrawPixel(dof?dofShader:bloomShader,target,read,u);
                const int expected=static_cast<int>(std::clamp(2*u-0.5f,0.f,1.f)*255.f+0.5f);
                const int actual=static_cast<int>(colour&255);
                std::printf("unit%u cacheHit=%d u=%.2f pixel=%08X expected=%d\n", dof?2u:1u,cacheHit,u,unsigned(colour),expected);
                Check(std::abs(actual-expected)<=2 && std::abs(static_cast<int>((colour>>8)&255)-expected)<=2 &&
                      std::abs(static_cast<int>((colour>>16)&255)-expected)<=2,
                      dof?"production DoF sample is bilinear and clamped":"production bloom sample is bilinear and clamped");
            }
        }
    }
    farDepth->Release(); edge->Release(); black->Release();
    dofShader->Release(); bloomShader->Release(); read->Release(); target->Release();
    spDevice->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCPostFxAuxSamplers: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
