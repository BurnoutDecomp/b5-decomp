// FX-LASTFIX (crash parity 2026-09-26) -- run_fxlastfix_ice_heading_slerp.py: BehaviourIceAnim::Update's heading space
// eases 20% a frame, as the console's words do.
//
// The runner lifts the revision's PRODUCTION text out of src/GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.cpp:
//   fxlastfix_iceanim_const.inc    the KF_HEADING_SPACE_2_SLERP_AMOUNT definition;
//   fxlastfix_iceanim_helpers.inc  the two inline helpers Update's arm calls (CreateHeadingSpaceLookAt,
//                                  GetVehicleWorldPosition);
//   fxlastfix_iceanim_arm.inc      Update's arm, from `if (!mbIsPrepared)` (the seed) to the end of the SLerp statement;
//   fxlastfix_iceanim_overload.inc (only a revision that has them) the pointer-amount SLerp declaration the file used to
//                                  carry and its DirectorLinkStubs.cpp link stub, so the old arm links as it shipped.
// It compiles them here against the REAL rw::math::vpu::SLerp (rw/math/vpu/matrix44affine_operation.h), inside a fixture
// whose members carry the behaviour's names, and replays FxLastfixIceHeadingSlerpData.h: chains of frames in which the
// console's window 0x8224725C..0x8224737C ran on emu64 (CreateLookAt 0x8220C4F8 and SLerp 0x82216858 interpreted, the
// amount the splat the CRT thunk 0x82C49580 builds from flt_82004744).
//   - Utils::CreateLookAt is a stand-in that hands back the CONSOLE's look-at for that call (the PC's CreateLookAt
//     normalises exactly where the console runs vrsqrtefp + two Newton steps -- FLAGged in CameraUtils.cpp and outside
//     this arm), and checks the eye / target the arm built against the console's v1 / v2 (x, y, z: the w lane of the
//     console's target is a vperm don't-care, eye.w + forward.x; the PC's is 0.0f; SLerp never reads an axis row's w
//     lane into an x / y / z lane).
//   - Per frame: the look-at calls (count, eye, target) and the heading space after the SLerp (all 16 lanes, bit for
//     bit; a NaN matches a NaN) -- two checks a frame.
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"

#include <cstdio>
#include <cstring>

#include "FxLastfixIceHeadingSlerpData.h"

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
    bool IsNaNBits(unsigned lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
    bool Same(unsigned luA, unsigned luB) { return luA == luB || (IsNaNBits(luA) && IsNaNBits(luB)); }

    rw::math::vpu::Vector3 Vec(const unsigned* lpu)
    {
        rw::math::vpu::Vector3 lv;
        lv.x = Float(lpu[0]); lv.y = Float(lpu[1]); lv.z = Float(lpu[2]); lv.w = Float(lpu[3]);
        return lv;
    }
    rw::math::vpu::Matrix44Affine Mat(const unsigned* lpu)
    {
        rw::math::vpu::Matrix44Affine lm;
        lm.xAxis = Vec(lpu); lm.yAxis = Vec(lpu + 4); lm.zAxis = Vec(lpu + 8); lm.wAxis = Vec(lpu + 12);
        return lm;
    }
    void Words(const rw::math::vpu::Matrix44Affine& lrM, unsigned* lpu)
    {
        const rw::math::vpu::Vector3* lap[4] = { &lrM.xAxis, &lrM.yAxis, &lrM.zAxis, &lrM.wAxis };
        for (int liRow = 0; liRow < 4; ++liRow)
        {
            lpu[4 * liRow + 0] = Bits(lap[liRow]->x); lpu[4 * liRow + 1] = Bits(lap[liRow]->y);
            lpu[4 * liRow + 2] = Bits(lap[liRow]->z); lpu[4 * liRow + 3] = Bits(lap[liRow]->w);
        }
    }

    // The CreateLookAt stand-in's queue: the console's calls of the current frame, in order.
    const IceHeadingFrame* gpFrame = nullptr;
    int  giLookAtCalls = 0;
    bool gbLookAtArgsOk = true;
    char gacLookAtNote[160] = "";
}

// The [DIAG] witnesses the fixed arm calls (NOT X360) -- not part of the console's words, inert here.
static void BrnDiag_HeadingEaseBefore(const rw::math::vpu::Matrix44Affine&, const rw::math::vpu::Matrix44Affine&) {}

namespace rw { namespace math { namespace vpu {
#include "fxlastfix_iceanim_overload.inc"
}}}

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
    {
        const int liCall = giLookAtCalls++;
        if (gpFrame == nullptr || liCall >= gpFrame->miLookAts || liCall >= 2)
        {
            gbLookAtArgsOk = false;
            std::snprintf(gacLookAtNote, sizeof(gacLookAtNote), "an extra CreateLookAt call (%d)", liCall + 1);
            Matrix44Affine lZero;
            std::memset(&lZero, 0, sizeof(lZero));
            return lZero;
        }
        const IceHeadingLookAt& lrCall = gpFrame->maLookAt[liCall];
        const unsigned lauEye[3] = { Bits(lEyePosition.x), Bits(lEyePosition.y), Bits(lEyePosition.z) };
        const unsigned lauTarget[3] = { Bits(lTargetPosition.x), Bits(lTargetPosition.y), Bits(lTargetPosition.z) };
        for (int liLane = 0; liLane < 3; ++liLane)
        {
            if (!Same(lauEye[liLane], lrCall.mauEye[liLane]) || !Same(lauTarget[liLane], lrCall.mauTarget[liLane]))
            {
                if (gbLookAtArgsOk)
                    std::snprintf(gacLookAtNote, sizeof(gacLookAtNote),
                                  "call %d lane %c: eye %08X target %08X, the console has %08X / %08X", liCall,
                                  "xyz"[liLane], lauEye[liLane], lauTarget[liLane], lrCall.mauEye[liLane],
                                  lrCall.mauTarget[liLane]);
                gbLookAtArgsOk = false;
            }
        }
        return Mat(lrCall.mauResult);
    }
}

#include "fxlastfix_iceanim_const.inc"
#include "fxlastfix_iceanim_helpers.inc"

// The ref the arm resolves through: Get(world) hands back the fixture's look-at vehicle.
struct FakeVehicleRef
{
    const VehicleInfo* mpVehicle;
    const VehicleInfo* Get(const void* lpWorld) const { (void)lpWorld; return mpVehicle; }
};

struct IceAnimHeadingFixture
{
    bool                          mbIsPrepared;
    rw::math::vpu::Matrix44Affine mHeadingSpaceTransform;
    FakeVehicleRef                mSecondaryVehicleRef;

    void RunArm(const void* lpWorld)
    {
#include "fxlastfix_iceanim_arm.inc"
    }
};
} // namespace Camera
} // namespace BrnDirector

int main()
{
    using namespace BrnDirector::Camera;
    static VehicleInfo sVehicle;
    unsigned luChecks = 0, luFailures = 0, luPrinted = 0;
    const int liWorld = 0;

    for (const IceHeadingChain& lrChain : kaIceHeadingChains)
    {
        IceAnimHeadingFixture lFixture;
        lFixture.mbIsPrepared = false;
        lFixture.mHeadingSpaceTransform = Mat(lrChain.mauInitial);
        lFixture.mSecondaryVehicleRef.mpVehicle = &sVehicle;
        for (int liFrame = 0; liFrame < lrChain.miCount; ++liFrame)
        {
            const IceHeadingFrame& lrFrame = kaIceHeadingFrames[lrChain.miFirst + liFrame];
            sVehicle.mRaceCarState.mTransform = Mat(lrFrame.mauVehicle);
            lFixture.mbIsPrepared = (lrFrame.miPrepared != 0);
            gpFrame = &lrFrame;
            giLookAtCalls = 0;
            gbLookAtArgsOk = true;
            gacLookAtNote[0] = '\0';

            lFixture.RunArm(&liWorld);

            ++luChecks;
            if (!gbLookAtArgsOk || giLookAtCalls != lrFrame.miLookAts)
            {
                ++luFailures;
                if (luPrinted++ < 12)
                    std::printf("FAIL  %s frame %d: the arm's CreateLookAt calls: %d (the console makes %d)%s%s\n",
                                lrChain.mpcLabel, liFrame, giLookAtCalls, lrFrame.miLookAts,
                                gacLookAtNote[0] ? "; " : "", gacLookAtNote);
            }

            unsigned lauHeading[16];
            Words(lFixture.mHeadingSpaceTransform, lauHeading);
            ++luChecks;
            for (int liLane = 0; liLane < 16; ++liLane)
            {
                if (!Same(lauHeading[liLane], lrFrame.mauHeading[liLane]))
                {
                    ++luFailures;
                    if (luPrinted++ < 12)
                        std::printf("FAIL  %s frame %d: mHeadingSpaceTransform row %d.%c = %08X (%.6g), the console has "
                                    "%08X (%.6g)\n", lrChain.mpcLabel, liFrame, liLane / 4, "xyzw"[liLane % 4],
                                    lauHeading[liLane], Float(lauHeading[liLane]), lrFrame.mauHeading[liLane],
                                    Float(lrFrame.mauHeading[liLane]));
                    break;
                }
            }
        }
    }
    std::printf("FxLastfixIceHeadingSlerp: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
