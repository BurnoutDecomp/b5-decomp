// L2 CAMCOLLIDE (owner's list 2026-09-27): the three volume line kernels and the two primitive tests they call,
// rwcSphereLineSegIntersect @0x82BA81D8 / rwcCylinderLineSegIntersect @0x82BAF8A0, against the console's own answers.
// Two runners build this file:
//   run_l2_line_kernel_rounding.py  (/DL2_LKN_FINITE_ONLY) the finite rows, bit for bit -- the console's rounding of
//                                   the primitives (vmsum3fp128 rule 1, the vnmsubfp cross and fmsubs / fmadds rule 3);
//   run_l2_line_kernel_nan.py       every row -- also the NaN arms below.
// REVIEW-K DELTA 2 found 13 fcmpu branches of 623c92bf that were spelled `!(a <= b)` /
// `!(a >= b)` where the console's `ble` / `bge` (bc 4,gt / bc 4,lt) are TAKEN on an unordered compare, so a NaN went
// to the other arm (and the primitives' discriminant tests 0x82BA8288 / 0x82BAF95C had the same misreading):
//   rw::collision::SphereVolume::LineSegIntersect   @0x82BA82C8   0x82BA83BC
//   rw::collision::BoxVolume::LineSegIntersect      @0x82BA9478   0x82BA96C4 0x82BA96DC 0x82BA96F0 0x82BA9704
//                                                                 0x82BA99E8 0x82BA99FC 0x82BA9ACC
//   rw::collision::CapsuleVolume::LineSegIntersect  @0x82BAFCF8   0x82BAFEF0 0x82BAFF04 0x82BAFFA4 0x82BAFFC4
//                                                                 0x82BB0078
//   rw::collision::rwcSphereLineSegIntersect        @0x82BA81D8   0x82BA8288
//   rw::collision::rwcCylinderLineSegIntersect      @0x82BAF8A0   0x82BAF95C
// The runners compile the revision's LineSegIntersect.cpp beside this file (the revision's headers shadowed in) and
// replay every row of L2LineKernelNanData.h -- the ARTIST words of each function run whole on emu64
// (scratch/OWNERLIST_0927/L2/emu/gen_linekernel_nan.py): finite rows (the kernels over spheres, boxes and capsules
// with and without a transform and a fatness; the two primitives called directly, the cylinder with arbitrary axes),
// then every base with one lane of pt1 / pt2 / the volume / tm / the fatness set to +-NaN or +-inf, plus random pairs.
//
// A row passes when the PC function returns the console's value and every result lane matches: an unwritten console
// lane (0xA5A5A5A5) must stay unwritten, a NaN lane must be NaN (any payload: the VMX and SSE NaN payloads differ),
// every other lane bit for bit. Special rows are grouped by the site whose NaN arm they reach (the row's site mask),
// so a failure names the branch.
#include "types.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/CapsuleVolume.hpp"
#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/LineSegKernelMath.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"   // TriangleNearestPointRegion (stubbed below)

#include "L2LineKernelNanData.h"

namespace rw { namespace collision {
// LineSegIntersect.cpp's fat triangle walk calls it; none of the three kernels reaches that walk.
s32 TriangleNearestPointRegion(Vec4*, f32*, f32*, Vec4, Vec4, Vec4, Vec4) { return 0; }
} }

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

static const u32 KU_UNWRITTEN = 0xA5A5A5A5u;
#if defined(L2_LKN_FINITE_ONLY)
static const bool KB_FINITE_ONLY = true;      // run_l2_line_kernel_rounding.py: the special rows are not judged
#else
static const bool KB_FINITE_ONLY = false;
#endif

static bool LaneMatches(u32 luConsole, u32 luPc)
{
    if (luConsole == KU_UNWRITTEN)
        return luPc == KU_UNWRITTEN;
    if (IsNanBits(luConsole))
        return IsNanBits(luPc);
    return luConsole == luPc;
}

static bool VecMatches(const u32 (&lauConsole)[4], const Vec4& arPc)
{
    return LaneMatches(lauConsole[0], Bits(arPc.x)) && LaneMatches(lauConsole[1], Bits(arPc.y))
        && LaneMatches(lauConsole[2], Bits(arPc.z)) && LaneMatches(lauConsole[3], Bits(arPc.w));
}

static Vec4 VecFromWords(const u32* lpu)
{
    Vec4 l = { FromBits(lpu[0]), FromBits(lpu[1]), FromBits(lpu[2]), FromBits(lpu[3]) };
    return l;
}

// One call's inputs: the row's base input with the row's overwritten words applied.
struct Call
{
    u32 muKind;
    u32 mauVolume[24];
    bool mbHasTm;
    u32 mauTm[16];
    u32 mauPt1[4];
    u32 mauPt2[4];
    u32 muFatness;
};

static void Overwrite(Call& arCall, u32 luWhere, u32 luWord, u32 luBits)
{
    switch (luWhere)
    {
    case 1: arCall.mauPt1[luWord & 3u] = luBits; break;
    case 2: arCall.mauPt2[luWord & 3u] = luBits; break;
    case 3: arCall.mauVolume[luWord % 24u] = luBits; break;
    case 4: arCall.mauTm[luWord & 15u] = luBits; break;
    case 5: arCall.muFatness = luBits; break;
    default: break;
    }
}

static void BuildCall(const L2KernelRow& lrRow, Call& arCall)
{
    const L2KernelInput& lrIn = kaL2Inputs[lrRow.muInput];
    arCall.muKind = lrIn.muKind;
    std::memcpy(arCall.mauVolume, kaL2Volumes[lrIn.muVolume].mau, sizeof(arCall.mauVolume));
    arCall.mbHasTm = (lrIn.miTm >= 0);
    std::memset(arCall.mauTm, 0, sizeof(arCall.mauTm));
    if (arCall.mbHasTm)
        std::memcpy(arCall.mauTm, kaL2Tms[lrIn.miTm].mau, sizeof(arCall.mauTm));
    std::memcpy(arCall.mauPt1, lrIn.mauPt1, sizeof(arCall.mauPt1));
    std::memcpy(arCall.mauPt2, lrIn.mauPt2, sizeof(arCall.mauPt2));
    arCall.muFatness = lrIn.muFatness;
    Overwrite(arCall, lrRow.muWhere0, lrRow.muWord0, lrRow.muBits0);
    Overwrite(arCall, lrRow.muWhere1, lrRow.muWord1, lrRow.muBits1);
}

// Kinds 3 / 4: the primitive called directly. The console's return is the s32; its Fraction lands in position[0..1].
static bool RunPrimitive(const L2KernelRow& lrRow, const Call& arCall, char* lpcWhy, size_t luWhy)
{
    Fraction lDist;
    std::memset(&lDist, 0xA5, sizeof(lDist));
    const Vec4 lvStart = VecFromWords(arCall.mauPt1);
    const Vec4 lvDelta = VecFromWords(arCall.mauPt2);
    const Vec4 lvCentre = VecFromWords(&arCall.mauVolume[0]);
    const Vec4 lvAxis = VecFromWords(&arCall.mauVolume[4]);
    s32 liRet;
    if (arCall.muKind == 3)
    {
        liRet = rwcSphereLineSegIntersect(&lDist, &lvStart, &lvDelta, &lvCentre, FromBits(arCall.mauVolume[8]));
    }
    else
    {
        liRet = rwcCylinderLineSegIntersect(&lDist, FromBits(arCall.mauVolume[9]), FromBits(arCall.mauVolume[8]),
                                            static_cast<s32>(arCall.mauVolume[10]),
                                            static_cast<s32>(arCall.mauVolume[11]), lvStart, lvDelta, lvCentre, lvAxis);
    }
    const bool lbRetOk = (static_cast<u32>(liRet) == lrRow.muRet);
    const bool lbNum = LaneMatches(lrRow.mauPosition[0], Bits(lDist.num));
    const bool lbDen = LaneMatches(lrRow.mauPosition[1], Bits(lDist.den));
    if (lbRetOk && lbNum && lbDen)
        return true;
    std::snprintf(lpcWhy, luWhy, "ret %08X/%08X | num %08X pc %08X, den %08X pc %08X", lrRow.muRet,
                  static_cast<u32>(liRet), lrRow.mauPosition[0], Bits(lDist.num), lrRow.mauPosition[1],
                  Bits(lDist.den));
    return false;
}

// One row through the PC function; true when every output agrees with the console's.
static bool RunRow(const L2KernelRow& lrRow, char* lpcWhy, size_t luWhy)
{
    Call lCall;
    BuildCall(lrRow, lCall);
    if (lCall.muKind >= 3)
        return RunPrimitive(lrRow, lCall, lpcWhy, luWhy);

    alignas(16) u8 laVolume[96];
    std::memcpy(laVolume, lCall.mauVolume, sizeof(laVolume));
    Vec4 laTm[4];
    for (int i = 0; i < 4; ++i)
    {
        laTm[i] = VecFromWords(&lCall.mauTm[4 * i]);
    }
    const Vec4 lvPt1 = VecFromWords(lCall.mauPt1);
    const Vec4 lvPt2 = VecFromWords(lCall.mauPt2);
    const Vec4* lpTm = lCall.mbHasTm ? laTm : 0;
    const f32 lfFat = FromBits(lCall.muFatness);

    VolumeLineSegIntersectResult lResult;
    std::memset(&lResult, 0xA5, sizeof(lResult));

    RwBool lbRet = 0;
    const void* lpVolumeObject = 0;
    if (lCall.muKind == 0)
    {
        SphereVolume lSphere;
        std::memcpy(&lSphere, laVolume, sizeof(laVolume));
        lpVolumeObject = &lSphere;
        lbRet = lSphere.LineSegIntersect(lvPt1, lvPt2, lpTm, lResult, lfFat);
    }
    else if (lCall.muKind == 1)
    {
        BoxVolume lBox;
        std::memcpy(&lBox, laVolume, sizeof(laVolume));
        lpVolumeObject = &lBox;
        lbRet = lBox.LineSegIntersect(lvPt1, lvPt2, lpTm, lResult, lfFat);
    }
    else
    {
        CapsuleVolume lCapsule;
        std::memcpy(&lCapsule, laVolume, sizeof(laVolume));
        lpVolumeObject = &lCapsule;
        lbRet = lCapsule.LineSegIntersect(lvPt1, lvPt2, lpTm, lResult, lfFat);
    }

    // result.v: the console writes the volume's address (muV = 1) or leaves the sentinel (muV = 0).
    const bool lbVWritten = (lResult.v != static_cast<uintptr_t>(0xA5A5A5A5A5A5A5A5ull));
    const bool lbVOk = (lrRow.muV == 0) ? !lbVWritten
                                        : (lResult.v == reinterpret_cast<uintptr_t>(lpVolumeObject));
    const bool lbRetOk = (static_cast<u32>(lbRet ? 1 : 0) == lrRow.muRet);
    const bool lbPos = VecMatches(lrRow.mauPosition, lResult.position);
    const bool lbNrm = VecMatches(lrRow.mauNormal, lResult.normal);
    const bool lbVp  = VecMatches(lrRow.mauVolParam, lResult.volParam);
    const bool lbLp  = LaneMatches(lrRow.muLineParam, Bits(lResult.lineParam));
    if (lbRetOk && lbVOk && lbPos && lbNrm && lbVp && lbLp)
        return true;
    std::snprintf(lpcWhy, luWhy,
                  "ret %u/%d v %d pos %d nrm %d vp %d lp %d | console lp %08X pc %08X, pos %08X,%08X,%08X pc %08X,%08X,%08X",
                  lrRow.muRet, lbRet ? 1 : 0, lbVOk ? 1 : 0, lbPos ? 1 : 0, lbNrm ? 1 : 0, lbVp ? 1 : 0, lbLp ? 1 : 0,
                  lrRow.muLineParam, Bits(lResult.lineParam), lrRow.mauPosition[0], lrRow.mauPosition[1],
                  lrRow.mauPosition[2], Bits(lResult.position.x), Bits(lResult.position.y), Bits(lResult.position.z));
    return false;
}

int main()
{
    const int KI_SITES = static_cast<int>(sizeof(kapcL2Sites) / sizeof(kapcL2Sites[0]));
    const int KI_ROWS  = static_cast<int>(sizeof(kaL2KernelRows) / sizeof(kaL2KernelRows[0]));

    int laNanRows[16] = { 0 };
    int laNanFails[16] = { 0 };
    int laFiniteRows[2] = { 0, 0 }, laFiniteFails[2] = { 0, 0 };   // [0] the three kernels, [1] the two primitives
    int liOtherRows = 0, liOtherFails = 0;
    int liShown = 0;
    static char lacFiniteLines[12][400];
    int liFiniteShown = 0;
    for (int r = 0; r < KI_ROWS; ++r)
    {
        const L2KernelRow& lrRow = kaL2KernelRows[r];
        char lacWhy[320] = { 0 };
        const bool lbOk = RunRow(lrRow, lacWhy, sizeof(lacWhy));
        const bool lbFinite = (lrRow.muWhere0 == 0);          // a row that overwrites no input word
        const u32 luKind = kaL2Inputs[lrRow.muInput].muKind;
        bool lbAnyNanSite = false;
        for (int s = 0; s < KI_SITES; ++s)
        {
            if ((lrRow.muSites >> (2 * s)) & 2u)
            {
                lbAnyNanSite = true;
                ++laNanRows[s];
                if (!lbOk) ++laNanFails[s];
            }
        }
        if (lbFinite)
        {
            ++laFiniteRows[luKind >= 3 ? 1 : 0];
            if (!lbOk) ++laFiniteFails[luKind >= 3 ? 1 : 0];
        }
        else if (!lbAnyNanSite)
        {
            ++liOtherRows;
            if (!lbOk) ++liOtherFails;
        }
        // The runner keeps only the tail of the output: the finite failures are held back and printed last.
        if (!lbOk && lbFinite && liFiniteShown < 12)
        {
            std::snprintf(lacFiniteLines[liFiniteShown++], sizeof(lacFiniteLines[0]),
                          "  row %d (input %u, kind %u) sites %08X: %s", r, lrRow.muInput, luKind, lrRow.muSites, lacWhy);
        }
        else if (!lbOk && !lbFinite && liShown < 16 && !KB_FINITE_ONLY)
        {
            ++liShown;
            std::printf("  row %d (input %u, kind %u, word %u:%u = %08X, word %u:%u = %08X) sites %08X: %s\n", r,
                        lrRow.muInput, luKind, lrRow.muWhere0, lrRow.muWord0, lrRow.muBits0, lrRow.muWhere1,
                        lrRow.muWord1, lrRow.muBits1, lrRow.muSites, lacWhy);
        }
    }
    for (int i = 0; i < liFiniteShown; ++i)
    {
        std::printf("%s\n", lacFiniteLines[i]);
    }

    char lacLabel[256];
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "F1 finite kernel rows (sphere / box / capsule): %d of %d match the console bit for bit",
                  laFiniteRows[0] - laFiniteFails[0], laFiniteRows[0]);
    Check(laFiniteFails[0] == 0 && laFiniteRows[0] > 0, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "F2 finite primitive rows (rwcSphere / rwcCylinder, arbitrary axes): %d of %d match the console bit "
                  "for bit", laFiniteRows[1] - laFiniteFails[1], laFiniteRows[1]);
    Check(laFiniteFails[1] == 0 && laFiniteRows[1] > 0, lacLabel);
#if !defined(L2_LKN_FINITE_ONLY)
    for (int s = 0; s < KI_SITES; ++s)
    {
        if (laNanRows[s] == 0)
        {
            std::snprintf(lacLabel, sizeof(lacLabel),
                          "N%-2d %s: no row reaches its NaN arm (unreachable through the inputs)", s, kapcL2Sites[s]);
            std::printf("NOTE  %s\n", lacLabel);
            continue;
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "N%-2d %s: %d of %d rows that reach its NaN arm match the console",
                      s, kapcL2Sites[s], laNanRows[s] - laNanFails[s], laNanRows[s]);
        Check(laNanFails[s] == 0, lacLabel);
    }
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "O  special rows that reach no listed NaN arm: %d of %d match the console", liOtherRows - liOtherFails,
                  liOtherRows);
    Check(liOtherFails == 0, lacLabel);
#else
    (void)laNanRows; (void)laNanFails; (void)liOtherRows; (void)liOtherFails; (void)KI_SITES; (void)liShown;
#endif

    std::printf("L2LineKernelNan: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
