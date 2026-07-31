#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_EFFECTS_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_EFFECTS_H

#include "types.hpp"
#include "SharedClasses/Graphics/BrnEffectsData.h"          // BrnDirector::Camera::MotionBlurData
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT (the hook-name tripwires)
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"   // BrnDirector::HookNameStringWrapper

// ============================================================================
// GameSource/Director/Camera/BrnCameraEffects.h
//
// BrnDirector::Camera::CameraEffects -- the director camera's per-frame effects
// parameter block (motion blur, post-FX request, fade/overlay, bloom, time-of-day,
// shake, game-camera blend, ...). DWARF home BrnCameraEffects.h:32.
//
// This is the SHARED home for the type that BrnDirector::Camera::Camera embeds by
// value (mEffects). The class is reconstructed at the SIZE the X360 proves: the
// Camera::Construct asm @0x82255E68 inlines the effects-block constructor as a run
// of stores into the camera's +0x68..+0x122 span, and the next camera member
// (mDepthOfField) lands at +0x124 -- so CameraEffects is exactly 0x124-0x68 = 0xBC
// (188) bytes.
//
// FLAG (size): the file-local CameraEffects definition in BrnCameraEffects.cpp (a
// different TU) currently rounds to 0xD8; the X360 Camera layout proves 0xBC. The
// 0xBC here is the offset-authoritative one (it is what makes mDepthOfField land at
// the asm-attested +0x124 and the state flags word at +0x140). The fields below are
// pinned NAME-by-NAME only where the Camera::Construct asm writes them; the rest of
// the block stays an explicit reserved span (CameraEffects' own ledger TU fills it
// in). Construct() is DECLARATION-ONLY here -- the per-TU `cl /c` gate does not link;
// its body lands with the CameraEffects TU.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// DWARF: BrnCameraEffects.h:32 (struct BrnDirector::Camera::CameraEffects).
struct CameraEffects
{
    // Zero/identity-initialise the effects block. Inlined into Camera::Construct on
    // the X360 (no out-of-line call); de-inlined here to the single named call.
    // @ (inlined) -- body lands with the CameraEffects TU.
    void Construct();

    // Blend two effects blocks: byte-copies lLhs as the base (188 = 0xBC bytes, the
    // X360-proven CameraEffects stride) then overwrites the interpolated sub-fields.
    // @0x8220B050 -- body lands with the CameraEffects TU (BrnCameraEffects.cpp).
    static CameraEffects Interpolate(const CameraEffects& lLhs, const CameraEffects& lRhs, f32 lfT);

    // --- Layout (offsets relative to the CameraEffects sub-object @ camera +0x68) ---
    // Only the offsets the Camera::Construct asm pins by store are named; everything
    // else is a reserved span owned by the CameraEffects TU.

    // +0x00 / +0x21: the start/stop camera-PFX hook-name wrappers (both NUL'd by
    //   Construct). CARVED by the CameraEffects class TU (the five hook-name
    //   accessors @0x821F1910/0x821F1968/0x821F19C0/0x821F1A18/0x821F1A70 return
    //   this+0 / this+0x21 and read the has-flags at +0xB7/+0xB8).
    HookNameStringWrapper mStartHookNameString;   // +0x00 (33 bytes)
    HookNameStringWrapper mStopHookNameString;    // +0x21 (33 bytes -> ends +0x42)
    u8  maReserved42[0x44 - 0x42];                // +0x42 .. +0x43

    // +0x44: the embedded motion-blur parameter block (the Construct asm zeroes its two
    //   blur amounts at +0x44/+0x48 and its two bool flags at +0x4C/+0x4D). Shared type
    //   from BrnEffectsData.h.
    MotionBlurData mMotionBlurData;             // +0x44 (12 bytes -> ends +0x50)

    // +0x50 .. +0x77: background-effect request + leading post-FX scalars. Construct
    //   zeroes bytes/words at +0x50/+0x78. NOMINAL span.
    u8  maReserved50[0x78 - 0x50];

    u8  maReserved78[4];                          // +0x78 (Construct zeroes it)

    // +0x7C: the requested camera post-FX id (Construct zeroes it; MomentPlayerStunt
    //   @0x82272750 compares it against the 2dFlash id 575791 to publish state flag
    //   20 -- the "SetRequestedPostFX" word the arbitrator FLAG names).
    u32 muRequestedPostFxId;                      // +0x7C

    // +0x80: the start-hook blend amount (GetStartHookNameBlendAmount @0x821F1910
    //   `lfs f1, 0x80`, guarded by mbHasStartHookNameString -- the REAL blend member;
    //   the earlier +0x9C mis-name is what the mfSimTimeScale reconcile retired).
    f32 mfStartHookNameBlendAmount;               // +0x80

    // +0x84 .. +0x97: the stop-hook counterpart scalars + fade floats (Construct zeroes
    //   +0x90/+0x94). NOMINAL span.
    u8  maReserved84[0x98 - 0x84];

    // +0x98: the requested world clock (hours, 0..24). CARVED 2026-07-31 from the DWARF
    //   member order (BrnCameraEffects.h:311 puts mfTimeOfDay immediately before
    //   mfSimTimeScale @+0x9C) and X360-attested by ArbStateCarSelect::Update, which stores
    //   16.5 here for the junkyard / game-intro states and 12.5 for the outro, always paired
    //   with the mbSetTimeOfDay byte below (DWARF :329, the byte right after
    //   mbHasStopHookNameString @+0xB8).
    f32 mfTimeOfDay;                            // +0x98

    // +0x9C: the requested sim-time (timestep) scale. Construct sets it to 1.0f
    //   (normal speed). RECONCILED 2026-07 (was "mfStartHookBlendAmount", an
    //   uncited name): MomentHardStop::Update @0x82271788 stores its DWARF-named
    //   mfUltraSloMoTimestepScale here (the crash ultra-slomo, 0.005..0.01), and
    //   ArbStateTakedown writes 2/7 (~0.286 -- the takedown slow-mo) -- both are
    //   TIME scales, matching the un-homed "CameraEffects::SetSimTimeScale" the
    //   arbitrator FLAG lists. Interpolate's lerp fits a time-scale blend equally.
    f32 mfSimTimeScale;                         // +0x9C

    // +0xA0: the GAME-CAMERA BLEND amount. CARVED 2026-07-31 out of the previous nominal
    //   4-byte span, X360-attested by BrnDirector::KeyAnimController::UpdateCameraFromICE
    //   @0x8221E630, which stores `ICETake::GetValueFloat(CAMERA_BLEND_AMOUNT) * 0.01f` here
    //   (`stfs f0, 0x108(camera)` == effects +0xA0) -- the authored ICE element is literally
    //   named CAMERA_BLEND_AMOUNT (ICEData.cpp element [10], a 0..100 percentage), and it
    //   lands three words before the shake triple alongside mfCameraLag, whose own element is
    //   CAMERA_LAG_AMOUNT. This is the `mfGameCameraBlend` the FLAG on mfShakeFrequency below
    //   says the DWARF names "elsewhere in the PS3 0xD8-sized block": on the X360 0xBC layout
    //   it is here. Construct zeroes it. Same 4 bytes, same placement -- no size change.
    f32 mfGameCameraBlend;                      // +0xA0

    // +0xA4: the camera-lag (inertia) amount (DWARF member name mfCameraLag).
    //   X360-attested: BrnDirector::InertiaController::Update @0x8221ECD0 reads
    //   camera+0x10C (== effects +0xA4) as a float and slerps the camera toward its
    //   previous transform by (1 - this value). Construct zeroes it.
    f32 mfCameraLag;

    // +0xA8: race-end effect amount (DWARF member name mfRaceEndEffectAmount,
    //   BrnCameraEffects.cpp's DWARF member list). X360-attested: CameraEffects::
    //   Interpolate @0x8220B050 lerps this float the same way as mfSimTimeScale
    //   (fsubs/fmadds pair at +0xA8, alongside the +0x9C pair) -- a real interpolated
    //   field, not filler. Construct zeroes it.
    f32 mfRaceEndEffectAmount;                  // +0xA8

    // +0xAC / +0xB0 / +0xB4: the camera-shake request triple (DWARF names
    //   mfShakeAmplitude / mfShakeFrequency / mu8ShakeType, BrnCameraEffects.cpp's DWARF
    //   member list). X360-attested as a coherent triple:
    //   * PerlinShakeController::Update @0x8221E798 reads camera+0x114/+0x118
    //     (== effects +0xAC/+0xB0) as the shake AMPLITUDE (compared > 0) and FREQUENCY
    //     (multiplied into the Perlin-noise phase), via the DWARF-named
    //     Camera::GetEffects()/CameraEffects::GetShakeAmplitude() accessors;
    //   * Camera::SetImpactShake(amplitude, frequency, shakeType) writes +0xAC/+0xB0/+0xB4
    //     in argument order (see Camera.h);
    //   * Construct zeroes the amplitude and sets +0xB0 to 1.0f -- the default shake
    //     frequency. (+0xB0 was previously guessed as a "game-camera blend"; the DWARF
    //     order puts mfGameCameraBlend elsewhere in the PS3 0xD8-sized block, which the
    //     X360 0xBC layout does not carry at this offset.)
    f32 mfShakeAmplitude;                       // +0xAC (zeroed by Construct)
    f32 mfShakeFrequency;                       // +0xB0 (Construct sets 1.0f)
    u8  mu8ShakeType;                           // +0xB4

    // +0xB5 / +0xB6: the two blend-shape selectors that go with mfGameCameraBlend @+0xA0.
    //   CARVED 2026-07-31 out of the previous nominal 2-byte span, X360-attested by
    //   BrnDirector::KeyAnimController::UpdateCameraFromICE @0x8221E630: `stb` of
    //   ICETake::GetValueInt(BLEND_CURVE) at camera +0x11D (== effects +0xB5) and of
    //   GetValueInt(INTERPOLATE_TYPE) at camera +0x11E (== effects +0xB6). Those are
    //   ICEData.cpp elements [36] and [37], both on the take's BLEND channel (channel 1)
    //   together with element [10] CAMERA_BLEND_AMOUNT and [11] CAMERA_LAG_AMOUNT -- one
    //   coherent blend request. Zeroed by Construct. Same 2 bytes, same placement.
    u8  mu8BlendCurve;                          // +0xB5
    u8  mu8InterpolateType;                     // +0xB6

    // +0xB7 / +0xB8: the hook-name presence flags (member NAMES from the accessor
    //   asserts, BrnCameraEffects.h:175/:194).
    bool mbHasStartHookNameString;                // +0xB7
    bool mbHasStopHookNameString;                 // +0xB8

    // +0xB9: "the requested world clock above is live this frame" (DWARF :329
    //   mbSetTimeOfDay -- the byte immediately after mbHasStopHookNameString). Zeroed by
    //   Construct; raised (with mfTimeOfDay) by ArbStateCarSelect's junkyard / game-intro
    //   / outro arms. CARVED 2026-07-31 alongside mfTimeOfDay above.
    bool mbSetTimeOfDay;                          // +0xB9

    // +0xBA .. +0xBB: trailing bytes (+0xBA zeroed; +0xBB the final pad). NOMINAL span;
    //   pads the block to the X360-proven 0xBC stride.
    u8  maReservedBA[0xBC - 0xBA];

    // The shake-request read accessors (DWARF: CameraEffects::GetShakeAmplitude is named
    // by the PerlinShakeController::Update hint; GetShakeFrequency by symmetry).
    f32 GetShakeAmplitude() const { return mfShakeAmplitude; }
    f32 GetShakeFrequency() const { return mfShakeFrequency; }

    // ---- the hook-name accessor set (class:CameraEffects TU; all header-inline in
    // the original -- the h:175/:178/:194 class-body copies and the h:497/:524
    // out-of-class copies compile as SEPARATE X360 functions distinguished only by
    // their assert line, modelled here as the const/non-const pairs). All tripwires
    // non-gating (the X360 returns the member regardless). ----

    // @0x821F1910 (h:175) -- the start-hook blend amount.
    f32 GetStartHookNameBlendAmount() const
    {
        CGS_ASSERT(mbHasStartHookNameString, "mbHasStartHookNameString");   // :175 (non-gating)
        return mfStartHookNameBlendAmount;
    }

    // @0x821F1968 (h:178, const) / @0x821F1A18 (h:497, non-const).
    const HookNameStringWrapper& GetStartHookNameString() const
    {
        CGS_ASSERT(mbHasStartHookNameString, "mbHasStartHookNameString");   // :178 (non-gating)
        return mStartHookNameString;
    }
    HookNameStringWrapper& GetStartHookNameString()
    {
        CGS_ASSERT(mbHasStartHookNameString, "mbHasStartHookNameString");   // :497 (non-gating)
        return mStartHookNameString;
    }

    // @0x821F19C0 (h:194, const) / @0x821F1A70 (h:524, non-const).
    const HookNameStringWrapper& GetStopHookNameString() const
    {
        CGS_ASSERT(mbHasStopHookNameString, "mbHasStopHookNameString");     // :194 (non-gating)
        return mStopHookNameString;
    }
    HookNameStringWrapper& GetStopHookNameString()
    {
        CGS_ASSERT(mbHasStopHookNameString, "mbHasStopHookNameString");     // :524 (non-gating)
        return mStopHookNameString;
    }
    f32 GetCameraLag() const      { return mfCameraLag; }

    // ADDITIVE GROW (MomentPlayerJumping::Update @0x82275AA4..: the moment registers
    // its "Jump_Effect" start hook on the stack camera it is about to adopt with the
    // inlined triple store -- HookNameStringWrapper::Set on mStartHookNameString, the
    // +0xB7 gate byte, the +0x80 blend). De-inlined to a named CameraEffects operation
    // so callers never poke the fields by offset; the store ORDER matches the asm.
    void SetStartHookName(const char* lpcName, f32 lfBlendAmount)
    {
        mStartHookNameString.Set(lpcName);
        mbHasStartHookNameString   = true;
        mfStartHookNameBlendAmount = lfBlendAmount;
    }
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_EFFECTS_H
