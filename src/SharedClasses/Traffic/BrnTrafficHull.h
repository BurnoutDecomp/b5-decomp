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
#include <cstddef>   // offsetof (host layout static_asserts)

namespace BrnTraffic
{

// -----------------------------------------------------------------------------
// UN-HOMED dependency placeholders.
//
// FLAG: the broader Hull geometry value types below (Section, LaneRung,
// Neighbour, SectionSpan, SectionFlow, StaticTrafficVehicle, StopLine,
// LightTrigger, LightTriggerStartData, JunctionLogicBox) have their real DWARF
// home at SharedClasses/Traffic/BrnTrafficSection.h (and siblings), which is NOT
// yet reconstructed in this tree. They are NOT owned by this group, so they are
// declared here only as the minimum the Hull layout + GetSection need:
//
//   * Section is given an exact 48-byte placeholder body. The X360 asm for
//     GetSection multiplies the index by 48 (`index*3 << 4`), so sizeof(Section)
//     == 48 is load-bearing for the `&mpaSections[luIndex]` arithmetic to be
//     store-for-store faithful. The natural member sum of the DWARF Section list
//     reaches +44 at mfLength; the X360 build sizes the record to 48, so the
//     placeholder carries the proven 48-byte footprint. When BrnTrafficSection.h
//     is reconstructed it should DEFINE Section (size 48) and this placeholder
//     must be dropped; it is guarded so the real home wins.
//   * The remaining types are pointer targets only inside Hull, so opaque
//     forward declarations are sufficient for this slice.
// -----------------------------------------------------------------------------

#ifndef BRNTRAFFIC_SECTION_DEFINED
#define BRNTRAFFIC_SECTION_DEFINED
// PLACEHOLDER (un-homed): exact 48-byte footprint proven by the GetSection stride.
// Replace with the reconstructed BrnTrafficSection.h definition when it lands.
struct Section
{
    u8 maPlaceholder[48];
};
#endif

struct LaneRung;
struct Neighbour;
struct SectionSpan;
struct SectionFlow;
struct StaticTrafficVehicle;
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
    // FLAG (opaque-element stride): Neighbour / StaticTrafficVehicle / the
    // stop-line index element are forward-declared-only in this slice (their real
    // record layouts belong to the not-yet-reconstructed BrnTrafficSection.h /
    // BrnTrafficStopLine.h homes). The three accessors below are bodied store-for-
    // store off the X360 asm, which uses an EXPLICIT element stride per accessor:
    //   GetNeighbour      stride 4   (`slwi r11, r29, 2`)        @ 0x821F5358
    //   GetStaticVehicle  stride 80  (`(idx*5)<<4` = 80*idx)     @ 0x82705C90
    //   GetStopLine       stride 2   (`slwi r10, r30, 1`)        @ 0x82705C20
    // The returned address is computed by raw byte arithmetic over the matching
    // pointer member, exactly as the asm does; the pointer-target types stay opaque.

    // X360 @ 0x821F5358. asm: assert(mpaNeighbourData != 0); assert(luIndex <
    // muNumNeighbours); return mpaNeighbourData + 4*luIndex.
    inline const void* GetNeighbour(u32 luIndex) const;

    // X360 @ 0x82705C90. asm: assert(luIndex < muNumStaticTraffic);
    // return mpaStaticTrafficVehicles + 80*luIndex.
    inline const void* GetStaticVehicle(u32 luIndex) const;

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

inline const void* Hull::GetStaticVehicle(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muNumStaticTraffic, "luIndex < muNumStaticTraffic");
    return reinterpret_cast<const u8*>(mpaStaticTrafficVehicles) + 80u * luIndex;
}

inline const void* Hull::GetStopLine(u32 luIndex) const
{
    CGS_ASSERT(luIndex < muNumStoplines, "luIndex < muNumStoplines");
    return reinterpret_cast<const u8*>(mpaStopLines) + 2u * luIndex;
}

}
