// crash parity FX-NETCRASH (2026-09-25): WorldModule::BridgeEntityModulesToOutput_PostPhysics leg 13 hands the
// traffic module's per-frame SOUND and DIRECTOR records to the world update-output buffer, straight after the
// network snapshot, as the console does:
//   0x827AF0D4..0x827AF0E4  out->SetTrafficNetworkOutputInterface(trafficOut->GetNetworkInterface())
//   0x827AF0F8..0x827AF108  out->SetTrafficSoundOutputInterface(trafficOut->GetTrafficSoundOutputInterface())
//   0x827AF11C..0x827AF12C  out->SetTrafficDirectorOutputInterface(trafficOut->GetTrafficDirectorOutputInterface())
//
// The PRODUCTION leg-13 statements (from the network snapshot to the end of the function) are compiled here, with
// the production body of TrafficDirectorOutputInterface::GetTrafficDirectorEntityArray() const when the revision
// has one. The traffic side is a fake output buffer holding the REAL record types; the world side is a fake whose
// three setters log their order and copy like the real ones (UpdateOutputBuffer::SetTrafficSoundOutputInterface
// @0x827A4A78 = TrafficSoundOutputInterface::operator= @0x823A7F18, linked from BrnTrafficSoundInterfaces.cpp;
// SetTrafficDirectorOutputInterface @0x827A8AB0 = the u16 head + the Array<TrafficDirectorEntity,32> copy).
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::printf("ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnTraffic
{
namespace BrnTrafficIO
{
#include "director_accessor.inc"
}
}

using BrnTraffic::BrnTrafficIO::TrafficSoundEntity;
using BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface;
using BrnTraffic::BrnTrafficIO::TrafficDirectorEntity;
using BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface;

namespace
{
    struct FakeNetwork { int miUnused; };

    // The traffic module's OutputBuffer_PostPhysics, reduced to the three read-locked getters leg 13 calls
    // (0x827A08D8 +0xDA0 / 0x827A0980 +0xE30 / 0x827A0A28 +0x1840); each counts its calls.
    struct FakeTrafficOut
    {
        FakeNetwork                    mNetwork;
        TrafficSoundOutputInterface    mSound;
        TrafficDirectorOutputInterface mDirector;
        mutable unsigned               muSoundGets;
        mutable unsigned               muDirectorGets;

        const FakeNetwork* GetNetworkInterface() const { return &mNetwork; }
        const TrafficSoundOutputInterface* GetTrafficSoundOutputInterface() const { ++muSoundGets; return &mSound; }
        const TrafficDirectorOutputInterface* GetTrafficDirectorOutputInterface() const
        {
            ++muDirectorGets;
            return &mDirector;
        }
    };

    // BrnWorldIO::UpdateOutputBuffer, reduced to leg 13's three setters. Each logs its letter in call order and
    // copies the way the production setter does.
    struct FakeWorldOut
    {
        char                                  macOrder[8];
        unsigned                              muCalls;
        const FakeNetwork*                    mpNetworkFrom;
        const TrafficSoundOutputInterface*    mpSoundFrom;
        const TrafficDirectorOutputInterface* mpDirectorFrom;
        TrafficSoundOutputInterface           mSound;
        TrafficDirectorOutputInterface        mDirector;

        void Log(char lcWhich)
        {
            if (muCalls < sizeof(macOrder) - 1)
            {
                macOrder[muCalls++] = lcWhich;
            }
        }
        void SetTrafficNetworkOutputInterface(const FakeNetwork* lpInterface)
        {
            Log('N');
            mpNetworkFrom = lpInterface;
        }
        void SetTrafficSoundOutputInterface(const TrafficSoundOutputInterface* lpInterface)
        {
            Log('S');
            mpSoundFrom = lpInterface;
            mSound = *lpInterface;
        }
        void SetTrafficDirectorOutputInterface(const TrafficDirectorOutputInterface* lpInterface)
        {
            Log('D');
            mpDirectorFrom = lpInterface;
            mDirector = *lpInterface;
        }
    };

    void RunLeg13(FakeWorldOut* lpOutputBuffer, const FakeTrafficOut* lpTrafficOutput_PostPhysics)
    {
#include "leg13.inc"
    }
}

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

static TrafficSoundEntity SoundEntity(u16 lu16Index, u8 lu8Class, bool lbCrashed, bool lbPhysical, f32 lfSpeed)
{
    TrafficSoundEntity lEntity;
    std::memset(&lEntity, 0, sizeof(lEntity));
    lEntity.mLocalTransform.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lEntity.mLocalTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lEntity.mLocalTransform.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    lEntity.mLocalTransform.wAxis = Vector3{ 3390.0f + lu16Index, 0.25f, -1641.0f - lu16Index, 1.0f };
    lEntity.mEntityId.muValue = 0x02000000u | (static_cast<u32>(lu16Index) << 10);
    lEntity.mfSpeed = lfSpeed;
    lEntity.mu16EntityIndex = lu16Index;
    lEntity.muVehicleClass = lu8Class;
    lEntity.mbIsEngineOn = !lbCrashed;
    lEntity.mbIsHooting = false;
    lEntity.mbIsCrashed = lbCrashed;
    lEntity.mbIsPhysical = lbPhysical;
    lEntity.muAlarmType = TrafficSoundEntity::E_ALARM_NONE;
    return lEntity;
}

static TrafficDirectorEntity DirectorEntity(u16 lu16Index, f32 lfSpeed)
{
    TrafficDirectorEntity lEntity;
    std::memset(&lEntity, 0, sizeof(lEntity));
    lEntity.mLocalTransform.wAxis = Vector3{ 3390.0f + lu16Index, 1.0f, -1641.0f - lu16Index, 1.0f };
    lEntity.mVelocity = Vector3{ 0.0f, 0.0f, -lfSpeed, 0.0f };
    lEntity.mHalfExtents = Vector3{ 0.99f, 0.74f, 2.70f, 0.0f };
    lEntity.mVehicleId = static_cast<CgsID>(0x1000u + lu16Index);
    lEntity.mu16EntityIndex = lu16Index;
    return lEntity;
}

static bool SameBytes(const void* lpA, const void* lpB, size_t luBytes) { return std::memcmp(lpA, lpB, luBytes) == 0; }

int main()
{
    static FakeTrafficOut lTraffic;
    static FakeWorldOut   lWorld;
    std::memset(&lTraffic, 0, sizeof(lTraffic));
    std::memset(&lWorld, 0, sizeof(lWorld));

    // ---- frame 1: three nearby cars for the sound, two for the director ---------------------------------
    // The world copy starts with a stale previous frame (5 sound records, 4 director records).
    for (u16 lu16Stale = 0; lu16Stale < 5; ++lu16Stale)
    {
        lWorld.mSound.AddTrafficEntity(SoundEntity(static_cast<u16>(400 + lu16Stale), 1, false, false, 9.0f));
    }
    lWorld.mDirector.GetTrafficDirectorEntityArray().Construct();   // (the interface's own Construct has no body)
    for (u16 lu16Stale = 0; lu16Stale < 4; ++lu16Stale)
    {
        lWorld.mDirector.GetTrafficDirectorEntityArray().Append(DirectorEntity(static_cast<u16>(400 + lu16Stale), 9.0f));
    }

    lTraffic.mSound.mu16EntityCount = 0;
    lTraffic.mSound.AddTrafficEntity(SoundEntity(17, 2, true, true, 12.5f));    // a crashed, physical car
    lTraffic.mSound.AddTrafficEntity(SoundEntity(311, 0, false, false, 15.8f));
    lTraffic.mSound.AddTrafficEntity(SoundEntity(599, 3, false, false, 0.0f));  // the last global index
    lTraffic.mDirector.mu16EntityCount = 0xBEEF;   // the head Construct never stores; the console copies it anyway
    lTraffic.mDirector.GetTrafficDirectorEntityArray().Construct();
    lTraffic.mDirector.GetTrafficDirectorEntityArray().Append(DirectorEntity(17, 12.5f));
    lTraffic.mDirector.GetTrafficDirectorEntityArray().Append(DirectorEntity(311, 15.8f));

    RunLeg13(&lWorld, &lTraffic);

    Check(lWorld.muCalls == 3 && std::strcmp(lWorld.macOrder, "NSD") == 0,
          "leg 13 order: network snapshot, then sound, then director (0x827AF0E4 / 0x827AF108 / 0x827AF12C)");
    Check(lWorld.mpSoundFrom == &lTraffic.mSound && lTraffic.muSoundGets >= 1,
          "the sound setter takes the traffic module's own sound records (GetTrafficSoundOutputInterface const "
          "@0x827A0980, +0xE30)");
    Check(lWorld.mpDirectorFrom == &lTraffic.mDirector && lTraffic.muDirectorGets >= 1,
          "the director setter takes the traffic module's own director records (GetTrafficDirectorOutputInterface "
          "const @0x827A0A28, +0x1840)");

    bool lbSound = lWorld.mSound.mu16EntityCount == 3;
    for (u16 lu16Entity = 0; lbSound && lu16Entity < 3; ++lu16Entity)
    {
        lbSound = SameBytes(&lWorld.mSound.maActiveEntityList[lu16Entity],
                            &lTraffic.mSound.maActiveEntityList[lu16Entity], sizeof(TrafficSoundEntity));
    }
    Check(lbSound, "world sound copy: the 3 records replace the stale 5, field for field (operator= @0x823A7F18)");

    const Array<TrafficDirectorEntity, 32u>& lrWorldDirector = lWorld.mDirector.GetTrafficDirectorEntityArray();
    bool lbDirector = lrWorldDirector.GetLength() == 2 && lWorld.mDirector.mu16EntityCount == 0xBEEF;
    for (u32 luEntity = 0; lbDirector && luEntity < 2; ++luEntity)
    {
        lbDirector = SameBytes(&lrWorldDirector[luEntity],
                               &lTraffic.mDirector.GetTrafficDirectorEntityArray()[luEntity],
                               sizeof(TrafficDirectorEntity));
    }
    Check(lbDirector, "world director copy: the u16 head (0x827A8B50 lhz/sth) and the 2 records replace the stale 4 "
                      "(Array copy 0x823B2368)");

    // What the sound consumer then does with it: CollisionStateManager::FindEntity @0x826A0398 and
    // MapEntityIdToMaterial @0x826A0CF8 look a traffic car up by entity index (GetTrafficEntityIndex @0x82681EC8).
    const TrafficSoundEntity* lpFound = lWorld.mSound.GetTrafficEntityIndex(17);
    Check(lpFound != nullptr && lpFound->mbIsCrashed && lpFound->mbIsPhysical && lpFound->muVehicleClass == 2
              && lpFound->mfSpeed == 12.5f,
          "the crashed car is found by its entity index in the world copy, crashed / physical / class / speed intact");
    Check(lWorld.mSound.GetTrafficEntityIndex(403) == nullptr && lWorld.mSound.GetTrafficEntityIndex(18) == nullptr,
          "a car not in this frame's records (a stale one, or one the query missed) is not found");

    // ---- frame 2: no nearby traffic ----------------------------------------------------------------------
    lTraffic.mSound.mu16EntityCount = 0;
    lTraffic.mDirector.GetTrafficDirectorEntityArray().Construct();
    lWorld.muCalls = 0;
    std::memset(lWorld.macOrder, 0, sizeof(lWorld.macOrder));

    RunLeg13(&lWorld, &lTraffic);

    Check(lWorld.mSound.mu16EntityCount == 0 && lWorld.mDirector.GetTrafficDirectorEntityArray().GetLength() == 0
              && std::strcmp(lWorld.macOrder, "NSD") == 0,
          "an empty frame empties the world copies too: the leg copies every frame, whatever it carries");

    Check(gAsserts == 0, "no assert fired");
    std::printf("FxNetcrashTrafficBridges: %u checks, %u failures (%u asserts)\n", gChecks, gFailures, gAsserts);
    return gFailures ? 1 : 0;
}
