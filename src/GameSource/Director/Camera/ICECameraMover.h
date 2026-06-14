#pragma once

#include "types.hpp"

namespace ICE
{
class ICECameraMover
{
public:
    ICECameraMover();

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
