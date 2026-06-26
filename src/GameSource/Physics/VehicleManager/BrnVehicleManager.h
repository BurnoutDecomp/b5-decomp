#pragma once

// BrnPhysics::Vehicle::VehicleManager -- the per-frame vehicle physics manager. It owns the
// race-car rigid bodies and runs the impact/takedown classification when two cars contact.
//
// MINIMAL-SLICE HOME (now extended for the takedown sub-classifiers). The real VehicleManager is
// enormous (64 functions, a ~172 KB class with several parallel per-car arrays). This header
// provides what the takedown CLASSIFIER chain needs to compile: the nested RaceCarResponseInfo
// working-set struct (the per-contact data the classifiers read), the classifier method
// declarations, and the deep VehicleManager data members the classifiers + InstantTakedown +
// SetRaceCarCrashing reach. Everything not modelled is opaque padding so each named member lands
// at its asm-proven byte offset (pinned by the offsetof asserts in _AssertLayout). The
// CheckForAllTypesOfImpacts entry point reads none of the deep members (only its argument), so the
// layout growth does not disturb that body.
//
// RaceCarResponseInfo layout is verbatim from the DecFIGS DWARF (BrnVehicleManager.h:763-802);
// the speed/crashing offsets it exposes (+0x5C/+0x60 speeds, +0x50/+0x51 crash flags) are the
// ones the CheckForAllTypesOfImpacts X360 asm reads (a2+92/+96/+80/+81).

#include "types.hpp"
#include <cstddef>                                                // offsetof (layout asserts)
#include "BrnCommonTypes.h"                                       // Vector3, VecFloat, EntityId, Matrix44Affine
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h" // EImpactType, EImpactSituation
#include "GameSource/GameState/BrnTakedownType.h"                 // BrnGameState::ETakedownType
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"  // RaceCarContact (mNormal @+48, mPointOnA @+64)

// Pointer-only collaborators in RaceCarResponseInfo -- forward-declared in their real namespaces
// (homed by their own TUs; the classifier never dereferences them here).
namespace BrnPhysics { namespace PhysicsModuleIO { class VehicleOutputRequestInterface; } }
namespace BrnPhysics { namespace Deformation { class DeformationInputInterface; } }
namespace BrnGameState { namespace GameStateModuleIO { class VehicleOutputInterface; } }

namespace BrnPhysics
{
namespace Vehicle
{
    class RaceCarPhysics;                 // pointer-only collaborator
    struct VehicleManagerOutputInterface; // pointer-only collaborator (DWARF BrnVehicleConstants.h)

    class VehicleManager
    {
    public:
        // The per-contact working set the impact classifiers read/populate. Verbatim DWARF
        // layout (BrnVehicleManager.h:763). Pointer members use the forward-declared collaborators.
        struct RaceCarResponseInfo
        {
            BrnPhysics::ContactSpy::RaceCarContact*           mpContact;                  // +0x00
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* mpRequestOutputInterface; // +0x04
            BrnGameState::GameStateModuleIO::VehicleOutputInterface*    mpVehicleOutputInterface; // +0x08
            VehicleManagerOutputInterface*                    mpManagerOutputInterface;   // +0x0C
            BrnPhysics::Deformation::DeformationInputInterface* mpDeformationInterface;    // +0x10
            EntityId             mRaceCarAEntityID;            // +0x14
            EntityId             mRaceCarBEntityID;            // +0x18
            EActiveRaceCarIndex  meActiveRaceCarIndexA;        // +0x1C
            EActiveRaceCarIndex  meActiveRaceCarIndexB;        // +0x20
            RaceCarPhysics*      mpRaceCarA;                   // +0x24
            RaceCarPhysics*      mpRaceCarB;                   // +0x28
            Vector3              mClosingVelocityAtoB;         // +0x30 (16-aligned)
            VecFloat             mvfSlamMagnitude;             // +0x40
            bool                 mbRaceCarAIsCrashing;         // +0x50
            bool                 mbRaceCarBIsCrashing;         // +0x51
            bool                 mbRaceCarAIsPlayer;           // +0x52
            bool                 mbRaceCarBIsPlayer;           // +0x53
            bool                 mbRaceCarAIsNetworkCar;       // +0x54
            bool                 mbRaceCarBIsNetworkCar;       // +0x55
            bool                 mbOtherCarIsAI;               // +0x56
            f32                  mfClosingSpeed;               // +0x58
            f32                  mfRaceCarASpeed;              // +0x5C
            f32                  mfRaceCarBSpeed;              // +0x60
            f32                  mfNormalStressSq;             // +0x64
            Matrix44Affine       mRaceCarATransform;           // +0x70 (16-aligned)
            Matrix44Affine       mRaceCarBTransform;           // +0xB0
            f32                  mfAngleBetweenCars;           // +0xF0
            EImpactType          meImpactType;                 // +0xF4
            EActiveRaceCarIndex  meAggressorActiveRaceCarIndex; // +0xF8
            EActiveRaceCarIndex  meVictimActiveRaceCarIndex;   // +0xFC
            bool                 mbCrashRaceCarA;              // +0x100
            bool                 mbCrashRaceCarB;              // +0x101
            bool                 mbPlayerWonImpact;            // +0x102
            u32                  muImpactScore;                // +0x104
            EImpactSituation     meImpactSitutation;           // +0x108
        };

        // --- the takedown impact classifier this slice bodies (DWARF h:1182; X360 @0x82642E58) ---
        // Runs the per-type sub-classifiers in strict priority order and stops at the first that
        // fires; the classification's side effects happen inside the sub-classifiers, so this
        // returns void. Called by HandleRaceCarRaceCarContact.
        void CheckForAllTypesOfImpacts(RaceCarResponseInfo* lpInfo);

        // --- the priority-ordered sub-classifiers ---
        // Bodied here: PlayerSlammingAIIntoAI, HittingAlreadyCrashingCar, VerticalTakedown, TBone,
        // HeadToHead, ShuntAndNudge, SlamAndTradingPaint, StationaryTargetTakedown.
        bool CheckForPlayerSlammingAIIntoAI(RaceCarResponseInfo* lpInfo);
        bool CheckForHittingAlreadyCrashingCar(RaceCarResponseInfo* lpInfo);
        bool CheckForVerticalTakedown(RaceCarResponseInfo* lpInfo);
        bool CheckForTBoneTakedown(RaceCarResponseInfo* lpInfo);
        bool CheckForHeadToHead(RaceCarResponseInfo* lpInfo);
        bool CheckForShuntAndNudge(RaceCarResponseInfo* lpInfo);
        bool CheckForSlamAndTradingPaint(RaceCarResponseInfo* lpInfo);
        bool CheckForStationaryTargetTakedown(RaceCarResponseInfo* lpInfo);

        // --- the takedown COMMIT routine the classifiers call once a takedown is decided ---
        // (DWARF h:1257; X360 @0x82636108). Decodes the victim/aggressor EntityIds to active-car
        // indices, crashes the victim via SetRaceCarCrashing (unless it is already in the fatal
        // crash state), then stamps the per-car last-attacker / taken-down bookkeeping. lfNormalStressSq
        // is consumed by the classifier but NOT forwarded to SetRaceCarCrashing.
        void InstantTakedown(EntityId lVictimEntityId,
                             EntityId lAggressorEntityId,
                             Vector3 lCollisionNormal,
                             Vector3 lContactPoint,
                             f32 lfNormalStressSq,
                             BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                             BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                             BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                             BrnGameState::ETakedownType leTakedownType);

        // --- declare-only callees (bodied by their own TUs / not in this dossier) ---------------
        // The crash commit itself (DWARF h:1218; 9-param TU, X360 @0x82634C90).
        void SetRaceCarCrashing(EntityId lVictimEntityId,
                                EntityId lAggressorEntityId,
                                Vector3 lCollisionNormal,
                                Vector3 lContactPoint,
                                BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                VehicleManagerOutputInterface* lpManagerOutputInterface,
                                BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                                BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                BrnGameState::ETakedownType leTakedownType);

        // The shunt/slam force-physics appliers (X360 ApplyShunt @0x8261A5B0, ApplySlam @0x8261A738).
        // DWARF/asm shape is (VehicleManager* this, RaceCarResponseInfo* lpInfo).
        void ApplyShunt(RaceCarResponseInfo* lpInfo);
        void ApplySlam(RaceCarResponseInfo* lpInfo);

        // Per-victim "does this impact qualify to crash the car" predicate (consumed by #1/#2).
        // No standalone export in this dossier -- declare-only; FLAG: signature inferred from the
        // call sites (this, victim active-index, victim RaceCarPhysics*, other RaceCarPhysics*).
        bool ShouldRaceCarCrashOnCarImpact(s32 liVictimActiveRaceCarIndex,
                                           RaceCarPhysics* lpVictim,
                                           RaceCarPhysics* lpOther);

        // The vertical-takedown geometric sub-test (#3 helper). Not in this dossier -- declare-only.
        // FLAG: signature inferred (this, victim RaceCarPhysics*, aggressor RaceCarPhysics*).
        bool CheckForVerticalTakedownSituation(RaceCarPhysics* lpVictim, RaceCarPhysics* lpOther);

        // The T-bone side-plane containment test (#4 helper). Not in this dossier -- declare-only.
        // The X360 builds two parallel planes from the victim transform + half-width and tests the
        // contact point against them. FLAG: signature inferred (point + the two plane vectors); the
        // VMX plane math is delegated to this (unrecovered) helper rather than reconstructed inline.
        bool IsPointBetweenTwoParallelPlanes(Vector3 lPoint, Vector3 lPlaneA, Vector3 lPlaneB);

        // The recency throttle (X360 @0x825B4EB8). Its body is NOT in this dossier -- declare-only.
        // FLAG: body unrecovered; signature inferred (this, victim active-index).
        bool HasRaceCarHadRecentImpact(s32 liActiveRaceCarIndex);

    private:
        // ------------------------------------------------------------------------------------------
        // Deep VehicleManager data members the takedown chain touches, recovered by LAYOUT RECOVERY
        // WITH PADDING from the X360 asm offsets (offsets are asm-authoritative; member NAMES marked
        // "FLAG" are proposed by role -- only mePlayerActiveRaceCarIndex is DWARF-attested). The full
        // VehicleManager is ~172 KB across many parallel per-car arrays; only the members the
        // takedown classifiers + InstantTakedown reach are modelled here. Everything else is opaque
        // padding so each named member lands at its proven byte offset (pinned by the offsetof
        // asserts in _AssertLayout). The gate FAILS if any padding run is wrong, which is the signal.
        // ------------------------------------------------------------------------------------------

        // Per-car STATUS record array @ class offset 0. Stride 224 (asm: 224*idx + 124). Only the
        // "taken down this frame" byte at in-record +124 is named; the rest is opaque.
        // FLAG: record/field names proposed; the 224-byte stride and +124 field offset are asm-proven.
        // (A second per-record flag byte at in-record +123 is read by the shunt classifiers -- the
        // "boost-charged" / boost-slam-eligible bit; modelled below as mbBoostImpactEligible.)
        struct RaceCarStatusRecord
        {
            unsigned char mPad0000[123];
            unsigned char mbBoostImpactEligible; // +123 (asm: 224*idx+123; promotes SLAM->BOOST_SLAM, SHUNT->BOOST_SHUNT)
            unsigned char mbTakenDown;           // +124 (asm stores literal 1)
            unsigned char mPad007D[224 - 125];
        };

        // Per-car VEHICLE/physics record array @ class offset 1856. Stride 5216 -- this is the
        // BrnPhysics::Vehicle::RaceCarPhysics[8] array the DWARF attests at this slot
        // (sizeof(RaceCarPhysics) ~= 5216). Modelled as an opaque 5216-byte blob because the full
        // RaceCarPhysics layout is not reconstructed here; only the recovery/grace-timer float at
        // in-record +5120 (asm: 5216*idx + 6976, and 6976-1856 == 5120) is named.
        // FLAG: the stand-in record type + the mfRecoveryTimer field name are proposed; the 5216
        // stride and the +5120 field offset are asm-proven (the same array IsRaceCarCrashing reads).
        struct RaceCarVehicleRecord
        {
            unsigned char mPad0000[5120];
            f32           mfRecoveryTimer;     // +5120 (asm stores 0.0f when the victim is the player)
            unsigned char mPad1404[5216 - 5124];
        };

        // The takedown-type record pool. 32 entries, 12-byte stride, @ class offset +43808.
        // SetRaceCarCrashing allocates a slot {entity id, ETakedownType, priority} here for the
        // scoring/UI layer to read back by entity id (doc §3b).
        // FLAG: struct + field names proposed; the 12-byte stride and +43808 base are asm-proven.
        struct RaceCarCrashData
        {
            u32 mEntityId;   // +0
            u32 meType;      // +4 (BrnGameState::ETakedownType, stored as a 4-byte word)
            f32 mfPriority;  // +8
        };

        RaceCarStatusRecord  maRaceCarStatus[8];     // +0       (224 * 8 = 1792)
        unsigned char        mPad0700[1856 - sizeof(RaceCarStatusRecord) * 8];
        RaceCarVehicleRecord maRaceCarVehicles[8];   // +1856    (5216 * 8 = 41728; ends at 43584)

        unsigned char        mPadAA40[43808 - 43584];
        // The crash-data type pool @ +43808 (32 * 12 = 384; ends at 44192, abutting maRaceCarCrashState).
        RaceCarCrashData     maRaceCarCrashData[32];  // +43808

        // Per-car crash-state array @ +44192. Stride 4 (asm: 4*(idx+11048) == 4*idx+44192). The asm
        // compares this != 2 to decide whether the victim still needs crashing (sentinel 2 == the
        // fatal/active-crash state). FLAG: no recovered enum home for the crash-state values -- left
        // as a plain s32 here and compared against the literal 2 in the body (see KI_RACECAR_CRASH_STATE_FATAL).
        s32                  maRaceCarCrashState[8];  // +44192   (4 * 8 = 32; ends at 44224)

        // The live-car bitset and the crash-data free-list, modelled as opaque blobs at their proven
        // offsets. FLAG: names proposed; only the +44224/+44232 offsets are asm-proven (the internal
        // CgsBitArray layout is not reconstructed here -- treat as opaque storage).
        unsigned char        mUsedRaceCars[8];              // +44224 (CgsBitArray<8> opaque blob)
        unsigned char        mRaceCarCrashDataAllocBits[8]; // +44232 (CgsBitArray<32> opaque blob)

        unsigned char        mPadACE8[171464 - (44232 + 8)];
        // Master "takedowns enabled" gate @ +171464 (asm: a non-zero byte gates the whole routine).
        // FLAG: name proposed; offset asm-proven.
        bool                 mbTakedownsEnabled;      // +171464

        // The slam/shunt-physics enable gate @ +171465, one byte past the takedowns gate. Read by
        // HandleRaceCarRaceCarContact before ApplySlam/ApplyShunt. FLAG: name proposed; offset asm-proven.
        bool                 mbSlamShuntPhysicsEnabled; // +171465

        unsigned char        mPad29DAA[171540 - (171465 + 1)];
        // The attacker value stamped into maRaceCarLastAttacker[victim] @ +171540 (asm copies
        // *(this+171540) into the per-victim last-attacker slot). FLAG: name proposed; offset asm-proven.
        s32                  miAttackerToRecord;      // +171540 (ends 171544)

        unsigned char        mPad29DFC[171564 - (171540 + 4)];
        // ---- per-type tuning floats (asm-proven offsets; FLAG: names proposed) --------------------
        // T-bone test: angle band (degrees) and the side-plane half-width (the latter * flt_82F31928).
        f32                  mfTBoneAngleBandDegrees;   // +171564
        f32                  mfTBoneSidePlaneHalfWidth; // +171568 (ends 171572)

        unsigned char        mPad29E14[171580 - (171568 + 4)];
        // Shunt/nudge closing-speed bands (each * flt_82F31928 before comparison).
        f32                  mfNudgeMaxClosingSpeed;    // +171580
        f32                  mfShuntMaxClosingSpeed;    // +171584 (ends 171588)

        unsigned char        mPad29E24[171616 - (171584 + 4)];
        // Trading-paint energy band (each * flt_82F31928).
        f32                  mfTradingPaintMinSpeed;    // +171616
        f32                  mfTradingPaintMaxSpeed;    // +171620 (ends 171624)

        unsigned char        mPad29E38[171628 - (171620 + 4)];
        // Head-to-head test: angle tolerance (degrees, subtracted from 180) and min closing speed
        // (* flt_82F31928).
        f32                  mfHeadToHeadAngleToleranceDeg; // +171628
        unsigned char        mPad29E40[171636 - (171628 + 4)];
        f32                  mfHeadToHeadMinClosingSpeed;   // +171636 (ends 171640)

        unsigned char        mPad29E48[171644 - (171636 + 4)];
        // Per-car last-impact magnitude @ +171644. Stride 4. FLAG: name proposed; offset asm-proven.
        f32                  maRaceCarLastImpactMagnitude[8]; // +171644 (4 * 8 = 32; ends 171676)
        // Per-car "taken down this frame" byte @ +171676. Stride 1; abuts maRaceCarLastAttacker.
        unsigned char        maRaceCarTakenDownThisFrame[8];  // +171676 (1 * 8 = 8; ends 171684)

        // Per-car LAST-ATTACKER array @ +171684. Stride 4 (asm: 4*(victim+42921) == 4*victim+171684);
        // written from miAttackerToRecord. FLAG: name proposed; offset asm-proven.
        s32                  maRaceCarLastAttacker[8]; // +171684 (4 * 8 = 32; ends at 171716)

        unsigned char        mPad29E84[172204 - (171684 + 32)];
        // The local player's active-race-car slot @ +172204. DWARF-attested name (BrnVehicleManager.h:559).
        EActiveRaceCarIndex  mePlayerActiveRaceCarIndex; // +172204 (ends 172208)

        unsigned char        mPad2A04C[172306 - (172204 + 4)];
        // ---- crash-suppression + alternate-entry gates (asm-proven offsets; FLAG: names proposed) --
        bool                 mbSuppressPlayerCrash;            // +172306
        bool                 mbSuppressIfAlreadyCrashState1;   // +172307 (ends 172308)
        unsigned char        mPad2A0B4[172311 - (172307 + 1)];
        bool                 mbHornTakedownEnabled;            // +172311
        unsigned char        mPad2A0B8[172315 - (172311 + 1)];
        bool                 mbStationaryTakedownsEnabled;     // +172315

        unsigned char        mPad2A0BC[172612 - (172315 + 1)];
        // Per-frame takedown-event cap counter @ +172612 (throttled < 32). FLAG: name proposed.
        u32                  muTakedownEventsThisFrame;        // +172612 (ends 172616)

        // Pin every recovered offset. Never called -- exists only so offsetof can see the private
        // members (offsetof on a private member needs member-function context). The gate FAILS if any
        // padding run is wrong, which is the intended signal.
        static void _AssertLayout();
    };
}
}
