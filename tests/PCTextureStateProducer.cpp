#define NOMINMAX
#include <windows.h>
#include <d3d9.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rw/rwcore_structs.h"
#include "pc/gcm/renderengine/WorldTextureFiltering.h"
#include "pc/gcm/renderengine/renderstates.h"
#include "pc/gcm/renderengine/texture.h"
#include "pc_texturestate_producer_shader.inc"

extern "C" void PCTextureStateProducerBuildSdkSamplerState(void* lpStateBytes,
                                                              const void* lpParameterBytes);

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static unsigned suChecks = 0;
static unsigned suFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++suChecks;
    if (!lbPass)
    {
        ++suFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

static int SetupFailure(const char* lpcStage, HRESULT lhResult)
{
    std::printf("SETUP_FAILURE stage=%s hresult=0x%08lX\n", lpcStage,
                static_cast<unsigned long>(lhResult));
    return 78;
}

static int Unsupported(const char* lpcReason)
{
    std::printf("SETUP_UNSUPPORTED %s\n", lpcReason);
    return 77;
}

static bool WriteBytes(const char* lpcPath, const void* lpBytes, size_t luSize)
{
    FILE* lpFile = nullptr;
    if (fopen_s(&lpFile, lpcPath, "wb") != 0 || lpFile == nullptr)
        return false;
    const bool lbComplete = fwrite(lpBytes, 1, luSize, lpFile) == luSize;
    const bool lbClosed = fclose(lpFile) == 0;
    return lbComplete && lbClosed;
}

static void PrintBytes(const char* lpcLabel, const uint8_t* lpBytes, size_t luSize)
{
    std::printf("%s", lpcLabel);
    for (size_t luIndex = 0; luIndex < luSize; ++luIndex)
        std::printf(" %02X", static_cast<unsigned>(lpBytes[luIndex]));
    std::printf("\n");
}

static DWORD GetSamplerState(IDirect3DDevice9* lpDevice, D3DSAMPLERSTATETYPE leState)
{
    DWORD luValue = 0;
    const HRESULT lhResult = lpDevice->GetSamplerState(0, leState, &luValue);
    if (FAILED(lhResult))
    {
        std::printf("SETUP_FAILURE stage=GetSamplerState(%u) hresult=0x%08lX\n",
                    static_cast<unsigned>(leState), static_cast<unsigned long>(lhResult));
        std::exit(78);
    }
    return luValue;
}

static HRESULT SetWrapDefaults(IDirect3DDevice9* lpDevice)
{
    renderengine::gPCSamplerStateCache.Invalidate();
    HRESULT lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    if (FAILED(lhResult)) return lhResult;
    lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    if (FAILED(lhResult)) return lhResult;
    lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
    if (FAILED(lhResult)) return lhResult;
    lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    if (FAILED(lhResult)) return lhResult;
    lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    if (FAILED(lhResult)) return lhResult;
    lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    if (FAILED(lhResult)) return lhResult;
    lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_MAXANISOTROPY, 1);
    if (FAILED(lhResult)) return lhResult;
    lhResult = renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_MAXMIPLEVEL, 0);
    if (FAILED(lhResult)) return lhResult;
    return renderengine::PCSetSamplerState(lpDevice, 0, D3DSAMP_MIPMAPLODBIAS, 0);
}

struct Vertex
{
    float mfX;
    float mfY;
    float mfZ;
    float mfRhw;
    float mfU;
    float mfV;
};

static HRESULT DrawAndReadPixel(IDirect3DDevice9* lpDevice,
                                IDirect3DSurface9* lpTarget,
                                IDirect3DSurface9* lpReadback,
                                DWORD* lpuPixelOut)
{
    HRESULT lhResult = lpDevice->SetRenderTarget(0, lpTarget);
    if (FAILED(lhResult)) return lhResult;
    const D3DVIEWPORT9 lViewport = {0, 0, 16, 16, 0.0f, 1.0f};
    lhResult = lpDevice->SetViewport(&lViewport);
    if (FAILED(lhResult)) return lhResult;
    lhResult = lpDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(lhResult)) return lhResult;

    const Vertex kaVertices[] = {
        {-0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f},
        {15.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f},
        {-0.5f, 15.5f, 0.0f, 1.0f, 0.5f, 0.5f},
        {15.5f, 15.5f, 0.0f, 1.0f, 0.5f, 0.5f},
    };
    lhResult = lpDevice->BeginScene();
    if (FAILED(lhResult)) return lhResult;
    lhResult = lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, kaVertices, sizeof(Vertex));
    const HRESULT lhEnd = lpDevice->EndScene();
    if (FAILED(lhResult)) return lhResult;
    if (FAILED(lhEnd)) return lhEnd;
    lhResult = lpDevice->GetRenderTargetData(lpTarget, lpReadback);
    if (FAILED(lhResult)) return lhResult;

    D3DLOCKED_RECT lLock = {};
    lhResult = lpReadback->LockRect(&lLock, nullptr, D3DLOCK_READONLY);
    if (FAILED(lhResult)) return lhResult;
    *lpuPixelOut = *reinterpret_cast<const DWORD*>(
        static_cast<const uint8_t*>(lLock.pBits) + 8 * lLock.Pitch + 8 * sizeof(DWORD));
    return lpReadback->UnlockRect();
}

static bool IsBrightGreen(DWORD luPixel)
{
    const unsigned luRed = (luPixel >> 16) & 0xFFu;
    const unsigned luGreen = (luPixel >> 8) & 0xFFu;
    const unsigned luBlue = luPixel & 0xFFu;
    return luGreen >= 240u && luRed <= 12u && luBlue <= 12u;
}

static void PrintPixel(const char* lpcLabel, DWORD luPixel)
{
    std::printf("%s ARGB=%08lX rgb=(%u,%u,%u)\n", lpcLabel,
                static_cast<unsigned long>(luPixel), (luPixel >> 16) & 0xFFu,
                (luPixel >> 8) & 0xFFu, luPixel & 0xFFu);
}

static void CheckSamplerContract(const char* lpcPrefix, IDirect3DDevice9* lpDevice,
                                 DWORD luExpectedLodBiasBits)
{
    const bool lbAddress = GetSamplerState(lpDevice, D3DSAMP_ADDRESSU) == D3DTADDRESS_CLAMP &&
        GetSamplerState(lpDevice, D3DSAMP_ADDRESSV) == D3DTADDRESS_CLAMP &&
        GetSamplerState(lpDevice, D3DSAMP_ADDRESSW) == D3DTADDRESS_CLAMP;
    const bool lbFilters = GetSamplerState(lpDevice, D3DSAMP_MINFILTER) == D3DTEXF_LINEAR &&
        GetSamplerState(lpDevice, D3DSAMP_MAGFILTER) == D3DTEXF_LINEAR &&
        GetSamplerState(lpDevice, D3DSAMP_MIPFILTER) == D3DTEXF_NONE;
    const bool lbLod = GetSamplerState(lpDevice, D3DSAMP_MIPMAPLODBIAS) == luExpectedLodBiasBits &&
        GetSamplerState(lpDevice, D3DSAMP_MAXMIPLEVEL) == 0;
    const bool lbAnisotropy = GetSamplerState(lpDevice, D3DSAMP_MAXANISOTROPY) == 1;

    char lacName[160] = {};
    sprintf_s(lacName, "%s maps generated U/V/W to CLAMP", lpcPrefix);
    Check(lbAddress, lacName);
    sprintf_s(lacName, "%s maps generated LINEAR min/mag and NONE mip", lpcPrefix);
    Check(lbFilters, lacName);
    sprintf_s(lacName, "%s maps LOD bias and max-mip request", lpcPrefix);
    Check(lbLod, lacName);
    sprintf_s(lacName, "%s maps the authored anisotropy byte", lpcPrefix);
    Check(lbAnisotropy, lacName);
}

static void SetTintVolumeParameters(renderengine::TextureState::Parameters* lpParams,
                                    renderengine::Texture* lpRaster)
{
    // These words are the volume lookup state built by post-fx Tint::Initialize.
    // The SDK member names at the same offsets are legacy; the current bytes follow
    // ARTIST SamplerState::Initialize and ImRendererBase::ConstructSamplerState.
    std::memset(lpParams, 0, sizeof(*lpParams));
    lpParams->muAddressU = 2;
    lpParams->muAddressV = 2;
    lpParams->muAddressW = 2;
    lpParams->muMagFilter = 1;
    lpParams->muMinFilter = 1;
    lpParams->muMipFilter = 0;
    lpParams->muField6 = 1;
    lpParams->muField7 = 1;
    lpParams->muMaxAnisotropy = 13;
    lpParams->muField9 = 0;
    lpParams->muField10 = 1;
    // -0.5 is also an observed TextureState caller value (CgsFont::CreateTextureState).
    lpParams->mfMipLodBias = -0.5f;
    lpParams->mfField12 = 0.0f;
    lpParams->mu8Field43 = 1;
    lpParams->mu8Field44 = 1;
    lpParams->mpTexture = lpRaster;
}

static HRESULT FillVolumeLut(IDirect3DVolumeTexture9* lpTexture)
{
    D3DVOLUME_DESC lDesc = {};
    HRESULT lhResult = lpTexture->GetLevelDesc(0, &lDesc);
    if (FAILED(lhResult)) return lhResult;
    D3DLOCKED_BOX lLock = {};
    lhResult = lpTexture->LockBox(0, &lLock, nullptr, 0);
    if (FAILED(lhResult)) return lhResult;
    for (UINT luZ = 0; luZ < lDesc.Depth; ++luZ)
    {
        for (UINT luY = 0; luY < lDesc.Height; ++luY)
        {
            DWORD* lpRow = reinterpret_cast<DWORD*>(
                static_cast<uint8_t*>(lLock.pBits) + luZ * lLock.SlicePitch + luY * lLock.RowPitch);
            const DWORD luColour = luY == 0
                ? D3DCOLOR_ARGB(255, 255, 0, 255)  // wrapped green=1 reaches the magenta edge row
                : D3DCOLOR_ARGB(255, 0, 255, 0);   // clamped green=1 reaches the green boundary
            for (UINT luX = 0; luX < lDesc.Width; ++luX)
                lpRow[luX] = luColour;
        }
    }
    return lpTexture->UnlockBox(0);
}

int main()
{
    static_assert(offsetof(renderengine::TextureState::Parameters, mpTexture) == 0x48,
                  "TextureState raster pointer must follow the 0x48-byte sampler parameter prefix");
    static_assert(sizeof(renderengine::TextureState::Parameters) >= 0x48 + sizeof(void*),
                  "TextureState parameters must contain the raster pointer");

    WNDCLASSA lWindowClass = {};
    lWindowClass.lpfnWndProc = DefWindowProcA;
    lWindowClass.hInstance = GetModuleHandleA(nullptr);
    lWindowClass.lpszClassName = "PcTextureStateProducerFixtureWindow";
    if (RegisterClassA(&lWindowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return SetupFailure("RegisterClass", HRESULT_FROM_WIN32(GetLastError()));
    HWND lhWindow = CreateWindowExA(0, lWindowClass.lpszClassName, "TextureState producer fixture",
        WS_POPUP, 0, 0, 32, 32, nullptr, nullptr, lWindowClass.hInstance, nullptr);
    if (lhWindow == nullptr)
        return SetupFailure("CreateWindow", HRESULT_FROM_WIN32(GetLastError()));

    IDirect3D9* lpApi = Direct3DCreate9(D3D_SDK_VERSION);
    if (lpApi == nullptr)
        return Unsupported("Direct3DCreate9 returned null");

    D3DPRESENT_PARAMETERS lPresent = {};
    lPresent.BackBufferWidth = 16;
    lPresent.BackBufferHeight = 16;
    lPresent.BackBufferFormat = D3DFMT_X8R8G8B8;
    lPresent.BackBufferCount = 1;
    lPresent.MultiSampleType = D3DMULTISAMPLE_NONE;
    lPresent.SwapEffect = D3DSWAPEFFECT_DISCARD;
    lPresent.hDeviceWindow = lhWindow;
    lPresent.Windowed = TRUE;
    lPresent.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* lpDevice = nullptr;
    HRESULT lhResult = lpApi->CreateDevice(0, D3DDEVTYPE_HAL, lhWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &lPresent, &lpDevice);
    if (FAILED(lhResult) || lpDevice == nullptr)
        return Unsupported("HAL D3D9 device creation failed for the hidden window");

    D3DCAPS9 lCaps = {};
    lhResult = lpDevice->GetDeviceCaps(&lCaps);
    if (FAILED(lhResult)) return SetupFailure("GetDeviceCaps", lhResult);
    std::printf("D3D9 caps pixelShader=%u.%u volumeAddressCaps=0x%08lX volumeFilterCaps=0x%08lX maxAnisotropy=%lu\n",
        D3DSHADER_VERSION_MAJOR(lCaps.PixelShaderVersion), D3DSHADER_VERSION_MINOR(lCaps.PixelShaderVersion),
        static_cast<unsigned long>(lCaps.VolumeTextureAddressCaps),
        static_cast<unsigned long>(lCaps.VolumeTextureFilterCaps),
        static_cast<unsigned long>(lCaps.MaxAnisotropy));
    if (lCaps.PixelShaderVersion < D3DPS_VERSION(3, 0))
        return Unsupported("pixel shader model 3.0 is unavailable");
    if ((lCaps.VolumeTextureAddressCaps & D3DPTADDRESSCAPS_CLAMP) == 0)
        return Unsupported("volume textures do not support CLAMP addressing");
    if ((lCaps.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) == 0 ||
        (lCaps.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) == 0)
        return Unsupported("volume textures do not support linear min/mag filtering");

    IDirect3DVolumeTexture9* lpVolume = nullptr;
    lhResult = lpDevice->CreateVolumeTexture(4, 4, 4, 1, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, &lpVolume, nullptr);
    if (FAILED(lhResult) || lpVolume == nullptr)
        return Unsupported("A8R8G8B8 volume LUT creation failed");
    lhResult = FillVolumeLut(lpVolume);
    if (FAILED(lhResult)) return SetupFailure("FillVolumeLut", lhResult);

    IDirect3DSurface9* lpTarget = nullptr;
    IDirect3DSurface9* lpReadback = nullptr;
    lhResult = lpDevice->CreateRenderTarget(16, 16, D3DFMT_A8R8G8B8,
        D3DMULTISAMPLE_NONE, 0, FALSE, &lpTarget, nullptr);
    if (FAILED(lhResult) || lpTarget == nullptr)
        return Unsupported("A8R8G8B8 render target creation failed");
    lhResult = lpDevice->CreateOffscreenPlainSurface(16, 16, D3DFMT_A8R8G8B8,
        D3DPOOL_SYSTEMMEM, &lpReadback, nullptr);
    if (FAILED(lhResult) || lpReadback == nullptr)
        return Unsupported("system-memory readback surface creation failed");

    IDirect3DPixelShader9* lpShader = nullptr;
    lhResult = lpDevice->CreatePixelShader(kauTextureStateProducerShader, &lpShader);
    if (FAILED(lhResult) || lpShader == nullptr)
        return Unsupported("compiled PS 3.0 volume-LUT shader could not be created");
    lhResult = lpDevice->SetPixelShader(lpShader);
    if (FAILED(lhResult)) return SetupFailure("SetPixelShader", lhResult);
    lhResult = lpDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    if (FAILED(lhResult)) return SetupFailure("SetFVF", lhResult);
    lpDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    lpDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    lpDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    renderengine::Texture raster = {};
    raster.mpD3DTexture = lpVolume;
    renderengine::TextureState::Parameters lParams = {};
    SetTintVolumeParameters(&lParams, &raster);
    if (!WriteBytes("texturestate_parameters_prefix.bin", &lParams, 0x48))
        return SetupFailure("write parameter input snapshot", E_FAIL);

    uint8_t lauSdkSamplerState[0x20] = {};
    PCTextureStateProducerBuildSdkSamplerState(lauSdkSamplerState, &lParams);
    if (!WriteBytes("sdk_sampler_runtime32.bin", lauSdkSamplerState, sizeof(lauSdkSamplerState)))
        return SetupFailure("write SDK runtime-state snapshot", E_FAIL);
    PrintBytes("SDK SamplerState::Initialize runtime32:", lauSdkSamplerState, sizeof(lauSdkSamplerState));

    DWORD luLodBiasBits = 0;
    std::memcpy(&luLodBiasBits, &lParams.mfMipLodBias, sizeof(luLodBiasBits));
    float lfGeneratedBias = 0.0f;
    std::memcpy(&lfGeneratedBias, lauSdkSamplerState, sizeof(lfGeneratedBias));
    Check(lfGeneratedBias == lParams.mfMipLodBias, "SDK producer emits the authored LOD float at +0");
    Check(lauSdkSamplerState[0x08] == 2 && lauSdkSamplerState[0x09] == 2 &&
          lauSdkSamplerState[0x0A] == 2, "SDK producer emits the caller's U/V/W CLAMP bytes");
    Check(lauSdkSamplerState[0x0C] == 0 && lauSdkSamplerState[0x12] == 1 &&
          lauSdkSamplerState[0x14] == 1 && lauSdkSamplerState[0x17] == 1,
          "SDK producer emits Tint's MIP/MIN/MAG/anisotropy caller bytes");

    alignas(renderengine::TextureState) uint8_t lauResourceMemory[sizeof(renderengine::TextureState)] = {};
    rw::Resource lResource;
    lResource.m_baseResources[0] = lauResourceMemory;
    renderengine::TextureState* lpTextureState =
        renderengine::TextureState::Initialize(&lResource, &lParams);
    if (lpTextureState == nullptr)
        return SetupFailure("TextureState::Initialize returned null", E_FAIL);
    if (!WriteBytes("texturestate_producer_runtime32.bin", lpTextureState->mauSamplerState,
                    sizeof(lpTextureState->mauSamplerState)))
        return SetupFailure("write TextureState runtime-state snapshot", E_FAIL);
    PrintBytes("TextureState::Initialize runtime32:", lpTextureState->mauSamplerState,
               sizeof(lpTextureState->mauSamplerState));
    Check(reinterpret_cast<void*>(lpTextureState) == lResource.m_baseResources[0],
          "TextureState uses the supplied resource memory");
    Check(lpTextureState->mpRaster == &raster,
          "TextureState preserves the supplied raster identity");
    Check(std::memcmp(lpTextureState->mauSamplerState, lauSdkSamplerState,
                      sizeof(lauSdkSamplerState)) == 0,
          "TextureState runtime32 matches the real SDK SamplerState initializer");

    const DWORD luInitialTextureFilterSetting = renderengine::GetGraphicsSettingsPC().miAnisotropicFiltering;
    renderengine::GetGraphicsSettingsPC().miAnisotropicFiltering = 1;

    // Positive control: the exact output of the canonical SDK initializer drives
    // the existing production reader and a real volume-texture lookup at green=1.
    lhResult = lpDevice->SetTexture(0, raster.mpD3DTexture);
    if (FAILED(lhResult)) return SetupFailure("SetTexture reference", lhResult);
    lhResult = SetWrapDefaults(lpDevice);
    if (FAILED(lhResult)) return SetupFailure("reset sampler reference", lhResult);
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0, lauSdkSamplerState);
    CheckSamplerContract("SDK-generated state", lpDevice, luLodBiasBits);
    DWORD luSdkPixel = 0;
    lhResult = DrawAndReadPixel(lpDevice, lpTarget, lpReadback, &luSdkPixel);
    if (FAILED(lhResult)) return SetupFailure("SDK-generated volume LUT draw/readback", lhResult);
    PrintPixel("SDK-generated volume LUT green=1 control", luSdkPixel);
    Check(IsBrightGreen(luSdkPixel),
          "SDK-generated LINEAR/CLAMP volume state keeps green=1 on the green LUT boundary");

    // This is the production TextureState::Initialize result under test. The old
    // memcpy body remains live in this fixture, so its wrap/miscopied bytes produce
    // a visible non-green boundary pixel instead of a setup-only failure.
    lhResult = lpDevice->SetTexture(0, lpTextureState->mpRaster->mpD3DTexture);
    if (FAILED(lhResult)) return SetupFailure("SetTexture production state", lhResult);
    lhResult = SetWrapDefaults(lpDevice);
    if (FAILED(lhResult)) return SetupFailure("reset sampler production state", lhResult);
    renderengine::ApplyXenonSamplerBlockPC(lpDevice, 0, lpTextureState->mauSamplerState);
    CheckSamplerContract("TextureState producer", lpDevice, luLodBiasBits);
    DWORD luProducerPixel = 0;
    lhResult = DrawAndReadPixel(lpDevice, lpTarget, lpReadback, &luProducerPixel);
    if (FAILED(lhResult)) return SetupFailure("TextureState volume LUT draw/readback", lhResult);
    PrintPixel("TextureState producer volume LUT green=1", luProducerPixel);
    Check(IsBrightGreen(luProducerPixel),
          "TextureState LINEAR/CLAMP producer does not wrap green=1 into the magenta LUT edge");

    renderengine::GetGraphicsSettingsPC().miAnisotropicFiltering = luInitialTextureFilterSetting;
    std::printf("PCTextureStateProducer: %u checks, %u failures\n", suChecks, suFailures);

    lpDevice->SetTexture(0, nullptr);
    lpShader->Release();
    lpReadback->Release();
    lpTarget->Release();
    lpVolume->Release();
    lpDevice->Release();
    lpApi->Release();
    DestroyWindow(lhWindow);
    return suFailures == 0 ? 0 : 1;
}
