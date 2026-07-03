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
// wrapper the sound-logic layer embeds. VoicePool<4> is an array of four of them
// (embedded by BrnSound::Vehicles::Wheels::InAirEffect); the pool's slot view of the
// same memory is CgsSound::Logic::PooledVoice (CgsVoicePool.h -- an opaque 0x50 span
// over this wrapper + the pool bookkeeping scalars).
//
// -----------------------------------------------------------------------------
// VoiceWrapper X360 LAYOUT (proven by the inlined ~VoiceWrapper in VoicePool<4>::
// ~VoicePool<4> @ 0x826E5370 and by VoiceWrapper::GetGain @ 0x826C5218):
//   the wrapper slot spans 0x5C (92 bytes). PROOF: the ctor loop @ 0x826E5328
//   strides the four wrappers by 0x5C (`addi r30,r30,0x5C`), and the dtor walks
//   this+0x184 down to this+0x14 in 0x5C steps (0x14 + 4*0x5C == 0x184).
//
//     +0x00  vptr                     wrapper vtable (off_820B0E54 mid-teardown;
//                                      off_820AA820 == MemBase base settles last)
//     +0x34  mVoice                   embedded CgsSound::Logic::Voice (its own vptr
//                                      off_820B0E20 re-installed by ~Voice; its
//                                      mVoiceHandle.mpObject read as *(this+0x38))
//     +0x40  mpObject                 a ref-counted Playback::Object* handle,
//                                      dropped FIRST in the dtor (v1[16]==+0x40)
//
// GetGain (0x826C5218) null-checks the embedded Voice's owned pointer at +0x38 and
// forwards to Voice::GetGain(&mVoice, ...).
//
// FLAG (additive, minimal): only the two members reached by this TU's own bodies
// (mVoice @+0x34, mpObject @+0x40) and the four methods the pool + these bodies call
// (GetGain / Release / Update / ~VoiceWrapper) are materialised. Everything else in
// the 0x5C slot (the leading +0x04..+0x33 words the pool's Prepare zeros, any other
// surface) is DEFERRED -- the pool reaches those raw through PooledVoice, not here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// the embedded Voice and the object handle hold pointers that widen to 8 bytes on
// the 64-bit host, so the absolute byte offsets are recorded in comments only and
// are NOT static_asserted here.
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
    // (embedded Voice null-inits its handle; mpObject null-inits).
    VoiceWrapper() : mpObject(0) {}

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
    // separate slice (DEFERRED on the wrapper Release recon).
    void Release();

    // Per-frame wrapper tick, called by VoicePoolBase::Update on every slot.
    // Declared only; its body is a separate slice (DEFERRED).
    void Update();

private:
    // +0x34 (X360). The embedded sound-logic voice. Its mVoiceHandle.mpObject is read
    // as *(this+0x38) by GetGain and torn down by the dtor.
    Voice mVoice;

    // +0x40 (X360). A ref-counted playback object this wrapper owns. The dtor drops it
    // FIRST (assert its refcount > 0, --refcount, DoDispose at zero). Held by raw
    // pointer to mirror the X360 handle word (`lwz r31, 0x40(r30)`).
    Playback::Object* mpObject;
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H
