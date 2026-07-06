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

namespace BrnTraffic
{
    // The corona colour state (X360 stores it per corona as a u8 in mpaCoronaTypes).
    // Pinned by CalcArbitraryAmberCoronaTransform's asm: the per-corona type is asserted
    // `< 3` (`E_TRAFFICLIGHTSTATE_COUNT`) and an AMBER corona is the `== 1` state. Homed here
    // in the collection's owning header (the corona colours are a property of this baked view;
    // the runtime light-phase state machine's enum in BrnTrafficLightManager.h is a distinct
    // type). If a canonical DWARF home lands, GROW that home and drop this local enum.
    enum ETrafficLightState
    {
        E_TRAFFICLIGHTSTATE_RED   = 0,
        E_TRAFFICLIGHTSTATE_AMBER = 1,
        E_TRAFFICLIGHTSTATE_GREEN = 2,
        E_TRAFFICLIGHTSTATE_COUNT = 3,  // first invalid value (X360 asserts corona type < 3)
    };

    // DWARF BrnTrafficLightCollection.h:60 -- a 2-byte record naming a type's corona run
    // (offset + count into the collection's flat corona arrays). Stride pinned at 2 by
    // GetTrafficLightType (slwi ,1); fields pinned by CalcArbitraryAmberCoronaTransform
    // (lbz 0 = muCoronaOffset, lbz 1 = muNumCoronas).
    struct TrafficLightType
    {
        u8 muCoronaOffset;  // +0
        u8 muNumCoronas;    // +1
    };

    // The transform expander lives in the BrnTraffic namespace (RECOVERED sibling in
    // class:BrnTraffic; body in that TU). Referenced by CalcInstanceTransform: it expands
    // a packed Vector3Plus (xyz = position, w = Y rotation angle) into a full affine.
    Matrix44Affine ExpandPosPlusYRotToTransform(const Vector3Plus& lPosAndYRot);

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
        u32 GetNumTrafficLights() const;
        const Vector3 GetInstancePos(u32 luInstance) const;
        void FixUp(const void* lpBaseData);
        void FixDown(const void* lpBaseData);

    private:
        // DWARF BrnTrafficLightCollection.h:167/168.
        static const u32 KU_INSTANCE_ID_HASH_MASK       = 127;  // 0x7F
        static const u32 KU_INSTANCE_ID_HASH_TABLE_SIZE = 129;  // 0x81

        // GetInstanceType @ 0x8274F438 -- mpauInstanceTypes[luInstance].
        u32 GetInstanceType(u32 luInstance) const;
        // GetTrafficLightType @ 0x8274F4A0 -- &mpaTrafficLightTypes[luType].
        const TrafficLightType* GetTrafficLightType(u32 luType) const;
        // GetCoronaState -- mpaCoronaTypes[luCorona] as ETrafficLightState (body: another TU).
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
