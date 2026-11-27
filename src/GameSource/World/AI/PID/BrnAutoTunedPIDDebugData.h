#pragma once
#include "GameSource/World/AI/PID/BrnPIDController.h"

namespace BrnAI
{
    // ARTIST AIDebugComponent::Construct 0x82767188..0x82767198.
    struct AutoTunedPIDDebugData
    {
        // DWARF BrnAutoTunedPID.h:54, defined in the class: inlined by AIDebugComponent::Construct
        // (DWARF lists AutoTunedPIDDebugData::Clear there; ARTIST 0x82767188..0x82767198).
        void Clear()
        {
            meCoefficientToBeTuned = E_COEFFICIENT_COUNT;
            mfPCoefficient = 0.0f;
            mfICoefficient = 0.0f;
            mfDCoefficient = 0.0f;
        }

        EPIDCoefficient meCoefficientToBeTuned;
        f32 mfPCoefficient;
        f32 mfICoefficient;
        f32 mfDCoefficient;
    };
}
