#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase::operator<<(const char*)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"        // CgsCore::SPrintf

// CgsResource::ID::HashString -- faithful to ARTIST 0x828D84A8: a reflected CRC32 (polynomial 0xEDB88320,
// init 0xFFFFFFFF, final bitwise-NOT) over the string LOWERCASED (A-Z -> a-z first). The X360 uses a
// precomputed table (dword_820F71F0 = the standard reflected CRC32 table); the bit-by-bit form here is
// numerically identical and self-contained (the table's definition was never carried over). Resource ids
// are therefore case-insensitive name hashes -- VIDEOLIST.BUNDLE's VideoDataResource ids are HashString of
// the (lowercased) video names (e.g. "eafranchise", "criterion").
//
// The ASM only asserts on a NULL lpcString (CGS_ASSERT below) -- it does NOT early-return for NULL, and
// falls through to dereference it regardless (matches the project's CGS_ASSERT idiom: log-and-continue).
// The empty-string case ("" -> luChar == 0 on the first iteration) IS a genuine early-out in the ASM
// (loc_828D8568 returns the untouched initial r9 == 0), so that check stays.

namespace CgsResource
{
    s32 ID::HashString(const u8* lpString)
    {
        CGS_ASSERT(lpString != 0, "lpcString != NULL");

        if (*lpString == 0)
            return 0;

        u32 luCrc = 0xFFFFFFFFu;
        for (u8 luChar = *lpString; luChar; luChar = *++lpString)
        {
            if (luChar >= 'A' && luChar <= 'Z')
                luChar = static_cast<u8>(luChar + ('a' - 'A'));

            luCrc ^= luChar;
            for (s32 li = 0; li < 8; ++li)
                luCrc = (luCrc >> 1) ^ (0xEDB88320u & (0u - (luCrc & 1u)));
        }
        return static_cast<s32>(~luCrc);
    }

    // CgsResource::operator<< -- faithful to X360 0x826801F0. Stream-inserts a resource ID into a
    // CgsDev::StrStreamBase as 16 lowercase hex digits: the 8 bytes of the 64-bit hash emitted
    // most-significant byte first (the X360 spills the u64 id with a big-endian `std`, then reads
    // the spilled bytes [0]..[7] in ascending memory order -- byte[0] is the MSB). Each byte is
    // formatted with "%02x" via CgsCore::SPrintf into a 32-byte stack buffer, then the string is
    // forwarded to the stream's virtual char* sink (vtable+4, StrStreamBase::operator<<(const char*)).
    // We extract the bytes MSB-first by shift so the output is byte-identical on a little-endian PC.
    CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lStream, ID lId)
    {
        const u64 luHash = lId.GetHash();
        char lacBuffer[32];
        CgsCore::SPrintf(
            lacBuffer, 32, "%02x%02x%02x%02x%02x%02x%02x%02x",
            static_cast<u32>((luHash >> 56) & 0xFF),
            static_cast<u32>((luHash >> 48) & 0xFF),
            static_cast<u32>((luHash >> 40) & 0xFF),
            static_cast<u32>((luHash >> 32) & 0xFF),
            static_cast<u32>((luHash >> 24) & 0xFF),
            static_cast<u32>((luHash >> 16) & 0xFF),
            static_cast<u32>((luHash >>  8) & 0xFF),
            static_cast<u32>((luHash      ) & 0xFF));
        lStream << lacBuffer;
        return lStream;
    }
}
