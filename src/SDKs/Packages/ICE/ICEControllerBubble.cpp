// ============================================================================
// SDKs/Packages/ICE/ICEControllerBubble.cpp
//
// ICE::ICEController -- the SERIALIZE ("bubble") group of the in-game ICE
// camera-take editor: ToBubble and FromBubble.
//
// The "bubble" is a yaw/pitch/radius SPHERICAL encoding of the camera look
// direction the editor's on-screen bubble cursor drives. A look direction is the
// difference between two world-space points the take stores per element-channel
// (a "from" point and a "to" point), each expressed in its own ICE reference
// space and projected to world through the camera-space handler.
//
//   * ToBubble   : world "from"/"to" points -> direction vector D ->
//                  (mi16BubbleYaw, mi16BubblePitch, mfBubbleRadius).
//                  yaw   = atan2(D.x, D.z)             (heading in the x/z plane)
//                  pitch = atan2(D.y, |D.xz|)          (elevation above the plane)
//                  radius= |D|                         (full 3D length)
//   * FromBubble : (yaw, pitch, radius) -> direction vector D (spherical->cartesian)
//                  -> add/subtract onto the world anchor point -> back into the
//                  per-element take values.
//
// ToBubble / FromBubble are declaration-only in ICEController.hpp; this TU bodies
// them. Member access is BY NAME against the layout pinned in ICEController.hpp
// (the +0xNN offsets in the comments are the source-spine facts, not host byte
// offsets -- the embedded ICETake/widgets are wider on the 64-bit host).
//
// FLAG (ICEAuthor base): the controller IS-AN ICEAuthor and the same `this`. The
// author members/methods used here (mpCameraSpaceHandler, IsEndKeyState,
// GetWorldTransformationMatrix, SetKeyElementValue) are declared on ICEAuthor, so
// they are reached through reinterpret_cast<ICE::ICEAuthor*>(this). When
// ICEController grows its real `: public ICEAuthor` base these casts become plain
// base accesses.
//
// The take value reads/writes go through the embedded edit take (mEditTake, the
// +0x28 anchor): the per-element value getter is ICETake::GetValueFloat(channel,
// key); the per-element reference-space selector is ICETake::GetValueInt(elem).
//
// The two functions are kept self-contained: the shared key-resolve prologue and
// the point/vector transforms are TU-local free helpers (NOT new class members --
// the header is frozen), so nothing is added to ICEController.hpp.
// ============================================================================

#include "SDKs/Packages/ICE/ICEController.hpp"
#include "SDKs/Packages/ICE/ICEAuthor.hpp"               // ICEAuthor base members/methods (via reinterpret_cast<this>)
#include "SDKs/Packages/ICE/ICEData.hpp"                 // ICETake value/interval accessors
#include "SDKs/Packages/ICE/ICEMath.hpp"                 // ICEMath::ATan / SinCos / Identity / Sqrt, Vector4, Matrix4
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"   // CameraSpaceHandler::GetTransformToWorld
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace ICE
{

namespace
{
    // -----------------------------------------------------------------------
    // Resolve the take key the bubble reads/writes for the editor's current
    // channel and state.
    //
    // The take stores per-channel interval brackets; the bubble works on the key
    // at the current interval boundary, biased to the END key for the states that
    // edit the end of a key (SELECT_COPY_MODE asks the author whether the current
    // key is an end key; END_KEY_SELECTED / EDIT_END_KEY / ASSEMBLY_EDIT_END are
    // always end).
    //
    // source-spine inline walk (reproduced by named accessors):
    //   interval = mEditTake.GetCurrentInterval(channel)
    //   key      = (interval == 0) ? 0 : mEditTake.GetIntervalKey(channel, interval)
    //   key     += endKeyBias(state)
    // The interval-key table read is exactly ICETake::GetIntervalKey(channel,
    // interval) (the bracket key for the interval).
    // -----------------------------------------------------------------------
    u16 ResolveBubbleKey(const ICEController* lpController)
    {
        // GetEditTake() returns &mEditTake (the +0x28 embedded edit take the bubble
        // reads). const-qualified access via the controller's accessor.
        const ICETake& lrTake = *const_cast<ICEController*>(lpController)->GetEditTake();
        const s32 liChannel = lpController->miCurrentChannel;          // +0xEA0

        const u16 lu16Interval = lrTake.GetCurrentInterval(liChannel);
        u16 lu16Key = 0;
        if (lu16Interval != 0)
        {
            lu16Key = lrTake.GetIntervalKey(liChannel, lu16Interval);
        }

        s32 liEndKey = 0;
        if (lpController->miState == E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)   // 16
        {
            // FLAG (ICEAuthor base): IsEndKeyState is an author method on this `this`.
            // miStateArg (+0xE9C) is the key index the copy-mode path disambiguates with.
            liEndKey =
                reinterpret_cast<const ICEAuthor*>(lpController)->IsEndKeyState(lpController->miStateArg)
                    ? 1 : 0;
        }
        else if (lpController->miState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED    // 13
              || lpController->miState == E_ICE_AUTHOR_STATE_EDIT_END_KEY        // 15
              || lpController->miState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END)  // 18
        {
            liEndKey = 1;
        }

        return static_cast<u16>(lu16Key + static_cast<u16>(liEndKey));
    }

    // Transform a point (w == 0) by a reference-space->world affine: row-major
    // 4x4 * column point, the standard rows[0]*x + rows[1]*y + rows[2]*z + rows[3]
    // multiply-accumulate the source-spine builds, with the w lane forced to
    // 1 afterwards (the affine translation row).
    Vector4 TransformPointAffine(const Matrix44Affine& lrMatrix, const Vector4& lvPoint)
    {
        Vector4 lvOut;
        lvOut.x = lrMatrix.xAxis.x * lvPoint.x + lrMatrix.yAxis.x * lvPoint.y
                + lrMatrix.zAxis.x * lvPoint.z + lrMatrix.wAxis.x;
        lvOut.y = lrMatrix.xAxis.y * lvPoint.x + lrMatrix.yAxis.y * lvPoint.y
                + lrMatrix.zAxis.y * lvPoint.z + lrMatrix.wAxis.y;
        lvOut.z = lrMatrix.xAxis.z * lvPoint.x + lrMatrix.yAxis.z * lvPoint.y
                + lrMatrix.zAxis.z * lvPoint.z + lrMatrix.wAxis.z;
        lvOut.w = 1.0f;
        return lvOut;
    }

    // Same point transform against the ICE Matrix4 (the destination-orientation
    // scratch ICEAuthor::GetWorldTransformationMatrix fills).
    Vector4 TransformPointMatrix4(const Matrix4& lrMatrix, const Vector4& lvPoint)
    {
        const Vector4* lpRows = lrMatrix.maRows;
        Vector4 lvOut;
        lvOut.x = lpRows[0].x * lvPoint.x + lpRows[1].x * lvPoint.y
                + lpRows[2].x * lvPoint.z + lpRows[3].x;
        lvOut.y = lpRows[0].y * lvPoint.x + lpRows[1].y * lvPoint.y
                + lpRows[2].y * lvPoint.z + lpRows[3].y;
        lvOut.z = lpRows[0].z * lvPoint.x + lpRows[1].z * lvPoint.y
                + lpRows[2].z * lvPoint.z + lpRows[3].z;
        lvOut.w = 1.0f;
        return lvOut;
    }
}

// ---------------------------------------------------------------------------
// ToBubble
//
// Encode the current look direction into (yaw, pitch, radius).
//
// 1. Read the "from" point (elements 0,1,2) and the "to" point (elements 3,4,5)
//    out of the edit take at the resolved key. w lanes are 0.
// 2. Project each through the camera-space handler to world: the "from" point in
//    the space ICETake::GetValueInt(elem 30) names, the "to" point in the space
//    element 31 names.
// 3. D = worldFrom - worldTo.
// 4. yaw    = atan2(D.x, D.z).
//    The horizontal (D.x, D.z) projection is normalised; its length feeds
//    pitch = atan2(D.y, |horizontal|).
// 5. radius = |D| (full 3D length of the difference vector).
//
// The source-spine builds the two transforms and the difference with the
// 4x4 multiply-accumulate transform idiom, and the planar-length / radius with
// vrsqrtefp + two Newton-Raphson refine steps; here the same quantities are
// computed through the named ICEMath / handler surface.
// ---------------------------------------------------------------------------
void ICEController::ToBubble()
{
    const u16 lu16Key = ResolveBubbleKey(this);

    // The two take points (w == 0). Element value getter is the keyed GetValueFloat.
    Vector4 lvFrom;
    lvFrom.x = mEditTake.GetValueFloat(0, lu16Key);
    lvFrom.y = mEditTake.GetValueFloat(1, lu16Key);
    lvFrom.z = mEditTake.GetValueFloat(2, lu16Key);
    lvFrom.w = 0.0f;

    Vector4 lvTo;
    lvTo.x = mEditTake.GetValueFloat(3, lu16Key);
    lvTo.y = mEditTake.GetValueFloat(4, lu16Key);
    lvTo.z = mEditTake.GetValueFloat(5, lu16Key);
    lvTo.w = 0.0f;

    // Reference spaces: element 30 names the "from" space, element 31 the "to" space.
    const u8 lu8FromSpace = static_cast<u8>(mEditTake.GetValueInt(30));
    const u8 lu8ToSpace   = static_cast<u8>(mEditTake.GetValueInt(31));

    // FLAG (ICEAuthor base): mpCameraSpaceHandler (+0x08) is an author-base member;
    // reached through the author view of this `this`.
    const ICEAuthor* lpAuthor = reinterpret_cast<const ICEAuthor*>(this);
    const Matrix44Affine lFromToWorld =
        lpAuthor->mpCameraSpaceHandler->GetTransformToWorld(static_cast<eICESpace>(lu8FromSpace));
    const Matrix44Affine lToToWorld =
        lpAuthor->mpCameraSpaceHandler->GetTransformToWorld(static_cast<eICESpace>(lu8ToSpace));

    // Project both points and take the world-space difference (the look direction).
    const Vector4 lvWorldFrom = TransformPointAffine(lFromToWorld, lvFrom);
    const Vector4 lvWorldTo   = TransformPointAffine(lToToWorld, lvTo);
    const f32 lfDx = lvWorldFrom.x - lvWorldTo.x;
    const f32 lfDy = lvWorldFrom.y - lvWorldTo.y;
    const f32 lfDz = lvWorldFrom.z - lvWorldTo.z;

    // Heading: yaw = atan2(D.x, D.z). ICEMath::ATan(f32 lfY, f32 lfX) returns
    // atan2(lfY, lfX), so the heading is ATan(lfDx, lfDz) (lfY = D.x, lfX = D.z).
    mi16BubbleYaw = static_cast<s16>(static_cast<u16>(ICEMath::ATan(lfDx, lfDz)));

    // Elevation: the horizontal (x, z) projection's length is the planar component,
    // and pitch = atan2(D.y, horizontalLength).
    const f32 lfHorizSq  = (lfDx * lfDx) + (lfDz * lfDz);
    const f32 lfHorizLen = (lfHorizSq > 0.0f) ? ICEMath::Sqrt(lfHorizSq) : 0.0f;
    mi16BubblePitch = static_cast<s16>(static_cast<u16>(ICEMath::ATan(lfDy, lfHorizLen)));

    // Radius: the full 3D length of the difference vector (0 when degenerate).
    const f32 lfLenSq = (lfDx * lfDx) + (lfDy * lfDy) + (lfDz * lfDz);
    mfBubbleRadius = (lfLenSq > 0.0f) ? ICEMath::Sqrt(lfLenSq) : 0.0f;
}

// ---------------------------------------------------------------------------
// FromBubble
//
// Decode (yaw, pitch, radius) back into the per-element take values -- the inverse
// of ToBubble.
//
// Spherical -> cartesian (the source-spine SinCos pair + the three fmuls):
//   D.x = radius * cos(pitch) * sin(yaw)
//   D.y = radius * sin(pitch)
//   D.z = radius * cos(pitch) * cos(yaw)
//   D.w = 0
//
// Then, depending on the "edit orientation" flag (miFlagOrient, +0x7251):
//   * orient set : anchor = world("to" point, elements 3,4,5 in space elem 31);
//                  target = anchor + D; express target in the destination
//                  orientation (space elem 30) and write elements 0,1,2.
//   * orient clear: anchor = world("from" point, elements 0,1,2 in space elem 30);
//                  target = anchor - D; express in the destination orientation
//                  (space elem 31) and write elements 3,4,5.
// The destination orientation matrix is built by GetWorldTransformationMatrix,
// which overwrites the identity scratch seeded first. (The source-spine reads one
// extra element per branch -- 32 / 33 -- and discards it.)
// ---------------------------------------------------------------------------
void ICEController::FromBubble()
{
    // Spherical -> cartesian. ICEMath::SinCos(f32* sinOut, f32* cosOut, Angle).
    f32 lfSinYaw, lfCosYaw;
    ICEMath::SinCos(&lfSinYaw, &lfCosYaw, Angle(static_cast<u16>(mi16BubbleYaw)));
    f32 lfSinPitch, lfCosPitch;
    ICEMath::SinCos(&lfSinPitch, &lfCosPitch, Angle(static_cast<u16>(mi16BubblePitch)));

    const f32 lfRadiusCosPitch = mfBubbleRadius * lfCosPitch;
    Vector4 lvBubbleDir;
    lvBubbleDir.x = lfRadiusCosPitch * lfSinYaw;
    lvBubbleDir.y = mfBubbleRadius   * lfSinPitch;
    lvBubbleDir.z = lfRadiusCosPitch * lfCosYaw;
    lvBubbleDir.w = 0.0f;

    // Seed the destination-orientation scratch with identity (overwritten below).
    Matrix4 lDestOrient;
    ICEMath::Identity(&lDestOrient);

    const u16 lu16Key = ResolveBubbleKey(this);

    // FLAG (ICEAuthor base): the author view of this `this` for the author members
    // and methods used below (mpCameraSpaceHandler, GetWorldTransformationMatrix,
    // SetKeyElementValue).
    ICEAuthor* lpAuthor = reinterpret_cast<ICEAuthor*>(this);

    if (miFlagOrient != 0)                                    // +0x7251 set
    {
        // Anchor = world "to" point (elements 3,4,5) in the space element 31 names.
        Vector4 lvAnchor;
        lvAnchor.x = mEditTake.GetValueFloat(3, lu16Key);
        lvAnchor.y = mEditTake.GetValueFloat(4, lu16Key);
        lvAnchor.z = mEditTake.GetValueFloat(5, lu16Key);
        lvAnchor.w = 0.0f;

        const u8 lu8AnchorSpace = static_cast<u8>(mEditTake.GetValueInt(31));
        const Matrix44Affine lAnchorToWorld =
            lpAuthor->mpCameraSpaceHandler->GetTransformToWorld(static_cast<eICESpace>(lu8AnchorSpace));
        const Vector4 lvWorldAnchor = TransformPointAffine(lAnchorToWorld, lvAnchor);

        // Destination orientation: element 30 (element 32 is read and discarded).
        (void)mEditTake.GetValueInt(32);
        const u8 lu8DestSpace = static_cast<u8>(mEditTake.GetValueInt(30));
        lpAuthor->GetWorldTransformationMatrix(&lDestOrient, lu8DestSpace);

        // target = anchor + D, expressed in the destination orientation.
        Vector4 lvTarget;
        lvTarget.x = lvWorldAnchor.x + lvBubbleDir.x;
        lvTarget.y = lvWorldAnchor.y + lvBubbleDir.y;
        lvTarget.z = lvWorldAnchor.z + lvBubbleDir.z;
        lvTarget.w = 0.0f;
        const Vector4 lvLocal = TransformPointMatrix4(lDestOrient, lvTarget);

        lpAuthor->SetKeyElementValue(0, lvLocal.x);
        lpAuthor->SetKeyElementValue(1, lvLocal.y);
        lpAuthor->SetKeyElementValue(2, lvLocal.z);
        return;
    }

    // orient clear: anchor = world "from" point (elements 0,1,2) in space element 30.
    Vector4 lvAnchor;
    lvAnchor.x = mEditTake.GetValueFloat(0, lu16Key);
    lvAnchor.y = mEditTake.GetValueFloat(1, lu16Key);
    lvAnchor.z = mEditTake.GetValueFloat(2, lu16Key);
    lvAnchor.w = 0.0f;

    const u8 lu8AnchorSpace = static_cast<u8>(mEditTake.GetValueInt(30));
    const Matrix44Affine lAnchorToWorld =
        lpAuthor->mpCameraSpaceHandler->GetTransformToWorld(static_cast<eICESpace>(lu8AnchorSpace));
    const Vector4 lvWorldAnchor = TransformPointAffine(lAnchorToWorld, lvAnchor);

    // Destination orientation: element 31 (element 33 is read and discarded).
    (void)mEditTake.GetValueInt(33);
    const u8 lu8DestSpace = static_cast<u8>(mEditTake.GetValueInt(31));
    lpAuthor->GetWorldTransformationMatrix(&lDestOrient, lu8DestSpace);

    // target = anchor - D, expressed in the destination orientation.
    Vector4 lvTarget;
    lvTarget.x = lvWorldAnchor.x - lvBubbleDir.x;
    lvTarget.y = lvWorldAnchor.y - lvBubbleDir.y;
    lvTarget.z = lvWorldAnchor.z - lvBubbleDir.z;
    lvTarget.w = 0.0f;
    const Vector4 lvLocal = TransformPointMatrix4(lDestOrient, lvTarget);

    lpAuthor->SetKeyElementValue(3, lvLocal.x);
    lpAuthor->SetKeyElementValue(4, lvLocal.y);
    lpAuthor->SetKeyElementValue(5, lvLocal.z);
}

} // namespace ICE
