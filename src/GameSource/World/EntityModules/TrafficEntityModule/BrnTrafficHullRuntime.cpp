// ============================================================================
// BrnTraffic::HullRuntime -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnTraffic::HullRuntime::Construct               @ 0x82751428
//   BrnTraffic::HullRuntime::Prepare                 @ 0x82751438
//   BrnTraffic::HullRuntime::Release                 @ 0x82751578
//   BrnTraffic::HullRuntime::SetFirstParamInSection  @ 0x82706408
//   BrnTraffic::HullRuntime::SetStoplineRed          @ 0x82706630
//   BrnTraffic::HullRuntime::GetFirstParamInSection  @ 0x82706768
//   BrnTraffic::HullRuntime::IsStoplineRed           @ 0x827068A0
// plus Destruct, which ARTIST inlines everywhere and which is ported verbatim from the
// Feb-2007 body (references/.../BrnTrafficHullRuntime.cpp).
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"  // KU_INVALID_PARAM
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
// [stuntrace waveB CLOSURE round, 2026-08-26] BrnTraffic::JunctionLogicBox + TrafficLightController
// now have a real header home and are INCLUDED, not re-declared. Until this round both types were
// declared in this .cpp under a banner reading "RETIRE THIS BLOCK when BrnJunctionLogicBox.h lands:
// delete both types and include that header instead"; that block is now gone and this include is
// the instruction carried out. The reconstruction (stride 0x120, muNumStates +0x34, the two
// start-data indices at +0x3C/+0x40) moved VERBATIM, along with its layout pins -- which are now
// namespace-scope static_asserts that fire in every including TU rather than one never-called
// JunctionLogicBox::_AssertLayout member. The move was forced by
// Hull::GetLightTriggerStartDataForJunction @0x82752900 (SharedClasses/Traffic/BrnTrafficHull.cpp),
// which reads this record and could not be written while the type lived in a GameSource .cpp.
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"

#include <cstddef>   // offsetof, for the never-called HullRuntime::_AssertLayout pin below

namespace BrnTraffic
{
// @ 0x82751428
void HullRuntime::Construct()
{
    mbPrepared = false;
}

// @ 0x82751438 -- seed the per-hull scratch from the streamed hull record. Feb-2007 body
// (BrnTrafficHullRuntime.cpp:60) matches the X360 loop for loop; the assert lines the XEX
// bakes are 70 (lpHull) and 71 (mbPrepared == false).
void HullRuntime::Prepare(const Hull* lpHull, u16 luHull)
{
    CGS_ASSERT(lpHull != nullptr, "lpHull");
    CGS_ASSERT(mbPrepared == false, "mbPrepared == false");

    for (u32 luStopline = 0; luStopline < lpHull->muNumStoplines; ++luStopline)
    {
        mabStoplineRedState[luStopline] = false;
    }

    for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
    {
        mauFirstParamInSection[luSection] = static_cast<u16>(KU_INVALID_PARAM);
    }

    for (u32 luJunction = 0; luJunction < lpHull->muNumJunctions; ++luJunction)
    {
        mafJunctionStateChangeTimes[luJunction] = 0.0f;
        mauJunctionCurrentStates[luJunction] =
            static_cast<u8>(lpHull->mpaJunctions[luJunction].GetNumStates() - 1);
    }

    mbPrepared = true;
    muHullIndex = luHull;
    muNumSectionsInHull = lpHull->muNumSections;
    muNumStoplinesInHull = lpHull->muNumStoplines;
}

// @ 0x82751578
void HullRuntime::Release()
{
    CGS_ASSERT(mbPrepared == true, "mbPrepared == true");
    mbPrepared = false;
}

// Feb-2007 BrnTrafficHullRuntime.cpp:125 -- assert-only; ARTIST inlines it to nothing in a
// release-shaped build, which is why it has no ledger address.
void HullRuntime::Destruct()
{
    CGS_ASSERT(mbPrepared == false, "mbPrepared == false");
}

// @ 0x82706408 -- asserts baked at BrnTrafficHullRuntime.h:122/:123/:130. The third is the
// linked-list integrity check: the caller hands in the value it believes the section head
// holds, and the console refuses to relink if the table disagrees. The X360 indexes the table
// as (luSection + 0x28)*2 bytes from `this`, i.e. &mauFirstParamInSection[luSection].
void HullRuntime::SetFirstParamInSection(u32 luSection, u16 luParam, u16 luOldParam)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luSection < muNumSectionsInHull, "luSection < muNumSectionsInHull");
    CGS_ASSERT(mauFirstParamInSection[luSection] == luOldParam,
               "mauFirstParamInSection[luSection] == luOldParam");

    mauFirstParamInSection[luSection] = luParam;
}

// @ 0x82706630 -- asserts baked at BrnTrafficHullRuntime.h:141/:142 (line 0x8D and its
// successor). Tail is `add r11, luStopline, this ; stb lbRed, 0x250(r11)`.
void HullRuntime::SetStoplineRed(u32 luStopline, bool lbRed)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luStopline < muNumStoplinesInHull, "luStopline < muNumStoplinesInHull");

    mabStoplineRedState[luStopline] = lbRed;
}

// @ 0x82706768 -- the section's first-param head index. The asm reads the u16 at
// this + 2*(luSection+40); the table base is byte +0x50 == 2*40. The X360 bounds assert
// streams a rich message ("Out of range section in hull runtime for hull <i>, section was
// <n>, max is <m>") through CgsDev stream operators this TU does not own; the stringized
// CGS_ASSERT carries the same guard.
u16 HullRuntime::GetFirstParamInSection(u32 luSection) const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luSection < muNumSectionsInHull, "luSection < muNumSectionsInHull");
    return mauFirstParamInSection[luSection];
}

// @ 0x827068A0 -- same shape and same rich-vs-stringized assert note as the accessor above;
// the asm reads the byte at this + 0x250 + luStopline.
bool HullRuntime::IsStoplineRed(u32 luStopline) const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luStopline < muNumStoplinesInHull, "luStopline < muNumStoplinesInHull");
    return mabStoplineRedState[luStopline];
}

// Layout pin. NEVER CALLED. Absolute offsets are legitimate because the record holds NO
// POINTERS (five plain arrays, four integer scalars), so console offsets are host offsets.
// BrnTrafficHullRuntime.h's banner names which X360 function attests each one.
void HullRuntime::_AssertLayout()
{
    static_assert(offsetof(HullRuntime, mafJunctionStateChangeTimes) == 0x000,
                  "mafJunctionStateChangeTimes");
    static_assert(offsetof(HullRuntime, mauJunctionCurrentStates) == 0x040,
                  "mauJunctionCurrentStates");
    static_assert(offsetof(HullRuntime, mauFirstParamInSection) == 0x050,
                  "mauFirstParamInSection");
    static_assert(offsetof(HullRuntime, mabStoplineRedState) == 0x250, "mabStoplineRedState");
    static_assert(offsetof(HullRuntime, mauSectionSpanVehicleCount) == 0x290,
                  "mauSectionSpanVehicleCount");
    static_assert(offsetof(HullRuntime, muHullIndex) == 0x490, "muHullIndex");
    static_assert(offsetof(HullRuntime, mbPrepared) == 0x492, "mbPrepared");
    static_assert(offsetof(HullRuntime, muNumSectionsInHull) == 0x493, "muNumSectionsInHull");
    static_assert(offsetof(HullRuntime, muNumStoplinesInHull) == 0x494, "muNumStoplinesInHull");
    static_assert(sizeof(HullRuntime) == 0x498, "sizeof(HullRuntime)");
}
}
