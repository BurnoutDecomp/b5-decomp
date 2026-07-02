#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_EFFECTS_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_EFFECTS_H

#include "types.hpp"
#include "SharedClasses/Graphics/BrnEffectsData.h"   // BrnDirector::Camera::MotionBlurData

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

    // +0x00 .. +0x43: start/stop hook-name string wrappers (mStartHookNameStringWrapper
    //   head byte at +0x00, mStopHookNameStringWrapper head byte at +0x21 -- both NUL'd
    //   by Construct). NOMINAL span.
    u8  maReserved00[0x44];

    // +0x44: the embedded motion-blur parameter block (the Construct asm zeroes its two
    //   blur amounts at +0x44/+0x48 and its two bool flags at +0x4C/+0x4D). Shared type
    //   from BrnEffectsData.h.
    MotionBlurData mMotionBlurData;             // +0x44 (12 bytes -> ends +0x50)

    // +0x50 .. +0x9B: background-effect request + leading post-FX/fade scalars. Construct
    //   zeroes bytes/words at +0x50/+0x78/+0x7C and floats at +0x90/+0x94. NOMINAL span.
    u8  maReserved50[0x9C - 0x50];

    // +0x9C: start-hook blend amount. Construct sets it to 1.0f.
    f32 mfStartHookBlendAmount;                 // +0x9C

    // +0xA0: fade colour / overlay lead word (Construct zeroes it). NOMINAL span.
    u8  maReservedA0[0xA4 - 0xA0];

    // +0xA4: the camera-lag (inertia) amount (DWARF member name mfCameraLag).
    //   X360-attested: BrnDirector::InertiaController::Update @0x8221ECD0 reads
    //   camera+0x10C (== effects +0xA4) as a float and slerps the camera toward its
    //   previous transform by (1 - this value). Construct zeroes it.
    f32 mfCameraLag;

    // +0xA8: race-end effect amount (DWARF member name mfRaceEndEffectAmount,
    //   BrnCameraEffects.cpp's DWARF member list). X360-attested: CameraEffects::
    //   Interpolate @0x8220B050 lerps this float the same way as mfStartHookBlendAmount
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

    // +0xB5 .. +0xBB: trailing flag/enum bytes (+0xB7/+0xB8/+0xB9/+0xBA zeroed; +0xBB the
    //   final pad). NOMINAL span; pads the block to the X360-proven 0xBC stride.
    u8  maReservedB5[0xBC - 0xB5];

    // The shake-request read accessors (DWARF: CameraEffects::GetShakeAmplitude is named
    // by the PerlinShakeController::Update hint; GetShakeFrequency by symmetry).
    f32 GetShakeAmplitude() const { return mfShakeAmplitude; }
    f32 GetShakeFrequency() const { return mfShakeFrequency; }
    f32 GetCameraLag() const      { return mfCameraLag; }
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_EFFECTS_H
