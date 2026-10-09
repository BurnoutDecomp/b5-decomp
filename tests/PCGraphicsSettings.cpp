#include <Windows.h>
#include <d3d9.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "pc/gcm/renderengine/GraphicsSettingsPCLeaf.h"

namespace renderengine {
    bool gFullscreen = false;
    s32 gDisplayWidth = 640, gDisplayHeight = 360, gAdapterIndex = 0, gVSync = 1;
    s32 gAntiAliasing = 0, gAlphaToCoverage = 1, gEnvironmentMap = 1;
    s32 gEnvironmentMap30Hz = 1, gCoronas = 1, gSunCorona = 1;
    IDirect3DDevice9* gDevice = nullptr;
}
namespace BrnGame { bool gbDecoupleSimulationFromRenderRate = true; }
namespace BrnWorld { f32 KA_VEHICLE_QUALITY_LOD_DISTANCE[5] = {10,22,35,50,70}; }
namespace CgsGraphics { struct Model { enum State { E_STATE_LOD_0, E_STATE_LOD_1, E_STATE_LOD_2, E_STATE_LOD_3, E_STATE_LOD_4 }; }; }
namespace CgsDev { namespace Log { void WriteToLog(const char* lpcText) { std::printf("%s", lpcText); } } }
f32 gfBloomLuminanceScale = 1.0f;
static const u32 KU_NUM_VEHICLE_LODS = 5;
static char gacTestDirectory[MAX_PATH];
static char gacTestIni[MAX_PATH];
static IDirect3DDevice9* Dev() { return renderengine::gDevice; }
static void getGameSaveDir(char* lpcPath, const char* lpcFile)
{
    std::snprintf(lpcPath, MAX_PATH, "%s%s", gacTestDirectory, lpcFile);
}
#include "pc_graphics_settings.inc"

static int giChecks, giFailures;
static void Check(bool lbGood, const char* lpcMessage)
{
    ++giChecks;
    if (!lbGood) { ++giFailures; std::printf("FAIL %s\n", lpcMessage); }
}
static void Set(const char* lpcKey, const char* lpcValue)
{
    WritePrivateProfileStringA("Graphics", lpcKey, lpcValue, gacTestIni);
}
static void CheckTable(const float* lpafExpected)
{
    for (u32 luLod = 0; luLod < 5u; ++luLod)
        Check(BrnWorld::KA_VEHICLE_QUALITY_LOD_DISTANCE[luLod] == lpafExpected[luLod], "exact vehicle LOD distance reaches engine global");
}

static void ConfigChecks()
{
    LoadConfig();
    const auto& lrGraphics = renderengine::GetGraphicsSettingsPC();
    Check(gfBloomLuminanceScale == 1.0f && lrGraphics.miEnvironmentMapLod == 2 && !lrGraphics.mbTrafficShadows,
          "missing graphics section preserves console defaults");
    Check(lrGraphics.miWorldLodOverrideDistance == 0 && lrGraphics.miPropLodOverrideDistance == 0,
          "absent overrides use asset LOD distances");
    Check(lrGraphics.miAnisotropicFiltering == 1, "missing filtering option preserves original trilinear sampling");
    bool lbOverride = false;
    s32 laiDistances[3] = {300,600,900};
    lrGraphics.ApplyLodOverride(0, lbOverride, laiDistances);
    Check(!lbOverride && laiDistances[0] == 300 && laiDistances[2] == 900, "default does not enable distance overrides");

    // Reference float32 bit patterns, decoded independently of production's decimal tables.
    // High's LOD3/4 distances are 150/210.
    const u32 kaauExpectedBits[6][5] =
    {
        {0x41200000,0x41b00000,0x420c0000,0x42480000,0x428c0000},
        {0x3f800000,0x40000000,0x40800000,0x40c00000,0x41200000},
        {0x40a00000,0x41300000,0x41880000,0x41c80000,0x420c0000},
        {0x41a00000,0x42300000,0x428c0000,0x42c80000,0x430c0000},
        {0x41f00000,0x42840000,0x42d20000,0x43160000,0x43520000},
        {0x42480000,0x42dc0000,0x432f0000,0x437a0000,0x43af0000}
    };
    float kaafExpected[6][5];
    std::memcpy(kaafExpected,kaauExpectedBits,sizeof(kaafExpected));
    const char* kapcPresets[6] = {"Default","Potato","Low","medium","High","Ultra"};
    for (u32 luPreset = 0; luPreset < 6u; ++luPreset)
    {
        Set("VehicleLODPreset", kapcPresets[luPreset]); LoadConfig(); CheckTable(kaafExpected[luPreset]);
        const float lfBoundary = kaafExpected[luPreset][0];
        Check(ClassifyVehicleLOD(lfBoundary - 0.25f, BrnWorld::KA_VEHICLE_QUALITY_LOD_DISTANCE) == CgsGraphics::Model::E_STATE_LOD_0,
              "selected preset keeps LOD0 up to its distance");
        Check(ClassifyVehicleLOD(lfBoundary, BrnWorld::KA_VEHICLE_QUALITY_LOD_DISTANCE) == CgsGraphics::Model::E_STATE_LOD_1,
              "original strict LOD boundary remains intact");
    }
    Set("BloomLuminanceScale","0.3"); Set("EnvironmentMapLOD","0"); Set("TrafficShadows","1");
    Set("AnisotropicFiltering","16");
    Set("WorldLODOverrideDistance","3000"); Set("PropLODOverrideDistance","3000");
    WritePrivateProfileStringA("Settings","AntiAliasing","8",gacTestIni);
    LoadConfig();
    Check(gfBloomLuminanceScale == 0.3f && lrGraphics.miEnvironmentMapLod == 0 && lrGraphics.mbTrafficShadows,
          "configured bloom/reflection/shadow settings reach native state");
    lrGraphics.ApplyLodOverride(lrGraphics.miWorldLodOverrideDistance, lbOverride, laiDistances);
    Check(lbOverride && laiDistances[0] == 3000 && laiDistances[1] == 6000 && laiDistances[2] == 9000,
          "extended world LOD override uses all three multiplied bands");
    lrGraphics.ApplyLodOverride(lrGraphics.miPropLodOverrideDistance, lbOverride, laiDistances);
    Check(lbOverride && laiDistances[0] == 3000 && laiDistances[2] == 9000, "extended prop LOD distance bands");
    Set("WorldLODOverrideDistance","1"); Set("PropLODOverrideDistance","50"); LoadConfig();
    lrGraphics.ApplyLodOverride(lrGraphics.miWorldLodOverrideDistance, lbOverride, laiDistances);
    Check(laiDistances[0] == 1 && laiDistances[2] == 3, "short world LOD distance bands");
    lrGraphics.ApplyLodOverride(lrGraphics.miPropLodOverrideDistance, lbOverride, laiDistances);
    Check(laiDistances[0] == 50 && laiDistances[2] == 150, "short prop LOD distance bands");
    Set("VehicleLOD0Distance","12.5"); Set("VehicleLODPreset","Default"); LoadConfig(); SaveConfig();
    CheckTable(kaafExpected[0]);
    char lacValue[128];
    GetPrivateProfileStringA("Graphics","VehicleLOD0Distance","",lacValue,sizeof(lacValue),gacTestIni);
    Check(std::strcmp(lacValue,"12.5") == 0, "named presets preserve inactive custom values");
    Set("VehicleLODPreset","Custom"); LoadConfig();
    const float kafCustom[5] = {12.5f,22,35,50,70}; CheckTable(kafCustom);
    WritePrivateProfileStringA("Unrelated","Keep","yes",gacTestIni);
    SaveConfig(); renderengine::GetGraphicsSettingsPC() = renderengine::GraphicsSettingsPC(); LoadConfig();
    CheckTable(kafCustom);
    Check(gfBloomLuminanceScale == 0.3f && lrGraphics.mbTrafficShadows && lrGraphics.miEnvironmentMapLod == 0,
          "graphics values survive normal save/reload");
    Check(lrGraphics.miAnisotropicFiltering == 16, "texture filtering survives normal save/reload");
    Check(renderengine::gAntiAliasing == 8 && MultisampleOverrideSampleCount() == 8, "8x AA persists through real LoadConfig and SaveConfig");
    GetPrivateProfileStringA("Unrelated","Keep","",lacValue,sizeof(lacValue),gacTestIni);
    Check(std::strcmp(lacValue,"yes") == 0, "save retains unrelated INI data");
    Set("VehicleLOD1Distance","1"); LoadConfig(); CheckTable(kaafExpected[0]);
    Check(lrGraphics.meVehicleLodPreset == renderengine::E_VEHICLE_LOD_DEFAULT, "decreasing custom table is rejected");
    for (const char* lpcInvalid : {"NaN","inf","-1","11","0.3garbage",""})
    {
        Set("BloomLuminanceScale",lpcInvalid); LoadConfig();
        Check(gfBloomLuminanceScale == 1.0f, "invalid bloom value uses original default");
    }
    Set("EnvironmentMapLOD","3"); Set("TrafficShadows","-1");
    for (const char* lpcInvalid : {"0", "3", "17", "NaN", "16garbage", ""})
    {
        Set("AnisotropicFiltering", lpcInvalid); LoadConfig();
        Check(lrGraphics.miAnisotropicFiltering == 1, "invalid anisotropy falls back to original filtering");
    }
    Set("WorldLODOverrideDistance","3000.5"); Set("PropLODOverrideDistance","999999999999999999999");
    Set("VehicleLODPreset","unknown"); LoadConfig();
    Check(lrGraphics.miEnvironmentMapLod == 2 && !lrGraphics.mbTrafficShadows &&
          lrGraphics.miWorldLodOverrideDistance == 0 && lrGraphics.miPropLodOverrideDistance == 0,
          "invalid integers cannot wrap or accidentally enable graphics changes");
    CheckTable(kaafExpected[0]);
}

static void NativeAA()
{
    IDirect3D9* lpD3D = Direct3DCreate9(D3D_SDK_VERSION);
    Check(lpD3D != nullptr, "native D3D9 available"); if (!lpD3D) return;
    HWND lhWindow = CreateWindowExA(0,"STATIC","Graphics settings AA regression",WS_OVERLAPPEDWINDOW,0,0,200,160,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    D3DPRESENT_PARAMETERS lPresent = {};
    lPresent.Windowed = TRUE; lPresent.hDeviceWindow = lhWindow; lPresent.SwapEffect = D3DSWAPEFFECT_DISCARD;
    lPresent.BackBufferWidth = 128; lPresent.BackBufferHeight = 128; lPresent.BackBufferFormat = D3DFMT_X8R8G8B8;
    lPresent.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    const HRESULT lhCreate = lpD3D->CreateDevice(0,D3DDEVTYPE_HAL,lhWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&lPresent,&renderengine::gDevice);
    Check(SUCCEEDED(lhCreate), "native HAL device created");
    if (FAILED(lhCreate)) { DestroyWindow(lhWindow); lpD3D->Release(); return; }
    Check(MultisampleSampleCountForConsoleFormat(1) == 2 && MultisampleSampleCountForConsoleFormat(2) == 4,
          "console multisample formats mean 2x/4x, not raw enum samples");
    for (const int liRequest : {0,1,2,4,8,16})
    {
        renderengine::gAntiAliasing = liRequest;
        const u32 luOverride = MultisampleOverrideSampleCount();
        Check(luOverride == (liRequest == 0 ? 0u : (liRequest > 8 ? 8u : static_cast<u32>(liRequest))),
              "native AA override maps default/off/sample count and existing 8x maximum");
        const u32 luRequest = luOverride == 0 ? MultisampleSampleCountForConsoleFormat(1) : luOverride;
        const MultisampleChoice lChoice = ChooseMultisampleType(luRequest,D3DFMT_A8R8G8B8);
        Check(lChoice.muSamples >= 1 && lChoice.muSamples <= luRequest, "unsupported AA requests fall back downward");
        DWORD luColourLevels = 0, luDepthLevels = 0;
        if (lChoice.muSamples > 1)
        {
            Check(SUCCEEDED(lpD3D->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,TRUE,lChoice.meType,&luColourLevels)) &&
                  SUCCEEDED(lpD3D->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,lChoice.meDepthFormat,TRUE,lChoice.meType,&luDepthLevels)),
                  "AA choice supports both actual surface formats");
        }
        DWORD luEightLevels = 0;
        if (liRequest == 8 && SUCCEEDED(lpD3D->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,TRUE,D3DMULTISAMPLE_8_SAMPLES,&luEightLevels)) &&
            SUCCEEDED(lpD3D->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_D24S8,TRUE,D3DMULTISAMPLE_8_SAMPLES,&luEightLevels)))
            Check(lChoice.muSamples == 8, "8x-capable hardware really selects 8x");
        IDirect3DSurface9* lpColour = CreateMultisampledColourSurface(128,128,D3DFMT_A8R8G8B8,lChoice);
        IDirect3DSurface9* lpDepth = CreateMultisampledDepthSurface(128,128,lChoice);
        Check(lpColour && lpDepth, "production AA surface pair allocates");
        if (lpColour && lpDepth)
        {
            D3DSURFACE_DESC lColour = {}, lDepth = {}; lpColour->GetDesc(&lColour); lpDepth->GetDesc(&lDepth);
            Check(lColour.MultiSampleType == lChoice.meType && lDepth.MultiSampleType == lChoice.meType &&
                  lColour.MultiSampleQuality == lDepth.MultiSampleQuality, "colour/depth use matching sample count and quality");
            auto* lpDevice = renderengine::gDevice;
            Check(SUCCEEDED(lpDevice->SetRenderTarget(0,lpColour)) && SUCCEEDED(lpDevice->SetDepthStencilSurface(lpDepth)), "matching AA surfaces bind");
            const D3DVIEWPORT9 lViewport = {0,0,128,128,0,1}; lpDevice->SetViewport(&lViewport);
            Check(SUCCEEDED(lpDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0)), "native AA target clears");
            struct Vertex { float x,y,z,rhw; DWORD colour; };
            const Vertex kaVertices[3] = {{8.25f,8.25f,0.5f,1,0xffffffff},{110.25f,20.25f,0.5f,1,0xffffffff},{20.25f,110.25f,0.5f,1,0xffffffff}};
            lpDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE); lpDevice->SetRenderState(D3DRS_LIGHTING,FALSE);
            lpDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE); lpDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,TRUE);
            lpDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
            lpDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
            const HRESULT lhBegin = lpDevice->BeginScene();
            const HRESULT lhDraw = lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,kaVertices,sizeof(Vertex));
            const HRESULT lhEnd = lpDevice->EndScene();
            Check(SUCCEEDED(lhBegin) && SUCCEEDED(lhDraw) && SUCCEEDED(lhEnd), "native AA triangle draws");
            IDirect3DSurface9* lpResolved = nullptr; IDirect3DSurface9* lpRead = nullptr;
            lpDevice->CreateRenderTarget(128,128,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&lpResolved,nullptr);
            lpDevice->CreateOffscreenPlainSurface(128,128,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&lpRead,nullptr);
            Check(lpResolved && lpRead, "AA readback targets allocate");
            if (lpResolved && lpRead)
            {
                const HRESULT lhResolve = lpDevice->StretchRect(lpColour,nullptr,lpResolved,nullptr,D3DTEXF_NONE);
                Check(SUCCEEDED(lhResolve) && SUCCEEDED(lpDevice->GetRenderTargetData(lpResolved,lpRead)), "MSAA colour resolves and reads back");
                D3DLOCKED_RECT lLocked = {}; bool lbCoverage = false, lbWhite = false;
                if (SUCCEEDED(lpRead->LockRect(&lLocked,nullptr,D3DLOCK_READONLY)))
                {
                    for (u32 luY = 0; luY < 128; ++luY)
                    {
                        const auto* lpRow = reinterpret_cast<const DWORD*>(static_cast<const char*>(lLocked.pBits) + luY*lLocked.Pitch);
                        for (u32 luX = 0; luX < 128; ++luX)
                        { const DWORD luRgb = lpRow[luX]&0xffffff; lbWhite |= luRgb == 0xffffff; lbCoverage |= luRgb != 0 && luRgb != 0xffffff; }
                    }
                    lpRead->UnlockRect();
                }
                Check(lbWhite && (lChoice.muSamples == 1 ? !lbCoverage : lbCoverage), "resolved pixels show real multisample edge coverage");
            }
            if (lpRead) lpRead->Release(); if (lpResolved) lpResolved->Release();
            lpDevice->SetDepthStencilSurface(nullptr);
        }
        std::printf("AA requested=%u actual=%u\n",luRequest,lChoice.muSamples);
        if (lpColour) lpColour->Release(); if (lpDepth) lpDepth->Release();
    }
    renderengine::gDevice->Release(); renderengine::gDevice = nullptr;
    DestroyWindow(lhWindow); lpD3D->Release();
}

int main()
{
    GetTempPathA(MAX_PATH,gacTestDirectory);
    const size_t luLength = std::strlen(gacTestDirectory);
    std::snprintf(gacTestDirectory + luLength,MAX_PATH-luLength,"brn_graphics_%lu\\",GetCurrentProcessId());
    CreateDirectoryA(gacTestDirectory,nullptr); getGameSaveDir(gacTestIni,"config.ini");
    ConfigChecks(); NativeAA();
    DeleteFileA(gacTestIni); RemoveDirectoryA(gacTestDirectory);
    std::printf("PCGraphicsSettings: %d checks, %d failures\n",giChecks,giFailures);
    return giFailures ? 1 : 0;
}
