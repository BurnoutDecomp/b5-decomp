#include "GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.h"

// =============================================================================
// BrnSound::Vehicles::Engines::HybridEngineControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: HybridEngineControl : public
// HybridExhaustControl. Recon'd function set:
//   UpdateGinsuRPM                 @ 0x82699B98
//   CreateObject(u32)              @ 0x826CC738
//   `scalar deleting destructor'   @ 0x826CC7F0  (-> ~HybridEngineControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// HybridEngineControl::UpdateGinsuRPM  @ 0x82699B98  (single-step Ginsu RPM shift)
//   lwz  r11, 0x130(this)   ; mpEngineControl (the sampled control)
//   lfs  f13, 0x8C(this)    ; mGinsuRpm.mCurrentValue (old current)
//   lfs  f0,  0x8C(r11)     ; mpEngineControl->mGinsuRpm.mCurrentValue (new sample)
//   stfs f13, 0x90(this)    ; mGinsuRpm.mPreviousValue = old current
//   stfs f0,  0x8C(this)    ; mGinsuRpm.mCurrentValue  = new sample
//
// mGinsuRpm is the base HybridExhaustControl's committed DataPoint<f32> (mCurrentValue
// @ +0x8C, mPreviousValue @ +0x90). The sampled control (mpEngineControl, base member
// @ +0x130) is an opaque void*; its own mGinsuRpm.mCurrentValue lives at the same +0x8C
// offset -- read via a FLAGGED raw byte view (rule 4: un-homed base sub-state) rather
// than a fabricated typed EngineControl member.
// ---------------------------------------------------------------------------
void HybridEngineControl::UpdateGinsuRPM()
{
    // mpEngineControl->mGinsuRpm.mCurrentValue @ +0x8C (opaque control; byte-viewed).
    const f32 lfNewSample =
        *reinterpret_cast<const f32*>(static_cast<const u8*>(mpEngineControl) + 0x8C);

    mGinsuRpm.mPreviousValue = mGinsuRpm.mCurrentValue; // previous = old current
    mGinsuRpm.mCurrentValue  = lfNewSample;             // current  = new sample
}

// ---------------------------------------------------------------------------
// HybridEngineControl::CreateObject(u32)  @ 0x826CC738   (the RTTI factory hook)
//
// The X360 allocates a 320-byte (0x140) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "HybridEngineControl" and inline-constructs a
// HybridEngineControl, returning it upcast to CgsSound::Logic::EffectControl*. `a1`
// only selects the operator-new flavour (0/1); both arms use the same size + ctor.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the observable result matches. Mirrors
// the committed CollisionControl::CreateObject. The 0x140 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* HybridEngineControl::CreateObject( u32 /*luType*/ )
{
    return new HybridEngineControl();
}

// ---------------------------------------------------------------------------
// ~HybridEngineControl  @ 0x826CC7F0  (anchor for the X360 `scalar deleting destructor').
// The X360 chains HybridExhaustControl::~HybridExhaustControl; HybridEngineControl adds
// no member teardown of its own, so this leaf body is empty. The (a2 & 1) allocator-free
// tail is re-synthesised by the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
HybridEngineControl::~HybridEngineControl()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
