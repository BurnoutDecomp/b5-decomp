#include "BrnNetworkGameSearchParams.h"
#include <string.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::GameSearchParamsBase::operator=  @ 0x8255EA80

namespace BrnNetwork
{
    // operator= (asm @ 0x8255EA80):
    //   bl  CgsNetwork__ServerInterfaceGameSearchParamsX360   -> base copy-assign
    //   memcpy(this+0x6C, rhs+0x6C, 0x2A0)                     -> bulk payload
    //   0x83-byte lbzx/stb loop over this+0x30C from rhs+0x30C -> trailing block
    GameSearchParamsBase& GameSearchParamsBase::operator=(const GameSearchParamsBase& lrhs)
    {
        // 1. Base subobject copy-assignment (the X360 search-params block).
        static_cast<CgsNetwork::ServerInterfaceGameSearchParams&>(*this) =
            static_cast<const CgsNetwork::ServerInterfaceGameSearchParams&>(lrhs);

        // 2. Bulk game-side block: memcpy(this+0x6C, rhs+0x6C, 0x2A0).
        memcpy(maGameSearchPayload, lrhs.maGameSearchPayload, sizeof(maGameSearchPayload));

        // 3. Trailing block: 0x83 bytes copied one at a time (asm lbzx/stb loop).
        for (s32 li = 0; li < static_cast<s32>(sizeof(maGameSearchTail)); ++li)
            maGameSearchTail[li] = lrhs.maGameSearchTail[li];

        return *this;
    }
}
