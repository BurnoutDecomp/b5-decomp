#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "pc/gcm/renderengine/ShadowQuality.h"
#include "pc/gcm/renderengine/ShadowReceiver.h"
#include "pc_shadow_quality_program.inc"

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement=1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1; }
static IDirect3DDevice9* spDevice;
static bool sbShadowPassActive=false;
static u32 suRasterSlopeBiasBasePC=0;
static unsigned checks=0,failures=0;
static IDirect3DDevice9* Dev() {return spDevice;}
namespace renderengine {
bool ShadowDepthFormatIsHardwareCompare() {return true;}
void PCSetSamplerState(IDirect3DDevice9* d,u32 u,D3DSAMPLERSTATETYPE s,DWORD v) {d->SetSamplerState(u,s,v);}
}
#include "pc_shadow_quality.inc"
static void Check(bool ok,const char* name) {++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
static void Require(HRESULT h,const char* name) {if(FAILED(h)){std::printf("GPU FAILURE %s %08x\n",name,unsigned(h));std::exit(2);}}
static DWORD Bits(float v) {DWORD b;std::memcpy(&b,&v,4);return b;}
struct Vertex {float x,y,z,rhw,u,v;};
static void Quad(float x0,float y0,float x1,float y1,float z0,float z1)
{
    const Vertex v[]={{x0,y0,z0,1,0,0},{x1,y0,z1,1,1,0},
                      {x0,y1,z0,1,0,1},{x1,y1,z1,1,1,1}};
    Require(spDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex)),"quad");
}
struct Samples {unsigned mean,dark,minimum,maximum,centre;};
static Samples Read(IDirect3DSurface9* target,IDirect3DSurface9* read)
{
    Require(spDevice->GetRenderTargetData(target,read),"readback");
    D3DLOCKED_RECT lock={};Require(read->LockRect(&lock,nullptr,D3DLOCK_READONLY),"lock");
    unsigned sum=0,dark=0,minimum=255,maximum=0,centre=0;
    for(unsigned x=16;x<240;++x){const DWORD p=reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(lock.pBits)+32*lock.Pitch)[x];
        const unsigned r=(p>>16)&255;sum+=r;dark+=r<250;
        if(r<minimum)minimum=r;if(r>maximum)maximum=r;if(x==128)centre=r;}
    Require(read->UnlockRect(),"unlock");return {sum/224,dark,minimum,maximum,centre};
}
int main()
{
    HWND window=CreateWindowA("STATIC","shadow quality",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);if(!window||!api)return 2;
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    Require(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&spDevice),"device");
    D3DCAPS9 caps={};Require(spDevice->GetDeviceCaps(&caps),"caps");
    const auto original=renderengine::ChooseShadowAtlasSizePC(spDevice,1);
    Check(original.muWidth==1280 && original.muHeight==1920 && original.muScale==1,"original atlas dimensions");
    const auto requested=renderengine::ChooseShadowAtlasSizePC(spDevice,2);
    const unsigned effective=caps.MaxTextureWidth>=2560 && caps.MaxTextureHeight>=3840?2:1;
    Check(requested.muScale==effective && requested.muWidth==1280*effective && requested.muHeight==1920*effective,"resolution respects actual device caps");
    Check(renderengine::ChooseShadowAtlasSizePC(nullptr,2).muScale==1,"no device retains original dimensions");
    Check(renderengine::ChooseShadowAtlasSizePC(spDevice,3).muScale==1,"invalid scale retains original dimensions");
    IDirect3DPixelShader9* shader=nullptr;Require(spDevice->CreatePixelShader(kauShadowQualityProgram,&shader),"comparison shader");
    IDirect3DSurface9 *target=nullptr,*read=nullptr;
    Require(spDevice->CreateRenderTarget(256,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr),"receiver target");
    Require(spDevice->CreateOffscreenPlainSurface(256,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr),"receiver readback");
    for(unsigned scale=1;scale<=effective;++scale)
    {
        const auto size=renderengine::ChooseShadowAtlasSizePC(spDevice,scale);
        IDirect3DTexture9* depth=nullptr;IDirect3DSurface9 *surface=nullptr,*colour=nullptr;
        Require(spDevice->CreateTexture(size.muWidth,size.muHeight,1,D3DUSAGE_DEPTHSTENCIL,D3DFMT_D24X8,D3DPOOL_DEFAULT,&depth,nullptr),"hardware comparison atlas");
        Require(depth->GetSurfaceLevel(0,&surface),"atlas surface");
        Require(spDevice->CreateRenderTarget(size.muWidth,size.muHeight,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&colour,nullptr),"caster target");
        for(unsigned mode=0;mode<2;++mode)
        {
            renderengine::GetGraphicsSettingsPC().mfShadowSlopeBias=float(mode);
            Require(spDevice->SetTexture(15,nullptr),"unbind comparison texture");
            Require(spDevice->SetRenderTarget(0,colour),"bind caster target");
            Require(spDevice->SetDepthStencilSurface(surface),"bind atlas");
            D3DVIEWPORT9 vp={0,0,size.muWidth,size.muHeight,0,1};Require(spDevice->SetViewport(&vp),"atlas viewport");
            Require(spDevice->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,1,0),"clear atlas");
            spDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);spDevice->SetVertexShader(nullptr);spDevice->SetPixelShader(nullptr);
            spDevice->SetRenderState(D3DRS_ZENABLE,TRUE);spDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
            spDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);spDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
            spDevice->SetRenderState(D3DRS_COLORWRITEENABLE,0);spDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
            D3DDevice_SetRenderState_SlopeScaleDepthBias(spDevice,0);
            renderengine::BeginShadowBiasTest();
            Require(spDevice->BeginScene(),"begin caster");
            Quad(-.5f,-.5f,float(size.muWidth)-.5f,float(size.muHeight/3)-.5f,.4f,.6f);
            Require(spDevice->EndScene(),"end caster");
            renderengine::EndShadowBiasTest();
            DWORD restored=~0u;spDevice->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,&restored);
            Check(restored==0,"caster slope compensation does not leak past the pass bracket");
            Require(spDevice->SetDepthStencilSurface(nullptr),"unbind atlas surface");
            Require(spDevice->SetRenderTarget(0,target),"receiver target bind");
            vp={0,0,256,64,0,1};spDevice->SetViewport(&vp);
            spDevice->SetRenderState(D3DRS_ZENABLE,FALSE);spDevice->SetRenderState(D3DRS_COLORWRITEENABLE,15);
            Require(spDevice->SetTexture(15,depth),"bind shadow atlas");renderengine::ShadowSampler_ApplyState(15);
            spDevice->SetPixelShader(shader);
            const float params[]={.4f,.2f,1.0f/6.0f,0};spDevice->SetPixelShaderConstantF(0,params,1);
            Require(spDevice->BeginScene(),"begin receiver");Quad(-.5f,-.5f,255.5f,63.5f,0,0);Require(spDevice->EndScene(),"end receiver");
            const auto samples=Read(target,read);
            std::printf("scale=%u slope=%u selfReceiverMean=%u bandedPixels=%u/224 range=%u..%u\n",scale,mode,samples.mean,samples.dark,samples.minimum,samples.maximum);
            Check(mode==0 ? samples.dark>100 : samples.dark==0,"slope compensation removes self-shadow bands at native atlas resolution");
            Check(mode==0 ? samples.maximum-samples.minimum>128 : samples.maximum==samples.minimum,
                "native PCF stripes disappear across nonintegral texture phases");
            spDevice->SetTexture(15,nullptr);spDevice->SetRenderTarget(0,colour);spDevice->SetDepthStencilSurface(surface);
            vp={0,0,size.muWidth,size.muHeight,0,1};spDevice->SetViewport(&vp);spDevice->SetPixelShader(nullptr);
            spDevice->SetRenderState(D3DRS_ZENABLE,TRUE);spDevice->SetRenderState(D3DRS_COLORWRITEENABLE,0);
            renderengine::BeginShadowBiasTest();Require(spDevice->BeginScene(),"begin real occluder");
            Quad(float(size.muWidth)*.35f-.5f,-.5f,float(size.muWidth)*.65f-.5f,float(size.muHeight/3)-.5f,.25f,.25f);
            Require(spDevice->EndScene(),"end real occluder");renderengine::EndShadowBiasTest();
            spDevice->SetDepthStencilSurface(nullptr);spDevice->SetRenderTarget(0,target);
            vp={0,0,256,64,0,1};spDevice->SetViewport(&vp);spDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
            spDevice->SetRenderState(D3DRS_COLORWRITEENABLE,15);spDevice->SetTexture(15,depth);spDevice->SetPixelShader(shader);
            Require(spDevice->BeginScene(),"begin occluded receiver");Quad(-.5f,-.5f,255.5f,63.5f,0,0);Require(spDevice->EndScene(),"end occluded receiver");
            Check(Read(target,read).centre<4,"a real foreground occluder still casts a dark shadow");
        }
        spDevice->SetTexture(15,nullptr);surface->Release();depth->Release();colour->Release();
    }
    renderengine::GetGraphicsSettingsPC().mfShadowSlopeBias=1;
    D3DDevice_SetRenderState_SlopeScaleDepthBias(spDevice,Bits(.5f));
    renderengine::BeginShadowBiasTest();DWORD active=0;spDevice->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,&active);
    Check(active==Bits(1.5f),"configured bias adds to the authored caster bias");
    D3DDevice_SetRenderState_SlopeScaleDepthBias(spDevice,Bits(.25f));
    renderengine::EndShadowBiasTest();spDevice->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,&active);
    Check(active==Bits(.25f),"latest material state is restored without invalidating its shadow cache");
    read->Release();target->Release();shader->Release();spDevice->Release();api->Release();DestroyWindow(window);
    std::printf("PCShadowQuality: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
