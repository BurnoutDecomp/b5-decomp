#include "GameSource/Sound/Global/BrnMixerControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>

// =============================================================================
// BrnSound::Logic::MixerControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnMixerControl.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries:
//   `vector deleting destructor'              @ 0x826BAED8
//   `vector deleting destructor' adjustor{4}  @ 0x826BAED0
// (the second is a compiler-generated thunk; see note).
//
// dep_flags: none un-homed for THIS TU. The destructor teardown touches only the
// committed BrnEffectControl base members (meDetachState / mbResourcesReady /
// meAttachState, all pinned BY NAME). The (a2 & 1) deallocation tail dispatches
// the global sound allocator (off_82FFB954); that allocator vtable is not homed
// here, so the `delete` half of the X360 vector deleting destructor is left to the
// host toolchain (same treatment as the CameraControl / BrnEffectControl siblings).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

CgsSound::Logic::EffectControl* MixerControl::CreateObject(u32)
{
    return new MixerControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* MixerControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x00, "MixerControl", CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &MixerControl::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* MixerControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* MixerControl::GetTypeName() const
{
    return "MixerControl";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpMixerControlReg =
    CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(MixerControl::GetStaticTypeInfo());

void MixerControl::SetupLoadData()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CgsSound::Playback::Environment* lpPlaybackEnvironment =
        lpModule->GetPlaybackModule().GetEnvironment();
    CGS_ASSERT(lpPlaybackEnvironment != 0, "mpObject");

    if (lpPlaybackEnvironment->GetAudioMode() ==
        CgsSound::Playback::Environment::E_AUDIO_MODE_SURROUND)
    {
        mpcNicotineBundle = "Sound\\NicotineAssetSurround.bundle";
        mpcNicotineAsset = "NicotineAssetSurround";
        mpcNicotineSnapshotAsset = "NicotineAssetSurround.mss";
    }
    else
    {
        mpcNicotineBundle = "Sound\\NicotineAssetMain.bundle";
        mpcNicotineAsset = "NicotineAssetMain";
        mpcNicotineSnapshotAsset = "NicotineAssetMain.mss";
    }

    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        mpcNicotineBundle, mpcNicotineAsset, ResourceRegistrar::E_DATA);
    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        mpcNicotineBundle, mpcNicotineSnapshotAsset, ResourceRegistrar::E_DATA);
}

bool MixerControl::Attach()
{
    mCachedSettings.miMusicVolume = 8;
    mCachedSettings.miSFXVolume = 8;
    CgsSound::Logic::EffectBase::Attach();
    mpMixerData = 0;
    RestartMixer();
    return true;
}

void MixerControl::UpdateParams(f32)
{
    const BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    const BrnSound::Logic::FrameInformation& lrFrame = lpModule->GetFrameInformation();

    SetMixerInputValue(0, lrFrame.meImpactTime.GetCurrent() == 1 ? 0 : 0x7FFF);
    SetMixerInputValue(1, lrFrame.meFatality.GetCurrent() == E_FATAL_OFF ? 0 : 0x7FFF);
    SetMixerInputValue(2, (!lrFrame.maPaused.IsZero() || lrFrame.mbInReplay) ? 0 : 0x7FFF);
    SetMixerInputValue(3, 0x7FFF * mCachedSettings.miMusicVolume / 11);
    SetMixerInputValue(4, 0x7FFF * mCachedSettings.miSFXVolume / 11);
}

void MixerControl::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    CGS_ASSERT(apMessage && (apMessage->GetEventId() == 12 || apMessage->GetEventId() == 43),
        "( lpMessageHeader ) && ( lpMessageHeader->GetEventId() == E_SOUNDMESSAGE_SETTINGS || lpMessageHeader->GetEventId() == E_SOUNDMESSAGE_RESTART_MIXER )");
    if (!apMessage)
        return;

    if (apMessage->GetEventId() == 12)
    {
        const CgsSound::Io::Message<GuiEventAudioSettings>* lpSettings =
            static_cast<const CgsSound::Io::Message<GuiEventAudioSettings>*>(apMessage);
        mCachedSettings = lpSettings->mData;
    }
    else if (apMessage->GetEventId() == 43)
    {
        RestartMixer();
    }
}

void MixerControl::RestartMixer()
{
    ResourceRegistrar& lrRegistrar = GetResourceRegistrar();
    CgsResource::ResourceHandle* lpMapHandle =
        lrRegistrar.GetResource(mpcNicotineBundle, mpcNicotineAsset);
    CgsResource::ResourceHandle* lpSnapshotHandle =
        lrRegistrar.GetResource(mpcNicotineBundle, mpcNicotineSnapshotAsset);
    CGS_ASSERT(lpMapHandle != 0, "lpMixerMapHandle");
    CGS_ASSERT(lpSnapshotHandle != 0, "lpSnapshotHandle");
    if (!lpMapHandle || !lpSnapshotHandle)
        return;

    CgsResource::ResourcePtr<CgsResource::BinaryFileResource> lMap(*lpMapHandle);
    CgsResource::ResourcePtr<CgsResource::BinaryFileResource> lSnapshots(*lpSnapshotHandle);
    const u32 luMapSize = lMap->GetSize();

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CgsSound::Playback::Environment* lpPlaybackEnvironment =
        lpModule->GetPlaybackModule().GetEnvironment();
    if (mpMixerData)
        lpPlaybackEnvironment->Free(mpMixerData);
    mpMixerData = static_cast<u8*>(
        lpPlaybackEnvironment->Allocate(luMapSize, 16, "MixerMapData"));
    CGS_ASSERT(mpMixerData != 0, "mpMixerData");
    if (!mpMixerData)
        return;
    std::memcpy(mpMixerData, lMap->GetData(), luMapSize);

    Nicotine::IDynamicMixer& lrMixer = lpModule->GetEnvironment().GetDynamicMixer();
    lrMixer.DestroyMap();
    lrMixer.InitMap(reinterpret_cast<int*>(mpMixerData));
    lrMixer.InitSnapshots(const_cast<void*>(lSnapshots->GetData()));
    lrMixer.SetSnapshot(0, true);
    lrMixer.ProcessMixMap(0, 0.16f);

}

// ---------------------------------------------------------------------------
// ~MixerControl  @ 0x826BAED8  (the X360 `vector deleting destructor')
//
//   stw  off_820AEA6C, 0(this)   ; primary (MixerControl) vptr settle
//   stw  off_820AEA38, 4(this)   ; (transient) base IResourceRequester vptr
//   li   r7, 3 ; stw r7, 0x28(this) ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(this)   ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(this)           ; mbResourcesReady = false
//   stw  0, 0x24(this)           ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 }
//   return this
//
// The vptr stores are the compiler-emitted devirtualization of the destructor
// base sub-objects; the observable member teardown is the inherited
// BrnEffectControl teardown (meDetachState/mbResourcesReady/meAttachState). The
// MixerControl-owned members are non-owning, so this leaf destructor runs only the
// inherited base teardown (the base destructor body), leaving this body empty.
// ---------------------------------------------------------------------------
MixerControl::~MixerControl()
{
}

// ---------------------------------------------------------------------------
// `vector deleting destructor' adjustor{4}  @ 0x826BAED0
//
//   0x826BAED0  addi r3, r3, -4
//   0x826BAED4  b    BrnSound::Logic::MixerControl::`vector deleting destructor'
//
// The IResourceRequester sub-object (the second base of BrnEffectControl) lives
// at this+4; a delete through an IResourceRequester* enters here, recovers the
// primary MixerControl `this` (this - 4), and forwards to the real destructor. In
// reconstructed C++ this thunk is generated by the compiler from BrnEffectControl's
// multiple-inheritance vtable layout (the virtual destructor declared in the header
// is reached through the IResourceRequester base), so no hand-written body is
// emitted.
// FLAG: thunk reproduced structurally by the inheritance declaration in the header
// (MixerControl : public BrnEffectControl); not a hand-bodied function.
// ---------------------------------------------------------------------------

} // namespace Logic
} // namespace BrnSound
