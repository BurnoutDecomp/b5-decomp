// L2 CAMCOLLIDE (owner's list 2026-09-28): the two camera utils CollisionPolicyAttachedToVehicle's scene-query
// pair calls, against the console's own answers (tests/L2CameraUtilsData.h, the ARTIST words run whole on emu64 by
// scratch/OWNERLIST_0927/L2/emu/gen_camutils.py):
//   BrnDirector::Camera::Utils::ApplyPitchAboutPointRads                @0x822183E0
//   BrnDirector::Camera::Utils::ResolveLineTestNearestUsingNormalStrict @0x8220CD58
// run_l2_camera_utils.py extracts the revision's two bodies (and the lane helpers above them) from CameraUtils.cpp
// into l2_camutils_bodies.inc, included below. ApplyPitchAboutPointRads is fed the CONSOLE's look-at: the fixture's
// CreateLookAt checks it is asked with the console's two arguments and answers with the console's matrix (the
// PC CreateLookAt is the flagged de-optimised form, not this function's subject), so every lane of the result must
// match bit for bit (a NaN lane by class: the VMX and SSE NaN payloads differ).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Utils/BrnPostBox.h"
#include "GameSource/Director/Utils/BrnDirectorPostOfficeTypes.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "SDKs/XboxMath/XMVectorSinCos.h"

#include "L2CameraUtilsData.h"

static int giAsserts = 0;
namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++giAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool LaneMatches(u32 luConsole, u32 luPc)
{
    return IsNanBits(luConsole) ? IsNanBits(luPc) : luConsole == luPc;
}

static Vector3 VecFromWords(const u32* lpu)
{
    Vector3 l;
    l.x = FromBits(lpu[0]);
    l.y = FromBits(lpu[1]);
    l.z = FromBits(lpu[2]);
    l.w = FromBits(lpu[3]);
    return l;
}

static bool VecMatches(const u32* lpuConsole, const Vector3& lrPc)
{
    return LaneMatches(lpuConsole[0], Bits(lrPc.x)) && LaneMatches(lpuConsole[1], Bits(lrPc.y))
        && LaneMatches(lpuConsole[2], Bits(lrPc.z)) && LaneMatches(lpuConsole[3], Bits(lrPc.w));
}

// ---- the fixture look-at: the console's answer for the current row --------------------------------------------
static const L2PitchRow* gpRow = 0;
static int  giLookAtCalls = 0;
static bool gbLookAtArgsOk = true;

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
    {
        ++giLookAtCalls;
        if (!VecMatches(gpRow->mauLookAtEye, lEyePosition) || !VecMatches(gpRow->mauLookAtTarget, lTargetPosition))
            gbLookAtArgsOk = false;
        Matrix44Affine l;
        l.xAxis = VecFromWords(&gpRow->mauLookAt[0]);
        l.yAxis = VecFromWords(&gpRow->mauLookAt[4]);
        l.zAxis = VecFromWords(&gpRow->mauLookAt[8]);
        l.wAxis = VecFromWords(&gpRow->mauLookAt[12]);
        return l;
    }

#include "l2_camutils_bodies.inc"
}
}
}

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

int main()
{
    using namespace BrnDirector::Camera::Utils;
    char lacLabel[256];

    // ---- ApplyPitchAboutPointRads -----------------------------------------------------------------------------
    const int KI_PITCH = static_cast<int>(sizeof(kaL2PitchRows) / sizeof(kaL2PitchRows[0]));
    int liArgsBad = 0, liRowsBad = 0, liPosBad = 0, liShown = 0;
    for (int r = 0; r < KI_PITCH; ++r)
    {
        const L2PitchRow& lrRow = kaL2PitchRows[r];
        gpRow = &lrRow;
        giLookAtCalls = 0;
        gbLookAtArgsOk = true;
        Matrix44Affine lM;
        lM.xAxis = VecFromWords(&lrRow.mauTransform[0]);
        lM.yAxis = VecFromWords(&lrRow.mauTransform[4]);
        lM.zAxis = VecFromWords(&lrRow.mauTransform[8]);
        lM.wAxis = VecFromWords(&lrRow.mauTransform[12]);
        const BrnDirector::VecFloat lvElevation(FromBits(lrRow.muElevation));
        ApplyPitchAboutPointRads(lM, VecFromWords(lrRow.mauCentre), lvElevation);

        const bool lbArgs = (giLookAtCalls == 1 && gbLookAtArgsOk);
        const bool lbRows = VecMatches(&lrRow.mauOut[0], lM.xAxis) && VecMatches(&lrRow.mauOut[4], lM.yAxis)
                         && VecMatches(&lrRow.mauOut[8], lM.zAxis);
        const bool lbPos = VecMatches(&lrRow.mauOut[12], lM.wAxis);
        liArgsBad += lbArgs ? 0 : 1;
        liRowsBad += lbRows ? 0 : 1;
        liPosBad += lbPos ? 0 : 1;
        if ((!lbArgs || !lbRows || !lbPos) && liShown < 8)
        {
            ++liShown;
            std::printf("  pitch row %d: args %d rows %d pos %d | console pos %08X %08X %08X pc %08X %08X %08X | "
                        "console x %08X %08X %08X pc %08X %08X %08X\n", r, lbArgs ? 1 : 0, lbRows ? 1 : 0,
                        lbPos ? 1 : 0, lrRow.mauOut[12], lrRow.mauOut[13], lrRow.mauOut[14], Bits(lM.wAxis.x),
                        Bits(lM.wAxis.y), Bits(lM.wAxis.z), lrRow.mauOut[0], lrRow.mauOut[1], lrRow.mauOut[2],
                        Bits(lM.xAxis.x), Bits(lM.xAxis.y), Bits(lM.xAxis.z));
        }
    }
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "A1 ApplyPitchAboutPointRads asks CreateLookAt once, with the console's eye and flat forward: "
                  "%d of %d rows", KI_PITCH - liArgsBad, KI_PITCH);
    Check(liArgsBad == 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "A2 ApplyPitchAboutPointRads: the three direction rows, every lane, bit for bit: %d of %d rows",
                  KI_PITCH - liRowsBad, KI_PITCH);
    Check(liRowsBad == 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "A3 ApplyPitchAboutPointRads: the position row (pivoted back), every lane, bit for bit: %d of %d rows",
                  KI_PITCH - liPosBad, KI_PITCH);
    Check(liPosBad == 0, lacLabel);

    // ---- ResolveLineTestNearestUsingNormalStrict ---------------------------------------------------------------
    const int KI_STRICT = static_cast<int>(sizeof(kaL2StrictRows) / sizeof(kaL2StrictRows[0]));
    int laRows[2] = { 0, 0 }, laBad[2] = { 0, 0 };
    liShown = 0;
    for (int r = 0; r < KI_STRICT; ++r)
    {
        const L2StrictRow& lrRow = kaL2StrictRows[r];
        CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult lPackage;
        std::memset(&lPackage, 0, sizeof(lPackage));
        lPackage.mPosition = VecFromWords(lrRow.mauHit);
        lPackage.mNormal = VecFromWords(lrRow.mauNormal);
        lPackage.mbIntersection = (lrRow.muIntersection != 0);
        BrnDirector::LineTestNearestPostBox lBox;
        lBox.Construct();
        lBox.WaitForPackage();
        lBox.TakePackage(lPackage);
        Vector3 lPosition = VecFromWords(lrRow.mauPosition);
        const bool lbRet = ResolveLineTestNearestUsingNormalStrict(lBox, lPosition, FromBits(lrRow.muMinDistance));

        bool lbSpecial = false;
        for (int k = 0; k < 3; ++k)
        {
            lbSpecial = lbSpecial || ((lrRow.mauHit[k] & 0x7F800000u) == 0x7F800000u)
                     || ((lrRow.mauNormal[k] & 0x7F800000u) == 0x7F800000u)
                     || ((lrRow.mauPosition[k] & 0x7F800000u) == 0x7F800000u);
        }
        lbSpecial = lbSpecial || ((lrRow.muMinDistance & 0x7F800000u) == 0x7F800000u);
        const bool lbOk = (static_cast<u32>(lbRet ? 1 : 0) == lrRow.muRet) && VecMatches(lrRow.mauOut, lPosition);
        ++laRows[lbSpecial ? 1 : 0];
        laBad[lbSpecial ? 1 : 0] += lbOk ? 0 : 1;
        if (!lbOk && liShown < 8)
        {
            ++liShown;
            std::printf("  strict row %d: ret %u/%d pos console %08X %08X %08X pc %08X %08X %08X\n", r, lrRow.muRet,
                        lbRet ? 1 : 0, lrRow.mauOut[0], lrRow.mauOut[1], lrRow.mauOut[2], Bits(lPosition.x),
                        Bits(lPosition.y), Bits(lPosition.z));
        }
    }
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "S1 ResolveLineTestNearestUsingNormalStrict, finite rows: answer and position bit for bit: %d of %d",
                  laRows[0] - laBad[0], laRows[0]);
    Check(laBad[0] == 0 && laRows[0] > 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "S2 ResolveLineTestNearestUsingNormalStrict, NaN / infinity rows (0x8220CE34 bge: a NaN length "
                  "leaves the position): %d of %d", laRows[1] - laBad[1], laRows[1]);
    Check(laBad[1] == 0 && laRows[1] > 0, lacLabel);

    std::printf("L2CameraUtils: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
