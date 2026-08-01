#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysCollectionKey.h"

#include <cstring>   // std::strlen (the inlined length walk)

#include "rw/core/stdc/stdc.h"   // rw::core::stdc::ConvertI64ToA

// CgsAttribSys::AttribSysCollectionKey -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameShared/GameClasses/System/AttribSys/CgsAttribSysCollectionKey.cpp):
//   AttribSysCollectionKey::GetHashKey @0x82805C20

namespace CgsAttribSys
{

// @ 0x82805C20 -- print the 64-bit asset GUID as decimal text, then hash the text
// with the attribsys string hash (the standard 0xABCDEF00_11223344 seed the X360
// stages at the call site). An empty print yields key 0.
u64 AttribSysCollectionKey::GetHashKey() const
{
    char lacTemp[512];   // DWARF cpp:81
    rw::core::stdc::ConvertI64ToA(miAssetGuid, lacTemp, 10);

    if (lacTemp[0] == '\0')
        return 0;

    // ⭐ NARROWING RETIRED 2026-08-01 (physics wave 1). This used to return
    // `static_cast<Attribute::Key>(...)` -- a 32-bit truncation -- with a FLAG saying the
    // honest width was 64 but that Attribute::Key "is ALSO the type of 4-byte serialised
    // event payload fields (... BrnVehicleEvents.h at @960 ...)" and so could not be widened.
    // ⛔ THAT REASON IS DISPROVED for every field this function's value actually reaches.
    // VehicleOutputInterface::UpdateRaceCarState @0x825EC808 stores RaceCarState's key with
    // `std` (8 bytes at state+960), CreateRaceCarEvent::operator= @0x822AE040 and
    // CreatePhysicalTrafficEvent::operator= @0x825B7AE8 both copy theirs with a single
    // `ld/std` at event+0x70, and CreateArticulatedTrafficEvent::operator= @0x8270BF70 copies
    // its two with `ld/std` at +0xD0/+0xD8. All four are now spelled u64.
    // ⚠️ AND THE TRUNCATION WAS LIVE: this key is the COLLECTION KEY that
    // VehicleManager::ProcessCreateEvents @0x82616770 feeds to Attrib::FindCollection
    // alongside hash64("burnoutcarasset") == 0x52B81656F3ADF675. FindCollection hashes the
    // whole doubleword, so a 32-bit key can never match -- the same defect that already cost
    // this project the class-key and Attrib::StringToKey incidents.
    // The X360 body @0x82805C20 tail-calls the hash and returns r3 whole, which is exactly
    // what this now does. (The ::Attribute::Key typedef itself is left at u32: the 4-byte
    // claim is still untested for BrnDirectorEvents.h:32 and BrnMessageData.h.)
    return Attrib::StringToKey(lacTemp, static_cast<u32>(std::strlen(lacTemp)),
                               0xABCDEF0011223344ULL);
}

}
