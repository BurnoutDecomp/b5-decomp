#include "ICECameraMover.h"

namespace ICE
{
ICECameraMover::ICECameraMover()
{
    for (CameraMoveChannel& lChannel : maChannels)
    {
        lChannel.mfPositionX = 0.0f;
        lChannel.mfPositionY = 0.0f;
        lChannel.mfPositionZ = 0.0f;
        lChannel.mfVelocityX = 0.0f;
        lChannel.mfVelocityY = 0.0f;
        lChannel.mfVelocityZ = 0.0f;
        lChannel.mfAccelerationX = 0.0f;
        lChannel.mfAccelerationY = 0.0f;
        lChannel.mfBlend = 0.0f;
        lChannel.mfWeight = 1.0f;
        lChannel.mu16Flags = 0;
        lChannel.mu16Enabled = 1;
    }
}
}
