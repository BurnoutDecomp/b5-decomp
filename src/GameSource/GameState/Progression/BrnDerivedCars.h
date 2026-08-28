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
// So there is NO BrnDerivedCars.cpp to link against; bodies land HERE.
// [map arm 2026-08-27] BOTH Construct builders are now bodied in this header:
// VehicleListEntry.h grew the faces they needed (IsLiveryColour() over the corrected
// ELiveryType values; GetParentId/GetLiveryType were already committed), and the
// BrnMainMapLinkGates.cpp stand-in for ConstructPatternLiveryList died with the change.
// Still declaration-only: GetParentId (element-0 inference unproven, see its note) and
// DEBUG_PrintArray (needs the CgsDev::Message stream helpers; no caller in the tree).
//
// CONSUMERS now servable: BrnGui::PreRaceFlyByState::SetBurningRouteDescription
// @0x824C76D8 (wave J), plus the two committed HONEST PARTIALs that name this class --
// BrnCarSelectManager.cpp:473 and BrnProgressionManager.cpp:345 (their notes can retire
// when those TUs are next touched).
// ===================================================================================

#include <cstddef>   // offsetof (DerivedCarArray::_AssertLayout)

#include "types.hpp"
#include "BrnCommonTypes.h"                              // CgsID (u64)
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT (GetLiveryType bounds guard)
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N> -- the base AND the member
#include "SharedClasses/DataLists/VehicleListEntry.h"    // BrnResource::VehicleListEntry::ELiveryType
// [map arm 2026-08-27] the two builder bodies below walk the vehicle list, so the full
// type is required -- consistent with the original header-resident bodies (the CGS_ASSERT
// __FILE__ evidence in the banner), which could not have compiled against a fwd-decl.
#include "SharedClasses/DataLists/VehicleList.h"         // BrnResource::VehicleList (the builder walks)

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
        // [map arm 2026-08-27] BODIED (the BrnMainMapLinkGates stand-in died with it).
        // Builds the colour-livery family of lParentOrSiblingCarId: resolve the seed
        // entry, hop to its parent when the seed is itself a colour livery, clear both
        // parallel arrays, Append the parent (id + its own livery kind), then Append
        // every vehicle-list entry whose parent id matches AND whose livery kind is in
        // the colour set. Parameter/assert text verbatim from the X360 (h:93/94/97/112/
        // 120/130); the console file path in those asserts is the original P4 one -- the
        // house CGS_ASSERT carries the expression strings only.
        void ConstructColourLiveryList(const BrnResource::VehicleList* lpVehicleList,
                                       const CgsID& lParentOrSiblingCarId)
        {
            CGS_ASSERT(lpVehicleList != 0, "lpVehicleList");                          // h:93
            CGS_ASSERT(lParentOrSiblingCarId != 0, "lParentOrSiblingCarId != kCGSID_NULL");   // h:94

            // Resolve the seed entry (index < 0 and a null row both fire h:97).
            const s32 liSeedIndex = lpVehicleList->GetVehicleIndex(lParentOrSiblingCarId);
            const BrnResource::VehicleListEntry* lpVehicleListEntry =
                (liSeedIndex >= 0) ? lpVehicleList->GetVehicleData(liSeedIndex) : 0;
            CGS_ASSERT(lpVehicleListEntry != 0, "lpVehicleListEntry");                // h:97

            // A colour-livery seed hops to its parent; a base car IS the parent.
            const CgsID lParentCarId = lpVehicleListEntry->IsLiveryColour()
                                           ? lpVehicleListEntry->GetParentId()
                                           : lParentOrSiblingCarId;
            CGS_ASSERT(lParentCarId != 0, "lParentCarId != kCGSID_NULL");             // h:112

            Clear();                 // the CgsID array count   (X360 `*(this+64)=0`)
            maLiveryTypes.Clear();   // the parallel kind array (X360 `*(this+104)=0`)

            // Resolve the PARENT's entry (h:120), then seed both arrays with it.
            const s32 liParentIndex = lpVehicleList->GetVehicleIndex(lParentCarId);
            const BrnResource::VehicleListEntry* lpParentEntry =
                (liParentIndex >= 0) ? lpVehicleList->GetVehicleData(liParentIndex) : 0;
            CGS_ASSERT(lpParentEntry != 0, "lpVehicleListEntry");                     // h:120
            Append(lParentCarId);
            maLiveryTypes.Append(static_cast<BrnResource::VehicleListEntry::ELiveryType>(
                lpParentEntry->GetLiveryType()));

            // Every entry parented on lParentCarId whose kind is in the colour set.
            for (s32 liEntry = 0;
                 liEntry < lpVehicleList->GetVehicleCount(); ++liEntry)
            {
                const BrnResource::VehicleListEntry* lpEntry =
                    lpVehicleList->GetVehicleData(liEntry);
                CGS_ASSERT(lpEntry != 0, "lpVehicleListEntry");                       // h:130
                if (lpEntry->GetParentId() == lParentCarId && lpEntry->IsLiveryColour())
                {
                    Append(lpEntry->GetId());
                    maLiveryTypes.Append(static_cast<BrnResource::VehicleListEntry::ELiveryType>(
                        lpEntry->GetLiveryType()));
                }
            }
        }

        // DWARF h:61, body h:~156-205 IN THIS HEADER, emitted out-of-line @0x823751C0.
        // [map arm 2026-08-27] BODIED. Same walk for the PATTERN livery kind (== 2);
        // additionally asserts the seed entry is not a colour livery (h:164), and the
        // parent hop keys on the seed being a PATTERN livery rather than a colour one.
        void ConstructPatternLiveryList(const BrnResource::VehicleList* lpVehicleList,
                                        const CgsID& lParentOrSiblingCarId)
        {
            CGS_ASSERT(lpVehicleList != 0, "lpVehicleList");                          // h:159
            CGS_ASSERT(lParentOrSiblingCarId != 0, "lParentOrSiblingCarId != kCGSID_NULL");   // h:160

            const s32 liSeedIndex = lpVehicleList->GetVehicleIndex(lParentOrSiblingCarId);
            const BrnResource::VehicleListEntry* lpVehicleListEntry =
                (liSeedIndex >= 0) ? lpVehicleList->GetVehicleData(liSeedIndex) : 0;
            CGS_ASSERT(lpVehicleListEntry != 0, "lpVehicleListEntry");                // h:163
            CGS_ASSERT(!lpVehicleListEntry->IsLiveryColour(),
                       "!lpVehicleListEntry->IsLiveryColour()");                      // h:164

            const CgsID lParentCarId =
                (lpVehicleListEntry->GetLiveryType() ==
                 BrnResource::VehicleListEntry::E_LIVERY_TYPE_PATTERN)
                    ? lpVehicleListEntry->GetParentId()
                    : lParentOrSiblingCarId;
            CGS_ASSERT(lParentCarId != 0, "lParentCarId != kCGSID_NULL");             // h:178

            Clear();                 // X360 `*(this+64)=0`
            maLiveryTypes.Clear();   // X360 `*(this+104)=0`

            const s32 liParentIndex = lpVehicleList->GetVehicleIndex(lParentCarId);
            const BrnResource::VehicleListEntry* lpParentEntry =
                (liParentIndex >= 0) ? lpVehicleList->GetVehicleData(liParentIndex) : 0;
            CGS_ASSERT(lpParentEntry != 0, "lpVehicleListEntry");                     // h:186
            Append(lParentCarId);
            maLiveryTypes.Append(static_cast<BrnResource::VehicleListEntry::ELiveryType>(
                lpParentEntry->GetLiveryType()));

            for (s32 liEntry = 0;
                 liEntry < lpVehicleList->GetVehicleCount(); ++liEntry)
            {
                const BrnResource::VehicleListEntry* lpEntry =
                    lpVehicleList->GetVehicleData(liEntry);
                CGS_ASSERT(lpEntry != 0, "lpVehicleListEntry");                       // h:196
                if (lpEntry->GetParentId() == lParentCarId
                    && lpEntry->GetLiveryType() ==
                           BrnResource::VehicleListEntry::E_LIVERY_TYPE_PATTERN)
                {
                    Append(lpEntry->GetId());
                    maLiveryTypes.Append(static_cast<BrnResource::VehicleListEntry::ELiveryType>(
                        lpEntry->GetLiveryType()));
                }
            }
        }

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
// (The old tail FLAG about ELiveryType's wrong modelled values is PAID OFF -- [map arm
// 2026-08-27] VehicleListEntry.h now carries the corrected enumeration ({1,3,4} = the
// colour set behind IsLiveryColour(), 2 = pattern, 4 = the AddCar "silver" arm) with the
// evidence table in its own banner.)
// ---------------------------------------------------------------------------------------
