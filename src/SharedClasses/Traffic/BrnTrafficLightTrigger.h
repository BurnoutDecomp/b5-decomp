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
#include "BrnCommonTypes.h"   // Vector3 / Vector3Plus / Matrix44Affine (rw::math::vpu), CgsID

#include <cstddef>            // offsetof (the LightTrigger layout pin below)

namespace BrnTraffic
{
// =============================================================================================
// [stuntrace waveD, agent D1] THE PACKED LightTriggerId, and the TRIGGER BOX it names.
//
// DWARF declares `struct BrnTraffic::LightTriggerId : public TriggerId` at :55 with Set(u32,u32),
// SetInvalid(), IsValid(), GetHull() and GetLightTriggerIndex(). THIS TREE STILL MODELS THE
// HANDLE AS A BARE `typedef u32 LightTriggerId` (BrnGameModeParams.h:85) and that stub is not
// promoted here -- doing so would change the type of GameModeParams::mTrafficLightTriggerId,
// StartGameModeParams::Set/GetTrafficLightTriggerId and TriggerQueryManager::
// mPlayerCurrentTrafficLightId in one edit, which is a header wave of its own. What DOES land
// here are the two console constants the handle's Set() asserts against, so that the three places
// that pack or unpack it stop each carrying a private copy of the magic numbers.
//
// THE PACKING, from BrnTraffic::TrafficEntityModule::ManageTriggers @0x82747518 -- the ONE
// producer, the function that registers every light-trigger box with the world:
//   0x82747798  cmplwi r28, 0x190   -> assert BrnTrafficLightTrigger.h:211 "luHull < KU_MAX_HULLS"
//   0x827477B8  cmplwi r29, 0x100   -> assert BrnTrafficLightTrigger.h:212 "luLightTriggerIndex < 256"
//   0x827477EC  slwi  r11, r28, 8   \
//   0x827477F8  oris  r11, r11, 0x3900   >  id = (hull << 8) | 0x39000000 | lightTriggerIndex
//   0x827477FC  or    r30, r11, r29 /
// and the matching UNPACK in TrafficData::GetJunctionLogicBoxForTrafficLight @0x82207FA4/FB0:
//   hull = (id >> 8) & 0xFFFF   (extrwi 16,8 -- the tag is dropped BY THE MASK, not stripped
//                                first), lightTriggerIndex = id & 0xFF; either half all-ones
//   means "no light trigger", which is what LightTriggerId::SetInvalid()'s stw -1 produces.
// The owner byte 0x39 == 57 is the tag TriggerQueryManager::PostWorldUpdate @0x82386F54 switches
// on to recognise a traffic-light line-test result (56 == a TriggerData region, 57 == this).
// =============================================================================================
// (KU_MAX_HULLS -- the 0x190 the :211 assert compares against -- is ALREADY HOMED, in
// SharedClasses/Traffic/BrnTrafficSharedConstants.h:30, recovered there from
// TrafficNetworkOutputInterface::ActivateHull's own bounds assert against the same 0x190. Include
// that header and use it; do not re-declare it here.)
static const u32 KU_LIGHT_TRIGGER_ID_OWNER_TAG = 0x39000000u;  // owner 57, in bits 24..31

// BrnTrafficLightTrigger.h:89 -- ONE JUNCTION-APPROACH TRIGGER BOX. A car inside this volume is
// "at the lights" for the junction the hull's mpaLightTriggerJunctionLookup maps this trigger to.
//
// [stuntrace waveD, agent D1] HOMED. Until now this type was a bare forward declaration in
// BrnTrafficHull.h:33 ("PARK -- still un-homed"), which made Hull::mpaLightTriggers unindexable
// and left junction detection with nothing to test the car against.
//
// LAYOUT, from ManageTriggers' per-trigger body (the only reader of the array in the image):
//   0x827477D8  lwz     r11, 0x34(r26)  ; lpHull->mpaLightTriggers      (console Hull +0x34)
//   0x827477E0  add     r31, r11, r27   ; &mpaLightTriggers[i]
//   0x82747884  addi    r27, r27, 0x20  ; ELEMENT STRIDE = 32
//   0x827477F0  lvx128  v1, r31, r23    ; r23 == 16 -> mPosPlusYRot @ +0x10, handed straight to
//                                       ;   BrnTraffic::ExpandPosPlusYRotToTransform -> GetTransform()
//   0x82747808  lvx128  v127, r0, r31   ; mDimensions @ +0x00
//   0x82747834  vandc128 v0, v127, <0x80000000 splat>   ; per-lane fabs of the dimensions
//   0x8274787C  lbz     r11, 0xE(r26)   ; loop bound == lpHull->muNumLightTriggers
// and the DWARF member list (:117 mDimensions, :118 mPosPlusYRot) agrees: two 16-byte SIMD lanes,
// no pointers, so console offsets are host offsets and the static_asserts below are exact. The
// same two members and the same stride 32 are what tools/assets/bundles/lane_transcode.py emits
// (STRUCTS['LightTrigger'] @ :248).
//
// [!] mDimensions ARE FULL EXTENTS, NOT HALF EXTENTS. ManageTriggers hands abs(mDimensions)
// through InAddBoxTriggerEvent::mDimensions unchanged, and it is the WORLD side that halves them:
// TriggerEntityModule::ProcessAddTriggerEvents @0x822D9520..0x822D9554 does
// `vspltisw128 v126,1 ; vcsxwfp128 v127,v126,1` (== 0.5) then `vmulfp128 v1, v0, v127` and only
// then calls rw::collision::BoxVolume::Initialize. So any point test written against this record
// must use `abs(GetDimensions()) * 0.5f` as the half extents -- see the light-region stand-in in
// BrnTriggerQueryManager.cpp, which is the one place in this tree that runs that test directly.
struct LightTrigger
{
    // :93. No standalone X360 symbol (folded into ManageTriggers' `lvx128 v127, r0, r31`), so it
    // is inline here for the same reason Hull::GetSection and friends are.
    Vector3 GetDimensions() const { return mDimensions; }

    // :94. Likewise folded on the console -- ManageTriggers calls
    // BrnTraffic::ExpandPosPlusYRotToTransform(mPosPlusYRot) directly @0x82747800. Bodied in
    // BrnTrafficLightTrigger.cpp rather than here so this header need not pull in
    // Junctions/BrnTrafficLightCollection.h (which drags BrnCoronaManager.h behind it).
    Matrix44Affine GetTransform() const;

    // :100 / :105 -- declared-only, the LightTriggerStartData precedent below. The record holds no
    // pointers, so both are no-ops on the shipped data and nothing in this tree calls them.
    void FixUp(const void* lpBase);
    void FixDown(const void* lpBase);

    // Layout pin. NEVER CALLED, in-class because the members it measures are private.
    static void _AssertLayout()
    {
        static_assert(offsetof(LightTrigger, mDimensions)  == 0x00, "LightTrigger::mDimensions");
        static_assert(offsetof(LightTrigger, mPosPlusYRot) == 0x10, "LightTrigger::mPosPlusYRot");
        static_assert(sizeof(LightTrigger) == 0x20, "sizeof(LightTrigger) -- ManageTriggers stride");
    }

private:
    Vector3     mDimensions;    // +0x00 (:117)  FULL extents -- see the banner
    Vector3Plus mPosPlusYRot;   // +0x10 (:118)  xyz == centre, w == Y rotation (radians)
};

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
