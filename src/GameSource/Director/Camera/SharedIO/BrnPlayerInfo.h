#ifndef GAMESOURCE_DIRECTOR_CAMERA_SHAREDIO_BRN_PLAYER_INFO_H
#define GAMESOURCE_DIRECTOR_CAMERA_SHAREDIO_BRN_PLAYER_INFO_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector3, Matrix44Affine
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // BrnPhysics::Vehicle::RaceCarState (committed home)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                        // BrnDirector::Camera::AABBox (THE home; the local copy is retired)

// ============================================================================
// GameSource/Director/Camera/Camera/SharedIO/BrnPlayerInfo.h
//
// BrnDirector::Camera::VehicleInfo -- the per-frame vehicle snapshot the director's
// camera behaviours read. It is a full BrnPhysics::Vehicle::RaceCarState (the physics
// publish) plus a handful of director-camera-only fields: the centre-of-mass transform
// to frame a crashing car around, the car's world AABB, and the hardest impact this
// frame (normal + magnitude + a couple of flags). The vehicle tracker / behaviour rigs
// copy one of these into their working state each update, which is exactly what the two
// recovered functions do (a copy constructor and a copy-assignment).
//
// ----------------------------------------------------------------------------
// LAYOUT (member NAMES + types from the DecFIGS DWARF for this struct
// (BrnPlayerInfo.h:91); OFFSETS/ORDER are pinned by the recovered copy-ctor
// @0x8221CDC8 and operator= @0x821F49C8 member-copy sequences). Members are accessed
// BY NAME; the struct-relative offsets in comments are provenance only, never casts.
//
//   mRaceCarState                @0x000  RaceCarState        (committed, sizeof==1120 / 0x460)
//   mCrashingCentreOfMass        @0x460  Matrix44Affine      (64 / 0x40)
//   mAABB                        @0x4A0  AABBox              (32 / 0x20)
//   mHardestNormalStressNormal   @0x4C0  Vector3             (16)
//   mHardestNormalStress         @0x4D0  Vector3             (16)
//   mfHardestImpact              @0x4E0  f32
//   mbHardestImpactIsAgainstWorld@0x4E4  bool
//   mbHasCrashingCenterOfMass    @0x4E5  bool
//   mbEngineOn                   @0x4E6  bool
//
// The X360 copies the whole mCrashingCentreOfMass matrix (all four 16-byte lanes) and
// copies mAABB as a raw 32-byte (two-Vector3) block. The reconstruction below performs the
// same member-wise copy; the type is otherwise a plain trivially-copyable aggregate.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{
    // RETIRED (2026-07-29): this header used to carry its own byte-identical copy of the
    // BrnDirector::Camera::AABBox storage stand-in, whose own comment already noted the
    // duplicate in Camera/Utils/CameraUtils.h. Keeping both meant any TU that reached this
    // header AND the camera utils died with C2011 -- which is exactly what happened the
    // moment the canonical Behaviour.h started embedding VehicleInfo by value. One home now
    // (CameraUtils.h, included above); the definition itself is unchanged.

    // Per-frame vehicle info published to the director's camera. See file header for the
    // full layout / provenance.
    struct alignas(16) VehicleInfo
    {
        BrnPhysics::Vehicle::RaceCarState mRaceCarState;            // @0x000
        Matrix44Affine                    mCrashingCentreOfMass;    // @0x460
        AABBox                             mAABB;                    // @0x4A0
        Vector3                            mHardestNormalStressNormal; // @0x4C0
        Vector3                            mHardestNormalStress;     // @0x4D0
        f32                                mfHardestImpact;          // @0x4E0
        bool                               mbHardestImpactIsAgainstWorld; // @0x4E4
        bool                               mbHasCrashingCenterOfMass; // @0x4E5
        bool                               mbEngineOn;               // @0x4E6

        // Copy constructor @0x8221CDC8. The X360 body runs the RaceCarState base copy then
        // a member-wise copy of the director-camera fields (VMX 16-byte loads/stores for the
        // SIMD members + an 8-dword loop for the AABB + scalar copies). Reconstructed
        // out-of-line in the .cpp as the equivalent member-wise copy.
        VehicleInfo(const VehicleInfo& rhs);

        // operator= @0x821F49C8. The X360 body XMemCpy's the whole RaceCarState then does the
        // same member-wise copy of the director-camera fields. Returns the assigned object's
        // address (this); reconstructed as a conventional reference-returning operator= in the
        // .cpp.
        VehicleInfo& operator=(const VehicleInfo& rhs);

        // The recovered TU only defines the copy ctor + operator=. Declaring a copy ctor
        // suppresses the implicit default ctor, but RaceCarState (and the rest) are
        // default-constructible, so provide a defaulted one so the type stays usable.
        VehicleInfo() = default;
    };

    // ------------------------------------------------------------------------
    // ⭐⭐ BrnDirector::Camera::PlayerCrashInfo -- HOMED 2026-08-29 (crash-camera wave). The
    // per-crash analysis record the world publishes to the director: what the player hit, how
    // hard, and the two "this crash was special" verdicts.
    //
    // ⛔ IT WAS NEVER "un-homed". Three separate places in this tree record it as having NO
    // layout -- DirectorLinkStubs.cpp's crash-camera remainder, BrnDirectorArbitrator.cpp's
    // gated "BlackFade_Water" branch ("mpPlayerCrashInfo[+39] ... has no homed layout ... DO NOT
    // run either arm on a guessed condition"), and BrnDirectorModuleIOInputBuffer.cpp's FLAG on
    // the slot accessor. The DWARF has had it all along, at BrnPlayerInfo.h:112..:131 -- the
    // searches missed it because it is `BrnDirector::Camera::PlayerCrashInfo` and every
    // consumer forward-declares `BrnDirector::PlayerCrashInfo`: the SAME namespace fork that
    // hid Camera::VehicleInfo from ArbStateSharedInfo::mpPlayerCar.
    //
    // ⭐ TWO INDEPENDENT CONSUMERS CONFIRM THE MEMBER ORDER, and each one's gate now has a name:
    //   +0x26  mbWrecked   -- ArbStateCrashing::Update @0x8226BFB0 reads `*(crashInfo + 38)` to
    //                         pick "Wrecked" over "Crash" for the screen effect and to latch
    //                         mbPlayerWasWreckedThisCrash, which chooses the wrecked exit.
    //   +0x27  mbHitWater  -- Arbitrator::Update reads `mpPlayerCrashInfo[+39]` as the condition
    //                         on the "BlackFade_Water" full-screen fade. A drowning fade keyed
    //                         on a flag called mbHitWater is as strong a corroboration as the
    //                         offsets themselves.
    // Two consecutive bools, two consumers, two roles that match their names exactly.
    //
    // ⚠️ THE PRODUCER IS STILL DROPPED. Step 13 of BridgeWorldToDirector ("build the 48-byte
    // PlayerCrashInfo block from the vehicle manager's crash queue") is not reconstructed, so
    // the storage the input buffer hands out is not yet filled by anything. This header closes
    // the READ end only. DELETE-WHEN (for the write end): GameBridgeWorldToX.cpp step 13 lands.
    // The console block is 40 bytes and 16-aligned (two Vector3s lead it), which is where the
    // "48-byte block" the bridge banner quotes comes from.
    // ------------------------------------------------------------------------
    struct alignas(16) PlayerCrashInfo
    {
        // BrnPlayerInfo.h:114 -- seed the record. Declaration-only (its own ledger function;
        // the producer TU owns the body).
        void Construct();

        Vector3 mvCollisionNormal;   // :125  +0x00
        Vector3 mvContactPoint;      // :126  +0x10
        f32     mfSpeedMPH;          // :127  +0x20
        bool    mbHardstopVsWall;    // :128  +0x24
        bool    mbHardStopVsAI;      // :129  +0x25
        bool    mbWrecked;           // :130  +0x26  (ArbStateCrashing: "Wrecked" vs "Crash")
        bool    mbHitWater;          // :131  +0x27  (Arbitrator: the "BlackFade_Water" gate)
    };

    static_assert(offsetof(PlayerCrashInfo, mbWrecked) == 0x26,
                  "PlayerCrashInfo::mbWrecked @ +0x26 (ArbStateCrashing::Update's read)");
    static_assert(offsetof(PlayerCrashInfo, mbHitWater) == 0x27,
                  "PlayerCrashInfo::mbHitWater @ +0x27 (Arbitrator::Update's BlackFade_Water gate)");
}
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_SHAREDIO_BRN_PLAYER_INFO_H
