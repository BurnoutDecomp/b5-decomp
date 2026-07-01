// Per-instantiation .cpp for
//   Array<BrnSound::Vehicles::Environment::StaticPassbyControl::PassbyHistory::PassbyRecord, 5>
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> body (Append /
// Erase / operator[] / GetItem) is fully inline in CgsArray.h; the X360 emits one
// out-of-line copy of each USED member per using-TU. This TU is the explicit per-member
// instantiation only (do NOT re-define the generic). The members the X360 emitted
// out-of-line for this instance:
//   Array<PassbyRecord,5>::Append   @ 0x8268F450  (PassbyHistory::Record)
//   Array<PassbyRecord,5>::Erase    @ 0x8268F588  (PassbyHistory::Update)
//   Array<PassbyRecord,5>::GetItem  (Hex-Rays "CgsContainers"; Record + Update)
//
// Layout/asm parity: maElements[5] (5 * 32 = 160B; PassbyRecord is a 32-byte, 16-aligned
// aggregate -- Vector3 mvPosition @0x00 + f32 mfTimeStamp @0x10 + pad) + miCount @ +0xA0
// (== 5*0x20), matching the X360 count word, the slwi (index*0x20) element addressing,
// and the 4x ld/std (32-byte) element copy in Append.
//
// PassbyRecord is a plain aggregate with no operator==, so ONLY the used members are
// instantiated (NOT `template class`), avoiding forcing FindFirstInstanceOf/Contains --
// same per-member technique as the committed CgsArrayVpuVector3_10.cpp.

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Sound/Vehicles/Environment/BrnStaticPassbyControl.h"

namespace
{
typedef BrnSound::Vehicles::Environment::StaticPassbyControl::PassbyRecord
    PassbyRecord;
}

template void          Array<PassbyRecord, 5>::Append(const PassbyRecord&);
template void          Array<PassbyRecord, 5>::Erase(u32);
template PassbyRecord& Array<PassbyRecord, 5>::GetItem(u32);
