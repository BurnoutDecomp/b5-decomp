#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_DEPTH_OF_FIELD_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_DEPTH_OF_FIELD_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParams range tripwires)

// ============================================================================
// GameSource/Director/Camera/BrnDepthOfField.h
//
// BrnDirector::Camera::DepthOfField -- the camera depth-of-field parameter block: the focus
// distance band (start / perfect-start / perfect-end / end, all in metres) plus a blurriness
// scalar. HOME for the class slice owned by this TU:
//   - DepthOfField::SetParams @0x821F1AC8  (validate + store the five DOF parameters)
//
// The key-anim / ICE camera movers call SetParams each frame to drive the focus band.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

class DepthOfField
{
public:
    // Set the depth-of-field parameters. @0x821F1AC8. Validates the band is ordered and the
    // blurriness is in [0,1] (asserts only), then stores all five f32 values store-for-store.
    //
    // Argument -> register -> field mapping (from the asm fmr/stfs sequence):
    //   lfFocusStartDistanceMeters         (f1 / fp29) -> mfFocusStartDistanceMeters        @+0x00
    //   lfPerfectFocusStartDistanceMeters  (f2 / fp28) -> mfPerfectFocusStartDistanceMeters @+0x04
    //   lfPerfectFocusEndDistanceMeters    (f3 / fp27) -> mfPerfectFocusEndDistanceMeters   @+0x08
    //   lfFocusEndDistanceMeters           (f4 / fp26) -> mfFocusEndDistanceMeters          @+0x0C
    //   lfBlurriness                       (f5 / fp30) -> mfBlurriness                       @+0x10
    void SetParams(f32 lfFocusStartDistanceMeters,
                   f32 lfPerfectFocusStartDistanceMeters,
                   f32 lfPerfectFocusEndDistanceMeters,
                   f32 lfFocusEndDistanceMeters,
                   f32 lfBlurriness);

private:
    // Layout pinned store-for-store from the asm (stfs displacements off r30):
    f32 mfFocusStartDistanceMeters;        // +0x00  stfs f29, 0(r30)
    f32 mfPerfectFocusStartDistanceMeters; // +0x04  stfs f28, 4(r30)
    f32 mfPerfectFocusEndDistanceMeters;   // +0x08  stfs f27, 8(r30)
    f32 mfFocusEndDistanceMeters;          // +0x0C  stfs f26, 0xC(r30)
    f32 mfBlurriness;                      // +0x10  stfs f30, 0x10(r30)
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_DEPTH_OF_FIELD_H
