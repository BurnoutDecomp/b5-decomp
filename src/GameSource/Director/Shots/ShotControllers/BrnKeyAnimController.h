#ifndef GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_CONTROLLER_H
#define GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_CONTROLLER_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                        // rw::math::vpu::Vector3 (the look position)
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT (the mbPrepared guards)

// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.h
//
// BrnDirector::KeyAnimController -- the keyframed-animation shot controller: it drives a camera
// shot off a keyframed animation clip (eye space, look space, look-at position, a running time),
// sampled each frame by BehaviourIceAnim::Update. HOME for the four KeyAnimController slices this
// TU bodies:
//   - GetEyeSpace  @0x821F4138  (return the eye-space pointer @+0x758)
//   - GetLookSpace @0x821F4190  (return the look-space pointer @+0x75C)
//   - GetLookPos   @0x821F41E8  (return, by value, the 16-byte look position @+0x10)
//   - HasFinished  @0x821F4258  (clip done? forward: time >= duration; reverse: time <= 0)
// All four assert the controller has been prepared (!!mbPrepared @+0x765) before sampling. The
// full controller (Prepare/Update and the rest of the rig) lands with its own TU; this header
// models only the members these accessors touch, BY NAME, at their asm-attested offsets.
// ----------------------------------------------------------------------------

namespace BrnDirector
{

// FLAG: opaque space/transform handle. GetEyeSpace/GetLookSpace each return a single 32-bit word
//   (lwz) that the caller treats as a space object; its concrete type lands with the controller's
//   own TU. Modelled as a forward-declared opaque type so the accessors return a typed pointer.
class Space;

// FLAG: the keyframed animation clip the controller plays. Only the member HasFinished reads is
//   modelled -- the clip duration at +0x2C (lfs 0x2C(r11)). Its full type lands with the anim TU.
class KeyAnimClip
{
public:
    u8  maReserved00[0x2C];   // +0x00 .. +0x2B (clip header not modelled here)
    f32 mfDuration;           // +0x2C  clip duration in seconds (HasFinished comparand)
};

class KeyAnimController
{
public:
    // Return the eye-space pointer (the camera's eye transform space). Asserts prepared.
    // @0x821F4138: lwz r3, 0x758(this).
    Space* GetEyeSpace() const;

    // Return the look-space pointer (the look-at transform space). Asserts prepared.
    // @0x821F4190: lwz r3, 0x75C(this).
    Space* GetLookSpace() const;

    // Return (by value) the 16-byte look-at position. Asserts prepared. @0x821F41E8: a single
    // 16-byte aligned vector copied from +0x10 (lvx128) into the sret (stvx128); sret in r3,
    // this in r4.
    rw::math::vpu::Vector3 GetLookPos() const;

    // True iff the keyframed clip has finished. Asserts prepared. @0x821F4258:
    //   reverse (mbReverse @+0x767 != 0): finished when mfTime <= 0.0
    //   forward (mbReverse == 0):         finished when mfTime >= clip duration
    //                                     (duration = mpClip ? mpClip->mfDuration : 0.0)
    bool HasFinished() const;

    // Seek the controller's normalised playback parameter to lf01 (0..1) of the clip. The
    // rank-up arbitrator state rewinds a freshly-swapped take to its start with 0.0 each time
    // it advances to the next rival's take (BrnArbStateRankUp::Update @0x82236380). REAL X360
    // function (BrnKeyAnimController.cpp); DECLARATION-ONLY here (the per-TU cl /c gate does
    // not link, and the controller's full rig lands with its own TU).
    void SetParametricTime0To1(f32 lf01);

    // FLAG: only the members these accessors touch are modelled at their asm-attested offsets. The
    //   pinned region uses SIZE-STABLE fields only (the X360 is a 4-byte-pointer build; this PC
    //   reconstruction is 64-bit, so a real 8-byte pointer mid-struct would shift every later
    //   offset -- and the eye/look space pointers sit only 4 bytes apart at +0x758/+0x75C, which a
    //   pair of 8-byte pointers could not occupy). The console's 4-byte pointer slots are therefore
    //   reserved here; the type-correct pointer values live in the tail shadows below, reached by
    //   name through the accessors. Members are public so the file-scope offsetof pins in the .cpp
    //   can verify the (exact) layout. The rest of the controller rig lands with its own TU.
    u8                     maReserved00[0x10];          // +0x000 .. +0x00F  controller head (not modelled)
    rw::math::vpu::Vector3 mLookPos;                    // +0x010  look-at position (GetLookPos source)
    u8                     maReserved020[0x24 - 0x20];  // +0x020 .. +0x023 (rig members not modelled here)
    u32                    muClipSlot;                  // +0x024  clip pointer (X360 4B ptr slot)
    u8                     maReserved028[0x758 - 0x28]; // +0x028 .. +0x757 (rig members not modelled here)
    u32                    muEyeSpaceSlot;              // +0x758  eye-space pointer (X360 4B ptr slot)
    u32                    muLookSpaceSlot;             // +0x75C  look-space pointer (X360 4B ptr slot)
    f32                    mfTime;                      // +0x760  running clip time (HasFinished)
    u8                     maReserved764[0x765 - 0x764];// +0x764 (rig member not modelled here)
    u8                     mbPrepared;                  // +0x765  set once the controller is prepared
    u8                     maReserved766[0x767 - 0x766];// +0x766 (rig member not modelled here)
    u8                     mbReverse;                   // +0x767  play the clip backwards

    // x64 type-correct shadows of the three 4-byte pointer slots above. Appended at the tail so
    // they never disturb the pinned offsets; the X360 packs each pointer into its 4-byte slot. The
    // accessors read/deref these by name (the eye/look-space getters return a typed pointer;
    // HasFinished dereferences the clip's duration).
    Space*             mpEyeSpace;
    Space*             mpLookSpace;
    const KeyAnimClip* mpClip;
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_CONTROLLER_H
