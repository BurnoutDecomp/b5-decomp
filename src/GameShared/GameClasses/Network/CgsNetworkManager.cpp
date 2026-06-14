#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::NetworkManager::NetworkManager @ 0x827E3F38

namespace CgsSystem
{
    enum EFrameRate
    {
        E_FRAMERATE_UNKNOWN = 0
    };
}

namespace CgsNetwork
{
    struct VersionDisplay
    {
        VersionDisplay() {}
        u8 maStorage[28];
    };

    struct NetworkAdapter
    {
        NetworkAdapter() {}
        u8 maStorage[72];
    };

    struct PlayerManager
    {
        PlayerManager();
        u8 maStorage[9476];
    };

    struct HostMigrationManager
    {
        HostMigrationManager() {}
        u8 maStorage[1516];
    };

    struct StartTimeManager
    {
        StartTimeManager();
        u8 maStorage[1960];
    };

    struct SyncTimeMessageManager
    {
        SyncTimeMessageManager();
        u8 maStorage[816];
    };

    struct Time
    {
        Time() : muFrame(0), mfSeconds(0.0f) {}

        u32 muFrame;
        f32 mfSeconds;
    };

    struct TimeManager
    {
        TimeManager() : mStartTime(), mCurrentTime() {}

        Time mStartTime;
        Time mCurrentTime;
        u8 maStorage[64];
    };

    struct VoIPManager
    {
        VoIPManager() {}
    };

    struct NetworkManager
    {
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_NETWORK_ADAPTER = 1,
            E_PREPARESTAGE_PLAYER_MANAGER = 2,
            E_PREPARESTAGE_HOST_MIGRATION_MANGER = 3,
            E_PREPARESTAGE_START_TIME_MANAGER = 4,
            E_PREPARESTAGE_DONE = 5
        };

        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_VOIP_MANAGER = 1,
            E_RELEASESTAGE_START_TIME_MANAGER = 2,
            E_RELEASESTAGE_HOST_MIGRATION_MANGER = 3,
            E_RELEASESTAGE_PLAYER_MANAGER = 4,
            E_RELEASESTAGE_NETWORK_ADAPTER = 5,
            E_RELEASESTAGE_DONE = 6
        };

        NetworkManager();

        VersionDisplay mVersionDisplay;
        NetworkAdapter mNetworkAdapter;
        s32 miActiveControllerPort;
        bool mbSysMenuOnScreen;
        u8 maPad105[3];
        EPrepareStage mePrepareStage;
        EReleaseStage meReleaseStage;
        CgsSystem::EFrameRate meLocalConsoleFrameRate;
        PlayerManager mPlayerManager;
        HostMigrationManager mHostMigrationManager;
        StartTimeManager mStartTimeManager;
        SyncTimeMessageManager mSyncTimeMessageManager;
        TimeManager mTimeManager;
        VoIPManager mVoIPManager;
    };

    NetworkManager::NetworkManager()
        : mVersionDisplay(),
          mNetworkAdapter(),
          miActiveControllerPort(0),
          mbSysMenuOnScreen(false),
          maPad105(),
          mePrepareStage(E_PREPARESTAGE_START),
          meReleaseStage(E_RELEASESTAGE_START),
          meLocalConsoleFrameRate(CgsSystem::E_FRAMERATE_UNKNOWN),
          mPlayerManager(),
          mHostMigrationManager(),
          mStartTimeManager(),
          mSyncTimeMessageManager(),
          mTimeManager(),
          mVoIPManager()
    {
    }
}
