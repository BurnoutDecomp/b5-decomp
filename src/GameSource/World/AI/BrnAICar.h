#pragma once

// BrnAI::AICar -- the per-racer AI car state block (route / position / behaviour / per-frame
// flags) that the AI driver and the aggression state machine read while shadowing, overtaking
// and slamming a target car.
//
// MINIMAL-SLICE HOME (GROWN). This header began life letting BrnAIAggression.cpp compile its
// 35 function bodies against the AICar members/accessors they touch, using NAMED access (never
// raw offset casts). It is still NOT the full AICar layout -- the class is large (route data,
// nine Vector3 transforms, the section/portal indices, etc.); everything the bodied consumers
// do not touch is represented by explicit padding. Each grow carves the additionally-touched
// members out of the pads, names them from the DecFIGS BrnAICar.h DWARF, and pins EACH new
// offset with a static_assert. Existing members/offsets/asserts are kept byte-identical.
//
// GROWN by the AICar speed + route-progress core (BrnAICar.cpp: CalcDesiredSpeed /
// CalcPersonalitySpeed / CalcRoadRageSpeed / GetDecentSpeed / UpdateRaceDistance /
// UpdateRelativePositionToPlayer / NeedsNewRoute / UpdateRoute / UpdateRouteFinding /
// UpdateRouteFindingRace / CheckForSectionChange / CurrentSectionIsOK / GetBestSectionIndex /
// GetSectionIndexOfLastSectionInRoute). The newly-carved members and their X360-asm offsets:
//   meBehaviour            @+0x14B4 (5300)  -- CalcDesiredSpeed reads `meBehaviour != 1`
//   meSpeedSelectionMethod @+0x14BC (5308)  -- CalcDesiredSpeed switch discriminant
//   mfRouteTimer           @+0x14EC (5356)  -- UpdateRoute clears it on a fresh route
//   mfWrongWayTime         @+0x14F4 (5364)  -- CheckForSectionChange / UpdateRaceDistance timer
//   mfDistanceAheadOfPlayer@+0x14F8 (5368)  -- CalcDesiredSpeed gate / UpdateRaceDistance writes
//   mfRaceTimer            @+0x14FC (5372)  -- CalcDesiredSpeed "settled in race" gate (>15s)
//   mfAlternativeRouteTimer@+0x1500 (5376)  -- UpdateRouteFindingRace alt-route countdown
//   mfTimeInInvalidSection @+0x1514 (5396)  -- CheckForSectionChange invalid-section dwell timer
//   miRouteTimeStamp       @+0x1528 (5416)  -- UpdateRoute bumps it each fresh route
//   mbIsInGameMode         @+0x154B (5451)  -- UpdateRaceDistance schedule-accumulate gate
//   mbIsInShortcut         @+0x154C (5452)  -- (declaration-order filler between named members)
//   mbRouteRequested       @+0x154D (5453)  -- NeedsNewRoute result / route-finding "ask again"
//   mbIsInMasterRoute      @+0x154E (5454)  -- UpdateRoute clears it on a fresh route
//
// OFFSET AUTHORITY is the X360 pseudocode/asm (the DecFIGS DWARF gives the names/types/order
// but is the PS3 build and is NOT offset-authoritative). The anchor is mAggressiveness @
// this+0x140C (5132). Every named offset below is pinned by a static_assert (the compile gate
// enforces them).
//
// VISIBILITY: the touched data members are exposed here (public) so the bodies can read them by
// name; the real class marks most of them private and reaches them through the inlined-away
// getters. A named field read is the faithful de-inlined form of the X360's direct
// `*(car+offset)` load.

#include <cstddef>                 // offsetof

#include "types.hpp"               // f32, s32, u8, ...
#include "BrnCommonTypes.h"        // Vector3 (rw::math::vpu::Vector3, 16-byte SIMD)
#include "GameSource/World/AI/BrnAIAggressiveness.h"   // BrnAI::Aggressiveness (embedded by value)
#include "GameSource/World/AI/BrnAISharedConstants.h"  // BrnAI::ERouteFindingStyle, BrnAI::EAICarState

namespace BrnAI
{
    class AIDriver;                 // pointer-only collaborator (the aggression bodies never dereference it)
    struct RouteNode;               // pointer-only return of GetNextRouteNode (full type in BrnRoute.h)
    struct Route;                   // mRoute home (full type in BrnRoute.h)
    struct AISectionsData;          // route/section data passed to the speed + race-distance funcs
    struct RaceBalancingManager;    // the race rubber-band controller (ComputeTargetSpeed)

    // DWARF BrnAICar.h:48 -- where this car sits relative to the player, used by the
    // OUT_OF_RANGE / HANG_AROUND_AHEAD aggression states (read at AICar+0x14D4).
    enum ELocationRelativeToPlayer
    {
        E_RELATIVE_BEHIND_APPROACHING  = 0,
        E_RELATIVE_BEHIND_SEPARATING   = 1,
        E_RELATIVE_INFRONT_APPROACHING = 2,
        E_RELATIVE_INFRONT_SEPARATING  = 3,
        E_RELATIVE_UNKNOWN             = 4,
    };

    // DWARF BrnAICar.h:78. MINIMAL SLICE -- see file header.
    struct AICar
    {
        // ---- accessors the aggression bodies call (no storage) -----------------------------
        // The Vector3 getters use the sret ABI exactly as the X360 asm sets them up
        // (GetPosition(&out, car)). All are X360-attested separate TUs -> declare-only here.
        Vector3 GetPosition() const;          // X360 AICar::GetPosition       @0x8276B1F0
        Vector3 GetDirection() const;         // X360 AICar::GetDirection       @0x8276B488
        Vector3 GetUsefulDirection() const;   // X360 AICar::GetUsefulDirection @0x82770028
        Vector3 GetVelocity() const;          // X360 AICar::GetVelocity        @0x8276B570
        f32     GetSpeed() const;             // X360 AICar::GetSpeed           @0x82764D68
        bool    IsOnStartLine() const;        // X360 AICar::IsOnStartLine      @0x82764E68
        // GetRight is inlined on the X360 build (no standalone symbol) but IS a DWARF accessor
        // (BrnAICar.h:274) the aggression geometry helpers call. Declare-only; the per-TU cl /c
        // gate needs only the declaration. (Returns the car's world-space right vector.)
        Vector3 GetRight() const;

        // Inlined-away getter (DWARF BrnAICar.h:547). Restored inline -> &mAggressiveness so the
        // bodies reach the speed-match knobs via the public Aggressiveness getters.
        Aggressiveness*       GetAggressiveness()       { return &mAggressiveness; }
        const Aggressiveness* GetAggressiveness() const { return &mAggressiveness; }

        // ---- accessors the RaceBalancingManager (rubber-band) bodies call -------------------
        // All X360-attested separate TUs (or trivial inlined-away getters); declare-only here so
        // the manager .cpp compiles against named calls instead of raw `*(car+offset)` loads.
        // HasValidRoute(): X360 inlined as (mRoute.GetStatus() != E_STATUS_UNINITIALISED &&
        //   mRoute.GetNodeCount() > 0) -- reads Route::meStatus @+0x1408 and miNodeCount @+0x1400.
        bool             HasValidRoute() const;              // DWARF BrnAICar.h:262
        const RouteNode* GetNextRouteNode() const;           // DWARF BrnAICar.h:182 (Route::GetNode(miNextRouteNodeIndex), bounds-checked)
        s32              GetNextRouteNodeIndex() const;       // DWARF BrnAICar.h:185 (== miNextRouteNodeIndex @+0x1524)
        f32              GetMaxPlayerSpeed() const;           // DWARF BrnAICar.h:287 (== mfMaxPlayerSpeed @+0x1504)
        s8               GetOpponentIndex() const;            // DWARF BrnAICar.h:371 (== miOpponentIndex @+0x153A)
        bool             IsAheadOfPlayer() const;             // DWARF BrnAICar.h:332 (== mbIsAheadOfPlayer @+0x153B)
        EAICarState      GetState() const;                   // DWARF BrnAICar.h:275 (== meCarState @+0x14C8)
        bool             IsPlayerCar() const;                // DWARF BrnAICar.h:326 (== mbIsPlayer @+0x1549)

        // ---- accessors the speed + route-progress core (BrnAICar.cpp, this UNIT) call -------
        // GetDecentSpeed @0x82766030 -- the "decent" floor speed for the car's current route-
        // finding style (a fraction of mfMaxPlayerSpeed). Bodied in BrnAICar.cpp.
        f32  GetDecentSpeed() const;                          // DWARF BrnAICar.h:575
        // IsActive(): X360 inlined as (meCarState == IN_RANGE || meCarState == OUT_OF_RANGE).
        // Restored inline so the asserts in the speed/route funcs read true to the asm.
        bool IsActive() const
        {
            return meCarState == E_AI_CAR_STATE_IN_RANGE ||
                   meCarState == E_AI_CAR_STATE_OUT_OF_RANGE;
        }
        bool IsCrashing() const { return mbIsCrashing; }      // DWARF BrnAICar.h:302 (== mbIsCrashing @+0x1542)

        // ComputeDistanceToCheckpoint @ (separate TU; declare-only). Fills *lpfOutDistance with
        // this car's distance-to-checkpoint along the route; returns false when it cannot be
        // computed. UpdateRaceDistance consumes it.
        bool ComputeDistanceToCheckpoint(const AISectionsData* lpAISectionsData,
                                         const AICar* lpPlayerCar, f32* lpfOutDistance) const;

        // MoveToSectionOnRoute @ (separate TU; declare-only). True when the car can advance to
        // the given section on its current route. CheckForSectionChange drives it.
        bool MoveToSectionOnRoute(u16 luSectionIndex, const Route* lpRoute);

        // SetNextRouteNodeIndex @ (separate TU; declare-only). UpdateRoute seeds the next node.
        void SetNextRouteNodeIndex(s32 liNodeIndex);

        // The route-finding sub-dispatchers UpdateRouteFinding fans out to (separate TUs).
        void UpdateRouteFindingFreeRoam(const AICar* lpPlayerCar);   // DWARF BrnAICar.h:118
        bool IsExtrapolatedRouteGettingOld();                        // DWARF BrnAICar.h:637

        // ---- accessors the RouteRequestManager (route-request brain) bodies call --------------
        // Sentinel for "no section". X360 compares the section indices to 0x7FFF
        // (BrnWorld::KI_INVALID_SECTION_INDEX -- named in the GenerateExtrapolated/
        // GenerateRouteFleeingRouteRequest assert strings).
        static const u16 KI_INVALID_SECTION_INDEX = 0x7FFF;

        // X360-inlined at every Generate* call: the best section, falling back to the
        // default section when the best is still the invalid sentinel. Reads
        // muBestSectionIndex @+0x1534 then muDefaultSectionIndex @+0x1532.
        u16 GetBestSectionIndex() const
        {
            u16 luBest = muBestSectionIndex;
            if (luBest == KI_INVALID_SECTION_INDEX)
                luBest = muDefaultSectionIndex;
            return luBest;
        }

        // The destination section the route-request brain wrote (==muDestinationSectionIndex
        // @+0x1536; GenerateFreeRoamingDestination stores into it, the standard/alternative
        // builders read it back as the route's end section).
        u16  GetDestinationSectionIndex() const { return muDestinationSectionIndex; }
        void SetDestinationSectionIndex(u16 luSection) { muDestinationSectionIndex = luSection; }

        s32  GetRaceCarIndex() const { return miRaceCarIndex; }       // +0x14C4

        // Per-frame route-request decision flags (read directly by the X360 builders).
        bool WantsAlternativeRoute() const   { return mbWantsAlternativeRoute != 0; }   // +0x153F
        bool ForceStandardRoute() const      { return mbForceStandardRoute != 0; }      // +0x1548
        bool HasBlockCheckpoints() const     { return mbHasBlockCheckpoints != 0; }     // +0x153D
        bool UseAIShortcuts() const          { return mbUseAIShortcuts != 0; }          // +0x153E
        bool UseChosenDistanceFunction() const { return mbUseChosenDistanceFunction != 0; } // +0x1550

        // Inlined-away discriminators the route-request brain reads directly.
        ERouteFindingStyle GetRouteFindingStyle() const { return meRouteFindingStyle; }   // +0x14C0
        bool               IsDrivenByPlayer() const     { return mbIsDrivenByPlayer; }     // +0x154A
        s32                GetCurrentCheckpoint() const  { return miCurrentCheckpoint; }    // +0x1520

        // X360 AICar::NeedsNewRoute @0x82765F80 (called from RouteRequestManager::Update): true
        // when this car's route is stale and a fresh request must be issued. Bodied in
        // BrnAICar.cpp (this UNIT).
        bool NeedsNewRoute() const;

        // ---- the speed + route-progress core itself (bodied in BrnAICar.cpp, this UNIT) ------
        // Per-frame desired target speed (mode-dependent); restores the rubber-band call.
        f32  CalcDesiredSpeed(const RaceBalancingManager* lpRaceBalancingManager,
                              AISectionsData* lpAISectionsData, const AICar* lpPlayerCar);   // @0x82796078
        f32  CalcPersonalitySpeed(const AICar* lpPlayerCar);                                 // @0x8276F610
        f32  CalcRoadRageSpeed(const AICar* lpPlayerCar);                                    // @0x8276F8A0

        void UpdateRaceDistance(const AISectionsData* lpAISectionsData,
                                const AICar* lpPlayerCar, f32 lfTimeStep);                   // @0x8278AEB8
        void UpdateRelativePositionToPlayer(const AICar* lpPlayerCar);                       // @0x8276FA88

        void UpdateRoute(const Route* lpRoute, const AISectionsData* lpAISectionsData);      // @0x8276F2C8
        void UpdateRouteFinding(f32 lfTimeStep, const AICar* lpPlayerCar);                   // @0x8278ADB8
        void UpdateRouteFindingRace(f32 lfTimeStep, const AICar* lpPlayerCar);               // @0x82765E08

        void CheckForSectionChange(f32 lfTimeStep, AISectionsData* lpAISectionsData,
                                   const Route* lpRoute);                                    // @0x8277BFD0
        bool CurrentSectionIsOK();                                                           // @0x82765D18
        u16  GetSectionIndexOfLastSectionInRoute() const;                                    // @0x8276B710

        // ---- storage (declaration order == layout order; pinned by _AssertLayout below) -----
        // [0x0000 .. 0x140B] mRoute and the early transform/scalar block. Opaque to these bodies.
        u8 mPad0000[5132];

        Aggressiveness mAggressiveness;                 // +0x140C (5132)

        u8 mPad1424[144];                               // [0x1424 .. 0x14B3] vectors + mpDriver
        s32 meBehaviour;                                // +0x14B4 (5300) EAIBehaviour (==1 means INACTIVE-mode skip in CalcDesiredSpeed)
        u8 mPad14B8[4];                                 // +0x14B8 (mPreviousBehaviour)
        s32 meSpeedSelectionMethod;                     // +0x14BC (5308) EAISpeedSelectionMethod -- CalcDesiredSpeed switch
        ERouteFindingStyle meRouteFindingStyle;         // +0x14C0 (5312)
        s32 miRaceCarIndex;                             // +0x14C4 (5316) -- index packed into each route request
        EAICarState meCarState;                         // +0x14C8 (5320)
        u8 mPad14CC[8];                                 // +0x14CC (personality / reset-speed)
        ELocationRelativeToPlayer meRelativeLocation;   // +0x14D4 (5332)
        u8 mPad14D8[20];                                // +0x14D8 (asset key + behaviour/in-range/desired-speed scalars)
        f32 mfRouteTimer;                               // +0x14EC (5356) DWARF :687
        f32 mfDistanceToCheckpoint;                     // +0x14F0 (5360) DWARF :688
        f32 mfWrongWayTime;                             // +0x14F4 (5364) DWARF :689
        f32 mfDistanceAheadOfPlayer;                    // +0x14F8 (5368) DWARF :690
        f32 mfRaceTimer;                                // +0x14FC (5372) DWARF :691
        f32 mfAlternativeRouteTimer;                    // +0x1500 (5376) DWARF :692
        f32 mfMaxPlayerSpeed;                           // +0x1504 (5380) DWARF :693
        u8 mPad1508[12];                                // +0x1508 (dist-to-player / place-on-track / buzz)
        f32 mfTimeInInvalidSection;                     // +0x1514 (5396) DWARF :697
        u8 mPad1518[4];                                 // +0x1518 (mafScheduleOffsets[0])
        f32 mfScheduleOffset1;                          // +0x151C (5404) == mafScheduleOffsets[1] (DWARF :698)
        s32 miCurrentCheckpoint;                        // +0x1520 (5408) DWARF :700
        s32 miNextRouteNodeIndex;                       // +0x1524 (5412) DWARF :701
        s32 miRouteTimeStamp;                           // +0x1528 (5416) DWARF :702
        s32 miProximityIndex;                           // +0x152C (5420) DWARF :703
        u8 mPad1530[2];                                 // +0x1530 (muResetOnTrackSectionIndex)
        u16 muDefaultSectionIndex;                      // +0x1532 (5426) -- best-section fallback (muCurrentSectionIndex)
        u16 muBestSectionIndex;                         // +0x1534 (5428) -- 0x7FFF == invalid (muUnderCarSectionIndex)
        u16 muDestinationSectionIndex;                  // +0x1536 (5430) -- route end / free-roam target
        u8 mPad1538[2];                                 // +0x1538 (portal scratch)
        s8 miOpponentIndex;                             // +0x153A (5434) DWARF :714
        bool mbIsAheadOfPlayer;                         // +0x153B (5435) DWARF :715
        u8 mPad153C[1];                                 // +0x153C (place-on-track flag)
        u8 mbHasBlockCheckpoints;                       // +0x153D (5437) -- block current-checkpoint sections
        u8 mbUseAIShortcuts;                            // +0x153E (5438) -- request quality: allow AI shortcuts
        u8 mbWantsAlternativeRoute;                     // +0x153F (5439) -- race: take a different line
        u8 mPad1540[2];                                 // +0x1540 (alt-route scratch)
        bool mbIsCrashing;                              // +0x1542 (5442)
        u8 mPad1543[3];                                 // +0x1543 (showtime / drifting / touching-car)
        bool mbIsTouchingPlayer;                        // +0x1546 (5446)
        u8 mPad1547[1];                                 // +0x1547 (on-start-line flag)
        u8 mbForceStandardRoute;                        // +0x1548 (5448) -- override alternative with standard
        bool mbIsPlayer;                                // +0x1549 (5449)
        bool mbIsDrivenByPlayer;                        // +0x154A (5450)
        bool mbIsInGameMode;                            // +0x154B (5451) DWARF :731 -- UpdateRaceDistance schedule gate
        bool mbIsInShortcut;                            // +0x154C (5452) DWARF :732
        bool mbRouteRequested;                          // +0x154D (5453) DWARF :733 -- "a fresh route is needed"
        bool mbIsInMasterRoute;                         // +0x154E (5454) DWARF :734
        u8 mPad154F[1];                                 // +0x154F (mbIsInJunkYard)
        u8 mbUseChosenDistanceFunction;                 // +0x1550 (5456) -- run ChooseDistanceFunction

    private:
        // offsetof-on-private would need a member-function context, but every touched member is
        // public here, so the asserts live at namespace scope below. This function only documents
        // the contract; it is never called.
        static void _AssertLayout();
    };

    // Pin every touched offset -- the compile gate fails if the slice ever drifts.
    static_assert(offsetof(AICar, mAggressiveness)      == 0x140C, "AICar::mAggressiveness @ +0x140C");
    static_assert(offsetof(AICar, meBehaviour)          == 0x14B4, "AICar::meBehaviour @ +0x14B4");
    static_assert(offsetof(AICar, meSpeedSelectionMethod) == 0x14BC, "AICar::meSpeedSelectionMethod @ +0x14BC");
    static_assert(offsetof(AICar, meRouteFindingStyle)  == 0x14C0, "AICar::meRouteFindingStyle @ +0x14C0");
    static_assert(offsetof(AICar, miRaceCarIndex)       == 0x14C4, "AICar::miRaceCarIndex @ +0x14C4");
    static_assert(offsetof(AICar, meCarState)           == 0x14C8, "AICar::meCarState @ +0x14C8");
    static_assert(offsetof(AICar, meRelativeLocation)   == 0x14D4, "AICar::meRelativeLocation @ +0x14D4");
    static_assert(offsetof(AICar, mfRouteTimer)         == 0x14EC, "AICar::mfRouteTimer @ +0x14EC");
    static_assert(offsetof(AICar, mfDistanceToCheckpoint) == 0x14F0, "AICar::mfDistanceToCheckpoint @ +0x14F0");
    static_assert(offsetof(AICar, mfWrongWayTime)       == 0x14F4, "AICar::mfWrongWayTime @ +0x14F4");
    static_assert(offsetof(AICar, mfDistanceAheadOfPlayer) == 0x14F8, "AICar::mfDistanceAheadOfPlayer @ +0x14F8");
    static_assert(offsetof(AICar, mfRaceTimer)          == 0x14FC, "AICar::mfRaceTimer @ +0x14FC");
    static_assert(offsetof(AICar, mfAlternativeRouteTimer) == 0x1500, "AICar::mfAlternativeRouteTimer @ +0x1500");
    static_assert(offsetof(AICar, mfMaxPlayerSpeed)     == 0x1504, "AICar::mfMaxPlayerSpeed @ +0x1504");
    static_assert(offsetof(AICar, mfTimeInInvalidSection) == 0x1514, "AICar::mfTimeInInvalidSection @ +0x1514");
    static_assert(offsetof(AICar, mfScheduleOffset1)    == 0x151C, "AICar::mfScheduleOffset1 @ +0x151C");
    static_assert(offsetof(AICar, miCurrentCheckpoint)  == 0x1520, "AICar::miCurrentCheckpoint @ +0x1520");
    static_assert(offsetof(AICar, miNextRouteNodeIndex) == 0x1524, "AICar::miNextRouteNodeIndex @ +0x1524");
    static_assert(offsetof(AICar, miRouteTimeStamp)     == 0x1528, "AICar::miRouteTimeStamp @ +0x1528");
    static_assert(offsetof(AICar, miProximityIndex)     == 0x152C, "AICar::miProximityIndex @ +0x152C");
    static_assert(offsetof(AICar, muDefaultSectionIndex) == 0x1532, "AICar::muDefaultSectionIndex @ +0x1532");
    static_assert(offsetof(AICar, muBestSectionIndex)   == 0x1534, "AICar::muBestSectionIndex @ +0x1534");
    static_assert(offsetof(AICar, muDestinationSectionIndex) == 0x1536, "AICar::muDestinationSectionIndex @ +0x1536");
    static_assert(offsetof(AICar, miOpponentIndex)      == 0x153A, "AICar::miOpponentIndex @ +0x153A");
    static_assert(offsetof(AICar, mbIsAheadOfPlayer)    == 0x153B, "AICar::mbIsAheadOfPlayer @ +0x153B");
    static_assert(offsetof(AICar, mbHasBlockCheckpoints) == 0x153D, "AICar::mbHasBlockCheckpoints @ +0x153D");
    static_assert(offsetof(AICar, mbUseAIShortcuts)     == 0x153E, "AICar::mbUseAIShortcuts @ +0x153E");
    static_assert(offsetof(AICar, mbWantsAlternativeRoute) == 0x153F, "AICar::mbWantsAlternativeRoute @ +0x153F");
    static_assert(offsetof(AICar, mbIsCrashing)         == 0x1542, "AICar::mbIsCrashing @ +0x1542");
    static_assert(offsetof(AICar, mbIsTouchingPlayer)   == 0x1546, "AICar::mbIsTouchingPlayer @ +0x1546");
    static_assert(offsetof(AICar, mbForceStandardRoute) == 0x1548, "AICar::mbForceStandardRoute @ +0x1548");
    static_assert(offsetof(AICar, mbIsPlayer)           == 0x1549, "AICar::mbIsPlayer @ +0x1549");
    static_assert(offsetof(AICar, mbIsDrivenByPlayer)   == 0x154A, "AICar::mbIsDrivenByPlayer @ +0x154A");
    static_assert(offsetof(AICar, mbIsInGameMode)       == 0x154B, "AICar::mbIsInGameMode @ +0x154B");
    static_assert(offsetof(AICar, mbRouteRequested)     == 0x154D, "AICar::mbRouteRequested @ +0x154D");
    static_assert(offsetof(AICar, mbIsInMasterRoute)    == 0x154E, "AICar::mbIsInMasterRoute @ +0x154E");
    static_assert(offsetof(AICar, mbUseChosenDistanceFunction) == 0x1550, "AICar::mbUseChosenDistanceFunction @ +0x1550");
}
