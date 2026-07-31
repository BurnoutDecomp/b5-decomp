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
Attribute::Key AttribSysCollectionKey::GetHashKey() const
{
    char lacTemp[512];   // DWARF cpp:81
    rw::core::stdc::ConvertI64ToA(miAssetGuid, lacTemp, 10);

    if (lacTemp[0] == '\0')
        return 0;

    // FLAG (2026-07-31): Attrib::StringToKey returns the full 64-bit hash now that its
    // fabricated narrowing is gone, but this function's declared return type is
    // Attribute::Key, which this tree still typedefs to u32. The X360 body @0x82805C20
    // tail-calls the hash and returns r3 whole, so the honest return type here is u64 --
    // BUT Attribute::Key is ALSO the type of 4-byte serialised event payload fields
    // (BrnDirectorEvents.h:32 at +0x00, BrnVehicleEvents.h at @960, BrnMessageData.h's
    // DWARF-attested 12-byte struct), so one typedef is standing for two different widths
    // and cannot simply be widened. Narrowing is explicit here until that split is done.
    return static_cast<Attribute::Key>(
        Attrib::StringToKey(lacTemp, static_cast<u32>(std::strlen(lacTemp)),
                            0xABCDEF0011223344ULL));
}

}
