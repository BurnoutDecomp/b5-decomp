#pragma once

// ===================================================================================
// BrnProgression::DerivedCarArray
//   b5-decomp/src/GameSource/GameState/Progression/BrnDerivedCars.h
//
// The up-to-8 "derived car" list built from one parent car: element 0 is the parent
// itself, elements 1..N-1 are its sibling cars that share the parent id, with a
// PARALLEL array recording each entry's livery kind.
//
// SHAPE -- verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/GameState/Progression/BrnDerivedCars.h):
//     struct BrnProgression::DerivedCarArray : public Array<CgsID,8u> {
//       private: Array<BrnResource::VehicleListEntry::ELiveryType,8u> maLiveryTypes; // h:73
//       public:  void ConstructColourLiveryList(const VehicleList*, const CgsID&);   // h:56
//                void ConstructPatternLiveryList(const VehicleList*, const CgsID&);  // h:61
//                const CgsID GetParentId() const;                                    // h:64
//                BrnResource::VehicleListEntry::ELiveryType GetLiveryType(uint32_t) const; // h:67
//                void DEBUG_PrintArray() const;                                      // h:70
//     };
//
// LAYOUT, independently confirmed by the X360 asm (ProgressionManager::AddCar
// @0x8237A970 stack-builds one): the object is 0x70 bytes -- CgsID elements @+0x00,
// their count word @+0x40, the ELiveryType elements @+0x48, their count word @+0x68.
// Both count words are seeded with Array<T,N>'s -1 KI_UNCONSTRUCTED sentinel by the
// inlined default constructor, and both are zeroed at the top of the two Construct
// builders (0x82374F60 `*(this+64)=0; *(this+104)=0`). The struct is entirely
// pointer-free (CgsID is a u64), so the host layout reproduces the console's exactly
// -- see _AssertLayout below, which pins it WITHOUT baking a console constant.
//
// !!! WHERE THE BODIES LIVE -- read before "finishing" this header !!!
// Every function of this class was defined IN THIS HEADER in the original source, not
// in a .cpp. That is measured, not assumed: the CGS_ASSERT __FILE__ baked into the
// X360 images for all of them is
// "d:\p4\b5_main\burnout\main\code\gamesource\gamestate\Progression/BrnDerivedCars.h":
//     ConstructColourLiveryList  @0x82374F60 -> h:93, 94, 97, 112, 120, 130
//     ConstructPatternLiveryList @0x823751C0 -> h:159, 160, 163, 164, 178, 186, 196
//     GetLiveryType (inlined)                -> h:232 (via AddCar @0x8237A970 and
//                                               UnlockDerivedCarCollection @0x8237AD70)
//     DEBUG_PrintArray           @0x8236ACE8 -> h:248
// So there is NO BrnDerivedCars.cpp to link against, and the four declare-only members
// below will stay unresolved at link until someone writes their bodies HERE.
// Only GetLiveryType is reconstructed in this pass (its body needs nothing outside this
// header). The two Construct builders are NOT, deliberately: their bodies read fields of
// BrnResource::VehicleListEntry that the committed VehicleListEntry.h does not expose yet
// (the parent-car id at entry+0x08, the livery-kind byte at entry+0xE9 behind an
// IsLiveryColour() predicate) and that header is owned elsewhere. See the note on
// ELiveryType at the bottom of this file before adding them.
//
// CONSUMERS presently blocked on this type: BrnGui::PreRaceFlyByState::
// SetBurningRouteDescription @0x824C76D8 (wave J), plus the two committed HONEST PARTIALs
// that name it -- BrnCarSelectManager.cpp:473 and BrnProgressionManager.cpp:345. Those two
// "DELETE-WHEN BrnDerivedCars.h lands" notes must STAY: only the declarations landed here,
// the ~600 instructions of builder body they describe are still missing.
// ===================================================================================

#include <cstddef>   // offsetof (DerivedCarArray::_AssertLayout)

#include "types.hpp"
#include "BrnCommonTypes.h"                              // CgsID (u64)
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT (GetLiveryType bounds guard)
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N> -- the base AND the member
#include "SharedClasses/DataLists/VehicleListEntry.h"    // BrnResource::VehicleListEntry::ELiveryType

namespace BrnResource
{
    struct VehicleList;   // SharedClasses/DataLists/VehicleList.h (full home) -- by pointer only
}

namespace BrnProgression
{
    // Capacity of both parallel arrays. The NAME is attested verbatim by the assert string
    // the X360 bakes in at BrnDerivedCars.h:232 ("luIndex < KU_MAX_AMOUNT_OF_DERIVED_CARS")
    // and the value is the literal 8 that assert compares against (AddCar @0x8237A970:
    // `if (i >= 8)`), which is also the DWARF's template argument (Array<CgsID,8u>).
    // FLAG: only the name and the value are attested -- that it is a namespace-scope
    // constant rather than a class-scope one is an inference. It has to be visible before
    // the base-clause below, which rules out a member of DerivedCarArray.
    const u32 KU_MAX_AMOUNT_OF_DERIVED_CARS = 8;

    struct DerivedCarArray : public Array<CgsID, KU_MAX_AMOUNT_OF_DERIVED_CARS>
    {
        // DWARF h:56, body h:~90-140 IN THIS HEADER, emitted out-of-line @0x82374F60.
        // Builds the colour-livery family of lParentOrSiblingCarId: resolve the entry,
        // hop to its parent when the entry is itself a colour livery, clear both count
        // words, Append the parent, then Append every vehicle-list entry whose parent id
        // matches AND whose livery kind is one of the colour kinds. Parameter names are
        // the X360 assert strings (h:93 "lpVehicleList", h:94
        // "lParentOrSiblingCarId != kCGSID_NULL").
        void ConstructColourLiveryList(const BrnResource::VehicleList* lpVehicleList,
                                       const CgsID& lParentOrSiblingCarId);

        // DWARF h:61, body h:~156-205 IN THIS HEADER, emitted out-of-line @0x823751C0.
        // Same walk for the PATTERN livery kind; additionally asserts the seed entry is
        // not a colour livery (h:164 "!lpVehicleListEntry->IsLiveryColour()").
        void ConstructPatternLiveryList(const BrnResource::VehicleList* lpVehicleList,
                                        const CgsID& lParentOrSiblingCarId);

        // DWARF h:64 (returns CgsID by value, const-qualified as spelled). Body NOT
        // recovered: unlike the others it is never emitted out-of-line and never shows up
        // inlined with an assert of its own, so nothing pins its text. It is presumably
        // element 0 -- both builders Append the parent id first, and DEBUG_PrintArray
        // prints GetItem(0) as " Parent Car: " before looping from index 1 -- but that is
        // an inference, so it is left declared only rather than guessed at.
        const CgsID GetParentId() const;

        // DWARF h:67, body h:~230-234 IN THIS HEADER. Reconstructed: the X360 inlines it
        // at both ProgressionManager call sites as "assert the index is inside the FIXED
        // capacity (h:232), then hand off to the livery array's own checked accessor"
        // (AddCar @0x8237A970 / UnlockDerivedCarCollection @0x8237AD70: the `i >= 8`
        // guard, then a call to Array<ELiveryType,8>::GetItem(&maLiveryTypes, i) whose
        // result is dereferenced). Note the guard is against the CAPACITY, not the live
        // count -- the count check is the one GetItem does for itself.
        BrnResource::VehicleListEntry::ELiveryType GetLiveryType(u32 luIndex) const
        {
            CGS_ASSERT(luIndex < KU_MAX_AMOUNT_OF_DERIVED_CARS,
                       "luIndex < KU_MAX_AMOUNT_OF_DERIVED_CARS");
            return maLiveryTypes.GetItem(luIndex);
        }

        // DWARF h:70, body h:~246-262 IN THIS HEADER, emitted out-of-line @0x8236ACE8.
        // Debug-only dump ("--------\n", " Parent Car: <id>\n", then "  [i] Car: <id>
        // LiveryType: <kind>\n" per entry) through CgsDev::Log::gpDebugPrint, gated on
        // CgsDev::Message::gxMessageFilterFlags bit 0, after asserting GetLength() != 0
        // (h:248). NOT reconstructed here -- it needs the CgsDev::Message stream helpers
        // and has no caller in the tree. Declared so the class shape stays the DWARF's.
        void DEBUG_PrintArray() const;

    private:
        // DWARF h:73. X360 @+0x48 (elements) / @+0x68 (count word).
        Array<BrnResource::VehicleListEntry::ELiveryType, KU_MAX_AMOUNT_OF_DERIVED_CARS> maLiveryTypes;

        // Never called. offsetof on a private member is legal here (member-function body
        // == complete-class context). RELATIVE pin only: the livery array must start
        // immediately after the CgsID base sub-object, with no host-side gap. On the X360
        // that is +0x48 == 0x40 elements + a 4-byte count word rounded up to the u64
        // alignment; asserting it against sizeof(base) rather than against 0x48 keeps the
        // console number out of the host arithmetic.
        static void _AssertLayout()
        {
            static_assert(sizeof(CgsID) == 8,
                          "CgsID is the 64-bit packed id (BrnCommonTypes.h) -- the whole "
                          "struct is pointer-free, so host layout == console layout");
            static_assert(offsetof(DerivedCarArray, maLiveryTypes)
                              == sizeof(Array<CgsID, KU_MAX_AMOUNT_OF_DERIVED_CARS>),
                          "maLiveryTypes must abut the CgsID base sub-object (X360: the "
                          "livery elements sit at base+0x48, their count word at +0x68)");
        }
    };
}

// ---------------------------------------------------------------------------------------
// FLAG for whoever writes the two builder bodies -- DO NOT trust the enumerator VALUES in
// BrnResource::VehicleListEntry::ELiveryType (VehicleListEntry.h) as they stand today. That
// enum is commented there as "modelled" (colour=0, pattern=1, count=2), but the X360 livery
// kind byte read from entry+0xE9 is tested against 1, 2, 3 and 4:
//     0x82374F60  colour build : kind == 1 || kind == 3 || kind == 4  -> IsLiveryColour()
//     0x823751C0  pattern build: kind == 2                            -> the pattern kind
//     0x8237A970  AddCar       : kind == 4                            -> the "silver" cars
// so the real enumeration has at least five values and 0 is not the colour kind. Fixing it
// belongs to the VehicleListEntry.h owner, not here; recorded so the discrepancy is not
// discovered the hard way inside a builder body.
// ---------------------------------------------------------------------------------------
