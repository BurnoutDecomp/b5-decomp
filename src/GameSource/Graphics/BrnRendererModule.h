#pragma once

#include "types.hpp"

namespace EA
{
namespace Thread
{
class RWMutex
{
public:
    RWMutex(const char* lpcName, bool lbIntraProcess);
};
}

namespace Jobs
{
class Job
{
public:
    Job(s32 liPriority = 0);
};
}
}

namespace CgsModule
{
class ModuleSingleBuffered
{
public:
    ModuleSingleBuffered()
        : mInputRWMutex(0, true)
        , mOutputRWMutex(0, true)
        , meBufferState(0)
        , miInputBufferIndex(0)
        , miOutputBufferIndex(0)
        , miPendingInputBufferIndex(0)
        , miPendingOutputBufferIndex(0)
    {
    }

private:
    EA::Thread::RWMutex mInputRWMutex;
    EA::Thread::RWMutex mOutputRWMutex;
    s32                 meBufferState;
    s32                 miInputBufferIndex;
    s32                 miOutputBufferIndex;
    s32                 miPendingInputBufferIndex;
    s32                 miPendingOutputBufferIndex;
};
}

namespace CgsGraphics
{
class BufferedDispatchFrame
{
public:
    BufferedDispatchFrame();
};

class DispatchFrame
{
};

class DispatchPacketInterpreter;
class DispatchCommand;
class Im2dRenderBuffer
{
};

class Im2d
{
};

class Im2dUntex
{
};

class Im3dRenderBuffer
{
};

class Im3d
{
};

class Im3dRenderBufferUntex
{
};

class Im3dUntex
{
};

class Im3dZOnly
{
};

class OcclusionCullManager
{
};
}

namespace BrnGraphics
{
class EffectsArbitrator
{
};

class Im3dSkyDome
{
};
}

namespace BrnResource
{
class LinearResourceAllocator;
}

namespace RendererIO
{
struct RenderSwitches
{
    bool mbRenderShadows;
    bool mbRenderEnvmap;
    bool mbRenderWorld;
    bool mbRenderProps;
    bool mbRenderRaceCars;
    bool mbRenderTraffic;
};
}

namespace renderengine
{
class Texture;
class TextureState;
}

struct BrnRendererMemory
{
};

struct BrnShaderConstantsFrame
{
};

struct TextureStateParameters
{
};

struct Resource
{
};

struct BrnBlobbyShadowManager
{
};

struct BrnCoronaManager
{
};

struct CgsBlendStateFactory
{
};

struct CgsRasterizerStateFactory
{
};

struct CgsDepthStencilStateFactory
{
};

struct SortInfo
{
};

struct OcclusionJobData
{
};

struct DispatchObjectContext
{
};

struct DispatchList
{
};

struct BrnSkyDomeManager
{
};

struct BrnSunCorona
{
};

struct LoadingScreenRenderer
{
};

struct ResourceHandle
{
};

struct ShadowMapRenderManager
{
};

struct DebugComponent
{
};

struct Vector3
{
    f32 mfX;
    f32 mfY;
    f32 mfZ;
    f32 mfW;

    void SetZero()
    {
        mfX = 0.0f;
        mfY = 0.0f;
        mfZ = 0.0f;
        mfW = 0.0f;
    }
};

struct Vector4
{
    f32 mfX;
    f32 mfY;
    f32 mfZ;
    f32 mfW;

    void SetZero()
    {
        mfX = 0.0f;
        mfY = 0.0f;
        mfZ = 0.0f;
        mfW = 0.0f;
    }
};

class BrnRendererModule : public CgsModule::ModuleSingleBuffered
{
public:
    enum ERendererPrepareStage
    {
        eRendererPrepareStart,
        eRendererPrepareManager,
        eRendererPrepareBlobbyShadows,
        eRendererPrepareCoronas,
        eRendererPrepareDone
    };

    enum ERendererReleaseStage
    {
        eRendererReleaseStart,
        eRendererReleaseCoronas,
        eRendererReleaseBlobbyShadows,
        eRendererReleaseManager,
        eRendererReleaseDone
    };

    enum EFrameStallStage
    {
        E_FRAMESTALL_NOT_STALLED,
        E_FRAMESTALL_SYNCING_BUFFERS,
        E_FRAMESTALL_STALLED
    };

    struct BrnCpuMonitors
    {
        s32 miWholeDispatchThread;
        s32 miObjectToMeshListConversion;
        s32 miStartSortJobs;
        s32 miStartTintBlendJob;
        s32 miDispatchShadowmapNearCSMList;
        s32 miWaitOnShadowNearSortJob;
        s32 miDispatchShadowmapFarCSMList;
        s32 miWaitOnShadowFarSortJob;
        s32 miDispatchEnvmapLists;
        s32 miWaitOnEnvmapSortJobs;
        s32 miDispatchWorldLists;
        s32 miDispatchWorldOpaqueList;
        s32 miDispatchWorldTransparentList;
        s32 miWaitOnWorldOpaqueSortJob;
        s32 miWaitOnWorldTransparentSortJob;
        s32 miGenerateOcclusionQueryList;
        s32 miDispatchOcclusionQueries;
        s32 miDispatchCarLists;
        s32 miDispatchCarOpaqueList;
        s32 miDispatchCarTransparentList;
        s32 miWaitOnCarOpaqueSortJob;
        s32 miWaitOnCarTransparentSortJob;
        s32 miRenderSky;
        s32 miRenderCoronas;
        s32 miEffectsUpdate;
        s32 miBuildParticleVertexBuffers;
        s32 miWaitOnParticleVertexBuffersJob;
        s32 miRenderFullResParticles;
        s32 miRenderQuarterResParticles;
        s32 miPPUShaderPatching;
        s32 miRenderIm3d;
        s32 miRenderDebugData;
        s32 miRenderPostFX;
        s32 miRenderHUD;
        s32 miRenderApt;
        s32 miShowPixelBuffer;
        s32 miClearGraphicsContext;
        s32 miWaitOnPreZSortJob;
        s32 miDispatchPreZ;

        void Construct()
        {
            s32* lpMonitor = &miWholeDispatchThread;
            for (u32 luIndex = 0; luIndex < sizeof(BrnCpuMonitors) / sizeof(s32); ++luIndex)
                lpMonitor[luIndex] = 0;
        }
    };

    struct BrnGpuMonitors
    {
        s32 miScreenClear;
        s32 miShadowmap;
        s32 miSky;
        s32 miEnvironmentMap;
        s32 miWorldOpaque;
        s32 miWorldTransparent;
        s32 miCarOpaque;
        s32 miCarTransparent;
        s32 miDownsampleMSAAAndCompParticles;
        s32 miSunCoronaVisibilityTest;
        s32 miFullResParticles;
        s32 miQuarterResParticles;
        s32 miCoronas;
        s32 miPostFX;
        s32 miIm3dAndRacePositions;
        s32 miMenusAndHud;
        s32 miDebug3d;
        s32 miDebug2d;
        s32 miPreZ;

        void Construct()
        {
            s32* lpMonitor = &miScreenClear;
            for (u32 luIndex = 0; luIndex < sizeof(BrnGpuMonitors) / sizeof(s32); ++luIndex)
                lpMonitor[luIndex] = 0;
        }
    };

    struct BrnGpuHwCounters
    {
        s32 miEnvMap;
        s32 miShadowMap;
        s32 miWorldOpaque;
        s32 miCarOpaque;
        s32 miWorldTransparent;
        s32 miCarTransparent;
        s32 miPostFX;

        void Construct()
        {
            s32* lpMonitor = &miEnvMap;
            for (u32 luIndex = 0; luIndex < sizeof(BrnGpuHwCounters) / sizeof(s32); ++luIndex)
                lpMonitor[luIndex] = 0;
        }
    };

    BrnRendererModule();

private:
    enum
    {
        KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS = 16,
        KU_NUM_SHADOWMAP_DISPATCH_JOBS = 5,
        KU_NUM_ENVMAP_SORT_JOBS = 6,
        KU_NUM_INTERPRET_FUNCTIONS = 4,
        KU_SCREENSHOT_TEXT_LENGTH = 32
    };

    void ClearDispatchCounters();
    void ClearScreenshotState();
    void ConstructRenderSwitches();

    ERendererPrepareStage mePrepareStage;
    ERendererReleaseStage meReleaseStage;
    s32                   mDisplayType;
    u16                   mu16FrontBufferHeight;
    bool                  mbIsHD;
    bool                  mbIsInterlaced;
    BrnRendererMemory     mAllocatedRenderTargets;
    CgsGraphics::BufferedDispatchFrame mDoubleBufferedDispatchFrame;
    CgsGraphics::DispatchFrame         mSingleBufferedDispatchFrame;
    BrnGraphics::EffectsArbitrator     mEffectsArbitrator;
    BrnShaderConstantsFrame            maShaderConstantsFrames[2];
    u8                    mu8ShaderConstantsFrameInternal;
    u8                    mu8ShaderConstantsFrameExternal;
    CgsGraphics::DispatchPacketInterpreter* mpInterpreter;
    void (*maInterpretFunctions[KU_NUM_INTERPRET_FUNCTIONS])(CgsGraphics::DispatchCommand*, CgsGraphics::DispatchFrame*, void*, f32);
    CgsGraphics::Im2dRenderBuffer       mIm2dRenderBuffer;
    CgsGraphics::Im2d                   mIm2dRenderer;
    CgsGraphics::Im2dUntex              mIm2dRendererUntex;
    CgsGraphics::Im3dRenderBuffer       mIm3dRenderBuffer;
    CgsGraphics::Im3d                   mIm3dRenderer;
    CgsGraphics::Im3dRenderBufferUntex  mIm3dRenderBufferUntex;
    CgsGraphics::Im3dUntex              mIm3dRendererUntex;
    BrnGraphics::Im3dSkyDome            mIm3dRendererSkyDome;
    CgsGraphics::Im3dZOnly              mIm3dRendererZOnly;
    CgsGraphics::Im3dRenderBuffer       mIm3dDebugRenderBuffer;
    CgsGraphics::Im2dRenderBuffer       mIm2dDebugRenderBuffer;
    renderengine::TextureState*         mpTextureState;
    TextureStateParameters              mTextureStateParams;
    Resource                            mTextureStateResource;
    renderengine::TextureState*         mpEnvMapTextureState;
    TextureStateParameters              mEnvMapTextureStateParams;
    Resource                            mEnvMapTextureStateResource;
    renderengine::Texture*              mpGlassFractureTexture;
    renderengine::TextureState*         mpGlassFractureTextureState;
    TextureStateParameters              mGlassFractureTextureStateParams;
    Resource                            mGlassFractureTextureStateResource;
    Resource                            mBackBufferTextureResource;
    renderengine::TextureState*         mpShadowMapTextureState[2];
    CgsGraphics::Im3dRenderBuffer       mIm3dBufferRacePosition;
    CgsGraphics::Im3dRenderBuffer       mIm3dBufferMenusAndHud;
    BrnBlobbyShadowManager              mBlobbyShadowManager;
    renderengine::Texture*              mpBlobbyShadowTexture;
    f32                                 mfBlobbyShadowAlpha;
    BrnCoronaManager                    mCoronaManager;
    CgsBlendStateFactory                mBlendStateFactory;
    CgsRasterizerStateFactory           mRasterizerStateFactory;
    CgsDepthStencilStateFactory         mDepthStencilStateFactory;
    BrnResource::LinearResourceAllocator* mpGraphicsAllocator;
    EA::Jobs::Job                       maObjectToMeshJob[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS];
    DispatchObjectContext               maObjectToMeshJobContext[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS];
    DispatchList*                       mapaObjectToMeshJobOutputDispatchLists[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS];
    EA::Jobs::Job                       maShadowMapSortJob[KU_NUM_SHADOWMAP_DISPATCH_JOBS];
    SortInfo                            maShadowMapSortJobData[KU_NUM_SHADOWMAP_DISPATCH_JOBS];
    EA::Jobs::Job                       maEnvmapSortJobs[KU_NUM_ENVMAP_SORT_JOBS];
    SortInfo                            maEnvmapSortJobData[KU_NUM_ENVMAP_SORT_JOBS];
    EA::Jobs::Job                       mPreZSortJob;
    SortInfo                            mPreZSortJobData;
    EA::Jobs::Job                       mWorldOpaqueSortJob;
    SortInfo                            mWorldOpaqueSortJobData;
    EA::Jobs::Job                       mCarOpaqueSortJob;
    SortInfo                            mCarOpaqueSortJobData;
    EA::Jobs::Job                       mWorldTransparentSortJob;
    SortInfo                            mWorldTransparentSortJobData;
    EA::Jobs::Job                       mCarTransparentSortJob;
    SortInfo                            mCarTransparentSortJobData;
    EA::Jobs::Job                       mOcclusionWorldOpaqueJob;
    OcclusionJobData                    mOcclusionJobWorldOpaqueInfo;
    bool                                mbMultisampledBackbuffer;
    bool                                mbShowEnvironmentMap;
    bool                                mbShowShadowMap;
    bool                                mbSortDisplayListsWideNotLong;
    s32                                 miShowShadowMapIndex;
    f32                                 mfAspectCorrection;
    RendererIO::RenderSwitches          mRenderSwitches;
    bool                                mbRenderPreZ;
    bool                                mbRenderWorldOpaque;
    bool                                mbRenderCarsOpaque;
    bool                                mbRenderSky;
    bool                                mbRenderWorldTransparent;
    bool                                mbRenderCarsTransparent;
    bool                                mbRenderBlobbyShadows;
    bool                                mbRenderParticles;
    bool                                mbRenderCoronas;
    bool                                mbRenderWorldImmediateMode;
    bool                                mbRenderPostFX;
    bool                                mbRenderHudImmediateMode;
    bool                                mbOcclusionCullCarOpaque;
    bool                                mbOcclusionCullWorldOpaque;
    bool                                mbOcclusionCullCarTransparent;
    bool                                mbOcclusionCullWorldTransparent;
    bool                                mbOcclusionCullShadowMap;
    bool                                mbPreZNearOnly;
    bool                                mbRenderPreZAlpha;
    f32                                 mfPreZDistanceThreshold;
    f32                                 mfOccludeeNearClipOffset;
    u32                                 muOcclusionCullIndexCountThreshold;
    bool                                mbGreyBackgroundColour;
    bool                                mbClearDispatchCounts;
    u32                                 mu32NumWorldOpaqueObjectTotals;
    u32                                 mu32NumCarOpaqueObjectTotals;
    u32                                 mu32NumWorldTransparentObjectTotals;
    u32                                 mu32NumCarTransparentObjectTotals;
    u32                                 mu32NumShadowObjectTotals;
    u32                                 mu32DispatchFrameCounter;
    u32                                 mu32NumWorldOpaqueObjects;
    u32                                 mu32NumCarOpaqueObjects;
    u32                                 mu32NumWorldTransparentObjects;
    u32                                 mu32NumCarTransparentObjects;
    u32                                 mu32NumShadowObjects;
    bool                                mbUpdateThreadTakeScreenshot;
    bool                                mbDispatchThreadTakeScreenshot;
    bool                                mbCaptureOverlaysInScreenshot;
    u32                                 muScreenshotCounter;
    char                                macScreenShotText[KU_SCREENSHOT_TEXT_LENGTH];
    Vector3                             mKeyLightDirection;
    Vector3                             mKeyLightColor;
    Vector3                             mKeyLightSpecularColour;
    Vector3                             mAmbientColour;
    Vector4                             mvBackgroundColour;
    const renderengine::Texture*        mpCloudDensity0Texture;
    const renderengine::Texture*        mpCloudLighting0Texture;
    BrnSkyDomeManager                   mSkyDome;
    BrnSunCorona                        mSunCorona;
    EFrameStallStage                    meFrameStallStage;
    s32                                 miFrameStallCountdown;
    CgsGraphics::OcclusionCullManager   mOcclusionCullManager;
    LoadingScreenRenderer               mLoadingScreenRenderer;
    ResourceHandle                      mCalibrationTextureHandle;
    BrnCpuMonitors                      mCpuMonitors;
    BrnGpuMonitors                      mGpuMonitors;
    BrnGpuHwCounters                    mGpuHwMonitors;
    s32                                 miCpuPerfMonDispatchThread;
    ShadowMapRenderManager              mShadowMapRenderManager;
    bool                                mbDiskErrorLastFrame;
    s32                                 miFramesSinceDiskErrorReported;
    DebugComponent                      mDebugComponent;
};

void BrnRendererModule::ClearDispatchCounters()
{
    mu32NumWorldOpaqueObjectTotals = 0;
    mu32NumCarOpaqueObjectTotals = 0;
    mu32NumWorldTransparentObjectTotals = 0;
    mu32NumCarTransparentObjectTotals = 0;
    mu32NumShadowObjectTotals = 0;
    mu32DispatchFrameCounter = 0;
    mu32NumWorldOpaqueObjects = 0;
    mu32NumCarOpaqueObjects = 0;
    mu32NumWorldTransparentObjects = 0;
    mu32NumCarTransparentObjects = 0;
    mu32NumShadowObjects = 0;
}

void BrnRendererModule::ClearScreenshotState()
{
    mbUpdateThreadTakeScreenshot = false;
    mbDispatchThreadTakeScreenshot = false;
    mbCaptureOverlaysInScreenshot = false;
    muScreenshotCounter = 0;

    for (u32 luIndex = 0; luIndex < KU_SCREENSHOT_TEXT_LENGTH; ++luIndex)
        macScreenShotText[luIndex] = 0;
}

void BrnRendererModule::ConstructRenderSwitches()
{
    mRenderSwitches.mbRenderShadows = true;
    mRenderSwitches.mbRenderEnvmap = true;
    mRenderSwitches.mbRenderWorld = true;
    mRenderSwitches.mbRenderProps = true;
    mRenderSwitches.mbRenderRaceCars = true;
    mRenderSwitches.mbRenderTraffic = true;

    mbRenderPreZ = true;
    mbRenderWorldOpaque = true;
    mbRenderCarsOpaque = true;
    mbRenderSky = true;
    mbRenderWorldTransparent = true;
    mbRenderCarsTransparent = true;
    mbRenderBlobbyShadows = true;
    mbRenderParticles = true;
    mbRenderCoronas = true;
    mbRenderWorldImmediateMode = true;
    mbRenderPostFX = true;
    mbRenderHudImmediateMode = true;
}

BrnRendererModule::BrnRendererModule()
{
    mePrepareStage = eRendererPrepareStart;
    meReleaseStage = eRendererReleaseStart;
    mDisplayType = 0;
    mu16FrontBufferHeight = 0;
    mbIsHD = false;
    mbIsInterlaced = false;

    mu8ShaderConstantsFrameInternal = 0;
    mu8ShaderConstantsFrameExternal = 0;
    mpInterpreter = 0;
    for (u32 luIndex = 0; luIndex < KU_NUM_INTERPRET_FUNCTIONS; ++luIndex)
        maInterpretFunctions[luIndex] = 0;

    mpTextureState = 0;
    mpEnvMapTextureState = 0;
    mpGlassFractureTexture = 0;
    mpGlassFractureTextureState = 0;
    mpShadowMapTextureState[0] = 0;
    mpShadowMapTextureState[1] = 0;
    mpBlobbyShadowTexture = 0;
    mfBlobbyShadowAlpha = 0.0f;
    mpGraphicsAllocator = 0;
    for (u32 luIndex = 0; luIndex < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++luIndex)
        mapaObjectToMeshJobOutputDispatchLists[luIndex] = 0;

    mbMultisampledBackbuffer = false;
    mbShowEnvironmentMap = false;
    mbShowShadowMap = false;
    mbSortDisplayListsWideNotLong = false;
    miShowShadowMapIndex = 0;
    mfAspectCorrection = 0.0f;
    ConstructRenderSwitches();

    mbOcclusionCullCarOpaque = false;
    mbOcclusionCullWorldOpaque = false;
    mbOcclusionCullCarTransparent = false;
    mbOcclusionCullWorldTransparent = false;
    mbOcclusionCullShadowMap = false;
    mbPreZNearOnly = false;
    mbRenderPreZAlpha = false;
    mfPreZDistanceThreshold = 0.0f;
    mfOccludeeNearClipOffset = 0.0f;
    muOcclusionCullIndexCountThreshold = 0;
    mbGreyBackgroundColour = false;

    mbClearDispatchCounts = false;
    ClearDispatchCounters();
    ClearScreenshotState();

    mKeyLightDirection.SetZero();
    mKeyLightColor.SetZero();
    mKeyLightSpecularColour.SetZero();
    mAmbientColour.SetZero();
    mvBackgroundColour.SetZero();
    mpCloudDensity0Texture = 0;
    mpCloudLighting0Texture = 0;

    meFrameStallStage = E_FRAMESTALL_NOT_STALLED;
    miFrameStallCountdown = 0;
    mCpuMonitors.Construct();
    mGpuMonitors.Construct();
    mGpuHwMonitors.Construct();
    miCpuPerfMonDispatchThread = 0;
    mbDiskErrorLastFrame = false;
    miFramesSinceDiskErrorReported = 0;
}
