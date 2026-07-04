// GameSource/Replays/Array_ReplayPropRecord_254_pushback.cpp
//
// BrnReplays::BrnReplayArray<ReplayPropRecord16, 254>::PushBack @ 0x822AA5B8
//   (BrnReplays::PropSerialiserFrame::WriteProp)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Thin explicit instantiation of the generic
// BrnReplayArray<T,N>::PushBack committed inline in BrnReplayArray.h (do NOT re-define).
// X360 store-for-store: assert muLength < 0xFE (254 == MaxLength, the u8 count domain),
// "muLength < MaxLength" (BrnReplayArray.h:98), then `stvx128` the 16-byte element to
// &maElements[muLength] (`rotlwi r,muLength,4` == muLength*16 stride) and `stb muLength+1`
// post-increment. muLength @ +0xFE0 == 254*16 fixes N==254 and sizeof(T)==16.
//
// The element is a single 16-byte aligned VMX quantum (one lvx128/stvx128 pair); its field
// layout is not attested, so it is an opaque 16-byte record.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayArray.h"  // BrnReplayArray<T,N>::PushBack (inline generic)

namespace BrnReplays
{
    // One prop-serialiser record: a single 16-byte aligned quantum the WriteProp path
    // copies via lvx128/stvx128. Field layout unattested; modelled as an opaque span.
    struct alignas(16) ReplayPropRecord16
    {
        u8 maOpaque[16];
    };
}

template BrnReplays::ReplayPropRecord16&
BrnReplays::BrnReplayArray<BrnReplays::ReplayPropRecord16, 254>::PushBack(
    const BrnReplays::ReplayPropRecord16&);
