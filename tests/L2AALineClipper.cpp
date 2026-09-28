// L2 CAMCOLLIDE stage (c) (owner's list 2026-09-28): rw::collision::AALineClipper -- the clip the fat cylinder's torus
// arm (CylinderVolume::FatLineSegIntersect @0x82BAEB10, `bl` @0x82BAF2D8) and KdTreeLineQuery build -- against the
// console's own answers:
//   AALineClipper::Init           @0x828AED60   (kind 0: the caller's seed)
//   AALineClipper::AALineClipper  @0x82BAE3C8   (kind 1: the constructor builds the seed and calls Init)
// run_l2_aalineclipper.py compiles the revision's AALineClipper.cpp beside this file (the revision's headers shadowed
// in) and replays every row of L2AALineClipperData.h -- the ARTIST words run whole on emu64
// (scratch/OWNERLIST_0927/L2/emu/gen_aalineclipper.py). A row passes when the call returns the object and all 16
// words of the object match: a NaN lane must be NaN (any payload: the VMX and SSE payloads differ), every other lane
// bit for bit.
#include "types.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "vendor/renderware/collision/AALineClipper.hpp"

#include "L2AALineClipperData.h"

using namespace rw::collision;

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }

static AALineClipper::Vec4 VecFromWords(const u32 (&lau)[4])
{
    AALineClipper::Vec4 l = { FromBits(lau[0]), FromBits(lau[1]), FromBits(lau[2]), FromBits(lau[3]) };
    return l;
}

static bool IsSpecialRow(const L2AalcRow& lrRow)
{
    const u32* lapu[5] = { lrRow.mauStart, lrRow.mauEnd, lrRow.mauSeed, lrRow.mauBoxMin, lrRow.mauBoxMax };
    for (int v = 0; v < 5; ++v)
        for (int i = 0; i < 4; ++i)
            if ((lapu[v][i] & 0x7F800000u) == 0x7F800000u)
                return true;
    return false;
}

static bool RunRow(const L2AalcRow& lrRow, char* lpcWhy, size_t luWhy)
{
    const AALineClipper::Vec4 lvStart = VecFromWords(lrRow.mauStart);
    const AALineClipper::Vec4 lvEnd = VecFromWords(lrRow.mauEnd);
    AALineClipper::Aabb lBox;
    lBox.mMin = VecFromWords(lrRow.mauBoxMin);
    lBox.mMax = VecFromWords(lrRow.mauBoxMax);

    alignas(16) u8 laStorage[sizeof(AALineClipper)];
    std::memset(laStorage, 0xA5, sizeof(laStorage));
    AALineClipper* lpClipper = reinterpret_cast<AALineClipper*>(laStorage);
    const AALineClipper* lpReturned = lpClipper;
    if (lrRow.muKind == 0)
    {
        const AALineClipper::Vec4 lvSeed = VecFromWords(lrRow.mauSeed);
        lpReturned = lpClipper->Init(lvStart, lvEnd, lvSeed, &lBox);
    }
    else
    {
        lpClipper = new (laStorage) AALineClipper(lvStart, lvEnd, &lBox);
    }
    u32 lauPc[16];
    std::memcpy(lauPc, laStorage, sizeof(lauPc));
    if (lpReturned != lpClipper)
    {
        std::snprintf(lpcWhy, luWhy, "Init did not return this");
        return false;
    }
    for (int k = 0; k < 16; ++k)
    {
        const u32 luConsole = lrRow.mauOut[k];
        const bool lbOk = IsNanBits(luConsole) ? IsNanBits(lauPc[k]) : (luConsole == lauPc[k]);
        if (!lbOk)
        {
            std::snprintf(lpcWhy, luWhy, "word %d (+0x%02X): console %08X pc %08X", k, 4 * k, luConsole, lauPc[k]);
            return false;
        }
    }
    return true;
}

int main()
{
    static_assert(sizeof(AALineClipper) == 0x40, "AALineClipper is four 16-byte rows");
    const int KI_ROWS = static_cast<int>(sizeof(kaL2AalcRows) / sizeof(kaL2AalcRows[0]));
    int laRows[3] = { 0, 0, 0 }, laFails[3] = { 0, 0, 0 };   // [0] Init finite, [1] ctor finite, [2] special
    int liShown = 0;
    for (int r = 0; r < KI_ROWS; ++r)
    {
        const L2AalcRow& lrRow = kaL2AalcRows[r];
        char lacWhy[200] = { 0 };
        const bool lbOk = RunRow(lrRow, lacWhy, sizeof(lacWhy));
        const int liClass = IsSpecialRow(lrRow) ? 2 : (lrRow.muKind == 0 ? 0 : 1);
        ++laRows[liClass];
        if (!lbOk)
        {
            ++laFails[liClass];
            if (liShown < 12)
            {
                ++liShown;
                std::printf("  row %d (kind %u, start %08X,%08X,%08X,%08X end %08X,%08X,%08X,%08X): %s\n", r,
                            lrRow.muKind, lrRow.mauStart[0], lrRow.mauStart[1], lrRow.mauStart[2], lrRow.mauStart[3],
                            lrRow.mauEnd[0], lrRow.mauEnd[1], lrRow.mauEnd[2], lrRow.mauEnd[3], lacWhy);
            }
        }
    }
    char lacLabel[256];
    std::snprintf(lacLabel, sizeof(lacLabel), "I  Init @0x828AED60, finite rows: %d of %d match the console bit for bit",
                  laRows[0] - laFails[0], laRows[0]);
    Check(laFails[0] == 0 && laRows[0] > 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "C  the constructor @0x82BAE3C8, finite rows: %d of %d match the console bit for bit",
                  laRows[1] - laFails[1], laRows[1]);
    Check(laFails[1] == 0 && laRows[1] > 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "S  rows with a NaN / infinite input lane: %d of %d match the console (a NaN by class)",
                  laRows[2] - laFails[2], laRows[2]);
    Check(laFails[2] == 0 && laRows[2] > 0, lacLabel);
    std::printf("L2AALineClipper: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
