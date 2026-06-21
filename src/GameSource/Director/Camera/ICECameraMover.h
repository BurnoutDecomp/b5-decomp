#pragma once

#include "types.hpp"

namespace ICE
{
struct ICETake;   // ICETake* parameter of Construct (home: SDKs/Packages/ICE/ICEData.hpp)

class ICECameraMover
{
public:
    ICECameraMover();

    // Rebuild the mover from the active camera take. Called by
    // BrnDirector::ICEWrapper::ReconstructCameraMover with the arg shape
    // (this, flag=1, &inputBlockB, &inputBlockA, take, null=0, context).
    // DECLARATION-ONLY here: the body is a separate, not-yet-reconstructed TU
    // (ICE::ICECameraMover::Construct), so the /c gate only needs this declaration.
    // FLAG: the two input-block parameters are typed void* -- their pointee types are
    // not recovered yet; the caller (ICEWrapper) holds them as opaque blocks. lbReset
    // and lpExtra (the literal 1 and 0 in the call) carry their literal roles.
    void Construct(bool lbReset, void* lpInputBlockB, void* lpInputBlockA,
                   ICETake* lpCameraTake, s32 liExtra, s32 liContext);

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
