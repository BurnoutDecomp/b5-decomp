#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "pc/gcm/renderengine/WorldTextureFilteringPCLeaf.h"
#include "world_texture_filtering.inc"

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; }
static u32 suChecks = 0, suFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{ ++suChecks; if (!lbPass) { ++suFailures; std::printf("FAIL %s\n", lpcName); } }
static void Require(HRESULT lResult, const char* lpcName)
{ if (FAILED(lResult)) { std::printf("GPU FAILURE %s: %08X\n", lpcName, unsigned(lResult)); std::exit(2); } }
static DWORD Sampler(IDirect3DDevice9* lpDevice, D3DSAMPLERSTATETYPE leState)
{ DWORD luValue = 0; Require(lpDevice->GetSamplerState(0, leState, &luValue), "sampler readback"); return luValue; }

struct Vertex { float x, y, z, rhw, u, v; };
static DWORD Pixel(IDirect3DDevice9* lpDevice, IDirect3DSurface9* lpTarget, IDirect3DSurface9* lpReadback)
{
    const Vertex kaVertices[] = {{-0.5f,-0.5f,0,1,.5f,.5f},{-0.5f,15.5f,0,1,.5f,.5f},
                                {15.5f,-0.5f,0,1,.5f,.5f},{15.5f,15.5f,0,1,.5f,.5f}};
    Require(lpDevice->BeginScene(), "begin");
    Require(lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, kaVertices, sizeof(Vertex)), "draw");
    Require(lpDevice->EndScene(), "end");
    Require(lpDevice->GetRenderTargetData(lpTarget, lpReadback), "readback");
    D3DLOCKED_RECT lLock = {};
    Require(lpReadback->LockRect(&lLock, nullptr, D3DLOCK_READONLY), "pixel lock");
    const DWORD luPixel = reinterpret_cast<const DWORD*>(static_cast<const u8*>(lLock.pBits) + 8*lLock.Pitch)[8];
    Require(lpReadback->UnlockRect(), "pixel unlock");
    return luPixel;
}

int main()
{
    HWND lhWindow = CreateWindowA("STATIC", "world texture filtering", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9* lpApi = Direct3DCreate9(D3D_SDK_VERSION);
    if (!lhWindow || !lpApi) return 2;
    D3DPRESENT_PARAMETERS lPresent = {};
    lPresent.Windowed = TRUE; lPresent.hDeviceWindow = lhWindow; lPresent.SwapEffect = D3DSWAPEFFECT_DISCARD;
    IDirect3DDevice9* lpDevice = nullptr;
    Require(lpApi->CreateDevice(0, D3DDEVTYPE_HAL, lhWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &lPresent, &lpDevice), "device");
    D3DCAPS9 lCaps = {};
    Require(lpDevice->GetDeviceCaps(&lCaps), "caps");
    IDirect3DTexture9* lpTexture = nullptr;
    Require(lpDevice->CreateTexture(256,256,9,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&lpTexture,nullptr), "mip fixture");
    for (u32 luLevel=0; luLevel<9; ++luLevel)
    {
        D3DLOCKED_RECT lLock = {}; D3DSURFACE_DESC lDesc = {};
        Require(lpTexture->GetLevelDesc(luLevel, &lDesc), "mip dimensions");
        Require(lpTexture->LockRect(luLevel, &lLock, nullptr, 0), "mip lock");
        for (u32 luY=0; luY<lDesc.Height; ++luY)
        {
            DWORD* lpRow = reinterpret_cast<DWORD*>(static_cast<u8*>(lLock.pBits) + luY*lLock.Pitch);
            for (u32 luX=0; luX<lDesc.Width; ++luX) lpRow[luX] = D3DCOLOR_XRGB(32+luLevel*24,0,0);
        }
        Require(lpTexture->UnlockRect(luLevel), "mip unlock");
    }
    IDirect3DSurface9 *lpTarget=nullptr, *lpReadback=nullptr;
    Require(lpDevice->CreateRenderTarget(16,16,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&lpTarget,nullptr), "target");
    Require(lpDevice->CreateOffscreenPlainSurface(16,16,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&lpReadback,nullptr), "read surface");
    Require(lpDevice->SetRenderTarget(0,lpTarget), "bind target");
    Require(lpDevice->SetTexture(0,lpTexture), "bind texture");
    Require(lpDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1), "vertex format");
    lpDevice->SetRenderState(D3DRS_ZENABLE,FALSE); lpDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    IDirect3DPixelShader9* lpShader = nullptr;
    Require(lpDevice->CreatePixelShader(kauWorldFilterProgram,&lpShader), "gradient shader");
    Require(lpDevice->SetPixelShader(lpShader), "bind shader");
    const float lfAuthoredBias = -0.9f;
    DWORD luBiasBits; std::memcpy(&luBiasBits,&lfAuthoredBias,4);
    Require(lpDevice->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,luBiasBits), "authored bias");
    auto& lrSettings = renderengine::GetGraphicsSettingsPC();
    lrSettings.miAnisotropicFiltering=1;
    renderengine::ApplyWorldTextureFilteringPC(lpDevice,0,false);
    const DWORD luOriginal = Pixel(lpDevice,lpTarget,lpReadback);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR, "original trilinear minification");
    lrSettings.miAnisotropicFiltering=16;
    renderengine::ApplyWorldTextureFilteringPC(lpDevice,0,false);
    const bool lbSupported=(lCaps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC)!=0 && lCaps.MaxAnisotropy>1;
    const DWORD luExpected=lbSupported ? (lCaps.MaxAnisotropy<16?lCaps.MaxAnisotropy:16) : 1;
    Check(Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==luExpected, "request respects real adapter limit");
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==(lbSupported?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR), "device-supported minification");
    Check(Sampler(lpDevice,D3DSAMP_MAGFILTER)==D3DTEXF_LINEAR && Sampler(lpDevice,D3DSAMP_MIPFILTER)==D3DTEXF_LINEAR,
        "magnification and trilinear mip blending retained");
    Check(Sampler(lpDevice,D3DSAMP_MIPMAPLODBIAS)==luBiasBits, "authored mip bias retained");
    const DWORD luFiltered = Pixel(lpDevice,lpTarget,lpReadback);
    const u32 luOldRed=(luOriginal>>16)&255, luNewRed=(luFiltered>>16)&255;
    std::printf("grazing footprint original mip marker=%u filtered=%u effectiveAnisotropy=%lu\n",luOldRed,luNewRed,luExpected);
    Check(!lbSupported || luExpected<4 || luNewRed+24<luOldRed, "grazing surface samples measurably finer authored mip levels");
    renderengine::ApplyWorldTextureFilteringPC(lpDevice,0,true);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR && Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==1,
        "cube reflection filtering remains original");
    lrSettings.miAnisotropicFiltering=1;
    renderengine::ApplyWorldTextureFilteringPC(lpDevice,0,false);
    Check(Pixel(lpDevice,lpTarget,lpReadback)==luOriginal, "disabling override restores original pixels");
    lpDevice->SetTexture(0,nullptr); lpShader->Release(); lpReadback->Release(); lpTarget->Release();
    lpTexture->Release(); lpDevice->Release(); lpApi->Release(); DestroyWindow(lhWindow);
    std::printf("PCWorldTextureFiltering: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
