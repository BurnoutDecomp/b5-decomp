// ARTIST compiled this unit through the nested CgsBitArray.h include (its BitArray asserts name
// containers\./CgsBitArray.h, e.g. VehicleManager::RecordNetworkRaceCarsAddedForCollision 0x825C7EA8): see CgsBitArray.h.
#define CGS_ARTIST_CGSBITARRAY_NESTED_INCLUDE
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"  // RaceCarPhysics::SetCrashing (declare-only callee)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                    // CgsContainers::BitArray<N> (crash-data free-list + taken-down bitset)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"     // CgsDev::PerfMonCpu::AddMonitor -- Construct's thirty monitors
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT -- the two asserts Construct fires
#include "GameShared/GameClasses/Core/CgsCoreConstants.h"                     // CgsCore::KF_MPH_TO_MPS (flt_82F31928)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                  // KU_INVALID_ENTITY_ID (dword_82F2A3A4)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                      // K_INVALID_RIGID_BODY_ID (qword_82F2A3A8)
#include "rw/math/vpu/vector3_operation.h"                                    // vpu::Dot (T-bone side-speed gates, wave B3b)
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Physics/VehicleManager/BrnVehicleManager.cpp"

#include <cmath>    // std::fabs
#include "SDKs/XboxMath/XMVectorACos.h"   // XboxMath::XMVectorACos (X360 0x821F0980), the angle between the cars
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
            return VecFloat(lfValue);
        }

        // The console's `vmsum3fp128` -- a three-lane dot product left splatted across the
        // register. Every call site here consumes one lane, so it reduces to the scalar sum.
        inline f32 Dot3_VM(const Vector3& lrA, const Vector3& lrB)
        {
            return lrA.GetX() * lrB.GetX() + lrA.GetY() * lrB.GetY() + lrA.GetZ() * lrB.GetZ();
        }

        // The console's length idiom: dot3 the vector with itself, take an estimated reciprocal
        // square root, refine it with two Newton steps, multiply back -- and vsel a hard zero over
        // the result when the squared length compared equal to zero (the rsqrt of 0 is infinite).
        // The two Newton steps only chase the estimate's error, so the host takes the exact root.
        inline f32 Length3_VM(const Vector3& lrV)
        {
            const f32 lfLengthSq = lrV.GetX() * lrV.GetX() + lrV.GetY() * lrV.GetY() + lrV.GetZ() * lrV.GetZ();
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
            const f32 lfAlongUp = lrContactNormal.GetX() * lrVictimUp.GetX()
                                + lrContactNormal.GetY() * lrVictimUp.GetY()
                                + lrContactNormal.GetZ() * lrVictimUp.GetZ();
            const f32 lfFlatX = lrContactNormal.GetX() - lrVictimUp.GetX() * lfAlongUp;
            const f32 lfFlatY = lrContactNormal.GetY() - lrVictimUp.GetY() * lfAlongUp;
            const f32 lfFlatZ = lrContactNormal.GetZ() - lrVictimUp.GetZ() * lfAlongUp;
            return std::fabs(lfFlatX * lrClosingVelocity.GetX()
                           + lfFlatY * lrClosingVelocity.GetY()
                           + lfFlatZ * lrClosingVelocity.GetZ());
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

    // Game event 31: three words, as written at 0x82643B34..0x82643B54.
    struct ImpactIoEventRecord : public CgsModule::Event
    {
        EImpactType meImpactType;
        EActiveRaceCarIndex meAggressorIndex;
        EActiveRaceCarIndex meVictimIndex;
    };
    CGS_STATIC_ASSERT((sizeof(ImpactIoEventRecord) == 12), "Impact game-event payload");

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
        // 0x82643524 vmaxfp / 0x82643528 vminfp128 (the clamp above, a NaN kept), then 0x8264352C
        // bl XMVectorACos (crash parity FX-GATE: the console's own arc-cosine, not std::acos).
        lInfo.mfAngleBetweenCars = std::fabs(XboxMath::XMVectorACos(lfAlignment));
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
                ImpactIoEventRecord lEvent;
                lEvent.meImpactType = static_cast<EImpactType>(liGrindingType);
                lEvent.meAggressorIndex = static_cast<EActiveRaceCarIndex>(-1);
                lEvent.meVictimIndex = static_cast<EActiveRaceCarIndex>(-1);
                lpVehicleOutputInterface->GetGameEventQueue()->AddEventSafe(&lEvent, 31, sizeof(lEvent));
            }
        }
        CheckForAllTypesOfImpacts(&lInfo);
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
                    ImpactIoEventRecord lEvent;
                    lEvent.meImpactType = lInfo.meImpactType;
                    lEvent.meAggressorIndex = lInfo.meAggressorActiveRaceCarIndex;
                    lEvent.meVictimIndex = lInfo.meVictimActiveRaceCarIndex;
                    ++muTakedownEventsThisFrame;
                    lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(&lEvent, 31, sizeof(lEvent));
                }
            }
            GenerateContactSituation(&lInfo);

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
                vpu::Subtract(lContact.mPointOnA, lrCarA.mTransform.Pos())) > lrCarA.mDeformableAABB.mMin.GetZ() * 0.5f
            && lbPlayerInvolved)
        {
            lrCarB.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.GetZ() = 0.0f;
            lrCarB.mi8LastContactedRaceCar = static_cast<s8>(liIndexA);
        }
        if (!lrCarB.IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liIndexA))
            && lfPointBAlongCar > lrCarB.mDeformableAABB.mMin.GetZ() * 0.5f && lbPlayerInvolved)
        {
            lrCarA.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.GetZ() = 0.0f;
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


        // Energy gate: ignore contacts whose combined closing speed is below the threshold.
        const f32 lfSpeedSum =
            std::fabs(lpInfo->mfRaceCarBSpeed) + std::fabs(lpInfo->mfRaceCarASpeed);
        if (lfSpeedSum < KF_MIN_IMPACT_SPEED_SUM)
        {
            return;
        }


        // Highest priority: a player shunting an AI into a third AI, and re-hits on a car that is
        // already crashing -- these run even if a car is mid-crash.
        if (CheckForPlayerSlammingAIIntoAI(lpInfo))
        {
            return;
        }
        if (CheckForHittingAlreadyCrashingCar(lpInfo))
        {
            return;
        }

        // The geometric classifiers only apply to a fresh impact: a car already crashing cannot be
        // freshly taken down.
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
        {
            return;
        }

        if (CheckForVerticalTakedown(lpInfo))
        {
            return;
        }
        if (CheckForTBoneTakedown(lpInfo))
        {
            return;
        }
        if (CheckForHeadToHead(lpInfo))
        {
            return;
        }
        if (CheckForShuntAndNudge(lpInfo))
        {
            return;
        }
        if (CheckForSlamAndTradingPaint(lpInfo))
        {
            return;
        }
        CheckForStationaryTargetTakedown(lpInfo);
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
                    lrVictimRecord.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.GetX(),
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
                lvDelta.GetX() * lvPlayerForward.GetX() + lvDelta.GetY() * lvPlayerForward.GetY() + lvDelta.GetZ() * lvPlayerForward.GetZ();
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
                    lrVictimRecord.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.GetX(),
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
    // The speed-unit scale every tuning threshold multiplies by is CgsCore::KF_MPH_TO_MPS itself
    // (flt_82F31928, read from memory at each use: 0x8261A500, 0x8263D220, 0x8263D568, ...).
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
        EntityId lId = { (luEntityIndex << 10) | 0x1000000u };
        return lId;
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
        if (std::fabs(std::fabs(lpInfo->mfAngleBetweenCars) - lfPiOver2)
            < mfTBoneTakedownMaxAngle * lfDegToRad)
        {
            const f32 KF_SLAB_INSET     = 1.0f;   // 0x82FB8280 <- splat(flt_82001C98)
            const f32 lfMinLateralSpeed = mfTBoneTakedownSpeed * CgsCore::KF_MPH_TO_MPS;   // flt_82F31928

            RaceCarPhysics* const lpCarA =
                &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexA)];
            RaceCarPhysics* const lpCarB =
                &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexB)];

            // Arm 1 -- A rams B's side: A's last linear velocity across B's right axis clears the
            // threshold AND A's position sits between B's (inset) front/rear planes.
            if (std::fabs(vpu::Dot(lpCarB->mTransform.xAxis, lpCarA->mLastLinearVelocity))
                    > lfMinLateralSpeed)
            {
                const Vector3 lvBFront = lpCarB->mTransform.zAxis * lpCarB->GetDeformableAABB().mMax.GetZ()
                                       + lpCarB->mTransform.wAxis - lpCarB->mTransform.zAxis * KF_SLAB_INSET;
                const Vector3 lvBRear  = lpCarB->mTransform.zAxis * lpCarB->GetDeformableAABB().mMin.GetZ()
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
                const Vector3 lvAFront = lpCarA->mTransform.zAxis * lpCarA->GetDeformableAABB().mMax.GetZ()
                                       + lpCarA->mTransform.wAxis - lpCarA->mTransform.zAxis * KF_SLAB_INSET;
                const Vector3 lvARear  = lpCarA->mTransform.zAxis * lpCarA->GetDeformableAABB().mMin.GetZ()
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
        const f32 lfMinimumSpeed = mfMinHeadToHeadIndividualSpeed * CgsCore::KF_MPH_TO_MPS;
        if (!(lpInfo->mfRaceCarASpeed > lfMinimumSpeed) && !(lpInfo->mfRaceCarBSpeed > lfMinimumSpeed))
            return false;
        const f32 lfMomentumA = lpInfo->mpRaceCarA->GetMass() * lpInfo->mfRaceCarASpeed;
        const f32 lfMomentumB = lpInfo->mpRaceCarB->GetMass() * lpInfo->mfRaceCarBSpeed;
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
        const f32 ldx = lvPosA.GetX() - lvPosB.GetX();
        const f32 ldy = lvPosA.GetY() - lvPosB.GetY();
        const f32 ldz = lvPosA.GetZ() - lvPosB.GetZ();
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
        // 0x8263DA04 `fcmpu cr6, speedA, speedB` ; 0x8263DA0C `ble cr6, 0x8263DA40` (40990034 = bc 4,25):
        // the A-is-the-victim arm (0x8263DA40) is the TAKEN target, so an unordered (NaN) speed takes
        // it too; only an ordered speedA ABOVE speedB falls through to the B-is-the-victim arm.
        // (`speedA <= speedB` sent a NaN speed to the B arm.)
        if (!(lfSpeedA > lfSpeedB))
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
        if (lpInfo->mfClosingSpeed > mfFatalShuntSpeed * CgsCore::KF_MPH_TO_MPS)
        {
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true;
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true;
            return true;
        }
        lpInfo->meImpactType = lpInfo->mfClosingSpeed > mfMinShuntSpeed * CgsCore::KF_MPH_TO_MPS
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
        // 0x8261A0F8 `fcmpu cr6, closing, min` ; 0x8261A0FC `ble cr6, 0x8261A37C` (40990280 = bc 4,25):
        // the branch to `li r3, 0` is TAKEN whenever GT is clear -- an ordered closing at or under the
        // minimum AND an unordered (NaN) one -- so only an ordered closing ABOVE it falls through to the
        // fatal test. (`closing <= min` let a NaN closing through to the arms.)
        if (!(lpInfo->mfClosingSpeed > mfMinTradingPaintSpeed * CgsCore::KF_MPH_TO_MPS))
            return false;
        if (lpInfo->mfClosingSpeed > mfFatalSlamSpeed * CgsCore::KF_MPH_TO_MPS)
        {
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true;
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true;
            return true;
        }
        // flt_82F2A218: PLAYER, AI, NETWORK, INACTIVE steering thresholds, read from ARTIST.
        static const f32 KAF_SLAM_STEERING_THRESHOLD[4] = { 0.1f, 0.3f, 0.1f, 0.1f };
        const s32 liA = lpInfo->meActiveRaceCarIndexA;
        const s32 liB = lpInfo->meActiveRaceCarIndexB;
        // The A arm is left for the B test (0x8261A248) only by 0x8261A154 `blt cr6, steerA, steerB`
        // (419800F4 = bc 12,24 -- NOT taken on unordered) or 0x8261A15C `ble cr6, steerA, 0.0` (taken
        // on unordered). So a NaN steerB with steerA > 0 STAYS in the A arm (`steerA >= steerB` sent it
        // to the B test), while a NaN steerA still leaves it.
        if (!(lfSteerA < lfSteerB) && lfSteerA > 0.0f
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
            return false;
        }

        RaceCarPhysics* const lpVehB = &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexB)];
        RaceCarPhysics* const lpVehA = &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexA)];


        bool lbFired = false;
        EntityId lVictimId = { 0 };
        EntityId lAggressorId = { 0 };

        // CheckForVerticalTakedownSituation is recovered
        // (@0x825C56D8 -- victim car + CONTACT POINT, the 80%-footprint test) and the two gates
        // this caller wraps around it are decoded from @0x8263D7CC/@0x8263D80C: the AGGRESSOR
        // must be really airborne (its air-time lane +0x1060.z > 0.2 == splat @0x82FB82A0,
        // exactly RaceCarPhysics::IsReallyInAir) and the VICTIM's air-time lane must be ZERO
        // (grounded). Candidate 1 tests A falling onto B (contact point on B); candidate 2, run
        // unconditionally after it, tests B falling onto A and OVERWRITES the pair when it fires.
        if (CheckForVerticalTakedownSituation(lpVehB, lpInfo->mpContact->mPointOnB)
            && lpVehA->IsReallyInAir()
            && lpVehB->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.GetZ() == 0.0f)
        {
            lbFired = true;
            lVictimId    = lpInfo->mRaceCarBEntityID; // asm r25 = *(mpContact+4) = idB -> victim (r4)
            lAggressorId = lpInfo->mRaceCarAEntityID; // asm r5  = *(mpContact+0) = idA -> aggressor
        }

        if (CheckForVerticalTakedownSituation(lpVehA, lpInfo->mpContact->mPointOnA)
            && lpVehB->IsReallyInAir()
            && lpVehA->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.GetZ() == 0.0f)
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
            && lrCarA.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.GetZ() < 0.25f;
        const bool lbPlayerShovedB = lrCarB.mi8LastAttackersRaceCarIndex == mePlayerActiveRaceCarIndex
            && lrCarB.mi8LastContactedRaceCar == static_cast<s8>(mePlayerActiveRaceCarIndex)
            && lrCarB.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.GetZ() < 0.25f;
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
            && (lrCarA.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.GetY() > 1.0f || lpInfo->mbRaceCarAIsNetworkCar);
        const bool lbBIsObstacle = lpInfo->mbRaceCarBIsCrashing
            && (lrCarB.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.GetY() > 1.0f || lpInfo->mbRaceCarBIsNetworkCar);
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
            && lrObstacle.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.GetY() < 0.5f
            && lrObstacle.mi8LastContactedRaceCar == static_cast<s8>(mePlayerActiveRaceCarIndex)
            && lrObstacle.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.GetZ() < 1.0f)
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
        CGS_STATIC_ASSERT((offsetof(VehicleManager, mePrepareStage) == 0),  "mePrepareStage (asm stw 0, 0(r31))");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, meReleaseStage) == 4),  "meReleaseStage (asm stw 3, 4(r31))");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, mRandom)        == 16), "mRandom (asm addi r11, r31, 0x10) -- 16-aligned, NOT +8");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->mRandom) == 48), "CgsNumeric::Random is 44 bytes at align 16 -> sizeof 48");

        CGS_STATIC_ASSERT((sizeof(VehicleDriver)  == 224),  "VehicleDriver stride (asm: addi r25, r25, 0xE0)");
        // The three named bytes are at ABSOLUTE class offsets 224*idx + 123/124/125; with the array
        // correctly seated at +64 that is in-record 59/60/61 (inside VehicleDriver::mControls).
        // RE-NAMED 2026-08-03: the stand-in record retired, so these now pin REAL members.
        CGS_STATIC_ASSERT((offsetof(VehicleDriver, mControls) + offsetof(BrnAIDriverControls, mbBoost) == 59),
                      "mControls.mbBoost -- the boost-eligible byte (asm: 224*idx + 123)");
        CGS_STATIC_ASSERT((offsetof(VehicleDriver, mControls) + offsetof(BrnAIDriverControls, mbIsInvulnerableToVehicles) == 60),
                      "mControls.mbIsInvulnerableToVehicles (asm: 224*idx + 124)");
        CGS_STATIC_ASSERT((offsetof(VehicleDriver, mControls) + offsetof(BrnAIDriverControls, mbIsInvulnerableToWorld) == 61),
                      "mControls.mbIsInvulnerableToWorld (asm: 224*idx + 125)");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarDrivers) + 224 * 1 + 59 == 224 * 1 + 123),
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
        // RE-STATED 2026-08-11 (create-drain wave), and RE-SEATED at the merge of the two waves.
        // The +128 the model-handle split costs is NOT part of the race-car array's term: the two
        // components are independently derived from two different sizeofs, so they get two
        // constants and TWO asserts, and neither can absorb an error in the other. Mirrors the
        // mounted gate's form (BrnVehicleManager_layout_check.cpp).
        CGS_STATIC_ASSERT((__alignof(RaceCarPhysics) == 16 && (1856 % 16) == 0),
                      "element 0 keeps the asm-literal +1856 base and every element stays 16-aligned");
        CGS_STATIC_ASSERT((sizeof(RaceCarCrashData)     == 12),   "RaceCarCrashData stride (asm: 12)");

        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarDrivers)         == 64),     "maRaceCarDrivers (asm addi r25, r31, 0x40) -- was WRONGLY seated at 0");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarVehicles)        == 1856),   "maRaceCarVehicles (asm r29 - 0x140D)");
        // NO DRIFT TERM ON THIS ONE (corrected 2026-08-11). The +128 of
        // KU_HOST_DRIFT_AFTER_MODEL_HANDLES arises at +43616 -- the head of maRaceCarModelHandles,
        // which sits AFTER this array. Nothing before +43616 moves.
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarEntityIDs)       == 43584),  "maRaceCarEntityIDs (asm base 43584)");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarModelHandles)    == 43616),  "maRaceCarModelHandles (asm 8 * 0x154C; ProcessCreateEvents/ProcessValidationEvents)");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, maRaceCarGraphicsModelHandles)
                          == offsetof(VehicleManager, maRaceCarModelHandles)
                             + sizeof(((VehicleManager*)0)->maRaceCarModelHandles)),
                      "the two handle arrays abut (console 8 * 0x154C then 8 * 0x1554)");
        CGS_STATIC_ASSERT((sizeof(((VehicleManager*)0)->maRaceCarHandlingBodyIDs) == 64),
                      "RigidBodyId is 8 bytes -- the ctor's `std` + `addi r26, r26, 8`, and 43744 + 64 == 43808");
        // RE-SEATED 2026-08-03: the old `maRaceCarEntityIdRemap` sibling at +148128 is really the
        // embedded traffic manager's maTrafficEntityIDs. Same byte, real owner -- and the sum below
        // is a STRONGER assert than the old one, because it also pins the manager's own head.
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
        // ---- the tuning bank, pinned member by member (2026-08-03). Every offset below is an
        //      asm seat from VehicleManager::Construct @0x8263B7C8, cross-checked against the PS3
        //      DecFIGS build at Δ=672. The padding runs between them are what these asserts test.
        // The head and tail of the 44-float run, plus the closure that proves it has no gaps.
        // The two seats the committed header modelled as scalar grind thresholds are element 7 of
        // these two per-car arrays -- 171840 + 7*4 == 171868 and 171872 + 7*4 == 171900.
        CGS_STATIC_ASSERT((offsetof(VehicleManager, mCameraMatrix) % 16 == 0), "Matrix44Affine must land 16-aligned with no compiler-inserted padding");
        CGS_STATIC_ASSERT((offsetof(VehicleManager, mCachedCarCarPredictionNormal) % 16 == 0), "the prediction normal is loaded/stored with lvx128/stvx128 -- it must be 16-aligned");
        // THESE FOUR WERE STALE, AND NOT BECAUSE OF THIS WAVE (corrected 2026-08-11). They
        // carried KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS for seats that sit PAST the 2026-08-06
        // contact-generation carve, so each was 76 bytes short -- and nothing said so for waves,
        // because this whole TU was UNMOUNTED then (that is exactly the hole
        // BrnVehicleManager_layout_check.cpp was created to close; this TU is mounted today and its
        // asserts compile). Conformed to the mounted gate's expressions, verbatim.
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

        {
            const VecFloat lAggregateValue(lfRoughness);
            KAVF_SURFACE_ROUGHNESS[liSurface] = lAggregateValue;
        }
        {
            const VecFloat lAggregateValue(lfGrip);
            KAVF_SURFACE_GRIP[liSurface] = lAggregateValue;
        }
        {
            const VecFloat lAggregateValue(lfLinearDrag);
            KAVF_SURFACE_LINEAR_DRAG[liSurface] = lAggregateValue;
        }
        KAB_SURFACE_IS_WATER[liSurface]     = lGameplaySurface.IsWater();
    }


    gbReadSurfaceProperties = true;
}

}
}

// FOLDED FROM BrnVehicleManager_Construct.cpp
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerPerfMonHandles.h" // the 13 handles hoisted to external linkage (UpdateVehiclePhysics slice reads them)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"     // CgsDev::PerfMonCpu::AddMonitor -- Construct's thirty monitors
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT -- the assert Construct fires in the eight-car loop
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                  // K_INVALID_ENTITY_ID (dword_82F2A3A4)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                      // K_INVALID_RIGID_BODY_ID (qword_82F2A3A8)

// ==================================================================================================
// BrnPhysics::Vehicle::VehicleManager::Construct -- SPLIT OUT of BrnVehicleManager.cpp.
// BUILD-MECHANICS SPLIT ONLY: the body below, its recipe comment and the
// twenty-nine file-scope perfmon handles it owns were MOVED verbatim, not retyped or re-derived.
// Same precedent as RaceCarPhysics_Construct.cpp / BrnSimpleVehiclePhysics_Construct.cpp
// / TrafficPhysics_Construct.cpp. Its only caller is BrnPhysics::PhysicsModule::Construct.
//
// Construct's own closure is SIX functions, and every one of them is mounted:
//     CgsDev::PerfMonCpu::AddMonitor            CgsPerfMonCpu.cpp
//     VehicleManagerDebugComponent::Construct   BrnVehicleManagerDebugComponent.cpp
//     VehicleDriver::Construct                  BrnVehicleDriver.cpp
//     VehiclePhysics::Construct                 VehiclePhysics.cpp  (via RaceCarPhysics_Construct.cpp)
//     PhysicalTrafficManager::Construct         BrnPhysicalTrafficManager.cpp
//     StuntOffencesManager::Construct           BrnStuntOffencesManager_Construct.cpp
//
// BrnVehicleManager.cpp is mounted as well, so re-merging is a plain move of this text back into
// it, then deleting this TU.
// ==================================================================================================

namespace BrnPhysics
{
namespace Vehicle
{
    // ==============================================================================================
    // THE THIRTY PerfMonCpu MONITOR HANDLES. Twenty-nine of them are FILE-SCOPE globals on the
    // console (dword_82F2A14C..dword_82F2A290); the thirtieth is the member
    // miRaceCarWorldContactValidationPM. Every call passes page/min/budget/tag as (r4, r5, f1, r7)
    // with r6 never written, i.e. the FIVE-argument AddMonitor -- see the note in
    // CgsPerfMonCpu.h, which was settled from this very function.
    //
    // SEVEN OF THE TWENTY-NINE ARE GUARDED (`if (global < 0) global = AddMonitor(...)`), so they
    // MUST start NEGATIVE or they never register at all and the handle stays 0 -- a valid handle
    // belonging to somebody else's monitor. The guard is the console's register-once idiom for the
    // seven VehiclePhysics-LEVEL sub-monitors, which whichever object constructs first owns.
    // FLAG: only the SIGN of the console's initial value is observable (the guard is `blt`); -1 is
    // the committed sentinel this tree already uses for an unregistered handle, and it is also what
    // AddMonitor itself returns when the registry is full. The other twenty-two are unconditional,
    // so their initial value is never read; they are seeded identically for uniformity.
    // HOIST 2026-08-06 (UpdateVehiclePhysics wave): the THIRTEEN handles the per-frame
    // conductor brackets with (14C..19C) are now EXTERNAL (declared in
    // BrnVehicleManagerPerfMonHandles.h) because the UpdateVehiclePhysics slice TU reads them --
    // on the console both functions share one TU's file-scope statics. The rest stay static here.
    // These are file-scope HERE because nothing else in the tree registers them yet. On the
    // console the seven guarded slots are shared with the VehiclePhysics timing sites; when those
    // land they must bind to THESE symbols, not declare their own.
    // ==============================================================================================
    static const s32 KI_PERFMON_UNREGISTERED = -1;

    // flt_82004A20 -- the budget every one of the thirty passes in f1 (`lfs f22, flt_82004A20` once,
    // then `fmr f1, f22` at all thirty call sites).
    static const f32 KF_VMAN_PERFMON_BUDGET = 10.0f;

    s32 gs_iUpdateStuntOffencesPM            = KI_PERFMON_UNREGISTERED;   // dword_82F2A1A0
    s32 gs_iUpdateVehicleImpactsPM    = KI_PERFMON_UNREGISTERED;   // dword_82F2A14C
    s32 gs_iProcessAboveGroundLTsPM   = KI_PERFMON_UNREGISTERED;   // dword_82F2A150
    s32 gs_iTractionLTsPM             = KI_PERFMON_UNREGISTERED;   // dword_82F2A154
    // HOISTED 2026-08-11 (lifetime wave): Start/EndVehicleTractionLineTests are real bodies now
    // and bracket their stages with these five, so per the handles header's rule they move to
    // external linkage there rather than being re-declared locally.
    s32 gs_iTractionGetLinesPM        = KI_PERFMON_UNREGISTERED;   // dword_82F2A158
    s32 gs_iTractionLineTestsPM       = KI_PERFMON_UNREGISTERED;   // dword_82F2A15C
    // HOISTED 2026-08-10 (ground wave): RunTractionLineTestJobs @0x825B5168 brackets its Begin /
    // RunStream stages with these two ids, so per the handles header's rule they move to external
    // linkage there rather than being re-declared locally.
    s32 gs_iLineTestsBeginPM          = KI_PERFMON_UNREGISTERED;   // dword_82F2A168
    s32 gs_iLineTestsRunStreamPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A16C
    s32 gs_iLineTestsFinishPM         = KI_PERFMON_UNREGISTERED;   // dword_82F2A170
    s32 gs_iLineTestsEndPM            = KI_PERFMON_UNREGISTERED;   // dword_82F2A174
    s32 gs_iTractionProcessResultsPM  = KI_PERFMON_UNREGISTERED;   // dword_82F2A160
    static s32 gs_iTractionTrafficPM         = KI_PERFMON_UNREGISTERED;   // dword_82F2A164
    s32 gs_iCrashFatalPM              = KI_PERFMON_UNREGISTERED;   // dword_82F2A178
    s32 gs_iUpdateRaceCarsPM          = KI_PERFMON_UNREGISTERED;   // dword_82F2A17C
    s32 gs_iUpdateDriversPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A180
    s32 gs_iUpdateVehiclesPM          = KI_PERFMON_UNREGISTERED;   // dword_82F2A184
    // HOISTED 2026-08-07 (orchestrator wave): the seven guarded VPhys sub-monitors gained a
    // second reader (VehiclePhysics::Update @0x826412C0 brackets its stages with them), so per
    // the handles header's rule they move to external linkage there. [GUARDED] registration
    // below is unchanged.
    s32 gs_iVPhysUpdatePM             = KI_PERFMON_UNREGISTERED;   // dword_82F2A278   [GUARDED]
    s32 gs_iVPhysSwitchAttribsPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A27C   [GUARDED]
    s32 gs_iVPhysUpdateCrashingPM     = KI_PERFMON_UNREGISTERED;   // dword_82F2A280   [GUARDED]
    s32 gs_iVPhysUpdateAirRamsPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A284   [GUARDED]
    s32 gs_iVPhysUpdateSpinPM         = KI_PERFMON_UNREGISTERED;   // dword_82F2A288   [GUARDED]
    s32 gs_iVPhysUpdateDrivingPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A28C   [GUARDED]
    s32 gs_iVPhysUpdateLVPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A290   [GUARDED]
    s32 gs_iRBChangePM                = KI_PERFMON_UNREGISTERED;   // dword_82F2A188
    s32 gs_iAfterTouchPM              = KI_PERFMON_UNREGISTERED;   // dword_82F2A18C
    s32 gs_iUpdateTrafficPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A190
    s32 gs_iUpdateAggressiveDrivingPM = KI_PERFMON_UNREGISTERED;   // dword_82F2A194
    s32 gs_iUpdateCrashesPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A198
    s32 gs_iUpdatePassBysPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A19C

    // ARTIST 0x82615FF8. Release the driver and physics state for every race-car slot.
    bool VehicleManager::Release()
    {
        mbTrafficCheckingAllowed = true;
        mUsedRaceCars.Destruct();
        mRaceCarsAddedForCollision.Destruct();
        mNetworkCarsAddedForCollisionThisFrame.Destruct();
        mHiddenRaceCars.Destruct();

        for (s32 liCar = 0; liCar < 8; ++liCar)
        {
            maRaceCarDrivers[liCar].Release();
            maRaceCarVehicles[liCar].VehiclePhysics::Release();
            maRaceCarEntityIDs[liCar].muValue = CgsSceneManager::K_INVALID_ENTITY_ID;
            maRaceCarHandlingBodyIDs[liCar] = CgsPhysics::K_INVALID_RIGID_BODY_ID;
        }
        mPhysicalTrafficManager.Release();
        return true;
    }

    // ARTIST 0x826337F0. Destroy embedded owners and invalidate their registered IDs.
    void VehicleManager::Destruct()
    {
        mbTrafficCheckingAllowed = true;
        mDebugComponent.Destruct();
        mUsedRaceCars.Destruct();
        mRaceCarsAddedForCollision.Destruct();
        mNetworkCarsAddedForCollisionThisFrame.Destruct();
        mHiddenRaceCars.Destruct();

        for (s32 liCar = 0; liCar < 8; ++liCar)
        {
            maRaceCarDrivers[liCar].Destruct();
            maRaceCarVehicles[liCar].VehiclePhysics::Destruct();
            maRaceCarEntityIDs[liCar].muValue = CgsSceneManager::K_INVALID_ENTITY_ID;
            maRaceCarHandlingBodyIDs[liCar] = CgsPhysics::K_INVALID_RIGID_BODY_ID;
        }
        mPhysicalTrafficManager.Destruct();
        mPlayerAiDriver.Destruct();
    }

    // ==============================================================================================
    // VehicleManager::Construct  @0x8263B7C8 -- 943 instructions.
    //
    // The shape, in issue order (the full instruction-level recipe, every rodata symbol and every
    // default value live in the recipe block in BrnVehicleManager.h; this body is written off it
    // and adds no new decode):
    //     ~1..310    the thirty AddMonitor calls
    //     ~311..410  mePrepareStage / meReleaseStage, the debug component, then an INLINED
    //                CgsNumeric::Random::Construct
    //     ~411..510  the EIGHT-CAR LOOP
    //     ~511..600  the traffic manager, the discarded-contact queue, the camera matrix, the spare
    //                AI driver and the four RaceCarBitArray clears
    //     ~601..943  the TUNING BANK -- 91 seats
    //
    // THE BANK IS NOT OPTIONAL. A body that ran the spine and skipped the last ~340 instructions
    // would leave every takedown/slam/shunt threshold at zero while LOOKING complete -- the
    // silent-drop-stub failure class this project keeps paying for. All 91 seats are here, and the
    // 85 scalar ones were emitted by a machine join of the asm seat table against this class's own
    // declarations rather than retyped, so a value cannot drift from the header it was proven in.
    //
    // WHAT THIS FUNCTION DOES *NOT* DO, stated so it is not "completed" later: it never touches
    // mbEasyCrashingEnabled, DEBUG_mbAlwaysCrashRaceCarToRaceCar, DEBUG_mbHornTakedownEnabled,
    // mbDebugModifyTrafficContacts, mbUpdatedPlayerDriver, mbForceNoSlowMo, miPlayerSpeed/Strength/
    // Control/Boost, mn8RoundRobinControlWord, mCurrentTime/mStartModeTime, or the whole
    // contact-generation block at +172465..+172580. Those are the console's own omissions.
    //
    // ORDERING. Two things are issued LATE and are kept late because the asm puts them there:
    // StuntOffencesManager::Construct fires BETWEEN the +172580 and +172584 counter stores
    // (0x8263C61C / 0x8263C620 / 0x8263C640), and the thirtieth monitor + its assert sit inside the
    // bank, not with the other twenty-nine. Neither is tidied up.
    // ==============================================================================================
    void VehicleManager::Construct()
    {
        gs_iUpdateStuntOffencesPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Stunt Offences", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateVehicleImpactsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Vehicle Impacts", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iProcessAboveGroundLTsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Process Above Ground LTs", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionLTsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Traction LTs", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionGetLinesPM = CgsDev::PerfMonCpu::AddMonitor(
            "        GetLines", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionLineTestsPM = CgsDev::PerfMonCpu::AddMonitor(
            "        LineTests", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsBeginPM = CgsDev::PerfMonCpu::AddMonitor(
            "           Begin", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsRunStreamPM = CgsDev::PerfMonCpu::AddMonitor(
            "           RunStream", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsFinishPM = CgsDev::PerfMonCpu::AddMonitor(
            "           Finish", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsEndPM = CgsDev::PerfMonCpu::AddMonitor(
            "           End", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionProcessResultsPM = CgsDev::PerfMonCpu::AddMonitor(
            "        ProcessResults", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionTrafficPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Traffic", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iCrashFatalPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Crash Fatal", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateRaceCarsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Race Cars", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateDriversPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Drivers", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateVehiclesPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Vehicles", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdatePM < 0)
            gs_iVPhysUpdatePM = CgsDev::PerfMonCpu::AddMonitor(
                "          VPhys::Update", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysSwitchAttribsPM < 0)
            gs_iVPhysSwitchAttribsPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Switch Attribs", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateCrashingPM < 0)
            gs_iVPhysUpdateCrashingPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Crashing", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateAirRamsPM < 0)
            gs_iVPhysUpdateAirRamsPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Air Rams", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateSpinPM < 0)
            gs_iVPhysUpdateSpinPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Spin", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateDrivingPM < 0)
            gs_iVPhysUpdateDrivingPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Driving", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateLVPM < 0)
            gs_iVPhysUpdateLVPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update LV", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iRBChangePM = CgsDev::PerfMonCpu::AddMonitor(
            "        RB Change", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iAfterTouchPM = CgsDev::PerfMonCpu::AddMonitor(
            "        AfterTouch", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateTrafficPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Traffic", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateAggressiveDrivingPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Aggressive Driving", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateCrashesPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Crashes", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdatePassBysPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update PassBys", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);

        mePrepareStage = 0;   // +0  (stw r30, 0(r31))
        meReleaseStage = 3;   // +4  (stw r24, 4(r31))

        // +44224 / +44232. Cleared here, ~200 instructions before the four RaceCarBitArrays below --
        // the asm issues them with the two `std r30` at 0x8263BCD0/0x8263BCD4, before the debug
        // component call, not with the other bitsets.
        mUsedRaceCars.UnSetAll();
        mUsedRaceCarCrashesList.UnSetAll();

        // +161968. TWO arguments on the console (r3 = the component, r4 = r31 = the manager);
        // Hex-Rays renders the call with none.
        mDebugComponent.Construct(this);

        // +16. The block at 0x8263BCDC..0x8263BEE8 is CgsNumeric::Random::Construct() inlined
        // VERBATIM -- muSeed = KU_RANDOM_DEFAULT_SEED, index 0, ring[0] = 1.0f, then seven
        // AddRandomFloatToBuffer draws and one final index step. Nothing to write.
        mRandom.Construct();

        // ==========================================================================================
        // THE EIGHT-CAR LOOP (0x8263BF04..0x8263BF90). r29 walks the record at the asm-literal
        // stride 0x1460; r25 walks the drivers at 0xE0; r26/r28/r27 walk the three sibling arrays at
        // 8/4/0x400. Every statement below is one of those cursors.
        // ==========================================================================================
        for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
        {
            maRaceCarDrivers[liCar].Construct();    // +64,   stride 224

            // ONE CALL, not a base call plus six pokes. The X360 inlines RaceCarPhysics::Construct
            // here (`bl VehiclePhysics::Construct` then its own six writes); the PS3 build calls it
            // out of line at 0x6EB3D4 with exactly that body. See RaceCarPhysics::Construct.
            maRaceCarVehicles[liCar].Construct();   // +1856, stride 5216 on the console

            maRaceCarEntityIDs[liCar].muValue     = CgsSceneManager::K_INVALID_ENTITY_ID;
            maRaceCarHandlingBodyIDs[liCar]       = CgsPhysics::K_INVALID_RIGID_BODY_ID;
            maeRaceCarTypes[liCar]                = BrnWorld::E_RACE_CAR_TYPE_INACTIVE;   // == 3
            mauNetworkCarHiddenFramesRemaining[liCar] = 0;

            // The console's own assert, at the console's own file/line (`li r5, 0x8B4` ==
            // VehiclePhysics.h:2228). It is VACUOUS HERE and that is stated rather than dressed
            // up: on the X360 the argument is a walking cursor register (`addi r27,r27,0x400`) that
            // the compiler cannot prove non-null, while here it is the address of an array element,
            // which the standard guarantees is never null. Kept because it is the console's own
            // check at the console's own site, not because it can fire.
            CGS_ASSERT(&maRaceCarDebugComponent[liCar] != NULL, "lpDebugComponent != NULL");

            // A plain address-of a constructed object. This used to be a cast through an opaque
            // 8x1024 byte span, with a banner explaining why writing the down-force mirror through
            // it at a raw console offset was still in bounds; the span and the offset write are both
            // gone, so the banner goes with them. UpdateDownForce reaches the mirror by name.
            maRaceCarVehicles[liCar].mpDebugComponent = &maRaceCarDebugComponent[liCar];
        }

        mPhysicalTrafficManager.Construct();   // +44768

        // +160672. BaseEventQueue<T>::Construct sets {mpEvents = maEvents, miMaxLength = 20,
        // miLength = 0} and fires the same `lpEventBuffer != NULL` assert (CgsBaseEventQueue.h:160)
        // the console does -- the three `stw` at 0x8263C048/58/60 plus the guarded assert above them.
        mDiscardedContacts.Construct();

        // +172240. Four 16-byte lanes built on the stack from flt_82001C98 (1.0f) and flt_82001CC0
        // (0.0f): rows {1,0,0,0} {0,1,0,0} {0,0,1,0} and a ZERO fourth row -- which is exactly what
        // Matrix44Affine::SetIdentity() writes (its wAxis is {0,0,0,0}, not {0,0,0,1}).
        mCameraMatrix.SetIdentity();

        mPlayerAiDriver.Construct();   // +171968

        // +44704..+44736. Four `std r30` at 0x8263C0A8..0x8263C0C4 (the asm emits two of them twice;
        // there are four distinct addresses).
        mHiddenRaceCars.UnSetAll();
        mRaceCarsAddedForCollision.UnSetAll();
        mNetworkCarsAddedForCollisionThisFrame.UnSetAll();
        mNetworkCarsRecievedFirstUpdate.UnSetAll();

        // ==========================================================================================
        // THE TUNING BANK -- 91 seats, +171464..+172616. 85 emitted by machine join; the six below
        // that the join could not settle are hand-written and each says why.
        // ==========================================================================================
        mbSlamsAndShuntsOn                    = true;    // +171464
        mbAllowSlamsAndShuntsEffectsForRivals = true;    // +171465
        mfFrontRaySensorLength                = 4.0f;    // +171468
        mfFrontRayLength                      = 1.5f;    // +171472
        mfRearRayLength                       = 1.5f;    // +171476
        mfPlayerShuntScale                    = 0.325f;  // +171480
        mfAIShuntScale                        = 0.2f;    // +171484
        mfShuntDecay                          = 0.15f;   // +171488
        mfVulnerabilityFactorMax              = 4.0f;    // +171492
        mfPlayerVulnerabilityDurationSeconds  = 2.0f;    // +171496
        mfAIVulnerabilityDurationSeconds      = 4.0f;    // +171500
        mfMinSteeringOverrideTimeSlam         = 0.2f;    // +171504
        mfMinSteeringOverrideTimeShunt        = 0.2f;    // +171508
        mfPlayerMaxSteeringOverrideTimeSlam   = 0.7f;    // +171512
        mfAIMaxSteeringOverrideTimeSlam       = 0.9f;    // +171516
        mfPlayerMaxSteeringOverrideTimeShunt  = 0.4f;    // +171520
        mfAIMaxSteeringOverrideTimeShunt      = 0.7f;    // +171524
        mfPlayerSlamForceScale                = 0.25f;   // +171528
        mfAISlamForceScale                    = 0.25f;   // +171532
        mfMaxSlamClosingXSpeed                = 16.0f;   // +171536
        mfMinSecondsBetweenImpacts            = 0.3f;    // +171540
        mfMinAmountOfSlamForce                = 0.2f;    // +171544
        mfMinAmountOfShuntForce               = 0.25f;   // +171548

        // The one value X360 Hex-Rays carried in a register (it renders as `v62`). The PS3 build
        // gives it literally at its +170880, and the symbol the X360 loads it from is flt_82001C98 --
        // the SAME slot this function uses for the mCameraMatrix identity diagonal, so this function
        // alone proves the value is 1.0f without leaving the X360 image.
        mfTailgatingVunerabilityTime = 1.0f;;  // +171552

        mfBaseSlamMagnitude                    = 3.0f;    // +171556
        mfBaseShuntMagnitude                   = 22.5f;   // +171560
        mfTBoneTakedownMaxAngle                = 35.0f;   // +171564
        mfTBoneTakedownSpeed                   = 30.0f;   // +171568
        mfMaxShuntAngle                        = 25.0f;   // +171572
        mfMinNudgeSpeed                        = 8.0f;    // +171576
        mfMinShuntSpeed                        = 12.0f;   // +171580
        mfFatalShuntSpeed                      = 140.0f;  // +171584
        mfSlamDecayRate                        = 0.13f;   // +171588
        mfSlamEffectMinMagnitude               = 0.4f;    // +171592
        mfSlamEffectMaxMagnitude               = 2.0f;    // +171596
        mfMinShuntMagnitude                    = 0.2f;    // +171600
        mfMaxShuntMagnitude                    = 0.4f;    // +171604
        mfMinShuntBackwardsMagnitude           = 0.3f;    // +171608
        mfMaxShuntBackwardsMagnitude           = 0.75f;   // +171612
        mfMinTradingPaintSpeed                 = 0.8f;    // +171616
        mfFatalSlamSpeed                       = 140.0f;  // +171620
        mfFatalHitCrashingCarSpeed             = 50.0f;   // +171624
        mfMaxHeadToHeadAngle                   = 45.0f;   // +171628
        mfMinHeadToHeadSpeed                   = 40.0f;   // +171632
        mfMinHeadToHeadIndividualSpeed         = 40.0f;   // +171636
        mfAngleForVerticleTakedown             = 60.0f;   // +171640
        mfCrashingAICollisionCrashThresholdMPH = 50.0f;   // +172208
        mfHeadOnWorldCrashThreshold            = 40.5f;   // +172212
        mfSideOnWorldCrashThreshold            = 50.0f;   // +172216
        mfTrafficCollisionCheckThresholdMPH    = 30.0f;   // +172220
        mfMinRCTrafficTranslateSpeedMPH        = 40.0f;   // +172224
        mfVerticalTakedownAngleDeg             = 65.0f;   // +172228
        mbImpactTime                           = false;   // +172304
        mbStopPlayerCrashing                   = false;   // +172306
        mbStopAICrashing                       = false;   // +172307
        mbCrashOnHandbrakeTurn                 = false;   // +172308
        mbCrashPlayerNextUpdate                = false;   // +172309
        mbTrafficCheckingAllowed               = true;    // +172313
        mbAftertouchIsForceAdditive            = false;   // +172314
        mbIsOnlineGameMode                     = false;   // +172315
        mfPlayerStatStrength                   = 0.0f;    // +172320
        mfPlayerStatDamageLimit                = 0.0f;    // +172324
        miCarSpeed                             = 0;       // +172328
        miCarStrength                          = 0;       // +172332
        miCarControl                           = 0;       // +172336
        miCarBoost                             = 0;       // +172340
        meCarType                              = 3;       // +172344
        meCurrentGameModeType                  = -1;      // +172380
        mfCarStatStrengthSlamMax               = 2.0f;    // +172384
        mfCarrStatStrengthSlamMin              = 0.5f;    // +172388
        mfCarStatStrengthShuntMax              = 2.0f;    // +172392
        mfCarrStatStrengthShuntMin             = 0.05f;   // +172396
        mfCarStatStrengthBeingSlammedMax       = 2.0f;    // +172400
        mfCarStatStrengthBeingSlammedMin       = 0.5f;    // +172404
        mfCarStatStrengthBeingShuntedMax       = 2.0f;    // +172408
        mfCarrStatStrengthBeingShuntedMin      = 0.05f;   // +172412

        // +172416 / +172420. The DWARF types these `const SimpleVehiclePhysics*`; they are modelled
        // as u32 slots so the 16-aligned normal below keeps its asm-proven +172432 seat. Construct
        // only NULLs them (`stwx r30`).
        muCachedCarASlot = 0;
        muCachedCarBSlot = 0;

        mbCachedCarCarPredictionResult = false;   // +172424

        // +172432. `stvx128 v0, r31, r9` of the 16 bytes at unk_82181520 -- the identity basis row
        // {0,0,1,0}, settled in-repo twice (ICECameraSpaceHandler.cpp:124, BrnShadowMap.cpp:955/998).
        // i.e. the cached car-vs-car prediction normal starts as the world +Z axis.
        {
            const Vector3 lAggregateValue(0.0f, 0.0f, 1.0f);
            mCachedCarCarPredictionNormal = lAggregateValue;
        }

        meStationaryPlayerWheelAngle = 2;       // +172448
        mbCrashRaceCarWhenFatal      = true;    // +172452
        meShowtimeBehaviour          = BrnGameState::E_SHOWTIME_MODE_ON_SIXAXIS;   // +172456 = 2

        // THE THIRTIETH MONITOR -- page 6, not 12, and the only one stored INTO the object. The
        // console asserts its handle immediately (BrnVehicleManager.cpp:778, `li r5, 0x30A`), which
        // is where the member's DWARF name came from.
        miRaceCarWorldContactValidationPM = CgsDev::PerfMonCpu::AddMonitor(
            "PHYS ValidateRCWorldContact", CgsDev::E_PMP_6, false, KF_VMAN_PERFMON_BUDGET, true);
        CGS_ASSERT(miRaceCarWorldContactValidationPM >= 0,
                   "miRaceCarWorldContactValidationPM >= 0");

        miNumTrafficSphereWorldTests = 0;;  // +172580 (renamed at the 2026-08-06 carve; DWARF :1072)

        // ISSUED HERE, between the two counter stores -- 0x8263C61C stores +172580, 0x8263C620 is
        // this call, 0x8263C640 stores +172612. Not moved next to the other sub-constructors.
        mStuntOffencesManager.Construct();   // +44240

        muTakedownEventsThisFrame = 0;;  // +172612
        mpTractionLineStreamProducer = 0;;  // +172584 (renamed at the carve: the console `stwx 0` here null-stores the DWARF :1075 POINTER)

        mbInOnlineGameModeStartLine = false;;  // +172318
        mbPlayerCarInJunkYard       = false;;  // +172319

        // +172592 / +172608. One `stvx128 v127` (16 zero bytes) then one zero byte -- the DWARF's
        // adjacent `Sphere mStuckInCollisionTestCacheSphere` + `bool mbPlayerCarStuckInCollision`.
        // Raw bytes because Sphere has no committed home; the 16-byte size is what the stvx128 proves.
        for (s32 liByte = 0; liByte < 16; ++liByte)
            mStuckInCollisionTestCacheSphere[liByte] = 0;
        mbPlayerCarStuckInCollision = false;
    }
}
}

// FOLDED FROM BrnVehicleManager_GetUpdatedVehicleBodies.cpp
// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_GetUpdatedVehicleBodies.cpp
//
// THE PER-FRAME EXTERNAL-BODY PUBLISH -- the leg that tells the rigid-body simulation where
// every live car IS this frame. Two bodies, one slice TU (home TUs BrnVehicleManager.cpp /
// BrnPhysicalTrafficManager.cpp are still unmounted; this is the established slice pattern of
// BrnVehicleManager_ReadUpdatedBodies.cpp / _WriteOutVehicleStats.cpp -- fold back when the
// homes mount):
//
//   VehicleManager::GetUpdatedVehicleBodies         @0x82619340  (435 insns)  DWARF h:361 / .cpp:4154
//   PhysicalTrafficManager::GetUpdatedVehicleBodies @0x825EEF70  (~440 insns) DWARF h:162 / .cpp:1821
//
// Caller: PhysicsModule::BridgeUpdatedVehiclesToSimulation @0x825ADEA8, once per physics frame,
// AFTER UpdateVehiclePhysics and BEFORE the sim step, into a stack-local
// EventQueue<InUpdateExternalBody,60> that is then appended to the sim input. The sim's drain
// (PhysicsSimulationModule::ProcessUpdateExternalBodyQueue @0x828A3B30) looks each id up and
// SILENTLY skips one it does not know -- so a never-created proxy costs nothing.
//
// ---- WHY THIS EXISTS (2026-09-02, "prop collision at high speed only works sometimes") ------
// Every race car and every physical traffic car has a PROXY rigid body in the simulation --
// the E_ENTITYTYPE_PROP_COLLISION_RACECAR (11) / _TRAFFIC (12) body that ProcessCreateEvents /
// the traffic create path post with the car's mass. PropManager::SetupAndValidatePropContact
// re-routes every prop-vs-car contact onto that proxy (RoutePropVsRaceCarContactToDummyCar),
// and the sim solves the prop against IT. The proxy is an ACTIVE body with 10,000x drag: it
// goes nowhere on its own, and THIS function is the only thing that ever moves it. With this
// function gated the proxy sat at the car's creation pose with zero velocity for the whole
// session, so a whole prop promoted to E_PHYSICAL was solved against a body kilometres away
// with no momentum -- the lever arm made the proxy look weightless at the contact and the prop
// got nothing but a penetration nudge. Jointed props below their move threshold never noticed
// (HandleContactWithLean/TiltProp move them directly, off the car's own velocity), which is
// exactly the "works at low speed, not at high speed" shape of the report.
//
// ---- GROUND TRUTH (X360 ARTIST assembly, both functions read instruction by instruction) ----
// The Hex-Rays for 0x82619340 was an .ida-exports HOLE; it was pulled headless from the IDB on
// 2026-09-02 (tools/ida/export_all.py + EXPORT_ADDR_FILE) and the JSON landed beside the others
// in .ida-exports/BURNOUT_X360_ARTIST.XEX/. The traffic twin @0x825EEF70 was always exported.
//
// VehicleManager::GetUpdatedVehicleBodies -- register map (prologue 0x82619354..0x82619358):
//   r20 = this, arg_1C = r4 = lpUpdatedBodyQueue, r29 = the live-car index, r21 = 1, r28 = 0.
//   0x82619360  addis/addi this+0x10000-0x5340 == +44224 == mUsedRaceCars (one 64-bit word),
//               then the inlined BitArray<8>::GetFirstNonZeroBit (cntlzd on the lowest set bit);
//               no bit, or an index outside [0,8) -> straight to the traffic call (0x826199F0).
//   0x82619440  mulli r11,r29,0x1460 ; add this      -> &maRaceCarVehicles[idx] - 0x740 (the
//               console folds the +0x740 array base into every displacement below)
//   0x82619448  addi r10,r29,0x155C ; slwi 3 ; ldx    -> maRaceCarHandlingBodyIDs[idx] (+43744)
//   0x82619458  lbz 0x1B74(r11)      == car+0x1434 == RaceCarPhysics::mbAISlowMo
//   0x82619460  lvx128 v127, r11+0x7A0  == car+0x60 == mAngularVelocity
//   0x82619470  lvx128 v125..v122, r10=car+0x10 +0/+0x10/+0x20/+0x30 == mTransform's four rows
//   0x82619480  lvx128 v126, r11+0x790  == car+0x50 == mLinearVelocity
//   0x82619488  beq -> 0x82619520 : NOT in AI slow-mo -> `stb 0, 0x1B75` == SetWrittenIntoRWInSlowMo(false)
//   0x8261948C..0x82619518 (in slow-mo): `stb 1, 0x1B75` == SetWrittenIntoRWInSlowMo(true), then
//               `lfs flt_820049E0` (== 100.0f, the same AI-crash slow-motion factor
//               RaceCarPhysics::Update divides its timestep by) splatted into two VecFloats,
//               vrefp + two Newton-Raphson steps each (== the SDK operator/(Vector3,VecFloat)),
//               `vmulfp128 v127` and `vmulfp128 v126` -- BOTH velocities divided by 100.
//               (While the car integrates in slow motion its velocities are the SLOW ones; the
//               proxy is written at the same rate so the sim sees a consistent body.)
//   0x82619524..0x826197C0  the three tripwires, in this order: IsValid(mAngularVel) :4188,
//               IsValid(mVel) :4189, IsValid(mTransform) :4190 (all four rows, xyz lanes,
//               vcmpeqfp self-compare per lane == the NaN test).
//   0x826197C4..0x82619818  the event: rows -> +0x10..+0x40, v126 -> +0x50, v127 -> +0x60, and
//               mID = ((handlingBodyId>>32 & 0x00FFFFFF) | 0x0B000000) << 32 | (handlingBodyId & 0xFFFFFFFF)
//               -- the handling-body handle with its EntityId owner byte stamped to 11, i.e.
//               RigidBodyId::GetEntityId / EntityId::SetOwner(11) / RigidBodyId::SetEntityId,
//               which is the DWARF's callee list for the block at .cpp:4187 (lUpdateDummyRaceCarEvent).
//   0x8261981C  bl EventQueue<InUpdateExternalBody>::AddEvent -- ONE event per car.
//               (The DWARF's other local, lUpdateEvent @:4156, is the staging record the rows
//               are read into; the X360 emits exactly one AddEvent per car -- the dummy.)
//   0x82619820..0x826199E8  the inlined GetNextNonZeroBit with IsBitSet's "invalid index"
//               tripwire (CgsBitArray.h:203) -- unreachable, the bound is clamped to 8 first.
//   0x826199F0  addis/addi this+0x10000-0x5120 == +44768 == &mPhysicalTrafficManager ;
//               bl PhysicalTrafficManager::GetUpdatedVehicleBodies(r4 = the same queue) -- tail.
//
// PhysicalTrafficManager::GetUpdatedVehicleBodies -- register map (0x825EEF84..0x825EEFA0):
//   this+0x20000-0x6798 == +104552 == mUsedTrafficVehicles (BitArray<20>), same inlined walk,
//   bound 20 (0x825EEFF4 `cmpwi 0x14`).
//   0x825EF0C0  bl GetPhysicsEntityId(idx) -> r31 (owner 2, idx<<10) -- called BEFORE the
//               `liVehicle < ku8TotalMaxNumPhysicalTraffic` tripwire (h:730) at 0x825EF0C8.
//   0x825EF0E8  lwz this+0x20000-0x6B4C == +103604 == mpaTrafficVehicles ; slwi idx,6 (stride 64)
//   0x825EF100  lwz 0x1C(vehicle)  == PhysicalTrafficVehicle::mpVehicleBody (SimpleVehiclePhysics*)
//   0x825EF108  lvx128 v127 body+0x60 (mAngularVelocity), v126 body+0x50 (mLinearVelocity),
//               v125..v122 body+0x10.. (mTransform rows) -- the same ExternallySimulatedBody frame.
//   0x825EF124..0x825EF3B8  the three tripwires: :1844 mAngularVel, :1845 mVel, :1846 mTransform.
//   0x825EF3BC..0x825EF40C  mID = ((entity & 0x00FFFFFF) | 0x0C000000) << 32 | low32(entity<<32)
//               == the physics-traffic EntityId re-owned to 12 (PROP_COLLISION_TRAFFIC), index 0.
//   0x825EF410  bl AddEvent -- again ONE event per vehicle. No slow-mo arm on the traffic side.
//
// ---- LAYOUT DISCIPLINE (AGENTS.md gotcha 1) ------------------------------------------------
// Not one console byte offset is used below: mUsedRaceCars / maRaceCarVehicles /
// maRaceCarHandlingBodyIDs (through GetRigidBodyId) / mPhysicalTrafficManager /
// mUsedTrafficVehicles / mpaTrafficVehicles / mpVehicleBody are all reached BY NAME, and the
// ExternallySimulatedBody frame through its accessors.
//
// ---- NaN POLARITY (gotcha 4) -----------------------------------------------------------------
// The three tripwires are `vcmpeqfp.` self-compares reduced through CR6 -- a NaN lane compares
// unequal to itself, so a NaN FAILS the assert. rw::math::vpu::IsValid is exactly that test.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/World/BrnEntityTypes.h"                               // E_ENTITYTYPE_PROP_COLLISION_RACECAR / _TRAFFIC
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint (the opt-in [extbody] witness)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                   // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"  // InUpdateExternalBody
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                   // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"               // CgsSceneManager::EntityId::SetOwner
#include "rw/math/vpu/vector3_operation.h"                                 // rw::math::vpu::IsValid(Vector3) / Divide / Magnitude
#include "rw/math/vpu/matrix44affine_operation.h"                          // rw::math::vpu::IsValid(Matrix44Affine)
#include <stdlib.h>                                                        // getenv (host-only diag latch)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        // flt_820049E0 -- the .rdata literal the slow-mo arm loads at 0x82619490 and splats
        // into both reciprocal chains. Hex-Rays folds it to 100.0 at both `stfs` sites; it is
        // the same AI-crash slow-motion factor RaceCarPhysics::Update divides its timestep by
        // (RaceCarPhysics.cpp KVF_AI_CRASH_SLOWMO_FACTOR, seated from unk_82FB8880). Spelled
        // locally because the two are different rodata homes on the console.
        const f32 KF_AI_CRASH_SLOWMO_FACTOR = 100.0f;

    }

    // ========================================================================================
    // VehicleManager::GetUpdatedVehicleBodies   @0x82619340   (435 insns)
    // ========================================================================================
    void VehicleManager::GetUpdatedVehicleBodies(
        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>* lpUpdatedBodyQueue )
    {
        namespace vpu = rw::math::vpu;

        // The inlined BitArray<8> walk (GetFirstNonZeroBit / GetNextNonZeroBit). The console
        // additionally clamps the index to [0,8) before the body (0x826193B8/0x826193C4 and
        // the per-step `cmplwi 8` at 0x82619840); the committed walker never returns an index
        // outside the array, so the clamp is folded into the loop condition.
        for ( s32 i = mUsedRaceCars.GetFirstNonZeroBit();
              i >= 0 && i < 8;
              i = mUsedRaceCars.GetNextNonZeroBit( i ) )
        {
            RaceCarPhysics& lrRaceCar = maRaceCarVehicles[ i ];

            // DWARF :4156 lUpdateEvent -- the staging record. Rows/velocities first, exactly
            // the console's load order (angular, transform, linear), then the slow-mo arm.
            CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody lUpdateEvent;
            lUpdateEvent.mTransform  = lrRaceCar.GetTransform();
            lUpdateEvent.mVel        = lrRaceCar.GetLinearVelocity();
            lUpdateEvent.mAngularVel = lrRaceCar.GetAngularVelocity();

            if ( lrRaceCar.IsInAICrashSlowMo() )
            {
                // 0x8261949C `stb 1, 0x1B75` then the two vrefp+2xNR reciprocal multiplies.
                lrRaceCar.SetWrittenIntoRWInSlowMo( true );
                lUpdateEvent.mAngularVel = vpu::Divide( lUpdateEvent.mAngularVel, KF_AI_CRASH_SLOWMO_FACTOR );
                lUpdateEvent.mVel        = vpu::Divide( lUpdateEvent.mVel,        KF_AI_CRASH_SLOWMO_FACTOR );
            }
            else
            {
                lrRaceCar.SetWrittenIntoRWInSlowMo( false );   // 0x82619520 `stb 0, 0x1B75`
            }

            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mAngularVel ),
                        "rw::math::IsValid( lUpdateEvent.mAngularVel )" );            // :4188 (0x105C)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mVel ),
                        "rw::math::IsValid( lUpdateEvent.mVel )" );                   // :4189 (0x105D)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mTransform ),
                        "rw::math::IsValid( lUpdateEvent.mTransform )" );             // :4190 (0x105E)

            // DWARF :4187/:4189 -- the dummy race car: the handling-body handle with its
            // EntityId owner re-stamped to the PROP_COLLISION_RACECAR proxy (0x826197C8..
            // 0x826197F8: `srdi 32 ; clrlwi 8 ; oris 0xB00 ; sldi 32 ; or low32`). The
            // low dword (the handle's index half) is carried through untouched.
            CgsPhysics::RigidBodyId lDummyRaceCarId = GetRigidBodyId( i );
            CgsSceneManager::EntityId lDummyRaceCarEntityId = lDummyRaceCarId.GetEntityId();
            lDummyRaceCarEntityId.SetOwner( static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR ) );
            lDummyRaceCarId.SetEntityId( lDummyRaceCarEntityId );
            lUpdateEvent.mID = lDummyRaceCarId;

            // [DIAG] NOT IN THE X360 BINARY -- see PropDiagEnabled.
            {
                static bool sbLoggedFirst = false;
            }

            lpUpdatedBodyQueue->AddEvent( lUpdateEvent );                      // 0x8261981C
        }

        // 0x826199F0..0x826199FC -- the traffic half, same queue. On the console this is the
        // tail of the function whether or not any race car was live.
        mPhysicalTrafficManager.GetUpdatedVehicleBodies( lpUpdatedBodyQueue );
    }

    // ========================================================================================
    // PhysicalTrafficManager::GetUpdatedVehicleBodies   @0x825EEF70
    // ========================================================================================
    void PhysicalTrafficManager::GetUpdatedVehicleBodies(
        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>* lpUpdatedBodyQueue ) const
    {
        namespace vpu = rw::math::vpu;

        for ( s32 liUsedVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
              liUsedVehicle >= 0 && liUsedVehicle < static_cast<s32>( KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC );
              liUsedVehicle = mUsedTrafficVehicles.GetNextNonZeroBit( liUsedVehicle ) )
        {
            // DWARF :1824 lBodyID. 0x825EF0C0 -- called BEFORE the bound tripwire at 0x825EF0C8.
            const EntityId lBodyID = GetPhysicsEntityId( liUsedVehicle );

            CGS_ASSERT( liUsedVehicle < static_cast<s32>( KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC ),
                        "liVehicle < ku8TotalMaxNumPhysicalTraffic" );                 // h:730 (0x2DA)

            // DWARF :1834 / :1837. `lwz 0x1C(vehicle)` == mpVehicleBody -- the SimpleVehiclePhysics
            // base of whichever body (full or simple) this slot carries.
            const PhysicalTrafficVehicle* lpTrafficVehicle = GetTrafficVehicle( liUsedVehicle );
            const SimpleVehiclePhysics*   lpTrafficPhysics = lpTrafficVehicle->mpVehicleBody;

            CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody lUpdateEvent;
            lUpdateEvent.mAngularVel = lpTrafficPhysics->GetAngularVelocity();   // lvx128 v127 +0x60
            lUpdateEvent.mVel        = lpTrafficPhysics->GetLinearVelocity();    // lvx128 v126 +0x50
            lUpdateEvent.mTransform  = lpTrafficPhysics->GetTransform();         // v125..v122 +0x10..

            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mAngularVel ),
                        "RwMath::IsValid( lUpdateEvent.mAngularVel )" );              // :1844 (0x734)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mVel ),
                        "RwMath::IsValid( lUpdateEvent.mVel )" );                     // :1845 (0x735)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mTransform ),
                        "RwMath::IsValid( lUpdateEvent.mTransform )" );               // :1846 (0x736)

            // DWARF :1852/:1854 -- the dummy traffic car: RigidBodyId::Set(lBodyID, 0) then
            // the owner re-stamped to PROP_COLLISION_TRAFFIC (0x825EF3C4..0x825EF3EC:
            // `srdi 32 ; clrlwi 8 ; oris 0xC00 ; sldi 32 ; or low32`, low32 == 0 here).
            CgsPhysics::RigidBodyId lDummyCarId( static_cast<u64>( lBodyID.muValue ) << 32 );
            CgsSceneManager::EntityId lDummyRaceCarEntityId = lDummyCarId.GetEntityId();
            lDummyRaceCarEntityId.SetOwner( static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC ) );
            lDummyCarId.SetEntityId( lDummyRaceCarEntityId );
            lUpdateEvent.mID = lDummyCarId;

            // [DIAG] NOT IN THE X360 BINARY -- see PropDiagEnabled.
            {
                static bool sbLoggedFirst = false;
            }

            lpUpdatedBodyQueue->AddEvent( lUpdateEvent );                      // 0x825EF410
        }
    }
}
}

// FOLDED FROM BrnVehicleManager_MaintenanceEvents.cpp
// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_MaintenanceEvents.cpp
//
// VehicleManager::ProcessVehicleMaintenanceEvents @0x8264AB38 (118 insns) and FOUR of the five arms
// it dispatches, as REAL BODIES, plus the two create-path helpers those arms share with the create
// drain. The traffic twin stays a named one-shot gate (it is another wave's function).
//
// ProcessVehicleMaintenanceEvents        @0x8264AB38  (118)  bodied (earlier wave)
// RecordNetworkRaceCarsAddedForCollision @0x825C7EA8  (321)  BODIED THIS WAVE
// ProcessRemoveEvents                    @0x826160C8  (426)  BODIED THIS WAVE
// ProcessValidationEvents                @0x825E9010  ( 65)  BODIED THIS WAVE
// ProcessCollisionEvents                 @0x825E8F28  ( 57)  BODIED THIS WAVE  <- the "export hole"
// AddRaceCarDeformationModel             @0x825E9118  (153)  BODIED THIS WAVE
// SetAllNetworkRaceCarsHidden            @0x825E9380  (175)  BODIED THIS WAVE
// PhysicalTrafficManager::ProcessTrafficMaintenanceEvents @0x82649768 (246)  BODIED 2026-08-22
//       (wave T3 round 1, cluster C2 -- the gate is DELETED; its eight arms live here and in the
//        two sibling slices BrnPhysicalTrafficManager_Create.cpp / _Remove.cpp. The last two
//        gates are gone too: ProcessTrafficEvents was bodied 2026-08-29 (_TrafficEvents.cpp) and
//        CheckForTrafficHittingWater @0x8261DDF0 is bodied at the bottom of this file, G35-D1.)
//   ProcessCreateEvents                    @0x82616770  (1067) -> its OWN slice TU (see below)
//
// Slice TU (home BrnVehicleManager.cpp is still unmounted) -- the same shape as the sibling
// BrnVehicleManager_Prepare.cpp / _ReadUpdatedBodies.cpp / _TractionLineTests.cpp slices.
//
// -------------------------------------------------------------------------------------------------
// THE "EXPORT HOLE" AT @0x825E8F28 IS RETIRED, AND IT WAS NEVER A HOLE IN THE DATABASE.
//
// Every previous banner in this file recorded ProcessCollisionEvents as "a HOLE in the IDA export
// set -- it has no per-function JSON and is known only by the name IDA prints at this one call
// site ... the insn count is genuinely unknown and is left unstated rather than guessed."
//
// Retired with the targeted headless-idat technique of commit b53e2523 (one `idat.exe -A -S<script>`
// run over a private copy of BURNOUT_X360_ARTIST.XEX.i64). The result:
//     had_function = TRUE.  start 0x825E8F28, end 0x825E900C, 57 instructions,
//     name = BrnPhysics::Vehicle::VehicleManager::ProcessCollisionEvents,
//     Hex-Rays decompiles it clean, xrefs_to = the one expected caller.
// It is a fully analysed, correctly NAMED function that the export RUN simply never wrote a file
// for -- `tools/ida/export_all.py` walks `idautils.Functions()` and writes one JSON per function,
// and this one's write did not happen. ⇒ **"absent from .ida-exports" is a fact about the export,
// not about the database.** Two of this project's standing parks were on that word; anything else
// still parked "export hole" is worth re-checking the same way before it is planned around.
//
// -------------------------------------------------------------------------------------------------
// WHY ProcessCreateEvents IS NOT IN THIS FILE ANY MORE.
//
// It is bodied -- completely, all 1,067 instructions -- in the sibling slice
// BrnVehicleManager_ProcessCreateEvents.cpp, which compiles GREEN. It was split out so that the
// MOUNTING decision was separable: the four arms below are inert bookkeeping over queues that are
// empty today, while setting one bit of mUsedRaceCars switches on four already-called per-frame
// loops against a freshly constructed car. Those two things should not have to be mounted in the
// same commit, and a single TU would have forced that.
//
// ⇒ BOTH FILES ARE MOUNTED AS OF THE BOOT-VERIFIED WAVE. This banner used to say "THIS FILE IS
//   MOUNTED. The create TU is NOT (yet)"; that is retired -- tools/build/build_game_exe.bat now
//   echoes BrnVehicleManager_ProcessCreateEvents.cpp too, with the conductor's rationale beside
//   the line. The split stands on its own merits (separable slices, separable blame); it is no
//   longer a mount gate.
//
// -------------------------------------------------------------------------------------------------
// CONSOLE-VALUE HAZARD DECLARED ONCE, HERE, BECAUSE THREE BODIES BELOW HIT IT.
//
// The vehicle manager's race-car identity is a 64-bit CgsPhysics::RigidBodyId whose HIGH dword is
// the 32-bit entity word (`ld ; srdi r,r,32` at every read site).
//
// THE FORK THIS SECTION EXISTED TO MANAGE IS GONE (2026-08-11, handle-widening wave).
// It used to read: "the Deformation event structs this file posts into type their mHandlingBodyID
// with the GLOBAL 4-byte ::RigidBodyId { u32 muValue } stand-in ... so every post below assigns
// the ENTITY WORD into .muValue ... never a truncating cast of the whole handle."
// `::RigidBodyId` is now a typedef of the real 8-byte CgsPhysics::RigidBodyId (BrnCommonTypes.h,
// which carries the eight-witness evidence banner), so every post below passes the WHOLE HANDLE,
// which is what the console does: `std`/`ld` at every one of those seats.
//
// AND ONE OF THOSE HAND-CARRIED POSTS WAS A LIVE SILENT DROP, IN THIS MOUNTED FILE.
// ProcessCollisionEvents' two arms wrote `lPost.mHandlingBodyID.muValue = lrEvent.mBodyId.muValue`
// -- the entity word into the LOW dword. The console writes `extldi r11,r11,64,32`, i.e. into the
// HIGH dword, and every consumer starts with `ld` + `srdi 32`. The word was being delivered to the
// half everybody discards, with nothing asserting. Both arms are corrected in place, spelled as
// the shift the asm performs. The deactivate / remove / add posts were lossless by luck (they went
// through GetEntityId(), which reads the half that carries the value) and now pass the handle whole.
// The luck was real and is worth recording: ProcessCreateEvents is the only writer of
// maRaceCarHandlingBodyIDs and it stores `((u64)entityWord) << 32`, so the low dword is always zero
// TODAY. The widening is what stops that from being load-bearing.
//
// -------------------------------------------------------------------------------------------------
// THE HAZARD ABOVE WAS INDEPENDENTLY MEASURED BY THE SIBLING WAVE (2026-08-11), which recovered
// these same two functions, saw the same fork, and PARKED rather than shipped. Its evidence is
// folded in here because it CONFIRMS the reading above from the other direction -- and because its
// conclusion (which this file does not share) is the follow-up flag:
//
//   (a) Every access to maRaceCarHandlingBodyIDs is EIGHT bytes wide, in both directions --
//         ProcessRemoveEvents         0x82616444  ldx r11, r9, r25  ; 0x82616448 std r11, var_D0
//                                     0x826164B0  ldx r11, r11, r25 ; std r11, var_E8
//         AddRaceCarDeformationModel  0x825E9324  ldx r8,  r23, r29 ; std r8,  var_C8
//                                     0x825E9358  ld  r5,  0(r22)   -- AddDeformationModel's arg
//                                     0x825E932C  ldx r4,  r20, r29 -- its ResourceHandle, ALSO 8
//       and the 32-bit forms (lwzx/stw/lwz) appear all over both bodies for the genuinely 32-bit
//       fields, so this is a CONTRAST WITHIN ONE BODY, not one isolated instruction. ⇒ the
//       "handling-body handle is 64-bit" question BrnDeformationManager.h left open is DECIDED.
//
//   (b) It also settles that header's other open contradiction -- why the committed
//       BrnDeformationManager.cpp bodies do `muValue >> 24` / `(muValue >> 10) & 0x3FFF` when
//       CgsRigidBody.h documents the packing as (high dword = EntityId, low dword = index).
//       AddRaceCarDeformationModel does the SAME two extractions, but on the HIGH DWORD FIRST:
//         0x825E9218 srdi r11,r11,32 ; 0x825E9220 clrlwi r28,r11,0
//         0x825E9224 srwi r11,r28,24        <- the owner byte
//         0x825E9240 extrwi r11,r28,14,8    <- the 14-bit index
//       i.e. the ARITHMETIC in those consumers is right and only the WIDTH is short. Which is
//       exactly the "assign the ENTITY WORD, never a truncating cast" rule stated above.
//
// THE SIBLING WAVE DREW THE OPPOSITE CONCLUSION -- that these two bodies must not land at all,
//   because passing the u64 through the 4-byte stand-in "delivers ZERO with nothing asserting".
//   That failure mode is REAL for a truncating cast (`static_cast<u32>(theWholeHandle)` takes the
//   low dword, which for a race car is identically zero). It was NOT what the deactivate/remove/add
//   posts did (they went through GetEntityId(), the half that carries the value) -- but it WAS
//   exactly what the two ProcessCollisionEvents arms did, in the opposite direction. Both waves
//   were half right, and the debt they both named is what closed it.
//
// THE FOLLOW-UP FLAG THAT STOOD HERE IS DISCHARGED (2026-08-11). It read: "`::RigidBodyId`
//   in BrnCommonTypes.h:28 should be widened to match CgsPhysics::RigidBodyId ... That moves five
//   `mHandlingBodyID` fields, the BrnDeformationManager.cpp bodies that shift them, and those event
//   records' field offsets -- all in a MOUNTED subsystem. Its own wave, with its own boot test.
//   When it lands, every `GetEntityId()` seat in this file should be revisited to pass the whole
//   handle instead." That wave is this one; all five fields, both consumers and every seat in this
//   file are done, and the boot test is in the log. The evidence lives in BrnCommonTypes.h.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"  // the five drained queues
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleOutputRequestInterface (InRemoveRigidBody)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h" // the five posted queues
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"  // BrnWorld::ERaceCarType
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                        // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                // gbReadSurfaceProperties / KAB_SURFACE_IS_WATER
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h" // GetSimpleVehicleBox / GetAboveGroundTestResult
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"                   // CgsGeometric::Box
#include "SharedClasses/World/BrnCollisionTag.h"                                   // BrnWorld::KU_COLLISION_MASK_SURFACE_ID
#include "rw/math/vpu/vector3_operation.h"                                         // Vector3 * f32
#include <cmath>                                                                   // std::fabs
#include <cstdlib>                                                                 // std::getenv ([T-water] witness)

namespace
{
    inline void MaintenanceGateLogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
}

static const char KAC_ARTIST_ASSERT_FILE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Physics/VehicleManager/BrnVehicleManager.cpp";

namespace BrnPhysics
{
namespace Vehicle
{
    // ---------------------------------------------------------------------------------------------
    // ProcessVehicleMaintenanceEvents  @0x8264AB38  (118 insns; DWARF/asserts BrnVehicleManager.cpp
    // :1141..:1147)
    //
    // Read straight off the pseudocode AND the asm: seven null asserts in parameter order, then six
    // unconditional calls, no branch and no local state between them. The argument routing below is
    // the console's exactly -- note that the arms do NOT all take the same list:
    //     RecordNetworkRaceCarsAddedForCollision(this, in)
    //     ProcessRemoveEvents (this, in, out, mgrOut,        deform)
    //     ProcessCreateEvents (this, in, out, mgrOut,        deform)
    //     ProcessValidationEvents(this, in,                  deform)
    //     ProcessCollisionEvents (this, in,                  deform)
    //     PhysicalTrafficManager::ProcessTrafficMaintenanceEvents(this+44768, <all seven>)
    // The traffic call's r3 is `this + 44768`, which is &mPhysicalTrafficManager -- reached BY NAME
    // here, not by that offset.
    //
    // The console returns the traffic call's result (r3 falls through). Nothing reads it: the one
    // caller, PhysicsModule::PostSceneUpdate, ignores the return. Declared void here, matching the
    // shape the caller actually uses; the discarded value is noted rather than fabricated into a
    // return type nobody consumes.
    // ---------------------------------------------------------------------------------------------
    void VehicleManager::ProcessVehicleMaintenanceEvents(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        const VehicleInputInterface* lpInputInterface,
        VehicleOutputRequestInterface* lpOutputInterface,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        CGS_ASSERT_AT(lpInputBufferStack        != 0,
            "lpInputBufferStack != NULL", KAC_ARTIST_ASSERT_FILE, 1141 /* ARTIST 0x8264AB7C */);        // :1141

        CGS_ASSERT_AT(lpOutputBufferStack       != 0,
            "lpOutputBufferStack != NULL", KAC_ARTIST_ASSERT_FILE, 1142 /* ARTIST 0x8264ABA0 */);       // :1142

        CGS_ASSERT_AT(lpInputInterface          != 0,
            "lpInputInterface != NULL", KAC_ARTIST_ASSERT_FILE, 1143 /* ARTIST 0x8264ABC4 */);          // :1143

        CGS_ASSERT_AT(lpOutputInterface         != 0,
            "lpOutputInterface != NULL", KAC_ARTIST_ASSERT_FILE, 1144 /* ARTIST 0x8264ABE8 */);         // :1144

        CGS_ASSERT_AT(lpManagerOutputInterface  != 0,
            "lpManagerOutputInterface != NULL", KAC_ARTIST_ASSERT_FILE, 1145 /* ARTIST 0x8264AC0C */);  // :1145

        CGS_ASSERT_AT(lpVehicleOutputInterface  != 0,
            "lpVehicleOutputInterface != NULL", KAC_ARTIST_ASSERT_FILE, 1146 /* ARTIST 0x8264AC30 */);  // :1146

        CGS_ASSERT_AT(lpDeformationInterface    != 0,
            "lpDeformationInterface != NULL", KAC_ARTIST_ASSERT_FILE, 1147 /* ARTIST 0x8264AC54 */);    // :1147


        RecordNetworkRaceCarsAddedForCollision(lpInputInterface);

        ProcessRemoveEvents(lpInputInterface, lpOutputInterface,
                            lpManagerOutputInterface, lpDeformationInterface);

        ProcessCreateEvents(lpInputInterface, lpOutputInterface,
                            lpManagerOutputInterface, lpDeformationInterface);

        ProcessValidationEvents(lpInputInterface, lpDeformationInterface);

        ProcessCollisionEvents(lpInputInterface, lpDeformationInterface);

        // TEN arguments, not seven. The last three ride
        // the outgoing param-save slots at r1+0x54/+0x5C/+0x64 (0x8264ACC4..0x8264ACF0) and are
        // `r31+0x740` == maRaceCarVehicles, `r31+0x40` == maRaceCarDrivers and
        // `r31+0x10000-0x5340` == +44224 == mUsedRaceCars. Reached BY NAME here.
        mPhysicalTrafficManager.ProcessTrafficMaintenanceEvents(
            lpInputBufferStack, lpOutputBufferStack, lpInputInterface, lpOutputInterface,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
            maRaceCarVehicles, maRaceCarDrivers, &mUsedRaceCars);
    }

    // =============================================================================================
    // ARM 1 -- RecordNetworkRaceCarsAddedForCollision @0x825C7EA8 (321 insns)
    //          asserts BrnVehicleManager.cpp:9677 / :9699, CgsBitArray.h:203 / :222
    //
    // Two per-frame bitsets, one queue drain. Read off the asm register for register:
    //
    //   0x825C7EF0  addis r11,r30,1 ; addi r11,r11,-0x5150 ; std r20(0),0(r11)
    //               -> this + 44720 == mNetworkCarsAddedForCollisionThisFrame, cleared (one std,
    //                  i.e. one 8-byte BitArray<8> word).
    //   0x825C7F00  lis/ori r10 = 0x22B50 (142160) ; ldx r11,r31,r10
    //   0x825C7F10  lis/ori r9  = 0xAEA8  (44712)  ; stdx r11,r30,r9
    //               -> mRaceCarsAddedForCollision = *lpInputInterface->GetRaceCarsAddedForCollision()
    //                  (142160 is that member's console seat -- the very literal
    //                  BrnVehicleInputInterface.h's accessor banner already quotes).
    //   0x825C7F08  addis r3,r31,2 ; addi r3,r3,0x358 -> input + 131928 ==
    //               mNetworkCarsAddedRemovedForCollisionQueue; length re-read at +8 every iteration.
    //
    // THE LOOP BOUND IS RE-READ, NOT CACHED (`lwz r10,8(r10)` at 0x825C8394, inside the loop),
    // so it is written as a live `GetLength()` test rather than hoisted.
    // The two CgsBitArray.h asserts (:203 IsBitSet bounds, :222 SetBit bounds) are the
    // container's own tripwires and fire from inside the named calls; the console emits them
    // inline because it inlines the container. Not duplicated here.
    // The two "invalid index : %u < %u" / "Index: %u, Number of bits: %u" streams are console
    // gpcMessageBuffer formatting inside those same container asserts -- see the container header.
    // =============================================================================================
    // ---------------------------------------------------------------------------------------------
    // (E)/(F) RECONCILED (merge, 2026-08-11): the physics campaign's same-day re-measure (85ba9eb0,
    // written against ITS pre-wave tree) landed two findings whose DELETE-WHENs both fired within
    // hours -- kept here in corrected form because each carries a live lesson:
    //  (E) said the drain's one remaining hole was AddRaceCarDeformationModel @0x825E9118, held
    //      back over the 8-byte handling-body handle. In THIS tree it is BODIED with the
    //      entity-word-through-GetEntityId() discipline (see the hazard banner above); the 4-byte
    //      ::RigidBodyId widening remains flagged follow-up debt. Its false-negative list (the
    //      Attrib ctors exercised by SetAttributes) stands and is worth keeping.
    //  (F) proved the render pose (ActiveRaceCar::mRenderParams.mBodyTransform) is fed ONLY via
    //      ActiveRaceCar::UpdatePhysicsState <- ReadUpdatedActiveRaceCarDataFromPhysics @0x822E87B8
    //      -- absent in THEIR tree, LANDED in this one, so their conclusion inverts: the
    //      compensating pair is now REAL and the readback is what carries physics onto the screen.
    // Their trap warning SURVIVES and is the operative one for the no-movement diagnosis:
    //      a stable-looking frame is NOT evidence the car rests -- the readback only runs for
    //      slots set in VehicleOutputInterface::mUsedRaceCars and only copies what the physics->
    //      output publish leg (GetUpdatedVehicleBodies @0x82619340 / UpdateVehiclePhysicsPost-
    //      Simulation @0x826426E0 / WriteOutVehicleStats @0x8263F460 -- ALL still gates) writes.
    //      Measure motion with a VehicleManager-side probe, never off the screen.
    // ---------------------------------------------------------------------------------------------
    void VehicleManager::RecordNetworkRaceCarsAddedForCollision(
        const VehicleInputInterface* lpInputInterface)
    {
        CGS_ASSERT_AT(lpInputInterface != 0, "lpInputInterface != NULL", CGS_ARTIST_FILE, 9677);

        mNetworkCarsAddedForCollisionThisFrame.UnSetAll();
        mRaceCarsAddedForCollision = *lpInputInterface->GetRaceCarsAddedForCollision();

        const VehicleInputInterface::NetworkCarsAddRemoveForCollisionQueue* lpQueue =
            lpInputInterface->GetNetworkCarsAddRemoveForCollisionQueue();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const VehicleAddedForCollisionEvent& lrEvent = lpQueue->GetEvent(liEvent);

            // `ld r11,0(evt) ; srdi r11,r11,32 ; extrwi r29,r11,14,8` -- the 14-bit entity index
            // out of the embedded entity word, i.e. the active-race-car slot.
            const u32 luRaceCar = lrEvent.mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();

            CGS_ASSERT_AT(mUsedRaceCars.IsBitSet(luRaceCar),
                          "mUsedRaceCars.IsBitSet( liRaceCar )", CGS_ARTIST_FILE, 9699);

            // `lbz r11,8(evt)` -- VehicleAddedForCollisionEvent::mbAdded.
            if (lrEvent.mbAdded)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Network race car "
                        << static_cast<s32>(luRaceCar) << ", type "
                        << static_cast<s32>(maeRaceCarTypes[luRaceCar])
                        << " was added for collision\n";
                }
                mNetworkCarsAddedForCollisionThisFrame.SetBit(luRaceCar);
            }
        }
    }

    // =============================================================================================
    // ARM 2 -- ProcessRemoveEvents @0x826160C8 (426 insns)
    //          asserts BrnVehicleManager.cpp:1202 / :1203 / :1639 / :1640, CgsBitArray.h:203 / :241
    //
    // THIS IS PART OF THE CREATE WAVE, NOT A LATER ONE. A read-only probe at the drain point
    // (2026-08-11) reported three create events on one boot naming race-car slots **0, then 1, then
    // 2** -- three different slots, not one car three times. ProcessCreateEvents' own assert list
    // contains "Race Car Index Already Used", i.e. the console expects the slot to be FREE when it
    // is claimed, and this function is the only thing that frees one.
    //
    // The body, in the console's order (`bl sub_825BB8A0` is BaseEventQueue<RemoveRaceCarEvent>::
    // GetEvent; queue seat `addis r3,r4,2 ; addi r3,r3,-0x6D0` == input + 129328):
    //   1. owner + slot out of the event's VolumeInstanceId (`srwi r10,r27,24`, `extrwi r30,r27,14,8`)
    //   2. two asserts, then the driver-record clear
    //   3. two deformation posts (deactivate, remove) keyed on maRaceCarHandlingBodyIDs[slot]
    //   4. maRaceCarEntityIDs[slot] = the invalid sentinel (dword_82F2A3A4 == 0xFFFFFFFF, read out
    //      of the image), and an InRemoveRigidBody post whose id re-owners the entity word to 0x0B
    //   5. three per-car seeds inside maRaceCarVehicles[slot]
    //   6. mUsedRaceCars.UnSetBit(slot), the log, the network-car un-hide, and the type reset
    //
    // STEP 2 IS AN INLINED VehicleDriver::ClearControls(). The stores are, relative to
    // &maRaceCarDrivers[slot] (`mulli r11,r30,0xE0 ; add r11,r11,r24`, then +0x40..+0x8E):
    //     +0x00 = -1 (word)     +0x04..+0x30 = 0.0f (twelve floats)     +0x34 = 1.0f
    //     +0x38 = -1 (byte)     +0x39,+0x3B..+0x42 = 0                  +0x48 = 0.0f
    //     +0x4C,+0x4D,+0x4E = 0
    // -- which is BrnAIDriverControls in full, EXCEPT +0x3A (mbToggle) and +0x44 (meDriverType).
    // That is the identical store set (identical omissions included) as the out-of-line
    // BrnNetworkDriverControls::Clear @0x82581200 this tree already documents, so it is the same
    // "clear the controls, keep the driver type" routine inlined here. `VehicleDriver::ClearControls`
    // IS declared (BrnVehicleDriver.h:66) and, as of 2026-08-11, HAS a body -- BrnVehicleDriver.cpp,
    // recovered from these same two inline sites by the sibling wave. The block is therefore no
    // longer written out here: this drain CALLS it, at the console's own call position.
    // 0.0f is flt_82001CC0 and 1.0f is flt_82001C98, both read out of the image, not assumed.
    // =============================================================================================
    void VehicleManager::ProcessRemoveEvents(const VehicleInputInterface* lpInputInterface,
                                             VehicleOutputRequestInterface* lpOutputInterface,
                                             VehicleManagerOutputInterface*,
                                             Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::RemoveRaceCarEventQueue* lpQueue =
            lpInputInterface->GetRemoveRaceCarEvents();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const RemoveRaceCarEvent& lrEvent = lpQueue->GetEvent(liEvent);

            const u32 luEntityWord = static_cast<u32>(lrEvent.mVolumeInstanceID.muId >> 32);
            const u32 luOwner      = luEntityWord >> 24;
            const u32 luRaceCar    = lrEvent.mVolumeInstanceID.GetEntityIDEntityIndex();

            CGS_ASSERT(luOwner == 1u,
                "lRaceCarEvent.mVolumeInstanceID.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :1202
            CGS_ASSERT(mUsedRaceCars.IsBitSet(luRaceCar),
                       "Trying to remove an unused race car");                                          // :1203

            // ---- the inlined VehicleDriver::ClearControls (see the banner) --------------------
            // DELETE-WHEN HONOURED 2026-08-11 (merge of the two create-drain waves). The 28-store
            // block that stood open here is now VehicleDriver::ClearControls, an out-of-line body
            // in BrnVehicleDriver.cpp recovered from BOTH console inline sites at once. Its store
            // set was checked field-for-field against this copy before the swap: identical, same
            // two omissions (mbToggle, meDriverType), same lone 1.0f. The call goes exactly where
            // the console's inlined run sat.
            maRaceCarDrivers[luRaceCar].ClearControls();

            // ---- the two deformation posts ---------------------------------------------------
            // Both take maRaceCarHandlingBodyIDs[slot]; see this file's console-value banner for
            // why the ENTITY WORD is what lands in the 4-byte event field.
            const CgsPhysics::RigidBodyId lHandlingBodyId(maRaceCarHandlingBodyIDs[luRaceCar]);

            {
                // `ldx r11,r9,r25 ; std r11,var_D0 ; bl DeactivateDeformationModelEvent::AddEvent`
                //
                // AT THE MERGE, and this one was a real bug in the
                // committed body. It used to write ONLY mHandlingBodyID, with a note claiming "the
                // console leaves mfInitialDamageAmount and meDeformationResetType at whatever the
                // stack held". THAT IS WRONG, and the sibling wave caught it: the other two fields
                // are LOOP-INVARIANT CONSTANTS, so the compiler hoisted their stores OUT of the
                // drain loop, into the prologue -- which is why a read that only scanned the loop
                // body could not see them. Re-read from the ARTIST asm, addresses and values:
                //     0x82616118  lfs  f31, flt_82001CC0   <- 0.0f, read out of the image
                //     0x8261611C  li   r11, -1
                //     0x82616128  stfs f31, var_C8(r1)     <- event +8  mfInitialDamageAmount = 0.0f
                //     0x82616138  stw  r11, var_C4(r1)     <- event +12 meDeformationResetType = -1
                //     ...loop...
                //     0x82616444  ldx  r11, r9, r25        <- maRaceCarHandlingBodyIDs[slot]
                //     0x82616448  std  r11, var_D0(r1)     <- event +0  mHandlingBodyID
                // var_D0 is the 16-byte event base and var_C8/var_C4 are its other two fields, so
                // the console posts a FULLY initialised event every iteration. All three are
                // written here now. (-1 is not a named DeformationResetType enumerator; it is the
                // literal the image stores, so it is spelled as a cast of that literal and not
                // rounded to the nearest named value.)
                // the WHOLE 8-byte handle, verbatim, which is
                // literally what `0x82616448 std r11, var_D0(r1)` does -- one doubleword straight
                // out of maRaceCarHandlingBodyIDs into the event's first field. It used to be
                // narrowed to the entity word through the 4-byte `::RigidBodyId` stand-in.
                Deformation::DeactivateDeformationModelEvent lDeactivate;
                lDeactivate.mHandlingBodyID        = lHandlingBodyId;
                lDeactivate.mfInitialDamageAmount  = 0.0f;                                  // flt_82001CC0
                lDeactivate.meDeformationResetType =
                    static_cast<Deformation::DeformationResetType>(-1);                     // li r11, -1
                lpDeformationInterface->GetDeactivateDeformationModelQueue().AddEvent(lDeactivate);
            }

            // The console's inlined VehicleManager::RemoveRaceCarDeformationModel -- its two
            // asserts are the give-away, and they sit AFTER the deactivate post, exactly here.
            CGS_ASSERT(lpDeformationInterface != 0, "NULL != lpDeformationInterface");             // :1639
            CGS_ASSERT(static_cast<s32>(luRaceCar) >= 0 && static_cast<s32>(luRaceCar) < 8,
                       "(liVehicleIndex >= 0) && (liVehicleIndex < ku8MaxNumRaceCars)");           // :1640
            {
                // `0x826164B4 std r11, var_E8(r1)` -- the whole 8-byte handle (see the deactivate
                // post above; both were narrowed through the stand-in until 2026-08-11).
                Deformation::RemoveDeformationModelEvent lRemove;
                lRemove.mHandlingBodyID = lHandlingBodyId;
                lpDeformationInterface->GetRemoveDeformationModelQueue().AddEvent(lRemove);
            }

            // ---- forget the entity, and ask the simulation to drop the body ------------------
            // `lwz r11, dword_82F2A3A4 ; stwx r11,r9,r25` -- the sentinel is 0xFFFFFFFF, read out
            // of the image (not inferred from the name).
            maRaceCarEntityIDs[luRaceCar].muValue = 0xFFFFFFFFu;

            {
                // `clrlwi r10,r27,8 ; oris r10,r10,0xB00 ; sldi r11,r10,32`
                // -- the entity word with its OWNER byte replaced by 0x0B
                // (BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR), promoted into the handle's high
                // dword. That is the id the simulation knows this body by.
                CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lRemoveBody;
                lRemoveBody.mID = static_cast<u64>((luEntityWord & 0x00FFFFFFu) | 0x0B000000u) << 32;
                lRemoveBody.mbFailIfRigidBodyNotFound = false;   // `stb r28(0), var_B8`
                lpOutputInterface->GetRemoveRigidBodyQueue()->AddEvent(lRemoveBody);
            }

            // ---- the three per-car seeds inside the race-car record --------------------------
            // `mulli r11,r30,0x1460 ; add r11,r11,r25 ; addi r11,r11,0x740` == &maRaceCarVehicles[slot],
            // then `std 0,0x1158`, `std 0,0x1220`, `stb 0,0x1359` -- i.e. drop every queued air-ram
            // and spin slot and clear the deformation-model-is-active latch. All three reached BY
            // NAME (VehiclePhysics.h pins +0x1158 / +0x1220 / +0x1359 to these members).
            maRaceCarVehicles[luRaceCar].Release();

            mUsedRaceCars.UnSetBit(luRaceCar);   // CgsBitArray.h:241 bounds tripwire

            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Removing race car "
                    << static_cast<s32>(luRaceCar) << ", type "
                    << static_cast<s32>(maeRaceCarTypes[luRaceCar]) << "\n";
            }

            if (maeRaceCarTypes[luRaceCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Making network car "
                        << static_cast<s32>(luRaceCar)
                        << " not hidden because it was removed\n";
                }
                mHiddenRaceCars.UnSetBit(luRaceCar);   // CgsBitArray.h:241 bounds tripwire
            }

            maeRaceCarTypes[luRaceCar] = BrnWorld::E_RACE_CAR_TYPE_INACTIVE;   // `li r10,3 ; stwx`
        }
    }

    // =============================================================================================
    // ARM 4 -- ProcessValidationEvents @0x825E9010 (65 insns)
    //
    // The smallest of the five and completely unambiguous. Drain
    // mValidateRaceCarEventQueue (input + 131472) and, per event, either LATCH the two resource
    // handles into the manager's own tables or CLEAR them, then forward a ValidateRaceCarEvent onto
    // the deformation interface's own validate queue (deform + 3856).
    //
    //   `ld r10,8(evt) ; srdi ; extrwi r10,r10,14,8`   -- the slot, out of mVolumeInstanceID
    //   `lbz r9,0(evt)`                                -- mbValidate selects the arm
    //   validate arm  (0x825E9078..0x825E90C4): maRaceCarModelHandles[slot] = evt.mModelHandle,
    //       maRaceCarGraphicsModelHandles[slot] = evt.mGraphicsHandle, then the forwarded event is
    //       {mbValidate = 1, mVolumeInstanceID, mModelHandle}.
    //   invalidate arm(0x825E90C8..0x825E90F4): both table entries cleared to {0,0}, and the
    //       forwarded event is {mbValidate = 0, mVolumeInstanceID}.
    //
    // NEITHER ARM WRITES THE FORWARDED EVENT'S mGraphicsHandle, and the invalidate arm does not
    // write its mModelHandle either. Reproduced exactly -- the console posts a partly-uninitialised
    // stack event, and filling those fields in would be inventing input for the deformation drain.
    // 8 * (slot + 0x154C) and 8 * (slot + 0x1554) are the two CONSOLE strides into the tables;
    // they are reached by NAME here because the host stride is 16, not 8 (see BrnVehicleManager.h).
    // =============================================================================================
    void VehicleManager::ProcessValidationEvents(const VehicleInputInterface* lpInputInterface,
                                                 Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::ValidateRaceCarEventQueue* lpQueue =
            lpInputInterface->GetValidateRaceCarEvents();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const ValidateRaceCarEvent& lrEvent = lpQueue->GetEvent(liEvent);
            const u32 luRaceCar = lrEvent.mVolumeInstanceID.GetEntityIDEntityIndex();

            ValidateRaceCarEvent lForwarded;
            if (lrEvent.mbValidate)
            {
                maRaceCarModelHandles[luRaceCar]         = lrEvent.mModelHandle;
                maRaceCarGraphicsModelHandles[luRaceCar] = lrEvent.mGraphicsHandle;

                lForwarded.mbValidate        = true;
                lForwarded.mVolumeInstanceID = lrEvent.mVolumeInstanceID;
                lForwarded.mModelHandle      = lrEvent.mModelHandle;
                // mGraphicsHandle deliberately NOT written -- see the banner.
            }
            else
            {
                maRaceCarModelHandles[luRaceCar].Clear();
                maRaceCarGraphicsModelHandles[luRaceCar].Clear();

                lForwarded.mbValidate        = false;
                lForwarded.mVolumeInstanceID = lrEvent.mVolumeInstanceID;
                // mModelHandle / mGraphicsHandle deliberately NOT written -- see the banner.
            }

            lpDeformationInterface->GetValidateDeformationModelEvents().AddEvent(lForwarded);
        }
    }

    // =============================================================================================
    // ARM 5 -- ProcessCollisionEvents @0x825E8F28 (57 insns)   the retired "export hole"
    //
    // Two independent drains, both pure forwarders into the deformation interface:
    //   loop 1  input + 131744 (mSetRaceCarCollisionEventQueue)   -> deform + 4128
    //   loop 2  input + 131836 (mSetRaceCarCullingGroupEventQueue)-> deform + 4592
    //
    // Each event's 32-bit EntityId is promoted into the HIGH dword of a 64-bit rigid-body handle --
    // `lwz r11,<scratch> ; extldi r11,r11,64,32` is a rotate-left-32 of the entity word, i.e.
    // `(u64)entityWord << 32`. The payload byte / word is copied across unchanged
    // (`lbz var_50+4 ; stb var_38` and `lwz var_50+4 ; stw var_38`).
    //
    // Both loops re-read the queue length every iteration (`lwz r11,8(r30)` at 0x825E8F90 /
    // 0x825E8FF4); written as live GetLength() tests, not hoisted.
    // `this` IS UNUSED in the console body -- r3 is never read after the prologue. Kept a
    // non-static member because that is what the console's `bl` with r3 = this is.
    // =============================================================================================
    void VehicleManager::ProcessCollisionEvents(const VehicleInputInterface* lpInputInterface,
                                                Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::SetRaceCarCollisionEventQueue* lpCollisionQueue =
            lpInputInterface->GetSetRaceCarCollisionEvents();

        for (s32 liEvent = 0; liEvent < lpCollisionQueue->GetLength(); ++liEvent)
        {
            const SetRaceCarCollisionEvent& lrEvent = lpCollisionQueue->GetEvent(liEvent);

            // THIS LINE WAS AN ACTIVE SILENT DROP UNTIL 2026-08-11, in a MOUNTED TU.
            // The console is `lwz r11,<scratch> ; extldi r11,r11,64,32` == `(u64)entityWord << 32`
            // -- the entity word promoted into the HIGH dword of an 8-byte handle. Through the
            // 4-byte `::RigidBodyId` stand-in the word landed in the LOW dword instead, i.e. in the
            // half every consumer discards (they all do `ld` then `srdi 32` first -- see
            // ProcessAddDeformationModelEvents @0x82644940). The value was being posted to the
            // wrong half with nothing asserting. Now spelled as the shift the asm performs.
            Deformation::SetModelCollisionEvent lPost;
            lPost.mHandlingBodyID = CgsPhysics::RigidBodyId(
                static_cast<u64>(lrEvent.mBodyId.muValue) << 32);       // extldi r11,r11,64,32
            lPost.mbCollide               = lrEvent.mbCollide;
            lpDeformationInterface->GetSetModelCollisionEvents().AddEvent(lPost);
        }

        const VehicleInputInterface::SetRaceCarCullingGroupEventQueue* lpCullingQueue =
            lpInputInterface->GetSetRaceCarCullingGroupEvents();

        for (s32 liEvent = 0; liEvent < lpCullingQueue->GetLength(); ++liEvent)
        {
            const SetRaceCarCullingGroupEvent& lrEvent = lpCullingQueue->GetEvent(liEvent);

            // Same `extldi r11,r11,64,32` promotion as the collision arm above -- see that comment
            // for why the old `.muValue = ...` was a silent drop into the discarded half.
            Deformation::SetModelCullingGroupEvent lPost;
            lPost.mHandlingBodyID = CgsPhysics::RigidBodyId(
                static_cast<u64>(lrEvent.mBodyId.muValue) << 32);       // extldi r11,r11,64,32
            lPost.mCullGroup              = lrEvent.mCullingGroup;
            lpDeformationInterface->GetSetModelCullingGroupEvents().AddEvent(lPost);
        }
    }

    // =============================================================================================
    // AddRaceCarDeformationModel @0x825E9118 (153 insns)
    //   asserts BrnVehicleManager.cpp:1606 / :1607 / :1611, BrnVehicleManager.h:1985
    //
    // THIS FUNCTION WAS RECORDED AS "ALREADY BODIED AND MOUNTED" BY AN EARLIER BANNER IN THIS
    // FILE. It was ABSENT: before this wave the name appeared in exactly two places in the whole
    // tree, both of them comment lines in that banner. Its `xrefs_to` is a one-element set
    // (ProcessCreateEvents), which is why nothing ever failed to link.
    //
    // The body is a marshaller into DeformationInputInterface::AddDeformationModel. Eleven
    // arguments, every one of them read off the call-site asm at 0x825E9320..0x8261936C:
    //     r4 = maRaceCarModelHandles[index]      (`ldx r4,r20,r29`  -- the whole 8-byte handle)
    //     r5 = maRaceCarHandlingBodyIDs[index]   (`ldx r8,r23,r29 ; std ; ld r5`)
    //     r6 = GetEntityId(that handle)          (the inlined helper -- see BrnVehicleManager.h)
    //     r7 = &maRaceCarVehicles[index]         (`mulli r9,r31,0x1460 ; add r24 ; addi r7,r24,0x740`)
    //     v1 = mpAttribs->mBaseAttribs.mCOMOffset(`lwz r21,0xE60(r24)` == the record's +0x720
    //                                             pointer, then `lvx128 v1,r21,0x20`)
    //     r8 = the transform, passed straight through
    //     v2 = ZERO   (`stfs 0.0f x3 ; lvx128 v2,r0,&var_A0`)
    //     v3 = ZERO   (`stfs 0.0f x4 ; lvx128 v3,r0,&var_B0`)
    //     f1 = the damage amount, passed straight through
    //     r10 = the reset type, passed straight through
    //     +stack byte = 1  (`li r31,1 ; stb r31,var_D9`) -- lbUseSweptSphereTests
    //
    // PPC FLOAT-ARG ABI, AND IT IS LOAD-BEARING ON BOTH SIDES. This function's own prologue
    // consumes r4,r5,r6 then f1 then r8 -- **r7 is skipped**, because the float argument takes the
    // slot. Reading the prologue as r4,r5,r6,r7,r8 would have shifted the reset type onto the
    // transform and the transform onto the index.
    // The three zeroed stack vectors are 0.0f from flt_82001CC0, read out of the image.
    // =============================================================================================
    void VehicleManager::AddRaceCarDeformationModel(
        Deformation::DeformationInputInterface* lpDeformationInterface,
        s32 liVehicleIndex,
        Matrix44Affine lInitialWorldSpaceTransform,
        f32 lfInitialDamageAmount,
        Deformation::DeformationResetType leBaseDeformationType)
    {
        CGS_ASSERT(lpDeformationInterface != 0, "NULL != lpDeformationInterface");           // :1606
        CGS_ASSERT(liVehicleIndex >= 0 && liVehicleIndex < 8,
                   "(liVehicleIndex >= 0) && (liVehicleIndex < ku8MaxNumRaceCars)");         // :1607
        CGS_ASSERT(maRaceCarModelHandles[liVehicleIndex].mpResourceMemory != 0,
            "maRaceCarModelHandles[liVehicleIndex].GetResource()->GetMemoryResource() != NULL"); // :1611

        const CgsPhysics::RigidBodyId lHandlingBodyId(maRaceCarHandlingBodyIDs[liVehicleIndex]);
        const EntityId lGlobalEntityId = GetEntityId(lHandlingBodyId);   // BrnVehicleManager.h:1985 lives inside

        Vector3 lvZero(0.0f, 0.0f, 0.0f);

        // THE MODEL-HANDLE PARK IS RETIRED (2026-08-11, handle-widening wave).
        // It used to read: "THE MODEL-HANDLE ARGUMENT IS A LOUD PARK ... there is no honest
        // projection of a pair of 64-bit pointers onto one u32 ... DELETE-WHEN the Deformation
        // group de-forks its ResourceHandle ... retyping it grows AddDeformationModelEvent past the
        // X360-attested sizeof == 160 its own mounted BaseEventQueue TU static_asserts, so it needs
        // the queue stride re-derived with it."
        // BOTH HALVES OF THAT DELETE-WHEN ARE NOW DONE: BrnDeformationEvents.h types mModelHandle
        // as the real CgsResource::ResourceHandle, and the stride WAS re-derived (console 160 ->
        // host 176, member by member, in BaseEventQueue_AddDeformationModelEvent_AddEvent.cpp --
        // re-derived, never bumped). So the console's own value goes through, verbatim:
        //     0x825E932C  ldx r4, r20, r29    -- one doubleword out of maRaceCarModelHandles[index]
        // and the same array element the :1611 assert two lines above already dereferences.
        Deformation::ResourceHandle lModelHandle = maRaceCarModelHandles[liVehicleIndex];

        if (maRaceCarVehicles[liVehicleIndex].mpAttribs == 0)
        {
            return;
        }

        // THE WHOLE 8-BYTE HANDLE (2026-08-11): `0x825E9324 ldx r8, r23, r29` ; `std r8, var_C8`
        // ; `0x825E9358 ld r5, 0(r22)` -- the console loads it, spills it and reloads it as one
        // doubleword into AddDeformationModel's r5. It used to be narrowed to the entity word here
        // because the event field was a 4-byte stand-in; it is not any more.
        // (lGlobalEntityId, arg 3, STAYS 32-bit -- `stw r28` / `0x825E9354 lwz r6`.)
        lpDeformationInterface->AddDeformationModel(
            lModelHandle,
            lHandlingBodyId,
            lGlobalEntityId,
            &maRaceCarVehicles[liVehicleIndex],
            maRaceCarVehicles[liVehicleIndex].mpAttribs->mBaseAttribs.mCOMOffset,
            lInitialWorldSpaceTransform,
            lvZero,
            lvZero,
            lfInitialDamageAmount,
            leBaseDeformationType,
            true);
    }

    // =============================================================================================
    // SetAllNetworkRaceCarsHidden @0x825E9380 (175 insns)
    //
    // 175 instructions of which almost all are the INLINED CgsContainers::BitArray<8> cursor walk
    // (`GetFirstNonZeroBit` / `GetNextNonZeroBit`, the `addi r11,r11,-1 ; and ; subf ; cntlzd ;
    // subf ; addi 0x3F` lowest-set-bit idiom) plus its two bounds asserts. The actual work is three
    // lines. The bitset seat is `addis r19,r15,1 ; addi r19,r19,-0x5340` == this + 44224 ==
    // mUsedRaceCars, and the per-car test is `addi r11,r31,0x2B28 ; slwi r11,r11,2 ; lwzx` ==
    // maeRaceCarTypes[i], compared against 2 == E_RACE_CAR_TYPE_NETWORK.
    //
    // The only argument is r4, forwarded verbatim as SetNetworkRaceCarHidden's r5 -- the frame
    // count, not a bool. All three console call sites pass `li r4, 1`: ProcessCreateEvents
    // (@0x826177CC), ProcessResetEvents (@0x82617EEC, a player-car reset) and OnJunkYardDriveThru
    // (@0x825EB0E0, junkyard entry) -- ARTIST xrefs_to; the last two landed 2026-09-23 (FX-VMNET).
    // =============================================================================================
    void VehicleManager::SetAllNetworkRaceCarsHidden(s32 liFrames)
    {
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            if (maeRaceCarTypes[liCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            {
                SetNetworkRaceCarHidden(static_cast<EActiveRaceCarIndex>(liCar), liFrames);
            }
        }
    }

    // =============================================================================================
    // THE TRAFFIC TWIN IS BODIED (2026-08-22, wave T3 round 1, cluster C2). The
    // BRN_MAINTENANCE_GATE that stood here since 2026-08-10 is DELETED.
    //
    // PhysicalTrafficManager::ProcessTrafficMaintenanceEvents @0x82649768 (246)
    //   asserts BrnPhysicalTrafficManager.cpp:3690..:3699 then :3701/:3702/:3703, then eight
    //   unconditional calls with no branch between them. DWARF BrnPhysicalTrafficManager.h:143.
    //
    // Argument routing verbatim from the asm at 0x82649A9C..0x82649B34 -- the arms do NOT all take
    // the same list, and three of them re-order it:
    //     AllocateInternalBuffers          (in, out)
    //     ProcessRemoveEvents              (inputInterface+139544, outReq, deform)
    //     ProcessCreateEvents              (in, mgrOut, outReq, deform, raceCars, drivers, usedCars)
    //     ProcessTrafficEvents             (in, outReq, mgrOut, vehOut, deform)
    //     CheckForTrafficHittingWater      (mgrOut, outReq, deform)
    //     SendCreateRemoveTrafficEvents    (outReq, deform)
    //     BridgeArticulatedJointRequestsToSim (outReq)
    //     DeallocateInternalBuffers        (in, out)
    //
    // THE THREE OPENING BITSET ASSERTS ARE THE ROUND'S REAL TRIPWIRE. mAddedTrafficVehicles /
    // mRemovedTrafficVehicles / mMadeSimpleTrafficVehicles must be EMPTY at the top of every frame,
    // which is only true because SendCreateRemoveTrafficEvents clears all three at its own tail
    // (BrnPhysicalTrafficManager_Remove.cpp). Gate that function and these three fire on the frame
    // after the first promotion.
    //
    // The console returns DeallocateInternalBuffers' r3 (it falls through). Declared void, the
    // shape ProcessVehicleMaintenanceEvents actually uses -- same call as its own return.
    // =============================================================================================
    void PhysicalTrafficManager::ProcessTrafficMaintenanceEvents(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        const VehicleInputInterface* lpInputInterface,
        VehicleOutputRequestInterface* lpOutputInterface,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        RaceCarPhysics* lpaRaceCarVehicles,
        VehicleDriver* lpaRaceCarDrivers,
        CgsContainers::BitArray<8u>* lpUsedRaceCars)
    {
        CGS_ASSERT(lpInputBufferStack       != 0, "lpInputBufferStack != NULL");        // :3690
        CGS_ASSERT(lpOutputBufferStack      != 0, "lpOutputBufferStack != NULL");       // :3691
        CGS_ASSERT(lpInputInterface         != 0, "lpInputInterface != NULL");          // :3692
        CGS_ASSERT(lpOutputInterface        != 0, "lpOutputRequestInterface != NULL");  // :3693
        CGS_ASSERT(lpManagerOutputInterface != 0, "lpManagerOutputInterface != NULL");  // :3694
        CGS_ASSERT(lpVehicleOutputInterface != 0, "lpVehicleOutputInterface != NULL");  // :3695
        CGS_ASSERT(lpDeformationInterface   != 0, "lpDeformationInterface != NULL");    // :3696
        CGS_ASSERT(lpaRaceCarVehicles       != 0, "lpaRaceCarVehicles != NULL");        // :3697
        CGS_ASSERT(lpaRaceCarDrivers        != 0, "lpaRaceCarDrivers != NULL");         // :3698
        CGS_ASSERT(lpUsedRaceCars           != 0, "lpUsedRaceCars != NULL");            // :3699

        CGS_ASSERT(mAddedTrafficVehicles.GetFirstNonZeroBit()
                       == TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX,
                   "mAddedTrafficVehicles.GetFirstNonZeroBit() == -1");                 // :3701
        CGS_ASSERT(mRemovedTrafficVehicles.GetFirstNonZeroBit()
                       == TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX,
                   "mRemovedTrafficVehicles.GetFirstNonZeroBit() == -1");               // :3702
        CGS_ASSERT(mMadeSimpleTrafficVehicles.GetFirstNonZeroBit()
                       == TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX,
                   "mMadeSimpleTrafficVehicles.GetFirstNonZeroBit() == -1");            // :3703

        AllocateInternalBuffers(lpInputBufferStack, lpOutputBufferStack);

        // `addis r4,r26,2 ; addi r4,r4,0x2118` == lpInputInterface + 139544 ==
        // mRemoveCrashedTrafficEventQueue, reached by name through the DWARF-attested reader.
        ProcessRemoveEvents(lpInputInterface->GetRemoveTrafficEvents(),
                            lpOutputInterface, lpDeformationInterface);

        ProcessCreateEvents(lpInputInterface, lpManagerOutputInterface, lpOutputInterface,
                            lpDeformationInterface, lpaRaceCarVehicles, lpaRaceCarDrivers,
                            lpUsedRaceCars);

        ProcessTrafficEvents(lpInputInterface, lpOutputInterface, lpManagerOutputInterface,
                             lpVehicleOutputInterface, lpDeformationInterface);

        CheckForTrafficHittingWater(lpManagerOutputInterface, lpOutputInterface,
                                    lpDeformationInterface);

        SendCreateRemoveTrafficEvents(lpOutputInterface, lpDeformationInterface);

        BridgeArticulatedJointRequestsToSim(lpOutputInterface);

        DeallocateInternalBuffers(lpInputBufferStack, lpOutputBufferStack);
    }

    // ---- the crash-side arm that is STILL a named one-shot gate ----------------------------------
    // GATE DELETED 2026-08-29 (traffic-crash wave): PhysicalTrafficManager::ProcessTrafficEvents
    //    @0x82643FB0 (72) is BODIED in BrnPhysicalTrafficManager_TrafficEvents.cpp along with the
    //    three functions it reaches that had no definition anywhere in the tree
    //    (ProcessSetTrafficCrashingEvents / ProcessUpdateNetworkTrafficEvents /
    //    SetTrafficVehicleNotCrashing). Its DELETE-WHEN said "when the crash/takedown wave lands";
    //    that wave landed SetTrafficVehicleCrashing on 2026-08-25 and left this gate standing, so
    //    the whole event-driven half of the traffic crash pipeline stayed unreachable for four
    //    days while its own producer (VehicleInputInterface::SetTrafficCrashing) was live.
    //    Mount BrnPhysicalTrafficManager_TrafficEvents.cpp in tools/build/build_game_exe.bat or
    //    the call above is an LNK2019 at exe link.

    // =============================================================================================
    // PhysicalTrafficManager::CheckForTrafficHittingWater  @0x8261DDF0  (347 insns)  -- G35-D1
    //
    // BODIED 2026-09-24 (lane FX-TRAFFIC2). The gate that stood here blamed "the water-volume query
    // at 0x825C0758" -- that address is AddTrafficRemovedEvent, and the console body calls no water
    // query: it reads the car's own down-ray result. Every callee has a body (GetSimpleVehicleBox,
    // AddTrafficRemovedEvent, RemoveTrafficVehicle), so no traffic car was ever removed for water.
    //
    // Straight off the ARTIST asm (DecFIGS DWARF BrnPhysicalTrafficManager.cpp:4193.. for the local
    // names: liTraffic, lbHitWater, lpTrafficVehicle, lpTrafficPhysics, lpAboveGroundTest,
    // lCollisionTag, lu8SurfaceId, lTrafficBox, lHalfExtentsWorld, lvfLowestPointWorldSpace):
    //   0x8261DE14..0x8261DE88  three null asserts (.cpp 0x1063..0x1065 == :4195..:4197)
    //   0x8261DE8C..0x8261DEF0  GetFirstNonZeroBit of mUsedTrafficVehicles (this+0x19868)
    //   0x8261DF80              lbHitWater = false (r26)
    //   0x8261DF84..0x8261DFB4  GetTrafficVehicle (h 0x2E5 assert) ; lwz 0x1C = mpVehicleBody
    //   0x8261DFB8/BC           +0x570 mAboveGroundTestResult ; lbz 0x28 mbValid == 0 -> skip
    //   0x8261DFC8..0x8261DFDC  lwz 0x24 (mCollisionTag) ; lhz of its LOW halfword ; srwi 4 ;
    //                           clrlwi 26 -> the 6-bit surface id
    //   0x8261DFE0..0x8261E000  assert gbReadSurfaceProperties (byte_82FB7DF0, .cpp 0x1086 == :4230)
    //   0x8261E00C              lbzx byte_82FB7DF4[id] == KAB_SURFACE_IS_WATER[id] ; 0 -> skip
    //   0x8261E020              GetSimpleVehicleBox(lTrafficBox)
    //   0x8261E05C..0x8261E0DC  lHalfExtentsWorld = the box rows * the half-dims (x, y, z);
    //                           lowest = w.y - |hx.y| - |hy.y| - |hz.y| (vandc with the sign mask,
    //                           then three vsubfp in that order)
    //   0x8261E0E0              stvx128 -> this + 16*(i + 0x19B5) == mavfLowestPointWorldSpace[i]
    //   0x8261E0E4..0x8261E108  vsubfp KVF_RESET_ON_WATER_HEIGHT (unk_82FB92D0 = splat 0.25) ;
    //                           vcmpgefp. splat(hit.y), that ; CR6 all-true -> lbHitWater = true
    //   0x8261E118..0x8261E15C  AddTrafficRemovedEvent(maTrafficEntityIDs[i] (this+4*(i+0x64F0)),
    //                           mePhysicalTrafficState (lwz 0x20))
    //   0x8261E160..0x8261E174  RemoveTrafficVehicle((u8)i, outReq, deform, byte_82FB7DF1) --
    //                           KB_DO_NOT_FAIL_IF_RIGID_BODY_NOT_FOUND (DWARF cpp:46), 0 in the
    //                           image with no writer (findinit); r7 is dead in the callee. The
    //                           AddTrafficRemovedEvent + RemoveTrafficVehicle pair is the inlined
    //                           SendTrafficVehicleRecycledEvents (BrnPhysicalTrafficManager_Remove.cpp).
    //   then GetNextNonZeroBit on the LIVE bit array (RemoveTrafficVehicle clears the bit).
    // =============================================================================================
    void PhysicalTrafficManager::CheckForTrafficHittingWater(
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        VehicleOutputRequestInterface* lpOutputRequestInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        CGS_ASSERT(lpManagerOutputInterface != 0, "lpManagerOutputInterface != NULL");   // :4195
        CGS_ASSERT(lpOutputRequestInterface != 0, "lpOutputRequestInterface != NULL");   // :4196
        CGS_ASSERT(lpDeformationInterface   != 0, "lpDeformationInterface != NULL");     // :4197

        for (s32 liTraffic = mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
             liTraffic = mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            bool lbHitWater = false;

            PhysicalTrafficVehicle* const lpTrafficVehicle = GetTrafficVehicle(liTraffic);
            const SimpleVehiclePhysics* const lpTrafficPhysics = lpTrafficVehicle->mpVehicleBody;
            const AboveGroundTestResult* const lpAboveGroundTest =
                lpTrafficPhysics->GetAboveGroundTestResult();

            if (lpAboveGroundTest->mbValid)
            {
                const CollisionTag lCollisionTag = lpAboveGroundTest->mCollisionTag;
                const u16 lu16Tag = static_cast<u16>(lCollisionTag.muValue & 0xFFFFu);   // lhz: the low halfword
                const u8  lu8SurfaceId =
                    static_cast<u8>((lu16Tag & BrnWorld::KU_COLLISION_MASK_SURFACE_ID) >> 4);   // srwi 4 ; clrlwi 26

                CGS_ASSERT(gbReadSurfaceProperties, "BrnPhysics::Vehicle::gbReadSurfaceProperties");   // :4230

                // 0x8261E00C `lbzx byte_82FB7DF4[id]` -- no bound test on the console, and none
                // here: the id is always < 20 on the shipped data. The tag is a WORLDCOL.BIN
                // polygon-soup poly tag (the only PolygonSoupList bundle), the traction-line job
                // hands it to SetAboveGroundTestResult, and over all 850830 polys this field
                // takes exactly the 20 values 0..19 == the surfacelist's Num_Surfaces
                // (FX-FPUMAX, tests/run_fxfpumax_water_surface.py pins the chain and the data).
                if (KAB_SURFACE_IS_WATER[lu8SurfaceId])
                {
                    CgsGeometric::Box lTrafficBox;
                    lpTrafficPhysics->GetSimpleVehicleBox(lTrafficBox);

                    const Vector3 lvHalfDimensions = lTrafficBox.GetDimensions();
                    Matrix44Affine lHalfExtentsWorld = lTrafficBox.GetTransform();
                    lHalfExtentsWorld.xAxis = lHalfExtentsWorld.xAxis * lvHalfDimensions.mV.x;
                    lHalfExtentsWorld.yAxis = lHalfExtentsWorld.yAxis * lvHalfDimensions.mV.y;
                    lHalfExtentsWorld.zAxis = lHalfExtentsWorld.zAxis * lvHalfDimensions.mV.z;

                    f32 lfLowestPointY = lTrafficBox.GetTransform().wAxis.mV.y;
                    lfLowestPointY -= std::fabs(lHalfExtentsWorld.xAxis.mV.y);
                    lfLowestPointY -= std::fabs(lHalfExtentsWorld.yAxis.mV.y);
                    lfLowestPointY -= std::fabs(lHalfExtentsWorld.zAxis.mV.y);
                    const VecFloat lvfLowestPointWorldSpace(lfLowestPointY);

                    mavfLowestPointWorldSpace[liTraffic] = lvfLowestPointWorldSpace;

                    lbHitWater = lpAboveGroundTest->mIntersectionPosition.mV.y
                                 >= lvfLowestPointWorldSpace.mV.y - KVF_RESET_ON_WATER_HEIGHT.mV.y;
                }
            }

            if (lbHitWater)
            {
                // [DIAG] BRN_TRAFFIC_DIAG -- NOT IN THE X360 BINARY. Capped removal witness.
                static s32 siDiagLinesLeft = 16;

                lpManagerOutputInterface->AddTrafficRemovedEvent(
                    maTrafficEntityIDs[liTraffic],
                    static_cast<ETrafficType>(GetTrafficVehicle(liTraffic)->mePhysicalTrafficState));
                RemoveTrafficVehicle(static_cast<u8>(liTraffic), lpOutputRequestInterface,
                                     lpDeformationInterface, KB_DO_NOT_FAIL_IF_RIGID_BODY_NOT_FOUND);
            }
        }
    }
}
}

// FOLDED FROM BrnVehicleManager_PerFrameLeaves.cpp
// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_PerFrameLeaves.cpp
//
// The four SMALL per-frame leaves of BrnPhysics::PhysicsModule::Update @0x825B0640 (home TU
// BrnVehicleManager.cpp -- still unmounted, so this is a slice TU in the established
// BrnVehicleManager_ValidateSimulationContacts.cpp pattern; fold back when the home mounts):
//
//   FreeAllocations           @0x8261BAE0  (21 insns)   DWARF h:647
//   UpdateVehicleEffects      @0x82629E18  (65 insns)   DWARF h:908
//   ReadUpdatedBodyProperties @0x825C5520  (79 insns)   DWARF h:917
//   ProcessDeformationStates  @0x825EA580  (48 insns)   DWARF h:926
//
// Each body is reconstructed from its X360 asm (IDA exports read line by line 2026-08-06 --
// NOT from the Hex-Rays, which mis-renders ReadUpdatedBodyProperties' 64-bit id compare as a
// 32-bit one). Every member access is by name; the seats are pinned by the mounted layout gate
// (_AssertLayoutTuningBank) and by the per-member notes in BrnVehicleManager.h.
//
// The streamed asserts (off_82000D00/D08 + gpcMessageBuffer + BasePriorityQueue::Clear) are
// lowered to CGS_ASSERT with the static message, per the standing project rule.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags (the boot gate, 2026-08-09)
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"               // IOBufferStack::DestroyIOBuffer<T>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h" // InChangeRigidBodyInertia (80B; mID/mInertia/mu32Flags)
#include "GameSource/Physics/BrnContactGenerationList.h"                  // BrnPhysics::ContactGenList (complete for ~T()/sizeof)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CgsCollision::CollisionGenerator (complete for ~T()/sizeof)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEffectsInputInterface.h" // the air-ram queue surface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // mpDeformationState
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h" // DeformationState::GetCarStateF
#include "GameSource/World/BrnEntityTypes.h"                              // BrnWorld::E_ENTITYTYPE_RACECAR / _TRAFFIC_VEHICLE

namespace BrnPhysics
{
namespace Vehicle
{
    // ------------------------------------------------------------------------------------
    // FreeAllocations  @0x8261BAE0  -- end-of-frame teardown of the two contact-generation
    // IO allocations, in the console's order: the generator FIRST (this+172472), then the
    // gen list (this+172468). Each DestroyIOBuffer<T> pops the allocation off the stack and
    // NULLs the member through the passed T**.
    //
    // KNOWN TEMPLATE-SHAPE DIVERGENCE, inherited (not introduced here): the committed
    // CgsIOBufferStack.h DestroyIOBuffer<T> guards a null pointer and calls ~T(), where the
    // console instantiation @0x8259DE50 asserts on null ("Must pass in pointer to pointer to
    // buffer", CgsIOBufferStack.h:180), calls T's two-phase Destruct(), and returns Free's
    // result. That template already has many live instantiation sites (BrnGameModule /
    // CgsSceneManagerModule), so its shape is NOT changed in this wave -- flagged for its own
    // pass. This body is faithful in what IT does: the two calls, in order, on the two named
    // members. (The console DestroyIOBuffer<CollisionGenerator> frees 2171904 bytes ==
    // sizeof(CgsSceneManager::CgsCollision::CollisionGenerator) on the console -- the derived
    // 2 MB generator added to CgsCollisionGenerator.h this wave.)
    // ------------------------------------------------------------------------------------
    void VehicleManager::FreeAllocations(CgsModule::IOBufferStack* lpIOBufferStack)
    {
        lpIOBufferStack->DestroyIOBuffer(&mpContactGenerator);
        lpIOBufferStack->DestroyIOBuffer(&mpContactGenList);
    }

    // ------------------------------------------------------------------------------------
    // UpdateVehicleEffects  @0x82629E18  -- drain the frame's air-ram events. Per event
    // (64-byte by-value copy, ctr=8 ld/std -- reproduced by the by-value local):
    //   owner RACECAR (1):  maRaceCarVehicles[idx].AddAirRam(...)   (asm: this + 0x740 +
    //                       idx*0x1460 == &maRaceCarVehicles[idx], the VehiclePhysics base)
    //   owner TRAFFIC (2):  mPhysicalTrafficManager.ProcessAddAirRamEvent(&event)  (+44768)
    //   anything else:      the :3544 assert, fire-and-continue.
    // The queue length is re-read from the queue every iteration (asm `lwz 8(r30)` at the
    // loop tail) -- the for-condition below does the same.
    // ------------------------------------------------------------------------------------
    void VehicleManager::UpdateVehicleEffects(const VehicleEffectsInputInterface* lpEffectsInterface)
    {
        const VehicleEffectsInputInterface::CreateAirRamEventQueue* lpQueue =
            lpEffectsInterface->GetAirRamEventQueue();   // inlined on the console (queue @ interface+0)

        for (s32 liIndex = 0; liIndex < lpQueue->GetLength(); ++liIndex)
        {
            const CreateAirRamEvent lEvent = lpQueue->GetEvent(liIndex);

            // The volume id's HIGH dword is the packed EntityId word: owner [31..24],
            // entity index [23..10] (extrwi 14,8).
            const u32 luEntityWord = static_cast<u32>(lEvent.mVolumeId.muId >> 32);
            const u32 luOwner      = luEntityWord >> 24;

            if (luOwner == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR))
            {
                const u32 luIndex = (luEntityWord >> 10) & 0x3FFFu;

                // v1 carries the raw mDirectionAndMagnitude register (w = magnitude lane);
                // GetVector3() aliases the full register, including the magnitude lane.
                const Vector3 lvDirection = lEvent.mDirectionAndMagnitude.GetVector3();
                maRaceCarVehicles[luIndex].AddAirRam(
                    lEvent.muEffectFlags,                // r4      <- event+0x08
                    lEvent.mDirectionAndMagnitude.GetPlus(),     // f1      <- event+0x1C (lfFactor)
                    lEvent.mfDecay,                      // f2      <- event+0x0C
                    lvDirection,                         // v1      <- event+0x10
                    lEvent.mPosition,                    // v2      <- event+0x20
                    lEvent.mfStartTime);                 // f3      <- event+0x30
            }
            else if (luOwner == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
            {
                mPhysicalTrafficManager.ProcessAddAirRamEvent(&lEvent);
            }
            else
            {
                CGS_FAIL_AT(
                    "Invalid Entity type in air ram UpdateVehicleEffects Effects",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
                        "../Physics/VehicleManager/BrnVehicleManager.cpp", 3544 /* ARTIST 0x82629EF0 */);   // :3544

            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ReadUpdatedBodyProperties  @0x825C5520  -- apply queued sim inertia changes back onto
    // the race-car bodies. Per event (80-byte by-value copy, ctr=10):
    //   * owner RACECAR (1) only (high dword of the 64-bit mID, owner byte, index [23..10]);
    //   * index bounds tripwire (:4321, streamed "Received invalid entity index for race
    //     car body" -> CGS_ASSERT), fire-and-continue on the console;
    //   * the FULL 64-bit event mID must equal maRaceCarHandlingBodyIDs[idx] -- the asm is
    //     `ldx` + `cmpld` (the Hex-Rays renders a 32-bit compare; the asm is authoritative);
    //   * forward to the car's ExternalPhysicsBody base (asm this + 0x750 + idx*0x1460 ==
    //     &maRaceCarVehicles[idx] + 0x10, the base sub-object -- spelled here as the derived
    //     object's member call, which the compiler seats identically).
    // ------------------------------------------------------------------------------------
    void VehicleManager::ReadUpdatedBodyProperties(
        const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia, 200>* lpInertiaQueue)
    {
        for (s32 liIndex = 0; liIndex < lpInertiaQueue->GetLength(); ++liIndex)
        {
            const CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia lEvent =
                lpInertiaQueue->GetEvent(liIndex);

            const u32 luIdHigh = static_cast<u32>(lEvent.mID >> 32);
            if ((luIdHigh >> 24) != static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR))
            {
                continue;
            }

            const u32 luIndex = (luIdHigh >> 10) & 0x3FFFu;   // extrwi 14,8
            CGS_ASSERT(luIndex < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS),
                       "Received invalid entity index for race car body");   // :4321

            if (lEvent.mID == maRaceCarHandlingBodyIDs[luIndex])
            {
                maRaceCarVehicles[luIndex].ReadPropertiesFromChangeInertiaEvent(&lEvent);
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ProcessDeformationStates  @0x825EA580  -- in the two SHOWTIME game modes, feed the
    // player car's deformation state record into its showtime bounce modifiers:
    //   * gate: meCurrentGameModeType == 2 || == 16 (asm +172380);
    //   * assert mePlayerActiveRaceCarIndex in [0, 8) (:4642), fire-and-continue (the console
    //     indexes with the out-of-range value anyway; the reads below re-load the member each
    //     time exactly as the asm does, and the arrays are indexed unguarded to match --
    //     the assert is the only tripwire the console has here, and 0 <= index < 8 holds on
    //     every live path);
    //   * GetCarStateF(mpDeformationState, maRaceCarEntityIDs[idx]) -- null means "this car
    //     has no live deformation model", not an error;
    //   * maRaceCarVehicles[idx].UpdateShowtimeBounceModifiers(record) (asm this + 0x740 +
    //     idx*0x1460, the RaceCarPhysics element itself).
    // ------------------------------------------------------------------------------------
    void VehicleManager::ProcessDeformationStates(
        const Deformation::DeformationOutputInterface* lpDeformationInterface)
    {
        if (meCurrentGameModeType == 2 || meCurrentGameModeType == 16)   // the two showtime modes
        {
            CGS_ASSERT(mePlayerActiveRaceCarIndex >= 0
                           && mePlayerActiveRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS,
                       "mePlayerActiveRaceCarIndex >= 0 && mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :4642

            const Deformation::CarStateRecord* lpCarState =
                lpDeformationInterface->mpDeformationState->GetCarStateF(
                    maRaceCarEntityIDs[mePlayerActiveRaceCarIndex].muValue);

            if (lpCarState != 0)
            {
                maRaceCarVehicles[mePlayerActiveRaceCarIndex].UpdateShowtimeBounceModifiers(lpCarState);
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // VehicleManager::ValidateTrafficContact @0x825EAC28 (PS3 DecFIGS 0x6E6178; DWARF h:941).
    // Thin forwarder: three tripwires, then the embedded traffic manager's own validation.
    // Register-truth (X360): the wrapper reads muVolumeInstanceIdA's entity word only for the
    // owner tripwire, then tail-forwards (this+44768 == mPhysicalTrafficManager).
    // ------------------------------------------------------------------------------------
    bool VehicleManager::ValidateTrafficContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpContact,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
        f32 lfTimeStep)
    {
        // ARTIST's assert lines (0x825EAC60 / 0x825EAC84 / 0x825EACB4) in the unity-built
        // BrnVehicleManager.cpp.
        CGS_ASSERT_AT(lpContact != 0, "lpContact != NULL", CGS_ARTIST_FILE, 7727);
        CGS_ASSERT_AT(lpTriCacheInterface != 0,
                   "lpTriCacheInterface != NULL", CGS_ARTIST_FILE, 7728);
        CGS_ASSERT_AT(lpContact->muVolumeInstanceIdA.GetEntityIDOwner() == 2u,
                   "lpContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE",
                       CGS_ARTIST_FILE, 7730);


        return mPhysicalTrafficManager.ValidateTrafficContact(lpContact, lpTriCacheInterface, lfTimeStep);
    }

    // ------------------------------------------------------------------------------------
    // VehicleManager::BridgeSimpleTrafficWithCarContactsToSimulation @0x825C83B0
    //
    // BOOT GATE:
    // REACHED every frame by the landed BridgeContactsToSimulation. The REAL body (375
    // X360 asm lines / 9 callees; PS3 DecFIGS 0x70DB6C) is NOT reconstructed yet -- the
    // simple-traffic car contacts are DROPPED, one log line per boot says so.
    // Reconstruct and DELETE this gate.
    //
    // ⭐ SCOPE, MEASURED 2026-08-29 (was inferred for the WORLD twin, now measured for BOTH):
    // ITS QUEUE IS EMPTY. The [bridge-queues] witness accumulated every custom queue's length
    // over 6,300 frames of a free-burn run that provably rammed traffic
    // (scratch/flow_run/q_ram/BrnGame.log, -Teleport "3390.2,0.2,-1620.0,182"):
    //     q0=108/38482  q3=24/408  q5=83/33994  q6=75/32953  q8=7/1716
    //     q9=250/297580 q13=41/4459 src=27/1739
    // -- queues [10] (simpleTraffic/world) and [11] (SIMPLE TRAFFIC/CAR, this function's) were
    // NEVER non-empty, not once. That matches the console's own allocator: there are twenty FULL
    // slots and exactly one SIMPLE one, and GetFreeTrafficVehicleWithPhysics @0x82637608 hands
    // out only E_PHYSICAL_TRAFFIC_TYPE_FULL (BrnPhysicalTrafficManager_Create.cpp:351).
    // ⇒ THIS GATE IS NOT THE "Cars Crashed" BLOCKER. Landing it would change nothing until a
    // SIMPLE traffic slot can exist. Do not spend a wave on it for that reason.
    // ------------------------------------------------------------------------------------
    void VehicleManager::BridgeSimpleTrafficWithCarContactsToSimulation(
        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact, 1024>* /*lpContactQueue*/,
        const BrnPhysics::PhysicsModuleIO::PotentialContactInterface* /*lpContactInterface*/)
    {
        static bool s_bLogged = false;
        if (!s_bLogged)
        {
            s_bLogged = true;
        }
    }
}
}

// FOLDED FROM BrnVehicleManager_Prepare.cpp
// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_Prepare.cpp
//
// The PREPARE half of BrnPhysics::Vehicle::VehicleManager -- the leg that finally puts the
// cars into the triangle cache (home TU BrnVehicleManager.cpp is still unmounted, so this is a
// slice TU in the established BrnVehicleManager_PerFrameLeaves.cpp pattern; fold back when the
// home mounts):
//
//   VehicleManager::Prepare              @0x8263C688  (75 insns)  DWARF h:~330
//   VehicleManager::PrepareTriangleCache @0x82615BA0  (37 insns)
//
// Reconstructed from the X360 ASM (read line by line 2026-08-10), not from the Hex-Rays.
//
// WHY THIS IS THE `usedSlots` LEG, stated once so no later wave re-derives it.
// TriangleCacheManager::mUsedCacheSlots -- the bitset whose popcount is the "usedSlots" probe --
// has exactly ONE setter in the whole console image: ProcessAddToCacheEvents, draining
// InSceneUpdateInterface::mAddToCacheQueue. That queue has exactly one filler on the race-car
// path, and it is PrepareTriangleCache below. PhysicsModule::UpdateCachedPositions fills a
// DIFFERENT queue (mUpdateCachedPositionQueue) whose consumer ASSERTS the used bit is already
// set ("Bit not set for event index "), so the position half can only run AFTER this one.
//
// AND WHY STAGING INTO A DOOMED BUFFER IS CORRECT HERE. WorldModule::Prepare's
// eWorldPreparePhysicsModule stage creates a scene input buffer, runs PhysicsModule::Prepare
// into it, calls UpdateScene(..., lbPrepare = TRUE) and destroys the buffer -- all in one call.
// That looks like the silent-drop shape that ate the world-collision events, and it is not:
// SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules @0x828D1F88 drains the three
// triangle-cache queues *only* when its lbPrepare argument is non-zero (`stb r7, arg_37` at
// 0x828D1FA4, re-tested at 0x828D290C / 0x828D2D44 / 0x828D388C). The Prepare-time UpdateScene
// is the one call that passes TRUE. So the console consumes these 28 events two lines after
// they are posted, by design. The PC bridge had those legs missing; they were added this wave.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                             // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"             // SceneManagerIO::InputBuffer_Update::GetInSceneUpdateInterface (@0x825BD8C0, write-lock twin)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface::mAddToCacheQueue (X360 +0xC4930)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h" // TriangleCacheManagerIO::InEventAddToCache (8 bytes)
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"             // KF_TRIANGLE_CACHE_SPHERE_RADIUS
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"       // the traffic arm of UpdateTriangleCache
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // GetHalfExtent / GetPosition
#include "rw/math/vpu/vector3_operation.h"                                     // rw::math::vpu::MagnitudeSquared
#include <cmath>                                                               // std::sqrt (the rsqrt NR chain's answer)




namespace BrnPhysics
{
namespace Vehicle
{
    // ------------------------------------------------------------------------------------
    // VehicleManager::PrepareTriangleCache  @0x82615BA0  (37 insns)
    //
    //   0x82615BB4  cmplwi r29, 0 -> assert "lpSceneInputBuffer_Update != NULL"
    //                                BrnVehicleManager.cpp:891   (li r5, 0x37B)
    //   0x82615BE0  bl  GetInSceneUpdateInterface   <- @0x825BD8C0, the NON-CONST (write-lock,
    //                                                  bit 3) twin. Correct: the caller chain
    //                                                  WorldModule::Prepare -> PhysicsModule::
    //                                                  Prepare runs under LockForWrite().
    //   0x82615BE8  addis r30, r3, 0xC ; addi r30, r30, 0x4930  -> +0xC4930 == mAddToCacheQueue
    //   0x82615BF4  lfs f0, flt_8200426C ; stfs f0, var_2C(r1)  -> radius, hoisted OUT of the loop
    //   0x82615BFC  stw r31, var_30(r1)                          -> miCacheSlot = i
    //   0x82615C08  bl  BaseEventQueue<InEventAddToCache>::AddEvent
    //   0x82615C10  cmpwi r31, 8   -> 8 iterations == KI_MAX_ACTIVE_RACE_CARS
    //   0x82615C24  bl  PhysicalTrafficManager::PrepareTriangleCache  (r3 = this + 44768)
    //   0x82615C28  li r3, 1
    //
    // The stack record is the two adjacent slots var_30/var_2C -- i.e. {s32, f32} at +0/+4,
    // which is exactly the committed InEventAddToCache (sizeof 8, X360-attested by AddEvent's
    // `slwi r11, miLength, 3`). The console hoists the radius store out of the loop because it
    // never changes; written here as a whole-record assignment per iteration, which is the same
    // source shape and the same bytes appended.
    //
    // The traffic call is `addis r3,r28,1 ; addi r3,r3,-0x5120` == this + 0x10000 - 0x5120
    // == this + 44768 == mPhysicalTrafficManager, reached BY NAME here (the console offset is
    // 44768 on X360 and 44768+drift on the host -- see this header's KU_HOST_DRIFT_* block).
    //
    // AS-SHIPPED: the return is the constant 1; there is no failure path. Reproduced because
    // Prepare tests it (`clrlwi r11, r3, 24 ; beq`).
    // ------------------------------------------------------------------------------------
    bool VehicleManager::PrepareTriangleCache(
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update)
    {
        CGS_ASSERT_AT(lpSceneInputBuffer_Update != NULL,
            "lpSceneInputBuffer_Update != NULL",
                KAC_ARTIST_ASSERT_FILE, 891 /* ARTIST 0x82615BC4 */);  // :891


        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdate =
            lpSceneInputBuffer_Update->GetInSceneUpdateInterface();

        for (s32 liRaceCar = 0; liRaceCar < KI_MAX_ACTIVE_RACE_CARS; ++liRaceCar)
        {
            CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lEvent;
            lEvent.miCacheSlot         = liRaceCar;
            lEvent.mfCacheSphereRadius = KF_TRIANGLE_CACHE_SPHERE_RADIUS;
            lpSceneUpdate->mAddToCacheQueue.AddEvent(lEvent);
        }

        mPhysicalTrafficManager.PrepareTriangleCache(lpSceneInputBuffer_Update);

        return true;
    }

    // ------------------------------------------------------------------------------------
    // VehicleManager::UpdateTriangleCache  @0x82615C38  (240 insns)
    // THE POSITION HALF, and the sibling of
    // PrepareTriangleCache above: Prepare CLAIMS the 28 slots (mAddToCacheQueue, once), this
    // MOVES them (mUpdateCachedPositionQueue, every frame). Arm 1 of
    // PhysicsModule::UpdateCachedPositions @0x8259C370.
    //
    // WHY IT IS NOT OPTIONAL: CacheSlot::mLastCachedSphere is written NOWHERE ELSE. Until this
    // runs, every claimed slot's sphere centre is (0,0,0) and the fill worker caches the geometry
    // around the world origin -- so a car at the Junkyard would be traction-tested against real,
    // valid triangles from three kilometres away.
    //
    // Read off the ASM (the Hex-Rays is the usual inlined BitArray<8> walk plus its
    // CgsBitArray.h:203 assert-formatting maze). Per live race car:
    //   0x82615D48  car = &maRaceCarVehicles[i]            (mulli 0x1460 off the manager base)
    //   0x82615D6C  lvx128 v11, car+0xDE0                  -> mHalfExtent  (+0x6A0 in the car)
    //   0x82615D70  vaddfp v0, v11, *unk_82FB91D0          -> + kvfVehicleTriangleCachePadding (1.0)
    //   0x82615D74  lvx128 v11, car+0x780                  -> mTransform.wAxis (+0x40 in the car)
    //   0x82615D7C  vrlimi128 v11, 0, 1, 0                 -> clear the w lane of that position
    //   0x82615D90..DCC  vmsum3fp128 + vrsqrtefp + 2 NR    -> radius = |halfExtent + pad|, 0 if zero
    //   0x82615DD8  stfs into the position vector's w lane -> {pos.xyz, radius}
    //   0x82615DEC  InEventUpdateCachedPosition::AddEvent, miCacheSlot = the car index
    // then 0x82615FE0 chains to the traffic pool, whose slots are 8 + liVehicle.
    //
    // THE PADDING: `unk_82FB91D0` is kvfVehicleTriangleCachePadding == splat(1.0f), written at startup
    // by CRT thunk 0x82C5A470 (see KVF_VEHICLE_TRIANGLE_CACHE_PADDING in BrnVehicleConstants.h). An
    // earlier banner here read its .bss zero as the runtime value and dropped the add, which shrank
    // every car's inner radius (2.69 instead of ~4.26 for the Cavalry), so the 5 m cache sphere
    // re-centred only after ~2.3 m of travel instead of ~0.8 m (CacheSlot::UpdateCachedObject
    // @0x828BE660 re-centres when dist + innerRadius > 5.0) and walls/kerbs ahead of a fast car
    // could be missing from world-contact generation until the next refresh.
    //
    // THE ZERO GUARD IS `== 0`, NOT `> 0`: the console selects the zero vector on
    // `vcmpeqfp128 v9, 0, lensq` (0x82615DA0) and multiplies otherwise. Kept exactly.
    // ------------------------------------------------------------------------------------
    void VehicleManager::UpdateTriangleCache(
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update)
    {
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdate =
            lpSceneInputBuffer_Update->GetInSceneUpdateInterface();

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            const RaceCarPhysics& lrCar = maRaceCarVehicles[liCar];

            const Vector3 lvHalfExtent = lrCar.GetHalfExtent();
            const Vector3 lvPadded = rw::math::vpu::Add(lvHalfExtent, Vector3(KVF_VEHICLE_TRIANGLE_CACHE_PADDING, KVF_VEHICLE_TRIANGLE_CACHE_PADDING, KVF_VEHICLE_TRIANGLE_CACHE_PADDING));
            const f32 lfRadiusSq = rw::math::vpu::MagnitudeSquared(lvPadded);               // vmsum3fp128: xyz
            const f32 lfRadius   = (lfRadiusSq != 0.0f) ? std::sqrt(lfRadiusSq) : 0.0f;

            const Vector3& lrPosition = lrCar.GetPosition();

            CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition lEvent;
            lEvent.miCacheSlot                  = liCar;
            lEvent.mNewPositionAndRadius.SetVector3(lrPosition);
            lEvent.mNewPositionAndRadius.SetPlus(lfRadius);
            lpSceneUpdate->mUpdateCachedPositionQueue.AddEvent(lEvent);
        }

        mPhysicalTrafficManager.UpdateTriangleCache(lpSceneInputBuffer_Update);
    }

    // ------------------------------------------------------------------------------------
    // VehicleManager::Prepare  @0x8263C688  (75 insns)
    //
    // A resumable three-stage fall-through FSM over mePrepareStage, the same shape as
    // PhysicsModule::Prepare's ten-stage one. Each arm stamps its stage number FIRST so a
    // `false` return leaves the cursor exactly where the work stopped and the world spine
    // re-enters next frame. The jump table at 0x8263C718 covers cases 0..3 with 0 and 1
    // sharing an arm; anything above 3 is the "Invalid prepare stage\n" default.
    //
    //   :815 (0x32F)  assert lpPhysicsAllocator != NULL
    //   :816 (0x330)  assert lpSceneInputBuffer_Update != NULL
    //   case 0/1: mePrepareStage = 1; if (!PrepareData(lpPhysicsAllocator)) return false;
    //   case 2:   mePrepareStage = 2; if (!PrepareTriangleCache(lpScene))   return false;
    //   case 3:   meReleaseStage = 0;                 (stw r11=0, 4(r31))
    //             mePrepareStage = 3;                 (stw r10=3, 0(r31))
    //             mn8RoundRobinControlWord = 0;       (stbx r11=0, r31, 0x2A1B0 == +172464)
    //             mbTrafficCheckingAllowed = true;    (stbx r29=1, r31, 0x2A119 == +172313)
    //             return true;
    //   default:  assert "Invalid prepare stage\n"  :867 (0x363);  return false;
    //
    // Both tail bytes are `stbx` (BYTE stores), not words -- read off the asm, and both land
    // on members this header already maps by name, so neither console literal appears below.
    // Unlike PhysicsModule::Prepare, this one does NOT rewind its own cursor on success: it
    // leaves mePrepareStage at 3, so a second call re-enters case 3 and re-stamps the same two
    // bytes. Faithful; noted because it differs from its sibling FSM.
    //
    // PrepareData (BrnVehicleManager_PrepareData.cpp) always returns true, so the stage-1 arm
    // never stalls.
    // ------------------------------------------------------------------------------------
    bool VehicleManager::Prepare(rw::IResourceAllocator* lpPhysicsAllocator,
                                 CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer)
    {
        CGS_ASSERT_AT(lpPhysicsAllocator != NULL,
            "lpPhysicsAllocator != NULL",
                KAC_ARTIST_ASSERT_FILE, 815 /* ARTIST 0x8263C6B8 */);                 // :815

        CGS_ASSERT_AT(lpSceneInputBuffer != NULL,
            "lpSceneInputBuffer_Update != NULL",
                KAC_ARTIST_ASSERT_FILE, 816 /* ARTIST 0x8263C6DC */);          // :816


        switch (mePrepareStage)
        {
        default:
            CGS_FAIL_AT(
                "Invalid prepare stage\n",
                    KAC_ARTIST_ASSERT_FILE, 867 /* ARTIST 0x8263C7A0 */);                                     // :867

            return false;

        case 0:
        case 1:
            mePrepareStage = 1;
            if (!PrepareData(lpPhysicsAllocator))
            {
                return false;
            }
            // fall through

        case 2:
            mePrepareStage = 2;
            if (!PrepareTriangleCache(lpSceneInputBuffer))
            {
                return false;
            }
            // fall through

        case 3:
            meReleaseStage           = 0;
            mePrepareStage           = 3;
            mn8RoundRobinControlWord = 0;
            mbTrafficCheckingAllowed = true;
            return true;
        }
    }
}
}

// FOLDED FROM BrnVehicleManager_ProcessContactSpies.cpp
// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ProcessContactSpies.cpp
//
// THE RACE-CAR CONTACT-SPY DRIVER -- the function PhysicsModule::Update runs, on the non-catchup
// leg, over the frame's resolved race-car contacts. This is STAGE 0 of the takedown chain:
// every race-car-vs-race-car spy is handed to HandleRaceCarRaceCarContact (BrnVehicleManager.cpp),
// which is the only road to SetRaceCarCrashing -> AddRaceCarCrashEvent.
//
//   VehicleManager::ProcessContactSpies  @0x82646C98  (118 insns; DWARF BrnVehicleManager.cpp:4434)
//
// Slice TU (home BrnVehicleManager.cpp is mounted; this body lives in its own partfile like the
// other per-frame legs). The conductor gate that had stood for it was deleted in the same change
// (LNK2005 is the tripwire if it ever reappears).
//
// -------------------------------------------------------------------------------------------------
// THE WALK, verbatim (r28 == this, r20 == lpContactSpies, f30 == lfTimeStep):
//
//   0x82646CD0  stwx 0, this+0x2A244          -> muTakedownEventsThisFrame = 0   (+172612)
//   0x82646CD8  lwz  r19, 8(r20)              -> liNumContacts = mRaceCarContactQueue.GetLength()
//                                                (the queue is ContactSpyData's FIRST member, so
//                                                the queue pointer IS lpContactSpies; +8 is the
//                                                BaseEventQueue length word)
//   per contact i (0x82646D08..0x82646E44):
//     0x82646D10  BaseEventQueue<RaceCarContact>::GetEvent(i)   (stride 96, the 0x82285BB8 leaf)
//     0x82646D18  lbz 0(spy) == 1  else assert
//                 "lSpy.mEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"        (:4457)
//                 (the owner is the TOP byte of the big-endian EntityId word -> GetOwner())
//     0x82646D3C  luOwnerB = lbz 4(spy)        -> mEntityIdB.GetOwner()
//     0x82646D44  if (luOwnerB == E_ENTITYTYPE_RACECAR)
//     0x82646D8C      HandleRaceCarRaceCarContact(lSpy BY VALUE -- 7 doublewords in r4..r10 +
//                     a 40-byte memcpy of +0x38.. onto the stack == the 96-byte RaceCarContact --
//                     lpRequest, lpVehicle, lpManager, lpDeformation, lfTimeStep)
//     0x82646D90  lpCarA = &maRaceCarVehicles[mEntityIdA.GetEntityIndex()]
//                 (`extrwi 14,8` == (id >> 10) & 0x3FFF; stride 0x1460, base +0x740)
//     0x82646DA4  if (lpCarA->mbCrashing (+0x710)              -> IsCrashing()
//     0x82646DB0      && !lpCarA->mbStartedFatallyCrashing (+0x711) -> !IsFatallyCrashing()
//     0x82646DC4      && lwz 4(spy) == lpCarA+0x1450)          -> mEntityIdB == GetEntityCausingCrash()
//                   (a WHOLE-WORD compare of the EntityId; the earlier lbz was its owner byte)
//     0x82646DCC      if (luOwnerB == RACECAR || luOwnerB == TRAFFIC_VEHICLE)    -> set (below)
//     0x82646DE4      else n = lpCarA->GetNormalCausingCrash()          (asserts mbCrashing itself)
//     0x82646DE8..E0C   vspltisw -1 ; vslw ; vxor  == flip the sign bit of every lane == -n
//     0x82646DF4        lvx128 spy+0x30                                  -> lSpy.mNormal
//     0x82646E18        vmsum3fp128                                      -> vpu::Dot(mNormal, -n)
//     0x82646E24        vcmpgtfp. against the 0.7f splat (flt_82004C68)  -> operator>
//     0x82646E38      stb 1, 0x711(lpCarA)                               -> SetFatallyCrashing()
//   0x82646E50  ProcessShowtimeShunts(lpContactSpies)                     @0x82629F20
//   0x82646E5C  mPhysicalTrafficManager.DisposeOfNonCrashingTraffic()     @0x825EFB40 (this+44768)
//
// The DWARF variable hints for :4434 name exactly this set: lpRaceCarContactSpyQueue,
// liNumContacts, i, lSpy, luOwnerB, lpCarA, and the callees BaseEventQueue<RaceCarContact>::
// GetEvent, EntityId::operator u32, RaceCarPhysics::GetNormalCausingCrash, vpu::Dot,
// vpu::operator>, vpu::operator-, SimpleVehiclePhysics::SetFatallyCrashing.
//
// POLARITY NOTES (read twice, this runs every frame):
//   * The fatal-crash latch is raised only for the car ALREADY crashing (mbCrashing set) whose
//     recorded crash-causer is the other party of THIS contact. Any car or traffic causer
//     latches at once; anything else (world, prop) latches only when the contact normal is
//     within ~45 degrees of the OPPOSITE of the stored crash normal (dot > 0.7).
//   * HandleRaceCarRaceCarContact runs BEFORE that check, so a crash it just committed via
//     SetRaceCarCrashing is visible to the latch on the same contact -- console order.
//   * There is NO range check on the decoded race-car index (the console has none either; the
//     assert on the owner byte is the only guard, and it is not a guard).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // IsCrashing / IsFatallyCrashing / GetEntityCausingCrash / GetNormalCausingCrash / SetFatallyCrashing
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"       // DisposeOfNonCrashingTraffic (the tail call)
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"                  // ContactSpyData::GetRaceCarContacts / RaceCarContactQueue
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"                // RaceCarContact (mEntityIdA/B, mNormal)
#include "GameSource/World/BrnEntityTypes.h"                                    // BrnWorld::E_ENTITYTYPE_RACECAR / _TRAFFIC_VEHICLE
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                       // gpDebugPrint ([td-spies] witness)
#include "rw/math/vpu/vector3_operation.h"                                      // vpu::Dot, unary operator-

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    namespace
    {
        // flt_82004C68 == 0.7f (read from the image). The cosine threshold between the incoming
        // contact normal and the NEGATED stored crash normal that promotes a world/prop contact
        // to a fatal crash. FLAG: constant NAME proposed (no DWARF symbol for the literal); the
        // value and its use are asm-literal.
        const f32 KF_FATAL_CRASH_NORMAL_DOT_THRESHOLD = 0.7f;
    }

    void VehicleManager::ProcessContactSpies(const ContactSpy::ContactSpyData* lpContactSpies,
                                             BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                             VehicleOutputInterface* lpVehicleOutputInterface,
                                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                                             BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                             Deformation::DeformationManager* lpDeformationManager,
                                             f32 lfTimeStep)
    {
        (void)lpDeformationManager;   // asm: r9 (the manager) is never read in the 118 instructions.

        //
        // ⭐ WHAT ownerB == 3 ON EVERY ENTRY MEANS (takedown lane S1, 2026-09-13): it is the
        // FAITHFUL reading of the only race-car contact this tree currently produces, NOT a
        // mis-encoded id, and no owner byte anywhere on the store path needs repairing.
        //   * A prop-vs-race-car scene contact is routed onto the shared dummy-car body before it
        //     enters the simulation -- the race-car side's owner byte is restamped
        //     PROP_COLLISION_RACECAR -- so the resolved spy's A owner is 11 and the physics store's
        //     race-car arm fires. The ids the stored record carries come from the POTENTIAL
        //     contact, which still holds the untouched scene pair (RACECAR, PROP). Hence A == 1,
        //     B == 3, on every entry.
        //   * A car-vs-car BODY contact never travels that road. The contact-generation router
        //     canonicalises every race-car-vs-race-car overlap pair into the race-car-with-race-car
        //     custom queue, and that queue has exactly two drains: the interpolation fix-up (which
        //     mutates the record in place) and the deformation-sensor read (whose simulation-input
        //     parameter is genuinely unused on the console). Neither adds a simulation contact, so
        //     no spy can come back naming two race cars.
        // ⇒ The HandleRaceCarRaceCarContact arm below is waiting on a PRODUCER that does not exist
        //   in this tree yet, not on a bad owner byte in the spy record. Re-read the two inert
        //   deformation bridges (body-part-with-car and detached-wheel-with-car) before blaming
        //   anything on this file, the spy factory, or the deformation contact fix-ups -- all four
        //   were re-verified store-for-store against the console this wave and are faithful.
        // -------------------------------------------------------------------------------------
        static u32 suSpyCalls               = 0u;
        static u32 suSpyNonEmptySeen        = 0u;
        static u32 suSpyContactsTotal       = 0u;
        // One bucket per BrnWorld::EEntityTypeID value 0..12 plus a "13 or above" catch-all: the
        // sim-side race car arrives as PROP_COLLISION_RACECAR (11) while the arm below tests only
        // RACECAR (1), so 11 must be readable on its own and never folded in with the wheel and
        // deformable-part owners (8..10).
        static u32 sauSpyOwnerBHistogram[14] =
            { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };   // [13] == "13 or above"
        ++suSpyCalls;

        muTakedownEventsThisFrame = 0;                                          // 0x82646CD0

        const ContactSpy::ContactSpyData::RaceCarContactQueue* lpRaceCarContactSpyQueue =
            lpContactSpies->GetRaceCarContacts();
        const s32 liNumContacts = lpRaceCarContactSpyQueue->GetLength();        // 0x82646CD8

        for (s32 i = 0; i < liNumContacts; ++i)
        {
            const ContactSpy::RaceCarContact& lSpy = lpRaceCarContactSpyQueue->GetEvent(i);   // 0x82646D10

            // EntityId here is the physics-side `{ u32 muValue; }` word (BrnCommonTypes.h), so the
            // owner/index reads are spelled on the word exactly as HandleRaceCarRaceCarContact does:
            // owner == the top byte (`>> 24`), index == bits 10..23 (`(>> 10) & 0x3FFF`).
            CGS_ASSERT(static_cast<u8>(lSpy.mEntityIdA.muValue >> 24) == BrnWorld::E_ENTITYTYPE_RACECAR,
                       "lSpy.mEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :4457

            const u8 luOwnerB = static_cast<u8>(lSpy.mEntityIdB.muValue >> 24);   // 0x82646D3C lbz 4(spy)

            ++suSpyContactsTotal;
            ++sauSpyOwnerBHistogram[(luOwnerB < 13u) ? luOwnerB : 13u];

            if (luOwnerB == BrnWorld::E_ENTITYTYPE_RACECAR)                     // 0x82646D44
            {
                HandleRaceCarRaceCarContact(lSpy,                               // 0x82646D8C (by value)
                                            lpRequestOutputInterface,
                                            lpVehicleOutputInterface,
                                            lpManagerOutputInterface,
                                            lpDeformationInterface,
                                            lfTimeStep);
            }

            // Does this contact confirm the crash car A is already in? If the other party is the
            // recorded crash-causer, the crash becomes fatal -- immediately for a car or a traffic
            // causer, else only when the contact comes from the opposite side of the stored normal.
            RaceCarPhysics* lpCarA = &maRaceCarVehicles[(lSpy.mEntityIdA.muValue >> 10) & 0x3FFF];   // 0x82646D90 extrwi 14,8
            if (lpCarA->IsCrashing()                                            // 0x82646DA4  +0x710
                && !lpCarA->IsFatallyCrashing()                                 // 0x82646DB0  +0x711
                && lSpy.mEntityIdB.muValue == lpCarA->GetEntityCausingCrash().muValue)   // 0x82646DC4  +0x1450 (whole word)
            {
                if (luOwnerB == BrnWorld::E_ENTITYTYPE_RACECAR                  // 0x82646DCC
                    || luOwnerB == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE       // 0x82646DD4
                    || vpu::Dot(lSpy.mNormal, -lpCarA->GetNormalCausingCrash()) // 0x82646DE4..E18
                           > KF_FATAL_CRASH_NORMAL_DOT_THRESHOLD)               // 0x82646E24 vcmpgtfp
                {
                    lpCarA->SetFatallyCrashing();                               // 0x82646E38 stb 1,0x711
                }
            }
        }

        if (CgsDev::Log::gpDebugPrint != 0
            && ((liNumContacts > 0 && suSpyNonEmptySeen < 8u) || (suSpyCalls % 600u) == 0u))
        {
            if (liNumContacts > 0)
            {
                ++suSpyNonEmptySeen;
            }

            *CgsDev::Log::gpDebugPrint << "[td-spies] calls=" << suSpyCalls
                                       << " queueLen=" << liNumContacts
                                       << " contactsTotal=" << suSpyContactsTotal
                                       << " ownerB[0..12,13+]=";
            for (s32 liBucket = 0; liBucket < 14; ++liBucket)
            {
                *CgsDev::Log::gpDebugPrint << sauSpyOwnerBHistogram[liBucket];
                if (liBucket < 13)
                {
                    *CgsDev::Log::gpDebugPrint << ",";
                }
            }
        }

        ProcessShowtimeShunts(lpContactSpies);                                  // 0x82646E50 @0x82629F20
        mPhysicalTrafficManager.DisposeOfNonCrashingTraffic();                  // 0x82646E5C @0x825EFB40
    }

} // namespace Vehicle
} // namespace BrnPhysics

// FOLDED FROM BrnVehicleManager_ProcessCreateEvents.cpp
// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ProcessCreateEvents.cpp
//
// VehicleManager::ProcessCreateEvents @0x82616770 (1,067 insns) -- THE CREATE DRAIN. The only
// writer anywhere in the XEX that SETS a bit in mUsedRaceCars (the tree's only other write is
// BrnVehicleManager_Construct.cpp's UnSetAll()).
//
// NEW SLICE TU, 2026-08-11. Home BrnVehicleManager.cpp is still unmounted; its four sibling arms
// live in BrnVehicleManager_MaintenanceEvents.cpp. This one is split out for a reason that is NOT
// file size -- see the mounting block below.
//
// =================================================================================================
// MOUNTING: THIS TU **IS** IN tools/build/build_game_exe.bat as of the boot-verified wave
//     (the `echo "%SRC%\GameSource\Physics\VehicleManager\BrnVehicleManager_ProcessCreateEvents.cpp"`
//     line, with the conductor's rationale in the rem block above it). This banner used to say the
//     opposite -- "**NOT** IN build_game_exe.bat ... the reason is RUNTIME, not COMPILE" -- and that
//     text is retired here; the history below is kept because it is what the mount decision turned on.
//
// It compiles because the concurrent Prepare-chain wave landed the seat while this one was being
// written: RaceCarPhysics::Prepare @0x82639CB8 (42 insns) is declared at RaceCarPhysics.h:167 and
// bodied in the MOUNTED RaceCarPhysics.cpp, and the VehiclePhysics::Prepare it forwards into is
// mounted too. (The commented-out declaration at RaceCarPhysics.h:135 is the retired park block,
// not the live declaration -- do not read it as the current state, as this banner briefly did.)
//
// WHY THE MOUNT WAS HELD BACK, AND WHAT IT TURNS ON NOW THAT IT IS LANDED.
// ProcessVehicleMaintenanceEvents IS LIVE on this build (PhysicsModule::PostSceneUpdate reaches it
// every frame -- a read-only probe recorded three real drains on one boot, naming race-car slots
// 0, 1 and 2). Setting a bit in mUsedRaceCars switches on FOUR already-mounted, already-called
// per-frame loops at once, and as of this wave they ARE switched on:
//     BrnVehicleManager_ReadUpdatedBodies.cpp:100     gravity + ExternalPhysicsBody::IntegrateTransform
//     BrnVehicleManager_UpdateVehiclePhysics.cpp:383  ResetAboveGroundTestResult per car
//     BrnVehicleManager_UpdateVehiclePhysics.cpp:473  RaceCarPhysics::Update -- the 54 force leaves
//     BrnVehicleManagerContactGeneration.cpp:340      vehicle contact generation
// The same probe measured `frozen=0` and `simpleValid=0` -- the car is not frozen and its attribs
// are not seated -- so the FIRST frame after the bit goes up runs all four against a car this
// function has just constructed. That is exactly why the mount waited for a commit whose gated run
// is BOOTED AND CHECKED rather than for a green compile; this is that commit.
//
// ⇒ There is no half-mount: the bit-set and the seat are one atomic leg, and sub-gating either is
//   the compensating-pair shape this project has paid for repeatedly. Do not re-introduce a gate.
//
// =================================================================================================
// WHAT THE BODY IS, LEG BY LEG (asm @0x82616770..0x82617818; the whole loop is one iteration
// per event over mCreateRaceCarEventQueue, input + 128032).
//
//   head     copy the 0xA0-byte event to a local (four VMX matrix rows, two vectors, the scalar
//            tail), latch both resource handles into maRaceCar{,Graphics}ModelHandles[slot], four
//            asserts (:1303 owner, :1304 bound, "Race Car Index Already Used", model handle).
//   player   if the slot is the local player's, raise E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET.
//   attribs  burnoutcarasset(event.mCarAssetAttribKey) -> its +0x158 handling RefSpec ->
//            physicsvehiclehandling -> VehicleAttribs::Construct + SetupAttribs; then latch the
//            asset key into VehicleAttribs::mAttribsKey (+0x358).
//   wheels   TransformToNewCOMSpace(COM), then -- SKIPPED ENTIRELY when
//            event.mbDisablePhysicsStateReset -- a four-iteration walk over the spec's WheelSpecs
//            capturing position + radius and accumulating the four-wheel mean into the COM.
//   inertia  ~200 insns of VMX Newton-Raphson that are, exactly, the solid-box inertia tensor.
//            Two Inertia values fall out of it: one for the SIMULATION and one for the seat.
//   sim      InAddRigidBody post (192-byte event, memcpy 0xC0 on the console).
//   seat     GetBoundingBox -> the vcall at slot +0x30 -> RaceCarPhysics::Prepare. Also skipped
//            when event.mbDisablePhysicsStateReset.
//   register DebugComponent::Register once per slot, then the INLINED mUsedRaceCars.SetBit --
//            the observable this whole campaign is named for -- plus the entity-id and
//            handling-body-id latches.
//   publish  CreateVehicleResult, AddRaceCarDeformationModel, the race-car type, the driver-control
//            clear, the log, and the network/player hide arms.
//
// =================================================================================================
// THE INERTIA BLOCK IS NOT "~200 INSNS OF VMX" ANY MORE -- IT IS THE SOLID-BOX TENSOR, and
// every constant in it was READ OUT OF THE IMAGE (targeted headless idat over the ARTIST .i64),
// not inferred from role:
//     flt_82001CC0 = 0.0f      flt_82001C98 = 1.0f       flt_82001D9C = 2.0f
//     flt_82001DA0 = 0.5f      flt_8208FA0C = 4.0f       flt_82094724 = 0.0833333358f (== 1/12)
//     flt_82004884 = 1.0e-5f   flt_82004F5C = 30.0f      stru_8208F630 = 120000.0f
//     flt_82004014 = 0.1f      flt_82009B84 = 1.2f
// With those in hand the block reads straight:
//     dims          = 2 * (spec->mHandlingBodyDimensions + |attribs.mCOMOffset|)   (vandc = abs)
//     invInertia.x  = 1 / ( (mass/12) * (dims.y^2 + dims.z^2) )
//     invInertia.y  = 1 / ( (mass/12) * (dims.x^2 + dims.z^2) )
//     invInertia.z  = 1 / ( (mass/12) * (dims.x^2 + dims.y^2) )
// i.e. I = m/12 * (a^2 + b^2) per axis -- the textbook solid cuboid. The `vrefp` + two
// `vnmsubfp/vmaddfp` pairs around every one of those divides are the compiler's Newton-Raphson
// expansion of a float divide; they are DE-OPTIMISED back to `/` here, per the standing rule.
//
// AND THE 48-BYTE BLOCK IT BUILDS IS EXACTLY `rw::physics::Inertia`, FIELD FOR FIELD.
// The RaceCarPhysics.h banner says "`rw::physics::Inertia` has NO type in this tree yet ... the
// parameter is spelled in the declaration below only when that type lands". THAT IS STALE: the
// type has had a committed home since task #141 (vendor/renderware/include/rw/physics/inertia.h),
// complete with a compiled seven-member layout gate. Laying the console's stack block onto it:
//     +0x00 (16B)  the three reciprocals          -> mInvTens
//     +0x10        1 / mass                       -> mInvMass
//     +0x14        1 / min(the three reciprocals) -> mSpherical
//     +0x18        120000.0f                      -> mMaxVelocity
//     +0x1C        30.0f                          -> mMaxOmega
//     +0x20        1.0e-5f                        -> mLinearDrag
//     +0x24        1.0e-5f                        -> mAngularDrag
//     +0x28 (8B)   NEVER WRITTEN                  -> the tail padding that header documents
// and the +0x14 store is, instruction for instruction, `SetInverseInertia`'s own documented body
// (`fcmpu/blt/fmr` min-of-three then `fdivs 1.0f`). Seven of seven, including the hole.
//
// TWO Inertia VALUES, NOT ONE, and they differ in three fields. The console builds the block
// once, copies it to a second stack slot, and then OVERWRITES three of the copy's fields before
// handing that copy to the SIMULATION -- while the ORIGINAL goes to Prepare:
//                        to Prepare (var_920)     to InAddRigidBody (var_600)
//     mInvTens           invInertia               invInertia * 1.2f
//     mSpherical         1 / min(invInertia)      1 / min(invInertia * 1.2f)   (recomputed)
//     mLinearDrag        1.0e-5f                  0.1f
//     mAngularDrag       1.0e-5f                  0.1f
// Collapsing them into one value would be a silent physics change; both are reproduced.
//
// =================================================================================================
// ⛔⛔ AND NEITHER OF THEM IS THE TENSOR THE CAR ACTUALLY SPINS ON. SETTLED 2026-09-05 (crash wave)
// -- read this before "fixing" the |mCOMOffset| term below, because a previous wave called that
// difference "the strongest remaining lead" for the car flying and it is NOT a defect.
//
// The crash probe prints the live tensor on the entry frame of every banked run:
//     Iinv=(0.000307, 0.000273, 0.001315)   com=(0.000023, -0.403568, -0.081302)   mass=1589
// Recomputed from PUSMC01's SHIPPED bytes (mHandlingBodyDimensions == 1.05081 / 0.57608 / 2.41295,
// via tools/assets/bundles/vehicle_geometry_audit.py --car PUSMC01), the two candidate formulas are
//     with |mCOMOffset|:  dims 2.101666/1.959296/4.988504  ->  Iinv 2.629e-4 / 2.577e-4 / 9.147e-4
//     without:            dims 2.10162 /1.15216 /4.8259    ->  Iinv 3.068e-4 / 2.726e-4 / 1.3147e-3
// The live values are the SECOND, three lanes for three, to every printed digit. So the |COMOffset|
// term is genuinely absent from the tensor in play -- and that is CORRECT, because the tensor in
// play is not built here at all:
//   * the Inertia this function builds reaches RaceCarPhysics::Prepare @0x82639CB8 by value and is
//     NEVER READ there (r6..r10 are reloaded with the three trailing pointers at 0x82639CC4..D4
//     before the `bl VehiclePhysics::Prepare`; see RaceCarPhysics.cpp's banner), and
//   * its other copy goes to InAddRigidBody, whose only consumer is the deliberately inert
//     PhysicsModule gate (see the post below).
//   * What seats mLocalInverseInertia is SimpleVehiclePhysics::SetAttributes @0x826020A0, reached
//     Prepare -> SimpleVehiclePhysics::Prepare -> SetAttributes, and it builds the box from
//     mHalfExtent ALONE. Read out of the image this wave:
//         0x8260224C  li r8, 0x6A0            ; mHalfExtent
//         0x82602270  lvx128 v13, r26, r8
//         0x82602280  vmulfp128 v13, v13, v12 ; v12 == splat(flt_82001D9C == 2.0)
//     then the same m/12 * (a^2+b^2) reciprocal ladder into +0x80/+0x90/+0xA0. There is no
//     `vaddfp` of an absolute-valued CoM anywhere in those 503 instructions.
// ⇒ TWO DIFFERENT CONSOLE FUNCTIONS BUILD TWO DIFFERENT TENSORS FOR TWO DIFFERENT BODIES. The
//   "the car pitches 17% and rolls 44% faster than the formula says" reading compares this
//   function's tensor against the OTHER function's body. There is nothing here to correct, and
//   changing either formula would be a tune, not a decompilation.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"   // mCreateRaceCarEventQueue
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"  // the request + manager-output interfaces
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"                // the +0x158 handling RefSpec
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"         // GetBoundingBox out-param
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // DebugComponent::Register
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/physics/inertia.h"                                                    // rw::physics::Inertia
#include "rw/physics/rigidbody.h"                                                  // rw::physics::ACTIVE_BODY
#include "rw/math/vpu/vector3_operation.h"                                         // Add / Mult

namespace
{
    // The image constants this body reads (see the banner). Named rather than inlined so a
    // reviewer can check each against the dump, and so none of them can be mistaken for a guess.
    const f32 KF_TWO                 = 2.0f;        // flt_82001D9C
    const f32 KF_HALF                = 0.5f;        // flt_82001DA0
    const f32 KF_NUM_WHEELS          = 4.0f;        // flt_8208FA0C -- the four-wheel mean divisor
    const f32 KF_BOX_INERTIA_COEFF   = 1.0f / 12.0f;// flt_82094724 (0.0833333358f)
    const f32 KF_SEAT_DRAG           = 1.0e-5f;     // flt_82004884 -- linear AND angular, at the seat
    const f32 KF_SIM_DRAG            = 0.1f;        // flt_82004014 -- linear AND angular, in the sim
    const f32 KF_MAX_LINEAR_VELOCITY = 120000.0f;   // stru_8208F630
    const f32 KF_MAX_ANGULAR_VELOCITY= 30.0f;       // flt_82004F5C
    const f32 KF_SIM_INERTIA_SCALE   = 1.2f;        // flt_82009B84 -- the sim's softer tensor

    inline Vector3 AbsPerLane(Vector3 lvValue)
    {
        const __vector4 allBits = __vspltisw(-1);
        return Vector3(__vandc(lvValue.mV, __vslw(allBits, allBits)));
    }

}

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::ProcessCreateEvents(const VehicleInputInterface* lpInputInterface,
                                             VehicleOutputRequestInterface* lpOutputInterface,
                                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                                             Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::CreateRaceCarEventQueue* lpQueue =
            lpInputInterface->GetCreateRaceCarEventQueue();

        // The bound is re-read every iteration (`lwz r10,8(r10)` at 0x826177F4). Live test.
        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            // The console copies the whole 0xA0-byte record to the stack before touching it
            // (0x826169B4..0x82616A58) and then reads only the copy. Reproduced by value.
            const CreateRaceCarEvent lEvent = lpQueue->GetEvent(liEvent);

            const u32 luEntityWord = static_cast<u32>(lEvent.mVolumeInstanceID.muId >> 32);
            const u32 luOwner      = luEntityWord >> 24;
            const u32 luRaceCar    = lEvent.mVolumeInstanceID.GetEntityIDEntityIndex();

            // `stdx r11,r28,r24` / `stdx r10,r9,r24` -- the two latches happen BEFORE the asserts.
            maRaceCarModelHandles[luRaceCar]         = lEvent.mModelHandle;
            maRaceCarGraphicsModelHandles[luRaceCar] = lEvent.mGraphicsHandle;

            CGS_ASSERT(luOwner == 1u,
                "lEvent.mVolumeInstanceID.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :1303
            CGS_ASSERT(luRaceCar < 8u, "leActiveRaceCarIndex < ku8MaxNumRaceCars");               // :1304
            CGS_ASSERT(!mUsedRaceCars.IsBitSet(luRaceCar), "Race Car Index Already Used");
            CGS_ASSERT(lEvent.mModelHandle.mpResourceMemory != 0,
                       "lEvent.mModelHandle.GetResource() != NULL");

            // ---- the player arm -----------------------------------------------------------
            // `lwz r11,[this+172204] ; cmpw r27,r11 ; bne` then the OR of 0x20 into the stunt
            // manager's +0x28. Both reached by name.
            if (static_cast<s32>(luRaceCar) == static_cast<s32>(mePlayerActiveRaceCarIndex))
            {
                mStuntOffencesManager.SetCurrentRaceCarState(E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET);
            }

            // ---- the AttribSys chase ------------------------------------------------------
            // Each generated ctor below IS the console's inlined sequence, exactly as the
            // BrnSimpleVehiclePhysics.cpp:786 precedent spells it:
            //   Attrib::FindCollection(hash64("burnoutcarasset"), event.mCarAssetAttribKey)
            //     -> Attrib::Instance -> DefaultDataArea(0x228) when the collection has no layout
            //     -> RefSpec at +0x158 -> RefSpec::GetCollection -> physicsvehiclehandling
            VehicleAttribs lVehicleAttribs;
            {
                Attrib::Gen::burnoutcarasset lCarAsset(lEvent.mCarAssetAttribKey, NULL);
                Attrib::Gen::physicsvehiclehandling lHandling(
                    const_cast<Attrib::Collection*>(
                        lCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection()), NULL);

                lVehicleAttribs.Construct();
                // The checked copy @0x825BDB88 -- the console's by-value argument.
                Attrib::Gen::physicsvehiclehandling lHandlingCopy(lHandling);
                lVehicleAttribs.SetupAttribs(lHandlingCopy);
            }
            // `ld r10,var_860 ; std r10,var_148` == VehicleAttribs + 0x358 (mAttribsKey).
            lVehicleAttribs.mAttribsKey = lEvent.mCarAssetAttribKey;

            // ---- the deformation spec -----------------------------------------------------
            // `lwzx r11,r28,r24 ; lwz r28,0(r11)` -- the SafeResourceHandle double deref
            // (mpResourceMemory, then the SmallResource's main-memory pointer).
            CGS_ASSERT(maRaceCarModelHandles[luRaceCar].mpResourceMemory != 0,
                       "maRaceCarModelHandles[leActiveRaceCarIndex].GetResource() != NULL");
            Deformation::StreamedDeformationSpec* lpSpec =
                *reinterpret_cast<Deformation::StreamedDeformationSpec* const*>(
                     maRaceCarModelHandles[luRaceCar].mpResourceMemory);

            lpSpec->TransformToNewCOMSpace(lVehicleAttribs.mBaseAttribs.mCOMOffset);
            CGS_ASSERT(lpSpec != 0, "lpDeformationModel != NULL");

            // ---- the wheel walk (skipped when the event disables the physics-state reset) ---
            Vector3 lvHandlingBodyOffset;
            lvHandlingBodyOffset.mV.v[0] = 0.0f; lvHandlingBodyOffset.mV.v[1] = 0.0f;
            lvHandlingBodyOffset.mV.v[2] = 0.0f; lvHandlingBodyOffset.mV.v[3] = 0.0f;   // `vmr128 v125,v123(0)`

            Vector3 lavWheelPositions[4];
            f32     lafWheelRadii[4];

            if (!lEvent.mbDisablePhysicsStateReset)   // `lbz r11,var_83C ; cmplwi 0 ; bne`
            {
                for (s32 liWheel = 0; liWheel < 4; ++liWheel)
                {
                    // The console emits StreamedDeformationSpec::GetWheelSpec's own bound
                    // tripwire twice per iteration (once per member read), inlined.
                    CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");
                    lavWheelPositions[liWheel] = lpSpec->maWheelSpecs[liWheel].mPosition;

                    lvHandlingBodyOffset =
                        rw::math::vpu::Add(lvHandlingBodyOffset, lavWheelPositions[liWheel]);

                    CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");
                    // `vspltw v13,v13,1` == lane .y of maWheelSpecs[i].mScale, halved.
                    lafWheelRadii[liWheel] = KF_HALF * lpSpec->maWheelSpecs[liWheel].mScale.mV.v[1];
                }

                // `v125 = v125 * (1/4)` -- the VMX reciprocal expansion of a divide by 4.0f.
                lvHandlingBodyOffset = lvHandlingBodyOffset / KF_NUM_WHEELS;

                // `vaddfp128 v0,v13,v125 ; stvx128 v0,r0,&attribs+0x20`
                lVehicleAttribs.mBaseAttribs.mCOMOffset =
                    rw::math::vpu::Add(lVehicleAttribs.mBaseAttribs.mCOMOffset, lvHandlingBodyOffset);
            }

            // ---- the solid-box inertia tensor (see the banner) -----------------------------
            const Vector3 lvBoxDimensions =
                (rw::math::vpu::Add(lpSpec->mHandlingBodyDimensions,
                                    AbsPerLane(lVehicleAttribs.mBaseAttribs.mCOMOffset))) * KF_TWO;

            // `lvx128 v0,&attribs+0x70 ; vspltw v0,v0,0` -- lane .x of the packed mass register.
            const f32 lfMass =
                lVehicleAttribs.mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.mV.v[0];
            const f32 lfCoeff = KF_BOX_INERTIA_COEFF * lfMass;

            const f32 lfXX = lvBoxDimensions.mV.v[0] * lvBoxDimensions.mV.v[0];
            const f32 lfYY = lvBoxDimensions.mV.v[1] * lvBoxDimensions.mV.v[1];
            const f32 lfZZ = lvBoxDimensions.mV.v[2] * lvBoxDimensions.mV.v[2];

            Vector3 lvInverseInertia;
            lvInverseInertia.mV.v[0] = 1.0f / (lfCoeff * (lfYY + lfZZ));
            lvInverseInertia.mV.v[1] = 1.0f / (lfCoeff * (lfXX + lfZZ));
            lvInverseInertia.mV.v[2] = 1.0f / (lfCoeff * (lfXX + lfYY));
            // Lane .w is NEVER written by the console: the three `vrlimi128` inserts touch
            // lanes 0, 1 and 2 only, so v126's .w carries whatever the previous iteration left.
            // Reproduced as an explicit zero rather than as uninitialised stack -- the lane is
            // the Vector3 pad and nothing downstream reads it.
            lvInverseInertia.mV.v[3] = 0.0f;

            rw::physics::Inertia lSeatInertia;
            lSeatInertia.SetLinearDrag(KF_SEAT_DRAG);            // stfs -> block +0x20
            lSeatInertia.SetAngularDrag(KF_SEAT_DRAG);           //         block +0x24
            lSeatInertia.SetMaxLinearVelocity(KF_MAX_LINEAR_VELOCITY);   // block +0x18
            lSeatInertia.SetMaxAngularVelocity(KF_MAX_ANGULAR_VELOCITY); // block +0x1C
            lSeatInertia.SetInverseMass(1.0f / lfMass);          //         block +0x10
            // Sets +0x00 AND recomputes +0x14 as 1/min(diag) -- which is exactly the console's
            // `stvx128 v126 ; fcmpu/blt/fmr min-of-three ; fdivs 1.0f ; stfs +0x14` pair.
            lSeatInertia.SetInverseInertia(lvInverseInertia);

            // ---- the simulation copy: a softer tensor and 10,000x the drag ------------------
            rw::physics::Inertia lSimInertia = lSeatInertia;
            lSimInertia.SetInverseInertia(lvInverseInertia * KF_SIM_INERTIA_SCALE);
            lSimInertia.SetLinearDrag(KF_SIM_DRAG);
            lSimInertia.SetAngularDrag(KF_SIM_DRAG);

            // ---- post the body into the simulation request channel -------------------------
            // Its ONLY consumer is PhysicsModule::BridgeVehicleManagerToSimulation_PostScene
            // @0x825AB408, a DELIBERATELY INERT gate, and PostSceneUpdate destroys the VehManager
            // buffer in the same call that creates it -- so on this build the post is a producer
            // into a buffer nothing drains, BY THE CONSOLE'S OWN DESIGN. Reproduced as shipped.
            {
                CgsPhysics::PhysicsSimulationIO::InAddRigidBody lAddBody;
                // `clrlwi r11,r8,8 ; oris r11,r11,0xB00 ; sldi r11,r11,32` -- the entity word with
                // its owner byte replaced by 0x0B (E_ENTITYTYPE_PROP_COLLISION_RACECAR), promoted
                // into the handle's high dword. The symmetric partner of ProcessRemoveEvents'.
                lAddBody.mID = static_cast<u64>((luEntityWord & 0x00FFFFFFu) | 0x0B000000u) << 32;
                lAddBody.mRigidBody.GetObjectPointer()->mTransform       = lEvent.mInitialTransform;
                lAddBody.mRigidBody.GetObjectPointer()->mVelocity        = lEvent.mInitialVelocity;
                lAddBody.mRigidBody.GetObjectPointer()->mAngularVelocity = lEvent.mAngularVelocity;
                lAddBody.mRigidBody.GetObjectPointer()->mInertia         = lSimInertia;
                lAddBody.mRigidBody.GetObjectPointer()->mbSpy            = true;    // `li r29,1 ; stb r29,var_680`
                lAddBody.meState = rw::physics::ACTIVE_BODY;    // `li r11,4 ; stw r11,var_670`
                lpOutputInterface->GetRequiredRigidBodiesQueue()->AddEvent(lAddBody);
            }

            // ---- the seat ------------------------------------------------------------------
            CgsGeometric::AxisAlignedBox lAABB;
            lpSpec->GetBoundingBox(lAABB);

            if (!lEvent.mbDisablePhysicsStateReset)
            {
                // THE VCALL AT CONSOLE VTABLE SLOT +0x30 (`lwz r11,0(r3) ; lwz r11,0x30(r11) ;
                // mtctr ; bctrl` @0x82617224..0x8261724C) == RaceCarPhysics::Prepare @0x82639CB8.
                // Eleven arguments, every one of them identified at the call site:
                //   r4 = &lEvent.mInitialTransform     v1 = lEvent.mInitialVelocity
                //   v2 = lEvent.mAngularVelocity       v3 = lvHandlingBodyOffset (the wheel mean)
                //   v4 = lpSpec->mHandlingBodyDimensions
                //   r5 = the AABB GetBoundingBox filled one instruction earlier
                //   r6..r10 + stack@0x98 = lSeatInertia BY VALUE (six doublewords == 48 bytes)
                //   stack@0xA4/0xAC/0xB4 = &lVehicleAttribs / lavWheelPositions / lafWheelRadii
                //   stack@0xBF (a byte)  = lEvent.miCarStrengthStat
                // Spelled as a plain member call: this tree reaches members BY NAME, never by
                // console vtable slot (the host slot numbering is the compiler's).
                // Breaker passes the exact 32-byte buffer filled by GetBoundingBox straight to
                // Prepare in r5. All three physics declarations now use the canonical
                // CgsGeometric::AxisAlignedBox type, so no bridge object or cast is involved.
                maRaceCarVehicles[luRaceCar].Prepare(
                    lEvent.mInitialTransform,
                    lEvent.mInitialVelocity,
                    lEvent.mAngularVelocity,
                    lvHandlingBodyOffset,
                    lpSpec->mHandlingBodyDimensions,
                    lAABB,
                    lSeatInertia,
                    &lVehicleAttribs,
                    lavWheelPositions,
                    lafWheelRadii,
                    static_cast<u8>(lEvent.miCarStrengthStat));
            }

            // ---- register the per-car debug component, once ---------------------------------
            // `slwi r11,r27,10 ; add r11,r11,r24 ; addis r3,r11,2 ; addi r3,r3,0x7DC0`
            // == this + 163264 + 1024*slot == &maRaceCarDebugComponent[slot].
            if (!mabRaceCarDebugComponentRegistered[luRaceCar])
            {
                maRaceCarDebugComponent[luRaceCar].Register();
                mabRaceCarDebugComponentRegistered[luRaceCar] = true;
            }

            // ---- THE BIT ---------------------------------------------------------------
            // The four latches the console emits as one interleaved block (0x826173B8..0x826173E0):
            // the result event, the live-car bit, the entity id and the handling-body id.
            CGS_ASSERT(luRaceCar < 8u, "Index: liRaceCar, Number of bits: 8");   // CgsBitArray.h:222

            CreateVehicleResult lResult;
            lResult.mVolumeInstanceID = lEvent.mVolumeInstanceID;
            lResult.mbSuccess         = true;                        // `li r9,1 ; stb r9,var_818`

            mUsedRaceCars.SetBit(luRaceCar);
            maRaceCarEntityIDs[luRaceCar].muValue = luEntityWord;
            // `sldi r26,r8,32` -- the entity word promoted into the handle's HIGH dword, low dword
            // zero. This is what makes the 4-byte `::RigidBodyId` stand-in the deformation events
            // use lossless; see BrnVehicleManager_MaintenanceEvents.cpp's banner.
            maRaceCarHandlingBodyIDs[luRaceCar] = static_cast<u64>(luEntityWord) << 32;

            lpManagerOutputInterface->AddCreateVehicleResult(lResult);

            AddRaceCarDeformationModel(lpDeformationInterface,
                                       static_cast<s32>(luRaceCar),
                                       lEvent.mInitialTransform,
                                       lEvent.mfDeformAmount,
                                       lEvent.meBaseDeformationType);

            maeRaceCarTypes[luRaceCar] = lEvent.meRaceCarType;   // `stwx r10,r28,r24`

            // ---- the driver-control clear ---------------------------------------------------
            // Byte for byte the SAME inlined VehicleDriver::ClearControls as ProcessRemoveEvents
            // (0x82617420..0x8261748C vs 0x826163D4..0x82616444): the identical store set with the
            // identical two omissions (+0x3A mbToggle, +0x44 meDriverType).
            // DELETE-WHEN HONOURED 2026-08-11 (merge of the two create-drain waves):
            // VehicleDriver::ClearControls now HAS a body (BrnVehicleDriver.cpp), recovered from
            // these two inline sites together. The 28-store block that stood open here was checked
            // field-for-field against it before the swap -- identical, omissions included -- and the
            // call sits exactly where the console's inlined run did.
            maRaceCarDrivers[luRaceCar].ClearControls();

            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Created race car "
                    << static_cast<s32>(luRaceCar) << ", type "
                    << static_cast<s32>(maeRaceCarTypes[luRaceCar]) << "\n";
            }

            // ---- the two hide arms ----------------------------------------------------------
            if (lEvent.meRaceCarType == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            {
                // `lis/ori r10,0x2A11E ; lbzx r10,r24,r10 ; cmplwi 0 ; bne` -- 0x2A11E == 172318
                // == mbInOnlineGameModeStartLine. A network car created while the online mode is
                // sitting on the start line is NOT hidden.
                if (!mbInOnlineGameModeStartLine)
                {
                    mNetworkCarsRecievedFirstUpdate.UnSetBit(luRaceCar); // this + 44728

                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                    {
                        *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Making network race car "
                            << static_cast<s32>(luRaceCar) << ", type "
                            << static_cast<s32>(maeRaceCarTypes[luRaceCar])
                            << " hidden because it was just created\n";
                    }
                    SetNetworkRaceCarHidden(static_cast<EActiveRaceCarIndex>(luRaceCar), 1);
                }
            }
            else if (static_cast<s32>(luRaceCar) == static_cast<s32>(mePlayerActiveRaceCarIndex))
            {
                // The console prints mePlayerActiveRaceCarIndex here, NOT the loop's slot
                // (`lwz r10,var_9AC ; lwz r5,0(r10)` @0x82617768). They are equal on this branch,
                // so the difference is invisible -- reproduced as written rather than "tidied".
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: "
                        << "Making all network race cars hidden for at least 1 frame because player car "
                        << static_cast<s32>(mePlayerActiveRaceCarIndex) << " was just created\n";
                }
                SetAllNetworkRaceCarsHidden(1);
            }

            // The two Attrib::Instance destructors at 0x826177D8/0x826177E4 are the automatic
            // destruction of lHandling and lCarAsset; the scope above is what expresses them.
        }
    }
}
}

// FOLDED FROM BrnVehicleManager_TrafficCrashArms.cpp
// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_TrafficCrashArms.cpp
//
// THE TWO TRAFFIC-SIDE CRASH-PREDICTION ARMS DoCrashPrediction @0x82645FE0 dispatches -- the last
// two declare-only callees in that body, each behind a DELETE-WHEN gate since the head landed:
//
//   VehicleManager::HandleTrafficCarTrafficCarPotentialContact  @0x8263EC90 (279)  -- DecFIGS 0x797750
//   VehicleManager::HandleTrafficCarWorldPotentialContact       @0x8263F0F0 (220)  -- DecFIGS 0x7972AC
//
// Slice TU (home BrnVehicleManager.cpp is still unmountable). Both bodies are read off the ARTIST
// asm; the Hex-Rays for both is the usual 17/29-int prototype with "local variable allocation has
// failed". Signatures are the DecFIGS mangles verbatim:
//   (PotentialContact BY VALUE, VehicleOutputRequestInterface*, VehicleOutputInterface*,
//    VehicleManagerOutputInterface*, DeformationInputInterface*, float)
// and the caller confirms the order byte-for-byte: DoCrashPrediction stores r19 (request) /
// r20 (vehicle-out) / r17 (manager-out) / r21 (deformation) at var_81C/814/80C/804 before both
// calls, with f1 = timestep (0x82646310..0x82646338 and 0x82646BB4..0x82646BF8).
//
// ARGUMENT MAP (both prologues): r3 = this; the 80-byte contact arrives in r4..r10 (bytes 0..55,
// spilled to arg_20..arg_50) and on the stack from arg_58 (bytes 56..79); f1 = timestep. The four
// interface pointers sit at arg_74 (request) / arg_7C (vehicle-out) / arg_84 (manager-out) /
// arg_8C (deformation). NEITHER body ever loads arg_74: the request interface is accepted and
// ignored by the console, so the parameter is kept and unused.
//
// ⭐⭐ THE WORLD ARM HAS NO SIDE EFFECTS -- ON THE CONSOLE. Read before "fixing" it:
//   Every store in 0x8263F0F0..0x8263F45C is r1-relative (the two splat scratch vectors, the
//   timestep spill, and one dead bool), and the only calls are the three assert entry points and
//   the two PhysicalTrafficManager accessors (xrefs_from, .ida-exports). It resolves the traffic
//   car, drops the contact if the two bodies are separating, drops it if the car is already
//   CRASHING, tests the car's ground normal against its up axis (> 0.8) and the contact point's
//   height above the ground plane (< 0.4) -- and stores THAT comparison's CR bits into a stack
//   local nothing reads (`mfocrf r11,2 ; stw r11, var_A0` @0x8263F434, then the epilogue).
//   The DecFIGS internal PS3 build (Dec-2007, DEV) has the same shape: xrefs_from are the three
//   asserts only, the tail ends in the 0.8 splat. Whatever consumed the classification was compiled
//   out before Dec-2007 and never came back for retail. ⇒ a traffic car knocked into scenery is NOT
//   crashed by this arm on the console; it keeps the state the race-car / traffic-traffic arms gave
//   it, and its physical answer to the wall is the rigid-body contact solve, not a state change.
//   Reproduced as-is: classify, commit nothing. (The wave brief assumed the opposite; the asm wins.)
//
// THE CONSTANTS:
//   flt_82F31928 = 0.44704f (MPH -> m/s, .data, image-resident), flt_82092BC4 = 60.0f,
//   flt_8208F9C8 = 0.8f, flt_82F2A148 = 0.4f, flt_82001CC0 = 0.0f -- all read straight off the
//   image with tools/re/x360rd.py; none of them is a dyn-init zero.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                 // E_TRAFFIC_TYPE_*
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::E_ENTITYTYPE_*
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"  // BrnTraffic::GetVehicleSpecies
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [T13] witness (opt-in)
#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // flt_82092BC4 * flt_82F31928 (`fmuls f0, f0, f13` @0x8263EEE0): the closing speed, along the
    // contact normal, above which a traffic-vs-traffic contact CRASHES both cars instead of
    // slamming them. 60 mph, converted at the call site, not a pre-baked m/s literal.
    const f32 KF_TRAFFIC_TRAFFIC_CRASH_SPEED_MPH = 60.0f;          // flt_82092BC4

    // The world arm's two dead thresholds (see the banner).
    const f32 KF_TRAFFIC_WORLD_UPRIGHT_COSINE    = 0.800000012f;   // flt_8208F9C8

    inline u32 EntityWordOf(const CgsSceneManager::VolumeInstanceId& lrId)
    {
        return static_cast<u32>(lrId.muId >> 32);
    }

    inline u16 EntityIndexOfWord(u32 luEntityWord)
    {
        return static_cast<u16>((luEntityWord >> 10) & 0x3FFFu);
    }
}

// -------------------------------------------------------------------------------------------
// HandleTrafficCarTrafficCarPotentialContact  @0x8263EC90 (279)
//
// Called from DoCrashPrediction's queue-[13] walk for every traffic-with-traffic potential
// contact whose two owner bytes are both TRAFFIC_VEHICLE (0x826462FC..0x8264630C). Resolves both
// GLOBAL ids to PHYSICAL traffic slots (a non-physical car on either side ends it), asks the
// swept-box predictor whether the two bodies will actually meet, and then hands each car to one
// of the two response arms:
//     closing speed along the normal > 60 mph, OR the car is the TRAILER  -> CRASHING
//     otherwise, if the car is not already PHYSICAL                       -> SLAMMED by the other,
//                                                                            magnitude = closing * m_other / m_self
// The A half runs first, then the B half, with the roles swapped -- the console emits the two
// halves back to back (0x8263EF40..0x8263F008 and 0x8263F00C..0x8263F0DC), sharing lbHardHit and
// the closing speed (f31).
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleTrafficCarTrafficCarPotentialContact(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    f32 lfTimestep)
{
    // arg_74 is never loaded by the console body (see the banner).
    (void)lpRequestOutputInterface;

    // 0x8263ECA0 / 0x8263ECB8 -- the two GLOBAL entity words (r31 / r24).
    const u32 luTrafficAGlobalWord = EntityWordOf(lContact.muVolumeInstanceIdA);
    const u32 luTrafficBGlobalWord = EntityWordOf(lContact.muVolumeInstanceIdB);

    CGS_ASSERT((luTrafficAGlobalWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
               "lTrafficAEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :6690
    CGS_ASSERT((luTrafficBGlobalWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
               "lTrafficBEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :6691

    // 0x8263ED2C..0x8263EE54: GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe, inlined twice (its
    // h:944 bound assert and the EntityId ctor's < 0x4000 tripwire included). A 0x7F map slot on
    // EITHER side -- a traffic car that has no physics -- ends the contact with no assert.
    ::EntityId lTrafficAPhysicsID;
    if (!GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(luTrafficAGlobalWord, &lTrafficAPhysicsID))
        return;
    ::EntityId lTrafficBPhysicsID;
    if (!GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(luTrafficBGlobalWord, &lTrafficBPhysicsID))
        return;

    // 0x8263EE5C / 0x8263EE70: `extrwi 14,8` on each physics id -> the two physical slots.
    const u16 lu16TrafficAIndex = EntityIndexOfWord(lTrafficAPhysicsID.muValue);   // r20
    const u16 lu16TrafficBIndex = EntityIndexOfWord(lTrafficBPhysicsID.muValue);   // r17

    const PhysicalTrafficVehicle* const lpTrafficA =
        static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficAIndex));   // r25
    const PhysicalTrafficVehicle* const lpTrafficB =
        static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficBIndex));   // r24

    const SimpleVehiclePhysics* const lpBodyA = lpTrafficA->mpVehicleBody;   // lwz 0x1C(r25)
    const SimpleVehiclePhysics* const lpBodyB = lpTrafficB->mpVehicleBody;   // lwz 0x1C(r24)

    // 0x8263EE98: the swept-box prediction. A pair that will not meet is dropped -- there is no
    // near-miss arm for traffic-on-traffic.
    if (!PredictCarCarIntersection(lpBodyA, lpBodyB, lfTimestep))
        return;

    // 0x8263EEA8..0x8263EEF8: closing speed along the contact normal = (vB - vA) . n, against
    // 60 mph in m/s. `bgt` -> lbHardHit (r30, later r22).
    const Vector3 lvRelativeVelocity = lpBodyB->GetLinearVelocity() - lpBodyA->GetLinearVelocity();   // v12 - v13
    const f32  lfClosingSpeed = rw::math::vpu::Dot(lvRelativeVelocity, lContact.mNormal);           // f31
    const bool lbHardHit      = lfClosingSpeed > (KF_TRAFFIC_TRAFFIC_CRASH_SPEED_MPH * CgsCore::KF_MPH_TO_MPS);

    // 0x8263EEFC..0x8263EF3C: `cntlzw(species - 2) >> 5` == (species == E_SPECIES_TRAILER), on the
    // GLOBAL index of each car. A trailer half never takes the slam arm.
    const bool lbAIsTrailer =
        (BrnTraffic::GetVehicleSpecies(EntityIndexOfWord(luTrafficAGlobalWord)) == BrnTraffic::Vehicle::E_SPECIES_TRAILER);   // r23
    const bool lbBIsTrailer =
        (BrnTraffic::GetVehicleSpecies(EntityIndexOfWord(luTrafficBGlobalWord)) == BrnTraffic::Vehicle::E_SPECIES_TRAILER);   // r18

    // ---- [T13] DIAG. NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG). DELETE-WHEN-STABLE. ----
    // What reaches this arm and what it does with it: the two slots, both states, the closing
    // speed against its 60 mph gate, the trailer bits. Only pairs PredictCarCarIntersection
    // accepts arrive here (real since 2026-09-23). First 40 arrivals, then every 200th.
    {
        static u32 s_uArrivals = 0u;
        ++s_uArrivals;
    }

    // ---- the A half (0x8263EF40..0x8263F008): A is the victim, B the crasher --------------------
    if (lbHardHit || lbAIsTrailer)
    {
        mPhysicalTrafficManager.SetTrafficVehicleCrashing(
            lTrafficAPhysicsID, lTrafficBPhysicsID,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
    }
    else if (mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficAIndex))->mePhysicalTrafficState
                 != E_TRAFFIC_TYPE_PHYSICAL)                      // lwz 0x20 ; cmpwi 2
    {
        // v2 = (1 / massA) * (massB * closing): vrefp + two Newton steps on A's mass (+0xE0), B's
        // mass scaled by the closing speed, multiplied. v1 = the contact point ON A (arg_20).
        const f32 lfSlamMagnitude =
            lfClosingSpeed * lpBodyB->GetMass().GetFloat() / lpBodyA->GetMass().GetFloat();
        const VecFloat lvSlamMagnitude(lfSlamMagnitude);
        mPhysicalTrafficManager.SetTrafficVehicleSlammed(
            lTrafficAPhysicsID, lTrafficBPhysicsID, lpBodyB,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
            lContact.mPointOnA,
            lvSlamMagnitude);
    }

    // ---- the B half (0x8263F00C..0x8263F0DC): B is the victim, A the crasher --------------------
    if (lbHardHit || lbBIsTrailer)
    {
        mPhysicalTrafficManager.SetTrafficVehicleCrashing(
            lTrafficBPhysicsID, lTrafficAPhysicsID,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
    }
    else if (mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficBIndex))->mePhysicalTrafficState
                 != E_TRAFFIC_TYPE_PHYSICAL)
    {
        // v2 = (1 / massB) * (massA * closing); v1 = the contact point ON B (arg_30).
        const f32 lfSlamMagnitude =
            lfClosingSpeed * lpBodyA->GetMass().GetFloat() / lpBodyB->GetMass().GetFloat();
        const VecFloat lvSlamMagnitude(lfSlamMagnitude);
        mPhysicalTrafficManager.SetTrafficVehicleSlammed(
            lTrafficBPhysicsID, lTrafficAPhysicsID, lpBodyA,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
            lContact.mPointOnB,
            lvSlamMagnitude);
    }
}

// -------------------------------------------------------------------------------------------
// HandleTrafficCarWorldPotentialContact  @0x8263F0F0 (220)
//
// Called from DoCrashPrediction's queue-[9] walk for every traffic-with-world potential contact.
// Orients the record so the traffic car is "A" (the world's normal is sign-flipped when the
// scene manager reported the pair the other way round), resolves the physical slot, and drops the
// contact if the two are separating or the car is already crashing. Then it classifies the hit
// -- upright on its ground normal, contact low against the ground plane -- into a local that the
// retail build never reads. NO STATE CHANGES, NO EVENTS, NO CALLS PAST THE ACCESSORS (banner).
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleTrafficCarWorldPotentialContact(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    f32 lfTimestep)
{
    // None of the four interfaces is loaded by the console body (see the banner).
    (void)lpRequestOutputInterface;
    (void)lpVehicleOutputInterface;
    (void)lpManagerOutputInterface;
    (void)lpDeformationInterface;

    // 0x8263F118..0x8263F1A8: which side is the traffic car? Owner byte of id A == TRAFFIC_VEHICLE
    // keeps the record as-is (v127 = pointOnA, v126 = pointOnB, v125 = normal); otherwise the
    // roles swap and the normal is sign-flipped (`vspltisw -1 ; vslw ; vxor128`).
    const u32 luWordA = EntityWordOf(lContact.muVolumeInstanceIdA);
    const u32 luWordB = EntityWordOf(lContact.muVolumeInstanceIdB);

    u32     luTrafficGlobalWord;   // r31
    u32     luWorldWord;           // r30
    Vector3 lvPointOnTraffic;      // v127
    Vector3 lvPointOnWorld;        // v126
    Vector3 lvNormal;              // v125  (points OUT of the world, into the traffic car)
    if ((luWordA >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
    {
        luTrafficGlobalWord = luWordA;
        luWorldWord         = luWordB;
        lvPointOnTraffic    = lContact.mPointOnA;
        lvPointOnWorld      = lContact.mPointOnB;
        lvNormal            = lContact.mNormal;
    }
    else
    {
        luTrafficGlobalWord = luWordB;
        luWorldWord         = luWordA;
        lvPointOnTraffic    = lContact.mPointOnB;
        lvPointOnWorld      = lContact.mPointOnA;
        lvNormal            = rw::math::vpu::Negate(lContact.mNormal);
    }

    CGS_ASSERT((luTrafficGlobalWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
               "lTrafficEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :6831
    CGS_ASSERT((luWorldWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lWorldEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");               // :6832

    // 0x8263F204..0x8263F29C: the inlined _Safe lookup; a 0x7F slot ends it without an assert.
    ::EntityId lTrafficPhysicsID;
    if (!GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(luTrafficGlobalWord, &lTrafficPhysicsID))
        return;

    const u16 lu16TrafficIndex = EntityIndexOfWord(lTrafficPhysicsID.muValue);   // r28

    const PhysicalTrafficVehicle* lpTraffic =
        static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    CGS_ASSERT(lpTraffic != 0, "lpTraffic");                                       // :6848

    // ---- 0x8263F2E0..0x8263F38C: the separating test, the same shape as the race-car arm's.
    //      vP = w x (pTraffic - pos) + v ; (pTraffic + vP*dt - pWorld) . n > 0 -> moving APART.
    {
        const SimpleVehiclePhysics* const lpBody = lpTraffic->mpVehicleBody;           // lwz 0x1C
        const Matrix44Affine lTransform      = lpBody->GetTransform();                 // +0x10..+0x40
        const Vector3 lvRadius        = lvPointOnTraffic - lTransform.wAxis;           // v13
        const Vector3 lvPointVelocity =
            rw::math::vpu::Cross(lpBody->GetAngularVelocity(), lvRadius) + lpBody->GetLinearVelocity();   // +0x60 / +0x50
        const Vector3 lvPredictedPoint = lvPointVelocity * lfTimestep + lvPointOnTraffic;
        if (rw::math::vpu::Dot(lvPredictedPoint - lvPointOnWorld, lvNormal) > 0.0f)  // flt_82001CC0
            return;
    }

    // 0x8263F390..0x8263F3A4: already CRASHING -> nothing to do (lwz 0x20 ; cmpwi 1).
    if (mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex))->mePhysicalTrafficState
            == E_TRAFFIC_TYPE_CRASHING)
        return;

    // ---- 0x8263F3A8..0x8263F438: THE DEAD CLASSIFICATION. Body +0x580 is
    //      mAboveGroundTestResult.mIntersectionNormal and +0x570 its mIntersectionPosition.
    //      upright  := up . groundNormal > 0.8
    //      lowHit   := 0.4 > (pTraffic - groundPoint) . groundNormal        (only if upright)
    //      The console stores lowHit's CR word to var_A0 and returns. Nothing consumes it; there is
    //      no store outside the stack frame past this point. Reproduced as computed-and-discarded.
    lpTraffic = static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    {
        const SimpleVehiclePhysics* const lpBody = lpTraffic->mpVehicleBody;
        const AboveGroundTestResult* const lpGround = lpBody->GetAboveGroundTestResult();
        const Vector3 lvUp = lpBody->GetTransform().yAxis;                              // lvx128 +0x20

        bool lbLowContactOnUprightCar = false;                                          // var_A0, dead
        if (rw::math::vpu::Dot(lvUp, lpGround->mIntersectionNormal) > KF_TRAFFIC_WORLD_UPRIGHT_COSINE)   // vcmpgtfp. vs 0.8
        {
            lbLowContactOnUprightCar =
                gfGroundContactCullHeight >
                rw::math::vpu::Dot(lvPointOnTraffic - lpGround->mIntersectionPosition, lpGround->mIntersectionNormal);   // vcmpgtfp. 0.4 vs
        }
        (void)lbLowContactOnUprightCar;
    }
}

}   // namespace Vehicle
}   // namespace BrnPhysics

// FOLDED FROM BrnVehicleManager_UpdateCrashes.cpp
// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateCrashes.cpp
//
// THE PER-FRAME CRASH-RECORD TICKER -- called every frame by the landed UpdateVehiclePhysics
// (BrnVehicleManager_UpdateVehiclePhysics.cpp:844, between the gs_iUpdateCrashesPM monitors).
//
//   VehicleManager::UpdateCrashes  @0x825EA640  (203 insns; DWARF BrnVehicleManager.cpp:7105)
//
// Slice TU. This is the only definition tree-wide; the boot gate it replaced is gone (LNK2005 is
// the tripwire if it ever reappears).
//
// -------------------------------------------------------------------------------------------------
// WHAT THE 203 INSTRUCTIONS ARE. All but eleven of them are the INLINED CgsBitArray<32u>
// iterator pair (GetFirstNonZeroBit / GetNextNonZeroBit) plus the two CGS_ASSERT message
// builders it carries ("luIndex < NUMBITS" CgsBitArray.h:241 for UnSetBit, and the StrStream-
// formatted "invalid index : %u < 32" CgsBitArray.h:203 for IsBitSet). The body proper is:
//
//   0x825EA650  r22 = this + 0x10000 - 0x5338 == this + 44232   -> mUsedRaceCarCrashesList
//               (BitArray<32u>: ONE u64 word -- the word-scan loop bound is `cmplwi r11, 1`)
//   0x825EA66C..0x825EA6B4  lowest set bit: isolate (x & (x-1)) ^ x, cntlzd, 63 - clz
//   0x825EA6B8  exit unless 0 <= liIndex < 32
//   per set bit liIndex (0x825EA724..):
//     0x825EA724  r11 = this + liIndex*12 + 0xAB28   == &maRaceCarCrashes[liIndex] + 8
//                 (43808 + 8: the pool base is +43808, +8 is RaceCarCrashData::mfTimeSinceImpact)
//     0x825EA73C  f0 = mfTimeSinceImpact + lfSimTimerTimeStep ; stfs  (the ONLY non-bit store)
//     0x825EA748  fcmpu f0, flt_82004014 (== 0.1f) ; ble skip      -> if (time > 0.1f)
//     0x825EA770      word &= ~(1 << (liIndex & 63))                -> UnSetBit(liIndex)
//     0x825EA788..0x825EA958  GetNextNonZeroBit(liIndex): scan liIndex+1 .. min(word end, 32),
//                 then the next non-zero word (there is none: 1 word), then exit on -1 / >= 32.
//
// The DWARF hint block for :7105 lists exactly: liIndex, BitArray<32u>::GetFirstNonZeroBit,
// BitArray<32u>::UnSetBit, BitArray<32u>::GetNextNonZeroBit. Nothing else is called.
//
// WHAT IT MEANS. A set bit in mUsedRaceCarCrashesList is an ALLOCATED maRaceCarCrashes slot
// (SetRaceCarCrashing allocates one per committed crash, seeding mfTimeSinceImpact = 0 -- and,
// pool full, evicts the slot with the LARGEST mfTimeSinceImpact, i.e. the oldest). This ticker
// ages every allocated slot by the sim timestep and frees it once it is older than 0.1 s. The
// record therefore lives for ~3 sim frames at 30 Hz -- long enough for the consumers that match
// crash records by entity id (the crash-event / scoring readback) to see it, no longer.
//
// It does NOT end the car's crash state (mbCrashing lives on RaceCarPhysics and is cleared by the
// crash-lifecycle code on the physics side); a taken-down rival is not stranded by anything here.
//
// POLARITY, checked twice against the asm:
//   * `fcmpu cr6, f0, f13 ; ble cr6, skip` -- the bit is CLEARED when time > 0.1f, KEPT otherwise.
//   * `andc r10, r9, r10 ; stdx` -- the store is word & ~mask: an UNSET, not a set.
//   * The first/next-bit exits are `>= 32` and `<= -1`; the recon BitArray<32> returns -1 in
//     both cases, so `liIndex >= 0` is the whole loop condition.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<32> (mUsedRaceCarCrashesList)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        // flt_82004014 == 0.1f (read from the image). How long an allocated RaceCarCrashData slot
        // stays live before UpdateCrashes frees it. FLAG: constant NAME proposed (no DWARF symbol
        // for the literal); the value and its use are asm-literal.
        const f32 KF_RACE_CAR_CRASH_DATA_LIFETIME = 0.1f;
    }

    void VehicleManager::UpdateCrashes(f32 lfSimTimerTimeStep)
    {
        for (s32 liIndex = mUsedRaceCarCrashesList.GetFirstNonZeroBit();          // 0x825EA66C..0x825EA6C4
             liIndex >= 0;
             liIndex = mUsedRaceCarCrashesList.GetNextNonZeroBit(liIndex))         // 0x825EA788..0x825EA958
        {
            RaceCarCrashData& lrCrash = maRaceCarCrashes[liIndex];                 // 0x825EA724 (stride 12, +43808)

            lrCrash.mfTimeSinceImpact += lfSimTimerTimeStep;                       // 0x825EA740 fadds ; stfs
            if (lrCrash.mfTimeSinceImpact > KF_RACE_CAR_CRASH_DATA_LIFETIME)       // 0x825EA748 fcmpu ; ble
            {
                mUsedRaceCarCrashesList.UnSetBit(static_cast<u32>(liIndex));       // 0x825EA770..0x825EA784 andc ; stdx
            }
        }
    }

} // namespace Vehicle
} // namespace BrnPhysics

// FOLDED FROM BrnVehicleManager_UpdateVehiclePhysics.cpp
// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateVehiclePhysics.cpp
//
// BIG FIVE #3 -- BrnPhysics::Vehicle::VehicleManager::UpdateVehiclePhysics @0x82644FA8
// (1,038 X360 instructions), the per-frame FORCE PRODUCER: the manager-level conductor
// that dispatches every live car's driver + physics update (the seam into the 54 bodied
// force leaves), plus its in-TU per-frame siblings:
//
//   UpdateVehiclePhysics                @0x82644FA8  (1038)  DWARF h:896
//   IsRaceCarCrashing                   @0x825B5690  (30)    DWARF h:947
//   ForceRaceCarCrash (5-arg overload)  @0x82635B78  (34)    DWARF h:1239
//   ProcessAboveGroundLineTestsResults  @0x826183F8  (105)   DWARF h:866
//   ProcessAftertouchEvents             @0x82633DE8  (69)    DWARF h:1113
//
// Home TU BrnVehicleManager.cpp is still unmounted -- this is a slice TU in the
// established BrnVehicleManager_PerFrameLeaves.cpp pattern (fold back at the home mount).
//
// Every body is reconstructed from its X360 asm read line by line (the Hex-Rays for the
// conductor renders the BitArray iteration as ~200 lines of inlined streamed-assert
// machinery -- that all lives inside CgsBitArray.h's Get{First,Next}NonZeroBit and is NOT
// re-expanded here). Members are accessed BY NAME; every console offset the asm touches
// is cited at its use and lands on a member the committed headers already pin.
//
// sub_82635B78 IDENTITY (previously unnamed in the export set): it is the FIVE-argument
// ForceRaceCarCrash overload. Proof, three-legged: (1) DWARF declares the overload PAIR at
// BrnVehicleManager.h:1239 (5-arg) / :1242 (6-arg, + EntityId) and MSVC lays adjacent
// overloads in reversed declaration order -- 0x82635B00 (named ForceRaceCarCrash in the
// exports) is the 6-arg form, 0x82635B78 the 5-arg; (2) caller set: both in-body call
// sites here pass exactly this+5 with NO EntityId while CrashFatalRaceCars (which holds an
// EntityId) calls 0x82635B00; (3) raw image bytes (x360rd, self-test 10/10): 0x82635B78
// clears the driver's two invulnerability bytes, gates on !mbCrashing (RCP+0x710), and
// `bl 0x82634C90` (SetRaceCarCrashing) with r10 = -1 (the "no takedown type" sentinel)
// and both contact vectors zero -- while 0x82635B00 passes its EntityId parameter through.
//
// OUT-OF-TU TAILS THIS TU ARMS: SetRaceCarCrashing, UpdateVehicleImpacts, UpdateAggressiveDriving,
// UpdateCrashes, EndVehicleTractionLineTests, CrashFatalRaceCars, ReadSurfaceProperties(u64),
// VehicleDriver::UpdateVehicle, PhysicalTrafficManager::UpdateTrafficPhysics, PhysicalTraffic
// Manager::PassNearbyCrashingTrafficIdsToRaceCarModule, DebugComponent::Update. Each once had a
// trap stub in a stub TU that no longer exists.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerPerfMonHandles.h" // the 13 stage monitors (defined in _Construct.cpp)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"  // VehicleInputInterface (queues)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleOutputInterface / VehicleManagerOutputInterface / VehicleOutputRequestInterface
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"         // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h" // InEventLineTestNearest (GenerateAboveGroundLineTests)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue<1536,16> (the GameEventQueue)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                          // CgsSystem::Time (mCurrentTime = lrCurrentTime)
#include "SharedClasses/BrnSharedConstants.h"                                     // BrnUpdateSet
#include "rw/math/vpu/vector3_operation.h"                                        // vpu::{Magnitude, Normalize, Dot, Abs, Add, Mult}
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h" // BrnPhysics::Vehicle::DebugComponent (per-car tick)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "GameSource/GameState/BrnGameEvents.h"                                   // JustBouncedEvent / JustAppliedExtraSpinEvent (events 52 / 53)
#include <cstddef>                                                                // offsetof (the event 52 layout asserts)




namespace BrnPhysics
{

namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // ------------------------------------------------------------------------------------
    // File-scope statics of the console BrnVehicleManager.cpp translation unit that this
    // slice's bodies read.
    // ------------------------------------------------------------------------------------

    // byte_82FBA344 -- DWARF BrnVehicleManager.cpp:2585 `bool _lbOverrideForceFrozen`, the
    // debug "freeze the player car regardless of the update set" override. FLAG: its
    // console WRITER (a debug window) is not reconstructed, so on PC this stays false and
    // the `||` arm below is statically dead -- carried as shipped, not pruned.
    static bool gs_bOverrideForceFrozen = false;

    // qword_82FB7F10 -- the 64-bit AttribSys surface-list key UpdateVehiclePhysics passes
    // to ReadSurfaceProperties (`ld r4, qword_82FB7F10` @0x82645144). Breaker static
    // initializer @0x82C5B5F8 calls Attrib::StringToKey("340654") and stores the u64 result.
    // DWARF spells the parameter type Attribute::Key; the committed Attribute::Key typedef
    // is u32 while the console load is 8 bytes -- width conflict FLAGGED at the
    // declaration (BrnVehicleManager.h), not compounded here.
    static const u64 gs_uSurfaceListKey = Attrib::StringToKey("340654");

    // The stationary-pose wheel-angle scale (rodata flt_8208FA8C == 1.3f, read off the
    // image). DWARF BrnVehicleManager.cpp:223 names the constant
    // KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE; the meStationaryPlayerWheelAngle == 0 arm uses
    // the NEGATED value from a local literal (asm 0x82645F00 lfs flt_8209D0D4 == -1.3f).
    static const f32 KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE = 1.3f;

    // rodata 0x8208F5F4 == 0.01745329238f (pi/180). The console reaches it as
    // `lfs -0x3E0(&flt_8208F9D4)`; the DWARF hint spells the operation rw::math::vpu::DegToRad.
    static const f32 KF_DEG_TO_RAD = 0.01745329238474369f;

    // The handbrake-turn crash gate thresholds (both image-read):
    //   flt_8208F9D4 = 20.0f  -- minimum speed for the "sliding, not driving" crash test
    //   flt_82001DA0 = 0.5f   -- cos-angle floor: dot(forward, velocity-dir) below this crashes
    static const f32 KF_HANDBRAKE_CRASH_MIN_SPEED   = 20.0f;
    static const f32 KF_HANDBRAKE_CRASH_MAX_COS_DOT = 0.5f;

    // ------------------------------------------------------------------------------------
    // IsRaceCarCrashing  @0x825B5690  (30 insns)
    // Bounds-assert the index (console lines :8272/:8273) and return the car's crash
    // master flag (lbz RCP+0xE50-0x740 == SimpleVehiclePhysics::mbCrashing @+0x710).
    // ------------------------------------------------------------------------------------
    bool VehicleManager::IsRaceCarCrashing(s32 liRaceCarIndex)
    {
        CGS_ASSERT_AT(liRaceCarIndex >= 0,
            "liRaceCarIndex >= 0", KAC_ARTIST_ASSERT_FILE, 8272 /* ARTIST 0x825B56BC */);                    // :8272
        CGS_ASSERT_AT(liRaceCarIndex < 8,
            "liRaceCarIndex < ku8MaxNumRaceCars", KAC_ARTIST_ASSERT_FILE, 8273 /* ARTIST 0x825B56E0 */);      // :8273

        return maRaceCarVehicles[liRaceCarIndex].IsCrashing();                     // lbz 0xE50(rec)
    }

    // ------------------------------------------------------------------------------------
    // ForceRaceCarCrash (5-arg overload)  @0x82635B78  (34 insns) -- see the identity
    // banner at the top of this file.
    //
    // Clear the target driver's two invulnerability latches, then (unless the car is
    // already crashing) commit a crash through the universal sink with the car's own
    // entity id as both victim AND aggressor, zero contact geometry, and no takedown type:
    //   0x82635BA4/A8  stb 0, 0x7C/0x7D(this+224*idx)   -> driver+0x3C/+0x3D (invuln flags)
    //   0x82635BAC     ld  (this + 8*(idx+5468))        -> maRaceCarHandlingBodyIDs[idx]
    //   0x82635BB4/B8  srdi 32 / clrldi                 -> the EntityId is the HIGH word
    //                  (same extraction the PTM contact fix-up documents for RigidBodyId)
    //   0x82635BBC/C0  lbz 0xE50(this+5216*idx) gate    -> !mbCrashing
    //   0x82635BC8/DC  vspltisw v2,0 ; vmr v1,v2        -> both contact vectors zero
    //   0x82635BD8     li r10, -1                       -> ETakedownType "none"
    //   0x82635BE8     bl SetRaceCarCrashing @0x82634C90
    // ------------------------------------------------------------------------------------
    void VehicleManager::ForceRaceCarCrash(
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        EActiveRaceCarIndex leRaceCarIndex)
    {
        maRaceCarDrivers[leRaceCarIndex].SetInvulnerableToVehicles(false);   // stb 0, +0x7C
        maRaceCarDrivers[leRaceCarIndex].SetInvulnerableToWorld(false);      // stb 0, +0x7D

        if (!maRaceCarVehicles[leRaceCarIndex].IsCrashing())                 // lbz 0xE50 gate
        {
            // RigidBodyId keeps the EntityId in its HIGH 32 bits (ld ; srdi 32).
            const u32 luEntityId =
                static_cast<u32>(maRaceCarHandlingBodyIDs[leRaceCarIndex] >> 32);

            // CAST RETIRED 2026-08-11 (consolidation wave). SetRaceCarCrashing's committed
            // declaration used to type its output-interface parameter
            // BrnGameState::GameStateModuleIO::VehicleOutputInterface* -- a fork type that does
            // not exist in the DWARF at all -- while the DWARF (BrnVehicleManager.h:1218) and
            // this function's own signature use the BrnPhysics::Vehicle::VehicleOutputInterface
            // this TU carries (mangle `...PNS0_22VehicleOutputInterfaceE...`, NS0_ ==
            // BrnPhysics::Vehicle). The declaration is re-typed, so the pointer passes straight
            // through.
            // The manager decl's EntityId is BrnCommonTypes' packed-id POD (`{ u32 muValue; }`).
            EntityId lVictimId;
            lVictimId.muValue = luEntityId;
            const Vector3 lvZero(0.0f, 0.0f, 0.0f);
            SetRaceCarCrashing(
                lVictimId,
                lVictimId,
                lvZero,
                lvZero,
                lpRequestOutputInterface,
                lpVehicleManagerOutputInterface,
                lpVehicleOutputInterface,
                lpDeformationInterface,
                static_cast<BrnGameState::ETakedownType>(-1));               // li r10, -1
        }
    }

    // ------------------------------------------------------------------------------------
    // GenerateAboveGroundLineTests  @0x82633990  (109 insns)   -- THE PRODUCER
    //
    // For every LIVE race car, post one downward 10 m "nearest" line test into the physics
    // module's outgoing fine-query queue. The result comes back next frame through
    // WorldBridgeSceneToPhysics -> VehicleInputInterface::AddLineTestResult ->
    // ProcessAboveGroundLineTestsResults (immediately below), which stamps the car's
    // AboveGroundTestResult -- the thing RCEM::UpdateRaceCarCollisionTagging needs before a car
    // can be given an AI section index.
    //
    // Console body (r3 == this, r4 == lpRequestInterface):
    //   0x826339A0  the bit-array base: `addis r14,r3,1 ; addi r14,r14,-0x5340` == this+44224
    //               == &mUsedRaceCars, then the standard CgsBitArray GetFirstNonZeroBit /
    //               GetNextNonZeroBit walk (its own CgsBitArray.h:203 index tripwire inline).
    //   0x82633A24  the queue seat: `addi r15, r4, 0x28C0` == &mRequestFineLineQueue
    //   0x82633A74  `mulli r10, r31, 0x1460 ; add r10,r10,r3 ; lvx128 v0, r10, 0x780`
    //               == maRaceCarVehicles[i] (base +1856, stride 5216) + 0x40 == GetPosition()
    //   0x82633AD8  `vsubfp v0,v0,v13 ; vrlimi128 v12,v0,4,0` -- v13 is a splat of
    //               flt_82004A20 (READ FROM THE IMAGE: 10.0f) and vrlimi mask 4 selects WORD 1,
    //               so ONLY Y is lowered: the end point is the start point 10 m straight down.
    //   0x82633ABC  `clrlslwi r10,r9,24,8 ; oris r10,r10,2` -> mQueryId == 0x20000 | (i << 8).
    //               ⭐ THAT ENCODING IS THE CONTRACT WITH THE RESULT HALF: the consumer below
    //               reads request type as (id >> 16) & 0xFF (must be 2) and the car index as
    //               (id >> 8) & 0xFF. Producer and consumer are 130 KB apart in the image and
    //               nothing but this pair of shifts ties them together.
    //   0x82633A80  mx32EntityTypeFlags = 2 ; 0x82633AC0 mExcludeEntityId = dword_82F2A3A4
    //               (READ FROM THE IMAGE: 0xFFFFFFFF == EntityId::SetInvalid) ;
    //               0x82633A88 meExclusionMode = 0 ; 0x82633A84 mxVolumeTypeFlags = 2
    //   0x82633AEC  AddEvent<InEventLineTestNearest>(&event, 6)
    // ------------------------------------------------------------------------------------
    void VehicleManager::GenerateAboveGroundLineTests(
        VehicleOutputRequestInterface* lpRequestInterface)
    {
        CGS_ASSERT(lpRequestInterface != 0, "lpRequestInterface != NULL");
        if (lpRequestInterface == 0)
        {
            return;
        }

        // flt_82004A20, read out of the decrypted ARTIST image (0x41200000).
        static const f32 KF_ABOVE_GROUND_RAY_LENGTH = 10.0f;
        // The (id >> 16) & 0xFF value ProcessAboveGroundLineTestsResults tests for.
        static const u32 KU_REQUEST_TYPE_RACE_CAR_ABOVE_GROUND = 2u;
        // The console's `li r5, 6` -- the scene-query event TYPE id the variable queue tags the
        // record with. (3 is the traffic traction line test the physical-traffic manager posts;
        // the SceneManager's query dispatcher switches on this.)
        static const s32 KI_SCENE_QUERY_EVENT_LINE_TEST_NEAREST = 6;

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar >= 0;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {

            const Vector3 lStart = maRaceCarVehicles[liCar].GetPosition();
            Vector3 lEnd = lStart;
            lEnd.SetY(float(lEnd.GetY()) - KF_ABOVE_GROUND_RAY_LENGTH);

            CgsSceneManager::SceneManagerIO::InEventLineTestNearest lEvent;
            lEvent.mLineStart          = lStart;
            lEvent.mLineEnd            = lEnd;
            lEvent.mQueryId.mId        = (KU_REQUEST_TYPE_RACE_CAR_ABOVE_GROUND << 16) |
                                         (static_cast<u32>(liCar) << 8);
            lEvent.mx32EntityTypeFlags = 2u;
            lEvent.mExcludeEntityId.SetInvalid();
            lEvent.meExclusionMode     =
                CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY;
            lEvent.mxVolumeTypeFlags   = 2u;

            lpRequestInterface->GetRequestFineLineQueue()
                ->AddEvent<CgsSceneManager::SceneManagerIO::InEventLineTestNearest>(
                    lEvent, KI_SCENE_QUERY_EVENT_LINE_TEST_NEAREST);
        }
    }

    // ------------------------------------------------------------------------------------
    // ProcessAboveGroundLineTestsResults  @0x826183F8  (105 insns)
    // Drain the scene manager's line-test result queue: every VALID result of request type
    // 2 (a race car's above-ground down-ray) stamps that car's AboveGroundTestResult via
    // the already-bodied SimpleVehiclePhysics::SetAboveGroundTestResult; type 3 (traffic)
    // and unknown types fire the console's own streamed asserts (lowered to CGS_ASSERT
    // with the static message per the standing rule).
    //
    // Event decode (the committed OutEventLineTestNearestResult layout, producer-pinned in
    // CgsSceneManagerModuleIO.h; the console copies each 64-byte record to the stack ld x8):
    //   +0x00 mPosition (v1)   +0x10 mNormal (v2)   +0x28 mQueryId {.., type, index, ..}
    //   +0x34 mu16MaterialTag  +0x36 mu16GroupTag   +0x38 mbIntersection
    //   (extrwi 8,8 -> request type; extrwi 8,16 -> car index; asm 0x82618480..0x82618580)
    // ------------------------------------------------------------------------------------
    void VehicleManager::ProcessAboveGroundLineTestsResults(
        const VehicleInputInterface::InLineTestResultQueue* lpLineTestResults)
    {
        for (s32 liEvent = 0; liEvent < lpLineTestResults->GetLength(); ++liEvent)
        {
            const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult& lrResult =
                lpLineTestResults->GetEvent(liEvent);

            if (!lrResult.mbIntersection)
                continue;

            const u32 luRequestType  = (lrResult.mQueryId.mId >> 16) & 0xFFu;   // extrwi 8,8
            const u32 luRaceCarIndex = (lrResult.mQueryId.mId >> 8) & 0xFFu;    // extrwi 8,16

            if (luRequestType == 2u)
            {
                maRaceCarVehicles[luRaceCarIndex].SetAboveGroundTestResult(
                    lrResult.mPosition, lrResult.mNormal,
                    lrResult.mu16MaterialTag, lrResult.mu16GroupTag);
            }
            else if (luRequestType == 3u)
            {
                CGS_FAIL(
                    "A physical traffic vehicle tried submitted an 'above ground' line test to the SceneManager");   // :2752
            }
            else
            {
                CGS_FAIL("Received result for an unknown line test request");  // :2759
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ProcessAftertouchEvents  @0x82633DE8  (69 insns)
    // Only while the car is CRASHING: publish this frame's aftertouch stick values (event
    // 76, 12 bytes {index, y, x}), and -- while the player car is actually in showtime --
    // the recent-bounce report (event 52, 32 bytes) and the one-shot sixaxis-tilt latch
    // (event 53, 1 byte; the latch byte_82FB848A == msPlayerParams.mbSixaxisTiltApplied
    // is CONSUMED here: read then cleared, asm 0x82633ED0/0x82633ED4).
    //
    // THE GetAftertouchValues FORK RESOLVES AT THIS CALL SITE. The out-of-line leaf
    // @0x825B2E88 is called here (bl @0x82633E44) with FOUR arguments -- three float
    // pointers AND the bool r7 = (meShowtimeBehaviour == 2) -- and no return read: it is
    // the reference form `void GetAftertouchValues(f32&, f32&, f32&, bool) const` bodied
    // at BrnPlayerDriverControls.cpp:39. The 3-pointer `bool GetAftertouchValues(f32*,
    // f32*, f32*)` declaration was the fork no TU could define; it is deleted this wave
    // (BrnVehicleDriverControls.h) and UpdateAftertouch's call is re-pointed.
    // ------------------------------------------------------------------------------------
    void VehicleManager::ProcessAftertouchEvents(
        s32 liRaceCarIndex, CgsModule::VariableEventQueue<1536, 16>* lpOutputQueue)
    {
        RaceCarPhysics& lrCar = maRaceCarVehicles[liRaceCarIndex];

        if (!lrCar.IsCrashing())                                   // lbz 0xE50 gate
            return;

        // Event 76 (0x4C): the per-frame aftertouch values. 12-byte payload, in the
        // console's exact stack order {s32 index, f32 out2, f32 out1} (stw idx @sp+0x58,
        // outs written by the callee at sp+0x5C / sp+0x60; AddEvent copies from sp+0x58).
        struct AftertouchEventPayload
        {
            s32 miRaceCarIndex;
            f32 mfAftertouchY;
            f32 mfAftertouchX;
        };
        AftertouchEventPayload lPayload;
        lPayload.miRaceCarIndex = liRaceCarIndex;

        f32 lfOutZ = 0.0f;   // sp+0x54 -- written by the callee, NOT part of the event
        maRaceCarDrivers[liRaceCarIndex].mControls.GetAftertouchValues(
            lPayload.mfAftertouchX, lPayload.mfAftertouchY, lfOutZ,
            meShowtimeBehaviour == BrnGameState::E_SHOWTIME_MODE_ON_SIXAXIS);   // r7: cntlzw/extrwi idiom

        lpOutputQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lPayload), 76, 12);

        // Showtime-only tail: virtual slot +0x14 == IsPlayerVehicleActuallyInShowtime.
        if (lrCar.IsPlayerVehicleActuallyInShowtime())
        {
            // Event 52 (0x34): the recent-bounce report. GetRecentBounce @0x825B8B08 writes its
            // seven outputs STRAIGHT INTO the queued record at sp+0x70 -- r4 +0x00, r5..r8
            // +0x04..+0x07, r9 +0x08, r10 +0x10 (@0x82633E80..0x82633E98) -- and the record is
            // posted (`li r6, 0x20 ; li r5, 0x34` @0x82633EB0) only when the call returns true.
            // [FX-SHOWTIME2 2026-09-24] FOLDED: the record is the game-state header's own
            // JustBouncedEvent (DWARF BrnGameEvents.h:2782-2790), the one ProcessGameEvents'
            // case 52 @0x823A3D74 reads back; the local RecentBounceEventPayload fork is gone.
            // Same bytes, same order, same 32-byte size (the asserts below the function).
            BrnGameState::GameStateModuleIO::JustBouncedEvent lBounce;
            if (lrCar.GetRecentBounce(&lBounce.miBounceChain, &lBounce.mbFromStationary,
                                      &lBounce.mbOnCar, &lBounce.mbBoostedBounce,
                                      &lBounce.mbGoodImpact,
                                      &lBounce.midImpactEntityId,
                                      &lBounce.mContactPoint))
            {
                lpOutputQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lBounce),
                                        BrnGameState::GameStateModuleIO::E_EVENT_JUST_BOUNCED,
                                        static_cast<s32>(sizeof(lBounce)));
            }

            // Event 53 (0x35): one-shot sixaxis-tilt notification. Consume the latch.
            const bool lbTiltApplied = msPlayerParams.mbSixaxisTiltApplied;   // byte_82FB848A
            msPlayerParams.mbSixaxisTiltApplied = false;
            if (lbTiltApplied)
            {
                // sp+0x50 -- one uninitialised-on-console byte. [FX-SHOWTIME2] the
                // header's JustAppliedExtraSpinEvent (DWARF :2801, no members), posted `li r6, 1 ;
                // li r5, 0x35` @0x82633EE0.
                BrnGameState::GameStateModuleIO::JustAppliedExtraSpinEvent lSpin;
                lpOutputQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSpin),
                                        BrnGameState::GameStateModuleIO::E_EVENT_JUST_APPLIED_EXTRA_SPIN,
                                        static_cast<s32>(sizeof(lSpin)));
            }
        }
    }

    // [FX-SHOWTIME2 2026-09-24] What ProcessAftertouchEvents writes, tied to the records the
    // game-state consumer reads (ProcessGameEvents cases 52 / 53 @0x823A3D74 / 0x823A3E10).
    CGS_STATIC_ASSERT((BrnGameState::GameStateModuleIO::E_EVENT_JUST_BOUNCED == 0x34),
                  "event 52: li r5, 0x34 @0x82633EB4");
    CGS_STATIC_ASSERT((sizeof(BrnGameState::GameStateModuleIO::JustBouncedEvent) == 0x20),
                  "event 52: li r6, 0x20 @0x82633EB0");
    CGS_STATIC_ASSERT((offsetof(BrnGameState::GameStateModuleIO::JustBouncedEvent, miBounceChain) == 0x00),
                  "GetRecentBounce r4 = record+0x00 (addi r4, r1, 0x70)");
    CGS_STATIC_ASSERT((offsetof(BrnGameState::GameStateModuleIO::JustBouncedEvent, mbFromStationary) == 0x04),
                  "GetRecentBounce r5 = record+0x04 (addi r5, r1, 0x74)");
    CGS_STATIC_ASSERT((offsetof(BrnGameState::GameStateModuleIO::JustBouncedEvent, mbOnCar) == 0x05),
                  "GetRecentBounce r6 = record+0x05 (addi r6, r1, 0x75)");
    CGS_STATIC_ASSERT((offsetof(BrnGameState::GameStateModuleIO::JustBouncedEvent, mbBoostedBounce) == 0x06),
                  "GetRecentBounce r7 = record+0x06 (addi r7, r1, 0x76)");
    CGS_STATIC_ASSERT((offsetof(BrnGameState::GameStateModuleIO::JustBouncedEvent, mbGoodImpact) == 0x07),
                  "GetRecentBounce r8 = record+0x07 (addi r8, r1, 0x77)");
    CGS_STATIC_ASSERT((offsetof(BrnGameState::GameStateModuleIO::JustBouncedEvent, midImpactEntityId) == 0x08
                      && sizeof(EntityId) == sizeof(s32)),
                  "GetRecentBounce r9 = record+0x08, one word (addi r9, r1, 0x78)");
    CGS_STATIC_ASSERT((offsetof(BrnGameState::GameStateModuleIO::JustBouncedEvent, mContactPoint) == 0x10),
                  "GetRecentBounce r10 = record+0x10 (addi r10, r1, 0x80)");
    CGS_STATIC_ASSERT((BrnGameState::GameStateModuleIO::E_EVENT_JUST_APPLIED_EXTRA_SPIN == 0x35),
                  "event 53: li r5, 0x35 @0x82633EE4");
    CGS_STATIC_ASSERT((sizeof(BrnGameState::GameStateModuleIO::JustAppliedExtraSpinEvent) == 1),
                  "event 53: li r6, 1 @0x82633EE0");

    // ------------------------------------------------------------------------------------
    // UpdateVehiclePhysics  @0x82644FA8  (1,038 insns) -- THE FORCE PRODUCER.
    // Stage order and every monitor bracket are the console's; the 13 handles line up 1:1
    // with the call sequence (gs_iUpdateVehicleImpactsPM .. gs_iUpdatePassBysPM).
    // ------------------------------------------------------------------------------------
    void VehicleManager::UpdateVehiclePhysics(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        BrnUpdateSet lUpdateSet,
        CgsSystem::Time& lrCurrentTime,
        f32 lfSimTimerTimeStep,
        f32 lfGameTimerTimeStep,
        const VehicleInputInterface* lpInputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        bool lbIsOnlineGameMode,
        CgsSceneManager::EntityId lWorldEntityId)
    {
        CGS_ASSERT(lpInputBufferStack != NULL, "lpInputBufferStack != NULL");                      // :2443
        CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");                    // :2444
        CGS_ASSERT(lpInputInterface != NULL, "lpInputInterface != NULL");                          // :2445
        CGS_ASSERT(lpVehicleOutputInterface != NULL, "lpVehicleOutputInterface != NULL");          // :2446
        CGS_ASSERT(lpRequestOutputInterface != NULL, "lpRequestOutputInterface != NULL");          // :2447
        CGS_ASSERT(lpVehicleManagerOutputInterface != NULL, "lpVehicleManagerOutputInterface != NULL"); // :2448
        CGS_ASSERT(lpDeformationInterface != NULL, "lpDeformationInterface != NULL");              // :2449

        // ---- STAGE: impacts (dword_82F2A14C) -------------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateVehicleImpactsPM);

        // Player pressed RESET -> re-read the surface-property table (asm 0x82645110..48;
        // gate = maRaceCarDrivers[player].mControls.mbReset, the +0x39 byte).
        if (mePlayerActiveRaceCarIndex != -1 &&
            maRaceCarDrivers[mePlayerActiveRaceCarIndex].mControls.mbReset)
        {
            ReadSurfaceProperties(gs_uSurfaceListKey);             // ld qword_82FB7F10
        }

        mCurrentTime = lrCurrentTime;   // stw +0 / stfs +4 at this+172364 (Time == {s32,f32})

        // Online-mode retune (asm 0x82645174..0x82645220). All five stores + both branch
        // constant sets verified against the image (0.4/0.6 online, 0.2/0.4 offline,
        // 2.0/0.4/16.0 common) -- the DWARF names the online pair
        // KF_ONLINE_MIN/MAX_SHUNT_MAGNITUDE (BrnVehicleManager.cpp:201/202).
        if (mbIsOnlineGameMode != lbIsOnlineGameMode)
        {
            mbIsOnlineGameMode = lbIsOnlineGameMode;
            if (lbIsOnlineGameMode)
            {
                mfMinShuntMagnitude = 0.4f;    // flt_8200473C
                mfMaxShuntMagnitude = 0.6f;    // flt_82004D00
            }
            else
            {
                mfMinShuntMagnitude = 0.2f;    // flt_82004744
                mfMaxShuntMagnitude = 0.4f;    // flt_8200473C
            }
            mfSlamEffectMaxMagnitude = 2.0f;   // flt_82001D9C
            mfSlamEffectMinMagnitude = 0.4f;   // flt_8200473C
            mfMaxSlamClosingXSpeed   = 16.0f;  // flt_82004000
        }

        // Deferred "crash the player next update" request (asm 0x82645224..0x82645298).
        if (mbCrashPlayerNextUpdate)
        {
            mbStopPlayerCrashing = false;                          // stbx 0 @+172306
            ForceRaceCarCrash(lpRequestOutputInterface, lpVehicleManagerOutputInterface,
                              lpVehicleOutputInterface, lpDeformationInterface,
                              mePlayerActiveRaceCarIndex);
            CGS_ASSERT(IsRaceCarCrashing(mePlayerActiveRaceCarIndex),
                       "Couldn't force player car to crash - this could have nasty consequences.");  // :2499
            mbCrashPlayerNextUpdate = false;

        }

        UpdateVehicleImpacts(lpInputInterface->GetImpactEventQueue(),   // input+141376
                             lpVehicleOutputInterface, lpRequestOutputInterface,
                             lpVehicleManagerOutputInterface, lpDeformationInterface);

        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateVehicleImpactsPM);

        // ---- STAGE: above-ground results (dword_82F2A150) ------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iProcessAboveGroundLTsPM);

        // Reset every live car's wheel/above-ground line-test latches (the loop the
        // Hex-Rays renders as the giant inlined-BitArray blob, asm 0x826452D0..0x82645628;
        // the per-car stores are SimpleVehiclePhysics::ResetAboveGroundTestResult, bodied
        // in its own TU off this exact inline).
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            maRaceCarVehicles[liCar].ResetAboveGroundTestResult();
        }

        mPhysicalTrafficManager.ResetAboveGroundTestResults();     // bl @0x82645634
        ProcessAboveGroundLineTestsResults(lpInputInterface->GetLineTestResults());  // input+0

        CgsDev::PerfMonCpu::StopMonitor(gs_iProcessAboveGroundLTsPM);

        // ---- STAGE: traction line tests (dword_82F2A154) -------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iTractionLTsPM);
        // RE-POINTED AGAIN 2026-08-11 (lifetime wave): TWO arguments, restoring what the
        // note removed. That note reasoned from the callee ("the 68 instructions never
        // touch r5") and concluded the parameter was fabricated -- but an unread argument is not
        // an absent one, and THIS call site is the proof: the console emits `mr r5, r29` at
        // 0x8264565C, loading r29 fresh from an incoming argument slot at 0x82645638. The PS3
        // DWARF types the pair (IOBufferStack*, const VehicleInputInterface*). Nothing
        // behavioural changes -- the callee still ignores it -- but the declaration stops
        // claiming something the image contradicts.
        EndVehicleTractionLineTests(lpInputBufferStack, lpInputInterface);   // r4=stack, r5=input
        CgsDev::PerfMonCpu::StopMonitor(gs_iTractionLTsPM);

        // ---- STAGE: fatal crashes (dword_82F2A178) -------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iCrashFatalPM);

        // The handbrake-turn crash test (asm 0x82645680..0x826457FC): the player car is
        // "sliding, not driving" when faster than 20 with its velocity more than 60 deg
        // off its nose -- force the crash. As shipped, the player index is read with NO
        // != -1 guard on this path (the gate bool is only ever set with a live player).
        if (mbCrashOnHandbrakeTurn)
        {
            const s32 liPlayer = mePlayerActiveRaceCarIndex;
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[liPlayer];

            const Vector3 lCarVel = lrPlayer.GetLinearVelocity();                 // RCP+0x50
            const Vector3 lCarDir = lrPlayer.GetTransform().zAxis;                // RCP+0x30

            // vpu::Magnitude carries the console's zero-length guard (vsel on |v|^2 == 0);
            // the vendor rwmath de-models the VecFloat broadcast returns to scalar f32.
            if (vpu::Magnitude(lCarVel) > KF_HANDBRAKE_CRASH_MIN_SPEED)
            {
                if (vpu::Dot(vpu::Normalize(lCarDir),
                             vpu::Normalize(lCarVel))
                        < KF_HANDBRAKE_CRASH_MAX_COS_DOT)
                {
                    ForceRaceCarCrash(lpRequestOutputInterface,
                                      lpVehicleManagerOutputInterface,
                                      lpVehicleOutputInterface, lpDeformationInterface,
                                      static_cast<EActiveRaceCarIndex>(liPlayer));
                }
            }
        }

        if (mbCrashRaceCarWhenFatal)                               // lbzx @+172452
        {
            CrashFatalRaceCars(lpRequestOutputInterface, lpVehicleManagerOutputInterface,
                               lpVehicleOutputInterface, lpDeformationInterface,
                               lWorldEntityId);
        }

        CgsDev::PerfMonCpu::StopMonitor(gs_iCrashFatalPM);

        // ---- STAGE: the per-car update loop (dword_82F2A17C) ---------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateRaceCarsPM);

        // Player car handed to the AI (demo/autopilot): drop any live slam/shunt effect
        // (asm 0x82645844..0x826458AC -- the same partial clear VehiclePhysics::Reset's
        // recipe documents: slam scalars zeroed, slam number -1; shunt direction+speed
        // zeroed, Life = -1.0f, SpeedIncreaseToQuit = 0).
        if (maRaceCarDrivers[mePlayerActiveRaceCarIndex].meDriverType == E_DRIVER_TYPE_AI)
        {
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[mePlayerActiveRaceCarIndex];
            lrPlayer.mSlamEffect.mfSteering         = 0.0f;    // +0x1114
            lrPlayer.mSlamEffect.mfOriginalSteering = 0.0f;    // +0x1118
            lrPlayer.mSlamEffect.mfSlamLife         = 0.0f;    // +0x111C
            lrPlayer.mSlamEffect.mfTotalSlamTime    = 0.0f;    // +0x1120
            lrPlayer.mSlamEffect.mi8SlamNumber      = -1;      // stb @+0x1128
            const Vector3Plus lvZeroShunt(0.0f, 0.0f, 0.0f, 0.0f);
            lrPlayer.mShuntEffect.mDirectionPlusDesiredSpeed = lvZeroShunt; // stvx 0 @+0x1130
            lrPlayer.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.SetX(-1.0f);  // vrlimi 8 @+0x1140
            lrPlayer.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.SetY(0.0f);   // vrlimi 4 @+0x1140
        }

        // Freeze the player car while the update set says "menus own the frame" (bits
        // 0x200|0x800) or the debug override is latched (asm 0x826458B0..0x826458EC).
        if (mePlayerActiveRaceCarIndex != -1)
        {
            const bool lbForceFreezePlayerCar =
                (lUpdateSet & 0xA00) != 0 || gs_bOverrideForceFrozen;
            maRaceCarVehicles[mePlayerActiveRaceCarIndex].SetForceFrozen(lbForceFreezePlayerCar);
        }

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            // r25: player-and-additive flag, computed once per car (asm 0x826459A8..D4).
            const bool lbIsPlayerAftertouchAdditive =
                mbAftertouchIsForceAdditive && (liCar == mePlayerActiveRaceCarIndex);

            // -- driver tick (dword_82F2A180) --
            CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateDriversPM);
            maRaceCarDrivers[liCar].UpdateVehicle(&maRaceCarVehicles[liCar]);
            CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateDriversPM);

            // -- car physics tick (dword_82F2A184): the virtual slot +0xC dispatch into
            //    RaceCarPhysics::Update -- THE seam into the 54 force leaves. Args are the
            //    DWARF signature's (VehiclePhysics.h:1084), decoded off the call site
            //    (asm 0x82645A10..0x82645A5C):
            //      camera matrix   = &mCameraMatrix (r4 = this+172240)
            //      controls        = &maRaceCarDrivers[car].mControls (r5, the +0x40 record)
            //      impact time     = mbImpactTime (r6 = lbz this+172304)
            //      aftertouch add  = r25 (player && mbAftertouchIsForceAdditive)
            //      showtime        = (meShowtimeBehaviour == 2) (cntlzw/extrwi idiom, r8)
            //      random          = mRandom (r9 = this+16)
            //      v1/v2           = splat(lfSimTimerTimeStep) / splat(lfGameTimerTimeStep)
            CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateVehiclesPM);
            const VecFloat lvSimTimeStep(lfSimTimerTimeStep);
            const VecFloat lvGameTimeStep(lfGameTimerTimeStep);
            maRaceCarVehicles[liCar].Update(
                lvSimTimeStep,
                lvGameTimeStep,
                &mCameraMatrix,
                maRaceCarDrivers[liCar].GetControls(),
                mbImpactTime,
                lbIsPlayerAftertouchAdditive,
                meShowtimeBehaviour == BrnGameState::E_SHOWTIME_MODE_ON_SIXAXIS,
                mRandom);
            CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateVehiclesPM);

            // Per-car debug component tick (asm 0x82645A68..78; f1 = lfSimTimerTimeStep).
            maRaceCarDebugComponent[liCar].Update(lfSimTimerTimeStep);

            // -- rigid-body change on RESET (dword_82F2A188) --
            CgsDev::PerfMonCpu::StartMonitor(gs_iRBChangePM);
            if (maRaceCarDrivers[liCar].mControls.mbReset)         // lbz +0x79(record)
            {
                // SHIPPED-DEAD COMPUTATION, reconstructed as shipped and FLAGGED, not
                // "fixed": the console recomputes the car's inverse box inertia here (asm
                // 0x82645A90..0x82645BD4 -- extent = (mHalfExtent + |COM offset|) * 2, the
                // m/12 box formula, three reciprocal-refined lanes assembled into v127) --
                // and then NEITHER STORES NOR PASSES the result. v127 is callee-saved and
                // is restored in the epilogue; no memory write exists. The DWARF hint block
                // (BrnVehicleManager.cpp:2618-2622 lInertia/lInertialTensor/lBoxExtent/
                // lfMass/lfOne + operator/) confirms these were LOCALS. The consuming call
                // was evidently compiled out of the retail build. Locals only, same as the
                // console; the PC compiler is free to fold them away -- byte-effect nil
                // either way.
                RaceCarPhysics& lrCar = maRaceCarVehicles[liCar];
                const Vector3 lBoxExtent = vpu::Mult(
                    vpu::Add(lrCar.GetHalfExtent(),
                             vpu::Abs(lrCar.mpAttribs->mBaseAttribs.mCOMOffset)),  // attribs+0x20
                    2.0f);                                                          // flt_82001D9C
                const f32 lfMassOverTwelve =
                    lrCar.GetMass()   /* VecFloat == Vector4 splat; lane 0 */ * (1.0f / 12.0f);                // flt_82094724
                const Vector3 lInertialTensor(                            // lfOne / (m/12 * (b^2+c^2), ...)
                    1.0f / (lfMassOverTwelve * (lBoxExtent.GetY() * lBoxExtent.GetY() + lBoxExtent.GetZ() * lBoxExtent.GetZ())),
                    1.0f / (lfMassOverTwelve * (lBoxExtent.GetX() * lBoxExtent.GetX() + lBoxExtent.GetZ() * lBoxExtent.GetZ())),
                    1.0f / (lfMassOverTwelve * (lBoxExtent.GetX() * lBoxExtent.GetX() + lBoxExtent.GetY() * lBoxExtent.GetY())));
                (void)lInertialTensor;   // v127 -- never stored, never passed (see FLAG above)
            }
            CgsDev::PerfMonCpu::StopMonitor(gs_iRBChangePM);

            // -- aftertouch events (dword_82F2A18C) --
            CgsDev::PerfMonCpu::StartMonitor(gs_iAfterTouchPM);
            if (lbIsPlayerAftertouchAdditive)
            {
                ProcessAftertouchEvents(liCar,
                                        lpVehicleOutputInterface->GetGameEventQueue());  // veh+0x65F0
            }
            CgsDev::PerfMonCpu::StopMonitor(gs_iAfterTouchPM);
        }

        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateRaceCarsPM);

        // ---- STAGE: traffic physics (dword_82F2A190) -----------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateTrafficPM);
        mPhysicalTrafficManager.UpdateTrafficPhysics(
            lfSimTimerTimeStep, lfGameTimerTimeStep,               // f1 / f2
            &mCameraMatrix,                                        // r6 = this+172240
            mbImpactTime,                                          // r7 = lbzx this+172304
            false);                                                // r8 = 0
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateTrafficPM);

        // ---- STAGE: aggressive driving (dword_82F2A194) --------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateAggressiveDrivingPM);
        UpdateAggressiveDriving(lfSimTimerTimeStep, lpRequestOutputInterface,
                                lpVehicleManagerOutputInterface, lpVehicleOutputInterface,
                                lpDeformationInterface);
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateAggressiveDrivingPM);

        // ---- STAGE: crashes (dword_82F2A198) -------------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateCrashesPM);
        UpdateCrashes(lfSimTimerTimeStep);
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateCrashesPM);

        // ---- STAGE: traffic pass-bys (dword_82F2A19C) ----------------------------------
        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdatePassBysPM);
        // v1 = the player car's position row (lvx this+5216*player+0x780 == RCP+0x40 ==
        // mTransform.wAxis). As shipped: no player != -1 guard (index -1 reads the tail
        // of the drivers array on the console; reconstructed verbatim).
        const Vector3 lvPlayerPosition = maRaceCarVehicles[mePlayerActiveRaceCarIndex].GetTransform().wAxis;
        mPhysicalTrafficManager.PassNearbyCrashingTrafficIdsToRaceCarModule(
            lpVehicleManagerOutputInterface, lvPlayerPosition);
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdatePassBysPM);

        // ---- the stationary (car-select) wheel pose (asm 0x82645EDC..0x82645FA0) -------
        // SteeringAngle.x = -MaxSteeringAngle * scale * DegToRad, where scale is -1.3 for
        // mode 0 (wheels turned one way), +1.3 (rodata KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE)
        // for mode 1 (the other way), and mode 2 (the Construct default) leaves the wheels
        // alone. The lane write is VehiclePhysics::OverrideWheelAngle (DWARF hint
        // OverrideWheelAngle + GetMaxSteeringAngle + SetPackedSteeringAngle + SetX).
        if (meStationaryPlayerWheelAngle == 0)
        {
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[mePlayerActiveRaceCarIndex];
            const f32 lfAngle =
                -lrPlayer.GetMaxSteeringAngle() * (-KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE) // flt_8209D0D4 == -1.3
                * KF_DEG_TO_RAD;
            const VecFloat lvAngle(lfAngle);
            lrPlayer.OverrideWheelAngle(lvAngle);
        }
        else if (meStationaryPlayerWheelAngle == 1)
        {
            RaceCarPhysics& lrPlayer = maRaceCarVehicles[mePlayerActiveRaceCarIndex];
            const f32 lfAngle =
                -lrPlayer.GetMaxSteeringAngle() * KF_WHEEL_ANGLE_IN_CAR_SELECT_SCALE   // flt_8208FA8C == 1.3
                * KF_DEG_TO_RAD;
            const VecFloat lvAngle(lfAngle);
            lrPlayer.OverrideWheelAngle(lvAngle);
        }

        // ---- publish the player's wheel force-feedback spring (asm 0x82645FA4..C4) -----
        // Two-word copy RCP+0x13D0 -> manager-out+0x874; DWARF names the accessor
        // SetPlayerWheelFFSpring (BrnVehicleOutputInterface.h:256). As shipped: no
        // player != -1 guard here either.
        lpVehicleManagerOutputInterface->SetPlayerWheelFFSpring(
            maRaceCarVehicles[mePlayerActiveRaceCarIndex].mWheelFFSpring);
    }

    // ============================================================================================
    // VehicleManager::UpdateVehiclePhysicsPostSimulation  @0x826426E0
    //
    // Breaker is authoritative for this late-merge body and its StuntOffencesManager call ABI:
    // r4/r5/r6/r7/r8 are race bodies, drivers, active index, used-bitset, and game-event queue.
    // The VecFloat conversion is local to each vehicle call; the public manager ABI remains f32.
    // ============================================================================================
    void VehicleManager::UpdateVehiclePhysicsPostSimulation(
        const VehicleInputInterface* lpInputInterface,
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutputBuffer,
        f32 lfTimeStep,
        BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue)
    {
        const u32 luPlayerIndex = static_cast<u32>(mePlayerActiveRaceCarIndex);
        const bool lbPlayerCarUsed = mUsedRaceCars.IsBitSet(luPlayerIndex);

        if (lbPlayerCarUsed)
            DoPlayerTractionLineTestsPostSimulation(lpInputInterface, lfTimeStep);

        const VecFloat lvfTimeStep(lfTimeStep);
        for (s32 liRaceCar = mUsedRaceCars.GetFirstNonZeroBit();
             liRaceCar != CgsContainers::BitArray<8>::KI_INVALID_BITINDEX;
             liRaceCar = mUsedRaceCars.GetNextNonZeroBit(liRaceCar))
        {
            maRaceCarVehicles[liRaceCar].UpdatePostSimulation(lvfTimeStep);
            maRaceCarDrivers[liRaceCar].ClearSnappedThisFrame();
        }

        if (lbPlayerCarUsed)
            DoPlayerStuckLineTests(lpInputInterface);

        CgsDev::PerfMonCpu::StartMonitor(gs_iUpdateStuntOffencesPM);
        mStuntOffencesManager.Update(maRaceCarVehicles, maRaceCarDrivers,
                                     mePlayerActiveRaceCarIndex, &mUsedRaceCars,
                                     lpGameEventQueue, lfTimeStep);
        CgsDev::PerfMonCpu::StopMonitor(gs_iUpdateStuntOffencesPM);

        mPhysicalTrafficManager.UpdateTrafficPhysicsPostSimulation(lpSimOutputBuffer, lfTimeStep);
    }
}
}

// FOLDED FROM BrnVehicleManager_ValidateRaceCarWorldContact.cpp
// ============================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ValidateRaceCarWorldContact.cpp
//
// BrnPhysics::Vehicle::VehicleManager::ValidateRaceCarWorldContact @0x825C6088 (988 insns)
// -- THE VALIDATION WHALE (walls leg 3, 2026-08-14). PS3 DecFIGS 0x70AB20 (1523; the mangle
// is the signature authority, DWARF h:938). Slice TU: the home BrnVehicleManager.cpp (the
// asserts' own file, :7419..:7624) is still unmounted -- RaceCarPhysics_Construct precedent.
//
// Validate ONE race-car-vs-world potential contact (side A == the car, side B == the world
// triangle -- DoRaceCarWorldContactValidation swapped it before calling). The function may
// REWRITE the contact in place (the wall-normal flatten; the wheel/bottom-plane projection)
// and returns whether it survives into the Validated queue.
//
// EVERY CONSTANT BELOW IS MEASURED, none inferred (walls leg 3, x360rd out of the unpacked
// .i64 image; addresses cited per value). The two file-scope values are namespace globals on
// the console; the function-locals are its guarded function-statics (guard dword_82FBA030).
//
// METHOD NOTE: the two VMX-dense blocks (the bottom-plane rewrite and the final AABB
// acceptance) were derived from the RAW INSTRUCTION WORDS with the leg-2-validated field
// decoders (sim_kernel field layouts), NOT from the IDA operand text -- the +32 per-operand-
// field hazard and one vperm operand-order trap live exactly there. Where the raw words
// disagree with a naive reading, the words won (see the CONSOLE QUIRK note in the body).
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                               // gpDebugPrint / gxMessageFilterFlags (debug-draw gates)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"         // TriangleCacheInterface (GetCache / GetNumCachedTriangleBatches)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"            // PotentialContact
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                    // Triangle4 (+ AOSTriangle)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"             // RaceCarPhysics (GetHeightAboveRoad + the friend-granted member reads)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h" // Vehicle::DebugComponent (SetLastWallTriangle)
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"           // VehicleManagerDebugComponent (mbRenderWallContacts / mbRenderGroundContacts)

#include "rw/math/vpu/vector3_operation.h"                                               // vpu::{Dot, Subtract, Normalize}
#include <cstdlib>                                                                       // getenv -- the [kerb] probe's opt-in latch

namespace BrnPhysics
{
namespace Vehicle
{
    // MEASURED namespace globals (console .data):
    //   gfGroundContactCullHeight: default .data @0x82F2A148 == 0x3ECCCCCD == 0.4f
    //     (image-read; same ordinary-initialised .data bank as the flt_82F2A2xx seed constants).
    //     VehicleManagerDebugComponent::OnActivate exposes this storage as a writable slider.
    //   KVF_WALL_NORMAL_DOT_THRESHOLD: DYNAMIC-INIT (zero in the image at 0x82FB7F20). Its
    //     initialiser was FOUND AND READ this wave @0x82C5BBD8..0x82C5BBF8:
    //       lis r11,0x8200 ; lfs f0,0x1DA0(r11)   <- flt_82001DA0 == 0.5f
    //       lis r11,0x82FB ; ... vspltw ; stvx128 -> 0x82FB7F20
    //     == splat(0.5f). The PS3 cross-witness: _Z41__static_initialization_and_destruction
    //     @0x6C2CCC splat-initialises the same-named global. Contacts whose |normal.y| is
    //     below this are flattened to pure horizontal ("wall") normals.
    namespace
    {
        const f32 KF_WALL_NORMAL_DOT_THRESHOLD     = 0.5f;   // KVF_WALL_NORMAL_DOT_THRESHOLD (init @0x82C5BBD8, value == flt_82001DA0)
        const f32 KF_MPS_TO_MPH                    = 2.236936330795288f; // KF_MPS_TO_MPH rodata @0x8208F820 (named X360 symbol; PS3 identical)
        const f32 KF_NORMAL_ZERO_EPSILON           = 1.1920928955078125e-07f; // stru_8208F620 lane 0 == 0x34000000 == FLT_EPSILON
        const f32 KF_UP_DOT_GROUND_NORMAL_MIN      = 0.8f;   // flt_8208F9C8 == 0x3F4CCCCD (the above-ground-plane gate)
        const f32 KF_AABB_MARGIN_XY                = 0.1f;   // flt_82004014 == 0x3DCCCCCD (x/y lanes of the margin vector; the z lane is flt_82001CC0 == 0.0f)
        const f32 KF_LATERAL_RECHECK_MARGIN        = 0.2f;   // flt_82004744 == 0x3E4CCCCD (the velocity-expanded lateral band)
        const f32 KF_FINAL_ACCEPT_MAX_SPEED_MPH    = 25.0f;  // flt_82004FD8 (f29, shared with the clearance gate)

        // Per-lane finiteness of a normal's xyz (the console's vspltw + vcmpeqfp self-compare
        // triple, used four times in this body).
        inline bool IsValidVec3Lanes(const Vector3& lrV)
        {
            return lrV.mV.v[0] == lrV.mV.v[0] && lrV.mV.v[1] == lrV.mV.v[1] && lrV.mV.v[2] == lrV.mV.v[2];
        }
    }

    // ---- [kerb] PC bring-up instrument -- DELETE-WHEN the kerb response is proven 1:1 -------------
    // OPT-IN (BRN_KERB_PROBE=1, flow_run.ps1 -DiagEnv BRN_KERB_PROBE=1). NOT console code. Off by
    // default: the latch reads 0 once and every print is unreachable thereafter, so a default run
    // and every golden gate stay byte-identical to a build without it.
    //
    // WHY. The owner's kerb symptom ("the car reacts way too much to the curb") is decided in THIS
    // function: every race-car-vs-world contact passes through it, and the two ground culls are
    // what turn a kerb face into a wheel-plane push instead of a wall hit. The [drift]/[cvalid]
    // probes sample 1 frame in 60 and print only counts -- a kerb crossing is ~10 frames long, so
    // they cannot see it. This prints BOTH SIDES of every gate the function takes, per contact,
    // every frame: the incoming normal, the triangle's vertex heights vs kvfMaxCurbHeight, the
    // curb/wall verdict, both cull distances vs the scaled cull height, the above-ground normal and
    // its up-dot vs 0.8, the rewritten normal/point, and the verdict. Three tags share one frame
    // counter (bumped by DoRaceCarWorldContactValidation, the per-frame caller in the sibling TU):
    //   [kerb]      one line per contact, this function          (this TU)
    //   [kerb-car]  one line per moving car per frame             (BrnVehicleManagerContactGeneration.cpp)
    //   [kerb-imp]  one line per world impulse the solver applies (BrnDeformableObject.cpp)
    // Each tag is budget-limited and SAYS SO when the budget runs out -- a silent stop reads
    // exactly like "the kerb never touched anything".
    //
    // MEASURED 2026-09-02 (kerb waves r1/r2, Waterfront pavement kerb x~3391.5 z -1620..-1660, a
    // 0.15 m step; runs scratch/flow_run/kerbw_r1, kerbw_r2, BRN_KERB_PROBE=1, every frame):
    //   * The kerb FACE triangle (triN (-0.999, 0, 0.039), vertex y 0.27/0.285/0.12) classifies
    //     CURB on every contact -- h = -0.000 / -0.000 / -0.150 against kvfMaxCurbHeight 0.25.
    //     The harvest normal arrives TILTED, not the face normal: (-0.48, 0.87, 0.02) and
    //     (-0.71, 0.71, 0.03) -- the sensor sphere rides the kerb's top edge.
    //   * BOTH culls fire on every kerb-face contact: (a) dA 0.06..0.08 < wpH 0.0 + cullH 0.4
    //     (mbMinWheelDistValid 1), (b) upDotAg 0.9992 > 0.8 and dB 0.000002..0.000004 < 0.4.
    //     The contact is re-pointed to the body Up axis and the solver then applies NO impulse
    //     ([kerb-imp] is empty through the whole crossing) -- the kerb is NOT treated as a wall.
    //   * Cull (a) alone already catches every one of them, and its inputs (CalculateNewWheelPlane,
    //     e312c005 2026-08-07; this whale 43bb9c20 2026-08-14) predate the above-ground-ray fix
    //     (c6cb403f). So "the missing ray disabled the second cull and kerbs became walls" is
    //     REFUTED by chronology: pre-fix, cull (a) was re-pointing them just the same.
    //   * The car's response through a 25-degree, 30-38 mph forward crossing (r2 f1013-1040): body
    //     +0.10 m per axle step, roll rate peak +0.48 / -0.58 rad/s, pitch |0.10| rad/s, velocity
    //     heading 25.1 -> 25.8 deg, NO speed loss (accelerating 30.2 -> 39.7 mph throughout).
    //   * The one sharp event on film is NOT in this function: reversing over the same kerb at
    //     106 mph (r1 f2578-2588) the car lost 6.2 mph in ONE frame and yawed at 0.47 rad/s at
    //     the exact frame the FRONT wheel mounted (traction hit y 0.125 -> 0.276), with zero body
    //     impulses -- that is the wheel/suspension path (ApplyWheelWeight -> UpdateSuspensionSprings
    //     -> tyre forces), not the world-contact validation. Open lead, un-audited.
    //   * CONTROLS (kerbw_r4 / r4B / r4AB, one deterministic recipe, an uncommitted switch that
    //     discards a cull's verdict AFTER it has printed): with cull (b) discarded the crossing is
    //     frame-identical to the shipped build (52/52 re-pointed, 0 impulses, same mounts, same
    //     roll peak 0.35 rad/s) -- i.e. the pre-ray-fix state was already correct here. With BOTH
    //     discarded the kerb face reaches the solver as a wall: 51 impulses of 950..1121 along
    //     (-1.00, 0.00, 0.04), the wheels never mount (y 0.444 -> 0.445), the car is steered along
    //     the kerb 23 -> 16 deg. That is the failure the culls prevent, and NOT the owner's symptom.
    //
    // ==============================================================================================
    // MEASURED 2026-09-02, WAVE 2 (runs kerbX_r1..r4). Two INDEPENDENT kerb hypotheses tested; both
    // REFUTED. Nothing here changed a constant or a gate -- the whole wave is measurement.
    //
    // (A) "A WHEEL IS A CUBE AND A CUBE CANNOT CLIMB A STEP, SO THE GAME MUST HAVE A HOP THRESHOLD
    //     THE DECOMP IS MISSING." The premise is wrong on this build. An ATTACHED wheel's
    //     world-collision proxy is a SPHERE, written every frame by DeformableObject::
    //     UpdatePostPhysics @0x825DFEB0 into maWorldSensorSpheres[nSens + wheel] (radius =
    //     WheelSpec::mScale.x / 2; see the [kerb-wsph] banner in BrnDeformableObject_Update.cpp),
    //     and GetSpheresForCar hands the world test all nSens + 4 of them. MEASURED LIVE: nSens 20,
    //     wheel spheres at indices 20..23, radius 0.128 / 0.130 / 0.150 m, while the tyre's actual
    //     rolling radius is 0.331 / 0.342 m ([wsus-attr] "rad"). So the wheel primitive is a small
    //     hub-level sphere whose lowest point rides ~0.25 m ABOVE the road: it is not what meets the
    //     kerb at all. In four runs the wheel spheres produced 319 world contacts, none of them on a
    //     kerb face -- the kerb is met by the BODY-SHELL deformation sensors (indices 4..13).
    //
    // (B) "THE THRESHOLD IS THE GROUND CONTACT CULL HEIGHT AND IT IS 0.4, NOT 0.25." Both numbers
    //     are real, they are DIFFERENT constants, and this TU already has both right:
    //       * "Ground Contact Cull Height" is flt_82F2A148. The console NAMES IT ITSELF --
    //         VehicleManagerDebugComponent::OnActivate @0x825B5D90 calls
    //           CgsDev::DebugComponent::RegisterVariable(this, &flt_82F2A148, "Crashes",
    //                                                    "Ground Contact Cull Height");
    //           SetRange(&flt_82F2A148, 0.0, 1.0);  SetStep(&flt_82F2A148, 0.01);
    //         which is exactly the debug slider a retail-build experimenter would move. Image byte
    //         value (x360rd) 0x3ECCCCCD == 0.4 is the default. Its two live consumers are this
    //         function and HandleTrafficCarWorldPotentialContact @0x8263F0F0
    //         (BrnVehicleManager_TrafficCrashArms.cpp:77); the menu registration is its runtime writer.
    //       * kvfMaxCurbHeight is a SEPARATE literal. The load is unambiguous in the asm --
    //         0x825C6684  lfs f0, (flt_8208F834 - 0x8208F9C8)(r14)  -- and x360rd reads
    //         0x8208F834 == 0x3E800000 == 0.25. (The other seven functions that reference that word
    //         are unrelated 0.25s: GetAftertouchValues, Engine::Update, UpdateFreezing,
    //         ApplyEngineForces, SetupAttribsForAI, VehicleManager::Construct.) So "we picked the
    //         wrong pool entry" is refuted from the image, not argued.
    //     The two are not interchangeable: 0.25 is compared against TRIANGLE VERTEX HEIGHTS ABOVE
    //     THE ROAD (curb vs wall), 0.4 against a DISTANCE from the wheel/above-ground plane (cull or
    //     not). Lowering the 0.4 makes cull (a) miss and kerb faces reach the solver -- which is why
    //     moving that slider makes a retail car catch on everything, and is evidence FOR the
    //     mechanism, not against our value.
    //
    // (C) THE KERB IS HOPPED, MEASURED TWICE, AT BOTH ENDS OF THE SPEED RAMP (Waterfront kerb,
    //     x ~3391.5, face triN (-1.000, 0.000, 0.023), top y 0.24, road y 0.075..0.081 -> a
    //     0.158..0.165 m step):
    //       kerbX_r2  60 mph, ~10 deg oblique: ~200 contacts f1140-1175, every one curb 1 / wall 0,
    //                 cull (a) fires (dA 0.13..0.16 < 0.4), re-pointed, ZERO impulses, speed
    //                 58.2 -> 64.3 mph (still ACCELERATING), yaw 0.01 rad/s, roll +-0.5 rad/s.
    //       kerbX_r4  24 mph, HEAD-ON (perpendicular): kerb-face contacts f1007-1022 from sensors
    //                 4/6/8/10..13, every one curb 1 / wall 0, dA 0.038..0.095 < cullH, re-pointed,
    //                 ZERO impulses; front wheels mount at f1010 (road contact y 0.081 -> 0.239);
    //                 speed 22.4 -> 28.9 mph, again NO loss. At f1034 the car meets the building
    //                 beyond the pavement: wall 1, 3 impulses, 28.6 -> 10.6 mph. Correct, and the
    //                 contrast is the control -- this function CAN stop a car, and does, for a wall.
    //     ⭐ The speed ramp is corroborated to three decimals in the same run: cullH 0.372 at
    //     23.95 mph, 0.381 at 24.27, 0.389 at 24.59, 0.400 at >= 25 -- i.e. 0.4*(v-10)/15 exactly.
    //
    // (D) mi8NumWorldCollisions "3..15 on ~7% of flat-tarmac frames" is EXPLAINED, not a bug:
    //     [wsus] nwc over run r4 is 0 on 59322 steps and 6 on 8019 (11.8% non-zero), and the
    //     matching [kerb] lines show them to be body-shell sensor spheres touching the ROAD
    //     (triN ~ (0, 1, 0)), classified curb, culled to the wheel plane. That is what
    //     gfGroundContactCullHeight exists for; the console produces them too.
    //     ⚠️ CORRECTED 2026-09-03: this used to end "culled to the wheel plane, NO IMPULSE", and
    //     that half cannot be true. mi8NumWorldCollisions (+0x1353) has exactly three writers in
    //     the whole tree -- ApplyWallContactImpulse @0x825FEA18 (unconditional),
    //     ApplyCrashedContactImpulse @0x825D4D50 and ApplyShowtimeContactImpulse @0x825D4E00
    //     (both on their zero-response arm) -- and ALL THREE bank an impulse through
    //     AddWorldSpaceImpulse before returning; the counter is zeroed at the top of
    //     DeformableObject::UpdateContacts @0x826478B0's apply phase. So `nwc == 6` MEANS six
    //     world impulses were applied that step. They are the culled contacts re-pointed to the
    //     car's up axis, i.e. the ground pushing the car UP -- benign, and exactly what the cull
    //     is for -- but they are impulses, and [kerb-imp] prints one line for each of them.
    //     ⭐ AND THE CORRECTION HAS A CONSEQUENCE, which is why it is worth the paragraph:
    //     UpdateSuspensionPostSimulation @0x825F6BB0 gates its inanimate-world RECOVERY IMPULSE
    //     arm on `mi8NumWorldCollisions == 0`. So the two arms ALTERNATE on flat tarmac: on the
    //     11.8% of steps the world-contact arm fires, the recovery arm is suppressed; on the
    //     other 88.2% the recovery arm is the only thing applying impulses at all.
    //
    // (E) A KERB TALLER THAN 0.25 m -- TESTED 2026-09-02, AND `wall` IS THE CONSOLE'S OWN ANSWER.
    //     The previous wave left this as the leading remaining candidate for "the curbs are still
    //     weird". It is now measured, and separately settled from the asm.
    //     THE EDGE (Waterfront, found by driving, not by hunting): x ~3386.8, z ~-1724.5, face
    //     triN (-0.951, 0.000, 0.310) -- a vertical pavement edge with a fence behind it. Triangle
    //     vertex world y: road side -0.240, an intermediate ledge -0.015, pavement top +0.225, i.e.
    //     a TWO-STEP rise of 0.465 m from the road; the fence's base triangle shares it at +2.535.
    //     BOTH SIDES OF THE COMPARE, run kerbT_30, f1056, 44.7 mph, head-on (approach heading 108
    //     deg == along -triN), sensors 5/7/9:
    //       h 0.456 / 0.690 / 0.217  vs maxCurb 0.250  -> curb 0  wall 1
    //       h 0.450 / 0.217 / 0.690  vs maxCurb 0.250  -> curb 0  wall 1
    //       h 0.217 / 0.450 / -0.008 vs maxCurb 0.250  -> curb 0  wall 1   (ONE vertex over)
    //       h 0.690 / 0.456 / 2.999  vs maxCurb 0.250  -> curb 0  wall 1   (the fence)
    //     cullA 0 / cullB 0 on every one -- and dA was 0.128 / 0.141 / 0.224 / 0.250 / 0.277 on five
    //     of them, i.e. BELOW cullH 0.400, so cull (a) WOULD have fired: it is the wall flag that
    //     suppresses it. repointed 0, accept 1.
    //     THE EFFECT: 44.7 -> 37.7 -> 6.8 mph in about half a second, the car does NOT climb it, and
    //     it sits against the face at 2.4 mph on full throttle for the rest of the run. Set that
    //     against (C): the 0.16 m kerb is hopped at 24 AND 60 mph with ZERO impulses and no loss.
    //     THE SAME EDGE OBLIQUELY, run st_scoutB f1108, 33.5 mph, ~18 deg closing angle: identical
    //     h/curb/wall/cull values, accepted unmodified -- and the car did NOT lose speed (33.5 ->
    //     36.0, still accelerating). So the wall CLASSIFICATION alone does not stop a car; a
    //     near-normal closing angle into a wall-classified edge does.
    //
    //     ⭐ IS `wall` THE CONSOLE'S OWN VERDICT, OR IS OUR TRANSCRIPTION MISSING AN ARM? It is the
    //     console's, settled from the image rather than from feel, three ways:
    //       1. r27 (curb, 0x825C6750) and r28 (wall, 0x825C6794) are consumed at EXACTLY four sites
    //          in the whole 988-instruction function:
    //            0x825C68B4  r27 -> the mbRenderGroundContacts debug draw
    //            0x825C69A0  r29 (== r28) -> SetLastWallTriangle into the debug component
    //            0x825C6A9C  r29 -> suppress cull (a)
    //            0x825C6B60  r29 -> suppress cull (b)
    //          At 0x825C6B70 r29 is reloaded with the constant 0x30 and is dead as a boolean
    //          thereafter. There is NO step-up arm, no ride-over, no swept test and no speed term
    //          keyed on the classification.
    //       2. The sole caller, DoRaceCarWorldContactValidation @0x825EB6C8, adds none either: swap
    //          entity order, call this, append the survivors to queue [6].
    //       3. THE CONSOLE NAMES ONLY TWO CLASSES. VehicleManagerDebugComponent::OnActivate
    //          @0x825B5D90 registers 92 debug variables; its world-contact ones are exactly
    //          "Ground Contact Cull Height", "Draw wall contacts", "Draw ground contacts",
    //          "Head-on world crash factor", "Side-on world crash factor", "Modify traffic
    //          contacts", "Draw last RC-traffic contact", "Draw last crash contact", "Draw World
    //          Contact Spies", "Num traction tests" and the "Reset Rays..." trio. Not one of those
    //          92 -- nor of Vehicle::DebugComponent's 28 (0x82620730), DeformationDebugComponent's
    //          58 (0x82623198), CollisionDebugComponent's 4 (0x822A8B80) or
    //          TriangleCollisionDebugComponent's 17 (0x828AAA68) -- is kerb-, step- or ride-related.
    //          The retail tuning surface has TWO world-contact classes and this function produces
    //          exactly those two. (Same trick that named the 0.4: the registrations name things.)
    //     ⇒ The owner's "the curbs are still weird" is NOT an unimplemented hop threshold above
    //       0.25 m. A raised edge more than 0.25 m above the car's wheel plane is a wall on the
    //       console too, and it stops the car on the console too.
    //     ⚠️ WHAT THIS EVIDENCE CANNOT DISTINGUISH. The stopped car's contact set at f1056 holds
    //       BOTH the kerb triangles (vy -0.240 .. +0.225) and the base triangle of the fence behind
    //       the pavement (vy 2.535). The kerb triangles alone classify as wall, so the
    //       classification result stands either way -- but this run cannot apportion the 44.7 ->
    //       6.8 mph between the kerb face and the fence. A head-on crossing into a >0.25 m edge
    //       with NOTHING behind it is still not measured.
    //     ⭐ AND THE FRAMES WERE OPENED, because the last wave lost a run to exactly this trap.
    //       kerbT_30 dumped 325 frames; presents 1700-1950 (the whole approach and impact) show an
    //       empty road, the raised pavement, the fence and a "NO TOWING" sign, and NO traffic car
    //       anywhere. Eight of those frames are kept beside the run under frames/keep/.
    //
    // ==============================================================================================
    // (F) THE ONE-STEP SPEED LOSS, RE-READ FROM THE COMMITTED st_scoutB LOG (2026-09-03). No new
    //     run: every number below is in scratch/flow_run/st_scoutB/BrnGame.log, which the previous
    //     wave already captured. Two things it was believed to show are wrong.
    //
    //     ⭐ (F1) "28.7 -> 8.1 mph IN ONE STEP at f1012->f1013" IS A DISPLAY ARTEFACT. The mph
    //     field in [kerb]/[kerb-car] is mfSpeedMPH, a CACHE recomputed once per step, and it lags
    //     the live velocity by one step. The f1012 line says so on its own face:
    //         [kerb-car] f 1012 ... vel -0.228203 -0.593689 -3.206746 mph 28.650387
    //     -- a 3.26 m/s velocity (7.3 mph) printed beside a 28.65 mph label. Read the VELOCITY:
    //         f1011  vel (-0.166688, -0.595939, -12.654400)   pos (3384.4275, 0.21091, -1709.7499)
    //         f1012  vel (-0.228203, -0.593689,  -3.206746)   pos (3384.4231, 0.20468, -1709.7620)
    //     The real event is f1011 -> f1012: 9.45 m/s lost in ONE step, essentially all of it
    //     along +z, i.e. straight against travel. The mph column then catches up at f1013 (7.997)
    //     and that catch-up is what four reads mistook for the event.
    //
    //     ⛔ (F2) THE DEFORMATION-ARM LEAD IS REFUTED FOR THIS EVENT. The standing hypothesis was
    //     that a culled contact still feeds the deformation route, and the evidence offered was
    //     "[kerb-imp] recorded impulses of 590 and 256 N.s". Those lines are at f1013 --
    //     grep says there is NO [kerb-imp] line at f1011 or f1012 at all -- and their normals are
    //     (-0.494, 0.862, 0.114) / (-0.004, 1.000, -0.017) / (-0.006, 1.000, -0.021) with negative
    //     closing speeds: the car SETTLING onto the ground after it had already stopped. They are
    //     the aftermath, not the cause. f1011 carried 3 world contacts and f1012 carried 2, every
    //     one curb 1 / wall 0, both culls firing, re-pointed to the up axis.
    //     ⇒ 9.45 m/s left the car in a step with ZERO world impulses.
    //
    //     ⭐⭐⭐ (F2b) AND HERE IS WHAT "ZERO IMPULSES" ACTUALLY MEANT -- MEASURED 2026-09-03, run
    //     dv_r8, with the new [dv] one-step witness armed. IT WAS A BLIND SPOT IN THE PROBE, NOT AN
    //     ABSENCE OF IMPULSES, AND FOUR WAVES REASONED FROM IT.
    //       * UpdateContacts @0x826478B0 sorts the frame's contacts and routes EACH ONE to ONE OF
    //         TWO arms: ApplyCarWorldImpulse (which prints [kerb-imp]) and ApplyCarCarImpulse
    //         (which printed NOTHING until this commit). Both end in the same
    //         GetVehicleBody().CalculateNewVelocity(timeStep).
    //       * So "[kerb-imp] is empty" never meant "no impulse". It meant "no WORLD impulse" --
    //         exactly what the tag's own banner says, and exactly not how it was read.
    //       * THE PROOF IS A SINGLE RUN, four frames of it. [dv] records every accumulator drain
    //         with the caller's return address; all of these resolve to
    //         DeformableObject::UpdateContacts +0x4bd:
    //             f6355  1 drain  |dv| 2.92   [kerb-imp] lines: 1
    //             f6473  2 drains |dv| 2.96 + 2.88   [kerb-imp] lines: 2
    //             f7094  3 drains |dv| 3.24 + 3.21 + 2.29   [kerb-imp] lines: 3
    //             f6386  1 drain  |dv| 7.50, J = (5592.9, -115.1, -10519.6) N.s
    //                                             [kerb-imp] lines: ZERO
    //         Same function, same drain site, one visible arm and one invisible one. The invisible
    //         one took 7.5 m/s out of the car in a single step -- the same shape and the same order
    //         of magnitude as the st_scoutB event.
    //     ⇒ THE LIKELY ANSWER TO (F1)/(F2) IS A CAR-CAR CONTACT (traffic), and the whole four-wave
    //       mystery is a DIAGNOSTIC GAP, not a transcription defect. ⚠️ LIKELY, NOT PROVEN: nothing
    //       instrumented the car-car arm in the st_scoutB run, so that specific event still cannot
    //       be attributed from its log. The gap is closed as of this commit ([kerb-imp] now prints
    //       a CARCAR leg from BrnDeformableObject.cpp), so ONE re-run of the recipe decides it.
    //     ⭐ And it is the campaign's own standing hazard, met from the other side: the brief warns
    //       "four kerb catches across two waves were REAR-ENDING TRAFFIC -- open the frames". The
    //       probe could not have told anyone.
    //
    //     ⚠️ (F3) AND THE POSE MOVED TOO, by something other than the integrate. dp over that step
    //     is (-0.004394, -0.006230, -0.012085). Against v_before that implies dt = 0.0264 / 0.0104
    //     / 0.00096 s on the three axes; against v_after, 0.0193 / 0.0105 / 0.0038. No single dt
    //     fits either, so mTransform.wAxis was written by more than IntegrateTransform. The
    //     post-sim bump-stop translation (UpdateSuspensionPostSimulation @0x825F6BB0,
    //     `mTransform.wAxis += penetrationNormal * dot(up*maxPen, penetrationNormal)`) is the only
    //     other writer in the step and is the obvious suspect -- UNMEASURED, stated as a lead.
    //
    //     ⭐ (F4) THE MECHANISM INVENTORY -- every way a race car's velocity can change in one
    //     PhysicsModule::Update, so the next wave starts from a closed list instead of a hunch:
    //       * ExternalPhysicsBody::CalculateNewVelocity @(PS3 twin; X360 called from 7 sites) --
    //         THE funnel: v += (F*dt + J) / m, then the four accumulators are cleared. Everything
    //         that calls itself an "impulse" arrives here.
    //       * ReadUpdatedBodies @0x82619A10 -- v.y -= KF_GRAVITY*dt, then IntegrateTransform. The
    //         only place the pose advances.
    //       * ⭐ UpdateSuspensionPostSimulation @0x825F6BB0, the `mi8NumWorldCollisions == 0` arm:
    //         per COMPRESSED TRACTION spring, CalculateCollisionImpulseWithInanimateObject with
    //         restitution unk_8208FB10 == 0xBF333333 == -0.7 (image-read this wave) along THAT
    //         WHEEL'S ROAD-CONTACT NORMAL, then AddLocalImpulse + CalculateNewVelocity, up to four
    //         times. Wheel::SetRoadContact @0x825D6C08 only requires `normal.y > 0.5` for
    //         traction, so that normal may be ~60 deg off vertical and the impulse up to ~83%
    //         HORIZONTAL. Magnitude is 0.3 * |v.n| * m_eff: at |v.n| = 12 m/s and m = 1400 kg that
    //         is ~3.6 m/s PER WHEEL -- four wheels span the whole 9.45 m/s. This arm prints
    //         nothing unless BRN_WHEEL_SUS_PROBE is set ([wsus-imp]), which is why five waves of
    //         [kerb] logs could not see it.
    //       * ⭐ StabiliseAfterHardLanding (called from the same function, just above): a DIRECT
    //         VELOCITY REWRITE, not an impulse --
    //             mLinearVelocity -= agNormal * (dot(v, agNormal) * pow(damp, dt*60))
    //         gated on `TimeSinceHardLanding < TimeToDampAfterLanding`. No impulse probe anywhere
    //         can see this one; only a before/after sample can.
    //       * The showtime arms (SetPlayerVehicleInShowtime's 20 m/s launch overwrite,
    //         CapShowtimeVelocities, UpdateShowtimePhysics's vertical floor) -- all direct writes,
    //         all dead on the free-burn path.
    //     ⇒ THE WITNESS FOR THIS IS [dv] (BRN_DV_PROBE=<m/s>), landed 2026-09-03 in
    //       ExternalPhysicsBody.cpp: it samples the player body at eight stage boundaries of
    //       PhysicsModule::Update, records every drain in between with its banked J / F*dt / the
    //       caller's return address, and prints the ledger ONLY for a step whose |dv| crosses the
    //       threshold. A direct rewrite shows up as a mark-to-mark delta with no drain in it.
    //
    // ⛔ WHAT THIS WAVE DID NOT DO. IT DID NOT REPRODUCE EITHER ORIGINAL EVENT ON PURPOSE. Seven
    //     runs were spent; six ended with the harness's own verdict "*** THE CAR NEVER MOVED ***"
    //     for three reasons that had nothing to do with the physics (a real, now-fixed effects boot
    //     assert; a -Teleport recipe that places the car and then leaves it inert on a held
    //     throttle; one self-inflicted double-launch) -- all three written up in the [dv] banner in
    //     ExternalPhysicsBody.cpp so nobody re-spends them. The SEVENTH drove 571 m and produced
    //     the 13 step dumps that (F2b) rests on, but on ordinary traffic and world collisions along
    //     a free-burn route -- NOT the Waterfront recipe, and NOT the st_scoutB event itself.
    //     It did not reproduce the 106 mph one-step 6.2 mph loss either (open).
    //     It did not measure the (F3) pose anomaly; the bump-stop translation is a named suspect
    //     and nothing more.
    //     It did not re-decode the raw words of UpdatePostPhysics's wheel-sphere block (the geometry
    //     is read from the committed transcription, not re-derived).
    // ⚠️ AND IT LOST A RUN TO EXACTLY THE TRAP THE LAST WAVE FLAGGED. kerbX_r3 showed a textbook
    //     15.7 -> 4.0 mph one-step-ish stop "at the kerb"; the FRAMES show a stationary orange
    //     traffic car parked across the approach at (~3390, -1639). The log alone was persuasive and
    //     wrong. Open the frames.
    // ==============================================================================================

    // ==========================================================================================
    // ValidateRaceCarWorldContact @0x825C6088   (DWARF h:938; asserts BrnVehicleManager.cpp
    // :7419..:7624 -- every line number below is the console's own)
    // ==========================================================================================
    bool VehicleManager::ValidateRaceCarWorldContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpInOutContact,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
        f32 lfTimeStep)
    {
        namespace vpu = rw::math::vpu;
        typedef CgsGeometric::Triangle4 Triangle4;

        // ---- the guarded function-statics (guard dword_82FBA030; every value image-read) ------
        static const f32 KVF_GROUND_CLEARANCE_MAX_SPEED_MPH = 25.0f;  // bit0, flt_82004FD8
        static const f32 KVF_GROUND_CLEARANCE_MIN_SPEED_MPH = 10.0f;  // bit1, flt_82004A20
        static const f32 kvfMaxCurbHeight                   = 0.25f;  // bit2, flt_8208F834
        static const f32 kvfWallNormalMaxHeight             = 0.3f;   // bit3, flt_82004740 (f30)
        static const f32 KVF_CAR_BOTTOM_PLANE_MODIFIER      = 0.0f;   // bit4, flt_82001CC0

        CGS_ASSERT(lpInOutContact != 0, "lpInOutContact != NULL");           // :7419
        CGS_ASSERT(lpTriCacheInterface != 0, "lpTriCacheInterface != NULL"); // :7420

        CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                   "Normal invalid on entry to validate\n");                       // :7421
        CGS_ASSERT(fabsf(lpInOutContact->mNormal.mV.v[0]) > KF_NORMAL_ZERO_EPSILON
                       || fabsf(lpInOutContact->mNormal.mV.v[1]) > KF_NORMAL_ZERO_EPSILON
                       || fabsf(lpInOutContact->mNormal.mV.v[2]) > KF_NORMAL_ZERO_EPSILON,
                   "Normal zero on entry to validate\n");                          // :7422


        // ---- the wall-normal flatten: near-horizontal normals become PURE horizontal ----------
        if (KF_WALL_NORMAL_DOT_THRESHOLD > fabsf(lpInOutContact->mNormal.mV.v[1]))
        {
            Vector3 lFlat = lpInOutContact->mNormal;
            lFlat.mV.v[1] = 0.0f;                              // vrlimi zero-insert on lane 1
            lpInOutContact->mNormal = vpu::Normalize(lFlat);   // vmsum3fp + vrsqrtefp 2-Newton
            CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                       "Normal invalid after clearing Y component\n");             // :7432
        }

        CGS_ASSERT((lpInOutContact->muVolumeInstanceIdA.muId >> 56) == 1u,
                   "lpInOutContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :7436
        CGS_ASSERT((lpInOutContact->muVolumeInstanceIdB.muId >> 56) == 0u,
                   "lpInOutContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD");   // :7437

        const u32 luRaceCarIndex =
            static_cast<u32>(lpInOutContact->muVolumeInstanceIdA.muId >> 42) & 0x3FFFu;
        RaceCarPhysics& lrCar = maRaceCarVehicles[luRaceCarIndex];

        const Vector3 lPointOnCar   = lpInOutContact->mPointOnA;   // v117
        const Vector3 lPointOnWorld = lpInOutContact->mPointOnB;   // v119
        const Vector3 lUpAxis       = lrCar.GetUpAxis();           // v122 == car+0x20 (yAxis)

        // speed in MPH off the cached velocity magnitude (mNormLinearVelocityMag.w, car+0x1340
        // lane 3, m/s) -- distinct from the mfSpeedMPH read the final gate makes.
        const f32 lfSpeedMph = lrCar.mNormLinearVelocityMag.mV.v[3] * KF_MPS_TO_MPH;

        // ---- the contact's triangle, re-fetched from the car's cache window --------------------
        const u16 lu16TriangleIndex = lpInOutContact->mu16PrimitiveIndexB;   // +0x4A (the WORLD side)
        const s32 liLane  = static_cast<s32>(lu16TriangleIndex) % 4;         // srawi/addze signed split
        const s32 liBatch = static_cast<s32>(lu16TriangleIndex) / 4;

        const s32 liNumBatches =
            lpTriCacheInterface->GetNumCachedTriangleBatches(static_cast<s32>(luRaceCarIndex));
        // Streamed value tail ("Invalid contact with triangle N within batch M, when there are
        // only K batches in total") lowered to the static prefix per the standing project rule.
        CGS_ASSERT(liBatch < liNumBatches, "Invalid contact with triangle ");      // :7486

        // GetCache carries the "mpTriangleCacheManager != NULL" tripwire the console fires here.
        const Triangle4* lpCache =
            lpTriCacheInterface->GetCache(static_cast<s32>(luRaceCarIndex));
        Triangle4::AOSTriangle lTriangle;
        lpCache[liBatch].GetAOSTriangle(liLane, lTriangle);   // {V0,V1,V2, unit normal, edge cosines}

        // ---- curb / wall classification --------------------------------------------------------
        // Heights of the triangle's three vertices above the road plane(s) under the car's
        // on-ground wheels (GetHeightAboveRoad -- the real query-point overload, fixed this wave).
        const f32 lfHeight0 = lrCar.GetHeightAboveRoad(lTriangle.mVertex0).GetFloat();
        const f32 lfHeight1 = lrCar.GetHeightAboveRoad(lTriangle.mVertex1).GetFloat();
        const f32 lfHeight2 = lrCar.GetHeightAboveRoad(lTriangle.mVertex2).GetFloat();

        const bool lbIsCurb = kvfMaxCurbHeight > lfHeight0
                           && kvfMaxCurbHeight > lfHeight1
                           && kvfMaxCurbHeight > lfHeight2;
        const bool lbIsWall = !lbIsCurb
                           && (kvfWallNormalMaxHeight > fabsf(lTriangle.mNormal.mV.v[1]));

        // ARTIST 825C67B0..825C6998: one buffered triangle and three solid
        // spheres for each enabled classification. Only the triangle is offset
        // along the normal (0.05); the spheres stay at the vertices, radius 0.3.
        if ((mDebugComponent.RenderWallContacts() && lbIsWall)
            || (mDebugComponent.RenderGroundContacts() && lbIsCurb))
        {
            const Vector3 lvScale(VecFloat(0.05f).GetVector());
            const Vector3 lvVertex0 = vpu::MultAdd(lTriangle.mNormal, lvScale, lTriangle.mVertex0);
            const Vector3 lvVertex1 = vpu::MultAdd(lTriangle.mNormal, lvScale, lTriangle.mVertex1);
            const Vector3 lvVertex2 = vpu::MultAdd(lTriangle.mNormal, lvScale, lTriangle.mVertex2);
            const CgsDev::RGBA luColour = lbIsWall ? 0xFF0000FFu : 0xFF00FF00u;
            CgsDev::DebugInterface lDebugInterface;
            CgsDev::DebugRender& lrRender = lDebugInterface.GetRender();
            lrRender.DrawTriangle(lvVertex0, lvVertex1, lvVertex2, luColour);
            lrRender.DrawSolidSphere(lTriangle.mVertex0, 0.3f, luColour);
            lrRender.DrawSolidSphere(lTriangle.mVertex1, 0.3f, luColour);
            lrRender.DrawSolidSphere(lTriangle.mVertex2, 0.3f, luColour);
        }

        // ---- record the wall triangle on the car's debug component (REAL console behaviour,
        //      unconditional on the wall classification -- NOT a draw) --------------------------
        if (lbIsWall)
        {
            maRaceCarDebugComponent[luRaceCarIndex].SetLastWallTriangle(&lTriangle);
        }

        // ---- the speed-scaled ground-clearance gate --------------------------------------------
        f32 lfCullHeight = gfGroundContactCullHeight;                         // X360 reads flt_82F2A148
        if (KVF_GROUND_CLEARANCE_MAX_SPEED_MPH > lfSpeedMph)
        {
            if (KVF_GROUND_CLEARANCE_MIN_SPEED_MPH > lfSpeedMph)
            {
                // [kerb] the fast path is a verdict too -- say so rather than vanish. Filtered to a
                // MOVING car (>= 3 mph): a parked car's body shell posts ~24 ground contacts a frame
                // (measured, junkyard, kerb run r1), which ate the whole budget before the drive.
                return true;   // below 10 mph EVERY contact is accepted (li r3,1 fast path)
            }
            // 10..25 mph: scale the cull height by (speed-10)/15 (vrefp + 2 Newton reciprocal).
            lfCullHeight = lfCullHeight
                * (lfSpeedMph - KVF_GROUND_CLEARANCE_MIN_SPEED_MPH)
                / (KVF_GROUND_CLEARANCE_MAX_SPEED_MPH - KVF_GROUND_CLEARANCE_MIN_SPEED_MPH);
        }

        // ---- the two ground-cull candidates (both suppressed for walls) ------------------------
        bool lbCullToPlane = false;   // r30


        // (a) the wheel-plane test, gated on mbMinWheelDistValid (car+0x714): is the world-side
        //     point within the (scaled) cull height of the car's wheel plane?
        if (lrCar.mbMinWheelDistValid)
        {
            const Vector3 lDelta = vpu::Subtract(lPointOnWorld,
                                                 lrCar.mWheelPlanePosAndHeight.GetVector3());
            const f32 lfDist = vpu::Dot(lDelta, lUpAxis);
            if (lrCar.mWheelPlanePosAndHeight.mV.v[3] + lfCullHeight > lfDist && !lbIsWall)
            {
                lbCullToPlane = true;
            }
        }

        // (b) the above-ground-test plane (car+0x570), when its normal is up-ish (dot > 0.8).
        const AboveGroundTestResult& lrAboveGround = *lrCar.GetAboveGroundTestResult();
        if (vpu::Dot(lUpAxis, lrAboveGround.mIntersectionNormal) > KF_UP_DOT_GROUND_NORMAL_MIN)
        {
            // The console re-reads mPointOnB here and asserts it against the register-cached
            // copy ("lPointOnOther == lpInOutContact->mPointOnB", :7598). Nothing between the
            // entry load and this point writes mPointOnB, so the check is an identity on the
            // host -- reproduced as this note, not as a self-comparison.
            const f32 lfDist = vpu::Dot(
                vpu::Subtract(lPointOnWorld, lrAboveGround.mIntersectionPosition),
                lrAboveGround.mIntersectionNormal);
            if (lfCullHeight > lfDist && !lbIsWall)
            {
                lbCullToPlane = true;
            }
        }

        // ---- the plane projection rewrite ------------------------------------------------------
        if (lbCullToPlane)
        {
            // World-space plane anchor: the FRONT-LEFT wheel's streamed (local) position pushed
            // through the car transform (raw-word verified: vmaddfp chain vD = vA*vC + vB over
            // the xAxis/yAxis/zAxis rows + wAxis, local point = car+0x1C0 ==
            // maWheels[0].mStreamedPositionPlusTwistAmount.xyz).
            const Matrix44Affine& lrTransform = lrCar.GetTransform();
            const Vector3Plus& lrLocal =
                lrCar.GetWheel(eFrontLeftWheel).mStreamedPositionPlusTwistAmount;
            Vector3 lPlanePoint;
            lPlanePoint.mV.v[0] = lrTransform.wAxis.mV.v[0] + lrTransform.xAxis.mV.v[0] * lrLocal.mV.v[0]
                          + lrTransform.yAxis.mV.v[0] * lrLocal.mV.v[1] + lrTransform.zAxis.mV.v[0] * lrLocal.mV.v[2];
            lPlanePoint.mV.v[1] = lrTransform.wAxis.mV.v[1] + lrTransform.xAxis.mV.v[1] * lrLocal.mV.v[0]
                          + lrTransform.yAxis.mV.v[1] * lrLocal.mV.v[1] + lrTransform.zAxis.mV.v[1] * lrLocal.mV.v[2];
            lPlanePoint.mV.v[2] = lrTransform.wAxis.mV.v[2] + lrTransform.xAxis.mV.v[2] * lrLocal.mV.v[0]
                          + lrTransform.yAxis.mV.v[2] * lrLocal.mV.v[1] + lrTransform.zAxis.mV.v[2] * lrLocal.mV.v[2];
            lPlanePoint.mV.v[3] = 0.0f;

            // mNormal := the car's up axis; mPointOnA := its projection onto the wheel plane
            // (shifted by the zero KVF_CAR_BOTTOM_PLANE_MODIFIER, kept for fidelity).
            lpInOutContact->mNormal = lUpAxis;
            const f32 lfHeightAbovePlane =
                vpu::Dot(vpu::Subtract(lPointOnCar, lPlanePoint), lUpAxis)
                - KVF_CAR_BOTTOM_PLANE_MODIFIER;
            lpInOutContact->mPointOnA.mV.v[0] = lPointOnCar.mV.v[0] - lUpAxis.mV.v[0] * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.mV.v[1] = lPointOnCar.mV.v[1] - lUpAxis.mV.v[1] * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.mV.v[2] = lPointOnCar.mV.v[2] - lUpAxis.mV.v[2] * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.mV.v[3] = lPointOnCar.mV.v[3] - lUpAxis.mV.v[3] * lfHeightAbovePlane;

            CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                       "Normal invalid after setting to wheel plane normal\n");    // :7615
        }

        // ---- the final acceptance: is the world point inside the car's (deformable) AABB, ------
        //      expanded by the frame's displacement?
        const Matrix44Affine& lrTransform = lrCar.GetTransform();
        const Vector3 lDiff = vpu::Subtract(lPointOnWorld, lrTransform.wAxis);
        const f32 lfLocalX = vpu::Dot(lDiff, lrTransform.xAxis);
        const f32 lfLocalY = vpu::Dot(lDiff, lrTransform.yAxis);
        const f32 lfLocalZ = vpu::Dot(lDiff, lrTransform.zAxis);

        const Vector3 lDisplacement = lrCar.mLinearVelocity * lfTimeStep;
        const f32 lfDispX = vpu::Dot(lDisplacement, lrTransform.xAxis);   // v10 (kept SIGNED for the recheck)
        const f32 lfDispY = vpu::Dot(lDisplacement, lrTransform.yAxis);
        const f32 lfDispZ = vpu::Dot(lDisplacement, lrTransform.zAxis);

        // upper = max + (0.1, 0.1, 0.0) + |disp| ; lower = min - (0.1, 0.1, 0.0) - |disp|
        // (margin lanes raw-word verified: x/y == flt_82004014, z == flt_82001CC0 == 0).
        const f32 lfUpperX = lrCar.mDeformableAABB.mMax.mV.v[0] + KF_AABB_MARGIN_XY + fabsf(lfDispX);
        const f32 lfUpperY = lrCar.mDeformableAABB.mMax.mV.v[1] + KF_AABB_MARGIN_XY + fabsf(lfDispY);
        const f32 lfUpperZ = lrCar.mDeformableAABB.mMax.mV.v[2] + 0.0f            + fabsf(lfDispZ);
        const f32 lfLowerX = lrCar.mDeformableAABB.mMin.mV.v[0] - KF_AABB_MARGIN_XY - fabsf(lfDispX);
        const f32 lfLowerY = lrCar.mDeformableAABB.mMin.mV.v[1] - KF_AABB_MARGIN_XY - fabsf(lfDispY);
        const f32 lfLowerZ = lrCar.mDeformableAABB.mMin.mV.v[2] - 0.0f            - fabsf(lfDispZ);

        // CONSOLE QUIRK REPRODUCED, raw-word verified: the three LOWER-bound compares all
        // test the LOCAL-Y coordinate (0x825C6DAC == 0x825C6DE4 == word 112C4EC6 -> vcmpgtfp.
        // v9, v12(localY), v9(lower splat); 0x825C6E20 -> 100C06C6 likewise), where the upper-
        // bound compares use x/y/z correctly. The PS3 compiles the SAME pairing (its cmp2/4/6
        // all read the one splat register) -- a source-level copy/paste in the shipping game,
        // reproduced faithfully rather than "fixed".
        const bool lbInsideAabb = (lfUpperX > lfLocalX) && (lfLocalY > lfLowerX)
                               && (lfUpperY > lfLocalY) && (lfLocalY > lfLowerY)
                               && (lfUpperZ > lfLocalZ) && (lfLocalY > lfLowerZ);

        bool lbAccept;
        if (lbInsideAabb)
        {
            lbAccept = true;
        }
        else if (lrCar.mbCrashing)                    // car+0x710: crashing cars keep contacts
        {
            lbAccept = true;
        }
        else if (lrCar.mbHasAir)                      // car+0x1350: airborne cars keep contacts
        {
            lbAccept = true;
        }
        else
        {
            // slow-ish cars keep out-of-AABB contacts too (mfSpeedMPH is a lane splat).
            lbAccept = KF_FINAL_ACCEPT_MAX_SPEED_MPH > lrCar.GetSpeedMPH().GetFloat();
        }

        // ---- the velocity-expanded LATERAL recheck (always runs; can only REJECT) --------------
        // Band on the local-X axis only, margin 0.2, expanded toward the travel direction.
        // dx == 0.0 exactly collapses the band to zero (the console's vmr-zero path) -- only
        // reachable at speeds >= 10 mph (the sub-10 fast path returned above), where an exactly
        // zero lateral displacement dot is a measure-zero event, exactly as shipped.
        f32 lfBandUpperX;
        f32 lfBandLowerX;
        if (lfDispX > 0.0f)
        {
            lfBandUpperX = lrCar.mDeformableAABB.mMax.mV.v[0] + KF_LATERAL_RECHECK_MARGIN + lfDispX;
            lfBandLowerX = lrCar.mDeformableAABB.mMin.mV.v[0] - KF_LATERAL_RECHECK_MARGIN;
        }
        else if (0.0f > lfDispX)
        {
            lfBandUpperX = lrCar.mDeformableAABB.mMax.mV.v[0] + KF_LATERAL_RECHECK_MARGIN;
            lfBandLowerX = lrCar.mDeformableAABB.mMin.mV.v[0] - KF_LATERAL_RECHECK_MARGIN + lfDispX;
        }
        else
        {
            lfBandUpperX = 0.0f;
            lfBandLowerX = 0.0f;
        }
        if (lfLocalX > lfBandUpperX || lfBandLowerX > lfLocalX)
        {
            lbAccept = false;
        }

        // ---- [kerb] the per-contact witness: every gate above, both sides (TU-scope banner) ------
        // ---- end [kerb] ------------------------------------------------------------------------

        return lbAccept;
    }

    // ==========================================================================================
    // Vehicle::DebugComponent::SetLastWallTriangle @0x825B4D60: the temporary copy that lived
    // here is DELETED 2026-08-24 (physics mount wave B3) -- its declared home TU
    // (B5PhysicsHandlingDebugComponent.cpp) is mounted now and owns the identical body. The
    // old banner's vtable fear (dragging the CgsDev::DebugComponent base's unreconstructed
    // virtual surface) was RE-MEASURED this wave: ZERO LNK2019 -- that base surface has landed
    // since; the link flagged only the predicted LNK2005, exactly as the banner said it would.
    // ==========================================================================================
}
}

// FOLDED FROM BrnVehicleManager_ValidateSimulationContacts.cpp
// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_ValidateSimulationContacts.cpp
//
// BrnPhysics::Vehicle::VehicleManager::ValidateSimulationContacts  @ X360 0x825C8990
// (DWARF decl BrnVehicleManager.h:667; home TU BrnVehicleManager.cpp -- its own baked assert
// path/lines are BrnVehicleManager.cpp:9877/9899/9908. That TU is still unmounted, so this is
// a slice TU in the established BrnVehicleManager_Construct.cpp / *CrashPrediction.cpp /
// *PlayerStats.cpp pattern. Fold it back when the home TU mounts.)
//
// Reconstructed branch-for-branch from the X360 asm (IDA export 0x825C8990.json; bl target of
// PhysicsModule::ValidateSimulationContacts @0x825A1418). Debug-only validation of the
// outgoing simulation contact queue; called once per frame by the physics module's own
// ValidateSimulationContacts pass. Per queued InAddPotentialContact (80-byte GetEvent copy,
// ctr=10 block move):
//   * If mIDA's high-dword owner byte is E_ENTITYTYPE_TRAFFIC_VEHICLE (2):
//       - traffic index = (idHigh >> 10) & 0x3FFF (`extrwi r29, r28, 14,8` == the packed
//         EntityId::GetEntityIndex field, the same extraction BrnPhysicalTrafficManager.cpp
//         uses throughout);
//       - inlined BitArray bounds tripwire: index < 20 ("invalid index : %d < 20",
//         CgsBitArray.h:203) -- fire-and-continue;
//       - inlined BitArray<20>::IsBitSet against mPhysicalTrafficManager.mUsedTrafficVehicles
//         (the asm's `ldx` at this + 0x24748 == 44768 + 104552, the member's own pinned seat;
//         the friend declaration in BrnPhysicalTrafficManager.h is what lets the read stay
//         BY NAME) -- if the slot is not live, assert
//         "Simulation contact with traffic vehicle A that isn't alive, vehicle index %u,
//          physics ID 0x%X" (BrnVehicleManager.cpp:9899).
//   * Same for mIDB (":9908", "...vehicle B that isn't alive...").
//
// The console's message construction goes through the CgsDev::Assert::gpcMessageBuffer /
// StrStream machinery; lowered to the project CGS_ASSERT with the static message per the
// standing rule (see e.g. CgsScriptedFsm.cpp / CgsTimeManager.cpp banners).
//
// ONE DELIBERATE HOST DIVERGENCE, flagged: when the bounds tripwire fires (index >= 20)
// the console goes on to compute the bit test anyway, reading (index>>6)*8 bytes past the
// 1-word bit array -- an out-of-bounds read whose result only feeds the second (already
// garbage) diagnostic. The host skips the IsBitSet call for out-of-range indices instead of
// reproducing the OOB read; both asserts are non-gating diagnostics, so no live behaviour
// changes.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"  // PhysicsSimulationIO::InAddPotentialContact (+ the InAddContactQueue instantiation)
#include "GameSource/World/BrnEntityTypes.h"                              // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::ValidateSimulationContacts(
        const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact, 1024>* lpContactQueue )
    {
        CGS_ASSERT(lpContactQueue != 0, "lpContactQueue != NULL");   // :9877

        for (s32 liIndex = 0; liIndex < lpContactQueue->GetLength(); ++liIndex)
        {
            // The X360 copies the whole 80-byte event to the stack (ctr = 10 ld/std) before
            // reading the two id words; a by-value copy reproduces that.
            const CgsPhysics::PhysicsSimulationIO::InAddPotentialContact lContact =
                lpContactQueue->GetEvent(liIndex);

            const u32 luIdAHigh = static_cast<u32>(lContact.mIDA >> 32);
            const u32 luIdBHigh = static_cast<u32>(lContact.mIDB >> 32);

            if ((luIdAHigh >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
            {
                // Packed 14-bit entity index (EntityId::GetEntityIndex -- extrwi 14,8).
                const u32 luTrafficIndexA = (luIdAHigh >> 10) & 0x3FFFu;

                // Inlined BitArray<20> bounds tripwire (CgsBitArray.h:203; streamed
                // "invalid index : %d < 20" lowered to the static message).
                CGS_ASSERT(luTrafficIndexA < 20u, "invalid index : ");

                if (luTrafficIndexA < 20u)   // host bounds guard -- see the banner divergence note
                {
                    CGS_ASSERT(mPhysicalTrafficManager.mUsedTrafficVehicles.IsBitSet(luTrafficIndexA),
                               "Simulation contact with traffic vehicle A that isn't alive, vehicle index ");   // :9899
                }
            }

            if ((luIdBHigh >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
            {
                const u32 luTrafficIndexB = (luIdBHigh >> 10) & 0x3FFFu;

                CGS_ASSERT(luTrafficIndexB < 20u, "invalid index : ");

                if (luTrafficIndexB < 20u)
                {
                    CGS_ASSERT(mPhysicalTrafficManager.mUsedTrafficVehicles.IsBitSet(luTrafficIndexB),
                               "Simulation contact with traffic vehicle B that isn't alive, vehicle index ");   // :9908
                }
            }
        }
    }
}
}

// FOLDED FROM BrnVehicleManager_WorldCrashArm.cpp
// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_WorldCrashArm.cpp
//
// THE RACE-CAR-vs-WORLD CRASH ARM -- the two callees that kept HandleCrashPredictionForRaceCarAndWorld
// @0x82640C28 unmountable, and therefore kept "we can only crash on traffic, not on walls" true:
//
//   VehicleManager::HandleRaceCarWorldPotentialContact  @0x8263E3B8 (566 insns)  -- DWARF BrnVehicleManager.h:1200
//   VehicleManager::PredictCarWorldContactTime          @0x825B5300 ( 83 insns)  -- DWARF BrnVehicleManager.h:1215
//
// Slice TU (home BrnVehicleManager.cpp is still unmountable). Both bodies are read off the ARTIST
// asm. The Hex-Rays for the first is unusable (a 31-int prototype, "local variable allocation has
// failed"); for the second it DROPS the per-car index -- it prints this+1936 / this+1888, which is
// maRaceCarVehicles[0], but the asm at 0x825B53A0 / 0x825B53E4 does `extrwi r9, r9, 14,8 ;
// mulli r11, r9, 0x1460` first, so the reads are maRaceCarVehicles[idx].mLinearVelocity (+0x50)
// and .mTransform.yAxis (+0x20).
//
// ARGUMENT MAP (asm prologue 0x8263E3D4..0x8263E400): r3 = this; the 80-byte PotentialContact is
// passed BY VALUE -- r4..r10 carry bytes 0..55 (spilled to arg_20..arg_50), bytes 56..79 arrive on
// the stack at arg_58; f1 = lfTimestep (`fmr f31, f1`). The four interface pointers are stack args
// arg_74 / arg_7C / arg_84 / arg_8C and are only ever re-loaded for the SetRaceCarCrashing call
// (0x8263EC58..0x8263EC70), in the order request / vehicle-out / manager-out / deformation. The
// triangle-cache interface (arg_94) is NEVER read by this body: the console accepts it and
// ignores it, so the parameter is kept and unused.
//
// THE CONSTANTS -- three families, and two of them read ZERO in the image:
//   .rdata literals:   flt_82002138 = 0.01f (unit-normal tolerance), flt_82001CC0 = 0.0f,
//                      flt_82005450 = 0.9f, unk_82181510 = (0, 1, 0, 0) (world up).
//   function statics:  flt_8200D5FC = -0.70f, flt_8209C7F0 = -0.53f, flt_8201A1F0 = 200.0f,
//                      flt_82004740 = 0.30f -- four function-local `static const VecFloat` splats
//                      behind the dword_82FBA320 guard bits (0x8263E76C..0x8263E8E0 is MSVC's
//                      lazy init of each; unk_82FBA2E0/2F0/300/310 are the splat caches).
//   .data, dyn-init:   flt_82FB914C reads 0 in the image. It is WRITTEN by sub_82C5B8E0 as
//                      cosf(flt_82FB9CC8), and flt_82FB9CC8 is itself written by the EXPORT-HOLE
//                      thunk at 0x82C5B880..0x82C5B8DC as flt_82004A18 (80.0f) * flt_8208F5F4
//                      (0.017453292f) -- 80 degrees in radians. The two thunks are adjacent, in the
//                      same TU, in declaration order, so the cos sees the product:
//                      flt_82FB914C == cosf(80 deg) == 0.17364818f.
//                      unk_82FB82B0 reads 0 in the image. Written by the export-hole thunk at
//                      0x82C5BB88..0x82C5BBAC as splat(flt_82004014) == 0.10f.
//                      Neither thunk has a .ida-exports JSON; both were found by scanning the
//                      image for the `lis rX, 0x82FC ; stfs/addi rX, @l` pair (see the wave report).
//                      Reading either as its image value (0) would have turned the roll arm into
//                      "any tilt at all against any non-downward surface is a crash".
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::E_ENTITYTYPE_*
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>     // sqrtf / fabsf

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // flt_82002138 -- the unit-normal tolerance shared with the traffic arm's IsUnitLength.
    const f32 KF_UNIT_NORMAL_TOLERANCE          = 0.00999999978f;

    // The four function-local statics (dword_82FBA320 bits 1/2/4/8).
    const f32 KF_HEAD_ON_COSINE_STOPPED         = -0.699999988f;   // flt_8200D5FC  (unk_82FBA310)
    const f32 KF_HEAD_ON_COSINE_AT_SPEED        = -0.529999971f;   // flt_8209C7F0  (unk_82FBA300)
    const f32 KF_HEAD_ON_SPEED_RANGE            = 200.0f;          // flt_8201A1F0  (unk_82FBA2F0)
    const f32 KF_FRONT_FACE_DEPTH               = 0.300000012f;    // flt_82004740  (unk_82FBA2E0)

    // The two dyn-init'd .data constants (see the banner for the thunks that write them).
    const f32 KF_ROLLED_CRASH_UP_COSINE         = 0.173648178f;    // flt_82FB914C == cosf(80 deg)
    const f32 KF_ROLLED_CRASH_MIN_NORMAL_Y      = 0.100000001f;    // unk_82FB82B0 == splat(flt_82004014)

    // vcsxwfp128 v123, v13(1), 1 == 1 * 2^-1: the corner-clip half-width the player test uses.
    const f32 KF_CORNER_CLIP_WIDTH              = 0.5f;

    // flt_82005450 -- the "this normal is the ground" cosine PredictCarWorldContactTime tests.
    const f32 KF_GROUND_NORMAL_UP_COSINE        = 0.899999976f;











    // 0x8263E484..0x8263E51C: |n|^2 -> vrsqrtefp + two Newton steps -> |n| (vsel'd to 0 when
    // |n|^2 == 0) -> | |n| - 1 | > 0.01. The same shape the traffic arm reproduces.
    inline bool IsUnitLength(const Vector3& lrV)
    {
        const f32 lfLengthSq = rw::math::vpu::Dot(lrV, lrV);
        const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : lfLengthSq / sqrtf(lfLengthSq);
        return !(fabsf(lfLength - 1.0f) > KF_UNIT_NORMAL_TOLERANCE);
    }

    // 0x8263E70C..0x8263E75C: a world point expressed in the car's frame -- (p - pos) dotted
    // onto the three rotation rows, assembled with vrlimi128 8/4/2 into x/y/z.
    inline Vector3 ToCarFrame(const Matrix44Affine& lrTransform, const Vector3& lrWorldPoint)
    {
        const Vector3 lvDelta = lrWorldPoint - lrTransform.wAxis;
        return Vector3(rw::math::vpu::Dot(lvDelta, lrTransform.xAxis),
                       rw::math::vpu::Dot(lvDelta, lrTransform.yAxis),
                       rw::math::vpu::Dot(lvDelta, lrTransform.zAxis));
    }
}

// -------------------------------------------------------------------------------------------
// HandleRaceCarWorldPotentialContact  @0x8263E3B8 (566)  -- DWARF BrnVehicleManager.h:1200
//
// Called by HandleCrashPredictionForRaceCarAndWorld for every impact-time-ordered, validated
// race-car-vs-world potential contact. Decides whether THIS contact is a crash -- a head-on into
// the front face, a hard side/other impact above the axle line, or landing on the world while
// rolled past 80 degrees -- and, if so, commits it through the universal sink SetRaceCarCrashing
// and latches the player's slow-motion inhibit for a corner clip.
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleRaceCarWorldPotentialContact(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
    f32 lfTimestep)
{
    // arg_94 is never loaded by the console body (see the banner).
    (void)lpTriangleCacheInterface;

    // 0x8263E3F0 / 0x8263E438 -- the two owner bytes of the packed ids (BrnVehicleManager.cpp:5963/:5964).
    const u32 luRaceCarEntityWord = EntityWordOf(lContact.muVolumeInstanceIdA);   // r25
    const u32 luWorldEntityWord   = EntityWordOf(lContact.muVolumeInstanceIdB);   // r24

    CGS_ASSERT((luRaceCarEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
               "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");  // :5963
    CGS_ASSERT((luWorldEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");    // :5964

    // 0x8263E460..0x8263E5A0 (:5965). The console streams the offending normal after the prefix;
    // message-buffer streaming is lowered to the static prefix per the standing rule.
    const Vector3 lContactNormal = lContact.mNormal;   // v125 (spilled to var_120 around the asserts)
    CGS_ASSERT(IsUnitLength(lContactNormal), "Bad normal in HandleRaceCarWorldPotentialContact: ");

    // 0x8263E5AC / 0x8263E5DC -- the same two owner tests again on the extracted words (:5976/:5977).
    CGS_ASSERT((luRaceCarEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
               "lRaceCarEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :5976
    CGS_ASSERT((luWorldEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lWorldEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");       // :5977

    // 0x8263E60C..0x8263E620: `extrwi r4, r25, 14,8 ; lwzx maeRaceCarTypes[r4] ; cmpwi 2`.
    // A NETWORK car's world contacts are never crash-predicted locally.
    const u16 lu16RaceCarIndex = EntityIndexOfWord(luRaceCarEntityWord);   // r4
    if (maeRaceCarTypes[lu16RaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
        return;

    // r5 = this + 5216 * idx ; the record itself sits at r5 + 0x740 == maRaceCarVehicles[idx].
    const RaceCarPhysics& lrRaceCar     = maRaceCarVehicles[lu16RaceCarIndex];
    const Matrix44Affine  lTransform    = lrRaceCar.GetTransform();          // rec +0x10..+0x40
    const Vector3         lLinearVelocity  = lrRaceCar.GetLinearVelocity();  // rec +0x50
    const Vector3         lAngularVelocity = lrRaceCar.GetAngularVelocity(); // rec +0x60

    // ---- (1) 0x8263E624..0x8263E6E0: will the race-car point still be on the world side of the
    //      plane one timestep from now?  vP = w x (pA - pos) + v ; (pA + vP*dt - pB) . n > 0 => it
    //      is moving APART, and there is nothing to do.
    const Vector3 lvRadius        = lContact.mPointOnA - lTransform.wAxis;                     // v13
    const Vector3 lvPointVelocity = rw::math::vpu::Cross(lAngularVelocity, lvRadius) + lLinearVelocity;   // v7
    const Vector3 lvPredictedPoint = lvPointVelocity * lfTimestep + lContact.mPointOnA;        // vmaddfp v0,v7,v10,v12
    if (rw::math::vpu::Dot(lvPredictedPoint - lContact.mPointOnB, lContactNormal) > 0.0f)     // flt_82001CC0
        return;

    // ---- 0x8263E6E4..0x8263E760: both contact points in the car's frame.
    //      v9 (-> var_110) = the WORLD point in car space; v122 = the RACE-CAR point in car space.
    const Vector3 lvWorldPointLocal   = ToCarFrame(lTransform, lContact.mPointOnB);   // v9
    const Vector3 lvRaceCarPointLocal = ToCarFrame(lTransform, lContact.mPointOnA);   // v122

    // ---- 0x8263E81C..0x8263E870: the head-on cone widens with forward speed.
    //      threshold = -0.70 + (-0.53 - -0.70) * clamp(v.forward, 0, 200) / 200 ;
    //      lbHeadOn  = threshold > n . forward   (the normal faces OUT of the world, so a wall
    //      square in front of the car gives n . forward == -1).
    f32 lfForwardSpeed = rw::math::vpu::Dot(lLinearVelocity, lTransform.zAxis);   // v0 = vel . zAxis
    if (lfForwardSpeed < 0.0f)                    lfForwardSpeed = 0.0f;           // vmaxfp128 v0, 0, v0
    if (lfForwardSpeed > KF_HEAD_ON_SPEED_RANGE)  lfForwardSpeed = KF_HEAD_ON_SPEED_RANGE;   // vminfp v13, 200, v0
    const f32 lfHeadOnCosine =
        (lfForwardSpeed / KF_HEAD_ON_SPEED_RANGE) * (KF_HEAD_ON_COSINE_AT_SPEED - KF_HEAD_ON_COSINE_STOPPED)
        + KF_HEAD_ON_COSINE_STOPPED;                                                // vmaddfp v0, v0, v11, v10
    const bool lbHeadOn = lfHeadOnCosine > rw::math::vpu::Dot(lContactNormal, lTransform.zAxis);   // r8

    // ---- 0x8263E878..0x8263E8A8: the deformable box, copied to the stack (rec +0x6D0, 32 bytes).
    const CgsGeometric::AxisAlignedBox& lrBox = lrRaceCar.GetDeformableAABB();

    // ---- 0x8263E8E4..0x8263E970: is the WORLD point on the car's front face?  Within the box's
    //      width, and within 0.3 of its front plane.
    const bool lbOnFrontFace =
           lvWorldPointLocal.GetX() >= lrBox.mMin.GetX()                          // vcmpgefp. v13(Bx), v10(min.x)
        && lrBox.mMax.GetX() >= lvWorldPointLocal.GetX()                          // vcmpgefp. v6(max.x), v13
        && lvWorldPointLocal.GetZ() >= lrBox.mMax.GetZ() - KF_FRONT_FACE_DEPTH;   // vcmpgefp. v5(Bz), max.z - 0.3

    // 0x8263E974..0x8263E990: `lfsx f0, 4*(idx + 0xA7C0), this` == mafVulnerabilityFactor[idx].
    const f32 lfVulnerabilityFactor = mafVulnerabilityFactor[lu16RaceCarIndex];

    // ---- 0x8263E99C..0x8263EA1C: HEAD-ON. Front face, inside the cone, and the closing speed
    //      along the normal (scaled by the car's vulnerability) at or above the head-on threshold.
    bool lbHeadOnCrash = false;                                                     // r8 (re-used)
    if (lbOnFrontFace && lbHeadOn)
    {
        const f32 lfClosingSpeed = rw::math::vpu::Dot(rw::math::vpu::Negate(lContactNormal), lLinearVelocity);
        lbHeadOnCrash = (lfClosingSpeed * lfVulnerabilityFactor) >= mfHeadOnWorldCrashThreshold;   // +172212
    }

    // ---- 0x8263EA20..0x8263EAE0: SIDE-ON. Only when the world point is NOT on the front face:
    //      the POINT closing speed (scaled) at or above the side threshold, and the world point
    //      above the car's origin (localB.y > 0) -- a kerb scrape below the axle line is not a crash.
    bool lbSideOnCrash = false;                                                     // r9
    if (!lbOnFrontFace)
    {
        const f32 lfPointClosingSpeed =
            rw::math::vpu::Dot(rw::math::vpu::Negate(lContactNormal), lvPointVelocity);   // vmsum3fp128 v7, v6, v7
        if ((lfPointClosingSpeed * lfVulnerabilityFactor) >= mfSideOnWorldCrashThreshold)  // +172216
        {
            lbSideOnCrash = lvWorldPointLocal.GetY() > 0.0f;                             // flt_82001CC0 (f13)
        }
    }

    // ---- 0x8263EAE4..0x8263EB64: ROLLED. The car's up axis has tipped past 80 degrees from world
    //      up, and the surface it is touching faces upward (n.y > 0.1) -- it is landing on the world.
    bool lbRolledOntoWorld = false;                                                 // r10
    {
        const Vector3 lvWorldUp(0.0f, 1.0f, 0.0f);               // unk_82181510
        if (KF_ROLLED_CRASH_UP_COSINE > rw::math::vpu::Dot(lTransform.yAxis, lvWorldUp))   // flt_82FB914C > up.y
        {
            lbRolledOntoWorld = lContactNormal.GetY() > KF_ROLLED_CRASH_MIN_NORMAL_Y;    // vspltw128 v9, v125, 1 ; > unk_82FB82B0
        }
    }

    // ---- [world-crash] the DECISION, both sides of every compare ------------------------------
    // DIAG. NOT IN THE X360 BINARY. Opt-in (BRN_WORLD_CRASH_DIAG=1). Prints every classified
    // contact that got past the separating test -- crash or not -- so a run can show WHY a wall
    // hit did or did not commit: the head-on cosine vs the normal's forward component, the two
    // scaled closing speeds vs their thresholds, the world point in the car's frame, the roll
    // cosine. Budget: one line per validated contact per frame; a wall hit is a handful of
    // lines, a scrape along a barrier can be tens. DELETE-WHEN-STABLE.

    // 0x8263EB68..0x8263EB88: none of the three fired -> not a crash.
    if (!lbHeadOnCrash && !lbSideOnCrash && !lbRolledOntoWorld)
        return;

    // ---- 0x8263EB8C..0x8263EBE8: PLAYER CORNER CLIP. A head-on whose world point lies within 0.5
    //      of either side of the box, on the player's car.
    bool lbPlayerCornerClip = false;                                                // r11
    if (lbHeadOnCrash)
    {
        const f32 lfFromRightSide = lrBox.mMax.GetX() - lvWorldPointLocal.GetX();             // vsubfp v0, max.x, Bx
        const f32 lfFromLeftSide  = lvWorldPointLocal.GetX() - lrBox.mMin.GetX();             // vsubfp v12, Bx, min.x
        const f32 lfFromNearestSide = (lfFromRightSide < lfFromLeftSide) ? lfFromRightSide : lfFromLeftSide;   // vminfp
        if (KF_CORNER_CLIP_WIDTH > lfFromNearestSide                                // vcmpgtfp128. v123(0.5), v0
            && static_cast<u32>(mePlayerActiveRaceCarIndex) == lu16RaceCarIndex)   // lwzx this+0x2A0AC ; cmpw r4
        {
            lbPlayerCornerClip = true;
        }
    }

    // ---- 0x8263EBEC..0x8263EC2C: the slow-motion inhibit this crash will leave behind. A fresh
    //      corner clip inhibits; a re-hit while already crashing keeps whatever was latched, and
    //      only if this contact is a corner clip too.
    bool lbForceNoSlowMo = false;                                                   // r29
    if (!lrRaceCar.IsCrashing() && lbPlayerCornerClip)                              // lbz rec+0x710
    {
        lbForceNoSlowMo = true;
    }
    if (lrRaceCar.IsCrashing())
    {
        lbForceNoSlowMo = lbPlayerCornerClip ? mbForceNoSlowMo : false;            // lbzx this+0x2A11D
    }

    // 0x8263EC30: the debug snapshot (this + 161968 == mDebugComponent, &arg_20 == the by-value copy).
    mDebugComponent.RecordCrashContact(&lContact);

    // 0x8263EC40..0x8263EC74: THE COMMIT. Victim = the race car's entity word, aggressor = the world
    // entity word, normal SIGN-FLIPPED towards the victim (`vslw v13,v124,v124 ; vxor v1, v0, v13`),
    // point = v122 == the race-car contact point IN THE CAR'S FRAME (not the world point -- the
    // console passes the local vector it built at 0x8263E75C), takedown type -1 (NONE).
    ::EntityId lVictimID;
    lVictimID.muValue = luRaceCarEntityWord;
    ::EntityId lAggressorID;
    lAggressorID.muValue = luWorldEntityWord;

    SetRaceCarCrashing(lVictimID, lAggressorID,
                       rw::math::vpu::Negate(lContactNormal), lvRaceCarPointLocal,
                       lpRequestOutputInterface,
                       lpManagerOutputInterface,
                       lpVehicleOutputInterface,
                       lpDeformationInterface,
                       BrnGameState::E_TAKEDOWN_NONE);

    mbForceNoSlowMo = lbForceNoSlowMo;   // stbx r29, this, 0x2A11D
}

// -------------------------------------------------------------------------------------------
// PredictCarWorldContactTime  @0x825B5300 (83)  -- DWARF BrnVehicleManager.h:1215
//
// The impact-time key HandleCrashPredictionForRaceCarAndWorld orders each car's world contacts by:
//     t = (pA - pB) . n  /  v . (-n)
// i.e. the separation along the normal over the closing speed along it, plus a whole second when
// the normal is within ~26 degrees of the car's own up axis (n . up > 0.9 -- a ground-like contact
// is pushed to the back of the queue). The division is the console's vrefp + two Newton steps,
// de-optimised to an exact divide; the result is splatted to all four lanes, and the driver reads
// lane .x.
// -------------------------------------------------------------------------------------------
VecFloat VehicleManager::PredictCarWorldContactTime(const CgsSceneManager::SceneManagerIO::PotentialContact& lContact)
{
    const u32 luRaceCarEntityWord = EntityWordOf(lContact.muVolumeInstanceIdA);
    const u32 luWorldEntityWord   = EntityWordOf(lContact.muVolumeInstanceIdB);

    CGS_ASSERT((luRaceCarEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
               "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");  // :2839
    CGS_ASSERT((luWorldEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");    // :2840

    // 0x825B53A0 / 0x825B53E4: `extrwi r9, r9, 14,8 ; mulli r11, r9, 0x1460 ; add r11, r11, this`.
    const u16 lu16RaceCarIndex = EntityIndexOfWord(luRaceCarEntityWord);
    const RaceCarPhysics& lrRaceCar = maRaceCarVehicles[lu16RaceCarIndex];

    const Vector3 lLinearVelocity = lrRaceCar.GetLinearVelocity();        // lvx128 v9, r11, 0x790 (rec +0x50)
    const Vector3 lUpAxis         = lrRaceCar.GetTransform().yAxis;       // lvx128 v9, r11, 0x760 (rec +0x20)

    const f32 lfSeparation   = rw::math::vpu::Dot(lContact.mPointOnA - lContact.mPointOnB, lContact.mNormal);   // v11
    const f32 lfClosingSpeed = rw::math::vpu::Dot(lLinearVelocity, rw::math::vpu::Negate(lContact.mNormal));   // v13
    const f32 lfNormalUp     = rw::math::vpu::Dot(lContact.mNormal, lUpAxis);                                   // v0

    f32 lfImpactTime = lfSeparation / lfClosingSpeed;                     // vrefp + 2x NR, then vmulfp128
    if (lfNormalUp > KF_GROUND_NORMAL_UP_COSINE)                          // vcmpgtfp. v0, v0, splat(0.9)
    {
        lfImpactTime += 1.0f;                                             // vaddfp v0, v0, v12 (vcfsx 1)
    }

    return VecFloat(lfImpactTime);
}

}   // namespace Vehicle
}   // namespace BrnPhysics

#undef CGS_ARTIST_FILE
