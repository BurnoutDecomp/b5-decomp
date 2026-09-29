#pragma once

#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>

namespace BrnTraffic
{
// FLAG PC-platform leaf: traffic presentation between fixed simulation ticks.
// Latch only simulation endpoints; sample into render-local copies. Physics,
// collision queries, AI and network output always retain the unblended state.
class RenderPosePC
{
    CgsSystem::FrameInterpolation::PoseTrack mBody{};
    CgsSystem::FrameInterpolation::PoseTrack maWheels[4]{};
    rw::math::vpu::Vector4 mPreviousAngles{};
    rw::math::vpu::Vector4 mCurrentAngles{};
    bool mbValid = false;
    bool mabWheelValid[4] = {};

    static f32 BlendAngle(f32 lfPrevious, f32 lfCurrent, f32 lfAlpha)
    {
        const f32 KF_TWO_PI = 6.2831853071795864769f;
        return lfPrevious + std::remainder(lfCurrent - lfPrevious, KF_TWO_PI) * lfAlpha;
    }

public:
    void Reset()
    {
        mBody.Reset();
        mbValid = false;
        for (u32 luWheel = 0; luWheel < 4; ++luWheel)
        {
            maWheels[luWheel].Reset();
            mabWheelValid[luWheel] = false;
        }
    }

    void Latch(const rw::math::vpu::Matrix44Affine& lrBody,
               const rw::math::vpu::Vector4& lrAngles,
               const rw::math::vpu::Matrix44Affine* lpaWheels,
               const bool* lpabWheelExists)
    {
        auto lPreviousBody = lrBody;
        mBody.Restore(lPreviousBody);
        mBody.Latch(lrBody);
        mPreviousAngles = mbValid ? mCurrentAngles : lrAngles;
        mCurrentAngles = lrAngles;
        for (u32 luWheel = 0; luWheel < 4; ++luWheel)
        {
            if (!lpaWheels || !lpabWheelExists[luWheel])
            {
                maWheels[luWheel].Reset();
                mabWheelValid[luWheel] = false;
                continue;
            }
            if (!mabWheelValid[luWheel] && mbValid)
            {
                // The first physical wheel must share the body's interpolation
                // phase. Seat its current local pose on the previous body, so
                // promotion cannot put the wheels one tick ahead of the chassis.
                const auto lLocalWheel = rw::math::vpu::Mult(lpaWheels[luWheel],
                    rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lrBody));
                maWheels[luWheel].Latch(rw::math::vpu::Mult(lLocalWheel, lPreviousBody));
            }
            maWheels[luWheel].Latch(lpaWheels[luWheel]);
            mabWheelValid[luWheel] = true;
        }
        mbValid = true;
    }

    rw::math::vpu::Matrix44Affine SampleBody(
        const rw::math::vpu::Matrix44Affine& lrCurrent, f32 lfAlpha) const
    {
        auto lPose = lrCurrent;
        if (lfAlpha < 1.0f) mBody.Apply(lPose, lfAlpha);
        return lPose;
    }

    rw::math::vpu::Matrix44Affine SampleWheel(u32 luWheel,
        const rw::math::vpu::Matrix44Affine& lrCurrent, f32 lfAlpha) const
    {
        auto lPose = lrCurrent;
        if (lfAlpha < 1.0f) maWheels[luWheel].Apply(lPose, lfAlpha);
        return lPose;
    }

    rw::math::vpu::Vector4 SampleAngles(
        const rw::math::vpu::Vector4& lrCurrent, f32 lfAlpha) const
    {
        if (!mbValid || lfAlpha >= 1.0f) return lrCurrent;
        return rw::math::vpu::Vector4{
            BlendAngle(mPreviousAngles.x, mCurrentAngles.x, lfAlpha),
            BlendAngle(mPreviousAngles.y, mCurrentAngles.y, lfAlpha),
            BlendAngle(mPreviousAngles.z, mCurrentAngles.z, lfAlpha),
            BlendAngle(mPreviousAngles.w, mCurrentAngles.w, lfAlpha)};
    }
};
}
