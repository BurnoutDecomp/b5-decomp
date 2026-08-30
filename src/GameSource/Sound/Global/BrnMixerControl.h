#ifndef BRN_SOUND_LOGIC_BRN_MIXER_CONTROL_H
#define BRN_SOUND_LOGIC_BRN_MIXER_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"

// =============================================================================
// BrnSound::Logic::MixerControl
//   GameSource/Sound/Global/BrnMixerControl.h (DWARF home) +
//   GameSource/Sound/Global/BrnMixerControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. MixerControl is the sound-logic
// effect-control that drives the runtime audio mixer (the "Nicotine" snapshot
// mixer). DWARF (BrnMixerControl.h:36):
//   struct BrnSound::Logic::MixerControl : public BrnSound::Logic::BrnEffectControl
// so it reuses the committed BrnEffectControl base (which itself multiply
// inherits CgsSound::Logic::EffectControl @ this+0 and IResourceRequester @
// this+4). MixerControl adds its own mixer-data / Nicotine-asset members and a
// cached GUI audio-settings snapshot (modelled minimally below, by name).
//
// This TU's recon'd function set is exactly TWO entries:
//   `vector deleting destructor'              @ 0x826BAED8
//   `vector deleting destructor' adjustor{4}  @ 0x826BAED0
// The X360 vector deleting destructor teardown is the inherited BrnEffectControl
// teardown verbatim:
//   stw  off_820AEA6C, 0(this)   ; primary (MixerControl) vptr settle
//   stw  off_820AEA38, 4(this)   ; (transient) base IResourceRequester vptr
//   li   r7, 3 ; stw r7, 0x28(this) ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(this)   ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(this)           ; mbResourcesReady = false
//   stw  0, 0x24(this)           ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the global sound allocator) }
//   return this
// All observable member teardown is in the committed BrnEffectControl base; the
// MixerControl-owned members (mpMixerData / Nicotine asset name pointers /
// mCachedSettings) are non-owning POD here, so the leaf destructor body is empty
// (same shape as the CameraControl sibling home).
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace
// MixerControl TU. The full DWARF surface (the RTTI GetTypeInfo/GetTypeName/
// GetStaticTypeInfo/CreateObject hooks, SetupLoadData/Attach/UpdateParams/Notify,
// GetMusicVolumeForMixer, RestartMixer, and the real GuiEventAudioSettings /
// mixer-data types) is DEFERRED to its own TU(s); only the destructor (and thereby
// the adjustor{4} thunk) is materialised here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the inherited BrnEffectControl
// teardown offsets (meDetachState @ +0x28, mbResourcesReady @ +0x31,
// meAttachState @ +0x24) are X360 byte offsets assuming 4-byte pointers/vptr;
// members are pinned BY NAME via the committed base and absolute offsets are NOT
// static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnMixerControl.h:87 (DWARF). Cached GUI audio-settings snapshot the mixer
// reads when it (re)applies the Nicotine mixer state. The destructor does not
// touch this member, so only a minimal opaque shape is modelled here.
// FLAG: minimal placeholder for the un-homed GuiEventAudioSettings; its real
// surface (the GUI audio-settings fields) is reconstructed in its own home/TU.
struct GuiEventAudioSettings
{
    s32 miMusicVolume;
    s32 miSFXVolume;
};

// BrnMixerControl.h:36 (DWARF). Reuses the committed BrnEffectControl base by
// name. The virtual (vector deleting) destructor @ 0x826BAED8 and its
// IResourceRequester-base adjustor{4} thunk @ 0x826BAED0 are generated
// structurally from the dual-base layout of BrnEffectControl.
struct MixerControl : public BrnSound::Logic::BrnEffectControl
{
    MixerControl()
        : mpMixerData(nullptr), mpcNicotineBundle(nullptr), mpcNicotineAsset(nullptr),
          mpcNicotineSnapshotAsset(nullptr), mCachedSettings() {}
    virtual ~MixerControl();

    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectControl* CreateObject(u32 auType);

    void SetupLoadData() override;
    bool Attach() override;
    void UpdateParams(f32 afDeltaTime) override;
    void Notify(const CgsSound::Io::MessageHeader* apMessage) override;
    s32 GetMusicVolumeForMixer() const { return mCachedSettings.miMusicVolume; }

    // MixerControl-owned members observed in the DWARF (BrnMixerControl.h:80-87),
    // pinned BY NAME. The destructor does not tear these down (non-owning), so
    // they carry no teardown behaviour in this TU.
private:
    void RestartMixer();

    u8*         mpMixerData;              // BrnMixerControl.h:80
    const char* mpcNicotineBundle;        // BrnMixerControl.h:82
    const char* mpcNicotineAsset;         // BrnMixerControl.h:83
    const char* mpcNicotineSnapshotAsset; // BrnMixerControl.h:84
    GuiEventAudioSettings mCachedSettings; // BrnMixerControl.h:87
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_MIXER_CONTROL_H
