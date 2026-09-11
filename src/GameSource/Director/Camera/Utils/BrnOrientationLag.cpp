#include "GameSource/Director/Camera/Utils/BrnOrientationLag.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/vpu/matrix44affine_operation.h"      // rw::math::vpu::SLerp

// BrnDirector::Camera::Utils::OrientationLag -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Director/Camera/Utils/BrnOrientationLag.cpp):
//   OrientationLag::Update @0x82222FB8  (called by BehaviourFailsafe::Update and
//                                        BehaviourRig::Update)
//
// X360 asm walk: assert mpParameters (cpp:60; the assert does not early-out), then:
//   mbFirstFrame set   -> mLastTransform = lrTransform (4 row copies), clear the flag;
//   mbUseSlerpSpring   -> broadcast mpParameters->mfSlerpSpring and call
//                         rw::math::vpu::SLerp(mLastTransform, lrTransform, that amount,
//                         &angleOut) -- the same vendor op, and the same four-argument
//                         shape, as the reviewed BrnLooker::Track site. The angle-out
//                         slot is a stack local that is written and never read. Copy the
//                         blended rows into mLastTransform, then overwrite the w row with
//                         lrTransform's (the store of the blended w row is immediately
//                         re-stored from lrTransform: translation always snaps).
//                         The frame delta is NOT part of this call: nothing forwards
//                         lfTimestep into the blend, so the spring amount is used raw.
//   otherwise          -> mLastTransform = lrTransform (straight copy, flag untouched).

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // @ 0x82222FB8
    void OrientationLag::Update(f32 lfTimestep, const rw::math::vpu::Matrix44Affine& lrTransform)
    {
        CGS_ASSERT(mpParameters != NULL, "mpParameters != NULL");

        if (mbFirstFrame)
        {
            mLastTransform = lrTransform;
            mbFirstFrame   = false;
        }
        else if (mpParameters->mbUseSlerpSpring)
        {
            // Spherically blend the held orientation toward the new transform by the
            // slerp spring; the translation row snaps to the new transform. The frame
            // delta plays no part in the blend -- see the banner.
            (void)lfTimestep;
            rw::math::vpu::Vector3 lUnusedAngle;
            const rw::math::vpu::Matrix44Affine lBlended =
                rw::math::vpu::SLerp(mLastTransform, lrTransform,
                                     mpParameters->mfSlerpSpring, &lUnusedAngle);

            mLastTransform       = lBlended;
            mLastTransform.wAxis = lrTransform.wAxis;
        }
        else
        {
            mLastTransform = lrTransform;
        }
    }

    // Point the lag at a caller-owned tunables block. The console never emits this as a
    // standalone symbol -- every embedder inlines it -- so it is transcribed from the one
    // site that shows it whole: BehaviourRig::Prepare stores
    // &parameters->mOrientationLagParams into the lag's parameter slot and only THEN runs
    // the null assert, which carries this file's own assert text.
    void OrientationLag::SetParameters(const Parameters* lpParameters)
    {
        mpParameters = lpParameters;
        CGS_ASSERT(mpParameters != NULL, "mpParameters != NULL");
    }

    // The lagged output transform. Inlined at every read site as a plain load of the
    // first member.
    const rw::math::vpu::Matrix44Affine& OrientationLag::GetTransform() const
    {
        return mLastTransform;
    }
}
}
}
