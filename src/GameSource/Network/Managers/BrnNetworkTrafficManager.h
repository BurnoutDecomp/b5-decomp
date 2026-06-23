// ============================================================================
// b5-decomp/src/GameSource/Network/Managers/BrnNetworkTrafficManager.h
// ============================================================================
// BrnNetwork::TrafficManager::BufferedMessage -- one buffered traffic-replication
// message record held in the TrafficManager's fixed-capacity buffered-message ring
// (BrnNetworkTrafficManager.cpp). The manager keeps an array of these records; the
// consumer BrnNetwork::TrafficManager::RemoveBufferedMessage (X360 0x82559840) walks
// the array at a 1936-byte (0x790) stride, so sizeof(BufferedMessage) == 1936.
//
// No source or DWARF is available for this TU, so the SHAPE is recovered purely from
// the X360 asm:
//   * the copy-assignment BufferedMessage::operator= @ 0x82551D50 -- a plain whole-
//     object struct copy. Its body copies a 10-byte head (a u16 @ +0, an s32 @ +4,
//     an s32 @ +8) and then an array of 24 fixed 80-byte (0x50) records starting at
//     +0x10. Each record copy is a u16 head @ record+0 plus a 64-byte body @
//     record+0x10..+0x50 (emitted as four aligned 16-byte lvx128/stvx128 block moves
//     -- a wide memcpy of the record body, NOT a VMX math pipeline).
//   * RemoveBufferedMessage stamps the two head s32 fields (@ +4 and @ +8) with -1 on
//     removal (the X360 `*(... - 1932) = -1; *(... - 1928) = -1`), so those two words
//     are the record's id/index slots; the u16 @ +0 is a message-type/length tag.
//
// The 24 x 80-byte body records and the 64 payload bytes inside each are modelled as
// offset-pinned opaque storage (their finer field types are not recoverable from a
// pure copy/clear); every byte the X360 copies is preserved at its exact offset so a
// later finer reconstruction can subdivide the storage without moving anything. The
// copy is reproduced as an ordinary memberwise copy (= the X360's store-for-store
// whole-object copy) over that layout.
#pragma once

#include "types.hpp"

namespace BrnNetwork
{
namespace TrafficManager
{
    // ------------------------------------------------------------------------
    // One 80-byte (0x50) buffered-message body record. The copy reads a u16 head
    // (@ +0) and then copies the 64-byte body @ +0x10..+0x50 as a block; +0x02..+0x10
    // is head padding the copy skips. Modelled as offset-pinned storage.
    // ------------------------------------------------------------------------
    struct BufferedMessageRecord
    {
        u16 muTag;              // +0x00  (lhz/sth head copied per record)
        u8  maHeadPad[0x10 - 2];// +0x02  head padding (not touched by the copy)
        u8  maBody[0x40];       // +0x10  64-byte body block (4x aligned 16B moves)
    };
    static_assert(sizeof(BufferedMessageRecord) == 0x50, "BufferedMessage record stride (0x50)");

    // ------------------------------------------------------------------------
    // BrnNetwork::TrafficManager::BufferedMessage -- a single buffered traffic
    // message: a 10-byte head (type tag + two id/index words) followed by 24 body
    // records. sizeof == 1936 (the manager's array stride).
    // ------------------------------------------------------------------------
    struct BufferedMessage
    {
        u16 muType;                     // +0x00  message-type/length tag (lhz/sth head)
        u8  maHeadPad[2];               // +0x02  alignment to the +4 id word
        s32 miId0;                      // +0x04  id/index word (stamped -1 on remove)
        s32 miId1;                      // +0x08  id/index word (stamped -1 on remove)
        u8  maHeadTail[0x10 - 0x0C];    // +0x0C  pad to the +0x10 record array
        BufferedMessageRecord maRecords[24]; // +0x10  24 x 0x50 body records

        // X360 0x82551D50 -- whole-object copy. Reproduced as a memberwise copy
        // (the X360 emits the equivalent store-for-store wide copy).
        BufferedMessage& operator=(const BufferedMessage& lOther);
    };
    static_assert(sizeof(BufferedMessage) == 1936, "BufferedMessage array stride (0x790)");
}
}
