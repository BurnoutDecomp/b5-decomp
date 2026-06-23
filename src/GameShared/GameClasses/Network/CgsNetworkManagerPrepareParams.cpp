#include "GameShared/GameClasses/Network/CgsNetworkManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::NetworkManagerPrepareParams::Construct @ 0x82581468
//   (called by BrnNetwork::BrnNetworkManager::Prepare)
//
// Asserts the three source pointers are non-null (CGS_ASSERT trio), then copies the param
// sub-blocks in store-for-store:
//   +0x00 <- lpVersionDisplay[0..2]   (3 individual stw)
//   +0x0C <- lpNetworkAdapter[0..4]   (5-iteration word-copy loop, mtctr 5 / bdnz)
//   +0x20 <- lpPlayerManager[0..4]    (5-iteration word-copy loop, mtctr 5 / bdnz)
//   +0x34 <- a5                       (stw r26)
// The two unrolled-as-loop block copies are restored to explicit element-wise word copies.
//
// (Homed in this dedicated TU file rather than CgsNetworkManager.cpp, which already owns
// the separate CgsNetwork::NetworkManager::NetworkManager TU.)

namespace CgsNetwork
{

NetworkManagerPrepareParams* NetworkManagerPrepareParams::Construct(
        const u32* lpVersionDisplay, const u32* lpNetworkAdapter,
        const u32* lpPlayerManager, void* lpField_34)
{
    CGS_ASSERT(lpVersionDisplay, "lpVersionDisplay");
    CGS_ASSERT(lpNetworkAdapter, "lpNetworkAdapter");
    CGS_ASSERT(lpPlayerManager, "lpPlayerManager");

    // +0x00: three version-display words (three individual stw in the asm).
    maVersionDisplay[0] = lpVersionDisplay[0];
    maVersionDisplay[1] = lpVersionDisplay[1];
    maVersionDisplay[2] = lpVersionDisplay[2];

    // +0x0C: five network-adapter words (asm loop, count 5).
    for (s32 li = 0; li < 5; ++li)
    {
        maNetworkAdapter[li] = lpNetworkAdapter[li];
    }

    // +0x20: five player-manager words (asm loop, count 5).
    for (s32 li = 0; li < 5; ++li)
    {
        maPlayerManager[li] = lpPlayerManager[li];
    }

    mpField_34 = lpField_34;   // +0x34 (a5)

    return this;
}

}
