#include "GameSource/Graphics/BrnRendererModule.h"
#include "GameSource/Graphics/BrnAntiAliasTiling.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
#include "GameSource/Game/BrnGlobalCpuMonitors.h"
#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/jobs.h"
#include "GameSource/Graphics/PostFx/BrnPostFx.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/statehelper.h"
#include "rw/rwcore_general_alloc.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include <cstring>
#include <cstdio>
#include <xgraphics.h>

// ARTIST byte_82FAEDD0: shared with Render's present-interval change check.
// The original data image initializes this byte to zero.
bool gRendererPresentInterval82FAEDD0 = false;

// ARTIST823F5568. Assert failures report and execution continues, as in the original.
void BrnRendererModule::DeviceInitialise()
{
    const char* const lpcFile = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp";
    if (!shadow::Device::Initialize())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("RenderClub Graphics device intialise failed.\n", lpcFile, 488);
        CgsDev::Assert::EndAssert();
    }
    renderengine::Device::Parameters lParameters;
    lParameters.Initialize(4);
    lParameters.primaryRingBufferSize = 0x8000;
    lParameters.secondaryRingBufferSize = 0x800000;
    lParameters.numVsyncs = 1;
    gRendererPresentInterval82FAEDD0 = true;
    lParameters.depthStencilAutoCreate = false;
    if (!renderengine::Device::Start(lParameters))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Cannot start Graphics device.\n", lpcFile, 580);
        CgsDev::Assert::EndAssert();
    }
}

// ARTIST827DFF48..827E03FC. Genuine base/member constructors run in declaration
// order: mutexes, resources, sampler parameters, factories and32unnamed Jobs.
// Runtime flags and subsystem startup belong to Construct, not this C++ ctor.
BrnRendererModule::BrnRendererModule()
{
    CGS_STATIC_ASSERT((sizeof(BrnRendererModule) == 0xCA80), "ARTIST Renderer size");
    CGS_STATIC_ASSERT((__alignof(BrnRendererModule) == 128), "ARTIST Renderer alignment");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mAllocatedRenderTargets) == 0x238), "ARTIST mAllocatedRenderTargets");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mDoubleBufferedDispatchFrame) == 0x2A8), "ARTIST mDoubleBufferedDispatchFrame");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mSingleBufferedDispatchFrame) == 0x300), "ARTIST mSingleBufferedDispatchFrame");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mEffectsArbitrator) == 0x480), "ARTIST mEffectsArbitrator");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, maShaderConstantsFrames) == 0x490), "ARTIST maShaderConstantsFrames");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mIm2dRenderBuffer) == 0xAE8), "ARTIST mIm2dRenderBuffer");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mIm2dRenderer) == 0xB30), "ARTIST mIm2dRenderer");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mIm2dRendererUntex) == 0xF70), "ARTIST mIm2dRendererUntex");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mIm3dRenderBuffer) == 0x1090), "ARTIST mIm3dRenderBuffer");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mIm3dRenderer) == 0x10E0), "ARTIST mIm3dRenderer");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mpGraphicsAllocator) == 0x394C), "ARTIST mpGraphicsAllocator");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mSkyDome) == 0xC4E8), "ARTIST mSkyDome");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mOcclusionCullManager) == 0xC580), "ARTIST mOcclusionCullManager");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mLoadingScreenRenderer) == 0xC8C0), "ARTIST mLoadingScreenRenderer");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mCalibrationTextureHandle) == 0xC920), "ARTIST mCalibrationTextureHandle");
    CGS_STATIC_ASSERT((offsetof(BrnRendererModule, mDebugComponent) == 0xCA40), "ARTIST mDebugComponent");
}

// ARTIST823F5628: the main caller supplies no renderer receiver.
s32 BrnRendererModule::GetVSynchRate()
{
    XVIDEO_MODE mode;
    XGetVideoMode(&mode);
    return static_cast<s32>(mode.RefreshRate + 0.5f);
}

// Initial image values82F24234/38; DWARF cpp116/174 names the mutable globals.
u32 KU32_SIZEOFDISPATCHBIN_DOUBLEBUFFERED = 0x001BC000;
u32 KU32_SIZEOFDISPATCHBIN_SINGLEBUFFERED = 0x00187000;

namespace
{

typedef BrnRendererModule::BrnCpuMonitors RendererCpuMonitors;
struct CpuMonitorDefinition
{
    s32 RendererCpuMonitors::* member;
    const char* name;
    f32 budget;
};
const CpuMonitorDefinition kCpuMonitorDefinitions[] =
{
    { &RendererCpuMonitors::miWholeDispatchThread, "DT: All", 100.0f },
    { &RendererCpuMonitors::miObjectToMeshListConversion, "DT: Object lists to mesh lists", 100.0f },
    { &RendererCpuMonitors::miStartSortJobs, "DT: Start list sort jobs", 100.0f },
    { &RendererCpuMonitors::miStartTintBlendJob, "DT: Start tint blend job", 100.0f },
    { &RendererCpuMonitors::miDispatchShadowmapNearCSMList, "DT: Dispatch near CSM list", 100.0f },
    { &RendererCpuMonitors::miWaitOnShadowNearSortJob, "    WaitOn shadow0 sort job", 100.0f },
    { &RendererCpuMonitors::miDispatchShadowmapFarCSMList, "DT: Dispatch far CSM list", 100.0f },
    { &RendererCpuMonitors::miWaitOnShadowFarSortJob, "    WaitOn shadow1 sort job", 100.0f },
    { &RendererCpuMonitors::miDispatchEnvmapLists, "DT: Dispatch envmap lists", 100.0f },
    { &RendererCpuMonitors::miWaitOnEnvmapSortJobs, "    WaitOn envmap sort jobs", 100.0f },
    { &RendererCpuMonitors::miDispatchPreZ, "DT: Dispatch pre-z", 100.0f },
    { &RendererCpuMonitors::miWaitOnPreZSortJob, "    WaitOn pre-z sort job", 100.0f },
    { &RendererCpuMonitors::miDispatchCarLists, "DT: Dispatch car lists", 100.0f },
    { &RendererCpuMonitors::miDispatchCarOpaqueList, "    Dispatch car opaque", 100.0f },
    { &RendererCpuMonitors::miDispatchCarTransparentList, "    Dispatch car transparent", 100.0f },
    { &RendererCpuMonitors::miWaitOnCarOpaqueSortJob, "    WaitOn C opaque sort job", 100.0f },
    { &RendererCpuMonitors::miWaitOnCarTransparentSortJob, "    WaitOn C trans sort job", 100.0f },
    { &RendererCpuMonitors::miDispatchWorldLists, "DT: Dispatch world lists", 100.0f },
    { &RendererCpuMonitors::miDispatchWorldOpaqueList, "    Dispatch world opaque", 100.0f },
    { &RendererCpuMonitors::miDispatchWorldTransparentList, "    Dispatch world transparent", 100.0f },
    { &RendererCpuMonitors::miWaitOnWorldOpaqueSortJob, "    WaitOn W opaque sort job", 100.0f },
    { &RendererCpuMonitors::miWaitOnWorldTransparentSortJob, "    WaitOn W trans sort job", 100.0f },
    { &RendererCpuMonitors::miGenerateOcclusionQueryList, "    Wait on occlusion job", 100.0f },
    { &RendererCpuMonitors::miDispatchOcclusionQueries, "    Dispatch occlusion queries", 100.0f },
    { &RendererCpuMonitors::miRenderSky, "DT: Render sky", 100.0f },
    { &RendererCpuMonitors::miPPUShaderPatching, "DT: Fragment Shaders Patching", 100.0f },
    { &RendererCpuMonitors::miRenderCoronas, "DT: Render coronas", 100.0f },
    { &RendererCpuMonitors::miEffectsUpdate, "DT: Effects Update", 2.5f },
    { &RendererCpuMonitors::miBuildParticleVertexBuffers, "DT: Build particle VBs", 2.5f },
    { &RendererCpuMonitors::miWaitOnParticleVertexBuffersJob, "DT: WaitOn particle VBs job", 2.5f },
    { &RendererCpuMonitors::miRenderFullResParticles, "DT: Render full res particles", 2.5f },
    { &RendererCpuMonitors::miRenderQuarterResParticles, "DT: Render 1/4 res particles", 2.5f },
    { &RendererCpuMonitors::miRenderIm3d, "DT: Render Im3D", 100.0f },
    { &RendererCpuMonitors::miRenderDebugData, "DT; Render debug data", 100.0f },
    { &RendererCpuMonitors::miRenderPostFX, "DT: Render PostFX", 100.0f },
    { &RendererCpuMonitors::miRenderHUD, "DT: Render HUD", 100.0f },
    { &RendererCpuMonitors::miRenderApt, "DT: Render Apt", 100.0f },
    { &RendererCpuMonitors::miShowPixelBuffer, "DT: Show pixel buffer", 100.0f },
    { &RendererCpuMonitors::miClearGraphicsContext, "DT: Clear graphics context", 100.0f },
};

typedef BrnRendererModule::BrnGpuMonitors RendererGpuMonitors;
struct GpuMonitorDefinition
{
    s32 RendererGpuMonitors::* member;
    const char* name;
    f32 budget;
};
const GpuMonitorDefinition kGpuMonitorDefinitions[] =
{
    { &RendererGpuMonitors::miScreenClear, "ScreenClear", 1.0f },
    { &RendererGpuMonitors::miShadowmap, "ShadowMap", 10.0f },
    { &RendererGpuMonitors::miSky, "Sky", 3.0f },
    { &RendererGpuMonitors::miEnvironmentMap, "EnvironmentMap", 6.0f },
    { &RendererGpuMonitors::miPreZ, "Pre-Z", 3.0f },
    { &RendererGpuMonitors::miWorldOpaque, "WorldOpaque", 23.0f },
    { &RendererGpuMonitors::miWorldTransparent, "WorldTransparent", 3.0f },
    { &RendererGpuMonitors::miCarOpaque, "CarOpaque", 20.0f },
    { &RendererGpuMonitors::miCarTransparent, "CarTransparent", 3.0f },
    { &RendererGpuMonitors::miFullResParticles, "FullResParticles", 1.0f },
    { &RendererGpuMonitors::miQuarterResParticles, "ShrinkZ&DrawQuarterResParticles", 5.0f },
    { &RendererGpuMonitors::miResolveMSAAFromEDRAM, "ResolveMSAAFromEDRAM", 2.5f },
    { &RendererGpuMonitors::miDownsampleMSAAAndCompParticles, "CompParticles&DownsampledMSAA", 2.5f },
    { &RendererGpuMonitors::miSunCoronaVisibilityTest, "Sun corona visibility test", 0.1f },
    { &RendererGpuMonitors::miCoronas, "Coronas", 0.4f },
    { &RendererGpuMonitors::miPostFX, "PostFX", 9.0f },
    { &RendererGpuMonitors::miIm3dAndRacePositions, "Im3dAndRacePositions", 1.0f },
    { &RendererGpuMonitors::miMenusAndHud, "MenusAndHud", 3.0f },
    { &RendererGpuMonitors::miDebug3d, "Debug3d", 0.0f },
    { &RendererGpuMonitors::miDebug2d, "Debug2d", 0.0f },
};

// ARTIST8203BA70, native three-channel256-entry gamma ramp.
const D3DGAMMARAMP kInitialGammaRamp =
{
    {
        0x0000, 0x0200, 0x0300, 0x0500, 0x0700, 0x0800, 0x0A00, 0x0C00, 0x0D00, 0x0F00, 0x1000, 0x1200, 0x1400, 0x1500, 0x1600, 0x1800,
        0x1900, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700, 0x2800, 0x2900, 0x2A00,
        0x2B00, 0x2C00, 0x2D00, 0x2E00, 0x2F00, 0x3000, 0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700, 0x3800, 0x3900, 0x3A00,
        0x3B00, 0x3C00, 0x3D00, 0x3E00, 0x3F00, 0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4600, 0x4700, 0x4800, 0x4900,
        0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00, 0x5000, 0x5100, 0x5200, 0x5300, 0x5500, 0x5600, 0x5700, 0x5800, 0x5900, 0x5A00,
        0x5B00, 0x5C00, 0x5D00, 0x5E00, 0x5F00, 0x6000, 0x6100, 0x6200, 0x6300, 0x6400, 0x6500, 0x6600, 0x6700, 0x6800, 0x6900, 0x6A00,
        0x6B00, 0x6C00, 0x6D00, 0x6E00, 0x6F00, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500, 0x7600, 0x7700, 0x7800, 0x7900, 0x7A00,
        0x7B00, 0x7C00, 0x7D00, 0x7E00, 0x7F00, 0x8000, 0x8100, 0x8200, 0x8300, 0x8400, 0x8500, 0x8600, 0x8700, 0x8800, 0x8900, 0x8A00,
        0x8B00, 0x8C00, 0x8D00, 0x8E00, 0x8F00, 0x9000, 0x9100, 0x9200, 0x9300, 0x9400, 0x9500, 0x9600, 0x9700, 0x9800, 0x9900, 0x9A00,
        0x9B00, 0x9C00, 0x9C00, 0x9D00, 0x9E00, 0x9F00, 0xA000, 0xA100, 0xA200, 0xA300, 0xA400, 0xA500, 0xA600, 0xA700, 0xA800, 0xA900,
        0xAA00, 0xAB00, 0xAC00, 0xAD00, 0xAD00, 0xAE00, 0xAF00, 0xB000, 0xB100, 0xB200, 0xB300, 0xB400, 0xB500, 0xB600, 0xB700, 0xB800,
        0xB900, 0xBA00, 0xBA00, 0xBB00, 0xBC00, 0xBD00, 0xBE00, 0xBF00, 0xC000, 0xC100, 0xC200, 0xC300, 0xC400, 0xC400, 0xC500, 0xC600,
        0xC700, 0xC800, 0xC900, 0xCA00, 0xCB00, 0xCC00, 0xCD00, 0xCD00, 0xCE00, 0xCF00, 0xD000, 0xD100, 0xD200, 0xD300, 0xD400, 0xD500,
        0xD600, 0xD600, 0xD700, 0xD800, 0xD900, 0xDA00, 0xDB00, 0xDC00, 0xDD00, 0xDE00, 0xDE00, 0xDF00, 0xE000, 0xE100, 0xE200, 0xE300,
        0xE400, 0xE500, 0xE600, 0xE600, 0xE700, 0xE800, 0xE900, 0xEA00, 0xEB00, 0xEC00, 0xED00, 0xED00, 0xEE00, 0xEF00, 0xF000, 0xF100,
        0xF200, 0xF300, 0xF400, 0xF400, 0xF500, 0xF600, 0xF700, 0xF800, 0xF900, 0xFA00, 0xFB00, 0xFB00, 0xFC00, 0xFD00, 0xFE00, 0xFF00,
    },
    {
        0x0000, 0x0200, 0x0300, 0x0500, 0x0700, 0x0800, 0x0A00, 0x0C00, 0x0D00, 0x0F00, 0x1000, 0x1200, 0x1400, 0x1500, 0x1600, 0x1800,
        0x1900, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700, 0x2800, 0x2900, 0x2A00,
        0x2B00, 0x2C00, 0x2D00, 0x2E00, 0x2F00, 0x3000, 0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700, 0x3800, 0x3900, 0x3A00,
        0x3B00, 0x3C00, 0x3D00, 0x3E00, 0x3F00, 0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4600, 0x4700, 0x4800, 0x4900,
        0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00, 0x5000, 0x5100, 0x5200, 0x5300, 0x5500, 0x5600, 0x5700, 0x5800, 0x5900, 0x5A00,
        0x5B00, 0x5C00, 0x5D00, 0x5E00, 0x5F00, 0x6000, 0x6100, 0x6200, 0x6300, 0x6400, 0x6500, 0x6600, 0x6700, 0x6800, 0x6900, 0x6A00,
        0x6B00, 0x6C00, 0x6D00, 0x6E00, 0x6F00, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500, 0x7600, 0x7700, 0x7800, 0x7900, 0x7A00,
        0x7B00, 0x7C00, 0x7D00, 0x7E00, 0x7F00, 0x8000, 0x8100, 0x8200, 0x8300, 0x8400, 0x8500, 0x8600, 0x8700, 0x8800, 0x8900, 0x8A00,
        0x8B00, 0x8C00, 0x8D00, 0x8E00, 0x8F00, 0x9000, 0x9100, 0x9200, 0x9300, 0x9400, 0x9500, 0x9600, 0x9700, 0x9800, 0x9900, 0x9A00,
        0x9B00, 0x9C00, 0x9C00, 0x9D00, 0x9E00, 0x9F00, 0xA000, 0xA100, 0xA200, 0xA300, 0xA400, 0xA500, 0xA600, 0xA700, 0xA800, 0xA900,
        0xAA00, 0xAB00, 0xAC00, 0xAD00, 0xAD00, 0xAE00, 0xAF00, 0xB000, 0xB100, 0xB200, 0xB300, 0xB400, 0xB500, 0xB600, 0xB700, 0xB800,
        0xB900, 0xBA00, 0xBA00, 0xBB00, 0xBC00, 0xBD00, 0xBE00, 0xBF00, 0xC000, 0xC100, 0xC200, 0xC300, 0xC400, 0xC400, 0xC500, 0xC600,
        0xC700, 0xC800, 0xC900, 0xCA00, 0xCB00, 0xCC00, 0xCD00, 0xCD00, 0xCE00, 0xCF00, 0xD000, 0xD100, 0xD200, 0xD300, 0xD400, 0xD500,
        0xD600, 0xD600, 0xD700, 0xD800, 0xD900, 0xDA00, 0xDB00, 0xDC00, 0xDD00, 0xDE00, 0xDE00, 0xDF00, 0xE000, 0xE100, 0xE200, 0xE300,
        0xE400, 0xE500, 0xE600, 0xE600, 0xE700, 0xE800, 0xE900, 0xEA00, 0xEB00, 0xEC00, 0xED00, 0xED00, 0xEE00, 0xEF00, 0xF000, 0xF100,
        0xF200, 0xF300, 0xF400, 0xF400, 0xF500, 0xF600, 0xF700, 0xF800, 0xF900, 0xFA00, 0xFB00, 0xFB00, 0xFC00, 0xFD00, 0xFE00, 0xFF00,
    },
    {
        0x0000, 0x0200, 0x0300, 0x0500, 0x0700, 0x0800, 0x0A00, 0x0C00, 0x0D00, 0x0F00, 0x1000, 0x1200, 0x1400, 0x1500, 0x1600, 0x1800,
        0x1900, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700, 0x2800, 0x2900, 0x2A00,
        0x2B00, 0x2C00, 0x2D00, 0x2E00, 0x2F00, 0x3000, 0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700, 0x3800, 0x3900, 0x3A00,
        0x3B00, 0x3C00, 0x3D00, 0x3E00, 0x3F00, 0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4600, 0x4700, 0x4800, 0x4900,
        0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00, 0x5000, 0x5100, 0x5200, 0x5300, 0x5500, 0x5600, 0x5700, 0x5800, 0x5900, 0x5A00,
        0x5B00, 0x5C00, 0x5D00, 0x5E00, 0x5F00, 0x6000, 0x6100, 0x6200, 0x6300, 0x6400, 0x6500, 0x6600, 0x6700, 0x6800, 0x6900, 0x6A00,
        0x6B00, 0x6C00, 0x6D00, 0x6E00, 0x6F00, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500, 0x7600, 0x7700, 0x7800, 0x7900, 0x7A00,
        0x7B00, 0x7C00, 0x7D00, 0x7E00, 0x7F00, 0x8000, 0x8100, 0x8200, 0x8300, 0x8400, 0x8500, 0x8600, 0x8700, 0x8800, 0x8900, 0x8A00,
        0x8B00, 0x8C00, 0x8D00, 0x8E00, 0x8F00, 0x9000, 0x9100, 0x9200, 0x9300, 0x9400, 0x9500, 0x9600, 0x9700, 0x9800, 0x9900, 0x9A00,
        0x9B00, 0x9C00, 0x9C00, 0x9D00, 0x9E00, 0x9F00, 0xA000, 0xA100, 0xA200, 0xA300, 0xA400, 0xA500, 0xA600, 0xA700, 0xA800, 0xA900,
        0xAA00, 0xAB00, 0xAC00, 0xAD00, 0xAD00, 0xAE00, 0xAF00, 0xB000, 0xB100, 0xB200, 0xB300, 0xB400, 0xB500, 0xB600, 0xB700, 0xB800,
        0xB900, 0xBA00, 0xBA00, 0xBB00, 0xBC00, 0xBD00, 0xBE00, 0xBF00, 0xC000, 0xC100, 0xC200, 0xC300, 0xC400, 0xC400, 0xC500, 0xC600,
        0xC700, 0xC800, 0xC900, 0xCA00, 0xCB00, 0xCC00, 0xCD00, 0xCD00, 0xCE00, 0xCF00, 0xD000, 0xD100, 0xD200, 0xD300, 0xD400, 0xD500,
        0xD600, 0xD600, 0xD700, 0xD800, 0xD900, 0xDA00, 0xDB00, 0xDC00, 0xDD00, 0xDE00, 0xDE00, 0xDF00, 0xE000, 0xE100, 0xE200, 0xE300,
        0xE400, 0xE500, 0xE600, 0xE600, 0xE700, 0xE800, 0xE900, 0xEA00, 0xEB00, 0xEC00, 0xED00, 0xED00, 0xEE00, 0xEF00, 0xF000, 0xF100,
        0xF200, 0xF300, 0xF400, 0xF400, 0xF500, 0xF600, 0xF700, 0xF800, 0xF900, 0xFA00, 0xFB00, 0xFB00, 0xFC00, 0xFD00, 0xFE00, 0xFF00,
    },
};
}

// ARTIST8240A778. These references are the original boot caller's actual
// monitor input, aspect output and display-mode HD output, not host fallbacks.
void BrnRendererModule::Construct(const BrnGame::BrnCpuMonitors& globalMonitors,
    f32& aspectRatio, bool& highDefinition)
{
    mpGraphicsAllocator = BrnResource::GetGraphicsAllocator();
    rw::IResourceAllocator* const allocator = mpGraphicsAllocator;
    mbMultisampledBackbuffer = true;
    mEffectsArbitrator.Construct(mpGraphicsAllocator);
    maShaderConstantsFrames[0].Construct();
    maShaderConstantsFrames[1].Construct();
    mu8ShaderConstantsFrameExternal = 1;
    mu8ShaderConstantsFrameInternal = 0;
    maShaderConstantsFrames[1].LockForWriting();
    maShaderConstantsFrames[mu8ShaderConstantsFrameInternal].UnlockForWriting();
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = eRendererPrepareStart;
    mbIsInterlaced = false;
    mfAspectCorrection = 1.0f;
    meReleaseStage = eRendererReleaseDone;
    mDisplayType = renderengine::Device::Parameters::DISPLAYTYPE_HDTV_720P;
    renderengine::Device::Parameters parameters;
    parameters.Initialize(4);
    mu16FrontBufferHeight = static_cast<u16>(parameters.GetFrontBufferHeight());
    mbIsHD = mu16FrontBufferHeight >= 720;
    aspectRatio = 16.0f / 9.0f;
    XVIDEO_MODE videoMode;
    XGetVideoMode(&videoMode);
    highDefinition = videoMode.fIsHiDef != FALSE;
    rw::math::fpu::Matrix33Template<float> correction =
        { 1.0f, 0.0f, 0.0f, 0.0f, mfAspectCorrection, 0.0f, 0.0f, 0.0f, 1.0f };
    CgsGraphics::Im2dTransform::SetAspectCorrectionMatrix(correction);
    mAllocatedRenderTargets.Construct(parameters, mpGraphicsAllocator, mbMultisampledBackbuffer);

    mDoubleBufferedDispatchFrame.SetNumDispatchFrames(2);
    mDoubleBufferedDispatchFrame.Construct(13, KU32_SIZEOFDISPATCHBIN_DOUBLEBUFFERED, mpGraphicsAllocator);
    mSingleBufferedDispatchFrame.Construct(25, KU32_SIZEOFDISPATCHBIN_SINGLEBUFFERED, mpGraphicsAllocator);
    renderengine::StateHelper::Initialize();
    mBlendStateFactory.Construct(mpGraphicsAllocator);
    mRasterizerStateFactory.Construct(mpGraphicsAllocator);
    mDepthStencilStateFactory.Construct(mpGraphicsAllocator);
    CgsGraphics::SetupBuiltinInterpreters(maInterpretFunctions);
    mpInterpreter = new CgsGraphics::DispatchPacketInterpreter(maInterpretFunctions, 4);

    mIm2dRenderer.Construct(mpGraphicsAllocator);
    mIm2dRendererUntex.Construct(mpGraphicsAllocator);
    mIm3dRenderer.Construct(mpGraphicsAllocator);
    mIm3dRendererUntex.Construct(mpGraphicsAllocator);
    mIm3dRendererSkyDome.Construct(mpGraphicsAllocator);
    mIm3dRendererZOnly.Construct(mpGraphicsAllocator);
    mIm2dRenderBuffer.Construct();
    mIm3dRenderBuffer.Construct();
    mIm3dRenderBufferUntex.Construct();
    mIm3dDebugRenderBuffer.Construct();
    mIm2dDebugRenderBuffer.Construct();
    mIm3dBufferRacePosition.Construct();
    mIm3dBufferMenusAndHud.Construct();
    mBlobbyShadowManager.Construct();
    mpBlobbyShadowTexture = 0;
    mCoronaManager.Construct(*allocator);

    mCpuMonitors.Construct();
    mGpuMonitors.Construct();
    mGpuHwMonitors.Construct();
    for (u32 i = 0; i != sizeof(kCpuMonitorDefinitions) / sizeof(kCpuMonitorDefinitions[0]); ++i)
        mCpuMonitors.*kCpuMonitorDefinitions[i].member = CgsDev::PerfMonCpu::AddMonitor(
            kCpuMonitorDefinitions[i].name, CgsDev::E_PMP_10, false,
            kCpuMonitorDefinitions[i].budget, false);
    const rw::RGBA white(255, 255, 255, 255);
    for (u32 i = 0; i != sizeof(kGpuMonitorDefinitions) / sizeof(kGpuMonitorDefinitions[0]); ++i)
        mGpuMonitors.*kGpuMonitorDefinitions[i].member = CgsDev::PerfMonGpu::AddMonitor(
            kGpuMonitorDefinitions[i].name, white, kGpuMonitorDefinitions[i].budget);
    // ARTIST8240B714..B780 resets the hardware monitor IDs after registration.
    mGpuHwMonitors.miEnvMap = mGpuHwMonitors.miShadowMap = 0;
    mGpuHwMonitors.miWorldOpaque = mGpuHwMonitors.miCarOpaque = 0;
    mGpuHwMonitors.miWorldTransparent = mGpuHwMonitors.miCarTransparent = mGpuHwMonitors.miPostFX = 0;
    miCpuPerfMonDispatchThread = globalMonitors.miDT_DispatchToGpu;
    msPostFx.Construct(mpGraphicsAllocator);

    CgsGraphics::mShaderConstantTable.AddShaderConstant( 8u, "ViewPosition",                 16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant( 9u, "KeyLightColour",               16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(10u, "KeyLightDirection",            16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(11u, "KeyLightSpecularColour",       16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(12u, "KeyLightClampedColour",        16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(13u, "Time",                         16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(34u, "ViewProjectionModified",       64);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(27u, "ScattCoeffs",                  16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(28u, "FogColourPlusWhiteLevel",      16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(33u, "SkyReflectionColour",          16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(29u, "HDRConstants",                 16);
    CgsGraphics::mShaderConstantTable.AddShaderConstantArray(14u, "ShadowMap_WorldToLight", 64, 3);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(15u, "ShadowMap_Constants",          16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(16u, "ShadowMap_Constants2",         16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(17u, "ShadowMap_ObjectCsmSelect",    16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(20u, "g_paintColour",                16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(21u, "g_pearlescentColour",          16);
    CgsGraphics::mShaderConstantTable.AddShaderConstantArray(22u, "g_verletOffsets", 16, 128);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(23u, "g_damageConstants",            16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(24u, "g_selfIlluminationMask",       16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(25u, "g_wheelConstants",             16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(26u, "g_PerVehicleFog",              16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(18u, "IrradianceQuadricA",           64);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(19u, "IrradianceQuadricB",           64);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(30u, "g_glassFractureStrength",      16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(31u, "g_glassFractureUVOffsets",     16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(32u, "g_glassFractureFresnelRanges", 16);

    mbUpdateThreadTakeScreenshot = false;
    mbDispatchThreadTakeScreenshot = false;
    mbCaptureOverlaysInScreenshot = false;
    muScreenshotCounter = 1;
    macScreenShotText[0] = 0;
    mbShowEnvironmentMap = false;
    mbShowShadowMap = false;
    mbSortDisplayListsWideNotLong = false;
    miShowShadowMapIndex = 0;
    ConstructRenderSwitches();
    mbOcclusionCullCarOpaque = false;
    mbOcclusionCullWorldOpaque = true;
    mbOcclusionCullCarTransparent = false;
    mbOcclusionCullWorldTransparent = true;
    mbOcclusionCullShadowMap = false;
    mbPreZNearOnly = true;
    mbRenderPreZAlpha = false;
    mfPreZDistanceThreshold = 200.0f;
    mfOccludeeNearClipOffset = 150.0f;
    muOcclusionCullIndexCountThreshold = 32;
    mbGreyBackgroundColour = true;
    mfBlobbyShadowAlpha = 0.7f;
    mDebugComponent.Construct(this);
    ClearDispatchCounters();

    mTextureStateParams.muMagFilter = 0;
    mTextureStateParams.muMinFilter = 0;
    mTextureStateParams.muAddressU = 2;
    mTextureStateParams.muAddressV = 2;
    mTextureStateParams.mpTexture = mAllocatedRenderTargets.GetShadowMapBuffer(0)->GetDepthTexture();
    rw::ResourceDescriptor descriptor = renderengine::TextureState::GetResourceDescriptor(mTextureStateParams);
    mTextureStateResource = mpGraphicsAllocator->Allocate(descriptor, 0);
    mpShadowMapTextureState[0] = renderengine::TextureState::Initialize(mTextureStateResource, mTextureStateParams);
    mpShadowMapTextureState[1] = 0;
    mEnvMapTextureStateParams.muMagFilter = 1;
    mEnvMapTextureStateParams.muMinFilter = 1;
    mEnvMapTextureStateParams.muMipFilter = 1;
    mEnvMapTextureStateParams.muAddressU = 2;
    mEnvMapTextureStateParams.muAddressV = 2;
    mEnvMapTextureStateParams.muAddressW = 2;
    mEnvMapTextureStateParams.mpTexture = mAllocatedRenderTargets.GetEnvMapBuffer()->GetTexture(0);
    descriptor = renderengine::TextureState::GetResourceDescriptor(mEnvMapTextureStateParams);
    mEnvMapTextureStateResource = mpGraphicsAllocator->Allocate(descriptor, 0);
    mpEnvMapTextureState = renderengine::TextureState::Initialize(mEnvMapTextureStateResource, mEnvMapTextureStateParams);
    CgsGraphics::gTextureScopeTable.SetTexturePurposeData(CgsGraphics::E_TEXTURE_PURPOSE_SHADOW_MAP_0,
        mAllocatedRenderTargets.GetShadowMapBuffer(0)->GetDepthTexture());
    CgsGraphics::gTextureScopeTable.SetTexturePurposeTextureState(CgsGraphics::E_TEXTURE_PURPOSE_SHADOW_MAP_0, mpShadowMapTextureState[0]);
    CgsGraphics::gTextureScopeTable.SetTexturePurposeData(CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP,
        mAllocatedRenderTargets.GetEnvMapBuffer()->GetTexture(0));
    CgsGraphics::gTextureScopeTable.SetTexturePurposeTextureState(CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP, mpEnvMapTextureState);
    mpGlassFractureTextureState = 0;
    D3DDevice_SetGammaRamp(::gpD3DDevice, 0, &kInitialGammaRamp);
    mSkyDome.Construct();
    mpCloudDensity0Texture = 0;
    mpCloudLighting0Texture = 0;
    mSunCorona.Construct(mpGraphicsAllocator);
    mShadowMapRenderManager.Construct(2);
    meFrameStallStage = E_FRAMESTALL_NOT_STALLED;
    miFrameStallCountdown = 0;
    mOcclusionCullManager.Construct(&mIm3dRendererUntex, &mIm3dRendererZOnly, mpGraphicsAllocator, 8192);
    mLoadingScreenRenderer.Construct();
    mCalibrationTextureHandle = CgsResource::NULLResourceHandle;
    MemoryUsage();
    mbIsNewModule = true;
}

// ARTIST823FBDE0: sample all five resource lanes before any conditional log output.
void BrnRendererModule::MemoryUsage()
{
    const rw::Resource bases = mpGraphicsAllocator->GetLinearHeapBase();
    const rw::ResourceDescriptor usage = mpGraphicsAllocator->GetCurrentUsage();
    const rw::ResourceDescriptor capacity = mpGraphicsAllocator->GetCapacity();
    const u32 mainStart = reinterpret_cast<u32>(bases.m_baseResources[0]);
    const u32 physicalStart = reinterpret_cast<u32>(bases.m_baseResources[2]);
    const u32 mainCapacity = capacity.m_baseResourceDescriptors[0].m_size;
    const u32 physicalCapacity = capacity.m_baseResourceDescriptors[2].m_size;
    const u32 mainUsage = usage.m_baseResourceDescriptors[0].m_size;
    const u32 physicalUsage = usage.m_baseResourceDescriptors[2].m_size;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "===================================================\n";
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "BrnRendererModule: Memory Usage ";
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "for XBox360\n";
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        CgsDev::StrStreamBase* const stream = CgsDev::Log::gpDebugPrint;
        *stream << "Main Memory start address ";
        CgsDev::StrStreamBase* chain = &(*stream << mainStart);
        *chain << " end address: ";
        chain = &(*chain << (mainStart + mainCapacity));
        *chain << "\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        CgsDev::StrStreamBase* const stream = CgsDev::Log::gpDebugPrint;
        *stream << "Main Memory total usage ";
        CgsDev::StrStreamBase* chain = &(*stream << mainUsage);
        *chain << "b out of ";
        chain = &(*chain << mainCapacity);
        *chain << "b so ";
        chain = &(*chain << (mainCapacity - mainUsage));
        *chain << "b free\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        CgsDev::StrStreamBase* const stream = CgsDev::Log::gpDebugPrint;
        *stream << "Physical Memory start address ";
        CgsDev::StrStreamBase* chain = &(*stream << physicalStart);
        *chain << " end address: ";
        chain = &(*chain << (physicalStart + physicalCapacity));
        *chain << "\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        CgsDev::StrStreamBase* const stream = CgsDev::Log::gpDebugPrint;
        *stream << "Physical Memory total usage ";
        CgsDev::StrStreamBase* chain = &(*stream << physicalUsage);
        *chain << "b out of ";
        chain = &(*chain << physicalCapacity);
        *chain << "b so ";
        chain = &(*chain << (physicalCapacity - physicalUsage));
        *chain << "b free\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "======================================================\n";
}

// ARTIST82409C20. Failed manager preparation leaves its stage available to retry;
// completion publishes release-start and prepares the original seven streams.
bool BrnRendererModule::Prepare(const BrnResource::GameDataIO::AllocatorList* allocators)
{
    bool prepared = false;
    switch (mePrepareStage)
    {
    case eRendererPrepareStart:
        mePrepareStage++;
    case eRendererPrepareManager:
        if (!CgsModule::ModuleSingleBuffered::Prepare())
            break;
        mePrepareStage++;
    case eRendererPrepareBlobbyShadows:
        mePrepareStage++;
    case eRendererPrepareCoronas:
        mePrepareStage++;
    case eRendererPrepareDone:
        prepared = true;
        meReleaseStage = eRendererReleaseStart;
        break;
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid Render Module Prepare State",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1439);
        CgsDev::Assert::EndAssert();
        break;
    }
    if (!prepared)
        return false;

    rw::IResourceAllocator* const allocator = allocators->GetRWGeneralResourceAllocator(31);
    if (!mIm2dRenderBuffer.Prepare(110592, 393216, allocator, false))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mIm2dRenderBuffer.Prepare( KI_IM2D_RENDER_BUFFER_COMMAND_BUFFER_SIZE, KI_IM2D_RENDER_BUFFER_VERTEX_BUFFER_SIZE, lpAllocator )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1456);
        CgsDev::Assert::EndAssert();
    }
    if (!mIm3dRenderBuffer.Prepare(1024, 32768, allocator, false))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mIm3dRenderBuffer.Prepare( KI_IM3D_RENDER_BUFFER_COMMAND_BUFFER_SIZE, KI_IM3D_RENDER_BUFFER_VERTEX_BUFFER_SIZE, lpAllocator )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1466);
        CgsDev::Assert::EndAssert();
    }
    if (!mIm3dRenderBufferUntex.Prepare(16384, 524288, allocator, false))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mIm3dRenderBufferUntex.Prepare( KI_IM3D_RENDER_BUFFER_UNTEX_COMMAND_BUFFER_SIZE, KI_IM3D_RENDER_BUFFER_UNTEX_VERTEX_BUFFER_SIZE, lpAllocator )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1470);
        CgsDev::Assert::EndAssert();
    }
    if (!mIm3dDebugRenderBuffer.Prepare(1048576, 1048576, BrnResource::GetDebugAllocator(), true))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mIm3dDebugRenderBuffer.Prepare( KI_IM3D_DEBUG_RENDER_BUFFER_COMMAND_BUFFER_SIZE, KI_IM3D_DEBUG_RENDER_BUFFER_VERTEX_BUFFER_SIZE, BrnResource::GetDebugAllocator(), true )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1477);
        CgsDev::Assert::EndAssert();
    }
    if (!mIm2dDebugRenderBuffer.Prepare(81920, 2097152, BrnResource::GetDebugAllocator(), true))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mIm2dDebugRenderBuffer.Prepare( KI_IM2D_DEBUG_RENDER_BUFFER_COMMAND_BUFFER_SIZE, KI_IM2D_DEBUG_RENDER_BUFFER_VERTEX_BUFFER_SIZE, BrnResource::GetDebugAllocator(), true )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1482);
        CgsDev::Assert::EndAssert();
    }
    if (!mIm3dBufferRacePosition.Prepare(131072, 131072, allocator, false))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mIm3dBufferRacePosition.Prepare( KI_IM3D_RENDER_BUFFER_RACEPOSITIONS_COMMANDBUFFER_SIZE, KI_IM3D_RENDER_BUFFER_RACEPOSITIONS_VERTEXBUFFER_SIZE, lpAllocator )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1488);
        CgsDev::Assert::EndAssert();
    }
    if (!mIm3dBufferMenusAndHud.Prepare(512, 4, allocator, false))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mIm3dBufferMenusAndHud.Prepare( KI_IM3D_RENDER_BUFFER_MENUSANDHUD_COMMANDBUFFER_SIZE, KI_IM3D_RENDER_BUFFER_MENUSANDHUD_VERTEXBUFFER_SIZE, lpAllocator )",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1492);
        CgsDev::Assert::EndAssert();
    }
    mSkyDome.Prepare(&mIm3dRendererSkyDome, BrnResource::GetGraphicsAllocator());
    mDebugComponent.Register();
    MemoryUsage();
    return true;
}

// ARTIST823FFE28: one syncing countdown precedes the stalled-buffer state.
void BrnRendererModule::EndOfFrame(bool stalled)
{
    if (stalled)
    {
        if (meFrameStallStage == E_FRAMESTALL_NOT_STALLED)
        {
            meFrameStallStage = E_FRAMESTALL_SYNCING_BUFFERS;
            miFrameStallCountdown = 2;
        }
    }
    else if (meFrameStallStage != E_FRAMESTALL_NOT_STALLED)
    {
        meFrameStallStage = E_FRAMESTALL_NOT_STALLED;
        miFrameStallCountdown = 0;
    }

    bool swap = false;
    switch (meFrameStallStage)
    {
    case E_FRAMESTALL_NOT_STALLED:
        swap = true;
        break;
    case E_FRAMESTALL_SYNCING_BUFFERS:
        miFrameStallCountdown = static_cast<s32>(static_cast<u32>(miFrameStallCountdown) - 1u);
        if (miFrameStallCountdown != 0)
            break;
    case E_FRAMESTALL_STALLED:
        meFrameStallStage = E_FRAMESTALL_STALLED;
        swap = true;
        break;
    }
    if (swap)
        SwapBuffers();
    if (mbUpdateThreadTakeScreenshot)
    {
        mbUpdateThreadTakeScreenshot = false;
        mbDispatchThreadTakeScreenshot = true;
    }
}

// ARTIST823FBCE0: native staged release retries each failing owner in place.
bool BrnRendererModule::Release()
{
    switch (meReleaseStage)
    {
    case eRendererReleaseStart:
        meReleaseStage++;
    case eRendererReleaseCoronas:
        if (!mCoronaManager.Release(*BrnResource::GetGraphicsAllocator()))
            return false;
        meReleaseStage++;
    case eRendererReleaseBlobbyShadows:
        meReleaseStage++;
    case eRendererReleaseManager:
        if (!CgsModule::ModuleSingleBuffered::Release())
            return false;
        meReleaseStage++;
    case eRendererReleaseDone:
        mePrepareStage = eRendererPrepareStart;
        return true;
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid Render Module Release State",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp", 1608);
        CgsDev::Assert::EndAssert();
        return false;
    }
}

// ARTIST82408A58: clear the shared back-buffer aliases before freeing samplers.
void BrnRendererModule::Destruct()
{
    mSunCorona.Destruct();
    rw::graphics::postfx::RenderTarget* const back = mAllocatedRenderTargets.GetBackBuffer()->GetRenderTarget();
    if (back)
    {
        back->maColourTargets[0].mpTextureState = 0;
        back->mpSection0State = 0;
    }
    mpGraphicsAllocator->Free(mTextureStateResource);
    if (mpGlassFractureTextureState)
        mpGlassFractureTextureState = 0;
    mpGraphicsAllocator->Free(mEnvMapTextureStateResource);
    msPostFx.Destruct();
}

// ARTIST823FC160. The call at823FC2A4 targets the true BLR ICF body8284CB38.
void BrnRendererModule::StartOfFrame()
{
    mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite().Reset();
    mIm2dRenderBuffer.Clear();
    mIm3dRenderBuffer.Clear();
    mIm3dRenderBufferUntex.Clear();
    mIm3dDebugRenderBuffer.Clear();
    mIm2dDebugRenderBuffer.Clear();
    mIm3dBufferRacePosition.Clear();
    mIm3dBufferMenusAndHud.Clear();
    CgsGraphics::mShaderConstantTable.BeginFrame(
        &mDoubleBufferedDispatchFrame.GetDispatchBinForWrite());
    CgsGraphics::gTextureScopeTable.BeginFrame();
    mCoronaManager.Clear();
}

// ARTIST823FC678. Two calls at823FC6AC/B0 also target8284CB38 (BLR).
void BrnRendererModule::SwapBuffers()
{
    mDoubleBufferedDispatchFrame.Swap();
    mEffectsArbitrator.EndOfFrame();
    mu8ShaderConstantsFrameInternal = mu8ShaderConstantsFrameExternal;
    mu8ShaderConstantsFrameExternal = static_cast<u8>(1 - mu8ShaderConstantsFrameExternal);
    maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].Construct();
    maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].LockForWriting();
    maShaderConstantsFrames[mu8ShaderConstantsFrameInternal].UnlockForWriting();
    mIm2dRenderBuffer.Swap();
    mIm3dRenderBuffer.Swap();
    mIm3dRenderBufferUntex.Swap();
    mIm3dDebugRenderBuffer.Swap();
    mIm2dDebugRenderBuffer.Swap();
    mIm3dBufferRacePosition.Swap();
    mIm3dBufferMenusAndHud.Swap();
    mBlobbyShadowManager.Swap();
    mCoronaManager.Swap();
}

// ARTIST823FF8F8. Preserve the incoming glass pointer for the nonpersistent
// purpose; the persistent purpose reads the live member after Initialize.
bool BrnRendererModule::PrepareAgain(const renderengine::Texture* blobbyShadow,
    const renderengine::Texture* cloudDensity, const renderengine::Texture* cloudLighting,
    const renderengine::Texture* coronaAtlas, const renderengine::Texture* glassFracture)
{
    mpBlobbyShadowTexture = const_cast<renderengine::Texture*>(blobbyShadow);
    mpCloudDensity0Texture = cloudDensity;
    mpCloudLighting0Texture = cloudLighting;
    mCoronaManager.SetTextureAtlas(*BrnResource::GetGraphicsAllocator(),
        const_cast<renderengine::Texture*>(coronaAtlas));
    mpGlassFractureTexture = const_cast<renderengine::Texture*>(glassFracture);
    mGlassFractureTextureStateParams.mpTexture = const_cast<renderengine::Texture*>(glassFracture);
    const rw::ResourceDescriptor descriptor =
        renderengine::TextureState::GetResourceDescriptor(mGlassFractureTextureStateParams);
    rw::IResourceAllocator* const allocator = mpGraphicsAllocator;
    mGlassFractureTextureStateResource = allocator->DoAllocate(descriptor, 0);
    mpGlassFractureTextureState = renderengine::TextureState::Initialize(
        mGlassFractureTextureStateResource, mGlassFractureTextureStateParams);
    CgsGraphics::gTextureScopeTable.SetTexturePurposeData(
        CgsGraphics::E_TEXTURE_PURPOSE_GLASS_FRACTURE, glassFracture);
    CgsGraphics::gTextureScopeTable.SetTexturePurposeData(
        CgsGraphics::E_TEXTURE_PURPOSE_GLASS_FRACTURE_PERSISTENT, mpGlassFractureTexture);
    CgsGraphics::gTextureScopeTable.SetTexturePurposeTextureState(
        CgsGraphics::E_TEXTURE_PURPOSE_GLASS_FRACTURE_PERSISTENT, mpGlassFractureTextureState);
    return true;
}

// ARTIST 823F65B0. This X360 body uses neither the receiver nor the target input.
void BrnRendererModule::EndRenderPostFx(rw::graphics::postfx::RenderTarget*)
{
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
        E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
        E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(
        E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));
}

// ARTIST 823FC5E8. Resolve only the selected face of the first colour target.
void BrnRendererModule::EndRenderEnvironmentMapFace(u32 face)
{
    const char* const file = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp";
    if (!mAllocatedRenderTargets.GetEnvMapBuffer())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mAllocatedRenderTargets.GetEnvMapBuffer()", file, 4204);
        CgsDev::Assert::EndAssert();
    }
    if (!mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget()", file, 4205);
        CgsDev::Assert::EndAssert();
    }
    mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget()->maColourTargets[0].Resolve(face);
}

namespace
{

// ARTIST .data 82F2423C and 82F2423D.
bool gbObjectToMeshJobsEnabled = true;
bool gbObjectToMeshJobsWide = true;

// ARTIST 82FAEDD4, 82FAEDD8 and 82FAEE00.
u32 guSharedMemoryBlockMax;
CgsGraphics::DispatchCommand* gpSharedMemoryStart;
CGS_ALIGN(128) u32 guSharedMemoryBlockNextFree;

const char* const kRendererSource =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp";

// ARTIST 823F5670.
void FillInObjectToMeshJobData(EA::Jobs::Job* job, ObjectToMeshJobInfo* outData,
                               const ObjectToMeshJobInfo& data)
{
    if (!outData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutJobData", kRendererSource, 1734);
        CgsDev::Assert::EndAssert();
    }
    if (!job)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutJob", kRendererSource, 1735);
        CgsDev::Assert::EndAssert();
    }
    std::memcpy(outData, &data, sizeof(data));
    job->Clear();
    job->GetEntryPoint().SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
        reinterpret_cast<const void*>(&ObjectToMeshEntry), 0);
    job->SetData(outData, sizeof(*outData));
    job->GetEntryPoint().SetName("ObjectToMesh");
}

}

// ARTIST 823F5748. Each job owns its 240-byte context and a 25-list output array.
void BrnRendererModule::CreateObjectToMeshJob(ERendererModelOnlyDispatchJob jobIndex,
    const CgsGraphics::DispatchObjectContext& context, CgsGraphics::DispatchPacketInterpreter* interpreter,
    ERendererDoubleBufferedDispatchListID inputList, s32 groupIndex, s32 groupSize)
{
    CgsGraphics::DispatchList* const input = mDoubleBufferedDispatchFrame.GetDispatchFrameForRead()
        .GetList(inputList);
    CgsGraphics::DispatchObjectContext& jobContext = maObjectToMeshJobContext[jobIndex];
    std::memcpy(&jobContext, &context, sizeof(jobContext));
    jobContext.miListIdBase = groupIndex;

    ObjectToMeshJobInfo data;
    data.muListID = jobIndex;
    data.mpDispatchInterpreter = interpreter;
    data.mpDispatchObjectContext = &jobContext;
    data.mpDispatchListInput = input;
    data.mpaDispatchListOutputArray = mapaObjectToMeshJobOutputDispatchLists[jobIndex];
    data.muDispatchListOutputCount = 25;
    data.mpDispatchBinMasterAddress = interpreter->GetSingleBufferedDispatchFrame()->GetBin().GetBase();
    data.muSharedMemoryStartAddress = reinterpret_cast<uintptr_t>(gpSharedMemoryStart);
    data.mpSharedMemoryBlockNextFreeAtomic = &guSharedMemoryBlockNextFree;
    data.muSharedMemoryBlockMax = guSharedMemoryBlockMax;

    if (groupIndex < 0 || static_cast<u32>(groupIndex) >= static_cast<u32>(groupSize))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("0 <= luGroupIndex && luGroupIndex < luGroupSize", kRendererSource, 1794);
        CgsDev::Assert::EndAssert();
    }

    const u32 blocks = (input->GetCount() + 127u) >> 7;
    data.miStartIndex = (blocks * static_cast<u32>(groupIndex) / static_cast<u32>(groupSize)) << 7;
    data.miEndIndex = (blocks * static_cast<u32>(groupIndex + 1) / static_cast<u32>(groupSize)) << 7;
    data.mbSoftwareBreakpointOnEntry = false;
    FillInObjectToMeshJobData(&maObjectToMeshJob[jobIndex], &maObjectToMeshJobData[jobIndex], data);
}

// ARTIST 823F5898. Keep the two producer modes and the original list merge order.
void BrnRendererModule::ConvertObjectsToMeshes(CgsGraphics::BufferedDispatchFrame& gdlFrames,
    CgsGraphics::DispatchFrame& meshFrame, CgsGraphics::DispatchPacketInterpreter* interpreter,
    const CgsGraphics::DispatchObjectContext& context)
{
    using namespace CgsGraphics;
    DispatchBin& bin = interpreter->GetSingleBufferedDispatchFrame()->GetBin();
    if (gbObjectToMeshJobsEnabled)
    {
        const u32 cacheLine = 128;
        const u32 outputLists = 25;
        for (u32 job = 0; job < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++job)
        {
            bin.Align(cacheLine);
            mapaObjectToMeshJobOutputDispatchLists[job] = static_cast<DispatchList*>(
                bin.AllocateMemoryFast(outputLists * sizeof(DispatchList) / sizeof(DispatchCommand)));
        }

        bin.Align(cacheLine);
        DispatchCommand* const binEnd = bin.GetBase() + bin.GetSizeQwords();
        const ptrdiff_t bytesAvailable =
            reinterpret_cast<const u8*>(binEnd) - reinterpret_cast<const u8*>(bin.GetBinCurrent());
        const ptrdiff_t alignedBytes = bytesAvailable & -static_cast<ptrdiff_t>(cacheLine);
        const u32 quadWordsAvailable = static_cast<u32>(
            alignedBytes / static_cast<ptrdiff_t>(sizeof(DispatchCommand)) - 1);
        gpSharedMemoryStart = static_cast<DispatchCommand*>(bin.AllocateMemoryFast(quadWordsAvailable));
        EA::Jobs::AtomicStore(&guSharedMemoryBlockNextFree, 0);
        guSharedMemoryBlockMax = quadWordsAvailable / DispatchBin::KU_BLOCK_SIZE_IN_QUAD_WORDS;

        for (s32 group = 0; group < 4; ++group)
            CreateObjectToMeshJob(static_cast<ERendererModelOnlyDispatchJob>(group),
                context, interpreter, eRendererModelOnlyDispatchListWorld, group, 4);
        CreateObjectToMeshJob(eRendererModelOnlyDispatchJob_Car_Part0,
            context, interpreter, eRendererModelOnlyDispatchListCar, 0, 1);
        for (u32 list = eRendererModelOnlyDispatchListShadow0;
             list <= eRendererModelOnlyDispatchListEnvmapNegZ; ++list)
        {
            CreateObjectToMeshJob(static_cast<ERendererModelOnlyDispatchJob>(eRendererModelOnlyDispatchJob_Shadow0 + list),
                context, interpreter, static_cast<ERendererDoubleBufferedDispatchListID>(list), 0, 1);
        }

        EA::Jobs::JobScheduler& scheduler = *CgsSystem::JobManager();
        if (gbObjectToMeshJobsWide)
        {
            for (u32 job = 0; job < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++job)
                scheduler.AddJobs(&maObjectToMeshJob[job], 1);
        }
        else
        {
            for (u32 job = 1; job < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++job)
                maObjectToMeshJob[job].DependsOn(maObjectToMeshJob[job - 1], EA::Jobs::Event::EVENT_WHEN_JOB_END);
            scheduler.AddTree(&maObjectToMeshJob[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS - 1]);
        }
        for (u32 job = 0; job < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++job)
            maObjectToMeshJob[job].WaitOn();

        u32 blocksUsed = guSharedMemoryBlockNextFree;
        if (blocksUsed > guSharedMemoryBlockMax)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Object-to-mesh conversion jobs failed due to insufficient memory.",
                kRendererSource, 2001);
            CgsDev::Assert::EndAssert();
            blocksUsed = guSharedMemoryBlockMax;
        }
        bin.SetBinCurrent(gpSharedMemoryStart + blocksUsed * DispatchBin::KU_BLOCK_SIZE_IN_QUAD_WORDS);

        for (u32 job = 0; job < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++job)
        {
            DispatchList* const jobLists = mapaObjectToMeshJobOutputDispatchLists[job];
            for (u32 list = 0; list < outputLists; ++list)
            {
                jobLists[list].ReconnectChainBlocks();
                meshFrame.GetList(list)->Append(&jobLists[list]);
            }
        }

        const u32 mainLists[] = { 11, 15, 21 };
        for (u32 i = 0; i < sizeof(mainLists) / sizeof(mainLists[0]); ++i)
        {
            for (u32 group = 1; group <= 3; ++group)
            {
                DispatchList* const other = meshFrame.GetList(mainLists[i] + group);
                meshFrame.GetList(mainLists[i])->Append(other);
            }
        }
    }
    else
    {
        for (u32 list = 0; list < eRendererModelOnlyDispatchListEnd; ++list)
        {
            mShaderConstantTable.ResetShadowingForDispatch();
            gTextureScopeTable.ClearDispatchThreadPointers();
            DispatchObjectContext localContext;
            std::memcpy(&localContext, &context, sizeof(localContext));
            DispatchFrame* const output = interpreter->GetSingleBufferedDispatchFrame();
            gdlFrames.GetDispatchFrameForRead().GetList(list)->DispatchAllObjectToMesh(
                interpreter, output, &localContext, 0, -1);
        }
    }
}

namespace
{

// ARTIST 823F5EA0. Both key addresses describe the flat in-place sort buffer.
void PrepareRadixSortJob(EA::Jobs::Job* job, SortInfo* data, CgsGraphics::DispatchList* list)
{
    list->PrepareSortJobInfo(&data->mInputOutputInfo);
    const char* const file = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp";
    if (!data)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutJobData", file, 3573);
        CgsDev::Assert::EndAssert();
    }
    if (!job)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutJob", file, 3574);
        CgsDev::Assert::EndAssert();
    }

    const u32 address = reinterpret_cast<u32>(data->mInputOutputInfo.mpaFlatKeys);
    data->mu64KeyInAddress = address;
    data->mu16Count = static_cast<u16>(data->mInputOutputInfo.muKeyCount);
    data->mu64KeyOutAddress = address;
    job->Clear();
    job->GetEntryPoint().SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
        reinterpret_cast<const void*>(&RadixSortEntry), 0);
    job->SetData(data, sizeof(*data));
    job->GetEntryPoint().SetName("RadixSort");
}

}

// ARTIST 823F5F70. The wide path submits four shadow jobs; the chained path
// links all five before attaching the first environment-map job.
void BrnRendererModule::SortDispatchLists(CgsGraphics::DispatchFrame& meshFrame)
{
    const u32 shadowLists[] = { 0, 2, 1, 3, 4 };
    for (u32 i = 0; i < KU_NUM_SHADOWMAP_DISPATCH_JOBS; ++i)
        PrepareRadixSortJob(&maShadowMapSortJob[i], &maShadowMapSortJobData[i],
            meshFrame.GetList(shadowLists[i]));

    for (u32 i = 0; i < KU_NUM_ENVMAP_SORT_JOBS; ++i)
        PrepareRadixSortJob(&maEnvmapSortJobs[i], &maEnvmapSortJobData[i], meshFrame.GetList(5 + i));

    PrepareRadixSortJob(&mPreZSortJob, &mPreZSortJobData, meshFrame.GetList(21));
    PrepareRadixSortJob(&mWorldOpaqueSortJob, &mWorldOpaqueSortJobData, meshFrame.GetList(11));
    PrepareRadixSortJob(&mCarOpaqueSortJob, &mCarOpaqueSortJobData, meshFrame.GetList(19));
    PrepareRadixSortJob(&mWorldTransparentSortJob, &mWorldTransparentSortJobData, meshFrame.GetList(15));
    PrepareRadixSortJob(&mCarTransparentSortJob, &mCarTransparentSortJobData, meshFrame.GetList(20));

    EA::Jobs::Job* const sceneJobs[] = {
        &mPreZSortJob, &mWorldOpaqueSortJob, &mCarOpaqueSortJob,
        &mWorldTransparentSortJob, &mCarTransparentSortJob
    };
    EA::Jobs::JobScheduler& scheduler = *CgsSystem::JobManager();
    if (mbSortDisplayListsWideNotLong)
    {
        for (u32 i = 0; i < 4; ++i)
            scheduler.AddJobs(&maShadowMapSortJob[i], 1);
        for (u32 i = 0; i < KU_NUM_ENVMAP_SORT_JOBS; ++i)
            scheduler.AddJobs(&maEnvmapSortJobs[i], 1);
        for (u32 i = 0; i < sizeof(sceneJobs) / sizeof(sceneJobs[0]); ++i)
            scheduler.AddJobs(sceneJobs[i], 1);
    }
    else
    {
        EA::Jobs::Job* previous = &maShadowMapSortJob[0];
        for (u32 i = 1; i < KU_NUM_SHADOWMAP_DISPATCH_JOBS; ++i)
        {
            maShadowMapSortJob[i].DependsOn(*previous, EA::Jobs::Event::EVENT_WHEN_JOB_END);
            previous = &maShadowMapSortJob[i];
        }
        for (u32 i = 0; i < KU_NUM_ENVMAP_SORT_JOBS; ++i)
        {
            maEnvmapSortJobs[i].DependsOn(*previous, EA::Jobs::Event::EVENT_WHEN_JOB_END);
            previous = &maEnvmapSortJobs[i];
        }
        for (u32 i = 0; i < sizeof(sceneJobs) / sizeof(sceneJobs[0]); ++i)
        {
            sceneJobs[i]->DependsOn(*previous, EA::Jobs::Event::EVENT_WHEN_JOB_END);
            previous = sceneJobs[i];
        }
        scheduler.AddTree(previous);
    }
}

// ARTIST 823FFA18. The Xbox 360 path does not read the clear-stencil boolean.
void BrnRendererModule::BeginRenderAntiAliased(f32 whiteLevel, bool, u8 stencil)
{
    if (mbGreyBackgroundColour)
    {
        const f32 grey = whiteLevel * 0.3f;
        mvBackgroundColour.Set(grey, grey, grey, 0.0f);
    }
    else
        mvBackgroundColour.Set(whiteLevel * 0.72f, whiteLevel * 0.83f, whiteLevel * 0.89f, 0.0f);

    if (mbMultisampledBackbuffer)
    {
        const D3DVECTOR4 colour = { mvBackgroundColour.X().GetFloat(),
            mvBackgroundColour.Y().GetFloat(), mvBackgroundColour.Z().GetFloat(),
            mvBackgroundColour.W().GetFloat() };
        const BrnGraphics::AntiAliasTilingPlan& plan = BrnGraphics::KMSAA_TILING_PLAN;
        D3DRECT tiles[2];
        for (u32 i = 0; i < plan.mu32NumTiles; ++i)
        {
            const BrnGraphics::AntiAliasTilingPlan::TileRect& tile = plan.maTile[i];
            tiles[i].x1 = tile.mu32Left;
            tiles[i].y1 = tile.mu32Top;
            tiles[i].x2 = tile.mu32Right;
            tiles[i].y2 = tile.mu32Bottom;
        }

        D3DDevice* const device = gpD3DDevice;
        shadow::Device::SetState(mAllocatedRenderTargets.GetAntiAliasBuffer()
            ->GetRenderTarget()->mpSection0State);
        D3DDevice_BeginTiling(device, 0, plan.mu32NumTiles, tiles, &colour, 1.0f, stencil);
        CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miScreenClear);
        D3DDevice_SetPredication(device, 0);
        CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miScreenClear);
    }
    else
    {
        CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miScreenClear);
        shadow::Device::SetState(mAllocatedRenderTargets.GetAntiAliasBuffer()
            ->GetRenderTarget()->mpSection0State);
        CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miScreenClear);
    }
}

// ARTIST 823FFBE0. Resolve fragment zero for depth before resolving and clearing colour.
void BrnRendererModule::ResolveMSAA(f32, u8 stencil)
{
    const D3DVECTOR4 colour = { mvBackgroundColour.X().GetFloat(),
        mvBackgroundColour.Y().GetFloat(), mvBackgroundColour.Z().GetFloat(),
        mvBackgroundColour.W().GetFloat() };
    D3DDevice* const device = gpD3DDevice;
    const bool tiled = mbMultisampledBackbuffer;
    const BrnGraphics::AntiAliasTilingPlan& plan = tiled
        ? BrnGraphics::KMSAA_TILING_PLAN : BrnGraphics::KNO_MSAA_TILING_PLAN;
    CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miResolveMSAAFromEDRAM);
    for (u32 i = 0; i < plan.mu32NumTiles; ++i)
    {
        if (tiled)
            D3DDevice_SetPredication(device, 3u << (2 * i));

        const BrnGraphics::AntiAliasTilingPlan::TileRect& tile = plan.maTile[i];
        const D3DRECT rect = { tile.mu32Left, tile.mu32Top, tile.mu32Right, tile.mu32Bottom };
        const D3DPOINT dest = { tile.mu32Left, tile.mu32Top };
        D3DDevice_Resolve(device, D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_FRAGMENT0, &rect,
            mAllocatedRenderTargets.GetDownSampleBuffer()->GetDepthTexture(), &dest,
            0, 0, 0, 1.0f, stencil, 0);
        D3DDevice_Resolve(device, D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL, &rect,
            mAllocatedRenderTargets.GetDownSampleBuffer()->GetTexture(0), &dest,
            0, 0, &colour, 1.0f, stencil, 0);
    }
    CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miResolveMSAAFromEDRAM);
    if (tiled)
        D3DDevice_EndTiling(device, 0, 0, 0, 0, 1.0f, stencil, 0);
}

// ARTIST 82408B00. Keep the down-sample target saved across state and blit calls.
void BrnRendererModule::EndRenderAntiAliased()
{
    CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miDownsampleMSAAAndCompParticles);
    CgsRenderTarget* const downsample = mAllocatedRenderTargets.GetDownSampleBuffer();
    downsample->SetRenderTargetState(0);
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
        E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
        E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(
        E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));
    mAllocatedRenderTargets.BlitComposite(mIm2dRenderer, downsample,
        mAllocatedRenderTargets.GetParticleBuffer(), false, false);
    downsample->GetRenderTarget()->Resolve(false, true);
    CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miDownsampleMSAAAndCompParticles);
}

// ARTIST 823F63E0. Environment faces use inverted depth and the front-cull state.
void BrnRendererModule::BeginRenderEnvironmentMapFace(u32 face, f32 whiteLevel)
{
    const char* const file = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp";
    if (!mAllocatedRenderTargets.GetEnvMapBuffer())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mAllocatedRenderTargets.GetEnvMapBuffer()", file, 4163);
        CgsDev::Assert::EndAssert();
    }
    if (!mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget()", file, 4164);
        CgsDev::Assert::EndAssert();
    }
    mAllocatedRenderTargets.GetEnvMapBuffer()->SetRenderTargetStateInvertDepth(face);
    const f32 grey = whiteLevel * 0.3f;
    const renderengine::ClearColorParameters color = { { grey, grey, grey, 0.0f } };
    const renderengine::ClearDepthStencilParameters depthStencil = { 48, 0.0f, 0 };
    renderengine::Device::Clear(color, depthStencil, renderengine::RenderTargetState::TARGETID_COLOR_ALL);
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
        E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
        E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(
        E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));
}

// ARTIST 82408C38. Depth is copied only after binding and clearing the particle target.
void BrnRendererModule::BeginQuarterResBuffer()
{
    const char* const file = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp";
    if (!mAllocatedRenderTargets.GetParticleBuffer())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mAllocatedRenderTargets.GetParticleBuffer() != NULL", file, 4302);
        CgsDev::Assert::EndAssert();
    }
    if (!mAllocatedRenderTargets.GetParticleBuffer()->GetRenderTarget())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mAllocatedRenderTargets.GetParticleBuffer()->GetRenderTarget() != NULL", file, 4303);
        CgsDev::Assert::EndAssert();
    }
    mAllocatedRenderTargets.GetParticleBuffer()->SetRenderTargetState(0);
    const renderengine::ClearColorParameters color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    renderengine::Device::Clear(color, renderengine::RenderTargetState::TARGETID_COLOR_ALL);
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
        E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
        E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(
        E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST));
    mAllocatedRenderTargets.BlitDepth(mIm2dRenderer, mAllocatedRenderTargets.GetDownSampleBuffer());
    mAllocatedRenderTargets.GetParticleBuffer()->SetRenderTargetState(0);
}

// ARTIST 823F6730. Export bottom-up BGR rows from the native tiled front buffer.
void BrnRendererModule::TakeScreenshot()
{
    if (!mbDispatchThreadTakeScreenshot)
        return;

    renderengine::Device::LockedFrontBuffer frontBuffer;
    if (renderengine::Device::LockFrontBuffer(frontBuffer))
    {
        char name[256];
        CgsCore::SPrintf(name, sizeof(name), "%sscreenshot%d_%s.tga",
            "d:\\", muScreenshotCounter++, macScreenShotText);
        for (char* character = name; *character; ++character)
        {
            if (*character == ' ' || *character == ':')
                *character = '_';
        }

        FILE* const file = std::fopen(name, "wb");
        if (file)
        {
            u8 header[18] = {};
            header[2] = 2;
            header[12] = static_cast<u8>(frontBuffer.width);
            header[13] = static_cast<u8>(frontBuffer.width >> 8);
            header[14] = static_cast<u8>(frontBuffer.height);
            header[15] = static_cast<u8>(frontBuffer.height >> 8);
            header[16] = 24;
            std::fwrite(header, sizeof(header), 1, file);

            CGS_ALIGN(16) u8 row[0x1400];
            for (s32 y = static_cast<s32>(frontBuffer.height) - 1; y >= 0; --y)
            {
                if (frontBuffer.pitch > sizeof(row))
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("lLockedFrontBuffer.pitch <= kiMaxRowSize",
                        kRendererSource, 4752);
                    CgsDev::Assert::EndAssert();
                }
                const RECT source = { 0, y, frontBuffer.width, y + 1 };
                XGUntileSurface(row, frontBuffer.pitch, 0, frontBuffer.pixelData,
                    frontBuffer.width, frontBuffer.height, &source, 4);
                for (u32 x = 0; x < frontBuffer.width; ++x)
                {
                    const u8* const pixel = row + 4 * x;
                    const u8 bgr[] = { pixel[0], pixel[1], pixel[2] };
                    std::fwrite(bgr, sizeof(bgr), 1, file);
                }
            }
            std::fclose(file);
        }
        renderengine::Device::UnlockFrontBuffer(frontBuffer);
    }
    mbDispatchThreadTakeScreenshot = false;
}

// ARTIST 82405D80. The shader bank provides both VMX eye and light vectors.
void BrnRendererModule::ComputeSunCoronaVisibility()
{
    CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miSunCoronaVisibilityTest);
    const BrnShaderConstantsFrame& shaderFrame = maShaderConstantsFrames[mu8ShaderConstantsFrameInternal];
    const Matrix44 viewProjection = shaderFrame.GetViewProjectionMatrix();
    const Vector3 viewPosition = shaderFrame.GetViewPosition();
    mSunCorona.ComputeSunPositionOnScreen(viewProjection, viewPosition,
        shaderFrame.GetUnbiasedKeyLightDirection());
    mSunCorona.GenerateOcclusionBuffer(mAllocatedRenderTargets.GetSunCoronaBuffer(),
        mAllocatedRenderTargets.GetDownSampleBuffer());
    CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miSunCoronaVisibilityTest);
}

// ARTIST 82406208. Reuse the allocated preview sampler resource with the selected texture.
void BrnRendererModule::DebugDisplayRenderTarget(renderengine::Texture* texture,
    Vector2 position, Vector2 size, CgsDev::RGBA colour)
{
    using namespace CgsGraphics;
    mIm2dRenderer.BeginRendering();
    mTextureStateParams.mpTexture = texture;
    mpTextureState = renderengine::TextureState::Initialize(mTextureStateResource, mTextureStateParams);
    mIm2dRenderer.SetState(mpTextureState);

    const f32 x0 = position.mV.x;
    const f32 y0 = position.mV.y;
    const f32 x1 = x0 + size.mV.x;
    const f32 y1 = y0 + size.mV.y;
    const RGBA8 rgba = { static_cast<u8>(colour), static_cast<u8>(colour >> 8),
        static_cast<u8>(colour >> 16), static_cast<u8>(colour >> 24) };
    const Basic2dColouredTexturedVertex vertices[] = {
        { { x0, y0 }, rgba, { 0.0f, 0.0f } },
        { { x0, y1 }, rgba, { 0.0f, 1.0f } },
        { { x1, y0 }, rgba, { 1.0f, 0.0f } },
        { { x1, y1 }, rgba, { 1.0f, 1.0f } }
    };

    const ImRendererBase::StateLibrary& states = ImRendererBase::GetStateLibrary();
    mIm2dRenderer.SetTransform(Im2dTransform::GetDefault());
    mIm2dRenderer.SetState(states.mpBlendState_Standard);
    mIm2dRenderer.SetState(states.mpRasterizerState_CullNone);
    mIm2dRenderer.Render(static_cast<renderengine::PrimitiveType>(D3DPT_TRIANGLESTRIP), vertices, 4);
    mIm2dRenderer.EndRendering();
}

// ARTIST 82406410. The first six UV words inherit bytes from the shared default
// transform, which the original writes into the vertex scratch area before drawing.
void BrnRendererModule::RenderLetterBoxBars(CgsGraphics::Im2d& im2d, f32 aspectRatio)
{
    using namespace CgsGraphics;
    const f32 edge = (1.0f - aspectRatio) * 0.5f;
    const Im2dTransform transform = Im2dTransform::GetDefault();
    Basic2dColouredTexturedVertex vertices[4] = {};
    std::memcpy(vertices, &transform, sizeof(transform));
    im2d.BeginRendering();
    im2d.SetTransform(transform);

    const RGBA8 black = { 0, 0, 0, 0 };
    vertices[0].mv2Pos.x = 0.0f;
    vertices[0].mv2Pos.y = 0.0f;
    vertices[1].mv2Pos.x = 0.0f;
    vertices[1].mv2Pos.y = edge;
    vertices[2].mv2Pos.x = 1.0f;
    vertices[2].mv2Pos.y = 0.0f;
    vertices[3].mv2Pos.x = 1.0f;
    vertices[3].mv2Pos.y = edge;
    for (u32 i = 0; i < 4; ++i)
        vertices[i].mv4Colour = black;

    const renderengine::PrimitiveType strip = static_cast<renderengine::PrimitiveType>(D3DPT_TRIANGLESTRIP);
    im2d.Render(strip, vertices, 4);
    vertices[0].mv2Pos.y = 1.0f - edge;
    vertices[1].mv2Pos.y = 1.0f;
    vertices[2].mv2Pos.y = 1.0f - edge;
    vertices[3].mv2Pos.y = 1.0f;
    im2d.Render(strip, vertices, 4);
    im2d.EndRendering();
}

// ARTIST 82405A30. Three untextured strips in the standard 2D state.
void BrnRendererModule::RenderThreeThreadMonitors(bool thread0, bool thread1, bool thread2)
{
    using namespace CgsGraphics;
    const ImRendererBase::StateLibrary& states = ImRendererBase::GetStateLibrary();
    mIm2dRendererUntex.BeginRendering();
    mIm2dRendererUntex.SetState(states.mpBlendState_Standard);
    mIm2dRendererUntex.SetState(states.mpRasterizerState_CullNone);

    const f32 left[] = { 0.55f, 0.57f, 0.59f };
    const f32 right[] = { 0.565f, 0.585f, 0.605f };
    const bool overBudget[] = { thread0, thread1, thread2 };
    Basic2dColouredVertex vertices[3][4];
    for (u32 i = 0; i < 3; ++i)
    {
        const RGBA8 red = { 255, 0, 0, 255 };
        const RGBA8 green = { 0, 255, 0, 255 };
        const RGBA8 colour = overBudget[i] ? red : green;
        const Basic2dColouredVertex quad[] = {
            { { left[i], 0.91f }, colour }, { { left[i], 0.94f }, colour },
            { { right[i], 0.91f }, colour }, { { right[i], 0.94f }, colour }
        };
        std::memcpy(vertices[i], quad, sizeof(quad));
    }

    mIm2dRendererUntex.SetTransform(Im2dTransform::GetDefault());
    mIm2dRendererUntex.SetProgram(0);
    for (u32 i = 0; i < 3; ++i)
        mIm2dRendererUntex.Render(static_cast<renderengine::PrimitiveType>(D3DPT_TRIANGLESTRIP), vertices[i], 4);
    if (ImRendererBase::mgpActiveRenderer != static_cast<ImRendererBase*>(&mIm2dRendererUntex))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mgpActiveRenderer == this",
            "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/CgsImRenderer.h", 645);
        CgsDev::Assert::EndAssert();
    }
    ImRendererBase::mgpActiveRenderer = 0;
}

// ARTIST 0x82405E28: publish the update-side renderer resources.
void BrnRendererModule::Update(CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
                               RendererIO::InputBuffer* lpInput, RendererIO::OutputBuffer* lpOutput)
{
    const char* const file = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererModule.cpp";
    if (!lpOutput) { CgsDev::Assert::BeginAssert(); CgsDev::Assert::FireAssert("lpOutput", file, 4519); CgsDev::Assert::EndAssert(); }
    if (!lpInput) { CgsDev::Assert::BeginAssert(); CgsDev::Assert::FireAssert("lpInput", file, 4520); CgsDev::Assert::EndAssert(); }
    lpInput->LockForRead();
    lpOutput->LockForWrite();
    lpOutput->SetBrnCamera(lpInput->GetBrnCamera());
    lpOutput->SetReusableLoadingScreenAllocator(GetReusableLoadingScreenAllocator());
    const BrnDirector::Camera::Camera& camera = lpInput->GetBrnCamera();
    if (camera.GetFOV() > 0.1f)
    {
        CgsGraphics::Camera graphicsCamera;
        camera.CopyToCgsCamera(&graphicsCamera);
        const f32 horizontal = graphicsCamera.maProjectionScalars[1];
        const f32 vertical = graphicsCamera.maProjectionScalars[4];
        const f32 scale = horizontal >= 1.0f ? horizontal : 1.0f;
        const Vector4 viewScale(scale, (vertical / horizontal) * scale, 0.0f, 0.0f);
        mCoronaManager.GetSubmissionInterface()->SetCameraInfo(
            graphicsCamera.GetViewProjectionMatrix(), camera.GetPosition(), viewScale);
    }
    if (camera.GetEffects().mbRequestingScreenshot)
        mbUpdateThreadTakeScreenshot = true;
    lpInput->UnlockForRead();
    lpOutput->SetDispatchFrame(&mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite());
    lpOutput->SetIm2dRenderBuffer(&mIm2dRenderBuffer);
    lpOutput->SetIm3dRenderBuffer(&mIm3dRenderBuffer);
    lpOutput->SetIm3dRenderBufferUntex(&mIm3dRenderBufferUntex);
    lpOutput->SetIm3dDebugRenderBuffer(&mIm3dDebugRenderBuffer);
    lpOutput->SetIm2dDebugRenderBuffer(&mIm2dDebugRenderBuffer);
    lpOutput->SetBaseEffectsFrame(mEffectsArbitrator.GetExternalEffectsFrame(BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_BASE, 0));
    for (u8 slot = 0; slot < 4; ++slot)
        lpOutput->SetWorldEffectsFrame(slot, mEffectsArbitrator.GetExternalEffectsFrame(BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_WORLD, slot));
    for (u8 slot = 0; slot < 2; ++slot)
        lpOutput->SetFXEventsEffectsFrame(slot, mEffectsArbitrator.GetExternalEffectsFrame(BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_FX_EVENTS, slot));
    lpOutput->SetShaderConstantsFrame(&maShaderConstantsFrames[mu8ShaderConstantsFrameExternal]);
    lpOutput->SetBlobbyShadowBuffer(mBlobbyShadowManager.GetExternalBuffer());
    lpOutput->SetCoronaSubmissionInterface(mCoronaManager.GetSubmissionInterface());
    lpOutput->SetIm3dRenderBufferRacePosition(&mIm3dBufferRacePosition);
    lpOutput->SetIm3dRenderBufferMenusAndHud(&mIm3dBufferMenusAndHud);
    lpOutput->SetRenderSwitches(mRenderSwitches);
    RendererIO::ExternallyVisiblePerformanceMonitors monitors;
    monitors.miCPU_DT_RenderShadowmapNear = mCpuMonitors.miDispatchShadowmapNearCSMList;
    monitors.miCPU_DT_RenderShadowmapFar = mCpuMonitors.miDispatchShadowmapFarCSMList;
    monitors.miCPU_DT_RenderEnvmap = mCpuMonitors.miDispatchEnvmapLists;
    monitors.miCPU_DT_RenderPreZ = mCpuMonitors.miDispatchPreZ;
    monitors.miCPU_DT_RenderWorldOpaque = mCpuMonitors.miDispatchWorldOpaqueList;
    monitors.miCPU_DT_RenderWorldTransparent = mCpuMonitors.miDispatchWorldTransparentList;
    monitors.miGPU_RenderShadowmaps = mGpuMonitors.miShadowmap;
    monitors.miGPU_RenderEnvmap = mGpuMonitors.miEnvironmentMap;
    monitors.miGPU_RenderPreZ = mGpuMonitors.miPreZ;
    monitors.miGPU_RenderWorldOpaque = mGpuMonitors.miWorldOpaque;
    monitors.miGPU_RenderWorldTransparent = mGpuMonitors.miWorldTransparent;
    *lpOutput->GetExternallyVisiblePerformanceMonitors() = monitors;
    lpOutput->UnlockForWrite();
}
void BrnRendererModule::RenderAssert(const CgsDev::Assert::AssertData* lpAssertData)
{
    D3DDevice_AcquireThreadOwnership(gpD3DDevice);
    for (;;)
    {
        mIm3dDebugRenderBuffer.Clear();
        mIm2dDebugRenderBuffer.Clear();
        Matrix44 identity;
        identity.SetIdentity();
        CgsDev::DebugInterface debug;
        debug.GetDebugManager().RenderAssert(lpAssertData);
        const Vector3 cameraPosition(0.0f, 0.0f, 0.0f);
        debug.GetDebugManager().Render(identity, cameraPosition,
            &mIm3dDebugRenderBuffer, &mIm2dDebugRenderBuffer);
        mIm3dDebugRenderBuffer.Swap();
        mIm2dDebugRenderBuffer.Swap();
        D3DVIEWPORTF9 viewport;
        viewport.X = 0.0f; viewport.Y = 0.0f;
        viewport.Width = 1280.0f; viewport.Height = 720.0f;
        viewport.MinZ = 0.0f; viewport.MaxZ = 1.0f; viewport.Flags = 0;
        D3DDevice_SetViewportF(gpD3DDevice, &viewport);
        RECT scissor = { 0, 0, 1280, 720 };
        D3DDevice_SetScissorRect(gpD3DDevice, &scissor);
        mIm2dDebugRenderBuffer.Dispatch(&mIm2dRenderer);
        ++renderengine::gDeviceFrameCounter83271624;
        renderengine::Device::ShowPixelBuffer(&renderengine::gOriginalColorBuffer);
    }
}

// ARTIST 0x823F6690: clear the texture, vertex-stream and index bindings.
void BrnRendererModule::ClearGraphicsContext()
{
    D3DDevice* const device = gpD3DDevice;
    for (u32 sampler = 0; sampler < 16; ++sampler)
        D3DDevice_SetTexture(device, sampler, 0, 0x80000000ULL >> sampler);
    for (u32 stream = 0; stream < 5; ++stream)
        D3DDevice_SetStreamSource(device, stream, 0, 0, 0, 1ULL << (stream / 3));
    D3DDevice_SetIndices(device, 0);
}
