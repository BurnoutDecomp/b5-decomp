#pragma once

// ===================================================================================
// CgsNetwork::NetworkManagerPrepareParams -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/CgsNetworkManager.h
//
// The prepare-params block passed (by pointer) into CgsNetwork::NetworkManager::Prepare
// (CgsNetworkManager.h in the X360 build); populated by Construct @ 0x82581468 and
// consumed by BrnNetwork::BrnNetworkManager::Prepare. It aggregates, by value, three
// sub-blocks copied in from their own param structs plus one trailing word.
//
// LAYOUT (X360 asm @ 0x82581468 Construct -- block copies, used size 0x38 / 56B):
//   +0x00  maVersionDisplay[3]   3 words copied from lpVersionDisplay (stw 0/4/8)
//   +0x0C  maNetworkAdapter[5]   5 words copied from lpNetworkAdapter (loop, count 5)
//   +0x20  maPlayerManager[5]    5 words copied from lpPlayerManager  (loop, count 5)
//   +0x34  mpField_34            the 5th ctor arg (a5), one word (stw r26)
//
// The 5-word adapter block matches the size of CgsNetwork::NetworkAdapterPrepareParams
// (5 words / 0x14). Construct copies the source blocks verbatim (a memberwise word copy,
// the asm unrolls/loops it), so they are modelled here as fixed-size word arrays copied
// by name -- their individual sub-fields belong to the source param structs (NetworkAdapter
// / PlayerManager prepare-params) and are recovered there; reproducing them here would
// duplicate those homes. The struct is non-polymorphic (no vtable store).
// ===================================================================================

#include "types.hpp"

namespace CgsNetwork
{
    struct NetworkManagerPrepareParams
    {
        // @ 0x82581468 -- copy the three source param blocks in and stash the trailing
        // word (returns `this`). The blocks are passed by pointer; the asm copies 3 / 5 / 5
        // words respectively, then stores a5.
        NetworkManagerPrepareParams* Construct(const u32* lpVersionDisplay,
                                               const u32* lpNetworkAdapter,
                                               const u32* lpPlayerManager,
                                               void* lpField_34);

        u32   maVersionDisplay[3];   // +0x00  (copied from lpVersionDisplay)
        u32   maNetworkAdapter[5];   // +0x0C  (copied from lpNetworkAdapter)
        u32   maPlayerManager[5];    // +0x20  (copied from lpPlayerManager)
        void* mpField_34;            // +0x34  (5th ctor arg, a5)
    };
}
