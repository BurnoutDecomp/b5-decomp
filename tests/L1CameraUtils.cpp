// L1 (owner's list 2026-09-28, piece 6): the two leaf helpers the car-attached camera's frustum resolver needs, against
// the console's own answers (tests/L1CameraUtilsData.h, the ARTIST words run whole on emu64 by
// scratch/OWNERLIST_0927/L1/emu/gen_camutils_l1.py):
//   XMVectorTan                                                       @0x821F0788   (src/SDKs/XboxMath/XMVectorTan.h)
//   BrnDirector::Camera::Utils::ResolveLineTestNearestUsingDisplacementAndVector @0x8220CEB0   (CameraUtils.cpp)
//   XMVectorCos                                                       @0x821F06B0   (src/SDKs/XboxMath/XMVectorCos.h,
//                                                                                    piece 6b: the roof over a car)
// run_l1_camera_utils.py puts the revision's XMVectorTan.h in the include path and extracts the revision's lane helpers
// and the resolve body from CameraUtils.cpp into l1_camutils_bodies.inc.
//   T1  every XMVectorTan row, bit for bit (a NaN lane by class: the VMX and SSE NaN payloads differ)
//   T2  every XMVectorCos row, the same way (L1_NO_COS: the revision has no XMVectorCos.h -- counted failed)
//   D1  every resolve row's answer       D2  every resolve row's position, bit for bit       D3  the tripwires fired
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Utils/BrnPostBox.h"
#include "GameSource/Director/Utils/BrnDirectorPostOfficeTypes.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "SDKs/XboxMath/XMVectorTan.h"
#ifndef L1_NO_COS
#include "SDKs/XboxMath/XMVectorCos.h"
#endif

#include "L1CameraUtilsData.h"

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

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
#include "l1_camutils_bodies.inc"
}
}
}

static int giChecks = 0;
static int giFailures = 0;

static void Check(bool lbOk, const char* lpcName)
{
    ++giChecks;
    if (!lbOk)
        ++giFailures;
    std::printf("%s  %s\n", lbOk ? "PASS" : "FAIL", lpcName);
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool WordMatches(u32 luConsole, u32 luPc) { return IsNanBits(luConsole) ? IsNanBits(luPc) : luConsole == luPc; }

static Vector3 VecFromWords(const u32* lpu)
{
    Vector3 l;
    l.x = FromBits(lpu[0]);
    l.y = FromBits(lpu[1]);
    l.z = FromBits(lpu[2]);
    l.w = FromBits(lpu[3]);
    return l;
}

int main()
{
    char lacName[160];

    // T1
    int liBadTan = 0;
    const u32 luTanRows = sizeof(kaL1TanRows) / sizeof(kaL1TanRows[0]);
    for (u32 i = 0; i < luTanRows; ++i)
    {
        const u32 luPc = Bits(XboxMath::XMVectorTan(FromBits(kaL1TanRows[i].muX)));
        if (!WordMatches(kaL1TanRows[i].mauOut[0], luPc))
        {
            if (liBadTan < 8)
                std::printf("  tan(%08X): console %08X pc %08X\n", kaL1TanRows[i].muX, kaL1TanRows[i].mauOut[0], luPc);
            ++liBadTan;
        }
    }
    std::snprintf(lacName, sizeof(lacName), "T1 XMVectorTan @0x821F0788, every row (%d of %u wrong)", liBadTan, luTanRows);
    Check(liBadTan == 0, lacName);

    // T2
#ifndef L1_NO_COS
    int liBadCos = 0;
    const u32 luCosRows = sizeof(kaL1CosRows) / sizeof(kaL1CosRows[0]);
    for (u32 i = 0; i < luCosRows; ++i)
    {
        const u32 luPc = Bits(XboxMath::XMVectorCos(FromBits(kaL1CosRows[i].muX)));
        if (!WordMatches(kaL1CosRows[i].mauOut[0], luPc))
        {
            if (liBadCos < 8)
                std::printf("  cos(%08X): console %08X pc %08X\n", kaL1CosRows[i].muX, kaL1CosRows[i].mauOut[0], luPc);
            ++liBadCos;
        }
    }
    std::snprintf(lacName, sizeof(lacName), "T2 XMVectorCos @0x821F06B0, every row (%d of %u wrong)", liBadCos, luCosRows);
    Check(liBadCos == 0, lacName);
#else
    Check(false, "T2 XMVectorCos @0x821F06B0: not buildable (the revision has no src/SDKs/XboxMath/XMVectorCos.h)");
#endif

    // D1..D3
    int liBadRet = 0, liBadPos = 0, liBadAsserts = 0;
    const u32 luRows = sizeof(kaL1DisplacementRows) / sizeof(kaL1DisplacementRows[0]);
    for (u32 i = 0; i < luRows; ++i)
    {
        const L1DisplacementRow& lrRow = kaL1DisplacementRows[i];
        BrnDirector::LineTestNearestPostBox lBox;
        std::memset(&lBox, 0, sizeof(lBox));
        lBox.meState = static_cast<BrnDirector::LineTestNearestPostBox::EState>(lrRow.muState);
        lBox.mPackage.mPosition = VecFromWords(lrRow.mauHit);
        lBox.mPackage.mNormal = VecFromWords(lrRow.mauNormal);
        const u8 luIntersection = static_cast<u8>(lrRow.muIntersection);
        std::memcpy(&lBox.mPackage.mbIntersection, &luIntersection, 1);
        Vector3 lPosition = VecFromWords(lrRow.mauPosition);
        BrnDirector::VecFloat lvMin;
        for (int k = 0; k < 4; ++k)
            lvMin.maLanes[k] = FromBits(lrRow.mauMinDistance[k]);

        const int liAsserts0 = giAsserts;
        const bool lbRet = BrnDirector::Camera::Utils::ResolveLineTestNearestUsingDisplacementAndVector(
            lBox, VecFromWords(lrRow.mauTestPoint), lPosition, VecFromWords(lrRow.mauVector), lvMin);
        const int liFired = giAsserts - liAsserts0;

        const bool lbRetOk = (lbRet ? 1u : 0u) == lrRow.muRet;
        const bool lbPosOk = WordMatches(lrRow.mauOut[0], Bits(lPosition.x)) && WordMatches(lrRow.mauOut[1], Bits(lPosition.y))
                          && WordMatches(lrRow.mauOut[2], Bits(lPosition.z)) && WordMatches(lrRow.mauOut[3], Bits(lPosition.w));
        const bool lbAssertsOk = static_cast<u32>(liFired) == lrRow.muAsserts;
        liBadRet += lbRetOk ? 0 : 1;
        liBadPos += lbPosOk ? 0 : 1;
        liBadAsserts += lbAssertsOk ? 0 : 1;
        if (!(lbRetOk && lbPosOk && lbAssertsOk) && (liBadRet + liBadPos + liBadAsserts) <= 8)
            std::printf("  row %u: ret console %u pc %d | pos console %08X %08X %08X %08X pc %08X %08X %08X %08X | "
                        "asserts console %u pc %d\n", i, lrRow.muRet, lbRet ? 1 : 0, lrRow.mauOut[0], lrRow.mauOut[1],
                        lrRow.mauOut[2], lrRow.mauOut[3], Bits(lPosition.x), Bits(lPosition.y), Bits(lPosition.z),
                        Bits(lPosition.w), lrRow.muAsserts, liFired);
    }
    std::snprintf(lacName, sizeof(lacName), "D1 ResolveLineTestNearestUsingDisplacementAndVector @0x8220CEB0, the answer "
                  "(%d of %u wrong)", liBadRet, luRows);
    Check(liBadRet == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "D2 ... the position, every lane (%d of %u wrong)", liBadPos, luRows);
    Check(liBadPos == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "D3 ... the GetPackage tripwires fired (%d of %u wrong)", liBadAsserts, luRows);
    Check(liBadAsserts == 0, lacName);

    std::printf("L1CameraUtils: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
