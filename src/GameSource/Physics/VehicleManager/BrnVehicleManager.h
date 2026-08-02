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
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include <cstddef>                                                // offsetof (layout asserts)
#include "BrnCommonTypes.h"                                       // Vector3, VecFloat, EntityId, Matrix44Affine
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h" // EImpactType, EImpactSituation
#include "GameSource/GameState/BrnTakedownType.h"                 // BrnGameState::ETakedownType
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // BrnWorld::ERaceCarType (maeRaceCarTypes)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"  // RaceCarContact (mNormal @+48, mPointOnA @+64)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<1536,16> (the IO event queue the crash/takedown events push onto)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"        // CgsContainers::BitArray<N> (live-car bitset, crash-data free-list, taken-down bitset)

// Pointer-only collaborators in RaceCarResponseInfo -- forward-declared in their real namespaces
// (homed by their own TUs; the classifier never dereferences them here).
namespace BrnPhysics { namespace PhysicsModuleIO { class VehicleOutputRequestInterface; } }
namespace BrnPhysics { namespace Deformation { class DeformationInputInterface; } }
namespace BrnGameState { namespace GameStateModuleIO { class VehicleOutputInterface; } }

// Crash-prediction (race-car-vs-world) collaborators -- forward-declared in their real
// namespaces (homed by their own TUs; HandleCrashPredictionForRaceCarAndWorld only takes/forwards
// pointers + one by-value contact, so the declarations need no complete type here).
namespace BrnPhysics { namespace PhysicsModuleIO { struct PotentialContactInterface; } }
namespace BrnPhysics { namespace Vehicle { struct VehicleInputInterface; } }
namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact; struct TriangleCacheInterface; } }

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
    // RECONCILE 2026-07-24 (WorldModule fleet embed): the canonical
    // VehicleManagerOutputInterface home is SharedIO/BrnVehicleOutputInterface.h
    // (DWARF :82, full member layout). The declare-only shell that lived here was
    // retired; its method surface moved to the canonical struct additively.


    class VehicleManager
    {
    public:
        // ADDITIVE (WorldModule::Prepare @0x827D53B0 stage-8 success path reads the
        // surface-property table once the world entity module prepared). Static on the
        // X360 (a global manager pair). Declaration-only; body with this manager's TU.
        static void ReadSurfaceProperties();

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

        // ==========================================================================================
        // Race-car-vs-world crash-prediction slice (X360 @0x82640C28). DWARF-authoritative
        // signatures (BrnVehicleManager.h:1296/1200/1215). HandleCrashPredictionForRaceCarAndWorld
        // walks the interface's already-validated race-car-world potential-contact queue, groups the
        // contacts by their volume-A entity word (VolumeInstanceId high dword), orders each group by
        // predicted impact time via PotentialContactOrderer, and dispatches every surviving contact
        // to HandleRaceCarWorldPotentialContact. The two callees are bodied by their own TUs; declared
        // here (declare-only) so this slice can call them.
        // ------------------------------------------------------------------------------------------
        // @0x82640C28 -- the crash-prediction driver bodied by THIS slice.
        void HandleCrashPredictionForRaceCarAndWorld(
            f32 lfTimestep,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
            const VehicleInputInterface* lpVehicleInputInterface,
            BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // Declare-only callees (bodied by their own TUs). Signatures are DWARF-authoritative
        // (:1200 / :1215). The tri-cache arg is the nested VehicleInputInterface::InTriangleCacheInterface,
        // which is CgsSceneManager::SceneManagerIO::TriangleCacheInterface.
        void HandleRaceCarWorldPotentialContact(
            CgsSceneManager::SceneManagerIO::PotentialContact lContact,
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            f32 lfTimestep);

        VecFloat PredictCarWorldContactTime(const CgsSceneManager::SceneManagerIO::PotentialContact& lContact);

    private:
        // ------------------------------------------------------------------------------------------
        // Deep VehicleManager data members, recovered by LAYOUT RECOVERY WITH PADDING from the X360
        // asm offsets. The full VehicleManager is ~172 KB across many parallel per-car arrays;
        // everything not modelled is opaque padding so each named member lands at its proven byte
        // offset (pinned by the offsetof asserts in _AssertLayout / _AssertLayoutPlayerStats). The
        // gate FAILS if any padding run is wrong, which is the signal.
        //
        // ⭐ RE-SEATED 2026-08-03. Every member from the class head down to +44768 has now been
        // re-derived directly from `VehicleManager::Construct` @0x8263B7C8 (943 instructions) and
        // cross-checked against the DWARF's member ORDER (BrnVehicleManager.h:815-970), which lists
        // the same members in the same sequence. Four committed errors were corrected -- the whole
        // per-car driver array was 64 bytes too low, a stride-8 array was modelled at stride 4, a
        // car-TYPE array was named as a crash-STATE array, and three live bitsets were buried in
        // padding -- and nine members were newly pinned. Names below are the DWARF's wherever the
        // DWARF names that seat; the remaining "FLAG" names are still role-derived.
        //
        // ⚠️ Members reached by ABSOLUTE offset from inside a contained sub-object (the
        // PhysicalTrafficManager interior at +148128 / +149456) stay siblings here, because the X360
        // build folds them to absolute class offsets; that is deliberate, not an oversight.
        // ------------------------------------------------------------------------------------------

        // ==========================================================================================
        // ⭐ RE-SEATED 2026-08-03 (VehicleManager layout wave). This array used to be declared as
        // `RaceCarStatusRecord maRaceCarStatus[8]` at class offset **0**. It is really the DWARF's
        // `VehicleDriver maRaceCarDrivers[8]` at class offset **+64**, and the 64 bytes ahead of it
        // are mePrepareStage / meReleaseStage / mRandom (see the class head below).
        //
        // Why it mattered: the three named in-record fields were 64 bytes too high. They are
        // byte-faithful ONLY while the region is padding -- the instant a wave turns those 64 bytes
        // into real members, the three writes corrupt real driver state and the symptom (cars
        // mis-flagged) looks like a bug in the new constructor. Corrected while provably inert.
        //
        // [V] BOTH the base and the stride are asm-literal in VehicleManager::Construct @0x8263B7C8:
        //     0x8263BE90  addi r25, r31, 0x40      <- &maRaceCarDrivers[0] == this + 64
        //     0x8263BF08  bl   VehicleDriver::Construct
        //     0x8263BF80  addi r25, r25, 0xE0      <- stride 224, x8 -> ends at 1856
        // and 1856 is exactly where maRaceCarVehicles starts (asm below), so the array closes.
        //
        // ⚠️ THE RECORD IS A STAND-IN, NOT THE REAL VehicleDriver. The DWARF type is
        //   VehicleDriver { BrnAIDriverControls mControls;      // 0..80
        //                   Matrix44Affine mCatchupTargetTransform;  // +80
        //                   Matrix44Affine mSlerpTransform;          // +144
        //                   E_DRIVER_TYPE meDriverType;              // +208
        //                   int8_t mi8NumOfInterpSteps;              // +212
        //                   bool mbSnappedThisFrame; }               // +213  -> sizeof 224
        // so all three fields named here live INSIDE mControls. [I] Walking the DWARF
        // BrnPlayerDriverControls run (miVehicleID@0, twelve floats, miVehicleIDToMerge@52, then the
        // bool run mbReset@53 .. mbHorn@62) puts in-record 59/60/61 on mbBoostBounce /
        // mbIsOnStartLine / mbIsSteeringWheel. That is a HYPOTHESIS, not a measurement -- it assumes
        // an empty `CgsModule::Event` base (which this tree's `struct Event {}` is) and it has not
        // been checked against a use site. The role-derived names below are kept because the bodies
        // read them by role; DELETE-WHEN VehicleDriver + BrnAIDriverControls are really reconstructed
        // (that is wave T2-B), at which point this stand-in disappears entirely.
        struct RaceCarDriverRecord
        {
            unsigned char mPad0000[59];
            unsigned char mbBoostImpactEligible; // in-record +59 (asm: 224*idx + 123; promotes SLAM->BOOST_SLAM, SHUNT->BOOST_SHUNT)
            unsigned char mbTakenDown;           // in-record +60 (asm: 224*idx + 124; stores literal 1; also read as a crash-suppression flag)
            // in-record +61 (asm: 224*idx + 125): a second per-car suppression flag SetRaceCarCrashing
            // reads. It gates the crash by the cause sub-code (0/3/5). FLAG: role/name proposed.
            unsigned char mbSuppressByCause;     // in-record +61
            unsigned char mPad003E[224 - 62];
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

        // ==========================================================================================
        // THE CLASS HEAD -- re-derived 2026-08-03 from VehicleManager::Construct @0x8263B7C8 and
        // cross-checked against the DWARF member ORDER (BrnVehicleManager.h:815-847), which lists
        // exactly these members in exactly this sequence.
        //
        //   0x8263BCC0  stw  r30(0), 0(r31)      -> mePrepareStage = 0
        //   0x8263BCC8  stw  r24(3), 4(r31)      -> meReleaseStage = 3
        //   0x8263BCEC  addi r11, r31, 0x10      -> &mRandom == this + 16, then
        //               stw 1.0f, 0(r11) / stwx buf[i], 4*i(r11) / std seed, 0x20(r11) /
        //               stw index, 0x28(r11)
        //
        // ⚠️ mRandom is at +16, NOT +8. CgsNumeric::Random is
        // `union { f32[8]; u32[8]; VectorIntrinsic[2] } + u64 muSeed(+0x20) + u32 index(+0x28)`
        // == 44 bytes but **16-byte aligned** because of the VectorIntrinsic[2] member, so it
        // cannot sit at +8 and its sizeof is 48. 16 + 48 == 64 == &maRaceCarDrivers[0]: the head
        // closes on three independently-attested numbers. (An earlier brief reached the right
        // +64 answer from the wrong arithmetic -- mRandom@8 -- which would have left an 8-byte
        // hole in a different place.)
        // ==========================================================================================
        s32                  mePrepareStage;    // +0   EPrepareStage (Construct: 0)
        s32                  meReleaseStage;    // +4   EReleaseStage (Construct: 3)
        unsigned char        mPad0008[8];       // +8   alignment ahead of the 16-aligned mRandom
        // CgsNumeric::Random mRandom -- OPAQUE 48 bytes (alignas 16). The real type belongs to its
        // own TU (wave T2-B); modelled as a sized, aligned blob so every offset behind it is right.
        alignas(16) unsigned char mRandom[48];  // +16  (ends at 64)

        RaceCarDriverRecord  maRaceCarDrivers[8];   // +64      (224 * 8 = 1792; ends at 1856)
        RaceCarVehicleRecord maRaceCarVehicles[8];  // +1856    (5216 * 8 = 41728; ends at 43584)

        // Per-car EntityId validation table @ +43584. Stride 4 (asm: 4*(idx+10896) == 4*idx+43584;
        // Construct seeds it from dword_82F2A3A4 through a stride-4 cursor). SetRaceCarCrashing
        // asserts the packed victim/aggressor id matches the stored id here. Spelling per the DWARF
        // (`EntityId[8] maRaceCarEntityIDs`).
        EntityId             maRaceCarEntityIDs[8];   // +43584 (4 * 8 = 32; ends 43616)

        // ⭐ The 128 bytes at +43616..+43744 are the DWARF's two ResourceHandle arrays
        // (`ResourceHandle[8] maRaceCarModelHandles` then `ResourceHandle[8]
        // maRaceCarGraphicsModelHandles`, BrnVehicleManager.h:824/825). They fill the gap exactly at
        // **8 bytes per handle** -- 43616 + 64 = 43680, + 64 = 43744 -- which is the independent
        // confirmation that ResourceHandle is 8 bytes here. Modelled as an opaque span because
        // CgsResource::ResourceHandle has no committed home in this tree yet and Construct does not
        // touch either array; the two names + the 8-byte width are recorded so the next wave can
        // split it without re-deriving anything. DELETE-WHEN ResourceHandle lands.
        unsigned char        mPadAA60[43744 - 43616];  // maRaceCarModelHandles / maRaceCarGraphicsModelHandles

        // ⭐ CORRECTED 2026-08-03: this was committed as `EntityId
        // maAggressiveDrivingVictimEntityId[8]` at **stride 4**, which left 32 bytes of the span
        // unaccounted and would have mis-seated every element. It is the DWARF's
        // `RigidBodyId[8] maRaceCarHandlingBodyIDs` at **stride 8**:
        //   0x8263BE78/0x8263BE88  addis r26,r31,1 ; addi r26,r26,-0x5520  -> this + 43744
        //   0x8263BF44/0x8263BF48  ld r11, qword_82F2A3A8 ; std r11, 0(r26)   <- an 8-BYTE store
        //   0x8263BF78             addi r26, r26, 8                          <- stride 8, x8
        // 43744 + 64 == 43808, which is exactly where maRaceCarCrashes starts. RigidBodyId is
        // modelled as u64 (the sentinel it is seeded with, qword_82F2A3A8, is a 64-bit value --
        // the same K_INVALID_RIGID_BODY_ID idiom CgsPhysicsSimulationModule.h already names).
        u64                  maRaceCarHandlingBodyIDs[8]; // +43744 (8 * 8 = 64; ends 43808)

        // The crash-data pool @ +43808 (32 * 12 = 384; ends at 44192, abutting maeRaceCarTypes).
        // Spelling per the DWARF (`RaceCarCrashData[32] maRaceCarCrashes`).
        RaceCarCrashData     maRaceCarCrashes[32];  // +43808

        // ⭐ CORRECTED 2026-08-03: this was committed as `s32 maRaceCarCrashState[8]` with the note
        // "sentinel 2 == fatal crash state". It is the DWARF's `BrnWorld::ERaceCarType[8]
        // maeRaceCarTypes`, and the comparisons in the bodies are TYPE tests, not crash-state tests:
        //   Construct: `stw r24, 0(r28)` with r24 == 3 and r28 == this + 44192, stride 4, x8
        //              -- i.e. every slot is seeded E_RACE_CAR_TYPE_INACTIVE (== 3).
        //   the classifiers' `== 1`   is E_RACE_CAR_TYPE_AI      ("both cars are AI")
        //   SetRaceCarCrashing's `!= 2` is != E_RACE_CAR_TYPE_NETWORK ("not a network car")
        // The literals are numerically unchanged, so this is a NAMING correction with no behaviour
        // change -- but the old name made every read of it mean the wrong thing.
        BrnWorld::ERaceCarType maeRaceCarTypes[8];  // +44192   (4 * 8 = 32; ends at 44224)

        // The live-car bitset and the crash-data free-list, given their REAL CgsBitArray type so the
        // bodies use the container's named ops (IsBitSet/GetFirstNonZeroBit/SetBit) instead of raw
        // offset access. Each BitArray<N<=64> is a single 8-byte u64 field == the same image the X360
        // scans. Both offsets are asm-literal (Construct: `addis r11,r31,1; addi r11,r11,-0x5340`
        // -> 44224 and `addi r10,r10,-0x5338` -> 44232, each `std 0`). Names per the DWARF
        // (`mUsedRaceCars`, `BitArray<32u> mUsedRaceCarCrashesList`).
        CgsContainers::BitArray<8>  mUsedRaceCars;             // +44224 (live-car bitset)
        CgsContainers::BitArray<32> mUsedRaceCarCrashesList;   // +44232 (crash-data free-list)

        // ⭐ NEWLY PINNED: the contained StuntOffencesManager subobject. Construct calls
        // `StuntOffencesManager::Construct(this + 65536 - 0x5330)` == this + 44240 @0x8263C620, and
        // the next pinned member (mHiddenRaceCars) is at 44704, so the subobject occupies exactly
        // 464 bytes. Opaque until BrnStuntOffencesManager's layout pass; the span is what stops a
        // future member from being dropped into it.
        unsigned char        mStuntOffencesManager[44704 - 44240];  // +44240 (464 bytes)

        // ⭐ NEWLY PINNED / RENAMED: FOUR RaceCarBitArrays, not one. Construct zero-stores all four
        // back to back (0x8263C0A8..0x8263C0C0), and the DWARF lists exactly these four names in
        // this order (BrnVehicleManager.h:838-841):
        //   addi r11,r11,-0x5160 -> 44704   mHiddenRaceCars          (was mHiddenNetworkRaceCars)
        //   addi r9, r9, -0x5158 -> 44712   mRaceCarsAddedForCollision
        //   addi r8, r8, -0x5150 -> 44720   mNetworkCarsAddedForCollisionThisFrame
        //   addi r10,r10,-0x5148 -> 44728   mNetworkCarsRecievedFirstUpdate   (DWARF's spelling)
        // The committed header modelled 44712..44736 as padding, so three real bitsets were
        // invisible.
        CgsContainers::BitArray<8> mHiddenRaceCars;                        // +44704
        CgsContainers::BitArray<8> mRaceCarsAddedForCollision;             // +44712
        CgsContainers::BitArray<8> mNetworkCarsAddedForCollisionThisFrame; // +44720
        CgsContainers::BitArray<8> mNetworkCarsRecievedFirstUpdate;        // +44728
        // Per-car "hide for at least N frames" countdown @ +44736. Stride 4 (asm: `stw r30, 0x220(r28)`
        // off the stride-4 cursor at 44192 -> 44192 + 544 == 44736); SetNetworkRaceCarHidden stores
        // the requested frame count here. DWARF name (`uint32_t[8] mauNetworkCarHiddenFramesRemaining`).
        u32                  mauNetworkCarHiddenFramesRemaining[8];        // +44736 (ends 44768)

        // The contained PhysicalTrafficManager subobject begins at **+44768** -- asm-literal
        // (`addis r3,r31,1; addi r3,r3,-0x5120; bl PhysicalTrafficManager::Construct` @0x8263BF9C).
        // Still modelled as opaque padding, consistent with the existing layout that names
        // maRaceCarEntityIdRemap as a direct sibling at +148128 inside this region: the X360 build
        // folds every contained-manager member the VehicleManager methods touch to its absolute class
        // offset, so the members below are reached BY their absolute-offset NAMES rather than through
        // an embedded manager object.
        // Two further DWARF members live inside this span and are NOT separately pinned yet:
        // `PotentialContact[128] maNonPhysicalContacts` + `int32_t miNonPhysicalContactCount`
        // (BrnVehicleManager.h:850/851), which sit between the traffic manager and mDiscardedContacts.
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
        unsigned char        mPad24A68[160672 - 150056];

        // ⭐ NEWLY PINNED: the discarded-contact queue. Construct binds it in place @0x8263C048:
        //   addis r29,r31,2 ; addi r29,r29,0x73A0   -> this + 160672
        //   addi  r28,r29,0x10                      -> the buffer, this + 160688
        //   stw r28,0(r29) ; stw 0x14,4(r29) ; stw 0,8(r29)   -> {buffer, capacity 20, count 0}
        // plus the console's own `lpEventBuffer != NULL` assert (BrnContactSpyData.h:160). The
        // 16-byte header + 20 entries fills exactly to mDebugComponent, so each entry is 64 bytes.
        // DWARF: `ContactSpyData::DiscardedContactQueue mDiscardedContacts` (BrnVehicleManager.h:854).
        unsigned char        mDiscardedContacts[161968 - 160672];  // +160672 (1296 bytes)

        // ⭐ NEWLY PINNED: the manager's own debug component. Construct calls
        // `VehicleManagerDebugComponent::Construct(this + 161968, this)` @0x8263BCD8 -- note it takes
        // TWO arguments (r3 = the component, r4 = r31 = the manager); Hex-Rays renders it with none.
        unsigned char        mDebugComponent[163264 - 161968];     // +161968 (1296 bytes)

        // ⭐ NEWLY PINNED: the per-car debug components. Construct walks them in the 8-car loop with
        // `addis r27,r31,2 ; addi r27,r27,0x7DC0` -> this + 163264 and `addi r27,r27,0x400`
        // (stride 1024), storing each into the matching maRaceCarVehicles[] record and firing the
        // console's own `lpDebugComponent != NULL` assert. DWARF:
        // `BrnPhysics::Vehicle::DebugComponent[8] maRaceCarDebugComponent` (BrnVehicleManager.h:860),
        // immediately followed by `bool[8] mabRaceCarDebugComponentRegistered` (:861).
        // ⭐ THE CHAIN CLOSES TO THE BYTE: 163264 + 8*1024 == 171456, + 8 == 171464, which is the
        // independently asm-proven offset of the gate byte below. Four numbers, one closure.
        unsigned char        maRaceCarDebugComponent[8][1024];        // +163264 (ends 171456)
        bool                 mabRaceCarDebugComponentRegistered[8];   // +171456 (ends 171464)

        // Master gate byte @ +171464 (asm: a non-zero byte gates the whole routine).
        // ⚠️ FLAG (2026-08-03): the DWARF names the two bytes at this seat `mbSlamsAndShuntsOn`
        // (:865) and `mbAllowSlamsAndShuntsEffectsForRivals` (:866), and the closure above lands them
        // exactly here. The role-derived names are kept for now because the bodies read them as
        // gates and the X360 use sites have not been re-checked against the DWARF semantics; do the
        // rename in the wave that reconstructs the slam/shunt appliers, not in a layout pass.
        bool                 mbTakedownsEnabled;      // +171464 (DWARF: mbSlamsAndShuntsOn)

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

        unsigned char        mPad2A01C[171968 - (171900 + 4)];
        // ⭐ NEWLY PINNED: the manager's own spare AI driver. Construct calls
        // `VehicleDriver::Construct(this + 3*65536 - 0x6040)` == this + 171968 @0x8263C088 -- the
        // SECOND VehicleDriver::Construct call in the function, the first being the 8-car array at
        // +64. DWARF: `VehicleDriver mPlayerAiDriver` (BrnVehicleManager.h:953). Same 224-byte
        // stand-in record as the array.
        RaceCarDriverRecord  mPlayerAiDriver;         // +171968 (224; ends 172192)

        // [I] +172192..+172204 is the DWARF run that follows mPlayerAiDriver:
        //   bool mbPlayerAiDriverValid (:954), float mfPlayerRecentSteering (:955),
        //   float mfSteeringUpdateRemainder (:956)
        // -- which lands the next member on 172204 exactly, i.e. it is what CLOSES this span onto
        // the asm-proven mePlayerActiveRaceCarIndex. Left opaque rather than declared: the placement
        // comes from DWARF ORDER, not from an asm store, and a wrong guess here would be invisible.
        unsigned char        mPad2A040[172204 - 172192];

        // The local player's active-race-car slot @ +172204. DWARF-attested name (BrnVehicleManager.h:959).
        EActiveRaceCarIndex  mePlayerActiveRaceCarIndex; // +172204 (ends 172208)

        // [I] +172208..+172240 is the DWARF's six-float run (:962-968,
        // mfCrashingAICollisionCrashThresholdMPH .. mfVerticalTakedownAngleDeg) plus the 16-byte
        // alignment ahead of mCameraMatrix. Left opaque for the same reason as the span above.
        unsigned char        mPad2A050[172240 - (172204 + 4)];

        // ⭐ NEWLY PINNED: the camera matrix Construct stamps with the identity. Asm @0x8263C068:
        //   addis r11,r31,3 ; addi r11,r11,-0x5F30   -> this + 172240
        //   stvx128 v0,r0,r11 / v13,r11,0x10 / v12,r11,0x20 / v11,r11,0x30
        // -- four 16-byte lanes built on the stack from flt_82001C98 (1.0f) and flt_82001CC0 (0.0f).
        // DWARF: `Matrix44Affine mCameraMatrix` (BrnVehicleManager.h:970). 172240 is 16-aligned, so
        // the declaration needs no extra padding.
        Matrix44Affine       mCameraMatrix;           // +172240 (64; ends 172304)

        // [I] +172304 / +172305 are the DWARF's `mbImpactTime` (:972) and `mbEasyCrashingEnabled`
        // (:973); the next two bools ARE asm-proven and are named below.
        unsigned char        mPad2A0B0[172306 - 172304];
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
