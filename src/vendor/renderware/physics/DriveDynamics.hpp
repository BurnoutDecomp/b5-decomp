#pragma once

#include "types.hpp"   // f32
#include <cstddef>            // offsetof (the layout pins below)

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
            // DWARF drivedynamics.h:32 declares this constructor. Its body is the store
            // pattern the console inlines into PhysicsSimulationModule::PhysicsSimulationModule
            // @0x827DF250..0x827DF294 -- per Params, three `stfs 0.0f` at +0/+4/+8 and one
            // `stw 0` at +0xC, emitted twice per DriveDynamics and twice over for
            // DriveData's maDynamics[0] and maScaledDynamics[0]. ⚠️ ADDED 2026-08-04
            // (task #143) as the honest home for stores this tree already described as
            // "default-construct every entry" but spelled out by hand in DriveData's
            // constructor against a SECOND, forked copy of this class. Zero new behaviour:
            // the same eight slots are zeroed, from the type that owns them.
            Params()
                : mSpring(0.0f), mDamping(0.0f), mStrength(0.0f), mType(NO_DRIVE) {}

            const f32& GetSpring() const      { return mSpring; }
            const f32& GetMaxVelocity() const { return mSpring; }    // same slot, HARD_DRIVE
            const f32& GetDamping() const     { return mDamping; }
            const f32& GetMaxStrength() const { return mStrength; }
            DriveType  GetDriveType() const   { return mType; }

        private:
            friend void _rw_physics_DriveDynamics_AssertLayout();

            f32       mSpring;     // :204  +0x00
            f32       mDamping;    // :205  +0x04
            f32       mStrength;   // :206  +0x08  the per-row impulse clamp scale
            DriveType mType;       // :207  +0x0C  -> the two switches
        };

        const Params& LinearParams() const  { return mLinear; }
        const Params& AngularParams() const { return mAngular; }

    private:
        friend void _rw_physics_DriveDynamics_AssertLayout();

        Params mLinear;    // :261  +0x00
        Params mAngular;   // :262  +0x10
    };

    // ⭐ SOLE OWNER OF THIS RECORD SINCE 2026-08-04 (task #143), and these pins MOVED HERE
    // from CgsPhysicsSimulationModule.cpp with the seventh type fork they used to gate.
    // `CgsPhysics::DriveDynamics` was a SECOND, independent definition of these same six
    // fields (re-spelled mfSpring/mfDamping/mfStrength/meType over its own E_DriveType);
    // it is now a plain alias onto this class. ⛔ Do not re-introduce a local copy: this
    // type reaches the solver as a bare `void*` through Simulation::AddDrive, so a body
    // compiled against one copy converts SILENTLY at the call site and the link succeeding
    // proves nothing. See the block at CgsPhysicsSimulationModule.h.
    inline void _rw_physics_DriveDynamics_AssertLayout()
    {
        static_assert(offsetof(DriveDynamics::Params, mSpring)   == 0x00, "Params.mSpring   @+0x00 (ctor stfs 0.0f, 0(r11))");
        static_assert(offsetof(DriveDynamics::Params, mDamping)  == 0x04, "Params.mDamping  @+0x04 (ctor stfs 0.0f, 4(r11))");
        static_assert(offsetof(DriveDynamics::Params, mStrength) == 0x08, "Params.mStrength @+0x08 (ctor stfs 0.0f, 8(r11))");
        static_assert(offsetof(DriveDynamics::Params, mType)     == 0x0C, "Params.mType     @+0x0C (ctor stw 0, 0xC(r11))");

        // Adjacency form -- survives a member WIDENING, which a total-size check would not.
        static_assert(offsetof(DriveDynamics::Params, mDamping)  == offsetof(DriveDynamics::Params, mSpring)  + sizeof(DriveDynamics::Params::mSpring),   "mDamping follows mSpring");
        static_assert(offsetof(DriveDynamics::Params, mStrength) == offsetof(DriveDynamics::Params, mDamping) + sizeof(DriveDynamics::Params::mDamping),  "mStrength follows mDamping");
        static_assert(offsetof(DriveDynamics::Params, mType)     == offsetof(DriveDynamics::Params, mStrength)+ sizeof(DriveDynamics::Params::mStrength), "mType follows mStrength");

        static_assert(offsetof(DriveDynamics, mLinear)  == 0x00, "DriveDynamics.mLinear  @+0x00 (:261)");
        static_assert(offsetof(DriveDynamics, mAngular) == 0x10, "DriveDynamics.mAngular @+0x10 (asm second quad at +0x10..+0x1C)");
        static_assert(offsetof(DriveDynamics, mAngular) == offsetof(DriveDynamics, mLinear) + sizeof(DriveDynamics::mLinear), "mAngular follows mLinear");

        // The X360-attested ARRAY STRIDE -- DriveData::AddDrive @0x828A0330 reaches
        // maDynamics as `(i+2)<<5` and maScaledDynamics as `(i+3)<<5`, i.e. 32 bytes apart.
        static_assert(sizeof(DriveDynamics) == 32, "DriveDynamics array stride 32 (AddDrive (i+2)<<5 / (i+3)<<5)");
    }
}
}
