#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "pc/gcm/renderengine/reflections/ResolutionDebug.h"
#include "pc/gcm/renderengine/reflections/EnvironmentMap.h"
#include "pc/gcm/renderengine/reflections/CubeHistory.h"
#include "pc/debug/DebugIni.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.h"
#include "pc/gcm/renderengine/texture.h"

static u32 suChecks = 0, suFailures = 0;
static void Check(bool pass, const char* name)
{ ++suChecks; if (!pass) { ++suFailures; std::printf("FAIL %s\n", name); } }
static void Require(HRESULT result, const char* name)
{ if (FAILED(result)) { std::printf("GPU FAILURE %s %08x\n", name, unsigned(result)); std::exit(2); } }
#undef CGS_ASSERT
#define CGS_ASSERT(ok, message) Check(!!(ok), message)
namespace renderengine { IDirect3DDevice9* gDevice = nullptr; }
namespace CgsDev::Log { void WriteToLog(const char* value) { std::printf("%s", value); } }
static IDirect3DDevice9* Dev() { return renderengine::gDevice; }

// Capture the engine's allocation description before the native resource boundary.
// The body invoking these setters is the production CreateEnvmapBuffer.
class CgsRenderTarget
{
public:
    u32 muWidth = 0, muHeight = 0, muMips = 0, muTextureType = 0;
    s32 miMultisample = 0;
    bool mbColour = false, mbDepth = false, mbSampleDepth = false, mbConstructed = false;
    void ClearColourTargetInUse() { mbColour = false; }
    void SetDimensions(u32 width, u32 height) { muWidth = width; muHeight = height; }
    void SetNumMipMaps(u32 value) { muMips = value; }
    void SetMultisampleFormat(s32 value) { miMultisample = value; }
    void SetUseDepthStencilAsTexture(bool value) { mbSampleDepth = value; }
    void SetColourTargetInUse(u32, bool value) { mbColour = value; }
    void SetTextureType(u32, u32 value) { muTextureType = value; }
    void SetDepthTargetInUse(bool value) { mbDepth = value; }
    template<class... T> void SetColourTargetBufferFormat(T...) {}
    template<class... T> void SetColourTargetTextureFormat(T...) {}
    template<class... T> void SetColourTargetBaseEDRAM(T...) {}
    template<class... T> void SetDepthTargetBufferFormat(T...) {}
    template<class... T> void SetDepthTargetTextureFormat(T...) {}
    template<class... T> void SetDepthTargetBaseEDRAM(T...) {}
    void Construct(rw::IResourceAllocator*) { mbConstructed = true; }
};
class BrnRendererMemory
{
public:
    enum { E_RENDER_TARGET_ENV_MAP = 3 };
    CgsRenderTarget* mapRenderTarget[16] = {};
    void CreateEnvmapBuffer(rw::IResourceAllocator*);
};

#include "pc_reflection_resolution.inc"

using namespace CgsDev::DebugUI;
static VariableManager sManager;
static Variable sVariable;
static VariableMetadata sMin, sMax, sOptions, sCallback, sCallbackData;
static void Metadata(VariableMetadata& out, const Variant& value, VariableMetadata::Type type)
{ out.Prepare(value, type); sVariable.AddMetadata(&out); }
namespace CgsDev
{
    DebugInterface::DebugInterface() {}
    DebugInterface::~DebugInterface() {}
    void DebugInterface::RegisterVariable(s32* value, const char* path, const char* name)
    { sVariable.Prepare(Variant(value), name); QueueDebugIniPC(&sManager, &sVariable, path, name); }
    void DebugInterface::SetRange(s32*, s32 low, s32 high)
    { Metadata(sMin, Variant(low), VariableMetadata::E_TYPE_MIN); Metadata(sMax, Variant(high), VariableMetadata::E_TYPE_MAX); }
    void DebugInterface::SetOptions(s32*, const StringList* values)
    { Metadata(sOptions, Variant(values), VariableMetadata::E_TYPE_STRINGLIST); }
    void DebugInterface::SetChangeCallback(void*, Variant::UValue::VariableCallbackFunction callback, void* data)
    {
        Metadata(sCallback, Variant(callback), VariableMetadata::E_TYPE_CHANGE_CALLBACK);
        Variant parameter; parameter.SetVoidPointerType(data);
        Metadata(sCallbackData, parameter, VariableMetadata::E_TYPE_CHANGE_CALLBACK_PARAM);
    }
}

static void Settings()
{
    using namespace CgsPC::Reflections;
    Check(RequestedResolution() == 128 && CaptureResolution() == 128, "absent override preserves the original cube");
    for (s32 invalid : {-1, 0, 64, 129, 300, 4096, 2147483647})
        Check(SanitizeResolution(invalid) == 128, "invalid native sizes preserve the original extent");
    Check(LimitResolution(2048, 1000) == 512 && LimitResolution(512, 64) == 64
        && LimitResolution(512, 0) == 0, "hardware cap rounds down without exceeding available extent");
    char directory[MAX_PATH], ini[MAX_PATH]; GetTempPathA(MAX_PATH, directory);
    GetTempFileNameA(directory, "brn", 0, ini);
    WritePrivateProfileStringA("Debug", "World/Reflections/Resolution (restart)", "512", ini);
    LoadDebugIniPC(ini);
    CgsDev::DebugInterface debug;
    RegisterResolution(debug); sManager.ApplyIniOverridesPC();
    Check(RequestedResolution() == 512, "real typed INI override reaches the allocation setting before target creation");
    const auto* options = sVariable.GetStringList();
    const s32 expected[] = {128, 256, 512, 1024, 2048};
    for (u32 i = 0; i < 5; ++i) Check(options[i].miValue == expected[i], "resolution menu exposes each supported pixel size");
    RequestedResolution() = 128;
    for (u32 i = 1; i < 5; ++i) { sVariable.Increment(); Check(RequestedResolution() == expected[i], "menu edits walk powers of two instead of incrementing pixels"); }
    for (s32 i = 3; i >= 0; --i) { sVariable.Decrement(); Check(RequestedResolution() == expected[i], "menu edits also descend through powers of two"); }
    sVariable.Decrement(); Check(RequestedResolution() == 128, "the smallest menu choice remains within its range");
    Variant parsed;
    for (const char* invalid : {"300", "0", "-1", "NaN", "512oops", "99999999999999999"})
        Check(!PrepareDebugIniValuePC(sVariable, invalid, parsed), "INI rejects values absent from the resolution choices");
    Check(PrepareDebugIniValuePC(sVariable, "1024 x 1024", parsed), "named INI presets use the same option table");
    DeleteFileA(ini);
}

static void Projection(u32 pixels)
{
    using namespace CgsPC::Reflections;
    CgsGraphics::Camera camera = {};
    camera.mView = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    camera.maProjectionScalars[1] = camera.maProjectionScalars[3] = 1;
    camera.maProjectionScalars[7] = 0.1f; camera.maProjectionScalars[8] = 100;
    SetActiveResolution(pixels);
    renderengine::SetEnvironmentMapProjectionPC(camera);
    const auto projection = camera.mProjection;
    Check(std::fabs(projection.zAxis.x + 1.0f / pixels) < 1e-7f
        && std::fabs(projection.zAxis.y - 1.0f / pixels) < 1e-7f,
        "cube projection shifts by half a pixel of the allocated extent");
    RequestedResolution() = pixels == 2048 ? 128 : 2048;
    renderengine::SetEnvironmentMapProjectionPC(camera);
    Check(std::memcmp(&projection, &camera.mProjection, sizeof(projection)) == 0,
        "pending menu edits cannot change projection before target recreation");
}

int main()
{
    using namespace CgsPC::Reflections;
    Settings();
    HWND window = CreateWindowA("STATIC", "reflection resolution regression", WS_OVERLAPPEDWINDOW,
        0,0,32,32,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api = Direct3DCreate9(D3D_SDK_VERSION); if (!api || !window) return 2;
    D3DPRESENT_PARAMETERS present = {};
    present.Windowed = TRUE; present.hDeviceWindow = window; present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.BackBufferWidth = present.BackBufferHeight = 32; present.BackBufferFormat = D3DFMT_X8R8G8B8;
    Require(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &present,&renderengine::gDevice), "device");
    auto* device = Dev(); D3DCAPS9 caps = {}; Require(device->GetDeviceCaps(&caps), "caps");
    CubeHistory history;
    const auto msaa = ChooseMultisampleType(4, D3DFMT_A8R8G8B8);
    for (s32 requested : {128, 256, 512, 1024, 2048})
    {
        SetActiveResolution(0); RequestedResolution() = requested;
        const u32 expected = LimitResolution(requested, (std::min)(caps.MaxTextureWidth, caps.MaxTextureHeight));
        BrnRendererMemory memory; memory.CreateEnvmapBuffer(nullptr);
        auto* target = memory.mapRenderTarget[BrnRendererMemory::E_RENDER_TARGET_ENV_MAP];
        const u32 pixels = target->muWidth;
        Check(pixels == expected && target->muHeight == expected, "production allocator describes the selected square cube extent");
        Check(target->mbConstructed && target->mbColour && target->mbDepth && target->mbSampleDepth
            && target->muMips == 1 && target->miMultisample == 2 && target->muTextureType == 3,
            "higher resolution retains the original cube, MSAA and depth description");
        IDirect3DCubeTexture9* cube = nullptr;
        Require(device->CreateCubeTexture(pixels,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&cube,nullptr), "cube");
        auto* scratch = CreateMultisampledColourSurface(pixels,pixels,D3DFMT_A8R8G8B8,msaa);
        auto* depth = CreateMultisampledDepthSurface(pixels,pixels,msaa);
        Check(scratch && depth, "native MSAA colour and depth allocations support the cube extent");
        if (!scratch || !depth) return 2;
        for (auto* surface : {scratch, depth})
        {
            D3DSURFACE_DESC desc = {}; Require(surface->GetDesc(&desc), "surface description");
            Check(desc.Width == expected && desc.Height == expected && desc.MultiSampleType == msaa.meType,
                "native scratch and depth match the selected extent and sample count");
        }
        Require(device->SetRenderTarget(0, scratch), "bind colour"); Require(device->SetDepthStencilSurface(depth), "bind depth");
        D3DVIEWPORT9 viewport = {0,0,pixels,pixels,0,1}; Require(device->SetViewport(&viewport), "viewport");
        for (u32 face = 0; face < 6; ++face)
        {
            IDirect3DSurface9* destination = nullptr;
            Require(cube->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face),0,&destination), "face");
            Require(device->Clear(0,nullptr,D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,0xff123400u+face,1,0), "clear");
            Require(device->StretchRect(scratch,nullptr,destination,nullptr,D3DTEXF_NONE), "resolve"); destination->Release();
        }
        Check(history.Begin(device,cube,true), "reflection feedback snapshot adopts each cube size");
        auto* snapshot = static_cast<IDirect3DCubeTexture9*>(history.GetSnapshot());
        D3DSURFACE_DESC snapshotDesc = {}; Require(snapshot->GetLevelDesc(0,&snapshotDesc), "snapshot size");
        Check(snapshotDesc.Width == expected && snapshotDesc.Height == expected, "history size agrees with the live cube");
        IDirect3DSurface9* readback = nullptr;
        Require(device->CreateOffscreenPlainSurface(pixels,pixels,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr), "readback");
        for (u32 face = 0; face < 6; ++face)
        {
            IDirect3DSurface9* surface = nullptr;
            Require(snapshot->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face),0,&surface), "snapshot face");
            Require(device->GetRenderTargetData(surface,readback), "snapshot readback");
            D3DLOCKED_RECT locked = {}; Require(readback->LockRect(&locked,nullptr,D3DLOCK_READONLY), "lock");
            const DWORD colour = 0xff123400u + face;
            const auto* bottom = static_cast<const u8*>(locked.pBits) + (pixels-1)*locked.Pitch;
            Check(*static_cast<const DWORD*>(locked.pBits) == colour && reinterpret_cast<const DWORD*>(bottom)[pixels-1] == colour,
                "each resolved face and feedback snapshot covers its complete selected extent");
            readback->UnlockRect(); surface->Release();
        }
        Projection(expected);
        history.End(); device->SetTexture(13,nullptr); readback->Release(); cube->Release();
        IDirect3DSurface9* back = nullptr; Require(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back), "backbuffer");
        device->SetDepthStencilSurface(nullptr); device->SetRenderTarget(0,back); back->Release();
        scratch->Release(); depth->Release(); delete target;
    }
    history.Release(); device->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCReflectionResolution: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
