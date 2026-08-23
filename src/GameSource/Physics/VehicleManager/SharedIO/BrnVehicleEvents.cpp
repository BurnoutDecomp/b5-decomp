// BrnPhysics::Vehicle::RaceCarState ledger functions.
//
//   Clear()      @ 0x8229FFC8  -- zero the whole snapshot, then re-init the affine
//                                 transforms to identity, reset the above-ground test
//                                 result, and seed mfSpeedMPH from its constant.
//   RaceCarState(const RaceCarState&) @ 0x8220A4C0 -- the X360 body is a pure bitwise copy
//                                 of the entire 1120-byte object (memcpy 448 + memcpy 48 +
//                                 VMX 16-byte copies over the matrix region + word loops).
//                                 RaceCarState is trivially copyable, so `= default`
//                                 reproduces it byte-for-byte.
//
// Layout/shape authority: references/DecFIGS/dwarfdump/.../SharedIO/BrnVehicleEvents.h.
// Body authority: the X360 pseudocode/asm at 0x8229FFC8 / 0x8220A4C0. The reconstructed
// member offsets sum to sizeof(RaceCarState) == 1120, matching the memset length, which
// confirms the byte-offset -> named-member mapping below:
//   +496  -> mTransform              (identity affine: 3 unit rows + zero wAxis)
//   +560  -> maWheelTransforms[0..3] (the X360 do-while loop, stride 64, 4 iterations)
//   +480/+484/+486/+488 -> mAboveGroundTestResult.{mfVerticalDistance,mCollisionTag,mbValid}
//   +968  -> mfSpeedMPH              (set from const-pool dword_82CDB5A0; see KF_CLEAR_SPEED_MPH)
// All other re-init stores in the X360 body (the +1100..+1112 bool writes, and the zeroing
// of the AGTR intersection vectors) are already covered by the memset, so they are not
// restated here.

#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

#include <cstring>   // memset

namespace
{
// X360 Clear() seeds mfSpeedMPH (RaceCarState+968) from the const-pool dword_82CDB5A0
// (loaded `lwz`, stored `stw`) rather than the just-zeroed VMX register, so the value is
// non-zero. Its literal is a raw .rdata float address not recoverable from the function
// exports alone; 0.0f is a compile-safe placeholder. TODO(maintainer): read dword_82CDB5A0
// from BURNOUT_X360_ARTIST.XEX (.rdata @0x82CDB5A0) and substitute the exact float.
const f32 KF_CLEAR_SPEED_MPH = 0.0f;

// AboveGroundTestResult::Reset() (inlined into Clear) leaves mCollisionTag at the value the
// X360 builds with `sth -1 @+484` then `sth 0x8000 @+486` (big-endian u32 => 0xFFFF8000),
// i.e. the "invalid surface" tag. Confirm against the real Reset() when the
// BrnSimpleVehiclePhysics TU lands.
const u32 KU_CLEAR_COLLISION_TAG = 0xFFFF8000u;
}

namespace BrnPhysics
{
namespace Vehicle
{

// @0x8229FFC8
void RaceCarState::Clear()
{
    // X360: memset(this, 0, 1120). sizeof(RaceCarState) == 1120 by construction.
    memset(this, 0, sizeof(*this));

    // +496: the body transform is reset to the identity affine.
    mTransform.SetIdentity();

    // +560: each of the four wheel transforms is reset to identity (the do-while loop).
    for (s32 li = 0; li < 4; ++li)
    {
        maWheelTransforms[li].SetIdentity();
    }

    // +480/+484/+486/+488: AboveGroundTestResult::Reset() inlined. The intersection
    // vectors are already zero from the memset; restate the non-zero fields.
    mAboveGroundTestResult.mfVerticalDistance    = 0.0f;
    mAboveGroundTestResult.mCollisionTag.muValue = KU_CLEAR_COLLISION_TAG;
    mAboveGroundTestResult.mbValid               = false;

    // +968: seed the published speed from its const-pool value.
    mfSpeedMPH = KF_CLEAR_SPEED_MPH;
}

// @0x8220A4C0 -- pure bitwise copy of the whole object; trivially copyable => defaulted.
RaceCarState::RaceCarState(const RaceCarState&) = default;

// ============================================================================
// operator= (DWARF :214, returns void). No X360 address -- the build inlined every
// assignment site, and the copy constructor above proves the shape: a pure bitwise copy
// of the whole 1120-byte object.
//
// WHY THIS EXISTS NOW (2026-08-01, camera wave). This declared-but-undefined operator
// had been resolving from BrnBaselineLinkStubs.cpp as `{}` -- an EMPTY BODY -- whose own
// comment said "Only the Director camera path -- OFF the boot/title/menu path -- reaches
// it". That path is now live, and the effect was invisible and total: EVERY RaceCarState
// assignment in the tree silently copied nothing. In particular
//     RCEntityActiveRaceCarOutputInterface::operator= (maRaceCarStates[i] = ...)
//     BrnDirector::Camera::VehicleInfo::operator=     (mRaceCarState = ...)
// both ran, both looked right, and both dropped the car's transform -- so the world
// published a correctly-posed car at (3008.17, -1.16, -1874.30) and the director's camera
// received one at the ORIGIN. Bisected with a source/destination pose print.
//
// The member copy is spelled out rather than memcpy'd only where it costs nothing: the
// whole object is standard-layout scalars/arrays, so the bitwise copy IS the member-wise
// copy, and it is what the console emits.
// ============================================================================
void RaceCarState::operator=(const RaceCarState& lrOther)
{
    if (this != &lrOther)
    {
        memcpy(this, &lrOther, sizeof(*this));
    }
}

}
}
