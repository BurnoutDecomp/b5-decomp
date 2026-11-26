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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // the accessors' ARTIST assert lines
#include "GameSource/World/BrnWorldSharedConstants.h"                  // KI_MAX_RIVALS_IN_WORLD
#include "BrnCommonTypes.h"                                        // Matrix44Affine, Vector3, CgsID
#include "GameSource/BurnoutConstants.h"                          // EGlobalRaceCarIndex, EActiveRaceCarIndex
#include "GameSource/World/AI/BrnAISharedConstants.h"             // BrnAI::EResetType
#include "SharedClasses/World/BrnWorldRegion.h"                   // BrnWorld::WorldRegion, EDistrict, ECounty
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // BrnWorld::ERaceCarType

// The original header's __FILE__ as ARTIST bakes it into these accessors' asserts.
#define BRN_RACECAR_H_ASSERT_FILE "..\\..\\..\\GameSource\\World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"

namespace CgsWorld { struct WorldMap2D; }

namespace BrnWorld
{
namespace RaceCarEntityModuleIO { struct RCEntityGlobalRaceCarOutputInterface; }

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
    // The original header defines these accessors inline after the class, each with its own
    // asserts; ARTIST bakes BrnRaceCar.h's lines into every copy (out-of-line COMDATs such as
    // GetPosition @0x822B3500, IsInWorld @0x822A0C70, and every inlined call site). The type
    // predicates assert `muType < E_RACE_CAR_TYPE_COUNT` themselves (IsActive h:482,
    // IsInWorld h:547, IsPlayerDriven h:577, IsNetworkDriven h:590, IsAIDriven h:603), and
    // the IsInWorld-guarded getters add their own "IsInWorld()" assert on top (LegacySrc
    // BrnRaceCar.h has the same shape). MessageAt carries the ARTIST lines.
    ERaceCarType GetType() const { return static_cast<ERaceCarType>(muType); }
    Vector3 GetPosition() const                                  // ARTIST 0x822B3500
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 365);
        return mTransform.Pos();
    }
    Vector3 GetPreviousPosition() const                          // ARTIST 0x822B3588
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 372);
        return mPreviousPosition;
    }
    Vector3 GetDirection() const                                 // ARTIST 0x822B3610
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 379);
        return mTransform.At();
    }
    Vector3 GetVelocity() const                                  // ARTIST 0x822B3698
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 386);
        return mVelocity;
    }
    Matrix44Affine GetTransform() const { return mTransform; }

    ActiveRaceCar* GetActiveRaceCar();                          // 0x822B3988

    EGlobalRaceCarIndex GetGlobalRaceCarIndex() const            // ARTIST 0x822B3A18
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 408);
        return static_cast<EGlobalRaceCarIndex>(miGlobalRaceCarIndex);
    }

    s32 GetOpponentIndex() const { return miOpponentIndex; }
    CgsID GetRivalId() const                                     // ARTIST 0x822B3AA8
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 421);
        return mRivalId;
    }
    CgsID GetModelId() const                                     // ARTIST 0x822B3B38
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 428);
        CGS_ASSERT_AT(mModelId != 0, "mModelId != kCGSID_NULL", BRN_RACECAR_H_ASSERT_FILE, 429);
        return mModelId;
    }
    CgsID GetWheelModelId() const                                // ARTIST 0x822B3BF0
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 436);
        CGS_ASSERT_AT(mWheelId != 0, "mWheelId != kCGSID_NULL", BRN_RACECAR_H_ASSERT_FILE, 437);
        return mWheelId;
    }

    bool ToBeResetOnTrack() const { return mbToBeResetOnTrack; }
    BrnAI::EResetType GetResetOnTrackType() const { return meResetOnTrackType; }
    f32 GetResetOnTrackSpeed() const { return mfResetOnTrackSpeed; }
    f32 GetResetOnTrackDist() const { return mfResetOnTrackDistance; }

    s8 GetRivalIndex() const                                     // ARTIST 0x822B3CA8
    {
        CGS_ASSERT_AT(IsAIDriven(), "IsAIDriven()", BRN_RACECAR_H_ASSERT_FILE, 468);
        CGS_ASSERT_AT((miRivalIndex >= 0 && miRivalIndex < KI_MAX_RIVALS_IN_WORLD) || miRivalIndex == KI_INVALID_RIVAL,
                   "( miRivalIndex >= 0 && miRivalIndex < KI_MAX_RIVALS_IN_WORLD ) || miRivalIndex == KI_INVALID_RIVAL",
                       BRN_RACECAR_H_ASSERT_FILE, 469);

        return miRivalIndex;
    }
    EDistrict GetRivalDistrict() const { return static_cast<EDistrict>(miRivalDistrict); }

    bool IsActive() const                                        // HasActiveRaceCar @0x822A0CD8
    {
        CGS_ASSERT_AT(muType < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT", BRN_RACECAR_H_ASSERT_FILE, 482);
        return muType != E_RACE_CAR_TYPE_INACTIVE;
    }
    bool IsInWorld() const                                       // ARTIST 0x822A0C70
    {
        CGS_ASSERT_AT(muType < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT", BRN_RACECAR_H_ASSERT_FILE, 547);
        return muType != E_RACE_CAR_TYPE_INACTIVE;
    }
    bool HasActiveRaceCar() const;                             // 0x822A0CD8

    // X360 0x822BED20 -- publish this car's global snapshot (hud H3b tracking slice).
    // Body in BrnRaceCar.cpp; the caller (UpdateOutputInterfaces step 5) computes the
    // speed/AI-section pair off the attached active car.
    void FillInOutputInterface(
        RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalCarInterface,
        f32 lfSpeed,
        u16 lu16AISection);
    bool IsPlayerDriven() const                                  // ARTIST 0x822A0D60
    {
        CGS_ASSERT_AT(muType < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT", BRN_RACECAR_H_ASSERT_FILE, 577);
        return muType == E_RACE_CAR_TYPE_PLAYER;
    }
    bool IsNetworkDriven() const                                 // ARTIST 0x822A0DC0
    {
        CGS_ASSERT_AT(muType < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT", BRN_RACECAR_H_ASSERT_FILE, 590);
        return muType == E_RACE_CAR_TYPE_NETWORK;
    }
    bool IsAIDriven() const                                      // ARTIST 0x822A0E28
    {
        CGS_ASSERT_AT(muType < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT", BRN_RACECAR_H_ASSERT_FILE, 603);
        return muType == E_RACE_CAR_TYPE_AI;
    }
    bool IsOutOfRangeRival() const;
    bool IsInRangeRival() const;
    bool IsInCurrentGameMode() const                             // ARTIST 0x822A0FA0
    {
        CGS_ASSERT_AT(IsInWorld(), "IsInWorld()", BRN_RACECAR_H_ASSERT_FILE, 668);
        return mbIsInGameMode;
    }

    EActiveRaceCarIndex GetActiveRaceCarIndex() const
    { return static_cast<EActiveRaceCarIndex>(miActiveRaceCarIndex); }

    bool CanPassThroughTraffic() const { return mbCanPassThroughTraffic; }
    void SetCanPassThroughTraffic(bool lbCanPass) { mbCanPassThroughTraffic = lbCanPass; }

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leIndex);   // 0x822A1030
    void ClearActiveRaceCarIndex() { miActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID; }
    void ClearActiveRaceCar() { mpActiveRaceCar = 0; }

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

    // DWARF BrnRaceCar.h:293..:300, the in/out-of-range radii (defined in BrnRaceCar.cpp).
    static const f32 KF_IN_RANGE_DISTANCE_RACE_AHEAD;        // :293
    static const f32 KF_IN_RANGE_DISTANCE_RACE_BEHIND;       // :294
    static const f32 KF_IN_RANGE_DISTANCE_FREE_ROAM;         // :295
    static const f32 KF_OUT_OF_RANGE_DISTANCE_RACE_AHEAD;    // :298
    static const f32 KF_OUT_OF_RANGE_DISTANCE_RACE_BEHIND;   // :299
    static const f32 KF_OUT_OF_RANGE_DISTANCE_FREE_ROAM;     // :300

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
