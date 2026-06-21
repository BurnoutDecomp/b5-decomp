// ============================================================================
// SDKs/Packages/ICE/ICEAuthorSpaceOps.cpp
//
// ICE::ICEAuthor -- the SPACE / TRANSFORM group of the in-game ICE camera-take
// editor. Reconstructed against the frozen layout in ICEAuthor.hpp. THIS TU
// bodies one fan-out group of ICEAuthor (the declarations are in ICEAuthor.hpp;
// the foundational subset is bodied in ICEAuthor.cpp, the key-element/state group
// in ICEAuthorKeyElementOps.cpp). The per-TU `cl /c` gate does not link, so the
// un-bodied declarations from the other groups are fine.
//
// Group bodies:
//   GetWorldTransformationMatrix, TransformSpace, SwitchEyeSpace, SwitchLookSpace,
//   AdvanceAnimation, AdvanceAssemblyAnimation.
//
// These methods drive the take through the camera-space handler: a point or a
// matrix is projected through the per-frame reference-space transforms cached in
// mpCameraSpaceHandler (@0x08). The matrix math the build collapsed into 16-byte
// vector row moves (load/store of each Matrix4 row) and lane-broadcast multiply-
// add accumulation is reconstructed here as explicit row copies and an explicit
// homogeneous matrix * point transform over the named Vector4 lanes. The eye/look
// space switches re-project the take's stored eye/look position keys from their
// current reference spaces into a new pair of spaces and write the results back
// into the take. The two Advance entries step the take playback parameter forward
// by a time step scaled by the take length.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"               // ICE::ICETake (SetParameter/SetValue/GetValueFloat/GetValueInt); ICEElementDescription
#include "SDKs/Packages/ICE/ICEMath.hpp"               // ICE::Matrix4 / Identity / InvertTransformationMatrix
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp" // ICE::CameraSpaceHandler::GetTransformToWorld (real type behind mpCameraSpaceHandler)
#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/core/stdc/stdc.h"                          // rw::core::stdc (string/memory helpers shared by the ICE TUs)

namespace ICE
{

// ---------------------------------------------------------------------------
// File-local: copy one 4x4 affine's four rows into an ICE Matrix4.
//
// The camera-space handler returns the reference-space transform as a
// Matrix44Affine (four Vector3 rows, 16-byte aligned). ICE::Matrix4 is four
// Vector4 rows. Both rows are the same 16-byte (x,y,z,w) lane layout, so each row
// copies whole-lane -- this is the four 16-byte row load/store moves the build
// emitted to splice the affine into the Matrix4 the inverse operates on.
// ---------------------------------------------------------------------------
static void AffineToMatrix4(Matrix4* lpDst, const Matrix44Affine& lrSrc)
{
    lpDst->maRows[0].x = lrSrc.xAxis.x; lpDst->maRows[0].y = lrSrc.xAxis.y;
    lpDst->maRows[0].z = lrSrc.xAxis.z; lpDst->maRows[0].w = lrSrc.xAxis.w;
    lpDst->maRows[1].x = lrSrc.yAxis.x; lpDst->maRows[1].y = lrSrc.yAxis.y;
    lpDst->maRows[1].z = lrSrc.yAxis.z; lpDst->maRows[1].w = lrSrc.yAxis.w;
    lpDst->maRows[2].x = lrSrc.zAxis.x; lpDst->maRows[2].y = lrSrc.zAxis.y;
    lpDst->maRows[2].z = lrSrc.zAxis.z; lpDst->maRows[2].w = lrSrc.zAxis.w;
    lpDst->maRows[3].x = lrSrc.wAxis.x; lpDst->maRows[3].y = lrSrc.wAxis.y;
    lpDst->maRows[3].z = lrSrc.wAxis.z; lpDst->maRows[3].w = lrSrc.wAxis.w;
}

// ---------------------------------------------------------------------------
// File-local: transform a POINT by an ICE Matrix4 (affine point transform).
//
// Reconstructs the lane-broadcast multiply-add accumulation the build emitted:
// each of the point's four lanes (x,y,z,w) is splatted across a vector and
// multiply-added with the matching matrix row, giving
//   x*row0 + y*row1 + z*row2 + w*row3
// then row3 (the affine translation row, matrix+0x30) is added UNCONDITIONALLY
// to that chain (the trailing add of the row3 vector after the multiply-add
// run, present in both the source->world and world->destination transforms). So
// the point is moved as
//   result = x*row0 + y*row1 + z*row2 + w*row3 + row3
// The callers force w == 0 just before each transform, so the w*row3 term drops
// and the unconditional row3 add supplies the affine translation (w == 1
// behaviour). The written-back w lane is don't-care -- callers read only x/y/z.
// ---------------------------------------------------------------------------
static void TransformPoint(f32* lpfPoint, const Matrix4& lrMatrix)
{
    const f32 lfX = lpfPoint[0];
    const f32 lfY = lpfPoint[1];
    const f32 lfZ = lpfPoint[2];
    const f32 lfW = lpfPoint[3];

    for (s32 liLane = 0; liLane < 4; ++liLane)
    {
        const f32* lpfRow0 = &lrMatrix.maRows[0].x;
        const f32* lpfRow1 = &lrMatrix.maRows[1].x;
        const f32* lpfRow2 = &lrMatrix.maRows[2].x;
        const f32* lpfRow3 = &lrMatrix.maRows[3].x;
        lpfPoint[liLane] = lfX * lpfRow0[liLane]
                         + lfY * lpfRow1[liLane]
                         + lfZ * lpfRow2[liLane]
                         + lfW * lpfRow3[liLane]
                         + lpfRow3[liLane];
    }
}

// ---------------------------------------------------------------------------
// GetWorldTransformationMatrix
//
// Build the world->space transform for a single reference space: ask the camera-
// space handler for that space's space->world affine, splice it into an ICE
// Matrix4, invert the affine (the world->space matrix is the inverse of the
// space->world transform), and write the inverted matrix to *lpMatrix.
//
// The Identity seed is written before the affine is spliced in (a defensive
// init); the spliced affine then overwrites all four rows before the inverse runs.
// ---------------------------------------------------------------------------
void ICEAuthor::GetWorldTransformationMatrix(Matrix4* lpMatrix, u8 lu8Space) const
{
    Matrix4 lWorld;
    ICEMath::Identity(&lWorld);

    const Matrix44Affine lToWorld =
        mpCameraSpaceHandler->GetTransformToWorld((eICESpace)lu8Space);
    AffineToMatrix4(&lWorld, lToWorld);

    ICEMath::InvertTransformationMatrix(&lWorld, &lWorld);
    *lpMatrix = lWorld;
}

// ---------------------------------------------------------------------------
// TransformSpace
//
// Re-express a point from a source reference space into a destination reference
// space. The point is first carried from the source space into world space via
// the source space->world affine (lu8SrcA), then from world space into the
// destination space via the world->destination matrix (the inverse built by
// GetWorldTransformationMatrix for lu8DstA). The point is transformed in place.
//
// FLAG (B-space args unused on this path): the declared signature carries a
// source-B and destination-B space (lu8SrcB / lu8DstB) for the blended six-space
// TransformToWorld overload. This path uses only the single-space space->world
// affine (lu8SrcA) and the single-space world->space matrix (lu8DstA); lu8SrcB /
// lu8DstB are accepted (the eye/look switch callers pass the resolved B spaces)
// but not consumed here -- the build's body reads only the A space for each of the
// two transforms. Marked unused to keep the call shape the callers rely on.
// ---------------------------------------------------------------------------
void ICEAuthor::TransformSpace(f32* lpPoint, u8 lu8SrcA, u8 /*lu8SrcB*/,
                               u8 lu8DstA, u8 /*lu8DstB*/) const
{
    // Source space -> world. Identity-seed the matrix (overwritten by the affine
    // splice), fetch the source space->world affine, and project the point with
    // its w lane forced to 0; TransformPoint then adds the row3 translation
    // unconditionally (the affine point transform).
    Matrix4 lSrcToWorld;
    ICEMath::Identity(&lSrcToWorld);
    const Matrix44Affine lToWorld =
        mpCameraSpaceHandler->GetTransformToWorld((eICESpace)lu8SrcA);
    AffineToMatrix4(&lSrcToWorld, lToWorld);

    lpPoint[3] = 0.0f;
    TransformPoint(lpPoint, lSrcToWorld);

    // World -> destination space (the inverse the destination space's
    // GetWorldTransformationMatrix yields), then project the world point again,
    // with its w lane again forced to 0 (TransformPoint adds row3 unconditionally).
    Matrix4 lWorldToDst;
    GetWorldTransformationMatrix(&lWorldToDst, lu8DstA);

    lpPoint[3] = 0.0f;
    TransformPoint(lpPoint, lWorldToDst);
}

// ---------------------------------------------------------------------------
// File-local: bracket key of the current channel's current interval.
//
// Resolve the start-side bracket key of interval lu16Interval on the current
// channel: interval 0 -> key 0, an interior interval -> its stored boundary key,
// the final interval -> the last key. (The same start-side bracket the take's
// GetIntervalKey resolves; used by the eye/look space switches to find the two
// keys -- the bracket key and the next key -- whose stored position is moved.)
// ---------------------------------------------------------------------------
static u16 BracketKey(ICETake& lrTake, s32 liChannel, u16 lu16Interval)
{
    if (lu16Interval == 0)
    {
        return 0;
    }
    return (u16)lrTake.GetIntervalKey(liChannel, lu16Interval);
}

// ---------------------------------------------------------------------------
// File-local: re-project one position key and write its x/y/z back.
//
// Reads the three position-element values (the x/y/z element indices liElemX/Y/Z)
// at key lu16Key on the current channel, packs them into a homogeneous point,
// re-expresses it from (liSrcA, liSrcB) into (liDstA, liDstB) via TransformSpace,
// and writes each transformed component back into its element at the same key. The
// per-key bounds check the build emits before each write is preserved as a
// CGS_ASSERT on the element being inside the channel's key range.
// ---------------------------------------------------------------------------
static void SwitchPositionKey(ICEAuthor& lrAuthor, ICETake& lrTake, s32 liChannel,
                              u16 lu16Key, s32 liElemX, s32 liElemY, s32 liElemZ,
                              s32 liSrcA, s32 liSrcB, s32 liDstA, s32 liDstB)
{
    f32 lafPoint[4];
    lafPoint[0] = lrTake.GetValueFloat(liElemX, lu16Key);
    lafPoint[1] = lrTake.GetValueFloat(liElemY, lu16Key);
    lafPoint[2] = lrTake.GetValueFloat(liElemZ, lu16Key);
    lafPoint[3] = 0.0f;

    lrAuthor.TransformSpace(lafPoint, (u8)liSrcA, (u8)liSrcB, (u8)liDstA, (u8)liDstB);

    CGS_ASSERT(lu16Key < lrTake.GetNumKeys(liChannel),
        "lu16Index < ( lbIsKey ? lChannel.GetNumKeys() : lChannel.GetNumIntervals())");
    lrTake.SetValue(liElemX, lu16Key, ICEValue(lafPoint[0]));
    lrTake.SetValue(liElemY, lu16Key, ICEValue(lafPoint[1]));
    lrTake.SetValue(liElemZ, lu16Key, ICEValue(lafPoint[2]));
}

// ---------------------------------------------------------------------------
// File-local: re-express a position channel's interval boundary into new spaces.
//
// Shared body of SwitchEyeSpace / SwitchLookSpace. The eye/look position is stored
// across three coordinate elements (liElemX/Y/Z) plus a pair of space-index
// elements (liSpaceElemA -> current A space, liSpaceElemB -> current B space). The
// position is moved only when its current spaces differ from the requested target
// pair. Both keys bracketing interval lu16Interval (the bracket key and the next
// key) are re-projected from the current (srcA, srcB) spaces into (liTargetA,
// liTargetB).
// ---------------------------------------------------------------------------
static void SwitchPositionSpace(ICEAuthor& lrAuthor, ICETake& lrTake, s32 liChannel,
                                u16 lu16Interval, s32 liTargetA, s32 liTargetB,
                                s32 liElemX, s32 liElemY, s32 liElemZ,
                                s32 liSpaceElemA, s32 liSpaceElemB)
{
    // Current source spaces for this interval's bracket.
    const s32 liSrcA = lrTake.GetValueInt(liSpaceElemA, lu16Interval);
    const s32 liSrcB = lrTake.GetValueInt(liSpaceElemB, lu16Interval);

    // Nothing to do when the position already lives in the requested space pair.
    if (liSrcA == liTargetA && liSrcB == liTargetB)
    {
        return;
    }

    const u16 lu16KeyA = BracketKey(lrTake, liChannel, lu16Interval);
    const u16 lu16KeyB = (u16)(lu16KeyA + 1);

    SwitchPositionKey(lrAuthor, lrTake, liChannel, lu16KeyA,
                      liElemX, liElemY, liElemZ, liSrcA, liSrcB, liTargetA, liTargetB);
    SwitchPositionKey(lrAuthor, lrTake, liChannel, lu16KeyB,
                      liElemX, liElemY, liElemZ, liSrcA, liSrcB, liTargetA, liTargetB);
}

// ---------------------------------------------------------------------------
// SwitchEyeSpace
//
// Re-express the camera EYE position of interval lu16Interval into the (liSpaceA,
// liSpaceB) space pair. The eye position is the x/y/z elements 0/1/2; its current
// source spaces are read from the eye space-index elements 30 (A) and 32 (B). When
// the current pair already matches the requested pair the call is a no-op.
// ---------------------------------------------------------------------------
void ICEAuthor::SwitchEyeSpace(u16 lu16Interval, s32 liSpaceA, s32 liSpaceB)
{
    static const s32 KI_EYE_X        = 0;
    static const s32 KI_EYE_Y        = 1;
    static const s32 KI_EYE_Z        = 2;
    static const s32 KI_EYE_SPACE_A  = 30;
    static const s32 KI_EYE_SPACE_B  = 32;

    SwitchPositionSpace(*this, mEditTake, miCurrentChannel, lu16Interval,
                        liSpaceA, liSpaceB,
                        KI_EYE_X, KI_EYE_Y, KI_EYE_Z, KI_EYE_SPACE_A, KI_EYE_SPACE_B);
}

// ---------------------------------------------------------------------------
// SwitchLookSpace
//
// Re-express the camera LOOK-AT position of interval lu16Interval into the
// (liSpaceA, liSpaceB) space pair. The look position is the x/y/z elements 3/4/5;
// its current source spaces are read from the look space-index elements 31 (A) and
// 33 (B). Same early-out and two-key re-projection as SwitchEyeSpace.
// ---------------------------------------------------------------------------
void ICEAuthor::SwitchLookSpace(u16 lu16Interval, s32 liSpaceA, s32 liSpaceB)
{
    static const s32 KI_LOOK_X       = 3;
    static const s32 KI_LOOK_Y       = 4;
    static const s32 KI_LOOK_Z       = 5;
    static const s32 KI_LOOK_SPACE_A = 31;
    static const s32 KI_LOOK_SPACE_B = 33;

    SwitchPositionSpace(*this, mEditTake, miCurrentChannel, lu16Interval,
                        liSpaceA, liSpaceB,
                        KI_LOOK_X, KI_LOOK_Y, KI_LOOK_Z, KI_LOOK_SPACE_A, KI_LOOK_SPACE_B);
}

// ---------------------------------------------------------------------------
// AdvanceAnimation
//
// Step the take playback parameter forward by lfTimeStep, scaled by the take
// length (so a fixed real-time step advances the unit-interval parameter by the
// fraction of the take it covers). The advance only runs while the take has a
// positive length. Returns true while the parameter is still below the high play-
// range bound; once it reaches the bound the parameter is snapped to the low
// range bound (loop wrap) and the function returns false.
// ---------------------------------------------------------------------------
bool ICEAuthor::AdvanceAnimation(f32 lfTimeStep)
{
    if (mEditTake.GetData() != 0 && mEditTake.GetLength() > 0.0f)
    {
        const f32 lfLength = mEditTake.GetLength();
        mEditTake.SetParameter((lfTimeStep / lfLength) + mEditTake.GetParameter(), false, false);
    }

    if (mEditTake.GetParameter() < mfRangeHi)
    {
        return true;
    }

    mEditTake.SetParameter(mfRangeLo, false, false);
    return false;
}

// ---------------------------------------------------------------------------
// AdvanceAssemblyAnimation
//
// Assembly-edit variant of AdvanceAnimation. The time step is scaled by the same
// take-length factor AND by the active sub-range width (the zoom window when the
// editor is in the assembly-mark state, otherwise the play range) divided by the
// take length, giving the speed at which the assembly sub-range plays back. The
// sub-range pair is the zoom window (mfZoomLo/mfZoomHi) while miState ==
// ASSEMBLY_MARK, else the play range (mfRangeLo/mfRangeHi). With no valid take
// length the speed factor defaults to 1. Same high-bound test and low-bound wrap
// as AdvanceAnimation.
// ---------------------------------------------------------------------------
bool ICEAuthor::AdvanceAssemblyAnimation(f32 lfTimeStep)
{
    // Active sub-range: the zoom window in the assembly-mark state, else the play
    // range. (mfZoomLo/mfZoomHi and mfRangeLo/mfRangeHi are adjacent 2-float
    // windows; the build selects the zoom pair when miState == ASSEMBLY_MARK.)
    const f32 lfRangeLo = (miState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK) ? mfZoomLo : mfRangeLo;
    const f32 lfRangeHi = (miState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK) ? mfZoomHi : mfRangeHi;

    f32 lfSpeed = 1.0f;
    if (mEditTake.GetData() != 0 && mEditTake.GetLength() > 0.0f)
    {
        const f32 lfLength = mEditTake.GetLength();
        lfSpeed = (lfRangeHi - lfRangeLo) / lfLength;
    }

    mEditTake.SetParameter((lfSpeed * lfTimeStep) + mEditTake.GetParameter(), false, false);

    if (mEditTake.GetParameter() < mfRangeHi)
    {
        return true;
    }

    mEditTake.SetParameter(mfRangeLo, false, false);
    return false;
}

} // namespace ICE
