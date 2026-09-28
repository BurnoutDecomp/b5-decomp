// OWNERLIST 2026-09-27, lane L5 MENUS: the ICE camera mover's scalar sub-updates -- the pause camera's time scale, lens,
// depth of field, fade and bloom -- extracted from src/SDKs/Packages/ICE/ICECameraMover.cpp by run_menus_ice_mover.py
// and run against a stand-in take (values by ICEElementDescriptions slot) and a recording camera.
//
// Checked against the ARTIST asm (ICECameraMover.cpp in the DWARF):
//   UpdateSimTime @0x8252E418  Clamp(TIME_SCALE[19] * 0.01, 0.01, 1): fsel Max(0.01, .) then Min(1, .) -- a NaN is 1
//   UpdateLens    @0x8252E548  Clamp(LENS_LENGTH[9], desc[9].mMin, desc[9].mMax) (the image's 5.0 / 500.0), then
//                              ConvertLensLengthToFovAngle and AngToDeg (stand-ins here: the angle carries the length)
//   UpdateFocus   @0x8252E678  RAWFOCUS_OVERRIDE[39] == 0 or BLUR_INTENSITY[15] <= 0 -> all zero; else
//                              start = Max(0, NEAR[12] - FALLOFF[14]*50), perfectStart = Max(start + 0.001, NEAR),
//                              perfectEnd = Max(perfectStart, FAR[13]), end = Max(perfectEnd + 0.001, FAR + falloff),
//                              blurriness = Min(1, Max(0, BLUR)) -- SetDepthOfField(start, pStart, pEnd, end, blur)
//   UpdateFade    @0x8252E788  fade = Clamp(FADE[21] * 0.01, 0, 1); FADE_TO_COLOR[43] 0..4 = black/white/r/g/b, else no call
//   UpdateBloom   @0x8252E328  level = Clamp(FADE * 0.01, 0.2, 1); SetBloom((level - 0.2) * -2, FADE * 0.02)
// Constants: 0.01 flt_82002138, 0.2 flt_82004744, -2 flt_82006D70, 0.02 flt_82005574, 50 flt_8207AC98, 0.001 flt_8207AC9C.
// The float goldens are a numpy float32 model of the same operations (each op rounded once).
#include "types.hpp"
#include "SDKs/Packages/ICE/ICEMath.hpp"
#include "rw/math/fpu/scalar_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gChecks = 0, gFailures = 0;

#ifndef CGS_ASSERT
#define CGS_ASSERT(lbCondition, lpcMessage) ((void)0)
#endif

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

namespace ICE
{
    enum eICESpace { eICE_SPACE_STANDIN = 0 };

    struct ICETake
    {
        f32 mafValues[64];
        s32 maiValues[64];
        f32 mfParameter;
        f32 GetValueFloat(s32 liElement) const { return mafValues[liElement]; }
        s32 GetValueInt(s32 liElement) const { return maiValues[liElement]; }
        f32 GetParameter() const { return mfParameter; }
    };

    struct ICEValueStandIn { f32 mf; f32 GetFloat() const { return mf; } };
    struct ICEElementDescription { ICEValueStandIn mMin; ICEValueStandIn mMax; };
    ICEElementDescription ICEElementDescriptions[64];

    struct ICECamera
    {
        s32 miSimTimeCalls, miFovCalls, miDofCalls, miFadeCalls, miBloomCalls;
        f32 mfSimTime, mfFov, mafDof[5], mafFade[4], mafBloom[2];
        void SetSimTimeMultiplier(f32 lf) { ++miSimTimeCalls; mfSimTime = lf; }
        void SetFieldOfView(f32 lf) { ++miFovCalls; mfFov = lf; }
        void SetDepthOfField(f32 a, f32 b, f32 c, f32 d, f32 e)
        { ++miDofCalls; mafDof[0] = a; mafDof[1] = b; mafDof[2] = c; mafDof[3] = d; mafDof[4] = e; }
        void SetFadeColor(f32 r, f32 g, f32 b, f32 a)
        { ++miFadeCalls; mafFade[0] = r; mafFade[1] = g; mafFade[2] = b; mafFade[3] = a; }
        void SetBloom(f32 lfThreshold, f32 lfLuminance) { ++miBloomCalls; mafBloom[0] = lfThreshold; mafBloom[1] = lfLuminance; }
    };

    struct ICECameraMover
    {
        ICETake*   mpTake;
        ICECamera* mpICECamera;
        f32        mfSimTime;
        void UpdateSimTime(f32 lfTimeStep);
        void UpdateLens(f32 lfTimeStep);
        void UpdateFocus(f32 lfTimeStep);
        void UpdateFade(f32 lfTimeStep);
        void UpdateBloom(f32 lfTimeStep);
    };

    // The lens conversions are tested in run_menus_ice_math.py; here the angle carries the clamped length itself.
    namespace ICEMath
    {
        Angle ConvertLensLengthToFovAngle(f32 lfLensLength) { return Angle(static_cast<u16>(lfLensLength)); }
        namespace Angles
        {
            f32 AngToDeg(Angle leAngle) { return static_cast<f32>(static_cast<u16>(leAngle)); }
        }
    }

#include "menus_ice_mover.inc"
}

namespace
{
    enum { TIME_SCALE = 19, LENS = 9, NEAR = 12, FAR = 13, FALLOFF = 14, BLUR = 15, FADE = 21, OVERRIDE = 39, FADE_COLOUR = 43 };

    struct Rig
    {
        ICE::ICETake        mTake;
        ICE::ICECamera      mCamera;
        ICE::ICECameraMover mMover;
        Rig()
        {
            std::memset(&mTake, 0, sizeof(mTake));
            std::memset(&mCamera, 0, sizeof(mCamera));
            mTake.mfParameter   = 0.5f;
            mMover.mpTake       = &mTake;
            mMover.mpICECamera  = &mCamera;
            mMover.mfSimTime    = 1.0f;
        }
    };
}

int main()
{
    ICE::ICEElementDescriptions[LENS].mMin.mf = 5.0f;     // the image's LENS range
    ICE::ICEElementDescriptions[LENS].mMax.mf = 500.0f;

    // ---- UpdateSimTime -------------------------------------------------------------------------------------------
    { Rig r; r.mTake.mafValues[TIME_SCALE] = 50.0f; r.mMover.UpdateSimTime(1.0f);
      Check(r.mCamera.miSimTimeCalls == 1 && Bits(r.mCamera.mfSimTime) == 0x3F000000u && Bits(r.mMover.mfSimTime) == 0x3F000000u,
            "SimTime: TIME_SCALE 50 -> 0.5"); }
    { Rig r; r.mTake.mafValues[TIME_SCALE] = 0.0f; r.mMover.UpdateSimTime(1.0f);
      Check(Bits(r.mCamera.mfSimTime) == 0x3C23D70Au, "SimTime: TIME_SCALE 0 -> the 0.01 floor (0x3C23D70A)"); }
    { Rig r; r.mTake.mafValues[TIME_SCALE] = 250.0f; r.mMover.UpdateSimTime(1.0f);
      Check(Bits(r.mCamera.mfSimTime) == 0x3F800000u, "SimTime: TIME_SCALE 250 -> 1.0"); }
    { Rig r; r.mTake.mafValues[TIME_SCALE] = std::numeric_limits<f32>::quiet_NaN(); r.mMover.UpdateSimTime(1.0f);
      Check(Bits(r.mCamera.mfSimTime) == 0x3F800000u, "SimTime: a NaN TIME_SCALE -> 1.0 (the Min fsel)"); }

    // ---- UpdateLens ----------------------------------------------------------------------------------------------
    { Rig r; r.mTake.mafValues[LENS] = 2.0f; r.mMover.UpdateLens(1.0f);
      Check(r.mCamera.miFovCalls == 1 && r.mCamera.mfFov == 5.0f, "Lens: 2 mm clamps to the 5 mm minimum"); }
    { Rig r; r.mTake.mafValues[LENS] = 1000.0f; r.mMover.UpdateLens(1.0f);
      Check(r.mCamera.mfFov == 500.0f, "Lens: 1000 mm clamps to the 500 mm maximum"); }
    { Rig r; r.mTake.mafValues[LENS] = 50.0f; r.mMover.UpdateLens(1.0f);
      Check(r.mCamera.mfFov == 50.0f, "Lens: 50 mm passes through"); }

    // ---- UpdateFocus ---------------------------------------------------------------------------------------------
    { Rig r; r.mTake.maiValues[OVERRIDE] = 0; r.mTake.mafValues[BLUR] = 0.5f; r.mMover.UpdateFocus(1.0f);
      Check(r.mCamera.miDofCalls == 1 && r.mCamera.mafDof[0] == 0.0f && r.mCamera.mafDof[4] == 0.0f,
            "Focus: RAWFOCUS_OVERRIDE 0 -> the all-zero band"); }
    { Rig r; r.mTake.maiValues[OVERRIDE] = 1; r.mTake.mafValues[BLUR] = 0.0f; r.mMover.UpdateFocus(1.0f);
      Check(r.mCamera.miDofCalls == 1 && r.mCamera.mafDof[1] == 0.0f && r.mCamera.mafDof[4] == 0.0f,
            "Focus: BLUR_INTENSITY 0 -> the all-zero band"); }
    { Rig r; r.mTake.maiValues[OVERRIDE] = 1; r.mTake.mafValues[BLUR] = 0.5f; r.mTake.mafValues[FALLOFF] = 0.2f;
      r.mTake.mafValues[NEAR] = 20.0f; r.mTake.mafValues[FAR] = 40.0f; r.mMover.UpdateFocus(1.0f);
      Check(r.mCamera.mafDof[0] == 10.0f && r.mCamera.mafDof[1] == 20.0f && r.mCamera.mafDof[2] == 40.0f &&
            r.mCamera.mafDof[3] == 50.0f && r.mCamera.mafDof[4] == 0.5f,
            "Focus: (start, perfect start, perfect end, end, blur) == (10, 20, 40, 50, 0.5)"); }
    { Rig r; r.mTake.maiValues[OVERRIDE] = 1; r.mTake.mafValues[BLUR] = 2.0f; r.mTake.mafValues[FALLOFF] = 0.2f;
      r.mTake.mafValues[NEAR] = 20.0f; r.mTake.mafValues[FAR] = 40.0f; r.mMover.UpdateFocus(1.0f);
      Check(r.mCamera.mafDof[4] == 1.0f, "Focus: BLUR_INTENSITY 2 -> blurriness Min(1, .) == 1"); }

    // ---- UpdateFade ----------------------------------------------------------------------------------------------
    { Rig r; r.mTake.mafValues[FADE] = 50.0f; r.mTake.maiValues[FADE_COLOUR] = 2; r.mMover.UpdateFade(1.0f);
      Check(r.mCamera.miFadeCalls == 1 && r.mCamera.mafFade[0] == 1.0f && r.mCamera.mafFade[1] == 0.0f &&
            r.mCamera.mafFade[2] == 0.0f && Bits(r.mCamera.mafFade[3]) == 0x3F000000u, "Fade: 50 %, red -> (1, 0, 0, 0.5)"); }
    { Rig r; r.mTake.mafValues[FADE] = 150.0f; r.mTake.maiValues[FADE_COLOUR] = 0; r.mMover.UpdateFade(1.0f);
      Check(r.mCamera.miFadeCalls == 1 && r.mCamera.mafFade[0] == 0.0f && r.mCamera.mafFade[3] == 1.0f,
            "Fade: 150 %, black -> (0, 0, 0, 1)"); }
    { Rig r; r.mTake.mafValues[FADE] = 50.0f; r.mTake.maiValues[FADE_COLOUR] = 9; r.mMover.UpdateFade(1.0f);
      Check(r.mCamera.miFadeCalls == 0, "Fade: an unknown colour makes no call"); }

    // ---- UpdateBloom ---------------------------------------------------------------------------------------------
    { Rig r; r.mTake.mafValues[FADE] = 50.0f; r.mMover.UpdateBloom(1.0f);
      Check(r.mCamera.miBloomCalls == 1 && Bits(r.mCamera.mafBloom[0]) == 0xBF19999Au && r.mCamera.mafBloom[1] == 1.0f,
            "Bloom: FADE 50 -> (threshold 0xBF19999A == -0.6, luminance 1.0)"); }
    { Rig r; r.mTake.mafValues[FADE] = 0.0f; r.mMover.UpdateBloom(1.0f);
      Check(Bits(r.mCamera.mafBloom[0]) == 0x80000000u && r.mCamera.mafBloom[1] == 0.0f,
            "Bloom: FADE 0 -> level 0.2 -> threshold -0.0, luminance 0"); }
    { Rig r; r.mTake.mafValues[FADE] = 100.0f; r.mMover.UpdateBloom(1.0f);
      Check(Bits(r.mCamera.mafBloom[0]) == 0xBFCCCCCDu && r.mCamera.mafBloom[1] == 2.0f,
            "Bloom: FADE 100 -> (threshold 0xBFCCCCCD == -1.6, luminance 2.0)"); }

    std::printf("MenusIceMover: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
