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

    return Attrib::StringToKey(lacTemp, static_cast<u32>(std::strlen(lacTemp)),
                               0xABCDEF0011223344ULL);
}

}
