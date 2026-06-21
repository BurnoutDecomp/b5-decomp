#ifndef GAMESOURCE_DIRECTOR_CAMERA_ICECAMERAMOVER_H
#define GAMESOURCE_DIRECTOR_CAMERA_ICECAMERAMOVER_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                       // rw::math::vpu::Vector3 / Matrix44Affine
#include "SDKs/Packages/ICE/ICEPoint.hpp"            // ICE::Cubic3D (mAccelOffset / mForward by value)
#include "SDKs/Packages/ICE/ICEMath.hpp"             // ICE::Matrix4 / ICE::Vector3 / ICE::Angle
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp" // ICE::CameraSpaceHandler (ICECameraAnchor mSpace)
#include "SDKs/Packages/ICE/ICECamera.hpp"           // ICE::ICECamera (mpICECamera target)
#include "SDKs/Packages/ICE/ICEData.hpp"             // ICE::ICETake (mpTake; the take it drives)

// ============================================================================
// GameSource/Director/Camera/ICECameraMover.h
//
// ICE::ICECameraMover -- the runtime that drives the in-game (ICE) camera from a
// recorded camera take. Each frame it samples the active ICETake's per-element
// channels (eye/look position, lens, focus, fade, overlay, bloom, sim-time scale),
// blends them through cubic followers, builds the world-to-camera transform, and
// pushes the result into the ICE camera. It anchors against an ICECameraAnchor (the
// car's reference space) and remembers per-frame hysteresis state (last hard-cut
// interval, last overlay, last event tag).
//
// THIS FILE IS THE ICECameraMover HOME. BrnDirector::ICEWrapper embeds one of these
// by value (mCameraMover) and #includes this header; the runtime bodies live in the
// mirrored TU SDKs/Packages/ICE/ICECameraMover.cpp. The reconstruction reference
// declares the type in SDKs/Packages/ICE/ICECameraMover.hpp; this is the canonical
// reconstructed home the consumers include.
//
// ----------------------------------------------------------------------------
// LAYOUT (member NAMES/TYPES from the reconstruction reference; OFFSETS/ORDER pinned
// from the reconstructed functions' member accesses -- Construct's two 132-byte cubic
// blocks + the +0x110/+0x120/+0x160/+0x170/+0x180.. stores, and the per-frame Update*
// readers). Members are accessed BY NAME; the offsets are recorded for provenance,
// never used as casts.
//
//   +0x000  ICECameraAnchor*  mpCar             the car/reference-space anchor
//   +0x004  ICECamera*        mpICECamera       the ICE camera this mover drives
//   +0x008  Cubic3D           mAccelOffset      accel-offset follower (132 bytes)
//   +0x08C  Cubic3D           mForward          forward-vector follower (132 bytes)
//   +0x110  ICETake*          mpTake            the active camera take
//   +0x120  Matrix4           mWorldToCamera    the built world->camera transform (64B)
//   +0x160  Vector3           mBungeeCarPos     snapped car position (hard-cut bungee)
//   +0x170  Vector3           mBungeeCarFwd     snapped car forward  (hard-cut bungee)
//   +0x180  s32               miHardCutInterval last hard-cut interval seen
//   +0x184  f32               mfSimTime         per-frame sim-time scale (0..1)
//   +0x188  u32               muOldTag          last event tag / cached per-frame value
//   +0x18C  s32               miOldOverlay      last overlay id seen
//
// (The 12-byte gap +0x114..+0x11F after mpTake is the alignment pad before the
// 16-aligned mWorldToCamera; no member is attested there.)
// ============================================================================

namespace ICE
{

// ICEGroup -- the shake-group context handed to Construct. Pointer-only here (the
// mover stores nothing through it in this TU); no reconstructed home yet, so it is
// forward-declared. Self-corrects when the ICEGroup TU lands.
struct ICEGroup;

// ----------------------------------------------------------------------------
// ICE::ICECameraAnchor (reconstruction reference ICECameraMover.hpp:62). The reference
// space the mover anchors against -- it wraps a CameraSpaceHandler (the eight take
// reference-space matrices) and exposes the car's geometry position / forward /
// velocity / acceleration derived from it.
//
// Only the geometry-position and forward-vector accessors are touched by this TU
// (UpdateForwardVector / UpdateHardCuts read the car's forward + position), so the
// rest of the method set is DECLARATION-ONLY here; bodies land with the anchor's own
// TU (mSpace's mCarToWorld provides the reads at mpCar+0x20 / mpCar+0x30 -- the zAxis
// (forward) and wAxis (position) of the car-to-world affine).
// ----------------------------------------------------------------------------
struct ICECameraAnchor
{
public:
    void Construct();
    void Destruct();

    const CameraSpaceHandler& GetSpace() const;
    void SetAnchor(const CameraSpaceHandler& lrSpace);

    // Car geometry derived from mSpace.mCarToWorld (forward == zAxis, position ==
    // wAxis). Used by the mover; declaration-only here.
    Vector3 GetGeometryPosition();
    Vector3 GetForwardVector();
    Matrix4 GetGeometryOrientation() const;
    Vector3 GetVelocity();
    Vector3 GetAcceleration();
    f32     GetVelocityMagnitude();

private:
    // @0x00  The eight take reference-space transforms (mCarToWorld first); the
    // mover reads its mCarToWorld zAxis/wAxis (mpCar+0x20 / mpCar+0x30).
    CameraSpaceHandler mSpace;
};

// ----------------------------------------------------------------------------
// ICE::ICECameraMover -- drives the ICE camera from the active take (see file head).
// ----------------------------------------------------------------------------
class ICECameraMover
{
public:
    // Default constructor (embedded by value in BrnDirector::ICEWrapper). Clears the
    // pointers + scalar hysteresis and leaves the cubic followers / transform to their
    // members' default-init; the real per-frame state is set by Construct. Body in the
    // sibling TU GameSource/Director/Camera/ICECameraMover.cpp.
    ICECameraMover();

    // Rebuild the mover from a camera take. The real recovered shape (RECONCILED from
    // the earlier guessed `Construct(bool, CameraSpaceHandler*, ICECamera*, ICETake*,
    // s32, s32)` placeholder, which the reconstructed body disproves): the first param
    // is the view index,
    // the second is the car ANCHOR (stored at +0x00), the third the ICE camera (+0x04),
    // the fourth the take (+0x110); the shake-group and resource-manager are the take
    // pipeline context. Construct seeds the two cubic followers to identity-ish, marks
    // the hard-cut interval invalid (-1), zeroes the bungee/old-tag state, sets
    // mfSimTime to 1.0, flags the camera overlay-enabled, and snaps the bungee
    // position/forward to the anchor's current car-to-world.
    void Construct(s32 liViewIndex, ICECameraAnchor* lpCar, ICECamera* lpICECamera,
                   ICETake* lpTake, ICEGroup* lpShakeGroup,
                   const IResourceManager* lpResourceMgr);

    void Destruct();

    // Per-frame driver entry points (each declared with the frame time-step; some
    // helpers ignore it). Update / UpdateFrameBegin are sibling-TU entry points kept
    // declaration-only here (not in this TU).
    void Update(f32 lfTimeStep);
    void UpdateFrameBegin(f32 lfTimeStep);
    // Finish the per-frame update: while a take is bound, refresh the transform /
    // forward / lens / focus / hard-cuts / fade / overlay / bloom, then push the
    // world->camera matrix into the ICE camera. (Consumer: ICEWrapper::Update.)
    void UpdateFrameEnd(f32 lfTimeStep);

    ICECameraAnchor* GetAnchor() { return mpCar; }

    // Point the mover at the take it should drive (the manager's active take). The ICE
    // wrapper sets it after starting a movie; Update reads it back.
    void     SetTake(ICETake* lpTake) { mpTake = lpTake; }
    ICETake* GetTake() const          { return mpTake; }

    // Advance the per-frame sim-time scale from the take's sim-time channel, clamp it
    // into [0,1], store it, and push it into the ICE camera. (Consumer: ICEWrapper::Update.)
    void UpdateSimTime(f32 lfTimeStep);

    // Cache the active take's per-frame event-tag value (channel 41). The consumer
    // (ICEWrapper::Update) samples the channel and hands it here; UpdateEventTag (its
    // own TU) reads it back. Sets muOldTag (+0x188).
    void SetCurrentTakeValueInt(s32 liValue) { muOldTag = static_cast<u32>(liValue); }

private:
    // --- The per-frame take->camera pipeline reconstructed in this TU ---
    void UpdateTransformationMatrix(f32 lfTimeStep);
    void UpdateForwardVector(f32 lfTimeStep);
    void UpdateLens(f32 lfTimeStep);
    void UpdateFocus(f32 lfTimeStep);
    void UpdateHardCuts(f32 lfTimeStep);
    void UpdateFade(f32 lfTimeStep);
    void UpdateOverlay(f32 lfTimeStep);
    void UpdateBloom(f32 lfTimeStep);

    // --- Sibling-TU helpers reached by name (declaration-only; bodies land with the
    //     follow-on ICECameraMover work). Kept here so the type's method set matches
    //     the reconstruction reference; the per-TU `cl /c` gate does not link. ---
    void UpdateScreenshots(f32 lfTimeStep);
    void UpdateEventTag(f32 lfTimeStep);
    void UpdateLetterBox(f32 lfTimeStep);
    void UpdateConstraints(f32 lfTimeStep);
    void UpdateAccelOffset(f32 lfTimeStep, Vector3* lpAccelOffset);
    void CreateLookAtMatrix(Matrix4* lpMatrix, Vector3& lrEye, Vector3& lrCenter, Angle leDutch);
    Vector3 TransformToWorld(Vector3 lvPosition, u8 lu8Space, u8 lu8Avatar,
                             u8 lu8Lag, u8 lu8Bungee, f32 lfBlend);

    // --- Layout (see file head) ---
    ICECameraAnchor* mpCar;             // +0x000
    ICECamera*       mpICECamera;       // +0x004
    Cubic3D          mAccelOffset;      // +0x008
    Cubic3D          mForward;          // +0x08C
    ICETake*         mpTake;            // +0x110
    u8               mPadToMatrix[0x120 - (0x110 + 4)]; // align mWorldToCamera to 16
    Matrix4          mWorldToCamera;    // +0x120
    Vector3          mBungeeCarPos;     // +0x160
    Vector3          mBungeeCarFwd;     // +0x170
    s32              miHardCutInterval; // +0x180
    f32              mfSimTime;         // +0x184
    u32              muOldTag;          // +0x188
    s32              miOldOverlay;      // +0x18C
};

} // namespace ICE

#endif // GAMESOURCE_DIRECTOR_CAMERA_ICECAMERAMOVER_H
