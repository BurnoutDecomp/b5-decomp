#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "pc/gcm/renderengine/WorldTextureFiltering.h"
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
static void SetSamplerBlock(u8* lpBlock, u8 luMinFilter, u8 luMagFilter,
                            u8 luMipFilter, u8 luMaxAnisotropy,
                            u8 luAddress, f32 lfLodBias)
{
    std::memset(lpBlock, 0, 0x20);
    lpBlock[0x08] = luAddress;
    lpBlock[0x09] = luAddress;
    lpBlock[0x0A] = luAddress;
    lpBlock[0x0C] = luMipFilter;
    lpBlock[0x12] = luMinFilter;
    lpBlock[0x14] = luMagFilter;
    lpBlock[0x17] = luMaxAnisotropy;
    std::memcpy(lpBlock, &lfLodBias, sizeof(lfLodBias));
}

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
    const f32 lfAuthoredBias = -0.9f;
    DWORD luBiasBits; std::memcpy(&luBiasBits,&lfAuthoredBias,sizeof(lfAuthoredBias));
    u8 lauLinearBlock[0x20];
    SetSamplerBlock(lauLinearBlock, 1u, 1u, 2u, 1u, 0u, lfAuthoredBias);
    auto& lrSettings = renderengine::GetGraphicsSettingsPC();
    lrSettings.miAnisotropicFiltering=1;
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauLinearBlock);
    const DWORD luOriginal = Pixel(lpDevice,lpTarget,lpReadback);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR,
          "setting 1 preserves the authored linear minifier");
    Check(Sampler(lpDevice,D3DSAMP_MAGFILTER)==D3DTEXF_LINEAR &&
          Sampler(lpDevice,D3DSAMP_MIPFILTER)==D3DTEXF_LINEAR,
          "default block retains authored linear magnification and mip filtering");
    Check(Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==1u,
          "default block retains authored anisotropy count");
    Check(Sampler(lpDevice,D3DSAMP_MIPMAPLODBIAS)==luBiasBits,
          "raw block retains authored mip bias");
    lrSettings.miAnisotropicFiltering=16;
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauLinearBlock);
    const bool lbSupported=(lCaps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC)!=0 && lCaps.MaxAnisotropy>1;
    const DWORD luExpected=lbSupported ? (lCaps.MaxAnisotropy<16?lCaps.MaxAnisotropy:16) : 1;
    Check(Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==luExpected,
          "optional linear minification respects real adapter anisotropy support and cap");
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==(lbSupported?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR),
          "optional enhancement promotes only the authored linear minifier");
    Check(Sampler(lpDevice,D3DSAMP_MAGFILTER)==D3DTEXF_LINEAR && Sampler(lpDevice,D3DSAMP_MIPFILTER)==D3DTEXF_LINEAR,
        "authored magnification and trilinear mip filtering are unchanged");
    Check(Sampler(lpDevice,D3DSAMP_MIPMAPLODBIAS)==luBiasBits &&
          Sampler(lpDevice,D3DSAMP_ADDRESSU)==D3DTADDRESS_WRAP,
          "authored mip bias and address mode are unchanged");

    // The bind-time filter helper shares the block translation policy but must
    // leave later address, mip-filter, and LOD-bias writes untouched.
    const float lfLaterBias = 0.25f;
    DWORD luLaterBiasBits; std::memcpy(&luLaterBiasBits,&lfLaterBias,sizeof(lfLaterBias));
    Require(renderengine::PCSetSamplerState(lpDevice,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP),
            "later address override");
    Require(renderengine::PCSetSamplerState(lpDevice,0,D3DSAMP_MIPFILTER,D3DTEXF_POINT),
            "later mip override");
    Require(renderengine::PCSetSamplerState(lpDevice,0,D3DSAMP_MIPMAPLODBIAS,luLaterBiasBits),
            "later bias override");
    renderengine::ReapplyXenonTextureDependentFilterFieldsPC(lpDevice, 0u, lauLinearBlock);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==(lbSupported?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR) &&
          Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==luExpected,
          "post-bind filter replay shares the optional block policy");
    Check(Sampler(lpDevice,D3DSAMP_ADDRESSU)==D3DTADDRESS_CLAMP &&
          Sampler(lpDevice,D3DSAMP_MIPFILTER)==D3DTEXF_POINT &&
          Sampler(lpDevice,D3DSAMP_MIPMAPLODBIAS)==luLaterBiasBits,
          "post-bind filter replay preserves later unrelated sampler writes");

    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauLinearBlock);
    const DWORD luFiltered = Pixel(lpDevice,lpTarget,lpReadback);
    const u32 luOldRed=(luOriginal>>16)&255, luNewRed=(luFiltered>>16)&255;
    std::printf("grazing footprint original mip marker=%u filtered=%u effectiveAnisotropy=%lu\n",luOldRed,luNewRed,luExpected);
    Check(!lbSupported || luExpected<4 || luNewRed+24<luOldRed, "grazing surface samples measurably finer authored mip levels");

    u8 lauPointBlock[0x20];
    const f32 lfPointBias = -0.4f;
    SetSamplerBlock(lauPointBlock, 0u, 0u, 1u, 1u, 2u, lfPointBias);
    lrSettings.miAnisotropicFiltering=16;
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauPointBlock);
    DWORD luPointBiasBits; std::memcpy(&luPointBiasBits,&lfPointBias,sizeof(lfPointBias));
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_POINT &&
          Sampler(lpDevice,D3DSAMP_MAGFILTER)==D3DTEXF_POINT,
          "optional setting preserves authored point filters");
    Check(Sampler(lpDevice,D3DSAMP_MIPFILTER)==D3DTEXF_POINT &&
          Sampler(lpDevice,D3DSAMP_MIPMAPLODBIAS)==luPointBiasBits &&
          Sampler(lpDevice,D3DSAMP_ADDRESSU)==D3DTADDRESS_CLAMP,
          "point block retains authored mip filter, bias, and clamp address");

    u8 lauUnknownBlock[0x20];
    SetSamplerBlock(lauUnknownBlock, 3u, 1u, 2u, 1u, 0u, lfAuthoredBias);
    renderengine::PCSetSamplerState(lpDevice, 0u, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauUnknownBlock);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_POINT,
          "optional setting leaves an unknown authored minifier unmapped");

    u8 lauAnisotropicBlock[0x20];
    SetSamplerBlock(lauAnisotropicBlock, 4u, 1u, 2u, 8u, 0u, lfAuthoredBias);
    lrSettings.miAnisotropicFiltering=2;
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauAnisotropicBlock);
    const bool lbAnyAnisoSupported =
        (lCaps.TextureFilterCaps & (D3DPTFILTERCAPS_MINFANISOTROPIC |
                                    D3DPTFILTERCAPS_MAGFANISOTROPIC)) != 0 &&
        lCaps.MaxAnisotropy>1;
    const DWORD luBlockAnisotropy = lbAnyAnisoSupported
        ? (lCaps.MaxAnisotropy<8u ? lCaps.MaxAnisotropy : 8u) : 1u;
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==
              ((lCaps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC)!=0
                   ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR) &&
          Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==luBlockAnisotropy,
          "lower optional setting does not reduce an authored anisotropic block request");

    u8 lauMagAnisotropicBlock[0x20];
    SetSamplerBlock(lauMagAnisotropicBlock, 1u, 4u, 2u, 8u, 0u, lfAuthoredBias);
    lrSettings.miAnisotropicFiltering=16;
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauMagAnisotropicBlock);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR &&
          Sampler(lpDevice,D3DSAMP_MAGFILTER)==
              ((lCaps.TextureFilterCaps&D3DPTFILTERCAPS_MAGFANISOTROPIC)!=0
                   ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR) &&
          Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==luBlockAnisotropy,
          "optional setting leaves an authored anisotropic magnifier under block/cap policy");

    // Synthetic type caps exercise the same shared helper used by the whole-block
    // and post-bind paths without requiring extra cube/volume fixture resources.
    D3DCAPS9 lAllTypeAnisoCaps = {};
    lAllTypeAnisoCaps.MaxAnisotropy = 16u;
    lAllTypeAnisoCaps.TextureFilterCaps =
        D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC;
    lAllTypeAnisoCaps.CubeTextureFilterCaps = lAllTypeAnisoCaps.TextureFilterCaps;
    lAllTypeAnisoCaps.VolumeTextureFilterCaps = lAllTypeAnisoCaps.TextureFilterCaps;
    lrSettings.miAnisotropicFiltering=16;
    renderengine::ApplyXenonTextureDependentFilterFieldsPC(
        lpDevice, 0u, lauLinearBlock, D3DRTYPE_CUBETEXTURE, true, lAllTypeAnisoCaps,
        true, true);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR &&
          Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==1u,
          "optional enhancement is disabled for cube TextureStates");
    renderengine::ApplyXenonTextureDependentFilterFieldsPC(
        lpDevice, 0u, lauLinearBlock, D3DRTYPE_VOLUMETEXTURE, true, lAllTypeAnisoCaps,
        true, true);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR &&
          Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==1u,
          "optional enhancement is disabled for volume TextureStates");

    D3DCAPS9 lNoMinAnisoCaps = {};
    lNoMinAnisoCaps.MaxAnisotropy = 16u;
    lNoMinAnisoCaps.TextureFilterCaps = D3DPTFILTERCAPS_MAGFANISOTROPIC;
    renderengine::ApplyXenonTextureDependentFilterFieldsPC(
        lpDevice, 0u, lauLinearBlock, D3DRTYPE_TEXTURE, true, lNoMinAnisoCaps,
        true, true);
    Check(Sampler(lpDevice,D3DSAMP_MINFILTER)==D3DTEXF_LINEAR &&
          Sampler(lpDevice,D3DSAMP_MAXANISOTROPY)==1u,
          "optional enhancement requires native MIN-anisotropic filter support");

    lrSettings.miAnisotropicFiltering=1;
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0u, lauLinearBlock);
    Check(Pixel(lpDevice,lpTarget,lpReadback)==luOriginal,
          "setting 1 restores the raw block's original pixels");
    lpDevice->SetTexture(0,nullptr); lpShader->Release(); lpReadback->Release(); lpTarget->Release();
    lpTexture->Release(); lpDevice->Release(); lpApi->Release(); DestroyWindow(lhWindow);
    std::printf("PCWorldTextureFiltering: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
