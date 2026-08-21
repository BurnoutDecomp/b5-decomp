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
//
// AUDIT 2026-08-21 (cluster C3) -- WHAT CHANGED AND WHY.
//   * The class definition moved OUT of this .cpp into its own owning header,
//     BrnTrafficHullRuntime.h, so the keystone module header can finally embed
//     `HullRuntime maHullRuntimeData[72]` (its `[MEMBER HOLE 3]`).
//   * The 60-line hand-rolled `struct Hull` that used to sit at the top of this file is
//     GONE. BrnTraffic::Hull has a real home -- SharedClasses/Traffic/BrnTrafficHull.h --
//     and this file now includes it. The fork was a second definition of a real class in
//     the same namespace, differing in member set, and it survived only because no TU ever
//     saw both. It also encoded the traffic hull's pointer block as bare `void*`, which is
//     exactly the shape that hides the pointee types the wave needs.
//   * SetFirstParamInSection and SetStoplineRed -- both in the X360 ledger, both previously
//     missing a body -- are landed.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"  // KU_INVALID_PARAM
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"

#include <cstddef>   // offsetof, for the never-called _AssertLayout pin below

namespace BrnTraffic
{
// ---------------------------------------------------------------------------
// FLAG -- UN-HOMED DEPENDENCY, NOT A FORK OF CHOICE. Prepare's junction loop indexes
// lpHull->mpaJunctions[] and needs BrnTraffic::JunctionLogicBox::GetNumStates(). The
// type's canonical home is SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h and
// that file has NOT been reconstructed -- SharedClasses/Traffic/Junctions currently
// holds only BrnTrafficLightCollection and BrnTrafficStopLine, so BrnTrafficHull.h can
// only forward-declare `class JunctionLogicBox;` for its mpaJunctions pointer. Cluster
// C3 does not own SharedClasses/Traffic, so the header is NOT created here.
//
// FIX ROUND 2026-08-21 -- WHY THIS BLOCK GREW A BODY.
//   The first landing declared this class with ZERO members and an undefined
//   GetNumStates(). That completed the forward-declared type with sizeof == 1, so
//   `lpHull->mpaJunctions[luJunction]` advanced ONE byte per junction instead of the
//   console's 288, and every hull with more than one junction seeded
//   mauJunctionCurrentStates[i>0] from a byte 287 short of the right record -- silently,
//   with a plausible small value. HullRuntime::Prepare runs on hull activation, i.e. on
//   the wave-1 parked-car path. The bodyless GetNumStates() was also an unannounced
//   LNK2019 at mount time.
//
//   The stride and the member offset are BOTH read directly off ARTIST, in Prepare's own
//   junction loop @ 0x82751438 (verified from the raw listing in this fix round):
//       0x82751528  lwz  r8, 0x2C(r31)   ; lpHull->mpaJunctions (8th ptr after the
//                                        ;   16-byte scalar header: 0x10 + 7*4)
//       0x82751530  add  r8, r8, r9      ; + running byte offset
//       0x82751534  addi r9, r9, 0x120   ; ELEMENT STRIDE = 288
//       0x82751538  lbz  r8, 0x34(r8)    ; muNumStates at +0x34
//       0x8275153C  addi r8, r8, 0xFF    ; - 1
//       0x82751540  stbx r8, r7, r11     ; -> mauJunctionCurrentStates[i]  (r7 = this+0x40)
//   The DWARF member list (references/DecFIGS/dwarfdump/SharedClasses/Traffic/Junctions/
//   BrnJunctionLogicBox.h, :128..:141) sums to exactly 0x120 with muNumStates at 0x34,
//   so ARTIST and the DWARF agree and the record is reproduced here in full rather than
//   padding-forked.
//
//   ABSOLUTE OFFSETS ARE LEGITIMATE HERE: the record holds NO POINTERS (four integer
//   scalars, three integer arrays, eight 24-byte pointer-free TrafficLightControllers and
//   one 16-byte Vector3 lane), so widening a host pointer cannot move anything and the
//   console offsets ARE the host offsets. Nothing is hand-padded: the two holes below are
//   the natural alignment holes the member list implies, and _AssertLayout proves it.
//
// RETIRE THIS BLOCK the moment BrnJunctionLogicBox.h lands: delete both types, and
// include that header instead (it should carry exactly the declarations below plus the
// remaining DWARF accessors GetID/:83, GetTimeInState/:85, GetNumLights/:86, GetLight/:87,
// IsLightRed/:88, GetEventJunctionID/:90, GetOfflineStartDataIndex/:91,
// GetOnlineStartDataIndex/:92, GetPosition/:94, FixUp/:119, FixDown/:124, which no
// reconstructed caller in this TU needs). Nothing else in this file depends on it.
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

    // Layout pin. NEVER CALLED -- a static member function so offsetof() reaches the
    // private members. Sources: the Prepare junction loop above (stride 0x120,
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

// @ 0x82706408 -- BrnTrafficHullRuntime.h:122/:123/:130 (the baked assert lines are
// 0x7A / 0x7B / 0x82). The third assert is the linked-list integrity check: the caller
// hands in the value it believes the section head currently holds, and the console refuses
// to relink if the table disagrees. The X360 indexes the table as (luSection + 0x28)*2
// bytes from `this`, i.e. 0x50 + 2*luSection == &mauFirstParamInSection[luSection].
void HullRuntime::SetFirstParamInSection(u32 luSection, u16 luParam, u16 luOldParam)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luSection < muNumSectionsInHull, "luSection < muNumSectionsInHull");
    CGS_ASSERT(mauFirstParamInSection[luSection] == luOldParam,
               "mauFirstParamInSection[luSection] == luOldParam");

    mauFirstParamInSection[luSection] = luParam;
}

// @ 0x82706630 -- BrnTrafficHullRuntime.h:141/:142 (baked line 0x8D and its successor).
// Tail is `add r11, luStopline, this ; stb lbRed, 0x250(r11)` == mabStoplineRedState
// [luStopline] = lbRed.
void HullRuntime::SetStoplineRed(u32 luStopline, bool lbRed)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luStopline < muNumStoplinesInHull, "luStopline < muNumStoplinesInHull");

    mabStoplineRedState[luStopline] = lbRed;
}

// @ 0x82706768 -- the section's first-param head index. Asserts mbPrepared, bounds-asserts
// the section against muNumSectionsInHull, then returns mauFirstParamInSection[luSection]
// (the asm reads the u16 at this + 2*(luSection+40); the table base is byte +0x50 == 2*40).
// The X360 bounds assert builds a rich streamed message ("Out of range section in hull
// runtime for hull <i>, section was <n>, max is <m>") through the out-of-line CgsDev debug
// stream operators, which are not owned here; the stringized-condition CGS_ASSERT carries
// the same guard semantics and matches the surrounding TU style.
u16 HullRuntime::GetFirstParamInSection(u32 luSection) const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luSection < muNumSectionsInHull, "luSection < muNumSectionsInHull");
    return mauFirstParamInSection[luSection];
}

// @ 0x827068A0 -- whether the stopline's light is red. Same shape, same rich-vs-stringized
// assert note; the asm reads the byte at this + 0x250 + luStopline.
bool HullRuntime::IsStoplineRed(u32 luStopline) const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    CGS_ASSERT(luStopline < muNumStoplinesInHull, "luStopline < muNumStoplinesInHull");
    return mabStoplineRedState[luStopline];
}

// ---------------------------------------------------------------------------
// Layout pin. NEVER CALLED -- a static member function so offsetof() reaches the private
// members. Absolute offsets are legitimate for this record because it holds NO POINTERS:
// five plain arrays and four integer scalars, so widening a pointer cannot move anything
// and the console offsets are the host offsets. Every one is attested -- see the table in
// BrnTrafficHullRuntime.h's banner for which X360 function pins which.
// ---------------------------------------------------------------------------
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
