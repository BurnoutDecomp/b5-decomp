#pragma once

#include "types.hpp"
#include <cstddef>

namespace CgsResource { struct GuiGeometryObject; }

namespace CgsGui
{
    // Native little-endian x64 AptData resource header. XB1 FixUp/FixDown
    // sub_1400CD0E0/sub_1400CD110 relocate the five pointer-sized slots +0..+0x20;
    // LoadAnimation sub_1400C90F0 loads +0x10/+0x18 and stores ACTIVE at +0x2C.
    // The current resource adapter keeps pointer slots as resource-relative offsets.
    struct AptDataHeader
    {
        enum EAptDataState
        {
            E_APTDATASTATE_LOADING = 0,
            E_APTDATASTATE_LOADED  = 1,
            E_APTDATASTATE_ACTIVE  = 2,
        };

        u64 mpacMovieName;   // +0x00 serialised const char*
        u64 mpacBaseName;    // +0x08 serialised const char*
        u64 mpAptData;       // +0x10 serialised movie data
        u64 mpConstData;     // +0x18 serialised constant data
        u64 mpGeomStruct;    // +0x20 serialised GuiGeometryObject*
        u32 muSizeOfHeader;  // +0x28 resource span size
        u32 meCurrentState;  // +0x2C EAptDataState

        void FixUp(u32 luDelta);
        void FixDown(u32 luDelta, bool lbEndianSwap);
    };
    static_assert(offsetof(AptDataHeader, mpAptData) == 0x10, "AptDataHeader.data");
    static_assert(offsetof(AptDataHeader, mpConstData) == 0x18, "AptDataHeader.const");
    static_assert(offsetof(AptDataHeader, mpGeomStruct) == 0x20, "AptDataHeader.geometry");
    static_assert(offsetof(AptDataHeader, muSizeOfHeader) == 0x28, "AptDataHeader.size");
    static_assert(offsetof(AptDataHeader, meCurrentState) == 0x2C, "AptDataHeader.state");
    static_assert(sizeof(AptDataHeader) == 0x30, "AptDataHeader native x64 size");
}
