#pragma once

// BrnPhysics::Vehicle::VehicleManager -- the per-frame vehicle physics manager. It owns the
// race-car rigid bodies and runs the impact/takedown classification when two cars contact.
//
// MINIMAL-SLICE HOME. The real VehicleManager is enormous (64 functions, a ~700KB class with
// several parallel per-car arrays). This header provides only what is needed to compile the
// takedown CLASSIFIER entry point CheckForAllTypesOfImpacts (and its sibling sub-classifiers,
// declared declare-only): the nested RaceCarResponseInfo working-set struct (the per-contact
// data the classifiers read) plus the classifier method declarations. No VehicleManager data
// members are declared here -- CheckForAllTypesOfImpacts touches none (it only reads its
// RaceCarResponseInfo* argument). The full class layout (and InstantTakedown's deep members) is
// added when the rest of BrnVehicleManager.cpp is reconstructed.
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

// Pointer-only collaborators in RaceCarResponseInfo -- forward-declared in their real namespaces
// (homed by their own TUs; the classifier never dereferences them here).
namespace BrnPhysics { namespace ContactSpy { struct RaceCarContact; } }
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

        // --- the priority-ordered sub-classifiers (declare-only; their bodies are separate TUs) ---
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

        // --- declare-only: the crash commit itself (DWARF h:1218; its body is a separate 9-param
        // TU, X360 @0x82634C90). InstantTakedown forwards the victim/aggressor ids, the collision
        // normal + contact point, the four output/deformation interfaces, and the takedown type. ---
        void SetRaceCarCrashing(EntityId lVictimEntityId,
                                EntityId lAggressorEntityId,
                                Vector3 lCollisionNormal,
                                Vector3 lContactPoint,
                                BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                VehicleManagerOutputInterface* lpManagerOutputInterface,
                                BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                                BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                BrnGameState::ETakedownType leTakedownType);

    private:
        // ------------------------------------------------------------------------------------------
        // Deep VehicleManager data members InstantTakedown touches, recovered by LAYOUT RECOVERY
        // WITH PADDING from the X360 asm offsets (offsets are asm-authoritative; member NAMES marked
        // "FLAG" are proposed by role -- only mePlayerActiveRaceCarIndex is DWARF-attested). The full
        // VehicleManager is ~172 KB across many parallel per-car arrays; only the members this commit
        // routine reads/writes are modelled here. Everything else is opaque padding so each named
        // member lands at its proven byte offset (pinned by the offsetof asserts in _AssertLayout).
        // CheckForAllTypesOfImpacts (above) reads none of these -- it only touches its argument -- so
        // adding them does not disturb that body.
        // ------------------------------------------------------------------------------------------

        // Per-car STATUS record array @ class offset 0. Stride 224 (asm: 224*idx + 124). Only the
        // "taken down this frame" byte at in-record +124 is named; the rest is opaque.
        // FLAG: record/field names proposed; the 224-byte stride and +124 field offset are asm-proven.
        struct RaceCarStatusRecord
        {
            unsigned char mPad0000[124];
            unsigned char mbTakenDown;        // +124 (asm stores literal 1)
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

        RaceCarStatusRecord  maRaceCarStatus[8];     // +0       (224 * 8 = 1792)
        unsigned char        mPad0700[1856 - sizeof(RaceCarStatusRecord) * 8];
        RaceCarVehicleRecord maRaceCarVehicles[8];   // +1856    (5216 * 8 = 41728; ends at 43584)

        unsigned char        mPadAA40[44192 - 43584];
        // Per-car crash-state array @ +44192. Stride 4 (asm: 4*(idx+11048) == 4*idx+44192). The asm
        // compares this != 2 to decide whether the victim still needs crashing (sentinel 2 == the
        // fatal/active-crash state). FLAG: no recovered enum home for the crash-state values -- left
        // as a plain s32 here and compared against the literal 2 in the body (see KI_RACECAR_CRASH_STATE_FATAL).
        s32                  maRaceCarCrashState[8];  // +44192   (4 * 8 = 32; ends at 44224)

        unsigned char        mPadACE0[171464 - 44224];
        // Master "takedowns enabled" gate @ +171464 (asm: a non-zero byte gates the whole routine).
        // FLAG: name proposed; offset asm-proven.
        bool                 mbTakedownsEnabled;      // +171464

        unsigned char        mPad29DA9[171540 - (171464 + 1)];
        // The attacker value stamped into maRaceCarLastAttacker[victim] @ +171540 (asm copies
        // *(this+171540) into the per-victim last-attacker slot). FLAG: name proposed; offset asm-proven.
        s32                  miAttackerToRecord;      // +171540

        unsigned char        mPad29DF8[171684 - (171540 + 4)];
        // Per-car LAST-ATTACKER array @ +171684. Stride 4 (asm: 4*(victim+42921) == 4*victim+171684);
        // written from miAttackerToRecord. FLAG: name proposed; offset asm-proven.
        s32                  maRaceCarLastAttacker[8]; // +171684 (4 * 8 = 32; ends at 171716)

        unsigned char        mPad29E84[172204 - (171684 + 32)];
        // The local player's active-race-car slot @ +172204. DWARF-attested name (BrnVehicleManager.h:559).
        EActiveRaceCarIndex  mePlayerActiveRaceCarIndex; // +172204

        // Pin every recovered offset. Never called -- exists only so offsetof can see the private
        // members (offsetof on a private member needs member-function context). The gate FAILS if any
        // padding run is wrong, which is the intended signal.
        static void _AssertLayout();
    };
}
}
