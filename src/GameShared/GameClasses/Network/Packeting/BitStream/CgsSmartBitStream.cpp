#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsSmartBitStream.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdint>   // uintptr_t

// Reconstructed from BURNOUT_X360_*.XEX
//   CgsNetwork::SmartBitStream::AddRawData @ 0x82882ED8 — copy a raw byte block
//                                                         into the bitstream:
//                                                         emit a byte head until
//                                                         the data pointer is
//                                                         8-byte aligned, then a
//                                                         u64 bulk, then a byte
//                                                         tail. Returns false on
//                                                         the first AddBits
//                                                         overflow.
//   CgsNetwork::SmartBitStream::GetRawData @ 0x82883028 — inverse: read the
//                                                         block back with the
//                                                         same head/bulk/tail
//                                                         split keyed on the
//                                                         destination pointer's
//                                                         alignment.
//
// GetRawData was reconstructed from the decoded algorithm rather than the X360
// pseudocode: Hex-Rays emitted it with "local variable allocation has failed,
// the output may be wrong!" (register-mangled). It is the byte-identical inverse
// of AddRawData — a u64 read-then-write preserves all 8 bytes regardless of host
// endianness, and the head loop aligns on the DATA pointer's absolute address
// exactly as the X360 does (the sole caller, CgsNetwork::Message::
// PackOrUnpackBuffer, pairs pack/unpack with a matching buffer layout, so the
// alignment is symmetric and the round-trip is exact).

namespace CgsNetwork
{
    bool SmartBitStream::AddRawData(const char* lpcData, s32 liNumBytes)
    {
        CGS_ASSERT(lpcData != nullptr, "lpcData != NULL");
        CGS_ASSERT(liNumBytes >= 0, "liNumBytes >= 0");

        const char* lpcRemaining = lpcData;

        // Head: emit leading bytes until the data pointer is 8-byte aligned (or
        // the data is exhausted).
        while (liNumBytes > 0 && (reinterpret_cast<uintptr_t>(lpcRemaining) & 7) != 0)
        {
            if (!AddByte(static_cast<u8>(*lpcRemaining)))
            {
                return false;
            }
            --liNumBytes;
            ++lpcRemaining;
        }

        // Bulk: 64 bits at a time from the now-aligned pointer.
        const u64* lpuData = reinterpret_cast<const u64*>(lpcRemaining);
        while (liNumBytes >= 8)
        {
            if (!AddUInt(*lpuData))
            {
                return false;
            }
            ++lpuData;
            liNumBytes -= 8;
        }
        lpcRemaining = reinterpret_cast<const char*>(lpuData);

        // Tail: remaining bytes.
        while (liNumBytes > 0)
        {
            if (!AddByte(static_cast<u8>(*lpcRemaining)))
            {
                return false;
            }
            --liNumBytes;
            ++lpcRemaining;
        }

        CGS_ASSERT(liNumBytes == 0, "liNumBytes == 0");
        return true;
    }

    void SmartBitStream::GetRawData(char* lpcBuffer, s32 liNumBytes)
    {
        CGS_ASSERT(lpcBuffer != nullptr, "lpcBuffer != NULL");
        CGS_ASSERT(liNumBytes >= 0, "liNumBytes >= 0");

        char* lpcDst = lpcBuffer;

        // Head: read leading bytes until the dest pointer is 8-byte aligned.
        while (liNumBytes > 0 && (reinterpret_cast<uintptr_t>(lpcDst) & 7) != 0)
        {
            *lpcDst = static_cast<char>(GetByte());
            ++lpcDst;
            --liNumBytes;
        }

        // Bulk: 64 bits at a time into the now-aligned pointer.
        u64* lpuDst = reinterpret_cast<u64*>(lpcDst);
        while (liNumBytes >= 8)
        {
            *lpuDst = GetUInt();
            ++lpuDst;
            liNumBytes -= 8;
        }
        lpcDst = reinterpret_cast<char*>(lpuDst);

        // Tail: remaining bytes.
        while (liNumBytes > 0)
        {
            *lpcDst = static_cast<char>(GetByte());
            ++lpcDst;
            --liNumBytes;
        }
    }
}
