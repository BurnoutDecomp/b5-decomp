#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <array>
#include <map>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "pc/gcm/renderengine/NvApiResourceRegistryPCLeaf.h"
using u32 = std::uint32_t;
extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; }
static unsigned checks, failures, registrations, unregistrations, orderingFailures;
static void Check(bool ok, const char* text)
{ ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", text); } }
struct Resource {
    unsigned refs = 1, identity = 0;
    bool refuseRegister = false, refuseUnregister = false;
    void AddRef() { ++refs; }
    void Release();
};
static std::map<Resource*, unsigned> registered;
void Resource::Release()
{
    if (--refs == 0 && registered.count(this)) ++orderingFailures;
}
static int __cdecl Register(Resource* p)
{
    ++registrations;
    if (p->refuseRegister) return -1;
    if (registered.count(p)) { ++orderingFailures; return -1; }
    registered[p] = p->identity;
    return 0;
}
static int __cdecl Unregister(Resource* p)
{
    ++unregistrations;
    if (!p->refs || !registered.count(p)) { ++orderingFailures; return -1; }
    if (p->refuseUnregister) return -1;
    registered.erase(p);
    return 0;
}
static void Contract()
{
    std::array<Resource, 17> resources;
    renderengine::NvApiResourceRegistryPC<Resource> cache;
    Check(!cache.Register(&resources[0]), "unconfigured registry cannot pretend registration succeeded");
    Check(cache.Configure(Register, Unregister), "registration requires paired operations");
    bool complete = true;
    for (unsigned i = 0; i < resources.size(); ++i) {
        resources[i].identity = i;
        complete &= cache.Register(&resources[i]);
    }
    Check(complete && registered.size() == 17, "more than eight live resources can be registered");
    for (auto& resource : resources) complete &= cache.Register(&resource);
    Check(complete && registrations == 17, "every live registration is remembered and duplicates are skipped");
    bool owned = true;
    for (auto& resource : resources) { owned &= resource.refs == 2; resource.Release(); }
    Check(owned && orderingFailures == 0, "registry owns identity when an external owner releases it");
    Check(cache.Clear() && registered.empty() && unregistrations == 17,
          "all successful registrations retire, including overflow and unused variants");
    bool released = true;
    for (auto& resource : resources) released &= resource.refs == 0;
    Check(released && orderingFailures == 0, "unregistration precedes the last COM release");

    Resource& reused = resources[0]; reused.refs = 1; reused.identity = 700;
    const unsigned before = registrations;
    Check(cache.Register(&reused) && registrations == before + 1 && registered[&reused] == 700,
          "a new lifetime at an old address is registered again");
    Check(cache.Clear() && reused.refs == 1, "retirement preserves the external owner reference");
    Resource& refused = resources[1]; refused.refs = 1; refused.refuseRegister = true;
    Check(!cache.Register(&refused) && refused.refs == 1 && registered.empty(),
          "failed registration creates neither ownership nor a cached success");
    refused.refuseRegister = false;
    cache.Register(&refused); refused.Release(); refused.refuseUnregister = true;
    Check(!cache.Clear() && refused.refs == 1 && registered.count(&refused),
          "failed unregistration retains a live identity and reports failure");
    refused.refuseUnregister = false;
    Check(cache.Clear() && refused.refs == 0 && registered.empty(),
          "a later retirement retries and releases the retained resource");
    const unsigned cleared = unregistrations;
    Check(cache.Clear() && unregistrations == cleared, "empty retirement does no driver work");
}

// Existing rendered-pixel oracle, plus the actual game NVAPI loader/resolve.
#include "pc_nvapi_sampler.inc"
namespace renderengine {
static unsigned suDepthResolveFailures;
static void LogOnce(const char*, const char* text) { std::fputs(text, stdout); }
static void LogRepeating(unsigned& counter, const char* text) { ++counter; std::puts(text); }
#include "pc_nvapi_resolve.inc"
}
static void Native()
{
    HWND window = CreateWindowA("STATIC", "NVAPI lifecycle test", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9Ex* api = nullptr;
    IDirect3DDevice9Ex* device = nullptr;
    D3DPRESENT_PARAMETERS params{};
    params.Windowed = TRUE; params.SwapEffect = D3DSWAPEFFECT_FLIPEX;
    params.BackBufferCount = 2; params.BackBufferWidth = params.BackBufferHeight = 64;
    params.BackBufferFormat = D3DFMT_X8R8G8B8; params.hDeviceWindow = window;
    params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &api);
    if (SUCCEEDED(hr)) hr = api->CreateDeviceEx(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &params, nullptr, &device);
    Check(SUCCEEDED(hr) && device, "native D3D9Ex device created");
    if (!device) { if (api) api->Release(); DestroyWindow(window); return; }
    Check(renderengine::TilingNvApiReady(), "official paired NVAPI functions are available");
    if (renderengine::TilingNvApiReady()) {
        Sampler sample(device);
        bool allPixels = true, allRetired = true, allReset = true;
        unsigned generations = 0;
        for (unsigned i = 0; i < 12; ++i) {
            const unsigned size = 64 + i * 8;
            IDirect3DSurface9 *colour = nullptr, *depth = nullptr;
            IDirect3DTexture9* resolved = nullptr;
            hr = device->CreateRenderTarget(size, size, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_8_SAMPLES, 0, FALSE, &colour, nullptr);
            if (SUCCEEDED(hr)) hr = device->CreateDepthStencilSurface(size, size, D3DFMT_D24S8,
                D3DMULTISAMPLE_8_SAMPLES, 0, FALSE, &depth, nullptr);
            if (SUCCEEDED(hr)) hr = device->CreateTexture(size, size, 1, D3DUSAGE_DEPTHSTENCIL,
                static_cast<D3DFORMAT>(MAKEFOURCC('I','N','T','Z')), D3DPOOL_DEFAULT, &resolved, nullptr);
            if (FAILED(hr)) {
                std::printf("surface allocation hr=%08x\n", unsigned(hr));
                allPixels = false;
            } else {
                device->SetDepthStencilSurface(nullptr); device->SetRenderTarget(0, colour);
                device->SetDepthStencilSurface(depth);
                device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 0.25f, 0);
                allPixels &= renderengine::TilingResolveDepthViaNvApi(device, resolved);
                // Retire and reset BEFORE any readback can drain GPU work.
                allRetired &= renderengine::gNvApiDepthResourcesPC.Clear();
                depth->Release(); depth = nullptr;
                colour->Release(); colour = nullptr;
                if (allRetired) {
                    D3DPRESENT_PARAMETERS reset = params;
                    allReset &= SUCCEEDED(device->ResetEx(&reset, nullptr));
                    const float coord[] = {0.5f, 0.5f, 0, 0};
                    if (allReset) allPixels &= sample.Pixel(resolved, 0, coord, 0x00400000u, 0x00FF0000u);
                }
                ++generations;
            }
            if (resolved) resolved->Release(); if (depth) depth->Release(); if (colour) colour->Release();
            if (!allPixels || !allRetired || !allReset) break;
        }
        Check(generations == 12 && allPixels && renderengine::suDepthResolveFailures == 0,
              "twelve resized 8x MSAA depth generations resolve correct GPU pixels");
        Check(allRetired, "all source, texture and subresource NVAPI registrations retire successfully");
        Check(allReset, "native reset after retirement preserves the sampling pipeline");
        renderengine::gNvApiDepthResourcesPC.Clear();
    }
    device->Release(); api->Release(); DestroyWindow(window);
}
int main()
{
    Contract();
    if (!failures) Native();
    std::printf("PCNvApiResourceRegistry: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
