#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include "pc/gcm/renderengine/ShaderBindingsPCLeaf.h"
#include "pc/gcm/renderengine/AssertFramePCLeaf.h"

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; }
static unsigned suChecks = 0, suFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++suChecks;
    if (!lbPass) { ++suFailures; std::printf("FAIL: %s\n", lpcName); }
}

static void Protocol()
{
    struct Device
    {
        unsigned muVertex = 0, muPixel = 0, muDeclaration = 0;
        bool mbFailVertex = false, mbFailPixel = false, mbFailDeclaration = false;
        HRESULT SetVertexShader(IDirect3DVertexShader9*)
        { ++muVertex; return mbFailVertex ? D3DERR_INVALIDCALL : S_OK; }
        HRESULT SetPixelShader(IDirect3DPixelShader9*)
        { ++muPixel; return mbFailPixel ? D3DERR_INVALIDCALL : S_OK; }
        HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9*)
        { ++muDeclaration; return mbFailDeclaration ? D3DERR_INVALIDCALL : S_OK; }
        HRESULT SetFVF(DWORD) { return S_OK; }
    } lFirst, lSecond;
    renderengine::PCShaderBindingCache lCache;
    auto* lpVs = reinterpret_cast<IDirect3DVertexShader9*>(uintptr_t(1));
    auto* lpPs = reinterpret_cast<IDirect3DPixelShader9*>(uintptr_t(2));
    auto* lpDecl = reinterpret_cast<IDirect3DVertexDeclaration9*>(uintptr_t(3));
    lCache.SetVertex(&lFirst, nullptr); lCache.SetVertex(&lFirst, nullptr);
    lCache.SetPixel(&lFirst, lpPs); lCache.SetPixel(&lFirst, lpPs);
    lCache.SetDeclaration(&lFirst, lpDecl); lCache.SetDeclaration(&lFirst, lpDecl);
    Check(lFirst.muVertex == 1 && lFirst.muPixel == 1 && lFirst.muDeclaration == 1,
          "identical successful states, including null shader, avoid native writes");
    lFirst.mbFailVertex = true;
    Check(FAILED(lCache.SetVertex(&lFirst, lpVs)), "vertex failure propagates");
    lFirst.mbFailVertex = false; lCache.SetVertex(&lFirst, lpVs);
    Check(lFirst.muVertex == 3, "failed vertex binding is retried");
    lFirst.mbFailPixel = true;
    Check(FAILED(lCache.SetPixel(&lFirst, nullptr)), "pixel failure propagates");
    lFirst.mbFailPixel = false; lCache.SetPixel(&lFirst, nullptr);
    Check(lFirst.muPixel == 3, "failed pixel binding is retried");
    lFirst.mbFailDeclaration = true;
    Check(FAILED(lCache.SetDeclaration(&lFirst, nullptr)), "declaration failure propagates");
    lFirst.mbFailDeclaration = false; lCache.SetDeclaration(&lFirst, nullptr);
    Check(lFirst.muDeclaration == 3, "failed declaration binding is retried");
    lCache.SetDeclaration(&lFirst, lpDecl);
    lCache.SetFVF(&lFirst, D3DFVF_XYZ);
    lCache.SetDeclaration(&lFirst, lpDecl);
    Check(lFirst.muDeclaration == 5, "FVF prevents reuse of the prior declaration");
    lCache.SetVertex(&lSecond, lpVs);
    lCache.SetVertex(&lFirst, lpVs);
    Check(lSecond.muVertex == 1 && lFirst.muVertex == 4, "device switch invalidates borrowed identities");
    lCache.Invalidate(); lCache.SetVertex(&lFirst, lpVs);
    Check(lFirst.muVertex == 5, "explicit restoration invalidation forces a native write");
    Check(FAILED(lCache.SetVertex(static_cast<Device*>(nullptr), lpVs)), "missing device is rejected");
}

static ID3DBlob* Compile(const char* lpcSource, const char* lpcTarget)
{
    ID3DBlob *lpCode = nullptr, *lpErrors = nullptr;
    const HRESULT lhResult = D3DCompile(lpcSource, std::strlen(lpcSource), nullptr, nullptr,
        nullptr, "main", lpcTarget, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &lpCode, &lpErrors);
    if (FAILED(lhResult)) std::printf("shader compile: %s\n", lpErrors
        ? static_cast<const char*>(lpErrors->GetBufferPointer()) : "no diagnostics");
    if (lpErrors) lpErrors->Release();
    return lpCode;
}

static unsigned Pixel(IDirect3DDevice9* lpDevice, unsigned luX)
{
    struct Vertex { float mafLeft[3], mafRight[3]; };
    const Vertex laVertices[] = {
        {{-.9f,-.6f,.5f},{ .1f,-.6f,.5f}},
        {{-.1f,-.6f,.5f},{ .9f,-.6f,.5f}},
        {{-.5f, .6f,.5f},{ .5f, .6f,.5f}},
    };
    lpDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff000000, 1, 0);
    lpDevice->BeginScene();
    const HRESULT lhDraw = lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, laVertices, sizeof(Vertex));
    lpDevice->EndScene();
    if (FAILED(lhDraw)) return 0;
    IDirect3DSurface9 *lpTarget = nullptr, *lpCopy = nullptr;
    unsigned luPixel = 0;
    if (SUCCEEDED(lpDevice->GetRenderTarget(0, &lpTarget))
        && SUCCEEDED(lpDevice->CreateOffscreenPlainSurface(64, 64, D3DFMT_X8R8G8B8,
            D3DPOOL_SYSTEMMEM, &lpCopy, nullptr))
        && SUCCEEDED(lpDevice->GetRenderTargetData(lpTarget, lpCopy)))
    {
        D3DLOCKED_RECT lLock = {};
        if (SUCCEEDED(lpCopy->LockRect(&lLock, nullptr, D3DLOCK_READONLY)))
        {
            std::memcpy(&luPixel, static_cast<const char*>(lLock.pBits) + 32 * lLock.Pitch + 4 * luX, 4);
            lpCopy->UnlockRect();
        }
    }
    if (lpCopy) lpCopy->Release();
    if (lpTarget) lpTarget->Release();
    return luPixel & 0x00ffffff;
}
static bool NativeVertex(IDirect3DDevice9* lpDevice, IDirect3DVertexShader9* lpExpected)
{
    IDirect3DVertexShader9* lpActual = nullptr;
    const bool lbMatch = SUCCEEDED(lpDevice->GetVertexShader(&lpActual)) && lpActual == lpExpected;
    if (lpActual) lpActual->Release();
    return lbMatch;
}
static bool NativePixel(IDirect3DDevice9* lpDevice, IDirect3DPixelShader9* lpExpected)
{
    IDirect3DPixelShader9* lpActual = nullptr;
    const bool lbMatch = SUCCEEDED(lpDevice->GetPixelShader(&lpActual)) && lpActual == lpExpected;
    if (lpActual) lpActual->Release();
    return lbMatch;
}
static bool NativeDeclaration(IDirect3DDevice9* lpDevice, IDirect3DVertexDeclaration9* lpExpected)
{
    IDirect3DVertexDeclaration9* lpActual = nullptr;
    const bool lbMatch = SUCCEEDED(lpDevice->GetVertexDeclaration(&lpActual)) && lpActual == lpExpected;
    if (lpActual) lpActual->Release();
    return lbMatch;
}

int main()
{
    using namespace renderengine;
    Protocol();
    WNDCLASSA lClass = {}; lClass.lpfnWndProc = DefWindowProcA;
    lClass.hInstance = GetModuleHandleA(nullptr); lClass.lpszClassName = "PCShaderBindings";
    RegisterClassA(&lClass);
    HWND lhWindow = CreateWindowA(lClass.lpszClassName, "test", WS_OVERLAPPED,
        0, 0, 64, 64, nullptr, nullptr, lClass.hInstance, nullptr);
    IDirect3D9* lpD3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* lpDevice = nullptr;
    D3DPRESENT_PARAMETERS lParameters = {};
    lParameters.Windowed = TRUE; lParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    lParameters.hDeviceWindow = lhWindow; lParameters.BackBufferWidth = 64;
    lParameters.BackBufferHeight = 64; lParameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    lParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (!lpD3d || FAILED(lpD3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, lhWindow,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &lParameters, &lpDevice)))
        return 2;
    ID3DBlob* lpVsA = Compile("float4 main(float4 p:POSITION0):POSITION0{return p;}", "vs_3_0");
    ID3DBlob* lpVsB = Compile("float4 main(float4 p:POSITION0):POSITION0{return p+float4(1,0,0,0);}", "vs_3_0");
    ID3DBlob* lpPsR = Compile("float4 main():COLOR0{return float4(1,0,0,1);}", "ps_3_0");
    ID3DBlob* lpPsG = Compile("float4 main():COLOR0{return float4(0,1,0,1);}", "ps_3_0");
    if (!lpVsA || !lpVsB || !lpPsR || !lpPsG) return 3;
    IDirect3DVertexShader9 *lpVertexA = nullptr, *lpVertexB = nullptr;
    IDirect3DPixelShader9 *lpRed = nullptr, *lpGreen = nullptr;
    IDirect3DVertexDeclaration9 *lpDeclA = nullptr, *lpDeclB = nullptr;
    D3DVERTEXELEMENT9 laElements[] = {{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},D3DDECL_END()};
    bool lbReady = SUCCEEDED(lpDevice->CreateVertexShader(static_cast<const DWORD*>(lpVsA->GetBufferPointer()), &lpVertexA))
        && SUCCEEDED(lpDevice->CreateVertexShader(static_cast<const DWORD*>(lpVsB->GetBufferPointer()), &lpVertexB))
        && SUCCEEDED(lpDevice->CreatePixelShader(static_cast<const DWORD*>(lpPsR->GetBufferPointer()), &lpRed))
        && SUCCEEDED(lpDevice->CreatePixelShader(static_cast<const DWORD*>(lpPsG->GetBufferPointer()), &lpGreen))
        && SUCCEEDED(lpDevice->CreateVertexDeclaration(laElements, &lpDeclA));
    laElements[0].Offset = 12;
    lbReady &= SUCCEEDED(lpDevice->CreateVertexDeclaration(laElements, &lpDeclB));
    if (!lbReady) return 4;
    lpDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    lpDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    gPCShaderBindingCache.Invalidate();
    PCSetVertexShader(lpDevice, lpVertexA); PCSetPixelShader(lpDevice, lpRed);
    PCSetVertexDeclaration(lpDevice, lpDeclA);
    const auto luCalls = gPCShaderBindingCache.mStatistics.muNativeCalls;
    for (unsigned lu = 0; lu < 10; ++lu)
    { PCSetVertexShader(lpDevice, lpVertexA); PCSetPixelShader(lpDevice, lpRed); PCSetVertexDeclaration(lpDevice, lpDeclA); }
    Check(gPCShaderBindingCache.mStatistics.muNativeCalls == luCalls && Pixel(lpDevice,16) == 0xff0000,
          "native repeated bindings retain red left-hand triangle without API writes");
    PCSetPixelShader(lpDevice, lpGreen);
    Check(NativePixel(lpDevice, lpGreen) && Pixel(lpDevice,16) == 0x00ff00, "pixel shader change reaches native pixels");
    PCSetVertexShader(lpDevice, lpVertexB);
    Check(NativeVertex(lpDevice, lpVertexB) && Pixel(lpDevice,48) == 0x00ff00, "vertex shader change moves the triangle");
    PCSetVertexShader(lpDevice, lpVertexA); PCSetVertexDeclaration(lpDevice, lpDeclB);
    Check(NativeDeclaration(lpDevice, lpDeclB) && Pixel(lpDevice,48) == 0x00ff00, "declaration change selects the other vertex coordinates");
    PCSetVertexDeclaration(lpDevice, lpDeclA);
    PCSetFVF(lpDevice, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    PCSetVertexDeclaration(lpDevice, lpDeclA);
    Check(NativeDeclaration(lpDevice, lpDeclA), "explicit declaration is restored after a fixed-function FVF");
    Check(Pixel(lpDevice,16) == 0x00ff00, "FVF transition preserves actual geometry after rebinding");

    PCSetPixelShader(lpDevice, lpRed);
    IDirect3DSurface9* lpBack = nullptr;
    lpDevice->GetRenderTarget(0, &lpBack);
    {
        PCAssertFrame lModal(lpDevice, lpBack);
        Check(lModal.IsReady(), "real assertion frame captures the pipeline");
        PCSetVertexShader(lpDevice, lpVertexB); PCSetPixelShader(lpDevice, lpGreen);
        PCSetVertexDeclaration(lpDevice, lpDeclB);
    }
    lpBack->Release();
    PCSetVertexShader(lpDevice, lpVertexB); PCSetPixelShader(lpDevice, lpGreen);
    PCSetVertexDeclaration(lpDevice, lpDeclB);
    Check(NativeVertex(lpDevice, lpVertexB) && NativePixel(lpDevice, lpGreen)
          && NativeDeclaration(lpDevice, lpDeclB), "state-block restoration invalidates all three cached identities");
    PCSetVertexDeclaration(lpDevice, lpDeclA);
    Check(Pixel(lpDevice,48) == 0x00ff00, "post-assert pipeline produces the requested green right-hand triangle");
    PCSetVertexShader(lpDevice, nullptr); PCSetPixelShader(lpDevice, nullptr);
    Check(NativeVertex(lpDevice, nullptr) && NativePixel(lpDevice, nullptr), "null unbind reaches both native shader stages");

    gPCShaderBindingCache.Invalidate();
    lpVertexA->Release(); lpVertexB->Release(); lpRed->Release(); lpGreen->Release();
    lpDeclA->Release(); lpDeclB->Release();
    lpVsA->Release(); lpVsB->Release(); lpPsR->Release(); lpPsG->Release();
    lpDevice->Release(); lpD3d->Release(); DestroyWindow(lhWindow);
    std::printf("PCShaderBindings: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
