#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"

// ---------------------------------------------------------------------------
// BrnGui::OptionsDataProfile
//
// DWARF home: GameSource/Gui/BrnGuiOptionsDataProfile.h. The persistent per-profile
// options block (audio volumes, brightness/contrast, six-axis/force-feedback toggles,
// camera-feed setting, the Trax availability bit sets, the director profile data, and
// the created/received online-game-option route tables).
//
// LAYOUT NOTE (offsets are X360-asm-authoritative, NOT DWARF declaration order):
// every member touched by this TU's accessors was pinned from the X360 stores/loads in
// BrnGuiOptionsDataProfile.cpp (see _AssertLayout below). The object's leading region
// (the created/received OnlineSaveRoute tables, the trax-array prefix, num counters and
// the DirectorProfileData) is large and out of scope for this TU; it is reserved with a
// single explicit padding buffer up to the first member this TU names (0x7310). The
// named tail starting at 0x7310 is fully grounded.
// ---------------------------------------------------------------------------

namespace BrnGui
{
    // BrnGuiOptionsDataProfile.h:31..41 (DWARF) -- validated ranges / defaults.
    const s32 KI_MIN_BRIGHTNESS      = 1;
    const s32 KI_MIN_CONTRAST        = 1;
    const s32 KI_DEFAULT_BRIGHTNESS  = 50;
    const s32 KI_DEFAULT_MUSIC_VOLUME = 8;
    const s32 KI_DEFAULT_SFX_VOLUME   = 8;
    const s32 KI_DEFAULT_VOIP_VOLUME  = 10;
    const s32 KI_MAX_BRIGHTNESS      = 100;
    const s32 KI_MAX_CONTRAST        = 100;
    const s32 KI_DEFAULT_CONTRAST    = 50;

    // ---- Supporting types referenced by OptionsDataProfile -----------------
    // These have no committed home in the tree yet; they are declared here at the
    // shape the X360 build attests (DWARF nesting + asm-confirmed sizes) so this
    // header is a coherent compilable slice. When their real owning TUs are
    // reconstructed, these definitions move there and this header includes them.

    // GuiEventAudioTraxUpdate::EATraxArrayType is a 128-bit fixed bit set: the X360
    // stores/loads it as two adjacent u64 fields (16 bytes) per array. It is a
    // CgsContainers::FastBitArray<128>.
    struct GuiEventAudioTraxUpdate
    {
        typedef CgsContainers::FastBitArray<128> EATraxArrayType;
    };

    // GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode -- the trax play-order selector
    // (stored as a 4-byte word at +0x7340).
    struct GuiEventAudioTraxPlayOrder
    {
        enum ETraxPlayOrderMode
        {
            E_TRAX_PLAY_ORDER_MODE_DEFAULT = 0,
        };
    };

    class GuiCache;
    struct GuiEventNetworkGameParams;
    class GuiEventQueueSmall;
}

namespace BrnNetwork
{
    enum EBoostType
    {
        E_BOOST_TYPE_DEFAULT = 0,
    };
    enum EVehicleChoice
    {
        E_VEHICLE_CHOICE_DEFAULT = 0,
    };

    // BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions -- the camera-feed user
    // setting (stored as a 4-byte word at +0x7364). BrnNetworkModuleIO is a NAMESPACE
    // in its canonical home (BrnNetworkModuleIO.h) and in BrnGuiCache.h; declaring it as
    // a namespace here (not a struct) lets this header coexist with those in one TU
    // (e.g. CrashNavOptions, which needs both). Enum access is identical either way.
    namespace BrnNetworkModuleIO
    {
        enum ECameraUserOptions
        {
            CAMERA_USER_OFF          = 0,
            CAMERA_USER_ON           = 1,
            CAMERA_USER_FRIENDS_ONLY = 2,
        };
    }
}

namespace BrnGameState
{
    struct GameStateModuleIO
    {
        enum EGameModeType
        {
            E_GAME_MODE_TYPE_DEFAULT = 0,
        };
    };
}

namespace BrnDirector
{
    struct GameState
    {
        // 12-byte opaque block (matches BrnDirectorGameState.h's reconstruction).
        struct DirectorProfileData { u8 maOpaque[0x0C]; };
    };
}

namespace BrnGui
{

struct OptionsDataProfile
{
    static const s32 KI_VERSION_NUMBER                 = 12;
    static const s32 KI_MAX_CREATED_ONLINE_GAME_OPTIONS  = 10;
    static const s32 KI_MAX_RECEIVED_ONLINE_GAME_OPTIONS = 10;

    // ---- nested OnlineSaveRoute (declared at DWARF shape; out of scope here) ----
    struct OnlineSaveRoute
    {
        struct OnlineSaveRouteEvent
        {
            static const s32 KI_MAX_LANDMARKS_IN_MODE = 16;   // Construct bound: cmpwi r30,0x10

            void   Construct(s32 liEventID, u32 luJunctionID, CgsID* lpaLandmarks, s32 liNumLandmarks);
            s32    GetEventID() const;
            u32    GetJunctionID() const;
            s32    GetNumLandmarks() const;
            CgsID  GetLandmark(s32 liIndex) const;

        private:
            CgsID maLandmarkIndices[16];
            u32   mJunctionId;
            s32   miNumLandmarks;
            s32   miEventID;
        };

        void Construct();
        void SetFromGameParams(GuiCache* lpCache, const GuiEventNetworkGameParams* lpParams);
        void SetToGameParams(GuiCache* lpCache, GuiEventNetworkGameParams* lpParams) const;

    private:
        OnlineSaveRouteEvent                       maEvents[10];
        BrnGameState::GameStateModuleIO::EGameModeType meGameMode;
        BrnNetwork::EBoostType                     meBoostType;
        BrnNetwork::EVehicleChoice                 meVehicleChoice;
        s32  miTimeLimit;
        s32  miNumRounds;
        s32  miVehicleClass;
        s32  miNumRunnerCrashes;
        bool mbInfiniteBoost;
        bool mbTrafficOn;
        bool mbTrafficCheckingOn;
    };

    typedef GuiEventAudioTraxUpdate::EATraxArrayType EATraxArrayType;

    // ---- public interface (declarations; only the accessors below are bodied
    //      in this TU -- the rest live in other OptionsDataProfile TUs) --------
    void Construct();
    void Destruct();
    void LockData();
    void UnlockData();
    bool GetIsLocked();
    bool ValidateProfile();
    bool IsIncorrectVersion() const;
    bool IsUpgradable() const;
    bool UpgradeFrom(const void* lpvSourceProfile, s32* lpiOutOldVersionSize);
    void NotifyOtherModulesLoadComplete(GuiEventQueueSmall* lpQueue);

    void             SetTraxAvailableInFreeBurn(const EATraxArrayType* lpTraxAvailable);
    EATraxArrayType* GetTraxAvailableInFreeBurn();
    void             SetTraxAvailableInEvents(const EATraxArrayType* lpTraxAvailable);
    EATraxArrayType* GetTraxAvailableInEvents();
    void             SetTraxRemaining(const EATraxArrayType* lpTraxRemaining);
    EATraxArrayType* GetTraxRemaining();
    void             SetTraxPlayOrderMode(GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode lePlayOrderMode);
    GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode GetTraxPlayOrderMode();

    void SetCameraFeed(BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions leCameraFeedSetting);
    BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions GetCameraFeed();

    void SetDefaultGameCamera(bool lbDefaultGameCamera);
    bool GetDefaultGameCamera();

    void SetVoipVolume(s32 liVolume);
    s32  GetVoipVolume();
    void SetMusicVolume(s32 liVolume);
    s32  GetMusicVolume();

    void SetLastPlayedSongIndex(s32 liIndex);
    s32  GetLastPlayedSongIndex();
    void SetLastPlayedPictureParadiseMusicIndex(s32 liIndex);
    s32  GetLastPlayedPictureParadiseMusicIndex();

    void SetSFXVolume(s32 liVolume);
    s32  GetSFXVolume();
    void SetBrightness(s32 liNewBrightnessSetting);
    s32  GetBrightness() const;
    void SetContrast(s32 liNewContrastSetting);
    s32  GetContrast() const;

    void SetSixAxisShowtime(bool lbSixAxisShowtime);
    bool GetSixAxisShowtime();
    void SetSixAxisSteering(bool lbSixAxisSteering);
    bool GetSixAxisSteering();
    void SetForceFeedback(bool lbForceFeedback);
    bool GetForceFeedback();
    void SetTips(bool lbTips);
    bool GetTips();

    u64  GetUintFromBits(EATraxArrayType* lpBits);

    s32  GetNumCreatedOnlineGameOptions() const;
    s32  GetNumReceivedOnlineGameOptions() const;
    void GetCreatedOnlineGameOptions(s32 liIndex, GuiCache* lpCache, GuiEventNetworkGameParams* lpParams) const;
    void GetReceivedOnlineGameOptions(s32 liIndex, GuiCache* lpCache, GuiEventNetworkGameParams* lpParams) const;
    const BrnDirector::GameState::DirectorProfileData* GetDirectorProfileData() const;
    void SetCreatedOnlineGameOptions(s32 liIndex, GuiCache* lpCache, const GuiEventNetworkGameParams* lpParams);
    void AddReceivedOnlineGameOptions(GuiCache* lpCache, const GuiEventNetworkGameParams* lpParams);
    void SetUnreadNews(bool lbUnread);
    void SetDirectorProfileData(const BrnDirector::GameState::DirectorProfileData* lpData);
    bool IsThereUnreadNews() const;

private:
    // Leading version/director prefix. The X360 lays miVersionNumber at +0x0; the
    // first OnlineSaveRoute table starts at +0x8 (asm: SetFromGameParams(this+8,..)),
    // so exactly 8 bytes precede the created table. The DirectorProfileData (out of
    // scope for this TU) is reached through a separate accessor; it is not modelled
    // member-by-member here. Reserved so the created table lands at +0x8.
    u8 maOpaqueVersionPrefix[0x8];                    // +0x0

    // The created/received online-game-option route tables. The X360 stride between
    // successive entries is 0x5C0 (asm: `mulli r11, index, 0x5C0`); AddReceived
    // memmoves 9 entries (0x33C0 = 9*0x5C0) up one slot. Created base = +0x8,
    // received base = +0x3988 (= +0x8 + 10*0x5C0). See _AssertLayout.
    OnlineSaveRoute maCreatedOnlineGameOptions[KI_MAX_CREATED_ONLINE_GAME_OPTIONS];   // +0x0008
    OnlineSaveRoute maReceivedOnlineGameOptions[KI_MAX_RECEIVED_ONLINE_GAME_OPTIONS]; // +0x3988

    s32 miNumCreatedOnlineGameOptions;                // +0x7308
    s32 miNumReceivedOnlineGameOptions;               // +0x730C

    // Remaining leading region between the num counters and the trax tail (0x7310).
    // Empty in the current layout (counters end exactly at 0x7310), kept as a named
    // zero-length anchor documents the boundary. (No bytes: +0x7310 follows +0x730C+4.)

    // Tail region -- every offset confirmed from the X360 asm.
    EATraxArrayType mTraxAvailableInFreeBurn;        // +0x7310 (16 bytes)
    EATraxArrayType mTraxAvailableInEvents;          // +0x7320 (16 bytes)
    EATraxArrayType mTraxFullyPlayed;                // +0x7330 (16 bytes)
    GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode meTraxPlayOrderMode; // +0x7340
    s32  miLastPlayedSongIndex;                      // +0x7344
    s32  miLastPictureParadiseMusicIndex;            // +0x7348
    s32  mbDefaultGameCamera;                        // +0x734C (4-byte flag; Get == 1)
    s32  mBrightness;                                // +0x7350
    s32  mContrast;                                  // +0x7354
    s32  miVoipVolume;                               // +0x7358
    s32  mMusicVolume;                               // +0x735C
    s32  mSFXVolume;                                 // +0x7360
    BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions meCameraFeedSetting; // +0x7364
    bool mbIsNewsUnread;                             // +0x7368
    bool mbSixAxisShowtime;                          // +0x7369
    bool mbSixAxisSteering;                          // +0x736A
    bool mbForceFeedback;                            // +0x736B
    bool mbPad736C;                                  // +0x736C (un-named padding byte)
    bool mbTips;                                     // +0x736D
    bool mbIsLocked;                                 // +0x736E

private:
    // Pin the asm-authoritative offsets at compile time (never called).
    static void _AssertLayout();
};

} // namespace BrnGui
