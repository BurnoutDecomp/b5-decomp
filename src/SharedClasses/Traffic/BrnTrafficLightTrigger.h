#pragma once

// =============================================================================
// BrnTrafficLightTrigger.h  (NEW OWNING HEADER)
//
// Home for the BrnTraffic traffic-light-trigger value types. This slice
// reconstructs only BrnTraffic::LightTriggerStartData -- the per-junction
// start-grid block read by the ModeManager / online-mode start-grid setup when a
// race is launched at a set of lights. The X360 ARTIST build attests two of its
// methods as standalone functions:
//   GetStartPosition  @ 0x8231BB50
//   GetStartDirection @ 0x8231BC68
//
// LAYOUT: member names/types/order are DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficLightTrigger.h,
// struct @ line 133). Vector3 is the engine 16-byte SIMD type, so offset 400
// (muNumStartingPositions, the X360 `*(this+400)` bounds field) falls out of the
// natural member list with no synthetic padding.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3), CgsID

namespace BrnTraffic
{
// DWARF spells this "ERaceDesinationType" (sic) at BrnTrafficLightTrigger.h:145; the
// per-destination difficulty grade. Only referenced by the declared-only
// GetDestinationDifficulty below, so a forward enum decl is sufficient for this slice.
enum ERaceDesinationType : s32;

// BrnTrafficLightTrigger.h:133 -- the start/destination block for one traffic-light junction.
struct LightTriggerStartData
{
    // BrnTrafficLightTrigger.h:136 -- bound shared by the two bounds asserts ("<= 8").
    static const u32 KU_MAX_START_POSITIONS = 8;

    // --- attested standalone accessors (defined in BrnTrafficLightTrigger.cpp) ---
    Vector3 GetStartPosition(u32 luIndex) const;   // X360 @ 0x8231BB50
    Vector3 GetStartDirection(u32 luIndex) const;  // X360 @ 0x8231BC68

    // --- declared-only (inlined / PS3-drift in the X360 build) for shape coherence ---
    u32                 GetNumStartPositions() const;
    u32                 GetNumDestinations() const;
    CgsID               GetDestinationID(u32 luIndex) const;
    ERaceDesinationType GetDestinationDifficulty(u32 luIndex) const;
    void                FixUp(const void* lpBase);
    void                FixDown(const void* lpBase);

private:
    Vector3 maStartingPositions[KU_MAX_START_POSITIONS];   // +0
    Vector3 maStartingDirections[KU_MAX_START_POSITIONS];  // +128
    CgsID   maDestinationIDs[16];                           // +256
    u8      maeDestinationDifficulties[16];                 // +384
    u8      muNumStartingPositions;                         // +400
    u8      muNumDestinations;                              // +401
    u8      muNumLanes;                                     // +402
};
}
