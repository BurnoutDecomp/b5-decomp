#pragma once
#include "types.hpp"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include <cmath>
namespace BrnDirector { namespace Camera { namespace Utils {
// Inlined in ARTIST BehaviourManager::Construct / UpdateAllBehaviours.
struct TempCameraBoostResponder
{
    void Construct() { mbBoosting = false; mfParametricTime = 0.0f; }
    void Update(bool boosting, f32 timestep)
    {
        mfParametricTime = boosting ? std::fmin(1.0f, mfParametricTime + timestep * 0.5f)
                                    : std::fmax(0.0f, mfParametricTime - timestep);
    }
    f32 GetFOVBoostAmount() { return SineLerp(0.0f, 1.0f, mfParametricTime); }
private:
    bool mbBoosting;
    f32 mfParametricTime;
};
}}}
