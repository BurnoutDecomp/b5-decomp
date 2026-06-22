#ifndef GAMESOURCE_DIRECTOR_CAMERA_SHAREDIO_BRN_PLAYER_INFO_H
#define GAMESOURCE_DIRECTOR_CAMERA_SHAREDIO_BRN_PLAYER_INFO_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector3, Matrix44Affine
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // BrnPhysics::Vehicle::RaceCarState (committed home)

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
    // Axis-aligned bounding box storage. The engine's real AABBox is
    // rw::collision::AABBoxTemplate<Matrix44Affine, Vector3, ...> (SDKs/EATech
    // cmn/rw/collision/aabbox.h) -- two Vector3 min/max corners, 32 bytes. That full
    // RenderWare collision template is not yet reconstructed in a vendor home; this TU
    // only needs the 32-byte min/max storage that VehicleInfo embeds, so model exactly
    // that here. (HONEST: a storage-only stand-in for the EATech AABBox; grow/replace
    // with the real vendor home when that template is reconstructed.)
    struct alignas(16) AABBox
    {
        Vector3 mMin;   // aabbox.h: m_min
        Vector3 mMax;   // aabbox.h: m_max
    };

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
}
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_SHAREDIO_BRN_PLAYER_INFO_H
