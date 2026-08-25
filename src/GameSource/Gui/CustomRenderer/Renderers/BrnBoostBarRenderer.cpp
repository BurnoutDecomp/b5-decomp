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

#include <cstring>   // memset (the shatter-lattice clears)

// BrnGui::BoostBarRenderer -- the in-game boost gauge (see the header banner for the source
// map). This TU replaces the boot-trace minimal slice wholesale: the DWARF class landed in the
// header, and every body below is reconstructed from the X360 ARTIST asm with the PS3 DecFIGS
// export (which carries all the methods the X360 set lacks) as the shape/naming source.
//
// TODO(boost-render-half) -- CAMPAIGN IN FLIGHT (2026-08-24): the lifecycle/state half below
// (ctor..Update, InitResources, RenderQuad, the shard math helpers) is reconstructed and
// gate-clean; the RENDER half (RenderComponent @0x82466638, RenderFire @0x82452AD8,
// RenderBillboardBar @0x82453318, RenderShatteredBar @0x82460630, SetBackground @0x8245B040,
// SetChainedInactiveMask @0x824536A8, CalculateShardVertices @0x8244B248,
// CalculateBoostShardTransformation, ShowDebugScreen @0x82461250 + the four RenderDebugFire
// bodies) is still to land, together with the ImRenderBuffer SetState(TextureState/BlendState)
// instantiations and the fpu Vector4Template<float> ctor instantiation. Until then the TU is
// UNMOUNTED from the exe build (see build_game_exe.bat) and the manager's slot 4 stays null.
// The full ground-truth workpack (X360+PS3 pseudocode/asm per function, the TOC value dump,
// the X360 BSS constant map) lives in the session scratchpad's boostbar/ directory.
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
// RenderQuad -- faithful port of X360 @0x8245AE30 (PS3 0x425988). One textured quad into the
// set's 2D command buffer: clamp the colour to [0,1], scale to bytes and pack it, build the
// four tristrip corners from the rect {x0,y0,x1,y1} and the UV set {u0,v0,u1,v1}, bind the
// texture + blend states, publish the shared screen transform, submit as a 4-vertex strip
// (primitive 6).
// [FLAG PC fold] the console's shared transform (&unk_83011090) is the screen->NDC block its
// GPU consumed; the PC Apt dispatch consumes logical-screen-pixel transforms instead, so the
// screen-space identity (colour scale 255) is published -- the same fold the ticker's
// RenderComponent documents. The rect/UVs are 0..1 screen proportions on the console; the PC
// dispatch expects logical pixels, so the quad corners scale by the 1280x720 logical screen.
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
}

void BoostBarRenderer::RenderQuad(const Vector4& lv4Rect, const Vector4& lv4UV,
                                  const renderengine::TextureState* lpTextureState,
                                  const renderengine::BlendState* lpBlendState,
                                  Vector4 lv4Colour)
{
    if (mpImRenderers == 0)
        return;
    CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
        *reinterpret_cast<CgsGui::AptIm2dRenderBuffer* const*>(mpImRenderers);
    if (lpAptBuffer == 0)
        return;
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd =
        lpAptBuffer->mCommandBuffer;

    // The packed vertex colour (RGBA bytes from the clamped 0..1 lanes).
    const u32 luColour =
        (static_cast<u32>(BoostColourLaneToByte(lv4Colour.w)) << 24) |
        (static_cast<u32>(BoostColourLaneToByte(lv4Colour.z)) << 16) |
        (static_cast<u32>(BoostColourLaneToByte(lv4Colour.y)) << 8)  |
        (static_cast<u32>(BoostColourLaneToByte(lv4Colour.x)));

    // The four tristrip corners: (x0,y0)(x0,y1)(x1,y0)(x1,y1) with the matching UV corners
    // (the console's vertex build order). Console rect/UVs are screen proportions; scale to
    // the PC dispatch's logical-screen pixels (see the FLAG above).
    CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
    const f32 lafX[4] = { lv4Rect.x, lv4Rect.x, lv4Rect.z, lv4Rect.z };
    const f32 lafY[4] = { lv4Rect.y, lv4Rect.w, lv4Rect.y, lv4Rect.w };
    const f32 lafU[4] = { lv4UV.x,   lv4UV.x,   lv4UV.z,   lv4UV.z   };
    const f32 lafV[4] = { lv4UV.y,   lv4UV.w,   lv4UV.y,   lv4UV.w   };
    for (s32 liVert = 0; liVert < 4; ++liVert)
    {
        laVerts[liVert].mv2Pos.x    = lafX[liVert] * 1280.0f;
        laVerts[liVert].mv2Pos.y    = lafY[liVert] * 720.0f;
        laVerts[liVert].mv2Tex0UV.x = lafU[liVert];
        laVerts[liVert].mv2Tex0UV.y = lafV[liVert];
        *reinterpret_cast<u32*>(&laVerts[liVert].mv4Colour) = luColour;
    }

    lrCmd.SetState(lpTextureState);
    lrCmd.SetState(lpBlendState);

    // The shared screen transform (console &unk_83011090 -> the PC screen-space identity fold,
    // colour scale 255 -- the ticker's documented convention).
    {
        CgsGraphics::Im2dTransform lTransform = {};
        lTransform.mRightUp.x     = 1.0f;
        lTransform.mRightUp.w     = 1.0f;
        lTransform.mColourScale.x = 255.0f;
        lTransform.mColourScale.y = 255.0f;
        lTransform.mColourScale.z = 255.0f;
        lTransform.mColourScale.w = 255.0f;
        lrCmd.SetTransform(lTransform);
    }

    lrCmd.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
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
}
