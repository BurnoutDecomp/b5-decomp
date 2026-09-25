#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"  // RaceCarPhysics::SetCrashing (declare-only callee)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                    // CgsContainers::BitArray<N> (crash-data free-list + taken-down bitset)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"     // CgsDev::PerfMonCpu::AddMonitor -- Construct's thirty monitors
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT -- the two asserts Construct fires
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                  // KU_INVALID_ENTITY_ID (dword_82F2A3A4)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                      // K_INVALID_RIGID_BODY_ID (qword_82F2A3A8)
#include "rw/math/vpu/vector3_operation.h"                                    // vpu::Dot (T-bone side-speed gates, wave B3b)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // gpDebugPrint ([bringup] crash-entry banner)
#include "GameSource/World/BrnEntityTypes.h"                                   // BrnWorld::EEntityTypeID -- the owner byte the crash witness names

#include <cstdlib>  // getenv: opt-in PC contact diagnostics
#include <cmath>    // std::fabs, std::acos
#include <cstring>  // std::memcpy: the [td-replay] witness prints raw IEEE bits
#include <cstddef>  // offsetof (layout asserts)

// includes folded in from the BrnVehicleManager_w*.cpp partfiles (2026-09-15)
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"   // the four tables + gbReadSurfaceProperties
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/physicssurface.h"
#include "GameSource/AttribSys/Generated/classes/gameplaysurface.h"

// BrnPhysics::Vehicle::VehicleManager -- the car-vs-car takedown chain.
// This TU bodies the contact entry point HandleRaceCarRaceCarContact (STAGE 1), the classifier
// entry point CheckForAllTypesOfImpacts (STAGE 2), the commit routine InstantTakedown (STAGE 3a),
// the universal crash-commit sink SetRaceCarCrashing (STAGE 3b), and the eight per-type
// sub-classifiers (the full 64-function VehicleManager is built out by its own reconstruction
// passes). The bodies use NAMED access only -- the RaceCarResponseInfo fields, the deep
// VehicleManager members (BrnVehicleManager.h §7 layout), the per-car maRaceCarVehicles[idx]
// records (and their named in-record fields), and the contact-normal/point via
// mpContact->mNormal/mPointOnA. The SIMD plane-geometry sub-tests (IsPointBetweenTwoParallelPlanes,
// CheckForVerticalTakedownSituation), the recency throttle (HasRaceCarHadRecentImpact), the grind
// detector (CheckForGrindingAndRubbing), the situation resolver (GenerateContactSituation), the
// force appliers (ApplySlam/ApplyShunt) and the per-vehicle physics latches
// (RaceCarPhysics::SetCrashing, VehiclePhysics::SetWheelVelocities/IsBeingSlamedOrShuntedByRaceCar)
// are declared-only callees, bodied by their own TUs (FLAGged at their use sites).
//
// SetRaceCarCrashing + HandleRaceCarRaceCarContact are the TWO functions where Hex-Rays' local
// allocation FAILED ("local variable allocation has failed"); they are reconstructed from the
// asm-traced blueprints (scratchpad td_C3 / td_C4) + the raw __asm blocks, NOT the unreliable
// pseudocode. Every step that could not be pinned from the blueprint/asm is FLAGged inline.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;   // vpu::Dot (T-bone side-speed gates, wave B3b)

    namespace
    {
        // VecFloat is Vector4 here and has no scalar constructor; the console's `vspltw` splat.
        // (Same helper the traffic TU keeps file-locally -- neither is exported.)
        inline VecFloat SplatVecFloat_VM(f32 lfValue)
        {
            VecFloat lvfResult;
            lvfResult.x = lfValue; lvfResult.y = lfValue;
            lvfResult.z = lfValue; lvfResult.w = lfValue;
            return lvfResult;
        }

        // The console's `vmsum3fp128` -- a three-lane dot product left splatted across the
        // register. Every call site here consumes one lane, so it reduces to the scalar sum.
        inline f32 Dot3_VM(const Vector3& lrA, const Vector3& lrB)
        {
            return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
        }

        // The console's length idiom: dot3 the vector with itself, take an estimated reciprocal
        // square root, refine it with two Newton steps, multiply back -- and vsel a hard zero over
        // the result when the squared length compared equal to zero (the rsqrt of 0 is infinite).
        // The two Newton steps only chase the estimate's error, so the host takes the exact root.
        inline f32 Length3_VM(const Vector3& lrV)
        {
            const f32 lfLengthSq = lrV.x * lrV.x + lrV.y * lrV.y + lrV.z * lrV.z;
            if (lfLengthSq == 0.0f)
                return 0.0f;   // asm vcmpeqfp128 + vsel against the zero register
            return std::sqrt(lfLengthSq);
        }

        // The car-vs-car impact speed CheckForHittingAlreadyCrashingCar builds for each of its two
        // arms, read from asm @0x8263DC30..DC68 and @0x8263DE40..DE78 (identical but for which car
        // is the victim):
        //     lvx128 v13, mpContact, 0x30        ; the contact normal
        //     lvx128 v125/126, victimPhys, 0x20  ; victimPhys+0x20 == mTransform.yAxis (Up)
        //     vmsum3fp128 / vmulfp128 / vsubfp   ; flat = normal - up*dot3(up, normal)
        //     lvx128 v12, lpInfo, 0x30           ; lpInfo+0x30 == mClosingVelocityAtoB
        //     vmsum3fp128 v13, v13, v12          ; dot3(flat, closingVelocity)
        //     vandc v1, v13, sign-mask           ; fabs
        // This is the SAME construction the race-car-vs-TRAFFIC arm uses
        // (BrnVehicleManager_RaceCarTrafficContact.cpp:306-312), except that arm projects out the
        // RACE CAR's Up while this one projects out the VICTIM's -- which is what the two distinct
        // lvx bases (r30 vs r29, swapped between the arms) say.
        inline f32 CarCarImpactSpeed(const Vector3& lrContactNormal,
                                     const Vector3& lrVictimUp,
                                     const Vector3& lrClosingVelocity)
        {
            const f32 lfAlongUp = lrContactNormal.x * lrVictimUp.x
                                + lrContactNormal.y * lrVictimUp.y
                                + lrContactNormal.z * lrVictimUp.z;
            const f32 lfFlatX = lrContactNormal.x - lrVictimUp.x * lfAlongUp;
            const f32 lfFlatY = lrContactNormal.y - lrVictimUp.y * lfAlongUp;
            const f32 lfFlatZ = lrContactNormal.z - lrVictimUp.z * lfAlongUp;
            return std::fabs(lfFlatX * lrClosingVelocity.x
                           + lfFlatY * lrClosingVelocity.y
                           + lfFlatZ * lrClosingVelocity.z);
        }

        // CheckForHittingAlreadyCrashingCar passes a flat 1.0 scale on both arms
        // (vspltisw v124,1 ; vcsxwfp128 v127,v124,0 ; vmr128 v2,v127 at both call sites).
        const f32 KF_PILEON_CRASH_THRESHOLD_SCALE = 1.0f;

        // CheckForPlayerSlammingAIIntoAI selects its scale per victim through the shared vsel mask
        // pair at 0x8327F240: TRUE -> 2.0f (materialised inline), FALSE -> unk_82FB8320, which its
        // static-init thunk @0x82C5BB60..BB84 fills with splat(flt_82004018) == 0.75f. BOTH values
        // are read from the image; neither is a guess. The selector predicate itself is reconstructed
        // in CheckForPlayerSlammingAIIntoAI from the victim's own VehiclePhysics fields.
        const f32 KF_SLAM_REVENGE_CRASH_THRESHOLD_SCALE = 2.0f;   // inline vspltisw/vcfsx
        const f32 KF_SLAM_DEFAULT_CRASH_THRESHOLD_SCALE = 0.75f;  // unk_82FB8320 <- flt_82004018

        // Shunt/nudge alignment cap: the summed |dot| of each car's At axis against the contact normal
        // must stay BELOW this, or the contact is too head-on to be a shunt.
        static const f32 KF_SHUNT_ALIGNMENT_MIN    = 1.9f;    // flt_8207104C (image-read)
        // How much of a car's own deformable-AABB rear extent still counts as "the front of the car"
        // when deciding which car drove into which. A contact point further back than this does not
        // make that car the one doing the hitting.
        static const f32 KF_CONTACT_REAR_EXTENT_SCALE = 0.5f; // flt_82FB7F40, image-read via its initialiser
        // Revenge windows on VehiclePhysics' time-since-last-race-car-contact lane (seconds). The
        // player only counts as a car's current attacker while its last contact is this recent.
        static const f32 KF_REVENGE_WINDOW_SLAM    = 0.25f;   // unk_82FB7F80, image-read via its initialiser
        static const f32 KF_REVENGE_WINDOW_PILEON  = 1.0f;    // asm vcsxwfp128 v124,0 -> 1.0f
        // Pile-on timing band on the crashing car's time-crashing lane (seconds): it must have been
        // crashing longer than the floor to be a pile-on target at all, and less than the ceiling for
        // the player-revenge takedown to register.
        static const f32 KF_PILEON_MIN_TIME_CRASHING     = 1.0f;   // asm vcsxwfp128 v124,0 -> 1.0f
        static const f32 KF_PILEON_REVENGE_MAX_CRASHING  = 0.5f;   // asm vcsxwfp128 v124,1 -> 0.5f
        // Aggressive-driving veto: a player slam is suppressed while the relevant grinding frame
        // counter is still below this many frames.
        static const u8  KU_GRINDING_FRAMES_VETO   = 30;      // asm cmplwi 0x1E
        // ...and only while the slam-steer itself is still below this bound.
        static const f32 KF_GRINDING_VETO_STEER_BOUND = 0.5f; // flt_82001DA0 (image-read)
        // Per-car-type slam severity speed, indexed by BrnWorld::ERaceCarType. Above the entry the
        // in-band contact is a SLAM, at or below it trading paint. The console reads this flat table
        // with no bounds check (`clrlslwi` index scale only).
        static const f32 KAF_SLAM_SEVERITY_SPEED_BY_CAR_TYPE[8] =
            { 0.1f, 0.3f, 0.1f, 0.1f, 2.0f, 4.0f, 2.0f, 0.0f };   // flt_82F2A218 (image-read)
    }


    // The minimum combined closing speed below which a contact is too gentle to be any kind of
    // takedown/shunt (X360 reads the rodata float at flt_82FB8290 for the `>=` gate).
    // ⭐ RECOVERED 2026-08-24 (deform-land wave, P5/P9; physics11 audit, static-init writer
    // 0x82C5BB18 decoded via headless idat): flt_82FB8290 = flt_82F31928 * flt_820138DC
    // = 0.44704 * 50.0 == 50 MPH in m/s. The old 0.0 made the gate a pass-through.
    static const f32 KF_MIN_IMPACT_SPEED_SUM = 0.44704f * 50.0f;   // flt_82FB8290 <- init 0x82C5BB18 (22.352 m/s)

    // --------------------------------------------------------------------------------------
    // [td-contact] / [td-crash] PC witness helpers (NOT in the console): enum -> name, so the
    // witness lines below read as verdicts instead of bare integers. [FLAG PC witness]
    // DELETE-WHEN: the two witnesses below are deleted (organic takedown case green).
    // --------------------------------------------------------------------------------------
    // [PC HARNESS, NOT X360] BRN_TD_DIAG=1 turns on the takedown-classification trace
    // ([td-ladder] / [td-vert] / [td-tbone] / [td-instant] here, [td-detect] in the takedown
    // manager, [td-gui] in the game->gui bridge, [td-msg] in the HUD message analyzer). Off for
    // every run that does not set it. DELETE-WHEN every takedown type is proven on film.
    static bool TakedownDiagEnabled()
    {
        static const bool sbOn = (std::getenv("BRN_TD_DIAG") != 0);
        return sbOn;
    }

    // [PC HARNESS, NOT X360] BRN_TD_REPLAY=1 (with BRN_TD_DIAG=1) adds the [td-replay] line: every
    // input the impact ladder reads for a player-involved evaluation, as raw IEEE bits, so
    // tests/FxLadderReplay.cpp can feed a recorded contact through the extracted production ladder
    // bit for bit. DELETE-WHEN the FX-LADDER replay fixture no longer needs fresh recordings.
    static bool TakedownReplayEnabled()
    {
        static const bool sbOn = (std::getenv("BRN_TD_REPLAY") != 0);
        return sbOn;
    }

    static const char* WitnessImpactTypeName(s32 liImpactType)
    {
        switch (liImpactType)
        {
        case E_IMPACT_NONE:          return "none";
        case E_IMPACT_TRADING_PAINT: return "trading-paint";
        case E_IMPACT_NUDGE:         return "nudge";
        case E_IMPACT_SLAM:          return "slam";
        case E_IMPACT_SHUNT:         return "shunt";
        case E_IMPACT_BOOST_SLAM:    return "boost-slam";
        case E_IMPACT_BOOST_SHUNT:   return "boost-shunt";
        case E_IMPACT_GRINDING:      return "grinding";
        case E_IMPACT_RUBBING:       return "rubbing";
        default:                     return "?";
        }
    }

    static const char* WitnessTakedownTypeName(s32 liTakedownType)
    {
        switch (liTakedownType)
        {
        case BrnGameState::E_TAKEDOWN_NONE:          return "none";
        case BrnGameState::E_TAKEDOWN_STANDARD:      return "standard";
        case BrnGameState::E_TAKEDOWN_GRINDING:      return "grinding";
        case BrnGameState::E_TAKEDOWN_T_BONE:        return "t-bone";
        case BrnGameState::E_TAKEDOWN_VERTICAL:      return "vertical";
        case BrnGameState::E_TAKEDOWN_TRAFFIC_CHECK: return "traffic-check";
        case BrnGameState::E_TAKEDOWN_HEAD_ON:       return "head-on";
        case BrnGameState::E_TAKEDOWN_UNKNOWN0:      return "unknown0";
        case BrnGameState::E_TAKEDOWN_UNKNOWN1:      return "unknown1";
        case BrnGameState::E_TAKEDOWN_DOUBLE:        return "double";
        case BrnGameState::E_TAKEDOWN_REVENGE:       return "revenge";
        case BrnGameState::E_TAKEDOWN_INTO_CAR:      return "into-car";
        case BrnGameState::E_TAKEDOWN_INTO_VAN:      return "into-van";
        case BrnGameState::E_TAKEDOWN_INTO_BUS:      return "into-bus";
        default:                                     return "?";
        }
    }

    // The OWNER byte of an EntityId word, by name. The crash record's aggressor seat is an
    // EntityId, never a slot index: "no attacker" is spelled by the OWNER (world / traffic /
    // the victim's own id), and the entity field means nothing until the owner is read. A witness
    // that printed only bits 10..23 rendered a WORLD-owned aggressor as "attacker 0" -- which
    // reads as the player's race-car slot and is what made every self-inflicted rival crash look
    // like the player was credited for it.
    static const char* WitnessEntityOwnerName(u32 luOwner)
    {
        switch (static_cast<s32>(luOwner))
        {
        case BrnWorld::E_ENTITYTYPE_WORLD:                   return "world";
        case BrnWorld::E_ENTITYTYPE_RACECAR:                 return "racecar";
        case BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE:         return "traffic";
        case BrnWorld::E_ENTITYTYPE_PROP:                    return "prop";
        case BrnWorld::E_ENTITYTYPE_TRIGGER:                 return "trigger";
        case BrnWorld::E_ENTITYTYPE_WORLD_GRAPHICS:          return "world-graphics";
        case BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART: return "racecar-part";
        case BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART: return "traffic-part";
        case BrnWorld::E_ENTITYTYPE_RACECAR_WHEEL:           return "racecar-wheel";
        case BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL:  return "detached-racecar-wheel";
        case BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL:  return "detached-traffic-wheel";
        case BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR:  return "prop-collision-racecar";
        case BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC:  return "prop-collision-traffic";
        default:                                             return "?";
        }
    }

    // Game event 31: three words, as written at 0x82643B34..0x82643B54.
    struct ImpactIoEventRecord : public CgsModule::Event
    {
        EImpactType meImpactType;
        EActiveRaceCarIndex meAggressorIndex;
        EActiveRaceCarIndex meVictimIndex;
    };
    static_assert(sizeof(ImpactIoEventRecord) == 12, "Impact game-event payload");

    // The grind-event record HandleRaceCarRaceCarContact's pre-pass pushes onto the player-driver
    // queue (asm AddEventSafe(..., 31, 12) -- a 12-byte event). The X360 writes a grind type (7 or 8)
    // at +0 and two -1 sentinels (v205/v206). FLAG: 12-byte layout modelled as the fields the asm
    // writes; only the byte SIZE (12) is asm-proven.
    struct GrindIoEventRecord : public CgsModule::Event
    {
        s32 miGrindType;   // +0 (asm v204 = 7 or 8)
        s32 miReservedA;   // +4 (asm v205 = -1)
        s32 miReservedB;   // +8 (asm v206 = -1)
    };

    // The takedown-scored event HandleRaceCarRaceCarContact pushes when a takedown registers
    // (asm AddEvent(..., 31, 12) @0x82643B58). FLAG: 12-byte layout modelled; size 12 asm-proven.
    //
    // +0 was declared `f32 mfImpactMagnitude`. All three of the event's
    // stack stores are `stw` (0x82643B34 / 0x82643B3C / 0x82643B54), and +0's r23 is the very same
    // register 0x826439E8 stamps into maeImpactType[victim] -- loaded once at 0x82643908 from
    // var_14C and never rewritten in between. It is the EImpactType word. (Same mis-typing that
    // the tuning-bank wave found on the maeImpactType array itself: a slot IDA had typed float.)
    struct TakedownIoEventRecord : public CgsModule::Event
    {
        s32 miImpactType;      // +0 (asm r23 == var_14C == the value maeImpactType[victim] gets)
        s32 miAttackerIndex;   // +4 (asm r26 == var_148 @0x82643998 -- FLAG: provenance untraced)
        s32 miReserved;        // +8 (asm var_144 == r31 @0x826435BC -- FLAG: the -1 written is unproven)
    };

    // -------------------------------------------------------------------------------------------
    // HandleRaceCarRaceCarContact  @0x82642F78  -- STAGE 1, the car-vs-car contact driver.
    //
    // Reconstructed from blueprint td_C4 + the raw __asm (Hex-Rays' locals FAILED here too). It
    // decodes the two race-car EntityIds, gates, populates a stack-local RaceCarResponseInfo, runs the
    // grind pre-pass + the classifier ladder, commits flagged crashes, then drives the slam/shunt
    // physics + the last-attacker/revenge bookkeeping.
    //
    // A/B: there is NO swap. Re-read store by store from the asm -- the A record's crashing byte
    // goes to +0x50 (mbRaceCarAIsCrashing) and B's to +0x51, the A type-derived is-player byte to
    // +0x52 and B's to +0x53, the A is-network byte to +0x54 and B's to +0x55. An earlier pass read
    // the two crashing loads inverted and carried a "surprising swap" note for it; the loads are
    // straight (see the per-car flag populate below).
    //
    // The per-car SPEEDS (+0x5C/+0x60), the CLOSING velocity (+0x30) and speed (+0x58), and
    // mfAngleBetweenCars (+0xF0) are computed by long vmsum3fp/vrsqrtefp/XMVectorACos register
    // cascades. They are reconstructed here with NAMED Vector3 math against the two cars'
    // transforms and mLastLinearVelocity: the results match, while the rsqrt Newton refinement --
    // which only chases the estimate's error -- becomes an exact host square root.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::HandleRaceCarRaceCarContact(ContactSpy::RaceCarContact lContact,
        VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        f32 lfTimestep)
    {
        (void)lfTimestep;
        CGS_ASSERT((lContact.mEntityIdA.muValue >> 24) == 1, "Contact A must be a race car");
        CGS_ASSERT((lContact.mEntityIdB.muValue >> 24) == 1, "Contact B must be a race car");
        const s32 liIndexA = (lContact.mEntityIdA.muValue >> 10) & 0x3FFF;
        const s32 liIndexB = (lContact.mEntityIdB.muValue >> 10) & 0x3FFF;
        {
            // Keep a separate player allowance: the AI pack contacts itself before
            // the player arrives, otherwise those contacts exhaust the diagnostic.
            static s32 saiWitnessed[2] = {};
            s32& siWitnessed = saiWitnessed[liIndexA == mePlayerActiveRaceCarIndex || liIndexB == mePlayerActiveRaceCarIndex];
            if (std::getenv("BRN_CRASHCAM_DIAG") && siWitnessed < 16 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siWitnessed;
                *CgsDev::Log::gpDebugPrint << "[td-contact] entry race car " << liIndexA << " vs " << liIndexB
                                           << " [FLAG PC witness]\n";
            }
        }
        if (!mbSlamsAndShuntsOn)
            return;
        if (!mUsedRaceCars.IsBitSet(liIndexA) || !mUsedRaceCars.IsBitSet(liIndexB))
            return;

        const f32 lfNormalLength = std::sqrt(vpu::Dot(lContact.mNormal, lContact.mNormal));
        CGS_ASSERT(lfNormalLength - 1.0f <= 0.05f && lfNormalLength - 1.0f >= -0.05f,
                   "Race-car contact normal must be unit length");

        RaceCarPhysics& lrCarA = maRaceCarVehicles[liIndexA];
        RaceCarPhysics& lrCarB = maRaceCarVehicles[liIndexB];
        RaceCarResponseInfo lInfo = {};
        lInfo.mpContact = &lContact;
        lInfo.mpRequestOutputInterface = lpRequestOutputInterface;
        lInfo.mpVehicleOutputInterface = lpVehicleOutputInterface;
        lInfo.mpManagerOutputInterface = lpManagerOutputInterface;
        lInfo.mpDeformationInterface = lpDeformationInterface;
        lInfo.mRaceCarAEntityID = lContact.mEntityIdA;
        lInfo.mRaceCarBEntityID = lContact.mEntityIdB;
        lInfo.meActiveRaceCarIndexA = static_cast<EActiveRaceCarIndex>(liIndexA);
        lInfo.meActiveRaceCarIndexB = static_cast<EActiveRaceCarIndex>(liIndexB);
        lInfo.mpRaceCarA = &lrCarA;
        lInfo.mpRaceCarB = &lrCarB;
        lInfo.mbRaceCarAIsPlayer = maeRaceCarTypes[liIndexA] == BrnWorld::E_RACE_CAR_TYPE_PLAYER;
        lInfo.mbRaceCarBIsPlayer = maeRaceCarTypes[liIndexB] == BrnWorld::E_RACE_CAR_TYPE_PLAYER;
        lInfo.mbRaceCarAIsNetworkCar = maeRaceCarTypes[liIndexA] == BrnWorld::E_RACE_CAR_TYPE_NETWORK;
        lInfo.mbRaceCarBIsNetworkCar = maeRaceCarTypes[liIndexB] == BrnWorld::E_RACE_CAR_TYPE_NETWORK;
        lInfo.mbOtherCarIsAI = maeRaceCarTypes[liIndexA] == BrnWorld::E_RACE_CAR_TYPE_AI
                           || maeRaceCarTypes[liIndexB] == BrnWorld::E_RACE_CAR_TYPE_AI;
        lInfo.mbRaceCarAIsCrashing = lrCarA.mbCrashing;
        lInfo.mbRaceCarBIsCrashing = lrCarB.mbCrashing;
        // manager+0x1AF0 == car+0x13B0: last-frame velocity, not the post-solver velocity.
        lInfo.mClosingVelocityAtoB = vpu::Subtract(lrCarA.mLastLinearVelocity, lrCarB.mLastLinearVelocity);
        lInfo.mfClosingSpeed = std::sqrt(vpu::Dot(lInfo.mClosingVelocityAtoB, lInfo.mClosingVelocityAtoB));
        lInfo.mfRaceCarASpeed = std::sqrt(vpu::Dot(lrCarA.mLastLinearVelocity, lrCarA.mLastLinearVelocity));
        lInfo.mfRaceCarBSpeed = std::sqrt(vpu::Dot(lrCarB.mLastLinearVelocity, lrCarB.mLastLinearVelocity));
        lInfo.mRaceCarATransform = lrCarA.mTransform;
        lInfo.mRaceCarBTransform = lrCarB.mTransform;
        // Normalize both forward axes before acos (0x826434A8..0x8264352C).
        const Vector3 lvForwardA = vpu::Normalize(lrCarA.mTransform.At());
        const Vector3 lvForwardB = vpu::Normalize(lrCarB.mTransform.At());
        f32 lfAlignment = vpu::Dot(lvForwardA, lvForwardB);
        if (lfAlignment < -1.0f) lfAlignment = -1.0f;
        if (lfAlignment > 1.0f) lfAlignment = 1.0f;
        lInfo.mfAngleBetweenCars = std::fabs(std::acos(lfAlignment));
        lInfo.meAggressorActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
        lInfo.meVictimActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
        lInfo.meImpactSitutation = E_IMPACT_SITUATION_INVALID;
        CGS_ASSERT(!(lInfo.mbRaceCarAIsPlayer && lInfo.mbRaceCarBIsPlayer), "Both contact cars are players");
        CGS_ASSERT(liIndexA != liIndexB, "Race-car contact indices must differ");
        if ((lInfo.mbRaceCarAIsCrashing && lInfo.mbRaceCarBIsCrashing)
            || (lInfo.mbRaceCarAIsNetworkCar && lInfo.mbRaceCarBIsNetworkCar))
            return;

        const bool lbPlayerInvolved = lInfo.mbRaceCarAIsPlayer || lInfo.mbRaceCarBIsPlayer;
        if (lbPlayerInvolved && CheckForGrindingAndRubbing(&lInfo)
            && mePlayerActiveRaceCarIndex == -1)
        {
            // These fixed [7] seats and the absent-player gate are literal in ARTIST.
            s32 liGrindingType = -1;
            if (mafPlayerGrindingOtherDurationSeconds[7] >= 1.0f)
                liGrindingType = 7;
            else if (mafOtherGrindingPlayerDurationSeconds[7] >= 0.8f)
                liGrindingType = 8;
            if (liGrindingType != -1)
            {
                ImpactIoEventRecord lEvent = { {}, static_cast<EImpactType>(liGrindingType),
                    static_cast<EActiveRaceCarIndex>(-1), static_cast<EActiveRaceCarIndex>(-1) };
                lpVehicleOutputInterface->GetGameEventQueue()->AddEventSafe(&lEvent, 31, sizeof(lEvent));
            }
        }
        CheckForAllTypesOfImpacts(&lInfo);
        {
            static s32 saiVerdicts[2] = {};
            s32& siVerdicts = saiVerdicts[lbPlayerInvolved];
            if (std::getenv("BRN_CRASHCAM_DIAG") && siVerdicts < 16 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siVerdicts;
                *CgsDev::Log::gpDebugPrint << "[td-contact] verdict " << liIndexA << " vs " << liIndexB
                                           << " impact=" << WitnessImpactTypeName(static_cast<s32>(lInfo.meImpactType))
                                           << "(" << static_cast<s32>(lInfo.meImpactType) << ")"
                                           << " situation=" << static_cast<s32>(lInfo.meImpactSitutation)
                                           << " crashA=" << (lInfo.mbCrashRaceCarA ? 1 : 0)
                                           << " crashB=" << (lInfo.mbCrashRaceCarB ? 1 : 0)
                                           << " closingSpeed=" << lInfo.mfClosingSpeed
                                           << " aggressor=" << static_cast<s32>(lInfo.meAggressorActiveRaceCarIndex)
                                           << " victim=" << static_cast<s32>(lInfo.meVictimActiveRaceCarIndex)
                                           << " [FLAG PC witness]\n";
            }
        }
        if (lInfo.mbCrashRaceCarA)
            SetRaceCarCrashing(lContact.mEntityIdA, lContact.mEntityIdB,
                lContact.mNormal, lContact.mPointOnA, lpRequestOutputInterface,
                lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
                BrnGameState::E_TAKEDOWN_NONE);
        if (lInfo.mbCrashRaceCarB)
            SetRaceCarCrashing(lContact.mEntityIdB, lContact.mEntityIdA,
                vpu::Negate(lContact.mNormal), lContact.mPointOnB, lpRequestOutputInterface,
                lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
                BrnGameState::E_TAKEDOWN_NONE);

        if (lrCarA.mbCrashing || lrCarB.mbCrashing)
            return;
        if ((liIndexA == mePlayerActiveRaceCarIndex && lrCarA.mPreviousControls.GetType() != E_DRIVER_TYPE_PLAYER)
            || (liIndexB == mePlayerActiveRaceCarIndex && lrCarB.mPreviousControls.GetType() != E_DRIVER_TYPE_PLAYER))
            return;

        if (lInfo.meImpactType != E_IMPACT_NONE)
        {
            if (lbPlayerInvolved)
            {
                const s32 liPlayer = lInfo.mbRaceCarAIsPlayer ? liIndexA : liIndexB;
                const s32 liOther = lInfo.mbRaceCarAIsPlayer ? liIndexB : liIndexA;
                lInfo.mbPlayerWonImpact = liPlayer == lInfo.meAggressorActiveRaceCarIndex;
                maeImpactType[liOther] = lInfo.meImpactType;
                mafNoImpactTimeSeconds[liOther] = mfMinSecondsBetweenImpacts;
                lInfo.muImpactScore = 1;
                mauImpactScore[liOther] = 1;
                if (lInfo.mbPlayerWonImpact)
                    mPlayerWonImpact.SetBit(liOther);
                else
                    mPlayerWonImpact.UnSetBit(liOther);
                lpVehicleOutputInterface->FlagTakedownScoredForDriver(lInfo.mbPlayerWonImpact);
                if (muTakedownEventsThisFrame < 32)
                {
                    ImpactIoEventRecord lEvent = { {}, lInfo.meImpactType,
                        lInfo.meAggressorActiveRaceCarIndex, lInfo.meVictimActiveRaceCarIndex };
                    ++muTakedownEventsThisFrame;
                    lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(&lEvent, 31, sizeof(lEvent));
                }
            }
            GenerateContactSituation(&lInfo);

            // [td-react] PC witness, BRN_TD_DIAG only [FLAG PC witness]: the inputs
            // CalculateSlamData / CalculateShuntData read that ApplySlam / ApplyShunt overwrite
            // (the victim's slam steering is cleared by ApplySlam), snapshotted before the commit.
            const bool lbWReact = lbPlayerInvolved && TakedownDiagEnabled() && CgsDev::Log::gpDebugPrint != 0
                && static_cast<s32>(lInfo.meVictimActiveRaceCarIndex) >= 0 && static_cast<s32>(lInfo.meVictimActiveRaceCarIndex) < 8
                && static_cast<s32>(lInfo.meAggressorActiveRaceCarIndex) >= 0 && static_cast<s32>(lInfo.meAggressorActiveRaceCarIndex) < 8;
            f32 lfWPreSlamVictim = 0.0f;
            f32 lfWPreSlamAggressor = 0.0f;
            s32 liWPreSlamNumber = 0;
            if (lbWReact)
            {
                lfWPreSlamVictim    = maRaceCarVehicles[lInfo.meVictimActiveRaceCarIndex].mfSlamSteering;
                lfWPreSlamAggressor = maRaceCarVehicles[lInfo.meAggressorActiveRaceCarIndex].mfSlamSteering;
                liWPreSlamNumber    = maRaceCarVehicles[lInfo.meVictimActiveRaceCarIndex].mSlamEffect.mi8SlamNumber;
            }

            if (mbAllowSlamsAndShuntsEffectsForRivals)
            {
                switch (lInfo.meImpactType)
                {
                case E_IMPACT_TRADING_PAINT: case E_IMPACT_SLAM: case E_IMPACT_BOOST_SLAM:
                    ApplySlam(&lInfo); break;
                case E_IMPACT_NUDGE: case E_IMPACT_SHUNT: case E_IMPACT_BOOST_SHUNT:
                    ApplyShunt(&lInfo); break;
                default: break;
                }
            }

            // [td-react] COMMIT -- what ApplySlam / ApplyShunt just committed to the two cars, with
            // every input their Calculate*Data read, then the per-step trace of both cars
            // (RaceCarPhysics::TdReactWatch / TdReactStep). Pure reads. [FLAG PC witness]
            if (lbWReact)
            {
                static s32 siWReactId = 0;
                ++siWReactId;
                const s32 liWV  = static_cast<s32>(lInfo.meVictimActiveRaceCarIndex);
                const s32 liWAg = static_cast<s32>(lInfo.meAggressorActiveRaceCarIndex);
                const RaceCarPhysics& lrWV  = maRaceCarVehicles[liWV];
                const RaceCarPhysics& lrWAg = maRaceCarVehicles[liWAg];
                CgsDev::Log::DebugPrint& lrWOut = *CgsDev::Log::gpDebugPrint;
                auto lWV3 = [&](const char* lpcName, const Vector3& lrV)
                {
                    lrWOut << lpcName << "=(" << lrV.x << "," << lrV.y << "," << lrV.z << ")";
                };
                lrWOut << "[td-react] COMMIT id=" << siWReactId
                       << " type=" << WitnessImpactTypeName(static_cast<s32>(lInfo.meImpactType))
                       << "(" << static_cast<s32>(lInfo.meImpactType) << ")"
                       << " aggr=" << liWAg << " victim=" << liWV
                       << " sit=" << static_cast<s32>(lInfo.meImpactSitutation)
                       << " closing=" << lInfo.mfClosingSpeed
                       << " mode=" << meCurrentGameModeType << " online=" << (mbIsOnlineGameMode ? 1 : 0)
                       << " typeA=" << static_cast<s32>(maeRaceCarTypes[liWAg]) << " typeV=" << static_cast<s32>(maeRaceCarTypes[liWV])
                       << " netA=" << (lInfo.mbRaceCarAIsNetworkCar ? 1 : 0) << " netB=" << (lInfo.mbRaceCarBIsNetworkCar ? 1 : 0)
                       << " | in: slamAg=" << lfWPreSlamAggressor << " slamV=" << lfWPreSlamVictim
                       << " slamNumV=" << liWPreSlamNumber
                       << " massAg=" << lrWAg.GetMass().x << " massV=" << lrWV.GetMass().x << " ";
                lWV3("n", lInfo.mpContact->mNormal); lrWOut << " ";
                lWV3("posAg", lrWAg.mTransform.wAxis); lrWOut << " ";
                lWV3("posV", lrWV.mTransform.wAxis); lrWOut << " ";
                lWV3("rightV", lrWV.mTransform.xAxis); lrWOut << " ";
                lWV3("upV", lrWV.mTransform.yAxis); lrWOut << " ";
                lWV3("velAg", lrWAg.mLinearVelocity); lrWOut << " ";
                lWV3("velV", lrWV.mLinearVelocity);
                lrWOut << " | victim slam life=" << lrWV.mSlamEffect.mfSlamLife
                       << " total=" << lrWV.mSlamEffect.mfTotalSlamTime
                       << " orig=" << lrWV.mSlamEffect.mfOriginalSteering
                       << " rec=" << lrWV.mSlamEffect.mfRecoveryTime
                       << " num=" << static_cast<s32>(lrWV.mSlamEffect.mi8SlamNumber)
                       << " attacker=" << static_cast<s32>(lrWV.mi8LastAttackersRaceCarIndex)
                       << " shunt dir=(" << lrWV.mShuntEffect.mDirectionPlusDesiredSpeed.x
                       << "," << lrWV.mShuntEffect.mDirectionPlusDesiredSpeed.y
                       << "," << lrWV.mShuntEffect.mDirectionPlusDesiredSpeed.z << ")"
                       << " desired=" << lrWV.mShuntEffect.mDirectionPlusDesiredSpeed.w
                       << " life=" << lrWV.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x
                       << " sitq=" << lrWV.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y
                       << " vuln=" << mafVulnerableTimeSeconds[liWV]
                       << " | aggressor slam life=" << lrWAg.mSlamEffect.mfSlamLife
                       << " total=" << lrWAg.mSlamEffect.mfTotalSlamTime
                       << " orig=" << lrWAg.mSlamEffect.mfOriginalSteering
                       << " rec=" << lrWAg.mSlamEffect.mfRecoveryTime
                       << " [FLAG PC witness]\n";
                RaceCarPhysics::TdReactWatch(&lrWV, siWReactId, 0);
                RaceCarPhysics::TdReactWatch(&lrWAg, siWReactId, 1);
            }
            if (lbPlayerInvolved)
            {
                lrCarA.SetWheelVelocities(lrCarA.mLinearVelocity);
                lrCarB.SetWheelVelocities(lrCarB.mLinearVelocity);
            }
        }

        // 0x82C5B9B0 initializes 0x82FB7F40 from flt_82001DA0 == 0.5.
        // Compare each contact point in that car's forward frame with its AABB minimum Z.
        const f32 lfPointBAlongCar = vpu::Dot(lrCarB.mTransform.At(),
            vpu::Subtract(lContact.mPointOnB, lrCarB.mTransform.Pos()));
        if (!lrCarA.IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liIndexB))
            && vpu::Dot(lrCarA.mTransform.At(),
                vpu::Subtract(lContact.mPointOnA, lrCarA.mTransform.Pos())) > lrCarA.mDeformableAABB.mMin.z * 0.5f
            && lbPlayerInvolved)
        {
            lrCarB.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z = 0.0f;
            lrCarB.mi8LastContactedRaceCar = static_cast<s8>(liIndexA);
        }
        if (!lrCarB.IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liIndexA))
            && lfPointBAlongCar > lrCarB.mDeformableAABB.mMin.z * 0.5f && lbPlayerInvolved)
        {
            lrCarA.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z = 0.0f;
            lrCarA.mi8LastContactedRaceCar = static_cast<s8>(liIndexB);
        }
    }

    // -------------------------------------------------------------------------------------------
    // CheckForAllTypesOfImpacts  @0x82642E58
    //
    // The takedown CLASSIFIER. Given a populated RaceCarResponseInfo, run the per-type
    // sub-classifiers in strict priority order and stop at the first that fires (first-match-wins;
    // each sub-classifier commits its own side effects -- crashing the victim etc. -- when it
    // matches, so this entry point returns void). Priority order (from the X360 asm):
    //   1. player slamming an AI into another AI   2. hitting an already-crashing car
    //   -- the remaining geometric tests only run if NEITHER car is already crashing --
    //   3. vertical   4. T-bone   5. head-to-head   6. shunt/nudge   7. slam/trading-paint
    //   8. stationary-target.
    // A leading energy gate skips classification entirely for gentle contacts
    // (|speedB| + |speedA| must clear KF_MIN_IMPACT_SPEED_SUM).
    //
    // FLAG (signature): the X360 Hex-Rays rendered this `int(int result, int a2)`; the DWARF
    // (BrnVehicleManager.h:1182) gives the true shape `void CheckForAllTypesOfImpacts(
    // RaceCarResponseInfo*)`. The "result" the pseudocode returns is the first-match value, which
    // the declared void signature discards.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::CheckForAllTypesOfImpacts(RaceCarResponseInfo* lpInfo)
    {
        const bool lbWitness = TakedownDiagEnabled() && CgsDev::Log::gpDebugPrint != 0;

        // [td-ladder] EPISODE + SLAM-GATE witness -- PC only, BRN_TD_DIAG only [FLAG PC witness].
        // `gap` counts car A's physics steps since this pair was last evaluated: 0 = another spy
        // of the same step (ProcessContactSpy stores every contact twice, A/B and B/A), 1 = the
        // contact continues from the previous step, >1 or -1 (never) = the FIRST step of a new
        // contact episode. `gateA/gateB` are what RaceCarPhysics::Update's slam-steering gate
        // (0x82641814) read this step -- see RaceCarPhysics::TdSlamGateSample: n = miNumCollisions
        // (+0x1354, every contact impulse since the previous step's reset at 0x826415A0), w = the
        // world share (+0x1353), ctl = the control steer, pre = mfSlamSteering before the gate.
        // `now` is this step's count so far (the impulses this step's DeformationManager::Update
        // banked, i.e. what zeroes NEXT step's slam steering). Pure reads.
        const RaceCarPhysics::TdSlamGateSample* lpWGateA = nullptr;
        const RaceCarPhysics::TdSlamGateSample* lpWGateB = nullptr;
        s32 liWGap = -1;
        if (lbWitness)
        {
            static u32 suaWPairLastEval[8][8] = {};   // the car's own update count + 1 at the last evaluation
            const s32 liWPairA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA);
            const s32 liWPairB = static_cast<s32>(lpInfo->meActiveRaceCarIndexB);
            lpWGateA = RaceCarPhysics::FindTdSlamGateSample(lpInfo->mpRaceCarA);
            lpWGateB = RaceCarPhysics::FindTdSlamGateSample(lpInfo->mpRaceCarB);
            if (lpWGateA != nullptr && lpWGateB != nullptr
                && liWPairA >= 0 && liWPairA < 8 && liWPairB >= 0 && liWPairB < 8)
            {
                const u32 luWLast = suaWPairLastEval[liWPairA][liWPairB];
                liWGap = (luWLast == 0u) ? -1 : static_cast<s32>(lpWGateA->muUpdates + 1u - luWLast);
                suaWPairLastEval[liWPairA][liWPairB] = lpWGateA->muUpdates + 1u;
                suaWPairLastEval[liWPairB][liWPairA] = lpWGateB->muUpdates + 1u;
            }
        }
        auto lWitnessEpisode = [&]()
        {
            *CgsDev::Log::gpDebugPrint << " gap=" << liWGap;
            const RaceCarPhysics::TdSlamGateSample* const lapWGate[2] = { lpWGateA, lpWGateB };
            const RaceCarPhysics* const lapWCar[2] = { lpInfo->mpRaceCarA, lpInfo->mpRaceCarB };
            for (s32 liWSide = 0; liWSide < 2; ++liWSide)
            {
                *CgsDev::Log::gpDebugPrint << (liWSide == 0 ? " gateA=" : " gateB=");
                if (lapWGate[liWSide] == nullptr)
                {
                    *CgsDev::Log::gpDebugPrint << "?";
                    continue;
                }
                *CgsDev::Log::gpDebugPrint << "(n=" << lapWGate[liWSide]->miNumCollisions
                                           << ",w=" << lapWGate[liWSide]->miNumWorldCollisions
                                           << ",ctl=" << lapWGate[liWSide]->mfControlSteer
                                           << ",pre=" << lapWGate[liWSide]->mfSlamBeforeGate
                                           << (lapWGate[liWSide]->mbCrashingBranch ? ",crashing" : "")
                                           << ",now=" << lapWCar[liWSide]->miNumCollisions
                                           << ",nowW=" << static_cast<s32>(lapWCar[liWSide]->mi8NumWorldCollisions)
                                           << ")";
            }
        };

        // [td-replay] -- PC only, BRN_TD_DIAG + BRN_TD_REPLAY [FLAG PC witness]. Every input the
        // ladder below reads, BEFORE any rung writes the response info, as raw IEEE bits (hex).
        // Layout "v1" (tests/run_fxladder_replay.py parses it; keep the two in step):
        //   idxA idxB | flags(crashA,crashB,playerA,playerB,netA,netB,otherAI = bits 0..6)
        //   entityA entityB closing speedA speedB stressSq angle closingVel(4) transformA(16)
        //   transformB(16) | contact entityA entityB normal(4) pointOnA(4) pointOnB(4)
        //   | playerIndex online | per car A then B: type noImpact vulnerability pgo ogp boost
        //   | per car A then B: crashing slamSteering lastAttacker slamLife shuntW shuntLife
        //     lastContacted timeSinceContact airTime timeCrashing mass crashSpeedMPS
        //     lastLinearVelocity(4) deformableMin(4) deformableMax(4) transform(16)
        // The next [td-ladder] line of the same evaluation (REJECT energy / -> rung) is its result.
        if (lbWitness && TakedownReplayEnabled()
            && (lpInfo->mbRaceCarAIsPlayer || lpInfo->mbRaceCarBIsPlayer))
        {
            CgsDev::Log::DebugPrint& lrWOut = *CgsDev::Log::gpDebugPrint;
            auto lWWord = [&](u32 luValue) { lrWOut.AppendFormat(" %08X", luValue); };
            auto lWBits = [&](f32 lfValue)
            {
                u32 luBits = 0;
                std::memcpy(&luBits, &lfValue, sizeof(luBits));
                lWWord(luBits);
            };
            auto lWVec = [&](f32 lfX, f32 lfY, f32 lfZ, f32 lfW) { lWBits(lfX); lWBits(lfY); lWBits(lfZ); lWBits(lfW); };
            auto lWMat = [&](const Matrix44Affine& lrM)
            {
                lWVec(lrM.xAxis.x, lrM.xAxis.y, lrM.xAxis.z, lrM.xAxis.w);
                lWVec(lrM.yAxis.x, lrM.yAxis.y, lrM.yAxis.z, lrM.yAxis.w);
                lWVec(lrM.zAxis.x, lrM.zAxis.y, lrM.zAxis.z, lrM.zAxis.w);
                lWVec(lrM.wAxis.x, lrM.wAxis.y, lrM.wAxis.z, lrM.wAxis.w);
            };
            const s32 laWIdx[2] = { static_cast<s32>(lpInfo->meActiveRaceCarIndexA),
                                    static_cast<s32>(lpInfo->meActiveRaceCarIndexB) };
            lrWOut << "[td-replay] v1 " << laWIdx[0] << " " << laWIdx[1];
            lWWord((lpInfo->mbRaceCarAIsCrashing ? 1u : 0u) | (lpInfo->mbRaceCarBIsCrashing ? 2u : 0u)
                   | (lpInfo->mbRaceCarAIsPlayer ? 4u : 0u) | (lpInfo->mbRaceCarBIsPlayer ? 8u : 0u)
                   | (lpInfo->mbRaceCarAIsNetworkCar ? 16u : 0u) | (lpInfo->mbRaceCarBIsNetworkCar ? 32u : 0u)
                   | (lpInfo->mbOtherCarIsAI ? 64u : 0u));
            lWWord(lpInfo->mRaceCarAEntityID.muValue);
            lWWord(lpInfo->mRaceCarBEntityID.muValue);
            lWBits(lpInfo->mfClosingSpeed);
            lWBits(lpInfo->mfRaceCarASpeed);
            lWBits(lpInfo->mfRaceCarBSpeed);
            lWBits(lpInfo->mfNormalStressSq);
            lWBits(lpInfo->mfAngleBetweenCars);
            lWVec(lpInfo->mClosingVelocityAtoB.x, lpInfo->mClosingVelocityAtoB.y,
                  lpInfo->mClosingVelocityAtoB.z, lpInfo->mClosingVelocityAtoB.w);
            lWMat(lpInfo->mRaceCarATransform);
            lWMat(lpInfo->mRaceCarBTransform);
            const BrnPhysics::ContactSpy::RaceCarContact& lrWContact = *lpInfo->mpContact;
            lWWord(lrWContact.mEntityIdA.muValue);
            lWWord(lrWContact.mEntityIdB.muValue);
            lWVec(lrWContact.mNormal.x, lrWContact.mNormal.y, lrWContact.mNormal.z, lrWContact.mNormal.w);
            lWVec(lrWContact.mPointOnA.x, lrWContact.mPointOnA.y, lrWContact.mPointOnA.z, lrWContact.mPointOnA.w);
            lWVec(lrWContact.mPointOnB.x, lrWContact.mPointOnB.y, lrWContact.mPointOnB.z, lrWContact.mPointOnB.w);
            lWWord(static_cast<u32>(mePlayerActiveRaceCarIndex));
            lWWord(mbIsOnlineGameMode ? 1u : 0u);
            for (s32 liWSide = 0; liWSide < 2; ++liWSide)
            {
                const s32 liWCar = laWIdx[liWSide];
                lWWord(static_cast<u32>(maeRaceCarTypes[liWCar]));
                lWBits(mafNoImpactTimeSeconds[liWCar]);
                lWBits(mafVulnerabilityFactor[liWCar]);
                lWWord(static_cast<u32>(mau8FramesSincePlayerGrindingOther[liWCar]));
                lWWord(static_cast<u32>(mau8FramesSinceOtherGrindingPlayer[liWCar]));
                lWWord(maRaceCarDrivers[liWCar].mControls.mbBoost ? 1u : 0u);
            }
            for (s32 liWSide = 0; liWSide < 2; ++liWSide)
            {
                const RaceCarPhysics& lrWCar = maRaceCarVehicles[laWIdx[liWSide]];
                lWWord(lrWCar.mbCrashing ? 1u : 0u);
                lWBits(lrWCar.mfSlamSteering);
                lWWord(static_cast<u32>(static_cast<s32>(lrWCar.mi8LastAttackersRaceCarIndex)));
                lWBits(lrWCar.mSlamEffect.mfSlamLife);
                lWBits(lrWCar.mShuntEffect.mDirectionPlusDesiredSpeed.w);
                lWBits(lrWCar.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x);
                lWWord(static_cast<u32>(static_cast<s32>(lrWCar.mi8LastContactedRaceCar)));
                lWBits(lrWCar.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z);
                lWBits(lrWCar.mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z);
                lWBits(lrWCar.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y);
                lWBits(lrWCar.GetMass().x);
                lWBits(lrWCar.GetAttribs()->mCollisionAttribs.GetCrashSpeedMPS().x);
                lWVec(lrWCar.mLastLinearVelocity.x, lrWCar.mLastLinearVelocity.y,
                      lrWCar.mLastLinearVelocity.z, lrWCar.mLastLinearVelocity.w);
                const CgsGeometric::AxisAlignedBox& lrWBox = lrWCar.GetDeformableAABB();
                lWVec(lrWBox.mMin.x, lrWBox.mMin.y, lrWBox.mMin.z, lrWBox.mMin.w);
                lWVec(lrWBox.mMax.x, lrWBox.mMax.y, lrWBox.mMax.z, lrWBox.mMax.w);
                lWMat(lrWCar.mTransform);
            }
            lrWOut << " [FLAG PC witness]\n";
        }

        // Energy gate: ignore contacts whose combined closing speed is below the threshold.
        const f32 lfWitnessSpeedSum =
            std::fabs(lpInfo->mfRaceCarBSpeed) + std::fabs(lpInfo->mfRaceCarASpeed);
        if (lfWitnessSpeedSum < KF_MIN_IMPACT_SPEED_SUM)
        {
            // [td-ladder] PC witness, BRN_TD_DIAG only [FLAG PC witness]
            if (lbWitness)
            {
                *CgsDev::Log::gpDebugPrint << "[td-ladder] " << static_cast<s32>(lpInfo->meActiveRaceCarIndexA)
                                           << " vs " << static_cast<s32>(lpInfo->meActiveRaceCarIndexB)
                                           << " REJECT energy speedSum=" << lfWitnessSpeedSum
                                           << " need=" << KF_MIN_IMPACT_SPEED_SUM;
                lWitnessEpisode();
                *CgsDev::Log::gpDebugPrint << " [FLAG PC witness]\n";
            }
            return;
        }

        if (lbWitness)
        {
            *CgsDev::Log::gpDebugPrint << "[td-ladder] " << static_cast<s32>(lpInfo->meActiveRaceCarIndexA)
                                       << " vs " << static_cast<s32>(lpInfo->meActiveRaceCarIndexB)
                                       << " ENTER speedSum=" << lfWitnessSpeedSum
                                       << " crashA=" << (lpInfo->mbRaceCarAIsCrashing ? 1 : 0)
                                       << " crashB=" << (lpInfo->mbRaceCarBIsCrashing ? 1 : 0)
                                       << " playerA=" << (lpInfo->mbRaceCarAIsPlayer ? 1 : 0)
                                       << " playerB=" << (lpInfo->mbRaceCarBIsPlayer ? 1 : 0)
                                       << " angle=" << lpInfo->mfAngleBetweenCars << " [FLAG PC witness]\n";

            // [td-ladder] GATES -- the INPUTS of the two force rungs (ShuntAndNudge @0x8261A3A0,
            // SlamAndTradingPaint @0x82619F30), read BEFORE the ladder runs, with the same
            // operands those bodies read, so a rejection can be named gate by gate. Pure reads:
            // every call below is a const query. [FLAG PC witness, BRN_TD_DIAG only]
            const s32 liWA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA);
            const s32 liWB = static_cast<s32>(lpInfo->meActiveRaceCarIndexB);
            const Vector3& lvWN = lpInfo->mpContact->mNormal;
            const f32 lfWAlign = std::fabs(Dot3_VM(lvWN, lpInfo->mRaceCarATransform.At()))
                               + std::fabs(Dot3_VM(lvWN, lpInfo->mRaceCarBTransform.At()));
            const bool lbWABehind = Dot3_VM(vpu::Subtract(lpInfo->mRaceCarBTransform.Pos(),
                                                          lpInfo->mRaceCarATransform.Pos()),
                                            lpInfo->mRaceCarATransform.At()) >= 0.0f;
            const s32 liWShuntAggr = lbWABehind ? liWA : liWB;
            const s32 liWShuntVict = lbWABehind ? liWB : liWA;
            const Vector3 lvWBToA = vpu::Subtract(lpInfo->mRaceCarATransform.Pos(), lpInfo->mRaceCarBTransform.Pos());
            const f32 lfWSideA = Dot3_VM(lvWBToA, lpInfo->mRaceCarATransform.Right());
            const f32 lfWSideB = Dot3_VM(lvWBToA, lpInfo->mRaceCarBTransform.Right());
            const f32 lfWRawA = lpInfo->mpRaceCarA->GetSlamSteering();
            const f32 lfWRawB = lpInfo->mpRaceCarB->GetSlamSteering();
            *CgsDev::Log::gpDebugPrint
                << "[td-ladder] GATES closing=" << lpInfo->mfClosingSpeed
                << " sA=" << lpInfo->mfRaceCarASpeed << " sB=" << lpInfo->mfRaceCarBSpeed
                << " recentA=" << (HasRaceCarHadRecentImpact(liWA) ? 1 : 0) << "(" << mafNoImpactTimeSeconds[liWA] << ")"
                << " recentB=" << (HasRaceCarHadRecentImpact(liWB) ? 1 : 0) << "(" << mafNoImpactTimeSeconds[liWB] << ")"
                << " | shunt align=" << lfWAlign << "/1.9 aggr=" << liWShuntAggr
                << " aggrBeingByVict=" << (maRaceCarVehicles[liWShuntAggr].IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liWShuntVict)) ? 1 : 0)
                << " | slam dotAt=" << Dot3_VM(lpInfo->mRaceCarATransform.At(), lpInfo->mRaceCarBTransform.At())
                << " sideA=" << lfWSideA << " sideB=" << lfWSideB
                << " rawSteerA=" << lfWRawA << " rawSteerB=" << lfWRawB
                << " steerA=" << ((lfWSideA > 0.0f ? 1.0f : lfWSideA >= 0.0f ? 0.0f : -1.0f) * lfWRawA)
                << " steerB=" << (-(lfWSideB > 0.0f ? 1.0f : lfWSideB >= 0.0f ? 0.0f : -1.0f) * lfWRawB)
                << " beingAbyB=" << (lpInfo->mpRaceCarA->IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liWB)) ? 1 : 0)
                << " beingBbyA=" << (lpInfo->mpRaceCarB->IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liWA)) ? 1 : 0)
                << " pgoB=" << static_cast<s32>(mau8FramesSincePlayerGrindingOther[liWB])
                << " ogpA=" << static_cast<s32>(mau8FramesSinceOtherGrindingPlayer[liWA])
                << " pgoA=" << static_cast<s32>(mau8FramesSincePlayerGrindingOther[liWA])
                << " ogpB=" << static_cast<s32>(mau8FramesSinceOtherGrindingPlayer[liWB]);
            lWitnessEpisode();
            *CgsDev::Log::gpDebugPrint << " [FLAG PC witness]\n";
        }
        // The rung that ended the ladder and the classification it left in the response info.
        // [FLAG PC witness, BRN_TD_DIAG only]
        auto lWitnessResult = [&](const char* lpcRung)
        {
            *CgsDev::Log::gpDebugPrint << "[td-ladder] -> " << lpcRung
                                       << " impact=" << WitnessImpactTypeName(static_cast<s32>(lpInfo->meImpactType))
                                       << "(" << static_cast<s32>(lpInfo->meImpactType) << ")"
                                       << " aggr=" << static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex)
                                       << " victim=" << static_cast<s32>(lpInfo->meVictimActiveRaceCarIndex)
                                       << " crashFlagA=" << (lpInfo->mbCrashRaceCarA ? 1 : 0)
                                       << " crashFlagB=" << (lpInfo->mbCrashRaceCarB ? 1 : 0)
                                       << " closing=" << lpInfo->mfClosingSpeed << " [FLAG PC witness]\n";
        };

        // Highest priority: a player shunting an AI into a third AI, and re-hits on a car that is
        // already crashing -- these run even if a car is mid-crash.
        if (CheckForPlayerSlammingAIIntoAI(lpInfo))
        {
            if (lbWitness) lWitnessResult("SlammingAIIntoAI");
            return;
        }
        if (CheckForHittingAlreadyCrashingCar(lpInfo))
        {
            if (lbWitness) lWitnessResult("HittingAlreadyCrashingCar");
            return;
        }

        // The geometric classifiers only apply to a fresh impact: a car already crashing cannot be
        // freshly taken down.
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
        {
            if (lbWitness) lWitnessResult("REJECT already-crashing");
            return;
        }

        if (CheckForVerticalTakedown(lpInfo))
        {
            if (lbWitness) lWitnessResult("VERTICAL");
            return;
        }
        if (CheckForTBoneTakedown(lpInfo))
        {
            if (lbWitness) lWitnessResult("T_BONE");
            return;
        }
        if (CheckForHeadToHead(lpInfo))
        {
            if (lbWitness) lWitnessResult("HEAD_ON");
            return;
        }
        if (CheckForShuntAndNudge(lpInfo))
        {
            if (lbWitness) lWitnessResult("shunt/nudge");
            return;
        }
        if (CheckForSlamAndTradingPaint(lpInfo))
        {
            if (lbWitness) lWitnessResult("slam/paint");
            return;
        }
        const bool lbStationary = CheckForStationaryTargetTakedown(lpInfo);
        if (lbWitness)
            lWitnessResult(lbStationary ? "stationary=1" : "stationary=0 (ladder exhausted)");
    }

    // -------------------------------------------------------------------------------------------
    // `KI_RACECAR_CRASH_STATE_FATAL = 2`.
    // The array it guarded is not a crash-state array at all -- it is
    // `BrnWorld::ERaceCarType maeRaceCarTypes[8]` (DWARF BrnVehicleManager.h:828), and
    // VehicleManager::Construct seeds every slot with 3 == E_RACE_CAR_TYPE_INACTIVE
    // (`stw r24, 0(r28)`, r24 == 3, r28 == this + 44192, stride 4, x8). The literals the bodies
    // compare against are therefore enumerators, not sentinels: `!= 2` is
    // "not E_RACE_CAR_TYPE_NETWORK" and `== 1` is "is E_RACE_CAR_TYPE_AI". The committed tree
    // already half-knew this -- BrnVehicleManagerPlayerStats.cpp's own assert string spells it
    // "== BrnWorld::E_RACE_CAR_TYPE_NETWORK" while the member it read was named
    // maRaceCarCrashState. Every comparison keeps the same numeric value; only the meaning is
    // corrected.
    // -------------------------------------------------------------------------------------------

    // -------------------------------------------------------------------------------------------
    // InstantTakedown @0x82636108 -- BODY NOT HERE. Split into the mounted slice TU
    // BrnVehicleManager_InstantTakedown.cpp on 2026-08-11 (create-drain wave; the
    // RaceCarPhysics_Construct precedent) because DoHornTakedowns needs it linkable while this
    // home TU is unmountable. Byte-identical move, full banner travels with the body.
    // TO RE-MERGE: mount this TU, move the body back, delete the slice.
    // -------------------------------------------------------------------------------------------

    // -------------------------------------------------------------------------------------------
    // SetRaceCarCrashing  @0x82634C90  -- STAGE 3b, the UNIVERSAL crash-commit sink.
    //
    // Every takedown path (InstantTakedown, HandleRaceCarRaceCarContact's v251/v252 commits,
    // ForceRaceCarCrash, the traffic/world contact handlers) funnels here to actually wreck a
    // victim. Reconstructed from blueprint td_C3 + the raw __asm (Hex-Rays' locals FAILED here).
    //
    // ASM->C++ ARG MAPPING (the X360 packs `this` and the victim id into one 64-bit register `a1`,
    // so Hex-Rays' arg list is garbage; the DWARF 9-arg shape is authoritative and is what we body):
    //   a1 = { HIDWORD = this, LODWORD = lVictimEntityId.muValue }   (v35 = this, v34 = victim id)
    //   a2 = lAggressorEntityId.muValue  (v41; HIBYTE = the owner/cause sub-code v42)
    //   the two Vector3s + the four interfaces follow; leTakedownType is the trailing enum.
    //
    // SETTLED 2026-09-11 (this wave): the aggressor id has THREE roles here and none of them is a
    // takedown type. Its owner byte is the suppression-gate cause sub-code; the id itself is the
    // duplicate-crash key and the crash-data slot's +0x4 seat; and its traffic-remapped form is
    // what the victim's record and the crash event publish as the party that caused the crash.
    // The takedown type is a separate argument with a separate destination: it selects the physics
    // latch predicate and is forwarded to the crash event, where the takedown detector reads it.
    // The older note that put the takedown type in the slot's +0x4 seat was wrong, and the
    // remote-crash sink had already contradicted it.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::SetRaceCarCrashing(EntityId lVictimEntityId,
                                            EntityId lAggressorEntityId,
                                            Vector3 lCollisionNormal,
                                            Vector3 lContactPoint,
                                            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                            VehicleManagerOutputInterface* lpManagerOutputInterface,
                                            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                                            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                            BrnGameState::ETakedownType leTakedownType)
    {
        // Both contact vectors are forwarded to the crash event. The request-output and
        // deformation interfaces are untouched by this sink.
        (void)lpRequestOutputInterface;
        (void)lpDeformationInterface;

        // ===========================================================================================
        // ✅ THE BRN_ENABLE_CRASH_ENTRY BRING-UP FLAG IS **DELETED** (endcrash wave, 2026-08-27).
        // It gated this whole function -- the crash-entry sink -- behind an environment variable for
        // three waves. Its reasons were retired one at a time and the last one fell today:
        //   * "crash RECOVERY needs BrnAI::ResetOnTrackManager, so a heavy crash pins the car" --
        //     retired 2026-08-26 (resetpump wave): the request/result pump is plumbed end to end and
        //     a crashed car is put back on the road and drives away.
        //   * "NOTHING CLEARS THE CRASH STATE, so the recovered car keeps the crash bar up and every
        //     reader of IsPlayerCarCrashing lies" -- retired 2026-08-26 (crashclear wave):
        //     ProcessResetEvents dispatches RaceCarPhysics vtable slot 1 ==
        //     VehiclePhysics::ClearCrashing @0x825D5450, mbCrashing goes 1 -> 0 and LEAVE_CRASHED is
        //     posted.
        //   * "the HUD never comes back -- the HUD FSM enters the crashed state and cannot leave" --
        //     retired 2026-08-27 (endcrash wave). The missing piece was never an arm, it was a STATE:
        //     BrnGui::CrashedHudState declared no virtuals at all, so it never registered, never
        //     updated and never sent END_CRASH. It has OnEnter/OnLeave/Update/UpdatePermenant now
        //     (BrnCrashedHudState.cpp), and its TU is finally in the exe source list.
        // MEASURED, run ec_crash1 (-Frames -Drive -CrashPlayer 5700), asserts=0, no AV, full flow to
        // DRIVING: TWO crashes, each  START_CRASHED -> START_CRASH -> [HUD hidden] -> ClearCrashing
        // 1 -> 0 -> LEAVE_CRASHED -> END_CRASH -> [HUD BACK], and the HUD is still on screen at the
        // last dumped frame. Frame-measured, not just logged: the minimap box is empty across both
        // crash windows and full on both sides of them.
        // ⚠️ Crash entry is now ON for every run, including default ones. The console path below
        // is what always ran when the flag was set -- deleting the gate changes no behaviour that a
        // -CrashEntry run did not already have.
        // ===========================================================================================

        // ---- Step 1: index + early-out suppression gates (asm v36/v38/v43/v44/v45) ----
        const s32 liVictimIndex = static_cast<s32>((lVictimEntityId.muValue >> 10) & 0x3FFF);
        VehicleDriver& lrDriver = maRaceCarDrivers[liVictimIndex];   // asm 224*v36 + this
        const s32 liCrashState = maeRaceCarTypes[liVictimIndex];      // asm v44 = maeRaceCarTypes[v36]
        // RETIRED 2026-09-02 (deform close-out wave): `lbWasInCrashState1 = (liCrashState == 1)`
        // -- Hex-Rays' v45 -- used to be forwarded to BOTH AddRaceCarCrashEvent sites as the r8
        // argument. It is not that argument: v45 is one of the four AND terms of the
        // RaceCarPhysics::SetCrashing latch (`cntlzw r9, r7 ; extrwi ; and r11, r11, r9`
        // @0x826353C4..0x826353D4), which this body already models as `liCrashState == 1` inside
        // lbLatchPhysics. The real r8 is a hard 0 on path (A) and the post-SetCrashing mbCrashing
        // re-read on path (B); both sites now pass that.

        // The cause sub-code = the OWNER byte of the aggressor id (RACECAR=1, remap-required=2, ...).
        // asm: v42 = HIBYTE(a2).
        const u32 luCauseSubCode = (lAggressorEntityId.muValue >> 24) & 0xFF;

        // The two driver flag bytes the asm reads at +124 and +125. RE-NAMED 2026-08-03: they are
        // the victim's two INVULNERABILITY flags inside VehicleDriver::mControls (in-record 60/61),
        // and the cause sub-codes they gate on say so -- 1/2 are the vehicle-owner codes, 0/3/5 the
        // world ones. The old role names ("mbTakenDown" / "mbSuppressByCause") described the effect;
        // these are the console's own members.
        const bool lbInvulnerableToVehicles = lrDriver.mControls.mbIsInvulnerableToVehicles; // *(v38+124)
        const bool lbInvulnerableToWorld    = lrDriver.mControls.mbIsInvulnerableToWorld;    // *(v38+125)

        if ((lbInvulnerableToVehicles && (luCauseSubCode == 1 || luCauseSubCode == 2))
            || (lbInvulnerableToWorld && (luCauseSubCode == 0 || luCauseSubCode == 3 || luCauseSubCode == 5))
            || (mbStopPlayerCrashing && liVictimIndex == static_cast<s32>(mePlayerActiveRaceCarIndex))
            || (mbStopAICrashing && liCrashState == 1))
        {
            return;   // asm: goto LABEL_134 -- the crash is SUPPRESSED for this car.
        }

        // (asm: if v44 == 2 a debug-only assert fires that the car is not E_RACE_CAR_TYPE_NETWORK --
        // a developer check with no runtime effect; not reproduced.)

        // ---- Step 2: entity-id validation / remap (asm: pure debug asserts against the id tables) --
        // The asm validates the packed id against maRaceCarEntityIDs[victim] (+43584) and, when the
        // victim id's owner byte is 2 (E_ENTITYTYPE_TRAFFIC_VEHICLE), replaces it with the GLOBAL
        // entity id the traffic manager holds for that traffic slot. The asserts are debug-only; the
        // lookup is the load-bearing part for the secondary event below.
        //
        // RE-SEATED 2026-08-03 (the un-pin wave). This read used to be
        // `maRaceCarEntityIdRemap[liVictimIndex]`, a proposed-by-role sibling of VehicleManager at
        // class +148128 declared `EntityId[8]`. It is really
        // `mPhysicalTrafficManager.maTrafficEntityIDs` -- 44768 + 103360 == 148128, and
        // PhysicalTrafficManager::Construct @0x82636CA8 seeds exactly that array with
        // `stwx -1` over 4*(i+25840) for i<20. Two things were wrong and one was right:
        // the ADDRESS was right (the byte the asm loads is unchanged),
        // the BOUND was wrong -- this branch is taken for a TRAFFIC id, so liVictimIndex is a
        //      traffic index in [0,20) and the old [8] declaration made slots 8..19 an
        //      out-of-bounds read of the following member,
        // the NAME/role was wrong -- it is not a "race car remap", it is the traffic slot's
        //      global entity id.
        // The read stays a BARE one (no accessor): the X360 here is a plain `lwzx`, whereas
        // PhysicalTrafficManager::GetGlobalTrafficEntityId @0x825C2C38 fires an index assert AND an
        // mUsedTrafficVehicles.IsBitSet assert. Going through the accessor would add two asserts the
        // console does not fire at this site.
        //   asm  0x82634EEC  extrwi r11, r31, 14,8      ; the traffic index out of the packed id
        //        0x82634EF0  addis  r11, r11, 1
        //        0x82634EF4  addi   r11, r11, -0x6F58   ; == idx + 37032
        //        0x82634EF8  slwi   r11, r11, 2         ; == 4*idx + 148128
        //        0x82634EFC  lwzx   r26, r11, r18
        EntityId lValidatedVictimId = lVictimEntityId;   // asm v34
        const u32 luVictimOwner = (lVictimEntityId.muValue >> 24) & 0xFF;
        if (luVictimOwner == 2)   // asm: HIBYTE(LODWORD(v34)) == 2 -> traffic slot lookup
        {
            lValidatedVictimId = mPhysicalTrafficManager.maTrafficEntityIDs[liVictimIndex];
        }

        // ---- Step 2b: the same remap for the AGGRESSOR, by its OWN index ----
        // A traffic-owned aggressor id is republished as the traffic slot's global entity id; a
        // race-car-owned one passes through unchanged. This is the id the victim's record and the
        // crash event publish as the party that caused the crash -- the sink used to publish the
        // VICTIM id into both of those seats, which told the takedown layer that every car had
        // taken itself down.
        EntityId lRemappedAggressorId = lAggressorEntityId;
        if (luCauseSubCode == 2)
        {
            const s32 liAggressorIndex =
                static_cast<s32>((lAggressorEntityId.muValue >> 10) & 0x3FFF);
            lRemappedAggressorId = mPhysicalTrafficManager.maTrafficEntityIDs[liAggressorIndex];
        }

        // ---- Step 2c: the duplicate-crash gate ----
        // The same (victim, aggressor) pair commits ONCE while its crash record is still alive: a
        // repeat contact between the same two cars must not re-fire the crash event or burn a
        // second crash-data slot. The key is the two ids AS PASSED IN, before either remap, which
        // is exactly what the slot seats hold. This gate was missing from this sink entirely (the
        // remote-crash sink has always had it), so a sustained car-on-car contact re-committed the
        // victim every step and churned the 32-slot pool.
        for (s32 liUsedCrash = mUsedRaceCarCrashesList.GetFirstNonZeroBit();
             liUsedCrash >= 0;
             liUsedCrash = mUsedRaceCarCrashesList.GetNextNonZeroBit(liUsedCrash))
        {
            if (maRaceCarCrashes[liUsedCrash].mRaceCarEntityID.muValue == lVictimEntityId.muValue
                && maRaceCarCrashes[liUsedCrash].mOtherEntityID.muValue == lAggressorEntityId.muValue)
            {
                return;
            }
        }

        // Car type 1 is the AI type -- the same value the mbStopAICrashing suppression gate keys
        // on. The crash event carries it as its is-AI flag.
        const bool lbVictimIsAI = (liCrashState == 1);

        // ---- Step 3: the two crash-commit branches (the heart) ----
        RaceCarPhysics& lrVictimRecord = maRaceCarVehicles[liVictimIndex];   // asm _R31 = 5216*v36 + this
        RaceCarPhysics* const lpVictimPhysics =
            reinterpret_cast<RaceCarPhysics*>(&lrVictimRecord);                    // asm RaceCarPhysics @ _R31 + 1856

        if (lrVictimRecord.mbCrashing)
        {
            // (A) ALREADY-CRASHING path: fire the LIGHT crash event and fall through to the
            // crash-data slot alloc. asm loc_826354EC..0x82635530 -- the whole argument block is
            // `li r10,0 ; li r8,0 ; li r7,0`, i.e. BOTH bool arguments are hard zeros here.
            // (RE-NAMED 2026-09-02: this branch is not "remote", it is `mbCrashing` already TRUE
            // at 0x82635320 -- the victim is mid-crash and gets a second, lighter event.)
            if (lpManagerOutputInterface)
            {
                // CORRECTED 2026-09-11: this path publishes the INCOMING contact normal, not the
                // record's stored one. Only path (B) restamps the record, so reading it back here
                // handed the pile-on event the PREVIOUS wreck's normal.
                // The event's scalar float is the crash SPEED IN MPH -- the .x lane of the victim's
                // packed speed/time vector, not a position.
                lpManagerOutputInterface->AddRaceCarCrashEvent(
                    lValidatedVictimId,
                    lRemappedAggressorId,
                    lCollisionNormal,
                    lContactPoint,
                    /*lbIsPrimaryCrash=*/false,
                    /*lbRemoveHandlingVolumeFromScene=*/false,
                    lbVictimIsAI,
                    /*lbCarIsNetwork=*/false,
                    lrVictimRecord.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.x,
                    // A pile-on carries no classification of its own.
                    BrnGameState::E_TAKEDOWN_NONE);
            }
        }
        else
        {
            // (B) LOCAL / physical-crash path.
            // The latch bool is an AND of four conditions: the player slot is not itself crashing,
            // the victim is behind the player along the player's forward axis, the victim is an AI
            // car, and the takedown type is one of the two "no specific classification" values.
            // RESOLVED 2026-09-11: the fourth term used to be modelled as always-true on the
            // assumption that every caller passes the NONE sentinel. It is a real gate -- the
            // classifier arms pass a specific type, and a classified takedown deliberately does
            // NOT latch the victim's physics into the crash replay.
            const s32 liPlayerIndex = static_cast<s32>(mePlayerActiveRaceCarIndex);   // asm v109
            RaceCarPhysics& lrPlayerRecord = maRaceCarVehicles[liPlayerIndex];  // asm _R10 = 5216*v109 + this

            // RE-DECODED 2026-08-03 (VehiclePhysics own-block wave). This used to be
            //     ||victim.pos - player.pos||^2 < victim.mfProximityRadiusSq
            // built on two PHANTOM members (`mvWorldPosition` @+1920, `mfProximityRadiusSq` @+1904).
            // Both offsets are applied to a CLASS-relative base in the asm, so the in-record seats
            // are +0x40 and +0x30 -- rows 3 and 2 of the base's mTransform. There is no radius. The
            // console does (@0x82635364..0x82635398):
            //     lvx128 v0, victimClass+0x780   ; victim mTransform.pos
            //     lvx128 v12, playerClass+0x780  ; player mTransform.pos
            //     vsubfp v0, v0, v12             ; delta
            //     lvx128 v12, playerClass+0x770  ; player mTransform.zAxis (the forward axis)
            //     vmsum3fp128 v0, v0, v12        ; dot3(delta, playerForward)
            //     vcmpgtfp.  v0, splat(f30), v0  ; f30 == flt_82001CC0 == 0.0f
            // i.e. **"is the victim BEHIND the player along the player's forward axis"**. That is a
            // visibility test for whether the crash is worth latching for the replay camera, which
            // is what the surrounding branch is for -- a proximity radius never existed.
            const Vector3 lvDelta = lrVictimRecord.mTransform.Pos() - lrPlayerRecord.mTransform.Pos();
            const Vector3& lvPlayerForward = lrPlayerRecord.mTransform.At();   // mTransform row 2
            const f32 lfAlongForward =
                lvDelta.x * lvPlayerForward.x + lvDelta.y * lvPlayerForward.y + lvDelta.z * lvPlayerForward.z;
            const bool lbBehindPlayer = (0.0f > lfAlongForward);   // asm vcmpgtfp. 0.0f > dot

            const bool lbUnclassifiedTakedown = (leTakedownType == BrnGameState::E_TAKEDOWN_NONE)
                                             || (leTakedownType == BrnGameState::E_TAKEDOWN_STANDARD);

            const bool lbLatchPhysics =
                (!lrPlayerRecord.mbCrashing)
                && lbBehindPlayer
                && lbVictimIsAI
                && lbUnclassifiedTakedown;

            // The single call site of RaceCarPhysics::SetCrashing: latch the victim's physics into
            // the crash replay ONLY when near the player; distant cars crash "logically" (SetCrashing
            // is still called, with false -- the vtbl call runs but no velocity latch).
            lpVictimPhysics->SetCrashing(lbLatchPhysics);

            // (asm debug print " Physically crashing local car <idx>" -- log only, not reproduced.)

            // Stamp the vehicle record: mCrashNormal @ +5184 and mEntityCausingCrash @ +5200 (the
            // console's SetCrashEntityIdAndNormal inlined -- `stvx128 v127, r30, 0x1440` four
            // instructions before `stw r26, 0x1450(r30)`).
            // RE-SEATED 2026-08-03: the flag this used to set was `mbCrashCommitted` at +3097.
            // The asm store is `stb r20(1), 0x1359(r11)` and r11 was made the RECORD base two
            // instructions earlier (`addi r11, r11, 0x740`), so the seat is in-record 4953 and the
            // member is VehiclePhysics::mbDeformationModelIsActive.
            //
            // ⭐⭐ CONTROL FLOW RE-DECODED 2026-09-02 (deform close-out wave) -- the FLAG that used
            // to sit here ("ITS GUARD IS INVERTED HERE ... only when the RE-READ mbCrashing is
            // NON-zero, i.e. on the already-crashing path -- not on this local one") was WRONG
            // about WHICH path, and it hid a store the tree never made at all. The asm:
            //     0x82635320  lbz    r11, 0xE50(r31)   ; mbCrashing BEFORE anything -- the A/B split
            //     0x82635330  bne    cr6, loc_826354EC ; already crashing -> path (A), the light event
            //     ...         (path B) bl RaceCarPhysics::SetCrashing @0x826353E0
            //     0x82635424  lbz    r10, 0xE50(r11)   ; RE-READ, after SetCrashing
            //     0x8263542C  beq    cr6, loc_82635470 ; not crashing -> skip, r8 = r23 (0)
            //     0x82635430  addi   r11, r11, 0x740   ; the SimpleVehiclePhysics sub-object
            //     0x82635440  stb    r20, 0x1359(r11)  ; mbDeformationModelIsActive = 1
            //     0x82635438/3C + 0x8263544C..68        ; ResetDeformableAABB() inlined
            //     0x8263546C  b      loc_82635474      ; r8 = r20 (1)
            // So the re-read gate is on path (B) -- THIS branch -- and it is a COMMON TAIL after
            // SetCrashing, exactly as the second FLAG below said. It is also DEAD in one direction:
            // SimpleVehiclePhysics::SetCrashing @0x825D990C stores `stb 1, 0x710(r3)` (mbCrashing)
            // UNCONDITIONALLY, and path B always calls it, so the re-read is always non-zero here
            // and the taken arm always runs. The bool is kept as the console spells it rather than
            // constant-folded, so the shape stays readable against the asm.
            const bool lbCrashingAfterLatch = lrVictimRecord.mbCrashing;   // asm 0x82635424 re-read
            if (lbCrashingAfterLatch)
            {
                lrVictimRecord.mbDeformationModelIsActive = 1;      // asm *(record+4953) = 1

                // ⭐ THE MISSING STORE (deform close-out wave, 2026-09-02). NOTHING in the tree
                // ever restored mDeformableAABB: UpdateDeformedBBox @0x825E0D20 is the only other
                // writer and it only ever GROWS it from the live sensor spheres, and it runs only
                // on the IK-budgeted frames. The console snaps the box back onto mOriginalAABB the
                // moment a crash latches, so each impact starts from the undeformed box. Since
                // 97c3a7e1 made mDeformableAABB the CLAMP SOURCE for UpdateSkinningOffsets
                // @0x825DFA90, a box left grown by the previous wreck does not just read wrong --
                // it mis-clamps the next hit's driven points.
                lrVictimRecord.ResetDeformableAABB();               // asm 0x82635438..0x82635468
            }
            // The victim's record remembers the contact normal and WHO caused the crash.
            // CORRECTED 2026-09-11: the second seat takes the remapped AGGRESSOR, not the victim --
            // the member's own name says so, and the sink used to write the victim into it.
            lrVictimRecord.mCrashNormal        = lCollisionNormal;
            lrVictimRecord.mEntityCausingCrash = lRemappedAggressorId;

            // Fire the FULL crash event.
            if (lpManagerOutputInterface)
            {
                lpManagerOutputInterface->AddRaceCarCrashEvent(
                    lValidatedVictimId,
                    lRemappedAggressorId,
                    lCollisionNormal,
                    lContactPoint,
                    /*lbIsPrimaryCrash=*/true,
                    lbCrashingAfterLatch,
                    lbVictimIsAI,
                    /*lbCarIsNetwork=*/false,
                    lrVictimRecord.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.x,
                    leTakedownType);

                // Push the 32-byte crash record onto the game-side event queue at the vehicle
                // output interface's +0x65F0 -- the same sink as the takedown/grind pushes.
                RaceCarCrashIoEventRecord lEventRecord;
                lEventRecord.mVictimEntityId  = lValidatedVictimId;
                lEventRecord.mCrasherEntityId = lRemappedAggressorId;
                lEventRecord.mbFlag           = 0;
                lEventRecord.mfReserved       = 0.0f;
                lEventRecord.muVictimIndex    = static_cast<u32>(liVictimIndex);
                lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEventRecord), 63, 32);
            }
        }

        // ---- Step 4: allocate (or overwrite) a RaceCarCrashData[32] slot ----
        // The asm finds the first FREE slot via the complement-scan idiom (v140 = ~field; the lowest
        // set bit of the complement == the lowest CLEAR bit == the first free slot). A set bit in
        // mUsedRaceCarCrashesList == an allocated slot, so a free slot is the first CLEAR bit. The
        // CgsBitArray API exposes IsBitSet (used by name to find the first clear bit).
        s32 liSlot = -1;
        for (s32 liScan = 0; liScan < 32; ++liScan)   // asm: first clear bit in the alloc bitfield
        {
            if (!mUsedRaceCarCrashesList.IsBitSet(static_cast<u32>(liScan)))
            {
                liSlot = liScan;
                break;
            }
        }
        if (liSlot < 0)
        {
            // Pool full: overwrite the OLDEST occupied slot (asm: the "WARNING:
            // Overwriting a RaceCarCrashData class" path scans +43816 -- mfTimeSinceImpact, the
            // float UpdateCrashes @0x825EA640 ages every frame -- for the max and reuses that slot).
            liSlot = 0;
            f32 lfOldestTime = maRaceCarCrashes[0].mfTimeSinceImpact;
            for (s32 liScan = 1; liScan < 32; ++liScan)
            {
                if (maRaceCarCrashes[liScan].mfTimeSinceImpact > lfOldestTime)
                {
                    lfOldestTime = maRaceCarCrashes[liScan].mfTimeSinceImpact;
                    liSlot = liScan;
                }
            }
        }

        // Write the slot. Both ids are the PRE-REMAP ones the caller passed, which is what makes
        // the duplicate-crash gate above a valid key.
        maRaceCarCrashes[liSlot].mfTimeSinceImpact = 0.0f;
        maRaceCarCrashes[liSlot].mOtherEntityID    = lAggressorEntityId;
        maRaceCarCrashes[liSlot].mRaceCarEntityID  = lVictimEntityId;
        mUsedRaceCarCrashesList.SetBit(static_cast<u32>(liSlot));   // asm: set the allocation bit (v179 OR into field)

        // [td-crash] PC witness (NOT the console), first 16 only: this sink is the ONLY road from a
        // contact to a crash record, so a run with [td-contact] verdicts but no [td-crash] line was
        // suppressed or de-duplicated above, and a run with [td-crash] lines but nothing scored is a
        // fault further down the chain. Prints the two car indices, the takedown type the caller
        // classified, the slot just written, and how many crash records are live.
        //
        // ⭐ THE ATTACKER IS AN ENTITY ID, NOT A SLOT INDEX (re-read against the export this wave).
        // There is NO "-1 attacker" sentinel anywhere on this path: every caller passes a real
        // causing entity -- the world entity word for a wall hit, the traffic slot's global id for
        // a traffic hit, the other car's id for a car-on-car pair, and the victim's OWN id for the
        // self-inflicted arm. "No attacker" is carried by the OWNER byte, which is exactly what the
        // one consumer that reads this seat without a takedown type tests (the standard-takedown
        // classifier compares the owner against the traffic type and names the aggressor from the
        // victim's shunt state instead). So the earlier reading of "attacker 0 -> victim N" as the
        // player being credited for every rival's own crash was the WITNESS dropping the owner
        // byte: owner 0 is the WORLD, and its entity field is 0. Both owners are printed now.
        // [FLAG PC witness]  DELETE-WHEN: the organic takedown case goes green.
        {
            static s32 siCrashWitnessed = 0;
            if (siCrashWitnessed < 16 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siCrashWitnessed;

                const u32 luAggressorWitnessOwner = (lAggressorEntityId.muValue >> 24) & 0xFF;
                const s32 liAggressorWitnessIndex =
                    static_cast<s32>((lAggressorEntityId.muValue >> 10) & 0x3FFF);
                s32 liLiveCrashRecords = 0;
                for (s32 liScan = 0; liScan < 32; ++liScan)
                {
                    if (mUsedRaceCarCrashesList.IsBitSet(static_cast<u32>(liScan)))
                    {
                        ++liLiveCrashRecords;
                    }
                }

                *CgsDev::Log::gpDebugPrint << "[td-crash] attacker "
                                           << WitnessEntityOwnerName(luAggressorWitnessOwner)
                                           << "(" << static_cast<s32>(luAggressorWitnessOwner) << ")"
                                           << ":" << liAggressorWitnessIndex
                                           << " -> victim "
                                           << WitnessEntityOwnerName(luVictimOwner)
                                           << "(" << static_cast<s32>(luVictimOwner) << ")"
                                           << ":" << liVictimIndex
                                           << " takedownType=" << WitnessTakedownTypeName(static_cast<s32>(leTakedownType))
                                           << "(" << static_cast<s32>(leTakedownType) << ")"
                                           << " slot=" << liSlot
                                           << " records=" << liLiveCrashRecords
                                           << " [FLAG PC witness]\n";
            }
        }

        // ---- Step 5: secondary remapped-entity event ----
        // Fired only when the AGGRESSOR id is traffic-owned: the sub-event republishes the entity
        // index of that traffic slot's global id onto the manager's traffic-type request queue.
        // CORRECTED 2026-09-11: the index used to come from the traffic remap of the VICTIM's slot,
        // a different car -- and out of range whenever the victim is a race car.
        if (luCauseSubCode == 2 && lpManagerOutputInterface)
        {
            const u32 luRemappedIndex = (lRemappedAggressorId.muValue >> 10) & 0x3FFF;
            lpManagerOutputInterface->AddRemappedEntityIdEvent(luRemappedIndex);
        }
    }

    // ===========================================================================================
    // The eight per-type sub-classifiers (run in priority order by CheckForAllTypesOfImpacts).
    //
    // Conventions used below:
    //  - "named access only": the asm's raw offsets are resolved to the RaceCarResponseInfo fields
    //    (a2+92 == mfRaceCarASpeed, a2+96 == mfRaceCarBSpeed, a2+240 == mfAngleBetweenCars, etc.),
    //    the §7 deep VehicleManager tuning members, and maRaceCarVehicles[idx]. The collision
    //    normal / contact point the InstantTakedown calls pass-by-VMX-register are the contact's
    //    mpContact->mNormal (+48) and mpContact->mPointOnA (+64).
    //  - asm-visible immediates (pi/2, pi/180, 5.0, 0.04, 1.9, 180.0) are used directly; rodata
    //    floats whose VALUES are not in the per-function exports are file-static KF_/KVF_
    //    placeholders, FLAGged.
    //  - the SIMD plane-geometry helpers and the recency throttle are declared-only callees.
    // ===========================================================================================

    // ---- shared rodata values -- ⭐ ALL RECOVERED 2026-08-24 (deform-land wave, P5) ----------
    // physics11 audit cluster A: every writer decoded from the static-init region via headless
    // idat (0x82C5B950 / 0x82C5B970 / 0x82C5B990 / 0x82C5BA78), each consumer asm-witnessed
    // (stationary classifier 0x8263D9E0..0x8263DA50; paint-alignment vcmpgtfp 0x82619FC4..).
    // ⚠️ The old flags called 1.0/0.0 "identity" placeholders -- neither was: with 1.0 every
    // speed threshold in this file ran 2.237x hot (MPH taken as m/s), and the three 0.0 gates
    // never fired. flt_82F31928 == 0.44704 was already triple-witnessed in-tree
    // (TrafficPhysics.h:177, BrnSimpleVehiclePhysics.h:422, RaceCarPhysics.cpp:489).
    // The global speed-unit scale that multiplies every tuning speed threshold (flt_82F31928).
    static const f32 KF_SPEED_UNIT_SCALE       = 0.44704f;   // flt_82F31928 (image-read): MPH -> m/s
    // Stationary-target speed thresholds (flt_82FB8298 asymmetry gate, 829C slow cap, 7F18 fast floor).
    static const f32 KF_STATIONARY_MIN_SPEED_DIFF = 0.44704f * 40.0f; // flt_82FB8298 <- init 0x82C5B950 (17.8816 m/s)
    static const f32 KF_STATIONARY_SLOW_CAP        = 0.44704f * 20.0f; // flt_82FB829C <- init 0x82C5B970 (8.9408 m/s)
    static const f32 KF_STATIONARY_FAST_FLOOR      = 0.44704f * 60.0f; // flt_82FB7F18 <- init 0x82C5B990 (26.8224 m/s)
    // Trading-paint alignment-dot gate (unk_82FB8310). A dot >= this means the cars are aligned
    // enough to be side-by-side rather than a true crossing impact.
    static const f32 KF_PAINT_ALIGNMENT_GATE   = 0.75f;   // unk_82FB8310 <- init 0x82C5BA78 splat(flt_82004018 = 0.75)

    // EntityId packing helper: re-encode an active-race-car index into the EntityId word the
    // commit routines decode. The X360 spells this `(luEntityIndex << 10) | 0x1000000` (the
    // 0x1000000 type bits == E_ENTITYTYPE_RACECAR; the index occupies bits 10..23). Matches the
    // inline encode in the heavier classifiers.
    static inline EntityId MakeRaceCarEntityId(u32 luEntityIndex)
    {
        return EntityId{ (luEntityIndex << 10) | 0x1000000u };
    }

    // -------------------------------------------------------------------------------------------
    // CheckForTBoneTakedown  @0x8263D480  ->  T_BONE
    //
    // Perpendicular hit. Gates: neither car already crashing; the angle between the cars is within
    // an asm band of pi/2: |mfAngleBetweenCars - pi/2| < mfTBoneTakedownMaxAngle * (pi/180). Then a
    // side-plane containment test (IsPointBetweenTwoParallelPlanes, half-width
    // mfTBoneTakedownSpeed * scale) decides which car is the victim. Commits T_BONE via
    // InstantTakedown with the contact normal/point.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForTBoneTakedown(RaceCarResponseInfo* lpInfo)
    {
        // Already-crashing cars can't be freshly T-boned (asm: a2+80 / a2+81).
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        // IsPointBetweenTwoParallelPlanes is recovered
        // (@0x825C5660, four vector args -- point, a point on each plane, the shared normal) and
        // this block is rebuilt from the caller asm @0x8263D4F0..0x8263D69C. The console's actual
        // test is NOT "contact point in a slab": each arm asks whether ONE CAR'S POSITION lies
        // inside the OTHER car's fore/aft slab (the slab built from that car's At axis and its
        // deformable-AABB min/max z, inset by 1.0m each end == splat @0x82FB8280), gated on the
        // rammer's lateral speed across the victim's side (|dot(victim.right, rammer.mLastLinear-
        // Velocity)| > mfTBoneTakedownSpeed * 0.44704 -- a SPEED threshold, not a half-width).
        bool lbARamsB = false;  // asm r9  (A's position inside B's slab; A supplies the side speed)
        bool lbBRamsA = false;  // asm r26 (B's position inside A's slab)

        // Perpendicularity band: ||angle| - pi/2| < band(deg) * (pi/180)  (flt_8208F604/8208F5F4).
        const f32 lfPiOver2  = 1.5707964f;
        const f32 lfDegToRad = 0.017453292f;
        // [td-tbone] PC witness: the perpendicularity term and the two lateral-speed terms.
        if (TakedownDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
        {
            const RaceCarPhysics& lrWA = maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexA)];
            const RaceCarPhysics& lrWB = maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexB)];
            *CgsDev::Log::gpDebugPrint
                << "[td-tbone] |angle-pi/2|=" << std::fabs(std::fabs(lpInfo->mfAngleBetweenCars) - lfPiOver2)
                << " band=" << (mfTBoneTakedownMaxAngle * lfDegToRad)
                << " latA=" << std::fabs(vpu::Dot(lrWB.mTransform.xAxis, lrWA.mLastLinearVelocity))
                << " latB=" << std::fabs(vpu::Dot(lrWA.mTransform.xAxis, lrWB.mLastLinearVelocity))
                << " minLat=" << (mfTBoneTakedownSpeed * KF_SPEED_UNIT_SCALE)
                << " [FLAG PC witness]\n";
        }
        if (std::fabs(std::fabs(lpInfo->mfAngleBetweenCars) - lfPiOver2)
            < mfTBoneTakedownMaxAngle * lfDegToRad)
        {
            const f32 KF_SLAB_INSET     = 1.0f;   // 0x82FB8280 <- splat(flt_82001C98)
            const f32 lfMinLateralSpeed = mfTBoneTakedownSpeed * KF_SPEED_UNIT_SCALE;   // flt_82F31928

            RaceCarPhysics* const lpCarA =
                &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexA)];
            RaceCarPhysics* const lpCarB =
                &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexB)];

            // Arm 1 -- A rams B's side: A's last linear velocity across B's right axis clears the
            // threshold AND A's position sits between B's (inset) front/rear planes.
            if (std::fabs(vpu::Dot(lpCarB->mTransform.xAxis, lpCarA->mLastLinearVelocity))
                    > lfMinLateralSpeed)
            {
                const Vector3 lvBFront = lpCarB->mTransform.zAxis * lpCarB->GetDeformableAABB().mMax.z
                                       + lpCarB->mTransform.wAxis - lpCarB->mTransform.zAxis * KF_SLAB_INSET;
                const Vector3 lvBRear  = lpCarB->mTransform.zAxis * lpCarB->GetDeformableAABB().mMin.z
                                       + lpCarB->mTransform.wAxis + lpCarB->mTransform.zAxis * KF_SLAB_INSET;
                if (IsPointBetweenTwoParallelPlanes(lpCarA->mTransform.wAxis, lvBFront, lvBRear,
                                                    lpCarB->mTransform.zAxis))
                {
                    lbARamsB = true;
                }
            }

            // Arm 2 -- only when arm 1 did not fire: B rams A's side, the mirrored test.
            if (!lbARamsB
                && std::fabs(vpu::Dot(lpCarA->mTransform.xAxis, lpCarB->mLastLinearVelocity))
                       > lfMinLateralSpeed)
            {
                const Vector3 lvAFront = lpCarA->mTransform.zAxis * lpCarA->GetDeformableAABB().mMax.z
                                       + lpCarA->mTransform.wAxis - lpCarA->mTransform.zAxis * KF_SLAB_INSET;
                const Vector3 lvARear  = lpCarA->mTransform.zAxis * lpCarA->GetDeformableAABB().mMin.z
                                       + lpCarA->mTransform.wAxis + lpCarA->mTransform.zAxis * KF_SLAB_INSET;
                if (IsPointBetweenTwoParallelPlanes(lpCarB->mTransform.wAxis, lvAFront, lvARear,
                                                    lpCarA->mTransform.zAxis))
                {
                    lbBRamsA = true;
                }
            }
        }

        if (!lbARamsB && !lbBRamsA)
            return false;

        const bool lbInPlanesA = lbARamsB;   // keep the downstream victim-order code readable
        const bool lbInPlanesB = lbBRamsA;

        // Entity-id order, verbatim from the asm: the v4 (A-planes) path loads victim=idB,
        // aggressor=idA; the v6 (B-planes) path loads victim=idA, aggressor=idB.
        const EntityId lVictim    = lbInPlanesA ? lpInfo->mRaceCarBEntityID : lpInfo->mRaceCarAEntityID;
        const EntityId lAggressor = lbInPlanesA ? lpInfo->mRaceCarAEntityID : lpInfo->mRaceCarBEntityID;

        InstantTakedown(lVictim, lAggressor,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_T_BONE);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForHeadToHead  @0x8263D1A0  ->  HEAD_ON (the only classifier that BOTH shoves AND crashes)
    //
    // Gates: neither car crashing; |angle| >= (180 - tolerance) * (pi/180); at least one car's
    // speed > minClosingSpeed * scale. Decides the loser by mass times speed, multiplies the closing
    // magnitude by 5.0, sets impact type 4 (E_IMPACT_SHUNT), applies a shove (ApplyShunt), and
    // commits HEAD_ON for the network ownership case; offline continues down the classifier ladder.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForHeadToHead(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;
        if (std::fabs(lpInfo->mfAngleBetweenCars) < (180.0f - mfMaxHeadToHeadAngle) * 0.017453292f)
            return false;
        const f32 lfMinimumSpeed = mfMinHeadToHeadIndividualSpeed * KF_SPEED_UNIT_SCALE;
        if (!(lpInfo->mfRaceCarASpeed > lfMinimumSpeed) && !(lpInfo->mfRaceCarBSpeed > lfMinimumSpeed))
            return false;
        const f32 lfMomentumA = lpInfo->mpRaceCarA->GetMass().x * lpInfo->mfRaceCarASpeed;
        const f32 lfMomentumB = lpInfo->mpRaceCarB->GetMass().x * lpInfo->mfRaceCarBSpeed;
        EntityId lVictimId;
        EntityId lAggressorId;
        if (lfMomentumA > lfMomentumB)
        {
            if (lpInfo->mbRaceCarBIsNetworkCar) return false;
            lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA;
            lpInfo->meVictimActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexB;
            lpInfo->mbPlayerWonImpact = lpInfo->mbRaceCarAIsPlayer;
            lVictimId = lpInfo->mpContact->mEntityIdB;
            lAggressorId = lpInfo->mpContact->mEntityIdA;
        }
        else if (lfMomentumA < lfMomentumB)
        {
            if (lpInfo->mbRaceCarAIsNetworkCar) return false;
            lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexB;
            lpInfo->meVictimActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA;
            lpInfo->mbPlayerWonImpact = lpInfo->mbRaceCarBIsPlayer;
            lVictimId = lpInfo->mpContact->mEntityIdA;
            lAggressorId = lpInfo->mpContact->mEntityIdB;
        }
        else
        {
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true;
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true;
            return true;
        }
        lpInfo->mfClosingSpeed *= 5.0f;
        lpInfo->meImpactType = E_IMPACT_SHUNT;
        ApplyShunt(lpInfo);
        // The explicit HEAD_ON commit is network-only; offline falls through the classifier ladder.
        if (!(lpInfo->mbRaceCarBIsNetworkCar || lpInfo->mbRaceCarAIsNetworkCar))
            return false;
        if (lpInfo->mbPlayerWonImpact ? !(lAggressorId.muValue < lVictimId.muValue)
                                     : !(lVictimId.muValue < lAggressorId.muValue))
            return false;
        InstantTakedown(lVictimId, lAggressorId, lpInfo->mpContact->mNormal, lpInfo->mpContact->mPointOnA,
            lpInfo->mfNormalStressSq, lpInfo->mpRequestOutputInterface, lpInfo->mpManagerOutputInterface,
            lpInfo->mpVehicleOutputInterface, lpInfo->mpDeformationInterface, BrnGameState::E_TAKEDOWN_HEAD_ON);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForStationaryTargetTakedown  @0x8263D948  ->  STANDARD (taking down a near-stopped car)
    //
    // Gates: neither car crashing; the master gate mbIsOnlineGameMode; the two cars are
    // close (squared distance between their positions < 0.04); a speed asymmetry
    // (|speedA - speedB| >= flt_82FB8298, the slower car <= flt_82FB829C, the faster >= flt_82FB7F18).
    // No shove (the victim is stationary) -- commits straight to InstantTakedown.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForStationaryTargetTakedown(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;
        if (!mbIsOnlineGameMode)
            return false;

        // Exclude coincident spawn positions: squared distance must be >= 0.04 (asm immediate
        // 0.040000003). The asm subtracts the +160 / +224 transform lanes -- the position (Pos)
        // axes of mRaceCarA/BTransform.
        const Vector3 lvPosA = lpInfo->mRaceCarATransform.Pos();
        const Vector3 lvPosB = lpInfo->mRaceCarBTransform.Pos();
        const f32 ldx = lvPosA.x - lvPosB.x;
        const f32 ldy = lvPosA.y - lvPosB.y;
        const f32 ldz = lvPosA.z - lvPosB.z;
        const f32 lfDistSq = ldx * ldx + ldy * ldy + ldz * ldz;
        // 0x8263D9DC branches OUT when 0.04 > distance squared.
        if (lfDistSq < 0.040000003f)
            return false;

        // Speed asymmetry. The asm reads a2[23]/a2[24] (mfRaceCarASpeed/BSpeed).
        const f32 lfSpeedA = lpInfo->mfRaceCarASpeed;
        const f32 lfSpeedB = lpInfo->mfRaceCarBSpeed;
        if (std::fabs(lfSpeedA - lfSpeedB) < KF_STATIONARY_MIN_SPEED_DIFF) // FLAG: rodata flt_82FB8298
            return false;

        EActiveRaceCarIndex leVictim;
        EActiveRaceCarIndex leAggressor;
        EntityId lVictimId;
        EntityId lAggressorId;
        bool     lbPlayerWon;
        if (lfSpeedA <= lfSpeedB)
        {
            // A is the slower (stationary) VICTIM; B is the faster aggressor. (asm: speedA<=speedB)
            // asm: stamps victim(+63)=v16=indexA, aggressor(+62)=v15=indexB; InstantTakedown victim=idA.
            if (lfSpeedA > KF_STATIONARY_SLOW_CAP || lfSpeedB < KF_STATIONARY_FAST_FLOOR) // FLAG: rodata 829C / 7F18
                return false;
            leVictim    = lpInfo->meActiveRaceCarIndexA; // asm v16 = a2+7  -> victim slot a2[63]
            leAggressor = lpInfo->meActiveRaceCarIndexB; // asm v15 = a2+8  -> aggressor slot a2[62]
            lVictimId    = lpInfo->mRaceCarAEntityID;    // asm v18 = a2+5
            lAggressorId = lpInfo->mRaceCarBEntityID;    // asm v17 = a2+6
            lbPlayerWon  = lpInfo->mbRaceCarBIsPlayer;   // asm v14 = *(a2+83)
        }
        else
        {
            // B is the slower (stationary) VICTIM; A is the faster aggressor.
            if (lfSpeedB > KF_STATIONARY_SLOW_CAP || lfSpeedA < KF_STATIONARY_FAST_FLOOR) // FLAG: rodata 829C / 7F18
                return false;
            leVictim    = lpInfo->meActiveRaceCarIndexB;
            leAggressor = lpInfo->meActiveRaceCarIndexA;
            lVictimId    = lpInfo->mRaceCarBEntityID;
            lAggressorId = lpInfo->mRaceCarAEntityID;
            lbPlayerWon  = lpInfo->mbRaceCarAIsPlayer;   // asm v14 = *(a2+82)
        }

        // Stamp the impact bookkeeping the asm writes before committing.
        lpInfo->meVictimActiveRaceCarIndex    = leVictim;    // asm *(_R11+63)=v16
        lpInfo->meAggressorActiveRaceCarIndex = leAggressor; // asm *(_R11+62)=v15
        lpInfo->mbPlayerWonImpact             = lbPlayerWon;  // asm *(_R11+258)=v14

        InstantTakedown(lVictimId, lAggressorId,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_STANDARD);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForShuntAndNudge  @0x8261A3A0  ->  FORCE ONLY (no crash). Recency-gated.
    //
    // Bails if either car has had a recent impact. Alignment must REACH 1.9 (a shunt is a
    // rear-end hit, so the contact normal has to be near-parallel to both cars' At axes; a
    // glancing contact belongs to trading paint). Picks the aggressor/victim by a closing-
    // velocity sign test, then -- if the victim is not already being slammed/shunted --
    // classifies the contact as nudge (closing <= mfMinShuntSpeed*scale, type 2) or shunt
    // (<= mfFatalShuntSpeed*scale, type 4), promoting shunt->boost-shunt (6) when the victim
    // is boost-eligible. Returns 1; never crashes.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForShuntAndNudge(RaceCarResponseInfo* lpInfo)
    {
        if (HasRaceCarHadRecentImpact(lpInfo->meActiveRaceCarIndexB)
            || HasRaceCarHadRecentImpact(lpInfo->meActiveRaceCarIndexA))
            return false;
        const f32 lfAlignment = std::fabs(vpu::Dot(lpInfo->mpContact->mNormal, lpInfo->mRaceCarATransform.At()))
                              + std::fabs(vpu::Dot(lpInfo->mpContact->mNormal, lpInfo->mRaceCarBTransform.At()));
        if (1.9f > lfAlignment)
            return false;
        const bool lbAIsBehind = vpu::Dot(vpu::Subtract(lpInfo->mRaceCarBTransform.Pos(),
            lpInfo->mRaceCarATransform.Pos()), lpInfo->mRaceCarATransform.At()) >= 0.0f;
        lpInfo->meAggressorActiveRaceCarIndex = lbAIsBehind ? lpInfo->meActiveRaceCarIndexA : lpInfo->meActiveRaceCarIndexB;
        lpInfo->meVictimActiveRaceCarIndex = lbAIsBehind ? lpInfo->meActiveRaceCarIndexB : lpInfo->meActiveRaceCarIndexA;
        lpInfo->mbPlayerWonImpact = lbAIsBehind ? lpInfo->mbRaceCarAIsPlayer : lpInfo->mbRaceCarBIsPlayer;
        // The AGGRESSOR must not already be receiving a shove from this victim (0x8261A4C4).
        if (maRaceCarVehicles[lpInfo->meAggressorActiveRaceCarIndex].IsBeingSlamedOrShuntedByRaceCar(
                static_cast<s8>(lpInfo->meVictimActiveRaceCarIndex)))
            return false;
        if (lpInfo->mfClosingSpeed > mfFatalShuntSpeed * KF_SPEED_UNIT_SCALE)
        {
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true;
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true;
            return true;
        }
        lpInfo->meImpactType = lpInfo->mfClosingSpeed > mfMinShuntSpeed * KF_SPEED_UNIT_SCALE
                            ? E_IMPACT_SHUNT : E_IMPACT_NUDGE;
        if (maRaceCarDrivers[lpInfo->meAggressorActiveRaceCarIndex].mControls.mbBoost
            && lpInfo->meImpactType == E_IMPACT_SHUNT)
            lpInfo->meImpactType = E_IMPACT_BOOST_SHUNT;
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForSlamAndTradingPaint  @0x82619F30  ->  FORCE ONLY (no crash). The lightest tier.
    //
    // Recency-gated. The contact-normal alignment must clear the paint-alignment gate
    // (unk_82FB8310). The combined energy must fall in the band [mfMinTradingPaintSpeed,
    // mfFatalSlamSpeed] (* scale). Sets impact severity 1/3/5 (TRADING_PAINT / SLAM /
    // BOOST_SLAM), stores a slam vector, returns 1. No crash.
    //
    // The slammer is picked from each car's own RaceCarPhysics::mfSlamSteering (in-record +0x1404),
    // signed by which side of the other car it sits on; the larger, positive one is the slammer.
    // A player slam is additionally vetoed while the grinding frame counters say the two cars have
    // been rubbing rather than slamming, and the SLAM-vs-trading-paint severity comes from a
    // per-car-type speed table.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForSlamAndTradingPaint(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;
        if (HasRaceCarHadRecentImpact(lpInfo->meActiveRaceCarIndexA)
            || HasRaceCarHadRecentImpact(lpInfo->meActiveRaceCarIndexB))
            return false;
        if (KF_PAINT_ALIGNMENT_GATE > vpu::Dot(lpInfo->mRaceCarATransform.At(), lpInfo->mRaceCarBTransform.At()))
            return false;
        const Vector3 lvBToA = vpu::Subtract(lpInfo->mRaceCarATransform.Pos(), lpInfo->mRaceCarBTransform.Pos());
        const f32 lfSideA = vpu::Dot(lvBToA, lpInfo->mRaceCarATransform.Right());
        const f32 lfSideB = vpu::Dot(lvBToA, lpInfo->mRaceCarBTransform.Right());
        const f32 lfSteerA = (lfSideA > 0.0f ? 1.0f : lfSideA >= 0.0f ? 0.0f : -1.0f) * lpInfo->mpRaceCarA->GetSlamSteering();
        const f32 lfSteerB = -(lfSideB > 0.0f ? 1.0f : lfSideB >= 0.0f ? 0.0f : -1.0f) * lpInfo->mpRaceCarB->GetSlamSteering();
        const bool lbPlayerInvolved = lpInfo->mbRaceCarAIsPlayer || lpInfo->mbRaceCarBIsPlayer;
        if (lpInfo->mfClosingSpeed <= mfMinTradingPaintSpeed * KF_SPEED_UNIT_SCALE)
            return false;
        if (lpInfo->mfClosingSpeed > mfFatalSlamSpeed * KF_SPEED_UNIT_SCALE)
        {
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true;
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true;
            return true;
        }
        // flt_82F2A218: PLAYER, AI, NETWORK, INACTIVE steering thresholds, read from ARTIST.
        static const f32 KAF_SLAM_STEERING_THRESHOLD[4] = { 0.1f, 0.3f, 0.1f, 0.1f };
        const s32 liA = lpInfo->meActiveRaceCarIndexA;
        const s32 liB = lpInfo->meActiveRaceCarIndexB;
        if (lfSteerA >= lfSteerB && lfSteerA > 0.0f
            && !lpInfo->mpRaceCarA->IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liB)))
        {
            if (lbPlayerInvolved && lfSteerA < 0.5f)
            {
                if (liA == mePlayerActiveRaceCarIndex && mau8FramesSincePlayerGrindingOther[liB] < 30) return false;
                if (liB == mePlayerActiveRaceCarIndex && mau8FramesSinceOtherGrindingPlayer[liA] < 30) return false;
            }
            lpInfo->meImpactType = lfSteerA > KAF_SLAM_STEERING_THRESHOLD[maeRaceCarTypes[liA]]
                                ? E_IMPACT_SLAM : E_IMPACT_TRADING_PAINT;
            lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA;
            lpInfo->meVictimActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexB;
            lpInfo->mvfSlamMagnitude = SplatVecFloat_VM(lfSteerA);
        }
        else if (lfSteerB > 0.0f
            && !lpInfo->mpRaceCarB->IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liA)))
        {
            // ARTIST tests A's steering here too (fcmpu f31,f0 at 0x8261A294).
            if (lbPlayerInvolved && lfSteerA < 0.5f)
            {
                if (liA == mePlayerActiveRaceCarIndex && mau8FramesSincePlayerGrindingOther[liB] < 30) return false;
                if (liB == mePlayerActiveRaceCarIndex && mau8FramesSinceOtherGrindingPlayer[liA] < 30) return false;
            }
            lpInfo->meImpactType = lfSteerB > KAF_SLAM_STEERING_THRESHOLD[maeRaceCarTypes[liB]]
                                ? E_IMPACT_SLAM : E_IMPACT_TRADING_PAINT;
            lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexB;
            lpInfo->meVictimActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA;
            lpInfo->mvfSlamMagnitude = SplatVecFloat_VM(lfSteerB);
        }
        // Check the type first: when neither car steers in, there is no aggressor slot to read.
        if (lpInfo->meImpactType == E_IMPACT_SLAM
            && maRaceCarDrivers[lpInfo->meAggressorActiveRaceCarIndex].mControls.mbBoost)
            lpInfo->meImpactType = E_IMPACT_BOOST_SLAM;
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForVerticalTakedown  @0x8263D728  ->  VERTICAL
    //
    // Early-outs if BOTH cars had a recent impact. Then for each candidate victim it calls the
    // sibling CheckForVerticalTakedownSituation (the up-axis geometry test) and an up-axis height
    // comparison (the asm compares a transform +4192 lane against rodata unk_82FB82A0, then equality
    // against 0). Commits VERTICAL.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForVerticalTakedown(RaceCarResponseInfo* lpInfo)
    {
        // Early-out only when BOTH cars are recency-blocked (asm: && of the two recency checks).
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexB))
            && HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexA)))
        {
            if (TakedownDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "[td-vert] REJECT both-recent [FLAG PC witness]\n";
            return false;
        }

        RaceCarPhysics* const lpVehB = &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexB)];
        RaceCarPhysics* const lpVehA = &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexA)];

        // [td-vert] PC witness: every gate of both arms, so a rejection names its own term.
        if (TakedownDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[td-vert] armA(B victim) footprint=" << (CheckForVerticalTakedownSituation(lpVehB, lpInfo->mpContact->mPointOnB) ? 1 : 0)
                << " aggrInAir=" << (lpVehA->IsReallyInAir() ? 1 : 0)
                << " victimGroundedLane=" << lpVehB->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z
                << " | armB(A victim) footprint=" << (CheckForVerticalTakedownSituation(lpVehA, lpInfo->mpContact->mPointOnA) ? 1 : 0)
                << " aggrInAir=" << (lpVehB->IsReallyInAir() ? 1 : 0)
                << " victimGroundedLane=" << lpVehA->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z
                << " [FLAG PC witness]\n";
        }

        bool lbFired = false;
        EntityId lVictimId{};
        EntityId lAggressorId{};

        // CheckForVerticalTakedownSituation is recovered
        // (@0x825C56D8 -- victim car + CONTACT POINT, the 80%-footprint test) and the two gates
        // this caller wraps around it are decoded from @0x8263D7CC/@0x8263D80C: the AGGRESSOR
        // must be really airborne (its air-time lane +0x1060.z > 0.2 == splat @0x82FB82A0,
        // exactly RaceCarPhysics::IsReallyInAir) and the VICTIM's air-time lane must be ZERO
        // (grounded). Candidate 1 tests A falling onto B (contact point on B); candidate 2, run
        // unconditionally after it, tests B falling onto A and OVERWRITES the pair when it fires.
        if (CheckForVerticalTakedownSituation(lpVehB, lpInfo->mpContact->mPointOnB)
            && lpVehA->IsReallyInAir()
            && lpVehB->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z == 0.0f)
        {
            lbFired = true;
            lVictimId    = lpInfo->mRaceCarBEntityID; // asm r25 = *(mpContact+4) = idB -> victim (r4)
            lAggressorId = lpInfo->mRaceCarAEntityID; // asm r5  = *(mpContact+0) = idA -> aggressor
        }

        if (CheckForVerticalTakedownSituation(lpVehA, lpInfo->mpContact->mPointOnA)
            && lpVehB->IsReallyInAir()
            && lpVehA->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z == 0.0f)
        {
            lbFired = true;
            lVictimId    = lpInfo->mRaceCarAEntityID; // asm r25 = *(mpContact+0) = idA -> victim (r4)
            lAggressorId = lpInfo->mRaceCarBEntityID; // asm r5  = *(mpContact+4) = idB -> aggressor
        }

        if (!lbFired)
            return false;

        InstantTakedown(lVictimId, lAggressorId,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_VERTICAL);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForPlayerSlammingAIIntoAI  @0x8263E000  ->  STANDARD (domino). Highest priority.
    //
    // The player rams one AI into a second AI. Requires both cars active AI (maeRaceCarTypes==1),
    // neither crashing, and (via per-record attacker bookkeeping vs mePlayerActiveRaceCarIndex) that
    // the player is the slammer. Calls ShouldRaceCarCrashOnCarImpact per victim and commits each
    // that passes.
    //
    // "The player is this car's current attacker" is answered per candidate from three of the car's
    // own VehiclePhysics fields: its last slammer/shunter, its last contacted race car, and how long
    // ago that contact was. The answer is both this function's entry gate (at least one car must
    // say yes) and the per-victim crash-threshold scale.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForPlayerSlammingAIIntoAI(RaceCarResponseInfo* lpInfo)
    {
        const s32 liA = lpInfo->meActiveRaceCarIndexA;
        const s32 liB = lpInfo->meActiveRaceCarIndexB;
        if (maeRaceCarTypes[liA] != BrnWorld::E_RACE_CAR_TYPE_AI
            || maeRaceCarTypes[liB] != BrnWorld::E_RACE_CAR_TYPE_AI
            || lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;
        RaceCarPhysics& lrCarA = maRaceCarVehicles[liA];
        RaceCarPhysics& lrCarB = maRaceCarVehicles[liB];
        // 0x82FB7F80 <- flt_8208F834 == 0.25, initializer 0x82C5BB38.
        const bool lbPlayerShovedA = lrCarA.mi8LastAttackersRaceCarIndex == mePlayerActiveRaceCarIndex
            && lrCarA.mi8LastContactedRaceCar == static_cast<s8>(mePlayerActiveRaceCarIndex)
            && lrCarA.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z < 0.25f;
        const bool lbPlayerShovedB = lrCarB.mi8LastAttackersRaceCarIndex == mePlayerActiveRaceCarIndex
            && lrCarB.mi8LastContactedRaceCar == static_cast<s8>(mePlayerActiveRaceCarIndex)
            && lrCarB.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z < 0.25f;
        if (!lbPlayerShovedA && !lbPlayerShovedB)
            return false;
        const VecFloat lvfImpactSpeed = SplatVecFloat_VM(lpInfo->mfClosingSpeed);
        bool lbAny = false;
        if (ShouldRaceCarCrashOnCarImpact(lpInfo->meActiveRaceCarIndexA, &lrCarA, &lrCarB, lvfImpactSpeed,
                SplatVecFloat_VM(lbPlayerShovedA ? KF_SLAM_REVENGE_CRASH_THRESHOLD_SCALE : KF_SLAM_DEFAULT_CRASH_THRESHOLD_SCALE)))
        {
            lpInfo->mbCrashRaceCarA = true;
            InstantTakedown(MakeRaceCarEntityId(liA), MakeRaceCarEntityId(mePlayerActiveRaceCarIndex),
                lpInfo->mpContact->mNormal, lpInfo->mpContact->mPointOnA, lpInfo->mfNormalStressSq,
                lpInfo->mpRequestOutputInterface, lpInfo->mpManagerOutputInterface,
                lpInfo->mpVehicleOutputInterface, lpInfo->mpDeformationInterface, BrnGameState::E_TAKEDOWN_STANDARD);
            lbAny = true;
        }
        if (ShouldRaceCarCrashOnCarImpact(lpInfo->meActiveRaceCarIndexB, &lrCarB, &lrCarA, lvfImpactSpeed,
                SplatVecFloat_VM(lbPlayerShovedB ? KF_SLAM_REVENGE_CRASH_THRESHOLD_SCALE : KF_SLAM_DEFAULT_CRASH_THRESHOLD_SCALE)))
        {
            lpInfo->mbCrashRaceCarB = true;
            InstantTakedown(MakeRaceCarEntityId(liB), MakeRaceCarEntityId(mePlayerActiveRaceCarIndex),
                vpu::Negate(lpInfo->mpContact->mNormal), lpInfo->mpContact->mPointOnB, lpInfo->mfNormalStressSq,
                lpInfo->mpRequestOutputInterface, lpInfo->mpManagerOutputInterface,
                lpInfo->mpVehicleOutputInterface, lpInfo->mpDeformationInterface, BrnGameState::E_TAKEDOWN_STANDARD);
            lbAny = true;
        }
        return lbAny;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForHittingAlreadyCrashingCar  @0x8263DAC0  ->  STANDARD (pile-on / finish-off)
    //
    // One car is already crashing and the other rams it. Runs BEFORE the not-crashing gate. Includes
    // a player-revenge sub-gate (mePlayerActiveRaceCarIndex vs the record's current-attacker field).
    // Calls ShouldRaceCarCrashOnCarImpact for the still-live car; commits if it passes and the two
    // cars' types are both E_RACE_CAR_TYPE_AI.
    //
    // "Already crashing" is the response-info flag AND-folded with the car's own time-crashing lane
    // (or its network flag); the revenge sub-gate then asks the crashing car's own record whether the
    // player is the last race car it touched, and how long ago.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForHittingAlreadyCrashingCar(RaceCarResponseInfo* lpInfo)
    {
        const s32 liA = lpInfo->meActiveRaceCarIndexA;
        const s32 liB = lpInfo->meActiveRaceCarIndexB;
        RaceCarPhysics& lrCarA = maRaceCarVehicles[liA];
        RaceCarPhysics& lrCarB = maRaceCarVehicles[liB];
        const bool lbAIsObstacle = lpInfo->mbRaceCarAIsCrashing
            && (lrCarA.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y > 1.0f || lpInfo->mbRaceCarAIsNetworkCar);
        const bool lbBIsObstacle = lpInfo->mbRaceCarBIsCrashing
            && (lrCarB.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y > 1.0f || lpInfo->mbRaceCarBIsNetworkCar);
        if (!lbAIsObstacle && !lbBIsObstacle)
            return false;
        RaceCarPhysics& lrObstacle = lbAIsObstacle ? lrCarA : lrCarB;
        RaceCarPhysics& lrVictim = lbAIsObstacle ? lrCarB : lrCarA;
        const EActiveRaceCarIndex leVictim = lbAIsObstacle ? lpInfo->meActiveRaceCarIndexB : lpInfo->meActiveRaceCarIndexA;
        CGS_ASSERT(lbAIsObstacle ? !lpInfo->mbRaceCarBIsCrashing : !lpInfo->mbRaceCarAIsCrashing,
                   "The other race car must not already be crashing");
        if (!ShouldRaceCarCrashOnCarImpact(leVictim, &lrVictim, &lrObstacle,
                SplatVecFloat_VM(CarCarImpactSpeed(lpInfo->mpContact->mNormal, lrVictim.mTransform.Up(), lpInfo->mClosingVelocityAtoB)),
                SplatVecFloat_VM(KF_PILEON_CRASH_THRESHOLD_SCALE)))
            return false;
        if (lbAIsObstacle ? lpInfo->mbRaceCarBIsNetworkCar : lpInfo->mbRaceCarAIsNetworkCar)
            return true;
        if (maeRaceCarTypes[liA] == BrnWorld::E_RACE_CAR_TYPE_AI && maeRaceCarTypes[liB] == BrnWorld::E_RACE_CAR_TYPE_AI
            && lrObstacle.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y < 0.5f
            && lrObstacle.mi8LastContactedRaceCar == static_cast<s8>(mePlayerActiveRaceCarIndex)
            && lrObstacle.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z < 1.0f)
        {
            InstantTakedown(MakeRaceCarEntityId(leVictim), MakeRaceCarEntityId(mePlayerActiveRaceCarIndex),
                lpInfo->mpContact->mNormal, lpInfo->mpContact->mPointOnA, lpInfo->mfNormalStressSq,
                lpInfo->mpRequestOutputInterface, lpInfo->mpManagerOutputInterface,
                lpInfo->mpVehicleOutputInterface, lpInfo->mpDeformationInterface, BrnGameState::E_TAKEDOWN_STANDARD);
        }
        if (lbAIsObstacle) lpInfo->mbCrashRaceCarB = true;
        else lpInfo->mbCrashRaceCarA = true;
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // Layout pins for the deep members InstantTakedown reaches. Never called; exists only to host
    // the offsetof asserts (offsetof on a private member must be evaluated in member-function scope).
    // Each offset here is asm-proven; if a padding run drifts, the gate fails -- that is intended.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::_AssertLayout()
    {
        // ---- the class HEAD (re-seated 2026-08-03; see the header banner) --------------------
        static_assert(offsetof(VehicleManager, mePrepareStage) == 0,  "mePrepareStage (asm stw 0, 0(r31))");
        static_assert(offsetof(VehicleManager, meReleaseStage) == 4,  "meReleaseStage (asm stw 3, 4(r31))");
        static_assert(offsetof(VehicleManager, mRandom)        == 16, "mRandom (asm addi r11, r31, 0x10) -- 16-aligned, NOT +8");
        static_assert(sizeof(VehicleManager::mRandom) == 48, "CgsNumeric::Random is 44 bytes at align 16 -> sizeof 48");

        static_assert(sizeof(VehicleDriver)  == 224,  "VehicleDriver stride (asm: addi r25, r25, 0xE0)");
        // The three named bytes are at ABSOLUTE class offsets 224*idx + 123/124/125; with the array
        // correctly seated at +64 that is in-record 59/60/61 (inside VehicleDriver::mControls).
        // RE-NAMED 2026-08-03: the stand-in record retired, so these now pin REAL members.
        static_assert(offsetof(VehicleDriver, mControls) + offsetof(BrnAIDriverControls, mbBoost) == 59,
                      "mControls.mbBoost -- the boost-eligible byte (asm: 224*idx + 123)");
        static_assert(offsetof(VehicleDriver, mControls) + offsetof(BrnAIDriverControls, mbIsInvulnerableToVehicles) == 60,
                      "mControls.mbIsInvulnerableToVehicles (asm: 224*idx + 124)");
        static_assert(offsetof(VehicleDriver, mControls) + offsetof(BrnAIDriverControls, mbIsInvulnerableToWorld) == 61,
                      "mControls.mbIsInvulnerableToWorld (asm: 224*idx + 125)");
        static_assert(offsetof(VehicleManager, maRaceCarDrivers) + 224 * 1 + 59 == 224 * 1 + 123,
                      "the re-seat is byte-identical to the old model for every element");
        // THE RECORD IS GONE (2026-08-03, the fold wave). Ten `offsetof(RaceCarVehicleRecord,
        // ...) == <X360 in-record seat>` asserts used to stand here. They cannot survive the fold
        // and must not be faked: `maRaceCarVehicles` is now the real `RaceCarPhysics`, a HOST class
        // whose members sit at host offsets, so asserting a console seat on it would be simply
        // false. The seats moved to the two mounted console-arithmetic gates
        // (RaceCarPhysics_layout_check.cpp, VehiclePhysics_layout_check.cpp), whose chain closes on
        // this same 5216 -- see the fold-in note in BrnVehicleManager.h.
        //
        // What CAN still be asserted here is the part that is a claim about THIS class: that the
        // element type is the real one and that its host size is the number the drift term carries.
        // If either changes without the drift term changing, this fails.
        static_assert(sizeof(RaceCarPhysics) == 5216,
                      "host sizeof(RaceCarPhysics) == the console's 0x1460 stride (width-identical "
                      "since the 240-byte SimpleVehicleAttribs landed, 2026-08-09) -- the number "
                      "KU_HOST_DRIFT_AFTER_RACECAR_ARRAY (now 0) is derived from");
        // RE-STATED 2026-08-11 (create-drain wave), and RE-SEATED at the merge of the two waves.
        // The +128 the model-handle split costs is NOT part of the race-car array's term: the two
        // components are independently derived from two different sizeofs, so they get two
        // constants and TWO asserts, and neither can absorb an error in the other. Mirrors the
        // mounted gate's form (BrnVehicleManager_layout_check.cpp).
        static_assert(8 * (static_cast<std::ptrdiff_t>(sizeof(RaceCarPhysics)) - 5216)
                          == KU_HOST_DRIFT_AFTER_RACECAR_ARRAY,
                      "the race-car array's own term must BE 8 * (host sizeof RaceCarPhysics - the "
                      "console's 0x1460 stride) -- zero today, and this is what says so if the "
                      "class ever changes size again");
        static_assert(KU_HOST_DRIFT_AFTER_RACECAR_ARRAY
                          + 2 * 8 * (static_cast<std::ptrdiff_t>(sizeof(CgsResource::ResourceHandle)) - 8)
                          == KU_HOST_DRIFT_AFTER_MODEL_HANDLES,
                      "the handle term must BE the race-car term plus the two ResourceHandle arrays' "
                      "host/console width difference -- if the handle changes width and the constant "
                      "is not updated, every seat past +43744 moves and this line is what says so");
        static_assert(alignof(RaceCarPhysics) == 16 && (1856 % 16) == 0,
                      "element 0 keeps the asm-literal +1856 base and every element stays 16-aligned");
        static_assert(sizeof(RaceCarCrashData)     == 12,   "RaceCarCrashData stride (asm: 12)");

        static_assert(offsetof(VehicleManager, maRaceCarDrivers)         == 64,     "maRaceCarDrivers (asm addi r25, r31, 0x40) -- was WRONGLY seated at 0");
        static_assert(offsetof(VehicleManager, maRaceCarVehicles)        == 1856,   "maRaceCarVehicles (asm r29 - 0x140D)");
        // NO DRIFT TERM ON THIS ONE (corrected 2026-08-11). The +128 of
        // KU_HOST_DRIFT_AFTER_MODEL_HANDLES arises at +43616 -- the head of maRaceCarModelHandles,
        // which sits AFTER this array. Nothing before +43616 moves.
        static_assert(offsetof(VehicleManager, maRaceCarEntityIDs)       == 43584,  "maRaceCarEntityIDs (asm base 43584)");
        static_assert(offsetof(VehicleManager, maRaceCarModelHandles)    == 43616,  "maRaceCarModelHandles (asm 8 * 0x154C; ProcessCreateEvents/ProcessValidationEvents)");
        static_assert(offsetof(VehicleManager, maRaceCarGraphicsModelHandles)
                          == offsetof(VehicleManager, maRaceCarModelHandles)
                             + sizeof(VehicleManager::maRaceCarModelHandles),
                      "the two handle arrays abut (console 8 * 0x154C then 8 * 0x1554)");
        static_assert(offsetof(VehicleManager, maRaceCarHandlingBodyIDs) == 43744 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "maRaceCarHandlingBodyIDs (asm addi r26,r26,-0x5520)");
        static_assert(sizeof(VehicleManager::maRaceCarHandlingBodyIDs) == 64,
                      "RigidBodyId is 8 bytes -- the ctor's `std` + `addi r26, r26, 8`, and 43744 + 64 == 43808");
        static_assert(offsetof(VehicleManager, maRaceCarCrashes)         == 43808 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "maRaceCarCrashes (asm base 43808)");
        static_assert(offsetof(VehicleManager, maeRaceCarTypes)          == 44192 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "maeRaceCarTypes (asm base 44192; ctor seeds 3 == E_RACE_CAR_TYPE_INACTIVE)");
        static_assert(sizeof(CgsContainers::BitArray<8>)  == 8, "BitArray<8> single 64-bit field (8 bytes)");
        static_assert(sizeof(CgsContainers::BitArray<32>) == 8, "BitArray<32> single 64-bit field (8 bytes)");
        static_assert(offsetof(VehicleManager, mUsedRaceCars)            == 44224 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "mUsedRaceCars (asm +44224)");
        static_assert(offsetof(VehicleManager, mUsedRaceCarCrashesList)  == 44232 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "mUsedRaceCarCrashesList (asm +44232)");
        static_assert(offsetof(VehicleManager, mStuntOffencesManager)    == 44240 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES,  "mStuntOffencesManager (asm StuntOffencesManager::Construct(this + 44240))");
        static_assert(offsetof(VehicleManager, mRaceCarsAddedForCollision)             == 44712 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES, "mRaceCarsAddedForCollision (asm +44712)");
        static_assert(offsetof(VehicleManager, mNetworkCarsAddedForCollisionThisFrame) == 44720 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES, "mNetworkCarsAddedForCollisionThisFrame (asm +44720)");
        static_assert(offsetof(VehicleManager, mNetworkCarsRecievedFirstUpdate)        == 44728 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES, "mNetworkCarsRecievedFirstUpdate (asm +44728)");
        // RE-SEATED 2026-08-03: the old `maRaceCarEntityIdRemap` sibling at +148128 is really the
        // embedded traffic manager's maTrafficEntityIDs. Same byte, real owner -- and the sum below
        // is a STRONGER assert than the old one, because it also pins the manager's own head.
        static_assert(offsetof(VehicleManager, mPhysicalTrafficManager) == 44768 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES, "mPhysicalTrafficManager (asm PhysicalTrafficManager::Construct(this + 44768))");
        // AND IT HAD BEEN FAILING SINCE task #112.
        // This line used to read `== 148128 + KU_HOST_DRIFT_AFTER_RACECAR_ARRAY`, i.e. it applied
        // only the race-car array's drift to a seat that also sits behind maFullTrafficPhysics[20].
        // The TrafficPhysics de-fork shrank that array by 20 * (5168 - 4960) == 4160 bytes, so the
        // assert had been false -- by exactly 4160 -- from the moment that wave landed. NOTHING
        // CAUGHT IT: this TU was not in the build at the time, so the only compiler that would ever
        // have seen the line was a per-TU gate nobody ran on it. It surfaced the instant a wave
        // trial-mounted this file to measure its link closure. The lesson is general: a fold that re-measures
        // its own drift constants must also re-compile every UNMOUNTED TU that consumes them.
        //
        // The correction is derived, not typed: `20*sizeof(TrafficPhysics) - 103360` IS the array's
        // host-minus-console difference, so this stays true if that class is ever re-measured again.
        // The absolute value is independently asserted (== 99200) in BrnVehicleManager_layout_check.cpp,
        // which IS mounted -- that is the pair that makes this a gate rather than a restatement.
        static_assert(offsetof(VehicleManager, mPhysicalTrafficManager)
                          + offsetof(PhysicalTrafficManager, maTrafficEntityIDs)
                      == 148128 + KU_HOST_DRIFT_AFTER_MODEL_HANDLES
                               + (static_cast<std::ptrdiff_t>(20 * sizeof(TrafficPhysics)) - 103360),
                      "44768 + 103360 == 148128 -- the seat SetRaceCarCrashing's owner==2 branch loads");
        static_assert(offsetof(VehicleManager, mDiscardedContacts)       == 160672 + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER, "mDiscardedContacts (asm addi r29,r29,0x73A0)");
        static_assert(offsetof(VehicleManager, mDebugComponent)          == 161968 + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER, "mDebugComponent (asm VehicleManagerDebugComponent::Construct(this + 161968, this))");
        static_assert(offsetof(VehicleManager, maRaceCarDebugComponent)  == 163264 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "maRaceCarDebugComponent (asm addi r27,r27,0x7DC0; stride 0x400)");
        static_assert(offsetof(VehicleManager, mabRaceCarDebugComponentRegistered) == 171456 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS,
                      "163264 + 8*1024 == 171456, and +8 lands exactly on the asm-proven gate byte at 171464");
        // ---- the tuning bank, pinned member by member (2026-08-03). Every offset below is an
        //      asm seat from VehicleManager::Construct @0x8263B7C8, cross-checked against the PS3
        //      DecFIGS build at Δ=672. The padding runs between them are what these asserts test.
        static_assert(offsetof(VehicleManager, mbSlamsAndShuntsOn)       == 171464 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbSlamsAndShuntsOn (asm +171464)");
        static_assert(offsetof(VehicleManager, mbAllowSlamsAndShuntsEffectsForRivals) == 171465 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbAllowSlamsAndShuntsEffectsForRivals (asm +171465)");
        // The head and tail of the 44-float run, plus the closure that proves it has no gaps.
        static_assert(offsetof(VehicleManager, mfFrontRaySensorLength)   == 171468 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfFrontRaySensorLength (asm +171468)");
        static_assert(offsetof(VehicleManager, mfMaxSlamClosingXSpeed)   == 171536 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfMaxSlamClosingXSpeed (asm +171536)");
        static_assert(offsetof(VehicleManager, mfMinSecondsBetweenImpacts) == 171540 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfMinSecondsBetweenImpacts (asm +171540) -- was mis-typed s32 miAttackerToRecord");
        static_assert(offsetof(VehicleManager, mfTailgatingVunerabilityTime) == 171552 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfTailgatingVunerabilityTime (asm +171552; value recovered from the sibling build)");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownMaxAngle)  == 171564 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfTBoneTakedownMaxAngle (asm +171564)");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownSpeed)     == 171568 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfTBoneTakedownSpeed (asm +171568)");
        static_assert(offsetof(VehicleManager, mfMinShuntSpeed)          == 171580 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfMinShuntSpeed (asm +171580)");
        static_assert(offsetof(VehicleManager, mfFatalShuntSpeed)        == 171584 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfFatalShuntSpeed (asm +171584)");
        static_assert(offsetof(VehicleManager, mfMinTradingPaintSpeed)   == 171616 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfMinTradingPaintSpeed (asm +171616)");
        static_assert(offsetof(VehicleManager, mfFatalSlamSpeed)         == 171620 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfFatalSlamSpeed (asm +171620)");
        static_assert(offsetof(VehicleManager, mfMaxHeadToHeadAngle)     == 171628 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfMaxHeadToHeadAngle (asm +171628)");
        static_assert(offsetof(VehicleManager, mfMinHeadToHeadIndividualSpeed) == 171636 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfMinHeadToHeadIndividualSpeed (asm +171636)");
        static_assert(offsetof(VehicleManager, mfAngleForVerticleTakedown) == 171640 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfAngleForVerticleTakedown (asm +171640) -- last of the 44-float run");
        static_assert(offsetof(VehicleManager, maeImpactType) == 171644 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS,
                      "171468 + 44*4 == 171644: the 44-float tuning run closes exactly onto maeImpactType with no gaps");
        static_assert(offsetof(VehicleManager, mauImpactScore)  == 171676 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mauImpactScore (asm base 171676)");
        static_assert(offsetof(VehicleManager, mafNoImpactTimeSeconds)    == 171684 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mafNoImpactTimeSeconds (asm base 171684) -- was mis-typed s32[8]");
        static_assert(offsetof(VehicleManager, maiPhysicsSlamIndex) == 171716 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "maiPhysicsSlamIndex");
        static_assert(offsetof(VehicleManager, mPlayerWonImpact) == 171736 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mPlayerWonImpact (asm +171736)");
        // The two seats the committed header modelled as scalar grind thresholds are element 7 of
        // these two per-car arrays -- 171840 + 7*4 == 171868 and 171872 + 7*4 == 171900.
        static_assert(offsetof(VehicleManager, mafVulnerableTimeSeconds) == 171744 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mafVulnerableTimeSeconds");
        static_assert(offsetof(VehicleManager, mafPlayerGrindingOtherDurationSeconds) == 171840 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS,
                      "mafPlayerGrindingOtherDurationSeconds base; [7] == the old mfGrindingThresholdA seat 171868");
        static_assert(offsetof(VehicleManager, mafOtherGrindingPlayerDurationSeconds) == 171872 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS,
                      "mafOtherGrindingPlayerDurationSeconds base; [7] == the old mfGrindingThresholdB seat 171900");
        static_assert(offsetof(VehicleManager, mabRubbingThisUpdate) == 171952 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mabRubbingThisUpdate");
        static_assert(offsetof(VehicleManager, mPlayerAiDriver)          == 171968 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mPlayerAiDriver (asm VehicleDriver::Construct(this + 171968))");
        static_assert(offsetof(VehicleManager, mbPlayerAiDriverValid)    == 172192 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbPlayerAiDriverValid");
        static_assert(offsetof(VehicleManager, mfSteeringUpdateRemainder) == 172200 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfSteeringUpdateRemainder");
        static_assert(offsetof(VehicleManager, mePlayerActiveRaceCarIndex) == 172204 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mePlayerActiveRaceCarIndex (asm +172204)");
        static_assert(offsetof(VehicleManager, mfCrashingAICollisionCrashThresholdMPH) == 172208 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfCrashingAICollisionCrashThresholdMPH (asm +172208)");
        static_assert(offsetof(VehicleManager, mfVerticalTakedownAngleDeg) == 172228 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfVerticalTakedownAngleDeg (asm +172228; last of the six-float run)");
        static_assert(offsetof(VehicleManager, mCameraMatrix)            == 172240 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mCameraMatrix (asm addi r11,r11,-0x5F30; 4 x stvx128)");
        static_assert(offsetof(VehicleManager, mCameraMatrix) % 16 == 0, "Matrix44Affine must land 16-aligned with no compiler-inserted padding");
        static_assert(offsetof(VehicleManager, mbImpactTime)            == 172304 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbImpactTime (asm +172304)");
        static_assert(offsetof(VehicleManager, mbStopPlayerCrashing)    == 172306 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbStopPlayerCrashing (asm +172306)");
        static_assert(offsetof(VehicleManager, mbStopAICrashing) == 172307 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbStopAICrashing (asm +172307)");
        static_assert(offsetof(VehicleManager, DEBUG_mbHornTakedownEnabled)    == 172311 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "DEBUG_mbHornTakedownEnabled (asm +172311)");
        static_assert(offsetof(VehicleManager, mbTrafficCheckingAllowed) == 172313 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbTrafficCheckingAllowed (asm +172313; the one bool Construct seeds TRUE)");
        static_assert(offsetof(VehicleManager, mbIsOnlineGameMode) == 172315 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbIsOnlineGameMode (asm +172315)");
        static_assert(offsetof(VehicleManager, mbPlayerCarInJunkYard) == 172319 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbPlayerCarInJunkYard (asm +172319)");
        static_assert(offsetof(VehicleManager, mfPlayerStatStrength)  == 172320 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfPlayerStatStrength (asm +172320; stfsx => f32)");
        static_assert(offsetof(VehicleManager, miCarSpeed)            == 172328 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "miCarSpeed (asm +172328; stwx => s32)");
        static_assert(offsetof(VehicleManager, meCarType)             == 172344 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "meCarType (asm +172344; stwx, seeded 3)");
        static_assert(offsetof(VehicleManager, miPlayerBoost)         == 172360 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "miPlayerBoost");
        static_assert(offsetof(VehicleManager, meCurrentGameModeType) == 172380 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "meCurrentGameModeType (asm +172380; seeded -1)");
        static_assert(offsetof(VehicleManager, mfCarStatStrengthSlamMax) == 172384 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfCarStatStrengthSlamMax (asm +172384)");
        static_assert(offsetof(VehicleManager, mfCarrStatStrengthBeingShuntedMin) == 172412 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mfCarrStatStrengthBeingShuntedMin (asm +172412; last of the eight)");
        static_assert(offsetof(VehicleManager, muCachedCarASlot)      == 172416 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "muCachedCarASlot (asm +172416)");
        static_assert(offsetof(VehicleManager, mbCachedCarCarPredictionResult) == 172424 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbCachedCarCarPredictionResult (asm +172424)");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) == 172432 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mCachedCarCarPredictionNormal (asm stvx128 v0,r31,r9 with r9 == 172432)");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) % 16 == 0, "the prediction normal is loaded/stored with lvx128/stvx128 -- it must be 16-aligned");
        static_assert(offsetof(VehicleManager, meStationaryPlayerWheelAngle) == 172448 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "meStationaryPlayerWheelAngle (asm +172448; seeded 2)");
        static_assert(offsetof(VehicleManager, mbCrashRaceCarWhenFatal) == 172452 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "mbCrashRaceCarWhenFatal (asm +172452; seeded true)");
        static_assert(offsetof(VehicleManager, meShowtimeBehaviour)   == 172456 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS, "meShowtimeBehaviour (asm +172456; seeded 2)");
        static_assert(offsetof(VehicleManager, miRaceCarWorldContactValidationPM) == 172460 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS,
                      "miRaceCarWorldContactValidationPM (asm +172460; named by the console's own assert at BrnVehicleManager.cpp:778)");
        // THESE FOUR WERE STALE, AND NOT BECAUSE OF THIS WAVE (corrected 2026-08-11). They
        // carried KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS for seats that sit PAST the 2026-08-06
        // contact-generation carve, so each was 76 bytes short -- and nothing said so for waves,
        // because this whole TU was UNMOUNTED then (that is exactly the hole
        // BrnVehicleManager_layout_check.cpp was created to close; this TU is mounted today and its
        // asserts compile). Conformed to the mounted gate's expressions, verbatim.
        static_assert(offsetof(VehicleManager, miNumTrafficSphereWorldTests)
                          == 172580 + KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS + 68,
                      "console +172580; +68 = the growth ACCUMULATED BY THIS SEAT (the block's full "
                      "+76 lands only after the tail's own 4-byte alignment pad)");
        static_assert(offsetof(VehicleManager, mpTractionLineStreamProducer) > offsetof(VehicleManager, miNumTrafficSphereWorldTests), "renamed at the 2026-08-06 carve (console +172584 pointer; host seat via the mounted gate)");
        static_assert(offsetof(VehicleManager, mStuckInCollisionTestCacheSphere) == 172592 + KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK, "mStuckInCollisionTestCacheSphere (asm stvx128 v127,r31,r11 with r11 == 172592)");
        static_assert(offsetof(VehicleManager, mbPlayerCarStuckInCollision) == 172608 + KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK,
                      "172592 + 16 == 172608: the Sphere/bool pair (DWARF :1087/:1088) closes to the byte");
        static_assert(offsetof(VehicleManager, muTakedownEventsThisFrame) == 172612 + KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK, "muTakedownEventsThisFrame (asm +172612)");
    }
}
}

// ============================================================================
// FOLDED FROM BrnVehicleManager_wG_ReadSurface.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_wG_ReadSurface.cpp
//
// VehicleManager::ReadSurfaceProperties(u64) -- fills the four global per-surface banks
// (KAVF_SURFACE_GRIP / _ROUGHNESS / _LINEAR_DRAG and the KAB_SURFACE_IS_WATER flags) from the
// AttribSys `surfacelist` collection. Until it has run, gbReadSurfaceProperties is false and every
// VehiclePhysics road query reads the statically-zeroed bank.
//
// The vault must be built by the current attribsys-vault converter
// (build_game_data.py --only "SURFACELIST.BIN" --force): a stale SURFACELIST.BIN dies inside
// Attrib::Collection::GetData and then AVs.
// =================================================================================================


namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // The epsilon the console splats and compares the sample quad against, lane by lane.
    const f32 KF_SURFACE_LIST_MIN_MAGNITUDE = 1.1920928955078125e-07f;  // FLT_EPSILON

    // The element index of the surface the corruption check samples.
    const u32 KU_SAMPLE_SURFACE_INDEX = 1u;

    // The null-element fallback size for a RefSpec array slot (the console passes the byte
    // literal 0x18; sizeof is the host-correct spelling of the same record).
    const u32 KU_REF_SPEC_DATA_AREA_BYTES = static_cast<u32>(sizeof(Attrib::RefSpec));
}

// The surface-property bank loader. Called from WorldModule::Prepare's world-entity stage with
// the world's surface-list collection key.
void VehicleManager::ReadSurfaceProperties(u64 luSurfaceListKey)
{
    Attrib::Gen::surfacelist lSurfaceList;
    lSurfaceList.ChangeWithDefault(luSurfaceListKey);

    // [FLAG PC bring-up guard -- NOT in the console body.] The console's database is always up
    // by the time this runs; on the PC a failed resolve (database not initialised, or the class
    // / collection absent from a stale vault) leaves the instance with no collection, and the
    // element walk below then dies in Attrib::Collection::GetData and AVs -- taking the boot
    // with it. Bail loudly instead and leave gbReadSurfaceProperties FALSE, so the consumers'
    // own "before properties have been loaded" asserts are what names the failure.
    // DELETE-WHEN the attrib database bring-up guarantees the surfacelist collection resolves at
    // WorldModule::Prepare time.
    if (!lSurfaceList.IsValid())
    {
        static bool sbReported = false;
        if (!sbReported)
        {
            sbReported = true;
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[surface-bank] ReadSurfaceProperties: surfacelist did not resolve "
                       "(no collection bound by ChangeWithDefault) -- the grip/roughness/drag "
                       "bank stays at its static zeros [FLAG PC bring-up guard]\n";
            }
        }
        return;
    }

    // ---- the corruption check on element 1 -----------------------------------------------
    {
        void* lpSampleRefData = lSurfaceList.Surfaces(KU_SAMPLE_SURFACE_INDEX);
        if (!lpSampleRefData)
            lpSampleRefData = Attrib::DefaultDataArea(KU_REF_SPEC_DATA_AREA_BYTES);

        Attrib::RefSpec* lpSampleRef = static_cast<Attrib::RefSpec*>(lpSampleRefData);
        Attrib::Gen::surface lSampleSurface(
            const_cast<Attrib::Collection*>(lpSampleRef->GetCollection()), 0);

        // A componentwise fabs compared against a splatted epsilon; the branch is taken on
        // "NONE of the four lanes greater", so the assert condition is "at least one lane
        // exceeds epsilon".
        const f32* lpfSampleQuad = static_cast<const f32*>(lSampleSurface.GetAttributeData());
        const bool lbSurfaceListLooksSane =
               (std::fabs(lpfSampleQuad[0]) > KF_SURFACE_LIST_MIN_MAGNITUDE)
            || (std::fabs(lpfSampleQuad[1]) > KF_SURFACE_LIST_MIN_MAGNITUDE)
            || (std::fabs(lpfSampleQuad[2]) > KF_SURFACE_LIST_MIN_MAGNITUDE)
            || (std::fabs(lpfSampleQuad[3]) > KF_SURFACE_LIST_MIN_MAGNITUDE);
        CGS_ASSERT(lbSurfaceListLooksSane, "Surface list appears to be corrupt");
    }

    // ---- the count, then the per-surface walk ---------------------------------------------
    KI_NUM_USED_SURFACES = lSurfaceList.Num_Surfaces();

    // [FLAG PC host guard -- NOT in the console body.] The console loops to Num_Surfaces with no
    // upper bound because its four banks and the shipped list were sized together. On the host
    // the banks are KI_MAX_NUM_SURFACES long and a longer list would walk off the end of three
    // 16-byte-strided globals. Same shape of guard the reset pump already applies to
    // KAB_SURFACE_IS_WATER at its own read site.
    CGS_ASSERT(KI_NUM_USED_SURFACES <= KI_MAX_NUM_SURFACES,
               "KI_NUM_USED_SURFACES <= KI_MAX_NUM_SURFACES");
    const s32 liSurfaceCount = (KI_NUM_USED_SURFACES < KI_MAX_NUM_SURFACES)
                                   ? KI_NUM_USED_SURFACES
                                   : KI_MAX_NUM_SURFACES;

    for (s32 liSurface = 0; liSurface < liSurfaceCount; ++liSurface)
    {
        void* lpSurfaceRefData = lSurfaceList.Surfaces(static_cast<u32>(liSurface));
        if (!lpSurfaceRefData)
            lpSurfaceRefData = Attrib::DefaultDataArea(KU_REF_SPEC_DATA_AREA_BYTES);

        Attrib::Gen::surface lSurface(
            *static_cast<Attrib::RefSpec*>(lpSurfaceRefData), 0);
        // Construction order is gameplay-then-physics, as emitted; the refs are the surface
        // layout's +0x58 and +0x40 RefSpecs.
        Attrib::Gen::gameplaysurface lGameplaySurface(lSurface.GameplaySurface(), 0);
        Attrib::Gen::physicssurface lPhysicsSurface(lSurface.PhysicsSurface(), 0);

        // +0x00 roughness, +0x04 linear drag, +0x08 grip. Each scalar is splatted into all
        // four lanes of its table slot.
        const f32 lfRoughness  = lPhysicsSurface.Roughness();
        const f32 lfLinearDrag = lPhysicsSurface.LinearDrag();
        const f32 lfGrip       = lPhysicsSurface.Grip();

        KAVF_SURFACE_ROUGHNESS[liSurface]   = VecFloat{lfRoughness, lfRoughness, lfRoughness, lfRoughness};
        KAVF_SURFACE_GRIP[liSurface]        = VecFloat{lfGrip, lfGrip, lfGrip, lfGrip};
        KAVF_SURFACE_LINEAR_DRAG[liSurface] = VecFloat{lfLinearDrag, lfLinearDrag, lfLinearDrag, lfLinearDrag};
        KAB_SURFACE_IS_WATER[liSurface]     = lGameplaySurface.IsWater();
    }

    // [surface-bank] boot witness -- diagnostic only, DELETE-WHEN the bank has a test that
    // asserts a non-zero grip.
    {
        static bool sbBankReported = false;
        if (!sbBankReported)
        {
            sbBankReported = true;
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[surface-bank] ReadSurfaceProperties: numSurfaces="
                    << liSurfaceCount << " grip[0..3]=" << KAVF_SURFACE_GRIP[0].x << ","
                    << KAVF_SURFACE_GRIP[1].x << "," << KAVF_SURFACE_GRIP[2].x << ","
                    << KAVF_SURFACE_GRIP[3].x << " rough[1]=" << KAVF_SURFACE_ROUGHNESS[1].x
                    << " drag[1]=" << KAVF_SURFACE_LINEAR_DRAG[1].x
                    << " water[1]=" << (KAB_SURFACE_IS_WATER[1] ? 1 : 0) << "\n";
            }
        }
    }

    gbReadSurfaceProperties = true;
}

}
}
