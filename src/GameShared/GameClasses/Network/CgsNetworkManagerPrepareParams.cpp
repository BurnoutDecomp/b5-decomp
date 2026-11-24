#include "GameShared/GameClasses/Network/CgsNetworkManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

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

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network/CgsNetworkManager.h".

namespace CgsNetwork
{

void NetworkManagerPrepareParams::Construct(
        const VersionDisplayPrepareParams* lpVersionDisplay,
        const NetworkAdapterPrepareParams* lpNetworkAdapter,
        const PlayerManagerPrepareParams* lpPlayerManager,
        CgsSystem::EFrameRate leLocalConsoleFrameRate)
{
    CGS_ASSERT(lpVersionDisplay,
        CgsDev::Assert::MessageAt("lpVersionDisplay", __FILE__, 412 /* ARTIST 0x825814A0 */));
    CGS_ASSERT(lpNetworkAdapter,
        CgsDev::Assert::MessageAt("lpNetworkAdapter", __FILE__, 413 /* ARTIST 0x825814C4 */));
    CGS_ASSERT(lpPlayerManager,
        CgsDev::Assert::MessageAt("lpPlayerManager", __FILE__, 414 /* ARTIST 0x825814E8 */));

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
    CGS_ASSERT(lpcServerVersion != 0,
        CgsDev::Assert::MessageAt("lpcServerVersion", __FILE__, 452 /* ARTIST 0x8258159C */));
    CGS_ASSERT((leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT),
        CgsDev::Assert::MessageAt(
            "(leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT)",
            __FILE__, 453 /* ARTIST 0x825815C8 */));

    mpcVersion   = lpcServerVersion;
    muField_04   = luField_04;
    meServerType = leServerType;
}

void NetworkManagerPrepareParams::PlayerManagerPrepareParams::Construct(
        ServerInterface* lpServerInterface,
        CgsNetwork::PlayerManagerPrepareParams::OnReceivedFromWrongIPCallback* lpfOnReceivedFromWrongIPCallback,
        PlayersConnectionManager::ConnMgrConnectionFinalisedCallback lpfConnectionFinalisedCallback,
        void* lpConnectionFinalisedUserData,
        CgsMemory::HeapMalloc* lpNetworkHeapAllocator)
{
    CGS_ASSERT(lpServerInterface != 0,
        CgsDev::Assert::MessageAt(
            "lpServerInterface",
            "..\\..\\..\\GameShared\\GameClasses\\Network/CgsNetworkManager.h", 486 /* ARTIST 0x82581624 */));

    mpServerInterface                = lpServerInterface;
    mpfOnReceivedFromWrongIPCallback = lpfOnReceivedFromWrongIPCallback;
    mpfConnectionFinalisedCallback   = lpfConnectionFinalisedCallback;
    mpConnectionFinalisedUserData    = lpConnectionFinalisedUserData;
    mpNetworkHeapAllocator           = lpNetworkHeapAllocator;
}

}
