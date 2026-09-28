// L2 CAMCOLLIDE piece 5 (owner's list 2026-09-28): BrnDirector::Camera::Utils::VehicleCollisionPredictor::Update
// @0x822230D8 against the console's own answers (tests/L2VehiclePredictorData.h, the ARTIST words run whole on emu64
// by scratch/OWNERLIST_0927/L2/emu/gen_vcp.py). run_l2_vehicle_predictor.py compiles the revision's
// BrnVehicleCollisionPredictor.cpp beside this file (the revision's header shadowed in). Per row: the camera position
// and velocity, up to four traffic records; the predictor is pre-filled with 0xA5, so an untouched time reads
// 0xA5A5A5A5. A row passes when the flag byte, the time word (a NaN by class: the VMX and SSE payloads differ) and the
// number of asserts fired (IsOrthogonal3x3 / the radii / IsValid of the inverse, CgsLineTests.cpp:51 / 52 / 54) all
// match the console.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <new>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"

#include "L2VehiclePredictorData.h"

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

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool IsSpecialBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u; }

static Vector3 VecFromWords(const u32* lpu)
{
    Vector3 l;
    l.x = FromBits(lpu[0]);
    l.y = FromBits(lpu[1]);
    l.z = FromBits(lpu[2]);
    l.w = FromBits(lpu[3]);
    return l;
}

using BrnTraffic::BrnTrafficIO::TrafficDirectorEntity;
using BrnDirector::Camera::Utils::VehicleCollisionPredictor;

alignas(16) static u8 gaAllVehicleData[sizeof(BrnDirector::AllVehicleData)];
alignas(16) static u8 gaTraffic[sizeof(Array<TrafficDirectorEntity, 32u>)];

static bool RunRow(const L2VcpRow& lrRow, char* lpcWhy, size_t luWhy, bool* lpbSpecial)
{
    Array<TrafficDirectorEntity, 32u>& lrTraffic = *reinterpret_cast<Array<TrafficDirectorEntity, 32u>*>(gaTraffic);
    std::memset(gaTraffic, 0, sizeof(gaTraffic));
    *lpbSpecial = false;
    for (u32 i = 0; i < lrRow.muCount; ++i)
    {
        const L2VcpEntity& lrEntity = kaL2VcpEntities[lrRow.mauEntity[i]];
        TrafficDirectorEntity& lrOut = lrTraffic.maElements[i];
        lrOut.mLocalTransform.xAxis = VecFromWords(&lrEntity.mau[0]);
        lrOut.mLocalTransform.yAxis = VecFromWords(&lrEntity.mau[4]);
        lrOut.mLocalTransform.zAxis = VecFromWords(&lrEntity.mau[8]);
        lrOut.mLocalTransform.wAxis = VecFromWords(&lrEntity.mau[12]);
        lrOut.mVelocity = VecFromWords(&lrEntity.mau[16]);
        lrOut.mHalfExtents = VecFromWords(&lrEntity.mau[20]);
        for (u32 k = 0; k < 24u; ++k)
            if (IsSpecialBits(lrEntity.mau[k])) *lpbSpecial = true;
    }
    lrTraffic.miCount = static_cast<s32>(lrRow.muCount);
    for (u32 k = 0; k < 4u; ++k)
        if (IsSpecialBits(lrRow.mauPosition[k]) || IsSpecialBits(lrRow.mauVelocity[k])) *lpbSpecial = true;

    std::memset(gaAllVehicleData, 0, sizeof(gaAllVehicleData));
    BrnDirector::AllVehicleData& lrAll = *reinterpret_cast<BrnDirector::AllVehicleData*>(gaAllVehicleData);
    lrAll.mpTrafficVehicleArray = &lrTraffic;

    alignas(16) u8 laPredictor[sizeof(VehicleCollisionPredictor)];
    std::memset(laPredictor, 0xA5, sizeof(laPredictor));
    VehicleCollisionPredictor& lrPredictor = *reinterpret_cast<VehicleCollisionPredictor*>(laPredictor);

    giAsserts = 0;
    lrPredictor.Update(lrAll, BrnDirector::VecFloat(1.0f / 60.0f), VecFromWords(lrRow.mauPosition),
                       VecFromWords(lrRow.mauVelocity));
    u32 luFlag = laPredictor[0];
    u32 luTime;
    std::memcpy(&luTime, laPredictor + 4, 4);
    const bool lbFlag = (luFlag == lrRow.muFlag);
    const bool lbTime = IsNanBits(lrRow.muTime) ? IsNanBits(luTime) : (luTime == lrRow.muTime);
    const bool lbAsserts = (static_cast<u32>(giAsserts) == lrRow.muAsserts);
    if (lbFlag && lbTime && lbAsserts)
        return true;
    std::snprintf(lpcWhy, luWhy, "flag %u/%u time %08X/%08X asserts %u/%d", lrRow.muFlag, luFlag, lrRow.muTime, luTime,
                  lrRow.muAsserts, giAsserts);
    return false;
}

int main()
{
    static_assert(sizeof(TrafficDirectorEntity) == 0x70, "the console's 0x70-byte traffic record");
    const int KI_ROWS = static_cast<int>(sizeof(kaL2VcpRows) / sizeof(kaL2VcpRows[0]));
    int laRows[3] = { 0, 0, 0 }, laFails[3] = { 0, 0, 0 };   // [0] finite, no assert; [1] asserting; [2] NaN / inf
    int liHits = 0, liShown = 0;
    for (int r = 0; r < KI_ROWS; ++r)
    {
        const L2VcpRow& lrRow = kaL2VcpRows[r];
        char lacWhy[160] = { 0 };
        bool lbSpecial = false;
        const bool lbOk = RunRow(lrRow, lacWhy, sizeof(lacWhy), &lbSpecial);
        const int liClass = lbSpecial ? 2 : (lrRow.muAsserts != 0 ? 1 : 0);
        ++laRows[liClass];
        if (lrRow.muFlag) ++liHits;
        if (!lbOk)
        {
            ++laFails[liClass];
            if (liShown < 12)
            {
                ++liShown;
                std::printf("  row %d (class %d, %u cars): %s\n", r, liClass, lrRow.muCount, lacWhy);
            }
        }
    }
    char lacLabel[256];
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "F  finite rows (%d hits in all rows): %d of %d match the console -- flag, time bit for bit, no assert",
                  liHits, laRows[0] - laFails[0], laRows[0]);
    Check(laFails[0] == 0 && laRows[0] > 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "A  rows that fire the ellipsoid asserts (sheared / scaled frame, zero radii): %d of %d match -- flag, "
                  "time and the number of asserts", laRows[1] - laFails[1], laRows[1]);
    Check(laFails[1] == 0 && laRows[1] > 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "S  rows with a NaN / infinite input: %d of %d match -- flag, time (a NaN by class), asserts",
                  laRows[2] - laFails[2], laRows[2]);
    Check(laFails[2] == 0 && laRows[2] > 0, lacLabel);
    std::printf("L2VehiclePredictor: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
