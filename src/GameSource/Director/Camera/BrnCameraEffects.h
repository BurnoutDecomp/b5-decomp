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

    // +0xA0 .. +0xAF: fade colour / overlay / race-end amount block. Construct zeroes
    //   +0xA0/+0xA4/+0xA8/+0xAC. NOMINAL span.
    u8  maReservedA0[0xB0 - 0xA0];

    // +0xB0: game-camera blend amount. Construct sets it to 1.0f.
    f32 mfGameCameraBlend;                      // +0xB0

    // +0xB4 .. +0xBB: time-of-day float (+0xB4, zeroed) and the trailing flag/enum bytes
    //   (+0xB7/+0xB8/+0xB9/+0xBA zeroed; +0xBB the final pad). NOMINAL span; pads the
    //   block to the X360-proven 0xBC stride.
    u8  maReservedB4[0xBC - 0xB4];
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_EFFECTS_H
