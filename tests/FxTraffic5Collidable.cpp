// FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): TrafficEntityModule::UpdateCollidableVehicles @0x827302C8.
// The PRODUCTION body, extracted from src/GameSource/World/EntityModules/TrafficEntityModule/
// BrnTrafficEntityModule.cpp by run_fxtraffic5_collidable.py with the file-local helpers it uses
// (the lane splicers, the two .data constants, MakeTrafficVolumeInstanceId) and the accessors it
// reaches (GetVehicle, GetVehicleTransform, GetVehicleTypeRuntime, Vehicle::GetSpeed,
// Vehicle::SetCollidable, Camera::GetPosition). The input / output buffers are replaced by the
// runner with test doubles: an active-race-car table and a recording scene interface.
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   0x82731324 beq -> 0x82731A54  a car NOT in (selector & withEnt & alive) | (alive & physical) ...
//   0x82731B8C..0x82731BC8        ... reads mVehiclesAvoidableLastFrame (+0x72578): bit set ->
//   0x82731BCC li r25, 1          avoidable (cached; r15 collidable stays 0) and
//   0x82731D14 andc / stdx        the carry-over bit is CLEARED
//   0x827316F8 beq 0x82731D20     an evaluated car that is not avoidable leaves the bit alone
//   0x82731D20 lbz hidden         hidden traffic is never cached (after the carry-over is consumed)
//   0x82730CB0 ld +0x729D0 ; rlwinm 0,4,4 -- flag 27 (E_FLAG_ROAD_FOLLOWING_CAM) of mCameraLastFrame
//   0x82730CE0 Vector3_33::Append(+0x728C0) -- its Pos row becomes a SOURCE, after the average
//   0x82730C80..0x82730CAC vrefp + NR, no zero test: no sources -> mAveragePhysicalCentre is NaN
//   no ~alive & collidable sweep exists (KillDyingVehicleEntities @0x82741E40 owns that teardown)
//   0x82732454 VolumeId = type + 0x24; 0x82732360..0x82732424 row 3 = TransformPoint(xform, bbox
//   offset), rows 0..2 unchanged; 0x827324E4 AddForCollision(r5 2, r6 4, r7 2, v1 = At*speed*dt)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <vector>

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

// ---- the IO doubles the runner substitutes for the buffer types ---------------------------------
struct FakeRaceCarState
{
    Matrix44Affine mTransform;
    Vector3        mLinearVelocity;
    Vector3        mHalfExtent;
};

struct FakeRaceCars
{
    bool             mbActive[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    FakeRaceCarState maState[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    bool IsRaceCarActive(EActiveRaceCarIndex leCar) const { return mbActive[leCar]; }
    const FakeRaceCarState* GetRaceCarState(EActiveRaceCarIndex leCar) const { return &maState[leCar]; }
};

struct FakeInput
{
    FakeRaceCars mRaceCars;
    const FakeRaceCars* GetActiveRaceCarOutputInterface() const { return &mRaceCars; }
};

enum ESceneCall { E_ADD_VOLUME = 1, E_ADD_COLLISION, E_REMOVE_COLLISION, E_REMOVE_VOLUME };
struct SceneCall
{
    int            miKind;
    u64            muId;
    u64            muVolumeId;
    Matrix44Affine mTransform;
    int            miGroup, miBodyState, miCacheFlags;
    Vector3        mPadding;
};
static std::vector<SceneCall> gSceneCalls;

struct FakeScene
{
    void AddVolumeInstance(const CgsSceneManager::VolumeInstanceId& lrId, const CgsSceneManager::VolumeId& lrVolumeId,
                           const Matrix44Affine& lrTransform)
    {
        SceneCall lCall = {};
        lCall.miKind = E_ADD_VOLUME;
        lCall.muId = lrId.muId;
        lCall.muVolumeId = static_cast<u64>(lrVolumeId);
        lCall.mTransform = lrTransform;
        gSceneCalls.push_back(lCall);
    }
    template <class G, class S, class F>
    void AddForCollision(const CgsSceneManager::VolumeInstanceId& lrId, G leGroup, S leBodyState,
                         const Vector3& lrPadding, F leCacheFlags)
    {
        SceneCall lCall = {};
        lCall.miKind = E_ADD_COLLISION;
        lCall.muId = lrId.muId;
        lCall.miGroup = static_cast<int>(leGroup);
        lCall.miBodyState = static_cast<int>(leBodyState);
        lCall.miCacheFlags = static_cast<int>(leCacheFlags);
        lCall.mPadding = lrPadding;
        gSceneCalls.push_back(lCall);
    }
    void RemoveForCollision(const CgsSceneManager::VolumeInstanceId& lrId)
    {
        SceneCall lCall = {};
        lCall.miKind = E_REMOVE_COLLISION;
        lCall.muId = lrId.muId;
        gSceneCalls.push_back(lCall);
    }
    void RemoveVolumeInstance(const CgsSceneManager::VolumeInstanceId& lrId)
    {
        SceneCall lCall = {};
        lCall.miKind = E_REMOVE_VOLUME;
        lCall.muId = lrId.muId;
        gSceneCalls.push_back(lCall);
    }
};

struct FakeOutput
{
    FakeScene mScene;
    FakeScene* GetSceneInputInterface() { return &mScene; }
};

namespace BrnTraffic
{
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }   // the witness stream stays off

    struct CollidableFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mCachedCollidableList)         mCachedCollidableList;
        decltype(M::mAveragePhysicalCentre)        mAveragePhysicalCentre;
        decltype(M::mVehicleSoaData)               mVehicleSoaData;
        decltype(M::mVehiclesToUpdateCollidables)  mVehiclesToUpdateCollidables;
        decltype(M::mVehiclesAvoidableLastFrame)   mVehiclesAvoidableLastFrame;
        decltype(M::mbTrafficIsHidden)             mbTrafficIsHidden;
        decltype(M::mfSimTimeStepVec)              mfSimTimeStepVec;
        decltype(M::mCameraLastFrame)              mCameraLastFrame;
        decltype(M::maVehicles)                    maVehicles;
        decltype(M::maVehicleTransforms)           maVehicleTransforms;
        decltype(M::maVehicleTypeRuntime)          maVehicleTypeRuntime;

        Vehicle*                  GetVehicle(u32 luIndex);
        Matrix44Affine            GetVehicleTransform(u32 luIndex) const;
        const VehicleTypeRuntime* GetVehicleTypeRuntime(u32 luVehicleType) const;

        void UpdateCollidableVehicles(const FakeInput* lpInput, FakeOutput* lpOutput);
    };
}

// The production bodies under test.
#include "collidable.inc"

using namespace BrnTraffic;
typedef CollidableFixture Fixture;

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

static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1e-4f * (1.0f + std::fabs(lfB)); }

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];
static Fixture& F() { return *reinterpret_cast<Fixture*>(gaFixture); }
static FakeInput  gInput;
static FakeOutput gOutput;

static Matrix44Affine AtPosition(f32 lfX, f32 lfY, f32 lfZ)
{
    Matrix44Affine lMatrix;
    std::memset(&lMatrix, 0, sizeof(lMatrix));
    lMatrix.xAxis.x = 1.0f;
    lMatrix.yAxis.y = 1.0f;
    lMatrix.zAxis.z = 1.0f;
    lMatrix.wAxis.x = lfX;
    lMatrix.wAxis.y = lfY;
    lMatrix.wAxis.z = lfZ;
    lMatrix.wAxis.w = 1.0f;
    return lMatrix;
}

static void Fresh()
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    std::memset(&gInput, 0, sizeof(gInput));
    gSceneCalls.clear();
    gAsserts = 0;
    F().mCachedCollidableList.Clear();
    F().mfSimTimeStepVec.x = F().mfSimTimeStepVec.y = F().mfSimTimeStepVec.z = F().mfSimTimeStepVec.w = 0.5f;
    VehicleTypeRuntime& lrType = F().maVehicleTypeRuntime[0];
    lrType.mBBoxOffset.x = 0.0f;  lrType.mBBoxOffset.y = 0.5f;  lrType.mBBoxOffset.z = 0.25f;
    lrType.mBBoxHalfSize.x = 1.0f; lrType.mBBoxHalfSize.y = 0.75f; lrType.mBBoxHalfSize.z = 2.5f;
}

static void ActivateRaceCar(u32 luCar, f32 lfX, f32 lfY, f32 lfZ)
{
    gInput.mRaceCars.mbActive[luCar] = true;
    gInput.mRaceCars.maState[luCar].mTransform = AtPosition(lfX, lfY, lfZ);
    gInput.mRaceCars.maState[luCar].mHalfExtent.x = 1.0f;
    gInput.mRaceCars.maState[luCar].mHalfExtent.z = 2.0f;
}

// An alive, entity-owning, non-physical traffic car of type 0 at (x, 0, 0) moving at lfSpeed.
static void AddCar(u32 luVehicle, f32 lfX, f32 lfSpeed = 0.0f)
{
    Vehicle& lrVehicle = F().maVehicles[luVehicle];
    lrVehicle.muVehicleType = 0;
    lrVehicle.mxFlags = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_HASENTITY;
    lrVehicle.mSpeed_DistAcrossLane_SwerveAmount_W.x = lfSpeed;
    F().mVehicleSoaData.mAliveVehicles.SetBit(luVehicle);
    F().mVehicleSoaData.mVehiclesWithEntities.SetBit(luVehicle);
    F().maVehicleTransforms[luVehicle] = AtPosition(lfX, 0.0f, 0.0f);
}

static void Run() { F().UpdateCollidableVehicles(&gInput, &gOutput); }

// lane 1 of the one packet the frame appends (lane 0 is race car 0)
static f32 PacketLane1PosX()
{
    if (F().mCachedCollidableList.GetLength() < 1)
    {
        return -1.0f;
    }
    return F().mCachedCollidableList.GetItem(0).mPosition_X.y;
}

static int CountCalls(int liKind, u32 luVehicle)
{
    const u64 luId = MakeTrafficVolumeInstanceId(luVehicle).muId;
    int liCount = 0;
    for (const SceneCall& lrCall : gSceneCalls)
    {
        if (lrCall.miKind == liKind && lrCall.muId == luId)
        {
            ++liCount;
        }
    }
    return liCount;
}

static const SceneCall* FindCall(int liKind, u32 luVehicle)
{
    const u64 luId = MakeTrafficVolumeInstanceId(luVehicle).muId;
    for (const SceneCall& lrCall : gSceneCalls)
    {
        if (lrCall.miKind == liKind && lrCall.muId == luId)
        {
            return &lrCall;
        }
    }
    return nullptr;
}

int main()
{
    // ---- A: the avoidable carry-over for the half of the pool not evaluated this frame ---------
    {
        Fresh();
        ActivateRaceCar(0, 0.0f, 0.0f, 0.0f);
        AddCar(10, 30.0f);                                  // 900 < 2500 avoidable, > 400 not collidable
        F().mVehiclesToUpdateCollidables.SetBit(10);        // frame 1: vehicle 10's half is evaluated

        Run();                                              // F1
        Check(Near(PacketLane1PosX(), 30.0f), "A1 an evaluated avoidable car is cached (lane 1 = x 30)");
        Check(F().mVehiclesAvoidableLastFrame.IsBitSet(10), "A1 ...and remembered in mVehiclesAvoidableLastFrame");
        Check(!F().mVehiclesToUpdateCollidables.IsBitSet(10), "A1 the selector flips (0x82732988): next frame is the other half");

        gSceneCalls.clear();
        Run();                                              // F2: vehicle 10 is NOT a candidate
        Check(Near(PacketLane1PosX(), 30.0f),
              "A2 0x82731A54: a car of the other half that was avoidable is STILL cached (lane 1 = x 30)");
        Check(!F().mVehiclesAvoidableLastFrame.IsBitSet(10),
              "A2 0x82731D14 andc: the carry-over bit is consumed");
        Check(gSceneCalls.empty(), "A2 a carried-over car is never made collidable (no scene call)");

        F().maVehicleTransforms[10] = AtPosition(100.0f, 0.0f, 0.0f);
        Run();                                              // F3: evaluated again, now 100 m away
        Check(PacketLane1PosX() == FLT_MAX, "A3 an evaluated car out of range is not cached (lane 1 = FLT_MAX pad)");
        Check(!F().mVehiclesAvoidableLastFrame.IsBitSet(10),
              "A3 0x827316F8: an evaluated non-avoidable car leaves the (clear) bit clear");

        Run();                                              // F4: other half, bit clear
        Check(PacketLane1PosX() == FLT_MAX, "A4 no carry-over without the bit (lane 1 = FLT_MAX pad)");
    }

    // ---- B: the road-following camera is a collision source (0x82730CB0) ---------------------------
    {
        Fresh();
        ActivateRaceCar(0, 1000.0f, 0.0f, 0.0f);
        AddCar(20, 5.0f, 12.0f);                            // 995 m from the race car, 5 m from the camera
        F().mVehiclesToUpdateCollidables.SetBit(20);
        F().mCameraLastFrame.mTransform = AtPosition(0.0f, 0.0f, 0.0f);
        F().mCameraLastFrame.GetState().mCurrentFlags.SetBit(
            BrnDirector::Camera::CameraState::E_FLAG_ROAD_FOLLOWING_CAM);

        Run();
        Check(CountCalls(E_ADD_VOLUME, 20) == 1,
              "B1 E_FLAG_ROAD_FOLLOWING_CAM: the camera's Pos row is a source, the car 5 m from it gets a volume");
        Check(F().mVehicleSoaData.mCollidableVehicles.IsBitSet(20) && F().maVehicles[20].IsCollidable(),
              "B2 ...and is marked collidable (SoA bit + flag)");
        Check(Near(F().mAveragePhysicalCentre.x, 1000.0f) && Near(F().mAveragePhysicalCentre.y, 0.0f),
              "B3 the camera is appended AFTER the average: mAveragePhysicalCentre = the race car only");

        const SceneCall* lpAdd = FindCall(E_ADD_VOLUME, 20);
        Check(lpAdd != nullptr && lpAdd->muVolumeId == 0x24u, "B4 0x82732454: VolumeId = type (0) + 0x24");
        Check(lpAdd != nullptr && Near(lpAdd->mTransform.wAxis.x, 5.0f) && Near(lpAdd->mTransform.wAxis.y, 0.5f)
                  && Near(lpAdd->mTransform.wAxis.z, 0.25f) && Near(lpAdd->mTransform.zAxis.z, 1.0f),
              "B5 row 3 = TransformPoint(xform, bbox offset), rotation rows unchanged");
        const SceneCall* lpCollision = FindCall(E_ADD_COLLISION, 20);
        Check(lpCollision != nullptr && lpCollision->miGroup == 2 && lpCollision->miBodyState == 4
                  && lpCollision->miCacheFlags == 2,
              "B6 0x827324E4: AddForCollision(culling group 2, ACTIVE_BODY 4, E_DO_NOT_ADD_TO_CACHE_MANAGER 2)");
        Check(lpCollision != nullptr && Near(lpCollision->mPadding.z, 12.0f * 0.5f) && Near(lpCollision->mPadding.x, 0.0f),
              "B7 0x8273248C..0x827324BC: padding = At * (speed * mfSimTimeStepVec)");

        Fresh();
        ActivateRaceCar(0, 1000.0f, 0.0f, 0.0f);
        AddCar(20, 5.0f, 12.0f);
        F().mVehiclesToUpdateCollidables.SetBit(20);
        F().mCameraLastFrame.mTransform = AtPosition(0.0f, 0.0f, 0.0f);
        F().mCameraLastFrame.GetState().mCurrentFlags.SetBit(
            BrnDirector::Camera::CameraState::E_FLAG_RACING_GAMEPLAY_CAMERA);
        Run();
        Check(CountCalls(E_ADD_VOLUME, 20) == 0 && !F().mVehicleSoaData.mCollidableVehicles.IsBitSet(20),
              "B8 any other camera flag: the camera is NOT a source");
    }

    // ---- C: no source at all -> the console's NaN centre (no zero test at 0x82730C80) -------------
    {
        Fresh();
        AddCar(30, 5.0f);
        F().mVehiclesToUpdateCollidables.SetBit(30);
        Run();
        Check(std::isnan(F().mAveragePhysicalCentre.x) && std::isnan(F().mAveragePhysicalCentre.z),
              "C1 no active race car and no physical car: mAveragePhysicalCentre is NaN (vrefp(0) = inf, 0 * inf)");
        Check(CountCalls(E_ADD_VOLUME, 30) == 0, "C2 with no source the nearest distance stays FLT_MAX: nothing solid");
    }

    // ---- D: no ~alive & collidable sweep (KillDyingVehicleEntities owns that teardown) -----------
    {
        Fresh();
        ActivateRaceCar(0, 0.0f, 0.0f, 0.0f);
        Vehicle& lrDead = F().maVehicles[40];
        lrDead.mxFlags = Vehicle::E_FLAG_COLLIDABLE;       // dead, still registered for collision
        F().mVehicleSoaData.mCollidableVehicles.SetBit(40);
        F().maVehicleTransforms[40] = AtPosition(3.0f, 0.0f, 0.0f);
        Run();
        Check(CountCalls(E_REMOVE_COLLISION, 40) == 0 && CountCalls(E_REMOVE_VOLUME, 40) == 0,
              "D1 a dead collidable car is not touched here (no Remove* call)");
        Check(F().mVehicleSoaData.mCollidableVehicles.IsBitSet(40) && lrDead.IsCollidable(),
              "D2 ...and keeps its collidable bit and flag");
    }

    // ---- E: hidden traffic consumes the carry-over but is not cached (0x82731D14 before 0x82731D20)
    {
        Fresh();
        ActivateRaceCar(0, 0.0f, 0.0f, 0.0f);
        AddCar(50, 30.0f);
        F().mVehiclesToUpdateCollidables.SetBit(50);
        Run();                                              // evaluated, avoidable -> bit set
        F().mbTrafficIsHidden = true;
        Run();                                              // other half, hidden
        Check(PacketLane1PosX() == FLT_MAX, "E1 hidden traffic is never cached");
        Check(!F().mVehiclesAvoidableLastFrame.IsBitSet(50), "E2 ...but the carry-over bit is still consumed");
    }

    // ---- F: a physical car is always evaluated: both avoidable and collidable, bit set ----------
    {
        Fresh();
        ActivateRaceCar(0, 0.0f, 0.0f, 0.0f);
        AddCar(60, 300.0f);                                 // far from everything
        F().maVehicles[60].mxFlags |= Vehicle::E_FLAG_PHYSICAL;
        F().mVehicleSoaData.mPhysicalVehicles.SetBit(60);
        Run();                                              // selector bit 60 clear: physical makes it a candidate
        Check(CountCalls(E_ADD_VOLUME, 60) == 1 && F().mVehiclesAvoidableLastFrame.IsBitSet(60),
              "F1 0x827318F4: a physical car is collidable and avoidable with no distance test");
        Check(Near(F().mAveragePhysicalCentre.x, 150.0f), "F2 physical cars are sources: centre = (0 + 300) / 2");
    }

    Check(gAsserts == 0, "no assert fired in any scenario");
    std::printf("FxTraffic5Collidable: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
