#pragma once

#include "types.hpp"

namespace ICE
{
struct ICETake;   // ICETake* parameter of Construct (home: SDKs/Packages/ICE/ICEData.hpp)
struct CameraSpaceHandler; // Construct input block A (home: SDKs/Packages/ICE/ICECameraSpaceHandler.hpp)
struct ICECamera;          // Construct input block B (home: SDKs/Packages/ICE/ICECamera.hpp)

class ICECameraMover
{
public:
    ICECameraMover();

    // Rebuild the mover from the active camera take. Called by
    // BrnDirector::ICEWrapper::ReconstructCameraMover with the arg shape
    // (this, flag=1, &mCameraSpaceHandler, &mICECamera, take, null=0, context):
    // RECONCILED from the earlier void* input-block model -- the wrapper's two
    // by-address arguments are its per-frame reference-space cache (input block A,
    // +0x11ED0) and its ICE camera (input block B, +0x11D60).
    // DECLARATION-ONLY here: the body is a separate, not-yet-reconstructed TU
    // (ICE::ICECameraMover::Construct), so the /c gate only needs this declaration.
    // FLAG: lbReset and liExtra (the literal 1 and 0 in the call) carry their literal
    // roles; the pointee types self-correct when ICECameraMover::Construct is recovered.
    void Construct(bool lbReset, CameraSpaceHandler* lpSpaceHandler, ICECamera* lpICECamera,
                   ICETake* lpCameraTake, s32 liExtra, s32 liContext);

    // ------------------------------------------------------------------------
    // FLAG: minimal-slice driver methods BrnDirector::ICEWrapper::Update / PlayMovie
    // invoke by name. DECLARATION-ONLY (their bodies land with the ICECameraMover TU;
    // the per-TU `cl /c` gate does not link). Replace/grow with the real ICECameraMover
    // method set when that TU is reconstructed; the NAMES are stable.
    // ------------------------------------------------------------------------
    // Point the mover at the take it should drive this playback (the manager's active
    // take). PlayMovie sets it after starting a movie; Update reads it back.
    void     SetTake(ICETake* lpTake);
    ICETake* GetTake() const;
    // Advance the mover's blended position by the given sim-time delta.
    void UpdateSimTime(f32 lfSimTime);
    // Finish the mover's per-frame update with the given frame-time delta.
    void UpdateFrameEnd(f32 lfFrameTime);
    // Cache the active take's per-frame integer value (channel 41) the mover reads.
    void SetCurrentTakeValueInt(s32 liValue);

private:
    struct CameraMoveChannel
    {
        f32 mfPositionX;
        f32 mfPositionY;
        f32 mfPositionZ;
        f32 mfVelocityX;
        f32 mfVelocityY;
        f32 mfVelocityZ;
        f32 mfAccelerationX;
        f32 mfAccelerationY;
        f32 mfBlend;
        f32 mfWeight;
        u16 mu16Flags;
        u16 mu16Enabled;
    };

    u8                mPad0[8];
    CameraMoveChannel maChannels[6];
};
}
