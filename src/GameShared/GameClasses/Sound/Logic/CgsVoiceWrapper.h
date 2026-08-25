#ifndef CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H
#define CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"      // embedded CgsSound::Logic::Voice (+0x34)
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"  // ref-counted Playback::Object (+0x40 handle)

// =============================================================================
// CgsSound::Logic::VoiceWrapper -- per-slot voice wrapper (additive grow).
//   GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h  (DWARF home inferred) +
//   GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. VoiceWrapper is the per-slot voice
// wrapper the sound-logic layer embeds. VoicePool<4> is an array of four PooledVoice
// slots (CgsVoicePool.h: this wrapper + the pool bookkeeping scalars); the
// PresentationEffect AgingVoice embeds it too.
//
// -----------------------------------------------------------------------------
// VoiceWrapper X360 LAYOUT (console sizeof == 0x50; corrected 2026-08-25 wave 4 --
// the earlier "wrapper slot spans 0x5C" claim CONFLATED the POOLED-SLOT stride
// with the wrapper: the 0x5C ctor/dtor stride @0x826E5328/0x826E5370 covers
// PooledVoice == this 0x50 wrapper + {mfSecondaryGain, muAge, mbInUse} == 0x5C,
// and PresentationEffect's 0x80 slot == u16 age + this wrapper @+0x04 +
// PresentationEntry @+0x58 + timer):
//
//     +0x00        vptr               wrapper vtable (off_820B0E54 mid-teardown;
//                                      off_820AA820 == MemBase base settles last)
//     +0x04..+0x2F mau32Deferred04    11 words, zero-seeded by VoicePoolBase::
//                                      Prepare @0x826B6528 (fields un-attested)
//     +0x30        miNameWord30       Prepare seeds -1 (an interned-name/id slot)
//     +0x34        mVoice             embedded CgsSound::Logic::Voice (its own vptr
//                                      off_820B0E20; its mVoiceHandle.mpObject is
//                                      the console *(this+0x38))
//     +0x40        mpObject           a ref-counted Playback::Object* handle,
//                                      dropped FIRST in the dtor (v1[16]==+0x40)
//     +0x44        mu32Field44        zero-seeded by Prepare (un-attested)
//     +0x48        miState            the wrapper state word the pool/effects test
//                                      (KI_VOICE_STATE_FREE 0 / PLAYING 6 / STOPPED 7)
//     +0x4C        mu8Field4C         zero-seeded by Prepare (un-attested)
//
// GetGain (0x826C5218) null-checks the embedded Voice's owned pointer at +0x38 and
// forwards to Voice::GetGain(&mVoice, ...).
//
// FLAG (middle/trailing spans modelled with neutral names): the +0x04..+0x33 and
// +0x44/+0x4C fields carry no attested names yet -- they are materialised so the
// pool/effect bodies can seed and test them BY NAME instead of raw byte offsets
// (the former CgsVoicePool KU_POOLED_VOICE_*_OFFSET reaches). Rename as their
// slices land.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// pointer members widen to 8 bytes on the 64-bit host, so the absolute byte offsets
// are recorded in comments only and are NOT static_asserted here.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// The per-slot voice wrapper. Virtual (the X360 ctor/dtor install a vtable at +0).
class VoiceWrapper
{
public:
    // @ 0x826E5350 (the `bl VoiceWrapper::VoiceWrapper` in the pool ctor). The real
    // body is not separately dumped in this TU; the members below default-construct
    // (embedded Voice null-inits its handle; scalars zero; miNameWord30 = -1 mirrors
    // the Prepare seed).
    VoiceWrapper() : miNameWord30(-1), mpObject(0), mu32Field44(0), miState(0), mu8Field4C(0)
    {
        for (u32 lu = 0; lu < 11u; ++lu)
            mau32Deferred04[lu] = 0;
    }

    // Anchor for the X360 `scalar deleting destructor' @ 0x826E0B88. The teardown is
    // the sequence the pool dtor @ 0x826E5370 inlines per element: re-install the
    // wrapper vtable, Release() the wrapper, drop the +0x40 ref-counted object handle
    // (assert count > 0, --count, DoDispose at zero), then tear down the embedded
    // Voice's +0x38 handle the same way, and settle the MemBase base vtable. Bodied
    // out-of-line in CgsVoiceWrapper.cpp.
    virtual ~VoiceWrapper();

    // @ 0x826C5218. If the embedded Voice has no playback voice yet (its handle's
    // mpObject, read as *(this+0x38), is null) return 0.0f; otherwise spill the
    // caller's send-name word and forward to Voice::GetGain(&mVoice, &sendName).
    f32 GetGain(const s32* lpSendName) const;

    // The wrapper's own detach step -- the `bl VoiceWrapper::Release` the pool dtor
    // (and VoicePoolBase::Release/GetFreeVoice) run. Declared only; its body is a
    // separate slice (DEFERRED on the wrapper Release recon). FLAG: unresolved
    // external if a calling TU is mounted before that slice lands.
    void Release();

    // Per-frame wrapper tick, called by VoicePoolBase::Update on every slot.
    // Declared only; its body is a separate slice (DEFERRED). FLAG: unresolved
    // external if a calling TU is mounted before that slice lands.
    void Update();

    // ---- named access for the console raw reads (additive, 2026-08-25 wave 4) ----
    // The state word @+0x48 the pool (VoicePoolBase bodies) and the effects
    // (PresentationEffect::FindFreeVoice @0x82687D68) test/seed.
    s32  GetState() const      { return miState; }
    void SetState(s32 liState) { miState = liState; }

    // The embedded logic Voice @+0x34 (the console `slot + 0x34` reaches; SetGain /
    // SetParameter broadcast targets).
    Voice&       GetVoice()       { return mVoice; }
    const Voice& GetVoice() const { return mVoice; }

    // Whether the embedded Voice owns a live playback voice (the console non-null
    // test of *(this+0x38) == mVoice.mVoiceHandle.mpObject).
    bool HasLiveVoice() const { return mVoice.GetVoiceObject() != 0; }

    // Zero-seed the un-attested spans exactly as VoicePoolBase::Prepare @0x826B6528
    // stores them (11 zero words @+0x04.., -1 @+0x30, zeros @+0x44/+0x48/+0x4C).
    // Named replacement for Prepare's former raw byte stores; same store set.
    void ResetDeferredState()
    {
        for (u32 lu = 0; lu < 11u; ++lu)
            mau32Deferred04[lu] = 0;
        miNameWord30 = -1;
        mu32Field44  = 0;
        miState      = 0;
        mu8Field4C   = 0;
    }

private:
    // +0x04..+0x2F (X360). FLAG: un-attested fields, zero-seeded by the pool Prepare.
    u32 mau32Deferred04[11];

    // +0x30 (X360). Prepare seeds -1 (an interned-name/id slot). FLAG: name un-attested.
    s32 miNameWord30;

    // +0x34 (X360). The embedded sound-logic voice. Its mVoiceHandle.mpObject is read
    // as *(this+0x38) by GetGain and torn down by the dtor.
    Voice mVoice;

    // +0x40 (X360). A ref-counted playback object this wrapper owns. The dtor drops it
    // FIRST (assert its refcount > 0, --refcount, DoDispose at zero). Held by raw
    // pointer to mirror the X360 handle word (`lwz r31, 0x40(r30)`).
    Playback::Object* mpObject;

    // +0x44 (X360). FLAG: un-attested field, zero-seeded by the pool Prepare.
    u32 mu32Field44;

    // +0x48 (X360). The wrapper state word (KI_VOICE_STATE_*).
    s32 miState;

    // +0x4C (X360). FLAG: un-attested field, zero-seeded by the pool Prepare.
    u8 mu8Field4C;
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H
