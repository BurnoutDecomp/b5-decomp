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
// ✅ THE OLD FLAG HERE IS PAID OFF (2026-08-29, crash-camera wave). It read: "the embedded
// runtime CameraShake's committed definition currently lives in the BehaviourRig.h Utils stub
// block; to keep the runtime state slice untouched, mCameraShake stays modelled as
// correctly-sized opaque storage (4 floats == 16 bytes) -- adopt the real member when
// CameraShake migrates to its canonical BrnCameraShake.h home." It HAS migrated (this file
// already includes that home), so mCameraShake is now the real type. The trigger for paying it
// off: ImpactShakeController::Update hands `this + 4` to CameraShake::Update, and with an
// opaque span the only way to spell that is a raw-offset cast -- exactly what the faithfulness
// gate exists to stop.
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

        // The embedded runtime shake this effect drives. ⭐ DE-FORKED 2026-08-29 (crash-camera
        // wave): mCameraShake was `u8 maCameraShake[16]` behind the FLAG above, whose own
        // DELETE-WHEN read "adopt the real member when CameraShake migrates to its canonical
        // BrnCameraShake.h home". It has migrated -- this header already #includes that home --
        // so the real member is adopted here. Both forms are 16 bytes (four wobble floats), so
        // nothing shifts; what changes is that a caller can now hand the shake to
        // CameraShake::Update BY NAME instead of casting the opaque span.
        // X360 witness: ImpactShakeController::Update @0x82243720 calls RegisterImpact on
        // `this` and CameraShake::Update on `this + 4` -- i.e. exactly mfImpactFactor then
        // mCameraShake.
        CameraShake&       GetCameraShake()       { return mCameraShake; }
        const CameraShake& GetCameraShake() const { return mCameraShake; }

        // The accumulated impact magnitude the shake is scaled by (asm: `lfs f12, 0(this)`
        // in ImpactShakeController::Update, and the fmadds decay that follows writes it back).
        f32  GetImpactFactor() const        { return mfImpactFactor; }
        void SetImpactFactor(f32 lfFactor)  { mfImpactFactor = lfFactor; }

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
        f32         mfImpactFactor;  // :126  +0x00
        CameraShake mCameraShake;    // :127  +0x04 (16B: the four wobble floats)
    };

    // The whole record is 20 bytes on both the console and the host (five floats, no pointer).
    // That is the number ArbStateCrashing::ApplySlomoAndShake @0x8224F8D8 proves independently:
    // its shake-suppression arm clears exactly five floats at +0x1B4..+0x1C4 and the state's
    // mMomentSelector starts at +0x1C8.
    static_assert(sizeof(CameraImpactEffect) == 20, "CameraImpactEffect is f32 + CameraShake(16)");
}
}
}
