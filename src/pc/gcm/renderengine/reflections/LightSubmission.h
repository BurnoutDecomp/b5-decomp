#pragma once
#include "BrnCommonTypes.h"
#include "pc/gcm/renderengine/reflections/SceneSettings.h"

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: apply distance and capacity before the original
    // corona writer. The main view retains its authored culling/assert behavior.
    struct SubmissionContext { const void* mpInterface = nullptr; Vector3 mEye = {}; };
    inline thread_local SubmissionContext sSubmission;
    class LightSubmissionScope
    {
        SubmissionContext mPrevious;
    public:
        LightSubmissionScope(const void* lpInterface, Vector3 lvEye) : mPrevious(sSubmission)
        { sSubmission = {lpInterface, lvEye}; }
        ~LightSubmissionScope() { sSubmission = mPrevious; }
    };
    inline bool AcceptLight(const void* lpInterface, const Vector3& lvPosition,
        u32 luCount, u32 luCapacity, f32 lfAuthoredDistanceSquared)
    {
        if (lpInterface != sSubmission.mpInterface) return true;
        if (luCount >= luCapacity) return false;
        const f32 lfX = lvPosition.x - sSubmission.mEye.x;
        const f32 lfY = lvPosition.y - sSubmission.mEye.y;
        const f32 lfZ = lvPosition.z - sSubmission.mEye.z;
        return Lights().IsVisible(lfX * lfX + lfY * lfY + lfZ * lfZ,
            std::sqrt((std::max)(lfAuthoredDistanceSquared, 0.0f)));
    }
    // FLAG PC-platform leaf: keep the original fade shape over the selected
    // capture range. Main-view alpha and authored size curves stay unchanged.
    inline f32 LightFadeDistanceSquared(const void* lpInterface, f32 lfDistanceSquared,
        f32 lfAuthoredDistanceSquared)
    {
        if (lpInterface != sSubmission.mpInterface) return lfDistanceSquared;
        const f32 lfDistance = Lights().GetDrawDistance(std::sqrt(lfAuthoredDistanceSquared));
        return lfDistance > 0 ? lfDistanceSquared * lfAuthoredDistanceSquared / (lfDistance * lfDistance)
            : lfAuthoredDistanceSquared;
    }
}
