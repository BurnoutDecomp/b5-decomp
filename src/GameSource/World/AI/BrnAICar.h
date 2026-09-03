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
// GROWN by the AICar::Update wave (2026-09-03, BrnAICar_Update.cpp: Update @0x82798F68 + its
// callees + the module-facing writers UpdateInRangeData / UpdateOutOfRangeData / Reset /
// OnModeStart). Carved out of the pads, each pinned with its DWARF line (references/DecFIGS/
// dwarfdump/GameSource/World/AI/BrnAICar.h) and its X360 offset:
//   mPosition              @+0x1430 (5168)  :657  -- Update's distance-to-player / validity source
//   mDirection             @+0x1440 (5184)  :658  -- SetDirection @0x8276B2C0 stores here
//   mLastRoutePosition     @+0x1480 (5248)  :664  -- UpdateRoute snapshot; IsExtrapolatedRouteGettingOld
//   mPlaceOnTrackPosition  @+0x1490 (5264)  :666
//   mPlaceOnTrackDirection @+0x14A0 (5280)  :667
//   (guest mpDriver        @+0x14B0 (5296)  :670  -- 4-byte console slot kept as pad; the HOST
//                                                    pointer lives in the tail, see mpDriverHost)
//   mePersonalityType      @+0x14CC (5324)  :678
//   meResetSpeedType       @+0x14D0 (5328)  :679
//   mCarAssetAttribKey     @+0x14D8 (5336)  :682  -- SetDriver `ld r4,0x14D8`
//   mfPlaceOnTrackSpeed    @+0x150C (5388)  :695
//   mfBuzzFrequencyRatio   @+0x1510 (5392)  :696  -- UpdateOutOfRangeData writes min(1, speed/130)
//   mfScheduleOffset0      @+0x1518 (5400)  == mafScheduleOffsets[0] :698 (CalculateScheduleOffset out)
//   muResetOnTrackSectionIndex @+0x1530 (5424) :706
//   muResetOnTrackStartPortal  @+0x1538 (5432) :712
//   muResetOnTrackEndPortal    @+0x1539 (5433) :713
//   mbPlaceOnTrackRequested@+0x153C (5436)  :716
//   mbSectionChangedFlag   @+0x1540 (5440)  :720
//   mbIsInAir              @+0x1541 (5441)  :721  -- Update's last-good-position gate
//   mbIsInShowtime         @+0x1543 (5443)  :723
//   mbIsDrifting           @+0x1544 (5444)  :724
//   mbIsTouchingRaceCar    @+0x1545 (5445)  :725
//   mbIsInResetPairSection @+0x1551 (5457)  :737
//   mpDriverHost           @+0x1558 (host only; sizeof stays 0x1560 -- see the stride note)
// DWARF NAMES OF THE ALREADY-NAMED MEMBERS (kept as committed, other TUs read them by these
// names; the asm attests the ROLE, the DWARF the NAME):
//   mRight == mUnitRight :659 . mfSpeedInRange == mfActualInRangeSpeed :685 . mfSpeedOutOfRange
//   == mfDesiredSpeed :686 (Update stores CalcDesiredSpeed's result there; GetSpeed returns it
//   while OUT_OF_RANGE) . mfBuzzDistanceToPlayer == mfDistanceToPlayer :694 (Update writes
//   |mPosition - playerPos|) . muDefaultSectionIndex == muCurrentSectionIndex :707 .
//   muBestSectionIndex == muUnderCarSectionIndex :708 . muDestinationSectionIndex ==
//   muDestinationAISectionIndex :709 . mbHasBlockCheckpoints == mbUseBlockSections :717 .
//   mbUseAIShortcuts == mbCanUseAIShortcuts :718 . mbWantsAlternativeRoute ==
//   mbCanDeviateFromRoute :719 . mbForceStandardRoute == mbIsStartingRace :728 (Update zeroes
//   mfRaceTimer while it is set; OnModeStart sets it, OnModeStartRacing clears it) .
//   mbUseChosenDistanceFunction == mbIsOnline :736 (Update stores its lbIsOnline argument there
//   and skips CalcDesiredSpeed/CalculateScheduleOffset while it is set).
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
#include "GameSource/World/AI/BrnAISharedConstants.h"  // BrnAI::ERouteFindingStyle, BrnAI::EAICarState, EAISpeedSelectionMethod, EPersonalityType
#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISectionsData (full type: Update's callees index its sections), EResetSpeedType

namespace BrnAI
{
    struct AIDriver;                 // pointer-only collaborator (the aggression bodies never dereference it)   // (struct: matches BrnAIDriver.h:137 -- the class-key is part of the MSVC mangling)
    struct RouteNode;               // pointer-only return of GetNextRouteNode (full type in BrnRoute.h)
    struct Route;                   // mRoute home (full type in BrnRoute.h)
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

        // ---- the AICar leaf accessors/setters bodied in BrnAICar.cpp (this UNIT) -------------
        // The car's last on-track position (validity-asserted). X360 AICar::GetLastGoodPosition
        // @0x8276B640 -- a sret-returned copy of mLastGoodPosition.
        Vector3 GetLastGoodPosition() const;            // @0x8276B640

        // Store the car's per-frame behaviour, shifting the old one into mePreviousBehaviour and
        // resetting the behaviour timer. leBehaviour must be in [0, E_AI_BEHAVIOUR_COUNT).
        void SetBehaviour(EAIBehaviour leBehaviour);    // @0x82764DE0

        // Set whether this car is currently inside a junkyard (logs the transition).
        void SetIsInJunkyard(bool lbInJunkyard);        // @0x82764EC0

        // Store the car's world-space right vector (validity- and unit-length-asserted).
        void SetRight(Vector3 lRight);                  // @0x8276B7D0

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
        f32              GetBuzzFrequencyRatio() const { return mfBuzzFrequencyRatio; }   // mfBuzzFrequencyRatio @+0x1510 (DWARF :696); BuzzBy::ResetActiveList @0x82771C90 reads it (`lfs 0x1510(car)`)

        // ---- accessors the RaceBalancingDebugComponent (debug HUD) reads (its own TU) ----------
        // The car's embedded route (mRoute is at AICar+0; the X360 passes the AICar pointer straight
        // to Route::GetNode/GetNodeCount). Restored inline so the debug HUD reaches it by name.
        const Route* GetRoute() const { return reinterpret_cast<const Route*>(this); }
        // ---- ADDITIVE (aiwave lane A7, 2026-09-03): the NON-const twin. Five AIModule event
        //      handlers INVALIDATE a car's route in place -- `stw 0, 0x1408(car)` (Route::meStatus)
        //      + `stw 0, 0x1400(car)` (Route::miNodeCount) -- and the console reaches them exactly
        //      as this does, through the AICar pointer (mRoute is at AICar+0):
        //        HandleManagementEvents REMOVE_CAR_FROM_MODE  @0x82798B94/0x82798B98
        //        HandleGameActions      SET_PLAYER_CAR_DRIVER @0x827925D0/0x827925D8 and
        //                                                      0x827925EC/0x827925F0
        //        OnModeEnd              @0x8277BB2C/0x8277BB30      (via the same two words)
        //        OnModeFinished         @0x8277B9F8/0x8277BA04
        //        OnRaceCarReachedFinish @0x8277B90C/0x8277B910
        //      Same body, same cast, same reason as the const overload above.
        Route* GetRoute() { return reinterpret_cast<Route*>(this); }
        // Distance the car is ahead of the player along the route (== mfDistanceAheadOfPlayer
        // @+0x14F8). The debug HUD prints it and uses the 80m threshold to label the active graph.
        f32 GetDistanceAheadOfPlayer() const { return mfDistanceAheadOfPlayer; }
        // Speed-matching read-out for the debug HUD. The X360 reads a sub-controller pointer at
        // AICar+0x14B0 (only meaningful while IN_RANGE) and, when its "is speed matching" flag is
        // set (+0x1D66 on the pointee), prints its matched speed (+0x1D18). Exposed as named
        // accessors here; the bodies (which perform the +0x14B0 deref) live in the AICar TU.
        // UNCERTAIN: the pointee type and the two field offsets are recovered from the debug-HUD asm
        // only (not cross-checked against the owning class); see open questions.
        bool IsSpeedMatching() const;                         // X360: car+0x14B0 -> [+0x1D66] (IN_RANGE only)
        f32  GetSpeedMatchSpeed() const;                      // X360: car+0x14B0 -> [+0x1D18]

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

        // ---- the per-car AI brain tick + the module-facing writers (BrnAICar_Update.cpp) -----
        // DWARF BrnAICar.h:95. X360 @0x82798F68: Update(this=r3, r4=RBM, f1=dt, r5=<float skip>,
        // r6=player, r7=sections, r8=route, r9=lbIsOnline). See the partfile for the callee order.
        void Update(const RaceBalancingManager* lpRaceBalancingManager, f32 lfTimeStep,
                    const AICar* lpPlayerCar, AISectionsData* lpAISectionsData,
                    const Route* lpRoute, bool lbIsOnline);                                  // @0x82798F68
        void Reset(EPersonalityType lePersonalityType, bool lbKeepTransform);               // @0x82792800 (DWARF :100)
        void UpdateShortcut(AISectionsData* lpAISectionsData);                              // @0x82765D70 (DWARF :104)
        void UpdatePositionOutOfRange(f32 lfTimeStep, AISectionsData* lpAISectionsData);    // @0x8276F060 (DWARF :202)
        // DWARF :225. X360 @0x82792A18: r4=sections, r5=&transform, v1=velocity, f1=speed,
        // r7=under-car section (r6 is the float skip slot), r8..r10 + 6 stack slots = the bools
        // in this order (asm store order 0x1541/0x1542/0x1543/0x1547/0x1549/0x154A/0x1544/0x1545/0x1546).
        void UpdateInRangeData(AISectionsData* lpAISectionsData, const Matrix44Affine& lrTransform,
                               Vector3 lVelocity, f32 lfSpeed, u16 luUnderCarSectionIndex,
                               bool lbIsInAir, bool lbIsCrashing, bool lbIsInShowtime,
                               bool lbIsOnStartLine, bool lbIsPlayer, bool lbIsDrivenByPlayer,
                               bool lbIsDrifting, bool lbIsTouchingRaceCar, bool lbIsTouchingPlayer);
        void UpdateResetOnTrackSection(EResetSpeedType leResetSpeedType, u16 luResetOnTrackSectionIndex,
                                       u8 luStartPortal, u8 luEndPortal);                  // @0x82765EA0 (DWARF :232)
        void UpdateOutOfRangeData(Vector3 lPosition, Vector3 lAtVector, u16 luSectionIndex,
                                  u8 luSectionSpeed);                                       // @0x8276F398 (DWARF :240)
        void SetDriver(AIDriver* lpDriver);                                                 // @0x8276EF20 (DWARF :125)
        AIDriver* GetDriver() const { return mpDriverHost; }                                // DWARF :131 (inlined-away getter)
        void SetDirection(Vector3 lDirection);                                              // @0x8276B2C0 (DWARF :532)
        f32  GetCurrentNodeY(AISectionsData* lpAISectionsData);                             // @0x8276EFD8 (DWARF :379)
        void OnModeStart(EAISpeedSelectionMethod leSpeedSelectionMethod, s32 liOpponentIndex,
                         ERouteFindingStyle leRouteFindingStyle, bool lbCanDeviateFromRoute,
                         bool lbCanUseAIShortcuts, u16 luDestinationSectionIndex,
                         f32 lfProgressionRankAsRatio);                                    // @0x8277BD20 (DWARF :398)
        void OnModeStartRacing();                                                           // @0x82765C50 (DWARF :401)
        void SetRoadRageMadness(f32 lfMadness);                                             // @0x8276ECB0 (DWARF :556)
        bool IsFreeRoamingCarsRouteOld();                                                   // @0x8276FBD0 (DWARF :631)
        void CheckForFreeRoamSwapToPursuit();                                               // @0x8276EE68 (DWARF :646)
        bool IsInAir() const      { return mbIsInAir; }                                     // DWARF :299
        bool IsInShowtime() const { return mbIsInShowtime; }                                // DWARF :305
        f32  GetDesiredSpeed() const { return mfSpeedOutOfRange; }                          // DWARF :284 (== mfDesiredSpeed @+0x14E8)
        f32  GetDistanceToPlayer() const { return mfBuzzDistanceToPlayer; }                 // DWARF :290 (== mfDistanceToPlayer @+0x1508)

        // ---- storage (declaration order == layout order; pinned by _AssertLayout below) -----
        // [0x0000 .. 0x140B] mRoute and the early transform/scalar block. Opaque to these bodies.
        u8 mPad0000[5132];

        Aggressiveness mAggressiveness;                 // +0x140C (5132)

        u8 mPad1424[12];                                // [0x1424 .. 0x142F] the console Aggressiveness tail (0x24 on X360 vs 0x18 host)
        Vector3 mPosition;                              // +0x1430 (5168) DWARF :657 -- GetPosition @0x8276B1F0 / Update / UpdateInRangeData (row 3 of the transform)
        Vector3 mDirection;                             // +0x1440 (5184) DWARF :658 -- GetDirection @0x8276B488 / SetDirection @0x8276B2C0
        Vector3 mRight;                                 // +0x1450 (5200) car world-space right vector (SetRight @0x8276B7D0 stores here; DWARF mUnitRight :659)
        Vector3 mLastGoodPosition;                      // +0x1460 (5216) last on-track position (GetLastGoodPosition @0x8276B640)
        Vector3 mVelocity;                              // +0x1470 (5232) world-space velocity (GetVelocity @0x8276B570)
        Vector3 mLastRoutePosition;                     // +0x1480 (5248) DWARF :664 -- UpdateRoute snapshots mPosition here; route-age tests measure from it
        Vector3 mPlaceOnTrackPosition;                  // +0x1490 (5264) DWARF :666
        Vector3 mPlaceOnTrackDirection;                 // +0x14A0 (5280) DWARF :667
        u8 mPad14B0[4];                                 // +0x14B0 (5296) guest mpDriver :670 (4-byte console slot) -- the host pointer is mpDriverHost @+0x1558
        EAIBehaviour meBehaviour;                       // +0x14B4 (5300) (==1 ROLLING_START -> INACTIVE-mode skip in CalcDesiredSpeed)
        EAIBehaviour mePreviousBehaviour;               // +0x14B8 (5304) prior behaviour (SetBehaviour shifts meBehaviour into here)
        s32 meSpeedSelectionMethod;                     // +0x14BC (5308) EAISpeedSelectionMethod -- CalcDesiredSpeed switch
        ERouteFindingStyle meRouteFindingStyle;         // +0x14C0 (5312)
        s32 miRaceCarIndex;                             // +0x14C4 (5316) -- index packed into each route request
        EAICarState meCarState;                         // +0x14C8 (5320)
        EPersonalityType mePersonalityType;             // +0x14CC (5324) DWARF :678 -- Reset stores its argument; indexes the base-aggression table
        EResetSpeedType meResetSpeedType;               // +0x14D0 (5328) DWARF :679 -- UpdateResetOnTrackSection stores; Reset -> E_RESET_SPEED_TYPE_COUNT (20)
        ELocationRelativeToPlayer meRelativeLocation;   // +0x14D4 (5332)
        u64 mCarAssetAttribKey;                         // +0x14D8 (5336) DWARF :682 (Attribute::Key, 64-bit) -- SetDriver `ld r4,0x14D8` -> burnoutcarasset key
        f32 mfBehaviourTimer;                           // +0x14E0 (5344) per-behaviour timer (SetBehaviour @0x82764DE0 resets it to 0)
        f32 mfSpeedInRange;                             // +0x14E4 (5348) cached speed while IN_RANGE     (GetSpeed @0x82764D68)
        f32 mfSpeedOutOfRange;                          // +0x14E8 (5352) cached speed while OUT_OF_RANGE  (GetSpeed @0x82764D68)
        f32 mfRouteTimer;                               // +0x14EC (5356) DWARF :687
        f32 mfDistanceToCheckpoint;                     // +0x14F0 (5360) DWARF :688
        f32 mfWrongWayTime;                             // +0x14F4 (5364) DWARF :689
        f32 mfDistanceAheadOfPlayer;                    // +0x14F8 (5368) DWARF :690
        f32 mfRaceTimer;                                // +0x14FC (5372) DWARF :691
        f32 mfAlternativeRouteTimer;                    // +0x1500 (5376) DWARF :692
        f32 mfMaxPlayerSpeed;                           // +0x1504 (5380) DWARF :693
        f32 mfBuzzDistanceToPlayer;                     // +0x1508 (5384) DWARF mfDistanceToPlayer :694 -- Update writes |mPosition - playerPos| (BuzzBy AICarCanBuzz >=200 / BuzzOccured <=30)
        f32 mfPlaceOnTrackSpeed;                        // +0x150C (5388) DWARF :695
        f32 mfBuzzFrequencyRatio;                       // +0x1510 (5392) DWARF :696 -- UpdateOutOfRangeData: min(1, sectionSpeed/130)
        f32 mfTimeInInvalidSection;                     // +0x1514 (5396) DWARF :697
        f32 mfScheduleOffset0;                          // +0x1518 (5400) == mafScheduleOffsets[0] (DWARF :698) -- Update passes &mafScheduleOffsets[0] to CalculateScheduleOffset
        f32 mfScheduleOffset1;                          // +0x151C (5404) == mafScheduleOffsets[1] (DWARF :698)
        s32 miCurrentCheckpoint;                        // +0x1520 (5408) DWARF :700
        s32 miNextRouteNodeIndex;                       // +0x1524 (5412) DWARF :701
        s32 miRouteTimeStamp;                           // +0x1528 (5416) DWARF :702
        s32 miProximityIndex;                           // +0x152C (5420) DWARF :703
        u16 muResetOnTrackSectionIndex;                 // +0x1530 (5424) DWARF :706 -- UpdateResetOnTrackSection stores; Reset -> 0x7FFF
        u16 muDefaultSectionIndex;                      // +0x1532 (5426) -- best-section fallback (muCurrentSectionIndex)
        u16 muBestSectionIndex;                         // +0x1534 (5428) -- 0x7FFF == invalid (muUnderCarSectionIndex)
        u16 muDestinationSectionIndex;                  // +0x1536 (5430) -- route end / free-roam target
        u8 muResetOnTrackStartPortal;                   // +0x1538 (5432) DWARF :712 -- Reset -> 1
        u8 muResetOnTrackEndPortal;                     // +0x1539 (5433) DWARF :713
        s8 miOpponentIndex;                             // +0x153A (5434) DWARF :714
        bool mbIsAheadOfPlayer;                         // +0x153B (5435) DWARF :715
        bool mbPlaceOnTrackRequested;                   // +0x153C (5436) DWARF :716 -- Reset clears
        u8 mbHasBlockCheckpoints;                       // +0x153D (5437) -- block current-checkpoint sections
        u8 mbUseAIShortcuts;                            // +0x153E (5438) -- request quality: allow AI shortcuts
        u8 mbWantsAlternativeRoute;                     // +0x153F (5439) -- race: take a different line
        bool mbSectionChangedFlag;                      // +0x1540 (5440) DWARF :720
        bool mbIsInAir;                                 // +0x1541 (5441) DWARF :721 -- UpdateInRangeData stores; Update's last-good-position gate
        bool mbIsCrashing;                              // +0x1542 (5442)
        bool mbIsInShowtime;                            // +0x1543 (5443) DWARF :723
        bool mbIsDrifting;                              // +0x1544 (5444) DWARF :724
        bool mbIsTouchingRaceCar;                       // +0x1545 (5445) DWARF :725
        bool mbIsTouchingPlayer;                        // +0x1546 (5446)
        bool mbIsOnStartLine;                           // +0x1547 (5447) IsOnStartLine @0x82764E68 returns this
        u8 mbForceStandardRoute;                        // +0x1548 (5448) -- override alternative with standard
        bool mbIsPlayer;                                // +0x1549 (5449)
        bool mbIsDrivenByPlayer;                        // +0x154A (5450)
        bool mbIsInGameMode;                            // +0x154B (5451) DWARF :731 -- UpdateRaceDistance schedule gate
        bool mbIsInShortcut;                            // +0x154C (5452) DWARF :732
        bool mbRouteRequested;                          // +0x154D (5453) DWARF :733 -- "a fresh route is needed"
        bool mbIsInMasterRoute;                         // +0x154E (5454) DWARF :734
        bool mbIsInJunkYard;                            // +0x154F (5455) SetIsInJunkyard @0x82764EC0 stores here
        u8 mbUseChosenDistanceFunction;                 // +0x1550 (5456) DWARF mbIsOnline :736 -- Update stores its lbIsOnline argument here
        bool mbIsInResetPairSection;                    // +0x1551 (5457) DWARF :737 (the console's last member; object tail-padded to 0x1560)
        u8 mPad1552[6];                                 // [0x1552 .. 0x1557] console tail padding, now the host pointer's alignment gap
        // HOST-ONLY pointer slot: the console's 4-byte mpDriver @+0x14B0 cannot hold an 8-byte
        // host pointer without growing the object past the baked 5472 stride (see the stride note
        // below), so -- exactly like AIDriver::mpCarHost -- the real pointer lives in the console's
        // tail padding. SetDriver @0x8276EF20 stores it, GetDriver reads it; UpdateInRangeData
        // dereferences it (AIDriver::ResetPIDTuningState). Nothing on the console reads the
        // padding bytes, so this is invisible to every console-derived stride.
        AIDriver* mpDriverHost;                         // +0x1558 (host only) == guest mpDriver @+0x14B0 :670

    private:
        // offsetof-on-private would need a member-function context, but every touched member is
        // public here, so the asserts live at namespace scope below. This function only documents
        // the contract; it is never called.
        static void _AssertLayout();
    };

    // Pin every touched offset -- the compile gate fails if the slice ever drifts.
    static_assert(offsetof(AICar, mAggressiveness)      == 0x140C, "AICar::mAggressiveness @ +0x140C");
    static_assert(offsetof(AICar, mRight)               == 0x1450, "AICar::mRight @ +0x1450");
    static_assert(offsetof(AICar, mLastGoodPosition)    == 0x1460, "AICar::mLastGoodPosition @ +0x1460");
    static_assert(offsetof(AICar, mVelocity)            == 0x1470, "AICar::mVelocity @ +0x1470");
    static_assert(offsetof(AICar, meBehaviour)          == 0x14B4, "AICar::meBehaviour @ +0x14B4");
    static_assert(offsetof(AICar, mePreviousBehaviour)  == 0x14B8, "AICar::mePreviousBehaviour @ +0x14B8");
    static_assert(offsetof(AICar, mfBehaviourTimer)     == 0x14E0, "AICar::mfBehaviourTimer @ +0x14E0");
    static_assert(offsetof(AICar, mfSpeedInRange)       == 0x14E4, "AICar::mfSpeedInRange @ +0x14E4");
    static_assert(offsetof(AICar, mfSpeedOutOfRange)    == 0x14E8, "AICar::mfSpeedOutOfRange @ +0x14E8");
    static_assert(offsetof(AICar, mbIsOnStartLine)      == 0x1547, "AICar::mbIsOnStartLine @ +0x1547");
    static_assert(offsetof(AICar, mbIsInJunkYard)       == 0x154F, "AICar::mbIsInJunkYard @ +0x154F");
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
    // -- AICar::Update wave (2026-09-03) carve pins --
    static_assert(offsetof(AICar, mPosition)            == 0x1430, "AICar::mPosition @ +0x1430");
    static_assert(offsetof(AICar, mDirection)           == 0x1440, "AICar::mDirection @ +0x1440");
    static_assert(offsetof(AICar, mLastRoutePosition)   == 0x1480, "AICar::mLastRoutePosition @ +0x1480");
    static_assert(offsetof(AICar, mPlaceOnTrackPosition) == 0x1490, "AICar::mPlaceOnTrackPosition @ +0x1490");
    static_assert(offsetof(AICar, mPlaceOnTrackDirection) == 0x14A0, "AICar::mPlaceOnTrackDirection @ +0x14A0");
    static_assert(offsetof(AICar, mePersonalityType)    == 0x14CC, "AICar::mePersonalityType @ +0x14CC");
    static_assert(offsetof(AICar, meResetSpeedType)     == 0x14D0, "AICar::meResetSpeedType @ +0x14D0");
    static_assert(offsetof(AICar, mCarAssetAttribKey)   == 0x14D8, "AICar::mCarAssetAttribKey @ +0x14D8");
    static_assert(offsetof(AICar, mfBuzzDistanceToPlayer) == 0x1508, "AICar::mfBuzzDistanceToPlayer @ +0x1508");
    static_assert(offsetof(AICar, mfPlaceOnTrackSpeed)  == 0x150C, "AICar::mfPlaceOnTrackSpeed @ +0x150C");
    static_assert(offsetof(AICar, mfBuzzFrequencyRatio) == 0x1510, "AICar::mfBuzzFrequencyRatio @ +0x1510");
    static_assert(offsetof(AICar, mfScheduleOffset0)    == 0x1518, "AICar::mfScheduleOffset0 @ +0x1518");
    static_assert(offsetof(AICar, muResetOnTrackSectionIndex) == 0x1530, "AICar::muResetOnTrackSectionIndex @ +0x1530");
    static_assert(offsetof(AICar, muResetOnTrackStartPortal) == 0x1538, "AICar::muResetOnTrackStartPortal @ +0x1538");
    static_assert(offsetof(AICar, muResetOnTrackEndPortal) == 0x1539, "AICar::muResetOnTrackEndPortal @ +0x1539");
    static_assert(offsetof(AICar, mbPlaceOnTrackRequested) == 0x153C, "AICar::mbPlaceOnTrackRequested @ +0x153C");
    static_assert(offsetof(AICar, mbSectionChangedFlag) == 0x1540, "AICar::mbSectionChangedFlag @ +0x1540");
    static_assert(offsetof(AICar, mbIsInAir)            == 0x1541, "AICar::mbIsInAir @ +0x1541");
    static_assert(offsetof(AICar, mbIsInShowtime)       == 0x1543, "AICar::mbIsInShowtime @ +0x1543");
    static_assert(offsetof(AICar, mbIsDrifting)         == 0x1544, "AICar::mbIsDrifting @ +0x1544");
    static_assert(offsetof(AICar, mbIsTouchingRaceCar)  == 0x1545, "AICar::mbIsTouchingRaceCar @ +0x1545");
    static_assert(offsetof(AICar, mbIsInResetPairSection) == 0x1551, "AICar::mbIsInResetPairSection @ +0x1551");
    static_assert(offsetof(AICar, mpDriverHost)         == 0x1558, "AICar::mpDriverHost @ +0x1558 (host-only tail slot)");

    // ⭐⭐ THE WHOLE-OBJECT STRIDE, MEASURED 2026-08-26 (aimodule wave) AND NOW PINNED.
    // Every offset above pins a member; NONE of them pins the SIZE, and the size is what the
    // console indexes this class by. BrnAI::AIModule::GetAICar @0x82765AD0 returns
    //     module + 560 + 5472 * index
    // and BrnAI::ResetOnTrackManager::GetAICar @0x82765878 does the same off its own array
    // pointer -- both with the literal 5472 baked in. That arithmetic is only correct on this
    // host if sizeof(AICar) really is 5472, and it IS: measured 5472 == 0x1560 exactly (the
    // last member ends at 0x1551 and Vector3's 16-byte alignment rounds the object to 0x1560).
    // ⛔ IT IS TRUE BY ACCIDENT OF TODAY'S PAD MODEL, NOT BY CONSTRUCTION. This class is an
    // explicitly-padded reproduction of a 32-bit layout; the moment a grow carves a POINTER out
    // of one of the pads, the host object grows past 0x1560 and every one of those baked strides
    // silently starts walking the array at the wrong pitch -- silently, because every address it
    // produces is still inside the allocation. [[serialized slots stay 32-bit]] in reverse.
    // This assert is the tripwire: if it ever fires, the fix is to replace the console constants
    // with `&mpaAICars[index]` on the host type, NOT to adjust the padding to hide it.
    // 2026-09-03 (AICar::Update wave): the ONE pointer the class needs on the host (mpDriver) was
    // placed in the console's 14-byte tail padding (mpDriverHost @+0x1558) precisely so this
    // assert keeps holding without touching a single console offset.
    static_assert(sizeof(AICar) == 0x1560, "AICar stride: the console bakes 5472 into GetAICar");
}
