// BrnAI::RaceBalancingManager -- the five standalone X360 symbols of the race
// rubber-band controller. The manager keeps, per opponent slot, a speed-ratio curve
// (RaceBalancingGraph) and a par-time model (RaceBalancingRoute); from those it derives
// how fast an AI opponent should drive so the field stays competitive.
//
//   OnRaceStart                  @0x82789AF8
//   OnOpponentReachedCheckpoint  @0x82789D88
//   ComputeParSpeed (private)    @0x82789EC0
//   ComputeTargetSpeed (private) @0x827916E0
//   CalculateScheduleOffset      @0x82789E00
//   UpdateOpponentRoute          @0x82789C48  (export hole; ppcdis)          -- 2026-09-22
//   Update                       (inlined in AIModule::Update 0x8279B678..)  -- 2026-09-22
//
// The baked d:\p4 assert file/line are dropped in favour of __FILE__/__LINE__ by CGS_ASSERT; the
// assert MESSAGE strings are verbatim from the asm.

#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"
#include "GameSource/World/BrnWorldSharedConstants.h"   // BrnWorld::KI_MAX_RIVALS_IN_MODE (UpdateOpponentRoute's :185 assert)

#include <cstddef>                                   // offsetof (layout pins)
#include <cmath>                                     // std::fmaf (ComputeTargetSpeed's fmadds)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] BRN_RACEBAL_DIAG witness
#include <cstdlib>                                           // [DIAG] getenv
#include "GameSource/World/AI/BrnAICar.h"            // BrnAI::AICar + its accessors
#include "GameSource/World/AI/BrnAISharedConstants.h"// EAICarState
#include "GameSource/World/AI/Route/BrnRoute.h"      // BrnAI::RouteNode (GetNextRouteNode deref)
#include "SharedClasses/AI/AISectionsResourceType.h" // BrnAI::AISection / AISectionsData

namespace BrnAI
{
// ===========================================================================================
// OnRaceStart @0x82789AF8
// Clears and (re)builds the per-opponent graph/route tables at the start of a race:
//   - copies the caller's RaceBalancingGraph array in (Construct + AppendArray<7>);
//   - sizes the route table to the same opponent count and resets each route to "empty,
//     invalid", seeding its per-takedown time penalty (20s when lbHighTakenDownPenalty,
//     else 5s -- the X360 chooses this from the r6 argument);
//   - zeroes the race clock, stores the checkpoint count, and raises mbInRace/mbOnStartLine.
// ===========================================================================================
void RaceBalancingManager::OnRaceStart(const Array<RaceBalancingGraph, 7u>* lpRaceBalancingGraphArray,
                                       s32 liCheckpointCount, bool lbHighTakenDownPenalty)
{
    CGS_ASSERT(lpRaceBalancingGraphArray != NULL, "lpRaceBalancingGraphArray != NULL");

    // Copy the incoming graph table in (the X360 stores 0 into maRaceBalancingGraphs.miCount
    // first == Construct/Clear, then AppendArray<7> appends every live source graph).
    maRaceBalancingGraphs.Construct();
    maRaceBalancingGraphs.AppendArray(*lpRaceBalancingGraphArray);

    // One route per graph (opponent). The X360 sets the route table's live count to the graph
    // count up-front then fills slots 0..count-1 in place; appending each freshly-reset route
    // (AddNew == reserve next slot, count++) reaches the identical end state via the public API.
    maRaceBalancingRoutes.Construct();
    const u32 luOpponentCount = maRaceBalancingGraphs.GetLength();
    for (u32 luOpponent = 0; luOpponent < luOpponentCount; ++luOpponent)
    {
        RaceBalancingRoute* lpRoute = maRaceBalancingRoutes.AddNew();

        // Inlined per-route reset (the X360 emits these six stores directly; manager is a
        // friend of RaceBalancingRoute so the de-inlined named writes are faithful).
        lpRoute->miTimeCount              = 0;            // +0xA00
        lpRoute->miTakenDownCount         = 0;            // +0xA04
        lpRoute->miCurrentCheckpointIndex = 0;            // +0xA08
        lpRoute->mfDistance               = 0.0f;         // +0xA0C
        lpRoute->mbValid                  = false;        // +0xA14
        // Per-takedown seconds penalty: 20.0s (0x41A00000) when the high-penalty flag is set,
        // else 5.0s (0x40A00000). Literals visible in the X360 pseudocode (not fabricated).
        lpRoute->mfTakenDownTimePenalty   = lbHighTakenDownPenalty ? 20.0f : 5.0f; // +0xA10
    }

    mfRaceTime        = 0.0f;             // +0x4870
    miCheckpointCount = liCheckpointCount;// +0x4874
    mbInRace          = true;             // +0x4878
    mbOnStartLine     = true;             // +0x4879
}

// ===========================================================================================
// OnRaceStartPlaying -- no standalone console symbol; the console inlines it at its one caller.
// AIModule::OnModeStartRacing is that caller and is the whole body: a single
// `stbx r26(0), r31, 0x42249` == module + 0x42249 == &mRaceBalancingManager + 0x4879 ==
// mbOnStartLine. The grid has released, so the balancer stops treating the field as stationary
// and starts accruing mfRaceTime (OnRaceStart raises this flag; this is its only clear).
// ===========================================================================================
void RaceBalancingManager::OnRaceStartPlaying()
{
    mbOnStartLine = false;                // +0x4879
}

// ===========================================================================================
// OnRaceEnd -- likewise inlined at its one caller. AIModule::OnModeEnd
// is four stores off one base register (`addis r11,r30,4 ; addi r11,r11,-0x2630` == module +
// 0x3D9D0 == this): the in-race flag down, both per-opponent tables emptied and the checkpoint
// count cleared. Everything the balancer needs is rebuilt by the next OnRaceStart, so the race
// clock (+0x4870) is deliberately left where it stopped -- the console does not clear it here.
// ===========================================================================================
void RaceBalancingManager::OnRaceEnd()
{
    mbInRace          = false;            // +0x4878
    maRaceBalancingGraphs.Clear();        // +0x01C0 count -> 0
    maRaceBalancingRoutes.Clear();        // +0x486C count -> 0
    miCheckpointCount = 0;                // +0x4874
}

// ===========================================================================================
// Update -- DWARF BrnRaceBalancingManager.cpp:149. The X360 has no standalone symbol: its one
// caller, AIModule::Update, inlines the whole body at 0x8279B678..0x8279B6C0 off
// `addis r11,r31,4 ; addi r11,r11,-0x2630` == module + 0x3D9D0 == this (the PS3 DecFIGS keeps it
// out of line at 0x9B4DF8, the identical body):
//     0x8279B680  lbz 0x4878 (mbInRace)      beq -> skip
//     0x8279B68C  lbz 0x4879 (mbOnStartLine) bne -> skip
//     0x8279B698  lbz 0x1542(player)         (AICar::mbIsCrashing)
//     crashing:   0x8279B6B0 fmadds f0 = dt * flt_820C4168 (0.5) + [0x4870]
//     otherwise:  0x8279B6BC fadds  f0 = dt + [0x4870]
//     0x8279B6C0  stfs f0 -> +0x4870 (mfRaceTime)
// THE RACE CLOCK. Until 2026-09-22 (crash parity G05-D4) nothing on PC advanced it: the block
// was misfiled as an AIDebugComponent accumulator and dropped, so every par-time comparison
// (ComputeTargetSpeed / CalculateScheduleOffset) ran against a clock frozen at OnRaceStart's 0.
// ===========================================================================================
void RaceBalancingManager::Update(const AICar* lpPlayerCar, f32 lfTimeStep)
{
    if (mbInRace && !mbOnStartLine)
    {
        // `lbz 0x1542(r3)` @0x8279B698 on AIModule::GetAICar(mePlayerGlobalRaceCarIndex), no null
        // test -- the console's GetAICar never returns null. Nor can the PC's here: its only
        // null is the out-of-range bail, and mePlayerGlobalRaceCarIndex is never out of range
        // (Construct seeds 0; row 11 stores the active player driver's car slot, and an active
        // driver always has a car -- AIDriver::SetAICar is the only writer of mbIsActive = 1 and
        // binds the car in the same call). The host-only [GUARD] that stood here is removed
        // (crash parity FX-NANPOL, 2026-09-24; review A on dce59f43).
        if (lpPlayerCar->IsCrashing())
        {
            mfRaceTime = lfTimeStep * KF_PLAYER_CRASHING_TIME_FACTOR + mfRaceTime;
        }
        else
        {
            mfRaceTime = lfTimeStep + mfRaceTime;
        }

        // [DIAG] NOT IN THE X360 BINARY (BRN_RACEBAL_DIAG=1): ONE line, the first frame the clock
        // runs -- the live dispatch witness for the unguarded dereference above
        // (tests/FxNanpolGuardsLive.ps1). DELETE-WHEN that case is banked.
        static const bool sbClockWitness = (getenv("BRN_RACEBAL_DIAG") != 0);
        static bool sbClockWitnessed = false;
        if (sbClockWitness && !sbClockWitnessed && CgsDev::Log::gpDebugPrint != 0)
        {
            sbClockWitnessed = true;
            *CgsDev::Log::gpDebugPrint
                << (lpPlayerCar->IsCrashing() ? "[racebal] race clock running (player crashing)\n"
                                              : "[racebal] race clock running (player not crashing)\n");
        }
    }
}

// ===========================================================================================
// UpdateOpponentRoute @0x82789C48 -- an ARTIST export HOLE (no JSON); read with
// tools/re/ppcdis.py 82789C48 0x50 (0x82789C48..0x82789D80). DWARF BrnRaceBalancingManager.cpp:
//     0x82789C60  lbz 0x4878 (mbInRace); beq -> return
//     0x82789C6C  assert !IsPlayerCar()                         (lbz 0x1549; :183, li r5,0xB7)
//     0x82789C9C  assert GetOpponentIndex() >= 0                (lbz 0x153A; cmplwi 0x80; :184)
//     0x82789CC4  assert GetOpponentIndex() < KI_MAX_RIVALS_IN_MODE (extsb; cmpwi 7; :185)
//     0x82789CF0  lwz 0x1400(car) -- the car's own Route (AICar+0) node count; <= 1 -> return
//     0x82789D08  route = Array<RaceBalancingRoute,7>::GetItem(this+0x1C4, idx)  @0x8276A7F8
//     0x82789D1C  graph = Array<RaceBalancingGraph,7>::GetItem(this, idx)        @0x8276A5E8
//     0x82789D20  lbz 0xA14(route) (mbValid)
//       set:   0x82789D44 RaceBalancingRoute::Recalculate(route, sections, graph, car-route,
//                                                          lwz 0x4874 miCheckpointCount)
//       clear: 0x82789D58 v1 = AICar::GetPosition(car)
//              0x82789D78 RaceBalancingRoute::Prepare(route, v1, sections, graph, car-route,
//                                                      miCheckpointCount)
// Its ONLY caller is AIModule::UpdateCarRoutes (0x8279575C), and it is the ONLY caller of
// Prepare and Recalculate: until 2026-09-22 (crash parity G04-D4) it had no body, every
// opponent route stayed miTimeCount == 0 / mbValid == false, and the public ComputeTargetSpeed
// answered KF_DEFAULT_SPEED (60 mph) for every rival in every balanced race.
// ===========================================================================================
void RaceBalancingManager::UpdateOpponentRoute(const AICar* lpAICar, const AISectionsData* lpAISectionsData)
{
    if (!mbInRace)
    {
        return;
    }

    CGS_ASSERT(!lpAICar->IsPlayerCar(), "!lpAICar->IsPlayerCar()");                                 // :183
    CGS_ASSERT(lpAICar->GetOpponentIndex() >= 0, "lpAICar->GetOpponentIndex() >= 0");               // :184
    CGS_ASSERT(lpAICar->GetOpponentIndex() < BrnWorld::KI_MAX_RIVALS_IN_MODE,
               "lpAICar->GetOpponentIndex() < BrnWorld::KI_MAX_RIVALS_IN_MODE");                     // :185

    const Route* lpRoute = lpAICar->GetRoute();
    if (lpRoute->GetNodeCount() > 1)
    {
        const u32 luOpponentIndex = static_cast<u32>(static_cast<s32>(lpAICar->GetOpponentIndex()));
        RaceBalancingRoute*       lpRaceBalancingRoute = &maRaceBalancingRoutes.GetItem(luOpponentIndex);
        const RaceBalancingGraph* lpRaceBalancingGraph = &maRaceBalancingGraphs.GetItem(luOpponentIndex);

        if (lpRaceBalancingRoute->mbValid)
        {
            lpRaceBalancingRoute->Recalculate(lpAISectionsData, lpRaceBalancingGraph, lpRoute,
                                              miCheckpointCount);
        }
        else
        {
            lpRaceBalancingRoute->Prepare(lpAICar->GetPosition(), lpAISectionsData,
                                          lpRaceBalancingGraph, lpRoute, miCheckpointCount);
        }
    }
}

// ===========================================================================================
// OnOpponentReachedCheckpoint @0x82789D88
// An AI opponent crossed checkpoint liCheckpointIndex: invalidate its route's cached timing
// and advance its current-checkpoint cursor to the next checkpoint. (The X360 inlined what the
// PS3 DWARF spells as RaceBalancingRoute::OnCheckpoint to these two direct stores; the dev
// StrStream log present in the PS3 build is compiled out here.)
// ===========================================================================================
void RaceBalancingManager::OnOpponentReachedCheckpoint(const AICar* lpAICar, s32 liCheckpointIndex)
{
    if (mbInRace)
    {
        CGS_ASSERT(!lpAICar->IsPlayerCar(), "!lpAICar->IsPlayerCar()");

        RaceBalancingRoute& lrRoute = maRaceBalancingRoutes[static_cast<u32>(lpAICar->GetOpponentIndex())];
        lrRoute.mbValid                  = false;                  // +0xA14 -> 0
        lrRoute.miCurrentCheckpointIndex = liCheckpointIndex + 1;  // +0xA08 -> idx+1
    }
}

// ===========================================================================================
// ComputeParSpeed @0x82789EC0  (private)
// The "par" speed for this opponent at its current route position: sample the opponent's
// speed-ratio curve at its race-completion fraction, then convert that ratio into a concrete
// section speed. Returns KF_DEFAULT_SPEED when the racer has no valid route or the race is
// not running.
// ===========================================================================================
f32 RaceBalancingManager::ComputeParSpeed(GraphType leGraphType, const AICar* lpAICar,
                                          const AISectionsData* lpAISectionsData) const
{
    if (!(lpAICar->HasValidRoute() && mbInRace))
    {
        return KF_DEFAULT_SPEED;   // flt_8300D7F8
    }

    const s8 liOpponent = lpAICar->GetOpponentIndex();
    const RaceBalancingGraph& lrGraph = maRaceBalancingGraphs[static_cast<u32>(liOpponent)];
    const RaceBalancingRoute& lrRoute = maRaceBalancingRoutes[static_cast<u32>(liOpponent)];

    // Next route node (bounds-checked against the route node count; null if out of range).
    const RouteNode* lpNode = lpAICar->GetNextRouteNode();

    // The AI section this node sits in, and the racer's 0..1 completion of the whole route.
    const AISection* lpAISection = lpAISectionsData->GetAISection(lpNode->GetSectionIndex());
    const f32 lfRaceCompletionRatio =
        lrRoute.ComputeRaceCompletionRatio(lpNode->GetDistanceToCheckpoint(), miCheckpointCount);

    // Curve-sampled speed ratio for this completion fraction.
    const f32 lfSpeedRatio = lrGraph.ComputeSpeedRatio(leGraphType, lfRaceCompletionRatio);

    // Convert the ratio into a section speed. GetAISectionSpeed is declared non-const on
    // RaceBalancingRoute (X360 ignored const-correctness here); cast to match its signature.
    return const_cast<RaceBalancingRoute&>(lrRoute).GetAISectionSpeed(lpAISection, lpAISectionsData,
                                                                      lfSpeedRatio);
}

// ===========================================================================================
// ComputeTargetSpeed (public overload) -- sub_82794AA0 (an IDA export hole; 56 insns decoded from
// image.bin with capstone, 2026-09-03). Called from AICar::CalcDesiredSpeed @0x82796078 case 1.
//   lbz 0x4878(this)          mbInRace == 0                     -> KF_DEFAULT_SPEED (f31 = flt_8300D7F8)
//   lwz 0x14C0(car) != 1      meRouteFindingStyle != RACE       -> default
//   0x1408 != 0 && 0x1400 > 0 HasValidRoute() false             -> default
//   lbz 0x153A / addi r3,this,0x1C4 / bl Array<RaceBalancingRoute,7>::GetItem(miOpponentIndex)
//   lwz 0x1524(car) >= lwz 0xA00(route)  miNextRouteNodeIndex >= miTimeCount -> default
//   lfs 0x14F8(car) > flt_82004A18 (80.0f) ? GraphType 0 (AHEAD) : 1 (BEHIND) -> the private overload
// The DWARF :92 third parameter (lbPlayerIsCrashing, r6) is never read by this body.
// ===========================================================================================
f32 RaceBalancingManager::ComputeTargetSpeed(const AICar* lpAICar,
                                             const AISectionsData* lpAISectionsData,
                                             bool /*lbPlayerIsCrashing*/) const
{
    // [DIAG] NOT IN THE X360 BINARY (BRN_RACEBAL_DIAG=1). EVERY early return here hands the
    // opponent KF_DEFAULT_SPEED == 60 mph, which is both "the AI is too slow" and "the AI
    // never boosts" at once: AIDriver::CheckForBoosting only boosts when the desired speed is
    // above 130 mph or more than 50 mph above the current speed, and a 60 mph target can
    // reach neither. So WHICH guard fires is the whole question. Rate limited to one line per
    // (reason, opponent) pair so a per-frame path cannot storm the log.
    #define BRN_RACEBAL_BAIL(reason)                                                        \
        do {                                                                                \
            if (getenv("BRN_RACEBAL_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)           \
            {                                                                               \
                static u32 suSeen[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };                          \
                const s32 liOpp = lpAICar->GetOpponentIndex();                              \
                const u32 luBit = 1u << (reason);                                           \
                const s32 liSlot = (liOpp >= 0 && liOpp < 8) ? liOpp : 0;                   \
                if ((suSeen[liSlot] & luBit) == 0)                                           \
                {                                                                           \
                    suSeen[liSlot] |= luBit;                                                \
                    *CgsDev::Log::gpDebugPrint                                              \
                        << "[racebal] opp " << liOpp << " -> DEFAULT 60mph, reason "        \
                        << (reason)                                                          \
                        << " (0 notInRace 1 styleNotRace 2 noValidRoute 3 nodeBeyondTimes)\n"; \
                }                                                                           \
            }                                                                               \
        } while (0)

    if (!mbInRace)
    {
        BRN_RACEBAL_BAIL(0);
        return KF_DEFAULT_SPEED;
    }
    if (lpAICar->GetRouteFindingStyle() != E_ROUTE_FINDING_RACE)
    {
        BRN_RACEBAL_BAIL(1);
        return KF_DEFAULT_SPEED;
    }
    if (!lpAICar->HasValidRoute())
    {
        BRN_RACEBAL_BAIL(2);
        return KF_DEFAULT_SPEED;
    }
    const RaceBalancingRoute& lrRoute = maRaceBalancingRoutes[static_cast<u32>(lpAICar->GetOpponentIndex())];
    if (lpAICar->GetNextRouteNodeIndex() >= lrRoute.GetTimeCount())
    {
        BRN_RACEBAL_BAIL(3);
        return KF_DEFAULT_SPEED;
    }
    #undef BRN_RACEBAL_BAIL
    const f32 KF_AHEAD_GRAPH_DISTANCE = 80.0f;   // flt_82004A18
    const GraphType leGraphType = (lpAICar->GetDistanceAheadOfPlayer() > KF_AHEAD_GRAPH_DISTANCE)
                                      ? E_GRAPH_TYPE_AHEAD : E_GRAPH_TYPE_BEHIND;
    return ComputeTargetSpeed(leGraphType, lpAICar, lpAISectionsData);
}

// ===========================================================================================
// ComputeTargetSpeed @0x827916E0  (private GraphType overload)
// Turns the par speed into the speed the opponent should actually drive: nudges it up when the
// racer is behind its scheduled par time and down when ahead, within a bounded multiplier band
// (wider when the car is OUT_OF_RANGE), then caps the result at the player's max speed (except
// for an out-of-range car that is NOT already ahead of the player).
// ===========================================================================================
f32 RaceBalancingManager::ComputeTargetSpeed(GraphType leGraphType, const AICar* lpAICar,
                                             const AISectionsData* lpAISectionsData) const
{
    f32 lfSpeed = KF_DEFAULT_SPEED;   // flt_8300D7F8 (default if no valid route / not racing)

    if (lpAICar->HasValidRoute() && mbInRace)
    {
        const RaceBalancingRoute& lrRoute =
            maRaceBalancingRoutes[static_cast<u32>(lpAICar->GetOpponentIndex())];

        const f32 lfParSpeed = ComputeParSpeed(leGraphType, lpAICar, lpAISectionsData);
        lfSpeed = lfParSpeed;

        if (lrRoute.mbValid)
        {
            const f32 lfTargetTime = lrRoute.GetTime(leGraphType, lpAICar->GetNextRouteNodeIndex());

            // Wider band when the car is not in the player's range.
            f32 lfMinMultiplier;
            f32 lfMaxMultiplier;
            if (lpAICar->GetState() != E_AI_CAR_STATE_IN_RANGE)
            {
                lfMinMultiplier = KF_MIN_SPEED_MULTIPLIER_OUT_OF_RANGE; // 0.8
                lfMaxMultiplier = KF_MAX_SPEED_MULTIPLIER_OUT_OF_RANGE; // 1.2
            }
            else
            {
                lfMinMultiplier = KF_MIN_SPEED_MULTIPLIER_IN_RANGE;     // 0.9
                lfMaxMultiplier = KF_MAX_SPEED_MULTIPLIER_IN_RANGE;     // 1.1
            }

            // Schedule offset -> bounded speed multiplier: 1 + (raceTime - parTime)*0.1, clamped by
            // the inlined fpu::Clamp<float> ladder: `fsubs t,min,m ; fsel m,t,min,m` 0x827917BC/
            // 0x827917C0 (0x827917D4/0x827917D8 out of range) then `fsubs t,max,m ; fsel m,t,m,max`
            // 0x827917E0/0x827917E4. fsel takes its THIRD operand on an unordered test, so a NaN
            // multiplier is the max (crash parity FX-AINAN2; the old if/if kept the NaN).
            // 0x827917AC `fmadds f0, f12, f0, f13` = (raceTime - parTime) * 0.1 + 1.0, ONE rounding (ROUNDING_RULE 3;
            // the fsubs at 0x82791790 is rule 4; 0.1 @0x820C424C, 1.0 @0x82001C98).
            f32 lfMultiplier = std::fmaf(mfRaceTime - lfTargetTime, KF_SPEED_DIFFERENCE_MULTIPLIER, 1.0f);
            lfMultiplier = ((lfMinMultiplier - lfMultiplier) >= 0.0f) ? lfMinMultiplier : lfMultiplier;
            lfMultiplier = ((lfMaxMultiplier - lfMultiplier) >= 0.0f) ? lfMultiplier : lfMaxMultiplier;

            lfSpeed = lfParSpeed * lfMultiplier;
        }

        // Cap at the player's max speed, EXCEPT when an out-of-range car is NOT already ahead
        // of the player (those laggards are allowed to run uncapped to catch back up; an
        // out-of-range car that IS ahead still gets capped, same as an in-range car).
        if (!(lpAICar->GetState() != E_AI_CAR_STATE_IN_RANGE && !lpAICar->IsAheadOfPlayer()))
        {
            const f32 lfMaxPlayerSpeed = lpAICar->GetMaxPlayerSpeed();
            if (lfSpeed > lfMaxPlayerSpeed)
            {
                lfSpeed = lfMaxPlayerSpeed;
            }
        }
    }

    return lfSpeed;
}

// ===========================================================================================
// CalculateScheduleOffset @0x82789E00
// Fills lafOutScheduleOffsets[2] with how far (in seconds) this opponent is from its AHEAD and
// BEHIND par schedules at the current checkpoint: positive == that much par time still remains
// (ahead of where it "should" be), relative to the live race clock. Both entries are 0 when the
// racer has no valid route / the race is not running / the route timing is not yet valid.
//   [E_GRAPH_TYPE_AHEAD]  = parTime(AHEAD,  nextNode) - raceTime
//   [E_GRAPH_TYPE_BEHIND] = parTime(BEHIND, nextNode) - raceTime
// (The X360 writes the BEHIND entry first, then the AHEAD entry.)
// ===========================================================================================
void RaceBalancingManager::CalculateScheduleOffset(const AICar* lpAICar,
                                                   f32* lafOutScheduleOffsets) const
{
    lafOutScheduleOffsets[E_GRAPH_TYPE_BEHIND] = 0.0f;
    lafOutScheduleOffsets[E_GRAPH_TYPE_AHEAD]  = 0.0f;

    if (mbInRace)
    {
        const RaceBalancingRoute& lrRoute =
            maRaceBalancingRoutes[static_cast<u32>(lpAICar->GetOpponentIndex())];

        if (lpAICar->HasValidRoute() && lrRoute.mbValid)
        {
            const s32 liNodeIndex = lpAICar->GetNextRouteNodeIndex();
            lafOutScheduleOffsets[E_GRAPH_TYPE_BEHIND] =
                lrRoute.GetTime(E_GRAPH_TYPE_BEHIND, liNodeIndex) - mfRaceTime;
            lafOutScheduleOffsets[E_GRAPH_TYPE_AHEAD] =
                lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, liNodeIndex) - mfRaceTime;
        }
    }
}

// Never called -- pins the touched private offsets so the compile gate enforces the X360
// layout (offsetof needs this member-function context to see the private members).
void RaceBalancingManager::_AssertLayout()
{
    static_assert(offsetof(RaceBalancingManager, maRaceBalancingGraphs) == 0x0000,
                  "maRaceBalancingGraphs @ +0");
    static_assert(offsetof(RaceBalancingManager, maRaceBalancingRoutes) == 0x01C4,
                  "maRaceBalancingRoutes @ +0x1C4");
    static_assert(offsetof(RaceBalancingManager, mfRaceTime)            == 0x4870,
                  "mfRaceTime @ +0x4870");
    static_assert(offsetof(RaceBalancingManager, miCheckpointCount)     == 0x4874,
                  "miCheckpointCount @ +0x4874");
    static_assert(offsetof(RaceBalancingManager, mbInRace)              == 0x4878,
                  "mbInRace @ +0x4878");
    static_assert(offsetof(RaceBalancingManager, mbOnStartLine)         == 0x4879,
                  "mbOnStartLine @ +0x4879");
}

// ---- the extern tuning constants BrnRaceBalancingManager.h:56.. declares (DWARF :102..:108); defined here
// 2026-09-03 (conductor). KF_DEFAULT_SPEED is .bss flt_8300D7F8 with its dyn-init in an export hole:
// @0x82C69488 `lfs f13, flt_820C4158 (60.0) ; fmuls f0, f0, flt_82F31928 (0.44704) ; stfs f0, 0x8300D7F8`
// -- 60 mph in m/s. The band limits are rodata, each read from image.bin at the address given.
const f32 KF_DEFAULT_SPEED                       = 60.0f * 0.44704f;   // 26.8224 (flt_8300D7F8 <- flt_820C4158 * flt_82F31928)
const f32 KF_SPEED_DIFFERENCE_MULTIPLIER         = 0.1f;               // flt_820C424C
const f32 KF_MAX_SPEED_MULTIPLIER_IN_RANGE       = 1.1f;               // flt_820C3D90
const f32 KF_MIN_SPEED_MULTIPLIER_IN_RANGE       = 0.9f;               // flt_820C48A0
const f32 KF_MAX_SPEED_MULTIPLIER_OUT_OF_RANGE   = 1.2f;               // flt_820C48A4
const f32 KF_MIN_SPEED_MULTIPLIER_OUT_OF_RANGE   = 0.8f;               // flt_820C4330
// DWARF :109. The multiplier Update applies to the race clock while the player is crashing:
// AIModule::Update 0x8279B6A4/0x8279B6AC `lfs f0, flt_820C4168` -- x360rd 820C4168 == 0x3F000000.
const f32 KF_PLAYER_CRASHING_TIME_FACTOR         = 0.5f;               // flt_820C4168

}
