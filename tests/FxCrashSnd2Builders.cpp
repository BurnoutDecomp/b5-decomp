// FX-CRASHSND2 (crash parity 2026-09-24, item 1): the collision-sound InputCollision builders and
// the orientation / material mappers they call, against the ARTIST machine code.
//
//   InputCollision (regular: race-car / traffic contacts)  sub_826D3850   DWARF h:405
//   InputCollision (prop contacts)                          sub_826E8B20   DWARF h:414
//   MapPositionToOrientation                                 0x8269F418     DWARF cpp:318
//   MapPositionToOrientationUsingBox                         0x8269ED18     DWARF cpp:190
//   MapEntityIdToMaterial                                    0x826A0CF8     DWARF cpp:3593
//
// run_fxcrashsnd2_builders.py extracts the PRODUCTION region of BrnCollisionStateManager.cpp that
// holds these (from the traffic material table through the prop builder), TrafficClassToSize from
// BrnTrafficStateManager.h, GetRaceCar from BrnRaceCarCache.cpp and GetTrafficEntityIndex from
// BrnTrafficSoundInterfaces.cpp, and compiles them here against a fixture manager / input buffer.
// The InputCollision / ScrapeInfo / RaceCarCache / contact / traffic-interface types are the real
// headers.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"                 // EActiveRaceCarIndex (RaceCarCache::Update)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameSource/Sound/Collision/BrnRaceCarCache.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"
#include "GameSource/AttribSys/Enums/eMaterialType.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

// ---- the fixture input buffer (only what the builders read) -------------------------------------
namespace BrnWorld { namespace RaceCarEntityModuleIO {
struct VehicleInterfaceFixture
{
    s32 miPlayer = 0;
    s32 GetPlayerActiveRaceCarIndex() const { return miPlayer; }
};
} }
namespace BrnSound { namespace Module { namespace Io {
struct RootInputBuffer
{
    BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface mTraffic;
    BrnWorld::RaceCarEntityModuleIO::VehicleInterfaceFixture mVehicle;
    const BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface& GetTrafficOutputInterface() const { return mTraffic; }
    const BrnWorld::RaceCarEntityModuleIO::VehicleInterfaceFixture* GetVehicleInterface() const { return &mVehicle; }
};
} } }

namespace BrnTraffic { namespace BrnTrafficIO {
#include "fxcrashsnd2_get_traffic_entity_index.inc"
} }

namespace BrnSound { namespace Logic { namespace Traffic {
enum ETrafficSize { E_SMALL = 0, E_MEDIUM = 1, E_LARGE = 2, E_MAX_SIZES = 3 };
struct TrafficStateManager
{
#include "fxcrashsnd2_traffic_class_to_size.inc"
};
} } }

// ---- RaceCarCache::Update's two sources (fixtures of the real interface / deformation state) ------
namespace BrnPhysics { namespace Vehicle {
struct RaceCarState { Matrix44Affine mTransform; Vector3 mComOffset; EntityId mEntityId; };
} }
namespace BrnWorld { namespace RaceCarEntityModuleIO {
struct RCEntityActiveRaceCarOutputInterface
{
    bool mabActive[8] = {};
    BrnPhysics::Vehicle::RaceCarState maStates[8] = {};
    bool IsRaceCarActive(EActiveRaceCarIndex leIndex) const { return mabActive[leIndex]; }
    const BrnPhysics::Vehicle::RaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const { return &maStates[leIndex]; }
};
} }
namespace BrnPhysics { namespace Deformation {
struct CarState { Vector3 mDeformedBBoxMin; Vector3 mDeformedBBoxMax; };
struct DeformationState
{
    u32 muCarWithState = 0xFFFFFFFFu;
    CarState mState = {};
    const CarState* GetCarStateF(u32 luCarId) const { return luCarId == muCarWithState ? &mState : nullptr; }
};
} }

namespace BrnSound { namespace Logic { namespace Collision {

#include "fxcrashsnd2_get_race_car.inc"

typedef AttribSys::Enums::eMaterialType::eMaterialType EeMaterialType;

struct CameraInfo
{
    Matrix44Affine mTransform;
    f32 mfFieldOfView, mfCosineHalfFov, mfAspectRatio, mfZoom;
};

class CollisionStateManager
{
public:
    RaceCarCache mRaceCarCache;
    FrameInformation mFrameInformation;
    ScrapeInfo mHistoryHit;                 // what FindInScrapeHistory returns when armed
    bool mbHistoryArmed = false;
    std::vector<InputCollision> maAdded;    // AddInputCollision's record

    const RaceCarCache& GetRaceCarCache() const { return mRaceCarCache; }
    const FrameInformation& GetFrameInformation() const { return mFrameInformation; }
    ScrapeInfo* FindInScrapeHistory(const ScrapeInfo&) { return mbHistoryArmed ? &mHistoryHit : nullptr; }
    bool MapPropTypeToMaterial(u16 luPropType, u64& lruMaterial) const
    {
        if (luPropType != 5)
            return false;
        lruMaterial = 1ull << 7;
        return true;
    }
    void AddInputCollision(const InputCollision& lrCollision) { maAdded.push_back(lrCollision); }
};

EeMaterialType MapEntityIdToMaterial(EntityId lEntityId, s32 liPlayerIndex, const LogicInputBuffer& lInput);
AttribSys::Enums::eOrientation::eOrientation MapPositionToOrientationUsingBox(
    Vector3 lPosition, Vector3 lNormal, Matrix44Affine lTransform, Vector3 lUnused,
    Vector3 lComOffset, Vector3 lMin, Vector3 lMax);
bool MapPositionToOrientation(Matrix44Affine lTransform, Vector3 lPosition, Vector3 lNormal,
                              EntityId lVehicleIdA, EntityId lVehicleIdB,
                              const CollisionStateManager& lMgr,
                              AttribSys::Enums::eOrientation::eOrientation& leOrientation);

#include "fxcrashsnd2_builders_region.inc"

} } }

// ---- checks ----------------------------------------------------------------------------------------
using namespace BrnSound::Logic;
using namespace BrnSound::Logic::Collision;
namespace eO = AttribSys::Enums::eOrientation;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}
static bool Near(f32 a, f32 b, f32 tol = 1e-6f) { return std::fabs(a - b) <= tol; }

static EntityId Id(u32 luOwner, u32 luIndex) { EntityId l; l.muValue = (luOwner << 24) | (luIndex << 10); return l; }
static Vector3 V(f32 x, f32 y, f32 z) { Vector3 l = { x, y, z, 0.0f }; return l; }
static Matrix44Affine Frame(Vector3 lRight, Vector3 lUp, Vector3 lAt, Vector3 lPos)
{
    Matrix44Affine l; l.xAxis = lRight; l.yAxis = lUp; l.zAxis = lAt; l.wAxis = lPos; return l;
}
static Matrix44Affine Identity() { return Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(0, 0, 0)); }

static const Vector3 KV_MIN = { -1.0f, -0.5f, -2.0f, 0.0f };
static const Vector3 KV_MAX = { 1.0f, 0.5f, 2.0f, 0.0f };

static eO::eOrientation Box(Vector3 p, Matrix44Affine t = Identity(), Vector3 com = V(0, 0, 0))
{
    return MapPositionToOrientationUsingBox(p, V(0, 1, 0), t, V(0, 0, 0), com, KV_MIN, KV_MAX);
}

// A race-car cache node: previous = the frame the console reads (+0x11 / +0x60), current = a decoy.
static void ArmCar(CollisionStateManager& lrMgr, u32 luIndex, bool lbPreviousActive, bool lbCurrentActive)
{
    RaceCarCache::RaceCarCacheNode& lrNode = lrMgr.mRaceCarCache.maRaceCars[luIndex];
    lrNode.mbActive.mCurrentValue = lbCurrentActive;
    lrNode.mbActive.mPreviousValue = lbPreviousActive;
    lrNode.mTransform.mPreviousValue = Identity();
    lrNode.mTransform.mCurrentValue = Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(100, 0, 0));
    lrNode.mComOffset = V(0, 0, 0);
    lrNode.mMin = KV_MIN;
    lrNode.mMax = KV_MAX;
}

static InputContactSpy Spy(EntityId a, EntityId b, Vector3 onA, Vector3 onB, Vector3 normal, Vector3 stress)
{
    InputContactSpy l = {};
    l.mEntityIdA = a; l.mEntityIdB = b; l.mCollisionTagB.muValue = 0x5A5A;
    l.mFrictionStress = V(0, 0, 0); l.mNormalStress = stress; l.mNormal = normal;
    l.mPointOnA = onA; l.mPointOnB = onB;
    return l;
}

int main()
{
    // ---- MapPositionToOrientationUsingBox (0x8269ED18) ----------------------------------------
    Check(Box(V(0, 0, 1.9f)) == eO::Front, "UsingBox: near the max-z face is Front");
    Check(Box(V(0, 0, -1.9f)) == eO::Rear, "UsingBox: near the min-z face is Rear");
    Check(Box(V(0.95f, 0, 0)) == eO::Side, "UsingBox: near the max-x face is Side");
    Check(Box(V(-0.95f, 0, 0)) == eO::Side, "UsingBox: near the min-x face is Side");
    Check(Box(V(0, 0.49f, 0)) == eO::Roof, "UsingBox: near the max-y face is Roof");
    Check(Box(V(0, -0.49f, 0)) == eO::Bottom, "UsingBox: near the min-y face is Bottom (after Roof in the chain)");
    {
        // A car yawed 90 degrees (At = +X) standing at (10, 0, 5): the transposed rotation and the
        // translation term bring world points into its frame.
        const Matrix44Affine lYawed = Frame(V(0, 0, -1), V(0, 1, 0), V(1, 0, 0), V(10, 0, 5));
        Check(Box(V(11.9f, 0, 5), lYawed) == eO::Front, "UsingBox: yawed car, a point ahead along At is Front");
        Check(Box(V(8.1f, 0, 5), lYawed) == eO::Rear, "UsingBox: yawed car, a point behind is Rear");
        Check(Box(V(10, 0, 4.05f), lYawed) == eO::Side, "UsingBox: yawed car, a point along Right is Side");
    }
    Check(Box(V(0, 0, -1.1f), Identity(), V(0, 0, -3)) == eO::Front,
          "UsingBox: the COM offset is subtracted from the local point (0x8269EE14)");
    {
        const Vector3 lMin = { -1.0f, -1.0f, -2.0f, 0.0f };
        const Vector3 lMax = { 1.0f, 1.0f, 2.0f, 0.0f };
        Check(MapPositionToOrientationUsingBox(V(0, 0, 0), V(0, 1, 0), Identity(), V(0, 0, 0), V(0, 0, 0), lMin, lMax)
                  == eO::Side,
              "UsingBox: ties keep the earlier face (strict vcmpgtfp.): front == back, side wins, roof/bottom tie");
    }

    // ---- MapPositionToOrientation (0x8269F418) ---------------------------------------------------
    {
        CollisionStateManager lMgr;
        ArmCar(lMgr, 2, true, false);   // previous active: the console reads +0x11
        ArmCar(lMgr, 3, false, true);   // current active only
        eO::eOrientation le = eO::Roof;
        Check(!MapPositionToOrientation(Identity(), V(0.95f, 0, 0), V(0, 1, 0), Id(0, 0), Id(1, 2), lMgr, le)
                  && le == eO::Front,
              "MPO: A not a race car -> false and Front");
        le = eO::Roof;
        Check(!MapPositionToOrientation(Identity(), V(0.95f, 0, 0), V(0, 1, 0), Id(1, 3), Id(0, 0), lMgr, le)
                  && le == eO::Front,
              "MPO: a car active only in the CURRENT frame -> false and Front (reads mbActive previous)");
        le = eO::Front;
        Check(MapPositionToOrientation(Identity(), V(0.95f, 0, 0), V(0, 1, 0), Id(1, 2), Id(0, 0), lMgr, le)
                  && le == eO::Side,
              "MPO: the PREVIOUS cached transform decides (current would say Roof) -> Side");
        le = eO::Front;
        Check(MapPositionToOrientation(Identity(), V(0.95f, 0, 0), V(0, 1, 0), Id(1, 2), Id(1, 5), lMgr, le)
                  && le == eO::Front,
              "MPO: Side against another race car becomes Front");
        le = eO::Rear;
        Check(MapPositionToOrientation(Identity(), V(0, 0, 1.9f), V(0, 1, 0), Id(1, 2), Id(1, 5), lMgr, le)
                  && le == eO::Front,
              "MPO: Front against a race car stays Front");
    }

    // ---- MapEntityIdToMaterial (0x826A0CF8) ------------------------------------------------------
    BrnSound::Module::Io::RootInputBuffer lInput;
    lInput.mVehicle.miPlayer = 1;
    lInput.mTraffic.mu16EntityCount = 4;
    const u16 kauTrafficIndex[4] = { 7, 9, 11, 13 };
    const u8 kauClass[4] = { BrnTraffic::E_VEHICLECLASS_VAN, BrnTraffic::E_VEHICLECLASS_BUS,
                             BrnTraffic::E_VEHICLECLASS_CAR, BrnTraffic::E_VEHICLECLASS_BIGRIG };
    for (u32 i = 0; i < 4; ++i)
    {
        lInput.mTraffic.maActiveEntityList[i].mu16EntityIndex = kauTrafficIndex[i];
        lInput.mTraffic.maActiveEntityList[i].muVehicleClass = kauClass[i];
    }
    Check(MapEntityIdToMaterial(Id(0, 0), 1, lInput) == 0x10, "MEM: the world is World (0x10)");
    Check(MapEntityIdToMaterial(Id(1, 1), 1, lInput) == 0x2, "MEM: the player's race car is PlayerCar (2)");
    Check(MapEntityIdToMaterial(Id(1, 4), 1, lInput) == 0x4, "MEM: another race car is AiCar (4)");
    Check(MapEntityIdToMaterial(Id(2, 7), 1, lInput) == 0x800000, "MEM: a van is TrafficCarMedium (class->size {0,1,2,2})");
    Check(MapEntityIdToMaterial(Id(2, 9), 1, lInput) == 0x1000000, "MEM: a bus is TrafficCarLarge");
    Check(MapEntityIdToMaterial(Id(2, 11), 1, lInput) == 0x8, "MEM: a car is TrafficCar");
    Check(MapEntityIdToMaterial(Id(2, 13), 1, lInput) == 0x1000000, "MEM: a big rig is TrafficCarLarge");
    Check(MapEntityIdToMaterial(Id(2, 99), 1, lInput) == 0x8, "MEM: traffic not in the sound output is size 0 (TrafficCar)");
    Check(MapEntityIdToMaterial(Id(3, 20), 1, lInput) == 0x1, "MEM: a prop is Nothing (1)");
    Check(MapEntityIdToMaterial(Id(7, 1), 1, lInput) == 0x1, "MEM: any other owner is Nothing (1)");

    // ---- InputCollision, the regular builder (sub_826D3850) -------------------------------------
    CameraInfo lCamera = {};
    lCamera.mTransform = Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(0, 0, -10));
    const f32 kfDt = 1.0f / 60.0f;
    {
        CollisionStateManager lMgr;
        ArmCar(lMgr, 1, true, true);
        // Car 3 hits car 1: the builder orders them (1, 3) and swaps the contact points too, so the
        // orientation is car 1's at the ORIGINAL point on B (0.95, 0, 0) -> Side -> Front against a
        // race car. Taken at the unswapped point (5, 0, 0) it would be Roof.
        const InputContactSpy lSpy = Spy(Id(1, 3), Id(1, 1), V(5, 0, 0), V(0.95f, 0, 0), V(0, 1, 0), V(3, 4, 0));
        InputCollision lIn(lCamera, lMgr, lSpy, lInput, 7.0f, kfDt);
        Check(lIn.maEntityID[0].muValue == Id(1, 1).muValue && lIn.maEntityID[1].muValue == Id(1, 3).muValue,
              "regular: two race cars are ordered by index (the higher second)");
        Check(Near(lIn.mPosition.x, 5.0f) && Near(lIn.mPosition.y, 0.0f),
              "regular: mPosition is the ORIGINAL spy's point on A (stored before the swap)");
        Check(lIn.meOrientation == eO::Front,
              "regular: orientation from the swapped A at ITS point (Side vs a race car -> Front)");
        Check(Near(lIn.maParameter[0].x, 5.0f * (1.0f / (320000.0f * kfDt)), 1e-9f) &&
                  lIn.maParameter[0].w == lIn.maParameter[0].x,
              "regular: impulse = |normal stress| * 1/(320000*dt), splatted");
        Check(lIn.maMaterial[0] == 0x2 && lIn.maMaterial[1] == 0x4, "regular: materials PlayerCar / AiCar");
        Check(lIn.mePipeline == InputCollision::E_REGULAR && lIn.meAction == AttribSys::Enums::eAction::Collision &&
                  !lIn.mbCull && lIn.mfPriorityAddition == 0.0f,
              "regular: pipeline / action / cull / priority defaults");
        Check(lIn.mScrapeInfo.mbValid && !lIn.mScrapeInfo.mbCrashing &&
                  lIn.mScrapeInfo.mEntityIdA.muValue == Id(1, 1).muValue &&
                  lIn.mScrapeInfo.mEntityIdB.muValue == Id(1, 3).muValue &&
                  lIn.mScrapeInfo.mCollisionTagB.muValue == 0x5A5A && lIn.mScrapeInfo.mfTimeStamp == 7.0f &&
                  lIn.mScrapeInfo.meOrientation == lIn.meOrientation &&
                  lIn.mScrapeInfo.mfIntensity == lIn.maParameter[0].x &&
                  lIn.mScrapeInfo.mRelativeVelocity.x == 0.0f,
              "regular: the scrape entry = {pair, tag B, stamp, orientation, intensity, valid}");
    }
    {
        // Facing: normalize(position - camera) . the camera's At row -- NOT the contact normal.
        CollisionStateManager lMgr;
        const InputContactSpy lSpy = Spy(Id(1, 1), Id(0, 0), V(0, 0, 0), V(0, 0, 0), V(0, 1, 0), V(1, 0, 0));
        InputCollision lIn(lCamera, lMgr, lSpy, lInput, 0.0f, kfDt);
        Check(Near(lIn.maParameter[1].x, 100.0f) && Near(lIn.maParameter[2].x, 1.0f),
              "regular: distance^2 to the camera 100, facing along the camera At = 1 (the normal would give 0)");
        CameraInfo lSideways = lCamera;
        lSideways.mTransform.zAxis = V(1, 0, 0);
        InputCollision lIn2(lSideways, lMgr, lSpy, lInput, 0.0f, kfDt);
        Check(Near(lIn2.maParameter[2].x, 0.0f), "regular: facing follows the camera At row (perpendicular -> 0)");
    }
    {
        // A race car's contact with a prop: with the car in the cache the pair stays (car, prop) and
        // the prop's material is exactly Nothing (1) -- SelectBin's `cmpldi 1` exit.
        CollisionStateManager lMgr;
        ArmCar(lMgr, 1, true, true);
        const InputContactSpy lSpy = Spy(Id(1, 1), Id(3, 20), V(0, 0, 1.9f), V(0, 0, 1.9f), V(0, 1, 0), V(1, 0, 0));
        InputCollision lIn(lCamera, lMgr, lSpy, lInput, 0.0f, kfDt);
        Check(lIn.maMaterial[0] == 0x2 && lIn.maMaterial[1] == 1ull,
              "regular: car vs prop -> (PlayerCar, Nothing == 1) -- no 0x2000000000 bit, so SelectBin drops it");
        // Not in the cache (never updated): MapPositionToOrientation fails and the pair is reversed.
        CollisionStateManager lCold;
        InputCollision lIn2(lCamera, lCold, lSpy, lInput, 0.0f, kfDt);
        Check(lIn2.maEntityID[0].muValue == Id(3, 20).muValue && lIn2.maMaterial[0] == 1ull &&
                  lIn2.maMaterial[1] == 0x2 && lIn2.meOrientation == eO::Front,
              "regular: a race car absent from the cache -> the pair reversed (prop, car), Front");
    }
    {
        // Pair reversed when A is not an active race car: B's side, B's point.
        CollisionStateManager lMgr;
        ArmCar(lMgr, 2, true, true);
        const InputContactSpy lSpy = Spy(Id(2, 7), Id(1, 2), V(5, 5, 5), V(0.95f, 0, 0), V(0, 1, 0), V(1, 0, 0));
        InputCollision lIn(lCamera, lMgr, lSpy, lInput, 0.0f, kfDt);
        Check(lIn.maEntityID[0].muValue == Id(1, 2).muValue && lIn.maEntityID[1].muValue == Id(2, 7).muValue &&
                  lIn.meOrientation == eO::Side && lIn.mScrapeInfo.mEntityIdA.muValue == Id(1, 2).muValue,
              "regular: A not a race car -> the pair reversed, orientation from B at B's point (Side vs traffic)");
        Check(lIn.maMaterial[0] == 0x4 && lIn.maMaterial[1] == 0x800000,
              "regular: the reversed pair's materials (AiCar / van -> TrafficCarMedium)");
    }
    {
        // SloMoCrash culling (0x826D3C54..0x826D3C90).
        CollisionStateManager lMgr;
        lMgr.mbHistoryArmed = true;
        lMgr.mHistoryHit.mfTimeStamp = 3.0f;
        const InputContactSpy lSpy = Spy(Id(1, 1), Id(0, 0), V(0, 0, 0), V(0, 0, 0), V(0, 1, 0), V(1, 0, 0));
        lMgr.mFrameInformation.meImpactTime.mCurrentValue = AttribSys::Enums::eImpactTime::True;
        Check(InputCollision(lCamera, lMgr, lSpy, lInput, 7.9f, kfDt).mbCull,
              "regular: in impact time a scrape seen 4.9 s ago culls the collision");
        Check(!InputCollision(lCamera, lMgr, lSpy, lInput, 8.1f, kfDt).mbCull,
              "regular: 5.1 s ago it does not (age < 5.0, flt_820ABCD8)");
        lMgr.mFrameInformation.meImpactTime.mCurrentValue = AttribSys::Enums::eImpactTime::False;
        Check(!InputCollision(lCamera, lMgr, lSpy, lInput, 7.9f, kfDt).mbCull,
              "regular: outside impact time (False) nothing is culled");
    }
    {
        CollisionStateManager lMgr;
        const InputContactSpy lSpy = Spy(Id(1, 1), Id(0, 0), V(0, 0, 0), V(0, 0, 0), V(0, 1, 0), V(3, 4, 0));
        InputCollision lIn(lCamera, lMgr, lSpy, lInput, 0.0f, 0.0f);
        Check(std::isinf(lIn.maParameter[0].x), "regular: no dt guard (dt = 0 -> infinite impulse, as fdivs gives)");
    }

    // ---- InputCollision, the prop builder (sub_826E8B20) -----------------------------------------
    {
        CollisionStateManager lMgr;
        ArmCar(lMgr, 1, true, true);
        InputPropSpy lProp = {};
        static_cast<InputContactSpy&>(lProp) =
            Spy(Id(3, 20), Id(1, 1), V(1, 0, 0), V(0.95f, 0, 0), V(0, 1, 0), V(3, 4, 0));
        lProp.muType = 5;
        lProp.muBeganMoving = 0;
        InputCollision lIn(lCamera, lMgr, lProp, lInput, 0.0f, kfDt);
        Check(lIn.mePipeline == InputCollision::E_PROP && !lIn.mbCull && lIn.maMaterial[0] == (1ull << 7) &&
                  lIn.maMaterial[1] == 0x2,
              "prop: pipeline PROP, the prop type's material, B = PlayerCar");
        Check(lIn.meOrientation == eO::Side,
              "prop: orientation from B (the race car) at B's point, against a prop -> Side");
        Check(Near(lIn.mPosition.x, 1.0f) && lIn.mfPriorityAddition == 0.0f && lMgr.maAdded.empty(),
              "prop: mPosition is the point on A; no priority; no extra collision");
        lProp.muType = 6;
        lProp.muBeganMoving = 1;
        InputCollision lIn2(lCamera, lMgr, lProp, lInput, 0.0f, kfDt);
        Check(lIn2.mbCull && lIn2.maMaterial[0] == 0,
              "prop: an unmapped prop type is culled with material 0 (the zeroed local)");
        Check(lIn2.mfPriorityAddition == 1.0f, "prop: a prop that began moving adds priority 1.0");
    }
    {
        // Prop against prop: A against the world, and B added as its own collision.
        CollisionStateManager lMgr;
        InputPropSpy lProp = {};
        static_cast<InputContactSpy&>(lProp) =
            Spy(Id(3, 20), Id(3, 21), V(1, 2, 3), V(4, 5, 6), V(0, 1, 0), V(3, 4, 0));
        lProp.muType = 5;
        InputCollision lIn(lCamera, lMgr, lProp, lInput, 0.0f, kfDt);
        Check(lIn.maEntityID[1].muValue == 0 && lIn.maMaterial[1] == 0x10,
              "prop vs prop: this collision is A against the world (B := 0, World)");
        Check(lMgr.maAdded.size() == 1u, "prop vs prop: B gets its own collision, added to the manager");
        if (lMgr.maAdded.size() == 1u)
        {
            const InputCollision& lrB = lMgr.maAdded[0];
            Check(lrB.maEntityID[0].muValue == Id(3, 21).muValue && lrB.maEntityID[1].muValue == 0 &&
                      Near(lrB.mPosition.x, 4.0f) && Near(lrB.mPosition.z, 6.0f) && lrB.maMaterial[1] == 0x10 &&
                      lrB.maMaterial[0] == (1ull << 7) && lrB.mePipeline == InputCollision::E_PROP,
                  "prop vs prop: B's collision = {B, world}, at B's point, World, prop pipeline");
        }
    }

    // ---- RaceCarCache::Update (0x826BF478) ---------------------------------------------------------
    {
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface lIface;
        BrnPhysics::Deformation::DeformationState lDeform;
        RaceCarCache lCache;
        for (u32 i = 0; i < 8; ++i)
        {
            lCache.maRaceCars[i].mComOffset = V(9, 9, 9);
            lCache.maRaceCars[i].mMin = V(-9, -9, -9);
            lCache.maRaceCars[i].mMax = V(9, 9, 9);
        }
        lIface.mabActive[2] = true;
        lIface.maStates[2].mTransform = Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(1, 2, 3));
        lIface.maStates[2].mComOffset = V(0, 0.5f, 0);
        lIface.maStates[2].mEntityId = Id(1, 2);
        lIface.mabActive[3] = true;
        lIface.maStates[3].mTransform = Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(7, 7, 7));
        lIface.maStates[3].mEntityId = Id(1, 3);
        lDeform.muCarWithState = Id(1, 2).muValue;
        lDeform.mState.mDeformedBBoxMin = KV_MIN;
        lDeform.mState.mDeformedBBoxMax = KV_MAX;

        lCache.Update(lIface, lDeform);
        const RaceCarCache::RaceCarCacheNode& lr2 = lCache.maRaceCars[2];
        const RaceCarCache::RaceCarCacheNode& lr3 = lCache.maRaceCars[3];
        Check(lr2.mbActive.mCurrentValue && !lr2.mbActive.mPreviousValue && !lCache.maRaceCars[0].mbActive.mCurrentValue,
              "RCC::Update: mbActive = IsRaceCarActive(i) through the DataPoint (previous <- old current)");
        Check(lr2.mTransform.mCurrentValue.wAxis.z == 3.0f,
              "RCC::Update: an active car's transform becomes the DataPoint's current");
        Check(lr2.mComOffset.y == 0.5f && lr2.mMin.z == KV_MIN.z && lr2.mMax.x == KV_MAX.x,
              "RCC::Update: with a deformation state, COM offset and the deformed box are copied");
        Check(lr3.mTransform.mCurrentValue.wAxis.x == 7.0f && lr3.mComOffset.x == 9.0f && lr3.mMin.x == -9.0f,
              "RCC::Update: without a deformation state the transform moves but COM / box keep their last values");

        lIface.maStates[2].mTransform = Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(50, 0, 0));
        lIface.mabActive[3] = false;
        lIface.maStates[3].mTransform = Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(8, 8, 8));
        lCache.Update(lIface, lDeform);
        Check(lr2.mTransform.mPreviousValue.wAxis.z == 3.0f && lr2.mTransform.mCurrentValue.wAxis.x == 50.0f &&
                  lr2.mbActive.mPreviousValue && lr2.mbActive.mCurrentValue,
              "RCC::Update: the second frame shifts current into previous");
        Check(!lr3.mbActive.mCurrentValue && lr3.mbActive.mPreviousValue && lr3.mTransform.mCurrentValue.wAxis.x == 7.0f,
              "RCC::Update: a car gone inactive keeps its last transform (only mbActive moves)");

        // End to end: orientation is taken in the PREVIOUS cached frame (the car at (1,2,3), COM
        // +0.5 y), where (1.95, 2.5, 3) is at its max-x face -- in the current frame (50,0,0) it
        // would be Front.
        CollisionStateManager lMgr;
        lMgr.mRaceCarCache = lCache;
        eO::eOrientation le = eO::Front;
        Check(MapPositionToOrientation(Identity(), V(1.95f, 2.5f, 3), V(0, 1, 0), Id(1, 2), Id(0, 0), lMgr, le) &&
                  le == eO::Side,
              "RCC -> MPO: the orientation uses the previous cached transform, the COM offset and the deformed box");
    }

    std::printf("FxCrashSnd2Builders: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
