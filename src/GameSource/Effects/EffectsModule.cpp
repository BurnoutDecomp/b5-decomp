#include "GameSource/Effects/EffectsModule.h"

#include "GameSource/Effects/ParticleEffectHelper.h"                               // ParticleEffectHelper / RaceCarParticleEffectHelper
#include "GameSource/Effects/BrnEffectsUtils.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h"   // [diag] BRN_SIMPLEFX_DIAG                                  // Utils::Vector3Randomiser / Vector4Randomiser (the crash dust)
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
#include "GameSource/AttribSys/Generated/classes/physicssurface.h"                 // [diag] Attrib::Gen::physicssurface (the [skid-bind] material line)
#include "GameSource/AttribSys/Generated/classes/nativeparticleparams.h"           // Attrib::Gen::nativeparticleparams (LoadNativeParticleParams)
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"                  // Attrib::FindCollection
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "GameSource/Graphics/PostFx/BrnPostFx.h"                                  // msPostFx (the colour-cube seed)
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"                           // gBrnWorldShaderConstantsFrameBringUp -- the live world white level
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"                          // BrnGame::DispatchThreadInputBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                         // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Development/BrnDiagFilmLatch.h"                   // [diag] BRN_FRAME_DUMP_ARM=skid
#include "GameShared/GameClasses/Development/BrnDiagTrailHeight.h"                 // [diag] BRN_TRAIL_HEIGHT_DIAG (issue #21)
#include "rw/math/vpu/vector3_operation.h"                                         // rw::math::vpu::{operator-, Dot}

#include <cmath>    // std::fabs
#include <cstring>  // std::memcpy / memset (the replayed contact, the recorded table)
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
// BrnSimpleParticleArray::UpdateParams has no body) and the prop-locator VFX.
// ⚠️ CORRECTED 2026-09-06: this list used to end "...and the spark parameter copies into the
// particle module's (placeholder) spark arrays". Both halves went stale in this wave --
// maSparks[4] is no longer a placeholder (ParticleModule.h:668) and PushSparkParams below is
// bodied, calling maSparks[i].UpdateParams for all four banks. None of them is
// a trap: a CGS_ASSERT in HandleCrashingTrail or JunkyardVfxStart would kill every crash
// and every junkyard boot on the shared box; none is silent either -- each writes ONE
// `[effects] NOT RECONSTRUCTED: ...` line to BrnGame.log so a run that needed the arm says so.
// ============================================================================

namespace BrnEffects
{
// ------------------------------------------------------------------------------------------------
// The spark-shower parameter blocks (FX-CRASHVFX 2026-09-24). DWARF EffectsModule.cpp:119 / :137.
// Every shower the effects module fires -- world grinding, vehicle grinding, crashing, the showtime
// bounce, the jump landing -- is one SparkShowerController: a SMALL and a LARGE argument set that
// DoSparkShower lerps between by the shower's size, plus the three scalars it passes through. The
// five const instances are CRT-initialised .data (the image holds zeros at their addresses); their
// values below were read by RUNNING each init thunk on the emulator
// (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/crtinit.py), thunk address cited per instance.
// ------------------------------------------------------------------------------------------------
struct SparkShowerArgs
{
    Vector4 mLateralAngleMinMaxForwardAngleMinMax;        // +0x00  DEGREES (converted in the accessor)
    Vector4 mSpawnVelocityMinMaxInheritedVelocityMinMax;  // +0x10
    Vector4 mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ;    // +0x20
};

struct SparkShowerController
{
    SparkShowerArgs                    mSmallArgs;                     // +0x00  :194
    SparkShowerArgs                    mLargeArgs;                     // +0x30  :195
    f32                                mfReflectionAmount;             // +0x60  :200
    f32                                mfVelocityScaleSpeedThreshold;  // +0x64  :204
    BrnParticle::Native::ESparkArrayID meSparkArrayId;                 // +0x68  :206

    // :143 / :154 / :165 -- each a lane-wise `vsubfp` (large - small) then ONE `vmaddfp` by the
    // splatted size (DoSparkShower 0x82292108..0x82292140); the angles then go to radians
    // (`vmulfp128 v1, v0, v11`, v11 = K_VECFLOAT_DEGREES_TO_RADIANS).
    Vector4 GetLateralAngleMinMaxForwardAngleMinMax(VecFloat lvSize) const;
    Vector4 GetSpawnVelocityMinMaxInheritedVelocityMinMax(VecFloat lvSize) const;
    Vector4 GetSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ(VecFloat lvSize) const;
    f32 GetReflectionAmount() const                            { return mfReflectionAmount; }             // :174
    f32 GetVelocitySpeedScaleThreshold() const                 { return mfVelocityScaleSpeedThreshold; }  // :180
    BrnParticle::Native::ESparkArrayID GetSparkArrayId() const { return meSparkArrayId; }                 // :186
};

namespace
{
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface RCEntityActiveRaceCarOutputInterface;
    typedef ::EActiveRaceCarIndex                                                EActiveRaceCarIndex;
    typedef BrnPhysics::Vehicle::RaceCarState                                    RaceCarState;
    typedef BrnPhysics::Vehicle::WheelLite                                       WheelLite;

    // [FLAG PC witness] NOT CONSOLE BEHAVIOUR: ours, log-only. Hard first-N ceiling for the opt-in witnesses in
    // this TU, so an armed knob still cannot flood the log. DELETE-WHEN those witnesses go.
    const u32 KU_EFFECTS_DIAG_MAX_LINES = 128u;

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

    // ---- the contact-spark drains (2026-09-06) -------------------------------------------------
    // unk_8200D990 == FLT_EPSILON -- the constant RwMathVPU::IsZero compares |lane| against, in
    // the two HandleSparkContacts asserts (EffectsModule.cpp:1578 / :1579). The console's vector
    // form masks the sign bit off the whole quad and then `vrlimi128 v12, v0, 1, 1` overwrites the
    // w lane with a copy of x (rotate-by-one, mask w) so the pad lane cannot vote; the compare
    // that follows is over x, y AND z -- a THREE-lane test, the same idiom this tree reads in
    // BrnCrashModeScoring's IsVectorSet and BrnPropZoneManager's first-move gate.
    const f32 KF_RWMATH_IS_ZERO_EPSILON     = 1.1920928955078125e-07f;
    // flt_8200DD18 -- ProcessHingedPartContacts' friction-stress floor, used BOTH as its own
    // pre-filter and as the threshold it hands HandleSparkContacts.
    const f32 KF_HINGED_MIN_FRICTION_STRESS = 7.5f;
    // flt_8200DD14 -- the same role in ProcessCarDetatchedPartContacts' spark arm.
    const f32 KF_DETACHED_MIN_FRICTION_STRESS = 0.004999999888241291f;
    // flt_82004A20 / flt_82001DA0 -- the two |mVelocity| gates the detached-part drain uses to
    // pick its arm: above 10 m/s a spark contact, above 0.5 m/s (and part type 91) a dust burst.
    const f32 KF_DETACHED_SPARK_MIN_SPEED   = 10.0f;
    const f32 KF_DETACHED_DUST_MIN_SPEED    = 0.5f;
    // ⭐ THE TWO EBodyParts IDS ProcessHingedPartContacts SPARKS ON ARE THE TWO EXHAUSTS
    // (`cmpwi r11, 0x54` / `0x55`). That is not this wave's inference: BrnDeformableObject_
    // Detach.cpp:188 already names 84/85 KI_BODY_PART_STRUCTURAL_A/B == "exhaust A/B", pinned
    // by the counter its own hinge logic guards being miNumAttachedExhausts. So this drain is
    // the EXHAUST-SCRAPE spark path specifically, not a general body-part one -- which is why
    // a wall-crash sweep measured 1,756 hinged contacts and ZERO past this filter: the parts
    // that ground were bumper / bonnet / grille. The general deformable-part grind is
    // ProcessCarDetatchedPartContacts' spark arm below, and that one does fire.
    const s32 KI_HINGED_SPARK_PART_EXHAUST_A = 84;
    const s32 KI_HINGED_SPARK_PART_EXHAUST_B = 85;
    // The one EBodyParts id ProcessCarDetatchedPartContacts routes to DUST instead of sparks
    // (`cmpwi cr6, r11, 0x5B`). Same FLAG as the pair above: the value is the console's.
    const s32 KI_DETACHED_DUST_PART_TYPE    = 91;
    // ---- the detached-part DUST arm (ProcessCarDetatchedPartContacts 0x82293150..0x8229333C) ----
    // flt_820054CC -- particles per second of scraping: the part's accumulator gains mDt * 20.0
    // each frame (`fmadds f0, f0, f26, f13`, f26 loaded at 0x8229304C).
    const f32 KF_DETACHED_DUST_PER_SECOND   = 20.0f;
    // flt_8200DD28 / flt_8200DD24 -- every velocity lane's jitter is Random() * 6.0 - 3.0
    // (`fmsubs f13, f13, f30, f29`: f30 at 0x82293000, f29 at 0x82293008), uniform in [-3, 3).
    const f32 KF_DETACHED_DUST_JITTER_SPAN  = 6.0f;
    const f32 KF_DETACHED_DUST_JITTER_HALF  = 3.0f;
    // flt_82001DA0 / flt_8200DD40 -- the size scale is Random() * 0.5 + 0.2
    // (`fmadds f1, f0, f28, f27`: f28 at 0x82293054 -- the same cell as the 0.5 m/s speed gate --
    // f27 at 0x82293010).
    const f32 KF_DETACHED_DUST_SIZE_RANGE   = 0.5f;
    const f32 KF_DETACHED_DUST_SIZE_MIN     = 0.2f;
    // flt_82001C98 -- the alpha every dust particle is spawned with (`fmr f3, f31`, f31 at 0x82293028).
    const f32 KF_DETACHED_DUST_ALPHA        = 1.0f;

    // The console's inline floor + fctiwz (0x82293170..0x82293194): `fsel` on the sign picks
    // -2^52 or +2^52 (dbl_82001CB8 / dbl_82001CB0), the subtract-then-add rounds to an integer,
    // a second `fsel` takes 1.0 (dbl_82001CA0) or 0.0 (dbl_82001CA8) off when that rounded up,
    // then frsp and fctiwz. That is floor() for every finite input. fctiwz SATURATES and turns a
    // NaN into 0x80000000; both are spelled out so the conversion is defined on the host too
    // (a NaN accumulator therefore spawns nothing, as on the console: 0x80000000 <= 0).
    s32 FloorToS32Fctiwz(f32 lfValue)
    {
        const f32 lfFloor = static_cast<f32>(std::floor(static_cast<f64>(lfValue)));
        if (lfFloor != lfFloor)
            return static_cast<s32>(0x80000000u);
        if (lfFloor >= 2147483648.0f)
            return 0x7FFFFFFF;
        if (lfFloor < -2147483648.0f)
            return static_cast<s32>(0x80000000u);
        return static_cast<s32>(lfFloor);
    }

    // ---- ProcessRaceCarContacts @0x82297C08 and its callees (FX-CRASHVFX 2026-09-24) ----------
    // K_VECFLOAT_DEGREES_TO_RADIANS (DWARF :309) == splat(0x3C8EFA35), unk_82FAC1F0 <- CRT thunk
    // 0x82C4A918. K_VECFLOAT_EPSILON (:310) == splat(0x3A83126F = 0.001), unk_82FAC390 <- thunk
    // 0x82C4A940. K_VECTOR3_1_0_1 (:311) == (1, 0, 1, 0), unk_82FAC200 <- thunk 0x82C4A968.
    const f32 KF_DEGREES_TO_RADIANS = 0.0174532924f;
    const f32 KF_VECFLOAT_EPSILON   = 0.00100000005f;
    // 0x82181500 / 0x82181510 -- the identity rows (1,0,0,0) / (0,1,0,0) the world-grinding
    // shower crosses the contact normal with (rw::math::vpu::detail::gIVector's first two rows).
    // flt_8201371C -- the grinding-spark rate wobble's angular frequency: cos(time * 0.8 pi).
    const f32 KF_GRINDING_WOBBLE_FREQUENCY = 2.51327419f;
    // flt_8200DD4C / flt_820054CC -- the wobble's amplitude and centre: rate factor cos*5 + 20.
    const f32 KF_GRINDING_WOBBLE_AMPLITUDE = 5.0f;
    const f32 KF_GRINDING_WOBBLE_CENTRE    = 20.0f;
    // flt_82013720 -- 4.4694443 m/s (10 mph): the slowest car that grinds sparks (world and
    // vehicle), and the zero of the crash shower's size ramp.
    const f32 KF_GRINDING_MIN_SPEED = 4.46944427f;
    // flt_82013278 -- 6.7041669 m/s (15 mph): the crash shower's tangential-speed floor.
    const f32 KF_CRASH_SHOWER_MIN_TANGENTIAL_SPEED = 6.70416689f;
    // flt_820138A4 -- the crash shower's size slope: (|vt| - 4.4694443) * 0.0559353642.
    const f32 KF_CRASH_SHOWER_SIZE_SLOPE = 0.0559353642f;
    // flt_820138A0 / flt_82005574 -- the next crash shower comes Random() * 0.13000001 + 0.02 s later.
    const f32 KF_CRASH_SHOWER_INTERVAL_RANGE = 0.13000001f;
    const f32 KF_CRASH_SHOWER_INTERVAL_MIN   = 0.0199999996f;
    // flt_82006530 / flt_8201387C -- the crash shower's spark count: size * 150 + 100.5 (fctidz).
    const f32 KF_CRASH_SHOWER_COUNT_RANGE = 150.0f;
    const f32 KF_CRASH_SHOWER_COUNT_BASE  = 100.5f;
    // flt_82002138 -- a surface's grinding scale (visualfxsurface +0x50) must be above 0.01, and a
    // debris burst's speed-ramped scale too.
    const f32 KF_MIN_EFFECT_SCALE = 0.00999999978f;
    const u32 KU_VFX_GRINDING_SCALE = 0x50;   // f32 (`lfs f27, 0x50(r11)`)
    // flt_8200DD4C -- a crashing car sheds impact dust above 5 m/s ...
    const f32 KF_CRASH_DUST_MIN_SPEED = 5.0f;
    // ... flt_82005548 -- at 2.5 particles per metre it travels while the contact lasts.
    const f32 KF_CRASH_DUST_PER_METRE = 2.5f;
    // The dust's two randomisers, built on the stack at 0x82298954..0x822989B8: position in the
    // box [point - (1, 0, 1), point + (1, 1, 1)], velocity lanes in [(-6, 1, -6), (6, 4, 6)] with a
    // w lane in [0.05, 0.15] -- the share of the car's own velocity each particle inherits.
    // (f31 1.0, f29 0.0, f20 -6.0 = flt_820138A8, f21 6.0 = flt_8200DD28, f19 4.0 = flt_82004EF4,
    //  0.05 = flt_820047C8, 0.15 = flt_82004E58.)
    const f32 KF_CRASH_DUST_BOX_BELOW_X   = 1.0f;
    const f32 KF_CRASH_DUST_BOX_BELOW_Y   = 0.0f;
    const f32 KF_CRASH_DUST_BOX_BELOW_Z   = 1.0f;
    const f32 KF_CRASH_DUST_BOX_ABOVE     = 1.0f;
    const f32 KF_CRASH_DUST_VELOCITY_MIN_XZ = -6.0f;
    const f32 KF_CRASH_DUST_VELOCITY_MIN_Y  = 1.0f;
    const f32 KF_CRASH_DUST_VELOCITY_MAX_XZ = 6.0f;
    const f32 KF_CRASH_DUST_VELOCITY_MAX_Y  = 4.0f;
    const f32 KF_CRASH_DUST_INHERIT_MIN   = 0.0500000007f;
    const f32 KF_CRASH_DUST_INHERIT_MAX   = 0.150000006f;
    // flt_820138AC / flt_8200DD44 -- dust size Random() * 0.400000036 + 0.8 (the 0.4 is the image's
    // own 0x3ECCCCCE, i.e. 1.2f - 0.8f folded in single precision, NOT 0.4f).
    const f32 KF_CRASH_DUST_SIZE_RANGE = 0.400000036f;
    const f32 KF_CRASH_DUST_SIZE_MIN   = 0.800000012f;
    // flt_82001C98 -- the dust's alpha (`fmr f3, f31`).
    const f32 KF_CRASH_DUST_ALPHA = 1.0f;
    // flt_8200DD40 -- a debris burst starts 0.2 of the contact normal out from the contact point.
    const f32 KF_DEBRIS_BURST_NORMAL_OFFSET = 0.200000003f;
    // flt_82001DA0 -- a random draw below 0.5 hands a takedown's debris to the lower-index car.
    const f32 KF_TAKEDOWN_DEBRIS_COIN = 0.5f;

    // The debrisparams layout words HandleBurstDebris / ProcessRaceCarContacts read. ⚠ FLAG -- the
    // names are the ROLES the two bodies give them (a speed -> scale ramp in two linear pieces and
    // the re-arm interval); AttribSys keys by hash and the class header carries no accessors.
    const u32 KU_DEBRIS_EMITTER_HALF_EXTENTS = 0x00;   // Vector4  (`lvx128 v3, r0, r11`)
    const u32 KU_DEBRIS_BURST_INTERVAL       = 0x84;   // f32      (the caller's `lfs f0, 0x84(r11)`)
    const u32 KU_DEBRIS_SCALE_AT_MIN_SPEED   = 0xB8;   // f32
    const u32 KU_DEBRIS_MIN_SPEED            = 0xBC;   // f32  (below it: no burst)
    const u32 KU_DEBRIS_SCALE_AT_MID_SPEED   = 0xC0;   // f32
    const u32 KU_DEBRIS_MID_SPEED            = 0xC4;   // f32
    const u32 KU_DEBRIS_SCALE_AT_MAX_SPEED   = 0xC8;   // f32
    const u32 KU_DEBRIS_MAX_SPEED            = 0xCC;   // f32  (speeds above clamp to it)

    // The shower controllers (DWARF :209..:289). Showtime-bounce and jump-sparks land with their
    // own callers (items 5 and the jump pass).
    // gSparkShowerControllerWorldGrinding (:229) -- unk_82CDB090 <- thunk 0x82C4A398.
    const SparkShowerController gSparkShowerControllerWorldGrinding =
    {
        { { 2.0f, 15.0f, -10.0f, 5.0f }, { 4.0f,  6.0f, 0.800000012f, 1.20000005f }, { 0.5f,  1.25f, 0.0f, 0.100000001f } },
        { { 2.0f, 15.0f, -10.0f, 5.0f }, { 8.0f, 12.0f, 0.800000012f, 1.20000005f }, { 0.75f, 1.75f, 0.0f, 0.600000024f } },
        0.899999976f, 44.6944427f, BrnParticle::Native::eSparkArray_GrindingWorld
    };
    // gSparkShowerControllerVehicleGrinding (:249) -- unk_82CDB100 <- thunk 0x82C4A4E8.
    const SparkShowerController gSparkShowerControllerVehicleGrinding =
    {
        { { -15.0f, 15.0f, -10.0f, 5.0f }, { 4.0f,  6.0f, 0.800000012f, 1.20000005f }, { 0.5f,  1.25f, 0.100000001f, 0.100000001f } },
        { { -15.0f, 15.0f, -10.0f, 5.0f }, { 8.0f, 12.0f, 0.800000012f, 1.20000005f }, { 0.75f, 1.75f, 0.100000001f, 0.300000012f } },
        0.0f, 44.6944427f, BrnParticle::Native::eSparkArray_GrindingRaceCars
    };
    // gSparkShowerControllerCrashing (:269) -- unk_82CDB170 <- thunk 0x82C4A630.
    const SparkShowerController gSparkShowerControllerCrashing =
    {
        { { 2.0f, 10.0f, -180.0f, 180.0f }, { 5.0f, 12.0f, 0.600000024f, 1.0f },         { 0.5f,  1.25f, 0.0f, 0.100000001f } },
        { { 2.0f, 15.0f, -180.0f, 180.0f }, { 8.0f, 20.0f, 0.600000024f, 1.20000005f }, { 0.75f, 1.75f, 0.0f, 0.200000003f } },
        2.0f, 44.6944427f, BrnParticle::Native::eSparkArray_Crashing
    };

    // DWARF EffectsModule.cpp:66 -- CB4SparkSpawnParams, and its one instance
    // _gSparkSpawnParamsRaceCarVehicle (:104): IMAGE-initialised .data at 0x82CDB3E4 (no CRT
    // store site), read straight out of the image: 6.7041669 / 44.694443 m/s (15 / 100 mph),
    // 90 / 360 sparks per second.
    struct CB4SparkSpawnParams
    {
        f32 mrInputVelocityMin;       // :69
        f32 mrInputVelocityMax;       // :70
        f32 mrSpawnCountPerSecMin;    // :71
        f32 mrSpawnCountPerSecMax;    // :72

        // :77, inlined into HandleRaceCarRaceCarSparks (0x82290AC4..0x82290B18): nothing at or
        // below the minimum (a NaN speed fails `bgt` too) -- and then no dt multiply either; the
        // maximum rate at or above the maximum; a linear ramp (one fmadds) in between.
        f32 VelocityToSpawnCount(f32 lfVelocity, f32 lfDt) const
        {
            if (!(lfVelocity > mrInputVelocityMin))
                return 0.0f;
            f32 lfPerSecond = mrSpawnCountPerSecMax;
            if (lfVelocity < mrInputVelocityMax)
            {
                const f32 lfT = (lfVelocity - mrInputVelocityMin) / (mrInputVelocityMax - mrInputVelocityMin);
                lfPerSecond = std::fma(lfT, mrSpawnCountPerSecMax - mrSpawnCountPerSecMin, mrSpawnCountPerSecMin);
            }
            return lfPerSecond * lfDt;
        }
    };
    const CB4SparkSpawnParams gSparkSpawnParamsRaceCarVehicle = { 6.70416689f, 44.6944427f, 90.0f, 360.0f };

    // byte_82CDB40D -- HandleRaceCarRaceCarSparks' function-local `static bool8_t lbDisableThisEffect`
    // (DecFIGS DWARF EffectsModule.cpp:1580). ⛔ IT IS TRUE, SO THE FUNCTION NEVER POSTS ON THE CONSOLE.
    // The byte is INITIALISED .data holding 0x01 (IDA flag word 0x00009501: FF_IVL set, value 1 --
    // not .bss; its calibrated neighbour _gSparkSpawnParamsRaceCarVehicle @0x82CDB3E4 reads
    // 6.7041669 / 44.694443 / 90 / 360 out of the same page). Nothing can change it: findinit.py
    // finds one site, the `lbz` at 0x82290A60, and a whole-image scan for the pointer value
    // 0x82CDB40D finds none (no tweakable table holds it). The Remaster agrees: BurnoutPR.exe's
    // ProcessRaceCarContacts (sub_9823E0) calls HandleVehicleVehicleSparks in the race-car and
    // traffic arms and NOTHING after it -- LTCG folded the never-written static to true and
    // dropped the call. The body below is still the console's, bit for bit; it just never runs.
    const bool KB_RACE_CAR_SPARKS_DISABLED = true;

    // The vector idioms these bodies are built from, spelled the console's way so the results are
    // its results to the bit (tests/run_fxcrashvfx_race_car_contacts.py runs them against the real
    // words):
    //   Dot3            `vmsum3fp128` -- FLAG (model): taken as ONE rounding of the exact sum. Two
    //                   f32 products are exact in f64 and their f64 sum rounds far below f32's ulp.
    //   Vnmsub          `vnmsubfp` / `vnmsubfp128`: -(a*c - b), rounded ONCE and then NEGATED --
    //                   zeros included, so an exact cancellation is -0 where std::fma(-a, c, b)
    //                   gives +0 (PowerPC: the fmsub result, negated; a QNaN keeps its sign).
    //   RefinedRsqrt    `vrsqrtefp` + two Newton-Raphson steps, each `vmulfp` est*est, `vmulfp`
    //                   est*0.5, `vnmsubfp` 1 - x*est^2 (fused), `vmaddfp` est + half*r (fused).
    //                   FLAG (model): the hardware estimate is modelled as its correctly rounded
    //                   value; the two refinements then pin the result.
    //   RefinedRecip    `vrefp` + two steps of `vnmsubfp` 1 - est*x / `vmaddfp` est + est*r.
    //   GuardedLength3  x * RefinedRsqrt(x) with the `vcmpeqfp`/`vsel` that maps x == 0 to 0.
    f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return static_cast<f32>(static_cast<f64>(lrA.x) * lrB.x + static_cast<f64>(lrA.y) * lrB.y
                              + static_cast<f64>(lrA.z) * lrB.z);
    }

    f32 Vnmsub(f32 lfA, f32 lfC, f32 lfB)
    {
        const f32 lfDifference = std::fma(lfA, lfC, -lfB);
        return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
    }

    f32 RefinedRsqrt(f32 lfX)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfX)));
        for (u32 luStep = 0; luStep < 2u; ++luStep)
        {
            const f32 lfSquared  = lfEstimate * lfEstimate;
            const f32 lfHalf     = lfEstimate * 0.5f;
            const f32 lfResidual = Vnmsub(lfX, lfSquared, 1.0f);
            lfEstimate = std::fma(lfHalf, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    f32 RefinedRecip(f32 lfX)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / static_cast<f64>(lfX));
        for (u32 luStep = 0; luStep < 2u; ++luStep)
        {
            const f32 lfResidual = Vnmsub(lfEstimate, lfX, 1.0f);
            lfEstimate = std::fma(lfEstimate, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    f32 GuardedLength3(const Vector3& lrv)
    {
        const f32 lfSquared = Dot3(lrv, lrv);
        return (lfSquared == 0.0f) ? 0.0f : lfSquared * RefinedRsqrt(lfSquared);
    }

    Vector3 Scale4(const Vector3& lrv, f32 lfScale)   // `vmulfp` by a splat, all four lanes
    {
        Vector3 lvResult;
        lvResult.x = lrv.x * lfScale;
        lvResult.y = lrv.y * lfScale;
        lvResult.z = lrv.z * lfScale;
        lvResult.w = lrv.w * lfScale;
        return lvResult;
    }

    Vector3 Negate4(const Vector3& lrv)                // `vxor` with splat(0x80000000): a sign flip
    {
        Vector3 lvResult;
        lvResult.x = -lrv.x;
        lvResult.y = -lrv.y;
        lvResult.z = -lrv.z;
        lvResult.w = -lrv.w;
        return lvResult;
    }

    VecFloat Splat(f32 lf)
    {
        VecFloat lv;
        lv.x = lf;
        lv.y = lf;
        lv.z = lf;
        lv.w = lf;
        return lv;
    }

    // `fctidz` + `stfiwx`: the LOW WORD of a 64-bit truncation (NaN -> 0x8000000000000000 -> 0).
    u32 FctidzLowWord(f32 lfValue)
    {
        const f64 lfWide = static_cast<f64>(lfValue);
        s64 liTruncated;
        if (lfWide != lfWide)
            liTruncated = static_cast<s64>(0x8000000000000000ULL);
        else if (lfWide >= 9223372036854775808.0)
            liTruncated = static_cast<s64>(0x7FFFFFFFFFFFFFFFULL);
        else if (lfWide < -9223372036854775808.0)
            liTruncated = static_cast<s64>(0x8000000000000000ULL);
        else
            liTruncated = static_cast<s64>(lfWide);
        return static_cast<u32>(static_cast<u64>(liTruncated));
    }

    // `lhs x rhs` spelled the console's way: P(a * P(b) - P(a) * b) with P = vpermwi 0x63 (y, z, x)
    // -- one `vmulfp` m = a * P(b), then ONE `vnmsubfp` per lane, -(P(a) * b - m), then the permute
    // back. w is P's w lane: -(a.w * b.w - a.w * b.w), which for finite w is an exact zero NEGATED:
    // -0, as the console writes it (the crash shower frame's zAxis.w).
    Vector3 CrossPermuted(const Vector3& lrA, const Vector3& lrB)
    {
        // u = -(P(a) * b - m), m = a * P(b) rounded first.
        const f32 lfU0 = Vnmsub(lrA.y, lrB.x, lrA.x * lrB.y);
        const f32 lfU1 = Vnmsub(lrA.z, lrB.y, lrA.y * lrB.z);
        const f32 lfU2 = Vnmsub(lrA.x, lrB.z, lrA.z * lrB.x);
        const f32 lfU3 = Vnmsub(lrA.w, lrB.w, lrA.w * lrB.w);
        Vector3 lvResult;
        lvResult.x = lfU1;   // P(u) = (u.y, u.z, u.x, u.w)
        lvResult.y = lfU2;
        lvResult.z = lfU0;
        lvResult.w = lfU3;
        return lvResult;
    }

    // The debrisparams layout block. debrisparams inherits Attrib::Instance PRIVATELY and exposes no
    // accessor; the C-style cast to the private base is the one conversion the language allows
    // for that ([expr.cast]/4), and it is exactly the console's `lwz r11, 4(r10)`.
    const u8* DebrisParamsLayout(const Attrib::Gen::debrisparams& lrParams)
    {
        return static_cast<const u8*>(((const Attrib::Instance&)lrParams).GetLayoutPointer());
    }

    f32 LayoutFloat(const u8* lpLayout, u32 luOffset)
    {
        return *reinterpret_cast<const f32*>(lpLayout + luOffset);
    }
    // The visualfxsurface words the two drains read past the skid block: +0x4F is the bool the
    // detached-part arm gates its HandleSparkContacts call on, +0x54 is the f32 both drains pass
    // as HandleSparkContacts' fifth float. ⚠ FLAG -- the NAMES are the consumers', not the
    // schema's (AttribSys keys by hash and the shipped schema carries no strings); what is
    // recovered is the offset and the role.
    const u32 KU_VFX_SPARKS_ENABLED         = 0x4F;   // bool  (`lbz r10, 0x4F(r11)`)
    const u32 KU_VFX_SPARK_SCALE            = 0x54;   // f32   (`lfs f5, 0x54(r11)`)

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

    // ⛔⛔ THE [skid] PROBE COULD NOT PRINT A "START", AND THAT IS WHY THIS ARRAY EXISTS
    // (measured 2026-09-06, effects-path 1:1 audit). The edge test read
    //     lbWasLaying = (lrEmitter.mrLastTrailTime >= 0.0f)
    // but TrailSystem::AddTrailSegment sets mrLastTrailTime = lrCurrentTime
    // (BrnTrailSystem.cpp:394) BEFORE the probe runs, and the `-1.0f` reset is the line AFTER
    // it. So on a laying frame both sides of the test are true and the edge never fires; the
    // instrument was structurally incapable of saying START. One 210 s run printed
    // FIFTY-TWO "STOP" and ZERO "START" while 625 of 1,579 sample lines carried the gate's own
    // `skid > thr` -- i.e. marks were being laid the whole time and every "the mark begins here"
    // line was missing. A wave reading "0 STARTs" as "no mark ever begins" is the next
    // diagnostics-that-lie entry, so the state is latched here instead: this is the PREVIOUS
    // frame's laying answer, written at the bottom of the wheel loop where lbTrailEnded is final.
    // ⚠️ Per WHEEL, not per (car, wheel) -- the same limitation sauLastSurface below already has,
    // and for the same reason (this is a bring-up instrument, and one player car is the subject).
    bool gabSkidProbeWasLaying[4] = { false, false, false, false };

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
    // [diag] the raw-word view the [skid-bind] record dump prints.
    inline u32 ReadU32(const void* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(lpBase) + luOffset);
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

        // asm 0x8229E910-0x8229E9E0: the four spark parameter sets are copied (a 0x90-byte
        // attrib data area each) into the particle module's four spark arrays. BODIED
        // 2026-09-06 -- SparkArray is a real type now, so this has a named destination.
        PushSparkParams();

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
// PushSparkParams -- the four-array SparkArray::UpdateParams publish.
//
// The console emits this INLINE at two sites, instruction for instruction:
//   EffectsModule::Prepare @0x8229E910..0x8229E9E0 and ::Update @0x8229EFC4..0x8229F084.
// Both walk `r8 = &mSparkParams[0].mpAttributeData` with a 16-byte stride and
// `r11 = &mParticleModule.maSparks[0] + 0x74` with a 0x90 stride, four times, and both
// issue exactly the same thirteen loads and thirteen stores. Outlined here once.
//
// ⭐ THE COLOURS ARE COPIED IN REVERSE, and that is the console's, not ours: the four
//    vector LOADS are v0 <- attrib+0x00, v13 <- +0x10, v12 <- +0x20, v11 <- +0x30, and the
//    four STORES are array+0x00 <- v11, +0x10 <- v12, +0x20 <- v13, +0x30 <- v0. So
//    maColours[i] takes the attrib's colour (3 - i). Checked register by register at both
//    sites; the loop below spells it as Colour(3 - i) rather than re-ordering inside
//    UpdateParams, so the destination member order stays the DWARF's.
// =============================================================================
void EffectsModule::PushSparkParams()
{
    // [DIAG] BRN_SPARK_DIAG=1 -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Says ONCE
    // whether each of the four sparkeffect collections actually RESOLVED. Every one of the
    // thirteen values below is a divisor or a gate downstream, and a collection that did not
    // resolve hands back Attrib::DefaultDataArea(0x90) -- 144 bytes of zeros that read as a
    // perfectly plausible "the parameters are all zero" rather than as a load failure.
    //
    // CHANGE-GATED, NOT ONCE-ONLY. A once-only line here LIED: PushSparkParams runs
    // from Prepare BEFORE 'PostFx/postfxvault.bin' registers and again from Update
    // after it has, so the first call legitimately reads four zeros and a once-only
    // print freezes that reading for the whole run. It printed "376835=0 ... 554431=0"
    // on the very run in which the vault had just been ported and every value WAS
    // live. It now prints whenever the validity word CHANGES, and prints the numbers,
    // so "resolved" is checkable against the vault's own bytes rather than asserted.
    {
        static u32 suLastSparkValidity = 0xFFFFFFFFu;
        const char* const lpcEnv = std::getenv("BRN_SPARK_DIAG");
        if (lpcEnv != 0 && lpcEnv[0] != '0')
        {
            u32 luValidity = 0;
            for (u32 luSlot = 0; luSlot < KU_NUM_SPARK_PARAMS; ++luSlot)
                luValidity |= (mSparkParams[luSlot].IsValid() ? 1u : 0u) << luSlot;
            if (luValidity != suLastSparkValidity)
            {
                suLastSparkValidity = luValidity;
                char lacMsg[320];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[spark] sparkeffect collections: %s=%d %s=%d %s=%d %s=%d "
                    "(1 == the collection resolved; 0 == DefaultDataArea zeros)"
                    " | 376835 grav=%.3f rad=%.4f blur=%.4f life0=%.3f tex=%s\n",
                    "376835", (luValidity & 1u) != 0 ? 1 : 0,
                    "376836", (luValidity & 2u) != 0 ? 1 : 0,
                    "376837", (luValidity & 4u) != 0 ? 1 : 0,
                    "554431", (luValidity & 8u) != 0 ? 1 : 0,
                    static_cast<double>(mSparkParams[0].GravityStrength()),
                    static_cast<double>(mSparkParams[0].SparkRadius()),
                    static_cast<double>(mSparkParams[0].MotionBlurTime()),
                    static_cast<double>(mSparkParams[0].Lifetimes().x),
                    (mSparkParams[0].SparkTextureName() != 0)
                        ? mSparkParams[0].SparkTextureName() : "(null)");
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
    }

    for (u32 luArray = 0; luArray < KU_NUM_SPARK_PARAMS; ++luArray)
    {
        const Attrib::Gen::sparkeffect& lrParams = mSparkParams[luArray];

        mParticleModule.maSparks[luArray].UpdateParams(
            lrParams.GravityStrength(),             // attrib +0x78 -> array +0x70
            lrParams.BounceStrength(),              // attrib +0x88 -> array +0x74
            lrParams.MotionBlurTime(),              // attrib +0x74 -> array +0x78
            lrParams.SparkRadius(),                 // attrib +0x68 -> array +0x7C
            lrParams.DragInitialVelocityScale(),    // attrib +0x80 -> array +0x80
            lrParams.DragTerminalVelocityScale(),   // attrib +0x7C -> array +0x84
            lrParams.DragDuration(),                // attrib +0x84 -> array +0x88
            lrParams.Colour(3), lrParams.Colour(2), // the reversal above
            lrParams.Colour(1), lrParams.Colour(0),
            lrParams.Lifetimes(),                   // attrib +0x40, all four lanes
            lrParams.SparkTextureName());           // attrib +0x50 -> array +0x8C
    }

    // =====================================================================================
    // [DIAG] BRN_SPARK_FORCE=1 -- NOT IN THE X360 BINARY. OFF BY DEFAULT. DELETE-WHEN the
    // sparkeffect collections resolve.
    //
    // The same shape and the same purpose as BRN_CRUMPLE_FORCE: when every input measures
    // zero, the honest next question is whether the code downstream has any authority at
    // all, and the only way to ask it is to force the input and look. MEASURED on this
    // build (see the "[spark] sparkeffect collections" line above): all four collections
    // report 0, i.e. Attrib::DefaultDataArea handed back 144 bytes of zeros, so every array
    // runs with a ZERO lifetime, ZERO radius, ZERO motion-blur time and a NULL texture name.
    // A zero lifetime alone empties the banks on the frame after the spark is born and makes
    // RenderBank's own age gate reject every sample, so nothing downstream can ever be
    // exercised while it stands.
    //
    // ⚠ EVERY NUMBER BELOW IS THIS INSTRUMENT'S, NOT THE CONSOLE'S. They are not a guess at
    // the authored values and must never be shipped as one: they exist so the geometry half
    // can be measured, and they go away with the collection load.
    {
        static bool sbProbed = false;
        static bool sbForce  = false;
        if (!sbProbed)
        {
            sbProbed = true;
            const char* const lpcEnv = std::getenv("BRN_SPARK_FORCE");
            sbForce = (lpcEnv != 0 && lpcEnv[0] != '0');
            if (sbForce)
                CgsDev::Log::WriteToLog("[spark] PARAMETER FORCE ARMED (BRN_SPARK_FORCE) -- "
                                        "the values below are the INSTRUMENT'S, not the console's\n");
        }
        if (sbForce)
        {
            const rw::math::vpu::Vector4 lColour    = { 1.0f, 0.75f, 0.30f, 1.0f };
            const rw::math::vpu::Vector4 lLifetimes = { 1.20f, 1.00f, 0.80f, 0.60f };
            for (u32 luArray = 0; luArray < KU_NUM_SPARK_PARAMS; ++luArray)
            {
                mParticleModule.maSparks[luArray].UpdateParams(
                    -9.81f,   // gravity
                    0.35f,    // bounce
                    0.10f,    // motion-blur window, seconds
                    0.05f,    // spark radius, metres
                    1.00f,    // drag initial velocity scale
                    0.20f,    // drag terminal velocity scale
                    0.50f,    // drag duration, seconds
                    lColour, lColour, lColour, lColour, lLifetimes,
                    "fxspark");
            }
        }
    }
}

// =============================================================================
// LoadNativeParticleParams  @0x82290510  (DWARF :588) -- 102 instructions.
//   Twelve {array index, collection key} pairs, built on the stack in index order
//   (0x8229051C..0x82290610): for each, StringToKey the literal, resolve it under the
//   nativeparticleparams class (0x43DA904B_E836238A -- lis/ori/insrdi at 0x82290624..38),
//   wrap it in an Attrib::Instance (DefaultDataArea(0x90) when the collection carries none)
//   and hand it to maSimpleParticles[index].UpdateParams (`mulli 160` + 0x23350 ==
//   mParticleModule +0xA80, maSimpleParticles +0x228D0). Array 0 (eParticleArray_None) is
//   never updated -- the table starts at 1.
//   BODIED 2026-09-24 (FX-CRASHVFX): it used to announce itself because UpdateParams had
//   no body; the attrib class it builds is Attrib::Gen::nativeparticleparams (new header).
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

    // [FLAG PC witness] NOT CONSOLE BEHAVIOUR: ours, log-only, printed on the first call with
    // BRN_SIMPLEFX_DIAG=1 (default OFF). Which of the twelve collections RESOLVED (a
    // DefaultDataArea fallback reads as a perfectly plausible all-zero parameter block, not as a
    // failure) and what the resolved texture names are -- the names are what LoadFXBundle stage
    // 12 must match. DELETE-WHEN-STABLE.
    static bool sbWitnessed = false;
    const bool lbWitness = !sbWitnessed && BrnParticle::Native::SimpleFxDiagArmed();
    u32 luValidMask = 0;

    for (u32 luEntry = 0; luEntry < 12u; ++luEntry)
    {
        const NativeParticleParamEntry& lrEntry = KAA_NATIVE_PARTICLE_PARAMS[luEntry];
        const Attrib::Gen::nativeparticleparams lParams(Attrib::StringToKey(lrEntry.lpcCollectionKey), 0);
        if (lParams.IsValid())
            luValidMask |= (1u << lrEntry.muArrayIndex);
        if (lbWitness)
        {
            char lacMsg[224];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[simplefx] nativeparticleparams %s -> array %u: valid=%d tex=%s life=%.3f blend=%u "
                "tiles=%ux%u\n",
                lrEntry.lpcCollectionKey, lrEntry.muArrayIndex, lParams.IsValid() ? 1 : 0,
                (lParams.TextureName() != 0) ? lParams.TextureName() : "(null)",
                static_cast<double>(lParams.LifeTime()), lParams.BlendMode(),
                lParams.TilesWide(), lParams.TilesHigh());
            CgsDev::Log::WriteToLog(lacMsg);
        }
        mParticleModule.maSimpleParticles[lrEntry.muArrayIndex].UpdateParams(lParams);
    }

    if (lbWitness)
    {
        sbWitnessed = true;
        char lacMsg[128];
        std::snprintf(lacMsg, sizeof(lacMsg),
            "[simplefx] LoadNativeParticleParams: valid mask=0x%04X (0x1FFE == arrays 1..12)\n",
            luValidMask);
        CgsDev::Log::WriteToLog(lacMsg);
    }
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
            // Name the MATERIAL as well as the record: a surface whose SkidMarksEnabled is
            // false is only believable once you can see what kind of surface it is. Grip /
            // roughness / drag come off the same surface's physicssurface ref, which
            // VehicleManager::ReadSurfaceProperties already reads the same way.
            Attrib::Gen::physicssurface lPhysics(lSurface.PhysicsSurface(), 0);
            char lacMsg[460];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[skid-bind] surf %2u: surfCol=%016llX surfLayout=%p "
                "vfxRef{class=%016llX col=%016llX res=%d} vfxCol=%016llX vfxLayout=%p "
                "thr=%.4f en=%d smoke=%d/%d type=%d grip=%.3f rough=%.3f drag=%.3f\n",
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
                static_cast<int>(ReadS16(lpVfxData, KU_VFX_SKID_MARK_TYPE_ID)),
                static_cast<double>(lPhysics.Grip()),
                static_cast<double>(lPhysics.Roughness()),
                static_cast<double>(lPhysics.LinearDrag()));
            CgsDev::Log::WriteToLog(lacMsg);

            // ⭐ AND THE WHOLE RECORD, as raw words. The four fields above are read at
            // CONSOLE byte offsets into a serialised layout block (0x48 threshold, 0x4C/0x4D
            // smoke, 0x4E skid-marks, 0x58 type -- from HandleWheels @0x82296C80 and
            // WheelStateMachine::Update @0x82293EB8). Believing "en is authored false" means
            // believing those offsets land where they are supposed to, so print the block and
            // let the next reader check the placement instead of taking it on trust: words 0-3
            // and 4-7 are the two skid-mark colours, so plausible 0..1 floats there vouch for
            // the whole record's alignment.
            if (lpBytes != 0)
            {
                char lacDump[420];
                int liLen = std::snprintf(lacDump, sizeof(lacDump), "[skid-bind] surf %2u raw:", luSurface);
                for (u32 luWord = 0; luWord < 24u && liLen > 0 && liLen < static_cast<int>(sizeof(lacDump)) - 12; ++luWord)
                {
                    liLen += std::snprintf(lacDump + liLen, sizeof(lacDump) - static_cast<size_t>(liLen),
                                           " %08X", ReadU32(lpVfxData, luWord * 4u));
                }
                std::snprintf(lacDump + liLen, sizeof(lacDump) - static_cast<size_t>(liLen), "\n");
                CgsDev::Log::WriteToLog(lacDump);
            }
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

    // asm 0x8229EFC4-0x8229F084: the four spark parameter sets copied into the particle
    // module's spark arrays every step -- instruction-identical to the Prepare-time copy
    // (same four vector loads, same nine scalar loads, same 0x90 cursor stride), which is
    // why both call the same helper. BODIED 2026-09-06.
    PushSparkParams();

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
            mParticleModule.ResetSparkFrameData(lfTime);   // `fmr f1, f31` -- the frame time
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

        // ---- [deformloc] witness. NOT console behaviour: ours, latched, log-only. ---------------
        // MEASURED 2026-09-05 (boost-exhaust wave): every Lion emitter sits at the world ORIGIN,
        // and BoostStateMachine::OnTick -- the ONLY thing that ever gives a boost effect a world
        // transform -- returns before doing anything, because lHelper.VehicleLocators() is null and
        // the replay serialiser is not playing. That pointer is THIS lookup's result. So the line
        // below says whether the deformation output published any locator table at all and whether
        // this car's entity id is among the ones it did publish. DELETE-WHEN-STABLE.
        // [FLAG PC witness] OPT-IN (BRN_DEFORMLOC_DIAG=1) + hard first-N cap: the key carries the
        // car index, so a field of cars cycles it every frame and the "latched" line printed
        // 44,810 lines in a 140 s run. DELETE-WHEN the boost-locator path is signed off.
        {
            static const bool sbDeformLocDiag = (std::getenv("BRN_DEFORMLOC_DIAG") != 0);
            static u32 suDeformLocLines = 0u;
            static u32 suLastKey = 0xFFFFFFFFu;
            const u32 luKey = (static_cast<u32>(lpDeformation->miNumLocatorOutputs) << 8)
                            | (lpLocators != 0 ? 1u : 0u) | (luCar << 16);
            if (sbDeformLocDiag && suDeformLocLines < KU_EFFECTS_DIAG_MAX_LINES && luKey != suLastKey)
            {
                ++suDeformLocLines;
                suLastKey = luKey;
                CgsSceneManager::EntityId lWantId;
                lWantId.Set(1u, luCar, 0u);
                char lacMsg[288];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[deformloc] car=%u want=%08X locatorOutputs=%d found=%d first=%08X"
                    "  (null -> BoostStateMachine::OnTick never positions the boost effects)\n",
                    luCar, static_cast<u32>(lWantId), (int)lpDeformation->miNumLocatorOutputs,
                    (int)(lpLocators != 0),
                    (lpDeformation->miNumLocatorOutputs > 0)
                        ? lpDeformation->maLocatorData[0].mEntityId.muValue : 0u);
                CgsDev::Log::WriteToLog(lacMsg);
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
        {
            lrEmitter.mrLastTrailTime = -1.0f;

            // [DIAG] NOT IN THE X360 BINARY -- the RESET WITNESS, gated on BRN_SKID_PROBE.
            // DELETE-WHEN-STABLE.
            // The sentinel above is the ONLY thing that can end a wheel's strip across a
            // place-on-track / event-start grid placement, and it only bites through
            // TrailSystem::AddTrailSegment's `now > 1.5*step + lastTrailTime` gate. A run that
            // sees an 18 m segment bridge a placement cannot tell "the flag never arrived"
            // from "the flag arrived and the gate swallowed it" without this line, and those
            // two have completely different fixes. One line per (wheel, frame) the flag is up.
            if (SkidProbeEnabled())
            {
                char lacRst[160];
                std::snprintf(lacRst, sizeof(lacRst),
                    "[skid] f=%u w=%u RESETXFORM mrLastTrailTime<-(-1) t=%.3f pos=%.2f,%.2f,%.2f\n",
                    gauSkidProbeFrame, luWheel,
                    static_cast<double>(mParticleModule.mRenderData.mfCurrentTime),
                    static_cast<double>(lrWheel.mRoadContact.mPosition.x),
                    static_cast<double>(lrWheel.mRoadContact.mPosition.y),
                    static_cast<double>(lrWheel.mRoadContact.mPosition.z));
                CgsDev::Log::WriteToLog(lacRst);
            }
        }

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

                bool lbSkidMarksEnabled = ReadBool(lpVfxData, KU_VFX_SKID_MARKS_ENABLED);
                // [DIAG] BRN_SKID_DISABLE -- THE CONTROL. NOT IN THE X360 BINARY, inert unless
                // the variable names a value. DELETE-WHEN-STABLE.
                //
                // The shipped surfaces all turn skid marks ON, so the only way to run the
                // negative half of the claim is to force the flag the console reads out of
                // visualfxsurface to false and check that the marks disappear. Two earlier
                // waves correctly declined to run this control while nothing was visible --
                // a control that cannot fail proves nothing. Now that a mark is on film it
                // discriminates, so it is armable. It forces exactly ONE bool and nothing
                // else: the surface still resolves, the threshold is still read, the wheel
                // state machine still runs, only the lay decision flips.
                {
                    static int siSkidDisabled = -1;
                    if (siSkidDisabled < 0)
                    {
                        const char* lpcValue = std::getenv("BRN_SKID_DISABLE");
                        siSkidDisabled = (lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0') ? 1 : 0;
                        if (siSkidDisabled == 1)
                            CgsDev::Log::WriteToLog("[skid] BRN_SKID_DISABLE=1 CONTROL ARMED:"
                                                    " SkidMarksEnabled forced false\n");
                    }
                    if (siSkidDisabled == 1)
                        lbSkidMarksEnabled = false;
                }
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

                    // [DIAG] NOT IN THE X360 BINARY -- BRN_TRAIL_HEIGHT_DIAG, issue #21's ONE
                    // NUMBER: how far above the road the mark this call lays actually sits.
                    // `dy` is (mark y) - (road y) in metres, where the mark y is the contact
                    // point this call hands the trail system PLUS the 0.03 m
                    // kTrailHeightAdjustment the trail system adds to it (BrnTrailSystem.cpp,
                    // vaddfp @0x8227AA28), and the road y is the traction line test's own hit
                    // for THIS wheel THIS step, latched before anything re-expressed it
                    // (BrnDiagTrailHeight.h). `dxz` is the horizontal distance between the two
                    // -- it is the honest check on the pairing itself: a large dxz means the
                    // carried contact point and the raw hit are not the same place at all, and
                    // then dy is not a height above "the road under the mark".
                    // DELETE-WHEN-STABLE.
                    if (BrnDiag::TrailHeightDiagEnabled())
                    {
                        const s32 liCarId = lpState->miRaceCarID;
                        // SEARCH, do not index: the latch is indexed by VehicleManager SLOT and
                        // this side only has the attribute id. See BrnDiagTrailHeight.h --
                        // keying one on the other differenced the player's mark against another
                        // car's road hit. slot == -1 means no entry claims this id.
                        s32 liSlot = -1;
                        for (s32 liS = 0; liS < BrnDiag::KI_TRAIL_HEIGHT_MAX_CARS; ++liS)
                        {
                            const BrnDiag::TrailHeightHit& lrCand =
                                BrnDiag::gaTrailHeightHits[liS][luWheel & 3u];
                            if (lrCand.muStamp != 0u && lrCand.miCarId == liCarId
                                && (liSlot < 0
                                    || lrCand.muStamp
                                       > BrnDiag::gaTrailHeightHits[liSlot][luWheel & 3u].muStamp))
                            {
                                liSlot = liS;
                            }
                        }
                        if (liSlot >= 0)
                        {
                            const BrnDiag::TrailHeightHit& lrHit =
                                BrnDiag::gaTrailHeightHits[liSlot][luWheel & 3u];
                            const f32 lfMarkY = lrWheel.mRoadContact.mPosition.y + 0.03f;
                            const f32 lfDx    = lrWheel.mRoadContact.mPosition.x - lrHit.mfX;
                            const f32 lfDz    = lrWheel.mRoadContact.mPosition.z - lrHit.mfZ;
                            char lacHt[400];
                            std::snprintf(lacHt, sizeof(lacHt),
                                "[trailht] f=%u car=%d slot=%d w=%u t=%.3f mark=%.4f,%.4f,%.4f "
                                "road=%.4f,%.4f,%.4f dy=%.4f dxz=%.4f n=%.4f,%.4f,%.4f "
                                "roadNy=%.4f lineDist=%.4f stamp=%u hit=%u\n",
                                gauSkidProbeFrame, liCarId, liSlot, luWheel,
                                static_cast<double>(mParticleModule.mRenderData.mfCurrentTime),
                                static_cast<double>(lrWheel.mRoadContact.mPosition.x),
                                static_cast<double>(lfMarkY),
                                static_cast<double>(lrWheel.mRoadContact.mPosition.z),
                                static_cast<double>(lrHit.mfX), static_cast<double>(lrHit.mfY),
                                static_cast<double>(lrHit.mfZ),
                                static_cast<double>(lfMarkY - lrHit.mfY),
                                static_cast<double>(std::sqrt(lfDx * lfDx + lfDz * lfDz)),
                                static_cast<double>(lrWheel.mRoadContact.mNormal.x),
                                static_cast<double>(lrWheel.mRoadContact.mNormal.y),
                                static_cast<double>(lrWheel.mRoadContact.mNormal.z),
                                static_cast<double>(lrHit.mfNormalY),
                                static_cast<double>(lrWheel.mRoadContact.mfLineDistanceToRoad),
                                lrHit.muStamp, lrHit.muHit);
                            CgsDev::Log::WriteToLog(lacHt);
                        }
                    }
                }

                // [skid] SURFACE-CHANGE EDGE. The periodic sample below can drive across a
                // whole material and never land on it, and a run that has to FIND a surface
                // with SkidMarksEnabled needs every distinct surface the wheels touch, with
                // the place it was touched -- so a later run can be teleported there. One
                // line per (wheel, new surface id), not per frame.
                if (SkidProbeEnabled())
                {
                    static u32 sauLastSurface[ActiveRaceCarData::KU_NUM_WHEELS] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
                    if (sauLastSurface[luWheel] != luSurfaceId)
                    {
                        sauLastSurface[luWheel] = luSurfaceId;
                        char lacChg[240];
                        std::snprintf(lacChg, sizeof(lacChg),
                            "[skid] f=%u w=%u SURFCHG surf=%u en=%d thr=%.4f type=%d "
                            "pos=%.2f,%.2f,%.2f\n",
                            gauSkidProbeFrame, luWheel, luSurfaceId,
                            ReadBool(lpVfxData, KU_VFX_SKID_MARKS_ENABLED) ? 1 : 0,
                            static_cast<double>(ReadF32(lpVfxData, KU_VFX_SKID_MARK_THRESHOLD)),
                            static_cast<int>(ReadS16(lpVfxData, KU_VFX_SKID_MARK_TYPE_ID)),
                            static_cast<double>(lrWheel.mRoadContact.mPosition.x),
                            static_cast<double>(lrWheel.mRoadContact.mPosition.y),
                            static_cast<double>(lrWheel.mRoadContact.mPosition.z));
                        CgsDev::Log::WriteToLog(lacChg);
                    }
                }

                // [skid] both sides of the gate, on the frames it matters.
                if (SkidProbeEnabled())
                {
                    // NOT `lrEmitter.mrLastTrailTime >= 0.0f` -- see gabSkidProbeWasLaying's
                    // banner: AddTrailSegment has already written that field this frame, so
                    // reading it here made a START unreachable.
                    const bool lbWasLaying = gabSkidProbeWasLaying[luWheel & 3u];
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

        // [skid probe] the edge latch, at the ONE point where lbTrailEnded is final for this
        // wheel this frame (every arm above has run, including the two that never look a
        // surface up). See gabSkidProbeWasLaying's banner. Costs one byte-store when unarmed.
        gabSkidProbeWasLaying[luWheel & 3u] = !lbTrailEnded;

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
        // The "crash triangle cache cleared" post to the dispatch thread. MEASURED at
        // 0x82296FB0..0x82296FC8:
        //     li r5, 0 ; li r4, 0 ; addi r3, r3, -0x7DFC
        //     bl CgsModule::VariableEventQueue<16384,16>::AllocateEventSafe
        // i.e. AllocateEventSafe(liType = 0, liSize = 0) -- a payload-less type-0 record. The
        // result is NOT tested: the bl is followed straight by `addi r1,r1,0x80 ; b
        // __restgprlr_27`, so there is no null check here to reproduce and none is added.
        //
        // ⭐ WHICH queue is pinned by ARITHMETIC, not by trusting the symbol name: r3 is
        // r28 + (3 << 16) - 0x7DFC == this + 0x28204, and mParticleModule sits at +0xA80
        // (EffectsModule.h:406) with mInterThreadEventQueue at +0x27784 inside it --
        // 0xA80 + 0x27784 == 0x28204 exactly.
        //
        // ⚠️ CORRECTED 2026-09-06. This was announced as "NOT RECONSTRUCTED: the inter-thread
        // queue is a placeholder". That was true when it was written and became FALSE in this
        // same wave, when the queue was promoted from a u8[0x4018] span to the real
        // VariableEventQueue<16384,16>. A stale "it is a placeholder" is the reason to re-read
        // these notes rather than trust them: nothing would have failed, the post would simply
        // have gone on not happening while the comment explained why it could not.
        //
        // Type 0 is eParticleEvent_ClearAllDebrisBuckets. Its CONSUMER is still announced --
        // but for an unrelated reason (BrnDebrisArray.cpp is deliberately unmounted), and that
        // announcement lives at the consumer, in ParticleModule_SparkEvents.cpp.
        mParticleModule.mInterThreadEventQueue.AllocateEventSafe(
            BrnParticle::eParticleEvent_ClearAllDebrisBuckets, 0);
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
        // =========================================================================================
        // FLAG PC bring-up: no BrnEffects::EffectsIO::DispatchInputBuffer exists on this build yet,
        // so the particle dispatch input keeps its Construct state -- zero lights, no environment
        // map -- EXCEPT for the WHITE LEVEL, which is now the world's own and not a constant.
        //
        // ⛔⛔ THE PRODUCER THIS ARM STANDS IN FOR IS **NOT** BridgeRendererToEffects @0x823C1168.
        // Three commits in this tree named that function as the reason the white level never
        // arrives. It is the wrong one, and its 22 instructions say so -- the whole body is
        //     GetDispatchFrame -> in+0x10;  GetBaseEffectsFrame -> SetBaseEffectsFrame;
        //     GetFXEventsEffectsFrame(0..1) -> SetFXEventsEffectsFrame;  SetEnvironmentMap
        // and it never touches a float. THE WHITE LEVEL'S PRODUCER IS THE FUNCTION IMMEDIATELY
        // AFTER IT IN THE IMAGE -- BrnGameModule::BridgeWorldToEffects_Dispatch @0x823C11F8, whose
        // last two instructions are the whole point:
        //     0x823C126C  bl  BrnWorldIO::DispatchOutputBuffer::GetWhiteLevel
        //     0x823C1274  bl  BrnEffects::EffectsIO::DispatchInputBuffer::SetWhiteLevel
        // and which is the ONLY xref of SetWhiteLevel @0x823BAB40 in the entire image. Both bridges
        // are called from DoDispatch @0x823DC458; the adjacent addresses are what got confused.
        // The body is landed in its console home, GameSource/Game/GameBridgeWorldToX.cpp.
        //
        // ⭐⭐ WHY THE CONSTANT 1.0f WAS A DEFECT, MEASURED ON THIS BUILD'S OWN LOG.
        // colourScale (BrnLionBlendIm3d's "colourScale" shader variable, staged as
        // (w, w, w, 1) by Im3dBlend::BeginRendering) IS this white level: ParticleModule::
        // RenderFullResParticles passes mRenderData.mfWhiteLevel as cLionFX::Dispatch's f1, which
        // becomes cParticleRender::Dispatch's afWhiteLevel and then LionBlendRenderer::
        // BeginRendering's afColourScale. The same number also scales the tyre-mark trail pass.
        // One shipped run, three passes, one frame:
        //     [sky]       ... whiteLevel 0.500000        <- EnvironmentManager::mfWhiteLevel
        //     [suncorona] ... whiteLevel=0.500           <- the corona vertex program
        //     [lionfx] Dispatch: batches=2 ... white=1.000   <- THE PARTICLES
        //     [trailpass] RenderFullResParticles live: ... white=1.000
        // EnvironmentManager::GenerateShaderConstants @0x827D0098 pre-multiplies EVERY published
        // colour by mfWhiteLevel (0.5, KF_DEF_WHITE_LEVEL) and the post-fx composite divides it
        // straight back out (BrnPostFxShader.cpp:1472, GlobalParams.x = 1 / lfWhiteLevel = 2.0).
        // That round trip cancels for the world, the sky and the coronas -- and did NOT cancel for
        // the particles, because this arm handed them 1.0. Every Lion particle and every trail
        // therefore left the composite at EXACTLY 2x its authored value, which under the material's
        // own authored additive blend (eBLEND_SRCALPHA_ADD, 13c415d5) is what blew the boost plume
        // to a white core. It is not a colour that was tuned; it is the missing half of a round
        // trip the rest of the frame already performs.
        //
        // THE SOURCE IS THE FRAME, NOT A NUMBER WE CHOSE: gBrnWorldShaderConstantsFrameBringUp is
        // the same object BrnRendererModule::Render reads lfFrameWhiteLevel out of to build that
        // 1/whiteLevel (BrnRendererModule.cpp:4283), written by WorldModule::
        // SetupShaderConstantsBeforeRendering @0x827D1410 -- the console function that ALSO writes
        // the DispatchOutputBuffer word BridgeWorldToEffects_Dispatch reads. Same producer, same
        // value; only the carrier differs, and the carrier is the missing IO buffer set.
        //
        // ONE-FRAME LAG, stated rather than hidden: this producer runs earlier in DoDispatch than
        // WorldModule::GenerateDispatchListsBringUp, so it reads the frame the world published LAST
        // frame. The console has the same producer/consumer split across the dispatch buffer pair.
        // Before the first world publish the valid flag is clear and the Construct seed (1.0f, the
        // identity) stands -- the same gate BrnRendererModule::PublishSkyConstantsBringUp uses.
        //
        // DELETE-WHEN the effects dispatch IO buffer set is real: then DoDispatch calls
        // BridgeWorldToEffects_Dispatch and this whole arm goes with the null test above it.
        // =========================================================================================
        static bool sbLogged = false;
        LogNotReconstructed(sbLogged,
            "the effects DISPATCH input (BridgeWorldToEffects_Dispatch @0x823C11F8 has a body but no "
            "IO buffer set to carry it); the particle dispatch input keeps zero key light / no env "
            "map, and takes its WHITE LEVEL from the live world shading frame");

        f32 lfLiveWhiteLevel = gbBrnWorldShaderConstantsFrameBringUpValid
                             ? gBrnWorldShaderConstantsFrameBringUp.GetWhiteLevel()
                             : 1.0f;

        // [DIAG] BRN_LION_WHITE_PIN=1 restores the OLD constant 1.0f. It exists for exactly one
        // purpose: a SAME-BUILD A/B, so the before/after film differs in this one scalar and in
        // nothing else -- no second compile, no second commit, no "is the other build stale".
        // Inert unless the variable is set. DELETE-WHEN-STABLE with the [lionfx] family.
        {
            static s32 siPin = -1;
            if (siPin < 0)
            {
                const char* lpcPin = std::getenv("BRN_LION_WHITE_PIN");
                siPin = (lpcPin != 0 && lpcPin[0] != '0') ? 1 : 0;
            }
            if (siPin != 0)
            {
                lfLiveWhiteLevel = 1.0f;
            }
        }

        lpParticleInput->SetWhiteLevel(lfLiveWhiteLevel);

        // [lionfx] one line whenever the value the particle pass will scale by changes, so the
        // colourScale that reaches the shader is readable in the same log as [sky] and [lionfx]
        // Dispatch. DELETE-WHEN-STABLE with the rest of the [lionfx] family.
        {
            static f32 sfLastWhiteLevel = -1.0f;
            if (lfLiveWhiteLevel != sfLastWhiteLevel)
            {
                sfLastWhiteLevel = lfLiveWhiteLevel;
                char lacMsg[128];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[lionfx] particle colourScale <- world white level %.3f (valid=%d)\n",
                              static_cast<double>(lfLiveWhiteLevel),
                              gbBrnWorldShaderConstantsFrameBringUpValid ? 1 : 0);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
    }

    lpParticleInput->LockForRead();
    lpDispatchThreadInputBuffer->LockForWrite();
    mParticleModule.GenerateRenderRequests(lpParticleInput, lpDispatchThreadInputBuffer);
    lpDispatchThreadInputBuffer->UnlockForWrite();
    lpParticleInput->UnlockForRead();

    const bool lbDestroyed = lpInputBufferStack->DestroyIOBuffer(&lpParticleInput);
    CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");   // CgsModuleIOHelper.h:57
    (void)lbDestroyed;

    // =====================================================================================
    // THE LION EFFECT HAND-OFF (added 2026-09-05, the boost-exhaust wave).
    //
    // ⭐⭐ WITHOUT THESE TWO CALLS NOTHING IN THE GAME EVER CREATES A PARTICLE EMITTER, and
    // that is measured, not argued: with the whole Lion render closure landed and reachable,
    // a two-minute driving run printed `[lionfx] Render: emitters live=0` once and never
    // again. StartLionEffect only STAMPS a maPlayingEffects slot with CREATE; the pair below
    // is the only thing that reads that stamp, and DispatchThreadUpdate's
    // cLionFX::EffectCreate -> cLionEffectManager::EffectCreate ->
    // cLionParticleEffectManager::BindingsAttach -> cParticleEmitterManager::Register is the
    // only route to a live emitter that exists.
    //
    // CONSOLE SHAPE, exactly: they are MODULE VTABLE ENTRIES, not calls this function makes.
    // BrnEffects::EffectsModule::PreRenderUpdate @0x8227FE10 write-locks the dispatch buffer,
    // memcpys the crash triangle cache into it and then calls the particle module's slot 72
    // (its own PreRenderUpdate); ::DispatchThreadUpdate @0x8227FE88 read-locks the buffer and
    // calls slot 76. The module framework drives both once a frame, in that order.
    //
    // FLAG PC bring-up wiring: the framework's PreRenderUpdate / DispatchThreadUpdate leg does
    // not exist on this build (no module's PreRenderUpdate is called anywhere in the tree), so
    // the pair is driven from HERE -- GenerateDispatchLists, the per-frame dispatch hook that
    // IS live, is the same DoDispatch pass the console's own calls belong to, and it already
    // owns this very buffer one statement earlier. The order below is the console's: publish
    // (write lock), then consume (read lock), both BEFORE the render thread's
    // BuildLionVertexBuffers runs cLionFX::Update on the emitters this creates.
    // ⛔ THE TWO LOCK WINDOWS MUST NOT NEST -- IOBuffer::LockForRead asserts "Already locked
    // for write" -- which is why they sit outside the GenerateRenderRequests bracket above
    // rather than inside it, and why PreRenderUpdate takes its own write lock internally
    // exactly as the console's wrapper does.
    // DELETE-WHEN the module framework's PreRenderUpdate / DispatchThreadUpdate leg lands:
    // these become EffectsModule::PreRenderUpdate @0x8227FE10 / ::DispatchThreadUpdate
    // @0x8227FE88 overrides and the framework calls them.
    //
    // ⭐ FX-CRASHVFX 2026-09-25: EffectsModule::PreRenderUpdate's OWN statement, which this stand-in
    // used to drop (the banner above names it; nothing did it): LockForWrite, then
    // memcpy(GetBufferCrashTriangleCache(), &mCrashTriangleCache, 0x1E80) -- `addis r4, r30, 3 ;
    // addi r4, r4, -0x2C00` (this + 0x2D400) and `li r5, 0x1E80`, 0x8227FE30..0x8227FE50 -- then
    // UnlockForWrite, and only then the particle module's slot 72. That buffer copy is the cache
    // ParticleModule::BeginSimulateDebris hands every debris job; without it the jobs saw an EMPTY
    // cache (the glass live run 20260925_115837: crash=1 on every frame, collide=0 on every frame)
    // and no piece ever collided with the ground the crashing car's triangles describe.
    // =====================================================================================
    lpDispatchThreadInputBuffer->LockForWrite();
    *lpDispatchThreadInputBuffer->GetBufferCrashTriangleCache() = mCrashTriangleCache;
    lpDispatchThreadInputBuffer->UnlockForWrite();
    mParticleModule.PreRenderUpdate(lpDispatchThreadInputBuffer);

    lpDispatchThreadInputBuffer->LockForRead();
    mParticleModule.DispatchThreadUpdate(lpDispatchThreadInputBuffer);
    lpDispatchThreadInputBuffer->UnlockForRead();
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

// =================================================================================================
// ⭐⭐⭐ THE CONTACT SPARK DRAINS -- @0x8229B7F8 / @0x82293470 / @0x822906A8.
//
// ⛔ 2026-09-24 (FX-CRASHVFX): HandleSparkContacts is CONSOLE-DEAD -- its lbDisableThisEffect byte
// (byte_82CDB40C) is initialised TRUE and never written, so the along-line route below is the
// console's code but never its behaviour. See the switch's own note in HandleSparkContacts.
//
// This is the producer half of the grinding-spark chain. The consumer half (the particle module's
// inter-thread queue, ProcessEventQueue and HandleSpawnSparksAlongLineEvent) landed in the same
// change; before it, HandleSparkContacts had nowhere to post and these three had no reason to run.
//
// The route, per contact:
//     contact spy queue -> Process*Contacts -> HandleSparkContacts
//         -> mParticleModule.mInterThreadEventQueue.AddEventSafe(&rec, 3, 80)
//         -> (next frame, dispatch thread) ParticleModule::HandleSpawnSparksAlongLineEvent
//         -> SparkArray::SpawnSpark
//
// ⚠️ THE ONE THING TO NOT MISREAD HERE is which vector each drain passes as the spark VELOCITY.
// ProcessHingedPartContacts passes the contact's mFrictionStress (`addi r11, r1, var_B0` with the
// 112-byte copy based at var_C0, i.e. copy + 0x10); ProcessCarDetatchedPartContacts passes the
// PhysicalCarPartContact's own mVelocity (copy + 0x60). They are different members and the
// pseudocode shows neither.
// =================================================================================================

// ------------------------------------------------------------------------------------------------
// HandleSparkContacts @0x822906A8 (232 instr, DWARF EffectsModule.cpp:1452).
//
// DWARF signature:
//   void HandleSparkContacts(const BaseContact&, Vector3, Native::ESparkArrayID,
//                            f32, f32, f32, f32, f32, bool);
// and the ABI confirms it: r3 this, r4 the contact, the Vector3 in v1, r5 the array id, the five
// f32s in f1..f5, and the bool as a STACK argument at the caller's +0x64 (its byte at +0x67 --
// both call sites do `stb r26, 0x160+var_F9(r1)` / `stb r27, 0x240+var_1D9(r1)`, which is that
// slot). Hex-Rays renders this as a 32-argument function because each f32 eats a GPR slot.
//
// ⚠️ TWO OF THE FIVE FLOATS ARE NEVER READ BY THIS BUILD'S BODY -- f1 and f5. The asm touches f2
// (`fmr f31, f2`), f3 (`fmr f30, f3`) and f4 (`stfs f4, var_100`) and nothing else; there is no
// other fp load of an incoming register anywhere in the 232 instructions. Both call sites still
// compute and pass them (f1 = params.mDt, f5 = the visualfxsurface word at +0x54), so they are
// real parameters that this revision's body stopped using -- NOT parameters that do not exist.
// They are named for what the callers put in them and marked unused rather than deleted.
//
// WHAT IT ACTUALLY DOES: reject the contact if its friction stress is shorter than the caller's
// threshold, then fill a 80-byte SpawnSparksAlongLineEvent whose segment runs from the contact
// point along the NORMALISED friction stress for a randomly drawn length, and post it.
// ------------------------------------------------------------------------------------------------
void EffectsModule::HandleSparkContacts(const BrnPhysics::ContactSpy::BaseContact& lrContact,
                                        Vector3 lvVelocity,
                                        BrnParticle::Native::ESparkArrayID leSparkType,
                                        f32 /*lfDt*/,                    // f1 -- see the banner
                                        f32 lfTime,                      // f2
                                        f32 lfGroundPositionY,           // f3
                                        f32 lfMinFrictionStress,         // f4
                                        f32 /*lfSurfaceSparkScale*/,     // f5 -- see the banner
                                        bool lbIsCrashing)
{
    // byte_82CDB40C -- this function's `static bool8_t lbDisableThisEffect` (DecFIGS DWARF
    // EffectsModule.cpp:1455). ⛔⛔ IT IS TRUE, SO ON THE CONSOLE THIS FUNCTION NEVER POSTS ANYTHING.
    // ⚠️ CORRECTED 2026-09-24 (FX-CRASHVFX). This read "NOTHING IN THE IMAGE WRITES IT ... so it is a
    // .bss byte that stays false and the gate never fires" (b5 6b2d999c, the 09-06 spark wave). The
    // first half holds -- findinit.py finds one site, the `lbz` below, and a whole-image scan for the
    // pointer value 0x82CDB40C finds none, so no tweakable table can reach it -- but the byte is NOT
    // .bss: it is INITIALISED .data holding 0x01 (IDA flag word 0x00009501, FF_IVL set; the calibrated
    // neighbour _gSparkSpawnParamsRaceCarVehicle @0x82CDB3E4 reads 6.7041669 / 44.694443 / 90 / 360
    // out of the same page). Never written, it stays TRUE and the gate ALWAYS fires. The console's own
    // words agree (gen_switch_data.py: 0x822906A8 with the image's page posts nothing and draws no
    // random number; forced to 0 it posts one type-3 record). So does the Remaster: BurnoutPR.exe's
    // ProcessHingedPartContacts (sub_981D70) keeps the 84/85 filter and the |friction| > 7.5 gate and
    // then calls NOTHING -- LTCG dropped the dead call -- and none of its 40,742 .code functions posts
    // a 0x50-byte type-3 record.
    // ⇒ THE "SCRAPE SPARKS" THE PC BUILD DREW FROM 6b2d999c UNTIL THIS CHANGE -- along-line sparks off
    //   detached and hinged parts -- WERE A PATH THE CONSOLE NEVER RUNS. The console's scrape and crash
    //   sparks are ProcessRaceCarContacts' showers (world grinding, vehicle grinding, the crash shower),
    //   drawn by ParticleModule::HandleSpawnSparkShowerFromPointEvent. Everything below the gate is
    //   still the console's body, bit for bit; it just never runs (and neither do the
    //   gauSparkContact* [spark] counters inside it).
    static const bool sbSparkContactsDisabled = true;       // byte_82CDB40C == 0x01
    if (sbSparkContactsDisabled)
        return;

    ++BrnParticle::gauSparkContactCalls;   // [DIAG] DELETE-WHEN-STABLE

    // The console computes Length(mFrictionStress) with vrsqrtefp + two Newton steps and a vsel
    // that maps a zero vector to 0 rather than NaN; sqrtf does the same on both counts.
    const Vector3& lrFriction = lrContact.mFrictionStress;      // contact + 0x10
    const f32 lfFrictionLength = sqrtf(lrFriction.x * lrFriction.x
                                     + lrFriction.y * lrFriction.y
                                     + lrFriction.z * lrFriction.z);

    // `vcmpgtfp. v0, v9, v0` -- threshold > length rejects. Note the sense: EQUAL passes.
    if (lfMinFrictionStress > lfFrictionLength)
    {
        ++BrnParticle::gauSparkContactRejected;   // [DIAG] DELETE-WHEN-STABLE
        return;
    }

    // The console's own two self-checks, EffectsModule.cpp:1578 and :1579. IsZero here is the
    // RwMathVPU one: it masks the sign bit off each lane, replaces the w lane with x
    // (`vrlimi128 v12, v0, 1, 1`) and compares (|x|,|y|,|z|,|x|) against unk_8200D990 ==
    // FLT_EPSILON; the assert fires only when NO lane exceeds it, i.e. x, y and z are all zero.
    // A stress along a single world axis (a normal of exactly (0,0,+-1) against an axis-aligned
    // wall while the car runs along x) has x == y == 0 and z != 0 and must NOT fire. Written as
    // `!(|lane| > eps)` so a NaN lane counts as "not greater", the way the vector compare's
    // all-false bit counts it.
    const bool lbFrictionIsZero = !(std::fabs(lrFriction.x) > KF_RWMATH_IS_ZERO_EPSILON)
                               && !(std::fabs(lrFriction.y) > KF_RWMATH_IS_ZERO_EPSILON)
                               && !(std::fabs(lrFriction.z) > KF_RWMATH_IS_ZERO_EPSILON);
    const bool lbNormalIsZero   = !(std::fabs(lrContact.mNormalStress.x) > KF_RWMATH_IS_ZERO_EPSILON)
                               && !(std::fabs(lrContact.mNormalStress.y) > KF_RWMATH_IS_ZERO_EPSILON)
                               && !(std::fabs(lrContact.mNormalStress.z) > KF_RWMATH_IS_ZERO_EPSILON);

    // [DIAG] BRN_EFFECTS_DIAG=1 -- NOT IN THE CONSOLE BINARY. DELETE-WHEN-STABLE. Prints the two
    // stress vectors for every contact whose x and y lanes are both zero, so a run can attest
    // which lane carried the stress on the contacts that used to trip the assert above, and
    // whether any contact reaches here with a genuinely all-zero stress.
    {
        static const bool sbEffectsDiag = (std::getenv("BRN_EFFECTS_DIAG") != 0);
        if (sbEffectsDiag
            && std::fabs(lrContact.mNormalStress.x) <= KF_RWMATH_IS_ZERO_EPSILON
            && std::fabs(lrContact.mNormalStress.y) <= KF_RWMATH_IS_ZERO_EPSILON)
        {
            char lacMsg[320];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[spark-iszero] type=%d ownerA=%u normalStress=(%g,%g,%g) frictionStress=(%g,%g,%g)"
                " normal=(%g,%g,%g) allZero=%d\n",
                static_cast<int>(leSparkType),
                static_cast<unsigned>(lrContact.mEntityIdA.muValue >> 24),
                static_cast<double>(lrContact.mNormalStress.x),
                static_cast<double>(lrContact.mNormalStress.y),
                static_cast<double>(lrContact.mNormalStress.z),
                static_cast<double>(lrFriction.x),
                static_cast<double>(lrFriction.y),
                static_cast<double>(lrFriction.z),
                static_cast<double>(lrContact.mNormal.x),
                static_cast<double>(lrContact.mNormal.y),
                static_cast<double>(lrContact.mNormal.z),
                lbNormalIsZero ? 1 : 0);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    CGS_ASSERT(!lbFrictionIsZero, "RwMathVPU::IsZero( lContact.mFrictionStress ) == false");
    CGS_ASSERT(!lbNormalIsZero,   "RwMathVPU::IsZero( lContact.mNormalStress ) == false");

    // The spark type indexes mSparkParams[] (this + 16*type + 0x2D348) and everything below comes
    // out of that sparkeffect's attribute block -- the fields postfxvault.bin publishes.
    const Attrib::Gen::sparkeffect& lrParams = mSparkParams[leSparkType];

    // The two draws, in the console's order (one shared LCG -- swapping them changes both).
    const f32 lfLineLength = mRandom.RandomFloat(lrParams.SparkLineLengthMin(),
                                                 lrParams.SparkLineLengthMax());
    const f32 lfNumSparks  = mRandom.RandomFloat(lrParams.NumSparksMin(),
                                                 lrParams.NumSparksMax());

    // The segment: from the contact point, along the NORMALISED friction stress, lfLineLength long.
    // (`vmulfp128 v0, v13, v0` normalises, then `vmaddfp v0, v0, v11, v7` with v11 == the contact
    // point and v7 == splat(lfLineLength) -- raw field order D,A,B,C, so dir * length + point.)
    // ⚠ NO ZERO GUARD, DELIBERATELY. The console's SECOND normalise (0x82290858..0x822909EC) has
    // no `vsel` -- unlike the threshold one above it -- so on a zero friction stress it would
    // produce a NaN segment. It cannot: the threshold test three lines up already rejected that
    // case for every caller (both pass a positive threshold, 7.5 and 0.005). Adding a guard here
    // would be a defensive arm the binary does not have.
    const f32 lfInvLength = 1.0f / lfFrictionLength;

    BrnParticle::SpawnSparksAlongLineEvent lEvent;
    lEvent.mvStartPos = lrContact.mPointOnA;                                  // contact + 0x40
    lEvent.mvEndPos.x = lrContact.mPointOnA.x + lrFriction.x * lfInvLength * lfLineLength;
    lEvent.mvEndPos.y = lrContact.mPointOnA.y + lrFriction.y * lfInvLength * lfLineLength;
    lEvent.mvEndPos.z = lrContact.mPointOnA.z + lrFriction.z * lfInvLength * lfLineLength;
    lEvent.mvEndPos.w = lrContact.mPointOnA.w + lrFriction.w * lfInvLength * lfLineLength;
    lEvent.mvVelocity = lvVelocity;                                           // the v1 argument
    lEvent.mfCurrentTime                 = lfTime;                            // f2
    lEvent.mfNumSparks                   = lfNumSparks;
    // `vspltw v10, v11, 1` (the contact point's y) minus splat(f3) -- the height of the segment's
    // start above whatever plane the caller nominated. BOTH current callers pass the contact
    // point's OWN y, so this is 0 for them; it is not hard-coded to 0 because the parameter is
    // real and the race-car drain (not landed here) has a genuine ground plane to pass.
    lEvent.mfHeightAboveGroundOfStartPos = lrContact.mPointOnA.y - lfGroundPositionY;
    lEvent.mfVelocityInheritanceMin      = lrParams.VelocityInheritanceMin();
    lEvent.mfVelocityInheritanceMax      = lrParams.VelocityInheritanceMax();
    lEvent.meSparkType                   = leSparkType;
    lEvent.mbIsCrashing                  = lbIsCrashing;

    const bool lbPosted = mParticleModule.mInterThreadEventQueue.AddEventSafe(
        &lEvent, BrnParticle::eParticleEvent_SpawnSparksAlongLine,
        static_cast<s32>(sizeof(BrnParticle::SpawnSparksAlongLineEvent)));   // li r6, 0x50
    if (lbPosted)
        ++BrnParticle::gauSparkContactPosted;   // [DIAG] DELETE-WHEN-STABLE
}

// ------------------------------------------------------------------------------------------------
// ProcessHingedPartContacts @0x82293470 (105 instr, DWARF EffectsModule.cpp:3767).
//
// The hinged deformable parts -- bonnet, boot, bumper, grille -- grinding on the world. This is
// the drain whose queue used to be provably empty on every frame of every run: its only producer
// in the whole image, PhysicalBodyPart::AddContactSpy @0x8260B8D8, was a log-once stub until the
// physics lane bodied it (0d6cc68b), and the accessor that reaches the queue
// (ContactSpyInterface::GetHingedPartContacts @0x822779B8) did not exist at either level.
// ------------------------------------------------------------------------------------------------
void EffectsModule::ProcessHingedPartContacts(
         const BrnPhysics::ContactSpy::ContactSpyData::HingedCarPartContactQueue* lpQueue,
         const EffectsModuleParams& lrParams)
{
    const s32 liLength = lpQueue->GetLength();          // `lwz r24, 8(r27)`
    BrnParticle::gauSparkHingedSeen += static_cast<u32>(liLength > 0 ? liLength : 0);   // [DIAG]

    for (s32 liIndex = 0; liIndex < liLength; ++liIndex)
    {
        // The console copies the whole 112-byte record onto its stack (14 std pairs) before
        // touching it, and hands HandleSparkContacts a pointer to THAT copy.
        const BrnPhysics::ContactSpy::HingedPartContact lContact = lpQueue->GetEvent(liIndex);

        // Only the two EXHAUST part ids spark here (84 / 85 -- see the constants' note).
        if (lContact.meType != KI_HINGED_SPARK_PART_EXHAUST_A && lContact.meType != KI_HINGED_SPARK_PART_EXHAUST_B)
            continue;
        ++BrnParticle::gauSparkHingedTyped;   // [DIAG] DELETE-WHEN-STABLE

        const Vector3& lrFriction = lContact.mFrictionStress;
        const f32 lfFrictionLength = sqrtf(lrFriction.x * lrFriction.x
                                         + lrFriction.y * lrFriction.y
                                         + lrFriction.z * lrFriction.z);

        // `vcmpgtfp. v0, v0, v9` -- the caller-side pre-filter, the same 7.5 it then hands
        // HandleSparkContacts as its own threshold (flt_8200DD18).
        if (!(lfFrictionLength > KF_HINGED_MIN_FRICTION_STRESS))
            continue;
        ++BrnParticle::gauSparkHingedStress;   // [DIAG] DELETE-WHEN-STABLE

        // The surface lookup: (mCollisionTagB's low half >> 4) & 0x3F indexes mSurfaceList's
        // "Surfaces" array; the surface's layout carries the visualfxsurface RefSpec at +0x10.
        const u32 luSurfaceId =
            (static_cast<u16>(lContact.mCollisionTagB.muValue) >> KU_SURFACE_ID_SHIFT) & KU_SURFACE_ID_MASK;

        void* lpSurfaceRef = mSurfaceList.Surfaces(luSurfaceId);
        if (!lpSurfaceRef)
            lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);

        Attrib::Gen::surface lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
        Attrib::Gen::visualfxsurface lVfx(VfxSurfaceRef(lSurface.GetAttributeData()), 0);

        // f5 -- the visualfxsurface word at +0x54. Passed, and this build's HandleSparkContacts
        // does not read it (see its banner). Reproduced because the console computes it.
        const f32 lfSurfaceSparkScale =
            *reinterpret_cast<const f32*>(static_cast<const u8*>(lVfx.GetAttributeData()) + KU_VFX_SPARK_SCALE);

        HandleSparkContacts(lContact,
                            lrFriction,                                   // v1: the friction stress
                            BrnParticle::Native::eSparkArray_BodyPart_Contact,  // r5 == 3
                            lrParams.mDt,                                 // f1
                            lrParams.mTime,                               // f2
                            lContact.mPointOnA.y,                         // f3 (=> height 0)
                            KF_HINGED_MIN_FRICTION_STRESS,                // f4
                            lfSurfaceSparkScale,                          // f5
                            false);                                       // the stack byte
    }
}

// ------------------------------------------------------------------------------------------------
// ProcessCarContactQueues @0x8229B7F8 (74 instr, DWARF EffectsModule.cpp:3012) -- the fan-out.
//
// ⭐⭐ THE GATE IS THE REPLAY SERIALISER'S MODE, AND IT IS AN OVERRIDE, NOT AN EARLY-OUT.
// `addis r11, r26, 3 ; addi r11, r11, -0xAB0 ; lwz r11, 0(r11)` reads this + 0x2F550, which is
// mEffectsSerialiser's leading word == BaseSerialiser::meMode; the three values it tests -- 4, 5,
// 6 -- are E_MODE_PLAYING_PREPARING, E_MODE_PLAYING and E_MODE_PLAYING_STALLED, i.e. exactly the
// three PLAYBACK states. So: while a replay is playing back, the live contact queues are ignored
// and ProcessRaceCarContacts runs with a NULL queue (it replays the recorded contacts out of
// EffectsSerialiserStaticLayout::GetCarContact instead), and the detached-part and hinged-part
// drains do not run at all. Otherwise the spy is asserted bound and all three are drained.
// ⚠ Read by NAME (mEffectsSerialiser.GetMode()), never by the console byte offset -- every member
// ahead of it on this host carries widened pointers, so +0x2F550 is not that word here.
// ------------------------------------------------------------------------------------------------
void EffectsModule::ProcessCarContactQueues(const EffectsModuleParams& lrParams,
                                            const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                            const BrnPhysics::ContactSpy::ContactSpyInterface* lpContactSpy,
                                            const BrnDirector::Camera::Camera* lpCamera)
{
    ++BrnParticle::gauSparkContactQueueCalls;   // [DIAG] DELETE-WHEN-STABLE

    const BrnReplays::BaseSerialiser::EMode leReplayMode = mEffectsSerialiser.GetMode();
    const bool lbForceRaceCarPass =
        (leReplayMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_PREPARING
      || leReplayMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING
      || leReplayMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_STALLED);

    const bool lbHasSpyData = (lpContactSpy != 0 && lpContactSpy->IsValid());
    if (!lbHasSpyData && !lbForceRaceCarPass)
        return;

    const BrnPhysics::ContactSpy::ContactSpyData::RaceCarContactQueue*         lpRaceCar  = 0;
    const BrnPhysics::ContactSpy::ContactSpyData::PhysicalCarPartContactQueue* lpCarPart  = 0;
    const BrnPhysics::ContactSpy::ContactSpyData::HingedCarPartContactQueue*   lpHinged   = 0;

    if (!lbForceRaceCarPass)
    {
        // The console re-reads mpData here and asserts it -- BrnContactSpyInterface.h:179, which is
        // the assert GetRaceCarContacts() itself carries; the three accessors are inlined/called in
        // this order (`lwz r31, 0(r29)` then `bl _` then `bl sub_822779B8`).
        lpRaceCar = lpContactSpy->GetRaceCarContacts();
        lpCarPart = lpContactSpy->GetPhysicalCarPartContacts();
        lpHinged  = lpContactSpy->GetHingedPartContacts();
    }

    if (lpRaceCar != 0 || lbForceRaceCarPass)
        ProcessRaceCarContacts(lpRaceCar, lpActiveRaceCars, lrParams, lpCamera);

    if (lpCarPart != 0)
        ProcessCarDetatchedPartContacts(lpCarPart, lpActiveRaceCars, lrParams);

    if (lpHinged != 0)
        ProcessHingedPartContacts(lpHinged, lrParams);
}

// =================================================================================================
// ⭐⭐⭐ THE RACE-CAR CONTACT DRAIN AND ITS SHOWERS -- FX-CRASHVFX 2026-09-24.
//
// ProcessRaceCarContacts @0x82297C08 drains the race-car contact queue (or, in replay playback, the
// recorded contacts) and turns each contact into what a player SEES at a hit:
//   not crashing, against the world   -> world-grinding sparks (BurstAccumulator-paced shower);
//   not crashing, against a race car  -> vehicle-grinding sparks + a spark burst off the contact,
//                                        and in the two Road Rage modes the takedown debris burst;
//   not crashing, against traffic     -> vehicle-grinding sparks + the contact spark burst;
//   CRASHING, against world/car/traffic -> the big crash spark shower (100..250 sparks);
//   CRASHING, anything                -> a debris burst (crashing debris params);
//   CRASHING, against the world, > 5 m/s -> CRASH IMPACT DUST, 2.5 simple particles per metre.
// Its callees -- DoSparkShower, HandleRaceCarRaceCarSparks, HandleVehicleVehicleSparks,
// HandleBurstDebris -- and the two particle-module producers behind them (SpawnSparkShowerFromPoint,
// FireDebrisBurst) had no bodies; every one is below / in ParticleModule.cpp, straight from its asm.
// =================================================================================================

namespace
{
    // (large - small) * size + small, per lane: `vsubfp` then ONE `vmaddfp` (DoSparkShower
    // 0x82292108..0x82292140).
    Vector4 LerpShowerArgs(const Vector4& lrSmall, const Vector4& lrLarge, const VecFloat& lrvSize)
    {
        Vector4 lv;
        lv.x = std::fma(lrLarge.x - lrSmall.x, lrvSize.x, lrSmall.x);
        lv.y = std::fma(lrLarge.y - lrSmall.y, lrvSize.y, lrSmall.y);
        lv.z = std::fma(lrLarge.z - lrSmall.z, lrvSize.z, lrSmall.z);
        lv.w = std::fma(lrLarge.w - lrSmall.w, lrvSize.w, lrSmall.w);
        return lv;
    }

    bool IsReplayPlayback(BrnReplays::BaseSerialiser::EMode leMode)     // `cmpwi 4 / 5 / 6`
    {
        return leMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_PREPARING
            || leMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING
            || leMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_STALLED;
    }

    bool IsReplayRecording(BrnReplays::BaseSerialiser::EMode leMode)    // `cmpwi 1 / 2 / 3`
    {
        return leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING_PREPARING
            || leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING
            || leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING_STALLED;
    }

    // The effects serialiser's car-contact table, which ProcessRaceCarContacts reads (the count, in
    // playback) and appends to (while recording) INLINE -- the layout class publishes these offsets
    // and has no accessor for either. 0x82297C80 `lwz r10, 0x34(r3)`; 0x82297F4C..0x82297FC4.
    typedef BrnReplays::EffectsSerialiserStaticLayout EffectsStaticLayout;

    s32 NumRecordedCarContacts(const EffectsStaticLayout* lpLayout)
    {
        return *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpLayout)
                                             + EffectsStaticLayout::KI_OFF_NUM_CONTACTS);
    }

    void RecordCarContact(EffectsStaticLayout* lpLayout, const BrnPhysics::ContactSpy::RaceCarContact& lrContact)
    {
        u8* const lpBase = reinterpret_cast<u8*>(lpLayout);
        s32& lriCount = *reinterpret_cast<s32*>(lpBase + EffectsStaticLayout::KI_OFF_NUM_CONTACTS);
        if (lriCount == 32)                    // `cmpwi cr6, r11, 0x20 ; beq` -- a full table drops it
            return;
        const s32 liSlot = lriCount;
        *reinterpret_cast<u32*>(lpBase + EffectsStaticLayout::KI_OFF_CONTACT_FIELD1 + 4 * liSlot) = lrContact.mEntityIdA.muValue;
        *reinterpret_cast<u32*>(lpBase + EffectsStaticLayout::KI_OFF_CONTACT_FIELD2 + 4 * liSlot) = lrContact.mEntityIdB.muValue;
        *reinterpret_cast<u32*>(lpBase + EffectsStaticLayout::KI_OFF_CONTACT_FIELD3 + 4 * liSlot) = lrContact.mCollisionTagB.muValue;
        std::memcpy(lpBase + EffectsStaticLayout::KI_OFF_CONTACT_VEC_A + 16 * liSlot, &lrContact.mNormal, 16);
        // `vrlimi128 v0, v13, 1, 0`: the point's x, y, z over the slot's own w (the active flag
        // UpdateCarContact sets and GetCarContact reads back).
        f32* const lpSlotB = reinterpret_cast<f32*>(lpBase + EffectsStaticLayout::KI_OFF_CONTACT_VEC_B + 16 * liSlot);
        lpSlotB[0] = lrContact.mPointOnA.x;
        lpSlotB[1] = lrContact.mPointOnA.y;
        lpSlotB[2] = lrContact.mPointOnA.z;
        lriCount = liSlot + 1;
    }

    // The two identity rows (0x82181500 / 0x82181510) the world-grinding frame crosses the normal with.
    Vector3 AxisX()
    {
        Vector3 lv;
        lv.x = 1.0f; lv.y = 0.0f; lv.z = 0.0f; lv.w = 0.0f;
        return lv;
    }
    Vector3 AxisY()
    {
        Vector3 lv;
        lv.x = 0.0f; lv.y = 1.0f; lv.z = 0.0f; lv.w = 0.0f;
        return lv;
    }

    // [DIAG] BRN_SIMPLEFX_DIAG=1 -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Capped lines naming
    // each shower / burst / dust spawn the drain fires, so a live run can say which contact made what.
    bool RaceCarContactDiagArmed()
    {
        return BrnParticle::Native::SimpleFxDiagArmed();
    }
    u32 guRaceCarContactDiagLines = 0;
}

Vector4 SparkShowerController::GetLateralAngleMinMaxForwardAngleMinMax(VecFloat lvSize) const
{
    Vector4 lv = LerpShowerArgs(mSmallArgs.mLateralAngleMinMaxForwardAngleMinMax,
                                mLargeArgs.mLateralAngleMinMaxForwardAngleMinMax, lvSize);
    lv.x *= KF_DEGREES_TO_RADIANS;
    lv.y *= KF_DEGREES_TO_RADIANS;
    lv.z *= KF_DEGREES_TO_RADIANS;
    lv.w *= KF_DEGREES_TO_RADIANS;
    return lv;
}

Vector4 SparkShowerController::GetSpawnVelocityMinMaxInheritedVelocityMinMax(VecFloat lvSize) const
{
    return LerpShowerArgs(mSmallArgs.mSpawnVelocityMinMaxInheritedVelocityMinMax,
                          mLargeArgs.mSpawnVelocityMinMaxInheritedVelocityMinMax, lvSize);
}

Vector4 SparkShowerController::GetSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ(VecFloat lvSize) const
{
    return LerpShowerArgs(mSmallArgs.mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ,
                          mLargeArgs.mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ, lvSize);
}

// ------------------------------------------------------------------------------------------------
// DoSparkShower @0x822920C0 (39 instr, DWARF EffectsModule.cpp:2521).
// ABI: r4 the controller, v1 the splatted size, r5 the transform (by pointer), v2 the velocity to
// inherit, f1 / f2 (eating r6 / r7), r8 the count. A zero count posts nothing (`cmplwi r9, 0 ; beq`).
// ------------------------------------------------------------------------------------------------
void EffectsModule::DoSparkShower(const SparkShowerController& lrController,
                                  VecFloat lvSize,
                                  Matrix44Affine lTransform,
                                  Vector3 lvVelocityToInherit,
                                  f32 lfCurrentTime,
                                  f32 lfGroundPositionY,
                                  u32 luNumToSpawn)
{
    if (luNumToSpawn == 0u)
        return;

    mParticleModule.SpawnSparkShowerFromPoint(lTransform,
                                              lrController.GetLateralAngleMinMaxForwardAngleMinMax(lvSize),
                                              lrController.GetSpawnVelocityMinMaxInheritedVelocityMinMax(lvSize),
                                              lvVelocityToInherit,
                                              lrController.GetSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ(lvSize),
                                              lfCurrentTime,
                                              lfGroundPositionY,
                                              lrController.GetVelocitySpeedScaleThreshold(),
                                              lrController.GetReflectionAmount(),
                                              luNumToSpawn,
                                              lrController.GetSparkArrayId());
}

// ------------------------------------------------------------------------------------------------
// HandleRaceCarRaceCarSparks @0x82290A48 (96 instr, DWARF EffectsModule.cpp:1577).
// One SpawnSparksFromPoint record (type 1, 0x50 bytes) off the contact point: the car's own velocity,
// the contact normal, a spark count from _gSparkSpawnParamsRaceCarVehicle's speed ramp times dt, the
// height of the point above the car's ground plane, and the GrindingRaceCars sparkeffect's velocity
// inheritance (mSparkParams[1] +0x54 / +0x58). The dt float is f1, time f2, ground height f3.
// ------------------------------------------------------------------------------------------------
void EffectsModule::HandleRaceCarRaceCarSparks(f32 lfDt,
                                               f32 lfTime,
                                               Vector3 lvPosition,
                                               Vector3 lvNormal,
                                               const RaceCarState* lpRaceCarState,
                                               f32 lfGroundPositionY)
{
    if (KB_RACE_CAR_SPARKS_DISABLED)                                    // byte_82CDB40D
        return;

    const Vector3& lrVelocity = lpRaceCarState->mLinearVelocity;        // +0x330
    const f32 lfSpeed = GuardedLength3(lrVelocity);
    const Attrib::Gen::sparkeffect& lrSparkParams = mSparkParams[BrnParticle::Native::eSparkArray_GrindingRaceCars];

    BrnParticle::SpawnSparksFromPointEvent lEvent;
    lEvent.mvWorldSpacePoint    = lvPosition;
    lEvent.mvVelocity           = lrVelocity;
    lEvent.mvNormal             = lvNormal;
    lEvent.meSparkType          = BrnParticle::Native::eSparkArray_GrindingRaceCars;    // `li r8, 1`
    lEvent.mfCurrentTime        = lfTime;
    lEvent.mfNumSparks          = gSparkSpawnParamsRaceCarVehicle.VelocityToSpawnCount(lfSpeed, lfDt);
    lEvent.mfHeightAboveGround  = lvPosition.y - lfGroundPositionY;     // splat(y) - splat(ground)
    lEvent.mfVelocityInheritMin = lrSparkParams.VelocityInheritanceMin();
    lEvent.mfVelocityInheritMax = lrSparkParams.VelocityInheritanceMax();
    lEvent.mbIsCrashRelated     = false;                                // `stb r10 (0)`

    mParticleModule.mInterThreadEventQueue.AddEventSafe(
        &lEvent, BrnParticle::eParticleEvent_SpawnSparksFromPoint,
        static_cast<s32>(sizeof(BrnParticle::SpawnSparksFromPointEvent)));    // `li r6, 0x50`
}

// ------------------------------------------------------------------------------------------------
// HandleVehicleVehicleSparks @0x82296790 (148 instr, DWARF EffectsModule.cpp:1512).
// The vehicle-grinding shower. Faster than 10 mph (a NaN speed goes on: `blt` only), the grinding
// burst accumulator takes speed * dt * (cos(time * 0.8 pi) * 5 + 20); when it bursts, the shower is
// sized by the burst's cubed interpolant and framed on the HORIZONTAL part of the two cars' mean
// velocity -- normalised WITHOUT a zero guard, as the console does.
// ⚠ Row 0 of that frame is (h.z, 0, h.x) -- `vperm128` with 0x82CDA350 then `vrlimi128` lane z <-
// h.x -- which is not the cross product (up x h) = (h.z, 0, -h.x). The console's own frame; kept.
// ------------------------------------------------------------------------------------------------
void EffectsModule::HandleVehicleVehicleSparks(Vector3 lvPosition,
                                               Vector3 lvOtherVelocity,
                                               ActiveRaceCarData& lrActiveRaceCar,
                                               const RaceCarState* lpRaceCarState,
                                               f32 lfDt,
                                               f32 lfTime)
{
    const Vector3& lrVelocity = lpRaceCarState->mLinearVelocity;        // +0x330
    const f32 lfSpeed = GuardedLength3(lrVelocity);
    if (lfSpeed < KF_GRINDING_MIN_SPEED)
        return;

    const f32 lfWobble = static_cast<f32>(std::cos(static_cast<f64>(lfTime * KF_GRINDING_WOBBLE_FREQUENCY)));
    const f32 lfRate   = std::fma(lfWobble, KF_GRINDING_WOBBLE_AMPLITUDE, KF_GRINDING_WOBBLE_CENTRE);
    BurstAccumulator& lrBurst = lrActiveRaceCar.GetBurstAccumulatorWorldGrinding();
    const u32 luNumSparks = lrBurst.Update((lfRate * lfSpeed) * lfDt, lfTime, mRandom);
    if (luNumSparks == 0u)
        return;

    // (v + other) * 0.5, then * K_VECTOR3_1_0_1 (unk_82FAC200) -- the horizontal part.
    Vector3 lvMean;
    lvMean.x = (lrVelocity.x + lvOtherVelocity.x) * 0.5f;
    lvMean.y = (lrVelocity.y + lvOtherVelocity.y) * 0.5f;
    lvMean.z = (lrVelocity.z + lvOtherVelocity.z) * 0.5f;
    lvMean.w = (lrVelocity.w + lvOtherVelocity.w) * 0.5f;
    Vector3 lvHorizontal;
    lvHorizontal.x = lvMean.x * 1.0f;
    lvHorizontal.y = lvMean.y * 0.0f;
    lvHorizontal.z = lvMean.z * 1.0f;
    lvHorizontal.w = lvMean.w * 0.0f;

    const f32 lfT = lrBurst.GetBurstSizeAsInterpolatorBetweenMinAndMax(luNumSparks);
    const f32 lfSize = (lfT * lfT) * lfT;

    const Vector3 lvHorizontalHat = Scale4(lvHorizontal, RefinedRsqrt(Dot3(lvHorizontal, lvHorizontal)));

    Matrix44Affine lFrame;
    lFrame.xAxis.x = lvHorizontalHat.z;
    lFrame.xAxis.y = 0.0f;
    lFrame.xAxis.z = lvHorizontalHat.x;
    lFrame.xAxis.w = lvHorizontalHat.z;
    lFrame.yAxis   = AxisY();
    lFrame.zAxis   = lvHorizontalHat;
    lFrame.wAxis   = lvPosition;

    DoSparkShower(gSparkShowerControllerVehicleGrinding, Splat(lfSize), lFrame, lvMean,
                  lfTime, lrActiveRaceCar.GetGroundPositionY(), luNumSparks);
}

// ------------------------------------------------------------------------------------------------
// HandleBurstDebris @0x82290BC8 (89 instr, DWARF EffectsModule.cpp:1680).
// A debris burst sized by the car's speed through the debrisparams' two-piece speed ramp. Below the
// minimum speed nothing (a NaN speed goes on, and then fails the scale test); the scale must exceed
// 0.01. The burst starts 0.2 of the contact normal out from the point (fused), inherits the car's
// velocity, carries its colour and the camera position. The interface, index and dt are unused.
// ------------------------------------------------------------------------------------------------
void EffectsModule::HandleBurstDebris(const RCEntityActiveRaceCarOutputInterface* /*lpActiveRaceCars*/,
                                      EActiveRaceCarIndex /*leIndex*/,
                                      f32 /*lfDt*/,
                                      f32 lfTime,
                                      Vector3 lvPosition,
                                      Vector3 lvNormal,
                                      const RaceCarState* lpRaceCarState,
                                      const BrnDirector::Camera::Camera* lpCamera,
                                      const Attrib::Gen::debrisparams& lrDebrisParams,
                                      const RwRGBAReal& lrColour)
{
    const u8* const lpLayout = DebrisParamsLayout(lrDebrisParams);
    const Vector3& lrVelocity = lpRaceCarState->mLinearVelocity;        // +0x330
    const f32 lfSpeed = GuardedLength3(lrVelocity);

    const f32 lfMinSpeed = LayoutFloat(lpLayout, KU_DEBRIS_MIN_SPEED);
    if (lfSpeed < lfMinSpeed)
        return;

    const f32 lfMidSpeed = LayoutFloat(lpLayout, KU_DEBRIS_MID_SPEED);
    f32 lfScale;
    if (lfSpeed < lfMidSpeed)
    {
        const f32 lfScaleMin = LayoutFloat(lpLayout, KU_DEBRIS_SCALE_AT_MIN_SPEED);
        const f32 lfT = (lfSpeed - lfMinSpeed) / (lfMidSpeed - lfMinSpeed);
        lfScale = std::fma(lfT, LayoutFloat(lpLayout, KU_DEBRIS_SCALE_AT_MID_SPEED) - lfScaleMin, lfScaleMin);
    }
    else
    {
        const f32 lfMaxSpeed = LayoutFloat(lpLayout, KU_DEBRIS_MAX_SPEED);
        const f32 lfScaleMid = LayoutFloat(lpLayout, KU_DEBRIS_SCALE_AT_MID_SPEED);
        const f32 lfClamped  = (lfSpeed - lfMaxSpeed >= 0.0f) ? lfMaxSpeed : lfSpeed;     // fsel
        const f32 lfT = (lfClamped - lfMidSpeed) / (lfMaxSpeed - lfMidSpeed);
        lfScale = std::fma(lfT, LayoutFloat(lpLayout, KU_DEBRIS_SCALE_AT_MAX_SPEED) - lfScaleMid, lfScaleMid);
    }
    if (!(lfScale > KF_MIN_EFFECT_SCALE))
        return;

    Vector3 lvSpawnPosition;
    lvSpawnPosition.x = std::fma(lvNormal.x, KF_DEBRIS_BURST_NORMAL_OFFSET, lvPosition.x);
    lvSpawnPosition.y = std::fma(lvNormal.y, KF_DEBRIS_BURST_NORMAL_OFFSET, lvPosition.y);
    lvSpawnPosition.z = std::fma(lvNormal.z, KF_DEBRIS_BURST_NORMAL_OFFSET, lvPosition.z);
    lvSpawnPosition.w = std::fma(lvNormal.w, KF_DEBRIS_BURST_NORMAL_OFFSET, lvPosition.w);

    Vector4 lvColour;
    lvColour.x = lrColour.red;
    lvColour.y = lrColour.green;
    lvColour.z = lrColour.blue;
    lvColour.w = lrColour.alpha;

    Vector3 lvHalfExtents;
    std::memcpy(&lvHalfExtents, lpLayout + KU_DEBRIS_EMITTER_HALF_EXTENTS, sizeof(lvHalfExtents));

    mParticleModule.FireDebrisBurst(lvSpawnPosition,
                                    lpCamera->GetTransform().wAxis,     // camera + 0x30
                                    lvHalfExtents,
                                    lrVelocity,
                                    lfTime,
                                    lfScale,
                                    lrDebrisParams,
                                    lvColour);
}

// ------------------------------------------------------------------------------------------------
// ProcessRaceCarContacts @0x82297C08 (965 instr, DWARF EffectsModule.cpp:3292).
//
// Per contact (the queue's 96-byte copy, or in replay playback the recorded contact, whose point
// stands in for both points):
//   * the A car's index is asserted valid (:3655) and its state non-null (:3659); a HIDDEN car
//     (+0x452) contributes nothing;
//   * while RECORDING, the contact is appended to the serialiser's car-contact table;
//   * the B entity's owner byte picks the arm, and the A car's mbCrashing (+0x44A) the half.
// The replay mode is re-read at every test, as the console reloads it.
// ------------------------------------------------------------------------------------------------
void EffectsModule::ProcessRaceCarContacts(
         const BrnPhysics::ContactSpy::ContactSpyData::RaceCarContactQueue* lpQueue,
         const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
         const EffectsModuleParams& lrParams,
         const BrnDirector::Camera::Camera* lpCamera)
{
    const s32 liNumContacts = IsReplayPlayback(mEffectsSerialiser.GetMode())
                            ? NumRecordedCarContacts(mEffectsSerialiser.GetStaticLayout())
                            : lpQueue->GetLength();

    // `cmpwi 0xB ; cmpwi 3` on meCurrentGameMode -- the takedown debris belongs to Road Rage.
    const bool lbRoadRage = (meCurrentGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE
                          || meCurrentGameMode == BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE);

    for (s32 liContact = 0; liContact < liNumContacts; ++liContact)
    {
        bool lbRecordedCarB = false;                                    // var_3E0 (`stb 0` per contact)
        BrnPhysics::ContactSpy::RaceCarContact lContact;
        if (IsReplayPlayback(mEffectsSerialiser.GetMode()))
        {
            u32 luEntityA = 0, luEntityB = 0;
            u16 lau16Tag[2] = { 0, 0 };
            Vector4 lvNormal, lvPoint;
            mEffectsSerialiser.GetStaticLayout()->GetCarContact(liContact, &lbRecordedCarB, &luEntityA,
                                                                &luEntityB, lau16Tag, &lvNormal, &lvPoint);
            std::memset(&lContact, 0, sizeof(lContact));
            lContact.mEntityIdA.muValue = luEntityA;
            lContact.mEntityIdB.muValue = luEntityB;
            std::memcpy(&lContact.mCollisionTagB, lau16Tag, sizeof(lau16Tag));
            std::memcpy(&lContact.mNormal,   &lvNormal, sizeof(lvNormal));   // the two quadwords, as read
            std::memcpy(&lContact.mPointOnA, &lvPoint,  sizeof(lvPoint));
            std::memcpy(&lContact.mPointOnB, &lvPoint,  sizeof(lvPoint));  // `stvx128 v122 -> var_2A0`
        }
        else
        {
            lContact = lpQueue->GetEvent(liContact);
        }
        Vector3 lvPoint = lContact.mPointOnA;                           // v122

        const CgsSceneManager::EntityId lEntityA(lContact.mEntityIdA.muValue);
        const EActiveRaceCarIndex leIndexA = static_cast<EActiveRaceCarIndex>(lEntityA.GetEntityIndex());
        CGS_ASSERT(leIndexA != E_ACTIVE_RACE_CAR_INDEX_INVALID && leIndexA != E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "( leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT )");
        const RaceCarState* const lpStateA = lpActiveRaceCars->GetRaceCarState(leIndexA);
        CGS_ASSERT(lpStateA != nullptr, "lpActiveRaceCarState != NULL");
        if (lpStateA->mbIsHidden)                                       // +0x452
            continue;

        if (IsReplayRecording(mEffectsSerialiser.GetMode()))
            RecordCarContact(mEffectsSerialiser.GetStaticLayout(), lContact);

        Vector3 lvNormal = lContact.mNormal;                            // v127
        ActiveRaceCarData& lrCarA = maActiveRaceCarData[leIndexA];
        const RwRGBAReal lColourA = lpActiveRaceCars->GetRaceCarColour(leIndexA);
        const u32 luOwnerB = lContact.mEntityIdB.muValue >> 24;

        if (!lpStateA->mbCrashing)                                      // +0x44A
        {
            if (luOwnerB == 0u)
            {
                // ---- against the world: the world-grinding shower (0x82298344..0x822985DC) ----
                const u32 luSurfaceId =
                    (static_cast<u16>(lContact.mCollisionTagB.muValue) >> KU_SURFACE_ID_SHIFT) & KU_SURFACE_ID_MASK;
                void* lpSurfaceRef = mSurfaceList.Surfaces(luSurfaceId);
                if (!lpSurfaceRef)
                    lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);
                Attrib::Gen::surface lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
                Attrib::Gen::visualfxsurface lVfx(VfxSurfaceRef(lSurface.GetAttributeData()), 0);
                const f32 lfSurfaceScale = *reinterpret_cast<const f32*>(
                    static_cast<const u8*>(lVfx.GetAttributeData()) + KU_VFX_GRINDING_SCALE);

                const Vector3& lrVelocity = lpStateA->mLinearVelocity;  // v126
                const f32 lfSpeed = GuardedLength3(lrVelocity);
                if (!(lfSurfaceScale > KF_MIN_EFFECT_SCALE) || lfSpeed < KF_GRINDING_MIN_SPEED)
                    continue;

                const f32 lfTime   = lrParams.mTime;
                const f32 lfWobble = static_cast<f32>(std::cos(static_cast<f64>(lfTime * KF_GRINDING_WOBBLE_FREQUENCY)));
                const f32 lfRate   = std::fma(lfWobble, KF_GRINDING_WOBBLE_AMPLITUDE, KF_GRINDING_WOBBLE_CENTRE);
                BurstAccumulator& lrBurst = lrCarA.GetBurstAccumulatorWorldGrinding();
                const u32 luNumSparks = lrBurst.Update(((lfSpeed * lrParams.mDt) * lfRate) * lfSurfaceScale,
                                                       lfTime, mRandom);
                if (luNumSparks == 0u)
                    continue;

                const f32 lfT = lrBurst.GetBurstSizeAsInterpolatorBetweenMinAndMax(luNumSparks);
                const f32 lfSize = (lfT * lfT) * lfT;

                // The frame: the normal; a unit vector across it (n x Y, or n x X when that is
                // shorter than K_VECFLOAT_EPSILON -- `vcmpgefp`, so a NaN picks X), turned to face
                // along the car's velocity; and their cross.
                const Vector3 lvCrossY = CrossPermuted(lvNormal, AxisY());
                const Vector3 lvCrossX = CrossPermuted(lvNormal, AxisX());
                const Vector3 lvAcross = (Dot3(lvCrossY, lvCrossY) >= KF_VECFLOAT_EPSILON) ? lvCrossY : lvCrossX;
                const Vector3 lvAcrossHat = Scale4(lvAcross, RefinedRsqrt(Dot3(lvAcross, lvAcross)));

                Matrix44Affine lFrame;
                lFrame.xAxis = lvNormal;
                lFrame.yAxis = CrossPermuted(lvAcrossHat, lvNormal);
                lFrame.zAxis = (Dot3(lvAcrossHat, lrVelocity) >= 0.0f) ? lvAcrossHat : Negate4(lvAcrossHat);
                lFrame.wAxis = lvPoint;

                DoSparkShower(gSparkShowerControllerWorldGrinding, Splat(lfSize), lFrame, lrVelocity,
                              lfTime, lrCarA.GetGroundPositionY(), luNumSparks);
                if (RaceCarContactDiagArmed() && guRaceCarContactDiagLines < KU_EFFECTS_DIAG_MAX_LINES)
                {
                    ++guRaceCarContactDiagLines;
                    char lacMsg[200];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[racecar-contact] world-grinding shower car=%d sparks=%u size=%.3f speed=%.2f t=%.3f\n",
                        static_cast<int>(leIndexA), static_cast<unsigned>(luNumSparks),
                        static_cast<double>(lfSize), static_cast<double>(lfSpeed), static_cast<double>(lfTime));
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
            else if (luOwnerB == 1u)
            {
                // ---- against a race car (0x82298084..0x82298340): each pair once, from the lower index ----
                const u32 luIndexB = (lContact.mEntityIdB.muValue >> 10) & 0x3FFFu;
                if (!(static_cast<u32>(leIndexA) < luIndexB))
                    continue;
                const EActiveRaceCarIndex leIndexB = static_cast<EActiveRaceCarIndex>(luIndexB);
                const RaceCarState* const lpStateB = lpActiveRaceCars->GetRaceCarState(leIndexB);
                CGS_ASSERT(lpStateB != nullptr, "lpActiveRaceCarStateB != NULL");

                HandleVehicleVehicleSparks(lvPoint, lpStateB->mLinearVelocity, lrCarA, lpStateA,
                                           lrParams.mDt, lrParams.mTime);
                HandleRaceCarRaceCarSparks(lrParams.mDt, lrParams.mTime, lvPoint, lvNormal, lpStateA,
                                           lrCarA.GetGroundPositionY());
                if (!lbRoadRage)
                    continue;

                // Which car sheds the takedown debris: the recorded choice in playback, otherwise a
                // coin (`fcmpu f0, f26 ; bge` -- B at or above 0.5).
                bool lbCarB;
                if (IsReplayPlayback(mEffectsSerialiser.GetMode()))
                    lbCarB = lbRecordedCarB;
                else
                    lbCarB = !(mRandom.RandomFloat() < KF_TAKEDOWN_DEBRIS_COIN);

                EActiveRaceCarIndex leDebrisCar;
                const RaceCarState* lpDebrisState;
                RwRGBAReal lDebrisColour;
                if (!lbCarB)
                {
                    (void)lpActiveRaceCars->GetCarModelId(leIndexA);
                    leDebrisCar   = leIndexA;
                    lpDebrisState = lpStateA;
                    lDebrisColour = lColourA;
                }
                else
                {
                    (void)lpActiveRaceCars->GetCarModelId(leIndexB);
                    leDebrisCar   = leIndexB;
                    lpDebrisState = lpStateB;
                    lDebrisColour = lpActiveRaceCars->GetRaceCarColour(leIndexB);
                    lvNormal      = Negate4(lvNormal);
                    if (!IsReplayPlayback(mEffectsSerialiser.GetMode()))
                        lvPoint = lContact.mPointOnB;
                    if (IsReplayRecording(mEffectsSerialiser.GetMode()))
                        mEffectsSerialiser.GetStaticLayout()->UpdateCarContact(&lvPoint);
                }

                if (!(mafTimeUntilNextDebrisBurst[leDebrisCar] > 0.0f))
                {
                    HandleBurstDebris(lpActiveRaceCars, leDebrisCar, lrParams.mDt, lrParams.mTime, lvPoint,
                                      lvNormal, lpDebrisState, lpCamera, mRoadRageDebrisParams, lDebrisColour);
                    mafTimeUntilNextDebrisBurst[leDebrisCar] =
                        LayoutFloat(DebrisParamsLayout(mRoadRageDebrisParams), KU_DEBRIS_BURST_INTERVAL);
                }
            }
            else if (luOwnerB == 2u)
            {
                // ---- against traffic (0x82298034..0x82298080): the car's own velocity, halved,
                // stands in for the other vehicle's (`vcfsx v0, v0, 1` == 0.5) ----
                HandleVehicleVehicleSparks(lvPoint, Scale4(lpStateA->mLinearVelocity, 0.5f), lrCarA, lpStateA,
                                           lrParams.mDt, lrParams.mTime);
                HandleRaceCarRaceCarSparks(lrParams.mDt, lrParams.mTime, lvPoint, lvNormal, lpStateA,
                                           lrCarA.GetGroundPositionY());
            }
            continue;
        }

        // ---- CRASHING (0x822985E0..0x82298AE8) ----
        if (luOwnerB <= 2u)
        {
            // The crash shower, framed on the contact normal and the car's velocity ALONG the
            // surface (the normal component taken out), normalised through `vrefp` with no guard.
            const Vector3& lrVelocity = lpStateA->mLinearVelocity;
            const Vector3 lvNormalPart = Scale4(lvNormal, Dot3(lvNormal, lrVelocity));
            Vector3 lvTangential;
            lvTangential.x = lrVelocity.x - lvNormalPart.x;
            lvTangential.y = lrVelocity.y - lvNormalPart.y;
            lvTangential.z = lrVelocity.z - lvNormalPart.z;
            lvTangential.w = lrVelocity.w - lvNormalPart.w;
            const f32 lfTangentialSpeed = GuardedLength3(lvTangential);
            const Vector3 lvTangentialHat = Scale4(lvTangential, RefinedRecip(lfTangentialSpeed));

            Matrix44Affine lFrame;
            lFrame.xAxis = lvNormal;
            lFrame.yAxis = lvTangentialHat;
            lFrame.zAxis = CrossPermuted(lvNormal, lvTangentialHat);
            lFrame.wAxis = lvPoint;

            const u32 luSurfaceId =
                (static_cast<u16>(lContact.mCollisionTagB.muValue) >> KU_SURFACE_ID_SHIFT) & KU_SURFACE_ID_MASK;
            void* lpSurfaceRef = mSurfaceList.Surfaces(luSurfaceId);
            if (!lpSurfaceRef)
                lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);
            Attrib::Gen::surface lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
            Attrib::Gen::visualfxsurface lVfx(VfxSurfaceRef(lSurface.GetAttributeData()), 0);
            const u8* const lpVfxData = static_cast<const u8*>(lVfx.GetAttributeData());

            if (lpVfxData[KU_VFX_SPARKS_ENABLED] != 0
                && lfTangentialSpeed > KF_CRASH_SHOWER_MIN_TANGENTIAL_SPEED
                && !(*reinterpret_cast<const f32*>(lpVfxData + KU_VFX_GRINDING_SCALE) < KF_MIN_EFFECT_SCALE)
                && !(mafTimeUntilNextSparksBurst[leIndexA] > 0.0f))
            {
                const f32 lfRamp      = (lfTangentialSpeed - KF_GRINDING_MIN_SPEED) * KF_CRASH_SHOWER_SIZE_SLOPE;
                const f32 lfFloored   = (-lfRamp >= 0.0f) ? 0.0f : lfRamp;
                const f32 lfSaturated = (1.0f - lfFloored >= 0.0f) ? lfFloored : 1.0f;
                mafTimeUntilNextSparksBurst[leIndexA] =
                    std::fma(mRandom.RandomFloat(), KF_CRASH_SHOWER_INTERVAL_RANGE, KF_CRASH_SHOWER_INTERVAL_MIN);
                const f32 lfSize  = (lfSaturated * (mRandom.RandomFloat() + 1.0f)) * 0.5f;
                const u32 luCount = FctidzLowWord(std::fma(lfSize, KF_CRASH_SHOWER_COUNT_RANGE, KF_CRASH_SHOWER_COUNT_BASE));

                DoSparkShower(gSparkShowerControllerCrashing, Splat(lfSize * lfSize), lFrame, lvTangential,
                              lrParams.mTime, lrCarA.GetGroundPositionY(), luCount);
                if (RaceCarContactDiagArmed() && guRaceCarContactDiagLines < KU_EFFECTS_DIAG_MAX_LINES)
                {
                    ++guRaceCarContactDiagLines;
                    char lacMsg[200];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[racecar-contact] crash shower car=%d owner=%u sparks=%u size=%.3f vt=%.2f t=%.3f\n",
                        static_cast<int>(leIndexA), static_cast<unsigned>(luOwnerB), static_cast<unsigned>(luCount),
                        static_cast<double>(lfSize), static_cast<double>(lfTangentialSpeed),
                        static_cast<double>(lrParams.mTime));
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
        }

        if (!(mafTimeUntilNextDebrisBurst[leIndexA] > 0.0f))
        {
            HandleBurstDebris(lpActiveRaceCars, leIndexA, lrParams.mDt, lrParams.mTime, lvPoint, lvNormal,
                              lpStateA, lpCamera, mCrashingDebrisParams, lColourA);
            mafTimeUntilNextDebrisBurst[leIndexA] =
                LayoutFloat(DebrisParamsLayout(mCrashingDebrisParams), KU_DEBRIS_BURST_INTERVAL);
        }

        // ---- CRASH IMPACT DUST: a crashing car against the WORLD above 5 m/s ----
        const Vector3& lrVelocity = lpStateA->mLinearVelocity;          // v127 (the normal is done)
        const f32 lfSpeed = GuardedLength3(lrVelocity);
        if (!(lfSpeed > KF_CRASH_DUST_MIN_SPEED) || luOwnerB != 0u)
            continue;

        f32& lrfAccumulated = mafAccumulatedParticleCountCrash[leIndexA];
        lrfAccumulated = std::fma(lfSpeed * lrParams.mDt, KF_CRASH_DUST_PER_METRE, lrfAccumulated);
        const u32 luNumParticles = FctidzLowWord(lrfAccumulated);

        Vector3 lvBoxLow;
        lvBoxLow.x = lvPoint.x - KF_CRASH_DUST_BOX_BELOW_X;
        lvBoxLow.y = lvPoint.y - KF_CRASH_DUST_BOX_BELOW_Y;
        lvBoxLow.z = lvPoint.z - KF_CRASH_DUST_BOX_BELOW_Z;
        lvBoxLow.w = lvPoint.w - 0.0f;
        Vector3 lvBoxHigh;
        lvBoxHigh.x = lvPoint.x + KF_CRASH_DUST_BOX_ABOVE;
        lvBoxHigh.y = lvPoint.y + KF_CRASH_DUST_BOX_ABOVE;
        lvBoxHigh.z = lvPoint.z + KF_CRASH_DUST_BOX_ABOVE;
        lvBoxHigh.w = lvPoint.w + 0.0f;
        Utils::Vector3Randomiser lPositionRandomiser;
        lPositionRandomiser.Prepare(lvBoxLow, lvBoxHigh);

        Vector4 lvVelocityLow;
        lvVelocityLow.x = KF_CRASH_DUST_VELOCITY_MIN_XZ;
        lvVelocityLow.y = KF_CRASH_DUST_VELOCITY_MIN_Y;
        lvVelocityLow.z = KF_CRASH_DUST_VELOCITY_MIN_XZ;
        lvVelocityLow.w = KF_CRASH_DUST_INHERIT_MIN;
        Vector4 lvVelocityHigh;
        lvVelocityHigh.x = KF_CRASH_DUST_VELOCITY_MAX_XZ;
        lvVelocityHigh.y = KF_CRASH_DUST_VELOCITY_MAX_Y;
        lvVelocityHigh.z = KF_CRASH_DUST_VELOCITY_MAX_XZ;
        lvVelocityHigh.w = KF_CRASH_DUST_INHERIT_MAX;
        Utils::Vector4Randomiser lVelocityRandomiser;
        lVelocityRandomiser.Prepare(lvVelocityLow, lvVelocityHigh);

        lrfAccumulated -= static_cast<f32>(luNumParticles);             // fcfid of the zero-extended word
        if (luNumParticles == 0u)
            continue;

        if (RaceCarContactDiagArmed() && guRaceCarContactDiagLines < KU_EFFECTS_DIAG_MAX_LINES)
        {
            ++guRaceCarContactDiagLines;
            char lacMsg[200];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[racecar-contact] crash dust car=%d n=%u speed=%.2f carry=%.3f t=%.3f at (%.2f,%.2f,%.2f)\n",
                static_cast<int>(leIndexA), static_cast<unsigned>(luNumParticles), static_cast<double>(lfSpeed),
                static_cast<double>(lrfAccumulated), static_cast<double>(lrParams.mTime),
                static_cast<double>(lvPoint.x), static_cast<double>(lvPoint.y), static_cast<double>(lvPoint.z));
            CgsDev::Log::WriteToLog(lacMsg);
        }

        for (u32 luParticle = luNumParticles; luParticle != 0u; --luParticle)
        {
            // The size's draw FIRST, then the two randomisers (0x82298A4C..0x82298AA8).
            const f32 lfSizeFraction = mRandom.RandomFloat();
            const Vector3 lvPosition = lPositionRandomiser.RandomiseXYZ(mRandom);
            const Vector4 lvVelocity4 = lVelocityRandomiser.RandomiseXYZW(mRandom);
            // `vmaddcfp128 v2, v127, v2, v0`: the car's velocity * the draw's w (the inherited share)
            // + the draw, all four lanes, fused.
            Vector3 lvVelocity;
            lvVelocity.x = std::fma(lrVelocity.x, lvVelocity4.w, lvVelocity4.x);
            lvVelocity.y = std::fma(lrVelocity.y, lvVelocity4.w, lvVelocity4.y);
            lvVelocity.z = std::fma(lrVelocity.z, lvVelocity4.w, lvVelocity4.z);
            lvVelocity.w = std::fma(lrVelocity.w, lvVelocity4.w, lvVelocity4.w);
            mParticleModule.SpawnSimple(lvPosition, lvVelocity, BrnParticle::Native::eParticleArray_CrashImpactDust,
                                        std::fma(lfSizeFraction, KF_CRASH_DUST_SIZE_RANGE, KF_CRASH_DUST_SIZE_MIN),
                                        lrParams.mTime, KF_CRASH_DUST_ALPHA);
        }
    }
}

// ------------------------------------------------------------------------------------------------
// ProcessCarDetatchedPartContacts @0x82292FA0 (308 instr, DWARF EffectsModule.cpp:3654).
//
// The deformable car parts (attached or knocked off) hitting the world. Two arms, split on the
// contact's meType at copy + 0x70, and they are NOT alternatives of the same effect:
//   * meType == 91  -> a DUST burst: CRASH IMPACT DUST simple particles (type 2) through
//                      ParticleModule::SpawnSimple, at a rate of 20 per second of scraping.
//                      (FX-CRASHVFX 2026-09-24: this arm used to announce itself, blocked on
//                      SpawnSimple / BrnSimpleParticleArray::SpawnParticle, which 2a28113a landed,
//                      and on the render half, which f5018cb0 landed.)
//                      ⛔ A CONSOLE-DEAD PATH ON RETAIL DATA -- do not try to make it fire. 91 is
//                      EBodyParts eWHEEL (DecFIGS BrnPhysicsPartTypes.h). Its two producers are
//                      CreateDetachedPartContactEvent @0x825DD628 (meType = the detached body part's
//                      IK part type -- and NONE of the 430 retail VEH_*_AT.BIN IK tables carries a
//                      part of type 87..95, surveyed 2026-09-24 with tools/re/part_bbox_dump.py's
//                      readers) and CreateDetachedWheelContactEvent @0x825B95B0, which ZEROES
//                      mVelocity (`vspltisw v0, 0 ; li r11, 0x60 ; stvx128 v0, r29, r11` at
//                      0x825B9688..0x825B969C) and nothing rewrites it. So every eWHEEL contact
//                      reaches this arm at speed 0 and fails the 0.5 gate, on the console as here.
//                      The arm is still the console's, bit for bit (tests/run_fxcrashvfx_detached_
//                      dust.py runs it against the drain's own instruction words), and the
//                      [simplefx] dust gate census below is the live proof that it stays shut.
//   * meType != 91  -> the SPARK arm.
//
// The gates ahead of both arms:
//   * an entity whose EntityId owner byte is E_ENTITYTYPE_RACECAR (1) contributes its 14-bit entity
//     index as the accumulator slot AND is skipped entirely when that car's RaceCarState says
//     mbIsHidden (`lbz r11, 0x452(r3)` == +1106); anything else uses slot 8. The console reads the
//     state through GetRaceCarStateMutable @0x8227D690, which returns &maRaceCarStates[i] and is
//     never null, and it tests the byte without a null check -- so there is none here either;
//   * the dust arm needs |mVelocity| > 0.5 m/s, the spark arm |mVelocity| > 10 m/s and the
//     surface's visualfxsurface "sparks enabled" bool at +0x4F.
// ------------------------------------------------------------------------------------------------
void EffectsModule::ProcessCarDetatchedPartContacts(
         const BrnPhysics::ContactSpy::ContactSpyData::PhysicalCarPartContactQueue* lpQueue,
         const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
         const EffectsModuleParams& lrParams)
{
    const s32 liLength = lpQueue->GetLength();          // `lwz r18, 8(r19)`

    for (s32 liIndex = 0; liIndex < liLength; ++liIndex)
    {
        // The 128-byte copy (16 std pairs): BaseContact(96) + mVelocity + meType + mbIsHinged,
        // alignas(16) => 128.
        const BrnPhysics::ContactSpy::PhysicalCarPartContact lContact = lpQueue->GetEvent(liIndex);

        // The accumulator slot -- the race car's own index, or 8 for anything that is not one.
        const CgsSceneManager::EntityId lEntityId(lContact.mEntityIdA.muValue);
        u32 luAccumulatorSlot = KU_NUM_ACTIVE_RACE_CARS;                     // `li r31, 8`

        if (lEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)
        {
            luAccumulatorSlot = lEntityId.GetEntityIndex();
            if (lpActiveRaceCars->GetRaceCarState(
                    static_cast<EActiveRaceCarIndex>(luAccumulatorSlot))->mbIsHidden)   // +1106
                continue;
        }

        const Vector3& lrVelocity = lContact.mVelocity;                       // copy + 0x60
        const f32 lfSpeed = sqrtf(lrVelocity.x * lrVelocity.x
                                + lrVelocity.y * lrVelocity.y
                                + lrVelocity.z * lrVelocity.z);

        if (lContact.meType == KI_DETACHED_DUST_PART_TYPE)
        {
            // ---- the dust arm (0x822930E8..0x8229333C) ----
            // [DIAG] BRN_SIMPLEFX_DIAG=1 -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. The gate
            // census: every eWHEEL contact this drain sees and how many of them move faster than
            // the gate below, logged at each power-of-two count (bounded). On retail data
            // `moving` stays 0 -- see the banner -- and this line is how a live run shows it.
            {
                static const bool sbDustGateDiag = BrnParticle::Native::SimpleFxDiagArmed();
                static u32 suDustGateSeen = 0, suDustGateMoving = 0, suDustGateLines = 0;
                if (sbDustGateDiag)
                {
                    ++suDustGateSeen;
                    if (lfSpeed > KF_DETACHED_DUST_MIN_SPEED)
                        ++suDustGateMoving;
                    if (suDustGateLines < KU_EFFECTS_DIAG_MAX_LINES
                        && (suDustGateSeen & (suDustGateSeen - 1u)) == 0u)
                    {
                        ++suDustGateLines;
                        char lacMsg[192];
                        std::snprintf(lacMsg, sizeof(lacMsg),
                            "[simplefx] dust gate: eWHEEL contacts=%u moving=%u (this one: slot=%u speed=%.3f)\n",
                            suDustGateSeen, suDustGateMoving, static_cast<unsigned>(luAccumulatorSlot),
                            static_cast<double>(lfSpeed));
                        CgsDev::Log::WriteToLog(lacMsg);
                    }
                }
            }
            // `vcmpgtfp.` against splat(flt_82001DA0) and a branch on the ALL-TRUE bit: a NaN
            // speed fails, like any speed at or under 0.5.
            if (!(lfSpeed > KF_DETACHED_DUST_MIN_SPEED))
                continue;

            // The accumulator gains mDt * 20 (ONE rounding: `fmadds`) and is stored back BEFORE
            // the count is taken (`stfsx` at 0x8229316C), so a frame that does not reach a whole
            // particle still banks its fraction.
            f32& lrfAccumulated = mafAccumulatedParticleCountTyres[luAccumulatorSlot];   // +0x2D014
            lrfAccumulated = std::fma(lrParams.mDt, KF_DETACHED_DUST_PER_SECOND, lrfAccumulated);

            const s32 liNumParticles = FloorToS32Fctiwz(lrfAccumulated);
            if (liNumParticles <= 0)                                          // `ble cr6`
                continue;
            lrfAccumulated -= static_cast<f32>(liNumParticles);              // fcfid, frsp, fsubs

            // [DIAG] BRN_SIMPLEFX_DIAG=1 -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. One line
            // per burst, capped, so a live run can name the frame a scraping part shed its dust.
            {
                static const bool sbDustDiag = BrnParticle::Native::SimpleFxDiagArmed();
                static u32 suDustDiagLines = 0;
                if (sbDustDiag && suDustDiagLines < KU_EFFECTS_DIAG_MAX_LINES)
                {
                    ++suDustDiagLines;
                    char lacMsg[256];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[simplefx] dust burst slot=%u n=%d speed=%.3f carry=%.4f t=%.3f at (%.2f,%.2f,%.2f)\n",
                        static_cast<unsigned>(luAccumulatorSlot), static_cast<int>(liNumParticles),
                        static_cast<double>(lfSpeed), static_cast<double>(lrfAccumulated),
                        static_cast<double>(lrParams.mTime),
                        static_cast<double>(lContact.mPointOnA.x),
                        static_cast<double>(lContact.mPointOnA.y),
                        static_cast<double>(lContact.mPointOnA.z));
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }

            for (s32 liParticle = liNumParticles; liParticle != 0; --liParticle)
            {
                // Four draws on the module's ring, in the console's order: the SIZE first, then the
                // jitter's z, y and x lanes (stored to +0x88, +0x84, +0x80 of the quad the `vaddfp`
                // at 0x82293328 adds to the normal; its w lane is the `stw r27, 0x8C` zero).
                const f32 lfSizeFraction = mRandom.RandomFloat();
                const f32 lfJitterZ = std::fma(mRandom.RandomFloat(), KF_DETACHED_DUST_JITTER_SPAN,
                                               -KF_DETACHED_DUST_JITTER_HALF);
                const f32 lfJitterY = std::fma(mRandom.RandomFloat(), KF_DETACHED_DUST_JITTER_SPAN,
                                               -KF_DETACHED_DUST_JITTER_HALF);
                const f32 lfJitterX = std::fma(mRandom.RandomFloat(), KF_DETACHED_DUST_JITTER_SPAN,
                                               -KF_DETACHED_DUST_JITTER_HALF);

                Vector3 lvVelocity = lContact.mNormal;                         // copy + 0x30
                lvVelocity.x += lfJitterX;
                lvVelocity.y += lfJitterY;
                lvVelocity.z += lfJitterZ;
                lvVelocity.w += 0.0f;

                mParticleModule.SpawnSimple(lContact.mPointOnA,                // v1: copy + 0x40
                                            lvVelocity,                        // v2
                                            BrnParticle::Native::eParticleArray_CrashImpactDust,  // `li r4, 2`
                                            std::fma(lfSizeFraction, KF_DETACHED_DUST_SIZE_RANGE,
                                                     KF_DETACHED_DUST_SIZE_MIN),               // f1
                                            lrParams.mTime,                    // f2: params + 4
                                            KF_DETACHED_DUST_ALPHA);           // f3
            }
            continue;
        }

        // ---- the spark arm (0x82293340..0x82293450) ----
        if (!(lfSpeed > KF_DETACHED_SPARK_MIN_SPEED))
            continue;

        const u32 luSurfaceId =
            (static_cast<u16>(lContact.mCollisionTagB.muValue) >> KU_SURFACE_ID_SHIFT) & KU_SURFACE_ID_MASK;

        void* lpSurfaceRef = mSurfaceList.Surfaces(luSurfaceId);
        if (!lpSurfaceRef)
            lpSurfaceRef = Attrib::DefaultDataArea(KU_SURFACE_REFSPEC_SIZE);

        Attrib::Gen::surface lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), 0);
        Attrib::Gen::visualfxsurface lVfx(VfxSurfaceRef(lSurface.GetAttributeData()), 0);
        const u8* const lpVfxData = static_cast<const u8*>(lVfx.GetAttributeData());

        // ⚠ THE GATE THE HINGED DRAIN DOES NOT HAVE: `lbz r10, 0x4F(r11) ; cmplwi ; beq` -- a
        // surface that does not have sparks enabled produces none here.
        if (lpVfxData[KU_VFX_SPARKS_ENABLED] == 0)
            continue;

        HandleSparkContacts(lContact,
                            lrVelocity,                                   // v1: the part's velocity
                            BrnParticle::Native::eSparkArray_BodyPart_Contact,  // r5 == 3
                            lrParams.mDt,                                 // f1
                            lrParams.mTime,                               // f2
                            lContact.mPointOnA.y,                         // f3 (=> height 0)
                            KF_DETACHED_MIN_FRICTION_STRESS,              // f4 == 0.005
                            *reinterpret_cast<const f32*>(lpVfxData + KU_VFX_SPARK_SCALE),  // f5
                            false);                                       // the stack byte
    }
}

// =================================================================================================
// ⭐⭐⭐ THE GLASS SMASH -- FX-CRASHVFX 2026-09-25.
//
// EffectsModule::HandleGlassSmashEventsForAllCars @0x82297420 (DWARF EffectsModule.cpp:2792) drains the
// deformation system's glass queue -- DeformationOutputInterface::mGlassSmashOrCrackQueue (+0x1AF0), or the
// replay's recorded copy while a replay plays. Every pane that went SMASHED (meNewState 2 and
// mbDontPlaySmashEffect clear) gets:
//   1. a GLASS DEBRIS burst over the pane, BurstAreaEmitParticles @0x82292160 (DWARF :2559) into
//      eDebrisArray_Glass: 320 pieces per unit of pane area (flt_820137D4), sizes 1.0..1.75 (flt_82001C98 /
//      flt_82004F68). Only for a TRAFFIC car's pane (entity type 2), or a RACE car's (type 1) while that car
//      is crashing (its ActiveRaceCarData eARDFlagIsCrashing: `lhzx` this+0x2C520 + index*0x180, bit 1);
//   2. up to three 'Glass_shattering' LION effects (mGlassSmashManager.FireGlassEffect), the count
//      floor(min(area * 3, 3) + 0.5) (flt_8200DD24, both), spaced along the pane's longer midline, each
//      turned about the pane normal by a random angle in [-3.14, 3.14) (flt_820137D8 / flt_820137DC through
//      the TrigBaseFunctions5 sin/cos, Utils::SinCosCycles);
// and after the queue mGlassSmashManager.UpdateVehicleEffectPositions re-seats the live shatter effects on
// their cars. While RECORDING, each handled pane is appended to the replay layout (SetGlassEventData
// @0x8227ED88); while PLAYING, the layout's panes replace the queue (GetGlassEventData @0x82287A48).
//
// ⚠ THE REPLAY ACCESSORS ARE CALLED AS DECLARED (the Replays tree is not this lane's). The console passes
// SetGlassEventData the four corners AND the pane normal and velocity (it packs the normal into the corners'
// w lanes, 0x8227EE70..0x8227EEB8, and the velocity into +0x260, 0x8227EECC); the declaration takes four
// opaque corner pointers, so a recorded pane loses both, and GetGlassEventData hands back the first corner's
// record as the normal instead of rebuilding it (vperm unk_82CDB450). Live play never reads the layout; a
// played-back smash has the wrong normal and velocity on PC until those two accessors are fixed
// (scratch/CRASHPARITY_0922/FOLLOWUPS.md).
// =================================================================================================
namespace
{
    // ---- BurstAreaEmitParticles' literals ----
    const f32 KF_BURST_AREA_INHERIT_SPEED_CLAMP = 40.0f;   // flt_82004D0C (the fsel clamp at 0x82292450)
    const f32 KF_BURST_AREA_SPEED_MIN_BASE      = 2.0f;    // flt_82001D9C \  the outward speed range, raised by the
    const f32 KF_BURST_AREA_SPEED_MIN_PER_MPS   = 0.01f;   // flt_82002138  | clamped inherited speed (two fmadds,
    const f32 KF_BURST_AREA_SPEED_MAX_BASE      = 10.0f;   // flt_82004A20  | 0x8229246C / 0x82292458)
    const f32 KF_BURST_AREA_SPEED_MAX_PER_MPS   = 0.15f;   // flt_82004E58 /
    const f32 KF_BURST_AREA_INHERIT_MIN         = 0.4f;    // flt_82011C18 -- the share of the inherited velocity
    const f32 KF_BURST_AREA_INHERIT_MAX         = 0.6f;    // flt_82004D00
    const f32 KF_BURST_AREA_RADIAL_MIN          = 3.0f;    // flt_8200DD24 -- the speed away from the pane's centre
    const f32 KF_BURST_AREA_RADIAL_MAX          = 8.0f;    // flt_82004C88

    // ---- HandleGlassSmashEventsForAllCars' literals ----
    const f32 KF_GLASS_DEBRIS_SIZE_MIN      = 1.0f;     // flt_82001C98 (f2 at 0x82297888)
    const f32 KF_GLASS_DEBRIS_SIZE_MAX      = 1.75f;    // flt_82004F68 (f3 at 0x82297884)
    const f32 KF_GLASS_DEBRIS_DENSITY       = 320.0f;   // flt_820137D4 (f4 at 0x82297880)
    const f32 KF_GLASS_SHATTER_PER_AREA     = 3.0f;     // flt_8200DD24 (`stfs f25, 0x90(r1)`)
    const f32 KF_GLASS_SHATTER_MAX          = 3.0f;     // flt_8200DD24 (`stfs f25, 0x94(r1)`)
    const f32 KF_GLASS_SHATTER_ANGLE_RANGE  = 6.28f;    // flt_820137D8
    const f32 KF_GLASS_SHATTER_ANGLE_OFFSET = 3.14f;    // flt_820137DC

    // Four-lane VMX arithmetic, w included (vaddfp / vsubfp / vmulfp, and the fused vmaddfp by a splat).
    Vector3 Add4(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lv;
        lv.x = lrA.x + lrB.x; lv.y = lrA.y + lrB.y; lv.z = lrA.z + lrB.z; lv.w = lrA.w + lrB.w;
        return lv;
    }

    Vector3 Sub4(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lv;
        lv.x = lrA.x - lrB.x; lv.y = lrA.y - lrB.y; lv.z = lrA.z - lrB.z; lv.w = lrA.w - lrB.w;
        return lv;
    }

    Vector3 Mul4(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lv;
        lv.x = lrA.x * lrB.x; lv.y = lrA.y * lrB.y; lv.z = lrA.z * lrB.z; lv.w = lrA.w * lrB.w;
        return lv;
    }

    Vector3 MaddSplat4(const Vector3& lrA, f32 lfB, const Vector3& lrC)   // a * splat(b) + c, rounded ONCE
    {
        Vector3 lv;
        lv.x = std::fma(lrA.x, lfB, lrC.x);
        lv.y = std::fma(lrA.y, lfB, lrC.y);
        lv.z = std::fma(lrA.z, lfB, lrC.z);
        lv.w = std::fma(lrA.w, lfB, lrC.w);
        return lv;
    }

    Vector3 MakeVector3(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        Vector3 lv;
        lv.x = lfX; lv.y = lfY; lv.z = lfZ; lv.w = lfW;
        return lv;
    }

    // CgsNumeric::Random::RandomUnitVector, which the console inlines at 0x822925AC..0x82292688 (no symbol of
    // its own): a point drawn uniformly in the unit disc by rejection, two RandomFloat() each try (x first),
    // each mapped by `fmsubs r, 2.0, 1.0`; the length is one fused `fmadds y*y + x*x`; then Marsaglia's lift
    // onto the sphere, (2 sqrt(1 - s) x, 2 sqrt(1 - s) y, 2s - 1), w 0.
    Vector3 RandomUnitVector(CgsNumeric::Random& lrRandom)
    {
        f32 lfX, lfY, lfLengthSquared;
        do
        {
            lfX = std::fma(lrRandom.RandomFloat(), 2.0f, -1.0f);
            const f32 lfXSquared = lfX * lfX;
            lfY = std::fma(lrRandom.RandomFloat(), 2.0f, -1.0f);
            lfLengthSquared = std::fma(lfY, lfY, lfXSquared);
        } while (lfLengthSquared >= 1.0f);
        const f32 lfScale = std::sqrt(1.0f - lfLengthSquared) * 2.0f;
        return MakeVector3(lfScale * lfX, lfScale * lfY, std::fma(lfLengthSquared, 2.0f, -1.0f), 0.0f);
    }

    // The replay layout's glass-event count (+0x30), which the drain reads INLINE while playing back
    // (`lwz r15, 0x30(r3)` at 0x822974B0) -- the layout class publishes the offset and has no accessor.
    s32 NumRecordedGlassEvents(const EffectsStaticLayout* lpLayout)
    {
        return *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpLayout)
                                             + EffectsStaticLayout::KI_OFF_NUM_GLASS_EVENTS);
    }

    // [DIAG] BRN_GLASS_DIAG=1 -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. One capped line per glass event
    // the drain reads (cracked panes included), naming the pane's car, its state and what the drain did with
    // it: the debris burst (emit) and the number of shatter effects.
    void GlassSmashWitness(s32 liEvent, s32 liNumEvents, const BrnPhysics::Deformation::GlassSmashOrCrackEvent& lrEvent,
                           bool lbPlayback, bool lbHandled, bool lbEmit, f32 lfNumEffects, f32 lfTime)
    {
        static const bool sbArmed = []() {
            const char* const lpcValue = std::getenv("BRN_GLASS_DIAG");
            return lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0';
        }();
        static u32 suLines = 0;
        if (!sbArmed || suLines >= 200u)
            return;
        ++suLines;
        char lacMsg[320];
        std::snprintf(lacMsg, sizeof(lacMsg),
            "[glass] event %d/%d%s id=0x%08X owner=%u state=%d dont=%d handled=%d emit=%d shatters=%.0f "
            "normal=(%.3f,%.3f,%.3f) vel=(%.2f,%.2f,%.2f) t=%.3f\n",
            liEvent, liNumEvents, lbPlayback ? " (replay)" : "", static_cast<unsigned>(lrEvent.mVehicleEntityId.muValue),
            static_cast<unsigned>(lrEvent.mVehicleEntityId.muValue >> 24),
            lbPlayback ? -1 : static_cast<int>(lrEvent.meNewState), lbPlayback ? -1 : (lrEvent.mbDontPlaySmashEffect ? 1 : 0),
            lbHandled ? 1 : 0, lbEmit ? 1 : 0, static_cast<double>(lfNumEffects),
            static_cast<double>(lrEvent.mNormal.x), static_cast<double>(lrEvent.mNormal.y),
            static_cast<double>(lrEvent.mNormal.z), static_cast<double>(lrEvent.mLinearVelocity.x),
            static_cast<double>(lrEvent.mLinearVelocity.y), static_cast<double>(lrEvent.mLinearVelocity.z),
            static_cast<double>(lfTime));
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

// -------------------------------------------------------------------------------------------------
// BurstAreaEmitParticles @0x82292160 (346 instr). The pane is the bilinear patch over its four corners,
// P(u, v) = A + u (B - A) + v (D - A) + u v (A - B + C - D). lfArea * lfParticleDensity pieces (the loop runs
// while its VecFloat count is below that, so a fractional count rounds up), each:
//   lParamsA = RandomiseXYZ over (2 + 0.01 c, 0.4, 3) .. (10 + 0.15 c, 0.6, 8), c = |inherited velocity|
//              clamped to 40 -- the outward speed, the share of the inherited velocity, the radial speed;
//   lParamsB = RandomiseXYZ over (0, 0, sqrt(min size)) .. (1, 1, sqrt(max size)) -- the spot (u, v) and the
//              root of the size (so the size is its square);
//   position   P(u, v);
//   velocity   normal * (4u(1-u) * 4v(1-v) * outward speed) + inherited * share + (P - centre) * radial speed
//              -- fastest from the middle of the pane, as glass bows out;
//   spin axis  CgsNumeric::Random::RandomUnitVector; colour white (the array's own colour wins for every
//              type but eDebrisArray_Coloured).
// Every multiply-add the console fuses is fused here (vmaddfp / vmaddcfp128 / fmadds: std::fma), every
// other operation rounds where it does; the vector idioms are the ones ProcessRaceCarContacts uses (Dot3,
// CrossPermuted, GuardedLength3 -- see their banner).
// -------------------------------------------------------------------------------------------------
void EffectsModule::BurstAreaEmitParticles(Vector3* lpCorners,
                                           Vector3 lvNormal,
                                           Vector3 lvInheritedVelocity,
                                           BrnParticle::Native::EDebrisArrayID leDebrisType,
                                           f32 lfCurrentTime,
                                           f32 lfSizeMin,
                                           f32 lfSizeMax,
                                           f32 lfParticleDensity)
{
    CGS_ASSERT(lfSizeMax >= lfSizeMin, "lfSizeMax >= lfSizeMin");            // fcmpu / bge (a NaN fires it)
    CGS_ASSERT(lfSizeMin > 0.0f, "lfSizeMin > 0.0f");
    CGS_ASSERT(lfSizeMax > 0.0f, "lfSizeMax > 0.0f");
    CGS_ASSERT(lfParticleDensity > 0.0f, "lfParticleDensity > 0.0f");

    const Vector3 lCornerA = lpCorners[0];
    const Vector3 lCornerB = lpCorners[1];
    const Vector3 lCornerC = lpCorners[2];
    const Vector3 lCornerD = lpCorners[3];

    const Vector3 lBminusA = Sub4(lCornerB, lCornerA);
    const Vector3 lCminusA = Sub4(lCornerC, lCornerA);
    const Vector3 lDminusA = Sub4(lCornerD, lCornerA);
    // The DWARF names this local lDminusB (:2583); the console subtracts corner 1 from corner 2 (`vsubfp v11,
    // v12, v11` at 0x82292284), and the patch's cross term is built from it: (C - B) - (D - A) = A - B + C - D
    // (`vsubfp128 v121, v11, v125` at 0x822922B8).
    const Vector3 lDminusB = Sub4(lCornerC, lCornerB);
    const Vector3 lAminusBplusCminusB = Sub4(lDminusB, lDminusA);
    // ((A + B) + C) + D, then one vmulfp128 by 0.5 * 0.5.
    const Vector3 lCentrePos = Scale4(Add4(Add4(Add4(lCornerA, lCornerB), lCornerC), lCornerD), 0.25f);

    // The two triangles' parallelograms, halved: (|ABC| + |ACD|) * 0.5 (vaddfp v11, v11, v9 at 0x82292388).
    const Vector3 lCrossABC = CrossPermuted(lBminusA, lCminusA);
    const Vector3 lCrossACD = CrossPermuted(lCminusA, lDminusA);
    const f32 lfArea = (GuardedLength3(lCrossABC) + GuardedLength3(lCrossACD)) * 0.5f;
    const f32 lfSpawnCount = lfArea * lfParticleDensity;

    const f32 lfInheritedVelocityMagnitude = GuardedLength3(lvInheritedVelocity);
    // `fsubs f10, |v|, 40 ; fsel f0, f10, 40, |v|` -- a NaN magnitude passes through.
    const f32 lfInheritedVelocityMagnitudeClamped =
        ((lfInheritedVelocityMagnitude - KF_BURST_AREA_INHERIT_SPEED_CLAMP) >= 0.0f)
            ? KF_BURST_AREA_INHERIT_SPEED_CLAMP : lfInheritedVelocityMagnitude;

    Utils::Vector3Randomiser lRandomiserA;
    lRandomiserA.Prepare(
        MakeVector3(std::fma(lfInheritedVelocityMagnitudeClamped, KF_BURST_AREA_SPEED_MIN_PER_MPS, KF_BURST_AREA_SPEED_MIN_BASE),
                    KF_BURST_AREA_INHERIT_MIN, KF_BURST_AREA_RADIAL_MIN, 0.0f),
        MakeVector3(std::fma(lfInheritedVelocityMagnitudeClamped, KF_BURST_AREA_SPEED_MAX_PER_MPS, KF_BURST_AREA_SPEED_MAX_BASE),
                    KF_BURST_AREA_INHERIT_MAX, KF_BURST_AREA_RADIAL_MAX, 0.0f));
    // Vector3(x, y, z) is built with the vperm mask unk_82CDA350 {x, y', x, x} and a vrlimi of z, so its w lane
    // is its x: (0, 0, sqrt min, 0) and (1, 1, sqrt max, 1).
    Utils::Vector3Randomiser lRandomiserB;
    lRandomiserB.Prepare(MakeVector3(0.0f, 0.0f, std::sqrt(lfSizeMin), 0.0f),
                         MakeVector3(1.0f, 1.0f, std::sqrt(lfSizeMax), 1.0f));

    for (f32 lfCount = 0.0f; lfCount < lfSpawnCount; lfCount += 1.0f)       // vcmpgtfp128. / vaddfp128
    {
        const Vector3 lParamsA = lRandomiserA.RandomiseXYZ(mRandom);
        const Vector3 lParamsB = lRandomiserB.RandomiseXYZ(mRandom);
        const f32 lfU = lParamsB.x;
        const f32 lfV = lParamsB.y;

        // P(u, v) as three vmaddfp: (A + (D - A) v) + ((B - A) + (A - B + C - D) v) u.
        const Vector3 lSpawnPos = MaddSplat4(MaddSplat4(lAminusBplusCminusB, lfV, lBminusA), lfU,
                                             MaddSplat4(lDminusA, lfV, lCornerA));

        // (lParamsB - lParamsB^2) * 4 per lane: x and y are 4u(1-u) and 4v(1-v).
        const Vector3 lOutVelScale = Scale4(Sub4(lParamsB, Mul4(lParamsB, lParamsB)), 4.0f);
        const f32 lfOutwardsSpeed = (lOutVelScale.x * lOutVelScale.y) * lParamsA.x;

        const Vector3 lInheritedVelocity = Scale4(lvInheritedVelocity, lParamsA.y);
        // normal * outward speed + inherited (one vmaddfp128), + (P - centre) * radial (one vmaddfp).
        const Vector3 lSpawnVelocity = MaddSplat4(Sub4(lSpawnPos, lCentrePos), lParamsA.z,
                                                  MaddSplat4(lvNormal, lfOutwardsSpeed, lInheritedVelocity));
        const f32 lfSizeScale = lParamsB.z * lParamsB.z;

        const Vector3 lRotationAxis = RandomUnitVector(mRandom);
        Vector4 lvWhite;
        lvWhite.x = 1.0f; lvWhite.y = 1.0f; lvWhite.z = 1.0f; lvWhite.w = 1.0f;   // v4 = v127, the 1.0 splat
        mParticleModule.SpawnDebris(leDebrisType, lSpawnPos, lSpawnVelocity, lRotationAxis, lvWhite,
                                    lfSizeScale, lfCurrentTime);
    }
}

// -------------------------------------------------------------------------------------------------
// HandleGlassSmashEventsForAllCars @0x82297420 (491 instr) -- see the region banner above.
// -------------------------------------------------------------------------------------------------
void EffectsModule::HandleGlassSmashEventsForAllCars(const EffectsIO::InputBuffer* lpInputBuffer,
                                                     const RCEntityActiveRaceCarOutputInterface* /*lpActiveRaceCars*/,
                                                     f32 /*lfDt*/, f32 lfTime)
{
    typedef BrnPhysics::Deformation::GlassSmashOrCrackEvent GlassEvent;
    const DeformationOutputInterface::GlassSmashOrCrackQueue& lrQueue =
        lpInputBuffer->GetDeformationInterface()->mGlassSmashOrCrackQueue;

    s32 liNumEvents = lrQueue.GetLength();                                     // `lwz r15, 8(r16)`
    if (IsReplayPlayback(mEffectsSerialiser.GetMode()))
        liNumEvents = NumRecordedGlassEvents(mEffectsSerialiser.GetStaticLayout());

    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        GlassEvent lEvent;
        const bool lbPlayback = IsReplayPlayback(mEffectsSerialiser.GetMode());   // re-read per event
        if (lbPlayback)
        {
            // The recorded pane (index as a byte, `clrlwi r4, r18, 0x18`): id, transform, corners, normal,
            // velocity -- called as declared, see the banner.
            mEffectsSerialiser.GetStaticLayout()->GetGlassEventData(
                static_cast<u8>(liEvent), &lEvent.mVehicleEntityId.muValue, &lEvent.mTransform,
                &lEvent.maCorners[0], &lEvent.maCorners[1], &lEvent.maCorners[2], &lEvent.maCorners[3],
                &lEvent.mNormal, &lEvent.mLinearVelocity);
        }
        else
        {
            lEvent = lrQueue.GetEvent(liEvent);
            if (lEvent.meNewState != BrnPhysics::Deformation::E_GLASS_STATE_SMASHED || lEvent.mbDontPlaySmashEffect)
            {
                GlassSmashWitness(liEvent, liNumEvents, lEvent, false, false, false, 0.0f, lfTime);   // [DIAG]
                continue;
            }
            if (IsReplayRecording(mEffectsSerialiser.GetMode()))
            {
                // Called as declared: the normal and the velocity have no slot (see the banner).
                mEffectsSerialiser.GetStaticLayout()->SetGlassEventData(
                    static_cast<int>(lEvent.mVehicleEntityId.muValue), &lEvent.mTransform,
                    &lEvent.maCorners[0], &lEvent.maCorners[1], &lEvent.maCorners[2], &lEvent.maCorners[3]);
            }
        }

        // ---- the debris: a traffic car's pane, or a crashing race car's ----
        const u32 luOwner = lEvent.mVehicleEntityId.muValue >> 24;
        bool lbRaceCarIsCrashing = false;
        if (luOwner == 1u)
            lbRaceCarIsCrashing = maActiveRaceCarData[(lEvent.mVehicleEntityId.muValue >> 10) & 0x3FFFu].IsCrashing();
        const bool lbEmit = (luOwner == 2u) || (luOwner == 1u && lbRaceCarIsCrashing);
        if (lbEmit)
        {
            BurstAreaEmitParticles(lEvent.maCorners, lEvent.mNormal, lEvent.mLinearVelocity,
                                   BrnParticle::Native::eDebrisArray_Glass, lfTime,
                                   KF_GLASS_DEBRIS_SIZE_MIN, KF_GLASS_DEBRIS_SIZE_MAX, KF_GLASS_DEBRIS_DENSITY);
        }

        // ---- the shatter effects: floor(min(area * 3, 3) + 0.5) ----
        const Vector3& lCornerA = lEvent.maCorners[0];
        const Vector3 lCminusA = Sub4(lEvent.maCorners[2], lCornerA);
        const Vector3 lDminusA = Sub4(lEvent.maCorners[3], lCornerA);
        const Vector3 lDminusB = Sub4(lEvent.maCorners[3], lEvent.maCorners[1]);
        const Vector3 lBminusA = Sub4(lEvent.maCorners[1], lCornerA);
        // |ABC| + |ACD| in that order (`vaddfp v12, v11, v12` at 0x822979A4), halved, then scaled and capped
        // (`vminfp` keeps a NaN: the comparison is false for it), + 0.5, floor.
        const f32 lfArea = (GuardedLength3(CrossPermuted(lBminusA, lCminusA))
                          + GuardedLength3(CrossPermuted(lCminusA, lDminusA))) * 0.5f;
        const f32 lfScaled = lfArea * KF_GLASS_SHATTER_PER_AREA;
        const f32 lfCapped = (lfScaled > KF_GLASS_SHATTER_MAX) ? KF_GLASS_SHATTER_MAX : lfScaled;
        f32 lfNumEffects = std::floor(lfCapped + 0.5f);                           // vrfim128
        GlassSmashWitness(liEvent, liNumEvents, lEvent, lbPlayback, true, lbEmit, lfNumEffects, lfTime);   // [DIAG]
        if (!(lfNumEffects >= 1.0f))                                              // vcmpgefp128.
            continue;

        // The pane's two midlines, (C - A) -/+ (D - B), halved; the effects walk the longer one from the
        // middle of the edge it starts on, half a step in, one step apart.
        const Vector3 lvMidlineA = Scale4(Sub4(lCminusA, lDminusB), 0.5f);
        const Vector3 lvMidlineB = Scale4(Add4(lCminusA, lDminusB), 0.5f);
        const bool lbAIsLonger = Dot3(lvMidlineA, lvMidlineA) > Dot3(lvMidlineB, lvMidlineB);   // vcmpgtfp / vsel
        const Vector3& lvLonger    = lbAIsLonger ? lvMidlineA : lvMidlineB;
        const Vector3& lvStartEdge = lbAIsLonger ? lDminusA : lBminusA;
        const Vector3 lvStep = Scale4(lvLonger, RefinedRecip(lfNumEffects));
        // `vmaddcfp128 v124, v0, v124, v125`: 0.5 * (edge + step) + A, rounded once per lane.
        Vector3 lvPosition = MaddSplat4(Add4(lvStartEdge, lvStep), 0.5f, lCornerA);

        // The effect frame: x = unit(up x normal) (no zero guard -- a pane facing straight up gives the
        // console's NaNs), z-row the normal, and x / (normal x x) turned by the random angle.
        const Vector3& lvNormal = lEvent.mNormal;
        const Vector3 lvCross = CrossPermuted(AxisY(), lvNormal);
        const Vector3 lvAxisX = Scale4(lvCross, RefinedRsqrt(Dot3(lvCross, lvCross)));
        const Vector3 lvAxisZ = CrossPermuted(lvNormal, lvAxisX);

        do
        {
            const f32 lfAngle = std::fma(mRandom.RandomFloat(), KF_GLASS_SHATTER_ANGLE_RANGE,
                                         -KF_GLASS_SHATTER_ANGLE_OFFSET);          // fmsubs
            f32 lfSin, lfCos;
            Utils::SinCosCycles(lfAngle, lfSin, lfCos);

            Matrix44Affine lEffectTransform;
            lEffectTransform.xAxis = MaddSplat4(lvAxisX, lfCos, Scale4(lvAxisZ, lfSin));   // x cos + z sin
            lEffectTransform.yAxis = Sub4(Scale4(lvAxisZ, lfCos), Scale4(lvAxisX, lfSin)); // z cos - x sin
            lEffectTransform.zAxis = lvNormal;
            lEffectTransform.wAxis = lvPosition;
            mGlassSmashManager.FireGlassEffect(lEffectTransform, lEvent.mTransform, lEvent.mVehicleEntityId, lfTime);

            lfNumEffects -= 1.0f;                                                  // vsubfp128
            lvPosition = Add4(lvPosition, lvStep);                                 // vaddfp128
        } while (lfNumEffects >= 1.0f);
    }

    mGlassSmashManager.UpdateVehicleEffectPositions(lpInputBuffer, lfTime);
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
