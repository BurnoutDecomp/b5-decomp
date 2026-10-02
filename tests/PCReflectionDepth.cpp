// A native GPU regression for inverted reflection depth and the sky drawn last.
// Production functions are extracted by the runner; geometry is the fixture.
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#include "pc/gcm/renderengine/device.h"
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"
#include "pc/gcm/renderengine/DepthRangePCLeaf.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"

static IDirect3DDevice9* spDevice;
static u32 suChecks, suFailures;
static IDirect3DDevice9* Dev() { return spDevice; }
static void LogLine(const char*) {}
// These fixture seams have no depth semantics; SetState's real surface binds run.
static IDirect3DSurface9* AcquireNullColourSurface(u32, u32) { return nullptr; }
namespace renderengine {
    u32 PostFxDepthSampler_BoundUnitMask() { return 0; }
    void PCAlphaCoverage_Reconcile() {}
}
void* shadow::Device::SetResource(void*, u32) { return nullptr; }
#include "reflection_depth.inc"

static void Check(bool lbResult, const char* lpcName)
{
    ++suChecks;
    if (!lbResult) { ++suFailures; std::printf("FAIL %s\n", lpcName); }
}
static void Require(HRESULT lhResult, const char* lpcName)
{
    if (FAILED(lhResult))
    {
        std::printf("GPU SETUP/DRAW FAILURE %s hr=%08X\n", lpcName, unsigned(lhResult));
        std::exit(2);
    }
}
static void Viewport(bool lbInverted)
{
    float lafViewport[] = { 0, 0, 64, 64, lbInverted ? 1.f : 0.f,
                            lbInverted ? 0.f : 1.f, 0 };
    D3DDevice_SetViewportF(spDevice, lafViewport);
}
struct Vertex { f32 mfX, mfY, mfZ; DWORD mColour; };
static void Rectangle(f32 lfX, f32 lfHalfX, f32 lfHalfY, f32 lfZ, DWORD lColour)
{
    const Vertex laVertices[] = {
        { lfX-lfHalfX, -lfHalfY, lfZ, lColour }, { lfX-lfHalfX, lfHalfY, lfZ, lColour },
        { lfX+lfHalfX, -lfHalfY, lfZ, lColour }, { lfX+lfHalfX, lfHalfY, lfZ, lColour } };
    Require(spDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, laVertices, sizeof(Vertex)), "rectangle");
}
static DWORD Pixel(IDirect3DSurface9* lpSource, IDirect3DSurface9* lpRead, u32 luX, u32 luY)
{
    Require(spDevice->GetRenderTargetData(lpSource, lpRead), "readback");
    D3DLOCKED_RECT lLock = {};
    Require(lpRead->LockRect(&lLock, nullptr, D3DLOCK_READONLY), "lock readback");
    DWORD lColour;
    std::memcpy(&lColour, static_cast<char*>(lLock.pBits) + luY*lLock.Pitch + luX*4, 4);
    Require(lpRead->UnlockRect(), "unlock readback");
    return lColour & 0xFFFFFFu;
}
static void Clear(bool lbInverted, bool lbCombined)
{
    const renderengine::ClearDepthStencilParameters lDepth = { 0x30u, lbInverted ? 0.f : 1.f, 0 };
    if (lbCombined)
    {
        const renderengine::ClearColorParameters lColour = { { 0, 0, 0, 1 } };
        renderengine::Device::Clear(lColour, lDepth, renderengine::E_TARGETID_COLOUR_ALL);
    }
    else
    {
        Require(spDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF000000u, 1.f, 0), "colour clear");
        DeviceClearDepthStencil(&lDepth);
    }
}
static void DrawFace(bool lbInverted, bool lbNearFirst, bool lbRight, bool lbCombined,
                     bool lbSky = true, bool lbSetViewport = true)
{
    if (lbSetViewport) Viewport(lbInverted);
    D3DDevice_SetRenderState_ZFunc(spDevice, lbInverted ? 6u : 3u);
    spDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    spDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    Clear(lbInverted, lbCombined);
    D3DMATRIX lWorld = {};
    lWorld._11 = lWorld._22 = lWorld._33 = lWorld._44 = 1.f;
    const auto lProp = [&]() {
        // Move the prop via its WORLD TRANSFORM, as the prop dispatcher does.
        lWorld._41 = lbRight ? .5f : -.5f;
        Require(spDevice->SetTransform(D3DTS_WORLD, &lWorld), "moving prop transform");
        Rectangle(0.f, .2f, .3f, .25f, 0xFFFF0000u);
        lWorld._41 = 0.f;
        Require(spDevice->SetTransform(D3DTS_WORLD, &lWorld), "world transform");
    };
    Require(spDevice->BeginScene(), "begin face");
    if (lbNearFirst) lProp();
    Rectangle(0.f, .9f, .9f, .75f, 0xFF0000FFu);
    if (!lbNearFirst) lProp();
    if (lbSky)
    {
        if (lbInverted) ImDeviceSetDepthStencilState(&sSkyDomeEnvMapDepthStencilState);
        else
        {
            spDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            D3DDevice_SetRenderState_ZFunc(spDevice, 3u);
        }
        Rectangle(0.f, 1.f, 1.f, 1.f, 0xFF00FF00u); // sky submitted AFTER world and props
    }
    Require(spDevice->EndScene(), "end face");
}
int main()
{
    HWND lWindow = CreateWindowA("STATIC", "reflection depth regression", WS_OVERLAPPEDWINDOW,
                                0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9* lpApi = Direct3DCreate9(D3D_SDK_VERSION);
    if (!lpApi || !lWindow) return 2;
    D3DPRESENT_PARAMETERS lPresent = {};
    lPresent.Windowed = TRUE; lPresent.hDeviceWindow = lWindow;
    lPresent.SwapEffect = D3DSWAPEFFECT_DISCARD;
    lPresent.BackBufferWidth = lPresent.BackBufferHeight = 64;
    lPresent.BackBufferFormat = D3DFMT_X8R8G8B8;
    Require(lpApi->CreateDevice(0, D3DDEVTYPE_HAL, lWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &lPresent, &spDevice), "device");
    IDirect3DSurface9 *lpColour = nullptr, *lpRead = nullptr;
    Require(spDevice->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&lpColour,nullptr), "colour");
    Require(spDevice->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&lpRead,nullptr), "read surface");
    Require(spDevice->SetRenderTarget(0, lpColour), "bind colour");
    D3DMATRIX lIdentity = {};
    lIdentity._11 = lIdentity._22 = lIdentity._33 = lIdentity._44 = 1.f;
    for (D3DTRANSFORMSTATETYPE leTransform : { D3DTS_WORLD, D3DTS_VIEW, D3DTS_PROJECTION })
        Require(spDevice->SetTransform(leTransform, &lIdentity), "identity transform");
    Require(spDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE), "vertex format");
    spDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    spDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    spDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    spDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);

    Check(sSkyDomeEnvMapDepthStencilState.mbDepthTestEnable == TRUE
          && sSkyDomeEnvMapDepthStencilState.mbDepthWriteEnable == FALSE
          && sSkyDomeEnvMapDepthStencilState.meDepthFunction == D3DCMP_GREATEREQUAL,
          "ARTIST sky depth state (test=1, write=0, Xenos func=6)");
    for (D3DFORMAT leFormat : { D3DFMT_D24S8, D3DFMT_D16 })
    {
        IDirect3DSurface9* lpDepth = nullptr;
        Require(spDevice->CreateDepthStencilSurface(64,64,leFormat,D3DMULTISAMPLE_NONE,0,FALSE,&lpDepth,nullptr), "depth");
        Require(spDevice->SetDepthStencilSurface(lpDepth), "bind depth");
        // Isolate depth ordering from the independently incorrect sky state.
        for (bool lbInverted : { false, true })
        for (bool lbNearFirst : { false, true })
        {
            DrawFace(lbInverted, lbNearFirst, false, true, false);
            Check(Pixel(lpColour,lpRead,16,32) == 0xFF0000u, "nearest surface wins in either submission order without sky");
        }
        for (bool lbInverted : { false, true })
        for (bool lbNearFirst : { false, true })
        for (bool lbRight : { false, true })
        for (bool lbCombined : { false, true })
        {
            DrawFace(lbInverted, lbNearFirst, lbRight, lbCombined);
            Check(Pixel(lpColour,lpRead,lbRight ? 48 : 16,32) == 0xFF0000u, "prop survives later world and sky");
            Check(Pixel(lpColour,lpRead,lbRight ? 16 : 48,32) == 0x0000FFu, "previous prop position reveals world");
            Check(Pixel(lpColour,lpRead,32,32) == 0x0000FFu, "world survives sky");
            Check(Pixel(lpColour,lpRead,1,1) == 0x00FF00u, "sky fills cleared background");
        }
        // The actual envmap -> BeginRenderAntiAliased handoff changes target
        // through Device::SetState without issuing a viewport call afterwards.
        const renderengine::RenderTargetState lScene = { lpColour, lpDepth, 64, 64 };
        renderengine::Device::SetState(&lScene);
        DrawFace(false, true, false, true, true, false);
        Check(Pixel(lpColour,lpRead,16,32) == 0xFF0000u, "main scene prop survives implicit viewport reset");
        Check(Pixel(lpColour,lpRead,32,32) == 0x0000FFu, "main scene world survives implicit viewport reset");
        Check(Pixel(lpColour,lpRead,1,1) == 0x00FF00u, "main scene sky fills only background after reflection pass");
        spDevice->SetDepthStencilSurface(nullptr);
        lpDepth->Release();
    }
    // The engine can keep a cached state pointer while changing render targets.
    // A viewport transition must still retranslate the LAST LOGICAL comparison.
    const DWORD lauReverse[] = { D3DCMP_NEVER, D3DCMP_GREATER, D3DCMP_EQUAL,
        D3DCMP_GREATEREQUAL, D3DCMP_LESS, D3DCMP_NOTEQUAL, D3DCMP_LESSEQUAL, D3DCMP_ALWAYS };
    for (u32 luFunc = 0; luFunc < 8; ++luFunc)
    {
        BeginNormalTargetViewport(64,64);
        D3DDevice_SetRenderState_ZFunc(spDevice, luFunc);
        DWORD luGot = 0;
        spDevice->GetRenderState(D3DRS_ZFUNC,&luGot);
        Check(luGot == luFunc+1, "normal target preserves depth function");
        Viewport(true);
        spDevice->GetRenderState(D3DRS_ZFUNC,&luGot);
        Check(luGot == lauReverse[luFunc], "inverted target remaps cached depth function");
        const renderengine::RenderTargetState lTarget = { lpColour, nullptr, 64, 64 };
        renderengine::Device::SetState(&lTarget); // main scene bind has no explicit viewport
        spDevice->GetRenderState(D3DRS_ZFUNC,&luGot);
        Check(luGot == luFunc+1, "main scene target bind resets inverted depth mode");
        Viewport(true);
        BeginNormalTargetViewport(64,64);
        spDevice->GetRenderState(D3DRS_ZFUNC,&luGot);
        Check(luGot == luFunc+1, "normal target restores cached depth function");
    }
    lpRead->Release(); lpColour->Release(); spDevice->Release(); lpApi->Release(); DestroyWindow(lWindow);
    std::printf("PCReflectionDepth: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
