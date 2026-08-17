#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h"

#include <math.h>   // sqrtf

// ⚠️ FLAG PC QUALITY-OF-LIFE LAYER -- see the banner in CgsFrameInterpolation.h.
// No X360 counterpart exists for anything in this file.
namespace CgsSystem
{
    namespace FrameInterpolation
    {
        namespace
        {
            f32  sfAlpha        = 1.0f;
            f32  sfFrameSeconds = 1.0f / 60.0f;
            bool sbEnabled      = false;

            // A one-tick delta larger than either of these is a CUT, not motion.
            //
            // Both are set an order of magnitude clear of anything the game can
            // legitimately produce in one 60 Hz tick, so they never fire on real
            // movement and always fire on a shot change:
            //   * 25 m/tick is 1500 m/s. The fastest car in the game does ~0.9 m/tick.
            //   * cos(30 deg) is 1800 deg/s of camera yaw. A fast chase-camera whip
            //     is nearer 360 deg/s, i.e. 6 deg/tick.
            const f32 KF_CUT_DISTANCE_SQ = 25.0f * 25.0f;
            const f32 KF_CUT_FORWARD_DOT = 0.866f;   // cos(30 degrees)

            // Below this the blend is a no-op either way and the caller gets the
            // endpoint back untouched (bit-identical to not blending at all).
            const f32 KF_ALPHA_EPSILON = 1.0f / 512.0f;

            struct V3
            {
                f32 x, y, z;
            };

            V3 Make(f32 lfX, f32 lfY, f32 lfZ)
            {
                V3 lResult;
                lResult.x = lfX;
                lResult.y = lfY;
                lResult.z = lfZ;
                return lResult;
            }

            V3 Lerp(const V3& lrA, const V3& lrB, f32 lfT)
            {
                return Make(lrA.x + (lrB.x - lrA.x) * lfT,
                            lrA.y + (lrB.y - lrA.y) * lfT,
                            lrA.z + (lrB.z - lrA.z) * lfT);
            }

            f32 Dot(const V3& lrA, const V3& lrB)
            {
                return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
            }

            V3 Cross(const V3& lrA, const V3& lrB)
            {
                return Make(lrA.y * lrB.z - lrA.z * lrB.y,
                            lrA.z * lrB.x - lrA.x * lrB.z,
                            lrA.x * lrB.y - lrA.y * lrB.x);
            }

            // Returns false (leaving lrVector untouched) when the vector is too short to
            // carry a direction -- the caller then abandons the blend rather than
            // publishing a degenerate basis.
            bool Normalise(V3& lrVector)
            {
                const f32 lfLengthSq = Dot(lrVector, lrVector);
                if (lfLengthSq < 1.0e-12f)
                    return false;
                const f32 lfScale = 1.0f / sqrtf(lfLengthSq);
                lrVector.x *= lfScale;
                lrVector.y *= lfScale;
                lrVector.z *= lfScale;
                return true;
            }

            // Matrix44Affine's four rows are Vector3 -- four 16-byte lane registers whose
            // fourth lane is unused padding (rw/math/vpu/types.h).
            V3 Row(const rw::math::vpu::Vector3& lrRow)
            {
                return Make(lrRow.x, lrRow.y, lrRow.z);
            }

            void StoreRow(rw::math::vpu::Vector3& lrRow, const V3& lrValue, f32 lfW)
            {
                lrRow.x = lrValue.x;
                lrRow.y = lrValue.y;
                lrRow.z = lrValue.z;
                lrRow.w = lfW;
            }
        }

        void SetAlpha(f32 lfAlpha)
        {
            if (lfAlpha < 0.0f)
                lfAlpha = 0.0f;
            if (lfAlpha > 1.0f)
                lfAlpha = 1.0f;
            sfAlpha = lfAlpha;
        }

        f32 GetAlpha()
        {
            return sfAlpha;
        }

        void SetEnabled(bool lbEnabled)
        {
            sbEnabled = lbEnabled;
        }

        bool IsEnabled()
        {
            return sbEnabled;
        }

        void SetFrameSeconds(f32 lfSeconds)
        {
            // Clamped so a breakpoint, a stalled load or a window drag cannot hand a
            // consumer a multi-second step. One tenth of a second is six 60 Hz ticks.
            if (lfSeconds < 0.0f)
                lfSeconds = 0.0f;
            if (lfSeconds > 0.1f)
                lfSeconds = 0.1f;
            sfFrameSeconds = lfSeconds;
        }

        f32 GetFrameSeconds()
        {
            return sfFrameSeconds;
        }

        f32 BlendScalar(f32 lfPrevious, f32 lfCurrent, f32 lfAlpha)
        {
            if (lfAlpha >= 1.0f - KF_ALPHA_EPSILON)
                return lfCurrent;
            if (lfAlpha <= KF_ALPHA_EPSILON)
                return lfPrevious;
            return lfPrevious + (lfCurrent - lfPrevious) * lfAlpha;
        }

        rw::math::vpu::Matrix44Affine BlendTransform(
            const rw::math::vpu::Matrix44Affine& lrPrevious,
            const rw::math::vpu::Matrix44Affine& lrCurrent,
            f32                                  lfAlpha)
        {
            if (lfAlpha >= 1.0f - KF_ALPHA_EPSILON)
                return lrCurrent;

            const V3 lPrevPos = Row(lrPrevious.wAxis);
            const V3 lCurPos  = Row(lrCurrent.wAxis);

            // ---- cut rejection: translation --------------------------------------
            const V3 lDelta = Make(lCurPos.x - lPrevPos.x,
                                   lCurPos.y - lPrevPos.y,
                                   lCurPos.z - lPrevPos.z);
            if (Dot(lDelta, lDelta) > KF_CUT_DISTANCE_SQ)
                return lrCurrent;

            const V3 lPrevX = Row(lrPrevious.xAxis);
            const V3 lPrevY = Row(lrPrevious.yAxis);
            const V3 lPrevZ = Row(lrPrevious.zAxis);
            const V3 lCurX  = Row(lrCurrent.xAxis);
            const V3 lCurY  = Row(lrCurrent.yAxis);
            const V3 lCurZ  = Row(lrCurrent.zAxis);

            // ---- cut rejection: orientation --------------------------------------
            if (Dot(lPrevZ, lCurZ) < KF_CUT_FORWARD_DOT)
                return lrCurrent;

            // ---- the basis, measured handedness ----------------------------------
            // cross(y, z) is either +x or -x depending on which convention the source
            // matrix was built with; read it off lrCurrent instead of assuming.
            const f32 lfHandedness = (Dot(Cross(lCurY, lCurZ), lCurX) >= 0.0f) ? 1.0f : -1.0f;

            V3 lForward = Lerp(lPrevZ, lCurZ, lfAlpha);
            if (!Normalise(lForward))
                return lrCurrent;

            const V3 lUpGuess = Lerp(lPrevY, lCurY, lfAlpha);

            V3 lRight = Cross(lUpGuess, lForward);
            if (!Normalise(lRight))
                return lrCurrent;
            lRight.x *= lfHandedness;
            lRight.y *= lfHandedness;
            lRight.z *= lfHandedness;

            V3 lUp = Cross(lForward, lRight);
            lUp.x *= lfHandedness;
            lUp.y *= lfHandedness;
            lUp.z *= lfHandedness;

            const V3 lPos = Lerp(lPrevPos, lCurPos, lfAlpha);

            // The w lanes are carried from lrCurrent: on this engine's affine rows they
            // are padding/flags, not part of the pose, so they must not be blended.
            rw::math::vpu::Matrix44Affine lResult = lrCurrent;
            StoreRow(lResult.xAxis, lRight,   lrCurrent.xAxis.w);
            StoreRow(lResult.yAxis, lUp,      lrCurrent.yAxis.w);
            StoreRow(lResult.zAxis, lForward, lrCurrent.zAxis.w);
            StoreRow(lResult.wAxis, lPos,     lrCurrent.wAxis.w);
            return lResult;
        }
    }
}
