#pragma once

#include "types.hpp"

// CgsGui::OpaqueBuffer -- a typeless {pointer, byte-size} view over a caller-owned
// object, used by the save/load vocabulary to describe the raw save image a
// SaveLoadMetadata points at. Shape + names from the DecFIGS DWARF
// (GameShared/GameClasses/Gui/CgsOpaqueBuffer.h:60; the DWARF also attests the
// MakeOpaqueBuffer<BrnGui::ProfileManager::ProfileStoredData> instantiation the
// profile manager uses to publish its 256KB stored-data image).
//
// On the X360 the pair is {ptr @+0, size @+4}; the pointer widens on the x64 host
// and all access is by name.

namespace CgsGui
{
    struct OpaqueBuffer
    {
        typedef u8 Byte;                 // CgsOpaqueBuffer.h:49

        Byte* mpData;                    // CgsOpaqueBuffer.h:61 (X360 +0x00)
        u32   miSize;                    // CgsOpaqueBuffer.h:62 (X360 +0x04) [DWARF name kept]
    };

    // CgsOpaqueBuffer.h:76 -- wrap lrObject as an OpaqueBuffer ({&lrObject, sizeof}).
    // The sizeof() keeps the published size correct on the widened x64 host.
    template <typename BufferType>
    OpaqueBuffer MakeOpaqueBuffer(BufferType& lrObject)
    {
        OpaqueBuffer lBuffer;
        lBuffer.mpData = reinterpret_cast<OpaqueBuffer::Byte*>(&lrObject);
        lBuffer.miSize = static_cast<u32>(sizeof(BufferType));
        return lBuffer;
    }
}
