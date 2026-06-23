#include "GameShared/GameClasses/Network/CgsNetworkManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::NetworkManagerPrepareParams::Construct                              @ 0x82581468
//   CgsNetwork::NetworkManagerPrepareParams::VersionDisplayPrepareParams::Construct @ 0x82581568
//   CgsNetwork::NetworkManagerPrepareParams::PlayerManagerPrepareParams::Construct  @ 0x825815F0
//   (all called by BrnNetwork::BrnNetworkManager::Prepare)
//
// NetworkManagerPrepareParams::Construct asserts the three source pointers are non-null
// (CGS_ASSERT trio), then copies the param sub-blocks in store-for-store:
//   +0x00 <- lpVersionDisplay[0..2]   (3 individual stw)
//   +0x0C <- lpNetworkAdapter[0..4]   (5-iteration word-copy loop, mtctr 5 / bdnz)
//   +0x20 <- lpPlayerManager[0..4]    (5-iteration word-copy loop, mtctr 5 / bdnz)
//   +0x34 <- a5                       (stw r26)
// The two unrolled-as-loop block copies are restored to explicit element-wise word copies.
//
// The two nested sub-block builders (Version-display / Player-manager prepare-params) each
// assert their inputs then store their words verbatim; their bodies follow below.
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

// @ 0x82581568 -- asm: cmplwi r29(lpcServerVersion),0 / bne (assert if null); then
//   cmpwi r28(leServerType),0 / blt  and  cmpwi r28,7 / blt  -> assert unless 0 <= type < 7
//   (the "(leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT)" range
//   guard -- two SIGNED compares, NOT the Hex-Rays `> 6` unsigned reading). Then stw r29 @+0,
//   stw r27 @+4, stw r28 @+8; returns this (r31).
NetworkManagerPrepareParams::VersionDisplayPrepareParams*
NetworkManagerPrepareParams::VersionDisplayPrepareParams::Construct(
        const char* lpcServerVersion,
        u32 luField_04,
        u32 leServerType)
{
    CGS_ASSERT(lpcServerVersion != 0, "lpcServerVersion");
    CGS_ASSERT(static_cast<s32>(leServerType) >= 0 && static_cast<s32>(leServerType) < 7,
               "(leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT)");

    mpcServerVersion = lpcServerVersion;
    muField_04       = luField_04;
    meServerType     = leServerType;
    return this;
}

// @ 0x825815F0 -- asm: cmplwi r30(lpServerInterface),0 / bne (assert if null); then
//   stw r30 @+0, stw r29 @+4, stw r28 @+8, stw r27 @+0xC, stw r26 @+0x10; returns this (r31).
NetworkManagerPrepareParams::PlayerManagerPrepareParams*
NetworkManagerPrepareParams::PlayerManagerPrepareParams::Construct(
        void* lpServerInterface,
        u32 luField_04,
        u32 luField_08,
        u32 luField_0C,
        u32 luField_10)
{
    CGS_ASSERT(lpServerInterface != 0, "lpServerInterface");

    mpServerInterface = lpServerInterface;
    muField_04        = luField_04;
    muField_08        = luField_08;
    muField_0C        = luField_0C;
    muField_10        = luField_10;
    return this;
}

}
