#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h
// ============================================================================
// Home of BrnGameState::RoadRageModeScoring (bodies: BrnRoadRageModeScoring.cpp).
//
// PURPOSE: ScoringSystem embeds this Road-Rage sub-scorer BY VALUE and drives it
// through the lifecycle/query methods declared below. The full DWARF member run is
// named here and the method BODIES live in this type's own TU. Single owner: grow
// this header, do not fork.
//
// SHAPE is DWARF-authoritative
// (references/DecFIGS/dwarfdump/GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h):
//   - It is a plain `struct RoadRageModeScoring` (no base in the DWARF).
//   - The DWARF spells the FULL data-member run (10 members, BrnRoadRageModeScoring.h:112-122),
//     so we name ALL of them in order -- no padding blob needed; the run is small and fully
//     typed. The X360 trailing tail pad (struct alignment) is not asserted (by-value embed,
//     named access -> MSVC computes the stride; no sizeof assert).
//   - All methods are declared here; bodies are in BrnRoadRageModeScoring.cpp.
//
// The DWARF emits two per-using-TU instances of this struct that differ ONLY in
// IncrementPlayerNumTakedowns' GameActionQueue param namespace (InputBuffer:: vs
// OutputBuffer::). Both are the ONE real typedef GameStateModuleIO::GameActionQueue
// (BrnGameStateSharedIO.h, == CgsModule::VariableEventQueue<13312,16>), and the X360
// caller ScoringSystem::OnPlayerDoesATakedown @0x8234CE08 hands the output buffer's
// queue straight through -- so the param is the typed pointer now.
// [2026-09-02] The two former `void* /* RealType */` placeholders are RETIRED: the
// GameActionQueue typedef and RCEntityActiveRaceCarOutputInterface both have committed
// homes. Bodies live in BrnRoadRageModeScoring.cpp (this type's own TU).

#include "types.hpp"                                          // f32, s32/u32, s16/u16
#include "GameShared/GameClasses/System/Timer/CgsTime.h"      // CgsSystem::Time (committed home)
#include "GameSource/GameState/BrnGameStateSharedIO.h"        // GameStateModuleIO::GameActionQueue (real typedef)

// Pointer-only param of Update (DWARF PostWorldInputBuffer::RCEntityActiveRaceCarOutputInterface
// == BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface, the same forward the
// BrnGameStateModuleIO.h / BrnChallengeManager.h precedents use). Full home:
// GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnGameState
{
    class ScoringSystem; // embeds this type by value + is passed to IncrementPlayerNumTakedowns

    // DWARF home BrnRoadRageModeScoring.h:47. Road-Rage mode scoring sub-object: tracks the
    // running takedown count, the moving target / time-extension state and the player-damage /
    // car-destroyed flags the HUD + mode logic poll. Embedded by value in ScoringSystem.
    struct RoadRageModeScoring
    {
    public:
        // ---- lifecycle (bodies in BrnRoadRageModeScoring.cpp) ----
        void Construct();                                  // DWARF .h:52
        bool Prepare(s32 liTargetNumTakedowns,
                     u16 luRoadRageExtensionTime);          // DWARF .h:58
        // DWARF .h:64 (.cpp:88 names the params lpActiveRaceCarInterface / lfSimTimeStep).
        // param0 is PostWorldInputBuffer::RCEntityActiveRaceCarOutputInterface* (== the BrnWorld
        // RaceCarEntityModuleIO interface forward-declared above); param1 is the sim time step.
        void Update(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                    f32 lfSimTimeStep);                     // DWARF .h:64
        bool Release();                                    // DWARF .h:68
        void Destruct();                                   // DWARF .h:72
        void ClearData();                                  // DWARF .h:76

        // DWARF .h:83 / X360 0x823445D0 (the type's only out-of-line symbol). Bumps the takedown
        // counters and, on threshold crossings, posts PlayerReachedRoadRageTarget (103) and the
        // time-extension HUD message (255, twice). The queue is the real GameStateModuleIO typedef
        // (the DWARF's InputBuffer::/OutputBuffer:: split names the same type). Time stays BY VALUE
        // here to match the committed caller ScoringSystem::OnPlayerDoesATakedown (the DWARF .cpp:170
        // spells it `Time&`; on the X360 both forms pass r5 as a pointer, so the asm cannot tell).
        void IncrementPlayerNumTakedowns(ScoringSystem* lpScoringSystem,
                                         CgsSystem::Time lCurrentTime,
                                         GameStateModuleIO::GameActionQueue* lpOutputActionQueue); // DWARF .h:83

        // ---- queries (const where the DWARF says const) ----
        s32  GetNumTakedownsAchieved() const;              // DWARF .h:86
        s32  GetTargetNumTakedowns() const;                // DWARF .h:89
        bool PlayerCarWasDestroyed() const;                // DWARF .h:92
        bool DoesDamageCriticalMessageNeedToBeSent() const; // DWARF .h:95
        void ResetDamageCriticalMessageFlag();             // DWARF .h:98
        bool IsActive();                                   // DWARF .h:101 (non-const per DWARF)
        void SetTakeDownTarget(s32 liTargetNumTakedowns);  // DWARF .h:105
        bool HasBeatenRoadRageTarget() const;              // DWARF .h:108

    private:
        // Full member run, DWARF-authoritative (BrnRoadRageModeScoring.h:112-122), in order.
        s32 miNumTakedownsAchieved;                  // .h:112
        s32 miNumTakedownsAchievedForNextExtention;  // .h:113
        s16 muRoadRageTriggerExtension;              // .h:114  (DWARF int16_t)
        u16 muRoadRageExtensionTime;                 // .h:115  (DWARF uint16_t)
        s32 miTargetNumTakedowns;                    // .h:116
        s32 miNextTimeIncreaseIndex;                 // .h:117
        bool mbDamageCriticalMessageNeedToBeSent;    // .h:119
        bool mbPlayerDamageCritical;                 // .h:120
        bool mbPlayerCarDestroyed;                   // .h:121
        bool mbGameModeActive;                       // .h:122
        // NOMINAL: exact struct tail padding deferred to this type's own TU (no sizeof assert --
        // embedded by value with named access; MSVC computes the stride from the members above).
    };
}
