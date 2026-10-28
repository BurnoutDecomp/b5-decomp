#pragma once

#include "types.hpp"

// CgsResource::ID - a resource's identity: a single 64-bit hash. Bundles store one per resource entry +
// import; the loader/manager look resources up by it. Shape taken from the Feb-2007 partial source
// (GameShared/GameClasses/System/Resource/CgsResourceID.h), which the X360 spine matches for this type.
//
// ARTIST HashString828D84A8 returns the low CRC32 zero-extended in its
// 64-bit result (clrldi r3,r11,32 at828D8550, and the same empty-string tail).
// DecFIGS CgsResourceID.h:80 declares that result uint64_t. Retain the full
// unsigned identity when callers store it directly into a request's u64 ID.
// The existing static byte-string entry point represents that outlined hasher;
// Construct/HashData API recovery remains outside this correction.

namespace CgsDev { struct StrStreamBase; }

namespace CgsResource
{
    class ID
    {
        typedef u64 CHash;

    public:
        ID() {}

        u64  GetHash() const     { return mHash; }
        void SetHash(u64 luHash) { mHash = luHash; }

        static u64 HashString(const u8* lpString);   // original unsigned64 result, zero-extended CRC32

        friend bool operator==(ID lLeft, ID lRight) { return lLeft.mHash == lRight.mHash; }
        friend bool operator!=(ID lLeft, ID lRight) { return lLeft.mHash != lRight.mHash; }
        friend bool operator< (ID lLeft, ID lRight) { return lLeft.mHash <  lRight.mHash; }
        friend bool operator> (ID lLeft, ID lRight) { return lLeft.mHash >  lRight.mHash; }
        friend bool operator<=(ID lLeft, ID lRight) { return lLeft.mHash <= lRight.mHash; }
        friend bool operator>=(ID lLeft, ID lRight) { return lLeft.mHash >= lRight.mHash; }

    private:
        CHash mHash;
    };

    // @ 0x826801F0 -- stream-insert a resource ID into a CgsDev::StrStreamBase as 16 lowercase
    // hex digits (the 64-bit hash, most-significant byte first). Bodied in CgsResourceID.cpp.
    CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lStream, ID lId);
}
