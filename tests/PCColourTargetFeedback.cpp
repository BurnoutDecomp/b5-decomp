#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "pc/gcm/renderengine/device.h"
#include "pc/gcm/renderengine/DepthRangePCLeaf.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

static IDirect3DDevice9* spDevice;
static u32 suChecks, suFailures;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++suFailures; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace shadow {
IDirect3DDevice9* gpD3DDevice;
void* Device::mapSamplerTexture[KU_MAX_TEXTURE_STATES] = {};
const renderengine::TextureState* Device::mapTextureState[KU_MAX_TEXTURE_STATES] = {};
}
static auto& sapTextureShadow = shadow::Device::mapSamplerTexture;
static auto& sapWholeUnitShadow = shadow::Device::mapTextureState;
static IDirect3DDevice9* Dev() { return spDevice; }
static IDirect3DSurface9* AcquireNullColourSurface(u32, u32) { return nullptr; }
namespace renderengine {
void PCAlphaCoverage_Reconcile() {}
}
unsigned int D3DDevice_SetTexture(IDirect3DDevice9* device, u32 unit, void* texture, u32)
{
    device->SetTexture(unit, static_cast<IDirect3DBaseTexture9*>(texture));
    return 0;
}
#include "colour_feedback.inc"

static void Check(bool value, const char* name)
{
    ++suChecks;
    if (!value) { ++suFailures; std::printf("FAIL %s\n", name); }
}
static void Require(HRESULT result, const char* name)
{
    if (FAILED(result)) {
        std::printf("GPU SETUP FAILURE %s hr=%08X\n", name, unsigned(result));
        std::exit(2);
    }
}
static void Bind(u32 unit, IDirect3DBaseTexture9* texture)
{
    Require(spDevice->SetTexture(unit, texture), "bind sampler");
    sapTextureShadow[unit] = texture;
    sapWholeUnitShadow[unit] = reinterpret_cast<const renderengine::TextureState*>(1);
}
static IDirect3DBaseTexture9* Bound(u32 unit)
{
    IDirect3DBaseTexture9* texture = nullptr;
    Require(spDevice->GetTexture(unit, &texture), "read sampler");
    if (texture) texture->Release();
    return texture;
}
static void BeginTarget(IDirect3DSurface9* surface, u32 width, u32 height,
    IDirect3DSurface9* depth = nullptr)
{
    renderengine::RenderTargetState state = {};
    state.mpColourSurface = surface;
    state.mpDepthSurface = depth;
    state.muWidth = width; state.muHeight = height;
    renderengine::Device::SetState(&state);
    IDirect3DSurface9* actual = nullptr;
    Require(spDevice->GetRenderTarget(0, &actual), "read target");
    Check(actual == surface, "actual target is the requested surface");
    if (actual) actual->Release();
}
int main()
{
    HWND window = CreateWindowA("STATIC", "colour target feedback regression", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9* api = Direct3DCreate9(D3D_SDK_VERSION);
    if (!window || !api) return 2;
    D3DPRESENT_PARAMETERS present = {};
    present.Windowed = TRUE; present.hDeviceWindow = window;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.BackBufferWidth = present.BackBufferHeight = 64;
    present.BackBufferFormat = D3DFMT_A8R8G8B8;
    Require(api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &spDevice), "create device");
    shadow::gpD3DDevice = spDevice;
    IDirect3DSurface9* back = nullptr;
    Require(spDevice->GetRenderTarget(0, &back), "get backbuffer");
    IDirect3DTexture9 *bloom = nullptr, *work = nullptr, *unrelated = nullptr;
    Require(spDevice->CreateTexture(32, 32, 2, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &bloom, nullptr), "bloom target");
    Require(spDevice->CreateTexture(32, 32, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &work, nullptr), "work target");
    Require(spDevice->CreateTexture(32, 32, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &unrelated, nullptr), "unrelated texture");
    IDirect3DSurface9 *bloomSurface = nullptr, *workSurface = nullptr, *mipSurface = nullptr;
    Require(bloom->GetSurfaceLevel(0, &bloomSurface), "bloom surface");
    Require(bloom->GetSurfaceLevel(1, &mipSurface), "bloom mip surface");
    Require(work->GetSurfaceLevel(0, &workSurface), "work surface");
    for (u32 frame = 0; frame < 3; ++frame) {
        BeginTarget(back, 64, 64);
        Bind(0, bloom); Bind(1, bloom); Bind(7, unrelated); Bind(15, bloom);
        BeginTarget(bloomSurface, 32, 32);
        for (u32 unit : {0u, 1u, 15u}) {
            Check(Bound(unit) == nullptr, "bloom destination is unbound on every aliasing sampler");
            Check(sapTextureShadow[unit] == nullptr && sapWholeUnitShadow[unit] == nullptr,
                "texture and whole-unit shadow are invalidated together");
        }
        Check(Bound(2) == nullptr && Bound(3) == nullptr, "previously empty samplers remain empty");
        Check(Bound(7) == unrelated && sapWholeUnitShadow[7] != nullptr, "unrelated sampler remains bound");
        Bind(0, work);
        BeginTarget(workSurface, 32, 32);
        Check(Bound(0) == nullptr, "blur work destination cannot retain its previous sampled source");
        BeginTarget(back, 64, 64);
        Bind(1, bloom);
        Check(Bound(1) == bloom && sapWholeUnitShadow[1] != nullptr, "composite rebind is visible after target write");
    }
    BeginTarget(back, 64, 64); Bind(0, bloom);
    BeginTarget(mipSurface, 16, 16);
    Check(Bound(0) == nullptr, "writing another mip still excludes the entire texture from sampling");
    BeginTarget(back, 64, 64);
    sapTextureShadow[1] = nullptr;
    sapWholeUnitShadow[1] = reinterpret_cast<const renderengine::TextureState*>(1);
    Require(spDevice->SetTexture(1, bloom), "direct native bind with a null engine key");
    BeginTarget(bloomSurface, 32, 32);
    Check(Bound(1) == nullptr, "null engine key cannot suppress a native target-alias unbind");
    Check(sapTextureShadow[1] == nullptr && sapWholeUnitShadow[1] == nullptr,
        "native alias clearing invalidates the whole-unit key even with a null texture key");
    IDirect3DCubeTexture9* cube = nullptr;
    IDirect3DSurface9* cubeFace = nullptr;
    Require(spDevice->CreateCubeTexture(32, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &cube, nullptr), "cube target");
    Require(cube->GetCubeMapSurface(D3DCUBEMAP_FACE_NEGATIVE_Z, 0, &cubeFace), "cube face");
    BeginTarget(back, 64, 64); Bind(13, cube);
    BeginTarget(cubeFace, 32, 32);
    Check(Bound(13) == nullptr, "a cube face excludes its parent cube sampler");

    IDirect3DTexture9* rawDepth = nullptr;
    IDirect3DSurface9* rawDepthSurface = nullptr;
    Require(spDevice->CreateTexture(32, 32, 1, D3DUSAGE_DEPTHSTENCIL,
        static_cast<D3DFORMAT>(MAKEFOURCC('I','N','T','Z')), D3DPOOL_DEFAULT,
        &rawDepth, nullptr), "actual INTZ raw-depth texture");
    Require(rawDepth->GetSurfaceLevel(0, &rawDepthSurface), "raw-depth surface");
    BeginTarget(back, 64, 64);
    Bind(4, rawDepth);
    guRawDepthSamplerUnits |= 1u << 4; // texture-hook claim seam
    BeginTarget(workSurface, 32, 32); // no depth destination yet
    Check(Bound(4) == rawDepth, "a colour-only target preserves the depth source");
    sapTextureShadow[4] = nullptr;
    sapWholeUnitShadow[4] = reinterpret_cast<const renderengine::TextureState*>(1);
    Bind(7, unrelated);
    BeginTarget(workSurface, 32, 32, rawDepthSurface);
    Check(Bound(4) == nullptr, "null engine key cannot suppress an actual raw-depth alias unbind");
    Check(sapTextureShadow[4] == nullptr && sapWholeUnitShadow[4] == nullptr,
        "raw-depth alias clearing invalidates the whole-unit key despite the null texture key");
    Check(renderengine::PostFxDepthSampler_BoundUnitMask() == 0,
        "the production mask self-heals after the native depth unit is cleared");
    Check(Bound(7) == unrelated, "raw-depth clearing preserves unrelated texture units");
    BeginTarget(back, 64, 64);
    Bind(4, unrelated); guRawDepthSamplerUnits |= 1u << 4;
    BeginTarget(workSurface, 32, 32, rawDepthSurface);
    Check(Bound(4) == unrelated && sapWholeUnitShadow[4] != nullptr,
        "the production mask rejects a stale claim replaced with ordinary world texture");
    Check(renderengine::PostFxDepthSampler_BoundUnitMask() == 0,
        "an ordinary world binding removes its obsolete raw-depth claim");
    BeginTarget(back, 64, 64);
    for (u32 unit = 0; unit < 16; ++unit) spDevice->SetTexture(unit, nullptr);
    cubeFace->Release(); cube->Release(); mipSurface->Release();
    bloomSurface->Release(); workSurface->Release();
    bloom->Release(); work->Release(); unrelated->Release(); back->Release();
    rawDepthSurface->Release(); rawDepth->Release();
    spDevice->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCColourTargetFeedback: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
