#pragma once

// ============================================================================
// BrnWorld::RaceCar -- one global race-car slot in the RaceCarEntityModule.
//
// A RaceCar is the always-resident record for a car the world tracks (the player,
// AI rivals, network cars). It holds the car's world transform, identity (model /
// wheel / rival CgsIDs), world region, reset-on-track request, and lifecycle flags.
// When the car is close enough to simulate it is paired with an ActiveRaceCar
// (mpActiveRaceCar) that owns the physics/AI state.
//
// LAYOUT: 0xB0 (176) bytes -- this is the X360-attested array stride used by
// RaceCarEntityModule::maRaceCars (see BrnRaceCarEntityModule.h: stride 0xB0). Every
// offset below is proven by the X360 ARTIST asm of Construct (0x822A4B08),
// Prepare (0x822A4BE0) and AddToWorld (0x822BE4F0); member NAMES/types come from the
// DecFIGS DWARF for BrnRaceCar.h. The DWARF also lists colour/opponent/persistent-
// damage members that the lifecycle code touches (Prepare zeroes them).
//
// Source-of-truth notes:
//  * Construct takes an EGlobalRaceCarIndex (DWARF :61) and stores it at +0xA5 as an
//    int8 (E_GLOBAL_RACE_CAR_INDEX_INVALID == -1 reads back as 0xFF).
//  * "rival" cars are E_RACE_CAR_TYPE_AI in the X360 build: AddToWorld's rival-index
//    branch is taken for muType == E_RACE_CAR_TYPE_AI (the asm `a2 == 1`).
//  * IsInWorld() is muType != E_RACE_CAR_TYPE_INACTIVE (the asserts spell it
//    "IsInWorld()" but the inlined test compares muType to 3).
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the inline accessors below)
#include "BrnCommonTypes.h"                                        // Matrix44Affine, Vector3, CgsID
#include "GameSource/BurnoutConstants.h"                          // EGlobalRaceCarIndex, EActiveRaceCarIndex
#include "GameSource/World/AI/BrnAISharedConstants.h"             // BrnAI::EResetType
#include "SharedClasses/World/BrnWorldRegion.h"                   // BrnWorld::WorldRegion, EDistrict, ECounty
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // BrnWorld::ERaceCarType

namespace CgsWorld { struct WorldMap2D; }

namespace BrnWorld
{
class ActiveRaceCar;

class RaceCar
{
public:
    // ---- Lifecycle (X360 verified) -----------------------------------------
    void Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex);   // 0x822A4B08
    bool Prepare();                                             // 0x822A4BE0
    void Release();
    void Destruct();

    // 0x822BE4F0. "rival" == E_RACE_CAR_TYPE_AI; liRivalIndex/liOpponentIndex only
    // meaningful for that type. The three CgsIDs are rival/model/wheel.
    void AddToWorld(ERaceCarType leType,
                    const Matrix44Affine& lTransform,
                    CgsID lRivalId,
                    CgsID lModelId,
                    CgsID lWheelModelId,
                    s8 liRivalIndex,
                    s32 liOpponentIndex);

    void AssignActiveRaceCar(ActiveRaceCar* lpActiveRaceCar);   // 0x822BE8C8
    void RemoveActiveRaceCar();                                 // 0x822BEA00
    void RemoveFromWorld();                                     // 0x822A4C98

    // 0x822D3788. Saves the current position, stores the new transform, then resolves
    // the world region (district) from the 2D world map at the new position.
    void UpdatePositioningData(const Matrix44Affine& lTransform, CgsWorld::WorldMap2D* lpWorldMap);

    void UpdateVelocity(Vector3 lVelocity);
    void SetStartingPositionOnGrid(s8 liGridPosition);
    void SetInCurrentGameMode(bool lbInGameMode, bool lbCarSelectAllowed);

    // ---- Simple state accessors --------------------------------------------
    ERaceCarType GetType() const { return static_cast<ERaceCarType>(muType); }
    // GetPosition @0x822B3588 / GetDirection @0x822B3610. Both are one-line transform-row
    // reads guarded by the same two asserts, and the X360 INLINES both at every call site
    // (e.g. ActiveRaceCar::GetDirection @0x822CD038 carries the expanded assert pair). Homed
    // here as inline members -- alongside GetTransform/GetVelocity, which were already
    // header-inline -- so a consumer of this class can link without BrnRaceCar.cpp's whole
    // lifecycle TU, which needs the WorldRegion district/county NAME TABLES: pure .rodata
    // that the function-only IDA export cannot supply. Bodies verbatim from the .cpp.
    Vector3 GetPosition() const
    {
        CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
        CGS_ASSERT(IsInWorld(), "IsInWorld()");

        return mTransform.Pos();
    }
    Vector3 GetPreviousPosition() const { return mPreviousPosition; }
    Vector3 GetDirection() const
    {
        CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
        CGS_ASSERT(IsInWorld(), "IsInWorld()");

        return mTransform.At();
    }
    Vector3 GetVelocity() const { return mVelocity; }
    Matrix44Affine GetTransform() const { return mTransform; }

    ActiveRaceCar* GetActiveRaceCar();                          // 0x822B3988

    EGlobalRaceCarIndex GetGlobalRaceCarIndex() const
    { return static_cast<EGlobalRaceCarIndex>(miGlobalRaceCarIndex); }

    s32 GetOpponentIndex() const { return miOpponentIndex; }
    CgsID GetRivalId() const { return mRivalId; }
    CgsID GetModelId() const { return mModelId; }
    CgsID GetWheelModelId() const { return mWheelId; }

    bool ToBeResetOnTrack() const { return mbToBeResetOnTrack; }
    BrnAI::EResetType GetResetOnTrackType() const { return meResetOnTrackType; }
    f32 GetResetOnTrackSpeed() const { return mfResetOnTrackSpeed; }
    f32 GetResetOnTrackDist() const { return mfResetOnTrackDistance; }

    s8 GetRivalIndex() const { return miRivalIndex; }
    EDistrict GetRivalDistrict() const { return static_cast<EDistrict>(miRivalDistrict); }

    bool IsActive() const { return muType != E_RACE_CAR_TYPE_INACTIVE; }
    bool IsInWorld() const { return muType != E_RACE_CAR_TYPE_INACTIVE; }
    bool HasActiveRaceCar() const;                             // 0x822A0CD8
    bool IsPlayerDriven() const { return muType == E_RACE_CAR_TYPE_PLAYER; }
    bool IsNetworkDriven() const { return muType == E_RACE_CAR_TYPE_NETWORK; }
    bool IsAIDriven() const { return muType == E_RACE_CAR_TYPE_AI; }
    bool IsOutOfRangeRival() const;
    bool IsInRangeRival() const;
    bool IsInCurrentGameMode() const { return mbIsInGameMode; }

    EActiveRaceCarIndex GetActiveRaceCarIndex() const
    { return static_cast<EActiveRaceCarIndex>(miActiveRaceCarIndex); }

    bool CanPassThroughTraffic() const { return mbCanPassThroughTraffic; }
    void SetCanPassThroughTraffic(bool lbCanPass) { mbCanPassThroughTraffic = lbCanPass; }

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leIndex);   // 0x822A1030
    void ClearActiveRaceCarIndex() { miActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID; }
    void ClearActiveRaceCar() { mpActiveRaceCar = nullptr; }

    s8 GetStartingGridPosition() { return miStartingGridPosition; }
    s8 IsInRace();
    bool IsDispersing() const { return mbIsDispersing; }
    void SetDispersing(bool lbDispersing) { mbIsDispersing = lbDispersing; }

    // 0x822BEB28. Records a reset-on-track request (unless one is pending, or the
    // active car is already queued to be placed on track).
    void RequestResetOnTrack(f32 lfSpeed, BrnAI::EResetType leType, f32 lfDistance);
    void ClearResetOnTrack() { mbToBeResetOnTrack = false; }

    WorldRegion GetWorldRegion() const { return mWorldRegion; }
    bool IsAllowedInRoadRage() { return mbIsAllowedInRoadRage; }
    void SetAllowedInRoadRage(bool lbAllowed) { mbIsAllowedInRoadRage = lbAllowed; }

    bool ShouldBeInRange(const RaceCar* lpPlayerRaceCar) const;
    bool ShouldBeOutOfRange(const RaceCar* lpPlayerRaceCar) const;

    void SetColourIndex(s32 liColourIndex) { miColourIndex = liColourIndex; }
    void SetColourPalette(s32 liColourPalette) { miColourPalette = liColourPalette; }
    void ResetColourIndex();
    s32 GetColourIndex() const { return miColourIndex; }
    s32 GetColourPalette() const { return miColourPalette; }

    bool ToBeRenderedDamaged() const;
    f32 GetPersistentDamage() const { return mfPersistentDamage; }
    bool IncreasePersistentDamage();

private:
    bool AreCarsHeadOn(const RaceCar* lpRaceCar0, const RaceCar* lpRaceCar1) const;

    // ---- Layout (176 bytes; offsets X360-asm-proven) -----------------------
    Matrix44Affine    mTransform;                 // +0x00  world transform (wAxis == position)
    Vector3           mPreviousPosition;          // +0x40
    Vector3           mVelocity;                   // +0x50
    CgsID             mRivalId;                    // +0x60
    CgsID             mModelId;                    // +0x68
    CgsID             mWheelId;                    // +0x70
    ActiveRaceCar*    mpActiveRaceCar;             // +0x78
    WorldRegion       mWorldRegion;                // +0x7C  (meCounty +0x7C, meDistrict +0x80)
    f32               mfResetOnTrackSpeed;         // +0x84
    f32               mfResetOnTrackDistance;      // +0x88
    BrnAI::EResetType meResetOnTrackType;          // +0x8C
    bool              mbToBeResetOnTrack;          // +0x90
    s32               miColourIndex;               // +0x94
    s32               miColourPalette;             // +0x98
    s32               miOpponentIndex;             // +0x9C
    f32               mfPersistentDamage;          // +0xA0
    u8                muType;                       // +0xA4  ERaceCarType (stored as byte)
    s8                miGlobalRaceCarIndex;        // +0xA5  EGlobalRaceCarIndex (byte; -1 == INVALID)
    bool              mbIsInGameMode;              // +0xA6
    bool              mbCarSelectAllowedInGameMode;// +0xA7
    s8                miRivalIndex;                // +0xA8
    s8                miRivalDistrict;             // +0xA9
    s8                miStartingGridPosition;      // +0xAA
    bool              mbIsDispersing;              // +0xAB
    s8                miActiveRaceCarIndex;        // +0xAC  EActiveRaceCarIndex (byte; -1 == INVALID)
    bool              mbCanPassThroughTraffic;     // +0xAD
    bool              mbIsAllowedInRoadRage;       // +0xAE
    u8                muPadAF_;                    // +0xAF  (tail pad to the 0xB0 stride)
};
}
