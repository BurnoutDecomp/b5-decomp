// The extracted four real bloom binds feed the unchanged production shaders.
// Texture wrapping and target geometry are fixtures; native sampler/pixels are real.
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
#include "pc/gcm/renderengine/renderstates.h"

static IDirect3DDevice9* spDevice;
static u32 suChecks, suFailures;
static bool gbPostFxSourceSamplerApplied;
static const u32 KU_RAW_DEPTH_MAX_SAMPLER_UNITS = 16;
static const u32 KU_POSTFX_SOURCE_FILTER_ANISO = 4;
static const u32 KU_POSTFX_SOURCE_FILTER_LINEAR = 1;
static const u32 KU_SAMPLER_SOURCE = 0;
static IDirect3DDevice9* Dev() { return spDevice; }
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
extern const u8 gauPostFxBloomDSPixelProgramPC[];
extern const u8 gauPostFxBloomBlurPixelProgramPC[];
extern const u8 gauPostFxBloomBlurOldPixelProgramPC[];
void PCSetSamplerState(IDirect3DDevice9* device, u32 unit, D3DSAMPLERSTATETYPE state, DWORD value)
{ device->SetSamplerState(unit, state, value); }
void PostFxBloomSampler_ApplyState(u32);
}
struct TargetFixture { struct { renderengine::TextureState* mpTextureState; } maColourTargets[1]; };
unsigned int D3DDevice_SetTexture(IDirect3DDevice9* device, u32 unit, void* texture, u32)
{ return device->SetTexture(unit, static_cast<IDirect3DBaseTexture9*>(texture)); }
#include "bloom_source_sampler.inc"

static void Check(bool good, const char* name)
{
    ++suChecks;
    if (!good) { ++suFailures; std::printf("FAIL %s\n", name); }
}
static void Require(HRESULT result, const char* name)
{
    if (FAILED(result)) { std::printf("GPU FAILURE %s hr=%08X\n", name, unsigned(result)); std::exit(2); }
}
static DWORD Sampler(D3DSAMPLERSTATETYPE type)
{
    DWORD value=0;
    Require(spDevice->GetSamplerState(0,type,&value),"sampler readback");
    return value;
}
static void PriorWorldSampler(bool linear)
{
    Require(spDevice->SetSamplerState(0,D3DSAMP_MINFILTER,linear?D3DTEXF_LINEAR:D3DTEXF_POINT),"prior min");
    Require(spDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,linear?D3DTEXF_LINEAR:D3DTEXF_POINT),"prior mag");
    Require(spDevice->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP),"prior U");
    Require(spDevice->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP),"prior V");
    Require(spDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR),"prior mip");
    Require(spDevice->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,4),"prior anisotropy");
    Require(spDevice->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,TRUE),"prior gamma");
}
struct Vertex { float x,y,z,rhw; float uv[3][4]; };
static DWORD DrawPixel(IDirect3DPixelShader9* shader, IDirect3DSurface9* target,
                       IDirect3DSurface9* read, float u, float invWidth, bool downsample)
{
    Vertex vertices[4]={};
    const float xy[4][2]={{-0.5f,-0.5f},{-0.5f,63.5f},{63.5f,-0.5f},{63.5f,63.5f}};
    for (u32 i=0;i<4;++i) {
        vertices[i].x=xy[i][0]; vertices[i].y=xy[i][1]; vertices[i].rhw=1;
        for (u32 j=0;j<3;++j) {
            vertices[i].uv[j][0]=vertices[i].uv[j][2]=u;
            vertices[i].uv[j][1]=vertices[i].uv[j][3]=0.5f;
        }
        if (downsample) {
            vertices[i].uv[0][0]-=invWidth; vertices[i].uv[0][2]+=invWidth;
            vertices[i].uv[1][0]-=invWidth; vertices[i].uv[1][2]+=invWidth;
        }
    }
    Require(spDevice->SetPixelShader(shader),"production bloom shader");
    Require(spDevice->BeginScene(),"begin");
    Require(spDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)),"draw");
    Require(spDevice->EndScene(),"end");
    Require(spDevice->GetRenderTargetData(target,read),"pixel readback");
    D3DLOCKED_RECT lock={};
    Require(read->LockRect(&lock,nullptr,D3DLOCK_READONLY),"pixel lock");
    DWORD pixel=0;
    std::memcpy(&pixel,static_cast<u8*>(lock.pBits)+32*lock.Pitch+32*4,4);
    Require(read->UnlockRect(),"pixel unlock");
    return pixel;
}
int main()
{
    HWND window=CreateWindowA("STATIC","bloom producer sampler regression",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);
    if (!window || !api) return 2;
    D3DPRESENT_PARAMETERS present={};
    present.Windowed=TRUE; present.hDeviceWindow=window; present.SwapEffect=D3DSWAPEFFECT_DISCARD;
    Require(api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,&present,&spDevice),"device");
    shadow::gpD3DDevice=spDevice;
    IDirect3DSurface9 *target=nullptr,*read=nullptr;
    Require(spDevice->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr),"target");
    Require(spDevice->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr),"readback");
    Require(spDevice->SetRenderTarget(0,target),"bind target");
    Require(spDevice->SetDepthStencilSurface(nullptr),"no depth");
    Require(spDevice->SetRenderState(D3DRS_ZENABLE,FALSE),"Z off");
    Require(spDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"cull none");
    Require(spDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"blend off");
    Require(spDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX3|D3DFVF_TEXCOORDSIZE4(0)|
        D3DFVF_TEXCOORDSIZE4(1)|D3DFVF_TEXCOORDSIZE4(2)),"declaration");
    const u8* programs[]={renderengine::gauPostFxBloomDSPixelProgramPC,
        renderengine::gauPostFxBloomBlurPixelProgramPC,renderengine::gauPostFxBloomBlurOldPixelProgramPC};
    IDirect3DPixelShader9* shaders[3]={};
    for (u32 i=0;i<3;++i)
        Require(spDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(programs[i]+20),&shaders[i]),"program");
    typedef void (*Bind)(TargetFixture*,TargetFixture*,TargetFixture*);
    const Bind binds[]={BindBloomSource0,BindBloomSource1,BindBloomSource2,BindBloomSource3};
    for (const u32 width : {1280u,2560u}) {
        const u32 height=width*9/16, edge=width/2;
        IDirect3DTexture9* texture=nullptr;
        Require(spDevice->CreateTexture(width,height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr),"scene fixture");
        D3DLOCKED_RECT lock={};
        Require(texture->LockRect(0,&lock,nullptr,0),"texture lock");
        for (u32 y=0;y<height;++y) {
            DWORD* row=reinterpret_cast<DWORD*>(static_cast<u8*>(lock.pBits)+y*lock.Pitch);
            for (u32 x=0;x<width;++x) row[x]=x<edge?0xFF000000:0xFFBFBFBF;
        }
        Require(texture->UnlockRect(0),"texture unlock");
        renderengine::TextureState state={};
        state.mpRaster=reinterpret_cast<renderengine::Texture*>(texture);
        TargetFixture source={{{&state}}};
        std::memset(shadow::Device::mapTextureState,0,sizeof(shadow::Device::mapTextureState));
        for (u32 pass=0;pass<4;++pass) {
            for (u32 prior=0;prior<2;++prior) {
                PriorWorldSampler(prior!=0);
                gbPostFxSourceSamplerApplied=false;
                binds[pass](&source,&source,&source);
                Check(Sampler(D3DSAMP_MINFILTER)==D3DTEXF_LINEAR && Sampler(D3DSAMP_MAGFILTER)==D3DTEXF_LINEAR,
                      "producer replaces prior filter with original LINEAR");
                Check(Sampler(D3DSAMP_ADDRESSU)==D3DTADDRESS_CLAMP && Sampler(D3DSAMP_ADDRESSV)==D3DTADDRESS_CLAMP,
                      "producer replaces prior addressing with original CLAMP");
                Check(Sampler(D3DSAMP_MIPFILTER)==D3DTEXF_NONE && Sampler(D3DSAMP_MAXANISOTROPY)==1 &&
                      Sampler(D3DSAMP_SRGBTEXTURE)==FALSE,"single-level colour sampler is deterministic");
                Check(!gbPostFxSourceSamplerApplied,"producer does not claim the composite diagnostic latch");
                Check(shadow::Device::mapTextureState[0]==&state && shadow::Device::mapSamplerTexture[0]==state.mpRaster,
                      "original TextureState and texture shadow keys retained");
                float constants[2][4]={};
                if (pass==0) {
                    constants[0][0]=constants[0][1]=constants[0][2]=1.f/3.f;
                    constants[1][0]=0.3f; constants[1][1]=0.25f/0.7f;
                } else if (pass>=2) constants[0][0]=1;
                Require(spDevice->SetPixelShaderConstantF(0,&constants[0][0],2),"constants");
                for (const float phase : {-0.75f,-0.25f,0.25f,0.75f}) {
                    const float u=(static_cast<float>(edge)+phase)/static_cast<float>(width);
                    const DWORD pixel=DrawPixel(shaders[pass==0?0:pass==1?1:2],target,read,u,1.f/width,pass==0);
                    const auto sample=[phase](float offset) {
                        return std::clamp(phase+offset+0.5f,0.f,1.f)*(191.f/255.f);
                    };
                    float expected=sample(0);
                    if (pass==0) {
                        const auto bright=[&sample](float offset) {
                            const float value=sample(offset);
                            return std::clamp((value-0.3f)*value,0.f,1.f);
                        };
                        expected=(2*bright(-1)+2*bright(1))*(0.25f/0.7f);
                    }
                    const int expectedByte=static_cast<int>(expected*255.f+0.5f);
                    const int actual=static_cast<int>(pixel&255u);
                    std::printf("size=%ux%u pass=%u prior=%u phase=%.2f pixel=%08X expected=%d\n",
                        width,height,pass,prior,phase,unsigned(pixel),expectedByte);
                    Check(std::abs(actual-expectedByte)<=2 && std::abs(static_cast<int>((pixel>>8)&255)-expectedByte)<=2 &&
                          std::abs(static_cast<int>((pixel>>16)&255)-expectedByte)<=2,
                          "moving bloom edge agrees with independent original bilinear bright/blur arithmetic");
                }
            }
        }
        Require(spDevice->SetTexture(0,nullptr),"retire fixture");
        texture->Release();
    }
    gbPostFxSourceSamplerApplied=false;
    renderengine::PostFxSourceSampler_ApplyState(0,1,1);
    Check(gbPostFxSourceSamplerApplied,"composite source keeps its own diagnostic latch");
    gbPostFxSourceSamplerApplied=false;
    renderengine::PostFxSourceSampler_ApplyState(1,1,1);
    Check(!gbPostFxSourceSamplerApplied,"composite auxiliary source does not claim its draw");
    for (auto* shader : shaders) shader->Release();
    read->Release(); target->Release(); spDevice->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCBloomSourceSampler: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
