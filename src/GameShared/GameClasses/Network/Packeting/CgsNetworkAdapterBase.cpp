#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::NetworkAdapterPrepareParams::Construct @ 0x82581330
//   (called by BrnNetwork::BrnNetworkManager::Prepare)
//
// Bounds-checks the server type and the two required pointers (CGS_ASSERT trio), then
// writes the five param words store-for-store in the asm's order:
//   stw r25(a6), 0x00 ;  stw r29(leServerType), 0x04 ;  stw r28(lpHeapMalloc), 0x08
//   stw r27(lpNetworkManager), 0x0C ;  stw r26(a5), 0x10
// The server-type guard is the combined "(leServerType >= E_SERVER_TYPE_LOCAL) &&
// (leServerType < E_SERVER_TYPE_COUNT)" range test (the asm splits it into blt 0 / blt 7).

namespace CgsNetwork
{

NetworkAdapterPrepareParams* NetworkAdapterPrepareParams::Construct(
        u32 leServerType, void* lpHeapMalloc, void* lpNetworkManager,
        void* lpField_10, void* lpField_00)
{
    CGS_ASSERT((leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT),
               "(leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT)");
    CGS_ASSERT(lpHeapMalloc, "lpHeapMalloc");
    CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

    mpField_00       = lpField_00;        // +0x00 (a6)
    meServerType     = leServerType;      // +0x04
    mpHeapMalloc     = lpHeapMalloc;      // +0x08
    mpNetworkManager = lpNetworkManager;  // +0x0C
    mpField_10       = lpField_10;        // +0x10 (a5)

    return this;
}

}
