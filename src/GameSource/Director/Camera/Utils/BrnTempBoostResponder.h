#pragma once
#include "types.hpp"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "rw/math/fpu/scalar_operation.h"
namespace BrnDirector { namespace Camera { namespace Utils {
// Inlined in ARTIST BehaviourManager::Construct / UpdateAllBehaviours.
struct TempCameraBoostResponder
{
    void Construct() { mbBoosting = false; mfParametricTime = 0.0f; }
    void Update(bool lbBoosting, f32 lfTimeStep)
    {
        // ARTIST 82251A40..74: fmadds/fsubs, followed by the native fsel clamp.
        if (lbBoosting)
            mfParametricTime = rw::math::fpu::Min(mfParametricTime + lfTimeStep * 0.5f, 1.0f);
        else
            mfParametricTime = rw::math::fpu::Max(mfParametricTime - lfTimeStep, 0.0f);
    }
    f32 GetFOVBoostAmount() { return SineLerp(0.0f, 1.0f, mfParametricTime); }
private:
    bool mbBoosting;
    f32 mfParametricTime;
};
}}}
