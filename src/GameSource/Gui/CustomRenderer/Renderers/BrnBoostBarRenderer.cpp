#include "BrnBoostBarRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::BoostBarRenderer::BoostBarRenderer        @ 0x827DF4F0  (EXECUTED in the boot trace)
//   BrnGui::BoostBarRenderer::GetInnerBoostBarColour  @ 0x824EC750
//   BrnGui::BoostBarRenderer::GetOuterBoostBarColour  @ 0x824EC7D0
//
// Only these three functions are in scope, so this TU recovers a MINIMAL slice of the
// (very large) guest object — see the header for the omitted-member FLAG list.
//
// The constructor primes a set of bounding-box / extent accumulators with FLT_MAX
// sentinels and zeros, installs a vtable pointer, fills a block of pointer slots with a
// shared default-element pointer, zero-initialises a band of state arrays, clears six
// billboard renderers in a loop, and finally sets a trailing index to -1. It does NOT
// touch the boost-colour arrays or meCurrentBoostType (the X360 pseudocode begins writing
// past them), so those are left uninitialised here — faithful to the asm.
//
// The two getters return the boost-type-indexed colour by value (each Vector3 is a 16-byte
// SIMD register on the guest, hence the asm's 16*(bt+5) / 16*(bt+2) element addressing) and
// assert that meCurrentBoostType is a real boost type first. The static CGS_ASSERT supplies
// __FILE__/__LINE__; the baked X360 line numbers (641 inner / 658 outer) are NOT reproduced.

namespace BrnGui
{
namespace
{
    // FLAG: vtable pointer recovered from the data segment (ctor writes *this = &off_820CF8A0).
    // 64-bit literal so the guest 32-bit address widens cleanly to the host void* (no C4312).
    void* const KP_Vtable = reinterpret_cast<void*>(0x820CF8A0ull);

    // FLAG: shared default-element pointer (&unk_820D0000) the ctor stores into ~20 slots.
    void* const KP_DefaultElement = reinterpret_cast<void*>(0x820D0000ull);

    // -FLT_MAX / +FLT_MAX as written by the X360 ctor (bounding-box / extent accumulators).
    const f32 KF_ExtentNeg = -3.4028235e38f;
    const f32 KF_ExtentPos =  3.4028235e38f;
}

BoostBarRenderer::BoostBarRenderer()
{
    // Vtable install: *this = &off_820CF8A0.
    mpVtable = KP_Vtable;

    // Extent accumulators primed with -FLT_MAX (guest +212/+216/+1388 and the +1404..+1456
    // group), +FLT_MAX (guest +1392), and 0.0f (guest +1376/+1380/+1384). Each write below
    // reproduces the exact constant the pseudocode stores into the corresponding field.
    mfExtentNegA = KF_ExtentNeg; // guest +212
    mfExtentNegB = KF_ExtentNeg; // guest +216
    mfExtentNegC = KF_ExtentNeg; // guest +1388
    mfExtentPos  = KF_ExtentPos; // guest +1392

    mav3ExtentMinZero[0] = 0.0f; // guest +1376
    mav3ExtentMinZero[1] = 0.0f; // guest +1380
    mav3ExtentMinZero[2] = 0.0f; // guest +1384

    // guest +1404/+1408/+1420/+1424/+1436/+1440/+1452/+1456 — all -FLT_MAX.
    for (int i = 0; i < 8; ++i)
    {
        maExtentNegGroup[i] = KF_ExtentNeg;
    }

    // Default-element pointer fill (guest +1480..+1568): four groups of five dwords on a
    // 24-byte stride. The pseudocode writes each group with a duplicate-first-write; the 6th
    // dword of each group (+1500/+1524/+1548) is a GAP the guest never touches, so only the
    // five real slots per group are written (gap-aware, matching maZeroGroups below).
    for (int g = 0; g < 4; ++g)
    {
        for (int i = 0; i < 5; ++i)
        {
            mapDefaultElement[g][i] = KP_DefaultElement;
        }
    }
    // FLAG: guest +1560 (group 3, slot 2) stores the uninitialised `back_chain` stack slot,
    // not the default pointer — a decompiler artifact. Modelled as 0.
    mapDefaultElement[3][2] = 0;

    // Zero-init state band (guest +1576..+1784): nine groups of five dwords.
    for (int g = 0; g < 9; ++g)
    {
        for (int i = 0; i < 5; ++i)
        {
            maZeroGroups[g][i] = 0;
        }
    }

    // Clear the six billboard renderers (guest do-while, v1 = 5 down through -1 -> six
    // iterations; v2 starts at guest +10036 and strides 2067 dwords per element). Each
    // element's leading ten dwords are zeroed.
    for (int b = 0; b < 6; ++b)
    {
        for (int i = 0; i < 10; ++i)
        {
            mBillboardRenderer[b].maClearedFields[i] = 0;
        }
    }

    // Trailing sentinel set to -1 (guest +53456).
    miTrailingSentinel = -1;
}

Vector3 BoostBarRenderer::GetInnerBoostBarColour()
{
    CGS_ASSERT(meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT && meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE,
               "( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT ) && ( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE )");
    // asm element offset 16*(bt+5): inner colours are the array the boost type indexes.
    return mav3BoostInnerColours[meCurrentBoostType];
}

Vector3 BoostBarRenderer::GetOuterBoostBarColour()
{
    CGS_ASSERT(meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT && meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE,
               "( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_COUNT ) && ( meCurrentBoostType != BrnWorld::E_BOOST_TYPE_NONE )");
    // asm element offset 16*(bt+2): outer colours are the array the boost type indexes.
    return mav3BoostOuterColours[meCurrentBoostType];
}
}
