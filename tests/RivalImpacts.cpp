// Production classifier regression: geometry, attribution, cooldown and severity boundaries.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"
#undef protected
#undef private
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{
    std::fprintf(stderr, "Assertion: %s\n", lpcMessage); std::abort();
}
void* EndAssert() { return nullptr; }
} }

using namespace BrnPhysics::Vehicle;
static bool gbReceivingImpact = false;
static bool gbCrashDecision = true;
static int giShoves = 0, giCrashes = 0;
static EntityId gVictim, gAggressor;
static float gfScaleA, gfScaleB;

static CgsSceneManager::SceneManagerIO::PotentialContact gPotential;
static u32 guRequestedContact;
namespace BrnPhysics { namespace Deformation {
DeformationSensor::DeformationSensor() {}
void DeformationSensor::ApplyLocalImpulse(ImpulseParams*) { std::abort(); }
void DeformationSensor::RecievePassedOnImpulse(const ImpulseParams*, VecFloat) { std::abort(); }
} namespace PhysicsModuleIO {
const CgsSceneManager::SceneManagerIO::PotentialContact& PotentialContactInterface::GetEvent(ContactId lId) const
{ guRequestedContact = lId.muId; return gPotential; }
} }
namespace CgsPhysics { namespace PhysicsSimulationIO {
OutputBuffer::OutContactSpyQueue* OutputBuffer::GetContactSpyQueue() { return &mContactSpyQueue; }
} }

// The fixture constructs the real manager and its contained cars. These unrelated
// vtable entries must link but must never run in a classifier test.
namespace CgsDev {
DebugComponent::DebugComponent() {}
void DebugComponent::Update() { std::abort(); }
void DebugComponent::RenderWorld(Debug3DImmediateRender*) { std::abort(); }
void DebugComponent::RenderHUD(Debug2DImmediateRender*) { std::abort(); }
const char* DebugComponent::GetPath() const { std::abort(); }
const char* DebugComponent::GetName() const { std::abort(); }
bool DebugComponent::IsSimple() const { std::abort(); }
void DebugComponent::OnActivate() { std::abort(); }
void DebugComponent::OnRegister() { std::abort(); }
namespace DebugUI {
Window::Window() {}
void Window::Update(f32, InputEvent) { std::abort(); }
void Window::Render(Debug2DImmediateRender*) { std::abort(); }
void Window::OnGetFocus() { std::abort(); }
void Window::OnLostFocus() { std::abort(); }
void Window::GetMenuPath(char*, s32) { std::abort(); }
void Window::GetSelectedItemString(char*, s32) const { std::abort(); }
} }

namespace BrnPhysics { namespace Vehicle {
VecFloat SimpleVehiclePhysics::GetSteeringAngle() const { std::abort(); }
void SimpleVehiclePhysics::ClearCrashing() { std::abort(); }
void SimpleVehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
void VehiclePhysics::UpdateSuspension(VecFloat) { std::abort(); }
void VehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::ClearCrashing() { std::abort(); }
VecFloat VehiclePhysics::GetSteeringAngle() const { std::abort(); }
bool RaceCarPhysics::IsCrashingNormally() const { std::abort(); }
bool RaceCarPhysics::Prepare(Matrix44Affine, Vector3, Vector3, Vector3, Vector3,
    const CgsGeometric::AxisAlignedBox&, rw::physics::Inertia, VehicleAttribs*, const Vector3*, const f32*, u8)
{ std::abort(); }
void RaceCarPhysics::SetCrashing() { std::abort(); }
void RaceCarPhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
f32 RaceCarPhysics::GetShowtimePlayerCarStrength() const { std::abort(); }
bool RaceCarPhysics::IsPlayerVehicleInShowtime() const { std::abort(); }
void RaceCarPhysics::UpdateAftertouch(const BrnPlayerDriverControls*, const Matrix44Affine*, VecFloat, bool, bool)
{ std::abort(); }
void TrafficPhysics::Update(f32, f32, const Matrix44Affine*, const BrnPlayerDriverControls*, bool, bool, bool)
{ std::abort(); }
const char* PhysicalTrafficManagerDebugComponent::GetName() const { std::abort(); }
const char* VehicleManagerDebugComponent::GetName() const { std::abort(); }
const char* DebugComponent::GetName() const { std::abort(); }
const char* DebugComponent::GetPath() const { std::abort(); }
bool VehicleManager::HasRaceCarHadRecentImpact(s32 liIndex)
{ return mafNoImpactTimeSeconds[liIndex] > 0.0f; }
bool VehiclePhysics::IsBeingSlamedOrShuntedByRaceCar(s8) const { return gbReceivingImpact; }
void VehicleManager::ApplyShunt(RaceCarResponseInfo*) { ++giShoves; }
bool VehicleManager::ShouldRaceCarCrashOnCarImpact(EActiveRaceCarIndex leIndex,
    const RaceCarPhysics*, const SimpleVehiclePhysics*, VecFloat, VecFloat lvfScale) const
{
    if (leIndex == 0) gfScaleA = lvfScale.x; else gfScaleB = lvfScale.x;
    return gbCrashDecision;
}
void VehicleManager::InstantTakedown(EntityId lVictim, EntityId lAggressor, Vector3, Vector3, f32,
    VehicleOutputRequestInterface*, VehicleManagerOutputInterface*, VehicleOutputInterface*,
    BrnPhysics::Deformation::DeformationInputInterface*, BrnGameState::ETakedownType)
{ ++giCrashes; gVictim = lVictim; gAggressor = lAggressor; }
} }

static void Check(bool lbCondition, const char* lpcMessage)
{ if (!lbCondition) { std::fprintf(stderr, "FAIL: %s\n", lpcMessage); std::abort(); } }

static void CheckSensorOutput()
{
    static CgsPhysics::PhysicsSimulationIO::OutputBuffer lOutput;
    BrnPhysics::Deformation::DeformationSensor lSensor;
    lOutput.mContactSpyQueue.Construct();
    for (int i = 0; i < 4; ++i)
    {
        lSensor.maPostPhysicsVec0[i] = static_cast<float>(i + 10);
        lSensor.maPostPhysicsVec1[i] = 0;
    }
    lSensor.OutputContactSpy(&lOutput, nullptr, EntityId{0});
    Check(lOutput.mContactSpyQueue.GetLength() == 0, "zero sensor stress produces no contact");
    lSensor.maPostPhysicsVec1[0] = 0.001f;
    lSensor.OutputContactSpy(&lOutput, nullptr, EntityId{0});
    Check(lOutput.mContactSpyQueue.GetLength() == 0, "subthreshold stress produces no contact");
    lSensor.maPostPhysicsVec1[0] = 5;
    lSensor.mSpyNormal = Vector3{1, 2, 3, 4};
    lSensor.mSpyPointOnA = Vector3{5, 6, 7, 8};
    lSensor.mSpyPointOnB = Vector3{9, 10, 11, 12};
    lSensor.mSpyVolumeInstanceIdA = 0x0100040000000009ULL;
    lSensor.mSpyVolumeInstanceIdB = 0x010008000000000AULL;
    lSensor.mSpyContactId = 0x07000012;
    gPotential.muVolumeInstanceIdA.muId = lSensor.mSpyVolumeInstanceIdA;
    gPotential.muVolumeInstanceIdB.muId = lSensor.mSpyVolumeInstanceIdB;
    // GetEvent is an observed-output double; it does not dereference the interface.
    static BrnPhysics::PhysicsModuleIO::PotentialContactInterface lContacts;
    lSensor.OutputContactSpy(&lOutput, &lContacts, EntityId{0x01000400});
    const auto& lFirst = lOutput.mContactSpyQueue.GetEvent(0);
    Check(lOutput.mContactSpyQueue.GetLength() == 1 && guRequestedContact == 0x07000012,
          "race-car sensor publishes the tagged collision");
    Check(lFirst.mIDA == lSensor.mSpyVolumeInstanceIdA && lFirst.mNormal.x == 1
          && lFirst.mPointOnA.x == 5 && lFirst.mNormalStress.x == 5 && lFirst.mFrictionStress.w == 13,
          "sensor payload preserves geometry and stress");
    // Physical traffic slot 3 belongs to global scene slot 7, which is contact B.
    lSensor.mSpyVolumeInstanceIdA = 0x02000C0000000009ULL;
    lSensor.mSpyVolumeInstanceIdB = 0x010008000000000AULL;
    gPotential.muVolumeInstanceIdA.muId = lSensor.mSpyVolumeInstanceIdB;
    gPotential.muVolumeInstanceIdB.muId = 0x02001C0000000009ULL;
    lSensor.OutputContactSpy(&lOutput, &lContacts, EntityId{0x02001C00});
    const auto& lSecond = lOutput.mContactSpyQueue.GetEvent(1);
    Check(lSecond.mIDA == 0x010008000000000AULL && lSecond.mIDB == 0x02000C0000000009ULL
          && lSecond.mNormal.x == -1 && lSecond.mNormal.w == -4
          && lSecond.mPointOnA.x == 9 && lSecond.mPointOnB.x == 5,
          "traffic scene identity orients physical ids and all normal lanes");
    Check(lSensor.mSpyVolumeInstanceIdA == lSecond.mIDA && lSensor.mSpyPointOnA.x == 9
          && lSecond.muTag == 0x07000012 && lSecond.mNormalStress.x == 5 && lSecond.mFrictionStress.w == 13,
          "swap persists on sensor while stress and tag remain intact");
}

static VehicleManager gManager;
static BrnPhysics::ContactSpy::RaceCarContact gContact;

static VehicleManager::RaceCarResponseInfo Setup()
{
    auto& lrManager = gManager;
    lrManager.mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
    lrManager.maeRaceCarTypes[0] = BrnWorld::E_RACE_CAR_TYPE_PLAYER;
    lrManager.maeRaceCarTypes[1] = BrnWorld::E_RACE_CAR_TYPE_AI;
    lrManager.mfMinShuntSpeed = 12.0f;
    lrManager.mfFatalShuntSpeed = 140.0f;
    lrManager.mfMinTradingPaintSpeed = 0.8f;
    lrManager.mfFatalSlamSpeed = 140.0f;
    lrManager.mfMaxHeadToHeadAngle = 45.0f;
    lrManager.mfMinHeadToHeadIndividualSpeed = 40.0f;
    lrManager.mbIsOnlineGameMode = false;
    gbReceivingImpact = false; gbCrashDecision = true; giShoves = 0; giCrashes = 0;
    for (int liCar = 0; liCar < 2; ++liCar)
    {
        lrManager.mafNoImpactTimeSeconds[liCar] = 0;
        lrManager.mau8FramesSincePlayerGrindingOther[liCar] = 30;
        lrManager.mau8FramesSinceOtherGrindingPlayer[liCar] = 30;
        lrManager.maRaceCarDrivers[liCar].mControls.mbBoost = false;
        auto& lrCar = lrManager.maRaceCarVehicles[liCar];
        lrCar.mTransform = {};
        lrCar.mTransform.xAxis.x = 1; lrCar.mTransform.yAxis.y = 1; lrCar.mTransform.zAxis.z = 1;
        lrCar.mfMass = {1000,1000,1000,1000};
        lrCar.mfSlamSteering = 0;
        lrCar.mi8LastAttackersRaceCarIndex = -1; lrCar.mi8LastContactedRaceCar = -1;
        lrCar.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z = 10;
        lrCar.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y = 0;
    }
    gContact = {};
    gContact.mEntityIdA = {0x01000000}; gContact.mEntityIdB = {0x01000400};
    gContact.mNormal.z = -1;
    VehicleManager::RaceCarResponseInfo lInfo = {};
    lInfo.mpContact = &gContact;
    lInfo.mpRaceCarA = &lrManager.maRaceCarVehicles[0]; lInfo.mpRaceCarB = &lrManager.maRaceCarVehicles[1];
    lInfo.meActiveRaceCarIndexA = static_cast<EActiveRaceCarIndex>(0);
    lInfo.meActiveRaceCarIndexB = static_cast<EActiveRaceCarIndex>(1);
    lInfo.mRaceCarAEntityID = gContact.mEntityIdA; lInfo.mRaceCarBEntityID = gContact.mEntityIdB;
    lInfo.mRaceCarATransform = lInfo.mpRaceCarA->mTransform;
    lInfo.mRaceCarBTransform = lInfo.mpRaceCarB->mTransform;
    lInfo.mRaceCarBTransform.wAxis.z = 4;
    lInfo.mbRaceCarAIsPlayer = true;
    lInfo.mfClosingSpeed = 10;
    lInfo.meAggressorActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
    lInfo.meVictimActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
    lInfo.meImpactSitutation = E_IMPACT_SITUATION_INVALID;
    return lInfo;
}

int main()
{
    auto lInfo = Setup();
    Check(gManager.CheckForShuntAndNudge(&lInfo), "rear-end contact produces a shunt");
    Check(lInfo.meImpactType == E_IMPACT_SHUNT && lInfo.meAggressorActiveRaceCarIndex == 0,
          "rear car is aggressor");
    lInfo = Setup(); gContact.mNormal = {1,0,0,0};
    Check(!gManager.CheckForShuntAndNudge(&lInfo), "side contact is not a shunt");
    lInfo = Setup(); lInfo.mRaceCarBTransform.wAxis.z = -4;
    Check(gManager.CheckForShuntAndNudge(&lInfo) && lInfo.meAggressorActiveRaceCarIndex == 1,
          "reversed ordering attributes rear impact to B");
    lInfo = Setup(); lInfo.mfClosingSpeed = 12 * 0.44704f;
    Check(gManager.CheckForShuntAndNudge(&lInfo) && lInfo.meImpactType == E_IMPACT_NUDGE,
          "threshold equality stays nudge");
    lInfo = Setup(); gManager.maRaceCarDrivers[0].mControls.mbBoost = true;
    Check(gManager.CheckForShuntAndNudge(&lInfo) && lInfo.meImpactType == E_IMPACT_BOOST_SHUNT,
          "boost promotes a shunt");
    lInfo = Setup(); gbReceivingImpact = true;
    Check(!gManager.CheckForShuntAndNudge(&lInfo), "a shove cannot reverse an ongoing shove");
    lInfo = Setup(); gManager.mafNoImpactTimeSeconds[1] = 0.01f;
    Check(!gManager.CheckForShuntAndNudge(&lInfo), "cooldown suppresses duplicate impact");
    lInfo = Setup(); lInfo.mfClosingSpeed = 100; lInfo.mbRaceCarBIsNetworkCar = true;
    Check(gManager.CheckForShuntAndNudge(&lInfo) && lInfo.mbCrashRaceCarA && !lInfo.mbCrashRaceCarB,
          "fatal shunt leaves remote victim to its owner");

    lInfo = Setup(); lInfo.mRaceCarATransform.wAxis.x = 2; lInfo.mpRaceCarA->mfSlamSteering = 0.8f;
    Check(gManager.CheckForSlamAndTradingPaint(&lInfo) && lInfo.meImpactType == E_IMPACT_SLAM,
          "side steering generates a slam");
    Check(lInfo.meAggressorActiveRaceCarIndex == 0 && std::fabs(lInfo.mvfSlamMagnitude.x-0.8f)<0.0001f,
          "slam magnitude and aggressor survive classification");
    lInfo = Setup(); lInfo.mRaceCarATransform.wAxis.x = 2; lInfo.mpRaceCarB->mfSlamSteering = -0.8f;
    Check(gManager.CheckForSlamAndTradingPaint(&lInfo) && lInfo.meAggressorActiveRaceCarIndex == 1,
          "B can slam A");
    lInfo = Setup(); lInfo.mRaceCarATransform.wAxis.x = 2; lInfo.mpRaceCarA->mfSlamSteering = 0.05f;
    Check(gManager.CheckForSlamAndTradingPaint(&lInfo) && lInfo.meImpactType == E_IMPACT_TRADING_PAINT,
          "small steer is trading paint");
    lInfo = Setup();
    Check(gManager.CheckForSlamAndTradingPaint(&lInfo) && lInfo.meImpactType == E_IMPACT_NONE,
          "neither steering yields no fabricated aggressor");
    lInfo = Setup(); lInfo.mRaceCarATransform.wAxis.x = 2; lInfo.mpRaceCarA->mfSlamSteering = 0.4f;
    gManager.mau8FramesSincePlayerGrindingOther[1] = 29;
    Check(!gManager.CheckForSlamAndTradingPaint(&lInfo), "recent grinding vetoes weak slam");

    lInfo = Setup(); lInfo.mfAngleBetweenCars = 3.14159265f;
    lInfo.mfRaceCarASpeed = 20; lInfo.mfRaceCarBSpeed = 30; lInfo.mpRaceCarA->mfMass.x = 2000;
    gManager.CheckForHeadToHead(&lInfo);
    Check(giShoves == 1 && lInfo.meVictimActiveRaceCarIndex == 1,
          "heavier slower car wins head-on momentum comparison");
    lInfo = Setup(); gManager.mbIsOnlineGameMode = true; lInfo.mfRaceCarASpeed = 40;
    Check(gManager.CheckForStationaryTargetTakedown(&lInfo) && giCrashes == 1,
          "stationary target several metres away can be taken down");
    lInfo = Setup(); gManager.mbIsOnlineGameMode = true; lInfo.mfRaceCarASpeed = 40;
    lInfo.mRaceCarBTransform.wAxis.z = 0.1f;
    Check(!gManager.CheckForStationaryTargetTakedown(&lInfo), "coincident spawn positions are excluded");

    lInfo = Setup(); gManager.maeRaceCarTypes[0] = BrnWorld::E_RACE_CAR_TYPE_AI;
    Check(!gManager.CheckForPlayerSlammingAIIntoAI(&lInfo) && giCrashes == 0,
          "unrelated AI collision does not award player a domino takedown");
    lInfo.mpRaceCarA->mi8LastAttackersRaceCarIndex = 0; lInfo.mpRaceCarA->mi8LastContactedRaceCar = 0;
    lInfo.mpRaceCarA->mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z = 0.1f;
    Check(gManager.CheckForPlayerSlammingAIIntoAI(&lInfo) && giCrashes == 2 && gfScaleA == 2 && gfScaleB == 0.75f,
          "recent player shove enables domino thresholds for both cars");
    lInfo = Setup(); lInfo.mbRaceCarAIsCrashing = true;
    Check(!gManager.CheckForHittingAlreadyCrashingCar(&lInfo), "fresh crash is not yet an obstacle");
    lInfo.mpRaceCarA->mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y = 1.01f;
    Check(gManager.CheckForHittingAlreadyCrashingCar(&lInfo) && lInfo.mbCrashRaceCarB,
          "older wreck can crash the other car");
    CheckSensorOutput();
    std::puts("PASS: rival impact and sensor output regression checks");
}
