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
//   +0x000  Array<u32,8> mauBlockSectionIds[16]   (16 * 36 == 576 bytes)
//   +0x240  AStarDistanceFunction meDefaultAStarDistanceFunction
// Construct() (X360 @0x8278A3B0, KEPT verbatim) seeds a file-scope weight/bound table
// and clears each block-id slot's head word; the +0x240 default heuristic is set by
// SetDefaultAStarDistanceFunction. The standard/alternative builders read
// meDefaultAStarDistanceFunction as `*(this+0x240)`.
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

namespace BrnAI
{
// Forward-declared collaborator (its real home is its own TU; the full interface is
// declared TU-local in BrnRouteRequestManager.cpp). Pointer-only here.
class BuzzBy;

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
    // Per-checkpoint block-section-id list. Each entry is the X360 Array<u32,8>:
    // 8 inline ids + a trailing u32 count (36-byte stride, == the Construct loop's
    // 9-word slot). KEEP this shape so the committed Construct body still compiles.
    struct RequestSlot
    {
        u32 mWords[9];   // [0..7] ids, [8] count

        // X360 int_8_::GetItem / the +32 length read: named views over the slot.
        u32 GetBlockSectionCount() const { return mWords[8]; }
        u32 GetBlockSectionId(u32 luIndex) const { return mWords[luIndex]; }
    };

    RouteRequestManager* Construct();

    // DWARF :76 -- per-frame entry. Walks the 36-car roster; for each car that
    // GenerateRoute's mode applies to and that NeedsNewRoute(), issues a request.
    void Update(AICar* lpaAICars, AICar* lpPlayerCar, const AISectionsData* lpAISectionData,
                RouteMapModuleIO::InputBuffer* lpRouteInputBuffer, BuzzBy* lpBuzzByManager);

    void SetDefaultAStarDistanceFunction(AStarDistanceFunction leFunction)
    {
        meDefaultAStarDistanceFunction = leFunction;
    }

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
    RequestSlot           mauBlockSectionIds[16];          // +0x000 (16 * 36 == 576)
    AStarDistanceFunction meDefaultAStarDistanceFunction;  // +0x240 (576) DWARF :159
};
}

#endif
