#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"

#include <cstddef>

#include "GameShared/GameClasses/Core/CgsAssert.h"

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
// Compile-time layout pin: every offset here is X360-asm-authoritative.
// ---------------------------------------------------------------------------
void OptionsDataProfile::_AssertLayout()
{
    static_assert(sizeof(EATraxArrayType) == 16, "EATraxArrayType must be a 128-bit (16-byte) bit set");

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

} // namespace BrnGui
