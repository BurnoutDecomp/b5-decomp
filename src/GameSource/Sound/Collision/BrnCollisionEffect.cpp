#include "GameSource/Sound/Collision/BrnCollisionEffect.h"

// =============================================================================
// BrnSound::Logic::Collision::CollisionEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnCollisionEffect.h for the
// base-reuse rationale and the X360-32-bit-vs-host-64-bit offset note.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// CollisionEffect  @ 0x826AFD20  (field-init ctor)
//
//   stfs f0(=0.0), 0x20(this)            ; base mfRunningTime/mfDeltaTime region
//   stfs f0,       0x1C(this)            ; base field
//   stw  off_820AE954, 4(this)           ; (transient) IResourceRequester sub-vptr
//   sth  0, 0x10 ; stw 0, 0xC ; stw 0, 0x34 ; stw 0, 8 ; stb 0, 0x30 ; stw 0,0x28
//   stw  0, 0x24 ; sth 0, 0x12           ; base + leaf zero-init region
//   stw  off_820B1390, 0(this)           ; primary vptr (CollisionEffect vtable)
//   stw  off_820B135C, 4(this)           ; IResourceRequester sub-object vptr (final)
//   stw  0, 0x38 ; stw 0, 0x3C           ; mePrepareState / mbFirstUpdate+mbUseAzimuth
//   stw  &off_820B0E20, 0x44(this)       ; mCrashVoice vtable (Voice vptr @ +0x44)
//   stw  0, 0x48 ; stw 0, 0x4C           ; mCrashVoice handle/owner words
//   stfs f0(=0.0),  0x50(this)           ; mfIntensity = 0.0f
//   stw  5,         0x54(this)           ; meNicotineVolumeSlider = 5
//   stw  5,         0x58(this)           ; meNicotinePitchSlider  = 5
//   stfs f13(=1.0), 0x5C(this)           ; mSizeSettings.mfVolume = 1.0f
//   stfs f13,       0x60(this)           ; mSizeSettings.mfPitch  = 1.0f
//
// The leading vptr stores (+0/+4) and the base-region zero stores are the
// compiler-emitted base sub-object construction (BrnEffectObject's own ctor chain),
// produced implicitly here by the base default ctor. This body sets the LEAF
// members the X360 explicitly initialises. The +0x34 slot (mpCollisionDMixIo) is
// nulled by the X360 (stw 0,0x34) -- see header FLAG on the DWARF name divergence.
// ---------------------------------------------------------------------------
CollisionEffect::CollisionEffect()
    : mpCollisionDMixIo(nullptr)          // stw 0, 0x34
    , mePrepareState(E_PREPARE_STATE_CONSTRUCT_VOICE) // +0x38 zero-region
    , mbFirstUpdate(false)                // +0x3C zero-region
    , mbUseAzimuth(false)                 // +0x3C zero-region
    , mfIntensity(0.0f)                   // stfs 0.0, 0x50
    , meNicotineVolumeSlider(5)           // stw 5, 0x54
    , meNicotinePitchSlider(5)            // stw 5, 0x58
    // mCrashVoice: default-constructed (Voice vptr @ +0x44 + nulled handle/owner)
    // mSizeSettings: default-constructed (mfVolume/mfPitch = 1.0f -> +0x5C/+0x60)
{
}

// ---------------------------------------------------------------------------
// ~CollisionEffect  @ 0x826C90D8  (the X360 `vector deleting destructor')
//
//   bl   CollisionEffect::~CollisionEffect          ; chain to the real dtor
//   if (a2 & 1) {                                   ; the `delete' half
//       memset(&local[1], 0, 16); local[0] = this;
//       (*(*off_82FFB954 + 0x14))(off_82FFB954, local) ; free via MemBase allocator
//   }
//   return this
//
// The leaf destructor adds no teardown of its own: the X360 inner ~CollisionEffect
// is the inherited BrnEffectObject dual-base settle (both vptr stores + the
// attach/detach/resources-ready member clears), identical store-for-store to the
// committed BrnEffectObject dtor @ 0x826AF4C8 and the sibling SingleGinsuEffect
// dtor. In reconstructed C++ the dual-base settle and the deleting-destructor thunk
// are compiler-synthesised from the virtual dtor declared in the header, so the
// hand-written body is empty.
// FLAG: the (a2 & 1) tail frees the object through the global sound MemBase
// allocator (off_82FFB954); that allocator's vtable call is not homed here, so the
// `delete' half of the vector deleting destructor is left to the host toolchain
// rather than reproducing the raw allocator vtable dispatch.
// ---------------------------------------------------------------------------
CollisionEffect::~CollisionEffect()
{
}

// ---------------------------------------------------------------------------
// GetGain  @ 0x82688138
//
//   lwz  r3, 0x34(this)       ; r3 = mpCollisionDMixIo
//   lwz  r4, 0x54(this)       ; r4 = meNicotineVolumeSlider
//   cmplwi r3, 0
//   beq  zero                 ; no dmix handle -> gain contribution 0
//     li   r5, 0              ; preset = 0  (DMX_VOL)
//     bl   Nicotine::DMixIO::GetDMixOutput(r3, r4, 0)
//     extsw r11, r3           ; SIGN-extend the s32 result
//     fcfid / frsp            ; (s64)->f64->f32
//     fmuls f0, f12, flt_820AA8F8   ; * 0.000030518509  (== 1/32768, the Q15 scale)
//     b    tail
//   zero:
//     lfs  f0, flt_82001CC0   ; f0 = 0.0
//   tail:
//   lfs  f13, 0x5C(this)      ; f13 = mSizeSettings.mfVolume
//   fmuls f1, f13, f0         ; return mfVolume * scaledMixerOutput
//   blr
//
// Reads the latched dynamic-mixer output for the volume slider, converts the signed
// Q15 mixer value to a normalised f32, and scales it by the size-specific volume.
// When no dmix handle is connected the mixer contribution is 0.0f (so the gain is 0).
// FLAG: DWARF declares GetGain() const; the +0x34 slot is the latched
// Nicotine::DMixIO (see header FLAG on the DWARF mpCollisionControl name).
// ---------------------------------------------------------------------------
f32 CollisionEffect::GetGain() const
{
    // 0.000030518509f == flt_820AA8F8 (the Q15 -> [0,1) scale, 1/32768).
    const f32 KF_Q15_TO_NORMALISED = 0.000030518509f;

    f32 lfMixerOutput;
    if (mpCollisionDMixIo != nullptr) // lwz r3,0x34; cmplwi; beq
    {
        // GetDMixOutput(slot, preset): preset 0 == DMX_VOL. extsw -> signed s32.
        const s32 liOutput =
            mpCollisionDMixIo->GetDMixOutput(meNicotineVolumeSlider, Nicotine::DMixIO::DMX_VOL);
        lfMixerOutput = static_cast<f32>(liOutput) * KF_Q15_TO_NORMALISED;
    }
    else
    {
        lfMixerOutput = 0.0f; // lfs f0, flt_82001CC0
    }

    return mSizeSettings.mfVolume * lfMixerOutput; // fmuls f1, f13, f0
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
