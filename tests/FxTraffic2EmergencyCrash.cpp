// FX-TRAFFIC2 (crash parity 2026-09-24, G59-D2): the PRODUCTION
//   TrafficEntityModule::SendEmergencyCrashEvents                         @0x82747BB8
//   Vehicle::IsCrashing / Vehicle::IsRecoveringFromSlam                   @0x82704A70 / @0x82704C90
//   EntityIndexOf (the TU's 14/10 entity-id unpack, `extrwi 14,8` at 0x82747CF4)
// extracted from the b5 sources by run_fxtraffic2_emergency_crash.py and hosted on a fixture that
// has the module's real member types; the callees the body hands work to (GetVehicle,
// RecordTrafficVehicleIsPhysical, MakeVehiclePhysical, SetTrafficCrashing) are recorders here.
// Every expected value below is derived from the ARTIST asm (the decode is in the body's banner):
//   per queued TrafficCrashInfo: already in lpCreatedBodies -> nothing (GetVehicle not reached);
//   dead or IsCrashing -> nothing; physical: IsRecoveringFromSlam ->
//   RecordTrafficVehicleIsPhysical(v, victim, causer, 0 Standard, 0.0f, 0.0f) (flt_82001CC0),
//   then SetTrafficCrashing(victim) whether recovering or not; non-physical ->
//   MakeVehiclePhysical(v, out, bodies, causer, 1 CRASHING, 0 Standard); the queue is cleared.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static const char* gpcLastAssert = "";

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        gpcLastAssert = lpcMessage;
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

// ---- the recorders -------------------------------------------------------------------------
struct RecordCall { u32 muVehicle, muVictim, muCauser; s32 miCrashType; f32 mfA, mfB; };
struct MakeCall   { u32 muVehicle; const void* mpOutput; const void* mpBodies; u32 muCauser; s32 miTrafficType, miCrashType; };
static std::vector<RecordCall> gRecords;
static std::vector<MakeCall>   gMakes;
static std::vector<u32>        gCrashing;        // SetTrafficCrashing ids, in order
static std::vector<u32>        gVehicleFetches;  // GetVehicle indices, in order
static std::string             gSequence;        // R = record, S = SetTrafficCrashing, M = make

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleInputInterface::SetTrafficCrashing(EntityId lEntityId)
    {
        gCrashing.push_back(lEntityId.muValue);
        gSequence += 'S';
    }
}
}

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // The write-locked accessor (0x82711460) without the lock bookkeeping.
    OutputBuffer_PrePhysics::VehicleInputInterface* OutputBuffer_PrePhysics::GetVehicleInputInterface()
    {
        return &mVehicleInputInterface;
    }
}

    struct EmergencyFixture
    {
        typedef TrafficEntityModule M;
        typedef M::TotalTrafficBitArray TotalTrafficBitArray;

        decltype(M::maEmergencyCrashingVehicles) maEmergencyCrashingVehicles;
        decltype(M::maVehicles)                  maVehicles;

        Vehicle* GetVehicle(u32 luIndex)
        {
            gVehicleFetches.push_back(luIndex);
            return &maVehicles[luIndex];
        }
        void RecordTrafficVehicleIsPhysical(u32 luVehicle, EntityId lEntityId, EntityId lTargetEntityId,
                                            BrnPhysics::Vehicle::eCrashTrafficType leCrashType,
                                            f32 lfArg4, f32 lfArg5)
        {
            gRecords.push_back(RecordCall{ luVehicle, lEntityId.muValue, lTargetEntityId.muValue,
                                           static_cast<s32>(leCrashType), lfArg4, lfArg5 });
            gSequence += 'R';
        }
        void MakeVehiclePhysical(u32 luVehicle, BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                 TotalTrafficBitArray* lpMadePhysical, EntityId lTargetEntityId,
                                 BrnPhysics::Vehicle::ETrafficType leTrafficType,
                                 BrnPhysics::Vehicle::eCrashTrafficType leCrashType)
        {
            gMakes.push_back(MakeCall{ luVehicle, lpOutput, lpMadePhysical, lTargetEntityId.muValue,
                                       static_cast<s32>(leTrafficType), static_cast<s32>(leCrashType) });
            gSequence += 'M';
        }

        void SendEmergencyCrashEvents(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                      TotalTrafficBitArray* lpCreatedBodies);
    };
}

// The production bodies under test.
#include "emergency_crash.inc"

using namespace BrnTraffic;
using namespace BrnTraffic::BrnTrafficIO;
typedef EmergencyFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];
alignas(64) static unsigned char gaOutput[sizeof(OutputBuffer_PrePhysics)];
static Fixture::TotalTrafficBitArray gBodies;

static Fixture& F()                  { return *reinterpret_cast<Fixture*>(gaFixture); }
static OutputBuffer_PrePhysics& Out() { return *reinterpret_cast<OutputBuffer_PrePhysics*>(gaOutput); }

static u32 TrafficId(u32 luVehicle) { return (luVehicle << 10) | 0x02000000u; }   // owner 2
static u32 RaceCarId(u32 luRaceCar) { return (luRaceCar << 10) | 0x01000000u; }   // owner 1

// Every slot dead, crash-traffic type 1 (not crashing, not recovering).
static void Fresh()
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    std::memset(gaOutput, 0, sizeof(gaOutput));
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        F().maVehicles[luVehicle].mxFlags            = 0;
        F().maVehicles[luVehicle].muCrashTrafficType = 1;
    }
    F().maEmergencyCrashingVehicles.Clear();
    gBodies.UnSetAll();
    gRecords.clear();
    gMakes.clear();
    gCrashing.clear();
    gVehicleFetches.clear();
    gSequence.clear();
    gAsserts = 0;
    gpcLastAssert = "";
}

static void Car(u32 luVehicle, bool lbAlive, bool lbPhysical, u8 luCrashTrafficType)
{
    Vehicle& lr = F().maVehicles[luVehicle];
    lr.mxFlags = static_cast<u8>((lbAlive ? Vehicle::E_FLAG_ALIVE : 0) | (lbPhysical ? Vehicle::E_FLAG_PHYSICAL : 0));
    lr.muCrashTrafficType = luCrashTrafficType;
}

static void Queue(u32 luVictim, u32 luCauserId)
{
    TrafficCrashInfo lInfo;
    std::memset(&lInfo, 0, sizeof(lInfo));
    lInfo.mVictimId.muValue = TrafficId(luVictim);
    lInfo.mCauserId.muValue = luCauserId;
    F().maEmergencyCrashingVehicles.Append(lInfo);
}

static void Run()
{
    F().SendEmergencyCrashEvents(&Out(), &gBodies);
}

int main()
{
    // A 11: physical, recovering from a slam (crash type 3)   -> Record + SetTrafficCrashing
    // B 22: alive, not physical                              -> MakeVehiclePhysical as CRASHING
    // C 33: physical, not recovering (crash type 2)          -> SetTrafficCrashing only
    // D 44: already promoted this frame (bit set)            -> nothing, GetVehicle not reached
    // E 55: dead                                             -> nothing
    // F 66: already crashing (alive, physical, crash type 0) -> nothing
    Fresh();
    Car(11, true, true, 3);
    Car(22, true, false, 1);
    Car(33, true, true, 2);
    Car(44, true, false, 1);
    Car(55, false, false, 1);
    Car(66, true, true, 0);
    gBodies.SetBit(44);
    Queue(11, TrafficId(12));   // the other half of a pair: causer = the half that crashed
    Queue(22, RaceCarId(0));
    Queue(33, TrafficId(34));
    Queue(44, TrafficId(45));
    Queue(55, TrafficId(56));
    Queue(66, TrafficId(67));
    Run();

    Check(gRecords.size() == 1 && gRecords[0].muVehicle == 11 && gRecords[0].muVictim == TrafficId(11)
          && gRecords[0].muCauser == TrafficId(12) && gRecords[0].miCrashType == 0
          && gRecords[0].mfA == 0.0f && gRecords[0].mfB == 0.0f,
          "A (physical, recovering from slam): RecordTrafficVehicleIsPhysical(11, victim, causer, Standard, 0, 0), once");
    Check(gCrashing.size() == 2 && gCrashing[0] == TrafficId(11) && gCrashing[1] == TrafficId(33),
          "SetTrafficCrashing(victim) for both physical cars, A then C, and for nobody else");
    Check(gMakes.size() == 1 && gMakes[0].muVehicle == 22 && gMakes[0].mpOutput == &Out()
          && gMakes[0].mpBodies == &gBodies && gMakes[0].muCauser == RaceCarId(0)
          && gMakes[0].miTrafficType == 1 && gMakes[0].miCrashType == 0,
          "B (not physical): MakeVehiclePhysical(22, out, bodies, causer, E_TRAFFIC_TYPE_CRASHING, Standard), once");
    Check(gSequence == "RSMS", "console order: A record then crash, B promote, C crash");
    Check(gVehicleFetches.size() == 5 && gVehicleFetches[3] == 55,
          "GetVehicle for every entry but D -- the lpCreatedBodies test comes first (0x82747E54)");
    Check(F().maEmergencyCrashingVehicles.GetLength() == 0, "the queue is cleared (stwx 0 -> +0x57CF0)");
    Check(gBodies.IsBitSet(44) && !gBodies.IsBitSet(22), "lpCreatedBodies is only read here (the recorder owns the SetBit)");
    Check(gAsserts == 0, "no assert fired (victim != causer, indices in range)");

    // ---- an empty queue: nothing called, still empty ------------------------------------------
    Fresh();
    Car(11, true, true, 3);
    Run();
    Check(gRecords.empty() && gMakes.empty() && gCrashing.empty() && gVehicleFetches.empty()
          && F().maEmergencyCrashingVehicles.GetLength() == 0 && gAsserts == 0,
          "an empty queue makes no call and stays empty");

    // ---- a physical car that is NOT recovering gets no record, only the crash ---------------
    Fresh();
    Car(7, true, true, 2);
    Queue(7, TrafficId(8));
    Run();
    Check(gRecords.empty() && gCrashing.size() == 1 && gCrashing[0] == TrafficId(7) && gMakes.empty(),
          "a physical, non-recovering car: SetTrafficCrashing only");

    std::printf("FxTraffic2EmergencyCrash: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
