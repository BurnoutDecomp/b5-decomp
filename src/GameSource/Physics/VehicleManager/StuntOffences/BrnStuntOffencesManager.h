#pragma once

// BrnPhysics::StuntOffencesManager -- the PHYSICS-side stunt/offence detector. One instance lives
// inside the vehicle physics module (owned by VehicleManager). Each frame, for the player's active
// race car it: tracks in-air status (takeoff/landing), integrates body angular velocity into an
// accumulated in-air rotation to score barrel rolls + flat/air spins, classifies clean vs crash and
// "successful" (big-air) landings, scores drifts (time + distance), handbrake turns, and
// tailgating/convoy relationships against the other live cars. It emits two event streams onto the
// GameState GameEventQueue -- in-progress stunts (every frame a stunt is live) and completed stunts
// (one-shot when a stunt finishes) -- consumed by the GameState StuntManager / scoring.
//
// OWNING HOME (this TU). The X360 indexes this class by RAW BYTE OFFSET, so the byte LAYOUT is
// asm-authoritative and is reproduced exactly here; member NAMES/TYPES are from the DecFIGS DWARF
// (BrnStuntOffencesManager.h). Because the compiler reordered the source members, the physical
// layout is expressed with explicit offsets (named members + alignment pads). Every recovered offset
// is pinned by offsetof asserts in the never-called _AssertLayout(); sizeof is pinned to 452.
//
// The class accesses the FOREIGN per-car RaceCarPhysics record and per-driver record by raw offset
// (drift Z-speed @+0x1010, velocity reg @+0x6C0, position/transform row @+0x1340, drift-active float
// @+0x109C, handbrake-held byte @+0x135B, "should be airborne" gate @+0x1350, driver-state word
// @+0xD0). Those are spelled through narrow accessors GROWN onto VehiclePhysics by name (see the .cpp
// + shared_header_grows); each is FLAGged.

#include "types.hpp"
#include <cstddef>                                                  // offsetof (layout asserts)
#include "BrnCommonTypes.h"                                         // Vector2, Vector3, Matrix44Affine
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h" // the 3 enums
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // CgsModule::VariableEventQueue<1536,16>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"          // CgsContainers::BitArray<8> (live-car bitset)

namespace BrnPhysics { namespace Vehicle { class RaceCarPhysics; } }
namespace BrnGameState { namespace GameStateModuleIO { class GameEventQueue; } } // == VariableEventQueue<1536,16>

namespace BrnPhysics
{
    // FLAG: GameState-side output target (OutputStuntsInProgress arg). Not homed by this physics TU;
    // forward-declared. The body writes its live stunt scalars into it by word offset via a narrow grow.
    class RaceCarState;

    class StuntOffencesManager
    {
    public:
        void Construct();                                                                          // @0x825E8C08

        void Update(Vehicle::RaceCarPhysics* lpaRaceCarPhysics,
                    void* lpaRaceCarDrivers,
                    BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                    EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
                    const CgsContainers::BitArray<8>* lpUsedRaceCars,
                    f32 lfTimeStep);                                                               // @0x82642408

        void OutputStuntsInProgress(RaceCarState* lpRaceCarState,
                                    BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue); // @0x8263B278

    private:
        void SetCurrentCarInAirStatus(Vehicle::RaceCarPhysics* lpRaceCarPhysics, f32 lfTimeStep);   // @0x826135A8
        void CheckForTakenOffLanding(Vehicle::RaceCarPhysics* lpRaceCarPhysics);                    // @0x825BB078
        void UpdateInAirRotations(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                  BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                  f32 lfTimeStep);                                                  // @0x825BB168
        void CheckForRollsAndSpins(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                   BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                   f32 lfTimeStep);                                                 // @0x8263B508
        void CheckForHandBreakTurns(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                    BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                    f32 lfTimeStep);                                                // @0x825E3A38
        void CheckForCleanLanding(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                  BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                  f32 lfTimeStep);                                                  // @0x82614580
        void CheckForSuccessfulLanding(Vehicle::RaceCarPhysics* lpRaceCarPhysics, f32 lfTimeStep);  // @0x825BB320
        void CheckForDrift(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                           BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                           f32 lfTimeStep);                                                         // @0x82613820

        void CheckForConvoy(Vehicle::RaceCarPhysics* lpaRaceCarPhysics, void* lpaRaceCarDrivers,
                            EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
                            const CgsContainers::BitArray<8>* lpUsedRaceCars, f32 lfTimeStep);      // @0x82628EC8
        s32  GetTailgaterIndex(EActiveRaceCarIndex leActiveRaceCarIndex,
                               Vehicle::RaceCarPhysics* lpaRaceCarPhysics, void* lpaRaceCarDrivers,
                               const CgsContainers::BitArray<8>* lpRaceCarsToCheck);                // @0x82613F68
        s32  GetTailgateeIndex(EActiveRaceCarIndex leActiveRaceCarIndex,
                               Vehicle::RaceCarPhysics* lpaRaceCarPhysics, void* lpaRaceCarDrivers,
                               const CgsContainers::BitArray<8>* lpRaceCarsToCheck);                // @0x82613960
        static bool IsWithinTailgatingCone(const Vector3& lvForward, const Vector3& lvFrom,
                                           const Vector3& lvTo);                                    // @0x825BB290

        void ResetCompleteOutputValues();                                                          // @0x825C2D80
        void OutputStuntsCompleted(BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue); // @0x8263B3E8

        static void _AssertLayout();   // never called; pins offsets

        // ==========================================================================================
        // LAYOUT -- byte offsets are asm-proven (see the per-member // +0x.. note). Vector3/Vector2 are
        // 16-byte SIMD registers. Named members sit at their console offsets; `mPadNNNN[]` runs bridge
        // the holes the compiler left between reordered members so each named field lands exactly.
        // ==========================================================================================

        Vector3   mvCurrentInAirRotations;      // +0x000  integrated body angular velocity while airborne (.y spin axis, .z roll axis)
        Vector3   mvStuntRollInProgress;        // +0x010  running roll/spin angle accumulator this airborne pass
        f32       mfTimeInTheAirSoFar;          // +0x020  seconds airborne this jump
        f32       mfLastAirTime;                // +0x024  air time of the last completed jump
        u32       muCurrentRaceCarState;        // +0x028  ECurrentCarState bitfield
        u32       muStuntActionInProgress;      // +0x02C  EStuntActionInProgress bitfield
        u32       muStuntActionComplete;        // +0x030  EStuntActionComplete bitfield
        u8        mPad0034[0x040 - 0x034];      // align to the +0x40 Vector3
        Vector3   mvRaceCarPositionLastFrame;   // +0x040  car flattened position/at-vector last frame (handbrake bearing)
        f32       mfBearingLastFrame;           // +0x050  car heading last frame (atan2 of at-vector)
        f32       mfHandBreakAngleSoFar;        // +0x054  accumulated handbrake-turn angle (deg)
        f32       mfHandBrakeStabiliseTime;     // +0x058  stable-hold timer after a handbrake turn
        bool      mbHandbreakTurnAttempting;    // +0x05C  a handbrake turn is being tracked
        u8        mPad005D[0x060 - 0x05D];      // align to the +0x60 Vector3
        Vector3   mvTakeOffVector;              // +0x060  flattened car heading at takeoff (clean-landing compare)
        Vector3   mvLandingVector;              // +0x070  flattened landing heading (clean-landing compare)
        bool      mbKeepCheckingForCleanLanding;// +0x080  clean-landing geometry check armed
        u8        mPad0081[0x084 - 0x081];      // align to +0x84 float
        f32       mfCleanLandingCheckTimeSoFar; // +0x084  time spent in the clean-landing window
        u8        mPad0088[0x090 - 0x088];      // gap to the +0x90 Vector2
        Vector2   mvPositionAtTakeoff;          // +0x090  flattened car position at takeoff (jump-distance origin)
        f32       mfDistanceInAirSoFar;         // +0x0A0  horizontal distance travelled this jump (m)
        f32       mfDistanceOfLastJump;         // +0x0A4  distance of the last completed jump (m)
        f32       mfSuccesssfulLandingCheckTimeSoFar; // +0x0A8  big-air landing-check countdown (>=0 while armed)
        bool      mbKeepCheckingForSuccessfulLanding; // +0x0AC  big-air "successful landing" check armed
        bool      mbInAirLastFrame;             // +0x0AD
        bool      mbSuccessfulLanding;          // +0x0AE  a successful landing fired this frame
        bool      mbTookOffInReverse;           // +0x0AF  car left the ground travelling backwards
        bool      mbWasDriftingLastFrame;       // +0x0B0  a drift was live last frame
        u8        mPad00B1[0x0B4 - 0x0B1];      // align to +0xB4 float
        f32       mfTimeDriftingLastFrame;      // +0x0B4  drift time accumulated last frame

        // ---- per-link convoy slots: 3 parallel f32[8] @ +0xB8 / +0xD8 / +0xF8. Construct's stride-4
        //      loop base = this+0xD8, writing *v7 (+0xD8 timer, zero), v7[8] (+0xF8 dist2, zero), and
        //      *(v7-8) (+0xB8 dist, NaN == "no link this slot"). The convoy code fills these per link.
        f32       maConvoyDistance[8];          // +0x0B8  per-link convoy distance (NaN-seeded == "no link")
        f32       maConvoyTimer[8];             // +0x0D8  per-link convoy timer  (loop base; zero-seeded)
        f32       maConvoyDistance2[8];         // +0x0F8  per-link convoy second distance (zero-seeded)
        s32       miConvoyCount;                // +0x118  live convoy chain length (abuts maConvoyDistance2)

        // ---- COMPLETED-stunt output block. ResetCompleteOutputValues stride-4 loop base = this+0x13C:
        //      *v2 (+0x13C), *(v2-8) (+0x11C, NaN), v2[8] (+0x15C), flag byte @ this+0x17C+i.
        f32       maCompletedTailgateA[8];      // +0x11C  NaN-seeded
        f32       maCompletedTailgateB[8];      // +0x13C  zero-seeded (loop base)
        f32       maCompletedTailgateC[8];      // +0x15C  zero-seeded
        u8        maCompletedTailgateFlag[8];   // +0x17C  per-link complete flag
        s32       miCompletedConvoyCount;       // +0x184  completed convoy length
        f32       mfCompletedBarrelRollAngle;   // +0x188  total barrel-roll angle (deg)
        f32       mfCompletedAirSpinAngle;      // +0x18C  total air-spin angle (deg)
        f32       mfCompletedHandbreakTurnAngle;// +0x190  handbrake-turn angle (deg)
        f32       mfCompletedDriftTime;         // +0x194  drift duration (s)
        f32       mfCompletedDriftDistance;     // +0x198  drift distance (m)
        s32       miCompletedBarrelRolls;       // +0x19C  whole barrel rolls counted
        f32       mfCompletedAir;               // +0x1A0  air time (s)
        f32       mfCompletedAirDistance;       // +0x1A4  jump distance (m)
        bool      mbConvoyComplete;             // +0x1A8  the convoy fully broke this frame (Reset clears; event byte v25)
        u8        mPad01A9[0x1AC - 0x1A9];      // align to +0x1AC
        s32       miCompletedAirSpinTurns;      // +0x1AC  whole air-spin turns (Reset clears; abuts +0x1B0)

        f32       mfInProgressBarrelRollAngle;  // +0x1B0
        f32       mfInProgressAirSpinAngle;     // +0x1B4
        f32       mfInProgressHandbreakTurnAngle;// +0x1B8
        f32       mfInProgressDriftTime;        // +0x1BC
        f32       mfInProgressDriftDistance;    // +0x1C0
    };
}
