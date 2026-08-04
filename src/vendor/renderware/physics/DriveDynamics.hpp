#pragma once

#include "types.hpp"   // f32

// ===========================================================================
// rw::physics::DriveDynamics -- the two Params blocks (linear and angular) that describe how
// a DRIVE pulls its two bodies together: a spring/damper pair with a strength ceiling, or a
// hard velocity limit, selected by mType.
//
// PROVENANCE
//   NAMES / TYPES / ORDER : DecFIGS DWARF drivedynamics.h:66 / :204..207 / :261..262.
//   ATTESTED ON X360      : DriveJacobian::Build @0x82BC5590 reads `lfs 0(r28)` / `lfs 4(r28)`
//     / `lfs 8(r28)` and dispatches on `lwz 0xC(r28)`, and the same four again at +0x10..+0x1C
//     for the angular half.
//
// ⭐ WHY THE DWARF EXPOSES TWO ACCESSORS OVER THE SAME SLOT. GetSpring() and GetMaxVelocity()
// both read mParams+0x00, and GetMaxStrength()/GetDamping() overlap likewise. The X360 code
// proves the reading: the SOFT_DRIVE arm uses +0x00 and +0x04 as a spring/damper pair, while
// the HARD_DRIVE arm compares `dot3(err,err)` against `(mSpring*h)^2` -- i.e. in HARD the
// same slot is a per-step travel limit, `GetMaxVelocity() * h`. One slot, two meanings,
// chosen by mType.
// ===========================================================================

// ⚠️ WIN32 MACRO COLLISION. <windows.h> defines `GetDriveType` as `GetDriveTypeA`, which
// silently renames this SDK accessor and produces a C2039 on every call site. The DWARF name
// is GetDriveType (drivedynamics.h:158) and it is kept; the macro is dropped instead. This is
// the same class of collision as GetObject/GetMessage and is handled the same way.
#ifdef GetDriveType
#undef GetDriveType
#endif

namespace rw
{
namespace physics
{
    // drivedynamics.h:66. The 3-way dispatch at X360 0x82BC5860 (linear) and its mirror at
    // 0x82BC5D28 (angular) IS this enum.
    enum DriveType
    {
        NO_DRIVE   = 0,
        SOFT_DRIVE = 1,
        HARD_DRIVE = 2
    };

    class DriveDynamics
    {
    public:
        // drivedynamics.h:112 -- a nested class in the SDK.
        class Params
        {
        public:
            const f32& GetSpring() const      { return mSpring; }
            const f32& GetMaxVelocity() const { return mSpring; }    // same slot, HARD_DRIVE
            const f32& GetDamping() const     { return mDamping; }
            const f32& GetMaxStrength() const { return mStrength; }
            DriveType  GetDriveType() const   { return mType; }

        private:
            f32       mSpring;     // :204  +0x00
            f32       mDamping;    // :205  +0x04
            f32       mStrength;   // :206  +0x08  the per-row impulse clamp scale
            DriveType mType;       // :207  +0x0C  -> the two switches
        };

        const Params& LinearParams() const  { return mLinear; }
        const Params& AngularParams() const { return mAngular; }

    private:
        Params mLinear;    // :261  +0x00
        Params mAngular;   // :262  +0x10
    };
}
}
