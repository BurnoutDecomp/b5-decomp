#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "types.hpp"
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
namespace renderengine {
void PCSetSamplerState(IDirect3DDevice9* device, u32 unit, D3DSAMPLERSTATETYPE state, DWORD value)
{ device->SetSamplerState(unit, state, value); }
}
static bool gbPostFxSourceSamplerApplied;
static const u32 KU_RAW_DEPTH_MAX_SAMPLER_UNITS = 16;
static const u32 KU_POSTFX_SOURCE_FILTER_ANISO = 13;
static const u32 KU_POSTFX_SOURCE_FILTER_LINEAR = 1;
static u32 suChecks, suFailures;
static HRESULT QueryMotionCaps(IDirect3DDevice9*, D3DCAPS9*);
#include "motion_sampler.inc"
// Capability query is the fixture boundary. Texture state writes/readback use
// the actual device, including valid MIN anisotropy with LINEAR MAG. These
// masks cover adapters with no MAG support (observed in the live game log) and
// adapters with no anisotropy, even on a host whose driver advertises both.
static HRESULT QueryMotionCaps(IDirect3DDevice9* device, D3DCAPS9* caps)
{
    const HRESULT result = device->GetDeviceCaps(caps);
    if (MIN_ONLY) caps->TextureFilterCaps &= ~D3DPTFILTERCAPS_MAGFANISOTROPIC;
    if (NO_ANISO) caps->TextureFilterCaps &= ~(D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC);
    return result;
}
static void Check(bool value, const char* name)
{
    ++suChecks;
    if (!value) { ++suFailures; std::printf("FAIL %s\n", name); }
}
static DWORD Sampler(IDirect3DDevice9* device, D3DSAMPLERSTATETYPE type)
{
    DWORD value = 0;
    Check(SUCCEEDED(device->GetSamplerState(0, type, &value)), "native sampler state is readable");
    return value;
}
int main()
{
    HWND window = CreateWindowA("STATIC", "motion blur sampler regression", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9* api = Direct3DCreate9(D3D_SDK_VERSION);
    if (!window || !api) return 2;
    D3DPRESENT_PARAMETERS present = {};
    present.Windowed = TRUE; present.hDeviceWindow = window;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    IDirect3DDevice9* device = nullptr;
    if (FAILED(api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &device))) return 2;
    D3DCAPS9 caps = {};
    if (FAILED(QueryMotionCaps(device, &caps))) return 2;
    const bool minAniso = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0;
    const bool magAniso = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0;
    std::printf("effective caps minAniso=%d magAniso=%d MaxAnisotropy=%lu mask=%d/%d\n",
        minAniso, magAniso, caps.MaxAnisotropy, MIN_ONLY, NO_ANISO);
    for (u32 quality : {4u, 16u}) {
        ApplyPostFxSourceSamplerState(device, 0, KU_POSTFX_SOURCE_FILTER_ANISO, quality);
        Check(Sampler(device, D3DSAMP_MINFILTER) == (minAniso ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR),
            "supported anisotropic minification integrates the original streak footprint");
        Check(Sampler(device, D3DSAMP_MAGFILTER) == (minAniso && magAniso ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR),
            "magnification uses a device-supported filter");
        const u32 expected = minAniso ? (quality < caps.MaxAnisotropy ? quality : caps.MaxAnisotropy) : 1;
        Check(Sampler(device, D3DSAMP_MAXANISOTROPY) == expected,
            "cheap and expensive retain the console's four/sixteen sample budgets");
        Check(Sampler(device, D3DSAMP_ADDRESSU) == D3DTADDRESS_CLAMP &&
              Sampler(device, D3DSAMP_ADDRESSV) == D3DTADDRESS_CLAMP, "source stays clamped");
    }
    ApplyPostFxSourceSamplerState(device, 0, 0, 1);
    Check(Sampler(device, D3DSAMP_MINFILTER) == D3DTEXF_POINT &&
          Sampler(device, D3DSAMP_MAGFILTER) == D3DTEXF_POINT, "non-blurred point source remains point sampled");
    ApplyPostFxSourceSamplerState(device, 0, KU_POSTFX_SOURCE_FILTER_LINEAR, 1);
    Check(Sampler(device, D3DSAMP_MINFILTER) == D3DTEXF_LINEAR &&
          Sampler(device, D3DSAMP_MAGFILTER) == D3DTEXF_LINEAR, "linear source remains linear sampled");
    device->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCMotionBlurSampler: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
