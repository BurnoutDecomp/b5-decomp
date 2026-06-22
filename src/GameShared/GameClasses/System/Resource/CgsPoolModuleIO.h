// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h
//
// Canonical (DWARF) home for CgsResource::PoolIO::OutputBuffer (CgsPoolModuleIO.h).
// MINIMAL-COMPLETE slice covering ONLY the OutputBuffer's X360-emitted accessors owned
// by the IO-OutputBuffers group:
//   GetPoolOutputQueue() const          @ 0x828E1AA8  read-lock  (bit 4) -> +4    (DWARF :262)
//   GetPoolResourceRequestQueue() const @ 0x828E1B50  read-lock  (bit 4) -> +8212 (DWARF :263)
//   GetPoolOutputQueue()                @ 0x828E1BF8  write-lock (bit 3) -> +4    (DWARF :264)
//   GetPoolResourceRequestQueue()       @ 0x828E1CA0  write-lock (bit 3) -> +8212 (DWARF :265)
//
// LAYOUT (DWARF CgsPoolModuleIO.h:220 + X360 getter return-offsets, authoritative):
//   base  CgsModule::IOBuffer                       (1-byte FlagSet status; +1..+3 pad)
//   +4    PoolOutputQueue        mPoolOutputQueue          (PoolQueueTemplate<8192>)
//   +8212 PoolResourceRequestQueue mPoolResourceRequestQueue (ResourceRequestQueue<8192>)
// PoolQueueTemplate<8192> derives from CgsModule::VariableEventQueue<8192,16> (DWARF
// CgsPoolModuleIO.h:128) -- modelled here by the committed generic by name.
//
// FLAG (foreign type): PoolResourceRequestQueue (== ResourceRequestQueue<8192>, DWARF
// CgsPoolModuleIO.h:67/230) has its own owning home elsewhere and is NOT reconstructed
// here; it is modelled as correctly-sized, correctly-placed opaque byte storage so the
// X360 member offset (+8212) is exact. Adopt the named type additively when its home lands.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue

namespace CgsResource
{
namespace PoolIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // PoolQueueTemplate<8192> : VariableEventQueue<8192,16> (DWARF CgsPoolModuleIO.h:222/128).
        typedef CgsModule::VariableEventQueue<8192, 16> PoolOutputQueue;

        // FLAG: ResourceRequestQueue<8192> (foreign home). 8208 bytes (+8212..+...).
        struct PoolResourceRequestQueueStorage
        {
            unsigned char maBytes[8208];
        };

        // ---- accessors owned/bodied by this group --------------------------------------
        const PoolOutputQueue*                 GetPoolOutputQueue() const;          // +4,    read
        const PoolResourceRequestQueueStorage* GetPoolResourceRequestQueue() const; // +8212, read
        PoolOutputQueue*                       GetPoolOutputQueue();                // +4,    write
        PoolResourceRequestQueueStorage*       GetPoolResourceRequestQueue();       // +8212, write

        static void _AssertLayout();

    private:
        u8                              maStatusPad[3];            // +1..+3 (force +4 placement)
        PoolOutputQueue                 mPoolOutputQueue;          // +4    (DWARF :268)
        PoolResourceRequestQueueStorage mPoolResourceRequestQueue; // +8212 (DWARF :269)
    };
}
}
