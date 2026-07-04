// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/
//   BrnRCEntityActiveRaceCarOutputInterface.cpp
//
// Out-of-line bodies for the per-active-race-car OUTPUT interface
// (BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface).
// Member access is BY NAME at the asm-proven byte offsets; offsets are locked
// with static_assert(offsetof(...)) below. X360 asm is authoritative.
//
// All per-index getters/setters bounds-check the EActiveRaceCarIndex in [0,8)
// (the array dimension is E_ACTIVE_RACE_CAR_INDEX_COUNT == 8); the player-scoped
// getters assert the player index has been set (mePlayerActiveRaceCarIndex != -1,
// i.e. != E_ACTIVE_RACE_CAR_INDEX_INVALID). The enum indexes the arrays directly.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/BurnoutConstants.h"            // EActiveRaceCarIndex / EGlobalRaceCarIndex enumerators
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include <cstring>                                  // std::memcpy (RaceCarState payload copy)

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{

// ---- asm-proven member offsets (X360 ARTIST build) -------------------------
// offsetof on the private members must be evaluated where the members are accessible,
// so the lock lives inside a member function (compiled, never called). These are the
// X360 byte offsets the callers/asm prove; the caller-critical one is
// mePlayerActiveRaceCarIndex @+0x2858 (== 10328), which the ScoringSystem /
// StuntModeScoring bodies index. ResourceHandle/WorldMap2D/trailing-bool offsets are
// NOT locked here: the X360 packed them at +0x2864/+0x28E1 with no host padding, but
// MSVC's default alignment for CgsResource::ResourceHandle differs; locking them would
// force a repack of the committed (frozen) layout, so they stay documented-only.
// The offsetof locks live inside the copy constructor below (member scope grants the
// private access offsetof needs under MSVC).

// ============================================================================
// X360 0x826BB258 -- copy constructor. A full member-wise clone: the leading POD
// spans copied verbatim, each maRaceCarStates element copy-constructed, then the
// trailing parallel-array / scalar members copied. Modeled here as the equivalent
// named member-wise copy (the compiler lays the same stores down, in-order).
// ============================================================================
RCEntityActiveRaceCarOutputInterface::RCEntityActiveRaceCarOutputInterface(
        const RCEntityActiveRaceCarOutputInterface& lrOther)
    : maCarsInTheRace(lrOther.maCarsInTheRace)
{
    typedef RCEntityActiveRaceCarOutputInterface T;
    static_assert(offsetof(T, maBoostOutputInfo)                == 528,   "maBoostOutputInfo @+0x210");
    static_assert(offsetof(T, maRaceCarStates)                  == 816,   "maRaceCarStates @+0x330");
    static_assert(offsetof(T, maRivalIds)                       == 9776,  "maRivalIds @+0x2630");
    static_assert(offsetof(T, maCarModelIds)                    == 9840,  "maCarModelIds @+0x2670");
    static_assert(offsetof(T, mauActiveRaceCarColourIndex)      == 9904,  "mauActiveRaceCarColourIndex @+0x26B0");
    static_assert(offsetof(T, maiActiveRaceCarPaintFinishIndex) == 9936,  "maiActiveRaceCarPaintFinishIndex @+0x26D0");
    static_assert(offsetof(T, mau16ActiveRaceCarAISections)     == 9968,  "mau16ActiveRaceCarAISections @+0x26F0");
    static_assert(offsetof(T, maRaceCarMaterialColours)         == 9984,  "maRaceCarMaterialColours @+0x2700");
    static_assert(offsetof(T, maxRaceCarFlags)                  == 10112, "maxRaceCarFlags @+0x2780");
    static_assert(offsetof(T, maCurrentInAirRotations)          == 10128, "maCurrentInAirRotations @+0x2790");
    static_assert(offsetof(T, mbHasCrashedIntoWater)            == 10256, "mbHasCrashedIntoWater @+0x2810");
    static_assert(offsetof(T, maGlobalRaceCarIndices)           == 10264, "maGlobalRaceCarIndices @+0x2818");
    static_assert(offsetof(T, maeActiveRaceCarIndex)            == 10296, "maeActiveRaceCarIndex @+0x2838");
    static_assert(offsetof(T, mePlayerActiveRaceCarIndex)       == 10328, "mePlayerActiveRaceCarIndex @+0x2858 (ScoringSystem/StuntModeScoring caller offset)");
    static_assert(offsetof(T, mePlayerEngineState)              == 10332, "mePlayerEngineState @+0x285C");
    static_assert(offsetof(T, mbIsPlayerCarActive)              == 10336, "mbIsPlayerCarActive @+0x2860");
    static_assert(offsetof(T, mbAllActiveCarsReady)             == 10337, "mbAllActiveCarsReady @+0x2861");
    static_assert(sizeof(RaceCarState) == 1120, "RaceCarState stride 0x460");

    for (s32 luIndex = 0; luIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luIndex)
    {
        maBoostOutputInfo[luIndex]                = lrOther.maBoostOutputInfo[luIndex];
        maRaceCarStates[luIndex]                  = lrOther.maRaceCarStates[luIndex];
        maRivalIds[luIndex]                       = lrOther.maRivalIds[luIndex];
        maCarModelIds[luIndex]                    = lrOther.maCarModelIds[luIndex];
        mauActiveRaceCarColourIndex[luIndex]      = lrOther.mauActiveRaceCarColourIndex[luIndex];
        maiActiveRaceCarPaintFinishIndex[luIndex] = lrOther.maiActiveRaceCarPaintFinishIndex[luIndex];
        mau16ActiveRaceCarAISections[luIndex]     = lrOther.mau16ActiveRaceCarAISections[luIndex];
        maRaceCarMaterialColours[luIndex]         = lrOther.maRaceCarMaterialColours[luIndex];
        maxRaceCarFlags[luIndex]                  = lrOther.maxRaceCarFlags[luIndex];
        maCurrentInAirRotations[luIndex]          = lrOther.maCurrentInAirRotations[luIndex];
        mbHasCrashedIntoWater[luIndex]            = lrOther.mbHasCrashedIntoWater[luIndex];
        maGlobalRaceCarIndices[luIndex]           = lrOther.maGlobalRaceCarIndices[luIndex];
        maeActiveRaceCarIndex[luIndex]            = lrOther.maeActiveRaceCarIndex[luIndex];
        maDeformationModelResourceHandles[luIndex]= lrOther.maDeformationModelResourceHandles[luIndex];
    }
    mePlayerActiveRaceCarIndex = lrOther.mePlayerActiveRaceCarIndex;
    mePlayerEngineState        = lrOther.mePlayerEngineState;
    mbIsPlayerCarActive        = lrOther.mbIsPlayerCarActive;
    mbAllActiveCarsReady       = lrOther.mbAllActiveCarsReady;
    mWorldMap2D                = lrOther.mWorldMap2D;
    mbPlayerWrecked            = lrOther.mbPlayerWrecked;
    mbCanDriveAwayFromCrash    = lrOther.mbCanDriveAwayFromCrash;
}

// ============================================================================
// X360 0x8227D550 -- Clear. Resets the per-car arrays, the per-scoring-index
// active-car map and the player-scoped scalars to their cleared defaults:
//   mePlayerActiveRaceCarIndex = -1   (E_ACTIVE_RACE_CAR_INDEX_INVALID)
//   mePlayerEngineState        = E_ACTIVE_RACE_CAR_ENGINE_STATE_COUNT (4)
//   mbIsPlayerCarActive        = false
//   per car: AISection = 0x7FFF, colour idx = 0, paint idx = 0, flags = 0,
//            global idx = 0, crashed-into-water = 0
//   per scoring index: maeActiveRaceCarIndex[i] = E_ACTIVE_RACE_CAR_INDEX_COUNT (8)
//   mbPlayerWrecked = false; mbCanDriveAwayFromCrash = false
// ============================================================================
void RCEntityActiveRaceCarOutputInterface::Clear()
{
    mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;   // -1 @+0x2858
    mbIsPlayerCarActive        = false;                            // @+0x2860
    mePlayerEngineState        = E_ACTIVE_RACE_CAR_ENGINE_STATE_COUNT; // 4 @+0x285C

    for (s32 luIndex = 0; luIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luIndex)
    {
        mau16ActiveRaceCarAISections[luIndex] = 0x7FFF;            // @+0x26F0
        mauActiveRaceCarColourIndex[luIndex]  = 0;                 // @+0x26B0
        maiActiveRaceCarPaintFinishIndex[luIndex] = 0;             // @+0x26D0
        maxRaceCarFlags[luIndex]              = 0;                 // @+0x2780 (sets rival/network bits clear)
        maGlobalRaceCarIndices[luIndex]       = E_GLOBAL_RACE_CAR_INDEX_0; // 0 @+0x2818
        mbHasCrashedIntoWater[luIndex]        = false;             // @+0x2810
        CGS_ASSERT(luIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    for (s32 luScoringIndex = 0; luScoringIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luScoringIndex)
    {
        maeActiveRaceCarIndex[luScoringIndex] = E_ACTIVE_RACE_CAR_INDEX_COUNT; // 8 @+0x2838
        CGS_ASSERT(luScoringIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "leEnumIndex <= E_PLAYER_SCORING_INDEX_COUNT");
    }

    mbPlayerWrecked         = false;   // @+0x28E0 (the only trailing byte Clear stores: stb 0,0x28E0)
    // X360 Clear also resets the cars-in-race Array live-count (stw 0, +0x200 == ClearCarsInRace inlined).
    maCarsInTheRace.Clear();
    // NOTE: mbCanDriveAwayFromCrash (@+0x28E1) is NOT touched by Clear -- only SetRaceCarState / the
    // copy ctor write it (asm Clear has no store at 0x28E1).
}

// ============================================================================
// X360 0x8227D740 -- GetRaceCarColour. Bounds-checks the index, gates on
// IsRaceCarActive, returns the per-car material colour by reference
// (maRaceCarMaterialColours[idx], stride 16). (Truncated export name: "Get".)
// ============================================================================
const RwRGBAReal& RCEntityActiveRaceCarOutputInterface::GetRaceCarColour(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(IsRaceCarActive(leActiveRaceCarIndex),                "IsRaceCarActive( leActiveRaceCarIndex )");
    return maRaceCarMaterialColours[leActiveRaceCarIndex];
}

// X360 0x82542348 -- GetActiveRaceCarColourIndex (mauActiveRaceCarColourIndex[idx], stride 4).
u32 RCEntityActiveRaceCarOutputInterface::GetActiveRaceCarColourIndex(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return mauActiveRaceCarColourIndex[leActiveRaceCarIndex];
}

// X360 0x825423C0 -- GetActiveRaceCarPaintFinishIndex (maiActiveRaceCarPaintFinishIndex[idx], stride 4).
s32 RCEntityActiveRaceCarOutputInterface::GetActiveRaceCarPaintFinishIndex(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return maiActiveRaceCarPaintFinishIndex[leActiveRaceCarIndex];
}

// X360 0x82277A18 -- GetCarModelId (maCarModelIds[idx], CgsID stride 8).
CgsID RCEntityActiveRaceCarOutputInterface::GetCarModelId(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return maCarModelIds[leActiveRaceCarIndex];
}

// X360 0x823100C8 -- GetGlobalRaceCarIndex (maGlobalRaceCarIndices[idx], stride 4).
// (Truncated export name: "GetGlob".)
EGlobalRaceCarIndex RCEntityActiveRaceCarOutputInterface::GetGlobalRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return maGlobalRaceCarIndices[leActiveRaceCarIndex];
}

// X360 0x82277BF8 -- GetPlayerActiveRaceCarIndex (mePlayerActiveRaceCarIndex @+0x2858).
// (Truncated export name: "GetPlay".)
EActiveRaceCarIndex RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID, "Player car index hasn't been set");
    return mePlayerActiveRaceCarIndex;
}

// X360 0x8230FFD8 -- GetRaceCarAISection (mau16ActiveRaceCarAISections[idx], u16 stride 2).
u16 RCEntityActiveRaceCarOutputInterface::GetRaceCarAISection(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return mau16ActiveRaceCarAISections[leActiveRaceCarIndex];
}

// X360 0x82310050 -- GetRivalId (maRivalIds[idx], CgsID stride 8).
CgsID RCEntityActiveRaceCarOutputInterface::GetRivalId(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return maRivalIds[leActiveRaceCarIndex];
}

// X360 0x823A7BC8 -- HasCrashedIntoWater (mbHasCrashedIntoWater[idx], byte stride 1).
bool RCEntityActiveRaceCarOutputInterface::HasCrashedIntoWater(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return mbHasCrashedIntoWater[leActiveRaceCarIndex];
}

// X360 0x82310140 -- IsRaceCarNetwork: bit 3 of the per-car flags word (maxRaceCarFlags[idx]).
bool RCEntityActiveRaceCarOutputInterface::IsRaceCarNetwork(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return ((maxRaceCarFlags[leActiveRaceCarIndex] >> 3) & 1) != 0;
}

// X360 0x82705690 -- IsRaceCarRival: bit 2 of the per-car flags word (maxRaceCarFlags[idx]).
bool RCEntityActiveRaceCarOutputInterface::IsRaceCarRival(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return ((maxRaceCarFlags[leActiveRaceCarIndex] >> 2) & 1) != 0;
}

// X360 0x822A10D8 -- SetBoostOutputInfoN: store the 9-word BoostOutputInfo payload
// into maBoostOutputInfo[idx] (the asm copies 9 dwords == sizeof(BoostOutputInfo)).
void RCEntityActiveRaceCarOutputInterface::SetBoostOutputInfoN(EActiveRaceCarIndex leActiveRaceCarIndex,
                                                               const BoostOutputInfo& lrBoostOutputInfo)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    maBoostOutputInfo[leActiveRaceCarIndex] = lrBoostOutputInfo;
}

// X360 0x822A1180 -- SetPlayerActiveRaceCarData: latch the player's active-race-car index
// and engine state, and mark the player car active.
void RCEntityActiveRaceCarOutputInterface::SetPlayerActiveRaceCarData(EActiveRaceCarIndex leActiveRaceCarIndex,
                                                                     EActiveRaceCarEngineState leEngineState)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    mePlayerActiveRaceCarIndex = leActiveRaceCarIndex;   // @+0x2858
    mePlayerEngineState        = leEngineState;          // @+0x285C
    mbIsPlayerCarActive        = true;                   // @+0x2860 (stb 1)
}

// X360 0x822A1200 -- SetDeformationModelResourcePtr: copy the source ResourcePtr's embedded
// ResourceHandle (its two id words at +20/+24) into maDeformationModelResourceHandles[idx]
// (8-byte ResourceHandle, stride 8). Done by-name via GetResourceHandle().
void RCEntityActiveRaceCarOutputInterface::SetDeformationModelResourcePtr(
        EActiveRaceCarIndex leActiveRaceCarIndex,
        const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrResourcePtr)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    maDeformationModelResourceHandles[leActiveRaceCarIndex] = lrResourcePtr.GetResourceHandle();
}

// X360 0x822CC080 -- SetRaceCarState: publish a full per-car snapshot. Stores (by name, at
// the asm-proven slots): global index, rival id, car model id, the 1120-byte RaceCarState
// payload, the flags word, the AI-section, colour & paint indices, the material colour
// (Vector4 arg -> RwRGBAReal slot), the in-air rotations (Vector3 arg), the crashed-into-water
// flag and the can-drive-away flag. Float/SIMD args arrive in fp/vector registers (Vector4
// lrMaterialColour, Vector3 lrInAirRotations); the bool tail args are the water/drive flags.
void RCEntityActiveRaceCarOutputInterface::SetRaceCarState(
        EActiveRaceCarIndex leActiveRaceCarIndex,
        EGlobalRaceCarIndex leGlobalRaceCarIndex,
        CgsID               lRivalId,
        CgsID               lCarModelId,
        const RaceCarState* lpRaceCarState,
        u32                 luColourIndex,
        u16                 lu16AISection,
        u32                 luFlags,
        s32                 liPaintFinishIndex,
        Vector4             lMaterialColour,
        Vector3             lInAirRotations,
        bool                lbHasCrashedIntoWater,
        bool                lbCanDriveAwayFromCrash)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    maGlobalRaceCarIndices[leActiveRaceCarIndex] = leGlobalRaceCarIndex;          // 4*(idx+2566)+this
    maRivalIds[leActiveRaceCarIndex]             = lRivalId;                      // 8*(idx+1222)+this
    maCarModelIds[leActiveRaceCarIndex]          = lCarModelId;                   // 8*(idx+1230)+this
    std::memcpy(&maRaceCarStates[leActiveRaceCarIndex], lpRaceCarState, sizeof(RaceCarState)); // 1120*idx+this+816
    maxRaceCarFlags[leActiveRaceCarIndex]              = static_cast<u16>(luFlags);        // 2*(idx+5056)+this
    mau16ActiveRaceCarAISections[leActiveRaceCarIndex] = lu16AISection;                    // 2*(idx+4984)+this
    mauActiveRaceCarColourIndex[leActiveRaceCarIndex]  = luColourIndex;                    // 4*(idx+2476)+this
    maiActiveRaceCarPaintFinishIndex[leActiveRaceCarIndex] = liPaintFinishIndex;           // 4*(idx+2484)+this
    maRaceCarMaterialColours[leActiveRaceCarIndex] =
        reinterpret_cast<const RwRGBAReal&>(lMaterialColour);                              // 16*(idx+624)+this
    maCurrentInAirRotations[leActiveRaceCarIndex]  = lInAirRotations;                      // 16*(idx+633)+this
    mbHasCrashedIntoWater[leActiveRaceCarIndex]    = lbHasCrashedIntoWater;                // idx+this+10256
    mbCanDriveAwayFromCrash                        = lbCanDriveAwayFromCrash;              // this+10465
}

// X360 0x82287998 -- GetDeformationModelResourcePtr: const per-index deformation-model
// resource-ptr accessor. Bounds-checks the EActiveRaceCarIndex in [0,8), then returns a
// ResourcePtr bound from maDeformationModelResourceHandles[idx] (X360 default-constructs a
// BaseResourcePtr in the return slot, then CreateFromHandle(&handle)). The equivalent public
// path -- matching the X360 default-construct-then-bind-from-handle sequence -- is the
// ResourcePtr(const ResourceHandle&) constructor (CgsResourcePtr.h; CreateFromHandle is protected,
// callable only from the ResourcePtr ctor, not this non-derived class). handle ptr ==
// 8*idx + this + 10340 == &maDeformationModelResourceHandles[idx]. Pairs with the committed
// SetDeformationModelResourcePtr (X360 0x822A1200).
CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>
RCEntityActiveRaceCarOutputInterface::GetDeformationModelResourcePtr(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>(
        maDeformationModelResourceHandles[leActiveRaceCarIndex]);
}

// X360 0x8259BB58 -- GetPlayerSpeedMPH: the player car's current speed (mfSpeedMPH @968 in
// the player's RaceCarState). Asserts the player index has been set. (Additive accessor;
// the export demangled name was fully truncated -- mapped from the asm: it reads
// maRaceCarStates[mePlayerActiveRaceCarIndex] + 968 and returns the single 4-byte field.)
f32 RCEntityActiveRaceCarOutputInterface::GetPlayerSpeedMPH() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID, "Player car index hasn't been set");
    return maRaceCarStates[mePlayerActiveRaceCarIndex].mfSpeedMPH;
}

// ============================================================================
// X360 0x822B3F88 -- AddCarToRace. The build read the source RaceCar's position,
// previous position and (for the player type) velocity, plus its active-race-car
// index and type, packed them into a CarsInTheRaceData and appended it to
// maCarsInTheRace.
//
// FLAG: BrnWorld::RaceCar is only forward-declared at this interface's home (an
// incomplete type with no in-tree accessors: GetPosition/GetPreviousPosition/
// GetVelocity and the meActiveRaceCarIndex/muType fields the asm reads are NOT
// reconstructable until BrnRaceCar.h lands its own TU). Bodied here as the
// store-shape that compiles -- it appends a default-initialised CarsInTheRaceData
// (carrying the global-index arg) -- pending the RaceCar home, at which point the
// position/velocity/index/type reads must be wired in. The Append target and the
// E_ACTIVE_RACE_CAR_INDEX bounds assert match the asm 1:1.
// ============================================================================
void RCEntityActiveRaceCarOutputInterface::AddCarToRace(BrnWorld::RaceCar* /*lpRaceCar*/,
                                                       EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CarsInTheRaceData lCachedCar;
    // FLAG (RaceCar incomplete): lCachedCar.mPosition/mPreviousPosition/mDirection and
    // meActiveRaceCarIndex come from lpRaceCar once BrnRaceCar.h is homed; asm asserts
    // lCachedCar.meActiveRaceCarIndex in [0,8) and lpRaceCar->muType < E_RACE_CAR_TYPE_COUNT.
    lCachedCar.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    lCachedCar.meGlobalRaceCarIndex = leGlobalRaceCarIndex;
    lCachedCar.mbIsPlayer           = false;
    CGS_ASSERT((lCachedCar.meActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) &&
               (lCachedCar.meActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "( lCachedCar.meActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && "
               "(lCachedCar.meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");
    maCarsInTheRace.Append(lCachedCar);
}

}
}
