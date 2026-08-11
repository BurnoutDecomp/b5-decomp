#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"

#include <cstddef>
#include <cstring>   // memmove

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"       // CgsDev::StrStreamBase (ValidateProfile mismatch log)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<4096,16> (the recovered load-complete queue)

namespace BrnGui
{

// ---------------------------------------------------------------------------
// Construct
//
// Zeroes/initialises the profile to its defaults. The X360 body:
//   - miVersionNumber = KI_VERSION_NUMBER (12)
//   - clears the three trax FastBitArray<128> sets (SetAll then immediately the
//     ctor loop leaves them all-ones / -1 fields -- the asm stores 0xFFFFFFFF.. )
//   - default-constructs the created/received OnlineSaveRoute tables and the
//     DirectorProfileData
//   - mMusicVolume = mSFXVolume = KI_DEFAULT_*_VOLUME (8), miVoipVolume = 10
//   - mBrightness = mContrast = 50
//   - meTraxPlayOrderMode = 0
//   - mbSixAxisShowtime = mbTips = mbForceFeedback = true; mbSixAxisSteering = mbIsLocked = false
//
// The bulk of this body operates on the large out-of-scope leading region (the
// OnlineSaveRoute tables and director data), which is reserved opaque in the
// header. It is reconstructed in the owning OptionsDataProfile-Construct TU; here
// only the in-scope tail defaults are set so this accessor TU stays coherent.
// ---------------------------------------------------------------------------
void OptionsDataProfile::Construct()
{
    // The FIRST thing the X360 body does (0x824FF568 `li r10, 0xC` / 0x824FF574
    // `stw r10, 0(r20)`): stamp the version word. GuiCache::Construct @0x82505860 runs
    // this over the live options block, and ProfileManager::ReadProfileData @0x824FF298
    // memcpy's that live block into the stored image -- so this store is what makes a
    // FIRST boot (no save on the memory unit) present a version-current options segment
    // to ValidateProfiles. It was missing, which is why a fresh PC boot logged
    // "Options Data Profile version mismatch, expected 12, got 0".
    miVersionNumber = KI_VERSION_NUMBER;

    mTraxAvailableInFreeBurn.Construct();
    mTraxAvailableInEvents.Construct();
    mTraxFullyPlayed.Construct();
    mTraxAvailableInFreeBurn.SetAll();
    mTraxAvailableInEvents.SetAll();
    mTraxFullyPlayed.SetAll();

    meTraxPlayOrderMode             = GuiEventAudioTraxPlayOrder::E_TRAX_PLAY_ORDER_MODE_DEFAULT;
    miLastPlayedSongIndex           = -1;  // asm: stw r19(-1), 0x7344
    miLastPictureParadiseMusicIndex = 0;   // asm: stw r31(0),  0x7348

    mMusicVolume = KI_DEFAULT_MUSIC_VOLUME;   // 8
    mSFXVolume   = KI_DEFAULT_SFX_VOLUME;     // 8
    mBrightness  = KI_DEFAULT_BRIGHTNESS;     // 50
    mContrast    = KI_DEFAULT_CONTRAST;       // 50
    miVoipVolume = KI_DEFAULT_VOIP_VOLUME;    // 10

    meCameraFeedSetting = BrnNetwork::BrnNetworkModuleIO::CAMERA_USER_ON; // asm: stw r10(1), 0x7364
    mbDefaultGameCamera = 1;
    mbIsNewsUnread      = false;
    mbSixAxisShowtime   = true;
    mbSixAxisSteering   = false;  // asm: stb r31(0), 0x736A
    mbForceFeedback     = true;   // asm: stb r10(1), 0x736B
    mbTips              = true;
    mbIsLocked          = false;
}

// ---------------------------------------------------------------------------
// ValidateProfile  @0x824EFF58
//
// Version-check the loaded options profile: the +0x0 version word must equal
// KI_VERSION_NUMBER (12). On mismatch, log "Options Data Profile version
// mismatch, expected 12, got <v>" (gated on message filter bit 0) and fail.
// ---------------------------------------------------------------------------
bool OptionsDataProfile::ValidateProfile()
{
    using namespace CgsDev;

    const s32 liVersion = miVersionNumber;   // lwz r29, 0(r3)

    if (liVersion == KI_VERSION_NUMBER)
    {
        return true;
    }

    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint
            << "Options Data Profile version mismatch, expected "
            << KI_VERSION_NUMBER
            << ", got "
            << liVersion
            << "\n";
    }

    return false;
}

// ---------------------------------------------------------------------------
// Trax availability bit sets
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetTraxAvailableInFreeBurn(const EATraxArrayType* lpTraxAvailable)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mTraxAvailableInFreeBurn = *lpTraxAvailable;
}

OptionsDataProfile::EATraxArrayType* OptionsDataProfile::GetTraxAvailableInFreeBurn()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return &mTraxAvailableInFreeBurn;
}

void OptionsDataProfile::SetTraxAvailableInEvents(const EATraxArrayType* lpTraxAvailable)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mTraxAvailableInEvents = *lpTraxAvailable;
}

OptionsDataProfile::EATraxArrayType* OptionsDataProfile::GetTraxAvailableInEvents()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return &mTraxAvailableInEvents;
}

void OptionsDataProfile::SetTraxRemaining(const EATraxArrayType* lpTraxRemaining)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mTraxFullyPlayed = *lpTraxRemaining;
}

// ---------------------------------------------------------------------------
// Trax play-order mode
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetTraxPlayOrderMode(GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode lePlayOrderMode)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    meTraxPlayOrderMode = lePlayOrderMode;
}

GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode OptionsDataProfile::GetTraxPlayOrderMode()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return meTraxPlayOrderMode;
}

// ---------------------------------------------------------------------------
// Camera feed
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetCameraFeed(BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions leCameraFeedSetting)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    meCameraFeedSetting = leCameraFeedSetting;
}

BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions OptionsDataProfile::GetCameraFeed()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return meCameraFeedSetting;
}

// ---------------------------------------------------------------------------
// Default game camera (stored as a 4-byte 0/1 flag)
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetDefaultGameCamera(bool lbDefaultGameCamera)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mbDefaultGameCamera = lbDefaultGameCamera ? 1 : 0;
}

bool OptionsDataProfile::GetDefaultGameCamera()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return mbDefaultGameCamera == 1;
}

// ---------------------------------------------------------------------------
// VOIP volume
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetVoipVolume(s32 liVolume)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    miVoipVolume = liVolume;
}

s32 OptionsDataProfile::GetVoipVolume()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return miVoipVolume;
}

// ---------------------------------------------------------------------------
// Music volume
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetMusicVolume(s32 liVolume)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mMusicVolume = liVolume;
}

s32 OptionsDataProfile::GetMusicVolume()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return mMusicVolume;
}

// ---------------------------------------------------------------------------
// SFX volume
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetSFXVolume(s32 liVolume)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mSFXVolume = liVolume;
}

s32 OptionsDataProfile::GetSFXVolume()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return mSFXVolume;
}

// ---------------------------------------------------------------------------
// Brightness (clamped/validated to [KI_MIN_BRIGHTNESS, KI_MAX_BRIGHTNESS])
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetBrightness(s32 liNewBrightnessSetting)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    CGS_ASSERT(liNewBrightnessSetting >= KI_MIN_BRIGHTNESS, "lNewBrightnessSetting >= KI_MIN_BRIGHTNESS");
    CGS_ASSERT(liNewBrightnessSetting <= KI_MAX_BRIGHTNESS, "lNewBrightnessSetting <= KI_MAX_BRIGHTNESS");
    mBrightness = liNewBrightnessSetting;
}

s32 OptionsDataProfile::GetBrightness() const
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    CGS_ASSERT(mBrightness >= KI_MIN_BRIGHTNESS, "mBrightness >= KI_MIN_BRIGHTNESS");
    CGS_ASSERT(mBrightness <= KI_MAX_BRIGHTNESS, "mBrightness <= KI_MAX_BRIGHTNESS");
    return mBrightness;
}

// ---------------------------------------------------------------------------
// Contrast (clamped/validated to [KI_MIN_CONTRAST, KI_MAX_CONTRAST])
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetContrast(s32 liNewContrastSetting)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    CGS_ASSERT(liNewContrastSetting >= KI_MIN_CONTRAST, "lNewContrastSetting >= KI_MIN_CONTRAST");
    CGS_ASSERT(liNewContrastSetting <= KI_MAX_CONTRAST, "lNewContrastSetting <= KI_MAX_CONTRAST");
    mContrast = liNewContrastSetting;
}

s32 OptionsDataProfile::GetContrast() const
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    CGS_ASSERT(mContrast >= KI_MIN_CONTRAST, "mContrast >= KI_MIN_CONTRAST");
    CGS_ASSERT(mContrast <= KI_MAX_CONTRAST, "mContrast <= KI_MAX_CONTRAST");
    return mContrast;
}

// ---------------------------------------------------------------------------
// Six-axis showtime / steering
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetSixAxisShowtime(bool lbSixAxisShowtime)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mbSixAxisShowtime = lbSixAxisShowtime;
}

bool OptionsDataProfile::GetSixAxisShowtime()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return mbSixAxisShowtime;
}

void OptionsDataProfile::SetSixAxisSteering(bool lbSixAxisSteering)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mbSixAxisSteering = lbSixAxisSteering;
}

bool OptionsDataProfile::GetSixAxisSteering()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return mbSixAxisSteering;
}

// ---------------------------------------------------------------------------
// Force feedback
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetForceFeedback(bool lbForceFeedback)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mbForceFeedback = lbForceFeedback;
}

bool OptionsDataProfile::GetForceFeedback()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return mbForceFeedback;
}

// ---------------------------------------------------------------------------
// Tips
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetTips(bool lbTips)
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    mbTips = lbTips;
}

bool OptionsDataProfile::GetTips()
{
    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");
    return mbTips;
}

// ---------------------------------------------------------------------------
// Online game options — received table
//
// GetReceivedOnlineGameOptions: read entry liIndex of the received route table
// (base +0x3988, stride 0x5C0) back out into game params. The X360 asserts the
// cache/params pointers and that the index is in [0, 10). The index-range asserts
// stream the offending index into the assert buffer; per the assert convention
// they collapse to a single CGS_ASSERT with the constant message prefix.
// ---------------------------------------------------------------------------
void OptionsDataProfile::GetReceivedOnlineGameOptions(s32 liIndex, GuiCache* lpCache,
                                                      GuiEventNetworkGameParams* lpParams) const
{
    CGS_ASSERT(lpCache, "lpGuiCache");
    CGS_ASSERT(lpParams, "lpParams");
    CGS_ASSERT(liIndex >= 0, "Invalid Received online game options index of ");
    CGS_ASSERT(liIndex < KI_MAX_RECEIVED_ONLINE_GAME_OPTIONS, "Invalid Received online game options index of ");

    maReceivedOnlineGameOptions[liIndex].SetToGameParams(lpCache, lpParams);
}

// ---------------------------------------------------------------------------
// Online game options — created table
//
// SetCreatedOnlineGameOptions: store game params into entry liIndex of the created
// route table (base +0x8, stride 0x5C0). After writing, if the index reaches or
// exceeds the current count, bump the count and assert it is exactly liIndex+1.
// ---------------------------------------------------------------------------
void OptionsDataProfile::SetCreatedOnlineGameOptions(s32 liIndex, GuiCache* lpCache,
                                                     const GuiEventNetworkGameParams* lpParams)
{
    CGS_ASSERT(lpCache, "lpGuiCache");
    CGS_ASSERT(lpParams, "lpOptions");
    CGS_ASSERT(liIndex >= 0, "Invalid Created online game options index of ");
    CGS_ASSERT(liIndex < KI_MAX_CREATED_ONLINE_GAME_OPTIONS, "Invalid Created online game options index of ");

    maCreatedOnlineGameOptions[liIndex].SetFromGameParams(lpCache, lpParams);

    if (liIndex >= miNumCreatedOnlineGameOptions)
    {
        ++miNumCreatedOnlineGameOptions;
        CGS_ASSERT(miNumCreatedOnlineGameOptions == (liIndex + 1),
                   "miNumCreatedOnlineGameOptions == (liIndex+1)");
    }
}

// ---------------------------------------------------------------------------
// Online game options — append received
//
// AddReceivedOnlineGameOptions: shift the received route table (base +0x3988,
// stride 0x5C0) up by one slot to make room at the front (memmove of 9 entries =
// 0x33C0 bytes from slot 0 to slot 1), write the new entry into slot 0, then bump
// the received count (capped at KI_MAX_RECEIVED_ONLINE_GAME_OPTIONS).
// ---------------------------------------------------------------------------
void OptionsDataProfile::AddReceivedOnlineGameOptions(GuiCache* lpCache,
                                                      const GuiEventNetworkGameParams* lpParams)
{
    CGS_ASSERT(lpCache, "lpGuiCache");
    CGS_ASSERT(lpParams, "lpOptions");

    memmove(&maReceivedOnlineGameOptions[1], &maReceivedOnlineGameOptions[0],
            sizeof(OnlineSaveRoute) * (KI_MAX_RECEIVED_ONLINE_GAME_OPTIONS - 1));

    maReceivedOnlineGameOptions[0].SetFromGameParams(lpCache, lpParams);

    if (miNumReceivedOnlineGameOptions < KI_MAX_RECEIVED_ONLINE_GAME_OPTIONS)
        ++miNumReceivedOnlineGameOptions;
}

// ---------------------------------------------------------------------------
// Compile-time layout pin: every offset here is X360-asm-authoritative.
// ---------------------------------------------------------------------------
void OptionsDataProfile::_AssertLayout()
{
    static_assert(sizeof(EATraxArrayType) == 16, "EATraxArrayType must be a 128-bit (16-byte) bit set");

    // Online-game-option route tables: stride 0x5C0, created @+0x8, received @+0x3988,
    // num counters @+0x7308/+0x730C (all X360-asm-authoritative).
    static_assert(sizeof(OnlineSaveRoute) == 0x5C0, "OnlineSaveRoute stride must be 0x5C0");
    static_assert(offsetof(OptionsDataProfile, maCreatedOnlineGameOptions)    == 0x0008, "maCreatedOnlineGameOptions @ 0x0008");
    static_assert(offsetof(OptionsDataProfile, maReceivedOnlineGameOptions)   == 0x3988, "maReceivedOnlineGameOptions @ 0x3988");
    static_assert(offsetof(OptionsDataProfile, miNumCreatedOnlineGameOptions) == 0x7308, "miNumCreatedOnlineGameOptions @ 0x7308");
    static_assert(offsetof(OptionsDataProfile, miNumReceivedOnlineGameOptions) == 0x730C, "miNumReceivedOnlineGameOptions @ 0x730C");

    static_assert(offsetof(OptionsDataProfile, mTraxAvailableInFreeBurn) == 0x7310, "mTraxAvailableInFreeBurn @ 0x7310");
    static_assert(offsetof(OptionsDataProfile, mTraxAvailableInEvents)   == 0x7320, "mTraxAvailableInEvents @ 0x7320");
    static_assert(offsetof(OptionsDataProfile, mTraxFullyPlayed)         == 0x7330, "mTraxFullyPlayed @ 0x7330");
    static_assert(offsetof(OptionsDataProfile, meTraxPlayOrderMode)      == 0x7340, "meTraxPlayOrderMode @ 0x7340");
    static_assert(offsetof(OptionsDataProfile, mbDefaultGameCamera)      == 0x734C, "mbDefaultGameCamera @ 0x734C");
    static_assert(offsetof(OptionsDataProfile, mBrightness)              == 0x7350, "mBrightness @ 0x7350");
    static_assert(offsetof(OptionsDataProfile, mContrast)                == 0x7354, "mContrast @ 0x7354");
    static_assert(offsetof(OptionsDataProfile, miVoipVolume)             == 0x7358, "miVoipVolume @ 0x7358");
    static_assert(offsetof(OptionsDataProfile, mMusicVolume)             == 0x735C, "mMusicVolume @ 0x735C");
    static_assert(offsetof(OptionsDataProfile, mSFXVolume)               == 0x7360, "mSFXVolume @ 0x7360");
    static_assert(offsetof(OptionsDataProfile, meCameraFeedSetting)      == 0x7364, "meCameraFeedSetting @ 0x7364");
    static_assert(offsetof(OptionsDataProfile, mbSixAxisShowtime)        == 0x7369, "mbSixAxisShowtime @ 0x7369");
    static_assert(offsetof(OptionsDataProfile, mbSixAxisSteering)        == 0x736A, "mbSixAxisSteering @ 0x736A");
    static_assert(offsetof(OptionsDataProfile, mbForceFeedback)          == 0x736B, "mbForceFeedback @ 0x736B");
    static_assert(offsetof(OptionsDataProfile, mbTips)                   == 0x736D, "mbTips @ 0x736D");
    static_assert(offsetof(OptionsDataProfile, mbIsLocked)               == 0x736E, "mbIsLocked @ 0x736E");
}

// ---------------------------------------------------------------------------
// OnlineSaveRoute::OnlineSaveRouteEvent::Construct  @0x824EBD18
//   Store the event id / junction id / landmark count and copy liNumLandmarks
//   CgsID (u64) landmark indices into maLandmarkIndices[].
// ---------------------------------------------------------------------------
void OptionsDataProfile::OnlineSaveRoute::OnlineSaveRouteEvent::Construct(
    s32 liEventID, u32 luJunctionID, CgsID* lpaLandmarks, s32 liNumLandmarks)
{
    CGS_ASSERT(liNumLandmarks <= KI_MAX_LANDMARKS_IN_MODE,
               "liNumLandmarks <= KI_MAX_LANDMARKS_IN_MODE");

    miEventID      = liEventID;      // +0x88
    mJunctionId    = luJunctionID;   // +0x80
    miNumLandmarks = liNumLandmarks; // +0x84

    for (s32 li = 0; li < miNumLandmarks; ++li)
    {
        maLandmarkIndices[li] = lpaLandmarks[li];
    }
}

// ---------------------------------------------------------------------------
// OnlineSaveRoute::OnlineSaveRouteEvent::GetLandmark  @0x824EBDA8
//   Bounds-checked read of maLandmarkIndices[liIndex].
// ---------------------------------------------------------------------------
CgsID OptionsDataProfile::OnlineSaveRoute::OnlineSaveRouteEvent::GetLandmark(s32 liIndex) const
{
    CGS_ASSERT(liIndex >= 0, "liIndex=");
    CGS_ASSERT(liIndex < miNumLandmarks, "liIndex= GetNumLandmarks()=");

    return maLandmarkIndices[liIndex]; // +0x00 + liIndex*8
}

// ---------------------------------------------------------------------------
// FLAG trap-stub bodies (link scaffold, 2026-07-12): the two OnlineSaveRoute
// game-params transfer bodies below are declared (DWARF) and referenced by this
// TU's own Add/Get*OnlineGameOptions accessors, but their reconstructions
// (@0x82507448 SetFromGameParams / @0x82507570 SetToGameParams -- the
// GuiEventNetworkGameParams field-by-field route transfer) have not landed.
// Trap bodies per the stub scaffold -- reachable only through the online
// game-options flows, never on the boot path. Replace each with its faithful
// decompile.
// ---------------------------------------------------------------------------
void OptionsDataProfile::OnlineSaveRoute::SetFromGameParams(GuiCache*, const GuiEventNetworkGameParams*)       { __debugbreak(); }   // FLAG trap-stub
void OptionsDataProfile::OnlineSaveRoute::SetToGameParams(GuiCache*, GuiEventNetworkGameParams*) const         { __debugbreak(); }   // FLAG trap-stub

// ---------------------------------------------------------------------------
// NotifyOtherModulesLoadComplete  @0x82511098
//
// Post the loaded profile's settings to the other modules as one burst of GUI
// events on the caller's load-complete queue. The X360 record shapes / event
// ids / wire sizes, straight from the asm:
//   458 (0x1CA, 32B) : { mTraxAvailableInFreeBurn, mTraxAvailableInEvents }
//   459 (0x1CB, 24B) : { mTraxFullyPlayed, miLastPlayedSongIndex,
//                        miLastPictureParadiseMusicIndex }
//   463 (0x1CF,  8B) : { mMusicVolume, mSFXVolume }
//   -- assert "false == mbIsLocked" (cpp:640) --
//   474 (0x1DA,  4B) : miVoipVolume
//   462 (0x1CE,  4B) : meTraxPlayOrderMode
//   475 (0x1DB,  4B) : mbDefaultGameCamera (the 4-byte flag word)
//   472 (0x1D8,  3B) : { mbSixAxisShowtime, mbSixAxisSteering, mbForceFeedback }
//   473 (0x1D9,  1B) : mbTips
//   278 (0x116,  4B) : meCameraFeedSetting
//
// The parameter arrives as the header's opaque BrnGui::GuiEventQueueSmall
// forward; the real object is the CgsGui small queue (a CgsModule::
// VariableEventQueue<4096,16> -- ScreenLoading::ApplyOptionsDataProfileSettings
// constructs exactly that and bridges the pointer). Recovered here the same way
// the GUI states recover their opaque in-queues.
// ---------------------------------------------------------------------------
void OptionsDataProfile::NotifyOtherModulesLoadComplete(GuiEventQueueSmall* lpQueue)
{
    typedef CgsModule::VariableEventQueue<4096, 16> LoadCompleteQueue;
    LoadCompleteQueue* lpOutQueue = reinterpret_cast<LoadCompleteQueue*>(lpQueue);

    // 458: both trax-availability sets (two 16-byte FastBitArray<128> payloads).
    {
        struct TraxAvailableRecord
        {
            EATraxArrayType mFreeBurn;
            EATraxArrayType mEvents;
        };
        TraxAvailableRecord lRecord;
        lRecord.mFreeBurn = mTraxAvailableInFreeBurn;
        lRecord.mEvents   = mTraxAvailableInEvents;
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), 458, 32);
    }

    // 459: the fully-played set plus the two last-played indices.
    {
        struct TraxPlayedRecord
        {
            EATraxArrayType mFullyPlayed;
            s32             miLastPlayedSongIndex;
            s32             miLastPictureParadiseMusicIndex;
        };
        TraxPlayedRecord lRecord;
        lRecord.mFullyPlayed                    = mTraxFullyPlayed;
        lRecord.miLastPlayedSongIndex           = miLastPlayedSongIndex;
        lRecord.miLastPictureParadiseMusicIndex = miLastPictureParadiseMusicIndex;
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), 459, 24);
    }

    // 463: the music/SFX volume pair.
    {
        s32 laiVolumes[2] = { mMusicVolume, mSFXVolume };
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(laiVolumes), 463, 8);
    }

    CGS_ASSERT(false == mbIsLocked, "false == mbIsLocked");   // cpp:640

    // The scalar settings, one event each (X360 wire sizes).
    {
        s32 liValue = miVoipVolume;
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liValue), 474, 4);

        liValue = static_cast<s32>(meTraxPlayOrderMode);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liValue), 462, 4);

        liValue = mbDefaultGameCamera;
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liValue), 475, 4);
    }

    // 472: the three controller-behaviour bytes, contiguously.
    {
        u8 lauControllerFlags[3];
        lauControllerFlags[0] = mbSixAxisShowtime ? 1 : 0;
        lauControllerFlags[1] = mbSixAxisSteering ? 1 : 0;
        lauControllerFlags[2] = mbForceFeedback ? 1 : 0;
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lauControllerFlags), 472, 3);
    }

    // 473: the tips byte.
    {
        u8 luTips = mbTips ? 1 : 0;
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&luTips), 473, 1);
    }

    // 278: the camera-feed selection.
    {
        s32 liCameraFeed = static_cast<s32>(meCameraFeedSetting);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liCameraFeed), 278, 4);
    }
}

} // namespace BrnGui
