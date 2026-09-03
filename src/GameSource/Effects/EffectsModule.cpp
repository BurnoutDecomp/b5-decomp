#include "GameSource/Effects/EffectsModule.h"

#include "GameSource/Effects/ParticleEffectHelper.h"                               // ParticleEffectHelper / RaceCarParticleEffectHelper
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_InputBuffer.h"            // EffectsIO::InputBuffer
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h"           // EffectsIO::OutputBuffer
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"    // EffectsIO::DispatchInputBuffer
#include "GameSource/Effects/Particles/ParticleModuleIO.h"                         // ParticleIO::PrepareOutputBuffer / DispatchInputBuffer
#include "GameSource/Effects/Particles/BrnParticleDescription.h"                   // BrnParticle::ParticleDescription::HashString
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                        // CgsModule::IOBufferStack
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"            // CgsResource::Events::AcquireResource{Request,Response}
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"                 // BrnResource::GameDataIO::AllocatorList
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::RaceCarState / WheelLite
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // DeformationOutputInterface (the locator table)
#include "GameSource/GameState/BrnGameActions.h"                                   // the game-action records
#include "GameSource/Director/Camera/Camera.h"                                     // BrnDirector::Camera::Camera (+ CameraState)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"           // CgsSystem::TimerStatusInterface / TimerStatus
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                           // CgsSystem::Time
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"                 // BrnSound::Module::Io::PopEffectsMessage
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"   // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                       // CgsSceneManager::EntityId
#include "GameSource/AttribSys/Generated/classes/surface.h"                        // Attrib::Gen::surface
#include "GameSource/AttribSys/Generated/classes/visualfxsurface.h"                // Attrib::Gen::visualfxsurface
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"                  // Attrib::FindCollection
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "GameSource/Graphics/PostFx/BrnPostFx.h"                                  // msPostFx (the colour-cube seed)
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"                          // BrnGame::DispatchThreadInputBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                         // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Development/BrnDiagFilmLatch.h"                   // [diag] BRN_FRAME_DUMP_ARM=skid
#include "rw/math/vpu/vector3_operation.h"                                         // rw::math::vpu::{operator-, Dot}

#include <cmath>    // std::fabs
#include <cstdio>
#include <cstdlib>   // getenv (the [skid] probe gate)   // std::snprintf

// ============================================================================
// GameSource/Effects/EffectsModule.cpp
//
// BrnEffects::EffectsModule -- reconstructed from the X360 ARTIST build:
//
//   EffectsModule()                 @0x827E35E0   Construct                 @0x8228FE98
//   Prepare                         @0x8229E690   PrepareResources          @0x8229D8A8
//   GetNextAcquireResourceResponse  @0x8227F098   PostWorldPreparePrepare   @0x822902F0
//   Release                         @0x8227FCA8   Destruct                  @0x8227FD78
//   Update                          @0x8229EC28   ProcessActiveRaceCars     @0x8229EB30
//   UpdateActiveRaceCars            @0x8229DB30   HandleWheels              @0x82296C80
//   HandleJumpAndLandingEffects     @0x82288068   HandlePlayerTriangleCache @0x82296EA0
//   HandleGameActions               @0x82296FD8   HandleConvoySlipStream    @0x822926C8
//   GetPlayerRaceCarState           @0x822803C0   GenerateDispatchLists     @0x82296668
//   RestartEffects                  @0x822793E0   LoadNativeParticleParams  @0x82290510 (partial)
//
// 2026-09-02 (tyre-mark wave). The module used to be an opaque byte body with two bodies;
// it is now the DWARF's member set by name and the lifecycle above. THE PATH THIS WAVE
// LANDS is the tyre mark: Update -> ProcessActiveRaceCars -> UpdateActiveRaceCars ->
// ActiveRaceCarData::Tick -> HandleWheels -> TrailSystem::AddTrailSegment, with the
// prepare ladder (PrepareResources, ParticleModule::Prepare / LoadFXBundle) that makes the
// trail system READY and PostWorldPreparePrepare that gives every surface its skid colours.
//
// ⚠ NOT RECONSTRUCTED ON THIS BUILD, EACH ONE LOUD (logs once when first reached, then
// returns) -- the arms OFF the tyre-mark path: the crash sparks / debris / glass / crashing
// trail / showtime bounce / junkyard editor / QA tests, the post-fx effects frames
// (GenerateRenderRequests -- the renderer's base-frame bring-up producer still stands in
// for it), the native simple-particle parameter push (LoadNativeParticleParams' consumer
// BrnSimpleParticleArray::UpdateParams has no body), the prop-locator VFX and the spark
// parameter copies into the particle module's (placeholder) spark arrays. None of them is
// a trap: a CGS_ASSERT in HandleCrashingTrail or JunkyardVfxStart would kill every crash
// and every junkyard boot on the shared box; none is silent either -- each writes ONE
// `[effects] NOT RECONSTRUCTED: ...` line to BrnGame.log so a run that needed the arm says so.
// ============================================================================

namespace BrnEffects
{
namespace
{
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface RCEntityActiveRaceCarOutputInterface;
    typedef ::EActiveRaceCarIndex                                                EActiveRaceCarIndex;
    typedef BrnPhysics::Vehicle::RaceCarState                                    RaceCarState;
    typedef BrnPhysics::Vehicle::WheelLite                                       WheelLite;

    // ---- the X360 data cells this TU owns -------------------------------------------
    EffectsModule* gpEffectsModule = 0;          // off_82FAB594 (Construct stores `this`)
    bool           sbRestartEffects = false;     // byte_82FAB694 (RestartEffects raises, HandleQADebugTests consumes)
    s32            siEffectsSuspendState = 0;    // dword_82FAD294 (Update's suspend / resume ladder)
    s32            siEffectsSuspendFrames = 0;   // dword_82FAD290 (the 5-frame wait in state 4)
    f32            sfLastUpdateTime = 0.0f;      // flt_82FAD28C (the last non-stalled update time)

    // flt_82001CC0 -- the 0.0f the timer branch loads when the sim timer is not running.
    const f32 KF_ZERO_TIME_STEP = 0.0f;
    // HandleWheels @0x82296C80: the |dot(pos - prevPos, normal)| ceiling (v29[0] = 0.02).
    const f32 KF_TRAIL_NORMAL_DRIFT_MAX = 0.02f;
    // PrepareResources @0x8229D8A8: the five default tint weights (flt_8200DD40 == 0.2f) and
    // the default blend count (`li r9, 5`).
    const f32 KF_DEFAULT_TINT_FACTOR = 0.2f;
    const int KI_DEFAULT_TINT_BLEND_NUMBER = 5;

    // The convoy slip-stream effect name (the pointer baked into HandleConvoySlipStream).
    const char* const KAC_SLIPSTREAM_EFFECT = "gamedb://Instances/Effects/SlipStream/SlipStream";

    // The post-fx vault + colour-cube dictionary PrepareResources loads / acquires.
    const char* const KAC_POSTFX_VAULT_BUNDLE      = "PostFx/postfxvault.bin";
    const char* const KAC_COLOUR_CUBE_DICT_BUNDLE  = "PostFx/colourcubedictionary.bin";
    const char* const KAC_POSTFX_VAULT_RESOURCE    = "postfxvault";
    // off_82CDB414 -- the default colour cube's gamedb path (the same literal
    // BrnEnvironmentManager::Prepare @0x827D4B3C names, KAC_DEFAULT_COLOUR_CUBE_RESOURCE).
    const char* const KAC_DEFAULT_COLOUR_CUBE_RESOURCE =
        "gamedb://burnout5/Playground/PostFx/ColourCubeDictionary/rgb_colourcube.tga.ImageFile?ID=217407";
    // The pool ids the two LoadBundle / AcquireResource pairs carry (`li r6, 7` / `li r6, 0xA`).
    const s32 KI_POSTFX_VAULT_POOL      = 7;
    const s32 KI_COLOUR_CUBE_DICT_POOL  = 10;
    // The receiver-queue event ids the ladders wait on.
    const s32 KI_EVENT_ACQUIRE_RESOURCE_RESPONSE = 4;

    // The visualfxsurface attribute layout HandleWheels / PostWorldPreparePrepare read
    // (the same offsets WheelStateMachine::Update reads its layers at; the two colours lead).
    const u32 KU_VFX_SKID_MARK_START_COLOUR = 0x00;   // Vector4 (lvx128 v1, r0, r11)
    const u32 KU_VFX_SKID_MARK_END_COLOUR   = 0x10;   // Vector4 (lvx128 v2, r11, r28 == +16)
    // The world's surfacelist COLLECTION key, as a string: sub_82C4A1F8 stores
    // `Attrib::StringToKey("340654")` into qword_82FAB7A8, which PostWorldPreparePrepare
    // passes to FindCollectionWithDefault. Spelled as the string the console spells, not as
    // a baked hash, so it reads the way every other collection key in this tree does
    // (BrnDirectorResourceManager.cpp:492 `Attrib::StringToKey("430819")`).
    const char* const KAC_WORLD_SURFACELIST_COLLECTION = "340654";
    const u32 KU_VFX_SKID_MARK_THRESHOLD    = 0x48;   // f32   (`*(v31 + 72)`)
    const u32 KU_VFX_SKID_MARKS_ENABLED     = 0x4E;   // bool  (`*(v31 + 78)`)
    const u32 KU_VFX_SKID_MARK_TYPE_ID      = 0x58;   // s16   (`*(v31 + 88)`)
    // ⭐⭐ RENAMED 2026-09-03 from KU_VFX_SUBCOLLECTION_OFFSET, because the old name WAS the
    // bug. +0x10 in a surface's layout block is an Attrib::RefSpec (the ref to that surface's
    // visualfxsurface collection), not a collection: the console hands `surfaceLayout + 16`
    // to Attrib::Gen::visualfxsurface @0x8227FC00, whose Instance ctor is sub_8280A248 --
    // `bl Attrib__RefSpec__GetCollection` on the argument. Reading it as an
    // Attrib::Collection* dereferenced +0x28/+0x30 past the end of a 24-byte record and made
    // GetClass() load a garbage class pointer. See visualfxsurface.h for the full asm.
    const u32 KU_VFX_SURFACE_REF_OFFSET     = 16;     // the visualfxsurface RefSpec inside a surface's layout
    const u32 KU_SURFACE_ID_SHIFT           = 4;      // (tag >> 4) & 0x3F
    const u32 KU_SURFACE_ID_MASK            = 0x3F;
    const u32 KU_SURFACE_REFSPEC_SIZE       = 24;     // Attrib::DefaultDataArea(24) fallback

    // The surface layout's visualfxsurface reference: `surfaceInstance.mpAttributeData + 16`
    // read as the Attrib::RefSpec it is. One spelling for all four call sites.
    const Attrib::RefSpec& VfxSurfaceRef(const void* lpSurfaceLayout)
    {
        return *reinterpret_cast<const Attrib::RefSpec*>(
            reinterpret_cast<const u8*>(lpSurfaceLayout) + KU_VFX_SURFACE_REF_OFFSET);
    }

    // One line, once, for an arm this build does not carry. Never an assert: see the banner.
    void LogNotReconstructed(bool& lrbLogged, const char* lpcWhat)
    {
        if (lrbLogged)
            return;
        lrbLogged = true;
        char lacMsg[256];
        std::snprintf(lacMsg, sizeof(lacMsg), "[effects] NOT RECONSTRUCTED: %s\n", lpcWhat);
        CgsDev::Log::WriteToLog(lacMsg);
    }

    // =========================================================================================
    // [skid] THE HANDLE-WHEELS GATE PROBE  (BRN_SKID_PROBE=1)
    //
    // Prints BOTH SIDES of every compare HandleWheels makes, per wheel, and -- the part that
    // matters -- an EDGE line the frame a mark starts and the frame it stops. Off unless the
    // env var is set, so it costs a single cached read otherwise.
    //
    // Fields, in the order the gate tests them:
    //   grnd/trac/att   the three WheelLite bits (+40 mbIsOnGround, +97 mbHasTraction, +96 mbAttached)
    //   drift           dot(contactPos - prevPos, contactNormal) vs the 0.02 m limit -- the wheel
    //                   leaving its own contact plane ends the trail
    //   surf            (mCollisionTag.muValue >> KU_SURFACE_ID_SHIFT) & 0x3F, the raw operand
    //   en/thr/skid     visualfxsurface SkidMarksEnabled, SkidMarkThreshold, and WheelLite::
    //                   mfSkidFactor (+80) -- `skid > thr` is THE gate
    //   ready           TrailSystem::mbIsReady, which LoadFXBundle raises on the "fxskid" reply;
    //                   Render early-outs while it is false, so a mark can be LAID and not drawn
    //   t               ParticleModule::mRenderData.mfCurrentTime (+0x8E08), AddTrailSegment's
    //                   lrCurrentTime argument
    // =========================================================================================
    bool SkidProbeEnabled()
    {
        static int siEnabled = -1;
        if (siEnabled < 0)
        {
            const char* lpcValue = std::getenv("BRN_SKID_PROBE");
            siEnabled = (lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0') ? 1 : 0;
        }
        return siEnabled != 0;
    }

    // [skid-gate] WHICH EXIT OF EffectsModule::Update FIRED -- one line per DISTINCT reason,
    // once each. Run 7 measured zero [skid] lines with the flow in DRIVING and the trail
    // system READY, which says only that the wheel loop was never reached; it does not say
    // WHERE. Every early return in Update now names itself, so one run turns a guess into a
    // measurement. DELETE with the tyre-mark bring-up.
    void SkidGateExit(const char* lpcWhere)
    {
        if (!SkidProbeEnabled())
            return;
        static const char* sapcSeen[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (int li = 0; li < 8; ++li)
        {
            if (sapcSeen[li] == lpcWhere)
                return;
            if (sapcSeen[li] == 0)
            {
                sapcSeen[li] = lpcWhere;
                char lacMsg[224];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[skid-gate] EffectsModule::Update RETURNED EARLY at: %s\n", lpcWhere);
                CgsDev::Log::WriteToLog(lacMsg);
                return;
            }
        }
    }


    u32 gauSkidProbeFrame = 0;

    inline f32 ReadF32(const void* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpBase) + luOffset);
    }
    inline s16 ReadS16(const void* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const s16*>(reinterpret_cast<const u8*>(lpBase) + luOffset);
    }
    inline bool ReadBool(const void* lpBase, u32 luOffset)
    {
        return reinterpret_cast<const u8*>(lpBase)[luOffset] != 0;
    }
    inline Vector4 ReadVector4(const void* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const Vector4*>(reinterpret_cast<const u8*>(lpBase) + luOffset);
    }

    // The X360 record for game action 16 (E_ACTION_INPROGRESS_STUNT). The FIGS DWARF's
    // InProgressStuntAction ends at +0x18 (six words); the ARTIST record HandleGameActions
    // @0x82296FD8 reads is wider: `lfs f1, 0x18(r28)` (the slip-stream blend passed to
    // HandleConvoySlipStream) and `lwz r11, 0x1C(r28)` (the car-in-front index, -1 == none).
    // FLAG: the two tail names are inferred from their consumers; the six head names are the
    // DWARF's.
    struct InProgressStuntActionX360
    {
        u32 muStuntActionInProgress;        // +0x00 (DWARF :808)
        f32 mfInProgressBarrelRollAngle;    // +0x04
        f32 mfInProgressAirSpinAngle;       // +0x08
        f32 mfInProgressHandbreakTurnAngle; // +0x0C
        f32 mfInProgressDriftTime;          // +0x10
        f32 mfInProgressDriftDistance;      // +0x14
        f32 mfSlipStreamBlend;              // +0x18 (ARTIST-only)
        s32 miCarInFrontIndex;              // +0x1C (ARTIST-only)
    };
    // The bit of muStuntActionInProgress / muStuntActionComplete the convoy arms test
    // (`cmplwi cr6, r11, 0x80` / `cmplwi cr6, r11, 0x400` after `and`).
    const u32 KU_STUNT_IN_PROGRESS_SLIPSTREAM = 0x80;
    const u32 KU_STUNT_COMPLETE_SLIPSTREAM    = 0x400;
}

// =============================================================================
// EffectsModule::EffectsModule  @0x827E35E0
//   The C++ constructor: on the console every sub-object is placement-new'd /
//   vector-constructed at its byte offset (the particle module, the debug component
//   and its defaults, the 8 ActiveRaceCarData with their 4+4 machines, the attrib
//   instances, the surface list); the only scalar stores of its own are the debug
//   component's default values, which EffectsDebugComponent::Construct now owns.
//   Every member constructs itself here.
// =============================================================================
EffectsModule::EffectsModule()
    : mEffectInstanceHandle(0xFFFFFFFFu)
    , mQAEffectHandle(0xFFFFFFFFu)
    , liEffectInstanceIndex(0)
    , mResetAttribs(false)
    , mePrepareStage(E_PREPARESTAGE_START)
    , meReleaseStage(E_RELEASESTAGE_DONE)
    , meResourceStage(E_RESOURCESTAGE_START)
    , mpHeapMalloc(0)
    , meCurrentGameMode(BrnGameState::GameStateModuleIO::E_MODE_NONE)
    , mbEventIntroActive(false)
    , muNextShowtimeBounceEffect(0)
    , mfLastShowtimeBounceEffectTime(0.0f)
    , muSlipStreamEffectHandle(0xFFFFFFFFu)
    , mbUpdateRan2F5A8(false)
    , mbUpdateRan2F5A9(false)
{
    for (u32 lu = 0; lu < KU_MAX_SHOWTIME_BOUNCE_EFFECTS; ++lu)
        maShowtimeBounceEffectHandles[lu] = 0xFFFFFFFFu;
    for (u32 lu = 0; lu < KU_MAX_JUNKYARD_VFX; ++lu)
        maJunkyardEffectHandles[lu] = 0xFFFFFFFFu;
}

// =============================================================================
// Construct  @0x8228FE98  (DWARF EffectsModule.cpp:335)
// =============================================================================
void EffectsModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    meReleaseStage  = E_RELEASESTAGE_DONE;       // +0x23C = 2
    mePrepareStage  = E_PREPARESTAGE_START;      // +0x238 = 0
    gpEffectsModule = this;                      // off_82FAB594
    meResourceStage = E_RESOURCESTAGE_START;     // +0x240 = 0

    // The receiver queue: capacity 2048, align 16, buffer = its own storage, then Clear.
    mReceiverQueue.Construct();
    mReceiverQueue.Clear();

    // The embedded particle module's Construct (vtable slot 0 through +0xA80).
    mParticleModule.Construct();

    // The pseudo-random generator: the inlined LCG priming (seed 0x1AD0891B, buffer[0] =
    // 1.0f, seven AddRandomFloatToBuffer steps) that CgsNumeric::Random::Construct spells out.
    mRandom.Construct();

    mDebugComponent.Construct(this);
    mResetAttribs = false;

    for (u32 lu = 0; lu < KU_NUM_ACTIVE_RACE_CARS; ++lu)
    {
        maActiveRaceCarData[lu].Construct();
        mafTimeUntilNextDebrisBurst[lu] = 0.0f;
        mafTimeUntilNextSparksBurst[lu] = 0.0f;
        for (u32 luAcc = 0; luAcc < 6; ++luAcc)
            mafCrashingTrailAccumulators[lu][luAcc] = 0.0f;
    }

    meCurrentGameMode  = BrnGameState::GameStateModuleIO::E_MODE_NONE;   // +0x2D340 = -1
    mbEventIntroActive = false;                                          // +0x2D344 = 0

    mCrashTriangleCache.Construct();
    mGlassSmashManager.Construct(&mParticleModule);   // the inlined 8-slot init (+0x2F280..)

    mEffectInstanceHandle = 0xFFFFFFFFu;              // +0x228
    mQAEffectHandle       = 0xFFFFFFFFu;              // +0x22C
    for (u32 lu = 0; lu < KU_MAX_SHOWTIME_BOUNCE_EFFECTS; ++lu)
        maShowtimeBounceEffectHandles[lu] = 0xFFFFFFFFu;   // +0x2F510..
    muNextShowtimeBounceEffect     = 0;               // +0x2F51C
    mfLastShowtimeBounceEffectTime = 0.0f;            // +0x2F520
    for (u32 lu = 0; lu < KU_MAX_JUNKYARD_VFX; ++lu)
        maJunkyardEffectHandles[lu] = 0xFFFFFFFFu;    // +0x2F524..
    muSlipStreamEffectHandle = 0xFFFFFFFFu;           // +0x2F54C

    mEffectsSerialiser.Construct();                   // +0x2F550 (id 8, "Effects", 4816 / 4784)

    mbIsNewModule = true;                             // `this->field_4 = 1`
}

// =============================================================================
// GetNextAcquireResourceResponse  @0x8227F098
//   Iterate the module's resource-acquire reply queue: with no previous response,
//   return the FIRST queued response's payload (NULL when the queue is empty);
//   otherwise the one that follows lpPrevious (NULL at the end).
// =============================================================================
const CgsResource::Events::AcquireResourceResponse*
EffectsModule::GetNextAcquireResourceResponse(const CgsResource::Events::AcquireResourceResponse* lpPrevious)
{
    if (lpPrevious != 0)
    {
        const CgsModule::Event* lpNext = 0;
        s32 liSize = 0;
        mReceiverQueue.GetNextEvent(reinterpret_cast<const CgsModule::Event*>(lpPrevious), &lpNext, &liSize);
        return reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpNext);
    }
    if (mReceiverQueue.GetCount() <= 0)
        return 0;
    const CgsModule::Event* lpFirst = 0;
    s32 liSize = 0;
    mReceiverQueue.GetFirstEvent(&lpFirst, &liSize);
    return reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpFirst);
}

// =============================================================================
// PrepareResources  @0x8229D8A8  (DWARF :632)
//   The post-fx vault / default colour-cube acquire ladder, driven under the effects
//   output buffer's write lock. Returns true only at stage DONE.
//
//   stage 0  LoadBundle(pool 7, "PostFx/postfxvault.bin") + LoadBundle(pool 10,
//            "PostFx/colourcubedictionary.bin") into the output's resource request
//            queue (our receiver queue is the reply target) -> stage 1
//   stage 1  wait for BOTH load replies (count > 1); then queue the two
//            AcquireResourceRequests (event id 1 / pool 7 / id = hash|7<<32,
//            event id 2 / pool 10 / id = hash|10<<32), Clear -> stage 2
//   stage 2  wait for both acquire replies; the FIRST reply's handle is the schema /
//            vault handle (mSchemaResourceHandle) -> RegisterVault(vault, event 1,
//            RESIDENT); the SECOND reply's main-memory pointer is the default colour
//            cube -> the post-fx tint seed (5 cubes, weight 0.2 each, blend number 5);
//            Clear -> stage 3
//   stage 3  wait for the RegisterVault reply (count > 0), Clear -> stage 4
//   stage 4  done.
// =============================================================================
bool EffectsModule::PrepareResources(EffectsIO::OutputBuffer* lpOutputBuffer)
{
    typedef CgsResource::Events::AcquireResourceResponse AcquireResponse;

    lpOutputBuffer->LockForWrite();
    bool lbDone = false;

    switch (meResourceStage)
    {
    case E_RESOURCESTAGE_START:
        lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
            &mReceiverQueue, 1, KI_POSTFX_VAULT_POOL, KAC_POSTFX_VAULT_BUNDLE, false);
        lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
            &mReceiverQueue, 2, KI_COLOUR_CUBE_DICT_POOL, KAC_COLOUR_CUBE_DICT_BUNDLE, false);
        meResourceStage = E_RESOURCESTAGE_LOADING_VAULT;
        // fall through
    case E_RESOURCESTAGE_LOADING_VAULT:
        if (mReceiverQueue.GetCount() > 1)
        {
            // The two acquire records. The X360 builds each one inline; that inline
            // expansion IS RequestInterface<N>::AcquireResource (mpUser@0, miEventId@4,
            // miPoolId@8, mResourceId@0x10 = ID::HashString(name), AddEvent type 4) --
            // reuse the committed builder rather than re-forking the record here.
            // ⚠️ The id is the PLAIN zero-extended 32-bit CRC. An earlier draft of this TU
            // wrote `hash | (poolId << 32)`; that is the Hex-Rays fusion artifact already
            // diagnosed project-wide (the interleaved `li r10,<poolId>` is the SEPARATE
            // miPoolId store at +0x08, not part of the `std` of the hash). Pool::
            // FindResource compares the whole 64-bit value, so a tagged id matches NOTHING
            // and the request parks on the receiver queue for ever.
            lpOutputBuffer->GetResourceRequestInterface()->AcquireResource(
                &mReceiverQueue, 1, KI_POSTFX_VAULT_POOL, KAC_POSTFX_VAULT_RESOURCE);
            lpOutputBuffer->GetResourceRequestInterface()->AcquireResource(
                &mReceiverQueue, 2, KI_COLOUR_CUBE_DICT_POOL, KAC_DEFAULT_COLOUR_CUBE_RESOURCE);

            mReceiverQueue.Clear();
            meResourceStage = E_RESOURCESTAGE_ACQUIRING_VAULT;
        }
        break;
    case E_RESOURCESTAGE_ACQUIRING_VAULT:
        if (mReceiverQueue.GetCount() > 1)
        {
            const AcquireResponse* lpVaultReply = GetNextAcquireResourceResponse(0);
            mSchemaResourceHandle.mpResourceMemory = lpVaultReply->mpResourceMemory;
            mSchemaResourceHandle.mpSourceEntry    = lpVaultReply->mpSourceEntry;

            // ⚠ FLAG PC null-tolerance (2026-09-02, tyre-mark wave -- MEASURED, not defensive
            // dressing). On the console this reply always carries a resource: the vault bundle
            // loads. On this build it does not --
            //     [bundle] 'PostFx/postfxvault.bin' via async-FS (5504 bytes)
            //     [stream] LoadBundle 'PostFx/postfxvault.bin' -> pool 7: -1 resources
            // -- BundleLoader::LoadBundle REJECTS that bundle (negative == failed), so the
            // acquire reply comes back with a null resource. Handing that null on to
            // RegisterVault faults inside CgsAttribSys::VaultArray::GetFreeSlotIndex ->
            // ResourceHandle::GetResourceId (measured: an access violation on the first
            // effects prepare that got this far). The vault is the POST-FX schema and has
            // nothing to do with the trail system, so the ladder says so once and walks on
            // rather than taking the whole boot down with it.
            // DELETE-WHEN 'PostFx/postfxvault.bin' loads: this is an ASSET/LOADER defect, not
            // an effects one, and the guard is here only so it stops blocking everything
            // behind it.
            const bool lbVaultResourceValid = (lpVaultReply->mpResourceMemory != 0);
            if (lbVaultResourceValid)
            {
                CgsResource::ResourceHandle lVaultHandle;
                lVaultHandle.mpResourceMemory = lpVaultReply->mpResourceMemory;
                lVaultHandle.mpSourceEntry    = lpVaultReply->mpSourceEntry;
                lpOutputBuffer->GetVaultRequestInterface()->RegisterVault(
                    &mReceiverQueue, lVaultHandle, 1, CgsAttribSys::AttribSysIO::E_VAULT_TYPE_RESIDENT);
            }
            else
            {
                static bool sbLogged = false;
                LogNotReconstructed(sbLogged,
                    "EffectsModule::PrepareResources' RegisterVault -- SKIPPED because "
                    "'PostFx/postfxvault.bin' failed to load (the loader logs 'pool 7: -1 "
                    "resources'), so the acquire reply carries a null resource. ASSET/LOADER "
                    "defect, not an effects one");
            }

            // The second reply: its main-memory pointer IS the default colour cube
            // (`**(reply + 24)` -- the resource's main-memory lane). Same tolerance, same
            // reason: a failed dictionary load double-dereferences a null here.
            const AcquireResponse* lpCubeReply = GetNextAcquireResourceResponse(lpVaultReply);
            if (lpCubeReply != 0 && lpCubeReply->mpResourceMemory != 0)
            {
                rw::graphics::postfx::ColourCube* lpDefaultCube =
                    *reinterpret_cast<rw::graphics::postfx::ColourCube* const*>(lpCubeReply->mpResourceMemory);
                // dword_82FAF6E8 = 5; flt_82FAF6D0..E0 = 0.2f; dword_82FAF6BC..CC = the cube.
                msPostFx.SetTintBlendNumber(KI_DEFAULT_TINT_BLEND_NUMBER);
                for (int li = 0; li < KI_DEFAULT_TINT_BLEND_NUMBER; ++li)
                {
                    msPostFx.SetTintBlendFactor(li, KF_DEFAULT_TINT_FACTOR);
                    msPostFx.SetColourCube(li, lpDefaultCube);
                }
            }
            else
            {
                static bool sbLogged = false;
                LogNotReconstructed(sbLogged,
                    "EffectsModule::PrepareResources' default colour-cube seed -- SKIPPED "
                    "(the colourcubedictionary acquire reply carries no resource)");
            }

            mReceiverQueue.Clear();
            // If RegisterVault was skipped there is no reply to wait for, so the next stage
            // would park for ever -- go straight to DONE in that case.
            meResourceStage = lbVaultResourceValid ? E_RESOURCESTAGE_REGISTERING_VAULT
                                                   : E_RESOURCESTAGE_DONE;
        }
        break;
    case E_RESOURCESTAGE_REGISTERING_VAULT:
        if (mReceiverQueue.GetCount() > 0)
        {
            mReceiverQueue.Clear();
            meResourceStage = E_RESOURCESTAGE_DONE;
        }
        break;
    case E_RESOURCESTAGE_DONE:
        lbDone = true;
        break;
    default:
        break;
    }

    lpOutputBuffer->UnlockForWrite();
    return lbDone;
}

// =============================================================================
// Prepare  @0x8229E690  (DWARF :415)
//   The five-stage prepare ladder. Every "still preparing" exit copies the particle
//   module's staged resource requests into the effects OUTPUT buffer's resource request
//   interface (SetResourceRequestInterface), which the loading spine
//   (LoadingScriptedState::LoadEffectsModule @0x823E7820) forwards into the GameData pump.
// =============================================================================
bool EffectsModule::Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                            CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                            EffectsIO::OutputBuffer* lpOutputBuffer)
{
    typedef BrnParticle::ParticleIO::PrepareOutputBuffer ParticlePrepareOutput;

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        mePrepareStage = E_PREPARESTAGE_START;
        mReceiverQueue.Clear();
        mDebugComponent.Register();
        // fall through
    case E_PREPARESTAGE_MANAGER:
    {
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        if (!CgsModule::ModuleSingleBuffered::Prepare())
            return false;

        // The "Particles" prepare output (the CgsModuleIOHelper create/destroy pair): the
        // particle module's Prepare gets it beside the allocator list; if it is still
        // preparing, its staged requests ride out through our output buffer.
        ParticlePrepareOutput* lpParticleOutput = 0;
        const bool lbCreated = lpUpdateOutputBufferStack->CreateIOBuffer(&lpParticleOutput, "Particles");
        CGS_ASSERT(lbCreated, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");   // CgsModuleIOHelper.h:52
        (void)lbCreated;

        // ⚠ TWO arguments (corrected 2026-09-02): the console's own call site is
        // `mr r4,r29; mr r5,r30; lwz r11,0x40(vtbl); bctrl` @0x8229E73C -- r5 IS the
        // "Particles" prepare output buffer, and the FIGS DWARF (:422) declares the pair.
        const bool lbParticlePrepared = mParticleModule.Prepare(lpAllocatorList, lpParticleOutput);
        if (!lbParticlePrepared)
        {
            lpOutputBuffer->LockForWrite();
            lpOutputBuffer->SetResourceRequestInterface(lpParticleOutput->GetResourceRequestInterface());
            lpOutputBuffer->UnlockForWrite();
            const bool lbDestroyed = lpUpdateOutputBufferStack->DestroyIOBuffer(&lpParticleOutput);
            CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");   // CgsModuleIOHelper.h:57
            (void)lbDestroyed;
            return false;
        }
        const bool lbDestroyed = lpUpdateOutputBufferStack->DestroyIOBuffer(&lpParticleOutput);
        CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");
        (void)lbDestroyed;
    }
        // fall through
    case E_PREPARESTAGE_RESOURCES:
    {
        mePrepareStage = E_PREPARESTAGE_RESOURCES;
        if (!PrepareResources(lpOutputBuffer))
            return false;

        // The four spark-effect parameter sets, the three debris parameter sets and the
        // junkyard locators: each attrib instance is built over the collection the string
        // key names (the X360 keyed ctors hand r4 straight to FindCollection) and assigned
        // in (Attrib::Instance::operator=), the temporary released after each.
        static const char* const KAAC_SPARK_PARAM_KEYS[KU_NUM_SPARK_PARAMS] =
            { "376835", "376836", "376837", "554431" };
        for (u32 lu = 0; lu < KU_NUM_SPARK_PARAMS; ++lu)
        {
            mSparkParams[lu] = Attrib::Gen::sparkeffect(Attrib::StringToKey(KAAC_SPARK_PARAM_KEYS[lu]), 0);
        }
        mCrashingDebrisParams         = Attrib::Gen::debrisparams(Attrib::StringToKey("383338"), 0);
        mRoadRageDebrisParams         = Attrib::Gen::debrisparams(Attrib::StringToKey("595518"), 0);
        mAIRaceCarCrashingTrailDebris = Attrib::Gen::debrisparams(Attrib::StringToKey("608203"), 0);
        mJunkYardLocatorsData         = Attrib::Gen::junkyardlocators(Attrib::StringToKey("601979"), 0);

        LoadNativeParticleParams();

        // asm 0x8229E8F0-0x8229E9C4: the four spark parameter sets are copied (36 words
        // each: the 4-vector head + the 9 scalars) into the particle module's four spark
        // arrays (ParticleModule +0x9544..). NOT RECONSTRUCTED: the spark arrays are an
        // asm-sized placeholder in ParticleModule.h (no committed SparkArray type), so
        // there is no named destination. Loud, not silent.
        {
            static bool sbLogged = false;
            LogNotReconstructed(sbLogged,
                "EffectsModule::Prepare's spark-parameter copy into ParticleModule's spark arrays "
                "(the SparkArray type is a placeholder; sparks are not reconstructed)");
        }

        // Attrib::SetEditNotifier(sub_822793C8): the attrib live-edit hook that raises
        // mResetAttribs on the module. NOT RECONSTRUCTED: there is no attrib editor on
        // this build (SetEditNotifier has no PC body); nothing can raise the flag, so
        // Update's LoadNativeParticleParams re-push never fires. Recorded, not faked.
    }
        // fall through
    case E_PREPARESTAGE_POST_PREPARE_PREPARE:
    {
        mePrepareStage = E_PREPARESTAGE_POST_PREPARE_PREPARE;

        ParticlePrepareOutput* lpParticleOutput = 0;
        const bool lbCreated = lpUpdateOutputBufferStack->CreateIOBuffer(&lpParticleOutput, "Particles");
        CGS_ASSERT(lbCreated, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");
        (void)lbCreated;

        if (mParticleModule.PostPreparePrepare(lpParticleOutput))
        {
            const bool lbDestroyed = lpUpdateOutputBufferStack->DestroyIOBuffer(&lpParticleOutput);
            CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");
            (void)lbDestroyed;
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;
        }

        lpOutputBuffer->LockForWrite();
        lpOutputBuffer->SetResourceRequestInterface(lpParticleOutput->GetResourceRequestInterface());
        lpOutputBuffer->UnlockForWrite();
        const bool lbDestroyed = lpUpdateOutputBufferStack->DestroyIOBuffer(&lpParticleOutput);
        CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");
        (void)lbDestroyed;
        return false;
    }
    case E_PREPARESTAGE_DONE:
        meReleaseStage = E_RELEASESTAGE_START;
        mePrepareStage = E_PREPARESTAGE_DONE;
        return true;
    default:
        CGS_ASSERT(false, "Invalid Stage\n");   // EffectsModule.cpp:556
        return false;
    }
}

// =============================================================================
// LoadNativeParticleParams  @0x82290510  (DWARF :588)
//   Twelve {array index, collection key} pairs: resolve each simple-particle parameter
//   collection (class 0xE836C90A, the key StringToKey'd from the literal), wrap it in an
//   Attrib::Instance (default data area 144 when the collection carries none) and push it
//   into maSimpleParticles[index] through BrnSimpleParticleArray::UpdateParams.
//   PARTIAL: UpdateParams has no PC body and BrnSimpleParticleArray is a partial layout
//   (see ParticleModule.h), so the push is announced, not performed. The pair table and
//   the resolves are the console's.
// =============================================================================
void EffectsModule::LoadNativeParticleParams()
{
    struct NativeParticleParamEntry { u32 muArrayIndex; const char* lpcCollectionKey; };
    static const NativeParticleParamEntry KAA_NATIVE_PARTICLE_PARAMS[12] =
    {
        {  1, "504146" }, {  2, "504147" }, {  3, "554556" }, {  4, "554558" },
        {  5, "554557" }, {  6, "554559" }, {  7, "561481" }, {  8, "561868" },
        {  9, "561870" }, { 10, "561869" }, { 11, "561872" }, { 12, "561871" },
    };
    // Attrib::FindCollection(-399105142): the simple-particle-params class key. FLAG: only
    // the low word (0xE836C90A) is visible in the pseudocode; the 64-bit form is not
    // recovered here, and with the consumer absent the resolve is not performed either.
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged,
        "EffectsModule::LoadNativeParticleParams -> BrnSimpleParticleArray::UpdateParams x12 "
        "(no PC body; simple particles are not reconstructed)");
    (void)KAA_NATIVE_PARTICLE_PARAMS;
}

// =============================================================================
// PostWorldPreparePrepare  @0x822902F0  (DWARF :556)
//   The loading spine's LoadWorldCollision tail: re-point the surface list at the
//   world's surfacelist collection, sanity-check surface 1's leading vector, then push
//   every surface's skid-mark colour pair into the trail system.
// =============================================================================
void EffectsModule::PostWorldPreparePrepare()
{
    // ⭐⭐ THE COLLECTION KEY IS "340654", AND THE ZERO THAT STOOD HERE EMPTIED THE SURFACE LIST.
    //
    //   BrnEffects::EffectsModule::PostWorldPreparePrepare @0x822902F0
    //       CollectionWithDefault = Attrib::FindCollectionWithDefault(-2051685132, qword_82FAB7A8);
    //       Attrib::Instance::Change(a1 + 185288, CollectionWithDefault);
    //   and qword_82FAB7A8's ONE writer is a dynamic initialiser:
    //       sub_82C4A1F8:  qword_82FAB7A8 = Attrib::StringToKey("340654");
    // -- a literal, not a runtime value. (-2051685132 == 0x85B5C4F4 is the low word of
    // surfacelist::KU_SURFACELIST_CLASS_KEY, which ChangeWithDefault already passes, so the
    // collection key is the only argument that was ever missing.)
    //
    // The FLAG that stood here said the key "is not modelled on the host" and passed 0, which
    // resolves the class's DEFAULT collection. MEASURED CONSEQUENCE (run 15, BRN_SKID_PROBE,
    // the probe extended to print the surface count and whether the lookup resolved):
    //     [skid] ... surf=1/0 ref=0 en=0 skid=0.0440 > thr=0.0000 type=0 ready=1
    //     [skid] ... surf=2/0 ref=0 en=0 skid=0.0452 > thr=0.0000 type=0 ready=1
    //     324 lines, every one of them, across both surfaces the wheels touched
    // -- `/0` is Num_Surfaces() and `ref=0` is Surfaces(id) returning null. The list was EMPTY.
    // HandleWheels then took the console's own `Attrib::DefaultDataArea(24)` fallback, whose
    // area is ZEROED, so SkidMarksEnabled read false, the threshold read 0.0 and the type read
    // 0 -- three adjacent zeros that look exactly like a surface with skid marks switched off.
    // A "count 0 / empty array" defect wearing the costume of a design decision.
    //
    // It also silently disabled the loop below: Num_Surfaces() == 0 means
    // TrailSystem::UpdateTrailType never ran for any surface, so even a segment that HAD been
    // laid would have drawn with the trail types' construct-time colours.
    Attrib::Collection* const lpSurfaceCollection =
        mSurfaceList.ChangeWithDefault(Attrib::StringToKey(KAC_WORLD_SURFACELIST_COLLECTION));

    // ⭐⭐ THE BOOT FAULT OF RUNS 17/18 WAS NOT HERE, AND IT WAS NOT AN ASSET/LOADER GAP.
    // The note that stood here said "FindCollectionWithDefault(surfacelist, StringToKey(
    // \"340654\")) resolved to NOTHING on this build" and guarded on Change()'s return value.
    // Both halves were wrong:
    //   * Attrib::Instance::Change @0x8280D1A8 returns the collection the instance held
    //     BEFORE the swap on the null path (`lResult = mpCollection` never reassigned when
    //     lpNewCollection is 0), so `Change(...) == 0` is not a test of whether the resolve
    //     succeeded. That is why the guard "did not fire".
    //   * The record IS in the shipped data: SURFACELIST.BIN carries a CollectionLoadData
    //     with mKey = B96A0FF96535775A (= StringToKey("340654")), mClass = 42C25F4985B5C4F4
    //     (= StringToKey("surfacelist")) and one entry, key 0ADCE56EF3DA7F1F (= "Surfaces"),
    //     type Attrib::RefSpec -- and the bundle loads (BrnGame.log: "LoadBundle
    //     'surfacelist.bin' -> pool 7: 1 resources").
    // The real fault was the visualfxsurface constructor in the loop below taking an
    // Attrib::Collection* where the console passes an Attrib::RefSpec -- see visualfxsurface.h.
    //
    // [skid-bind] BOTH SIDES OF THE RESOLVE, once. Never control flow: the console has no
    // guard here, and an invented early-out is what hid the real defect for two runs.
    {
        static bool sbLoggedBind = false;
        if (!sbLoggedBind)
        {
            sbLoggedBind = true;
            void* const lpSurface1 = mSurfaceList.Surfaces(1);
            const Attrib::RefSpec* const lpRef1 =
                static_cast<const Attrib::RefSpec*>(lpSurface1);
            char lacMsg[400];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[skid-bind] PostWorldPreparePrepare: key=%016llX prevCollection=%p "
                "boundCollectionKey=%016llX Num_Surfaces=%d Surfaces(1)=%p "
                "ref1{class=%016llX collection=%016llX resolved=%d}\n",
                static_cast<unsigned long long>(
                    Attrib::StringToKey(KAC_WORLD_SURFACELIST_COLLECTION)),
                static_cast<const void*>(lpSurfaceCollection),
                static_cast<unsigned long long>(mSurfaceList.GetCollection()),
                mSurfaceList.Num_Surfaces(),
                lpSurface1,
                lpRef1 ? static_cast<unsigned long long>(lpRef1->GetClassKey()) : 0ull,
                lpRef1 ? static_cast<unsigned long long>(lpRef1->GetCollectionKey()) : 0ull,
                lpRef1 ? (lpRef1->HasResolvedCollection() ? 1 : 0) : -1);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    {
        // Surface element 1's leading 4-vector must carry a magnitude: |lane| > epsilon in
        // at least one lane (vandc sign-clear, vcmpgtfp against unk_8200D990, CR6 "all
        // false" fires the assert).
        //
        // ⭐ SPELLING FOLLOWS THE CONSOLE, which distinguishes the two generated surface
        // constructors: here it resolves the ref itself and calls the Collection* form
        //     Collection = Attrib::RefSpec::GetCollection(AttributePointer);
        //     Attrib::Gen::surface::surface(v47, Collection, 0);      // @0x8227FAB0
        // whereas HandleWheels @0x82296C80 / WheelStateMachine::Update @0x82293EB8 call the
        // RefSpec form (sub_8227FB58) directly. The two land in the same place; keeping the
        // spellings distinct keeps each site mapped 1:1 onto the symbol it really calls.
        void* lpSurfaceRef = mSurfaceList.Surfaces(1);
        if (!lpSurfaceRef)
            lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);
        Attrib::Collection* lpSurfaceCollectionForRef = const_cast<Attrib::Collection*>(
            static_cast<Attrib::RefSpec*>(lpSurfaceRef)->GetCollection());
        Attrib::Gen::surface lSurface(lpSurfaceCollectionForRef, 0);
        const f32* lpfLeading = reinterpret_cast<const f32*>(lSurface.GetAttributeData());
        const f32 KF_EPSILON = 1.1920929e-07f;   // unk_8200D990 (FLT_EPSILON splat)
        const bool lbAnyLane = std::fabs(lpfLeading[0]) > KF_EPSILON || std::fabs(lpfLeading[1]) > KF_EPSILON
                            || std::fabs(lpfLeading[2]) > KF_EPSILON || std::fabs(lpfLeading[3]) > KF_EPSILON;
        CGS_ASSERT(lbAnyLane, "Surface list appears to be corrupt");   // EffectsModule.cpp:573
        (void)lbAnyLane;
    }

    // [skid-bind] ONE LINE PER SURFACE on the FIRST pass only, printing BOTH SIDES of every
    // pointer this loop follows. The question it answers is the one the per-frame gate line
    // cannot: with Num_Surfaces() == 20, ref=1 and a real threshold of 0.3, `en=0` on every
    // surface could equally be (a) authored -- these surfaces genuinely have skid marks off --
    // or (b) all 20 resolving to ONE shared block (the class's default layout), which would
    // make every surface read identically whatever the data says. DISTINCT vfxLayout pointers
    // settle it in favour of (a); identical ones convict the resolve.
    static bool sbLoggedSurfaces = false;

    for (u32 luSurface = 0; luSurface < static_cast<u32>(mSurfaceList.Num_Surfaces()); ++luSurface)
    {
        void* lpSurfaceRef = mSurfaceList.Surfaces(luSurface);
        if (!lpSurfaceRef)
            lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);
        Attrib::Collection* lpElementCollection = const_cast<Attrib::Collection*>(
            static_cast<Attrib::RefSpec*>(lpSurfaceRef)->GetCollection());
        Attrib::Gen::surface lSurface(lpElementCollection, 0);
        // The visualfxsurface REF embedded in the surface layout at +0x10 (console:
        // `visualfxsurface(v49, v52 + 16, 0)` with v52 == the surface instance's
        // mpAttributeData, into the ctor whose Instance overload is sub_8280A248).
        const Attrib::RefSpec& lrVfxRef = VfxSurfaceRef(lSurface.GetAttributeData());
        Attrib::Gen::visualfxsurface lVfx(lrVfxRef, 0);
        const void* lpVfxData = lVfx.GetAttributeData();

        if (!sbLoggedSurfaces)
        {
            const u8* lpBytes = static_cast<const u8*>(lpVfxData);
            char lacMsg[420];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[skid-bind] surf %2u: surfCol=%016llX surfLayout=%p "
                "vfxRef{class=%016llX col=%016llX res=%d} vfxCol=%016llX vfxLayout=%p "
                "thr=%.4f en=%d smoke=%d/%d type=%d\n",
                luSurface,
                static_cast<unsigned long long>(lSurface.GetCollection()),
                lSurface.GetAttributeData(),
                static_cast<unsigned long long>(lrVfxRef.GetClassKey()),
                static_cast<unsigned long long>(lrVfxRef.GetCollectionKey()),
                lrVfxRef.HasResolvedCollection() ? 1 : 0,
                static_cast<unsigned long long>(lVfx.GetCollection()),
                lpVfxData,
                static_cast<double>(ReadF32(lpVfxData, KU_VFX_SKID_MARK_THRESHOLD)),
                lpBytes ? lpBytes[KU_VFX_SKID_MARKS_ENABLED] : -1,
                lpBytes ? lpBytes[0x4C] : -1,
                lpBytes ? lpBytes[0x4D] : -1,
                static_cast<int>(ReadS16(lpVfxData, KU_VFX_SKID_MARK_TYPE_ID)));
            CgsDev::Log::WriteToLog(lacMsg);
        }

        mParticleModule.TrailSystem().UpdateTrailType(
            ReadS16(lpVfxData, KU_VFX_SKID_MARK_TYPE_ID),
            ReadVector4(lpVfxData, KU_VFX_SKID_MARK_START_COLOUR),
            ReadVector4(lpVfxData, KU_VFX_SKID_MARK_END_COLOUR));
    }
    sbLoggedSurfaces = true;
}

// =============================================================================
// Release  @0x8227FCA8  (DWARF :767)
// =============================================================================
bool EffectsModule::Release()
{
    if (!mParticleModule.Release())      // vtable +8
        return false;

    switch (meReleaseStage)
    {
    case E_RELEASESTAGE_START:
        mReceiverQueue.Clear();
        // fall through
    case E_RELEASESTAGE_MANAGER:
        meReleaseStage = E_RELEASESTAGE_MANAGER;
        if (!CgsModule::ModuleSingleBuffered::Release())
            return false;
        break;
    case E_RELEASESTAGE_DONE:
        break;
    default:
        CGS_ASSERT(false, "Invalid Stage\n");   // EffectsModule.cpp:816
        return false;
    }
    meReleaseStage = E_RELEASESTAGE_MANAGER;
    mePrepareStage = E_PREPARESTAGE_START;
    return true;
}

// =============================================================================
// Destruct  @0x8227FD78  (DWARF :825)
// =============================================================================
void EffectsModule::Destruct()
{
    mReceiverQueue.Clear();
    mParticleModule.Destruct();          // vtable +12
    mDebugComponent.Destruct();          // asserts mpEffectsModule, clears it, base dtor
    CgsModule::ModuleSingleBuffered::Destruct();
}

// =============================================================================
// RestartEffects  @0x822793E0  (DWARF :2110) -- the QA "restart" latch.
// =============================================================================
void EffectsModule::RestartEffects()
{
    sbRestartEffects = true;
}

u32 EffectsModule::GetJunkyardEffectHandle(u32 luIndex) const
{
    CGS_ASSERT(luIndex < KU_MAX_JUNKYARD_VFX, "luIndex < KU_MAX_JUNKYARD_VFX");
    return maJunkyardEffectHandles[luIndex];
}

// =============================================================================
// GetPlayerRaceCarState  @0x822803C0  (DWARF :2946)
// =============================================================================
const RaceCarState*
EffectsModule::GetPlayerRaceCarState(const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars)
{
    if (!lpActiveRaceCars->IsPlayerCarActive())
        return 0;
    return lpActiveRaceCars->GetRaceCarState(lpActiveRaceCars->GetPlayerActiveRaceCarIndex());
}

// =============================================================================
// Update  @0x8229EC28  (DWARF :852) -- one simulation sub-step.
// =============================================================================
void EffectsModule::Update(CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                           CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                           const EffectsIO::InputBuffer* lpInputBuffer,
                           EffectsIO::OutputBuffer* lpOutputBuffer,
                           BrnUpdateSet /*leUpdateSet*/)
{
    // +0x2C340 -- the debug component's "bypass all VFX processing" byte.
    if (mDebugComponent.BypassAllVFXProcessing())
    {
        SkidGateExit("mDebugComponent.BypassAllVFXProcessing()");
        return;
    }
    // +0x2C351 -- the "dump running lion effects" latch: consumed, and the step skipped.
    if (mDebugComponent.ConsumeDumpRunningLionEffects())
    {
        SkidGateExit("mDebugComponent.ConsumeDumpRunningLionEffects()");
        return;
    }

    lpOutputBuffer->LockForWrite();
    lpOutputBuffer->GetReplayRequestInterface()->RegisterSerialiser(&mEffectsSerialiser);
    lpInputBuffer->LockForRead();

    if (mResetAttribs)
    {
        LoadNativeParticleParams();
        mResetAttribs = false;
    }

    // ---- the suspend / resume ladder (dword_82FAD294) ------------------------------------
    // 0 normal; on mbSuspendEffects: Lion off, -> 4 (wait 5 frames) -> 3 (suspend the playing
    // effects) -> 1 (wait for the flag to clear) -> 2 (resume, re-push every surface's skid
    // colours, and run THIS step) -> 0.
    bool lbRunStep = false;
    switch (siEffectsSuspendState)
    {
    case 0:
        if (!lpInputBuffer->GetSuspendEffects())
        {
            lbRunStep = true;
            break;
        }
        mParticleModule.mbLionEnabled = false;      // +0x23BB4
        siEffectsSuspendFrames = 0;
        siEffectsSuspendState  = 4;
        break;
    case 1:
        if (!lpInputBuffer->GetSuspendEffects())
            siEffectsSuspendState = 2;
        break;
    case 2:
        mParticleModule.ResumePlayingEffects();
        mParticleModule.mbLionEnabled = true;
        siEffectsSuspendState = 0;
        for (u32 luSurface = 0; luSurface < static_cast<u32>(mSurfaceList.Num_Surfaces()); ++luSurface)
        {
            void* lpSurfaceRef = mSurfaceList.Surfaces(luSurface);
            if (!lpSurfaceRef)
                lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);
            Attrib::Gen::surface lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
            Attrib::Gen::visualfxsurface lVfx(VfxSurfaceRef(lSurface.GetAttributeData()), 0);
            const void* lpVfxData = lVfx.GetAttributeData();
            mParticleModule.TrailSystem().UpdateTrailType(
                ReadS16(lpVfxData, KU_VFX_SKID_MARK_TYPE_ID),
                ReadVector4(lpVfxData, KU_VFX_SKID_MARK_START_COLOUR),
                ReadVector4(lpVfxData, KU_VFX_SKID_MARK_END_COLOUR));
        }
        lbRunStep = (siEffectsSuspendState == 0);
        break;
    case 3:
        mParticleModule.SuspendPlayingEffects();
        siEffectsSuspendState = 1;
        break;
    case 4:
        if (++siEffectsSuspendFrames == 5)
            siEffectsSuspendState = 3;
        break;
    default:
        lbRunStep = (siEffectsSuspendState == 0);
        break;
    }

    if (!lbRunStep)
    {
        SkidGateExit("the suspend/resume ladder (siEffectsSuspendState != 0)");
        lpInputBuffer->UnlockForRead();
        lpOutputBuffer->UnlockForWrite();
        return;
    }

    // ---- the per-system enables, debug component -> particle module (asm 0x8229EF2C..C0) ----
    // [skid-gate] the POSITIVE witness: no exit fired, the per-system enables are about to be
    // copied. Prints once with the values, because mbTrailsEnabled is what puts
    // eRenderDataFlagRenderTrails (0x20) into the render-data flag word.
    if (SkidProbeEnabled())
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            char lacMsg[224];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[skid-gate] Update REACHED the per-system enables: trails=%d sparks=%d "
                "debris=%d simple=%d lion=%d\n",
                mDebugComponent.TrailsEnabled() ? 1 : 0, mDebugComponent.SparksEnabled() ? 1 : 0,
                mDebugComponent.DebrisEnabled() ? 1 : 0, mDebugComponent.SimpleEnabled() ? 1 : 0,
                mDebugComponent.LionEnabled() ? 1 : 0);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }
    mParticleModule.mbSparksEnabled = mDebugComponent.SparksEnabled();   // +0x2C341 -> +0x23BB0
    mParticleModule.mbTrailsEnabled = mDebugComponent.TrailsEnabled();   // +0x2C342 -> +0x23BB1
    mParticleModule.mbDebrisEnabled = mDebugComponent.DebrisEnabled();   // +0x2C343 -> +0x23BB2
    mParticleModule.mbSimpleEnabled = mDebugComponent.SimpleEnabled();   // +0x2C346 -> +0x23BB3
    mParticleModule.mbLionEnabled   = mDebugComponent.LionEnabled();     // +0x2C347 -> +0x23BB4
    mParticleModule.mbZFadeEnabled  = mDebugComponent.UseZFade();        // +0x2C349 -> +0x23BB5

    // asm 0x8229EFC4-0x8229F0A8: the four spark parameter sets copied into the particle
    // module's spark arrays every step. NOT RECONSTRUCTED (placeholder destination); the
    // Prepare-time copy already announced it once.

    if (mEffectsSerialiser.GetStaticLayout() == 0)
    {
        SkidGateExit("mEffectsSerialiser.GetStaticLayout() == 0 -- the replay module never "
                     "gave the effects serialiser its static buffer");
        lpInputBuffer->UnlockForRead();
        lpOutputBuffer->UnlockForWrite();
        return;
    }

    const BrnDirector::Camera::Camera* lpCamera = lpInputBuffer->GetCameraInput();
    CGS_ASSERT(lpCamera != 0, "lpCamera != NULL");   // EffectsModule.cpp:1035
    const BrnDirector::Camera::CameraState& lrCameraState = lpCamera->GetState();

    // `(*(camera + 81) & 0x400000)` == CameraState flag index 22 (E_FLAG_IN_JY_CAMERA).
    bool lbInJunkyardCamera = lrCameraState.IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_IN_JY_CAMERA);
    bool lbWasInJunkyard    = mParticleModule.mbIsInJunkyard;   // +0x23BB6

    // The SIM timer status (+0x18 of the interface): dt = running ? base * multiplier : 0.
    const CgsSystem::TimerStatus* lpSimTimer = lpInputBuffer->GetTimerStatusInterface()->GetSimTimerStatus();
    const f32 lfTimeStepMultiplier = lpSimTimer->GetTimeStepMultiplier();
    const f32 lfDt = lpSimTimer->IsRunning()
                   ? (lpSimTimer->GetBaseTimeStep() * lfTimeStepMultiplier)
                   : KF_ZERO_TIME_STEP;
    const CgsSystem::Time lTime = lpSimTimer->GetTime();
    const f32 lfTime = static_cast<f32>(lTime.GetSeconds()) + lTime.GetFraction();

    EffectsModuleParams lParams;
    lParams.mDt   = lfDt;
    lParams.mTime = lfTime;
    lParams.mPad08[0] = lParams.mPad08[1] = lParams.mPad08[2] = lParams.mPad08[3] = 0;
    lParams.mPad08[4] = lParams.mPad08[5] = lParams.mPad08[6] = lParams.mPad08[7] = 0;
    lParams.mCameraPosition = lpCamera->GetTransform().wAxis;   // camera +0x30, the transform's position row

    mbUpdateRan2F5A9 = true;
    mbUpdateRan2F5A8 = true;

    BrnReplays::EffectsSerialiserStaticLayout* lpLayout = mEffectsSerialiser.GetStaticLayout();
    const BrnReplays::EffectsSerialiser::EMode leMode = mEffectsSerialiser.GetMode();
    const bool lbPlaying   = (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_PREPARING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_STALLED);
    const bool lbStalled   = (leMode == BrnReplays::EffectsSerialiser::E_MODE_RECORDING_STALLED)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_STALLED);
    const bool lbRecording = (leMode == BrnReplays::EffectsSerialiser::E_MODE_RECORDING_PREPARING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_RECORDING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_RECORDING_STALLED);

    if (lbPlaying && !lbStalled)
    {
        if (sfLastUpdateTime > lfTime)
            mParticleModule.ResetSparkFrameData();
        mEffectsSerialiser.Read();
        meCurrentGameMode  = static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(lpLayout->GetGameMode());
        lbInJunkyardCamera = lpLayout->GetInJunkyardCamera();
        lbWasInJunkyard    = lpLayout->GetWasInJunkyard();
        mbEventIntroActive = lpLayout->GetEventIntroActive();
    }
    if (!lbStalled)
        sfLastUpdateTime = lfTime;
    if (lbRecording)
    {
        lpLayout->Clear();
        lpLayout->SetInJunkyardCamera(lbInJunkyardCamera);
        lpLayout->SetWasInJunkyard(lbWasInJunkyard);
        lpLayout->SetGameMode(static_cast<s32>(meCurrentGameMode));
        lpLayout->SetEventIntroActive(mbEventIntroActive);
    }

    if (lbPlaying)
    {
        if (lpLayout->GetShowtimeBouncePending())
            HandleShowtimeTrafficBounce(0, lpInputBuffer);
    }
    else
    {
        HandleGameActions(lpInputBuffer->GetGameActionQueue(), lpInputBuffer);
    }

    if (lbInJunkyardCamera)
    {
        if (!lbWasInJunkyard)
        {
            mParticleModule.mbIsInJunkyard = true;
            JunkyardVfxStart(lParams.mCameraPosition);
        }
    }
    else if (lbWasInJunkyard)
    {
        JunkyardVfxStop();
        mParticleModule.mbIsInJunkyard = false;
    }

    const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars = lpInputBuffer->GetActiveRaceCarInterface();
    // `(*(camera + 81) & 0x10)` == flag index 4 (E_FLAG_BUMPER_CAM): the player's effects go
    // to world 1 (the in-car world) while the bumper camera is up.
    const bool lbBumperCam = lrCameraState.IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_BUMPER_CAM);
    const AudioEffectsMessageQueue*   lpAudioEffects = lpInputBuffer->GetAudioEffectsMessageQueue();
    const DeformationOutputInterface* lpDeformation  = lpInputBuffer->GetDeformationInterface();

    ProcessActiveRaceCars(lParams, lpActiveRaceCars, lpInputBuffer->GetBoostInfos(),
                          lpDeformation, lpAudioEffects, lbBumperCam);
    ProcessCarContactQueues(lParams, lpActiveRaceCars, lpInputBuffer->GetContactSpyInterface(), lpCamera);
    HandleGlassSmashEventsForAllCars(lpInputBuffer, lpActiveRaceCars, lfDt, lfTime);

    // GetPlayerRaceCarState + the prop-VFX locator queue -> PropCollisions::UpdateLocatorVfx
    // (&mParticleModule.mPropCollisions, dt, time, ...). NOT RECONSTRUCTED: PropCollisions
    // has no committed body; announced once.
    (void)GetPlayerRaceCarState(lpActiveRaceCars);
    {
        static bool sbLogged = false;
        LogNotReconstructed(sbLogged,
            "BrnEffects::PropCollisions::UpdateLocatorVfx (the prop-strike VFX; ParticleModule::mPropCollisions is a placeholder)");
    }

    if (lpActiveRaceCars->IsPlayerCarActive())
    {
        const EActiveRaceCarIndex lePlayer = lpActiveRaceCars->GetPlayerActiveRaceCarIndex();
        const RaceCarState* lpPlayerState  = lpActiveRaceCars->GetRaceCarState(lePlayer);
        HandlePlayerTriangleCache(lpInputBuffer, lpPlayerState, maActiveRaceCarData[lePlayer]);
        HandleQADebugTests(lfDt, lfTime, lpPlayerState);
        if (lrCameraState.HasChanged(BrnDirector::Camera::CameraState::E_FLAG_BUMPER_CAM))
        {
            // `ld r11, 8(state); rlwinm r11,r11,0,27,27` == the current BUMPER_CAM bit -> world 1.
            ParticleEffectHelper lHelper(mParticleModule);
            maActiveRaceCarData[lePlayer].mBoostMachine.SetWorldIndex(lHelper, lbBumperCam ? 1u : 0u);
        }
    }

    // The particle module's own step (console vtable +68): f1 = dt, f2 = time, f3 = multiplier.
    mParticleModule.Update(lfDt, lfTime, lfTimeStepMultiplier, lpCamera);

    if (lpActiveRaceCars->IsPlayerCarActive())
    {
        const RaceCarState* lpPlayerState =
            lpActiveRaceCars->GetRaceCarState(lpActiveRaceCars->GetPlayerActiveRaceCarIndex());
        mCarStateCache.SetLinearVelocity(lpPlayerState->mLinearVelocity);     // +816  -> +0x2C300
        mCarStateCache.SetAngularVelocity(lpPlayerState->mAngularVelocity);   // +832  -> +0x2C310
        mCarStateCache.SetSpeedMPH(lpPlayerState->mfSpeedMPH);                // +972  -> +0x2C320
        mCarStateCache.SetSteering(lpPlayerState->mfSteering);                // +1044 -> +0x2C324
        // `(*(camera + 81) & 8)` == flag index 3 (E_FLAG_RACING_GAMEPLAY_CAMERA).
        mCarStateCache.SetIsRacingGameplayCamera(
            lrCameraState.IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_RACING_GAMEPLAY_CAMERA));
    }

    lpInputBuffer->UnlockForRead();
    lpOutputBuffer->UnlockForWrite();

    if (lbRecording)
        mEffectsSerialiser.Write();
}

// =============================================================================
// ProcessActiveRaceCars  @0x8229EB30  (DWARF :2976)
// =============================================================================
void EffectsModule::ProcessActiveRaceCars(const EffectsModuleParams& lrParams,
                                          const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                          const BoostOutputInfo* lpBoostInfos,
                                          const DeformationOutputInterface* lpDeformation,
                                          const AudioEffectsMessageQueue* lpAudioEffects,
                                          bool lbBumperCam)
{
    CGS_ASSERT(lpActiveRaceCars != 0, "lpActiveRaceCarInterface != NULL");   // EffectsModule.cpp:3252
    EActiveRaceCarIndex lePlayer = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    if (lpActiveRaceCars->IsPlayerCarActive())
        lePlayer = lpActiveRaceCars->GetPlayerActiveRaceCarIndex();
    UpdateActiveRaceCars(lePlayer, lrParams, lpActiveRaceCars, lpBoostInfos, lpDeformation,
                         lpAudioEffects, lbBumperCam);
}

// =============================================================================
// UpdateActiveRaceCars  @0x8229DB30  (DWARF :3069) -- the per-car pipeline.
// =============================================================================
void EffectsModule::UpdateActiveRaceCars(EActiveRaceCarIndex lePlayerIndex,
                                         const EffectsModuleParams& lrParams,
                                         const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                         const BoostOutputInfo* lpBoostInfos,
                                         const DeformationOutputInterface* lpDeformation,
                                         const AudioEffectsMessageQueue* lpAudioEffects,
                                         bool lbBumperCam)
{
    // The audio module's exhaust-pop messages for this step (event type 1 == POP).
    bool labExhaustPopThisFrame[KU_NUM_ACTIVE_RACE_CARS];
    f32  lafExhaustPopIntensity[KU_NUM_ACTIVE_RACE_CARS];
    for (u32 lu = 0; lu < KU_NUM_ACTIVE_RACE_CARS; ++lu)
    {
        labExhaustPopThisFrame[lu] = false;
        lafExhaustPopIntensity[lu] = 0.0f;
    }
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = lpAudioEffects->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (liType == BrnSound::Module::Io::E_EFFECTS_MESSAGE_TYPES_POP)
            {
                const BrnSound::Module::Io::PopEffectsMessage* lpPop =
                    reinterpret_cast<const BrnSound::Module::Io::PopEffectsMessage*>(lpEvent);
                const u32 luRaceCarId = lpPop->muRaceCarID;
                CGS_ASSERT(luRaceCarId < KU_NUM_ACTIVE_RACE_CARS, "luRaceCarId < E_ACTIVE_RACE_CAR_INDEX_COUNT"); // :3378
                lafExhaustPopIntensity[luRaceCarId] = lpPop->mfIntensity;
                labExhaustPopThisFrame[luRaceCarId] = true;
            }
            const CgsModule::Event* lpNext = 0;
            liType = lpAudioEffects->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    const BrnReplays::EffectsSerialiser::EMode leMode = mEffectsSerialiser.GetMode();
    const bool lbPlaying   = (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_PREPARING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_STALLED);
    const bool lbRecording = (leMode == BrnReplays::EffectsSerialiser::E_MODE_RECORDING_PREPARING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_RECORDING)
                          || (leMode == BrnReplays::EffectsSerialiser::E_MODE_RECORDING_STALLED);

    for (u32 luCar = 0; luCar < KU_NUM_ACTIVE_RACE_CARS; ++luCar)
    {
        const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(luCar);
        ActiveRaceCarData& lrData = maActiveRaceCarData[luCar];

        const CgsID lModelId = lpActiveRaceCars->GetCarModelId(leIndex);
        const bool lbModelChanged = (lrData.GetID() != lModelId);
        const bool lbLoaded = lpActiveRaceCars->IsRaceCarLoaded(leIndex);   // flags bit 4

        // The two burst timers count down and clamp at zero (fsel).
        mafTimeUntilNextDebrisBurst[luCar] -= lrParams.mDt;
        if (mafTimeUntilNextDebrisBurst[luCar] < 0.0f) mafTimeUntilNextDebrisBurst[luCar] = 0.0f;
        mafTimeUntilNextSparksBurst[luCar] -= lrParams.mDt;
        if (mafTimeUntilNextSparksBurst[luCar] < 0.0f) mafTimeUntilNextSparksBurst[luCar] = 0.0f;

        if (lbModelChanged)
        {
            ParticleEffectHelper lResetHelper(mParticleModule);
            lrData.Reset(lResetHelper);
            if (!lbLoaded)
                continue;
        }

        if (!lpActiveRaceCars->IsRaceCarActive(leIndex))   // flags bit 0
            continue;
        if (!lpActiveRaceCars->IsRaceCarLoaded(leIndex))   // flags bit 4
            continue;

        const RaceCarState* lpState = lpActiveRaceCars->GetRaceCarState(leIndex);
        CGS_ASSERT(lpState != 0, "lpActiveRaceCarState != NULL");   // EffectsModule.cpp:3434
        const RwRGBAReal& lrColour = lpActiveRaceCars->GetRaceCarColour(leIndex);

        const u32 luWorldIndex = (leIndex == lePlayerIndex && lbBumperCam) ? 1u : 0u;

        // The car's deformation locators: the deformation output's table entry whose entity
        // id is this race car (owner 1, entity index == the slot).
        const BrnPhysics::Deformation::VehicleLocatorOutput* lpLocators = 0;
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set(1u, luCar, 0u);
            for (s32 li = 0; li < lpDeformation->miNumLocatorOutputs; ++li)
            {
                // VehicleLocatorOutput::mEntityId is the plain `struct EntityId { u32 muValue; }`
                // (BrnCommonTypes.h), not CgsSceneManager::EntityId -- the console compares the
                // two packed words, so compare the words.
                if (lpDeformation->maLocatorData[li].mEntityId.muValue == static_cast<u32>(lEntityId))
                {
                    lpLocators = &lpDeformation->maLocatorData[li];
                    break;
                }
            }
        }

        RaceCarParticleEffectHelper lHelper(*this, lrData, lpState, mParticleModule, &mDebugComponent,
                                            luWorldIndex, lrColour, meCurrentGameMode, lpLocators);

        if (lbModelChanged)
        {
            const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec> lPhysicsResource =
                lpActiveRaceCars->GetDeformationModelResourcePtr(leIndex);
            lrData.Initialise(lModelId, lPhysicsResource, lHelper);
        }

        // ---- the CarState record (v104 @sp+0x90) ---------------------------------------
        const BoostOutputInfo& lrBoost = lpBoostInfos[luCar];
        CarState lCarState;
        lCarState.muRaceCarIndex = luCar;
        for (u32 lu = 0; lu < 0x0C; ++lu) lCarState.mPad04[lu] = 0;
        lCarState.mEffectsModuleParams  = lrParams;
        lCarState.mpEffectsSerialiser   = &mEffectsSerialiser;
        lCarState.mbIsBoosting          = lrBoost.mbIsBoosting;                 // info +0
        lCarState.mPad35[0] = lCarState.mPad35[1] = lCarState.mPad35[2] = 0;
        lCarState.meBoostType           = static_cast<s32>(lrBoost.meBoostType); // info +32
        lCarState.mfBoostAmount         = lrBoost.mfBoostAmount;                 // info +16
        lCarState.mpCarState            = lpState;
        lCarState.mfSpeedMPH            = lpState->mfSpeedMPH;                   // +972
        lCarState.mfExhaustPopIntensity = lafExhaustPopIntensity[luCar];
        lCarState.mbExhaustPopThisFrame = labExhaustPopThisFrame[luCar];
        lCarState.mbCrashing            = lpState->mbCrashing;                   // +1098
        lCarState.mbJumping             = !lpState->mbCrashing && lpState->mfTimeInAir > 0.0f;   // +1028
        lCarState.mbEngineRunning       = lpActiveRaceCars->IsRaceCarEngineOn(leIndex);

        // The replay round-trip of the boost triple.
        if (lbPlaying)
        {
            u8  lu8Active = 0; f32 lfValue = 0.0f; s32 liType = 0;
            mEffectsSerialiser.GetStaticLayout()->GetBoostData(luCar, lu8Active, lfValue, liType);
            lCarState.mbIsBoosting  = (lu8Active != 0);
            lCarState.mfBoostAmount = lfValue;
            lCarState.meBoostType   = liType;
        }
        if (lbRecording)
        {
            mEffectsSerialiser.GetStaticLayout()->WriteBoostData(
                luCar, lCarState.mbIsBoosting ? 1 : 0, lCarState.mfBoostAmount, lCarState.meBoostType);
        }

        lrData.Tick(lCarState, *lpState, lHelper, mbEventIntroActive, leIndex == lePlayerIndex);

        // Just started crashing (IsCrashing && !WasCrashing): zero the crash / tyre
        // accumulators and the debris timer, latch the transform.
        if (lrData.JustStartedCrashing())
        {
            mafAccumulatedParticleCountCrash[luCar] = 0.0f;
            mafAccumulatedParticleCountTyres[luCar] = 0.0f;
            mafTimeUntilNextDebrisBurst[luCar]      = 0.0f;
            maRaceCarPreviousTransforms[luCar]      = lpState->mTransform;   // +496
        }

        HandleWheels(lCarState, lHelper);

        if (!mbEventIntroActive || leIndex == lePlayerIndex)
            HandleJumpAndLandingEffects(lCarState, lHelper, lrParams.mDt, lrParams.mTime,
                                        lrData.GetGroundPositionY());

        const RaceCarState* lpStateAgain = lpActiveRaceCars->GetRaceCarState(leIndex);
        if (lpState->mbCrashing)
        {
            (void)lpActiveRaceCars->GetRaceCarColour(leIndex);
            HandleCrashingTrail(lrData, lrParams.mDt, lrParams.mTime, lpStateAgain, leIndex);
        }
        maRaceCarPreviousTransforms[luCar] = lpStateAgain->mTransform;
    }
}

// =============================================================================
// HandleWheels  @0x82296C80  (DWARF :1743) -- the skid smoke, then THE TYRE MARK.
//
//   For each wheel: WheelStateMachine::Update (smoke). Then: a reset car transform, a
//   wheel off the ground / without traction / detached, or a wheel whose contact point
//   moved more than 0.02 m along its own contact normal since last step ends the trail
//   (mrLastTrailTime = -1); otherwise the contact surface's visualfxsurface decides:
//   SkidMarksEnabled AND mfSkidFactor > SkidMarkThreshold lays a segment
//   (TrailSystem::AddTrailSegment with the surface's SkidMarkTypeID, the skid factor
//   as the strength and the particle module's current time), else the trail ends.
//   Finally the wheel machine remembers this step's contact position.
// =============================================================================
void EffectsModule::HandleWheels(CarState& lrCarState, RaceCarParticleEffectHelper& lrHelper)
{
    ActiveRaceCarData&  lrData  = *lrHelper.ActiveRaceCar();
    const RaceCarState* lpState = lrHelper.RaceCarState();

    ++gauSkidProbeFrame;   // [skid probe] one tick per HandleWheels entry

    for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
        lrData.mWheelStateMachine[luWheel].Update(lrCarState, lrHelper);

    for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
    {
        BrnParticle::Native::TrailEmitterData& lrEmitter = lrData.mTrailEmitters[luWheel];
        const WheelLite&                       lrWheel   = lpState->maWheels[luWheel];
        WheelStateMachine&                     lrMachine = lrData.mWheelStateMachine[luWheel];

        if (lpState->mbResetCarTransform)                          // +1102
            lrEmitter.mrLastTrailTime = -1.0f;

        bool lbTrailEnded = true;
        f32  lfNormalDrift = 0.0f;   // [skid probe] hoisted so the OFFGATE arm can print it
        u32  luSurfaceId   = 0;      // [skid probe] ditto
        bool lbSurfaceResolved = false;  // [skid probe] did mSurfaceList.Surfaces() return a ref?
        if (lrWheel.mRoadContact.mbIsOnGround                       // +40
            && lrWheel.mbHasTraction                                // +97
            && lrWheel.mbAttached)                                  // +96
        {
            // |dot(pos - prevPos, normal)| > 0.02 -> the wheel left its contact plane.
            const Vector3 lvDelta = lrWheel.mRoadContact.mPosition - lrMachine.GetPreviousPosition();
            lfNormalDrift = rw::math::vpu::Dot(lvDelta, lrWheel.mRoadContact.mNormal);
            if (!(std::fabs(lfNormalDrift) > KF_TRAIL_NORMAL_DRIFT_MAX))
            {
                luSurfaceId =
                    (lrWheel.mRoadContact.mCollisionTag.muValue >> KU_SURFACE_ID_SHIFT) & KU_SURFACE_ID_MASK;
                void* lpSurfaceRef = mSurfaceList.Surfaces(luSurfaceId);
                // [skid probe] the ONE thing the gate line could not tell apart: whether the
                // surface resolved at all. The console's own fallback is
                // Attrib::DefaultDataArea(24) -- a ZEROED area -- so a failed lookup reads back
                // as en=0, thr=0.0, type=0, which is indistinguishable from a surface that
                // genuinely has skid marks turned off. Record which it was.
                lbSurfaceResolved = (lpSurfaceRef != 0);
                if (!lpSurfaceRef)
                    lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);
                // sub_8227FB58 -- the surface ctor's RefSpec overload, then the
                // visualfxsurface REF at layout+0x10 (console: `sub_8227FB58(v32,
                // AttributePointer, 0); visualfxsurface(v30, v33 + 16, 0)`).
                Attrib::Gen::surface lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
                Attrib::Gen::visualfxsurface lVfx(VfxSurfaceRef(lSurface.GetAttributeData()), 0);
                const void* lpVfxData = lVfx.GetAttributeData();

                const bool lbSkidMarksEnabled = ReadBool(lpVfxData, KU_VFX_SKID_MARKS_ENABLED);
                const f32  lfSkidThreshold    = ReadF32(lpVfxData, KU_VFX_SKID_MARK_THRESHOLD);
                const f32  lfSkidFactor       = lrWheel.mfSkidFactor;                 // +80
                if (lbSkidMarksEnabled && lfSkidFactor > lfSkidThreshold)
                {
                    mParticleModule.TrailSystem().AddTrailSegment(
                        &lrEmitter,
                        lrWheel.mRoadContact.mPosition,                               // +0
                        lrWheel.mRoadContact.mNormal,                                 // +16
                        static_cast<s8>(ReadS16(lpVfxData, KU_VFX_SKID_MARK_TYPE_ID)),
                        lfSkidFactor,
                        mParticleModule.mRenderData.mfCurrentTime);                   // module +0x8E08
                    lbTrailEnded = false;

                    // [DIAG] NOT IN THE X360 BINARY -- the tyre-mark film latch + telemetry
                    // (BrnDiagFilmLatch.h). This is the ONE place a mark is laid, so it is the
                    // one place that can arm a capture on it and the one place that knows where
                    // the segment went. Five stores; read only by the back-buffer writer under
                    // BRN_FRAME_DUMP_ARM=skid and by frames.csv. DELETE-WHEN-STABLE.
                    BrnDiag::gFilmLatch.muSkidLatched = 1u;
                    ++BrnDiag::gFilmLatch.muTrailSegments;
                    BrnDiag::gFilmLatch.mfLastSegX = lrWheel.mRoadContact.mPosition.x;
                    BrnDiag::gFilmLatch.mfLastSegY = lrWheel.mRoadContact.mPosition.y;
                    BrnDiag::gFilmLatch.mfLastSegZ = lrWheel.mRoadContact.mPosition.z;
                }

                // [skid] both sides of the gate, on the frames it matters.
                if (SkidProbeEnabled())
                {
                    const bool lbWasLaying = (lrEmitter.mrLastTrailTime >= 0.0f);
                    const bool lbNowLaying = !lbTrailEnded;
                    const bool lbEdge      = (lbWasLaying != lbNowLaying);
                    if (lbEdge || (gauSkidProbeFrame % 30u) == 0u)
                    {
                        char lacMsg[400];
                        std::snprintf(lacMsg, sizeof(lacMsg),
                            "[skid] f=%u w=%u %s grnd=%d trac=%d att=%d |drift|=%.5f<=%.5f "
                            "surf=%u/%d ref=%d en=%d skid=%.4f %s thr=%.4f type=%d ready=%d t=%.3f "
                            "pos=%.2f,%.2f,%.2f\n",
                            gauSkidProbeFrame, luWheel,
                            lbEdge ? (lbNowLaying ? "START" : "STOP ") : "     ",
                            lrWheel.mRoadContact.mbIsOnGround ? 1 : 0,
                            lrWheel.mbHasTraction ? 1 : 0,
                            lrWheel.mbAttached ? 1 : 0,
                            static_cast<double>(std::fabs(lfNormalDrift)),
                            static_cast<double>(KF_TRAIL_NORMAL_DRIFT_MAX),
                            luSurfaceId, mSurfaceList.Num_Surfaces(),
                            lbSurfaceResolved ? 1 : 0,
                            lbSkidMarksEnabled ? 1 : 0,
                            static_cast<double>(lfSkidFactor),
                            (lfSkidFactor > lfSkidThreshold) ? ">" : "<=",
                            static_cast<double>(lfSkidThreshold),
                            static_cast<int>(ReadS16(lpVfxData, KU_VFX_SKID_MARK_TYPE_ID)),
                            mParticleModule.TrailSystem().IsReady() ? 1 : 0,
                            static_cast<double>(mParticleModule.mRenderData.mfCurrentTime),
                            static_cast<double>(lrWheel.mRoadContact.mPosition.x),
                            static_cast<double>(lrWheel.mRoadContact.mPosition.y),
                            static_cast<double>(lrWheel.mRoadContact.mPosition.z));
                        CgsDev::Log::WriteToLog(lacMsg);
                    }
                }
            }
            else if (SkidProbeEnabled() && (gauSkidProbeFrame % 30u) == 0u)
            {
                // The wheel is on the ground with traction, but it LEFT ITS CONTACT PLANE --
                // the |dot| <= 0.02 test failed, so no surface is even looked up.
                char lacMsg[240];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[skid] f=%u w=%u DRIFTOUT |drift|=%.5f > %.5f (grnd/trac/att all set)\n",
                    gauSkidProbeFrame, luWheel,
                    static_cast<double>(std::fabs(lfNormalDrift)),
                    static_cast<double>(KF_TRAIL_NORMAL_DRIFT_MAX));
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
        else if (SkidProbeEnabled() && (gauSkidProbeFrame % 30u) == 0u)
        {
            char lacMsg[240];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[skid] f=%u w=%u OFFGATE grnd=%d trac=%d att=%d (no surface lookup)\n",
                gauSkidProbeFrame, luWheel,
                lrWheel.mRoadContact.mbIsOnGround ? 1 : 0,
                lrWheel.mbHasTraction ? 1 : 0,
                lrWheel.mbAttached ? 1 : 0);
            CgsDev::Log::WriteToLog(lacMsg);
        }
        if (lbTrailEnded)
            lrEmitter.mrLastTrailTime = -1.0f;

        lrMachine.SetPreviousPosition(lrWheel.mRoadContact.mPosition);
    }
}

// =============================================================================
// HandleJumpAndLandingEffects  @0x82288068  (DWARF :2921)
//   Tick the car's jump machine; if its jump effect is still the live slot, follow the
//   car (LionEffect::SetTransform == the four-row copy + the CHANGED flag).
// =============================================================================
void EffectsModule::HandleJumpAndLandingEffects(CarState& lrCarState, RaceCarParticleEffectHelper& lrHelper,
                                                f32 /*lfDt*/, f32 /*lfTime*/, f32 /*lfGroundPositionY*/)
{
    ActiveRaceCarData& lrData = *lrHelper.ActiveRaceCar();
    lrData.mJumpMachine.Tick(lrCarState, lrHelper);

    const u32 luHandle = lrData.mJumpEffectHandle;                  // +0x114
    BrnParticle::LionEffect* lpEffect = mParticleModule.GetLionEffect(luHandle);
    if (lpEffect != 0)
        lpEffect->SetTransform(lrHelper.RaceCarState()->mTransform);   // +496
}

// =============================================================================
// HandlePlayerTriangleCache  @0x82296EA0  (DWARF :2083)
// =============================================================================
void EffectsModule::HandlePlayerTriangleCache(const EffectsIO::InputBuffer* lpInputBuffer,
                                              const RaceCarState* lpRaceCarState,
                                              ActiveRaceCarData& lrActiveRaceCar)
{
    const bool lbIsCrashing  = lrActiveRaceCar.IsCrashing();
    const bool lbWasCrashing = lrActiveRaceCar.WasCrashing();
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCache =
        lpInputBuffer->GetTriangleCacheInterface();

    if (lbIsCrashing && !lbWasCrashing)
    {
        const s32 liSlot = lpRaceCarState->miRaceCarID;              // +1088
        const s32 liNumBatches = lpTriangleCache->GetNumCachedTriangleBatches(liSlot);
        const CgsGeometric::Triangle4* lpCache = lpTriangleCache->GetCache(liSlot);
        mCrashTriangleCache.ResetCounters();
        mCrashTriangleCache.AddTriangles(reinterpret_cast<const Triangle4*>(lpCache),
                                         static_cast<u32>(liNumBatches));
    }
    else if (lbIsCrashing)
    {
        const s32 liSlot = lpRaceCarState->miRaceCarID;
        const s32 liNumBatches = lpTriangleCache->GetNumCachedTriangleBatches(liSlot);
        const CgsGeometric::Triangle4* lpCache = lpTriangleCache->GetCache(liSlot);
        mCrashTriangleCache.AddTriangles(reinterpret_cast<const Triangle4*>(lpCache),
                                         static_cast<u32>(liNumBatches));
    }
    else if (lbWasCrashing)
    {
        mCrashTriangleCache.ResetCounters();
        // VariableEventQueue<16384,16>::AllocateEventSafe(&mParticleModule.mInterThreadEventQueue,
        // 0, 0): the "crash triangle cache cleared" post to the dispatch thread. NOT
        // RECONSTRUCTED: the inter-thread queue is a placeholder (its consumer is the Lion
        // dispatch pass). Announced once.
        static bool sbLogged = false;
        LogNotReconstructed(sbLogged,
            "HandlePlayerTriangleCache's inter-thread 'cache cleared' post (ParticleModule::mInterThreadEventQueue is a placeholder)");
    }
}

// =============================================================================
// HandleGameActions  @0x82296FD8  (DWARF :2206)
// =============================================================================
void EffectsModule::HandleGameActions(const CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue,
                                      const EffectsIO::InputBuffer* lpInputBuffer)
{
    using namespace BrnGameState::GameStateModuleIO;

    CGS_ASSERT(lpGameActionQueue != 0, "lpGameActionQueue != NULL");   // EffectsModule.cpp:2312

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpGameActionQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != 0)
    {
        switch (liType)
        {
        case E_ACTION_COMPLETED_STUNT:   // 15
        {
            const CompletedStuntAction* lpAction = reinterpret_cast<const CompletedStuntAction*>(lpEvent);
            if ((lpAction->muStuntActionComplete & KU_STUNT_COMPLETE_SLIPSTREAM) == KU_STUNT_COMPLETE_SLIPSTREAM)
            {
                if (muSlipStreamEffectHandle != BrnParticle::LionEffect::KU_HANDLE_INVALID)
                {
                    BrnParticle::LionEffect* lpEffect = mParticleModule.GetLionEffect(muSlipStreamEffectHandle);
                    mParticleModule.StopLionEffect(lpEffect);
                    muSlipStreamEffectHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
                }
            }
            break;
        }
        case E_ACTION_INPROGRESS_STUNT:  // 16
        {
            const InProgressStuntActionX360* lpAction = reinterpret_cast<const InProgressStuntActionX360*>(lpEvent);
            if ((lpAction->muStuntActionInProgress & KU_STUNT_IN_PROGRESS_SLIPSTREAM) == KU_STUNT_IN_PROGRESS_SLIPSTREAM
                && lpAction->miCarInFrontIndex != -1)
            {
                const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars = lpInputBuffer->GetActiveRaceCarInterface();
                CGS_ASSERT(lpActiveRaceCars != 0, "Cannot find the RaceCarInterface");        // :2381
                CGS_ASSERT(lpAction->miCarInFrontIndex < static_cast<s32>(KU_NUM_ACTIVE_RACE_CARS), "Invalid RaceCarIndex"); // :2383
                const EActiveRaceCarIndex leInFront = static_cast<EActiveRaceCarIndex>(lpAction->miCarInFrontIndex);
                if (lpActiveRaceCars->IsRaceCarActive(leInFront))
                {
                    const RaceCarState* lpState = lpActiveRaceCars->GetRaceCarState(leInFront);
                    CGS_ASSERT(lpState != 0, "Cannot find the RaceCarState for the Car in front of the player in the convoy"); // :2388
                    HandleConvoySlipStream(lpAction->mfSlipStreamBlend, 0u, lpState->mTransform);
                }
                else if (muSlipStreamEffectHandle != BrnParticle::LionEffect::KU_HANDLE_INVALID)
                {
                    BrnParticle::LionEffect* lpEffect = mParticleModule.GetLionEffect(muSlipStreamEffectHandle);
                    mParticleModule.StopLionEffect(lpEffect);
                    muSlipStreamEffectHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
                }
            }
            break;
        }
        case E_ACTION_PREPARE_FOR_MODE:  // 23: the mode type word at record +376
        {
            const PrepareForModeAction* lpAction = reinterpret_cast<const PrepareForModeAction*>(lpEvent);
            meCurrentGameMode = lpAction->GetGameModeParams()->GetGameModeType();
            break;
        }
        case E_ACTION_START_MODE_INTRO:  // 29
            mbEventIntroActive = true;
            break;
        case E_ACTION_STOP_MODE_INTRO:   // 30
            mbEventIntroActive = false;
            break;
        case E_ACTION_STOP_MODE:         // 39
            meCurrentGameMode = E_MODE_NONE;
            mCrashTriangleCache.ResetCounters();
            break;
        case E_ACTION_JUST_BOUNCED:      // 144
            HandleShowtimeTrafficBounce(lpEvent, lpInputBuffer);
            break;
        default:
            break;
        }
        const CgsModule::Event* lpNext = 0;
        liType = lpGameActionQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
}

// =============================================================================
// HandleConvoySlipStream  @0x822926C8   (called by HandleGameActions)
//   Drive the convoy slip-stream LION effect: lazily start it the first time (keying
//   KAC_SLIPSTREAM_EFFECT through ParticleDescription::HashString), then each call
//   re-point its world transform, clamp the passed blend to <= 1.0, flag the slot
//   changed and store the blend. r4 (an int/bool) is passed in the ABI but never read.
// =============================================================================
void EffectsModule::HandleConvoySlipStream(f32 lfBlend, u32 luUnused,
                                           const rw::math::vpu::Matrix44Affine& lrTransform)
{
    (void)luUnused;

    if (muSlipStreamEffectHandle == BrnParticle::LionEffect::KU_HANDLE_INVALID)
    {
        const u32 luNameHash = BrnParticle::ParticleDescription::HashString(KAC_SLIPSTREAM_EFFECT);
        muSlipStreamEffectHandle = mParticleModule.StartLionEffect(luNameHash, KAC_SLIPSTREAM_EFFECT, 0);
    }

    const u32 luHandle = muSlipStreamEffectHandle;
    CGS_ASSERT((luHandle & 0x7Fu) < BrnParticle::ParticleModule::KU_MAX_PLAYING_EFFECTS,
               "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
    BrnParticle::LionEffect* lpEffect = mParticleModule.GetLionEffect(luHandle);
    CGS_ASSERT(lpEffect != 0, "Lion Effect is NULL when it shouldn't be (SlipStream Effect)");

    lpEffect->mTransform = lrTransform;
    const f32 lfClampedBlend = (lfBlend >= 1.0f) ? 1.0f : lfBlend;   // fsel(blend - 1, 1, blend)
    lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
    lpEffect->mfStateBlend = lfClampedBlend;
}

// =============================================================================
// GenerateDispatchLists  @0x82296668  (DWARF :1345) -- once per frame from DoDispatch.
//   GenerateRenderRequests (the post-fx frames), then the particle dispatch input
//   ("Particles" on the input stack) is filled from the EFFECTS dispatch input -- the
//   dispatch frame, the key light direction / colour, the average irradiance, the
//   environment map, the white level -- and the particle module publishes its render
//   data into the dispatch-thread input buffer under that buffer's write lock.
// =============================================================================
void EffectsModule::GenerateDispatchLists(CgsModule::IOBufferStack* lpInputBufferStack,
                                          const EffectsIO::DispatchInputBuffer* lpDispatchInputBuffer,
                                          BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer)
{
    typedef BrnParticle::ParticleIO::DispatchInputBuffer ParticleDispatchInput;

    GenerateRenderRequests(lpDispatchInputBuffer);

    ParticleDispatchInput* lpParticleInput = 0;
    const bool lbCreated = lpInputBufferStack->CreateIOBuffer(&lpParticleInput, "Particles");
    CGS_ASSERT(lbCreated, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");   // CgsModuleIOHelper.h:52
    (void)lbCreated;

    if (lpDispatchInputBuffer != 0)
    {
        lpParticleInput->SetDispatchFrame(lpDispatchInputBuffer->GetDispatchFrame());         // +0x10 -> +0x04
        lpParticleInput->SetKeyLightDirection(lpDispatchInputBuffer->GetKeyLightDirection());   // +0x20 -> +0x10
        lpParticleInput->SetKeyLightColour(lpDispatchInputBuffer->GetKeyLightColour());         // +0x30 -> +0x20
        lpParticleInput->SetAverageIrradianceColour(
            lpDispatchInputBuffer->GetAverageIrradianceColour());                              // +0x40 -> +0x30
        lpParticleInput->SetEnvironmentMap(lpDispatchInputBuffer->GetEnvironmentMap());        // -> +0x40
        lpParticleInput->SetWhiteLevel(lpDispatchInputBuffer->GetWhiteLevel());                // -> +0x44
    }
    else
    {
        // FLAG PC bring-up: no BrnEffects::EffectsIO::DispatchInputBuffer exists on this build
        // yet (BridgeRendererToEffects @0x823C1168 is not reconstructed -- see the DoDispatch
        // banner), so the particle dispatch input keeps its Construct state: zero lights, no
        // environment map, white level 1.0 (the identity for the trail colour scale).
        static bool sbLogged = false;
        LogNotReconstructed(sbLogged,
            "the effects DISPATCH input (BridgeRendererToEffects @0x823C1168); the particle dispatch input keeps "
            "zero key light / no env map / white level 1.0");
        lpParticleInput->SetWhiteLevel(1.0f);
    }

    lpParticleInput->LockForRead();
    lpDispatchThreadInputBuffer->LockForWrite();
    mParticleModule.GenerateRenderRequests(lpParticleInput, lpDispatchThreadInputBuffer);
    lpDispatchThreadInputBuffer->UnlockForWrite();
    lpParticleInput->UnlockForRead();

    const bool lbDestroyed = lpInputBufferStack->DestroyIOBuffer(&lpParticleInput);
    CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");   // CgsModuleIOHelper.h:57
    (void)lbDestroyed;
}

// =============================================================================
// GenerateRenderRequests  @0x8227FF10  (DWARF :1197) -- the post-fx effects frames.
//   NOT RECONSTRUCTED: the base-frame / FX-events BrnEffectsFrame production (depth of
//   field, B4 blur, motion blur, the colour-cube tint layers) that reads the effects
//   dispatch input's camera and the TempRaceCarStateCache. The renderer's base-frame
//   bring-up producer (BrnRendererModule::PCBringUpSetCameraInput /
//   PCBringUpSetRaceCarStateCache, fed from BrnGameModule::DoDispatch) stands in for it
//   on this build; the effects dispatch input it would read does not exist here either.
// =============================================================================
void EffectsModule::GenerateRenderRequests(const EffectsIO::DispatchInputBuffer* /*lpDispatchInputBuffer*/)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged,
        "EffectsModule::GenerateRenderRequests @0x8227FF10 (the post-fx effects frames; the renderer's "
        "base-frame bring-up producer stands in)");
}

// =============================================================================
// The arms OFF the tyre-mark path -- each announces itself once, then returns.
// =============================================================================
void EffectsModule::HandleCrashingTrail(ActiveRaceCarData& /*lrActiveRaceCar*/, f32 /*lfDt*/, f32 /*lfTime*/,
                                        const RaceCarState* /*lpRaceCarState*/, EActiveRaceCarIndex /*leIndex*/)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::HandleCrashingTrail @0x82290D30 (the crash debris trail)");
}

void EffectsModule::ProcessCarContactQueues(const EffectsModuleParams& /*lrParams*/,
                                            const RCEntityActiveRaceCarOutputInterface* /*lpActiveRaceCars*/,
                                            const BrnPhysics::ContactSpy::ContactSpyInterface* /*lpContactSpy*/,
                                            const BrnDirector::Camera::Camera* /*lpCamera*/)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged,
        "EffectsModule::ProcessCarContactQueues @0x8229B7F8 (ProcessRaceCarContacts / "
        "ProcessCarDetatchedPartContacts / ProcessHingedPartContacts -- the contact sparks)");
}

void EffectsModule::HandleGlassSmashEventsForAllCars(const EffectsIO::InputBuffer* /*lpInputBuffer*/,
                                                     const RCEntityActiveRaceCarOutputInterface* /*lpActiveRaceCars*/,
                                                     f32 /*lfDt*/, f32 /*lfTime*/)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::HandleGlassSmashEventsForAllCars @0x82297420 (the glass smash VFX)");
}

// HandleQADebugTests @0x82291700: the ONE non-debug effect it has -- consuming the
// RestartEffects latch (byte_82FAB694): when the effects debug component is not enabled
// the particle module's simulation rate is put back to 1.0 -- is reconstructed; the QA
// test-effect spawning behind dword_82FAD270 is announced, not performed.
void EffectsModule::HandleQADebugTests(f32 /*lfDt*/, f32 /*lfTime*/, const RaceCarState* /*lpRaceCarState*/)
{
    if (sbRestartEffects)
    {
        sbRestartEffects = false;
        if (!mDebugComponent.IsEnabled())                 // +0x2C34D
            mParticleModule.mfSimulationRate = 1.0f;      // +0x23B70
    }
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::HandleQADebugTests @0x82291700 (the QA test-effect spawns)");
}

void EffectsModule::HandleShowtimeTrafficBounce(const void* /*lpJustBouncedAction*/,
                                                const EffectsIO::InputBuffer* /*lpInputBuffer*/)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::HandleShowtimeTrafficBounce @0x82292808 (the showtime bounce VFX)");
}

void EffectsModule::JunkyardVfxStart(Vector3 /*lvCameraPosition*/)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::JunkyardVfxStart @0x82291AE8 (the junkyard VFX editor session)");
}

void EffectsModule::JunkyardVfxStop()
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::JunkyardVfxStop @0x82292028 (the junkyard VFX editor session)");
}

} // namespace BrnEffects
