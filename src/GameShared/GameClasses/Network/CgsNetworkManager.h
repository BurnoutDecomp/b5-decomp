#pragma once

// ===================================================================================
// CgsNetwork::NetworkManager and its parameter blocks -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/CgsNetworkManager.h
//
// NetworkManager is the platform-independent core of the online hub: it owns the build
// banner (VersionDisplay), the platform network adapter, the player registry, host
// migration, the start-time handshake, the network clock and voice chat, and drives them
// through staged Prepare/Release state machines. BrnNetwork::BrnNetworkManager derives
// from it and adds the game-side managers after +0x38E8.
//
// LAYOUT (console byte offsets; every member is reached by name)
//   +0x0000  mVersionDisplay          VersionDisplay           0x1C
//   +0x001C  mNetworkAdapter          NetworkAdapterX360       0x44
//   +0x0060  miActiveControllerPort
//   +0x0064  mbSysMenuOnScreen
//   +0x0068  mePrepareStage
//   +0x006C  meReleaseStage
//   +0x0070  meLocalConsoleFrameRate
//   +0x0074  (padding: the player manager is 8-byte aligned)
//   +0x0078  PlayerManager            0x2500   pinned storage (see below)
//   +0x2578  mHostMigrationManager    HostMigrationManager     0x5F0
//   +0x2B68  StartTimeManager         0x768    pinned storage (see below)
//   +0x32D0  mTimeManager             TimeManager              0x39C
//   +0x366C  mVoIPManager             VoIPManager              0x27C
//   sizeof == 0x38E8
//
// Two sub-objects are offset-pinned byte storage of their console span:
//   * PlayerManager: Players/CgsPlayerManager.h declares methods only (no data members),
//     so it cannot stand in for the 0x2500-byte console object.
//   * StartTimeManager: no header yet; its home is StartTime/CgsStartTimeManager.h.
// Every function that has to reach either one stays declared-only until those headers
// carry the console layout and the called methods.
//
// Host layout: pointers widen on the x64 host, so the host offsets differ from the
// console ones. _AssertLayout() pins the console offsets in a 32-bit build only.
// ===================================================================================

#include <cstddef>                                                              // offsetof (_AssertLayout)

#include "types.hpp"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                 // EServerType
#include "GameShared/GameClasses/Network/CgsNetworkVersionDisplay.h"            // mVersionDisplay
#include "GameShared/GameClasses/Network/Packeting/X360/CgsNetworkAdapterX360.h" // mNetworkAdapter, NetworkAdapterPrepareParams
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"     // mHostMigrationManager
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                 // mTimeManager
#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h" // mVoIPManager
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"                   // CgsSystem::EFrameRate
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                        // CgsSystem::Time

namespace CgsSystem
{
    class TimerStatus;                   // pointer-only (Update / PostUpdate)
}

namespace CgsMemory
{
    class HeapMalloc;                    // pointer-only (PlayerManagerPrepareParams)
}

namespace CgsNetwork
{
    class ServerInterface;               // pointer-only (PlayerManagerPrepareParams)

    // Pointer-only (NetworkManagerConstructParams). Its home is Players/CgsPlayerManager.h,
    // which does not declare it yet.
    struct PlayerManagerConstructParams;

    // What the staged managers report back from Prepare.
    enum EManagerReturnCode
    {
        E_MANAGER_STATUS_READY = 0,
        E_MANAGER_STATUS_BUSY  = 1,
        E_MANAGER_STATUS_ERROR = 2,
    };

    // The construct-params block NetworkManager::Construct reads: +0x00 the player-manager
    // construct params, +0x04 the start-time manager's client-ready callback, +0x08 its user
    // data. The callback's type is StartTimeManager::ClientReadyCallback, which has no header
    // yet, so the block is declared here and defined once StartTime/CgsStartTimeManager.h
    // exists.
    struct NetworkManagerConstructParams;

    // -------------------------------------------------------------------------------
    // The prepare-params block BrnNetworkManager::Prepare fills and NetworkManager::Prepare
    // consumes (console size 0x38). Construct copies the three source sub-blocks in by value.
    struct NetworkManagerPrepareParams
    {
        // The build-banner sub-block (0x0C). The console carries a third word between the
        // version string and the server type (VersionDisplay prints it as "/%d").
        struct VersionDisplayPrepareParams
        {
            void Construct(const char* lpcServerVersion, u32 luField_04, EServerType leServerType);

            const char* mpcVersion;      // +0x00
            u32         muField_04;      // +0x04
            EServerType meServerType;    // +0x08
        };

        // The player-manager sub-block (0x14). The two callback words keep a plain pointer
        // type: their typedefs belong to PlayerManagerPrepareParams (Players/CgsPlayerManager.h)
        // and PlayersConnectionManager, neither of which declares them yet.
        struct PlayerManagerPrepareParams
        {
            void Construct(ServerInterface* lpServerInterface,
                           void* lpfOnReceivedFromWrongIPCallback,
                           void* lpfConnectionFinalisedCallback,
                           void* lpConnectionFinalisedUserData,
                           CgsMemory::HeapMalloc* lpNetworkHeapAllocator);

            ServerInterface*       mpServerInterface;                 // +0x00
            void*                  mpfOnReceivedFromWrongIPCallback;  // +0x04
            void*                  mpfConnectionFinalisedCallback;    // +0x08
            void*                  mpConnectionFinalisedUserData;     // +0x0C
            CgsMemory::HeapMalloc* mpNetworkHeapAllocator;            // +0x10
        };

        void Construct(const VersionDisplayPrepareParams* lpVersionDisplay,
                       const NetworkAdapterPrepareParams* lpNetworkAdapter,
                       const PlayerManagerPrepareParams* lpPlayerManager,
                       CgsSystem::EFrameRate leLocalConsoleFrameRate);

        VersionDisplayPrepareParams mVersionDisplay;           // +0x00
        NetworkAdapterPrepareParams mNetworkAdapter;           // +0x0C
        PlayerManagerPrepareParams  mPlayerManager;            // +0x20
        CgsSystem::EFrameRate       meLocalConsoleFrameRate;   // +0x34
    };

    // -------------------------------------------------------------------------------
    struct NetworkManager
    {
    public:
        enum EPrepareStage
        {
            E_PREPARESTAGE_START                 = 0,
            E_PREPARESTAGE_NETWORK_ADAPTER       = 1,
            E_PREPARESTAGE_PLAYER_MANAGER        = 2,
            E_PREPARESTAGE_HOST_MIGRATION_MANGER = 3,
            E_PREPARESTAGE_START_TIME_MANAGER    = 4,
            E_PREPARESTAGE_DONE                  = 5,
        };

        enum EReleaseStage
        {
            E_RELEASESTAGE_START                 = 0,
            E_RELEASESTAGE_VOIP_MANAGER          = 1,
            E_RELEASESTAGE_START_TIME_MANAGER    = 2,
            E_RELEASESTAGE_HOST_MIGRATION_MANGER = 3,
            E_RELEASESTAGE_PLAYER_MANAGER        = 4,
            E_RELEASESTAGE_NETWORK_ADAPTER       = 5,
            E_RELEASESTAGE_DONE                  = 6,
        };

        // Per-sub-manager CPU monitors, registered by Construct and bracketing Update.
        static s32 _miPlayerManagerUpdatePerfMon;
        static s32 _miNetworkAdapterUpdatePerfMon;
        static s32 _miHostMigrationManagerUpdatePerfMon;
        static s32 _miStartTimeManagerUpdatePerfMon;
        static s32 _miVOIPManagerUpdatePerfMon;

        NetworkManager();

        TimeManager*          GetTimeManager()          { return &mTimeManager; }
        HostMigrationManager* GetHostMigrationManager() { return &mHostMigrationManager; }
        VoIPManager*          GetVoIPManager()          { return &mVoIPManager; }
        s32                   GetActiveControllerPort() { return miActiveControllerPort; }

    protected:
        void               Construct(NetworkManagerConstructParams* lpConstructParams);
        void               Destruct();
        EManagerReturnCode Prepare(NetworkManagerPrepareParams* lpPrepareParams);
        bool               Release();
        void               Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame, bool lbInGame);
        void               PostUpdate(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);
        void               OnEnterGame();
        void               OnLeaveGame();
        void               OnGameStart(CgsSystem::Time lStartTime, u16 lu16CurrentFrame);
        void               OnGameFinish(CgsSystem::Time lFinishTime, u16 lu16CurrentFrame);
        void               OnRoundStart(CgsSystem::Time lStartTime, f32 lfTimeStep, u16 lu16CurrentFrame);
        void               OnRoundFinish(CgsSystem::Time lFinishTime, u16 lu16CurrentFrame);

        VersionDisplay     mVersionDisplay;                            // +0x0000
        NetworkAdapterX360 mNetworkAdapter;                            // +0x001C

    private:
        // Handed to the start-time manager as its "start message arrived late" callback.
        static void StartTimeMessageArrivedLate();

        // Console offsets are pinned in a 32-bit build; inert on the x64 host.
        static void _AssertLayout();

        s32                   miActiveControllerPort;                  // +0x0060
        bool                  mbSysMenuOnScreen;                       // +0x0064
        EPrepareStage         mePrepareStage;                          // +0x0068
        EReleaseStage         meReleaseStage;                          // +0x006C
        CgsSystem::EFrameRate meLocalConsoleFrameRate;                 // +0x0070

        // PlayerManager span; the header models none of its 0x2500 bytes.
        alignas(8) u8         maPlayerManagerStorage[0x2500];          // +0x0078

        HostMigrationManager  mHostMigrationManager;                   // +0x2578

        // StartTimeManager span; its header (StartTime/CgsStartTimeManager.h) does not exist yet.
        u8                    maStartTimeManagerStorage[0x768];        // +0x2B68

        TimeManager           mTimeManager;                            // +0x32D0
        VoIPManager           mVoIPManager;                            // +0x366C
    };

    inline void NetworkManager::_AssertLayout()
    {
#define CGS_NM_AT(member, off) \
        static_assert(sizeof(void*) != 4 || offsetof(NetworkManager, member) == off, #member " @ " #off)
        CGS_NM_AT(mVersionDisplay,            0x0000);
        CGS_NM_AT(mNetworkAdapter,            0x001C);
        CGS_NM_AT(miActiveControllerPort,     0x0060);
        CGS_NM_AT(mbSysMenuOnScreen,          0x0064);
        CGS_NM_AT(mePrepareStage,             0x0068);
        CGS_NM_AT(meReleaseStage,             0x006C);
        CGS_NM_AT(meLocalConsoleFrameRate,    0x0070);
        CGS_NM_AT(maPlayerManagerStorage,     0x0078);
        CGS_NM_AT(mHostMigrationManager,      0x2578);
        CGS_NM_AT(maStartTimeManagerStorage,  0x2B68);
        CGS_NM_AT(mTimeManager,               0x32D0);
        CGS_NM_AT(mVoIPManager,               0x366C);
#undef CGS_NM_AT
        static_assert(sizeof(void*) != 4 || sizeof(NetworkManager) == 0x38E8, "NetworkManager console size");
        static_assert(sizeof(void*) != 4 || sizeof(NetworkManagerPrepareParams) == 0x38, "NetworkManagerPrepareParams console size");
    }
}
