#pragma once

// =============================================================================
// BrnTraffic::Hull -- the per-hull traffic-graph geometry block of the TrafficData
// resource. Member order and names are DWARF-authoritative
// (dwarfdump/SharedClasses/Traffic/BrnTrafficHull.h, struct @ :53).
//
// The pointer block starts at +0x10, where the X360 asm reads mpaSections
// (`lwz r10, 0x10(r30)`). The 16 leading scalar bytes (7 u8, 1 alignment pad, u16
// muNumRungs, 2 u16 lights, 2 u8 counters) put it there with no synthetic padding.
//
// X360 pointers are 4 bytes and host pointers 8, so the console offsets hold only
// for the scalar header. Members are pinned BY NAME; the static_asserts below pin
// the HOST layout against the lane_transcode.py emitter, not the console's.
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"        // Section / LaneRung / Neighbour (real home)
#include "SharedClasses/Traffic/BrnTrafficStaticTraffic.h"  // StaticTrafficVehicle (by value in the array)
#include "SharedClasses/Traffic/BrnTrafficSectionFlow.h"    // SectionFlow (real pointee for mpaSectionFlows)
#include <cstddef>   // offsetof (host layout static_asserts)

namespace BrnTraffic
{

// PARK -- still un-homed. These are pointer targets only inside Hull, so opaque
// forward declarations suffice. Their DWARF homes (BrnTrafficSection.h siblings,
// BrnTrafficStopLine.h, BrnJunctionLogicBox.h) are not reconstructed yet.
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

    // X360 @ 0x821F52E0. Bounds-asserts luIndex against muNumSections, then returns
    // &mpaSections[luIndex] (stride 48, from the asm index math).
    inline const Section* GetSection(u32 luIndex) const;

    // Console-inlined (asm 0x8222A804..0x8222A814: `lwz r9, 0x18(hull)` then
    // `add r28, lpSection->muRungOffset << 2, r9`); DWARF names it in
    // BrnBehaviourRoadRunner.cpp's MoveAlongTrafficLane{Forwards,Backwards}. The
    // cumulative rung-length table is one array for the whole hull, so a section's
    // slice starts at its own muRungOffset.
    inline const f32* GetRungLengthsForSection(const Section* lpSection) const;

    // FLAG (opaque-element stride): the stop-line index element is forward-declared only,
    // so GetStopLine stays bodied off the asm's explicit element stride. DELETE WHEN the
    // BrnTrafficStopLine.h record is reconstructed and can be indexed by name.

    // X360 @ 0x821F5358. asm: assert(mpaNeighbourData != 0); assert(luIndex <
    // muNumNeighbours); return mpaNeighbourData + 4*luIndex (`slwi r11, r29, 2`), which is
    // sizeof(Neighbour) -- the element type is modelled and sizeof-pinned in
    // BrnTrafficSection.h, so this indexes by name.
    inline const Neighbour* GetNeighbour(u32 luIndex) const;

    // DWARF BrnTrafficHull.h:100. Console-inlined; the ship's assert sits at
    // BrnTrafficHull.h:231 (Section::FindNeighbourForRung @0x82752C2C hoists it out of the
    // scan loop, which is why the whole array -- not one element -- is the accessor).
    inline const Neighbour* GetNeighbours() const;

    // X360 @ 0x82705C90 (DWARF BrnTrafficHull.h:130). asm:
    // `return 80 * luIndex + *(this + 36);` -- +36 is the console's
    // mpaStaticTrafficVehicles slot and 80 is sizeof(StaticTrafficVehicle), so
    // indexing by name reproduces it. Assert baked at BrnTrafficHull.h:297.
    inline const StaticTrafficVehicle* GetStaticVehicle(u32 luIndex) const;

    // X360 @ 0x82705C20. asm: assert(luIndex < muNumStoplines);
    // return mpaStopLines + 2*luIndex (`slwi r10, r30, 1`).
    inline const void* GetStopLine(u32 luIndex) const;

    // X360 @0x827620A0 / @0x827622E0 (DWARF :135 / :140). Rebases the twelve
    // consecutive pointer slots (console +0x10..+0x3C) against the resource block
    // base, then asserts the junction count and the two 16-byte alignment guards.
    // Called per hull from TrafficData::FixUp / FixDown; bodied in the .cpp.
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

inline const Neighbour* Hull::GetNeighbour(u32 luIndex) const
{
    CGS_ASSERT(mpaNeighbourData != nullptr, "mpaNeighbourData");
    CGS_ASSERT(luIndex < muNumNeighbours, "luIndex < muNumNeighbours");
    return &mpaNeighbourData[luIndex];
}

inline const Neighbour* Hull::GetNeighbours() const
{
    CGS_ASSERT(mpaNeighbourData != nullptr, "mpaNeighbourData");
    return mpaNeighbourData;
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
