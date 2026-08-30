#include "GameSource/Sound/Global/BrnHUDEffect.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Dot + operator-(Vec,Vec) + operator*(Vec,float)
#include <cmath>                              // std::sqrt

// =============================================================================
// BrnSound::Logic::HUDEffect::GameModeData -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnHUDEffect.h for the layout
// rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries:
//   GameModeData::GameModeData (Construct)  @ 0x826AFE88
//   GameModeData::Reset                     @ 0x826977D8
//
// dep_flags: none un-homed for THIS TU. Every member written is modelled BY NAME
// in the owning header (the Average<10,f32> / DataPoint<T> generics live in the
// committed CgsSoundUtils home).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

CgsSound::Logic::EffectObject* HUDEffect::CreateObject(u32)
{
    return new HUDEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* HUDEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x10, "HUDEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &HUDEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* HUDEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* HUDEffect::GetTypeName() const
{
    return "HUDEffect";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpHUDEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(HUDEffect::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// HUDEffect::GameModeData::GameModeData  @ 0x826AFE88  (the DWARF Construct)
//
//   for ( i = 0; i < 10; ++i ) *(4*i + this) = 0.0;   ; mEventScoreDelta.maPoints
//   *(this+44) = 0.0;  *(this+40) = 0;                 ; mfAverage / muNextPoint
//   *(this+48..60) = 0.0;                              ; time-remaining / boost DPs
//   *(this+68..104) = 0;                               ; stunt + showtime DPs
//
// The X360 ctor zeroes every field it touches (it does NOT write +64
// mfTimeSinceScoreTick nor +108 mbTimeExtended -- those are left as Reset's job).
// Reconstructed as explicit member zeroing in DWARF order.
// ---------------------------------------------------------------------------
HUDEffect::GameModeData::GameModeData()
{
    // mEventScoreDelta: zero the 10 sample points, the next-point index and the
    // cached average (the i<10 loop + the +40/+44 stores).
    for (u32 lu = 0; lu < 10u; ++lu)
    {
        mEventScoreDelta.maPoints[lu] = 0.0f;
    }
    mEventScoreDelta.muNextPoint = 0;
    mEventScoreDelta.mfAverage   = 0.0f;

    mEventTimeRemaining.Flush(0.0f);   // +48,+52
    mEventBoostAmount.Flush(0.0f);     // +56,+60

    mStuntScore.Flush(0);              // +68,+72
    mStuntComboMultiplier.Flush(0);    // +76,+80
    mStuntResultScore.Flush(0);        // +84,+88

    mShowtimeScore.Flush(0.0f);        // +92,+96
    mShowtimeBoostDelta.Flush(0.0f);   // +100,+104
}

// ---------------------------------------------------------------------------
// HUDEffect::GameModeData::Reset  @ 0x826977D8
//
//   *(this+44) = 0.0;                       ; mEventScoreDelta.mfAverage
//   do { *(4*v1 + this) = 0.0; } while (v1 < 10);  ; maPoints
//   *(this+40) = 0;                          ; muNextPoint
//   *(this+48..60) = 0.0;                    ; time-remaining / boost DPs
//   *(this+64) = 0.0;                        ; mfTimeSinceScoreTick
//   *(this+68) = 0; *(this+72) = 0;          ; mStuntScore
//   *(this+76) = 1; *(this+80) = 1;          ; mStuntComboMultiplier = 1 (cur+prev)
//   *(this+84) = 0; *(this+88) = 0;          ; mStuntResultScore
//   *(this+92..104) = 0.0;                   ; showtime DPs
//   *(this+108) = 1;                         ; mbTimeExtended = true
//
// Reset differs from Construct: it ALSO clears mfTimeSinceScoreTick (+64), defaults
// the stunt combo multiplier to 1 (both current and previous words), and sets
// mbTimeExtended true.
// ---------------------------------------------------------------------------
void HUDEffect::GameModeData::Reset()
{
    mEventScoreDelta.mfAverage = 0.0f;
    for (u32 lu = 0; lu < 10u; ++lu)
    {
        mEventScoreDelta.maPoints[lu] = 0.0f;
    }
    mEventScoreDelta.muNextPoint = 0;

    mEventTimeRemaining.Flush(0.0f);   // +48,+52
    mEventBoostAmount.Flush(0.0f);     // +56,+60

    mfTimeSinceScoreTick = 0.0f;       // +64

    mStuntScore.Flush(0);              // +68,+72
    mStuntComboMultiplier.Flush(1);    // +76,+80  (combo defaults to 1)
    mStuntResultScore.Flush(0);        // +84,+88

    mShowtimeScore.Flush(0.0f);        // +92,+96
    mShowtimeBoostDelta.Flush(0.0f);   // +100,+104

    mbTimeExtended = true;             // +108
}

// =============================================================================
// BrnSound::Logic::HUDEffect -- the enclosing effect leaf (grown surface).
// =============================================================================

// Runtime-initialised module globals mirroring the X360 .data slots
// HUDEffect::GetSlipstreamAmount reads (the shared 0.0/1.0 singletons are in the
// 0x8200_xxxx rodata range; these two live in the 0x8300_xxxx MUTABLE data segment,
// so they are runtime-initialised FILE-SCOPE GLOBALS, not function-local constants):
//   * flt_830060C0 is written by sub_82C62DB8 as XMVectorCos(0.346f) == cos(0.346 rad)
//     == 0.9407368f (a ~19.8deg slipstream-cone half-angle cosine gate). VALUE RECOVERED
//     from the writer.
//   * flt_830083E4 has a single read-only xref (this function) and no exported writer --
//     its runtime value is UNRECOVERABLE. Modelled as a flagged global initialised to
//     1.0f (identity multiplier) so the shape compiles; confidence LOW on this one number.
static f32 gfSlipstreamConeCos = 0.9407368f;  // flt_830060C0 == cos(0.346f)
static f32 gfSlipstreamScale   = 1.0f;        // flt_830083E4 -- UNRESOLVED runtime value (confidence=low)

// ---------------------------------------------------------------------------
// HUDEffect::HUDEffect (Construct)  @ 0x826E1940  (MINIMAL -- see FLAGs)
//
// MSVC's INLINED full-object constructor: it does NOT `bl` a base ctor; it installs the
// BrnEffectObject dual leaf-vptr pair (this+0 primary off_820B588C, this+4
// IResourceRequester off_820B5858) and zero-inits the base members directly, then
// tail-constructs the embedded sub-objects (maVoices[3], maGameModeVoices[4] each
// running its mVoice VoiceWrapper ctor, mMusicStream @ +0x3A0, mGameModeData @ +0x430,
// mHudMessageData @ +0x4A0). Here the base vptr install + base zero-init are produced BY
// NAME by BrnEffectObject(), and mGameModeData is default-constructed as an ordinary
// member (matching the X360 tail `bl GameModeData::GameModeData`).
//
// FLAG (un-homed sub-object types -> DEFERRED): the CustomHudVoice array (maGameModeVoices),
// the BrnSound::Logic::MusicStream object (mMusicStream @ +0x3A0), the
// Attrib::Gen::presentationcomponent (mHudMessageData @ +0x4A0), the maVoices[3]
// VoiceWrapper array, and the trailing scalar fixups (volumes=1.0, the 96000.0f
// sample-rate, the 10-float average @ +0x400, mLastPlayedEvent/CgsID/round-robin @
// +0x4C8..) have no committed homes and are DEFERRED here -- not fabricated (matching the
// committed StreamingEffect `Attrib::Gen::streamsettings` treatment).
// ---------------------------------------------------------------------------
HUDEffect::HUDEffect()
    : BrnEffectObject()   // dual vptr install (this+0 / this+4) + base member zero-init, BY NAME
    // mGameModeData default-constructs (GameModeData::GameModeData, already committed).
    // maVoices[3], maGameModeVoices[4], mMusicStream and mHudMessageData are UN-HOMED and
    // DEFERRED (see FLAG) -- not fabricated as named members / calls in this minimal home.
{
}

// ---------------------------------------------------------------------------
// ~HUDEffect  (the out-of-line anchor the vector deleting destructor @ 0x826E1E00 and
// its adjustor{4} @ 0x826E1BC0 forward to). Both are MSVC compiler-synthesised thunks
// over this virtual destructor: the vector deleting destructor calls this leaf dtor then
// conditionally frees via the global sound MemBase allocator (off_82FFB954, slot +0x14),
// and the adjustor{4} recovers the primary `this` from the +4 IResourceRequester
// sub-object pointer. The observable member teardown lives in the inherited BrnEffectObject
// base chain (this slice defers the un-homed leaf members), so this anchor body is empty;
// the host toolchain re-synthesises both thunks from this class's dual base + virtual
// dtor. No fabricated allocator is added.
// ---------------------------------------------------------------------------
HUDEffect::~HUDEffect()
{
}

// ---------------------------------------------------------------------------
// HUDEffect::GetSlipstreamAmount  @ 0x82686BA8
//
// Slipstream-audio proximity/alignment weight for the car in front. `this` (r3) is
// unused; the three Vector3 args arrive in vector registers v1/v2/v3:
//   v1 = car-in-front velocity, v2 = player's weighted velocity,
//   v3 = negated look direction (caller vxor128 sign-flip).
//
// ASM (0x82686BA8-0x82686C5C), store-for-store:
//   relVel  = v1 - v2                                     ; vsubfp
//   distSq  = Dot3(relVel, relVel)                         ; vmsum3fp128
//   projRaw = Dot3(v3, relVel)                             ; vmsum3fp128
//   invDist = 1/sqrt(distSq)                               ; vrsqrtefp + 1 NR step
//   t1 = 1.0 - (projRaw - 25.0) * (1/15), saturate01       ; two fsel
//   unitRelVel = relVel * invDist                          ; normalize
//   projNorm   = Dot3(unitRelVel, v3)                      ; vmsum3fp128
//   t2 = saturate01(projNorm - gfSlipstreamConeCos)        ; two fsel
//   return (t2 * gfSlipstreamScale) * t1
//
// FLAG (VMX->portable): vrsqrtefp + 1 NR refine collapsed to exact 1.0f/std::sqrt
//   (project convention, matches the BrnMathUtils Magnitude family).
// FLAG (fsel-faithful clamps): both saturate pairs reproduce the PPC
//   `fsel(a,b,c) == (a >= 0.0f) ? b : c` sense exactly (NOT std::clamp).
// FLAG (globals, NOT rodata literals): gfSlipstreamConeCos (flt_830060C0 == cos(0.346))
//   and gfSlipstreamScale (flt_830083E4, UNRESOLVED, modelled as 1.0f) are runtime-
//   initialised file-scope globals -- see the definitions above.
// ---------------------------------------------------------------------------
f64 HUDEffect::GetSlipstreamAmount( Vector3 lCarInFrontVelocity,
                                    Vector3 lOwnWeightedVelocity,
                                    Vector3 lNegatedDirection )
{
    const Vector3 lRelativeVelocity = lCarInFrontVelocity - lOwnWeightedVelocity;   // v0 = v1 - v2

    const f32 lfDistSq  = rw::math::vpu::Dot(lRelativeVelocity, lRelativeVelocity); // vmsum3fp128 v13,v0,v0
    const f32 lfProjRaw = rw::math::vpu::Dot(lNegatedDirection, lRelativeVelocity); // vmsum3fp128 v10,v3,v0

    // vrsqrtefp + 1 Newton-Raphson refine -> exact reciprocal-sqrt (see FLAG).
    const f32 lfInvDist = 1.0f / std::sqrt(lfDistSq);

    // t1 = 1.0 - (projRaw - 25.0) * (1/15), then saturate to [0,1] (two fsel).
    f32 lfSpeedFactor = 1.0f - (lfProjRaw - 25.0f) * 0.06666667f;
    lfSpeedFactor = ((-lfSpeedFactor) >= 0.0f) ? 0.0f : lfSpeedFactor;                 // fsel: lower clamp -> 0
    lfSpeedFactor = ((1.0f - lfSpeedFactor) >= 0.0f) ? lfSpeedFactor : 1.0f;           // fsel: upper clamp -> 1

    const Vector3 lUnitRelativeVelocity = lRelativeVelocity * lfInvDist;              // vmulfp128 v0,v0,v13
    const f32 lfProjNorm = rw::math::vpu::Dot(lUnitRelativeVelocity, lNegatedDirection); // vmsum3fp128 v0,v0,v3

    // t2 = saturate01(projNorm - coneCos) (two fsel).
    f32 lfDistanceFactor = lfProjNorm - gfSlipstreamConeCos;
    lfDistanceFactor = ((-lfDistanceFactor) >= 0.0f) ? 0.0f : lfDistanceFactor;        // fsel: lower clamp -> 0
    lfDistanceFactor = ((1.0f - lfDistanceFactor) >= 0.0f) ? lfDistanceFactor : 1.0f;  // fsel: upper clamp -> 1

    return static_cast<f64>((lfDistanceFactor * gfSlipstreamScale) * lfSpeedFactor);
}

} // namespace Logic
} // namespace BrnSound
