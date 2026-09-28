#pragma once

// =============================================================================
// BrnTrafficLightCollection.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::TrafficLightType and BrnTraffic::TrafficLightCollection --
// the read-only, fixed-up (relocated-pointer) view over a track's baked traffic-
// light corona data. The X360 retail XEX bakes this exact header path into the
// collection's bounds asserts (".../sharedclasses/traffic/Junctions/
// BrnTrafficLightCollection.h", lines 208/215/222/223/230/231/248/249/288/294/295).
//
// Member offsets are pinned by the X360 asm across this batch's six functions:
//   +0x00  u16  muNumTrafficLights          lhz 0(this)
//   +0x02  u16  muNumTrafficLightTypes      lhz 2(this)
//   +0x04  u16  muNumCoronas                lhz 4(this)
//   +0x08  Vector3Plus* mpaPosAndYRotations lwz 8(this)   ; 16-byte stride (lvx, slwi ,4)
//   +0x0C  u32*  mpaInstanceIDs             (DWARF; not touched by this batch)
//   +0x10  u8*   mpauInstanceTypes          lwz 0x10(this); lbzx  (GetInstanceType)
//   +0x14  TrafficLightType* mpaTrafficLightTypes  lwz 0x14(this); slwi ,1 (2-byte stride)
//   +0x18  u8*   mpaCoronaTypes             lwz 0x18(this); lbzx (corona colour state)
//   +0x1C  Vector3* mpaCoronaPositions      lwz 0x1C(this); lvx  (16-byte stride)
//   +0x20  u16   mauInstanceHashOffsets[129]  lhzx this[luHash+16]/[luHash+17]
//   +0x124 u32*  mpauInstanceHashTable      lwz 0x124(this); +4*idx
//   +0x128 u16*  mpauInstanceHashToIndexLookup lwz 0x128(this); lhzx 2*idx
//
// (mauInstanceHashOffsets is a u16[129] at u16 index 16 == byte 0x20; 129*2 = 0x102
// bytes end at 0x122; the two trailing pointers begin at 0x124, so a 2-byte tail pad
// sits after the array.)
//
// DWARF (BrnTrafficLightCollection.h) is authoritative for member NAMES/offsets and
// method SHAPES; the six batch methods are DEFINED in the .cpp, the rest declared for
// a coherent type (bodies live in other TUs). GetCoronaState is declared because
// CalcArbitraryAmberCoronaTransform references it; its body is another TU's.
// =============================================================================

#include "BrnCommonTypes.h"                             // Vector3, Vector3Plus, Matrix44Affine
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameSource/Graphics/BrnCoronaManager.h"       // BrnCoronaManager::BrnSubmissionInterface, BrnCoronaType, eCoronaTypeTrafficLight*
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h" // BrnTraffic::ETrafficLightState (DWARF BrnTrafficSharedConstants.h:86)
#include <cstddef>                                      // offsetof (the host-layout pins in _AssertLayout)

namespace BrnTraffic
{
    // The corona colour state (X360 stores it per corona as a u8 in mpaCoronaTypes).
    // Pinned by CalcArbitraryAmberCoronaTransform's asm: the per-corona type is asserted
    // `< 3` (`E_TRAFFICLIGHTSTATE_COUNT`) and an AMBER corona is the `== 1` state.
    // [L3 RACEINTRO 2026-09-27, structural merge of the BL-1 ODR fork] The type is the DWARF's
    // BrnTraffic::ETrafficLightState (BrnTrafficSharedConstants.h:86, included above). This header
    // carried a second copy of the same enum, so no TU could hold both homes; the copy is retired
    // and these asserts prove the home carries the retired copy's values, enumerator by enumerator.
    static_assert(E_TRAFFICLIGHTSTATE_RED   == 0, "ETrafficLightState: RED is 0, as the retired copy");
    static_assert(E_TRAFFICLIGHTSTATE_AMBER == 1, "ETrafficLightState: AMBER is 1, as the retired copy");
    static_assert(E_TRAFFICLIGHTSTATE_GREEN == 2, "ETrafficLightState: GREEN is 2, as the retired copy");
    static_assert(E_TRAFFICLIGHTSTATE_COUNT == 3, "ETrafficLightState: COUNT is 3, as the retired copy");

    // DWARF BrnTrafficLightCollection.h:60 -- a 2-byte record naming a type's corona run
    // (offset + count into the collection's flat corona arrays). Stride pinned at 2 by
    // GetTrafficLightType (slwi ,1); fields pinned by CalcArbitraryAmberCoronaTransform
    // (lbz 0 = muCoronaOffset, lbz 1 = muNumCoronas).
    struct TrafficLightType
    {
        u8 muCoronaOffset;  // +0
        u8 muNumCoronas;    // +1
    };

    // ExpandPosPlusYRotToTransform @ 0x823610B8 (183 insns) -- expands a packed Vector3Plus
    // (xyz = position, w = Y rotation angle) into a full affine. Referenced by
    // CalcInstanceTransform.
    //
    // ⚠️ DWARF homes it in SharedClasses/Traffic/BrnTrafficSharedMaths.h:81, and the
    //    console's own baked assert file literal agrees ("..\..\..\SharedClasses\Traffic/
    //    BrnTrafficSharedMaths.h", lines 83/84). That header has no mirror in this tree, so
    //    the BODY currently lives in BrnTrafficLightCollection.cpp beside its only in-tree
    //    caller. Move both this declaration and that body to a
    //    real BrnTrafficSharedMaths.h the day one lands.
    //
    // ⚠️ The parameter is `const Vector3Plus&` here and in the retail mangled name (AEB);
    //    the DWARF DIE spells it by value. The reference form is the committed one -- do not
    //    "correct" it, it would change the mangled name. The parameter NAME is the console's
    //    own (`lPosPlusYRot`, from the two assert literals).
    Matrix44Affine ExpandPosPlusYRotToTransform(const Vector3Plus& lPosPlusYRot);

    // DWARF BrnTrafficLightCollection.h:78.
    struct TrafficLightCollection
    {
    public:
        // --- attested in this batch ---

        // CalcInstanceTransform @ 0x82753910
        Matrix44Affine CalcInstanceTransform(u32 luInstance) const;

        // CalcArbitraryAmberCoronaTransform @ 0x82757478
        const Matrix44Affine CalcArbitraryAmberCoronaTransform(u32 luInstance) const;

        // RenderCoronasForInstance @ 0x827571B8 -- submit this instance's active-state
        // coronas to the world corona buffer (back-face + distance culled, distance-scaled).
        void RenderCoronasForInstance(
            u32 luInstance,
            u32 luActiveStates,
            BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
            Vector3 lCameraPosition,
            Vector3 lCameraDirection,
            VecFloat lfCullDistSq) const;

        // GetInstanceIndexForInstanceID @ 0x8274F590
        s32 GetInstanceIndexForInstanceID(u32 luInstanceID) const;

        // --- declared for a coherent type (DWARF); bodies live in other TUs ---
        // [L3 RACEINTRO 2026-09-27, the BL-1 merge] :82 -- a header inline (the retired
        // SharedClasses/Traffic/BrnTrafficLightCollection.h carried this body).
        u32 GetNumTrafficLights() const { return muNumTrafficLights; }
        // [L3 RACEINTRO 2026-09-27] :93 -- a header inline, bodied at this header's lines 266 / 267:
        // MainDirector::CalcTrafficLightSpace @0x8221A3A8 inlines it (0x8221A56C..0x8221A5C4) as the
        // "luInstance < muNumTrafficLights" tripwire (li r5, 0x10A == 266), the "mpaPosAndYRotations"
        // tripwire (li r5, 0x10B == 267), then `lvx128` of mpaPosAndYRotations[luInstance] (slwi ,4):
        // the whole 16-byte lane, the Y rotation riding in w.
        const Vector3 GetInstancePos(u32 luInstance) const
        {
            CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");
            CGS_ASSERT(mpaPosAndYRotations, "mpaPosAndYRotations");

            const Vector3Plus& lrPosAndYRotation = mpaPosAndYRotations[luInstance];
            const Vector3 lInstancePos = { lrPosAndYRotation.x, lrPosAndYRotation.y,
                                           lrPosAndYRotation.z, lrPosAndYRotation.w };
            return lInstancePos;
        }
        void FixUp(const void* lpBaseData);
        void FixDown(const void* lpBaseData);

        // [L3 RACEINTRO 2026-09-27, the BL-1 merge] The host layout tools/assets/bundles/lane_transcode.py
        // writes TrafficData::mTrafficLights to, pinned by the retired SharedClasses/Traffic/
        // BrnTrafficLightCollection.h and moved here with it. The six relocated pointers widen 4 -> 8 on
        // x64 (console +0x08..+0x1C / +0x124 / +0x128). NEVER CALLED; a member because the members are
        // private (the BrnJunctionLogicBox.h precedent).
        static void _AssertLayout()
        {
            static_assert(sizeof(TrafficLightType) == 2, "TrafficLightType stride");
            static_assert(offsetof(TrafficLightCollection, mpaPosAndYRotations) == 0x08,
                          "TrafficLightCollection::mpaPosAndYRotations");
            static_assert(offsetof(TrafficLightCollection, mpaCoronaPositions) == 0x30,
                          "TrafficLightCollection::mpaCoronaPositions");
            static_assert(offsetof(TrafficLightCollection, mauInstanceHashOffsets) == 0x38,
                          "TrafficLightCollection::mauInstanceHashOffsets");
            static_assert(offsetof(TrafficLightCollection, mpauInstanceHashTable) == 0x140,
                          "TrafficLightCollection::mpauInstanceHashTable");
            static_assert(offsetof(TrafficLightCollection, mpauInstanceHashToIndexLookup) == 0x148,
                          "TrafficLightCollection::mpauInstanceHashToIndexLookup");
            static_assert(sizeof(TrafficLightCollection) == 0x150, "TrafficLightCollection host sizeof");
        }

    private:
        // DWARF BrnTrafficLightCollection.h:167/168.
        static const u32 KU_INSTANCE_ID_HASH_MASK       = 127;  // 0x7F
        static const u32 KU_INSTANCE_ID_HASH_TABLE_SIZE = 129;  // 0x81

        // GetInstanceType @ 0x8274F438 -- mpauInstanceTypes[luInstance].
        u32 GetInstanceType(u32 luInstance) const;
        // GetTrafficLightType @ 0x8274F4A0 -- &mpaTrafficLightTypes[luType].
        const TrafficLightType* GetTrafficLightType(u32 luType) const;
        // GetCoronaState @ 0x8274F510 -- mpaCoronaTypes[luCorona] as ETrafficLightState.
        // NOT an inline: a real out-of-line symbol (32 insns), asserts baked at :222/:223.
        // Body in BrnTrafficLightCollection.cpp.
        ETrafficLightState GetCoronaState(u32 luCorona) const;
        // GetCoronaPosition @ 0x82753820 -- mpaCoronaPositions[luCorona] (validated).
        Vector3 GetCoronaPosition(u32 luCorona) const;

        // --- data members (offsets pinned by the X360 asm; see header banner) ---
        u16 muNumTrafficLights;                  // +0x00
        u16 muNumTrafficLightTypes;              // +0x02
        u16 muNumCoronas;                        // +0x04
        Vector3Plus* mpaPosAndYRotations;        // +0x08
        u32* mpaInstanceIDs;                     // +0x0C
        u8*  mpauInstanceTypes;                  // +0x10
        TrafficLightType* mpaTrafficLightTypes;  // +0x14
        u8*  mpaCoronaTypes;                     // +0x18
        Vector3* mpaCoronaPositions;             // +0x1C
        u16  mauInstanceHashOffsets[129];        // +0x20 (129*u16; +2-byte tail pad)
        u32* mpauInstanceHashTable;              // +0x124
        u16* mpauInstanceHashToIndexLookup;      // +0x128
    };
}
