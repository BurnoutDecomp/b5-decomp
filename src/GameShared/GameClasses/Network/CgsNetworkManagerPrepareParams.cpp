#include "GameShared/GameClasses/Network/CgsNetworkManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::NetworkManagerPrepareParams::Construct                              @ 0x82581468
//   CgsNetwork::NetworkManagerPrepareParams::VersionDisplayPrepareParams::Construct @ 0x82581568
//   CgsNetwork::NetworkManagerPrepareParams::PlayerManagerPrepareParams::Construct  @ 0x825815F0
//   (all called by BrnNetwork::BrnNetworkManager::Prepare)
//
// NetworkManagerPrepareParams::Construct asserts the three source pointers are non-null,
// then copies each sub-block in by value (the version-display block as three word stores,
// the adapter and player-manager blocks as five-word copy loops) and stores the frame rate.
//
// (Homed in this dedicated TU file rather than CgsNetworkManager.cpp, which already owns
// the separate CgsNetwork::NetworkManager::NetworkManager TU.)

namespace CgsNetwork
{

void NetworkManagerPrepareParams::Construct(
        const VersionDisplayPrepareParams* lpVersionDisplay,
        const NetworkAdapterPrepareParams* lpNetworkAdapter,
        const PlayerManagerPrepareParams* lpPlayerManager,
        CgsSystem::EFrameRate leLocalConsoleFrameRate)
{
    CGS_ASSERT(lpVersionDisplay, "lpVersionDisplay");
    CGS_ASSERT(lpNetworkAdapter, "lpNetworkAdapter");
    CGS_ASSERT(lpPlayerManager, "lpPlayerManager");

    mVersionDisplay         = *lpVersionDisplay;
    mNetworkAdapter         = *lpNetworkAdapter;
    mPlayerManager          = *lpPlayerManager;
    meLocalConsoleFrameRate = leLocalConsoleFrameRate;
}

// The server-type range guard is two SIGNED compares (0 <= type < 7).
void NetworkManagerPrepareParams::VersionDisplayPrepareParams::Construct(
        const char* lpcServerVersion,
        u32 luField_04,
        EServerType leServerType)
{
    CGS_ASSERT(lpcServerVersion != 0, "lpcServerVersion");
    CGS_ASSERT((leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT),
               "(leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT)");

    mpcVersion   = lpcServerVersion;
    muField_04   = luField_04;
    meServerType = leServerType;
}

void NetworkManagerPrepareParams::PlayerManagerPrepareParams::Construct(
        ServerInterface* lpServerInterface,
        void* lpfOnReceivedFromWrongIPCallback,
        void* lpfConnectionFinalisedCallback,
        void* lpConnectionFinalisedUserData,
        CgsMemory::HeapMalloc* lpNetworkHeapAllocator)
{
    CGS_ASSERT(lpServerInterface != 0, "lpServerInterface");

    mpServerInterface                = lpServerInterface;
    mpfOnReceivedFromWrongIPCallback = lpfOnReceivedFromWrongIPCallback;
    mpfConnectionFinalisedCallback   = lpfConnectionFinalisedCallback;
    mpConnectionFinalisedUserData    = lpConnectionFinalisedUserData;
    mpNetworkHeapAllocator           = lpNetworkHeapAllocator;
}

}
