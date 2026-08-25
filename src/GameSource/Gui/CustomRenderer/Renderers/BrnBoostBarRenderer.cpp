#include "BrnBoostBarRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT + the Begin/Fire/EndAssert front-end
#include "GameShared/GameClasses/Development/CgsStrStream.h" // StrStream (runtime-composed assert messages)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // AddMonitor ("BoostBar", page 3)
#include "GameSource/Gui/BrnGuiCache.h"                   // GuiCache (resources, time, loaded textures)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple (the load table)
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"      // rw::math::fpu::Vector4Template
#include "pc/gcm/renderengine/renderstates.h"             // renderengine::TextureState (Initialize/GetResourceDescriptor)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // the 2D command buffer (SetState/SetTransform/Render/PushMask)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"    // CgsGuiModuleIO::ImRendererSet (complete type for the render handler)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"  // CgsGui::AptIm2dRenderBuffer (the set's slot-0 buffer)
#include "GameSource/Gui/BrnCustomRendererManager.h"      // BrnGui::SetMaskRect (the shared clip-mask helper this TU's render paths push through)

#include <cstring>   // memset (the shatter-lattice clears)
#include <cmath>     // sin/cos (the shatter shards' centroid rotation)

// BrnGui::BoostBarRenderer -- the in-game boost gauge (see the header banner for the source
// map). This TU replaces the boot-trace minimal slice wholesale: the DWARF class landed in the
// header, and every body below is reconstructed from the X360 ARTIST asm with the PS3 DecFIGS
// export (which carries all the methods the X360 set lacks) as the shape/naming source.
//
// CAMPAIGN COMPLETE (2026-08-25): the whole class is reconstructed -- the lifecycle/state half
// (ctor..Update, InitResources, the shard math helpers) AND the render half (RenderComponent
// @0x82466638, RenderFire @0x82452AD8, RenderBillboardBar @0x82453318, RenderShatteredBar
// @0x82460630, SetBackground @0x8245B040, SetChainedInactiveMask @0x824536A8,
// CalculateShardVertices @0x8244B248, RenderQuad @0x8245AE30, ShowDebugScreen @0x82461250 +
// the four RenderDebugFire bodies). The TU is mounted in the exe build and the manager's
// slot 4 (E_BOOSTBAR) holds the live by-value subobject. The shared collaborators landed with
// it: BrnGui::SetMaskRect (BrnCustomRenderer.cpp, its X360 home), the ImRenderBuffer typed
// SetState bodies + the opcode-17 PushMask retype/dispatch (CgsImRenderBufferTemplate/
// CgsIm2dRenderBuffer), and the CgsGui::BillboardRenderer mount with the GUI state-table
// globals' PC folds (CgsBillboardRenderer.cpp).
//
// CONSTANT VALUES: the class constants are dynamically initialised on both consoles (Vector3/
// Vector4/VecFloat have constructors). The values below were recovered by emulating the X360
// dynamic-initialiser region's straight-line stores into the 0x82FBxxxx constant block (the six
// boost colours cross-validate exactly against the PS3 __static_initialization literals); each
// constant's X360 BSS address is noted where its using body attests it.

namespace BrnGui
{
namespace
{
    // The X360 assert file path baked into this TU's assert sites.
    const char* const KPC_ASSERT_FILE =
        "..\\..\\..\\GameSource\\Gui/CustomRenderer/Renderers/BrnBoostBarRenderer.cpp";
}

// ---------------------------------------------------------------------------------------------
// File constants (DWARF BrnBoostBarRenderer.cpp:25-140). Console-verified values; the VecFloat
// constants are held as plain f32 (the console splats each across a VMX register -- the PC math
// below is scalar).
// ---------------------------------------------------------------------------------------------

// cpp:25/26 -- the bar's screen rectangle in 0..1 screen proportions {x, y, z=width-ish, w}:
// {0.082, 0.86, 0.5, 0.925} (PS3 static-init literals 0x3DA7EF9E/0x3F5C28F6/0x3F000000/0x3F6CCCCD).
static const Vector4 KV4_BOOSTBAR_RECT = { 0.082f, 0.86f, 0.5f, 0.925f };
static const rw::math::fpu::Vector4Template<f32> KV4_FPU_BOOSTBAR_RECT(0.082f, 0.86f, 0.5f, 0.925f);

// cpp:28-33 -- the per-boost-type inner/outer bar colours (HDR RGB; X360 BSS-map verified:
// danger inner 0x82FB3290, danger outer 0x82FB33E0, aggression inner 0x82FB2F90, aggression
// outer 0x82FB34A0, stunt inner 0x82FB3410, stunt outer 0x82FB3430).
static const Vector3 KV3_BOOSTTYPE_DANGER_INNER_COLOUR     = { 1.125f, 0.25f,  0.0f   };
static const Vector3 KV3_BOOSTTYPE_DANGER_OUTER_COLOUR     = { 2.0f,   1.625f, 1.2f   };
static const Vector3 KV3_BOOSTTYPE_AGGRESSION_INNER_COLOUR = { 2.0f,   0.125f, 0.15f  };
static const Vector3 KV3_BOOSTTYPE_AGGRESSION_OUTER_COLOUR = { 2.0f,   0.55f,  0.025f };
static const Vector3 KV3_BOOSTTYPE_STUNT_INNER_COLOUR      = { 0.375f, 0.95f,  0.3f   };
static const Vector3 KV3_BOOSTTYPE_STUNT_OUTER_COLOUR      = { 0.925f, 0.575f, 0.575f };

// cpp:124/140 -- the GUI-cache resource table Prepare waits on (X360 rodata @0x82F25728, count
// @0x82F25788): the twelve boost-bar textures, sequential cache ids 1..12, all request type 11.
// InitResources fetches each back with GuiCache::GetLoadedResource(<id>).
static const CgsGui::sResourceTuple maResourcesToLoad[12] =
{
    { 1,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 2,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 3,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 4,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 5,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 6,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 7,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 8,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 9,  static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 10, static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 11, static_cast<CgsGui::ResourceRequestTypes>(11) },
    { 12, static_cast<CgsGui::ResourceRequestTypes>(11) },
};
static const u32 muNumResourceToLoad = 12;

// ---------------------------------------------------------------------------------------------
// The render-half class statics (declared in the header; the consoles initialise every one of
// these dynamically -- the values are the PS3 unity static-init literals, and the four
// multiplier UV windows are additionally X360-BSS-verified at 0x82FB3210/0x82FB3460/
// 0x82FB34D0/0x82FB2FE0). All sizes/offsets are 0..1 screen proportions.
// ---------------------------------------------------------------------------------------------
const f32 BoostBarRenderer::KF_BOOSTING_FLAME_X_SCALE       = 0.1f;
const f32 BoostBarRenderer::KF_BOOSTING_FLAME_Y_SCALE       = 3.0f;
const f32 BoostBarRenderer::KF_BOOSTING_FLAME_X_OFFSET      = 0.039999999f;
const f32 BoostBarRenderer::KF_BOOSTING_FLAME_Y_OFFSET      = -0.029999999f;
const f32 BoostBarRenderer::KF_FIRE_MASK_WIDTH              = 1.0f;
const f32 BoostBarRenderer::KF_GROW_FIREBALL_X_SIZE         = 0.12f;
const f32 BoostBarRenderer::KF_GROW_FIREBALL_Y_SCALE        = 3.5f;
const f32 BoostBarRenderer::KF_GROW_FIREBALL_X_OFFSET       = 0.0f;
const f32 BoostBarRenderer::KF_BLACK_SMOKE_X_SIZE           = 0.12f;
const f32 BoostBarRenderer::KF_BLACK_SMOKE_Y_SCALE          = 2.8f;
// The PS3 initialiser materialises X_OFFSET as `vmaddfp(v7, zero-splat, 0.12-splat)` == 0.12
// (a compiler move-through-madd); flagged here because it is the one value read through that
// idiom rather than a direct literal store.
const f32 BoostBarRenderer::KF_BLACK_SMOKE_X_OFFSET         = 0.12f;
const f32 BoostBarRenderer::KF_BLACK_SMOKE_Y_OFFSET         = -0.0099999998f;
const f32 BoostBarRenderer::KF_EARN_FLAME_WIDTH             = 0.2f;
const f32 BoostBarRenderer::KF_EARN_FLAME_X_OFFSET          = -0.015f;
const f32 BoostBarRenderer::KF_EARN_FLAME_Y_SCALE           = 2.5f;
const f32 BoostBarRenderer::KF_EARN_FLAME_FLICKER_PROP      = 0.5f;
const f32 BoostBarRenderer::KF_BACKGROUND_ENDCAP_WIDTH      = 0.02f;
const f32 BoostBarRenderer::KF_BACKGROUND_ENDCAP_YSCALE     = 1.1f;
const f32 BoostBarRenderer::KF_BACKGROUND_ENDCAP_XOFFSET    = -0.013f;
const f32 BoostBarRenderer::KF_FIRE_BODY_X_SIZE             = 0.12f;
const f32 BoostBarRenderer::KF_FIRE_BODY_X_OFFSET           = -0.0099999998f;
const f32 BoostBarRenderer::KF_FIRE_BODY_Y_SCALE            = 3.0999999f;
const f32 BoostBarRenderer::KF_FIRE_BODY_Y_OFFSET           = -0.0080000004f;
const f32 BoostBarRenderer::KF_FIRE_BODY_MASK_WIDTH         = 1.0f;
const f32 BoostBarRenderer::KF_FIRE_BODY_ENDCAP_X_SIZE      = 0.07f;
const f32 BoostBarRenderer::KF_FIRE_BODY_ENDCAP_OFFSET      = -0.015f;
const f32 BoostBarRenderer::KF_FIRE_BODY_ENDCAP_FEATHER     = 0.02f;
const f32 BoostBarRenderer::KF_DANGER_END_GLOW_WIDTH        = 0.1f;
const f32 BoostBarRenderer::KF_AGRESSION_BOOST_TRANSPARENCY = 0.75f;
const f32 BoostBarRenderer::KF_BOOSTING_GLOW_INTENSITY      = 0.30000001f;   // X360 rodata 0x82054EE4
const f32 BoostBarRenderer::KF_SHAKE_X                      = 0.003f;
const f32 BoostBarRenderer::KF_SHAKE_Y                      = 0.005f;
const f32 BoostBarRenderer::KF_BACKGROUND_TILE_WIDTH        = 0.050000001f;
const Vector4 BoostBarRenderer::KV4_GLOW_COLOUR             = { 0.2f, 0.2f, 0.2f, 1.0f };
const Vector4 BoostBarRenderer::KV4_OVERLAY_COLOUR          = { 1.0f, 1.0f, 1.0f, 1.0f };
const Vector4 BoostBarRenderer::KV4_FIREBALL_COLOUR         = { 1.0f, 1.0f, 1.0f, 1.0f };
const Vector4 BoostBarRenderer::KV4_MULTIPLIER_2X_IMAGE_UV  = { 0.0f, 0.0f, 0.5f, 0.5f };
const Vector4 BoostBarRenderer::KV4_MULTIPLIER_3X_IMAGE_UV  = { 0.5f, 0.0f, 1.0f, 0.5f };
const Vector4 BoostBarRenderer::KV4_MULTIPLIER_2X_MASK_UV   = { 0.0f, 0.5f, 0.5f, 1.0f };
const Vector4 BoostBarRenderer::KV4_MULTIPLIER_3X_MASK_UV   = { 0.5f, 0.5f, 1.0f, 1.0f };

// The per-type constant pairs, boost-type indexed (the ForceSet(EBoostType) switch).
static const Vector3* const KAPV3_INNER_BY_TYPE[3] =
{
    &KV3_BOOSTTYPE_DANGER_INNER_COLOUR,
    &KV3_BOOSTTYPE_AGGRESSION_INNER_COLOUR,
    &KV3_BOOSTTYPE_STUNT_INNER_COLOUR,
};
static const Vector3* const KAPV3_OUTER_BY_TYPE[3] =
{
    &KV3_BOOSTTYPE_DANGER_OUTER_COLOUR,
    &KV3_BOOSTTYPE_AGGRESSION_OUTER_COLOUR,
    &KV3_BOOSTTYPE_STUNT_OUTER_COLOUR,
};

// ---------------------------------------------------------------------------------------------
// BoostBarRenderer -- construction / lifecycle
// ---------------------------------------------------------------------------------------------

// X360 ctor @0x827DF4F0 (EXECUTED in the boot trace). The member constructors do the real work
// on this build: the five Interpolators sentinel their time keys, the DeltaInterpolator opens
// its range, the six BillboardRenderers zero their bookkeeping tails, and the thirteen texture
// Resource slots start empty (the console seeds the first four with the shared empty-resource
// sentinel object and zero-fills the rest -- the PC rw::Resource's zero state is the same
// "nothing carved" condition). The one explicit store the members do not cover is the perfmon
// handle sentinel.
BoostBarRenderer::BoostBarRenderer()
{
    miBoostBarPM = -1;   // guest +53456 -1 store (Construct re-seeds it before AddMonitor)
}

// Faithful port of X360 Construct @0x8245A9A0 (with the PS3 0x401220 export as the member-name
// map). Base Construct, whole-state reset, the per-type colour arrays seeded from the constants,
// the shard random rolls pre-generated, and the "BoostBar" CPU perfmon monitor registered.
void BoostBarRenderer::Construct()
{
    CgsGui::CustomRenderComponentInterface::Construct();

    mePrepareStage      = E_PREPARESTAGE_START;     // guest +8
    meReleaseStage      = E_RELEASESTAGE_START;     // guest +12
    meBoostBarStatus    = E_STATUS_INVALID;         // guest +16
    meBoostBarMultiplier = E_MULTIPLIER_1X;         // guest +20
    mfLastTime          = -3.4028235e38f;           // guest +24

    // The six colour slots seed straight from the per-type constants (@0x8245A9F4-.. the six
    // lvx128/stvx128 pairs: outer[danger/aggression/stunt] then inner[...]).
    mav3BoostOuterColours[0] = KV3_BOOSTTYPE_DANGER_OUTER_COLOUR;
    mav3BoostOuterColours[1] = KV3_BOOSTTYPE_AGGRESSION_OUTER_COLOUR;
    mav3BoostOuterColours[2] = KV3_BOOSTTYPE_STUNT_OUTER_COLOUR;
    mav3BoostInnerColours[0] = KV3_BOOSTTYPE_DANGER_INNER_COLOUR;
    mav3BoostInnerColours[1] = KV3_BOOSTTYPE_AGGRESSION_INNER_COLOUR;
    mav3BoostInnerColours[2] = KV3_BOOSTTYPE_STUNT_INNER_COLOUR;

    meCurrentBoostType = BrnWorld::E_BOOST_TYPE_AGGRESSION;   // guest +128 = 1

    mpHeapAllocator = 0;                            // guest +1464
    mpImRenderers   = 0;                            // guest +1468

    // The eleven texture-state pointers the console zeroes here (@0x8245AA..: +1500/+1524/+1548/
    // +1596/+1620/+1644/+1668/+1692/+1716/+1740/+1764 -- the BackgroundEndCap and Glow pointers
    // are NOT in the console's store list, faithfully left to InitResources).
    mpWhiteTextureState         = 0;
    mpMaskTextureState          = 0;
    mpBackgroundTextureState    = 0;
    mpFireBodyTextureState      = 0;
    mpFireOverTextureState      = 0;
    mpEndCapTextureState        = 0;
    mpEarnFlameTextureState     = 0;
    mpEndGlowTextureState       = 0;
    mpBoostingFlameTextureState = 0;
    mpGrowFireballTextureState  = 0;
    mpMultiplierTextureState    = 0;

    // The shard random state: the console seeds the Random (guest +53504 = the packed
    // {1.0, 0x1AD0891B} seed pair, index 0) and pre-rolls its 8-slot float ring
    // (@0x8245AA80.. the eight inlined LCG iterations).
    mRandom.Construct();

    mpGuiCache = 0;                                 // guest +53520

    // Both boost-info payloads cleared with mbAllowedToBoost = true (the console's
    // field-by-field stores @+132..+157 and @+160..+185).
    mGuiEventBoostInfo.muNumChained            = 0;
    mGuiEventBoostInfo.mfBoostAmount           = 0.0f;
    mGuiEventBoostInfo.mfMaxBoost              = 0.0f;
    mGuiEventBoostInfo.meBoostType             = BrnWorld::E_BOOST_TYPE_DANGER;
    mGuiEventBoostInfo.mbBoostIsFull           = false;
    mGuiEventBoostInfo.mbIsBoosting            = false;
    mGuiEventBoostInfo.mbIsInAir               = false;
    mGuiEventBoostInfo.mbIsOncoming            = false;
    mGuiEventBoostInfo.mbIsDrifting            = false;
    mGuiEventBoostInfo.mbNearMiss              = false;
    mGuiEventBoostInfo.mbIsChainedMode         = false;
    mGuiEventBoostInfo.mbWasChainJustCompleted = false;
    mGuiEventBoostInfo.mbAllowedToBoost        = true;
    mGuiEventBoostInfo.mbIsTailgating          = false;
    mPreviousGuiEventBoostInfo                 = mGuiEventBoostInfo;

    mfIsEarningBoostStartTime = -3.4028235e38f;     // guest +188
    mfIsBoostingProp          = 0.0f;               // guest +192
    mfSlamGainStartTime       = -3.4028235e38f;     // guest +196
    mfSlamLossStartTime       = -3.4028235e38f;     // guest +200

    mChunkGainInterpolator.Invalidate();            // guest +212/+216 sentinel stores
    mfChunkGainPreviousMaxBoost = 0.0f;             // guest +220
    mfChunkGainShakeStartTime   = -3.4028235e38f;   // guest +224

    mfChunkLossStartTime        = 0.0f;             // guest +228
    mfChunkLossEndTime          = 0.0f;             // guest +232
    mfChunkLossPreviousMaxBoost = 0.0f;             // guest +236

    // The shatter lattice + shard motion cleared whole (the console's four memsets
    // @+240/280, +520/280, +800/384, +1184/192).
    std::memset(mv2VertexPos,                 0, sizeof(mv2VertexPos));
    std::memset(mv2VertexTex,                 0, sizeof(mv2VertexTex));
    std::memset(mav2ChunkLossShardVelocities, 0, sizeof(mav2ChunkLossShardVelocities));
    std::memset(mafChunkLossShardRotations,   0, sizeof(mafChunkLossShardRotations));

    mfCameraTransitionStopTime   = 0.0f;            // guest +1476
    mbCameraTransitionInProgress = false;           // guest +1473

    // The flame value runs 0..1 from zero (@ +1376..+1392: SetCurrentValue(0, 0),
    // SetDelta(0, 0), SetRange(0, 1)).
    mBoostFlameInterpolator.SetCurrentValue(0.0f, 0.0f);
    mBoostFlameInterpolator.SetDelta(0.0f, 0.0f);
    mBoostFlameInterpolator.SetRange(0.0f, 1.0f);

    // The amount interpolator holds a flat zero segment over all time (@ +1396..+1408:
    // {start 0 @ t=0, end 0 @ t=+FLT_MAX}); gain/chained stay ctor-invalidated.
    mBoostAmountInterpolator.SetStart(0.0f, 0.0f);
    mBoostAmountInterpolator.SetEnd(0.0f, 3.4028235e38f);

    mVisibilityInterpolator.Invalidate();           // guest +1452/+1456
    meVisibilityFadeState = E_VISIBILITY_FULL;      // guest +1460 = 3
    mbFirstFrame          = true;                   // guest +1472

    mbShowDebugScreen = false;                      // guest +53524

    // The "BoostBar" CPU perfmon monitor (page 3, budget 2.0ms, not libperf-tagged), with the
    // console's own registered-handle assert (:245).
    miBoostBarPM = -1;
    miBoostBarPM = CgsDev::PerfMonCpu::AddMonitor("BoostBar", static_cast<CgsDev::PerfMonCpuPage>(3),
                                                  false, 2.0f, false);
    CGS_ASSERT(miBoostBarPM >= 0, "miBoostBarPM >= 0");
}

// Faithful port of X360 Prepare @0x82451B28 -- the staged bring-up:
//   START: latch the heap allocator, -> LOAD.
//   LOAD : wait for the GuiCache to have every entry of maResourcesToLoad resident, -> INIT.
//   INIT : InitResources (build the thirteen texture states), -> DONE.
//   DONE : report prepared.
// An unknown stage fires the console's streamed assert (:302).
bool BoostBarRenderer::Prepare(CgsGui::GuiEventQueueSmall* /*lpEventQueue*/,
                               rw::IResourceAllocator* lpHeapAllocator,
                               rw::IResourceAllocator* /*lpTextureAllocator*/)
{
    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        mpHeapAllocator = lpHeapAllocator;   // guest a1[366]
        mePrepareStage  = E_PREPARESTAGE_LOAD;
        return false;

    case E_PREPARESTAGE_LOAD:
        if (mpGuiCache == 0 || !mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad,
                                                                     muNumResourceToLoad))
            return false;
        mePrepareStage = E_PREPARESTAGE_INIT;
        return false;

    case E_PREPARESTAGE_INIT:
        InitResources();
        mePrepareStage = E_PREPARESTAGE_DONE;
        return false;

    case E_PREPARESTAGE_DONE:
        mePrepareStage = E_PREPARESTAGE_DONE;
        return true;

    default:
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            lacMessage[0] = '\0';
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << " unknown prepare stage in BoostBarRenderer ";
            CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 302);
            CgsDev::Assert::EndAssert();
        }
        return false;
    }
}

// Faithful port of X360 Release @0x82446818: START or DONE both land DONE and report released;
// any other value fires the console's streamed assert (:343) and reports not-released.
bool BoostBarRenderer::Release()
{
    if (meReleaseStage != E_RELEASESTAGE_START && meReleaseStage != E_RELEASESTAGE_DONE)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        lacMessage[0] = '\0';
        CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << " unknown release stage in BoostBarRenderer ";
        CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 343);
        CgsDev::Assert::EndAssert();
        return false;
    }

    meReleaseStage = E_RELEASESTAGE_DONE;
    return true;
}

// PS3 0x3F9F84 (the X360 folds it): the base Destruct only.
void BoostBarRenderer::Destruct()
{
    CgsGui::CustomRenderComponentInterface::Destruct();
}

// Faithful port of X360 RecvEvent @0x8244A218. Three event routes:
//   64  -- the GuiCache pointer publish (asserted valid, :694);
//   206 -- the per-frame GuiEventBoostInfo payload: copied whole, range-checked with the
//          console's three "crazy value from bridge?" asserts (:640/:641/:642), the
//          over-full boost addition hack applied, and the first payload routed through
//          HandleFirstEvent;
//   377 -- the camera-transition gate: sub-modes 0/2 mark a transition in progress, 1/3 clear
//          it and hold the bar for KF_TRANSISTION_CAM_STOP_TIME (0.25s) past the stop.
void BoostBarRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
{
    if (!lpEvent)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        lacMessage[0] = '\0';
        CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << " null event passed ";
        CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 630);
        CgsDev::Assert::EndAssert();
    }

    switch (liEventType)
    {
    case 64:
        {
            GuiCache* const* lppGuiCache = reinterpret_cast<GuiCache* const*>(lpEvent);
            if (!*lppGuiCache)
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                lacMessage[0] = '\0';
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Invalid GUI cache pointer";
                CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 694);
                CgsDev::Assert::EndAssert();
            }
            mpGuiCache = *lppGuiCache;
        }
        break;

    case 206:
        {
            // The 28-byte payload copied whole (the console's 7-dword loop into +132).
            mGuiEventBoostInfo = *reinterpret_cast<const GuiEventBoostInfo*>(lpEvent);

            if (mGuiEventBoostInfo.mfMaxBoost <= 0.0f)
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                lacMessage[0] = '\0';
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Max boost is zero. Should never be told car can have zero boost. Crazy value from bridge?";
                CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 640);
                CgsDev::Assert::EndAssert();
            }
            if (mGuiEventBoostInfo.mfBoostAmount < 0.0f)
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                lacMessage[0] = '\0';
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Current boost is less than zero. Should never be told boost is less than zero. Crazy value from bridge?";
                CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 641);
                CgsDev::Assert::EndAssert();
            }
            if (mGuiEventBoostInfo.mfBoostAmount > mGuiEventBoostInfo.mfMaxBoost)
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                lacMessage[0] = '\0';
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Current boost is more than max. Should never be told boost is more than max. Crazy value from bridge?";
                CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 642);
                CgsDev::Assert::EndAssert();
            }

            // cpp:122 KF_BOOST_ADDITION_HACK: an over-unit boost amount is bumped by 1.5 (the
            // console folds the constant; the "hack" is the original's own name for it).
            if (mGuiEventBoostInfo.mfBoostAmount > 1.0f)
                mGuiEventBoostInfo.mfBoostAmount = mGuiEventBoostInfo.mfBoostAmount + 1.5f;

            if (mbFirstFrame)
            {
                HandleFirstEvent(&mGuiEventBoostInfo);
                mbFirstFrame = false;
            }
        }
        break;

    case 377:
        switch (*reinterpret_cast<const s32*>(lpEvent))
        {
        case 0:
        case 2:
            mbCameraTransitionInProgress = true;
            break;
        case 1:
        case 3:
            mbCameraTransitionInProgress = false;
            // KF_TRANSISTION_CAM_STOP_TIME (cpp:111) = 0.25s past the stop.
            mfCameraTransitionStopTime = mpGuiCache->GetTime() + 0.25f;
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}

// Faithful port of X360 HandleFirstEvent @0x824468D8: assert the payload, seed the visibility
// fade state from its allow-flag (allowed -> FULL, else NONE), and latch it as last frame's
// payload.
void BoostBarRenderer::HandleFirstEvent(const GuiEventBoostInfo* lpBoostBarInfo)
{
    CGS_ASSERT(lpBoostBarInfo != 0, "lpBoostBarInfo");

    meVisibilityFadeState = lpBoostBarInfo->mbAllowedToBoost ? E_VISIBILITY_FULL
                                                             : E_VISIBILITY_NONE;
    mPreviousGuiEventBoostInfo = *lpBoostBarInfo;
}

// X360 @0x8244B450 (an unnamed sub_ in the export set; PS3 0x3FBCE8 names it): select the
// per-type constant pair and set every slot of both colour arrays through the two-vector
// overload's stores. A type outside danger/aggression/stunt is a no-op.
void BoostBarRenderer::ForceSetBoostBarColours(BrnWorld::EBoostType leType)
{
    if (leType < BrnWorld::E_BOOST_TYPE_DANGER || leType > BrnWorld::E_BOOST_TYPE_STUNT)
        return;

    ForceSetBoostBarColours(*KAPV3_INNER_BY_TYPE[leType], *KAPV3_OUTER_BY_TYPE[leType]);
}

// Faithful port of X360 @0x82446970: all three outer slots take the outer colour, all three
// inner slots the inner colour (the asm stores the FIRST vector argument to the inner block
// +80/+96/+112 and the second to the outer block +32/+48/+64).
void BoostBarRenderer::ForceSetBoostBarColours(Vector3 lv3InnerColour, Vector3 lv3OuterColour)
{
    mav3BoostOuterColours[0] = lv3OuterColour;
    mav3BoostOuterColours[1] = lv3OuterColour;
    mav3BoostOuterColours[2] = lv3OuterColour;
    mav3BoostInnerColours[0] = lv3InnerColour;
    mav3BoostInnerColours[1] = lv3InnerColour;
    mav3BoostInnerColours[2] = lv3InnerColour;
}

// X360 @0x824468C0 -- the component's CgsID (the compressed "BoostBar", 0x740A1C80 == 1946688512).
CgsID BoostBarRenderer::GetID() const
{
    return 1946688512u;
}

// Faithful ports of X360 0x824EC750 / 0x824EC7D0: assert the current type is real, then return
// the type-indexed colour by value.
Vector3 BoostBarRenderer::GetInnerBoostBarColour()
{
    CGS_ASSERT(meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT &&
               meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE,
               "( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT ) && ( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE )");
    return mav3BoostInnerColours[meCurrentBoostType];
}

Vector3 BoostBarRenderer::GetOuterBoostBarColour()
{
    CGS_ASSERT(meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT &&
               meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE,
               "( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT ) && ( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE )");
    return mav3BoostOuterColours[meCurrentBoostType];
}

// ---------------------------------------------------------------------------------------------
// InitResources -- faithful port of X360 @0x8244A508 (PS3 0x403D3C). Builds the thirteen
// renderengine texture states over textures fetched from the GuiCache (the twelve
// maResourcesToLoad entries Prepare waited on, by cache id) plus the shared white texture.
// Per texture: fill a TextureState::Parameters block (the constant sampler config below, with
// the U-address mode the only per-texture variation -- 0 = wrap for the tiling background /
// end-cap / fire-body / fire-overlay / multiplier strips, 2 = clamp for the rest), carve the
// state's backing Resource through the heap allocator, and TextureState::Initialize it.
// Console block order (ptr slot <- source, addressU):
//   mpWhiteTextureState         <- the white texture global      U=2   (asserted, :726)
//   mpMaskTextureState          <- cache id 1                    U=2   (:741)
//   mpBackgroundTextureState    <- cache id 3                    U=0   (:756)
//   mpBackgroundEndCapTextureState <- cache id 4                 U=0
//   mpFireBodyTextureState      <- cache id 2                    U=0
//   mpFireOverTextureState      <- cache id 5                    U=0
//   mpEndCapTextureState        <- cache id 6                    U=2
//   mpEndGlowTextureState       <- cache id 7                    U=2
//   mpEarnFlameTextureState     <- cache id 8                    U=2
//   mpBoostingFlameTextureState <- cache id 9                    U=2
//   mpGrowFireballTextureState  <- cache id 10                   U=2
//   mpMultiplierTextureState    <- cache id 11                   U=0
//   mpGlowTextureState          <- cache id 12                   U=2
// [FLAG PC fold] the white texture: the console reads the immediate-mode state library's
// white texture global (X360 dword_83010F58 == mgStateLibrary.mpTexture_White). That library
// is a documented no-op on this backend (ImmediateModePCLeaf.cpp ConstructOnceOnly), so the
// white slot initialises over a null texture; the render paths guard it (the PC Apt dispatch
// draws untextured records as solid colour, which IS what a white-texture quad produces).
// ---------------------------------------------------------------------------------------------
namespace
{
    // One console texture-state build: parameters block + resource carve + Initialize.
    renderengine::TextureState* BuildBoostBarTextureState(rw::IResourceAllocator* lpAllocator,
                                                          rw::Resource& lrResource,
                                                          const renderengine::Texture* lpTexture,
                                                          u32 luAddressU)
    {
        renderengine::TextureState::Parameters lParams;
        lParams.muAddressU      = luAddressU;
        lParams.muAddressV      = 2;
        lParams.muAddressW      = 0;
        lParams.muMagFilter     = 1;
        lParams.muMinFilter     = 1;
        lParams.muMipFilter     = 2;
        lParams.muField6        = 0;
        lParams.muField7        = 0;
        lParams.muMaxAnisotropy = 13;
        lParams.muField9        = 0;
        lParams.muField10       = 1;
        lParams.mfMipLodBias    = 0.0f;
        lParams.mfField12       = 0.0f;
        lParams.muField13       = 0;
        lParams.muField14       = 0;
        lParams.muField15       = 0;
        lParams.mu8Field40      = 0;
        lParams.mu8Field41      = 0;
        lParams.mu8Field42      = 0;
        lParams.mu8Field43      = 1;
        lParams.mu8Field44      = 1;
        lParams.mpTexture       = const_cast<renderengine::Texture*>(lpTexture);

        // Size + carve the state's backing memory through the allocator (the console's
        // GetResourceDescriptor + vtbl DoAllocate pair), then initialise the state over it.
        renderengine::ResourceDescriptor5 lDescriptor;
        renderengine::TextureState::GetResourceDescriptor(reinterpret_cast<u32*>(&lDescriptor));
        lrResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), 0);
        return renderengine::TextureState::Initialize(&lrResource, &lParams);
    }
}

void BoostBarRenderer::InitResources()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

    // [FLAG PC fold -- see the banner] the state-library white texture global has no PC backing.
    const renderengine::Texture* lpWhiteTexture = 0;

    struct SlotBuild
    {
        rw::Resource*                lpResource;
        renderengine::TextureState** lppState;
        s32                          liCacheId;   // -1 = the white texture
        u32                          luAddressU;
    };
    const SlotBuild laBuilds[13] =
    {
        { &mWhiteTextureStateResource,            &mpWhiteTextureState,            -1, 2 },
        { &mMaskTextureStateResource,             &mpMaskTextureState,              1, 2 },
        { &mBackgroundTextureStateResource,       &mpBackgroundTextureState,        3, 0 },
        { &mBackgroundEndCapTextureStateResource, &mpBackgroundEndCapTextureState,  4, 0 },
        { &mFireBodyTextureStateResource,         &mpFireBodyTextureState,          2, 0 },
        { &mFireOverTextureStateResource,         &mpFireOverTextureState,          5, 0 },
        { &mEndCapTextureStateResource,           &mpEndCapTextureState,            6, 2 },
        { &mEndGlowTextureStateResource,          &mpEndGlowTextureState,           7, 2 },
        { &mEarnFlameTextureStateResource,        &mpEarnFlameTextureState,         8, 2 },
        { &mBoostingFlameTextureStateResource,    &mpBoostingFlameTextureState,     9, 2 },
        { &mGrowFireballTextureStateResource,     &mpGrowFireballTextureState,     10, 2 },
        { &mMultiplierTextureStateResource,       &mpMultiplierTextureState,       11, 0 },
        { &mGlowTextureStateResource,             &mpGlowTextureState,             12, 2 },
    };

    for (u32 luSlot = 0; luSlot < 13; ++luSlot)
    {
        const SlotBuild& lrBuild = laBuilds[luSlot];
        const renderengine::Texture* lpTexture;
        if (lrBuild.liCacheId < 0)
        {
            lpTexture = lpWhiteTexture;
            // The console's :726 assert names the white global; the PC fold's null is a
            // documented deviation (see the banner), so the assert is not reproduced for it.
        }
        else
        {
            lpTexture = static_cast<const renderengine::Texture*>(
                mpGuiCache->GetLoadedResource(static_cast<u32>(lrBuild.liCacheId)));
            CGS_ASSERT(lpTexture != 0, "lpTexture!=NULL");
        }
        *lrBuild.lppState = BuildBoostBarTextureState(mpHeapAllocator, *lrBuild.lpResource,
                                                      lpTexture, lrBuild.luAddressU);
    }

    // The six billboard-effect renderers, each Constructed over its texture state as soon as
    // that state exists on the console (interleaved with the builds above; gathered here after
    // the loop -- same states, same configs, no observable difference). Configs verbatim from
    // the six X360 Construct calls (max 32 billboards each; atlas framesX x framesY; the blend
    // pointers are the state-table entries dword_83010F20/dword_83010F24 -- the additive slot
    // goes to the fire-overlay and grow-fireball effects):
    //   [0] background    1x1   standard      [1] fire body     4x8   standard
    //   [2] fire overlay  4x5   ADDITIVE      [3] end cap       4x8   standard
    //   [4] boosting flame 4x4  standard      [5] grow fireball 12x7  ADDITIVE
    mBillboardRenderer[0].Construct(mpHeapAllocator, 32, mpBackgroundTextureState,
                                    CgsGui::gpGuiBlendStateStandard, 1, 1, 0);
    mBillboardRenderer[1].Construct(mpHeapAllocator, 32, mpFireBodyTextureState,
                                    CgsGui::gpGuiBlendStateStandard, 4, 8, 0);
    mBillboardRenderer[2].Construct(mpHeapAllocator, 32, mpFireOverTextureState,
                                    CgsGui::gpGuiBlendStateAdditive, 4, 5, 0);
    mBillboardRenderer[3].Construct(mpHeapAllocator, 32, mpEndCapTextureState,
                                    CgsGui::gpGuiBlendStateStandard, 4, 8, 0);
    mBillboardRenderer[4].Construct(mpHeapAllocator, 32, mpBoostingFlameTextureState,
                                    CgsGui::gpGuiBlendStateStandard, 4, 4, 0);
    mBillboardRenderer[5].Construct(mpHeapAllocator, 32, mpGrowFireballTextureState,
                                    CgsGui::gpGuiBlendStateAdditive, 12, 7, 0);
}

// ---------------------------------------------------------------------------------------------
// Update -- faithful port of X360 @0x82451C78 (PS3 0x410D10). The per-frame boost-state machine:
//   1. gates: the cache must exist, the gameplay-HUD ready trio (cache+0x4B54/56/58) must be up,
//      and the first event-206 payload must have arrived;
//   2. camera transitions FREEZE the bar: the live payload is reverted to last frame's and
//      nothing advances (also for KF_TRANSISTION_CAM_STOP_TIME past the stop, via RecvEvent);
//   3. the eased bar value re-keys from its current eased value to the live amount each frame
//      (0.1s ease on loss, 0.3s on gain); a positive delta stamps the earning-boost time;
//   4. the bar status derives from the boost type (danger active/inactive splits on whether the
//      chained-boost ramp is keyed), with the console's :456 unknown-type assert;
//   5. |delta| >= 15 stamps the slam gain (and keys the 1.9s gain flash) or slam loss;
//   6. a max-boost gain > 15 keys the 0.5s chunk-gain ramp; a loss < -15 rolls the 48 shatter
//      shards (velocity x in [0,0.2), y in (-0.6,0], rotation in [-4pi, 8pi) -- the console's
//      inlined RandomFloat draws in shard order x,y,rot), latches the chunk-loss window
//      (start = now + 0.25 delay, 1.25s transition) and rebuilds the shard lattice;
//   7. a FULL bar keys the chained-boost ramp (0.8s) + a 0.4s gain flash; dropping half a unit
//      below max while not boosting invalidates it;
//   8. the allowed-to-boost edge drives the visibility fade machine (0.25s in, 0.5s out,
//      partial-fade re-keys, with the console's four asserts :547/:555/:583/:591 and the two
//      streamed wrong-state asserts :565/:601);
//   9. the live payload latches as last frame's.
// ---------------------------------------------------------------------------------------------
void BoostBarRenderer::Update()
{
    if (mpGuiCache == 0)
        return;
    if (!mpGuiCache->GetGameplayHudReady())
        return;
    if (mbFirstFrame)
        return;

    const f32 lfTime = mpGuiCache->GetTime();

    if (mbCameraTransitionInProgress || lfTime < mfCameraTransitionStopTime)
    {
        mGuiEventBoostInfo = mPreviousGuiEventBoostInfo;
        return;
    }

    const f32 lfBoostDelta = mGuiEventBoostInfo.mfBoostAmount - mPreviousGuiEventBoostInfo.mfBoostAmount;
    const f32 lfMaxDelta   = mGuiEventBoostInfo.mfMaxBoost    - mPreviousGuiEventBoostInfo.mfMaxBoost;
    meCurrentBoostType = mGuiEventBoostInfo.meBoostType;

    if (lfBoostDelta > 0.0f)
        mfIsEarningBoostStartTime = lfTime;

    // Re-key the eased amount from its current eased value toward the live amount.
    const f32 lfEasedAmount = mBoostAmountInterpolator.GetCurrentValue(lfTime);
    mBoostAmountInterpolator.SetStart(lfEasedAmount, lfTime);
    mBoostAmountInterpolator.SetEnd(mGuiEventBoostInfo.mfBoostAmount,
                                    lfTime + ((lfBoostDelta < 0.0f) ? 0.1f : 0.30000001f));

    // 4. the bar status.
    switch (meCurrentBoostType)
    {
    case BrnWorld::E_BOOST_TYPE_DANGER:
        meBoostBarMultiplier = E_MULTIPLIER_1X;
        meBoostBarStatus = mChainedBoostInterpolator.IsValid() ? E_STATUS_DANGER_BOOST_ACTIVE
                                                               : E_STATUS_DANGER_BOOST_INACTIVE;
        break;
    case BrnWorld::E_BOOST_TYPE_AGGRESSION:
        meBoostBarStatus = E_STATUS_AGGRESSION_BOOST;
        break;
    case BrnWorld::E_BOOST_TYPE_STUNT:
        meBoostBarMultiplier = E_MULTIPLIER_1X;
        meBoostBarStatus = E_STATUS_STUNT_BOOST;
        break;
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Unknown boost type - cannot determine boost status",
                                   KPC_ASSERT_FILE, 456);
        CgsDev::Assert::EndAssert();
        meBoostBarMultiplier = E_MULTIPLIER_1X;
        meBoostBarStatus = E_STATUS_AGGRESSION_BOOST;
        break;
    }

    // 5. slam gain/loss.
    if (lfBoostDelta >= 15.0f && meBoostBarStatus != E_STATUS_DANGER_BOOST_INACTIVE)
    {
        mfSlamGainStartTime = lfTime;
        mBoostGainInterpolator.SetStart(0.0f, lfTime);
        mBoostGainInterpolator.SetEnd(1.0f, lfTime + 1.9f);
    }
    else if (lfBoostDelta <= -15.0f)
    {
        mfSlamLossStartTime = lfTime;
    }

    // 6. chunk gain / chunk-loss shatter.
    if (lfMaxDelta > 15.0f && mPreviousGuiEventBoostInfo.mfMaxBoost > 0.0f)
    {
        mChunkGainInterpolator.SetStart(0.0f, lfTime);
        mChunkGainInterpolator.SetEnd(1.0f, lfTime + 0.5f);   // KF_CHUNK_GAIN_TRANSITION_TIME
        mfChunkGainPreviousMaxBoost = mPreviousGuiEventBoostInfo.mfMaxBoost;
    }
    else if (lfMaxDelta < -15.0f)
    {
        // The console's per-shard draw order (x velocity, y velocity, rotation) is preserved so
        // the LCG sequence matches. Ranges: KF_CHUNK_LOSS_SHARD_VEL_X [0, 0.2),
        // KF_CHUNK_LOSS_SHARD_VEL_Y (-0.6, 0], rotation [-4pi, +8pi) (the inlined
        // rand*37.699112 - 12.566371).
        for (s32 liShard = 0; liShard < KI_CHUNK_LOSS_MAX_NUM_SHARDS; ++liShard)
        {
            mav2ChunkLossShardVelocities[liShard].x = mRandom.RandomFloat() * 0.2f;
            mav2ChunkLossShardVelocities[liShard].y = mRandom.RandomFloat() * -0.60000002f;
            mafChunkLossShardRotations[liShard]     = mRandom.RandomFloat() * 37.699112f - 12.566371f;
        }
        mfChunkLossPreviousMaxBoost = mPreviousGuiEventBoostInfo.mfMaxBoost;
        mfChunkLossStartTime = lfTime + 0.25f;                        // KF_CHUNK_LOSS_DELAY
        mfChunkLossEndTime   = mfChunkLossStartTime + 1.25f;          // KF_CHUNK_LOSS_TRANSITION_TIME
        CalculateShardVertices();
    }

    // 7. the chained-boost (full danger bar) ramp.
    if (mGuiEventBoostInfo.mbBoostIsFull)
    {
        if (!mChainedBoostInterpolator.IsValid())
        {
            mChainedBoostInterpolator.SetStart(0.0f, lfTime);
            mChainedBoostInterpolator.SetEnd(1.0f, lfTime + 0.80000001f);
            mfSlamGainStartTime = lfTime;
            mBoostGainInterpolator.SetStart(0.0f, lfTime);
            mBoostGainInterpolator.SetEnd(1.0f, lfTime + 0.40000001f);
        }
    }
    else if (!mGuiEventBoostInfo.mbIsBoosting &&
             mGuiEventBoostInfo.mfBoostAmount < (mGuiEventBoostInfo.mfMaxBoost - 0.5f))
    {
        mChainedBoostInterpolator.Invalidate();
    }

    // 8. the visibility fade machine, driven by the allowed-to-boost edge.
    const bool lbAllowed     = mGuiEventBoostInfo.mbAllowedToBoost;
    const bool lbWasAllowed  = mPreviousGuiEventBoostInfo.mbAllowedToBoost;
    if (!lbAllowed && lbWasAllowed)
    {
        // Boost just became unavailable -> fade out.
        switch (meVisibilityFadeState)
        {
        case E_VISIBILITY_FADING_IN:
            {
                if (!mVisibilityInterpolator.IsValid())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("mVisibilityInterpolator.IsValid() == true",
                                               KPC_ASSERT_FILE, 555);
                    CgsDev::Assert::EndAssert();
                }
                const f32 lfCurrent = mVisibilityInterpolator.GetCurrentValue(lfTime);
                mVisibilityInterpolator.SetStart(lfCurrent, lfTime);
                mVisibilityInterpolator.SetEnd(0.0f, lfCurrent * 0.5f + lfTime);
                meVisibilityFadeState = E_VISIBILITY_FADING_OUT;
            }
            break;

        case E_VISIBILITY_FULL:
            if (mVisibilityInterpolator.IsValid())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mVisibilityInterpolator.IsValid() == false",
                                           KPC_ASSERT_FILE, 547);
                CgsDev::Assert::EndAssert();
            }
            mVisibilityInterpolator.SetStart(1.0f, lfTime);
            mVisibilityInterpolator.SetEnd(0.0f, lfTime + 0.5f);   // KF_VISIBILITY_FADEOUT_TIME
            meVisibilityFadeState = E_VISIBILITY_FADING_OUT;
            break;

        default:
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                lacMessage[0] = '\0';
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Should not be able to run out of boost when renderer in visibility state "
                        << static_cast<s32>(meVisibilityFadeState) << "\n";
                CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 565);
                CgsDev::Assert::EndAssert();
            }
            mVisibilityInterpolator.SetStart(1.0f, lfTime);
            mVisibilityInterpolator.SetEnd(0.0f, lfTime + 0.5f);
            meVisibilityFadeState = E_VISIBILITY_FADING_OUT;
            break;
        }
    }
    else if (lbAllowed && !lbWasAllowed)
    {
        // Boost just became available -> fade in.
        switch (meVisibilityFadeState)
        {
        case E_VISIBILITY_NONE:
            if (mVisibilityInterpolator.IsValid())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mVisibilityInterpolator.IsValid() == false",
                                           KPC_ASSERT_FILE, 583);
                CgsDev::Assert::EndAssert();
            }
            mVisibilityInterpolator.SetStart(0.0f, lfTime);
            mVisibilityInterpolator.SetEnd(1.0f, lfTime + 0.25f);
            break;

        case E_VISIBILITY_FADING_OUT:
            {
                if (!mVisibilityInterpolator.IsValid())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("mVisibilityInterpolator.IsValid() == true",
                                               KPC_ASSERT_FILE, 591);
                    CgsDev::Assert::EndAssert();
                }
                const f32 lfCurrent = mVisibilityInterpolator.GetCurrentValue(lfTime);
                mVisibilityInterpolator.SetStart(lfCurrent, lfTime);
                mVisibilityInterpolator.SetEnd(1.0f, (1.0f - lfCurrent) * 0.25f + lfTime);
            }
            break;

        default:
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                lacMessage[0] = '\0';
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Should not be able to first earn boost when renderer in visibility state "
                        << static_cast<s32>(meVisibilityFadeState) << " \n";
                CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 601);
                CgsDev::Assert::EndAssert();
            }
            mVisibilityInterpolator.SetStart(0.0f, lfTime);
            mVisibilityInterpolator.SetEnd(1.0f, lfTime + 0.25f);
            break;
        }
        meVisibilityFadeState = E_VISIBILITY_FADING_IN;
    }

    // 9. latch the frame's payload.
    mPreviousGuiEventBoostInfo = mGuiEventBoostInfo;
}

// Faithful port of PS3 0x3FA100 (the X360 inlines it into SetBackground @0x8245B040, which
// arbitrates the member: `lfs f, +140` == mfMaxBoost): the chained-boost multiplier flame. Only
// the aggression-status bar carries it; the thresholds are KF_BOOST_2X_THRESHOLD (40) and
// KF_BOOST_3X_THRESHOLD (70) against the bar's MAX boost (the chained bars grow the capacity).
void BoostBarRenderer::DetermineBoostBarMultiplier()
{
    if (meBoostBarStatus == E_STATUS_AGGRESSION_BOOST &&
        mGuiEventBoostInfo.mfMaxBoost > 40.0f)
    {
        meBoostBarMultiplier = (mGuiEventBoostInfo.mfMaxBoost >= 70.0f) ? E_MULTIPLIER_3X
                                                                        : E_MULTIPLIER_2X;
    }
    else
    {
        meBoostBarMultiplier = E_MULTIPLIER_1X;
    }
}

// ---------------------------------------------------------------------------------------------
// RenderQuad -- faithful port of X360 @0x8245AE30 (PS3 export: RenderQuad(Vector4 const& rect,
// Vector4 const& colour, TextureState const*, BlendState const*, Vector4 uvRect)). One textured
// quad into the set's 2D command buffer: clamp the colour to [0,1], scale to bytes (the
// 255-splat at &unk_8305A950) and pack it, build the four tristrip corners from the rect
// {x0,y0,x1,y1} and the by-value UV window {u0,v0,u1,v1}, bind the texture + blend states,
// publish the shared screen transform (&unk_83011090 == gBillboardScreenTransform's console
// original), submit as a 4-vertex strip (primitive 6). The corner positions stay in the
// console's 0..1 screen proportions -- the shared transform carries the screen mapping, exactly
// as on the console (see gBillboardScreenTransform's PC-fold note in CgsBillboardRenderer.cpp).
// ---------------------------------------------------------------------------------------------
namespace
{
    // Clamp a colour lane to [0,1] and scale to a byte (the console's vmaxfp/vminfp + the
    // 255-splat multiply at &unk_8305A950).
    u8 BoostColourLaneToByte(f32 lfLane)
    {
        if (lfLane < 0.0f) lfLane = 0.0f;
        if (lfLane > 1.0f) lfLane = 1.0f;
        return static_cast<u8>(lfLane * 255.0f);
    }

    // The packed RGBA vertex-colour word from the clamped 0..1 lanes (shared by the quad and
    // billboard writers below).
    u32 PackBoostColour(const rw::math::vpu::Vector4& lrv4Colour)
    {
        return (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.w)) << 24) |
               (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.z)) << 16) |
               (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.y)) << 8)  |
               (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.x)));
    }

    // Resolve the set's slot-0 2D command buffer (the console's **(this+1468); the same double
    // deref RenderQuad below spells inline).
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>*
    ResolveBoostBarBuffer(CgsGui::ImRendererSet* lpImRenderers)
    {
        if (lpImRenderers == 0)
            return 0;
        CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
            *reinterpret_cast<CgsGui::AptIm2dRenderBuffer* const*>(lpImRenderers);
        return (lpAptBuffer != 0) ? &lpAptBuffer->mCommandBuffer : 0;
    }

    // Pack a 0..1 colour into the BillboardInfo::muDiffuse word. The billboard draw
    // (CgsGui::BillboardRenderer::Render) runs every stored word through its own X360-verbatim
    // PackVertexColour byte shuffle ([A B C D] -> [D B C A]) before it lands in the vertex, so
    // the stored word here is that shuffle's PREIMAGE: the shuffled result must be the
    // dispatch's little-endian [r,g,b,a] vertex word (the same convention RenderQuad packs
    // directly). Solving: store (r<<24)|(b<<16)|(g<<8)|a.
    u32 PackBillboardDiffuse(const rw::math::vpu::Vector4& lrv4Colour)
    {
        return (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.x)) << 24) |
               (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.z)) << 16) |
               (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.y)) << 8)  |
               (static_cast<u32>(BoostColourLaneToByte(lrv4Colour.w)));
    }
}

void BoostBarRenderer::RenderQuad(const Vector4& lv4Rect, const Vector4& lv4Colour,
                                  const renderengine::TextureState* lpTextureState,
                                  const renderengine::BlendState* lpBlendState,
                                  Vector4 lv4UVRect)
{
    if (mpImRenderers == 0)
        return;
    CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
        *reinterpret_cast<CgsGui::AptIm2dRenderBuffer* const*>(mpImRenderers);
    if (lpAptBuffer == 0)
        return;
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd =
        lpAptBuffer->mCommandBuffer;

    const u32 luColour = PackBoostColour(lv4Colour);

    // The four tristrip corners: (x0,y0)(x0,y1)(x1,y0)(x1,y1) with the matching UV corners
    // (the console's vertex build order), positions in 0..1 screen proportions.
    CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
    const f32 lafX[4] = { lv4Rect.x,   lv4Rect.x,   lv4Rect.z,   lv4Rect.z   };
    const f32 lafY[4] = { lv4Rect.y,   lv4Rect.w,   lv4Rect.y,   lv4Rect.w   };
    const f32 lafU[4] = { lv4UVRect.x, lv4UVRect.x, lv4UVRect.z, lv4UVRect.z };
    const f32 lafV[4] = { lv4UVRect.y, lv4UVRect.w, lv4UVRect.y, lv4UVRect.w };
    for (s32 liVert = 0; liVert < 4; ++liVert)
    {
        laVerts[liVert].mv2Pos.x    = lafX[liVert];
        laVerts[liVert].mv2Pos.y    = lafY[liVert];
        laVerts[liVert].mv2Tex0UV.x = lafU[liVert];
        laVerts[liVert].mv2Tex0UV.y = lafV[liVert];
        *reinterpret_cast<u32*>(&laVerts[liVert].mv4Colour) = luColour;
    }

    lrCmd.SetState(lpTextureState);
    lrCmd.SetState(lpBlendState);
    lrCmd.SetTransform(CgsGui::gBillboardScreenTransform);   // console &unk_83011090
    lrCmd.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
}

// =============================================================================================
// The DEBUG SCREEN (GuiCustRendererDebugComponent's menu toggle): a 4x5 grid of the four fire
// building blocks, each column a different inner/outer gradient-colour pairing, so a developer
// can eyeball every combination against the live boost type's colours.
// =============================================================================================

// Faithful port of X360 RenderDebugFireBody @0x82453758 (PS3 0x41FB40): one debug tile of the
// tiled fire BODY -- the same billboard build the real fire pass uses (cell width
// KF_FIRE_BODY_X_SIZE at 0.8 packing, height x KF_FIRE_BODY_Y_SCALE, frame seed time*30
// advancing +7 per cell), clipped by the MASK texture over the full-height strip ending at
// rect.z + sfEndCapOffset + KF_FIRE_BODY_ENDCAP_FEATHER, drawn by renderer [1].
void BoostBarRenderer::RenderDebugFireBody(const Vector4& lv4FireRect,
                                           const Vector4& lv4FireColour, f32 lfTimeNow)
{
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    if (lpRenderBuffer == 0)
        return;

    // The function-local static the X360 keeps at flt_82F25C3C (each debug tile owns one).
    static const f32 sfEndCapOffset = -0.015f;

    const f32 lfWidth   = lv4FireRect.z - lv4FireRect.x;
    const f32 lfHeight  = lv4FireRect.w - lv4FireRect.y;
    const f32 lfCentreY = (lv4FireRect.y + lv4FireRect.w) * 0.5f - 0.0080000004f;
    const f32 lfX0      = lv4FireRect.x + KF_FIRE_BODY_X_SIZE * 0.5f - 0.0099999998f;

    maBillboards.Clear();
    const s32 liCount = static_cast<s32>(lfWidth / (KF_FIRE_BODY_X_SIZE * 0.80000001f)) + 2;
    if (liCount > 0)
    {
        s32 liFrame = static_cast<s32>(lfTimeNow * 30.0f);
        for (s32 liCell = 0; liCell < liCount; ++liCell)
        {
            CgsGui::BillboardInfo lInfo = {};
            lInfo.mfPosX         = lfX0 + static_cast<f32>(liCell) *
                                          (KF_FIRE_BODY_X_SIZE * 0.80000001f);
            lInfo.mfPosY         = lfCentreY;
            lInfo.mfRotation     = 0.0f;
            lInfo.mfSizeX        = KF_FIRE_BODY_X_SIZE;
            lInfo.mfSizeY        = lfHeight * KF_FIRE_BODY_Y_SCALE;
            lInfo.muDiffuse      = PackBillboardDiffuse(lv4FireColour);
            lInfo.miTextureFrame = liFrame;
            maBillboards.Append(lInfo);
            liFrame += 7;   // the fire body's per-cell animation stagger
        }
    }

    const f32 lfMaskX1 = lv4FireRect.z + sfEndCapOffset + KF_FIRE_BODY_ENDCAP_FEATHER;
    const Vector4 lv4MaskRect = { lfMaskX1 - KF_FIRE_BODY_MASK_WIDTH, 0.0f, lfMaskX1, 1.0f };
    const Vector4 lv4MaskUVs  = { 0.0f, 0.0f, 1.0f, 1.0f };

    if (maBillboards.GetLength() > 0u)
    {
        BrnGui::SetMaskRect(lpRenderBuffer, mpMaskTextureState, lv4MaskRect, lv4MaskUVs);
        mBillboardRenderer[1].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                      static_cast<s32>(maBillboards.GetLength()));
        lpRenderBuffer->PopMask();
    }
}

// Faithful port of X360 RenderDebugFireOverlay @0x82453B60 (PS3 0x41F05C): the additive fire
// OVERLAY tile. Differences from the body tile, kept exactly: cell step 0.1 with NO 0.8
// packing (the COUNT still uses the body's 0.12*0.8 formula), height x0.7, centre y biased
// +0.005, one shared animation frame (no +7 stagger), renderer [2].
void BoostBarRenderer::RenderDebugFireOverlay(const Vector4& lv4FireRect,
                                              const Vector4& lv4FireColour, f32 lfTimeNow)
{
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    if (lpRenderBuffer == 0)
        return;

    static const f32 sfEndCapOffset = -0.015f;   // X360 flt_82F25C40

    const f32 lfWidth   = lv4FireRect.z - lv4FireRect.x;
    const f32 lfHeight  = lv4FireRect.w - lv4FireRect.y;
    const f32 lfCentreY = (lv4FireRect.y + lv4FireRect.w) * 0.5f - 0.0080000004f;

    const f32 lfMaskX1 = lv4FireRect.z + sfEndCapOffset + KF_FIRE_BODY_ENDCAP_FEATHER;
    const Vector4 lv4MaskRect = { lfMaskX1 - KF_FIRE_BODY_MASK_WIDTH, 0.0f, lfMaskX1, 1.0f };

    maBillboards.Clear();
    const s32 liCount = static_cast<s32>(lfWidth / (KF_FIRE_BODY_X_SIZE * 0.80000001f)) + 2;
    if (liCount > 0)
    {
        const u32 luDiffuse = PackBillboardDiffuse(lv4FireColour);
        const s32 liFrame   = static_cast<s32>(lfTimeNow * 30.0f);
        for (s32 liCell = 0; liCell < liCount; ++liCell)
        {
            CgsGui::BillboardInfo lInfo = {};
            lInfo.mfPosX         = 0.1f * 0.5f + lv4FireRect.x + static_cast<f32>(liCell) * 0.1f;
            lInfo.mfPosY         = lfCentreY + 0.004999999888f;
            lInfo.mfRotation     = 0.0f;
            lInfo.mfSizeX        = 0.1f;
            lInfo.mfSizeY        = lfHeight * 0.69999999f;
            lInfo.muDiffuse      = luDiffuse;
            lInfo.miTextureFrame = liFrame;
            maBillboards.Append(lInfo);
        }
    }

    if (maBillboards.GetLength() > 0u)
    {
        const Vector4 lv4MaskUVs = { 0.0f, 0.0f, 1.0f, 1.0f };
        BrnGui::SetMaskRect(lpRenderBuffer, mpMaskTextureState, lv4MaskRect, lv4MaskUVs);
        mBillboardRenderer[2].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                      static_cast<s32>(maBillboards.GetLength()));
        lpRenderBuffer->PopMask();
    }
}

// Faithful port of X360 RenderDebugFireEndCap @0x82454060 (PS3 0x41E7C8): ONE end-cap
// billboard at the fire end (rect.z + sfEndCapOffset), width KF_FIRE_BODY_ENDCAP_X_SIZE,
// height x KF_FIRE_BODY_Y_SCALE, clipped by the WHITE texture over {rect.x, 0, 1, 1} and
// drawn by renderer [3]. (Unlike the body/overlay tiles, no count guard: the single-billboard
// path always masks/renders/pops.)
void BoostBarRenderer::RenderDebugFireEndCap(const Vector4& lv4FireRect,
                                             const Vector4& lv4FireColour, f32 lfTimeNow)
{
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    if (lpRenderBuffer == 0)
        return;

    static const f32 sfEndCapOffset = -0.015f;   // X360 flt_82F25C44

    maBillboards.Clear();

    const f32 lfHeight  = lv4FireRect.w - lv4FireRect.y;
    const f32 lfCentreY = (lv4FireRect.y + lv4FireRect.w) * 0.5f - 0.0080000004f;

    CgsGui::BillboardInfo lInfo = {};
    lInfo.mfPosX         = lv4FireRect.z + sfEndCapOffset;
    lInfo.mfPosY         = lfCentreY;
    lInfo.mfRotation     = 0.0f;
    lInfo.mfSizeX        = KF_FIRE_BODY_ENDCAP_X_SIZE;
    lInfo.mfSizeY        = lfHeight * KF_FIRE_BODY_Y_SCALE;
    lInfo.muDiffuse      = PackBillboardDiffuse(lv4FireColour);
    lInfo.miTextureFrame = static_cast<s32>(lfTimeNow * 30.0f);
    maBillboards.Append(lInfo);

    const Vector4 lv4MaskRect = { lv4FireRect.x, 0.0f, 1.0f, 1.0f };
    const Vector4 lv4MaskUVs  = { 0.0f, 0.0f, 1.0f, 1.0f };
    BrnGui::SetMaskRect(lpRenderBuffer, mpWhiteTextureState, lv4MaskRect, lv4MaskUVs);
    mBillboardRenderer[3].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                  static_cast<s32>(maBillboards.GetLength()));
    lpRenderBuffer->PopMask();
}

// Faithful port of X360 RenderDebugFireGlow @0x8245B2C0 (PS3 0x425FC0; the mangling confirms
// the two-parameter form, and the colour parameter is DEAD on both consoles -- the tile's
// colour is derived locally). One additive glow quad: intensity = the KF_BOOSTING_GLOW_-
// INTENSITY lerp deliberately over-driven by 100 (RenderQuad's clamp saturates it to full),
// alpha 1, the glow texture over the whole tile, the additive blend slot (dword_83010F24).
void BoostBarRenderer::RenderDebugFireGlow(const Vector4& lv4FireRect,
                                           const Vector4& /*lv4FireColour -- dead, see above*/)
{
    const f32 lfGlow = (KF_BOOSTING_GLOW_INTENSITY - 0.0f) * 100.0f;
    const Vector4 lv4Colour = { lfGlow, lfGlow, lfGlow, 1.0f };
    const Vector4 lv4UVs    = { 0.0f, 0.0f, 1.0f, 1.0f };
    RenderQuad(lv4FireRect, lv4Colour, mpGlowTextureState,
               CgsGui::gpGuiBlendStateAdditive,   // X360 dword_83010F24
               lv4UVs);
}

// Faithful port of X360 ShowDebugScreen @0x82461250 (PS3 0x4260A8): the 4x5 debug grid.
// Program 3 (the boost-bar gradient shader) is bound for the whole grid; each cell pushes its
// gradient colour pair (opcode 21) then draws one fire building block -- rows are the four
// blocks (body / overlay / end cap / glow), columns the five colour pairings:
//   0: {0,0,0,0} / {1,1,1,0}          1: {1,1,1,0} / {0,0,0,0}
//   2: {0,0,0,0} / inner[type]        3: outer[type] / {0,0,0,0}
//   4: outer[type] / inner[type]           (type = meCurrentBoostType)
// [FLAG PC-platform leaf] the PC Im2d dispatch has no programmable 2D pipeline: SetProgram
// records ride the stream but select nothing (the fixed-function fold shades every batch the
// same way), and the opcode-21 colour pairs are recorded but not yet consumed. The grid still
// draws -- the textures/masks/billboards are all live -- without the gradient tinting.
void BoostBarRenderer::ShowDebugScreen()
{
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    if (lpRenderBuffer == 0)
        return;

    const f32 lfWidth   = KV4_BOOSTBAR_RECT.z - KV4_BOOSTBAR_RECT.x;
    const f32 lfHeight  = KV4_BOOSTBAR_RECT.w - KV4_BOOSTBAR_RECT.y;
    const f32 lfColumnW = 0.33333334f * lfWidth;
    const f32 lfStep    = lfColumnW + 0.015f;

    lpRenderBuffer->SetProgram(3);

    const Vector4 lv4Zero  = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector4 lv4Unit  = { 1.0f, 1.0f, 1.0f, 0.0f };
    const Vector4 lv4White = { 1.0f, 1.0f, 1.0f, 1.0f };

    // The live boost type's gradient pair, widened to Vector4 (the guest indexes the two
    // 16-byte-stride Vector3 arrays directly).
    const s32 liType = static_cast<s32>(meCurrentBoostType);
    const Vector4 lv4Inner = { mav3BoostInnerColours[liType].x, mav3BoostInnerColours[liType].y,
                               mav3BoostInnerColours[liType].z, 0.0f };
    const Vector4 lv4Outer = { mav3BoostOuterColours[liType].x, mav3BoostOuterColours[liType].y,
                               mav3BoostOuterColours[liType].z, 0.0f };

    const Vector4* lapv4ColourA[5] = { &lv4Zero, &lv4Unit, &lv4Zero,  &lv4Outer, &lv4Outer };
    const Vector4* lapv4ColourB[5] = { &lv4Unit, &lv4Zero, &lv4Inner, &lv4Zero,  &lv4Inner };
    const f32      lafRowY[4]      = { 0.1f, 0.2f, 0.30000001f, 0.40000001f };

    for (s32 liRow = 0; liRow < 4; ++liRow)
    {
        for (s32 liColumn = 0; liColumn < 5; ++liColumn)
        {
            lpRenderBuffer->PushBoostBarColours(*lapv4ColourA[liColumn],
                                                *lapv4ColourB[liColumn]);

            Vector4 lv4CellRect;
            lv4CellRect.x = lfStep * static_cast<f32>(liColumn) + 0.015f;
            lv4CellRect.y = lafRowY[liRow];
            lv4CellRect.z = lv4CellRect.x + lfColumnW;
            lv4CellRect.w = lv4CellRect.y + lfHeight;

            switch (liRow)
            {
            case 0: RenderDebugFireBody(lv4CellRect, lv4White, mpGuiCache->GetTime());    break;
            case 1: RenderDebugFireOverlay(lv4CellRect, lv4White, mpGuiCache->GetTime()); break;
            case 2: RenderDebugFireEndCap(lv4CellRect, lv4White, mpGuiCache->GetTime());  break;
            case 3: RenderDebugFireGlow(lv4CellRect, lv4White);                           break;
            }
        }
    }

    lpRenderBuffer->SetProgram(0);
}

// Faithful port of PS3 0x3FA070 (the X360 inlines it): each shard's tear-off is staggered by
// its reverse index at 1/192s per shard from the chunk-loss start; the lifetime is the time
// past that shard's own start (zero before it).
f32 BoostBarRenderer::CalculateBoostShardLifetime(s32 liShard, f32 lfTime)
{
    const f32 lfShardStart =
        static_cast<f32>(KI_CHUNK_LOSS_MAX_NUM_SHARDS - liShard) * 0.0052083335f +
        mfChunkLossStartTime;
    return (lfShardStart <= lfTime) ? (lfTime - lfShardStart) : 0.0f;
}

// Faithful port of X360 CalculateBoostShardAlpha @0x8244B3B0 (PS3 0x3FC404): full alpha for the
// first three quarters of the shard's life, a linear fade over the last quarter, zero past one;
// scaled to a clamped 0..255 byte (the console's fsel clamp pair).
u8 BoostBarRenderer::CalculateBoostShardAlpha(f32 lfLifetime, f32 lfAlpha)
{
    f32 lfFraction;
    if (lfLifetime > 1.0f)
        lfFraction = 0.0f;
    else if (lfLifetime <= 0.75f)
        lfFraction = lfAlpha;
    else
        lfFraction = lfAlpha - lfAlpha * ((lfLifetime - 0.75f) * 4.0f);

    f32 lfScaled = lfFraction * 255.0f;
    if (lfScaled < 0.0f)   lfScaled = 0.0f;
    if (lfScaled > 255.0f) lfScaled = 255.0f;
    return static_cast<u8>(lfScaled);
}

// =============================================================================================
// The RENDER half.
// =============================================================================================

// Faithful port of X360 SetChainedInactiveMask @0x824536A8 (PS3 0x40FA3C): push the
// chained-inactive clip mask -- the BACKGROUND texture over the given rect, its U window
// tiling 20 repeats per unit of bar width (the same 20.0 the shatter lattice's U scale uses),
// V spanning once.
void BoostBarRenderer::SetChainedInactiveMask(Im2dCommandBuffer* lpRenderBuffer, Vector4 lv4Rect)
{
    const Vector4 lv4MaskUVs = { 0.0f, 0.0f, 20.0f * (lv4Rect.z - lv4Rect.x), 1.0f };
    BrnGui::SetMaskRect(lpRenderBuffer, mpBackgroundTextureState, lv4Rect, lv4MaskUVs);
}

// Faithful port of X360 CalculateShardVertices @0x8244B248 (PS3 0x3F9FA8): rebuild the 5x7
// shatter lattice over the chunk being lost. The lattice spans the bar rect's X range between
// the CURRENT max boost and the max boost before the loss (both as 0..100 percentages of the
// bar width), split into 6 columns; Y spans the bar rect in 4 rows. The texture U runs at 20
// repeats per unit width across the lost extent (matching the background tiling), V spans the
// bar once in quarters. (The X360 reads the rect from its rodata copy of KV4_BOOSTBAR_RECT
// @0x82F25B40 -- byte-identical to the class constant.)
void BoostBarRenderer::CalculateShardVertices()
{
    const f32 lfBarWidth = KV4_BOOSTBAR_RECT.z - KV4_BOOSTBAR_RECT.x;
    const f32 lfRowStep  = (KV4_BOOSTBAR_RECT.w - KV4_BOOSTBAR_RECT.y) * 0.25f;
    const f32 lfXStart   = KV4_BOOSTBAR_RECT.x +
                           mGuiEventBoostInfo.mfMaxBoost * lfBarWidth * 0.0099999998f;
    const f32 lfXEnd     = KV4_BOOSTBAR_RECT.x +
                           mfChunkLossPreviousMaxBoost * lfBarWidth * 0.0099999998f;
    const f32 lfColStep  = (lfXEnd - lfXStart) * 0.16666667f;
    const f32 lfUScale   = (lfXEnd - lfXStart) * 20.0f;

    for (s32 liColumn = 0; liColumn < KI_CHUNK_LOSS_NUM_OF_SHARD_COLUMNS + 1; ++liColumn)
    {
        const f32 lfColumn = static_cast<f32>(liColumn);
        const f32 lfX = lfColumn * lfColStep + lfXStart;
        const f32 lfU = (lfUScale * lfColumn) * 0.16666667f;
        for (s32 liRow = 0; liRow < KI_CHUNK_LOSS_NUM_OF_SHARD_ROWS + 1; ++liRow)
        {
            mv2VertexPos[liRow][liColumn].x = lfX;
            mv2VertexPos[liRow][liColumn].y =
                KV4_BOOSTBAR_RECT.y + static_cast<f32>(liRow) * lfRowStep;
            mv2VertexTex[liRow][liColumn].x = lfU;
            mv2VertexTex[liRow][liColumn].y = static_cast<f32>(liRow) * 0.25f;
        }
    }
}

// Faithful port of X360 RenderBillboardBar @0x82453318 (PS3 export: RenderBillboardBar(Vector4
// const& rect, float proportion, Vector4 const& colour, BillboardRenderer*, float time)): tile
// the bar rect with one billboard per lfProportion-wide cell -- each cell-centred, full bar
// height, animation frame time*30 -- clip the run to the rect with the WHITE texture ({0,0,1,1}
// mask UVs == an opaque rectangular clip), submit through the given billboard renderer, pop.
void BoostBarRenderer::RenderBillboardBar(const Vector4& lv4Rect, f32 lfProportion,
                                          const Vector4& lv4Colour,
                                          CgsGui::BillboardRenderer* lpBillboardRenderer,
                                          f32 lfTime)
{
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    if (lpRenderBuffer == 0)
        return;

    maBillboards.Clear();   // the guest's direct count=0 store

    const f32 lfHeight = lv4Rect.w - lv4Rect.y;

    CgsGui::BillboardInfo lSeed = {};
    lSeed.mfPosX         = lv4Rect.x + lfProportion * 0.5f;
    lSeed.mfPosY         = lv4Rect.y + lfHeight * 0.5f;
    lSeed.mfRotation     = 0.0f;
    lSeed.mfSizeX        = lfProportion;
    lSeed.mfSizeY        = lfHeight;
    lSeed.muDiffuse      = PackBillboardDiffuse(lv4Colour);
    lSeed.miTextureFrame = static_cast<s32>(lfTime * 30.0f);
    maBillboards.Append(lSeed);

    // One more cell per whole lfProportion of bar width, each a copy of the previous shifted
    // one cell right (the guest's GetItem(i)/Append copy loop).
    const s32 liCells = static_cast<s32>((lv4Rect.z - lv4Rect.x) / lfProportion);
    for (s32 liCell = 0; liCell < liCells; ++liCell)
    {
        CgsGui::BillboardInfo lNext = maBillboards.GetItem(static_cast<u32>(liCell));
        lNext.mfPosX += lfProportion;
        maBillboards.Append(lNext);
    }

    const Vector4 lv4MaskUVs = { 0.0f, 0.0f, 1.0f, 1.0f };
    BrnGui::SetMaskRect(lpRenderBuffer, mpWhiteTextureState, lv4Rect, lv4MaskUVs);

    lpBillboardRenderer->Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                static_cast<s32>(maBillboards.GetLength()));
    lpRenderBuffer->PopMask();
}

// ---------------------------------------------------------------------------------------------
// RenderFire -- faithful port of X360 @0x82452AD8 (PS3 export: RenderFire(Vector4 const& rect,
// Vector4 const& colour, float timeNow)). The fire itself, three billboard passes over the fire
// rect, in this exact order:
//   1. FIRE BODY (renderer [1]): one billboard per KF_FIRE_BODY_X_SIZE*0.8 pitch across the
//      bar (count = width/pitch + 2), size {X_SIZE, height*Y_SCALE}, centred at
//      {rect.x + X_SIZE/2 + X_OFFSET, (rect.y+rect.w)/2 + Y_OFFSET}, animation frame
//      int(t*30) + 7 per cell, the caller's colour. Masked by the MASK texture over the
//      full-height strip {maskX - MASK_WIDTH, 0, maskX, 1} where maskX = rect.z +
//      ENDCAP_OFFSET + ENDCAP_FEATHER (the soft cut at the boost amount's end).
//   2. END CAP (renderer [3]): ONE billboard at {rect.z + ENDCAP_OFFSET, bodyCentreY}, size
//      {ENDCAP_X_SIZE, height*Y_SCALE}, frame int(t*30); masked by the WHITE texture over
//      {rect.x, 0, 1, 1} (everything right of the fire's start). Unconditional on both
//      consoles (the single-billboard path has no count guard).
//   3. FIRE OVERLAY (renderer [2], the additive pass): same count as pass 1 but pitch 0.1
//      (== its own billboard width, no 0.8 packing), size {0.1, height*0.7} (the function-
//      local static smv2OverlaySize, one-time-initialised from the first call's rect), origin
//      {rect.x + 0.05, bodyCentreY + 0.005}, ONE shared frame of int(t*45) while
//      mGuiEventBoostInfo.mbIsBoosting else int(t*30), colour = KV4_OVERLAY_COLOUR with the
//      caller colour's ALPHA spliced in (the gSwizzleStoreConstants[15] vperm == "copy b.w
//      into a.w": {1,1,1,fireColour.w}). Same mask strip as pass 1.
// ---------------------------------------------------------------------------------------------
void BoostBarRenderer::RenderFire(const Vector4& lv4Rect, const Vector4& lv4Colour,
                                  f32 lfTimeNow)
{
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    if (lpRenderBuffer == 0)
        return;

    const f32 lfBarHeight = lv4Rect.w - lv4Rect.y;
    const f32 lfBarWidth  = lv4Rect.z - lv4Rect.x;

    const f32 lfBodySizeY  = lfBarHeight * KF_FIRE_BODY_Y_SCALE;
    const f32 lfBodyX0     = lv4Rect.x + KF_FIRE_BODY_X_SIZE * 0.5f + KF_FIRE_BODY_X_OFFSET;
    const f32 lfBodyY      = (lv4Rect.y + lv4Rect.w) * 0.5f + KF_FIRE_BODY_Y_OFFSET;
    const f32 lfPitch      = KF_FIRE_BODY_X_SIZE * 0.80000001f;
    const s32 liNumBillboards = static_cast<s32>(lfBarWidth / lfPitch) + 2;

    // The pass-1/pass-3 mask strip: full height, ending at the fire's soft end.
    const f32 lfMaskX = lv4Rect.z + KF_FIRE_BODY_ENDCAP_OFFSET + KF_FIRE_BODY_ENDCAP_FEATHER;
    const Vector4 lv4BodyMaskRect = { lfMaskX - KF_FIRE_BODY_MASK_WIDTH, 0.0f, lfMaskX, 1.0f };
    const Vector4 lv4FullUVs      = { 0.0f, 0.0f, 1.0f, 1.0f };

    // ---- pass 1: the tiled fire body ---------------------------------------------------
    maBillboards.Clear();
    if (liNumBillboards > 0)
    {
        s32 liFrame = static_cast<s32>(lfTimeNow * 30.0f);
        for (s32 liCell = 0; liCell < liNumBillboards; ++liCell)
        {
            CgsGui::BillboardInfo lInfo = {};
            lInfo.mfPosX         = lfBodyX0 + static_cast<f32>(liCell) * lfPitch;
            lInfo.mfPosY         = lfBodyY;
            lInfo.mfRotation     = 0.0f;
            lInfo.mfSizeX        = KF_FIRE_BODY_X_SIZE;
            lInfo.mfSizeY        = lfBodySizeY;
            lInfo.muDiffuse      = PackBillboardDiffuse(lv4Colour);
            lInfo.miTextureFrame = liFrame;
            maBillboards.Append(lInfo);
            liFrame += 7;
        }
    }
    if (maBillboards.GetLength() != 0u)
    {
        BrnGui::SetMaskRect(lpRenderBuffer, mpMaskTextureState, lv4BodyMaskRect, lv4FullUVs);
        mBillboardRenderer[1].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                     static_cast<s32>(maBillboards.GetLength()));
        lpRenderBuffer->PopMask();   // the PS3 spells this BrnGui::UnsetMaskRect (a PopMask wrapper)
    }

    // ---- pass 2: the end cap (unconditional) -------------------------------------------
    maBillboards.Clear();
    {
        CgsGui::BillboardInfo lInfo = {};
        lInfo.mfPosX         = lv4Rect.z + KF_FIRE_BODY_ENDCAP_OFFSET;
        lInfo.mfPosY         = lfBodyY;
        lInfo.mfRotation     = 0.0f;
        lInfo.mfSizeX        = KF_FIRE_BODY_ENDCAP_X_SIZE;
        lInfo.mfSizeY        = lfBodySizeY;
        lInfo.muDiffuse      = PackBillboardDiffuse(lv4Colour);
        lInfo.miTextureFrame = static_cast<s32>(lfTimeNow * 30.0f);
        maBillboards.Append(lInfo);
    }
    {
        const Vector4 lv4EndCapMaskRect = { lv4Rect.x, 0.0f, 1.0f, 1.0f };
        BrnGui::SetMaskRect(lpRenderBuffer, mpWhiteTextureState, lv4EndCapMaskRect, lv4FullUVs);
        mBillboardRenderer[3].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                     static_cast<s32>(maBillboards.GetLength()));
        lpRenderBuffer->PopMask();
    }

    // ---- pass 3: the additive fire overlay ---------------------------------------------
    // {1, 1, 1, fireColour.w} -- the overlay colour with the caller's alpha spliced in.
    Vector4 lv4OverlayColour = KV4_OVERLAY_COLOUR;
    lv4OverlayColour.w = lv4Colour.w;

    const f32 lfOverlayFps = (mGuiEventBoostInfo.mbIsBoosting) ? 45.0f : 30.0f;

    // The console's function-local static (X360 0x82FB3750 + its 0x82FB3760 guard):
    // one-time-initialised from the FIRST call's rect height.
    struct OverlaySize { f32 x; f32 y; };
    static const OverlaySize smv2OverlaySize = { 0.1f, (lv4Rect.w - lv4Rect.y) * 0.69999999f };

    maBillboards.Clear();
    if (liNumBillboards > 0)
    {
        const u32 luDiffuse       = PackBillboardDiffuse(lv4OverlayColour);
        const s32 liOverlayFrame  = static_cast<s32>(lfOverlayFps * lfTimeNow);
        const f32 lfOverlayX0     = lv4Rect.x + smv2OverlaySize.x * 0.5f;
        const f32 lfOverlayY      = lfBodyY + 0.004999999888f;
        for (s32 liCell = 0; liCell < liNumBillboards; ++liCell)
        {
            CgsGui::BillboardInfo lInfo = {};
            lInfo.mfPosX         = lfOverlayX0 + static_cast<f32>(liCell) * smv2OverlaySize.x;
            lInfo.mfPosY         = lfOverlayY;
            lInfo.mfRotation     = 0.0f;
            lInfo.mfSizeX        = smv2OverlaySize.x;
            lInfo.mfSizeY        = smv2OverlaySize.y;
            lInfo.muDiffuse      = luDiffuse;
            lInfo.miTextureFrame = liOverlayFrame;
            maBillboards.Append(lInfo);
        }
    }
    if (maBillboards.GetLength() != 0u)
    {
        BrnGui::SetMaskRect(lpRenderBuffer, mpMaskTextureState, lv4BodyMaskRect, lv4FullUVs);
        mBillboardRenderer[2].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                     static_cast<s32>(maBillboards.GetLength()));
        lpRenderBuffer->PopMask();
    }
}

// ---------------------------------------------------------------------------------------------
// RenderShatteredBar -- faithful port of X360 @0x82460630 (PS3 0x424634; the DWARF keeps this
// one FPU-side, hence the scalar rect parameter). The chunk-loss shatter: the 4x6 lattice cells
// split into two triangles each (48 shards, 144 vertices, one triangle list). Per shard:
//   * lifetime from CalculateBoostShardLifetime (the reverse-index stagger);
//   * a positional offset {life*vel.x, life*(life*0.98 + vel.y)} while life is in [0,1] (the
//     0.98 gravity term is X360 rodata flt_82054EA4 = -0.98 through an fnmsubs -- the SAME
//     +0.98*life^2 the PS3 computes directly);
//   * a rotation of life*mafChunkLossShardRotations[i] about the triangle's own centroid
//     (pos[row][col] + {w/3,h/3} for the upper-left triangle, {2w/3,2h/3} for the lower-right;
//     w/h = the CALLER rect's cell extents). The console composes this as
//     T(-centroid) * M(quatZ(angle), centroid+offset) -- affine identities that reduce exactly
//     to p' = R(angle)*(p - centroid) + centroid + offset for the z=0 lattice points, which is
//     how the scalar body below spells it;
//   * vertex positions = the stored lattice points shifted by (rect.xy - lattice[0][0]) (the
//     lattice was built in KV4_BOOSTBAR_RECT space at loss time; the render rect may differ);
//   * vertex colours: the FIRST vertex of each triangle carries the caller's colour, the other
//     two carry white -- attested identically on BOTH consoles -- and every vertex's alpha
//     byte is replaced by CalculateBoostShardAlpha(life, colour.w);
//   * UVs straight from the stored lattice.
// Submit: SetState(mpBackgroundTextureState), SetState(standard blend dword_83010F20),
// SetTransform(&unk_83011090 == gBillboardScreenTransform), Render(TRIANGLES(4), verts, 144).
//
// (The PS3-only standalone CalculateBoostShardTransformation @0x401668 -- an uncalled sibling
// of the open-coded transform here, using a 0.3 edge-lerp centroid and a /48 column derivation
// where this body uses /4 -- is deliberately not reconstructed; the X360 target never emits it.)
// ---------------------------------------------------------------------------------------------
void BoostBarRenderer::RenderShatteredBar(const rw::math::fpu::Vector4Template<f32>& lv4Rect,
                                          f32 lfTime, const Vector4& lv4Colour)
{
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    if (lpRenderBuffer == 0)
        return;

    static const s32 KI_CHUNK_LOSS_NUM_VERTS = 144;   // 48 shards x 3 (the :1749 assert bound)

    const f32 lfShardWidth  = (lv4Rect.Z() - lv4Rect.X()) * 0.16666667f;
    const f32 lfShardHeight = (lv4Rect.W() - lv4Rect.Y()) * 0.25f;
    const f32 lfThirdW      = lfShardWidth  * 0.33333334f;
    const f32 lfThirdH      = lfShardHeight * 0.33333334f;
    const f32 lfTwoThirdW   = (lfShardWidth  * 2.0f) * 0.33333334f;
    const f32 lfTwoThirdH   = (lfShardHeight * 2.0f) * 0.33333334f;
    const f32 lfBaseX       = lv4Rect.X() - mv2VertexPos[0][0].x;
    const f32 lfBaseY       = lv4Rect.Y() - mv2VertexPos[0][0].y;

    // The base RGB of the caller's colour and of the two white corners, packed once (clamped
    // x255, the console's KF_COLOURSCALE fold); the per-shard alpha byte replaces .a below.
    const u32 luShardColourRGB = PackBoostColour(lv4Colour) & 0x00FFFFFFu;
    const u32 luWhiteRGB       = 0x00FFFFFFu;

    CgsGraphics::Basic2dColouredTexturedVertex laVerts[144];
    s32 liCurrentVertex = 0;

    for (s32 liShardIndex = 0; liShardIndex != KI_CHUNK_LOSS_MAX_NUM_SHARDS; liShardIndex += 2)
    {
        CGS_ASSERT(liCurrentVertex + 5 < KI_CHUNK_LOSS_NUM_VERTS,
                   "liCurrentVertex + 5 < KI_CHUNK_LOSS_NUM_VERTS");   // :1749

        const s32 liSquare = liShardIndex / 2;   // 0..23
        const s32 liRow    = liSquare % KI_CHUNK_LOSS_NUM_OF_SHARD_ROWS;
        const s32 liColumn = liSquare / KI_CHUNK_LOSS_NUM_OF_SHARD_ROWS;

        // The two triangles of this lattice cell: shard 2n = (R,C)(R,C+1)(R+1,C) about the
        // {w/3,h/3} centroid, shard 2n+1 = (R+1,C)(R+1,C+1)(R,C+1) about {2w/3,2h/3}.
        for (s32 liHalf = 0; liHalf < 2; ++liHalf)
        {
            const s32 liShard = liShardIndex + liHalf;
            const f32 lfLife  = CalculateBoostShardLifetime(liShard, lfTime);

            f32 lfOffsetX = 0.0f;
            f32 lfOffsetY = 0.0f;
            if (lfLife >= 0.0f && lfLife <= 1.0f)
            {
                lfOffsetX = lfLife * mav2ChunkLossShardVelocities[liShard].x;
                lfOffsetY = lfLife * (lfLife * 0.98000002f +
                                      mav2ChunkLossShardVelocities[liShard].y);
            }

            const u8  lu8Alpha  = CalculateBoostShardAlpha(lfLife, lv4Colour.w);
            const u32 luAlphaHi = static_cast<u32>(lu8Alpha) << 24;

            const f32 lfCentroidX = mv2VertexPos[liRow][liColumn].x +
                                    ((liHalf == 0) ? lfThirdW : lfTwoThirdW);
            const f32 lfCentroidY = mv2VertexPos[liRow][liColumn].y +
                                    ((liHalf == 0) ? lfThirdH : lfTwoThirdH);

            const f32 lfAngle = lfLife * mafChunkLossShardRotations[liShard];
            const f32 lfSin   = static_cast<f32>(std::sin(static_cast<double>(lfAngle)));
            const f32 lfCos   = static_cast<f32>(std::cos(static_cast<double>(lfAngle)));

            const s32 laiRows[2][3] = { { liRow,     liRow,     liRow + 1 },
                                        { liRow + 1, liRow + 1, liRow     } };
            const s32 laiCols[2][3] = { { liColumn,  liColumn + 1, liColumn     },
                                        { liColumn,  liColumn + 1, liColumn + 1 } };
            for (s32 liCorner = 0; liCorner < 3; ++liCorner)
            {
                const s32 liR = laiRows[liHalf][liCorner];
                const s32 liC = laiCols[liHalf][liCorner];

                const f32 lfInX = mv2VertexPos[liR][liC].x + lfBaseX;
                const f32 lfInY = mv2VertexPos[liR][liC].y + lfBaseY;

                // p' = R(angle)*(p - centroid) + centroid + offset (the console's
                // T(-c) * M(quatZ, c+off) affine pair, reduced -- see the banner).
                const f32 lfRelX = lfInX - lfCentroidX;
                const f32 lfRelY = lfInY - lfCentroidY;

                CgsGraphics::Basic2dColouredTexturedVertex& lrVert = laVerts[liCurrentVertex++];
                lrVert.mv2Pos.x = lfRelX * lfCos - lfRelY * lfSin + lfCentroidX + lfOffsetX;
                lrVert.mv2Pos.y = lfRelX * lfSin + lfRelY * lfCos + lfCentroidY + lfOffsetY;
                *reinterpret_cast<u32*>(&lrVert.mv4Colour) =
                    ((liCorner == 0) ? luShardColourRGB : luWhiteRGB) | luAlphaHi;
                lrVert.mv2Tex0UV.x = mv2VertexTex[liR][liC].x;
                lrVert.mv2Tex0UV.y = mv2VertexTex[liR][liC].y;
            }
        }
    }

    lpRenderBuffer->SetState(mpBackgroundTextureState);
    lpRenderBuffer->SetState(CgsGui::gpGuiBlendStateStandard);   // X360 dword_83010F20
    lpRenderBuffer->SetTransform(CgsGui::gBillboardScreenTransform);   // console &unk_83011090
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(4),   // TRIANGLES
                           laVerts, static_cast<u32>(KI_CHUNK_LOSS_NUM_VERTS));
}

// Faithful port of X360 SetBackground @0x8245B040 (PS3 export: SetBackground(Im2dRenderBuffer*,
// Vector4 rect, float alpha, float timeNow)): re-derive the chained-boost multiplier (the X360
// inlines DetermineBoostBarMultiplier here -- the member it keys on is mfMaxBoost, `lfs +140`),
// then draw the tiled background. With a multiplier flame active the background rect gives up
// its rightmost 0.05 to the flame strip (and the tiling clip is the strip's own multiplier-mask
// push); without one the whole rect draws under the MASK texture stretched once. Ends with the
// balancing PopMask either way.
void BoostBarRenderer::SetBackground(Im2dCommandBuffer* lpRenderBuffer, Vector4 lv4Rect,
                                     f32 lfAlpha, f32 lfTimeNow)
{
    DetermineBoostBarMultiplier();   // X360-inlined; keys on status==AGGRESSION + mfMaxBoost 40/70

    Vector4 lv4BackgroundRect = lv4Rect;
    const bool lbMultiplierFlame = (meBoostBarStatus == E_STATUS_AGGRESSION_BOOST &&
                                    meBoostBarMultiplier != E_MULTIPLIER_1X);
    if (lbMultiplierFlame)
    {
        // Shrink the background's right edge by the multiplier flame strip's width.
        lv4BackgroundRect.z = lv4Rect.z - KF_BACKGROUND_TILE_WIDTH;
    }
    else
    {
        const Vector4 lv4MaskUVs = { 0.0f, 0.0f, 1.0f, 1.0f };
        BrnGui::SetMaskRect(lpRenderBuffer, mpMaskTextureState, lv4Rect, lv4MaskUVs);
    }

    const Vector4 lv4Colour = { 1.0f, 1.0f, 1.0f, lfAlpha };
    RenderBillboardBar(lv4BackgroundRect, KF_BACKGROUND_TILE_WIDTH, lv4Colour,
                       &mBillboardRenderer[0], lfTimeNow);

    if (lbMultiplierFlame)
    {
        // The multiplier flame strip: the 0.05 between the shrunk background edge and the bar's
        // right edge, masked by the multiplier texture's own mask frame (2x = left column of
        // the 2x2 atlas's bottom row, 3x = right), tiled by the background renderer, then the
        // numeral quad from the atlas's image row with the {0,0,0,alpha} colour the guest
        // splices out of the background colour vector.
        const bool lb2X = (meBoostBarMultiplier == E_MULTIPLIER_2X);
        const Vector4 lv4MultiplierRect = { lv4BackgroundRect.z, lv4Rect.y, lv4Rect.z, lv4Rect.w };
        BrnGui::SetMaskRect(lpRenderBuffer, mpMultiplierTextureState, lv4MultiplierRect,
                            lb2X ? KV4_MULTIPLIER_2X_MASK_UV : KV4_MULTIPLIER_3X_MASK_UV);
        RenderBillboardBar(lv4MultiplierRect, KF_BACKGROUND_TILE_WIDTH, lv4Colour,
                           &mBillboardRenderer[0], lfTimeNow);

        const Vector4 lv4QuadColour = { 0.0f, 0.0f, 0.0f, lfAlpha };
        RenderQuad(lv4MultiplierRect, lv4QuadColour, mpMultiplierTextureState,
                   CgsGui::gpGuiBlendStateStandard,   // X360 dword_83010F20
                   lb2X ? KV4_MULTIPLIER_2X_IMAGE_UV : KV4_MULTIPLIER_3X_IMAGE_UV);
    }

    lpRenderBuffer->PopMask();
}

// ---------------------------------------------------------------------------------------------
// RenderComponent -- faithful port of X360 @0x82466638 (PS3 0x433C34, the shape/naming source;
// both builds inline nearly every callee, so the body below re-expresses the SAME sequence
// through the named methods). The per-frame draw orchestrator:
//   resource assert -> perfmon start -> buffer begin + cull-none rasterizer + the shared screen
//   transform -> the shaken bar rect -> the visibility fade gate -> background (SetBackground)
//   -> the chunk-gain flying bar -> the chunk-loss shatter -> program 3 + the gradient colour
//   pair -> the eased boost fill's fire (with the chained-boost transition / danger-inactive
//   dimming) -> the boosting flame billboard -> the grow fireball (slam gain) -> the earn-flame
//   flicker quad -> the multiplier flame window -> program 0 -> the danger end glow -> the
//   background end cap -> (debug screen) -> standard blend + end.
// ---------------------------------------------------------------------------------------------
void BoostBarRenderer::RenderComponent(CgsGui::ImRendererSet* lpImRenderers)
{
    // :961 -- every texture state must exist before the first draw.
    CGS_ASSERT(mpWhiteTextureState != 0 && mpMaskTextureState != 0 &&
               mpBackgroundTextureState != 0 && mpBackgroundEndCapTextureState != 0 &&
               mpFireBodyTextureState != 0 && mpFireOverTextureState != 0 &&
               mpEndCapTextureState != 0 && mpEndGlowTextureState != 0 &&
               mpEarnFlameTextureState != 0 && mpBoostingFlameTextureState != 0 &&
               mpGrowFireballTextureState != 0 && mpMultiplierTextureState != 0 &&
               mpGlowTextureState != 0,
               "BoostBarRenderer: RenderComponent() called when resources not loaded");

    if (miBoostBarPM >= 0)
        CgsDev::PerfMonCpu::StartMonitor(miBoostBarPM);

    CGS_ASSERT(lpImRenderers != 0, "lpImRenderers");                          // :967
    mpImRenderers = lpImRenderers;
    Im2dCommandBuffer* lpRenderBuffer = ResolveBoostBarBuffer(mpImRenderers);
    CGS_ASSERT(lpRenderBuffer != 0, "NULL != lpIm2dRenderBuffer");            // :972
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                                // :975
    if (lpRenderBuffer == 0 || mpGuiCache == 0)
    {
        if (miBoostBarPM >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miBoostBarPM);
        return;
    }

    const f32 lfTimeNow = mpGuiCache->GetTime();   // carries the :250 -FLT_MAX assert

    // The X360-only first-frame seed (absent from the PS3 build): an unset last-time backfills
    // one frame behind before the latch.
    if (mfLastTime == -3.4028235e38f)
        mfLastTime = lfTimeNow - mpGuiCache->GetTimeStep();
    mfLastTime = lfTimeNow;

    lpRenderBuffer->BeginRendering();
    lpRenderBuffer->SetState(CgsGui::gpGuiRasterizerStateCullNone);
    // The shared 2D screen transform the console stamps once for the whole bar stream
    // (Im2dTransform::mgAspectCorrected == the &unk_83011090 block; the PC fold is the
    // proportion -> logical-pixel scale).
    lpRenderBuffer->SetTransform(CgsGui::gBillboardScreenTransform);

    // ---- the shaken bar rect ------------------------------------------------------------
    Vector4 lv4Rect = KV4_BOOSTBAR_RECT;

    // (a) boosting jitter: +-half the shake extents on both corners.
    if (mGuiEventBoostInfo.mbIsBoosting)
    {
        const f32 lfShakeX = mRandom.RandomFloat(KF_SHAKE_X * -0.5f, KF_SHAKE_X * 0.5f);
        const f32 lfShakeY = mRandom.RandomFloat(KF_SHAKE_Y * -0.5f, KF_SHAKE_Y * 0.5f);
        lv4Rect.x += lfShakeX;  lv4Rect.z += lfShakeX;
        lv4Rect.y += lfShakeY;  lv4Rect.w += lfShakeY;
    }

    // (b) the chunk-gain impact: a damped 7 Hz X sine for 0.75s after the gain bar lands.
    const f32 lfChunkGainShakeElapsed = lfTimeNow - mfChunkGainShakeStartTime;
    if (lfChunkGainShakeElapsed < 0.75f)
    {
        const f32 lfSine = static_cast<f32>(std::sin(
            static_cast<double>(lfChunkGainShakeElapsed * 43.9823f)));   // 2*pi*7
        const f32 lfShift = (1.0f - lfChunkGainShakeElapsed / 0.75f) * -0.012f * lfSine;
        lv4Rect.x += lfShift;  lv4Rect.z += lfShift;
    }

    // (c) the chunk-loss pre-shake: ramps in over the 0.25s before the shards tear off.
    if (lfTimeNow < mfChunkLossStartTime)
    {
        f32 lfRamp = (mfChunkLossStartTime - lfTimeNow) * 4.0f;
        if (lfRamp > 1.0f) lfRamp = 1.0f;
        lfRamp = 1.0f - lfRamp;
        const f32 lfShakeX = mRandom.RandomFloat(lfRamp * -KF_SHAKE_X, lfRamp * KF_SHAKE_X);
        const f32 lfShakeY = mRandom.RandomFloat(lfRamp * -KF_SHAKE_Y, lfRamp * KF_SHAKE_Y);
        lv4Rect.x += lfShakeX;  lv4Rect.z += lfShakeX;
        lv4Rect.y += lfShakeY;  lv4Rect.w += lfShakeY;
    }

    // ---- the visibility fade gate -------------------------------------------------------
    if (mVisibilityInterpolator.IsFinished(lfTimeNow))
    {
        if (meVisibilityFadeState == E_VISIBILITY_FADING_IN)
            meVisibilityFadeState = E_VISIBILITY_FULL;
        else if (meVisibilityFadeState == E_VISIBILITY_FADING_OUT)
            meVisibilityFadeState = E_VISIBILITY_NONE;
        mVisibilityInterpolator.Invalidate();
    }

    f32  lfBaseAlpha  = 1.0f;
    bool lbRenderBody = true;
    switch (meVisibilityFadeState)
    {
    case E_VISIBILITY_NONE:
        if (mGuiEventBoostInfo.mbAllowedToBoost)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Visibility is none but we are allowed to boost",
                                       KPC_ASSERT_FILE, 1073);
            CgsDev::Assert::EndAssert();
        }
        lbRenderBody = false;
        break;

    case E_VISIBILITY_FADING_OUT:
    case E_VISIBILITY_FADING_IN:
        if (!((meVisibilityFadeState == E_VISIBILITY_FADING_IN &&
               mGuiEventBoostInfo.mbAllowedToBoost) ||
              (meVisibilityFadeState == E_VISIBILITY_FADING_OUT &&
               !mGuiEventBoostInfo.mbAllowedToBoost)))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Visibility is fading in the wrong direction for what we expect",
                KPC_ASSERT_FILE, 1065);
            CgsDev::Assert::EndAssert();
        }
        lfBaseAlpha = mVisibilityInterpolator.GetCurrentValue(lfTimeNow);
        if (!(lfBaseAlpha > 0.0f))
            lbRenderBody = false;
        break;

    case E_VISIBILITY_FULL:
        if (!mGuiEventBoostInfo.mbAllowedToBoost)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Visibility is full but we are not allowed to boost",
                                       KPC_ASSERT_FILE, 1080);
            CgsDev::Assert::EndAssert();
        }
        break;

    default:
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            lacMessage[0] = '\0';
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Unexpected visibility state in boost bar render cpt ( "
                    << static_cast<s32>(meVisibilityFadeState) << " ) \n";
            CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 1087);
            CgsDev::Assert::EndAssert();
        }
        break;
    }

    if (lbRenderBody)
    {
        // ---- base colour (the aggression bar runs at 0.75 alpha) ------------------------
        Vector4 lv4Colour = { 1.0f, 1.0f, 1.0f, lfBaseAlpha };
        if (meBoostBarStatus == E_STATUS_AGGRESSION_BOOST)
            lv4Colour.w *= KF_AGRESSION_BOOST_TRANSPARENCY;

        // ---- background ------------------------------------------------------------------
        // While a chunk gain is flying in, the background holds the PRE-GAIN max.
        const f32 lfMaxBoost = mChunkGainInterpolator.IsActive(lfTimeNow)
                             ? mfChunkGainPreviousMaxBoost
                             : mGuiEventBoostInfo.mfMaxBoost;
        const f32 lfBarWidth = lv4Rect.z - lv4Rect.x;

        Vector4 lv4BackgroundRect = lv4Rect;
        lv4BackgroundRect.z = lv4Rect.x + 0.0099999998f * lfBarWidth * lfMaxBoost;
        SetBackground(lpRenderBuffer, lv4BackgroundRect, lfBaseAlpha, lfTimeNow);

        const f32 lfBarHeight  = lv4Rect.w - lv4Rect.y;
        const f32 lfBarCentreY = (lv4Rect.w + lv4Rect.y) * 0.5f;

        // ---- the chunk-gain flying bar ---------------------------------------------------
        if (mChunkGainInterpolator.IsActive(lfTimeNow))
        {
            const f32 lfChunkWidth = 0.0099999998f * lfBarWidth * mfChunkGainPreviousMaxBoost;
            const f32 lfChunkX     = 1.0f + (lv4BackgroundRect.z - 1.0f) *
                                     mChunkGainInterpolator.GetCurrentValue(lfTimeNow);
            const Vector4 lv4ChunkRect =
                { lfChunkX, lv4Rect.y, lfChunkX + lfChunkWidth, lv4Rect.w };
            RenderBillboardBar(lv4ChunkRect, KF_BACKGROUND_TILE_WIDTH, lv4Colour,
                               &mBillboardRenderer[0], lfTimeNow);
        }
        if (mChunkGainInterpolator.IsFinished(lfTimeNow))
        {
            mfChunkGainShakeStartTime = lfTimeNow;
            mChunkGainInterpolator.Invalidate();
        }

        // ---- the chunk-loss shatter ------------------------------------------------------
        if (lfTimeNow < mfChunkLossEndTime)
        {
            const rw::math::fpu::Vector4Template<f32> lv4ShatterRect(
                lv4BackgroundRect.z,
                lv4Rect.y,
                lv4BackgroundRect.z + 0.0099999998f * lfBarWidth *
                    (mfChunkLossPreviousMaxBoost - mGuiEventBoostInfo.mfMaxBoost),
                lv4Rect.w);
            RenderShatteredBar(lv4ShatterRect, lfTimeNow, lv4Colour);
        }

        // ---- program 3 + the gradient colour pair ----------------------------------------
        lpRenderBuffer->SetProgram(3);
        {
            const s32 liType = static_cast<s32>(meCurrentBoostType);
            const rw::math::vpu::Vector4 lv4Outer =
                { mav3BoostOuterColours[liType].x, mav3BoostOuterColours[liType].y,
                  mav3BoostOuterColours[liType].z, 0.0f };
            const rw::math::vpu::Vector4 lv4Inner =
                { mav3BoostInnerColours[liType].x, mav3BoostInnerColours[liType].y,
                  mav3BoostInnerColours[liType].z, 0.0f };
            lpRenderBuffer->PushBoostBarColours(lv4Outer, lv4Inner);
        }

        // ---- the eased boost fill --------------------------------------------------------
        const f32 lfBoostProp = mBoostAmountInterpolator.GetCurrentValue(lfTimeNow);
        Vector4 lv4BoostRect = lv4Rect;
        lv4BoostRect.z = lv4Rect.x + 0.0099999998f * lfBarWidth * lfBoostProp;

        // The boost-flame ramp: +6/s while boosting, -1/s otherwise, clamped 0..1
        // (the console's Update / set-rate / Update triplet == SetDelta + read).
        mBoostFlameInterpolator.SetDelta(mGuiEventBoostInfo.mbIsBoosting ? 6.0f : -1.0f,
                                         lfTimeNow);
        mfIsBoostingProp = mBoostFlameInterpolator.GetCurrentValue(lfTimeNow);

        const f32 lfFlameSizeX = KF_BOOSTING_FLAME_X_SCALE;
        const f32 lfFlameSizeY = KF_BOOSTING_FLAME_Y_SCALE * lfBarHeight +
                                 KF_BOOSTING_FLAME_Y_OFFSET;

        // ---- the boosting flame billboard ------------------------------------------------
        if (mfIsBoostingProp > 0.0f)
        {
            maBillboards.Clear();

            f32 lfFlameAlpha = mfIsBoostingProp * 2.0f - 0.5f;
            if (lfFlameAlpha < 0.0f) lfFlameAlpha = 0.0f;
            if (lfFlameAlpha > 1.0f) lfFlameAlpha = 1.0f;

            CgsGui::BillboardInfo lInfo = {};
            lInfo.mfPosX     = lv4BoostRect.z - KF_BOOSTING_FLAME_X_SCALE +
                               0.5f * mfIsBoostingProp * KF_BOOSTING_FLAME_X_SCALE +
                               KF_BOOSTING_FLAME_X_OFFSET;
            lInfo.mfPosY     = lfBarCentreY + KF_BOOSTING_FLAME_Y_OFFSET;
            lInfo.mfRotation = 0.0f;
            lInfo.mfSizeX    = mfIsBoostingProp * lfFlameSizeX;
            lInfo.mfSizeY    = mfIsBoostingProp * lfFlameSizeY;
            Vector4 lv4FlameColour = lv4Colour;
            lv4FlameColour.w *= lfFlameAlpha;
            lInfo.muDiffuse      = PackBillboardDiffuse(lv4FlameColour);
            lInfo.miTextureFrame = static_cast<s32>(lfTimeNow * 30.0f);
            maBillboards.Append(lInfo);

            CGS_ASSERT(mpWhiteTextureState != 0, "mpWhiteTextureState");   // :1220
            const Vector4 lv4FlameMaskRect = { lv4BoostRect.x, 0.0f, 1.0f, 1.0f };
            const Vector4 lv4FullUVs       = { 0.0f, 0.0f, 1.0f, 1.0f };
            BrnGui::SetMaskRect(lpRenderBuffer, mpWhiteTextureState,
                                lv4FlameMaskRect, lv4FullUVs);
            mBillboardRenderer[4].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                         static_cast<s32>(maBillboards.GetLength()));
            lpRenderBuffer->PopMask();
        }

        // ---- the fire (chained-boost transition / danger-inactive dimming) ---------------
        const Vector4 lv4FullUVs = { 0.0f, 0.0f, 1.0f, 1.0f };
        if (mChainedBoostInterpolator.IsValid())
        {
            if (mChainedBoostInterpolator.IsActive(lfTimeNow))
            {
                // The chained-boost earn transition: dimmed red fire + glow under the
                // chained-inactive tiling mask, then the real fire sweeping in behind the
                // travelling mask edge.
                const f32 lfChained = mChainedBoostInterpolator.GetCurrentValue(lfTimeNow);
                const f32 lfChainX  = lv4BoostRect.x +
                                      (lv4BoostRect.z - lv4BoostRect.x) * lfChained;

                SetChainedInactiveMask(lpRenderBuffer, lv4BackgroundRect);
                Vector4 lv4FireColour = { 1.0f, lv4Colour.y * 0.4f, lv4Colour.z * 0.4f, 1.0f };
                RenderFire(lv4BoostRect, lv4FireColour, lfTimeNow);
                Vector4 lv4GlowColour = KV4_GLOW_COLOUR;
                lv4GlowColour.w = lfBaseAlpha;
                RenderQuad(lv4BoostRect, lv4GlowColour, mpGlowTextureState,
                           CgsGui::gpGuiBlendStateAdditive, lv4FullUVs);
                lpRenderBuffer->PopMask();

                // The function-local static the console keeps at RenderComponent::
                // sfTransitionMaskXoffset -- a plain zero-initialised POD nothing writes.
                static const f32 sfTransitionMaskXoffset = 0.0f;
                const f32 lfMaskX = lfChainX + sfTransitionMaskXoffset;
                const Vector4 lv4TransitionMaskRect =
                    { lfMaskX - KF_FIRE_MASK_WIDTH, 0.0f, lfMaskX, 1.0f };
                BrnGui::SetMaskRect(lpRenderBuffer, mpMaskTextureState,
                                    lv4TransitionMaskRect, lv4FullUVs);
                RenderFire(lv4BoostRect, lv4Colour, lfTimeNow);
                lpRenderBuffer->PopMask();
            }
            else
            {
                RenderFire(lv4BoostRect, lv4Colour, lfTimeNow);
            }
        }
        else
        {
            const bool lbDangerInactive = (meBoostBarStatus == E_STATUS_DANGER_BOOST_INACTIVE);
            if (lbDangerInactive)
            {
                SetChainedInactiveMask(lpRenderBuffer, lv4BackgroundRect);
                // Dim IN PLACE -- the later fireball/multiplier draws see the dimmed colour,
                // exactly as the console's register reuse does.
                lv4Colour.y *= 0.4f;
                lv4Colour.z *= 0.4f;
                lv4Colour.x = 1.0f;
                lv4Colour.w = 1.0f;
            }
            RenderFire(lv4BoostRect, lv4Colour, lfTimeNow);
            if (lbDangerInactive)
                lpRenderBuffer->PopMask();

            if (mfIsBoostingProp > 0.0f)
            {
                const f32 lfGlow = KF_BOOSTING_GLOW_INTENSITY * mfIsBoostingProp;
                const Vector4 lv4GlowColour = { lfGlow, lfGlow, lfGlow, lfBaseAlpha };
                RenderQuad(lv4BoostRect, lv4GlowColour, mpGlowTextureState,
                           CgsGui::gpGuiBlendStateAdditive, lv4FullUVs);
            }
        }

        // ---- the grow fireball (slam gain) -----------------------------------------------
        const f32 lfSlamGainElapsed = lfTimeNow - mfSlamGainStartTime;
        if (lfSlamGainElapsed <
            static_cast<f32>(mBillboardRenderer[5].GetNumFrames()) * (1.0f / 60.0f))
        {
            maBillboards.Clear();

            const f32 lfTargetX = lv4BoostRect.z - 0.5f * KF_GROW_FIREBALL_X_SIZE +
                                  KF_GROW_FIREBALL_X_OFFSET;
            const f32 lfX = lv4BoostRect.x + (lfTargetX - lv4BoostRect.x) *
                            mBoostGainInterpolator.GetCurrentValue(lfTimeNow);

            CgsGui::BillboardInfo lInfo = {};
            lInfo.mfPosX         = lfX;
            lInfo.mfPosY         = lfBarCentreY;
            lInfo.mfRotation     = 0.0f;
            lInfo.mfSizeX        = KF_GROW_FIREBALL_X_SIZE;
            lInfo.mfSizeY        = KF_GROW_FIREBALL_Y_SCALE * lfBarHeight;
            lInfo.muDiffuse      = PackBillboardDiffuse(lv4Colour);
            lInfo.miTextureFrame = static_cast<s32>(lfSlamGainElapsed * 60.0f);
            maBillboards.Append(lInfo);

            const Vector4 lv4FireballMaskRect = { lv4Rect.x, 0.0f, 1.0f, 1.0f };
            BrnGui::SetMaskRect(lpRenderBuffer, mpWhiteTextureState,
                                lv4FireballMaskRect, lv4FullUVs);
            mBillboardRenderer[5].Render(lpRenderBuffer, &maBillboards.GetItem(0u),
                                         static_cast<s32>(maBillboards.GetLength()));
            lpRenderBuffer->PopMask();
        }

        // ---- the earn-flame flicker quad -------------------------------------------------
        {
            f32 lfElapsed = lfTimeNow - mfIsEarningBoostStartTime;
            if (lfElapsed > 0.5f)
                lfElapsed = 1.0f - lfElapsed;
            f32 lfEarnFlameProp = lfElapsed * 4.0f;
            if (lfEarnFlameProp < 0.0f) lfEarnFlameProp = 0.0f;
            if (lfEarnFlameProp > 1.0f) lfEarnFlameProp = 1.0f;

            if (lfEarnFlameProp > 0.0f &&
                meBoostBarStatus != E_STATUS_DANGER_BOOST_INACTIVE &&
                meBoostBarStatus != E_STATUS_DANGER_BOOST_ACTIVE)
            {
                const f32 lfHalfHeight = 0.5f * lfBarHeight * KF_EARN_FLAME_Y_SCALE;
                const f32 lfX0 = lv4Rect.x + KF_EARN_FLAME_X_OFFSET;
                const f32 lfX1 = lfX0 + KF_EARN_FLAME_WIDTH;
                const f32 lfY0 = lfBarCentreY - lfHalfHeight;
                const f32 lfY1 = lfBarCentreY + lfHalfHeight;

                Vector4 lv4EarnColour = { 1.0f, 1.0f, 1.0f, lfBaseAlpha * lfEarnFlameProp };
                if (mRandom.RandomFloat() > 0.80000001f)
                    lv4EarnColour.w *= KF_EARN_FLAME_FLICKER_PROP;
                const u32 luColour = PackBoostColour(lv4EarnColour);

                CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
                const f32 lafX[4] = { lfX0, lfX0, lfX1, lfX1 };
                const f32 lafY[4] = { lfY0, lfY1, lfY0, lfY1 };
                const f32 lafU[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
                const f32 lafV[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
                for (s32 liVert = 0; liVert < 4; ++liVert)
                {
                    laVerts[liVert].mv2Pos.x    = lafX[liVert];
                    laVerts[liVert].mv2Pos.y    = lafY[liVert];
                    laVerts[liVert].mv2Tex0UV.x = lafU[liVert];
                    laVerts[liVert].mv2Tex0UV.y = lafV[liVert];
                    *reinterpret_cast<u32*>(&laVerts[liVert].mv4Colour) = luColour;
                }
                lpRenderBuffer->SetState(CgsGui::gpGuiBlendStateAdditive);
                lpRenderBuffer->SetState(mpEarnFlameTextureState);
                lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6),
                                       laVerts, 4);
            }
        }

        // ---- the multiplier flame window -------------------------------------------------
        if (meBoostBarStatus == E_STATUS_AGGRESSION_BOOST &&
            meBoostBarMultiplier != E_MULTIPLIER_1X)
        {
            const Vector4& lrv4UV = (meBoostBarMultiplier == E_MULTIPLIER_2X)
                                  ? KV4_MULTIPLIER_2X_IMAGE_UV
                                  : KV4_MULTIPLIER_3X_IMAGE_UV;
            const Vector4 lv4MultiplierRect =
                { lv4BackgroundRect.z - KF_BACKGROUND_TILE_WIDTH, lv4BackgroundRect.y,
                  lv4BackgroundRect.z, lv4BackgroundRect.w };
            BrnGui::SetMaskRect(lpRenderBuffer, mpMultiplierTextureState,
                                lv4MultiplierRect, lrv4UV);
            const Vector4 lv4MultiplierFireColour =
                { lv4Colour.x * 0.4f, lv4Colour.y * 0.4f,
                  lv4Colour.z * 0.4f, lv4Colour.w * 0.4f };
            RenderFire(lv4MultiplierRect, lv4MultiplierFireColour, lfTimeNow);
            lpRenderBuffer->PopMask();
        }

        lpRenderBuffer->SetProgram(0);

        // ---- the danger end glow ---------------------------------------------------------
        if (meBoostBarStatus == E_STATUS_DANGER_BOOST_INACTIVE)
        {
            SetChainedInactiveMask(lpRenderBuffer, lv4BackgroundRect);
            // The X-mirrored glow rect (x0 > x1 flips the sample) is the console's own build.
            const Vector4 lv4EndGlowRect =
                { lv4BoostRect.z, lv4BoostRect.y,
                  lv4BoostRect.z - KF_DANGER_END_GLOW_WIDTH, lv4BoostRect.w };
            const Vector4 lv4EndGlowColour =
                { 1.0f, 1.0f, 1.0f, mGuiEventBoostInfo.mfBoostAmount / lfMaxBoost };
            RenderQuad(lv4EndGlowRect, lv4EndGlowColour, mpEndGlowTextureState,
                       CgsGui::gpGuiBlendStateAdditive, lv4FullUVs);
            lpRenderBuffer->PopMask();
        }

        // ---- the background end cap ------------------------------------------------------
        {
            const f32 lfHalfHeight = 0.5f * KF_BACKGROUND_ENDCAP_YSCALE * lfBarHeight;
            const Vector4 lv4CapRect =
                { lv4Rect.x + KF_BACKGROUND_ENDCAP_XOFFSET,
                  lfBarCentreY - lfHalfHeight,
                  lv4Rect.x + KF_BACKGROUND_ENDCAP_XOFFSET + KF_BACKGROUND_ENDCAP_WIDTH,
                  lfBarCentreY + lfHalfHeight };
            const Vector4 lv4CapColour = { 1.0f, 1.0f, 1.0f, lfBaseAlpha };
            RenderQuad(lv4CapRect, lv4CapColour, mpBackgroundEndCapTextureState,
                       CgsGui::gpGuiBlendStateStandard, lv4FullUVs);
        }
    }

    // ---- epilogue -----------------------------------------------------------------------
    if (mbShowDebugScreen)
        ShowDebugScreen();
    lpRenderBuffer->SetState(CgsGui::gpGuiBlendStateStandard);
    lpRenderBuffer->EndRendering();

    if (miBoostBarPM >= 0)
        CgsDev::PerfMonCpu::StopMonitor(miBoostBarPM);
}
}
