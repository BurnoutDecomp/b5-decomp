#pragma once
#include "GameSource/World/AI/PID/BrnPIDController.h"

namespace BrnAI
{
    // ARTIST AIDebugComponent::Construct 0x82767188..0x82767198.
    struct AutoTunedPIDDebugData
    {
        EPIDCoefficient meCoefficientToBeTuned;
        f32 mfPCoefficient;
        f32 mfICoefficient;
        f32 mfDCoefficient;
    };
}
