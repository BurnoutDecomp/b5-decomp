#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

static IDirect3DDevice9* spDevice;
static u32 suChecks, suFailures, suDraws;
static f32 safExpected[32 * 16];
static IDirect3DDevice9* Dev() { return spDevice; }
static void LogLine(const char*) {}
namespace renderengine {
u32 guDiagDraws;
void WorldDraw_MarkImmediateMode() {}
void PCSetVertexShaderConstantF(IDirect3DDevice9* device, u32 index, const f32* values, u32 count)
{ device->SetVertexShaderConstantF(index, values, count); }
void PCSetPixelShaderConstantF(IDirect3DDevice9* device, u32 index, const f32* values, u32 count)
{ device->SetPixelShaderConstantF(index, values, count); }
void WorldDraw_IndexedUP(u32, u32, u32, u32);
void WorldDraw_NonIndexedUP(u32, u32, u32);
}
struct DebrisSetterFixture {
    static const u32 KU_NUM_TRANSFORMS = 32;
    renderengine::ProgramVariableHandle mTransformArrayStateHandle;
    void SetTransformArray(const Matrix44*);
};
#include "effect_constants.inc"
static void Check(bool value, const char* name)
{
    ++suChecks;
    if (!value) { ++suFailures; std::printf("FAIL %s\n", name); }
}
static void DrawBoundary()
{
    f32 actual[32 * 16] = {};
    const HRESULT result = spDevice->GetVertexShaderConstantF(0, actual, 128);
    Check(SUCCEEDED(result), "read native transform constant file");
    Check(suStagedCount == 0, "draw consumes pending rows after the debris setter");
    Check(std::memcmp(actual, safExpected, sizeof(actual)) == 0,
        "every transform belongs to this batch at the draw boundary");
    ++suDraws;
}
void renderengine::WorldDraw_IndexedUP(u32, u32, u32, u32) { DrawBoundary(); }
void renderengine::WorldDraw_NonIndexedUP(u32, u32, u32) { DrawBoundary(); }

int main()
{
    HWND window = CreateWindowA("STATIC", "effect constant regression", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    IDirect3D9* api = Direct3DCreate9(D3D_SDK_VERSION);
    if (!window || !api) return 2;
    D3DPRESENT_PARAMETERS present = {};
    present.Windowed = TRUE; present.hDeviceWindow = window;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    const HRESULT result = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &spDevice);
    if (FAILED(result)) return 2;
    DebrisSetterFixture setter = {};
    setter.mTransformArrayStateHandle.mu8RegisterSet = 0;
    setter.mTransformArrayStateHandle.mu8RegisterCount = 128;
    setter.mTransformArrayStateHandle.mu8ShaderType = 0;
    Matrix44 transforms[32];
    for (u32 array = 0; array < 3; ++array) {
        // RenderDebrisArray's mesh flush occurs before this bucket's writes.
        renderengine::ImShaderConstants_Flush();
        for (u32 batch = 0; batch < 4; ++batch) {
            f32* values = reinterpret_cast<f32*>(transforms);
            for (u32 lane = 0; lane < 32 * 16; ++lane)
                safExpected[lane] = values[lane] = f32(array * 10000 + batch * 1000 + lane);
            setter.SetTransformArray(transforms);
            D3DDevice_DrawIndexedVertices(spDevice, 4, 0, 0, 192);
        }
    }
    for (u32 lane = 0; lane < 32 * 16; ++lane)
        safExpected[lane] = reinterpret_cast<f32*>(transforms)[lane] = f32(-int(lane) - 1);
    setter.SetTransformArray(transforms);
    D3DDevice_DrawVertices(spDevice, 13, 0, 128);
    Check(suDraws == 13 && renderengine::guDiagDraws == 13, "draws and counters remain one per batch");
    spDevice->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCEffectConstantOrder: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
