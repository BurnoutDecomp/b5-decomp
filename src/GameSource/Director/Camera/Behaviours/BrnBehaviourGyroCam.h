#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h
//
// BrnDirector::Camera::BehaviourGyroCam -- the "gyro" follow-camera behaviour: it holds the
// camera a fixed height/distance/pitch off a tracked car along a world-space vector from the
// car, smoothing the rig with a PositionLag and a CameraShake and optionally an attachment
// "truck" that eases the offset distance in. HOME for the BehaviourGyroCam class slice this
// TU bodies (the SetParameters inline). The full behaviour (Construct/Prepare/Update and the
// rest of the rig) and its Behaviour base land with their own TUs; this header models only
// the slice SetParameters needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is SetParameters @0x821F4068: it asserts the supplied
// parameter block is a gyro-cam block (its type tag == eBehaviourGyroCam == 9) and stores
// the pointer in mpParameters. Everything else below is the minimal named scaffolding that
// inline needs to compile.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id
//   in the leading word of their Parameters block; SetParameters asserts the block's id is
//   the gyro-cam one. The console value for eBehaviourGyroCam is 9 (the asm compares the
//   block's first word against 9). Replace with the real EBehaviourType enum when the
//   Behaviour base TU lands; the gyro-cam enumerator's VALUE (9) is pinned from the asm.
enum EBehaviourType
{
    eBehaviourGyroCam = 9
};

// FLAG: minimal slice of the camera-behaviour base. The full Behaviour base (vtable + shared
//   flag/state block) lands with its own TU; SetParameters only needs the typed parameter
//   pointer member to exist on the derived class, so the base is modelled as an opaque head
//   here. The real base layout/method set replaces this when the Behaviour TU lands.
class Behaviour
{
public:
    // The base parameter block: a type tag in its leading word plus behaviour-specific data.
    // GetType returns the tag the derived SetParameters asserts on.
    class Parameters
    {
    public:
        EBehaviourType GetType() const { return static_cast<EBehaviourType>(meType); }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
    };

protected:
    void* mpVTable;        // +0x00  behaviour vtable (opaque base head)
};

class BehaviourGyroCam : public Behaviour
{
public:

    // The gyro-cam parameter block (a Behaviour::Parameters with the gyro-cam type tag).
    class Parameters : public Behaviour::Parameters {};

    // Adopt a gyro-cam parameter block: assert it carries the gyro-cam type tag, then store
    // it. @0x821F4068.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the two members SetParameters writes are modelled; the rest of the gyro-cam
    //   rig (transform, looker, position lag, shake, attachment truck, the height/distance/
    //   pitch tunables) lands with the full behaviour TU. Reserved padding places the two
    //   written members at their asm-attested offsets (+0x10 / +0x14).
    u8                maReserved04[0x10 - 0x04];  // +0x04 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                 // +0x10  cached lpParameters->miParamWord1
    const Parameters* mpParameters;                // +0x14  the adopted parameter block
};



// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::SetParameters @0x821F4068
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourGyroCam,
               "lpParameters->GetType() == eBehaviourGyroCam");
    mpParameters = lpParameters;                 // stw r31, 0x14(this)
    mParamWord1  = lpParameters->miParamWord1;   // lwz r11,4(lpParameters); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H
