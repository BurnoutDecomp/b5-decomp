#pragma once

#include "types.hpp"

// CgsResource::ID - a resource's identity: a single 64-bit hash. Bundles store one per resource entry +
// import; the loader/manager look resources up by it. Shape taken from the Feb-2007 partial source
// (GameShared/GameClasses/System/Resource/CgsResourceID.h), which the X360 spine matches for this type.
//
// RECONCILE: the X360/Feb-2007 runtime id-hashers are u64 (Construct -> HashString(const RwChar*) /
// HashData(const u8*, u32)). The static HashString below is the existing reconstruction and returns a
// 32-bit CRC - kept for its current callers. Reconstructing the real u64 hashers (+ Construct) from the
// X360 is a follow-on; it is the load-by-name path, not needed to READ ids already stored in a bundle
// (that only needs the value + GetHash/SetHash).

namespace CgsResource
{
    class ID
    {
        typedef u64 CHash;

    public:
        ID() {}

        u64  GetHash() const     { return mHash; }
        void SetHash(u64 luHash) { mHash = luHash; }

        static s32 HashString(const u8* lpString);   // existing 32-bit CRC hasher (see RECONCILE note)

        friend bool operator==(ID lLeft, ID lRight) { return lLeft.mHash == lRight.mHash; }
        friend bool operator!=(ID lLeft, ID lRight) { return lLeft.mHash != lRight.mHash; }
        friend bool operator< (ID lLeft, ID lRight) { return lLeft.mHash <  lRight.mHash; }
        friend bool operator> (ID lLeft, ID lRight) { return lLeft.mHash >  lRight.mHash; }
        friend bool operator<=(ID lLeft, ID lRight) { return lLeft.mHash <= lRight.mHash; }
        friend bool operator>=(ID lLeft, ID lRight) { return lLeft.mHash >= lRight.mHash; }

    private:
        CHash mHash;
    };
}
