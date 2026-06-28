#ifndef BRN_RACE_BALANCING_MANAGER_H
#define BRN_RACE_BALANCING_MANAGER_H

// BrnAI::RaceBalancingManager -- the race rubber-band CONTROLLER. It owns one
// RaceBalancingGraph + one RaceBalancingRoute per opponent slot (7 each) and, each
// frame, derives a "target speed" for an AI opponent so the field stays competitive:
// it samples the opponent's per-checkpoint par time, compares it to the live race
// time, and scales the section speed up (the racer is behind schedule) or down (the
// racer is ahead) within a bounded multiplier band.
//
// DWARF home: GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h
// (struct BrnAI::RaceBalancingManager, :48 in the DecFIGS dump). LAYOUT is recovered
// from the X360 asm (offset-authoritative); the DWARF supplies member names/types and
// declaration order. Touched offsets are pinned with static_assert via the never-called
// _AssertLayout() member.
//
// Bodied in this TU (the standalone X360 symbols):
//   OnRaceStart                  @0x82789AF8
//   OnOpponentReachedCheckpoint  @0x82789D88
//   ComputeParSpeed (private)    @0x82789EC0
//   ComputeTargetSpeed (private) @0x827916E0
//   CalculateScheduleOffset      @0x82789E00
// The remaining DWARF methods (Construct / OnRaceStartPlaying / OnRaceEnd / Update /
// UpdateOpponentRoute / OnOpponentTakenDown / the public ComputeTargetSpeed overload)
// were INLINED on X360 with no standalone symbol -> declared here, bodied in their own
// TUs (or inline at their call sites). Not bodied here; do not fabricate.

#include <cstddef>   // offsetof

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"               // Array<T,N>
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.h"  // BrnAI::RaceBalancingGraph, GraphType
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h"  // BrnAI::RaceBalancingRoute

namespace BrnAI
{
    // Pointer/reference-only collaborators reached through named accessors.
    struct AICar;
    struct AISectionsData;
    struct RouteNode;
    class  AIModule;

    // DWARF BrnAICar.h:648 / BrnRaceBalancingManager.h:102-109 -- the rubber-band
    // tuning constants (rodata). KF_DEFAULT_SPEED is the fall-back target speed
    // returned when the racer has no valid route / the race is not running
    // (flt_8300D7F8). The four band limits clamp the schedule-offset speed multiplier:
    // the IN_RANGE pair is the tight band used when the car is in the player's range
    // (E_AI_CAR_STATE_IN_RANGE), the OUT_OF_RANGE pair the wider band otherwise.
    extern const f32 KF_DEFAULT_SPEED;                       // :102 (flt_8300D7F8)
    extern const f32 KF_TARGET_TIME_TOLERANCE;               // :103
    extern const f32 KF_SPEED_DIFFERENCE_MULTIPLIER;         // :104 (0.1 ; flt_820C424C)
    extern const f32 KF_MAX_SPEED_MULTIPLIER_IN_RANGE;       // :105 (1.1 ; flt_820C3D90)
    extern const f32 KF_MIN_SPEED_MULTIPLIER_IN_RANGE;       // :106 (0.9 ; flt_820C48A0)
    extern const f32 KF_MAX_SPEED_MULTIPLIER_OUT_OF_RANGE;   // :107 (1.2 ; flt_820C48A4)
    extern const f32 KF_MIN_SPEED_MULTIPLIER_OUT_OF_RANGE;   // :108 (0.8 ; flt_820C4330)
    extern const f32 KF_PLAYER_CRASHING_TIME_FACTOR;         // :109

    // DWARF BrnRaceBalancingManager.h:48. The DecFIGS dump spells it `struct`.
    struct RaceBalancingManager
    {
        // ---- public interface (DWARF declaration order :56-97) -----------------------------
        void Construct(AIModule* lpAIModule);                                   // :56  (inlined on X360)

        // @0x82789AF8 -- bodied in this TU. NOTE: the DecFIGS DWARF lists only
        // (const Array*, int32_t), but the X360 asm reads a third register argument (r6)
        // -- a bool that selects each route's initial mfTakenDownTimePenalty (20.0s when
        // set, 5.0s otherwise). Reproduced here as an explicit 3rd param (likely a
        // defaulted arg the DWARF dropped); see open_questions.
        void OnRaceStart(const Array<RaceBalancingGraph, 7u>* lpRaceBalancingGraphArray,
                         s32 liCheckpointCount, bool lbHighTakenDownPenalty);   // :61

        void OnRaceStartPlaying();                                              // :64  (inlined on X360)
        void OnRaceEnd();                                                       // :67  (inlined on X360)
        void Update(const AICar* lpPlayerCar, f32 lfTimeStep);                  // :72  (inlined on X360)
        void UpdateOpponentRoute(const AICar* lpAICar,
                                 const AISectionsData* lpAISectionsData);       // :77  (inlined on X360)

        // @0x82789D88 -- bodied in this TU.
        void OnOpponentReachedCheckpoint(const AICar* lpAICar, s32 liCheckpointIndex); // :82

        void OnOpponentTakenDown(const AICar* lpAICar);                         // :86  (inlined on X360)

        // Public overload (inlined on X360); forwards to the private GraphType overload.
        f32  ComputeTargetSpeed(const AICar* lpAICar,
                                const AISectionsData* lpAISectionsData,
                                bool lbPlayerIsCrashing) const;                 // :92  (inlined on X360)

        // @0x82789E00 -- bodied in this TU. lafOutScheduleOffsets is a f32[2]:
        // [E_GRAPH_TYPE_AHEAD]=seconds the racer is ahead of schedule,
        // [E_GRAPH_TYPE_BEHIND]=seconds behind. (Note: the asm writes [1] then [0].)
        void CalculateScheduleOffset(const AICar* lpAICar, f32* lafOutScheduleOffsets) const; // :97

    private:
        // @0x827916E0 -- bodied in this TU.
        f32  ComputeTargetSpeed(GraphType leGraphType, const AICar* lpAICar,
                                const AISectionsData* lpAISectionsData) const;  // :115
        // @0x82789EC0 -- bodied in this TU.
        f32  ComputeParSpeed(GraphType leGraphType, const AICar* lpAICar,
                             const AISectionsData* lpAISectionsData) const;     // :121

        // Never called; gives offsetof a member context so the compile gate pins the
        // touched offsets (the private members below).
        static void _AssertLayout();

        // ---- storage (declaration order == layout order; DWARF :123-131) -------------------
        Array<RaceBalancingGraph, 7u> maRaceBalancingGraphs;  // +0x0000 (graphs.miCount @+0x01C0)
        Array<RaceBalancingRoute, 7u> maRaceBalancingRoutes;  // +0x01C4 (routes.miCount @+0x486C)
        f32  mfRaceTime;                                      // +0x4870 (18544)
        s32  miCheckpointCount;                               // +0x4874 (18548)
        bool mbInRace;                                        // +0x4878 (18552)
        bool mbOnStartLine;                                   // +0x4879 (18553)

        // PLACEHOLDER -- BrnAI::RaceBalancingDebugComponent (DWARF :131). It derives from
        // CgsDev::DebugComponent (vtable + base members) and adds mpAIModule /
        // mpRaceBalancingManager / miCurrentRival / mbShowData. Its exact sizeof is not
        // recoverable from this TU's asm (the 5 bodied funcs never touch it), so it is an
        // opaque byte placeholder here; total sizeof(RaceBalancingManager) is therefore NOT
        // pinned. Every member the bodied funcs DO touch is pinned below. See open_questions.
        u8 mRaceBalancingDebugComponent_placeholder[0x40];   // +0x487C (size unverified)
    };
    // The touched private offsets are pinned inside _AssertLayout() (BrnRaceBalancingManager.cpp)
    // where offsetof has the member context it needs; the compile gate enforces them.
}

#endif
