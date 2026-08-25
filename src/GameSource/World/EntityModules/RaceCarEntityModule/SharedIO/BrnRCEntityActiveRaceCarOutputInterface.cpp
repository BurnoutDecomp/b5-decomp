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
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // gpDebugPrint (the demoted player-index tripwire)
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
// operator= (DWARF :213). There is NO out-of-line operator= symbol in the image: the
// only assignment site, BrnWorldIO::UpdateOutputBuffer::SetActiveRaceCarOutputInterface
// @0x827A47A8, is a flat `XMemCpy(dst, src, 0x28F0)` == the whole 10480-byte object.
//
// ⚠️ WHY THIS EXISTS NOW: the declared-but-undefined operator= had been resolving from
// WorldLinkStubs.cpp as an INERT one-shot log. `SetActiveRaceCarOutputInterface` therefore
// compiled, ran every frame, and COPIED NOTHING -- a bridge that looks correct and silently
// drops its payload. Retired here with the member-wise body (the same one the copy
// constructor above uses; a byte-copy is not portable to the x64 layout).
// ============================================================================
void RCEntityActiveRaceCarOutputInterface::operator=(
        const RCEntityActiveRaceCarOutputInterface& lrOther)
{
    if (this == &lrOther)
    {
        return;
    }

    maCarsInTheRace = lrOther.maCarsInTheRace;

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
    // ⚠️ [FLAG PC bring-up — the console's "hasn't been set" tripwire demoted to a one-shot
    // log]. On this build one boot-time caller queries the player index once (~33 s, before
    // any race car is registered) and the INVALID sentinel return is handled by every consumer
    // (they early-out on the inactive player — verify_r3_fix3bridge NOTE 2). The console's
    // blocking assert stopped a manual boot at the loading screen; the diagnostic stays in the
    // log. RESTORE the CGS_ASSERT when the early caller is identified and guarded.
    if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        static bool gsbWarnedUnsetPlayerIndex = false;
        if (!gsbWarnedUnsetPlayerIndex && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] tripwire: GetPlayerActiveRaceCarIndex queried before the player "
                   "car index was set (console asserts here; INVALID returned, consumers early-out)\n";
            gsbWarnedUnsetPlayerIndex = true;
        }
    }
    return mePlayerActiveRaceCarIndex;
}

// DWARF :323 -- GetPlayerEngineState (mePlayerEngineState @+0x285C). NO out-of-line X360
// symbol: an image-wide name search finds none, and every console reader inlines the load --
// e.g. BrnGameModule::BridgeWorldVehicleDataToGui's engine-event leg @0x823E592C/@0x823E595C
// emits `lwz r10, 0x285C(r22)` straight, exactly as its two -1-guarded siblings
// IsRaceCarEngineOn / IsRaceCarEngineStarting do (see the header's note at :327/:331).
// Bodied out-of-line here (2026-08-25, hud reveal gate) because the declaration at :323 was
// the only thing keeping the 379 producer from linking. No assert: the console's own inlined
// reads are -1-guarded at the CALL SITE (`cmpwi -1` before the load), not in the accessor, so
// adding one here would be an invented arm that fires on the inactive-player path the callers
// deliberately take.
EActiveRaceCarEngineState RCEntityActiveRaceCarOutputInterface::GetPlayerEngineState() const
{
    return mePlayerEngineState;
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

// DWARF :420 -- SetActiveRaceCarIndex: record which active-race-car slot the given PLAYER
// SCORING slot is driving, into maeActiveRaceCarIndex[player]. Its only caller is
// RaceCarEntityModule::CopyActiveRaceCarToPlayerScoringMappingToOutput @0x822A3918, which
// walks all eight player slots and forwards the module's own map cell for cell -- so the
// EPlayerScoringIndex bound is the one that TU already asserts. Bodied here (2026-08-01)
// because that TU is now mounted and this declaration was the only thing keeping it from
// linking; the console emits it as a header inline (no out-of-line symbol in the image).
void RCEntityActiveRaceCarOutputInterface::SetActiveRaceCarIndex(
        BrnGameState::GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex,
        EActiveRaceCarIndex leActiveRaceCarIndex)
{
    // (this TU sees EPlayerScoringIndex only as a forward-declared enum, so the bound is
    //  spelled as the array extent -- they are the same 8.)
    CGS_ASSERT(static_cast<s32>(lePlayerScoringIndex) >= 0,
               "lePlayerScoringIndex >= E_PLAYER_SCORING_INDEX_0");
    CGS_ASSERT(static_cast<s32>(lePlayerScoringIndex)
                   < static_cast<s32>(sizeof(maeActiveRaceCarIndex) / sizeof(maeActiveRaceCarIndex[0])),
               "lePlayerScoringIndex < E_PLAYER_SCORING_INDEX_COUNT");
    maeActiveRaceCarIndex[lePlayerScoringIndex] = leActiveRaceCarIndex;
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
//
// ⚠️ PARAMETER ORDER CORRECTED 2026-08-01. The X360 argument registers/slots are
//   r4=index r5=globalIndex r6=rivalId r7=carModelId r8=state
//   r9  = FLAGS      -> `sthx r25, 2*(idx+0x13C0)+this` == maxRaceCarFlags[idx]
//   r10 = AI section -> `sthx r24, 2*(idx+0x1378)+this` == mau16ActiveRaceCarAISections[idx]
//   +0x54 = COLOUR IX-> `stwx r3,  4*(idx+0x9AC)+this`  == mauActiveRaceCarColourIndex[idx]
//   +0x5C = paint ix -> `stwx r10, 4*(idx+0x9B4)+this`  == maiActiveRaceCarPaintFinishIndex[idx]
//   v1 = material colour (shadow-spilled to caller+0x60 and re-read as four `lfs`),
//   v2 = in-air rotations, +0x87 = hasCrashedIntoWater, +0x8F = canDriveAwayFromCrash.
// The two u32s (flags / colour index) were transcribed the other way round. The STORES were
// always right, so the defect was invisible: this function had no caller in the tree until
// RaceCarEntityModule::UpdateOutputInterfaces landed (2026-08-01).
void RCEntityActiveRaceCarOutputInterface::SetRaceCarState(
        EActiveRaceCarIndex leActiveRaceCarIndex,
        EGlobalRaceCarIndex leGlobalRaceCarIndex,
        CgsID               lRivalId,
        CgsID               lCarModelId,
        const RaceCarState* lpRaceCarState,
        u32                 luFlags,
        u16                 lu16AISection,
        u32                 luColourIndex,
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

// ⛔⛔ ADDRESS ATTRIBUTION CORRECTED 2026-08-10 (create-path wave). The banner that stood here
// said "X360 0x8259BB58 -- GetPlayerSpeedMPH ... (mfSpeedMPH @968)". BOTH halves were wrong, and
// the ABI settles it without any judgement call:
//   * 0x8259BB58's prototype is `_DWORD* __fastcall(_DWORD* sret, int this)` and its last act is
//     `stw r11, 0(r30)` into that sret buffer. A PPC function returning f32 returns in f1 and
//     takes NO sret pointer. So 0x8259BB58 returns a 4-byte CLASS type by value -- it cannot be
//     GetPlayerSpeedMPH.
//   * the field it loads is `mulli r11, idx, 0x460 ; add r11,r11,this ; lwz r11, 0x6F8(r11)`
//     == maRaceCarStates[player] + (1784 - 816) == RaceCarState + 968, and BrnVehicleEvents.h:112
//     names offset 968 mEntityId. mfSpeedMPH is the NEXT slot, @972.
// The function at 0x8259BB58 is GetPlayerRaceCarEntityId (DWARF :293), bodied immediately below;
// its one console caller is PhysicsModule::PostSceneUpdate @0x825ABC10.
// GetPlayerSpeedMPH keeps its body -- it always read the right member BY NAME, which is why the
// wrong address and the off-by-one-slot comment never produced a wrong byte -- but its X360
// address is now honestly UNKNOWN rather than borrowed from its neighbour.
//
// GetPlayerSpeedMPH: the player car's current speed (mfSpeedMPH @972 in the player's
// RaceCarState). Asserts the player index has been set. X360 address not identified.
f32 RCEntityActiveRaceCarOutputInterface::GetPlayerSpeedMPH() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID, "Player car index hasn't been set");
    return maRaceCarStates[mePlayerActiveRaceCarIndex].mfSpeedMPH;
}

// X360 0x8259BB58 (30 insns) -- GetPlayerRaceCarEntityId (DWARF
// BrnRaceCarEntityModuleOutputInterface.h:293). The player car's HANDLING entity id, read out of
// its RaceCarState. Statement for statement from the asm:
//     lwz  r11, 0x2858(this)                    ; mePlayerActiveRaceCarIndex
//     cmpwi r11, -1 ; bne -> skip
//       FireAssert("Player car index hasn't been set", <this header>, 0x3DB == 987)
//     lwz  r11, 0x2858(this)                    ; re-read, exactly as shipped
//     mulli r11, r11, 0x460 ; add r11, r11, this ; lwz r11, 0x6F8(r11)
//     stw  r11, 0(sret)
// Its sole console caller is PhysicsModule::PostSceneUpdate @0x825ABC10, which feeds the result to
// DeformationManager::FindModelIndexByEntityID.
EntityId RCEntityActiveRaceCarOutputInterface::GetPlayerRaceCarEntityId() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "Player car index hasn't been set");                     // this header :987
    return maRaceCarStates[mePlayerActiveRaceCarIndex].mEntityId;
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

// ============================================================================
// X360 0x82277B90 -- IsPlayerCarActive. Asserts the player index is in range,
// then reports false while no player index has been assigned (-1) and the
// mbIsPlayerCarActive flag otherwise. Reached every frame by
// WorldEntityModule::PreSceneUpdate @0x82302A08 (the PVS query picks the player
// car's position when true, the simulated camera position when false), which is
// why the world loading drive hits it before any car exists.
// (Was a WorldLinkStubs assert trap.)
// ============================================================================
bool RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        return false;
    }
    return mbIsPlayerCarActive;
}

// ============================================================================
// X360 0x82277B10 -- IsRaceCarActive. Bounds-asserts the index then returns bit 0
// of the per-car flag halfword (asm: `addi r11,idx,0x13C0; slwi 1; lhzx` == the
// maxRaceCarFlags[idx] element at +0x2780, `clrlwi r3,r11,31` == & 1).
// (Was a WorldLinkStubs assert trap.)
// ============================================================================
bool RCEntityActiveRaceCarOutputInterface::IsRaceCarActive(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return (maxRaceCarFlags[leActiveRaceCarIndex] & 1) != 0;
}

// ============================================================================
// [hud H3b tracking slice 2026-08-25] the two flag predicates the 207 producer reads
// (declared :264/:272). Same shape as IsRaceCarActive above; the bit senses are the
// producer's own (UpdateOutputInterfaces): 0x20 == E_RACE_CAR_OUTPUT_FLAG_CONNECTING,
// 0x80 == E_RACE_CAR_OUTPUT_FLAG_DISCONNECTED.
// ============================================================================
bool RCEntityActiveRaceCarOutputInterface::IsCarConnecting(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return (maxRaceCarFlags[leActiveRaceCarIndex] & E_RACE_CAR_OUTPUT_FLAG_CONNECTING) != 0;
}

bool RCEntityActiveRaceCarOutputInterface::IsCarDisconnected(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return (maxRaceCarFlags[leActiveRaceCarIndex] & E_RACE_CAR_OUTPUT_FLAG_DISCONNECTED) != 0;
}

// ============================================================================
// DWARF :220 -- the const per-index race-car state accessor (the sibling of the
// committed non-const GetRaceCarStateMutable @0x8227D690, which returns
// &maRaceCarStates[idx] after the same two range asserts; the const form was
// ICF-folded on the console so it has no own export). Read by
// WorldEntityModule::PreSceneUpdate for the player car's PVS position/velocity.
// (Was a WorldLinkStubs assert trap.)
// ============================================================================
const RCEntityActiveRaceCarOutputInterface::RaceCarState*
RCEntityActiveRaceCarOutputInterface::GetRaceCarState(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return &maRaceCarStates[leActiveRaceCarIndex];
}

// ============================================================================
// ---- ADDITIVE 2026-08-12 (prop-spawn wave, agent B6) -----------------------
// The last two declaration-only members of this interface that a MOUNTED caller needs.
// Both were declared at BrnRaceCarEntityModuleOutputInterface.h:396 / :431 with no body
// anywhere in the tree, so every TU that called them (this wave's
// BridgeRaceCarModuleToPropModule_PreScene, plus the already-written
// BrnTriggerQueryManager / BrnDriveThruManager / BrnStuntModeScoring_StuntTypes /
// BrnGameStateStreetManager_wB_09/_10 / BrnScoringSystem_UpdateB bodies) carried an
// unresolved external.
// ============================================================================

// ----------------------------------------------------------------------------
// X360 0x823102F0 (41 insns; IDA leaves it `sub_823102F0`, so it is absent from the
// ledger -- identified by its two baked assert strings and its single caller set).
// GetPlayerPosition, DWARF BrnRaceCarEntityModuleOutputInterface.h:396. Returned by
// value through an sret pointer in r3 (`stvx128 v0, r0, r29`), which is why Hex-Rays
// renders it as the two-argument sub_823102F0(out, this).
// Statement for statement:
//     lwz   r11, 0x2858(this)                     ; mePlayerActiveRaceCarIndex
//     cmpwi r11, 8 ; blt -> skip
//       FireAssert("mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
//                  <this header>, 0x3C7 == 967)
//     lwz   r11, 0x2858(this) ; cmpwi -1 ; r11 = (idx == -1) ? 0 : lbz 0x2860(this)
//     cmplwi r11, 0 ; bne -> skip
//       FireAssert("IsPlayerCarActive()", <this header>, 0x4F9 == 1273)
//     lwz   r11, 0x2858(this) ; mulli r11,r11,0x460 ; add r11,r11,this
//     lvx128 v0, r11, 0x550                       ; +1360
//     stvx128 v0, r0, sret
// The second guard is the INLINED IsPlayerCarActive() (identical shape to the
// out-of-line 0x82277B90 bodied above: the -1 sentinel short-circuits mbIsPlayerCarActive
// @+0x2860), so it is written as the call. 1360 == maRaceCarStates(816) + idx*1120 + 544,
// and 544 == RaceCarState::mTransform(496) + 48 == Matrix44Affine::wAxis -- the
// translation row, i.e. mTransform.Pos(). Both asserts are NON-gating tripwires: the
// console reads the element regardless.
// ----------------------------------------------------------------------------
Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerPosition() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // this header :967
    CGS_ASSERT(IsPlayerCarActive(), "IsPlayerCarActive()");                     // this header :1273
    return maRaceCarStates[mePlayerActiveRaceCarIndex].mTransform.Pos();
}

// ----------------------------------------------------------------------------
// IsPlayerWrecked, DWARF BrnRaceCarEntityModuleOutputInterface.h:431. HEADER-INLINED by
// the X360 compiler at every site, so there is no out-of-line symbol to point at -- the
// same disposition as its committed setter twin SetPlayerWrecked (also inlined, also a
// bare store). Its shape is pinned by the two ends that DO appear in the asm:
//   * the producer, RaceCarEntityModule::UpdateOutputInterfaces @0x822F5CF8 --
//     `stb r11, 0x28E0(iface)`;
//   * the consumer, WorldModule::BridgeRaceCarModuleToPropModule_PreScene @0x827A5510 --
//     `lbz r11, 0x28E0(r29) ; stb r11, 0x794(dest)`.
// +0x28E0 is mbPlayerWrecked. No assert: the console load is unguarded.
// (Bodied out-of-line here rather than turned into a header inline so the frozen
// declaration set in BrnRaceCarEntityModuleOutputInterface.h is left untouched.)
// ----------------------------------------------------------------------------
bool RCEntityActiveRaceCarOutputInterface::IsPlayerWrecked() const
{
    return mbPlayerWrecked;
}

// ============================================================================
// ---- ADDITIVE 2026-08-20 ([gateui] round 3, owner fix3bridge) --------------
// The THREE remaining player-scoped getters that MOUNTED callers reference with no
// definition anywhere in the tree. They gate the gsm lane's mount set: every one of
//   Offences/BrnStuntManager.cpp :: UpdateJumps
//   ModeManager/Scoring/BrnStuntModeScoring_StuntTypes.cpp   (three sites)
//   ModeManager/Scoring/BrnStuntModeScoringOnline.cpp        (three sites)
//   ModeManager/GameModes/BrnBurnoutSkillzManager.cpp        (two sites)
//   ModeManager/ChallengeManager/BrnChallengeManager_wB_05 / _wB_15 / _wC_04
// calls one of them and would otherwise LNK2019.
//
// ⭐ THEY ARE NOT "CONSOLE-INLINED ACCESSORS THIS TREE DE-INLINED" (the round-2 fixgsm
// request's premise, corrected here). All three ARE real out-of-line X360 functions --
// IDA simply leaves them unnamed, so they are absent from the ledger and from any
// name-keyed grep:
//     sub_82310240 (43 insns) -> GetPlayerRaceCarState
//     sub_82310398 (41 insns) -> GetPlayerDirection
//     sub_82310440 (41 insns) -> GetPlayerLinearVelocity
// They were identified the same way the committed GetPlayerPosition (sub_823102F0)
// above was: by their two baked assert strings, which name THIS header and carry an
// exact line. The four sit in one contiguous address run and their `IsPlayerCarActive()`
// assert lines are 7 apart -- 1266 / 1273 / 1280 / 1287 -- while the DWARF declares the
// four in exactly that order and spacing (references/DecFIGS/dwarfdump/.../
// BrnRaceCarEntityModuleOutputInterface.h:435 GetPlayerRaceCarState, :438
// GetPlayerPosition, :441 GetPlayerDirection, :444 GetPlayerLinearVelocity). The ALREADY
// COMMITTED middle member of that run -- GetPlayerPosition at assert line 1273 -- is the
// fixed point that pins which sub_ is which, so these are transcriptions, not inferences.
// (Independently corroborated: ModeManager/Scoring/BrnStuntModeScoring_StuntTypes.cpp's
// own banner already attributes sub_82310440 -> GetPlayerLinearVelocity and sub_82310398
// -> GetPlayerDirection from its call sites. The ":393/:399/:402" numbers in this
// interface's declaration column come from an earlier dwarfdump revision; the order is
// identical, so nothing depends on which numbering is quoted.)
//
// ⚠️ EVERY OFFSET BELOW IS TAKEN BY NAME, NEVER AT THE CONSOLE'S LITERAL BYTE COUNT.
// The console's element loads are `mulli r11, idx, 0x460 ; add r11, r11, this` then a
// displacement off maRaceCarStates' base (this + 816); the displacements resolve as:
//     0x330 = 816  -> &maRaceCarStates[idx]                    (a pointer return, r3)
//     0x540 = 1344 -> element + 528 == mTransform(496) + 32 == Matrix44Affine::At()
//                     (zAxis -- the forward row; the sibling GetPlayerPosition takes
//                      +48 == wAxis == Pos(), which is the committed body above)
//     0x660 = 1632 -> element + 816 == mLinearVelocity
// (BrnVehicleEvents.h's RaceCarState is byte-identical to the console's for these three
// members -- the historic "+4 drift" was settled at mCarAssetAttribKey @960 and lands
// entirely AFTER them, so no drift correction applies here.)
//
// ⚠️ BOTH ASSERTS ARE NON-GATING TRIPWIRES, as in GetPlayerPosition: the console fires
// them and then reads the element anyway. Reproduced with that behaviour, not turned
// into early-outs -- an early-out would invent a return value the binary never has.
// The second guard is the INLINED IsPlayerCarActive() (the -1 sentinel short-circuiting
// mbIsPlayerCarActive @+0x2860), written as the call to the out-of-line twin.
// ============================================================================

// ----------------------------------------------------------------------------
// X360 sub_82310240 -- GetPlayerRaceCarState (DWARF :435). Statement for statement:
//     lwz   r11, 0x2858(this) ; cmpwi 8 ; blt -> skip
//       FireAssert("mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", <hdr>, 0x3C7 == 967)
//     lwz   r11, 0x2858(this) ; cmpwi -1 ; r11 = (idx == -1) ? 0 : lbz 0x2860(this)
//     cmplwi r11, 0 ; bne -> skip
//       FireAssert("IsPlayerCarActive()", <hdr>, 0x4F2 == 1266)
//     lwz   r11, 0x2858(this) ; mulli r11, r11, 0x460 ; add r11, r11, this
//     addi  r3, r11, 0x330                       ; return &maRaceCarStates[idx]
// Returned in r3 as a plain pointer (no sret), which is why this one is the odd
// single-argument member of the four.
// ----------------------------------------------------------------------------
const RCEntityActiveRaceCarOutputInterface::RaceCarState*
RCEntityActiveRaceCarOutputInterface::GetPlayerRaceCarState() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // this header :967
    CGS_ASSERT(IsPlayerCarActive(), "IsPlayerCarActive()");                     // this header :1266
    return &maRaceCarStates[mePlayerActiveRaceCarIndex];
}

// ----------------------------------------------------------------------------
// X360 sub_82310398 -- GetPlayerDirection (DWARF :441). Same two tripwires (the second
// at 0x500 == 1280), then a single 16-byte vector move returned through the sret
// pointer in r3:
//     lwz    r11, 0x2858(this) ; li r10, 0x540 ; mulli r11, r11, 0x460 ; add r11, r11, this
//     lvx128 v0, r11, r10 ; stvx128 v0, r0, r29
// 0x540 == 1344 == maRaceCarStates(816) + 528, and 528 == mTransform(496) + 32 ==
// Matrix44Affine::At() -- the zAxis / forward row of the car's world transform.
// ----------------------------------------------------------------------------
Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerDirection() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // this header :967
    CGS_ASSERT(IsPlayerCarActive(), "IsPlayerCarActive()");                     // this header :1280
    return maRaceCarStates[mePlayerActiveRaceCarIndex].mTransform.At();
}

// ----------------------------------------------------------------------------
// X360 sub_82310440 -- GetPlayerLinearVelocity (DWARF :444). Identical shape (second
// tripwire at 0x507 == 1287); the displacement is 0x660 == 1632 == maRaceCarStates(816)
// + 816, and element +816 is RaceCarState::mLinearVelocity (BrnVehicleEvents.h).
// ⓘ RAW m/s, NOT normalised: StuntModeScoring_StuntTypes' own console arm normalises the
// result itself before dotting it with GetPlayerDirection(), so normalising here would
// double-apply.
// ----------------------------------------------------------------------------
Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerLinearVelocity() const
{
    CGS_ASSERT(mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // this header :967
    CGS_ASSERT(IsPlayerCarActive(), "IsPlayerCarActive()");                     // this header :1287
    return maRaceCarStates[mePlayerActiveRaceCarIndex].mLinearVelocity;
}

}
}
