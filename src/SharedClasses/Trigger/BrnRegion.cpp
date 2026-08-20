// SharedClasses/Trigger/BrnRegion.cpp
//
// [gateui] NEW TU, 2026-08-20. The two out-of-line BoxRegion members:
//
//   BrnTrigger::BoxRegion::ComputeTransform  @ X360 0x821F2FD0
//   BrnTrigger::BoxRegion::ComputeDirection  @ X360 0x821F2CA8
//
// Both were declaration-only and are measured UNDEF externals in
// BrnStuntManager.obj, StuntManager_gUI_00.obj, BrnTriggerQueryManager.obj and
// BrnChallengeManager_wC_04.obj. `ComputeTransform` additionally had a LIVE
// IDENTITY STUB standing in for it -- see the CONDUCTOR note at the bottom.
//
// ------------------------------------------------------------------------------
// WHY THIS IS SIX LINES OF C++ AND NOT 300 LINES OF __asm
// ------------------------------------------------------------------------------
// The X360 bodies are 190 / 284 instructions of pure VMX with no calls at all,
// because the compiler INLINED the rwmath `SinCos` minimax three (resp. two)
// times. AGENTS.md's "inlining reversal" rule applies exactly: what is
// reconstructed is the source the compiler was handed, not its flattened output.
//
// The inlined helper is identified, not guessed. The range-reduction register the
// two bodies load first is `unk_82000C60`, and this tree has ALREADY decoded that
// constant from a third, independent site: rw/math/vpu/matrix44affine_operation.h
// (`Matrix44AffineFromAxisRotationAngle`, inlined by PropManager::
// HandleContactWithLeanProp @0x82610140) records
//     unk_82000C60 == { pi, 2pi, 1/pi, 1/2pi }
// with the coefficient blocks unk_82000BD0/BE0/BF0 and C00/C10/C20. Both bodies
// here do
//     vspltw v1, v0, 3        ; 1/2pi
//     vspltw v31, v0, 1       ; 2pi
//     vmulfp128 v10, angle, v1 / vrfin v12, v10     ; q = round(angle / 2pi)
//     vnmsubfp v4, v31, angle, v12                  ; r = angle - 2pi*q
// then feed `r` to the odd/even power series. That is textbook 2*pi range
// reduction, which pins TWO things at once: the helper is SinCos, and
// **mRotationX/Y/Z are RADIANS** (a degree-keyed field would carry a 1/360 lane,
// not 1/2pi). std::sin / std::cos are the exact forms of that minimax -- the same
// de-optimisation MakeRotationX/Y/Z, Normalize and Inverse already carry in this
// tree, never a placeholder.
//
// ------------------------------------------------------------------------------
// THE COMPOSITION, READ OFF THE ASM (this is the load-bearing half)
// ------------------------------------------------------------------------------
// ComputeTransform seeds the output with the world basis and then rotates it three
// times. The seed is explicit: three 16-byte stack vectors are built from
// flt_82001C98 (1.0f) and flt_82001CC0 (0.0f) --
//     sp+0x00 = (1,0,0,0)   sp+0x10 = (0,1,0,0)   sp+0x20 = (0,0,1,0)
// -- and stored straight into out+0x00 / out+0x10 / out+0x20 at
// 0x821F3064 / 0x821F3074 / 0x821F3080.
//
// Stage 1 (mRotationX, `lvlx v0, r4, 12`), at 0x821F31F4..0x821F3208 with
// v90 == the Y row and v89 == the Z row:
//     v5 = Z*sinX + Y*cosX          -> the new Y row
//     v6 = Z*cosX - Y*sinX          -> the new Z row   (stored to out+0x20)
//   which is MakeRotationX verbatim: yAxis (0, cos, sin), zAxis (0, -sin, cos).
//
// Stage 2 (mRotationY, `lvlx v13, r4, 16`), at 0x821F32FC..0x821F3324, over the
// CURRENT rows (v91 == out.xAxis reloaded at 0x821F3274, v6 == the stage-1 Z row):
//     v31 = X*cosY - Z1*sinY        -> the new X row
//     v1  = X*sinY + Z1*cosY        -> the new Z row   (stored to out+0x20)
//   which is `Mult(MakeRotationY, current)` -- RotY.xAxis is (cos,0,-sin) and
//   RotY.zAxis is (sin,0,cos), and Mult(A,B).row_i = A.row_i.x*B.xAxis +
//   A.row_i.y*B.yAxis + A.row_i.z*B.zAxis (rw/math/vpu/matrix44affine_operation.h).
//
// Stage 3 (mRotationZ, `lvlx v12, r4, 20`), at 0x821F340C..0x821F3438:
//     out.xAxis = X2*cosZ + Y1*sinZ
//     out.yAxis = Y1*cosZ - X2*sinZ
//   which is `Mult(MakeRotationZ, current)`.
//
// So the whole 3x3 is
//     Mult(RotZ, Mult(RotY, RotX))  ==  RotZ * (RotY * RotX)
// and the translation row is the raw centre: 0x821F33BC..0x821F33E4 copy
// `lfs 0(r4) / 4(r4) / 8(r4)` into a stack vector whose 4th lane is zeroed, and
// 0x821F3430 stores it to out+0x30. (Note the W lane is 0.0f, NOT 1.0f -- the SDK
// affine convention MakeRotation*/SetIdentity in this tree already follow.)
//
// ComputeDirection evaluates only TWO SinCos (mRotationX and mRotationY) and
// returns a single vector: 0x821F2EFC..0x821F2F00 build the stage-1 Z row
// (Z*cosX - Y*sinX), and 0x821F2FAC..0x821F2FC4 finish it as
// X*sinY + Z1*cosY -- i.e. exactly the Z ("at") row of Mult(RotY, RotX), stored
// through the sret pointer in r3. The console is entitled to drop the third
// rotation because rolling about the local Z leaves the local Z fixed, so this is
// the same vector as ComputeTransform().At(); it is written below as the two-
// rotation product the asm actually evaluates.
//
// No asserts: neither body calls anything (`blr` is the only branch out).
//
// ------------------------------------------------------------------------------
// CONDUCTOR -- GATE TO RETIRE (link-blocking, must land WITH this file)
// ------------------------------------------------------------------------------
// GameSource/Director/DirectorLinkStubs.cpp (mounted, build_game_exe.bat:3582)
// still defines `BrnTrigger::BoxRegion::ComputeTransform` as an IDENTITY stub, and
// its own comment says so ("BoxRegion::ComputeTransform below is STILL A STUB --
// it is only reached on a hit, which could not happen before and now can").
// Mounting this TU without deleting that block is LNK2005. The stub is also a live
// wrong answer today for every reader that DOES reach it -- an unrotated unit box
// at the world origin -- which is precisely the class of defect the same file's
// banner already retired two of.
// ------------------------------------------------------------------------------

#include "SharedClasses/Trigger/BrnRegion.h"

#include "rw/math/vpu/matrix44affine_operation.h"   // MakeRotationX/Y/Z, Mult / operator*

namespace BrnTrigger
{

// ====================================================================================
// BoxRegion::ComputeTransform  @ X360 0x821F2FD0
// The region's world transform: the intrinsic X-then-Y-then-Z rotation of the world
// basis by the stored Euler angles (radians), positioned at the stored centre.
// ====================================================================================
Matrix44Affine
BoxRegion::ComputeTransform() const
{
    // Stage 1 -> stage 2 -> stage 3, in the console's own order. Parenthesised to
    // mirror the asm's accumulation (Mult is associative, so the grouping is a
    // readability choice, not a behavioural one).
    Matrix44Affine lTransform =
        rw::math::vpu::MakeRotationZ( mRotationZ ) *
            ( rw::math::vpu::MakeRotationY( mRotationY ) *
              rw::math::vpu::MakeRotationX( mRotationX ) );

    // 0x821F33BC..0x821F3430: the centre, W lane zeroed.
    lTransform.wAxis = GetPosition();

    return lTransform;
}

// ====================================================================================
// BoxRegion::ComputeDirection  @ X360 0x821F2CA8
// The region's facing: the "at" (Z) row of the same basis with the roll term dropped
// -- the console evaluates only the X and Y rotations because a roll about the local
// Z cannot move the local Z.
// ====================================================================================
Vector3
BoxRegion::ComputeDirection() const
{
    const Matrix44Affine lRotation =
        rw::math::vpu::MakeRotationY( mRotationY ) *
        rw::math::vpu::MakeRotationX( mRotationX );

    return lRotation.At();
}

} // namespace BrnTrigger
