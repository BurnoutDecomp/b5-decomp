#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h
// ============================================================================
// Minimal sized-stub home for BrnGameState::RoadRageModeScoring.
//
// PURPOSE: ScoringSystem embeds this Road-Rage sub-scorer BY VALUE and drives it
// through the lifecycle/query methods declared below. This is a MINIMAL SLICE --
// a complete, compilable placeholder so ScoringSystem (and its dependents) can name
// the type and call into it; the full member layout + the method BODIES land with
// this type's own TU later. Single owner: grow this slice, do not fork.
//
// SHAPE is DWARF-authoritative
// (references/DecFIGS/dwarfdump/GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h):
//   - It is a plain `struct RoadRageModeScoring` (no base in the DWARF).
//   - The DWARF spells the FULL data-member run (10 members, BrnRoadRageModeScoring.h:112-122),
//     so we name ALL of them in order -- no padding blob needed; the run is small and fully
//     typed. The X360 trailing tail pad (struct alignment) is not asserted (by-value embed,
//     named access -> MSVC computes the stride; no sizeof assert).
//   - All methods are reproduced DECLARE-ONLY (bodies belong to this type's own TU).
//
// The DWARF emits two per-using-TU instances of this struct that differ ONLY in
// IncrementPlayerNumTakedowns' GameActionQueue param namespace (InputBuffer:: vs
// OutputBuffer::). Both resolve to the same not-yet-committed GameActionQueue type;
// per the sibling StuntManager slice convention we model the as-yet-uncommitted
// pointer params as `void* /* RealType */` so the slice stays self-contained and
// compilable on its own. When GameActionQueue / RCEntityActiveRaceCarOutputInterface
// land their real homes, swap the void* placeholders for the typed pointers.

#include "types.hpp"                                          // f32, s32/u32, s16/u16
#include "GameShared/GameClasses/System/Timer/CgsTime.h"      // CgsSystem::Time (committed home)

namespace BrnGameState
{
    class ScoringSystem; // embeds this type by value + is passed to IncrementPlayerNumTakedowns

    // DWARF home BrnRoadRageModeScoring.h:47. Road-Rage mode scoring sub-object: tracks the
    // running takedown count, the moving target / time-extension state and the player-damage /
    // car-destroyed flags the HUD + mode logic poll. Embedded by value in ScoringSystem.
    struct RoadRageModeScoring
    {
    public:
        // ---- lifecycle (declare-only; bodies belong to this type's own TU) ----
        void Construct();                                  // DWARF .h:52
        bool Prepare(s32 liTargetNumTakedowns,
                     u16 luRoadRageExtensionTime);          // DWARF .h:58
        // DWARF .h:64. param0 is PostWorldInputBuffer::RCEntityActiveRaceCarOutputInterface*
        // (== BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface, not yet
        // committed here) -> modelled void*; second param is the frame delta (DWARF float32_t -> f32).
        void Update(const void* /* RCEntityActiveRaceCarOutputInterface* */ lpRaceCarOutput,
                    f32 lfDeltaTime);                       // DWARF .h:64
        bool Release();                                    // DWARF .h:68
        void Destruct();                                   // DWARF .h:72
        void ClearData();                                  // DWARF .h:76

        // DWARF .h:83. Bumps miNumTakedownsAchieved and, when a threshold is crossed, queues the
        // appropriate game action. The GameActionQueue param is uncommitted (DWARF emits it under
        // InputBuffer:: in one TU instance and OutputBuffer:: in the other) -> modelled void*.
        // Time is passed BY VALUE (committed CgsSystem::Time home).
        void IncrementPlayerNumTakedowns(ScoringSystem* lpScoringSystem,
                                         CgsSystem::Time lTime,
                                         void* /* GameActionQueue* */ lpGameActionQueue); // DWARF .h:83

        // ---- queries (declare-only; const where the DWARF says const) ----
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
