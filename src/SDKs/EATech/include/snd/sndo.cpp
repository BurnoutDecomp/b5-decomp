// ============================================================================
// SDKs/EATech/include/snd/sndo.cpp
//
// Out-of-line definitions for the minimal Snd9 sndo.h surface:
//   - Snd9::IAemsSamplePlayer::~IAemsSamplePlayer  (base scalar-deleting dtor)
//       reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82681950
//   - Snd9::AemsPlayerInputAccessor::GetValueByType
//       reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82B6FE00
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/EATech/include/snd/sndo.h"

namespace Snd9
{
    // 0x82681950. The base polymorphic destructor. The X360 thunk stores the
    // IAemsSamplePlayer vtable into the object then, for the scalar-deleting variant,
    // conditionally calls operator delete. IAemsSamplePlayer holds no members and has
    // no base, so the destructor body is empty: defining it out-of-line emits exactly
    // the vtable store + (for the deleting variant) the operator-delete tail.
    IAemsSamplePlayer::~IAemsSamplePlayer()
    {
    }

    // 0x82B6FE00. Linear-search the per-input record table for the record whose
    // leading type byte equals aeType; return that record's +8 value. Returns 0 when
    // the count is 0, the record base is null, or no record matches -- store-for-store
    // with the X360 (which short-circuits to 0 on each of those conditions).
    s32 AemsPlayerInputAccessor::GetValueByType(s32 aeType) const
    {
        const u32    luCount   = muCount;
        const Input* lpRecords = mpRecords;

        if (luCount == 0u)
            return 0;
        if (lpRecords == 0)
            return 0;

        u32 luIndex = 0u;
        while (static_cast<s32>(lpRecords[luIndex].meType) != aeType)
        {
            if (++luIndex >= luCount)
                return 0;
        }

        return lpRecords[luIndex].miValue;
    }
} // namespace Snd9
