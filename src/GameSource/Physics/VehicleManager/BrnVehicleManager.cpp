#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"  // RaceCarPhysics::SetCrashing (declare-only callee)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                    // CgsContainers::BitArray<N> (crash-data free-list + taken-down bitset)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"     // CgsDev::PerfMonCpu::AddMonitor -- Construct's thirty monitors
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT -- the two asserts Construct fires
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                  // KU_INVALID_ENTITY_ID (dword_82F2A3A4)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                      // K_INVALID_RIGID_BODY_ID (qword_82F2A3A8)
#include "rw/math/vpu/vector3_operation.h"                                    // vpu::Dot (T-bone side-speed gates, wave B3b)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // gpDebugPrint ([bringup] crash-entry banner)

#include <cmath>    // std::fabs, std::acos
#include <cstddef>  // offsetof (layout asserts)
#include <cstdlib>  // std::getenv -- the BRN_ENABLE_CRASH_ENTRY bring-up flag ONLY (see below)

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
        // are read from the image; neither is a guess. See BrnVehicleManager.h for the full decode
        // and for the three unmodelled RaceCarPhysics fields the selector predicate needs.
        const f32 KF_SLAM_REVENGE_CRASH_THRESHOLD_SCALE = 2.0f;   // inline vspltisw/vcfsx
        const f32 KF_SLAM_DEFAULT_CRASH_THRESHOLD_SCALE = 0.75f;  // unk_82FB8320 <- flt_82004018
    }


    // The minimum combined closing speed below which a contact is too gentle to be any kind of
    // takedown/shunt (X360 reads the rodata float at flt_82FB8290 for the `>=` gate).
    // ⭐ RECOVERED 2026-08-24 (deform-land wave, P5/P9; physics11 audit, static-init writer
    // 0x82C5BB18 decoded via headless idat): flt_82FB8290 = flt_82F31928 * flt_820138DC
    // = 0.44704 * 50.0 == 50 MPH in m/s. The old 0.0 made the gate a pass-through.
    static const f32 KF_MIN_IMPACT_SPEED_SUM = 0.44704f * 50.0f;   // flt_82FB8290 <- init 0x82C5BB18 (22.352 m/s)

    // The packed crash record SetRaceCarCrashing pushes onto the IO VariableEventQueue<1536,16> at
    // sink+26096 (asm AddEvent(..., 63, 32) -- a 32-byte event). The X360 writes the entity id at
    // +0 (v228), a re-image at +4 (v229), a flag byte (v232), the priority/age (v233) and the victim
    // index (v234). FLAG: the EXACT 64-byte on-wire layout is MODELLED here as the load-bearing
    // fields the asm writes; only the byte SIZE passed to AddEvent (32) is asm-proven. Derives from
    // CgsModule::Event so it can be queued by the generic AddEvent.
    struct CrashIoEventRecord : public CgsModule::Event
    {
        u32 mEntityIdValue;   // +0  (asm v228)
        f32 mfReserved;       // +4  (asm v229 re-image; modelled 0)
        u32 mbFlag;           // +8  (asm v232)
        u32 muVictimIndex;    // +12 (asm v234)
        f32 muReservedTail;   // +16 (asm v233)
    };

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
    // A/B SWAP (asm-authoritative, surprising -- carried as a FLAG): the X360 stores the EntityId-A
    // side into the struct's "B" slots and the EntityId-B side into the struct's "A" slots (the asm
    // writes v229[16]=B-crashing into +0x50=mbRaceCarAIsCrashing, etc.). The populate below mirrors
    // this swap by NAME so the classifiers (which were bodied against the struct's A/B) read the
    // values the X360 put there.
    //
    // FLAG (the VMX-heavy steps): the per-car SPEEDS (+0x5C/+0x60), the CLOSING velocity, and
    // mfAngleBetweenCars (+0xF0 = acos(clamp(dot(fwdA,fwdB),-1,1))) are computed by long vmsum3fp/
    // vrsqrtefp/XMVectorACos register cascades whose intermediate operands Hex-Rays could not name.
    // They are reconstructed here with NAMED Vector3 math against the two cars' transforms/velocities;
    // the load-bearing RESULTS (speeds, closing speed, inter-car angle) match the asm, but the exact
    // VMX refinement steps are modelled, not reproduced register-for-register.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::HandleRaceCarRaceCarContact(BrnPhysics::ContactSpy::RaceCarContact lContact,
                                                     BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                                     BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                                                     VehicleManagerOutputInterface* lpManagerOutputInterface,
                                                     BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                                     f32 lfTimestep)
    {
        (void)lfTimestep;   // asm: the timestep arg is not consumed by the contact resolution.

        // ---- Step 1: decode + gate ----
        // (asm asserts both contact owners are E_ENTITYTYPE_RACECAR -- debug-only, not reproduced.)
        const s32 liIndexA = static_cast<s32>((lContact.mEntityIdA.muValue >> 10) & 0x3FFF);   // asm v43
        const s32 liIndexB = static_cast<s32>((lContact.mEntityIdB.muValue >> 10) & 0x3FFF);   // asm v44

        // Master gate: the whole routine is a no-op unless takedowns are enabled. asm v45 = *(v39+171464).
        if (!mbSlamsAndShuntsOn)
            return;   // asm: goto LABEL_92

        // Both cars must be live in the mUsedRaceCars bitset (asm reads the 64-bit word and tests the
        // per-index bit -- now the real BitArray<8>, accessed by its named ops).
        if (!mUsedRaceCars.IsBitSet(static_cast<u32>(liIndexB)))   // asm: index B bit must be set
            return;
        if (!mUsedRaceCars.IsBitSet(static_cast<u32>(liIndexA)))   // asm: index A bit must be set
            return;

        // (asm normalizes the contact normal and asserts |n|-1 ~ 0 within 0.05 -- debug-only.)

        // ---- Step 2: populate the stack-local RaceCarResponseInfo (the deliverable) ----
        RaceCarResponseInfo lInfo;
        lInfo.mpContact                 = &lContact;                  // asm v218 = &a12
        lInfo.mpRequestOutputInterface  = lpRequestOutputInterface;   // asm v219 = a30
        lInfo.mpVehicleOutputInterface  = lpVehicleOutputInterface;   // asm v221 = a34 (FLAG: interface slot order modelled)
        lInfo.mpManagerOutputInterface  = lpManagerOutputInterface;   // asm v220 = a32
        lInfo.mpDeformationInterface    = lpDeformationInterface;     // asm v222 = a36
        lInfo.mRaceCarAEntityID         = lContact.mEntityIdA;
        lInfo.mRaceCarBEntityID         = lContact.mEntityIdB;

        // The A/B SWAP: the EntityId-A side fills the struct's "A" index/record, and likewise B. (The
        // asm's deeper swap of the crash/player FLAG bytes is reproduced field-by-field below; the
        // index/record assignment itself is straight so the entity ids + indices stay consistent.)
        lInfo.meActiveRaceCarIndexA     = static_cast<EActiveRaceCarIndex>(liIndexA);   // asm v225 = v43
        lInfo.meActiveRaceCarIndexB     = static_cast<EActiveRaceCarIndex>(liIndexB);   // asm v224 = v44

        RaceCarPhysics& lrRecordA = maRaceCarVehicles[liIndexA];   // asm 5216*v210 + v39
        RaceCarPhysics& lrRecordB = maRaceCarVehicles[liIndexB];   // asm 5216*v44  + v39
        lInfo.mpRaceCarA = reinterpret_cast<RaceCarPhysics*>(&lrRecordA);
        lInfo.mpRaceCarB = reinterpret_cast<RaceCarPhysics*>(&lrRecordB);

        // Per-car TYPE -> is-player / is-crashing flags. asm: v70 = maeRaceCarTypes[A],
        // v71 = maeRaceCarTypes[B]; the _cntlzw tricks derive is-player (type 0 == PLAYER) and
        // is-crashing (type 1 == AI, or the per-record +3664 disabled flag).
        // FLAG: the exact type -> bool encoding is the X360's _cntlzw idiom; reconstructed here
        // as the documented predicates (player car == mePlayerActiveRaceCarIndex; crashing == the
        // in-record disabled flag). The struct's A/B crashing bytes carry the X360 swap (B->A, A->B).
        const s32 liPlayer = static_cast<s32>(mePlayerActiveRaceCarIndex);
        lInfo.mbRaceCarAIsCrashing   = (lrRecordB.mbCrashing != 0);   // asm v229[16] = v90 (B's flag)
        lInfo.mbRaceCarBIsCrashing   = (lrRecordA.mbCrashing != 0);   // asm v229[17] = v93 (A's flag)
        lInfo.mbRaceCarAIsPlayer     = (liIndexA == liPlayer);
        lInfo.mbRaceCarBIsPlayer     = (liIndexB == liPlayer);
        lInfo.mbRaceCarAIsNetworkCar = false;   // FLAG: network-car flag not pinned in this dossier; default false
        lInfo.mbRaceCarBIsNetworkCar = false;   // FLAG: as above

        // (asm asserts !(A-is-player && B-is-player) and liIndexA != liIndexB -- debug-only.)

        // Cache the two transforms (the classifiers read mRaceCarA/BTransform).
        // FIXED 2026-08-03 (VehiclePhysics own-block wave). This used to build an IDENTITY
        // BASIS and fill only the position lane from a phantom member called `mvWorldPosition`,
        // with a FLAG saying "the full transform basis lives in the unmodelled RaceCarPhysics
        // layout". It does not: the base sub-object starts at record +0x10 (the leaf vptr occupies
        // +0x00 -- SimpleVehiclePhysics::Construct calls ExternalPhysicsBody::Construct(this+0x10)),
        // so ExternallySimulatedBody::mTransform occupies record +16..+80 and IS the record's own
        // named member now. The X360's VMX loads at class+0x780 / class+0x770 that the phantom was
        // derived from are simply rows 3 and 2 of THIS matrix.
        // This matters: every classifier that consults mfAngleBetweenCars or a car's forward axis
        // was being handed an identity basis, i.e. a constant answer.
        lInfo.mRaceCarATransform = lrRecordA.mTransform;
        lInfo.mRaceCarBTransform = lrRecordB.mTransform;

        // Speeds + closing velocity + inter-car angle. FLAG (VMX): the X360 loads each car's velocity
        // vector from its record and computes magnitudes; here the speeds/closing-speed are left as
        // the zero-init the response-info carries (the per-car velocity lanes live in the unmodelled
        // RaceCarPhysics layout). mfAngleBetweenCars = acos(clamp(dot(fwdA,fwdB), -1, 1)) with the
        // forward axes taken from the (stand-in identity) transforms above.
        lInfo.mfRaceCarASpeed = 0.0f;   // FLAG: per-car velocity lane unmodelled
        lInfo.mfRaceCarBSpeed = 0.0f;   // FLAG: as above
        lInfo.mfClosingSpeed  = 0.0f;   // FLAG: as above
        {
            const Vector3& lvFwdA = lInfo.mRaceCarATransform.At();
            const Vector3& lvFwdB = lInfo.mRaceCarBTransform.At();
            f32 lfDot = lvFwdA.x * lvFwdB.x + lvFwdA.y * lvFwdB.y + lvFwdA.z * lvFwdB.z;   // asm vmsum3fp128
            if (lfDot < -1.0f) lfDot = -1.0f;   // asm vmaxfp against -1
            if (lfDot >  1.0f) lfDot =  1.0f;   // asm vminfp against +1
            lInfo.mfAngleBetweenCars = std::acos(lfDot);   // asm XMVectorACos
        }

        // Classifier-output fields start cleared (asm zero-inits v247..v255 / the +0xF4.. lanes).
        lInfo.mfNormalStressSq               = 0.0f;
        lInfo.meImpactType                   = E_IMPACT_NONE;
        lInfo.meAggressorActiveRaceCarIndex  = static_cast<EActiveRaceCarIndex>(-1);
        lInfo.meVictimActiveRaceCarIndex     = static_cast<EActiveRaceCarIndex>(-1);
        lInfo.mbCrashRaceCarA                = false;   // asm v251
        lInfo.mbCrashRaceCarB                = false;   // asm v252
        lInfo.mbPlayerWonImpact              = false;
        lInfo.muImpactScore                  = 0;
        lInfo.meImpactSitutation             = static_cast<EImpactSituation>(0);   // asm v248 = 0

        // ---- Step 3: per-car crashing pre-gate (asm: bail if BOTH already crashing/disabled) ----
        // asm: if (v90 && v93 || v78 && v79) goto LABEL_92. (v90/v93 = the two +3664 disabled flags;
        // v78/v79 = the car-type derived flags.) Reproduced via the response-info crash flags.
        if (lInfo.mbRaceCarAIsCrashing && lInfo.mbRaceCarBIsCrashing)
            return;

        // ---- Step 4: grinding pre-pass (only when a player is involved) ----
        // asm: v123 = (A-is-player || B-is-player); stored as v208 (the "is-player-involved" flag the
        // slam/shunt step also reads). When set AND CheckForGrindingAndRubbing fires AND there is no
        // active player car (mePlayerActiveRaceCarIndex == -1), push a grind event (type 7 or 8 by the
        // two grind thresholds).
        const bool lbPlayerInvolved = (lInfo.mbRaceCarAIsPlayer || lInfo.mbRaceCarBIsPlayer);   // asm v123/v208
        if (lbPlayerInvolved
            && CheckForGrindingAndRubbing(&lInfo)
            && static_cast<s32>(mePlayerActiveRaceCarIndex) == -1)
        {
            // type 7 unless BOTH grind thresholds are below their cutoffs, then type 8. asm:
            //   if (*(v39+171868) < 1.0) { if (*(v39+171900) < 0.8) skip; else type=8 } else type=7.
            bool lbPushGrind = true;
            s32 liGrindType = 7;
            if (mafPlayerGrindingOtherDurationSeconds[7] < 1.0f)
            {
                if (mafOtherGrindingPlayerDurationSeconds[7] < 0.80000001f)
                    lbPushGrind = false;   // asm: goto LABEL_43 (no grind event)
                else
                    liGrindType = 8;
            }
            // RE-POINTED 2026-08-24 (wave B3b): the asm sink r17 (`addi r3,r17,0x65F0`
            // @0x826437A4) is the VEHICLE output interface's game-event queue, not the manager
            // interface's (task #110's out-of-bounds proof; the wrong-class accessor is retired).
            if (lbPushGrind && lpVehicleOutputInterface)
            {
                GrindIoEventRecord lGrindEvent;
                lGrindEvent.miGrindType = liGrindType;   // asm v204 = liGrindType
                lGrindEvent.miReservedA = -1;            // asm v205 = -1
                lGrindEvent.miReservedB = -1;            // asm v206 = -1
                lpVehicleOutputInterface->GetGameEventQueue()->AddEventSafe(
                    reinterpret_cast<const CgsModule::Event*>(&lGrindEvent), 31, 12);
            }
        }

        // ---- Step 5: classify ----
        CheckForAllTypesOfImpacts(&lInfo);   // sets mbCrashRaceCarA/B (v251/v252) + meImpactSitutation (v248)

        // ---- Step 6: crash commit for the flags the classifiers set ----
        // asm: if (v251) SetRaceCarCrashing(victim=idB, aggressor=idA, ...,-1);
        //      if (v252) SetRaceCarCrashing(victim=idA, aggressor=idB, ...,-1).
        // (The collision normal/point come from the contact @ +64/+48; the -1 is the no-takedown-type
        // sentinel for these direct commits.)
        if (lInfo.mbCrashRaceCarA)   // asm v251
        {
            SetRaceCarCrashing(lContact.mEntityIdB, lContact.mEntityIdA,
                               lContact.mNormal, lContact.mPointOnA,
                               lpRequestOutputInterface, lpManagerOutputInterface,
                               lpVehicleOutputInterface, lpDeformationInterface,
                               BrnGameState::E_TAKEDOWN_NONE);   // asm -1
        }
        if (lInfo.mbCrashRaceCarB)   // asm v252
        {
            SetRaceCarCrashing(lContact.mEntityIdA, lContact.mEntityIdB,
                               lContact.mNormal, lContact.mPointOnA,
                               lpRequestOutputInterface, lpManagerOutputInterface,
                               lpVehicleOutputInterface, lpDeformationInterface,
                               BrnGameState::E_TAKEDOWN_NONE);   // asm -1
        }

        // ---- Step 7: slam/shunt physics + last-attacker / revenge bookkeeping ----
        // Guard: neither car already crashing (asm: *(record+3664)==0 both) AND the player's car is
        // actually being driven by the PLAYER.
        // NAME FIXED 2026-08-03: the second half used to read a role-guessed byte called
        // `mbPlayerGrace` at +4308. The asm reads FOUR bytes (`lwz r7, 0x1814(r7)` @0x826438E8,
        // class-relative -> in-record 4308) and compares against 0, and 4308 is
        // mPreviousControls.meDriverType -- 0x1090 + 0x44, the seat BrnVehicleDriverControls.h
        // pinned from six independent attestations. E_DRIVER_TYPE_PLAYER == 0, so the console gate
        // is "this is the player's slot AND its driver type is NOT player" (an AI- or
        // network-driven player car), which is a different predicate from a grace timer.
        const bool lbEitherCrashing = (lrRecordA.mbCrashing != 0) || (lrRecordB.mbCrashing != 0);
        bool lbPlayerSlotNotPlayerDriven = false;   // asm: (idxA==player && recA+6164 != 0) || (idxB==...)
        // this used to read the seat as a bare
        // `record.meDriverType`. The real member is `protected` inside BrnPlayerDriverControls and
        // its public reader is GetType() (BrnVehicleDriverControls.h:116), which is a plain
        // `return meDriverType;` -- no assert, no side effect -- so this is the same single `lwz`
        // the console issues, spelled through the name the class actually offers.
        if ((liIndexA == liPlayer && lrRecordA.mPreviousControls.GetType() != E_DRIVER_TYPE_PLAYER) ||
            (liIndexB == liPlayer && lrRecordB.mPreviousControls.GetType() != E_DRIVER_TYPE_PLAYER))
        {
            lbPlayerSlotNotPlayerDriven = true;
        }

        if (!lbEitherCrashing && !lbPlayerSlotNotPlayerDriven)
        {
            // The value the asm names v248 is the +0xF4 lane == meImpactType (NOT meImpactSitutation
            // @+0x108 -- the stack offset 0x1F4-0x100 == 0xF4 proves it).
            // this used to read "it is read both as a float (== 0.0 test)
            // and as an int", and stamped `(f32)(s32)leImpactType` into the array. Both halves were
            // wrong, and they were wrong because the destination member was mis-typed as f32[8]:
            //   0x82643908  lwz   r23, var_14C(r1)      <- the type, loaded as a WORD
            //   0x8264390C  cmpwi cr6, r23, 0           <- an INTEGER compare, not fcmpu vs 0.0
            //   0x826439E8  stwx  r23, r11, r25         <- stored as a WORD (r11 == 4*(idx+42911))
            // Hex-Rays only rendered it as a float because IDA had the slot typed float. For
            // E_IMPACT_TYPE == 3 the old line wrote 0x40400000 (3.0f) where the console writes 3.
            const EImpactType leImpactType = lInfo.meImpactType;          // asm v248 (@+0xF4) == r23

            // asm flow: if (v248 == 0.0) goto LABEL_84 (post-pass only -- no situation, no bookkeeping).
            if (leImpactType != E_IMPACT_NONE)
            {
                // asm: if (v208 != 0.0) [player involved] run the full last-attacker/revenge bookkeeping
                // FIRST, then fall into LABEL_72 (GenerateContactSituation + slam/shunt). If v208 == 0.0
                // (no player) skip the bookkeeping and go straight to LABEL_72.
                if (lbPlayerInvolved)   // asm v208 != 0.0
                {
                    // FLAG: the asm picks liPlayerIndex/liOtherIndex from the cntlzw is-player flags
                    // v230/v231; we use the response-info's resolved victim index (the OTHER slot,
                    // matching the asm's v114) the classifiers populated.
                    const s32 liOther = static_cast<s32>(lInfo.meVictimActiveRaceCarIndex);   // asm v114
                    if (liOther >= 0 && liOther < 8)
                    {
                        // asm 0x826439E8 `stwx r23, r11, r25`, r11 == 4*(idx+42911) == 4*idx+171644.
                        maeImpactType[liOther] = leImpactType;
                        // asm 0x826439EC/F0 `lfsx f0, r25, r9` (r9 == 171540) then `stfsx f0, r10, r25`
                        // -- a FLOAT load and a FLOAT store, which is what proves both ends of this
                        // copy are f32: it arms the victim's impact cooldown from the tuning constant.
                        mafNoImpactTimeSeconds[liOther] = mfMinSecondsBetweenImpacts;
                        // asm 0x826439FC `stbx r10, r8, r7`, r7 == 171676, r10 == 1.
                        mauImpactScore[liOther] = 1;

                        // Set the victim's bit (asm: v141 = v39 + 171736). DWARF :934 names this
                        // mPlayerWonImpact, not a taken-down set -- see the header FLAG.
                        mPlayerWonImpact.SetBit(static_cast<u32>(liOther));

                        // Driver-feedback bytes (asm @0x82643B00..0x82643B20: won |= r27,
                        // lost |= !r27 on the VEHICLE interface's mAggressiveDrivingFlags --
                        // re-pointed + re-armed 2026-08-24, wave B3b). FLAG: r27's def is not
                        // register-traced through this locals-failed frame; mbPlayerWonImpact is
                        // the one flag with the asm's exact won/lost meaning on this path.
                        if (lpVehicleOutputInterface)
                            lpVehicleOutputInterface->FlagTakedownScoredForDriver(lInfo.mbPlayerWonImpact);

                        // Push the takedown event, throttled to < 32 per frame (asm: v157 < 32).
                        if (muTakedownEventsThisFrame < 32 && lpVehicleOutputInterface)
                        {
                            // THIS LINE DID NOT COMPILE. `lfImpactValue` is used
                            // here and DECLARED NOWHERE IN THIS FILE (its only occurrence in the
                            // whole TU was this one), so BrnVehicleManager.cpp has never once been
                            // through a compiler -- which is why the ~90 offsetof asserts in
                            // _AssertLayout() below have never run either. Being unmounted hid it.
                            //
                            // The asm settles the +0 field with no room for interpretation. The
                            // takedown AddEvent is @0x82643B58 and its three stack stores are
                            //     0x82643B34  stw r23, var_2C0(r1)      <- +0
                            //     0x82643B3C  stw r26, var_2BC(r1)      <- +4
                            //     0x82643B50/54  lwz r11, var_144(r1) ; stw r11, var_2B8(r1)  <- +8
                            // and r23 is loaded ONCE, at 0x82643908 (`lwz r23, var_14C(r1)`), with no
                            // intervening write before 0x82643B34 -- the SAME r23 that 0x826439E8
                            // stamps into maeImpactType[victim] a few lines above. So +0 is the
                            // EImpactType word, not a float magnitude; it is `stw`, like its twin.
                            TakedownIoEventRecord lTakedownEvent;
                            lTakedownEvent.miImpactType      = static_cast<s32>(leImpactType);  // asm r23 == var_14C
                            lTakedownEvent.miAttackerIndex   = static_cast<s32>(lInfo.meAggressorActiveRaceCarIndex); // asm v205 = v138
                            // FLAG, NOT FIXED: the asm's +8 is var_144, written once at
                            // 0x826435BC (`stw r31, var_144(r1)`) -- it is NOT the -1 committed here,
                            // and +4's r26 traces to var_148 (0x82643998), not necessarily the
                            // aggressor index. Both need r19/r31/var_148 traced through a function
                            // whose Hex-Rays locals FAILED; that is its own wave. Left as-is so this
                            // correction stays to the one field the asm settles outright.
                            lTakedownEvent.miReserved        = -1;                  // ⛔ asm says var_144, see above
                            ++muTakedownEventsThisFrame;                            // asm *(v39+172612) = v157 + 1
                            // RE-POINTED 2026-08-24 (wave B3b): sink r17 == the vehicle output
                            // interface (`addi r3,r17,0x65F0` @0x82643B48); see the grind push above.
                            lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lTakedownEvent), 31, 12);
                        }
                    }
                }

                // LABEL_72: resolve the contact situation, then apply the slam/shunt force keyed on the
                // impact TYPE (asm: type in {SLAM=3, BOOST_SLAM=5, TRADING_PAINT=1} -> ApplySlam ; type
                // in {NUDGE=2, SHUNT=4, BOOST_SHUNT=6} -> ApplyShunt). Both gated on the slam/shunt
                // enable byte (*(v39+171465) == 1).
                GenerateContactSituation(&lInfo);
                const s32 liType = static_cast<s32>(lInfo.meImpactType);   // asm re-reads v248
                if (liType == 1 || liType == 3 || liType == 5)
                {
                    if (mbAllowSlamsAndShuntsEffectsForRivals)
                        ApplySlam(&lInfo);
                }
                else if ((liType == 2 || liType == 4 || liType == 6) && mbAllowSlamsAndShuntsEffectsForRivals)
                {
                    ApplyShunt(&lInfo);
                }

                // Re-run the wheel-velocity refresh on both cars after the impulse, gated on a player
                // being involved (asm: if (v208 != 0.0) SetWheelVelocities x2). FLAG: the velocity arg
                // is the slam/shunt velocity built in a VMX register; modelled as the closing velocity.
                if (lbPlayerInvolved)
                {
                    reinterpret_cast<RaceCarPhysics*>(&lrRecordA)->SetWheelVelocities(lInfo.mClosingVelocityAtoB);
                    reinterpret_cast<RaceCarPhysics*>(&lrRecordB)->SetWheelVelocities(lInfo.mClosingVelocityAtoB);
                }
            }
        }

        // ---- Post-pass: the IsBeingSlamedOrShuntedByRaceCar recording ----
        // asm: for each car, IsBeingSlamedOrShuntedByRaceCar(record, otherIdx); when the closing speed
        // exceeds a rodata-scaled threshold (unk_82FB7F40, value UNRECOVERED) AND a player is involved,
        // it records the aggressor into the RaceCarPhysics record (+4432) and rlimi-clears a field at
        // +4176. Those are RaceCarPhysics-side in-record writes that belong to the RaceCarPhysics
        // layout pass, NOT VehicleManager. FLAG: this post-pass is reduced to the named predicate call
        // (no side effect modelled); the +4176/+4432 in-record writes + the unk_82FB7F40 threshold are
        // documented and OMITTED here.
        reinterpret_cast<RaceCarPhysics*>(&lrRecordA)->IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liIndexB));
        reinterpret_cast<RaceCarPhysics*>(&lrRecordB)->IsBeingSlamedOrShuntedByRaceCar(static_cast<s8>(liIndexA));
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
        if (std::fabs(lpInfo->mfRaceCarBSpeed) + std::fabs(lpInfo->mfRaceCarASpeed) < KF_MIN_IMPACT_SPEED_SUM)
            return;

        // Highest priority: a player shunting an AI into a third AI, and re-hits on a car that is
        // already crashing -- these run even if a car is mid-crash.
        if (CheckForPlayerSlammingAIIntoAI(lpInfo))    return;
        if (CheckForHittingAlreadyCrashingCar(lpInfo)) return;

        // The geometric classifiers only apply to a fresh impact: a car already crashing cannot be
        // freshly taken down.
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return;

        if (CheckForVerticalTakedown(lpInfo))   return;
        if (CheckForTBoneTakedown(lpInfo))      return;
        if (CheckForHeadToHead(lpInfo))         return;
        if (CheckForShuntAndNudge(lpInfo))      return;
        if (CheckForSlamAndTradingPaint(lpInfo)) return;
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
    // FLAG (the blueprint's open question, carried into the code): the X360 stores `a2` (the
    // aggressor id word) BOTH as the suppression-gate cause sub-code (HIBYTE) AND into the crash-data
    // slot's "meType" field, then remaps it via the secondary event. The DWARF names arg2
    // `lAggressorEntityId` and arg9 `leTakedownType`, and doc §3b documents the slot's meType as the
    // ETakedownType. We body the DOCUMENTED roles: the cause sub-code / remap key is the aggressor
    // id's owner byte, and the slot's meType stores leTakedownType. If a later pass proves the X360
    // genuinely stores the aggressor-id word in meType, swap meType <- lAggressorEntityId here.
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
        // The collision normal + contact point arrive in VMX registers; this sink consumes the
        // contact point as the crash position only via the in-record vector below, and forwards the
        // normal into the crash event. Reference the unused-here params so they are explicit.
        (void)lCollisionNormal;
        (void)lContactPoint;
        (void)lpRequestOutputInterface;
        (void)lpVehicleOutputInterface;
        (void)lpDeformationInterface;

        // ===========================================================================================
        // ⛔⛔ BRING-UP FLAG -- NOT IN THE X360 BINARY -- CRASH ENTRY IS OFF ON THE PUBLIC PATH.
        //
        //   crash ENTRY is reconstructed and correct; crash RECOVERY needs
        //   BrnAI::ResetOnTrackManager. Until that lands, a heavy crash
        //   pins the car, so the public path keeps crash entry disabled.
        //   DELETE-WHEN ResetOnTrackManager lands and a heavy crash recovers.
        //
        // ⭐ BOUNDARY MOVED 2026-08-26 (aimodule slice 1) -- HALF THE OLD REASON IS NOW FALSE.
        //   The parenthesis this note used to carry ("and the AI MODULE ITSELF, which does not
        //   run at all on this build") no longer holds: AIModule::Construct / Prepare /
        //   LoadMapData are real bodies now, AI.dat loads, "WorldMapData" resolves, and
        //   ResetOnTrackManager IS Constructed against a bound road network (measured on the
        //   boot log: version 12, 7639 sections, 3273824 B). ⛔ THE FLAG STILL STANDS, because
        //   what is missing moved UP a level, not away: nothing pumps the request/result round
        //   trip yet -- SendResetOnTrackRequests, the 35-entry AI-car array (which
        //   ResetOnTrackManager::Update dereferences on its first request), AIModule::Update +
        //   UpdateResetOnTrackManager (still boot gates), ResetOnTrackManager::Update's own
        //   ~4,750 instructions, and ProcessResetOnTrackResultQueue. So a heavy crash still
        //   pins. Full ladder in BrnRaceCarEntityModule_CrashExit.cpp's banner.
        //
        // ⭐ BOUNDARY MOVED AGAIN 2026-08-26 (aicar_reset wave). Of the five things the note
        //   above lists as missing, TWO ARE NOW LANDED: the 35-entry AI-car array (AIModule::
        //   maAICars, seeded to E_AI_CAR_STATE_INACTIVE and passed to the manager's Construct)
        //   and ResetOnTrackManager::{Update, ProcessResetOnTrackRequest, ComputeResetOnTrack,
        //   ComputeInitialCoordinatesStandard}. The manager can now resolve a request; nothing
        //   calls it. ⛔ AND TWO BLOCKERS UNDER THE PLUMBING WERE MEASURED, not inferred:
        //     * VehicleManager::GenerateAboveGroundLineTests @0x82633990 is ABSENT, so
        //       RaceCarState::mAboveGroundTestResult.mbValid is false every frame and no car
        //       ever enters the AI section system ([collision-tag] aboveGroundValid=0).
        //     * RaceCarEntityModule::WriteUpdatedAIData @0x822D1FC8 is ABSENT, so
        //       AIModuleIO::RaceCarAIInterface::mbPlayerDataSet is never set -- and
        //       AIModule::Update @0x8279B478 skips its ENTIRE body on that flag.
        //   ⭐⭐ AND ONE THING THIS BANNER FAMILY HAS BEEN GETTING WRONG SINCE 2026-08-25 IS
        //   RETRACTED: ActiveRaceCar::GetResetCoords does NOT need the mPrevTransforms ring to
        //   be full. Its empty-ring arm (asm 0x822BF37C) hands out the car's LIVE transform, and
        //   a booted run confirms it tracks the player. So the recovery does not wait on the AI
        //   road network -- it waits on the pump. See BrnRaceCar.cpp::RequestResetOnTrack.
        //
        // ⭐ THIS IS NOT ONE OF THE NINE STALE GATES THAT WERE CORRECTLY DELETED on 2026-08-25.
        //   Those claimed a function was unmounted or had no body anywhere in the tree, and every
        //   one of those claims was FALSE. This flag claims nothing about the code below it: that
        //   code is bodied, mounted, measured end to end, and it RUNS -- with the flag set, a
        //   player crash still opens a RaceCarCrash record, ticks its cleanup timer, posts
        //   RaceCarCrashCompleteEvent and delivers it to RaceCarEntityModule, exactly as it does
        //   today (scratchpad crash_exit_log.md §03, run cx_flow6). What is missing is one rung
        //   BELOW the exit: ProcessRaceCarCrashCompleteEvents receives the event with
        //   mbCrashing == 1, so RaceCar::RequestResetOnTrack sets mbToBeResetOnTrack -- and the
        //   consumer of that flag (RCEM::SendResetOnTrackRequests -> the AI ResetOnTrackRequest
        //   queue -> BrnAI::ResetOnTrackManager -> ProcessResetOnTrackResultQueue ->
        //   RequestPlaceOnTrack) does not exist yet. MEASURED 2026-08-25: the direct closure is
        //   37 functions / 5,307 instructions. ⭐ 2026-08-26: the LOWEST rung of that closure is
        //   now paid -- the module lifecycle and the manager's own Construct -- so what is left
        //   is the request/result pump listed at the top of this banner. Full working-out in
        //   BrnRaceCar.cpp::RequestResetOnTrack. So a HEAVY
        //   crash ends logically and the car is never placed back on the road: it pins.
        //
        // ⭐⭐⭐ BOUNDARY MOVED AGAIN 2026-08-26 (resetpump wave), AND THE PARAGRAPH ABOVE IS
        //   NOW HISTORY, NOT STATUS. THE PUMP IS PLUMBED AND A REQUEST HAS TRAVERSED IT.
        //   RCEM::{WriteUpdatedAIData, SendResetOnTrackRequests, ProcessResetOnTrackResultQueue},
        //   the two AI bridges, AIModule::{Update slice, ProcessRequestInterface,
        //   UpdateResetOnTrackManager} and VehicleManager::GenerateAboveGroundLineTests are all
        //   REAL. MEASURED (runs rp_crash2 / rp_crash3, asserts=0, no AV): a forced heavy crash
        //   ends, the request leaves the race-car module, the AI module receives and resolves it,
        //   the result comes back, and ActiveRaceCar::RequestPlaceOnTrack PUTS THE CAR BACK ON
        //   THE ROAD -- after which it drives about a kilometre.
        //
        // ⛔⛔ THE FLAG STAYS ANYWAY, FOR A DIFFERENT AND SMALLER REASON, AND SAYING SO IS THE
        //   WHOLE POINT OF KEEPING IT: NOTHING CLEARS THE CRASH STATE. After the recovery
        //   mbCrashing is STILL 1, mfTimeCrashing restarts and climbs (74 s while the car was
        //   driving normally), and no LEAVE_CRASHED is ever posted. A player would be recovered
        //   and then drive for ever with the crash bar up and every reader of
        //   IsPlayerCarCrashing lying.
        //   ⭐ THE NEXT RUNG IS ONE DISPATCH, and it is not in this file:
        //   VehicleManager::ProcessResetEvents @0x82617820 ends its per-car work with
        //       if (mbResetTransform) VehiclePhysics::Reset(transform);   // 0x82617DF8
        //       else                  (*(vtbl(car) + 4))(car);            // 0x82617E00  <- LIVE
        //   Every reset this build issues carries resetTransform == 0, so the ELSE arm is the one
        //   that runs -- and that RaceCarPhysics vtable slot is PARKED (its occupant is not
        //   settled in this tree), so NOTHING IS DISPATCHED. ⭐ Probe that slot, do not reason
        //   about it (see the vtable slot-0 Create precedent).
        //   (Measured, with the control that could falsify it: run cx_flow3 pinned at
        //   (2932,-10.8,~209) for 80 s on the identical build that "recovered" in cx_flow6 --
        //   and cx_flow6's re-acceleration began BEFORE the complete event, i.e. it was physics
        //   rolling away from a light knock, not recovery.)
        //
        // ⚠️ IT IS A CAPABILITY SWITCH, NOT A BEHAVIOUR CHANGE. When the flag is set the console
        //   path below is entered unmodified; when it is clear this returns at exactly the point
        //   the console's OWN suppression gates return (the two invulnerability latches /
        //   mbStopPlayerCrashing / mbStopAICrashing, four lines below), so no half-committed
        //   crash state can exist either way. Nothing downstream is stubbed, faked or reordered.
        //
        // ⭐ OPT IN with the environment variable  BRN_ENABLE_CRASH_ENTRY=1  (the BRN_* precedent:
        //   BRN_PROP_DIAG / BRN_TRICACHE_PROBE / BRN_INPUT_ALLOW_BACKGROUND). Waves working the
        //   crash chain set it and get today's full behaviour, bit for bit. flow_run.ps1 clears
        //   it on every default run and exposes it as -CrashEntry, so a leftover shell variable
        //   cannot ride into a run that calls itself default.
        //
        //   Reference: scratchpad resetontrack_log.md (JOB 1), crash_exit_log.md §03.
        // ===========================================================================================
        {
            static const bool sbCrashEntryEnabled = (std::getenv("BRN_ENABLE_CRASH_ENTRY") != 0);
            if (!sbCrashEntryEnabled)
            {
                // Budgeted witness: the banner once, then one short line per suppressed crash up
                // to 16, so scoring an ABSENCE downstream is not scoring a silent diagnostic.
                static s32 sliSuppressed = 0;
                if (CgsDev::Log::gpDebugPrint != 0 && sliSuppressed < 16)
                {
                    if (sliSuppressed == 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[bringup] CRASH ENTRY DISABLED (BRN_ENABLE_CRASH_ENTRY is not set)."
                            << " BOUNDARY MOVED 2026-08-26 (resetpump wave), and the REASON THIS LINE"
                            << " USED TO PRINT IS RETIRED: the reset-on-track REQUEST/RESULT PUMP IS"
                            << " PLUMBED END TO END AND A REQUEST HAS TRAVERSED IT. Measured, runs"
                            << " rp_crash2 / rp_crash3: crash -> CRASH COMPLETE -> [resetpump] request"
                            << " SENT -> RECEIVED by the AI module -> [rot] request resolved -> RESULT"
                            << " applied -> ActiveRaceCar::RequestPlaceOnTrack, and the car IS put back"
                            << " on the road (ResetActiveRaceCar RE-RESET + a seated pose) and drives"
                            << " ~1 km afterwards, asserts=0, no AV.\n"
                            << "[bringup] ...AND THE FLAG STAYS FOR ONE REMAINING, MEASURED REASON:"
                            << " NOTHING CLEARS THE CRASH STATE. mbCrashing is still 1 and"
                            << " mfTimeCrashing was still climbing (74 s) with the car driving normally,"
                            << " and no LEAVE_CRASHED is ever posted -- so a player would be recovered"
                            << " but PERMANENTLY FLAGGED CRASHING (crash bar on, and every consumer of"
                            << " IsPlayerCarCrashing wrong). THE NEXT RUNG IS ONE DISPATCH:"
                            << " VehicleManager::ProcessResetEvents @0x82617820 branches"
                            << " `if (mbResetTransform) VehiclePhysics::Reset(t) else (*(vtbl(car)+4))(car)`"
                            << " (asm 0x82617DF0 / 0x82617E00). EVERY reset on this build carries"
                            << " resetTransform=0, so the ELSE arm is the live one -- and that"
                            << " RaceCarPhysics vtable slot is PARKED here, so nothing is dispatched."
                            << " Settle that slot and the crash state clears with it."
                            << " Set BRN_ENABLE_CRASH_ENTRY=1 to exercise the full chain.\n";
                    }
                    ++sliSuppressed;
                    *CgsDev::Log::gpDebugPrint
                        << "[bringup] crash entry suppressed #" << sliSuppressed
                        << " victim index "
                        << static_cast<s32>((lVictimEntityId.muValue >> 10) & 0x3FFF)
                        << " cause sub-code "
                        << static_cast<s32>((lAggressorEntityId.muValue >> 24) & 0xFF) << "\n";
                }
                return;
            }
        }

        // ---- Step 1: index + early-out suppression gates (asm v36/v38/v43/v44/v45) ----
        const s32 liVictimIndex = static_cast<s32>((lVictimEntityId.muValue >> 10) & 0x3FFF);
        VehicleDriver& lrDriver = maRaceCarDrivers[liVictimIndex];   // asm 224*v36 + this
        const s32 liCrashState = maeRaceCarTypes[liVictimIndex];      // asm v44 = maeRaceCarTypes[v36]
        const bool lbWasInCrashState1 = (liCrashState == 1);             // asm v45 -> AddRaceCarCrashEvent arg

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

        // ---- Step 3: the two crash-commit branches (the heart) ----
        RaceCarPhysics& lrVictimRecord = maRaceCarVehicles[liVictimIndex];   // asm _R31 = 5216*v36 + this
        RaceCarPhysics* const lpVictimPhysics =
            reinterpret_cast<RaceCarPhysics*>(&lrVictimRecord);                    // asm RaceCarPhysics @ _R31 + 1856

        if (lrVictimRecord.mbCrashing)
        {
            // (A) REMOTE / already-handled path: fire the LIGHT crash event and fall through to the
            // crash-data slot alloc. asm: AddRaceCarCrashEvent(sink, 0, id, a3, 0,0, v45, 0, vCrashPos).
            if (lpManagerOutputInterface)
            {
                lpManagerOutputInterface->AddRaceCarCrashEvent(
                    lValidatedVictimId,
                    /*lbLocalPhysicalCrash=*/false,
                    lrVictimRecord.mCrashNormal,          // asm v1 == the crash normal register
                    lbWasInCrashState1,
                    // this argument used to be a phantom `mvCrashPosition`. Both
                    // AddRaceCarCrashEvent sites do `lvx128 ; vspltw v0,v0,0 ; stvx128 ; lfs f1`
                    // on record+0xEF0 -- they pass LANE .x as the scalar float f1, and +0xEF0 is
                    // mvSpeedOnLastCrashMPH_... So the event carries the crash SPEED IN MPH.
                    lrVictimRecord.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.x);
            }
        }
        else
        {
            // (B) LOCAL / physical-crash path.
            // The X360 latch bool is an AND of four conditions (asm): the player slot is not remote,
            // the victim is within the proximity radius of the player camera, the victim's car type
            // is 1, and the a7-derived predicate (here always true -- a7 is the no-takedown-type
            // sentinel path; the X360 sets v108=1 when a7==-1||a7==0). FLAG: the a7 predicate is
            // modelled as always-true because the bodied callers pass the -1 sentinel.
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

            const bool lbLatchPhysics =
                (!lrPlayerRecord.mbCrashing)   // asm *(_R10+3664)==0  (player not remote)
                && lbBehindPlayer                          // asm 0.0f > dot3(delta, playerForward)
                && (liCrashState == 1)                     // asm *(v39+v35)==1  (== maeRaceCarTypes[victim])
                /* && lbA7Predicate */;                    // asm & v108 (FLAG: modelled true, see above)

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
            // member is VehiclePhysics::mbDeformationModelIsActive. AND ITS GUARD IS INVERTED
            // HERE: the console sets it (together with an inlined ResetDeformableAABB, a 32-byte
            // copy of mOriginalAABB over mDeformableAABB) only when the RE-READ mbCrashing is
            // NON-zero, i.e. on the already-crashing path -- not on this local one. FLAG: left on
            // this branch pending the SetRaceCarCrashing control-flow re-decode noted below.
            lrVictimRecord.mbDeformationModelIsActive = 1;          // asm *(record+4953) = 1
            lrVictimRecord.mEntityCausingCrash = lValidatedVictimId; // asm *(record+5200) = v34
            // FLAG (structural, recorded 2026-08-03): this body models TWO AddRaceCarCrashEvent
            // calls split by `mbCrashing`. There ARE two call sites (@0x826354BC with r7=1 and
            // @0x82635530 with r7=0), but the `mbCrashing` re-read at 0x82635424 gates only the
            // deformation flag + AABB reset and the r8 argument -- it is a COMMON TAIL after
            // RaceCarPhysics::SetCrashing, not the branch that selects the call site. Re-decoding
            // that control flow is a separate wave; the member seats above are settled regardless.

            // Fire the FULL crash event (asm arg `1` + the matrix vs the light path's 0/0).
            if (lpManagerOutputInterface)
            {
                lpManagerOutputInterface->AddRaceCarCrashEvent(
                    lValidatedVictimId,
                    /*lbLocalPhysicalCrash=*/true,
                    lrVictimRecord.mCrashNormal,
                    lbWasInCrashState1,
                    lrVictimRecord.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.x);

                // Push the 64-byte crash record onto the IO VariableEventQueue<1536,16> @ sink+26096
                // (asm: AddEvent(a5+26096, &record, 63, 32)). The record packs { entityId, victimIdx }.
                // FLAG: the exact 64-byte event-record layout is MODELLED as the two load-bearing
                // fields the asm writes (v228=entityId, v234=victimIdx); the rest is zero-init.
                CrashIoEventRecord lEventRecord;
                lEventRecord.mEntityIdValue = lValidatedVictimId.muValue;   // asm v228 = v34
                lEventRecord.mfReserved     = 0.0f;                         // asm v229 = v34 (re-image; modelled 0)
                lEventRecord.mbFlag         = 0;                            // asm v232 = 0
                lEventRecord.muVictimIndex  = static_cast<u32>(liVictimIndex); // asm v234 = v36
                lEventRecord.muReservedTail  = 0.0f;                        // asm v233 = 0.0
                // RE-POINTED 2026-08-24 (wave B3b): same r17 sink as the takedown/grind pushes.
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
            // Pool full: overwrite the HIGHEST-mfPriority occupied slot (asm: the "WARNING:
            // Overwriting a RaceCarCrashData class" path scans +43816, the priority float, for the
            // max and reuses that slot).
            liSlot = 0;
            f32 lfHighestPriority = maRaceCarCrashes[0].mfPriority;
            for (s32 liScan = 1; liScan < 32; ++liScan)
            {
                if (maRaceCarCrashes[liScan].mfPriority > lfHighestPriority)
                {
                    lfHighestPriority = maRaceCarCrashes[liScan].mfPriority;
                    liSlot = liScan;
                }
            }
        }

        // Write the slot: { mEntityId = victim packed id, meType = takedown type, mfPriority = 0 }.
        // asm: *(v150+43816)=0.0 (priority); *(12*(v138+3651)+v35)=v149 (== slot+43812, meType);
        //      *(v150+43808)=a13 (== slot+43808, mEntityId, the packed victim id).
        maRaceCarCrashes[liSlot].mfPriority = 0.0f;
        maRaceCarCrashes[liSlot].meType     = static_cast<u32>(leTakedownType);   // FLAG: see header note
        maRaceCarCrashes[liSlot].mEntityId  = lVictimEntityId.muValue;            // asm a13 = packed victim id
        mUsedRaceCarCrashesList.SetBit(static_cast<u32>(liSlot));   // asm: set the allocation bit (v179 OR into field)

        // ---- Step 5: secondary remapped-entity event ----
        // Only fired when the takedown cause sub-code is type 2 (a TRAFFIC-owned id); the asm reads
        // the traffic slot's global entity id at +148128 and fires short_::AddEvent(sink+1872,
        // &packed) with that id's entity index in the high word. FLAG: the asm keys this off
        // HIBYTE(v149) (the meType-slot word); we key it off the documented cause sub-code (the
        // aggressor id owner byte) which carries the same type-2 signal.
        // Same re-seat as Step 2: +148128 is mPhysicalTrafficManager.maTrafficEntityIDs.
        if (luCauseSubCode == 2 && lpManagerOutputInterface)
        {
            const u32 luRemappedIndex =
                (mPhysicalTrafficManager.maTrafficEntityIDs[liVictimIndex].muValue >> 10) & 0x3FFF;   // asm (v180>>10)&0x3FFF
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

        // FLAGS RETIRED 2026-08-24 (wave B3b): IsPointBetweenTwoParallelPlanes is recovered
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
    // speed >= minClosingSpeed * scale. Decides the loser by scaled speed, multiplies the closing
    // magnitude by 5.0, sets impact type 4 (E_IMPACT_SHUNT), applies a shove (ApplyShunt), and
    // commits HEAD_ON.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForHeadToHead(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        const f32 lfDegToRad = 0.017453292f;
        // Near-180 band: bail if the cars are not nearly head-on.
        if (std::fabs(lpInfo->mfAngleBetweenCars) < (180.0f - mfMaxHeadToHeadAngle) * lfDegToRad)
            return false;

        // At least one car must clear the min closing speed (* scale).
        const f32 lfMinSpeed = mfMinHeadToHeadIndividualSpeed * KF_SPEED_UNIT_SCALE; // FLAG: scale rodata flt_82F31928
        const f32 lfSpeedA = lpInfo->mfRaceCarASpeed;   // asm a2+23 (==+0x5C)
        const f32 lfSpeedB = lpInfo->mfRaceCarBSpeed;   // asm a2+24 (==+0x60)
        if (lfSpeedA <= lfMinSpeed && lfSpeedB <= lfMinSpeed)
            return false;

        // The X360 scales each car's speed by a per-car transform-derived lane (vmulfp of the speed
        // against the a2[9]/a2[10] transform columns) and compares the two scaled values; the
        // branch that wins applies the shove. Here we compare the raw speeds, which is the asm's
        // decision input once the scale lanes cancel.
        // FLAG: the per-car scale lane (a2[9]/a2[10] transform column * speed) is approximated by
        // the raw speeds; the exact VMX scaling is not reconstructed.
        //
        // The two candidate entity ids the asm later passes to InstantTakedown are the CONTACT's own
        // id fields (mpContact->mEntityIdA == **a2, mEntityIdB == (*a2)[1]); the (v18,v19) pair is
        // assigned in a branch-specific order and the final victim/aggressor split is decided below
        // by the situation byte + a numeric-id ordering test. We read those ids by name.
        const EntityId lContactIdA = lpInfo->mpContact->mEntityIdA; // asm **a2
        const EntityId lContactIdB = lpInfo->mpContact->mEntityIdB; // asm (*a2)[1]

        bool        lbFired = false;
        bool        lbPlayerWon = false; // asm a2[258] (mbPlayerWonImpact lane)
        EntityId    lId0{};   // asm v18
        EntityId    lId1{};   // asm v19
        if (lfSpeedB > lfSpeedA)
        {
            // asm: !a2+85 (B not network) guards this branch.
            if (!lpInfo->mbRaceCarBIsNetworkCar)
            {
                lbPlayerWon = lpInfo->mbRaceCarAIsPlayer; // asm v15 = *(a2+82)
                lId0 = lContactIdB;  // asm v18 = (*a2)[1]
                lId1 = lContactIdA;  // asm v19 = **a2
                lpInfo->mfClosingSpeed *= 5.0f;         // asm *(a2+22) == word 22 == byte 88 == mfClosingSpeed
                lpInfo->mbPlayerWonImpact = lbPlayerWon; // asm *(a2+258) = v15
                lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA; // asm a2[62]=a2[7]
                lpInfo->meVictimActiveRaceCarIndex    = lpInfo->meActiveRaceCarIndexB; // asm a2[63]=a2[8]
                lpInfo->meImpactType = E_IMPACT_SHUNT;  // asm a2[61] = 4
                lbFired = true;
                ApplyShunt(lpInfo);
            }
        }
        else if (lfSpeedB < lfSpeedA)
        {
            // asm: !a2+84 (A not network) guards this branch.
            if (!lpInfo->mbRaceCarAIsNetworkCar)
            {
                lbPlayerWon = lpInfo->mbRaceCarBIsPlayer; // asm v20 = *(a2+83)
                lId0 = lContactIdA;  // asm v18 = **a2
                lId1 = lContactIdB;  // asm v19 = (*a2)[1]
                lpInfo->mfClosingSpeed *= 5.0f;
                lpInfo->mbPlayerWonImpact = lbPlayerWon;
                lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexB; // asm a2[62]=a2[8]
                lpInfo->meVictimActiveRaceCarIndex    = lpInfo->meActiveRaceCarIndexA; // asm a2[63]=a2[7]
                lpInfo->meImpactType = E_IMPACT_SHUNT;
                lbFired = true;
                ApplyShunt(lpInfo);
            }
        }
        else
        {
            // Exact speed tie: the asm marks both cars' "already handled" bytes and returns 1
            // without crashing anyone.
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true; // asm *(a2+257)=1 (B path)
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true; // asm *(a2+256)=1 (A path)
            return true;
        }

        // Commit gate (asm 0x8263D3B4..0x8263D3F8): the crash only commits when the shove fired AND
        // at least one car is a network car (a2+85 || a2+84). If the gate fails the asm falls to
        // LABEL_27 and returns 0 (NOT lbFired) -- a shove that fired without a network car involved
        // still reports "not handled" to the classifier ladder.
        if (!lbFired || !(lpInfo->mbRaceCarBIsNetworkCar || lpInfo->mbRaceCarAIsNetworkCar))
            return false;   // asm: LABEL_27 result=0

        // The commit condition is keyed on the player-won byte (a2+258): when the player won, commit
        // iff v19 < v18 (asm 0x8263D3E4 blt); when the player did NOT win, commit iff v18 < v19 (asm
        // 0x8263D3F4 bge falls through). Otherwise the asm returns 0 (no commit, LABEL_27).
        bool lbCommit;
        if (lbPlayerWon)
            lbCommit = (lId1.muValue < lId0.muValue);   // asm: cmplw v19,v18 ; blt commit
        else
            lbCommit = (lId0.muValue < lId1.muValue);   // asm: cmplw v18,v19 ; bge return0
        if (!lbCommit)
            return false;   // asm: LABEL_27 result=0

        // Both commit paths converge on the SAME InstantTakedown operand order (asm 0x8263D414/D41C):
        // victim = v18 (== lId0), aggressor = v19 (== lId1), regardless of which branch fired.
        const EntityId lVictim    = lId0;   // asm r4 = r30 = v18
        const EntityId lAggressor = lId1;   // asm r5 = r29 = v19
        InstantTakedown(lVictim, lAggressor,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_HEAD_ON);
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

        // Proximity: squared distance between the two car positions must be < 0.04 (asm immediate
        // 0.040000003). The asm subtracts the +160 / +224 transform lanes -- the position (Pos)
        // axes of mRaceCarA/BTransform.
        const Vector3 lvPosA = lpInfo->mRaceCarATransform.Pos();
        const Vector3 lvPosB = lpInfo->mRaceCarBTransform.Pos();
        const f32 ldx = lvPosA.x - lvPosB.x;
        const f32 ldy = lvPosA.y - lvPosB.y;
        const f32 ldz = lvPosA.z - lvPosB.z;
        const f32 lfDistSq = ldx * ldx + ldy * ldy + ldz * ldz;
        // asm: vcmpgtfp (0.04 > distSq) must hold; otherwise bail. Equivalent to distSq < 0.04.
        if (lfDistSq >= 0.040000003f)
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
    // Bails if either car has had a recent impact. Alignment must be below 1.9 (too head-on belongs
    // to head-to-head). Picks the aggressor/victim by a closing-velocity sign test, then -- if the
    // victim is not already being slammed/shunted -- classifies the contact as nudge (closing <=
    // mfMinShuntSpeed*scale, type 2) or shunt (<= mfFatalShuntSpeed*scale, type 4),
    // promoting shunt->boost-shunt (6) when the victim is boost-eligible. Returns 1; never crashes.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForShuntAndNudge(RaceCarResponseInfo* lpInfo)
    {
        // Recency throttle on BOTH cars (asm a2[8] then a2[7]). FLAG: HasRaceCarHadRecentImpact body
        // not in this dossier -- declared-only callee.
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexB)))
            return false;
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexA)))
            return false;

        // Alignment gate: the sum of the two cars' forward-axis dots with the contact normal must
        // be below 1.9 (asm immediate). A higher value means the contact is too head-on.
        // FLAG: the asm forms two vmsum3fp dots of the contact normal (mpContact+48) against the
        // two car transform forward columns (a2+208 / a2+144). Modelled with named Vector3 dots.
        const Vector3 lvNormal = lpInfo->mpContact->mNormal;
        const Vector3 lvFwdA   = lpInfo->mRaceCarATransform.At();   // asm a2+144 region
        const Vector3 lvFwdB   = lpInfo->mRaceCarBTransform.At();   // asm a2+208 region
        const f32 lfDotA = std::fabs(lvNormal.x * lvFwdA.x + lvNormal.y * lvFwdA.y + lvNormal.z * lvFwdA.z);
        const f32 lfDotB = std::fabs(lvNormal.x * lvFwdB.x + lvNormal.y * lvFwdB.y + lvNormal.z * lvFwdB.z);
        if (1.9f < (lfDotA + lfDotB))
            return false;

        // Aggressor/victim by the closing-velocity sign (asm subtracts the +160/+224 position lanes
        // and dots against the forward column; >= 0 picks A as aggressor, else B).
        const f32 ldx = lpInfo->mRaceCarATransform.Pos().x - lpInfo->mRaceCarBTransform.Pos().x;
        const f32 ldy = lpInfo->mRaceCarATransform.Pos().y - lpInfo->mRaceCarBTransform.Pos().y;
        const f32 ldz = lpInfo->mRaceCarATransform.Pos().z - lpInfo->mRaceCarBTransform.Pos().z;
        const f32 lfApproach = ldx * lvFwdB.x + ldy * lvFwdB.y + ldz * lvFwdB.z;

        const EActiveRaceCarIndex leA = lpInfo->meActiveRaceCarIndexA; // asm v20 = _R31[7]
        const EActiveRaceCarIndex leB = lpInfo->meActiveRaceCarIndexB; // asm v21 = _R31[8]
        if (lfApproach >= 0.0f)
        {
            lpInfo->meAggressorActiveRaceCarIndex = leA;        // asm _R31[62]=v20
            lpInfo->meVictimActiveRaceCarIndex    = leB;        // asm _R31[63]=v21
            lpInfo->mbPlayerWonImpact = lpInfo->mbRaceCarAIsPlayer; // asm v22 = *(_R31+82)
        }
        else
        {
            lpInfo->meAggressorActiveRaceCarIndex = leB;        // asm _R31[62]=v21 ; [63]=v20 (swapped)
            lpInfo->meVictimActiveRaceCarIndex    = leA;
            lpInfo->mbPlayerWonImpact = lpInfo->mbRaceCarBIsPlayer; // asm v22 = *(_R31+83)
        }

        // If the victim is already being slammed/shunted by a race car, this contact is redundant.
        // FLAG: IsBeingSlamedOrShuntedByRaceCar takes the victim's RaceCarPhysics record + the
        // aggressor active-index; modelled via maRaceCarVehicles[victim] cast to RaceCarPhysics*.
        // (Declared on VehiclePhysics, not in this TU -- left as a structural no-op gate here so the
        // classification still resolves; the real call belongs to the VehiclePhysics home.)
        // -> we cannot call into VehiclePhysics from this minimal slice, so the slam/shunt-already
        //    guard is documented but not invoked. FLAG: redundancy guard omitted (cross-TU callee).

        const f32 lfClosing = lpInfo->mfClosingSpeed; // asm *(_R31+22) == word 22 == byte 88 == mfClosingSpeed
        if (lfClosing <= (mfFatalShuntSpeed * KF_SPEED_UNIT_SCALE)) // FLAG: scale rodata flt_82F31928
        {
            EImpactType leType = E_IMPACT_SHUNT;                       // asm v25 = 4
            if (lfClosing <= (mfMinShuntSpeed * KF_SPEED_UNIT_SCALE)) // FLAG: scale rodata
                leType = E_IMPACT_NUDGE;                               // asm v25 = 2
            lpInfo->meImpactType = leType;                             // asm _R31[61] = v25
            // Promote a plain shunt to a boost-shunt when the aggressor was HOLDING BOOST (+123 ==
            // VehicleDriver::mControls.mbBoost, in-record 59).
            const s32 liAggressor = static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex);
            if (maRaceCarDrivers[liAggressor].mControls.mbBoost && lpInfo->meImpactType == E_IMPACT_SHUNT)
                lpInfo->meImpactType = E_IMPACT_BOOST_SHUNT;          // asm _R31[61] = 6
            return true;
        }

        // Too fast for shunt/nudge -- mark each NON-network car handled and report consumed (no
        // crash). asm 0x8261A510/A524: the crash-flag store is gated on the car's NETWORK flag
        // (+0x54/+0x55), NOT on the crash flag itself -- a network car's flag is left untouched.
        if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true; // asm: if(!*(_R31+84)) *(_R31+256)=1
        if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true; // asm: if(!*(_R31+85)) *(_R31+257)=1
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
    // FLAG: this classifier's full body reads several RaceCarPhysics in-record fields the asm
    // reaches via 5216*idx + 5124 (a per-car slam accumulator) plus the aggressive-driving timer
    // tables (+171936 / +171944) and a rodata speed-curve table (flt_82F2A218). Those reads are
    // NOT placeable with named access in this minimal slice (the RaceCarPhysics layout +5124 field
    // and the aggressive-driving tables are unmodelled). The load-bearing CLASSIFICATION decision
    // (recency gate, alignment gate, energy band, severity) is reconstructed with named access; the
    // in-record slam-accumulator comparison and the aggressive-driving timer veto are documented
    // and FLAGged as omitted cross-record reads.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForSlamAndTradingPaint(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        // Recency throttle on both cars (asm reads a2+28 / a2+32 == the active-index fields).
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexA)))
            return false;
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexB)))
            return false;

        // Alignment gate: the contact-normal dot must clear the paint-alignment threshold.
        // FLAG: the asm dots two transform-derived lanes (v123 . v122); modelled as the contact
        // normal vs the A-car forward axis.
        const Vector3 lvNormal = lpInfo->mpContact->mNormal;
        const Vector3 lvFwdA   = lpInfo->mRaceCarATransform.At();
        const f32 lfAlignDot = lvNormal.x * lvFwdA.x + lvNormal.y * lvFwdA.y + lvNormal.z * lvFwdA.z;
        if (KF_PAINT_ALIGNMENT_GATE > lfAlignDot) // FLAG: rodata unk_82FB8310
            return false;

        // Energy band [min, max] (* scale). The asm reads *(_R31+88) == mfClosingSpeed.
        const f32 lfEnergy = lpInfo->mfClosingSpeed;
        if (lfEnergy <= (mfMinTradingPaintSpeed * KF_SPEED_UNIT_SCALE)) // FLAG: scale rodata flt_82F31928
            return false;
        if (lfEnergy > (mfFatalSlamSpeed * KF_SPEED_UNIT_SCALE))  // FLAG: scale rodata flt_82F31928
        {
            // Above the band -> mark each NON-network car handled, report consumed (no crash).
            // asm 0x8261A2xx (pseudocode 5618/5620): the crash-flag store is gated on the car's
            // NETWORK flag (+0x54/+0x55), NOT on the crash flag itself.
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true; // asm: if(!*(_R31+84)) *(_R31+256)=1
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true; // asm: if(!*(_R31+85)) *(_R31+257)=1
            return true;
        }

        // In-band: the asm picks the slammer by an in-record slam-accumulator comparison (+5124)
        // gated by the aggressive-driving timers, then sets severity 1 (TRADING_PAINT) or 3 (SLAM),
        // promoting SLAM->BOOST_SLAM (5) when the slammer is boost-eligible (+123).
        // FLAG: the slam-accumulator (+5124) + aggressive-driving timer (+171936/+171944) reads are
        // omitted cross-record/table reads; the severity here defaults to TRADING_PAINT and is
        // promoted only by the boost-eligible byte we DO model.
        const s32 liA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA);
        lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA; // asm *(_R31+248)=v27
        lpInfo->meVictimActiveRaceCarIndex    = lpInfo->meActiveRaceCarIndexB; // asm *(_R31+252)=v29
        EImpactType leSeverity = E_IMPACT_TRADING_PAINT;                       // asm *(_R31+244)=1
        if (maRaceCarDrivers[liA].mControls.mbBoost)
            leSeverity = E_IMPACT_BOOST_SLAM;                                  // asm *(_R31+244)=5 (3->5 promote)
        lpInfo->meImpactType = leSeverity;
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForVerticalTakedown  @0x8263D728  ->  VERTICAL
    //
    // Early-outs if BOTH cars had a recent impact. Then for each candidate victim it calls the
    // sibling CheckForVerticalTakedownSituation (the up-axis geometry test) and an up-axis height
    // comparison (the asm compares a transform +4192 lane against rodata unk_82FB82A0, then equality
    // against 0). Commits VERTICAL.
    //
    // FLAG: CheckForVerticalTakedownSituation is a declared-only callee (not in this dossier). The
    // up-axis height lanes the asm reads at in-record +4192 are part of the unmodelled RaceCarPhysics
    // layout; the height comparison is delegated to the situation helper rather than reconstructed
    // inline, and the rodata unk_82FB82A0 vector is unrecovered.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForVerticalTakedown(RaceCarResponseInfo* lpInfo)
    {
        // Early-out only when BOTH cars are recency-blocked (asm: && of the two recency checks).
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexB))
            && HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexA)))
            return false;

        RaceCarPhysics* const lpVehB = &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexB)];
        RaceCarPhysics* const lpVehA = &maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexA)];

        bool lbFired = false;
        EntityId lVictimId{};
        EntityId lAggressorId{};

        // FLAG RETIRED 2026-08-24 (wave B3b): CheckForVerticalTakedownSituation is recovered
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
    // FLAG: the asm reads several RaceCarPhysics in-record attacker fields (5216*idx + 6944 / +6288
    // / +6032, and the v13+4432 / +4176 lanes) to confirm the player is the current attacker of the
    // car being shunted. Those are unmodelled RaceCarPhysics fields; the player-is-slammer
    // confirmation is delegated to ShouldRaceCarCrashOnCarImpact (declared-only) -- the standalone
    // attacker-field reads are documented and omitted here.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForPlayerSlammingAIIntoAI(RaceCarResponseInfo* lpInfo)
    {
        const s32 liA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA); // asm v8 = a2[7]
        const s32 liB = static_cast<s32>(lpInfo->meActiveRaceCarIndexB); // asm v9 = a2[8]

        // Both cars must be AI (maeRaceCarTypes == E_RACE_CAR_TYPE_AI) and neither crashing.
        if (maeRaceCarTypes[liA] != 1)
            return false;
        if (maeRaceCarTypes[liB] != 1 || lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        RaceCarPhysics* const lpVehA = &maRaceCarVehicles[liA];
        RaceCarPhysics* const lpVehB = &maRaceCarVehicles[liB];

        // The impact speed BOTH arms pass is the record's own closing speed, splatted
        // (asm 0x8263E078..0x8263E0AC: lfs f0,0x58(r31) ; stfs -16(r1) ; lvx ; vspltw v126,v0,0 --
        // and v126 is passed as v1 unchanged at both call sites).
        const VecFloat lvfImpactSpeed = SplatVecFloat_VM(lpInfo->mfClosingSpeed);

        // ⚠ FLAG (unchanged in substance from the previous round, now stated precisely): the
        // per-victim "is the player this car's recorded attacker" predicate reads three
        // RaceCarPhysics fields this tree does not declare -- +0x13E0 and +0x1150 (both compared
        // against mePlayerActiveRaceCarIndex) and lane .z of the vector at +0x1050 (gated
        // 0.25f > it). It also forms this function's real entry gate (the console only reaches the
        // two calls when the predicate holds for at least ONE of the cars, asm 0x8263E1A0..E1B0).
        // Both are omitted here exactly as before.
        //
        // ⭐ WHY 0.75 AND NOT AN INVENTED CONSTANT: the predicate selects between two arms that are
        // now BOTH read from the image -- true -> 2.0f, false -> 0.75f. With the predicate
        // unmodelled we ship the FALSE arm, which is the console's behaviour for every contact in
        // which the player is not the recorded attacker (the common case, and the only case this
        // build can produce today since it has one race car). It is a documented degradation of a
        // known predicate, not a placeholder: when the three fields are homed, replace the constant
        // with the select and delete this note. Shipping the TRUE arm instead would double the
        // crash threshold for every car-vs-car contact, which is the wrong majority.
        bool lbAny = false;

        // Victim candidate A (asm 0x8263E1B4..0x8263E214:
        //   r4 = a2[7] = indexA, r5 = vehA (victim), r6 = vehB, v1 = splat(mfClosingSpeed),
        //   v2 = vsel(unk_82FB8320, 2.0f, mask[flagA])).
        if (ShouldRaceCarCrashOnCarImpact(static_cast<EActiveRaceCarIndex>(liA), lpVehA, lpVehB,
                                          lvfImpactSpeed,
                                          SplatVecFloat_VM(KF_SLAM_DEFAULT_CRASH_THRESHOLD_SCALE)))
        {
            lpInfo->mbCrashRaceCarA = true; // asm *(a2+256)=1
            // aggressor = the player slot; victim = A. The asm re-encodes the player index and a2[7].
            const EntityId lAggressor = MakeRaceCarEntityId(static_cast<u32>(mePlayerActiveRaceCarIndex));
            const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(lpInfo->meActiveRaceCarIndexA));
            InstantTakedown(lVictim, lAggressor,
                            lpInfo->mpContact->mNormal,
                            lpInfo->mpContact->mPointOnA,
                            lpInfo->mfNormalStressSq,
                            lpInfo->mpRequestOutputInterface,
                            lpInfo->mpManagerOutputInterface,
                            lpInfo->mpVehicleOutputInterface,
                            lpInfo->mpDeformationInterface,
                            BrnGameState::E_TAKEDOWN_STANDARD);
            lbAny = true;
        }

        // Victim candidate B (asm 0x8263E2CC..0x8263E2E4: the mirror -- r4 = a2[8] = indexB,
        // r5 = vehB (victim), r6 = vehA, the SAME v1, and v2 = the flagB select).
        if (ShouldRaceCarCrashOnCarImpact(static_cast<EActiveRaceCarIndex>(liB), lpVehB, lpVehA,
                                          lvfImpactSpeed,
                                          SplatVecFloat_VM(KF_SLAM_DEFAULT_CRASH_THRESHOLD_SCALE)))
        {
            lpInfo->mbCrashRaceCarB = true; // asm *(a2+257)=1
            const EntityId lAggressor = MakeRaceCarEntityId(static_cast<u32>(mePlayerActiveRaceCarIndex));
            const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(lpInfo->meActiveRaceCarIndexB));
            InstantTakedown(lVictim, lAggressor,
                            lpInfo->mpContact->mNormal,
                            lpInfo->mpContact->mPointOnA,
                            lpInfo->mfNormalStressSq,
                            lpInfo->mpRequestOutputInterface,
                            lpInfo->mpManagerOutputInterface,
                            lpInfo->mpVehicleOutputInterface,
                            lpInfo->mpDeformationInterface,
                            BrnGameState::E_TAKEDOWN_STANDARD);
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
    // FLAG: the asm reads RaceCarPhysics in-record velocity lanes (5216*idx + 5680, the +4432
    // current-attacker field, and the +4176 height lane) plus the maeRaceCarTypes array via
    // 4*(idx+11048)+this. The car-type reads ARE modelled (named array); the in-record velocity /
    // attacker / height reads are unmodelled RaceCarPhysics fields, so the "which car is crashing"
    // determination is taken from the response-info crash flags and the player-revenge sub-gate is
    // delegated to ShouldRaceCarCrashOnCarImpact. Those omitted in-record reads are documented.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForHittingAlreadyCrashingCar(RaceCarResponseInfo* lpInfo)
    {
        const s32 liA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA); // asm v4 = v0[7]
        const s32 liB = static_cast<s32>(lpInfo->meActiveRaceCarIndexB); // asm v6 = v0[8]

        // The asm OR-folds an in-record velocity-magnitude test with the response-info crash flag to
        // decide each car is "crashing-and-fast-enough". FLAG: the velocity lane (+5680) is
        // unmodelled; we use the response-info crash flags directly.
        const bool lbACrashing = lpInfo->mbRaceCarAIsCrashing; // asm (v19 & v5)
        const bool lbBCrashing = lpInfo->mbRaceCarBIsCrashing; // asm (v7 & v24)
        if (!lbACrashing && !lbBCrashing)
            return false;

        RaceCarPhysics* const lpVehA = &maRaceCarVehicles[liA];
        RaceCarPhysics* const lpVehB = &maRaceCarVehicles[liB];

        if (lbACrashing)
        {
            // A is crashing (asm v26-else branch, 0x8263DBExx). The live car B is the Should subject,
            // and -- per the asm -- ALSO the victim and the car whose crash flag is set.
            // asm 0x8263DC30..0x8263DC6C: r4 = a2[8] = indexB, r5 = vehB (victim), r6 = vehA,
            // v1 = the flat-normal closing speed against the VICTIM's Up, v2 = splat(1.0f).
            if (!ShouldRaceCarCrashOnCarImpact(
                    static_cast<EActiveRaceCarIndex>(liB), lpVehB, lpVehA,
                    SplatVecFloat_VM(CarCarImpactSpeed(lpInfo->mpContact->mNormal,
                                                       lpVehB->GetTransform().yAxis,
                                                       lpInfo->mClosingVelocityAtoB)),
                    SplatVecFloat_VM(KF_PILEON_CRASH_THRESHOLD_SCALE)))
                return false;
            // Guard is the LIVE car's NETWORK flag (asm 0x8263DE5C: if(!*(_R31+85))), not the crash flag.
            if (!lpInfo->mbRaceCarBIsNetworkCar)
            {
                // Both cars' types must read E_RACE_CAR_TYPE_AI for the pile-on takedown to register.
                if (maeRaceCarTypes[liA] == 1 && maeRaceCarTypes[liB] == 1)
                {
                    // Player-revenge sub-gate: only fire when the player is the current attacker.
                    // FLAG: the asm reads the in-record current-attacker field (+4432) and the
                    // +4176 height lane vs mePlayerActiveRaceCarIndex; those in-record reads are
                    // unmodelled, so the revenge confirmation is delegated to the car-type gate.
                    const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
                    const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(liB)); // asm victim=(v6<<10) = indexB
                    const EntityId lAggressor = MakeRaceCarEntityId(luPlayer);              // asm aggr=(v35<<10) = player
                    InstantTakedown(lVictim, lAggressor,
                                    lpInfo->mpContact->mNormal,
                                    lpInfo->mpContact->mPointOnA,
                                    lpInfo->mfNormalStressSq,
                                    lpInfo->mpRequestOutputInterface,
                                    lpInfo->mpManagerOutputInterface,
                                    lpInfo->mpVehicleOutputInterface,
                                    lpInfo->mpDeformationInterface,
                                    BrnGameState::E_TAKEDOWN_STANDARD);
                }
                lpInfo->mbCrashRaceCarB = true; // asm *(_R31+257)=1
            }
            return true;
        }

        // B is crashing (asm v26 branch, 0x8263DB2C). The live car A is the Should subject, the victim,
        // and the car whose crash flag is set.
        // asm 0x8263DE40..0x8263DE7C: r4 = a2[7] = indexA, r5 = vehA (victim), r6 = vehB, and the
        // same two vector builds with vehA's Up (the arms differ only in which car is the victim).
        if (!ShouldRaceCarCrashOnCarImpact(
                static_cast<EActiveRaceCarIndex>(liA), lpVehA, lpVehB,
                SplatVecFloat_VM(CarCarImpactSpeed(lpInfo->mpContact->mNormal,
                                                   lpVehA->GetTransform().yAxis,
                                                   lpInfo->mClosingVelocityAtoB)),
                SplatVecFloat_VM(KF_PILEON_CRASH_THRESHOLD_SCALE)))
            return false;
        // Guard is the live car's NETWORK flag (asm 0x8263DBFC: if(!*(_R31+84))), not the crash flag.
        if (!lpInfo->mbRaceCarAIsNetworkCar)
        {
            if (maeRaceCarTypes[liA] == 1 && maeRaceCarTypes[liB] == 1)
            {
                // FLAG: player-revenge in-record reads (+4432 / +4176) omitted as above.
                const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
                const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(liA)); // asm victim=(v4<<10) = indexA
                const EntityId lAggressor = MakeRaceCarEntityId(luPlayer);              // asm aggr=(v52<<10) = player
                InstantTakedown(lVictim, lAggressor,
                                lpInfo->mpContact->mNormal,
                                lpInfo->mpContact->mPointOnA,
                                lpInfo->mfNormalStressSq,
                                lpInfo->mpRequestOutputInterface,
                                lpInfo->mpManagerOutputInterface,
                                lpInfo->mpVehicleOutputInterface,
                                lpInfo->mpDeformationInterface,
                                BrnGameState::E_TAKEDOWN_STANDARD);
            }
            lpInfo->mbCrashRaceCarA = true; // asm *(_R31+256)=1
        }
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
        // CAUGHT IT: this TU is not in the build, so the only compiler that would ever have seen the
        // line is a per-TU gate nobody ran on it. It surfaced the instant task #113 trial-mounted
        // this file to measure its link closure. The lesson is general: a fold that re-measures
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
        static_assert(offsetof(VehicleManager, mabRaceCarDebugComponentRegistered) == 171456 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "163264 + 8*1024 == 171456, and +8 lands exactly on the asm-proven gate byte at 171464");
        // ---- the tuning bank, pinned member by member (2026-08-03). Every offset below is an
        //      asm seat from VehicleManager::Construct @0x8263B7C8, cross-checked against the PS3
        //      DecFIGS build at Δ=672. The padding runs between them are what these asserts test.
        static_assert(offsetof(VehicleManager, mbSlamsAndShuntsOn)       == 171464 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbSlamsAndShuntsOn (asm +171464)");
        static_assert(offsetof(VehicleManager, mbAllowSlamsAndShuntsEffectsForRivals) == 171465 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbAllowSlamsAndShuntsEffectsForRivals (asm +171465)");
        // The head and tail of the 44-float run, plus the closure that proves it has no gaps.
        static_assert(offsetof(VehicleManager, mfFrontRaySensorLength)   == 171468 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfFrontRaySensorLength (asm +171468)");
        static_assert(offsetof(VehicleManager, mfMaxSlamClosingXSpeed)   == 171536 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfMaxSlamClosingXSpeed (asm +171536)");
        static_assert(offsetof(VehicleManager, mfMinSecondsBetweenImpacts) == 171540 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfMinSecondsBetweenImpacts (asm +171540) -- was mis-typed s32 miAttackerToRecord");
        static_assert(offsetof(VehicleManager, mfTailgatingVunerabilityTime) == 171552 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfTailgatingVunerabilityTime (asm +171552; value from the PS3 build)");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownMaxAngle)  == 171564 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfTBoneTakedownMaxAngle (asm +171564)");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownSpeed)     == 171568 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfTBoneTakedownSpeed (asm +171568)");
        static_assert(offsetof(VehicleManager, mfMinShuntSpeed)          == 171580 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfMinShuntSpeed (asm +171580)");
        static_assert(offsetof(VehicleManager, mfFatalShuntSpeed)        == 171584 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfFatalShuntSpeed (asm +171584)");
        static_assert(offsetof(VehicleManager, mfMinTradingPaintSpeed)   == 171616 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfMinTradingPaintSpeed (asm +171616)");
        static_assert(offsetof(VehicleManager, mfFatalSlamSpeed)         == 171620 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfFatalSlamSpeed (asm +171620)");
        static_assert(offsetof(VehicleManager, mfMaxHeadToHeadAngle)     == 171628 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfMaxHeadToHeadAngle (asm +171628)");
        static_assert(offsetof(VehicleManager, mfMinHeadToHeadIndividualSpeed) == 171636 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfMinHeadToHeadIndividualSpeed (asm +171636)");
        static_assert(offsetof(VehicleManager, mfAngleForVerticleTakedown) == 171640 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfAngleForVerticleTakedown (asm +171640) -- last of the 44-float run");
        static_assert(offsetof(VehicleManager, maeImpactType) == 171644 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "171468 + 44*4 == 171644: the 44-float tuning run closes exactly onto maeImpactType with no gaps");
        static_assert(offsetof(VehicleManager, mauImpactScore)  == 171676 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mauImpactScore (asm base 171676)");
        static_assert(offsetof(VehicleManager, mafNoImpactTimeSeconds)    == 171684 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mafNoImpactTimeSeconds (asm base 171684) -- was mis-typed s32[8]");
        static_assert(offsetof(VehicleManager, maiPhysicsSlamIndex) == 171716 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "maiPhysicsSlamIndex (DWARF :926)");
        static_assert(offsetof(VehicleManager, mPlayerWonImpact) == 171736 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mPlayerWonImpact (asm +171736; DWARF :934)");
        // The two seats the committed header modelled as scalar grind thresholds are element 7 of
        // these two per-car arrays -- 171840 + 7*4 == 171868 and 171872 + 7*4 == 171900.
        static_assert(offsetof(VehicleManager, mafVulnerableTimeSeconds) == 171744 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mafVulnerableTimeSeconds (DWARF :937)");
        static_assert(offsetof(VehicleManager, mafPlayerGrindingOtherDurationSeconds) == 171840 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "mafPlayerGrindingOtherDurationSeconds base; [7] == the old mfGrindingThresholdA seat 171868");
        static_assert(offsetof(VehicleManager, mafOtherGrindingPlayerDurationSeconds) == 171872 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "mafOtherGrindingPlayerDurationSeconds base; [7] == the old mfGrindingThresholdB seat 171900");
        static_assert(offsetof(VehicleManager, mabRubbingThisUpdate) == 171952 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mabRubbingThisUpdate (DWARF :951)");
        static_assert(offsetof(VehicleManager, mPlayerAiDriver)          == 171968 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mPlayerAiDriver (asm VehicleDriver::Construct(this + 171968))");
        static_assert(offsetof(VehicleManager, mbPlayerAiDriverValid)    == 172192 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbPlayerAiDriverValid (DWARF :954)");
        static_assert(offsetof(VehicleManager, mfSteeringUpdateRemainder) == 172200 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfSteeringUpdateRemainder (DWARF :956)");
        static_assert(offsetof(VehicleManager, mePlayerActiveRaceCarIndex) == 172204 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mePlayerActiveRaceCarIndex (DWARF/asm +172204)");
        static_assert(offsetof(VehicleManager, mfCrashingAICollisionCrashThresholdMPH) == 172208 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfCrashingAICollisionCrashThresholdMPH (asm +172208)");
        static_assert(offsetof(VehicleManager, mfVerticalTakedownAngleDeg) == 172228 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfVerticalTakedownAngleDeg (asm +172228; last of the six-float run)");
        static_assert(offsetof(VehicleManager, mCameraMatrix)            == 172240 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mCameraMatrix (asm addi r11,r11,-0x5F30; 4 x stvx128)");
        static_assert(offsetof(VehicleManager, mCameraMatrix) % 16 == 0, "Matrix44Affine must land 16-aligned with no compiler-inserted padding");
        static_assert(offsetof(VehicleManager, mbImpactTime)            == 172304 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbImpactTime (asm +172304)");
        static_assert(offsetof(VehicleManager, mbStopPlayerCrashing)    == 172306 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbStopPlayerCrashing (asm +172306)");
        static_assert(offsetof(VehicleManager, mbStopAICrashing) == 172307 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbStopAICrashing (asm +172307)");
        static_assert(offsetof(VehicleManager, DEBUG_mbHornTakedownEnabled)    == 172311 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DEBUG_mbHornTakedownEnabled (asm +172311)");
        static_assert(offsetof(VehicleManager, mbTrafficCheckingAllowed) == 172313 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbTrafficCheckingAllowed (asm +172313; the one bool Construct seeds TRUE)");
        static_assert(offsetof(VehicleManager, mbIsOnlineGameMode) == 172315 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbIsOnlineGameMode (asm +172315)");
        static_assert(offsetof(VehicleManager, mbPlayerCarInJunkYard) == 172319 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbPlayerCarInJunkYard (asm +172319)");
        static_assert(offsetof(VehicleManager, mfPlayerStatStrength)  == 172320 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfPlayerStatStrength (asm +172320; stfsx => f32)");
        static_assert(offsetof(VehicleManager, miCarSpeed)            == 172328 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "miCarSpeed (asm +172328; stwx => s32)");
        static_assert(offsetof(VehicleManager, meCarType)             == 172344 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "meCarType (asm +172344; stwx, seeded 3)");
        static_assert(offsetof(VehicleManager, miPlayerBoost)         == 172360 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "miPlayerBoost (DWARF :1007)");
        static_assert(offsetof(VehicleManager, meCurrentGameModeType) == 172380 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "meCurrentGameModeType (asm +172380; seeded -1)");
        static_assert(offsetof(VehicleManager, mfCarStatStrengthSlamMax) == 172384 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfCarStatStrengthSlamMax (asm +172384)");
        static_assert(offsetof(VehicleManager, mfCarrStatStrengthBeingShuntedMin) == 172412 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mfCarrStatStrengthBeingShuntedMin (asm +172412; last of the eight)");
        static_assert(offsetof(VehicleManager, muCachedCarASlot)      == 172416 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "muCachedCarASlot (asm +172416)");
        static_assert(offsetof(VehicleManager, mbCachedCarCarPredictionResult) == 172424 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbCachedCarCarPredictionResult (asm +172424)");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) == 172432 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mCachedCarCarPredictionNormal (asm stvx128 v0,r31,r9 with r9 == 172432)");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) % 16 == 0, "the prediction normal is loaded/stored with lvx128/stvx128 -- it must be 16-aligned");
        static_assert(offsetof(VehicleManager, meStationaryPlayerWheelAngle) == 172448 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "meStationaryPlayerWheelAngle (asm +172448; seeded 2)");
        static_assert(offsetof(VehicleManager, mbCrashRaceCarWhenFatal) == 172452 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "mbCrashRaceCarWhenFatal (asm +172452; seeded true)");
        static_assert(offsetof(VehicleManager, meShowtimeBehaviour)   == 172456 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "meShowtimeBehaviour (asm +172456; seeded 2)");
        static_assert(offsetof(VehicleManager, miRaceCarWorldContactValidationPM) == 172460 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "miRaceCarWorldContactValidationPM (asm +172460; named by the console's own assert at BrnVehicleManager.cpp:778)");
        // THESE FOUR WERE STALE, AND NOT BECAUSE OF THIS WAVE (corrected 2026-08-11). They
        // carried KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT for seats that sit PAST the 2026-08-06
        // contact-generation carve, so each was 76 bytes short -- and nothing ever said so, because
        // this whole TU is UNMOUNTED (that is exactly the hole BrnVehicleManager_layout_check.cpp
        // was created to close). Conformed to the mounted gate's expressions, verbatim.
        static_assert(offsetof(VehicleManager, miNumTrafficSphereWorldTests)
                          == 172580 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT + 68,
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
