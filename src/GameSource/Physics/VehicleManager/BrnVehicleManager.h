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
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<1536,16> (the IO event queue the crash/takedown events push onto)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"        // CgsContainers::BitArray<N> (live-car bitset, crash-data free-list, taken-down bitset)

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

    // MINIMAL-SLICE definition of the manager-side output interface the crash/takedown events fan
    // out through. The real home is BrnVehicleConstants.h (DWARF); only the surface SetRaceCarCrashing
    // + HandleRaceCarRaceCarContact poke is modelled here, declare-only. FLAG: the EXACT event-queue
    // plumbing is MODELLED -- the X360 reaches a CgsModule::VariableEventQueue<1536,16> at sink+26096,
    // a secondary "remapped id" sub-queue at sink+1872, and two driver-feedback bytes at sink+27648/9
    // by raw offset. Here those are exposed by NAME (accessors / declare-only methods) rather than by
    // reproducing the full ~27KB byte layout; the method bodies belong to the interface's own TU.
    struct VehicleManagerOutputInterface
    {
        // sink+26096: the IO event queue the crash record + takedown/grind events push onto.
        // Returned by reference so the bodies can call .AddEvent / .AddEventSafe by name.
        CgsModule::VariableEventQueue<1536, 16>& GetEventQueue();

        // The cross-module "a race car crashed" event (X360 AddRaceCarCrashEvent). FLAG: the X360
        // call passes nine positional args (a leading 0, the victim entity id, a byte-offset/flag, a
        // local-vs-remote flag, an optional crash matrix, the was-crash-state-1 bool, a 0, and the
        // splatted crash position). The load-bearing args are kept; the rest are MODELLED as a single
        // packed call. Declare-only -- bodied by the interface's own TU.
        void AddRaceCarCrashEvent(EntityId lVictimEntityId,
                                  bool lbLocalPhysicalCrash,
                                  const Matrix44Affine& lCrashMatrix,
                                  bool lbWasInCrashState1,
                                  Vector3 lvCrashPosition);

        // sink+1872: the secondary "remapped entity id" sub-event the type-2-id path fires. Declare-only.
        void AddRemappedEntityIdEvent(u32 luRemappedActiveRaceCarIndex);

        // sink+27648 / sink+27649: the two driver-feedback bytes HandleRaceCarRaceCarContact OR-sets
        // when a takedown is scored (the asm `*(a32+27648) |= ...; *(a32+27649) = ...`). Declare-only.
        void FlagTakedownScoredForDriver(bool lbVictimIsHighSlot);
    };

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

        // --- the car-vs-car contact entry point this slice bodies (DWARF h:1149; X360 @0x82642F78) ---
        // STAGE 1 of the takedown chain. Called by ProcessContactSpies once per resolved race-car-vs-
        // race-car contact. Decodes the two EntityIds, gates on mbTakedownsEnabled + the live-car
        // bitset, populates a stack-local RaceCarResponseInfo (indices, crash/player flags, speeds,
        // mfAngleBetweenCars), runs the grinding pre-pass + CheckForAllTypesOfImpacts, commits any
        // flagged crash via SetRaceCarCrashing, then drives GenerateContactSituation -> ApplySlam/
        // ApplyShunt + the last-attacker/revenge bookkeeping. Signature is DWARF-authoritative
        // (the interface order is Request, Vehicle, Manager, Deformation -- note it differs from the
        // RaceCarResponseInfo member order).
        void HandleRaceCarRaceCarContact(BrnPhysics::ContactSpy::RaceCarContact lContact,
                                         BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                         BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                                         VehicleManagerOutputInterface* lpManagerOutputInterface,
                                         BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                         f32 lfTimestep);

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

        // The grind/rubbing pre-pass detector (X360 CheckForGrindingAndRubbing @0x825B5450). Returns
        // true when the player is grinding/rubbing the other car this frame. Declare-only -- bodied by
        // its own TU. DWARF/asm shape is (VehicleManager* this, RaceCarResponseInfo* lpInfo).
        bool CheckForGrindingAndRubbing(RaceCarResponseInfo* lpInfo);

        // Resolves the EImpactSituation that selects ApplySlam vs ApplyShunt (X360
        // GenerateContactSituation). Writes lpInfo->meImpactSitutation. Declare-only -- bodied by its
        // own TU. DWARF/asm shape is (VehicleManager* this, RaceCarResponseInfo* lpInfo).
        void GenerateContactSituation(RaceCarResponseInfo* lpInfo);

        // ==========================================================================================
        // Player-stats / showtime / network / lookup surface (X360 wave-10 fan-out). These nine
        // functions are independent of the takedown classifier chain above; they read/write the deep
        // §7 members (player active index, the player-stats + showtime blocks, the network-hidden
        // bitset/countdown, the traffic global->physical map). Offsets/constants are asm-proven.
        // ==========================================================================================

        // @0x8259BF00: copy the per-frame player-car stats action into the manager's stats block and
        // the player car's record. lpSendCarStatsAction points at >=6 floats: [0..3] -> maPlayerCarStats
        // [0..3]; [4] -> mfShowtimePlayerCarDamageLimit; [5] -> maPlayerCarStats[4] AND the player
        // record's mfPlayerBoostStrengthStat; (s32)[1] * 0.1f -> mfShowtimePlayerCarStrength.
        void ApplyPlayerStats(const f32* lpSendCarStatsAction);

        // @0x825B4DE0: resolve a GLOBAL entity id to a PHYSICS traffic entity id via the
        // global->physical index map. Returns true and writes *lpOutPhysicsEntityId (packed
        // (physicalIndex << 10) | E_ENTITYTYPE_TRAFFIC_VEHICLE bits) when the map slot is not the 0x7F
        // "no vehicle" sentinel; returns false otherwise.
        bool GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(u32 luGlobalEntityId,
                                                              EntityId* lpOutPhysicsEntityId);

        // @0x825B4F50: resolve a packed physics-vehicle id to its physics body. Owner==RACECAR (1)
        // returns &maRaceCarVehicles[index] (as the VehiclePhysics base); owner==TRAFFIC_VEHICLE (2)
        // delegates to the contained PhysicalTrafficManager. Returns an untyped body pointer (the two
        // branch types -- RaceCarPhysics : VehiclePhysics and SimpleVehiclePhysics : ExternalPhysicsBody
        // -- share no base, matching the X360's raw-pointer return).
        void* GetVehiclePhysi(EntityId lPhysicsVehicleId);

        // @0x825C3040: mark a NETWORK race car hidden for at least luFrames frames (sets its bit in
        // mHiddenNetworkRaceCars and stores luFrames into maHiddenForFrames[index]).
        void SetNetworkRaceCarHidden(EActiveRaceCarIndex leActiveRaceCarIndex, s32 liFrames);

        // @0x8259C028: store the local player's active-race-car slot (gated 0..7).
        void SetPlayerActiveRaceCarIndex(EActiveRaceCarIndex lePlayerActiveRaceCarIndex);

        // @0x8259C098: store the current showtime behaviour mode (gated 0..2).
        void SetShowtimeBehaviour(u32 luShowtimeBehaviour);

        // @0x8259C108: drive the player car into (or out of) showtime: forwards the cached showtime
        // strength/damage-limit to RaceCarPhysics::SetPlayerVehicleInShowtime on the player car and
        // latches the global player-in-showtime byte.
        void SetPlayerCarToShowtimeMode(bool lbInShowtime);

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
            unsigned char mbTakenDown;           // +124 (asm stores literal 1; also read as a crash-suppression flag)
            // +125: a second per-car suppression flag SetRaceCarCrashing reads (asm *(v38+125)). It
            // gates the crash by the cause sub-code (0/3/5). FLAG: role/name proposed; +125 asm-proven.
            unsigned char mbSuppressByCause;     // +125
            unsigned char mPad007E[224 - 126];
        };

        // Per-car VEHICLE/physics record array @ class offset 1856. Stride 5216 -- this is the
        // BrnPhysics::Vehicle::RaceCarPhysics[8] array the DWARF attests at this slot
        // (sizeof(RaceCarPhysics) ~= 5216). Modelled as a mostly-opaque 5216-byte blob because the
        // full RaceCarPhysics layout is not reconstructed here; only the handful of in-record fields
        // the takedown chain reaches are NAMED (offsets asm-proven; the surrounding bytes are
        // opaque padding). In-record offset == (class offset of the asm load) - 1856.
        // FLAG: the stand-in record type + every field NAME below are proposed-by-role; only the 5216
        // stride and each in-record field OFFSET are asm-proven (the same array IsRaceCarCrashing
        // reads). When the real RaceCarPhysics layout pass lands, these fields fold into it.
        struct RaceCarVehicleRecord
        {
            // +1808 (asm: record+3664): per-car "is crashing / network-remote / already-handled"
            // gate byte. Read by SetRaceCarCrashing to pick its remote vs physical-crash branch, and
            // by HandleRaceCarRaceCarContact to skip slam/shunt on an already-crashing car.
            unsigned char mPad0000[1808];
            unsigned char mbIsCrashingOrDisabled;   // +1808
            unsigned char mPad0711[1904 - 1809];
            // +1904 (asm: record+1904): a proximity radius-squared the SetRaceCarCrashing distance
            // gate compares against. FLAG: role inferred.
            f32           mfProximityRadiusSq;       // +1904
            unsigned char mPad0774[1920 - 1908];
            // +1920 (asm: record+1920): the car's world position (the distance-to-player test reads
            // both the victim's and the player's +1920). 16-aligned.
            Vector3       mvWorldPosition;           // +1920
            unsigned char mPad0790[3097 - 1936];
            // +3097 (asm: record+4953): "crash committed" flag the local-crash path sets to 1.
            unsigned char mbCrashCommitted;          // +3097
            unsigned char mPad0C19[3328 - 3098];
            // +3328 (asm: record+5184): the crash matrix the local-crash path stores + forwards to
            // AddRaceCarCrashEvent. 16-aligned (Matrix44Affine, 64 bytes).
            Matrix44Affine mCrashMatrix;             // +3328
            unsigned char mPad0D40[3824 - 3392];
            // +3824 (asm: record+5680): the crash-position vector splatted into the crash event. 16-aligned.
            Vector3       mvCrashPosition;           // +3824
            unsigned char mPad0EF0[4308 - 3840];
            // +4308 (asm: record+6164): per-car "protected player has grace" flag the slam/shunt
            // pre-gate consults. FLAG: role inferred.
            unsigned char mbPlayerGrace;             // +4308
            unsigned char mPad10D5[5084 - 4309];
            // +5084 (asm: record+5084 == 5216*playerIdx + 6940): ApplyPlayerStats stamps the player's
            // "boost strength" stat (lpSendCarStatsAction[5]) into the player car's record here, in
            // addition to the manager-level copy at mfShowtimePlayerCarDamageLimit. FLAG: name proposed;
            // the in-record +5084 offset is asm-proven (stw r10, 0x1B1C(5216*idx + this)).
            f32           mfPlayerBoostStrengthStat; // +5084
            unsigned char mPad13DC[5120 - 5088];
            f32           mfRecoveryTimer;           // +5120 (asm stores 0.0f when the victim is the player)
            unsigned char mPad1404[5200 - 5124];
            // +5200 (asm: record+7056): the crashing entity id stamped into the record by the
            // local-crash path.
            EntityId      mStampedEntityId;          // +5200
            unsigned char mPad1454[5216 - 5204];
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

        // Per-car EntityId validation table @ +43584. Stride 4 (asm: 4*(idx+10896) == 4*idx+43584).
        // SetRaceCarCrashing asserts the packed victim/aggressor id matches the stored id here.
        // FLAG: name proposed; +43584 / stride 4 asm-proven.
        EntityId             maRaceCarEntityId[8];    // +43584 (4 * 8 = 32; ends 43616)
        unsigned char        mPadAA60[43744 - 43616];
        // Per-car aggressive-driving victim EntityId @ +43744 (doc §7; not touched by these two
        // functions but carved so the offset is documented). FLAG: name proposed; offset from doc §7.
        EntityId             maAggressiveDrivingVictimEntityId[8]; // +43744 (4 * 8 = 32; ends 43776)
        unsigned char        mPadAAC0[43808 - 43776];
        // The crash-data type pool @ +43808 (32 * 12 = 384; ends at 44192, abutting maRaceCarCrashState).
        RaceCarCrashData     maRaceCarCrashData[32];  // +43808

        // Per-car crash-state array @ +44192. Stride 4 (asm: 4*(idx+11048) == 4*idx+44192). The asm
        // compares this != 2 to decide whether the victim still needs crashing (sentinel 2 == the
        // fatal/active-crash state). FLAG: no recovered enum home for the crash-state values -- left
        // as a plain s32 here and compared against the literal 2 in the body (see KI_RACECAR_CRASH_STATE_FATAL).
        s32                  maRaceCarCrashState[8];  // +44192   (4 * 8 = 32; ends at 44224)

        // The live-car bitset and the crash-data free-list, now given their REAL CgsBitArray type so
        // the bodies use the container's named ops (IsBitSet/GetFirstNonZeroBit/SetBit) instead of raw
        // offset access. Each BitArray<N<=64> is a single 8-byte u64 field == the same image the X360
        // scans. FLAG: names proposed; only the +44224/+44232 offsets are asm-proven.
        CgsContainers::BitArray<8>  mUsedRaceCars;               // +44224 (live-car bitset)
        CgsContainers::BitArray<32> mRaceCarCrashDataAllocBits;  // +44232 (crash-data free-list)

        unsigned char        mPadACE8[44704 - (44232 + 8)];
        // The "network race car hidden" bitset @ +44704 (CgsBitArray<8>, single 8-byte word). Read /
        // SetBit by SetNetworkRaceCarHidden (asm: this+44704, sets bit leActiveRaceCarIndex). FLAG:
        // name proposed; +44704 asm-proven (r27 = this + 44704; sld/or/stdx the per-index bit).
        CgsContainers::BitArray<8> mHiddenNetworkRaceCars;        // +44704 (ends 44712)
        unsigned char        mPadAEB8[44736 - (44704 + 8)];
        // Per-car "hide for at least N frames" countdown @ +44736. Stride 4 (asm: 4*(idx+11184) ==
        // 4*idx+44736); SetNetworkRaceCarHidden stores the requested frame count here. FLAG: name
        // proposed; +44736 / stride 4 asm-proven.
        s32                  maHiddenForFrames[8];                // +44736 (4 * 8 = 32; ends 44768)
        // (the contained PhysicalTrafficManager subobject begins at +44768 -- modelled as opaque
        // padding here, consistent with the existing layout that also names maRaceCarEntityIdRemap as
        // a direct sibling at +148128 inside this region. The X360 build folds every contained-manager
        // member the VehicleManager methods touch to its absolute class offset, so the members below
        // are reached BY their absolute-offset NAMES rather than through an embedded manager object.)
        unsigned char        mPadAEE0[148128 - 44768];
        // Per-car EntityId REMAP table @ +148128. Stride 4 (asm: 4*(idx+37032) == 4*idx+148128).
        // SetRaceCarCrashing remaps a "type 2" packed id through this table before re-validating /
        // firing the secondary remapped-id event. FLAG: name proposed; +148128 / stride 4 asm-proven.
        EntityId             maRaceCarEntityIdRemap[8]; // +148128 (4 * 8 = 32; ends 148160)
        unsigned char        mPad242C0[149456 - 148160];
        // The traffic "global entity index -> physical entity index" map @ +149456 (the contained
        // PhysicalTrafficManager's mu8GlobalToPhysicalEntityIndexMap, which the build folds to this
        // absolute class offset == 44768 + 104688). 600 entries, 1 byte each (asm asserts the global
        // index < 0x258 == 600). A slot value of 127 (0x7F) means "no physical vehicle for this global
        // index". GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe reads it. FLAG: name from the assert
        // string; +149456 / size 600 asm-proven (lbzx this+149456+idx; cmplwi idx, 0x258).
        unsigned char        mau8GlobalToPhysicalEntityIndexMap[600]; // +149456 (ends 150056)
        unsigned char        mPad24A68[171464 - 150056];
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

        unsigned char        mPad29F44[171736 - (171684 + 32)];
        // Per-car "taken down this frame" bit array @ +171736 (CgsBitArray<8>, 8-byte single field).
        // HandleRaceCarRaceCarContact sets the victim's bit here when a takedown is scored.
        // FLAG: name proposed; +171736 asm-proven (asm: v141 = v39 + 171736).
        CgsContainers::BitArray<8> mTakenDownRaceCarsBitArray; // +171736 (ends 171744)

        unsigned char        mPad29F98[171868 - (171736 + 8)];
        // Grind pre-pass thresholds @ +171868 / +171900 (asm: *(v39+171868) < 1.0 selects grind type
        // 7 vs 8; *(v39+171900) < 0.8 the second gate). FLAG: names proposed; offsets asm-proven.
        f32                  mfGrindingThresholdA;    // +171868
        unsigned char        mPad29FFC[171900 - (171868 + 4)];
        f32                  mfGrindingThresholdB;    // +171900 (ends 171904)

        unsigned char        mPad2A01C[172204 - (171900 + 4)];
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

        unsigned char        mPad2A0BC[172320 - (172315 + 1)];
        // ---- player-car stats (written by ApplyPlayerStats; the first two re-read by
        //      SetPlayerCarToShowtimeMode -> RaceCarPhysics::SetPlayerVehicleInShowtime) -------------
        // +172320 (asm v3[43080]): the showtime "player car strength" == (s32)lpSendCarStatsAction[1]
        //   sign-extended * 0.1f. +172324 (asm v3[43081]): the showtime "damage limit" ==
        //   lpSendCarStatsAction[4]. FLAG: names proposed by role; +172320/+172324 asm-proven.
        f32                  mfShowtimePlayerCarStrength;      // +172320
        f32                  mfShowtimePlayerCarDamageLimit;   // +172324
        // +172328..+172344 (asm v3[43082..43086]): the raw player-car stats block ApplyPlayerStats
        //   copies straight from lpSendCarStatsAction[0],[1],[2],[3],[5] (the [4] entry goes to the
        //   damage-limit field above, [1] also feeds the *0.1 strength). FLAG: names proposed;
        //   offsets/stride asm-proven (172328,172332,172336,172340,172344).
        f32                  maPlayerCarStats[5];              // +172328 (5 * 4 = 20; ends 172348)

        unsigned char        mPad2A0DC[172456 - (172328 + 20)];
        // +172456 (asm v3[43114]): the current showtime behaviour mode (BrnGameState::EShowtimeMode;
        //   gated 0..2 against E_SHOWTIME_MODE_COUNT==3). Stored as a 4-byte word (asm stwx). FLAG:
        //   stored as a plain u32 because the EShowtimeMode enum has no committed home yet; +172456
        //   asm-proven. Replace the type with the enum when its home lands.
        u32                  muShowtimeBehaviour;              // +172456 (ends 172460)

        unsigned char        mPad2A12C[172612 - (172456 + 4)];
        // Per-frame takedown-event cap counter @ +172612 (throttled < 32). FLAG: name proposed.
        u32                  muTakedownEventsThisFrame;        // +172612 (ends 172616)

        // Pin every recovered offset. Never called -- exists only so offsetof can see the private
        // members (offsetof on a private member needs member-function context). The gate FAILS if any
        // padding run is wrong, which is the intended signal.
        static void _AssertLayout();

        // As _AssertLayout, but pins the wave-10 player-stats / showtime / network / map members added
        // for the second .cpp (BrnVehicleManagerPlayerStats.cpp). Never called.
        static void _AssertLayoutPlayerStats();
    };
}
}
