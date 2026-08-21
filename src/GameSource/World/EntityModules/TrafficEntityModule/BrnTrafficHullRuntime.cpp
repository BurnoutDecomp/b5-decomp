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

#include <cstddef>   // offsetof, for the never-called _AssertLayout pin below

namespace BrnTraffic
{
// ---------------------------------------------------------------------------
// FLAG -- UN-HOMED DEPENDENCY. Prepare's junction loop indexes lpHull->mpaJunctions[] and
// needs BrnTraffic::JunctionLogicBox::GetNumStates(). That type's canonical home is
// SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h, which has NOT been reconstructed,
// so BrnTrafficHull.h can only forward-declare `class JunctionLogicBox;`.
//
// The record is reproduced in full below, not padding-forked, because the stride and the
// member offset both come straight off ARTIST in Prepare's junction loop @0x82751438:
//     0x82751528  lwz  r8, 0x2C(r31)   ; lpHull->mpaJunctions (8th ptr after the 16-byte
//                                      ;   scalar header: 0x10 + 7*4)
//     0x82751534  addi r9, r9, 0x120   ; ELEMENT STRIDE = 288
//     0x82751538  lbz  r8, 0x34(r8)    ; muNumStates at +0x34
//     0x8275153C  addi r8, r8, 0xFF    ; - 1
//     0x82751540  stbx r8, r7, r11     ; -> mauJunctionCurrentStates[i]  (r7 = this+0x40)
// The DWARF member list (BrnJunctionLogicBox.h :128..:141) sums to exactly 0x120 with
// muNumStates at 0x34, so ARTIST and the DWARF agree.
//
// A member-less stand-in is NOT safe here: it completes the forward declaration at sizeof
// == 1, so mpaJunctions[] advances one byte per junction instead of 288 and every hull with
// more than one junction reads a plausible wrong value, silently, on the parked-car path.
//
// Absolute offsets are legitimate: the record holds NO POINTERS (integer scalars and arrays,
// eight 24-byte pointer-free TrafficLightControllers, one Vector3 lane), so console offsets
// are host offsets. The two holes below are natural alignment holes, and _AssertLayout
// proves it.
//
// RETIRE THIS BLOCK when BrnJunctionLogicBox.h lands: delete both types and include that
// header instead. It should carry the declarations below plus the remaining DWARF accessors
// (GetID/:83, GetTimeInState/:85, GetNumLights/:86, GetLight/:87, IsLightRed/:88,
// GetEventJunctionID/:90, GetOfflineStartDataIndex/:91, GetOnlineStartDataIndex/:92,
// GetPosition/:94, FixUp/:119, FixDown/:124), which no caller in this TU needs.
// ---------------------------------------------------------------------------

// BrnJunctionLogicBox.h:51 -- the signal group driving one approach of a junction.
// 24-byte pointer-free record; eight of them sit inline in JunctionLogicBox.
struct TrafficLightController
{
    u16 mauTrafficLightIds[2];   // +0x00  (:53)
    u8  mauStopLineIds[6];       // +0x04  (:54)
    u16 mauStopLineHulls[6];     // +0x0A  (:55)
    u8  muNumStopLines;          // +0x16  (:56)
    u8  muNumTrafficLights;      // +0x17  (:57)
                                 //  sizeof == 0x18

    // :61 -- streamed-data byte swap; its own (not-yet-reconstructed) TU. Declaration
    // only, matching how this directory already handles LaneRung::EndianSwap.
    void EndianSwap();
};

class JunctionLogicBox
{
public:
    // BrnJunctionLogicBox.h:84 -- the only accessor ARTIST's Prepare needs.
    u8 GetNumStates() const { return muNumStates; }

    // Layout pin. NEVER CALLED. Sources: the Prepare junction loop above (stride 0x120,
    // muNumStates +0x34) and the DWARF member list :128..:141.
    static void _AssertLayout();

private:
    u32 muID;                                             // +0x000  (:128)
    u16 mauStateTimings[16];                              // +0x004  (:129)
    u8  mauStoppedLightStates[16];                        // +0x024  (:130)
    u8  muNumStates;                                      // +0x034  (:131)
    u8  muNumLights;                                      // +0x035  (:132)
    u8  maPad0x36[2];                                     // +0x036  natural hole
    u32 muEventJunctionID;                                // +0x038  (:135)
    s32 miOfflineStartDataIndex;                          // +0x03C  (:136)
    s32 miOnlineStartDataIndex;                           // +0x040  (:137)
    TrafficLightController maTrafficLightControllers[8];  // +0x044  (:139)  8 * 0x18
    u8  maPad0x104[12];                                   // +0x104  natural hole: the
                                                          //         Vector3 lane is
                                                          //         16-byte aligned
    Vector3 mPosition;                                    // +0x110  (:141)
};                                                        //  sizeof == 0x120

void JunctionLogicBox::_AssertLayout()
{
    static_assert(sizeof(TrafficLightController) == 0x18, "sizeof(TrafficLightController)");
    static_assert(offsetof(TrafficLightController, mauStopLineIds) == 0x04, "mauStopLineIds");
    static_assert(offsetof(TrafficLightController, mauStopLineHulls) == 0x0A, "mauStopLineHulls");
    static_assert(offsetof(TrafficLightController, muNumStopLines) == 0x16, "muNumStopLines");

    static_assert(offsetof(JunctionLogicBox, muID) == 0x000, "muID");
    static_assert(offsetof(JunctionLogicBox, mauStateTimings) == 0x004, "mauStateTimings");
    static_assert(offsetof(JunctionLogicBox, mauStoppedLightStates) == 0x024,
                  "mauStoppedLightStates");
    // The one offset ARTIST states outright: `lbz r8, 0x34(r8)` @ 0x82751538.
    static_assert(offsetof(JunctionLogicBox, muNumStates) == 0x034, "muNumStates");
    static_assert(offsetof(JunctionLogicBox, muNumLights) == 0x035, "muNumLights");
    static_assert(offsetof(JunctionLogicBox, muEventJunctionID) == 0x038, "muEventJunctionID");
    static_assert(offsetof(JunctionLogicBox, miOfflineStartDataIndex) == 0x03C,
                  "miOfflineStartDataIndex");
    static_assert(offsetof(JunctionLogicBox, miOnlineStartDataIndex) == 0x040,
                  "miOnlineStartDataIndex");
    static_assert(offsetof(JunctionLogicBox, maTrafficLightControllers) == 0x044,
                  "maTrafficLightControllers");
    static_assert(offsetof(JunctionLogicBox, mPosition) == 0x110, "mPosition");
    // The one size ARTIST states outright: `addi r9, r9, 0x120` @ 0x82751534.
    static_assert(sizeof(JunctionLogicBox) == 0x120, "sizeof(JunctionLogicBox)");
}

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
