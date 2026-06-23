#pragma once

// Home for BrnDirector::BoostShakeController -- the camera boost-shake intensity driver.
// DWARF home (nominal): GameSource/Director/Camera/BrnBoostShakeController.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x8220E548 (BoostShakeController::Update),
// semantic-parity (not byte-matching). The one ledger function maps the gameplay-external
// camera's boost state (an elapsed time over a duration) through a designer-tuned
// response curve into a shake amplitude that the camera effects path consumes.
//
// Update reads three pointers (X360 fastcall r4/r5/r6):
//   result (r4) -- the shake-output block. Writes the computed amplitude at +0x114 and an
//                  enable/style flag byte at +0x11C.
//   state  (r5) -- the gameplay-external camera state. Reads an elapsed value at +0x3CC
//                  and a duration at +0x3D4 (the boost ramp window).
//   params (r6) -- a wrapper holding the BoostShakeParams pointer at +0x04 (an asserted
//                  Reference<> handle on console); the params carry the response curve.
//
// The struct layouts of those three blocks are owned by their own TUs (the camera state
// is the >0x3D4-byte BehaviourGameplayExternal block; the params/output blocks belong to
// the camera-effects family). This home models ONLY the named fields the asm touches, at
// the proven offsets, as opaque-padded views so members are reached BY NAME -- never by
// raw-offset casts. offsetof static_asserts pin every touched field.

#include "types.hpp"
#include <cstddef>   // offsetof

namespace BrnDirector
{
    // The designer-tuned boost-shake response curve (the params block reached through the
    // wrapper's +0x04 handle). The asm reads four fields:
    //   mfStyleFlag      @+0x18  -- copied (as a byte) to the output's flag at +0x11C
    //   mfRampStart      @+0x1C  -- low edge of the remap window
    //   mfAmplitude      @+0x20  -- output scale (multiplies the remapped, clamped weight)
    //   mfRampEnd        @+0x24  -- high edge of the remap window
    struct BoostShakeParams
    {
        unsigned char maReserved0[0x18];  // +0x00 : preceding curve fields (own TU)
        s32  miStyleFlag;                 // +0x18 : style/enable flag (read as a byte)
        f32  mfRampStart;                 // +0x1C
        f32  mfAmplitude;                 // +0x20
        f32  mfRampEnd;                   // +0x24
    };

    // The wrapper the camera passes as `params` -- holds the BoostShakeParams pointer at
    // X360 +0x04 (an IsAllocated()-asserted handle on console; here just the held pointer).
    // mpParams is a real pointer so the deref is by-name; its x64 offset (8-byte pointer
    // alignment) differs from the X360 +0x04 and is not pinned.
    struct BoostShakeParamsRef
    {
        unsigned char            maReserved0[0x04];  // +0x00 (X360) : handle book-keeping (own TU)
        const BoostShakeParams*  mpParams;           // X360 +0x04
    };

    // The gameplay-external camera state block (subset). Only the boost ramp clock the asm
    // reads is modelled here; the full block is owned by BehaviourGameplayExternal's TU.
    struct BoostShakeCameraState
    {
        unsigned char maReserved0[0x3CC];  // +0x000 : camera state (own TU)
        f32  mfBoostElapsed;               // +0x3CC : elapsed boost time
        unsigned char maReserved1[0x04];   // +0x3D0 : (intervening field, own TU)
        f32  mfBoostDuration;              // +0x3D4 : boost ramp window length
    };

    // The shake-output block (subset). Update writes the amplitude + the flag byte.
    struct BoostShakeOutput
    {
        unsigned char maReserved0[0x114];  // +0x000 : output state (own TU)
        f32  mfShakeAmplitude;             // +0x114
        unsigned char maReserved1[0x04];   // +0x118 : (gap to the flag byte)
        unsigned char mbShakeFlag;         // +0x11C : enable/style flag (byte store)
    };

    // BrnDirector::BoostShakeController -- a stateless driver (all inputs by pointer).
    struct BoostShakeController
    {
        // @0x8220E548. Map the camera's boost ramp (elapsed/duration, clamped to [0,1])
        // through the params curve (remap into [rampStart,rampEnd], clamp to [0,1], scale
        // by amplitude) into the output's shake amplitude; copy the style flag byte. When
        // the duration is exactly zero the output amplitude is forced to zero.
        BoostShakeOutput* Update(BoostShakeOutput* lpOutput,
                                 const BoostShakeCameraState* lpState,
                                 const BoostShakeParamsRef* lpParamsRef) const;
    };

    // Pin every offset the X360 asm proves.
    static_assert(offsetof(BoostShakeParams, miStyleFlag) == 0x18, "BoostShakeParams flag @+0x18");
    static_assert(offsetof(BoostShakeParams, mfRampStart) == 0x1C, "BoostShakeParams rampStart @+0x1C");
    static_assert(offsetof(BoostShakeParams, mfAmplitude) == 0x20, "BoostShakeParams amplitude @+0x20");
    static_assert(offsetof(BoostShakeParams, mfRampEnd)   == 0x24, "BoostShakeParams rampEnd @+0x24");
    // (BoostShakeParamsRef::mpParams sits at X360 +0x04; not pinned on x64 -- 8-byte pointer.)
    static_assert(offsetof(BoostShakeCameraState, mfBoostElapsed)  == 0x3CC, "boost elapsed @+0x3CC");
    static_assert(offsetof(BoostShakeCameraState, mfBoostDuration) == 0x3D4, "boost duration @+0x3D4");
    static_assert(offsetof(BoostShakeOutput, mfShakeAmplitude) == 0x114, "shake amplitude @+0x114");
    static_assert(offsetof(BoostShakeOutput, mbShakeFlag)      == 0x11C, "shake flag byte @+0x11C");
}
