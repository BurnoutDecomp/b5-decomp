#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
// CameraShake (+ ::Parameters) now comes from its CANONICAL home -- the DWARF's own
// BrnCameraShake.h -- instead of the copy that used to live inside Behaviours/BehaviourRig.h.
// That is exactly what this file's FLAG below asked for, and it drops a heavy behaviour
// header (and its whole rig cascade) off this TU's include graph.
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"       // Utils::CameraShake::Parameters

// BrnDirector::Camera::Utils::CameraImpactEffect - the impact-driven camera-shake
// wrapper (an impact-magnitude accumulator feeding an embedded CameraShake).
// DWARF home BrnCameraShake.h:105 (members :126/:127; Parameters :133). This TU
// bodies RegisterImpact; Construct/Update are their own ledger functions.
//
// FLAG (member/type notes): the embedded runtime CameraShake's committed definition
// currently lives in the BehaviourRig.h Utils stub block; to keep the runtime state
// slice untouched, mCameraShake stays modelled as correctly-sized opaque storage
// (4 floats == 16 bytes, matching the committed wobble state) -- adopt the real
// member when CameraShake migrates to its canonical BrnCameraShake.h home.
// The Parameters record (:133) IS homed below (its three Serialise<S> field-walk
// visitors are this TU); its leading sub-block is a by-value CameraShake::Parameters
// (from BehaviourRig.h), matching the "Shake parameters" nested-section recursion in
// the asm.
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

        // --------------------------------------------------------------------
        // The impact-shake parameter block (DWARF BrnCameraShake.h:133). Layout pinned
        // from the three Serialise<S> visitor bodies (write @0x82232D98, read @0x82215CF0,
        // debug-menu @0x822327B0): a by-value CameraShake::Parameters sub-block at +0x00
        // (walked as the nested "Shake parameters" section) followed by three f32 impact
        // tunables. The trailing offsets are the a1+0x10 / a1+0x14 / a1+0x18 field
        // displacements the write/read/menu asm loads/stores. No pointers => the offsets
        // are host-pointer-width invariant (pinned in the .cpp).
        // --------------------------------------------------------------------
        class Parameters
        {
        public:
            // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the
            // camera-tunings serialiser S (DebugMenuSerialiser / TextFile{Read,Write}Serialiser),
            // recursing into the embedded shake block for the "Shake parameters" section. The
            // per-instance body is a separate TU (this one); bodied in BrnCameraImpactEffect.cpp
            // with one explicit instantiation per serialiser. Declared so a serialiser's
            // Serialise<Parameters> can drive it by name.
            template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

            CameraShake::Parameters mShakeParams;           // +0x00  embedded shake tunings ("Shake parameters")
            f32                     mfShakeDecayFactor;      // +0x10  "Shake decay factor"
            f32                     mfShakeMagnitude;        // +0x14  "Shake magnitude"
            f32                     mfShakeFrequencyScale;   // +0x18  "Shake frequency scale"
        };

    private:
        f32 mfImpactFactor;          // :126  +0x00
        u8  maCameraShake[16];       // :127  +0x04 (CameraShake; opaque -- see FLAG)
    };
}
}
}
