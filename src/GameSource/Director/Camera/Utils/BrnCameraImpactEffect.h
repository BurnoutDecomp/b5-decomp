#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnDirector::Camera::Utils::CameraImpactEffect - the impact-driven camera-shake
// wrapper (an impact-magnitude accumulator feeding an embedded CameraShake).
// DWARF home BrnCameraShake.h:105 (members :126/:127; Parameters :133). This TU
// bodies RegisterImpact; Construct/Update are their own ledger functions.
//
// FLAG (member/type notes): the embedded CameraShake's committed definition
// currently lives in the BehaviourRig.h Utils stub block -- to avoid pulling that
// heavy header (and any ODR risk) into this small home, mCameraShake is modelled
// as correctly-sized opaque storage (4 floats == 16 bytes, matching the committed
// wobble state); adopt the real member when CameraShake migrates to its canonical
// BrnCameraShake.h home. The Parameters record (:133) is likewise deferred to the
// consumers that read it (BehaviourPassengerCam holds it opaquely already).
namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    class CameraImpactEffect
    {
    public:
        // DWARF :111/:118 -- declared-only (their own ledger functions).
        void Construct();
        // (Update's full DWARF shape: Update(Camera&, const Parameters&, Random&, f32).)

        // @0x821F3648 (class TU; body in BrnCameraImpactEffect.cpp, DWARF :122,
        // assert BrnCameraShake.h:221) -- accumulate an impact: the pending factor
        // keeps the LARGEST registered magnitude (the X360 fsel max).
        void RegisterImpact(f32 lfImpulseMagnitude);

    private:
        f32 mfImpactFactor;          // :126  +0x00
        u8  maCameraShake[16];       // :127  +0x04 (CameraShake; opaque -- see FLAG)
    };
}
}
}
