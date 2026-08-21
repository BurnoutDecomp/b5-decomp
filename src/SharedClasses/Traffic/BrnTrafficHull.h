#pragma once

// =============================================================================
// BrnTrafficHull.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::Hull -- the per-hull traffic-graph geometry block of the
// TrafficData resource (the Hull subsystem gates OnlineStuntRunMode start-grid
// setup). This slice reconstructs the single attested standalone accessor:
//
//   BrnTraffic::Hull::GetSection  @ 0x821F52E0
//
// LAYOUT: the scalar-then-pointer member order and names are DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficHull.h, struct @
// line 53). The pointer block begins at byte offset 0x10, which is exactly where
// the X360 asm reads mpaSections (`lwz r10, 0x10(r30)`); the 16 leading scalar
// bytes (7 u8 + 1 alignment pad + u16 muNumRungs + 2*u16 lights + 2 u8 counters)
// place it there with no synthetic padding beyond the natural pad7 hole the
// DWARF list implies between muNumVehicleAssets (+6) and muNumRungs (+8).
//
// X360 NOTE: pointers are 4 bytes on X360 and 8 bytes on the host. Members are
// pinned BY NAME; absolute offsets are not static_asserted across the pointer
// block because pointer width differs between target and host.
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"        // Section / LaneRung / Neighbour (real home)
#include "SharedClasses/Traffic/BrnTrafficStaticTraffic.h"  // StaticTrafficVehicle (by value in the array)
#include "SharedClasses/Traffic/BrnTrafficSectionFlow.h"    // SectionFlow (real pointee for mpaSectionFlows)
#include <cstddef>   // offsetof (host layout static_asserts)

namespace BrnTraffic
{

// -----------------------------------------------------------------------------
// DEPENDENCY TYPES  (this list SHRANK on 2026-08-21, wave T1 cluster C2)
//
// HOMED, no longer placeholders:
//   * Section / LaneRung / Neighbour -- real definitions in BrnTrafficSection.h,
//     now INCLUDED above instead of being shadowed by a 48-byte
//     `struct Section { u8 maPlaceholder[48]; }` stand-in.
//     ⚠️ RETIRED TRAP: the stand-in was guarded by BRNTRAFFIC_SECTION_DEFINED so the
//     real home would win *if it happened to be included first*. That contract broke
//     silently once GetRungLengthsForSection landed below and dereferenced
//     Section::muRungOffset -- a member the stand-in does not have -- so ANY TU that
//     included this header without remembering to include BrnTrafficSection.h first
//     failed with C2039. Four did (BrnTrafficHull_embed_check.cpp,
//     BrnOnlineStuntRunMode.cpp, BrnTrafficDebugComponent.cpp, BrnTrafficVehicle.cpp).
//     Including the real home from here removes the ordering requirement entirely;
//     there is no include cycle, because BrnTrafficSection.h only forward-declares Hull.
//   * StaticTrafficVehicle -- real home BrnTrafficStaticTraffic.h (included above), so
//     GetStaticVehicle returns the DWARF-declared `const StaticTrafficVehicle*` instead
//     of an opaque `const void*` walked by a raw 80-byte stride.
//   * SectionFlow -- real home BrnTrafficSectionFlow.h (included above).
//
// STILL UN-HOMED (pointer targets only inside Hull, so opaque forward declarations
// are sufficient): SectionSpan, StopLine, LightTrigger, LightTriggerStartData,
// JunctionLogicBox. Their real DWARF homes (BrnTrafficSection.h siblings /
// BrnTrafficStopLine.h / BrnJunctionLogicBox.h) are not reconstructed yet and are not
// owned by this group.
// -----------------------------------------------------------------------------

struct SectionSpan;
struct StopLine;
struct LightTrigger;
struct LightTriggerStartData;
class  JunctionLogicBox;

// BrnTrafficHull.h:53 -- per-hull traffic-graph geometry block.
struct Hull
{
    // --- scalar header (offsets +0 .. +15) -----------------------------------
    u8  muNumSections;              // +0   (GetSection bounds field)
    u8  muNumSectionSpans;          // +1
    u8  muNumJunctions;             // +2
    u8  muNumStoplines;             // +3
    u8  muNumNeighbours;            // +4
    u8  muNumStaticTraffic;         // +5
    u8  muNumVehicleAssets;         // +6
    u8  maPad7;                     // +7   (alignment hole before muNumRungs)
    u16 muNumRungs;                 // +8
    u16 muFirstTrafficLight;        // +10
    u16 muLastTrafficLight;         // +12
    u8  muNumLightTriggers;         // +14
    u8  muNumLightTriggersStartData;// +15

    // --- pointer block (starts at +0x10 on X360) -----------------------------
    Section*               mpaSections;                 // +0x10
    LaneRung*              mpaRungs;
    f32*                   mpafCumulativeRungLengths;
    Neighbour*            mpaNeighbourData;
    SectionSpan*          mpaSectionSpans;
    StaticTrafficVehicle* mpaStaticTrafficVehicles;
    SectionFlow*          mpaSectionFlows;
    JunctionLogicBox*     mpaJunctions;
    StopLine*             mpaStopLines;
    LightTrigger*         mpaLightTriggers;
    LightTriggerStartData* mpaLightTriggerStartData;
    u8*                    mpaLightTriggerJunctionLookup;

    // BrnTrafficHull.h:90 -- inline per-hull vehicle-asset index table.
    static const u32 KU_MAX_VEHICLE_ASSETS_PER_HULL = 16;
    u8 mauVehicleAssets[KU_MAX_VEHICLE_ASSETS_PER_HULL];

    // --- attested standalone accessor (this TU) ------------------------------
    // X360 @ 0x821F52E0. Bounds-asserts luIndex against muNumSections, then
    // returns &mpaSections[luIndex] (stride 48, proven by the asm index math).
    inline const Section* GetSection(u32 luIndex) const;

    // DWARF BrnBehaviourRoadRunner.cpp names this in both MoveAlongTrafficLane{Forwards,
    // Backwards} call lists; the console inlines it (asm 0x8222A804..0x8222A814:
    // `lwz r9, 0x18(hull)` then `add r28, lpSection->muRungOffset << 2, r9`). The cumulative
    // rung-length table is ONE array for the whole hull; a section's slice starts at its own
    // muRungOffset, so `lengths[i+1] - lengths[i]` is the length of local segment i.
    inline const f32* GetRungLengthsForSection(const Section* lpSection) const;

    // --- attested standalone accessors (brn-traffic3 group) ------------------
    // FLAG (opaque-element stride, NARROWED 2026-08-21): Neighbour and the stop-line
    // index element are still forward-declared-only in this slice (their real record
    // layouts belong to the not-yet-reconstructed BrnTrafficSection.h /
    // BrnTrafficStopLine.h homes), so those two accessors stay bodied store-for-store
    // off the X360 asm's EXPLICIT element stride:
    //   GetNeighbour      stride 4   (`slwi r11, r29, 2`)        @ 0x821F5358
    //   GetStopLine       stride 2   (`slwi r10, r30, 1`)        @ 0x82705C20
    // GetStaticVehicle NO LONGER does: StaticTrafficVehicle now has a real home and
    // sizeof(StaticTrafficVehicle) == 80 is static_asserted there, so the asm's
    // `80 * luIndex + *(hull + 36)` becomes plain `&mpaStaticTrafficVehicles[luIndex]`
    // with no byte arithmetic and no cast.

    // X360 @ 0x821F5358. asm: assert(mpaNeighbourData != 0); assert(luIndex <
    // muNumNeighbours); return mpaNeighbourData + 4*luIndex.
    inline const void* GetNeighbour(u32 luIndex) const;

    // X360 @ 0x82705C90 (DWARF BrnTrafficHull.h:130 -- returns const StaticTrafficVehicle*).
    // asm: `if (luIndex >= *(this+5)) assert("luIndex < muNumStaticTraffic",
    // "...\\sharedclasses\\traffic\\BrnTrafficHull.h", 297);
    //  return 80 * luIndex + *(this + 36);`  -- +36 is X360's mpaStaticTrafficVehicles
    // slot (the sixth 4-byte pointer after the 16-byte scalar header) and 80 is
    // sizeof(StaticTrafficVehicle), so indexing by name reproduces it exactly.
    inline const StaticTrafficVehicle* GetStaticVehicle(u32 luIndex) const;

    // X360 @ 0x82705C20. asm: assert(luIndex < muNumStoplines);
    // return mpaStopLines + 2*luIndex.
    inline const void* GetStopLine(u32 luIndex) const;

    // --- load-time pointer relocation (DWARF :135 / :140) ---------------------
    // X360 @0x827620A0 / @0x827622E0. Rebases the twelve consecutive pointer slots
    // (X360 +0x10..+0x3C) against the resource block base, then asserts the junction
    // count and the two 16-byte alignment guards. Called per hull from
    // TrafficData::FixUp / FixDown; bodied in BrnTrafficHull.cpp.
    void FixUp(const void* lpBaseData);
    void FixDown(const void* lpBaseData);
};

// Host layout contract with tools/assets/bundles/lane_transcode.py's emitter.
static_assert(offsetof(Hull, muNumRungs)                    == 0x08, "Hull::muNumRungs");
static_assert(offsetof(Hull, mpaSections)                   == 0x10, "Hull::mpaSections");
static_assert(offsetof(Hull, mpaRungs)                      == 0x18, "Hull::mpaRungs");
static_assert(offsetof(Hull, mpaLightTriggerJunctionLookup) == 0x68, "Hull::mpaLightTriggerJunctionLookup");
static_assert(offsetof(Hull, mauVehicleAssets)              == 0x70, "Hull::mauVehicleAssets");
static_assert(sizeof(Hull) == 0x80, "Hull host sizeof");

inline const Section* Hull::GetSection(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muNumSections, "luIndex < muNumSections");
    return &mpaSections[luIndex];
}

inline const f32* Hull::GetRungLengthsForSection(const Section* lpSection) const
{
    CGS_ASSERT(lpSection != nullptr, "lpSection");
    return mpafCumulativeRungLengths + lpSection->muRungOffset;
}

inline const void* Hull::GetNeighbour(u32 luIndex) const
{
    CGS_ASSERT(mpaNeighbourData != nullptr, "mpaNeighbourData");
    CGS_ASSERT(luIndex < muNumNeighbours, "luIndex < muNumNeighbours");
    return reinterpret_cast<const u8*>(mpaNeighbourData) + 4u * luIndex;
}

inline const StaticTrafficVehicle* Hull::GetStaticVehicle(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muNumStaticTraffic, "luIndex < muNumStaticTraffic");
    return &mpaStaticTrafficVehicles[luIndex];
}

inline const void* Hull::GetStopLine(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muNumStoplines, "luIndex < muNumStoplines");
    return reinterpret_cast<const u8*>(mpaStopLines) + 2u * luIndex;
}

}
