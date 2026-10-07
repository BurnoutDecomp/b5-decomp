#include "SDKs/Realmc/RealmcLoadEntryInfo.h"
#include "SDKs/Realmc/RealmcDataBuffer.h"  // DataBuffer { mpData, muSize } (the ctor's two pairs)

#include <cstring>  // std::memcpy -- the X360 operator= body is literally a memcpy of
                    // the 32-byte head plus four individual word copies.

// ===========================================================================
// RealmcIface::LoadEntryInfo -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcLoadEntryInfo.h for the layout and the store-order notes.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// LoadEntryInfo::LoadEntryInfo @ 0x82B519E8
//
// Zero the +0x20 pair, the data pointer and the size (in that store order), then
// zero the leading head byte. The rest of the head is left as-is (the ctor only
// touches byte +0x00).
// ---------------------------------------------------------------------------
LoadEntryInfo::LoadEntryInfo()
{
    mpAuxData     = nullptr;
    muAuxDataSize = 0;
    mpData        = nullptr;
    muDataSize    = 0;
    maHead[0]     = 0;
}

// ---------------------------------------------------------------------------
// LoadEntryInfo::LoadEntryInfo (name + two pairs)
//
// Store order: pA's two words into +0x20/+0x24, pB's two words into +0x28/+0x2C,
// then, when pName is non-null, a 32-byte memcpy of the name into the head and a
// zero store to +0x1F; with no name only the leading head byte is zeroed.
// ---------------------------------------------------------------------------
LoadEntryInfo::LoadEntryInfo(const char* pName, const DataBuffer* pA, const DataBuffer* pB)
{
    mpAuxData     = pA->mpData;
    muAuxDataSize = pA->muSize;
    mpData        = pB->mpData;
    muDataSize    = pB->muSize;

    if (pName)
    {
        std::memcpy(maHead, pName, 0x20);
        maHead[0x1F] = 0;
    }
    else
    {
        maHead[0] = 0;
    }
}

// ---------------------------------------------------------------------------
// LoadEntryInfo::operator=
//
// Store order is exact: the +0x20 pair, then the data pointer and the size
// (individually), THEN the 32-byte head memcpy (Dst=this, Src=rOther, Size=0x20),
// THEN the +0x1F byte is cleared last.
// ---------------------------------------------------------------------------
LoadEntryInfo& LoadEntryInfo::operator=(const LoadEntryInfo& rOther)
{
    mpAuxData     = rOther.mpAuxData;
    muAuxDataSize = rOther.muAuxDataSize;
    mpData        = rOther.mpData;
    muDataSize    = rOther.muDataSize;

    std::memcpy(maHead, rOther.maHead, 0x20);  // memcpy(this, rOther, 32)

    maHead[0x1F] = 0;                          // clear the flag byte (last)
    return *this;
}

} // namespace RealmcIface
