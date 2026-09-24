// FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): TrafficEntityModule::AddVehicleToPhysics @0x827425B0,
// the create-queue-full leg 0x82742674..0x8274269C / 0x82742894..0x827428C0.
// The PRODUCTION body, extracted from src/GameSource/World/EntityModules/TrafficEntityModule/
// BrnTrafficEntityModule.cpp by run_fxtraffic5_create_queue.py with the accessors it reaches
// (GetVehicle, GetVehicleTransform). The VehicleInputInterface is replaced by a recording double that
// exposes the same two reads the console inlines (miLength +0x20778, miMaxLength +0x20774 of
// mCreateTrafficEventQueue +0x20770) and counts CreatePhysicalTraffic posts; CalculateInitialPhysicalState,
// the data pointer, the physics specs and GetCarAssetAttribKey are plain doubles (not under test).
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   0x82742684 cmpw length, max ; blt -> room: post (0x827427C0) and SetBit(lpCreatedBodies) (0x82742888)
//   no room (length >= max): 0x82742894 log (filter bit 0) and RETURN -- no post, no SetBit
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

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
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

// ---- the doubles --------------------------------------------------------------------------------
struct FakeCreateQueue
{
    s32 miLength;
    s32 miMaxLength;
    s32 GetLength() const    { return miLength; }
    s32 GetMaxLength() const { return miMaxLength; }
};

struct FakeVehicleInput
{
    FakeCreateQueue mCreateQueue;
    int             miPosts;
    u64             muLastVolumeInstanceId;
    int             miLastTrafficType;
    const FakeCreateQueue* GetCreateTrafficBodyEvents() const { return &mCreateQueue; }

    template <class A, class B, class C, class D, class E, class F, class G, class H, class I, class J>
    void CreatePhysicalTraffic(A lId, B, C, D, E, F, G, H leType, I, J)
    {
        ++miPosts;
        muLastVolumeInstanceId = lId.muId;
        miLastTrafficType = static_cast<int>(leType);
        ++mCreateQueue.miLength;       // the real AddEvent appends unconditionally
    }
};

struct FakeVehicleTypeData  { u8 muAssetId; };
struct FakeVehicleAssetData { CgsID mId; CgsID GetVehicleId() const { return mId; } };
struct FakeTrafficData      { FakeVehicleTypeData* mpaVehicleTypes; FakeVehicleAssetData* mpaVehicleAssets; };
struct FakeDataPtr          { FakeTrafficData* mp; FakeTrafficData* operator->() const { return mp; } };
struct FakeSpec
{
    CgsResource::ResourceHandle mHandle;
    CgsResource::ResourceHandle GetResourceHandle() const { return mHandle; }
};

namespace BrnTraffic
{
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }   // the witness stream stays off

    struct CreateFixture
    {
        typedef TrafficEntityModule M;
        typedef M::TotalTrafficBitArray TotalTrafficBitArray;

        decltype(M::maVehicles)          maVehicles;
        decltype(M::maVehicleTransforms) maVehicleTransforms;
        FakeDataPtr                      mpData;
        FakeSpec                         maTrafficVehiclePhysicsSpecs[KU_MAX_VEHICLE_ASSETS];

        Vehicle*       GetVehicle(u32 luIndex);
        Matrix44Affine GetVehicleTransform(u32 luIndex) const;

        void CalculateInitialPhysicalState(const Vehicle*, Matrix44Affine lVehicleTransform, Vector3& lOutInitialVelocity,
                                           Vector3& lOutAngularVelocity, u8* lpuOutAttribsId,
                                           Matrix44Affine& lOutTransform) const
        {
            std::memset(&lOutInitialVelocity, 0, sizeof(lOutInitialVelocity));
            std::memset(&lOutAngularVelocity, 0, sizeof(lOutAngularVelocity));
            *lpuOutAttribsId = 0;
            lOutTransform = lVehicleTransform;
        }
        VehicleTypeRuntime::AttribKey GetCarAssetAttribKey(u32) const { return VehicleTypeRuntime::AttribKey(); }

        void AddVehicleToPhysics(u32 luVehicle, EntityId lTargetEntityId, FakeVehicleInput* lpVehicleInput,
                                 BrnPhysics::Vehicle::ETrafficType leTrafficType, TotalTrafficBitArray* lpMadePhysical);
    };
}

// The production bodies under test.
#include "create_queue.inc"

using namespace BrnTraffic;
typedef CreateFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
    else
    {
        std::printf("ok: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];
static Fixture& F() { return *reinterpret_cast<Fixture*>(gaFixture); }
static FakeVehicleTypeData  gaTypes[4];
static FakeVehicleAssetData gaAssets[4];
static FakeTrafficData      gData = { gaTypes, gaAssets };
static FakeVehicleInput     gInput;
static Fixture::TotalTrafficBitArray gCreated;

static void Fresh(s32 liLength, s32 liMax)
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    std::memset(&gInput, 0, sizeof(gInput));
    F().mpData.mp = &gData;
    gInput.mCreateQueue.miLength = liLength;
    gInput.mCreateQueue.miMaxLength = liMax;
    gCreated.UnSetAll();
    Vehicle& lrVehicle = F().maVehicles[123];
    lrVehicle.muVehicleType = 1;
    lrVehicle.mxFlags = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_HASENTITY | Vehicle::E_FLAG_COLLIDABLE;
    gaTypes[1].muAssetId = 2;
    gaAssets[2].mId = 0xABCDu;
}

static void Post()
{
    EntityId lTarget;
    lTarget.muValue = 0x02000000u | (7u << 10);        // another traffic car
    F().AddVehicleToPhysics(123, lTarget, &gInput, BrnPhysics::Vehicle::E_TRAFFIC_TYPE_POTENTIAL, &gCreated);
}

int main()
{
    Fresh(0, 25);
    Post();
    Check(gInput.miPosts == 1 && gInput.mCreateQueue.miLength == 1, "R1 an empty queue: the create event is posted");
    Check(gCreated.IsBitSet(123), "R2 ...and the car is marked in lpCreatedBodies (0x82742888)");
    Check(((gInput.muLastVolumeInstanceId >> 42) & 0x3FFFu) == 123u && (gInput.muLastVolumeInstanceId >> 56) == 2u,
          "R3 the posted id is traffic owner 2, entity index 123");

    Fresh(24, 25);
    Post();
    Check(gInput.miPosts == 1 && gCreated.IsBitSet(123), "R4 the 25th slot (24 < 25, blt) is still used");

    Fresh(25, 25);
    Post();
    Check(gInput.miPosts == 0 && gInput.mCreateQueue.miLength == 25,
          "F1 0x82742894: a FULL queue (25 of 25) posts NOTHING -- no write past the 25-slot buffer");
    Check(!gCreated.IsBitSet(123), "F2 ...and does NOT mark lpCreatedBodies (the RETURN precedes 0x82742888)");

    Fresh(26, 25);
    Post();
    Check(gInput.miPosts == 0 && !gCreated.IsBitSet(123), "F3 length > max: `cmpw ; blt` treats it as full too");

    CgsDev::Message::gxMessageFilterFlags = 1;          // the log leg with no stream attached
    Fresh(25, 25);
    Post();
    Check(gInput.miPosts == 0, "F4 the message-filter log leg does not post either");
    CgsDev::Message::gxMessageFilterFlags = 0;

    Check(gAsserts == 0, "no assert fired");
    std::printf("FxTraffic5CreateQueue: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
