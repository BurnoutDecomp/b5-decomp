// FX-BRIDGES (crash parity 2026-09-24) CC-6: the PRODUCTION step 13 of
// BrnGame::BrnGameModule::BridgeWorldToDirector @0x823E3AB0 -- the region of
// src/GameSource/Game/GameBridgeWorldToX.cpp from `BrnDirector::Camera::PlayerCrashInfo lPlayerCrashInfo;`
// through `lpDirectorInput->SetPlayerCrashInfo(&lPlayerCrashInfo);`, extracted verbatim by
// run_fxbridges_crash_info.py -- compiled against the real PlayerCrashInfo / RaceCarCrashEvent /
// RaceCarCrashEventQueue / RaceCarState and small fixtures for the three interfaces it calls.
//
// Checked against the ARTIST asm (0x823E4DF0..0x823E4FE4):
//   Construct        vectors 0, mfSpeedMPH = f31 (0.0), hard-stop bools 0
//   mbWrecked        `lbz 0x28E0(iface)` = IsPlayerWrecked()
//   mbHitWater       HasCrashedIntoWater(GetPlayerActiveRaceCarIndex()) -- the INTERFACE's player index
//   queue            GetVehicleManagerOutputInterface()+0x3A0, `lwz 8` length, UNSIGNED loop, stride 0x40
//   match            (u64 at event+0) >> 32 == GetRaceCarState(<world output's player index>)->mEntityId
//   copy             +0x34 speed, +0x10 normal, +0x20 contact point -- LAST match wins
//   owner            `lbz 8` (high byte of mCrasherEntityID): 0 -> mbHardstopVsWall, 1 / 2 -> mbHardStopVsAI,
//                    anything else -> neither; the flags only ever set (they accumulate)
//   publish          48 bytes into the director input (SetPlayerCrashInfo)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameSource/World/BrnEntityTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

using BrnDirector::Camera::PlayerCrashInfo;
using BrnPhysics::Vehicle::RaceCarCrashEvent;
using BrnPhysics::Vehicle::RaceCarState;
typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface::RaceCarCrashEventQueue CrashQueue;

struct ActiveRaceCarsFixture
{
    bool                 mbWrecked;
    bool                 mabWater[8];
    EActiveRaceCarIndex  mePlayer;
    RaceCarState         maStates[8];
    mutable s32          miWaterIndexAsked;

    bool IsPlayerWrecked() const { return mbWrecked; }
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return mePlayer; }
    bool HasCrashedIntoWater(EActiveRaceCarIndex leIndex) const
    {
        miWaterIndexAsked = static_cast<s32>(leIndex);
        return mabWater[leIndex];
    }
    const RaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const { return &maStates[leIndex]; }
};

struct VehicleManagerOutputFixture
{
    CrashQueue mQueue;
    const CrashQueue* GetRaceCarCrashEventQueue() const { return &mQueue; }
};

struct WorldOutputFixture
{
    VehicleManagerOutputFixture mVehicleManager;
    const VehicleManagerOutputFixture* GetVehicleManagerOutputInterface() const { return &mVehicleManager; }
};

struct DirectorInputFixture
{
    PlayerCrashInfo mInfo;
    s32             miCalls;
    void SetPlayerCrashInfo(const PlayerCrashInfo* lpObject) { mInfo = *lpObject; ++miCalls; }
};

// The region under test, wrapped with the locals it reads under their production names.
static void Step13(const ActiveRaceCarsFixture* lpActiveRaceCars, const WorldOutputFixture* lpWorldOutput,
                   EActiveRaceCarIndex lePlayerIndex, DirectorInputFixture* lpDirectorInput)
{
#include "fxbridges_crash_info_region.inc"
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static bool Same(const Vector3& a, const Vector3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static const u32 KU_PLAYER_ENTITY = 0x01000C00u;   // owner 1 (race car), index 3
static const u32 KU_OTHER_ENTITY  = 0x01000400u;   // owner 1, index 1

static RaceCarCrashEvent MakeCrash(u32 luVictimEntity, u32 luOwner, f32 lfSpeed, f32 lfNormalX, f32 lfPointZ)
{
    RaceCarCrashEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.mRaceCarVolumeInstanceID.muId = static_cast<u64>(luVictimEntity) << 32;   // entity word high
    lEvent.mCrasherEntityID.muValue      = (luOwner << 24) | 0x400u;
    lEvent.mfSpeedMPH                    = lfSpeed;
    lEvent.mCollisionNormal              = Vector3{ lfNormalX, 0.5f, 0.0f, 0.0f };
    lEvent.mContactPoint                 = Vector3{ 10.0f, 20.0f, lfPointZ, 0.0f };
    return lEvent;
}

static ActiveRaceCarsFixture gCars;
static WorldOutputFixture    gWorld;
static DirectorInputFixture  gInput;

static void Reset(bool lbWrecked, s32 liWaterSlot)
{
    gCars.mbWrecked = lbWrecked;
    for (s32 i = 0; i < 8; ++i) gCars.mabWater[i] = (i == liWaterSlot);
    gCars.mePlayer = E_ACTIVE_RACE_CAR_INDEX_3;
    gCars.miWaterIndexAsked = -99;
    for (s32 i = 0; i < 8; ++i) gCars.maStates[i].Clear();
    gCars.maStates[3].mEntityId.muValue = KU_PLAYER_ENTITY;
    gCars.maStates[1].mEntityId.muValue = KU_OTHER_ENTITY;
    gWorld.mVehicleManager.mQueue.Construct();
    std::memset(&gInput.mInfo, 0x5A, sizeof(gInput.mInfo));   // stale bytes: the step must overwrite all
    gInput.miCalls = 0;
}

int main()
{
    const Vector3 kZero = { 0.0f, 0.0f, 0.0f, 0.0f };

    // (1) nothing happened: the Construct()ed record is published once.
    Reset(false, -1);
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.miCalls == 1, "(1) SetPlayerCrashInfo is called once");
    Check(gInput.mInfo.mfSpeedMPH == 0.0f && Same(gInput.mInfo.mvCollisionNormal, kZero)
          && Same(gInput.mInfo.mvContactPoint, kZero), "(1) empty queue: vectors and speed are the cleared record");
    Check(!gInput.mInfo.mbHardstopVsWall && !gInput.mInfo.mbHardStopVsAI && !gInput.mInfo.mbWrecked
          && !gInput.mInfo.mbHitWater, "(1) empty queue: all four flags clear");

    // (2) the wrecked byte and the player's water flag.
    Reset(true, 3);
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mbWrecked, "(2) mbWrecked = IsPlayerWrecked() (lbz 0x28E0)");
    Check(gInput.mInfo.mbHitWater && gCars.miWaterIndexAsked == 3,
          "(2) mbHitWater = HasCrashedIntoWater(GetPlayerActiveRaceCarIndex())");

    // (3) the player crashes into the WORLD.
    Reset(false, -1);
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_PLAYER_ENTITY, BrnWorld::E_ENTITYTYPE_WORLD, 71.5f, -1.0f, 3.0f));
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mfSpeedMPH == 71.5f, "(3) mfSpeedMPH copied from the event (+0x34)");
    Check(gInput.mInfo.mvCollisionNormal.x == -1.0f && gInput.mInfo.mvCollisionNormal.y == 0.5f,
          "(3) mvCollisionNormal copied (+0x10)");
    Check(gInput.mInfo.mvContactPoint.z == 3.0f && gInput.mInfo.mvContactPoint.x == 10.0f,
          "(3) mvContactPoint copied (+0x20)");
    Check(gInput.mInfo.mbHardstopVsWall && !gInput.mInfo.mbHardStopVsAI, "(3) owner WORLD (0) -> mbHardstopVsWall only");

    // (4) owner TRAFFIC_VEHICLE (2) and (5) owner RACECAR (1) -> hard stop vs AI.
    Reset(false, -1);
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_PLAYER_ENTITY, BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE, 40.0f, 0.25f, 1.0f));
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mbHardStopVsAI && !gInput.mInfo.mbHardstopVsWall, "(4) owner TRAFFIC_VEHICLE (2) -> mbHardStopVsAI");
    Reset(false, -1);
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_PLAYER_ENTITY, BrnWorld::E_ENTITYTYPE_RACECAR, 40.0f, 0.25f, 1.0f));
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mbHardStopVsAI && !gInput.mInfo.mbHardstopVsWall, "(5) owner RACECAR (1) -> mbHardStopVsAI");

    // (6) owner PROP (3): the vectors/speed are copied, neither flag is set.
    Reset(false, -1);
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_PLAYER_ENTITY, BrnWorld::E_ENTITYTYPE_PROP, 22.0f, 0.75f, 9.0f));
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mfSpeedMPH == 22.0f && !gInput.mInfo.mbHardstopVsWall && !gInput.mInfo.mbHardStopVsAI,
          "(6) owner PROP (3): copied, no hard-stop flag");

    // (7) another car's crash is ignored.
    Reset(false, -1);
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_OTHER_ENTITY, BrnWorld::E_ENTITYTYPE_WORLD, 99.0f, 1.0f, 5.0f));
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mfSpeedMPH == 0.0f && !gInput.mInfo.mbHardstopVsWall, "(7) another car's crash is not the player's");

    // (8) the match is on the HIGH dword of the 64-bit volume id (the entity word).
    Reset(false, -1);
    {
        RaceCarCrashEvent lLow = MakeCrash(KU_OTHER_ENTITY, BrnWorld::E_ENTITYTYPE_WORLD, 55.0f, 1.0f, 5.0f);
        lLow.mRaceCarVolumeInstanceID.muId = (static_cast<u64>(KU_OTHER_ENTITY) << 32) | KU_PLAYER_ENTITY;
        gWorld.mVehicleManager.mQueue.AddEvent(lLow);
    }
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mfSpeedMPH == 0.0f, "(8) the low dword never matches (entity word is muId >> 32)");

    // (9) two matches: the vectors/speed of the LAST one, both flags accumulated.
    Reset(false, -1);
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_PLAYER_ENTITY, BrnWorld::E_ENTITYTYPE_WORLD, 30.0f, -1.0f, 1.0f));
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_OTHER_ENTITY,  BrnWorld::E_ENTITYTYPE_WORLD, 77.0f, 7.0f, 7.0f));
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_PLAYER_ENTITY, BrnWorld::E_ENTITYTYPE_RACECAR, 45.0f, 0.5f, 2.0f));
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mfSpeedMPH == 45.0f && gInput.mInfo.mvCollisionNormal.x == 0.5f
          && gInput.mInfo.mvContactPoint.z == 2.0f, "(9) the LAST matching event's speed / normal / point win");
    Check(gInput.mInfo.mbHardstopVsWall && gInput.mInfo.mbHardStopVsAI, "(9) the flags accumulate across events");

    // (10) the entity is taken from the world output's player index (the r20 argument), not the
    //      interface's own player index.
    Reset(false, -1);
    gCars.mePlayer = E_ACTIVE_RACE_CAR_INDEX_1;   // the interface disagrees; the lookup must still use slot 3
    gWorld.mVehicleManager.mQueue.AddEvent(MakeCrash(KU_PLAYER_ENTITY, BrnWorld::E_ENTITYTYPE_WORLD, 12.0f, 1.0f, 1.0f));
    Step13(&gCars, &gWorld, E_ACTIVE_RACE_CAR_INDEX_3, &gInput);
    Check(gInput.mInfo.mfSpeedMPH == 12.0f, "(10) GetRaceCarState(lePlayerIndex) -- the world output's player index");

    std::printf("FxBridgesCrashInfo: %u checks, %u failures (%u asserts fired)\n", gChecks, gFailures, gAsserts);
    return gFailures == 0 ? 0 : 1;
}
