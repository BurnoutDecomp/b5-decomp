#ifndef BRN_REGION_H
#define BRN_REGION_H

#include "types.hpp"          // fixed-width ints (int32_t, f32, uint8_t ...)
#include "BrnCommonTypes.h"   // Vector2, Vector3, Matrix44Affine, CgsID

// SharedClasses/Trigger/BrnRegion.h
//
// Owning header for the trigger-region primitive `BrnTrigger::BoxRegion`.
//
// SHAPE is gated on the X360 DWARF (references/DecFIGS/dwarfdump/SharedClasses/
// Trigger/BrnRegion.h), which is authoritative. NOTE: the X360 build's BoxRegion
// differs from the older Feb-2007 partial source header — the leaked struct stored a
// Matrix44Affine transform + a Vector3 of dimensions; the shipped X360 layout
// instead stores nine scalar floats (position xyz / rotation xyz / dimensions xyz)
// and derives Vector3/Matrix44 results through accessors. We follow the DWARF.
//
// MINIMAL SCOPE: only BoxRegion is reconstructed here, because that is the only
// region type reached by BrnGameState::OfflineGameMode::SelectRandomDestinations
// (via Landmark -> TriggerRegion::GetBoxRegion() -> BoxRegion::GetPosition()).
// SphereRegion / LineRegion exist in the full file but are NOT needed by the
// consumer and are intentionally omitted here. The integrator should fold them
// back in if/when a consumer requires them (one owning header per type).
//
// INTEGRATOR NOTE (Vector2/Vector3 construction): the rw::math::vpu vector types
// are AGGREGATES (no user-declared constructor — see vendor/renderware/include/
// rw/math/vpu/types.h, which deliberately keeps that property so `{x,y,z,w}`
// initialisers and SetZero() callers stay valid). The accessor bodies therefore
// build their result with aggregate brace-init `Vector3{ x, y, z, 0.0f }` rather
// than a 3-arg paren constructor (which an aggregate does not have under
// /std:c++17). Same scalar values, valid syntax for the gate.

// Forward decl for the out-of-line Construct() signature only (DWARF: Quaternion
// rotation arg). No consumer in scope constructs a BoxRegion, so a forward decl
// is sufficient and avoids pulling the full math type (and avoids redefining the
// global `Quaternion` typedef that BrnCommonTypes-adjacent headers may provide).
// INTEGRATOR: this matches rw::math::vpu::Quaternion.
namespace rw { namespace math { namespace vpu { class Quaternion; } } }

namespace BrnTrigger
{

// On-disk / in-memory trigger box. 9 contiguous float32 (36 bytes), matching the
// X360 DWARF member order exactly. Accessors are INLINED in the X360 build (no
// standalone TUs in the ledger), so declarations + inline bodies are sufficient
// for the cl /c gate. ComputeDirection()/ComputeTransform() DO have their own
// TUs (ledger: BrnTrigger::BoxRegion::ComputeDirection / ::ComputeTransform) and
// are therefore left as out-of-line declarations only.
struct BoxRegion
{
public:
    void Construct();
    void Construct( Vector3 lCenter, rw::math::vpu::Quaternion lRotation, Vector3 lSize );

    void FixDown();
    void FixUp();

    // The accessor SelectRandomDestinations actually uses:
    inline Vector3 GetPosition() const;

    // Other inlined accessors present in the X360 build (kept for completeness of
    // the owning type; all inline, none have standalone TUs):
    inline Vector2   GetPosition2D() const;
    inline Vector3   GetRotation() const;
    inline Vector3   GetDimensions() const;
    inline f32 GetDimensionX() const;
    inline f32 GetDimensionY() const;
    inline f32 GetDimensionZ() const;

    // These two have their own out-of-line X360 bodies (ComputeDirection @0x821F2CA8,
    // ComputeTransform @0x821F2FD0). [gateui] 2026-08-20: both are now RECONSTRUCTED, in the
    // sibling BrnRegion.cpp -- read its banner before touching either. The short version:
    // the stored mRotationX/Y/Z are RADIANS (the console's inlined range-reduction lane is
    // 1/2pi, not 1/360), ComputeTransform is RotZ * (RotY * RotX) applied to the world basis
    // with the centre as the translation row, and ComputeDirection is the "at" row of
    // RotY * RotX.
    // ⛔ CONDUCTOR: GameSource/Director/DirectorLinkStubs.cpp still defines ComputeTransform
    // as an identity stub; that block must be deleted when BrnRegion.cpp mounts (LNK2005).
    Vector3        ComputeDirection() const;
    Matrix44Affine ComputeTransform() const;

private:
    f32 mPositionX;
    f32 mPositionY;
    f32 mPositionZ;
    f32 mRotationX;
    f32 mRotationY;
    f32 mRotationZ;
    f32 mDimensionX;
    f32 mDimensionY;
    f32 mDimensionZ;
};

inline Vector3
BoxRegion::GetPosition() const
{
    return Vector3{ mPositionX, mPositionY, mPositionZ, 0.0f };
}

inline Vector2
BoxRegion::GetPosition2D() const
{
    return Vector2{ mPositionX, mPositionZ, 0.0f, 0.0f };
}

inline Vector3
BoxRegion::GetRotation() const
{
    return Vector3{ mRotationX, mRotationY, mRotationZ, 0.0f };
}

inline Vector3
BoxRegion::GetDimensions() const
{
    return Vector3{ mDimensionX, mDimensionY, mDimensionZ, 0.0f };
}

inline f32
BoxRegion::GetDimensionX() const
{
    return mDimensionX;
}

inline f32
BoxRegion::GetDimensionY() const
{
    return mDimensionY;
}

inline f32
BoxRegion::GetDimensionZ() const
{
    return mDimensionZ;
}

} // namespace BrnTrigger

#endif // BRN_REGION_H
