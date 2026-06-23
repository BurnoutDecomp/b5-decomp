// ============================================================================
// b5-decomp/src/GameSource/Network/Managers/BrnNetworkTrafficManager.cpp
// ============================================================================
// BrnNetwork::TrafficManager::BufferedMessage::operator= @ 0x82551D50.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no leak / no DWARF). The X360 body is
// a plain whole-object struct copy: it copies the 10-byte head (u16 @ +0, s32 @ +4,
// s32 @ +8) then 24 fixed 80-byte body records starting at +0x10. Per record it
// copies a u16 head (lhz/sth @ record+0) and the 64-byte body (record+0x10..+0x50)
// via four aligned 16-byte lvx128/stvx128 block moves -- the compiler's wide memcpy
// of the record body, not a VMX arithmetic pipeline. It is reproduced here as an
// ordinary memberwise copy of the same layout, which lowers to the identical
// store-for-store whole-object copy.

#include "GameSource/Network/Managers/BrnNetworkTrafficManager.h"

#include <cstring>  // std::memcpy -- the X360 record body is copied as a 64-byte block

namespace BrnNetwork
{
namespace TrafficManager
{

BufferedMessage& BufferedMessage::operator=(const BufferedMessage& lOther)
{
    // Head: u16 @ +0, then the two s32 id words @ +4 / +8 (the X360 copies the head
    // halfword and the two words explicitly before the record loop).
    muType = lOther.muType;
    miId0  = lOther.miId0;
    miId1  = lOther.miId1;

    // 24 body records. Each record: u16 head @ +0 then the 64-byte body @ +0x10
    // (four 16-byte aligned block moves on the X360). Copied member-for-member here.
    for (int liRecord = 0; liRecord < 24; ++liRecord)
    {
        maRecords[liRecord].muTag = lOther.maRecords[liRecord].muTag;
        std::memcpy(maRecords[liRecord].maBody,
                    lOther.maRecords[liRecord].maBody,
                    sizeof(maRecords[liRecord].maBody));
    }
    return *this;
}

}
}
