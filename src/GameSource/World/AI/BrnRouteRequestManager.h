#ifndef BRN_ROUTE_REQUEST_MANAGER_H
#define BRN_ROUTE_REQUEST_MANAGER_H

// BrnAI::RouteRequestManager -- the "decide where the AI should go" brain. Per frame
// (Update) it walks the 36-car AI roster and, for any active car that needs a fresh
// route, issues the right kind of RouteMapModule request for that car's game mode:
//   * RACE                  -> GenerateStandardRouteRequest (full A* race route)
//   * RACE (alt line)       -> GenerateAlternativeRouteRequest
//   * ROAD_RAGE/PURSUIT/...  -> GenerateExtrapolatedRouteRequest (predict ahead)
//   * AVOID_PLAYER          -> GenerateRouteFleeingRouteRequest (run away from a threat)
//   * FREE_ROAM             -> GenerateFreeRoamingDestination + a standard request
// ChooseDistanceFunction picks the A* heuristic for the situation; ComputeSectionBehind
// finds the section behind the car (so a standard route can block U-turns).
//
// LAYOUT AUTHORITY is the X360 asm (BURNOUT_X360_ARTIST.XEX). The DecFIGS DWARF
// (BrnRouteRequestManager.h) supplies the member names/types/order and the method
// signatures; it marks mRandom `extern` -- i.e. mRandom is a FILE-SCOPE static, not a
// member (it is not part of the per-instance layout). The recovered member layout is:
//   +0x000  Array<u32,8> mauBlockSectionIds[16]   (16 * 36 == 576 bytes; count word at +0x20)
//   +0x240  AStarDistanceFunction meDefaultAStarDistanceFunction
// Construct() (X360 @0x8278A3B0) Constructs the file-static mRandom (0x8300D570), zeroes every
// block-section COUNT (+0x20 + 36*i) and the +0x240 default heuristic. AIModule::OnModeStart
// sets the heuristic (SetDefaultAStarDistanceFunction) and the per-checkpoint block sections
// (SetBlockSections, inlined there); OnModeEnd resets both (ClearBlockSections, inlined there).
// The standard/alternative builders read meDefaultAStarDistanceFunction as `*(this+0x240)`.
//
// The not-yet-homed collaborators ComputeSectionBehind/GenerateFreeRoamingDestination
// call (RacingLineGenerator::GetForwardPortalIndex, BuzzBy::IsPositionInNoBuzzZone) are
// declared TU-LOCAL in BrnRouteRequestManager.cpp (matching the BrnRouteMapModule.cpp
// precedent) so they do NOT collide here with BrnRouteMapModule.cpp's own ad-hoc
// BrnAI::RacingLineGenerator interface declaration (ODR). Only BuzzBy is forward-declared
// here, for the `BuzzBy*` parameter types in the method signatures.

#include "types.hpp"

#include "GameSource/World/AI/BrnAICar.h"                 // BrnAI::AICar
#include "GameSource/World/AI/Route/BrnAStar.h"           // BrnAI::AStarDistanceFunction
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"// RaceRouteRequest / InputBuffer / queues
#include "SharedClasses/AI/AISectionsResourceType.h"      // BrnAI::AISectionsData / AISection / Portal
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<u32,8u> (mauBlockSectionIds, DWARF :157)

namespace BrnAI
{
// Forward-declared collaborator (its real home is its own TU; the full interface is
// declared TU-local in BrnRouteRequestManager.cpp). Pointer-only here.
struct BuzzBy;   // (struct: matches the definition in BrnAIBuzzBy.h -- MSVC mangles the class-key; `class` here made RouteRequestManager::Update unresolvable)

// DWARF BrnRouteRequestManager.h:31 -- whether a standard route may double back.
enum EUTurns
{
    E_ALLOW_U_TURNS = 0,
    E_NO_U_TURNS    = 1,
};

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8278A3B0 (Construct).
class RouteRequestManager
{
public:
    // DWARF :68 `void Construct()` (X360 @0x8278A3B0).
    void Construct();

    // DWARF :76 -- per-frame entry. Walks the 36-car roster; for each car that
    // GenerateRoute's mode applies to and that NeedsNewRoute(), issues a request.
    void Update(AICar* lpaAICars, AICar* lpPlayerCar, const AISectionsData* lpAISectionData,
                RouteMapModuleIO::InputBuffer* lpRouteInputBuffer, BuzzBy* lpBuzzByManager);

    // DWARF :80. Inlined on X360: AIModule::OnModeStart 0x82791E64 (`stwx` of the mode's
    // A* type into +0x240) and AIModule::OnModeEnd 0x8277BAAC (`stwx 0`).
    void SetDefaultAStarDistanceFunction(AStarDistanceFunction leFunction)
    {
        meDefaultAStarDistanceFunction = leFunction;
    }

    // DWARF :85 / BrnRouteRequestManager.cpp:99. Inlined on X360 in AIModule::OnModeStart's
    // checkpoint loop (0x82791ED4..0x82791F08): assert (:101), `stw 0, 0x20(slot)` == Construct,
    // then Array<u32,8>::AppendArray<8>(slot, ids) @0x8278A108.
    void SetBlockSections(s32 liCheckpointIndex, const Array<u32, 8u>* laBlockSectionIds);

    // DWARF :88 / BrnRouteRequestManager.cpp:116. Inlined on X360 in AIModule::OnModeEnd
    // (0x8277BB8C..0x8277BBC8): 16 x `stw 0, 0(r9); r9 += 0x24` from this+0x20 == every count.
    void ClearBlockSections();

private:
    // DWARF :99 -- dispatch one car to the request builder for its route-finding style.
    void GenerateRoute(AICar* lpAICar, AICar* lpPlayerCar, AICar* lpaAICars,
                       const AISectionsData* lpAISectionData,
                       RouteMapModuleIO::InputBuffer* lpRouteInputBuffer, BuzzBy* lpBuzzByManager);

    // DWARF :111 -- pick a random reachable, non-shortcut, non-no-buzz section to cruise to.
    void GenerateFreeRoamingDestination(AICar* lpAICar, AICar* lpPlayerCar,
                                        const AISectionsData* lpAISectionData, BuzzBy* lpBuzzByManager);

    // DWARF :118 -- the full race route to the car's destination section.
    void GenerateStandardRouteRequest(AICar* lpAICar, const AISectionsData* lpAISectionData,
                                      RouteMapModuleIO::InputBuffer* lpRouteInputBuffer, EUTurns leAllowUTurns);

    // DWARF :125 -- a route biased onto a different line than the player's.
    void GenerateAlternativeRouteRequest(AICar* lpAICar, const AICar* lpPlayerCar,
                                         const AISectionsData* lpAISectionData,
                                         RouteMapModuleIO::InputBuffer* lpRouteInputBuffer);

    // DWARF :131 -- a short look-ahead request that predicts the car forward.
    void GenerateExtrapolatedRouteRequest(AICar* lpAICar, const AISectionsData* lpAISectionData,
                                          RouteMapModuleIO::InputBuffer* lpRouteInputBuffer);

    // DWARF :143 -- a look-ahead request whose direction is GetFleeVector (away from a threat).
    void GenerateRouteFleeingRouteRequest(AICar* lpAICar, AICar* lpCarToAvoid,
                                          const AISectionsData* lpAISectionData,
                                          RouteMapModuleIO::InputBuffer* lpRouteInputBuffer);

    // DWARF :136 -- the world-space direction to flee a car (its position minus the threat's,
    // or just the car's useful direction when no distinct threat exists).
    Vector3 GetFleeVector(AICar* lpCarToAvoid, AICar* lpAICar);

    // DWARF :148 -- the section index immediately behind the car (across its current
    // section's forward portal), used to block U-turns on a standard route.
    u16 ComputeSectionBehind(const AICar* lpAICar, const AISectionsData* lpAISectionData);

    // DWARF :155 -- pick the A* heuristic for this destination (Euclidean / X-biased /
    // Y-biased / Diagonal / Manhattan) from the start->end offset and the car's heading.
    AStarDistanceFunction ChooseDistanceFunction(const AICar* lpAICar, const AISectionsData* lpAISectionData,
                                                 AStarDistanceFunction leDefaultAStarDistanceFunction,
                                                 u16 luDestinationSectionIndex);

    // ---- storage (declaration order == X360 layout order) -------------------------------
    // One slot per mode checkpoint: 16 == BrnGameState::GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE
    // (the DWARF spells the extent as the literal; SetBlockSections asserts against the constant).
    Array<u32, 8u>        mauBlockSectionIds[16];                          // +0x000 (16 * 36 == 576) DWARF :157
    AStarDistanceFunction meDefaultAStarDistanceFunction;                 // +0x240 (576) DWARF :159
};
}

#endif
