#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include "pc/gcm/renderengine/ShadowReceiverPCLeaf.h"
#include "pc_reflection_shadow_programs.inc"

static IDirect3DDevice9* spDevice;
static u32 suChecks=0,suFailures=0;
static IDirect3DDevice9* Dev() { return spDevice; }
static void LogOnce(const char*,const char*) {}
namespace renderengine {
static void PCSetVertexShaderConstantF(IDirect3DDevice9* lpDevice,u32 luFirst,const f32* lpfValue,u32 luCount)
{ lpDevice->SetVertexShaderConstantF(luFirst,lpfValue,luCount); }
static void PCSetPixelShaderConstantF(IDirect3DDevice9* lpDevice,u32 luFirst,const f32* lpfValue,u32 luCount)
{ lpDevice->SetPixelShaderConstantF(luFirst,lpfValue,luCount); }
}
using namespace renderengine;
#include "pc_reflection_shadows.inc"
static void Check(bool lbPass,const char* lpcName)
{ ++suChecks; if(!lbPass){++suFailures;std::printf("FAIL %s\n",lpcName);} }
static void Require(HRESULT lhResult,const char* lpcName)
{ if(FAILED(lhResult)){std::printf("GPU FAILURE %s %08x\n",lpcName,unsigned(lhResult));std::exit(2);} }
struct Vertex {f32 x,y,z,w;};

int main()
{
    HWND lWindow=CreateWindowA("STATIC","reflection shadow regression",WS_OVERLAPPEDWINDOW,
        0,0,32,32,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* lpApi=Direct3DCreate9(D3D_SDK_VERSION);
    if(!lpApi || !lWindow) return 2;
    D3DPRESENT_PARAMETERS lPresent={};
    lPresent.Windowed=TRUE;lPresent.hDeviceWindow=lWindow;lPresent.SwapEffect=D3DSWAPEFFECT_DISCARD;
    lPresent.BackBufferWidth=lPresent.BackBufferHeight=32;lPresent.BackBufferFormat=D3DFMT_X8R8G8B8;
    Require(lpApi->CreateDevice(0,D3DDEVTYPE_HAL,lWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,
        &lPresent,&spDevice),"device");
    IDirect3DTexture9* lpAtlas=nullptr;
    IDirect3DSurface9 *lpDepth=nullptr,*lpColour=nullptr,*lpTarget=nullptr,*lpRead=nullptr;
    Require(spDevice->CreateTexture(128,192,1,D3DUSAGE_DEPTHSTENCIL,D3DFMT_D24X8,D3DPOOL_DEFAULT,&lpAtlas,nullptr),"comparison atlas");
    Require(lpAtlas->GetSurfaceLevel(0,&lpDepth),"atlas surface");
    Require(spDevice->CreateRenderTarget(128,192,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&lpColour,nullptr),"atlas colour target");
    Require(spDevice->SetRenderTarget(0,lpColour),"caster colour");
    Require(spDevice->SetDepthStencilSurface(lpDepth),"caster depth");
    D3DVIEWPORT9 lAtlasViewport={0,0,128,192,0,1};spDevice->SetViewport(&lAtlasViewport);
    const f32 KAF_CASTER_DEPTHS[]={.25f,.75f,.25f};
    for(u32 luCascade=0;luCascade<3;++luCascade)
    {
        D3DRECT lRect={0,LONG(luCascade*64),128,LONG((luCascade+1)*64)};
        Require(spDevice->Clear(1,&lRect,D3DCLEAR_ZBUFFER,0,KAF_CASTER_DEPTHS[luCascade],0),"cascade depth");
    }
    Require(spDevice->SetDepthStencilSurface(nullptr),"detach atlas depth");
    Require(spDevice->CreateRenderTarget(16,16,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&lpTarget,nullptr),"receiver target");
    Require(spDevice->CreateOffscreenPlainSurface(16,16,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&lpRead,nullptr),"readback");
    Require(spDevice->SetRenderTarget(0,lpTarget),"receiver target bind");
    D3DVIEWPORT9 lViewport={0,0,16,16,0,1};spDevice->SetViewport(&lViewport);
    spDevice->SetRenderState(D3DRS_ZENABLE,FALSE);spDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    spDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);spDevice->SetRenderState(D3DRS_LIGHTING,FALSE);
    Require(spDevice->SetTexture(15,lpAtlas),"comparison texture");
    for(D3DSAMPLERSTATETYPE leFilter:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})
        spDevice->SetSamplerState(15,leFilter,D3DTEXF_POINT);
    spDevice->SetSamplerState(15,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
    spDevice->SetSamplerState(15,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    spDevice->SetSamplerState(15,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    IDirect3DVertexShader9* lpVs=nullptr;IDirect3DPixelShader9* lpPs=nullptr;
    Require(spDevice->CreateVertexShader(KAU_VS_CODE,&lpVs),"receiver vertex shader");
    Require(spDevice->CreatePixelShader(KAU_PS_CODE,&lpPs),"receiver pixel shader");
    spDevice->SetVertexShader(lpVs);spDevice->SetPixelShader(lpPs);
    spDevice->SetFVF(D3DFVF_XYZW);
    f32 lafMatrices[48]={};
    for(u32 luCascade=0;luCascade<3;++luCascade)
    {
        lafMatrices[luCascade*16+12]=.5f;
        lafMatrices[luCascade*16+13]=(f32(luCascade)+.5f)/3.f;
        lafMatrices[luCascade*16+14]=.5f;
        lafMatrices[luCascade*16+15]=1.f;
    }
    const f32 KAF_SPLITS[]={10.5f,34.f,120.f,6.f};
    const f32 KAF_CONSTANTS2[]={22.25f,1.f,0.f,.05f};
    spDevice->SetVertexShaderConstantF(KU_VS_ShadowMap_WorldToLight,lafMatrices,12);
    spDevice->SetPixelShaderConstantF(KU_PS_ShadowMap_Constants,KAF_SPLITS,1);
    spDevice->SetPixelShaderConstantF(KU_PS_ShadowMap_Constants2,KAF_CONSTANTS2,1);
    spDevice->SetVertexShaderConstantF(KU_VS_ShadowMap_Constants,KAF_SPLITS,1);
    spDevice->SetVertexShaderConstantF(KU_VS_ShadowMap_Constants2,KAF_CONSTANTS2,1);
    const f32 KAF_FIRST_PAIR[]={0,1,10.5f,0};
    spDevice->SetVertexShaderConstantF(KU_VS_ShadowMap_ObjectCsmSelect,KAF_FIRST_PAIR,1);
    const Vertex KAV_QUAD[]={{-1,-1,.5f,1},{-1,1,.5f,1},{1,-1,.5f,1},{1,1,.5f,1}};
    auto SampleReceiver=[&]()
    {
        BindReceiverDepth(KAU_VS_CODE,KAU_PS_CODE);
        Require(spDevice->BeginScene(),"begin receiver");
        Require(spDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,KAV_QUAD,sizeof(Vertex)),"receiver draw");
        Require(spDevice->EndScene(),"end receiver");
        Require(spDevice->GetRenderTargetData(lpTarget,lpRead),"receiver readback");
        D3DLOCKED_RECT lLock={};Require(lpRead->LockRect(&lLock,nullptr,D3DLOCK_READONLY),"receiver lock");
        const u8 luValue=*(static_cast<const u8*>(lLock.pBits)+8*lLock.Pitch+8*4);
        lpRead->UnlockRect();
        return luValue;
    };
    // Same stationary receiver, viewed through different capture-face depths and
    // three main-camera positions. The sampled cascade must follow the atlas camera.
    if(KU_RECEIVER_KIND==3) for(f32 lfMainDepth:{6.f,20.f,50.f})
    {
        Matrix44 lMainVp;
        lMainVp.SetIdentity();lMainVp.xAxis.w=0;lMainVp.yAxis.w=0;
        lMainVp.zAxis.w=1;lMainVp.wAxis.w=lfMainDepth;
        SetShadowReceiverCameraPC(lMainVp);
        const f32 KAF_POINT[]={0,0,0,1};
        spDevice->SetVertexShaderConstantF(KU_VS_TestWorld,KAF_POINT,1);
        for(f32 lfFaceDepth:{5.f,20.f,50.f,90.f})
        {
            const f32 lafFace[]={lfFaceDepth,0,0,0};
            spDevice->SetVertexShaderConstantF(201u,lafFace,1);
            const u8 luValue=SampleReceiver();
            Check(lfMainDepth==20.f ? luValue>250 : luValue<5,"reflected shadow uses atlas-camera cascade for every face depth");
        }
    }
    ShadowReceiverViewDepthPC()={0,0,0,0};
    const f32 KAF_LEGACY_FACE[]={20,0,0,0};
    spDevice->SetVertexShaderConstantF(201u,KAF_LEGACY_FACE,1);
    const f32 KAF_UNPUBLISHED_POINT[]={0,0,0,1};
    spDevice->SetVertexShaderConstantF(KU_VS_TestWorld,KAF_UNPUBLISHED_POINT,1);
    if(KU_RECEIVER_KIND!=7) Check(SampleReceiver()>250,"an unpublished camera input preserves legacy receiver behavior");
    const f32 KAF_SENTINEL[]={7,8,9,10};
    spDevice->SetVertexShaderConstantF(255,KAF_SENTINEL,1);
    spDevice->SetPixelShaderConstantF(223,KAF_SENTINEL,1);
    const DWORD KAU_OLD_VERTEX[]={D3DVS_VERSION(3,0),0x0000ffff};
    const DWORD KAU_OLD_PIXEL[]={D3DPS_VERSION(3,0),0x0000ffff};
    BindReceiverDepth(KAU_OLD_VERTEX,KAU_OLD_PIXEL);
    f32 lafPreserved[4]={};spDevice->GetVertexShaderConstantF(255,lafPreserved,1);
    Check(std::memcmp(KAF_SENTINEL,lafPreserved,sizeof(lafPreserved))==0,"unmodified shaders keep their own c255 value");
    spDevice->GetPixelShaderConstantF(223,lafPreserved,1);
    Check(std::memcmp(KAF_SENTINEL,lafPreserved,sizeof(lafPreserved))==0,"unmodified shaders keep their own c223 value");

    Matrix44 lCoverageVp;
    lCoverageVp.SetIdentity();lCoverageVp.xAxis.w=0;lCoverageVp.yAxis.w=0;
    lCoverageVp.zAxis.w=1;lCoverageVp.wAxis.w=6.f;
    SetShadowReceiverCameraPC(lCoverageVp);
    const f32 KAF_POINT_ORIGIN[]={0,0,0,1};
    spDevice->SetVertexShaderConstantF(KU_VS_TestWorld,KAF_POINT_ORIGIN,1);
    auto Positions=[&](f32 u0,f32 v0,f32 u1,f32 v1,f32 u2,f32 v2,f32 z=.5f)
    {
        const f32 lafU[]={u0,u1,u2},lafV[]={v0,v1,v2};
        for(u32 i=0;i<3;++i) {
            lafMatrices[16*i+12]=lafU[i];lafMatrices[16*i+13]=lafV[i];
            lafMatrices[16*i+14]=z;
        }
        spDevice->SetVertexShaderConstantF(KU_VS_ShadowMap_WorldToLight,lafMatrices,12);
    };
    ConfigureReceiverBounds(true);
    Positions(.5f,.9f,.5f,.5f,.5f,5.f/6.f);
    Check(SampleReceiver()>250,"reflection falls back to a covering cascade instead of reading a foreign tile");
    Positions(1.2f,1.f/6.f,-.2f,.5f,.5f,1.5f);
    Check(SampleReceiver()>250,"receivers outside every cascade do not inherit clamped edge shadows");
    Positions(.5f,1.f/6.f,.5f,.5f,.5f,5.f/6.f,1.5f);
    Check(SampleReceiver()>250,"receivers beyond the atlas depth range do not cast false shadows");
    Positions(.5f,1.f/6.f,.5f,.5f,.5f,5.f/6.f);
    Check(SampleReceiver()<250,"valid near-cascade shadows remain visible in reflections");
    Positions(1.2f,1.f/6.f,-.2f,.5f,.5f,5.f/6.f);
    if(KU_RECEIVER_KIND==3 || KU_RECEIVER_KIND==7)
        Check(SampleReceiver()<5,"a covering far cascade retains its real shadow");
    ConfigureReceiverBounds(false);
    Positions(.5f,.9f,.5f,.5f,.5f,5.f/6.f);
    Check(SampleReceiver()<250,"the ordinary camera retains its existing receiver path");
    ConfigureReceiverBounds(true);
    spDevice->SetSamplerState(15,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
    spDevice->SetSamplerState(15,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    Positions(.5f,1.f/3.f+.1f/192.f,.5f,.5f,.5f,5.f/6.f);
    Check(SampleReceiver()>250,"hardware PCF at a cascade edge cannot fetch a neighbouring tile");
    if(KU_RECEIVER_KIND>=4 && KU_RECEIVER_KIND<=6)
    {
        const f32 KAF_SECOND_PAIR[]={1,2,34,0},KAF_SECOND_POINT[]={0,0,0,50};
        spDevice->SetVertexShaderConstantF(KU_VS_ShadowMap_ObjectCsmSelect,KAF_SECOND_PAIR,1);
        spDevice->SetVertexShaderConstantF(KU_VS_TestWorld,KAF_SECOND_POINT,1);
        lCoverageVp.wAxis.w=20.f;SetShadowReceiverCameraPC(lCoverageVp);
        Positions(.5f,1.f/6.f,.5f,.5f,.5f,5.f/6.f);
        Check(SampleReceiver()>250,"the selected second pair retains its original split and valid middle-cascade result");
        Positions(.5f,1.f/6.f,.5f,1.5f,.5f,5.f/6.f);
        Check(SampleReceiver()<5,"the selected second pair falls back to the actual far tile");
        ConfigureReceiverBounds(false);
        Positions(.5f,1.f/6.f,.5f,.5f,.5f,5.f/6.f);
        Check(SampleReceiver()>250,"the main camera retains the second pair's original split");
    }
    spDevice->SetTexture(15,nullptr);lpPs->Release();lpVs->Release();lpRead->Release();lpTarget->Release();
    lpColour->Release();lpDepth->Release();lpAtlas->Release();spDevice->Release();lpApi->Release();DestroyWindow(lWindow);
    std::printf("PCReflectionShadows: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
