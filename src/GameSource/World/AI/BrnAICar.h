#pragma once

// BrnAI::AICar -- the per-racer AI car state block (route / position / behaviour / per-frame
// flags) that the AI driver and the aggression state machine read while shadowing, overtaking
// and slamming a target car.
//
// MINIMAL-SLICE HOME. This header exists so BrnAIAggression.cpp can compile its 35 function
// bodies against the AICar members and accessors they touch, using NAMED access (never raw
// offset casts). It is NOT the full AICar layout -- the class is large (route data, nine
// Vector3 transforms, the section/portal indices, etc.); everything the aggression bodies do
// not touch is represented by explicit padding. When the real BrnAICar.cpp TU is reconstructed
// it OWNS this header and grows it (replacing the pads with the true member sequence); until
// then this slice must keep every TOUCHED member at its true X360 offset.
//
// OFFSET AUTHORITY is the X360 pseudocode/asm of BrnAIAggression.cpp (the DecFIGS DWARF gives
// the names/types but is the PS3 build and is NOT offset-authoritative). The anchor is
// mAggressiveness @ this+0x140C (5132): the aggression bodies pass (mpCar + 5132) to
// BrnAI::Aggressiveness::GetAggressionLevel, and read the four speed-match knobs at
// this+0x1414/0x1418/0x141C/0x1420 == mAggressiveness + 0x08/0x0C/0x10/0x14 -- exactly the
// committed BrnAIAggressiveness.h layout. From that anchor the DWARF member order reproduces
// every other touched offset on a coherent member; the discriminator at +0x14C0 is compared
// ==1/2/3/6, matching ERouteFindingStyle RACE/ROAD_RAGE/PURSUIT/MARKED_MAN. Every named offset
// below is pinned by a static_assert (the compile gate enforces them).
//
// VISIBILITY: the touched data members are exposed here (public) so the aggression bodies can
// read them by name; the real class marks most of them private and reaches them through the
// inlined-away getters (GetRouteFindingStyle/GetState/GetRelativePositionToPlayer/...). The
// slice keeps it simple -- a named field read is the faithful de-inlined form of the X360's
// direct `*(car+offset)` load. Names/types are from the DWARF (BrnAICar.h), gated on the X360
// ledger.

#include <cstddef>                 // offsetof

#include "types.hpp"               // f32, s32, u8, ...
#include "BrnCommonTypes.h"        // Vector3 (rw::math::vpu::Vector3, 16-byte SIMD)
#include "GameSource/World/AI/BrnAIAggressiveness.h"   // BrnAI::Aggressiveness (embedded by value)
#include "GameSource/World/AI/BrnAISharedConstants.h"  // BrnAI::ERouteFindingStyle, BrnAI::EAICarState

namespace BrnAI
{
    class AIDriver;   // pointer-only collaborator (the aggression bodies never dereference it)
    struct RouteNode; // pointer-only return of GetNextRouteNode (full type in BrnRoute.h)

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
        f32     GetDecentSpeed() const;       // X360 AICar::GetDecentSpeed     @0x82766030
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

        // X360 AICar::NeedsNewRoute @ (called from RouteRequestManager::Update): true when
        // this car's route is stale and a fresh request must be issued. Separate TU; declare-only.
        bool NeedsNewRoute() const;

        // ---- storage (declaration order == layout order; pinned by _AssertLayout below) -----
        // [0x0000 .. 0x140B] mRoute and the early transform/scalar block. Opaque to these bodies.
        u8 mPad0000[5132];

        Aggressiveness mAggressiveness;                 // +0x140C (5132)

        u8 mPad1424[156];                               // [0x1424 .. 0x14BF]
        ERouteFindingStyle meRouteFindingStyle;         // +0x14C0 (5312)
        s32 miRaceCarIndex;                             // +0x14C4 (5316) -- index packed into each route request
        EAICarState meCarState;                         // +0x14C8 (5320)
        u8 mPad14CC[8];                                 // +0x14CC (personality / reset-speed)
        ELocationRelativeToPlayer meRelativeLocation;   // +0x14D4 (5332)
        u8 mPad14D8[24];                                // +0x14D8 (asset key + behaviour timers)
        f32 mfDistanceToCheckpoint;                     // +0x14F0 (5360)
        u8 mPad14F4[16];                                // +0x14F4 (wrong-way / dist-ahead / race / alt-route timers)
        f32 mfMaxPlayerSpeed;                           // +0x1504 (5380) DWARF :693
        u8 mPad1508[20];                                // +0x1508 (dist-to-player / place-on-track / buzz / invalid-section / mafScheduleOffsets[0])
        f32 mfScheduleOffset1;                          // +0x151C (5404) == mafScheduleOffsets[1] (DWARF :698)
        s32 miCurrentCheckpoint;                        // +0x1520 (5408) DWARF :700
        s32 miNextRouteNodeIndex;                       // +0x1524 (5412) DWARF :701
        u8 mPad1528[4];                                 // +0x1528 (miRouteTimeStamp)
        s32 miProximityIndex;                           // +0x152C (5420)
        u8 mPad1530[2];                                 // +0x1530 (section index pair head)
        u16 muDefaultSectionIndex;                      // +0x1532 (5426) -- best-section fallback
        u16 muBestSectionIndex;                         // +0x1534 (5428) -- 0x7FFF == invalid
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
        u8 mPad154B[5];                                 // +0x154B (starting-race / misc flags)
        u8 mbUseChosenDistanceFunction;                 // +0x1550 (5456) -- run ChooseDistanceFunction

    private:
        // offsetof-on-private would need a member-function context, but every touched member is
        // public here, so the asserts live at namespace scope below. This function only documents
        // the contract; it is never called.
        static void _AssertLayout();
    };

    // Pin every touched offset -- the compile gate fails if the slice ever drifts.
    static_assert(offsetof(AICar, mAggressiveness)      == 0x140C, "AICar::mAggressiveness @ +0x140C");
    static_assert(offsetof(AICar, meRouteFindingStyle)  == 0x14C0, "AICar::meRouteFindingStyle @ +0x14C0");
    static_assert(offsetof(AICar, miRaceCarIndex)       == 0x14C4, "AICar::miRaceCarIndex @ +0x14C4");
    static_assert(offsetof(AICar, meCarState)           == 0x14C8, "AICar::meCarState @ +0x14C8");
    static_assert(offsetof(AICar, meRelativeLocation)   == 0x14D4, "AICar::meRelativeLocation @ +0x14D4");
    static_assert(offsetof(AICar, mfDistanceToCheckpoint) == 0x14F0, "AICar::mfDistanceToCheckpoint @ +0x14F0");
    static_assert(offsetof(AICar, mfMaxPlayerSpeed)     == 0x1504, "AICar::mfMaxPlayerSpeed @ +0x1504");
    static_assert(offsetof(AICar, mfScheduleOffset1)    == 0x151C, "AICar::mfScheduleOffset1 @ +0x151C");
    static_assert(offsetof(AICar, miCurrentCheckpoint)  == 0x1520, "AICar::miCurrentCheckpoint @ +0x1520");
    static_assert(offsetof(AICar, miNextRouteNodeIndex) == 0x1524, "AICar::miNextRouteNodeIndex @ +0x1524");
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
    static_assert(offsetof(AICar, mbUseChosenDistanceFunction) == 0x1550, "AICar::mbUseChosenDistanceFunction @ +0x1550");
}
