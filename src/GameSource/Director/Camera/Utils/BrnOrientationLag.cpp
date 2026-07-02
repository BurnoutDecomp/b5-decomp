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
//   mbUseSlerpSpring   -> splat mpParameters->mfSlerpSpring (lvlx params+0x10, vspltw)
//                         and call rw::math::vpu::SLerp(out, mLastTransform, lrTransform,
//                         ...) -- the same vendor op the reviewed BrnLooker::Track site
//                         maps to SLerp(from, to, &amount). Copy the blended rows into
//                         mLastTransform, then overwrite the w row with lrTransform's
//                         (the X360 stores out.wAxis then immediately re-stores
//                         lrTransform.wAxis over it: translation always snaps).
//                         FLAG: the X360 SLerp variant also receives lfTimestep in f1
//                         (the caller leaves it live) -- the spring-vs-time integration,
//                         and the PS3 hint's lAngle/lT locals, live INSIDE the vendor op,
//                         whose body is declaration-only in this vocabulary.
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
            // slerp spring; the translation row snaps to the new transform.
            // (lfTimestep feeds the vendor SLerp's spring integration -- see FLAG above.)
            (void)lfTimestep;
            const rw::math::vpu::Matrix44Affine lBlended =
                rw::math::vpu::SLerp(mLastTransform, lrTransform, &mpParameters->mfSlerpSpring);

            mLastTransform       = lBlended;
            mLastTransform.wAxis = lrTransform.wAxis;
        }
        else
        {
            mLastTransform = lrTransform;
        }
    }
}
}
}
