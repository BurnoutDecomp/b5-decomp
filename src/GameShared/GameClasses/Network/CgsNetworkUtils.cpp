#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT

#include <cstdio>   // sscanf

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork network utilities)
//
//   IPAddressIntToString @ 0x8286EFF8:
//     The asm unpacks the packed address into four unsigned bytes:
//       b0 = liIPAddress >> 24            (srwi r4,24)
//       b1 = (liIPAddress >> 16) & 0xFF   (srawi r4,0x10 ; clrlwi ,24)
//       b2 = (liIPAddress >>  8) & 0xFF   (srawi r4,8    ; clrlwi ,24)
//       b3 =  liIPAddress        & 0xFF   (clrlwi r4,24)
//     asserts each byte is in [0, 255] (the masks make that vacuously true -- the
//     CGS_ASSERT substitution preserves the eight original range checks at lines
//     0x96..0x9D), then formats "%d.%d.%d.%d" into a 16-byte buffer via
//     CgsCore::SPrintf(buffer, 16, "%d.%d.%d.%d", b0, b1, b2, b3).
//
//   IPAddressStringToInt: the inverse -- parse "a.b.c.d" and repack big-endian. The
//     X360 build asserts the parse yielded 4 fields each in [0,255]; reproduced with
//     the CRT-portable sscanf the leaf uses.

namespace CgsNetwork
{
    s32 IPAddressStringToInt(const char* lpacIPAddress)
    {
        s32 laiIPAddress[4];
        s32 liNumItems;

        liNumItems = sscanf(lpacIPAddress, "%d.%d.%d.%d",
                            &laiIPAddress[0], &laiIPAddress[1],
                            &laiIPAddress[2], &laiIPAddress[3]);

        CGS_ASSERT(liNumItems == 4,        "liNumItems == 4");
        CGS_ASSERT(laiIPAddress[0] >= 0,   "laiIPAddress[0] >= 0");
        CGS_ASSERT(laiIPAddress[0] <= 255, "laiIPAddress[0] <= 255");
        CGS_ASSERT(laiIPAddress[1] >= 0,   "laiIPAddress[1] >= 0");
        CGS_ASSERT(laiIPAddress[1] <= 255, "laiIPAddress[1] <= 255");
        CGS_ASSERT(laiIPAddress[2] >= 0,   "laiIPAddress[2] >= 0");
        CGS_ASSERT(laiIPAddress[2] <= 255, "laiIPAddress[2] <= 255");
        CGS_ASSERT(laiIPAddress[3] >= 0,   "laiIPAddress[3] >= 0");
        CGS_ASSERT(laiIPAddress[3] <= 255, "laiIPAddress[3] <= 255");

        return (laiIPAddress[0] * 0x01000000) +
               (laiIPAddress[1] * 0x00010000) +
               (laiIPAddress[2] * 0x00000100) +
               (laiIPAddress[3] * 0x00000001);
    }

    void IPAddressIntToString(char* lacIPAddress, s32 liIPAddress)
    {
        const u32 lu32IPAddress = static_cast<u32>(liIPAddress);

        // Unsigned byte extraction, matching the srwi/clrlwi byte unpacks in the asm.
        const s32 liByte0 = static_cast<s32>((lu32IPAddress >> 24) & 0xFF);
        const s32 liByte1 = static_cast<s32>((lu32IPAddress >> 16) & 0xFF);
        const s32 liByte2 = static_cast<s32>((lu32IPAddress >>  8) & 0xFF);
        const s32 liByte3 = static_cast<s32>( lu32IPAddress        & 0xFF);

        CGS_ASSERT(liByte0 >= 0,   "laiIPAddress[0] >= 0");
        CGS_ASSERT(liByte0 <= 255, "laiIPAddress[0] <= 255");
        CGS_ASSERT(liByte1 >= 0,   "laiIPAddress[1] >= 0");
        CGS_ASSERT(liByte1 <= 255, "laiIPAddress[1] <= 255");
        CGS_ASSERT(liByte2 >= 0,   "laiIPAddress[2] >= 0");
        CGS_ASSERT(liByte2 <= 255, "laiIPAddress[2] <= 255");
        CGS_ASSERT(liByte3 >= 0,   "laiIPAddress[3] >= 0");
        CGS_ASSERT(liByte3 <= 255, "laiIPAddress[3] <= 255");

        CgsCore::SPrintf(lacIPAddress, KI_IPADDRESS_STRING_LENGTH, "%d.%d.%d.%d",
                         liByte0, liByte1, liByte2, liByte3);
    }
}
