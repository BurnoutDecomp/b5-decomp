#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameSource/Director/Camera/Utils/BrnSpeedResponder.h"
#include "GameSource/Director/Camera/Utils/BrnTempBoostResponder.h"
#undef protected
#undef private
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { std::fprintf(stderr,"Assertion: %s\n",message); std::abort(); }
void* EndAssert() { return nullptr; }
} }
namespace BrnAI {
Vector3 AICar::GetDirection() const { return mDirection; }
Vector3 AICar::GetVelocity() const { return mVelocity; }
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetLastGoodPosition() const { return mLastGoodPosition; }
bool AICar::HasValidRoute() const { return GetRoute()->GetStatus() != Route::E_STATUS_UNINITIALISED && GetRoute()->GetNodeCount() > 0; }
const AISection* AISectionsData::GetAISection(u32 index) const { return &mpaSections[index]; }
const Portal* AISection::GetPortal(u8 index) const { return &mpaPortals[index]; }
u16 Portal::GetLinkSectionIndex() const { return mu16LinkSection; }
bool AISection::IsUnsuitableForResetOnTrackLink() const { return (mx8Flags & 0x41) != 0; }
Vector3 AISection::GetMiddle() const { return {0,0,5,0}; } // fixture section centre

}
// Geometry tests never bind a resource to their manager fixture.
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

// The runner inserts the production definitions here, unchanged.
#include "restored_methods.inc"

int main()
{
    using namespace rw::math::vpu;
    using namespace BrnDirector::Camera;
    int checks = 0;
    auto check = [&](bool value, const char* label) {
        ++checks; if (!value) { std::fprintf(stderr,"FAIL: %s\n",label); std::exit(1); }
    };
    auto near = [](float a,float b) { return std::fabs(a-b) < 0.0001f; };
    BrnAI::ResetOnTrackManager reset;
    auto p = reset.ComputeNearestPositionInSegment({3,9,5,0},{0,0,0,0},{0,0,10,0});
    check(near(p.x,0) && near(p.y,0) && near(p.z,5),"wall-side point projects onto road");
    p = reset.ComputeNearestPositionInSegment({0,0,-4,0},{0,0,0,0},{0,0,10,0});
    check(near(p.z,0),"before road segment clamps to entry portal");
    p = reset.ComputeNearestPositionInSegment({0,0,15,0},{0,0,0,0},{0,0,10,0});
    check(near(p.z,10),"after road segment clamps to exit portal");
    p = reset.ComputeNearestPositionInSegment({0,5,0,0},{0,0,0,0},{0,10,10,0});
    check(near(p.y,2.5f) && near(p.z,2.5f),"sloped road projection preserves elevation");
    BrnAI::AISection::Vector2 corners[] = {{-3,0},{3,0},{3,20},{-3,20}};
    BrnAI::AISection section{}; section.mpaCorners = corners;
    check(near(reset.ComputeAISectionWidth(&section,{0,1,0,0}),6),"road width across travel axis");
    check(near(reset.ComputeAISectionWidth(&section,{1,0,0,0}),20),"rotated travel axis selects opposite edges");
    BrnAI::AICar car{}; car.mDirection = {0,0,1,0};
    car.mVelocity = {0,0,-2,0};
    check(car.GetVelocityDirection().z == 1,"low speed recovery uses facing");
    car.mVelocity = {0,0,-20,0};
    check(near(car.GetVelocityDirection().z,-1),"fast reversing recovery uses velocity");

    AttachmentTruck truck; truck.Construct();
    AttachmentTruck::Parameters tp{4,0.5f};
    truck.Update({0,3,0,0},{0,0,20,0},BrnDirector::VecFloat(0.1f),tp);
    check(near(truck.GetPosition().z,5.2f),"truck starts ahead and converges at reduced speed");
    check(near(truck.GetPosition().y,3),"truck follows subject height");
    check(near(truck.GetVelocity().z,12),"truck velocity reports its own tracking speed");
    truck.Set(BrnDirector::VecFloat(0.5f),{0,0,20,0},{0,0,0,0});
    truck.Update({0,4,0,0},{0,0,20,0},BrnDirector::VecFloat(0.1f),tp);
    check(near(truck.GetPosition().z,1.9f),"planted truck decelerates toward half speed");
    truck.Construct(); truck.Update({4,5,6,0},{0,0,0,0},BrnDirector::VecFloat(0.1f),tp);
    check(near(truck.GetPosition().x,4) && near(truck.GetVelocity().z,0),"stationary truck remains finite");
    BehaviourGyroCam::Parameters params{}; params.Construct();
    check(params.meType == 9 && params.mfSlowDistance == 3 && params.mfFastDistance == 9,"gyro defaults have usable distances");
    check(params.mfField_B0 == 60 && params.mfHeightDistanceVelocityRange == 40,"gyro defaults have FOV and speed range");
    check(params.mLagParams.mfSmoothing == 0.5f && params.mLookerParams.mbInitialiseToLookingAtTarget,"gyro initializes lag and tracking");
    auto v = Utils::SafeSLerp({0,0,1,0},{1,0,0,0},BrnDirector::VecFloat(0.5f));
    check(near(v.x,0.70710678f) && near(v.z,0.70710678f),"camera direction interpolates through quarter turn");
    v = Utils::SafeSLerp({0,0,1,0},{0,0,-1,0},BrnDirector::VecFloat(0.5f));
    check(IsValid(v) && near(Magnitude(v),1),"opposite camera directions do not collapse");
    v = Utils::SafeSLerp({0,0,1,0},{1,0,0,0},BrnDirector::VecFloat(2));
    check(near(v.x,1),"camera interpolation clamps upper amount");
    Matrix44Affine transform; transform.SetIdentity(); float time;
    Utils::PointWillLeaveFrustrum(transform,{0,0,10,0},{2,0,0,0},90,90,&time);
    check(near(time,5),"camera predicts horizontal subject departure");
    Utils::PointWillLeaveFrustrum(transform,{0,0,10,0},{0,-4,0,0},90,90,&time);
    check(near(time,2.5f),"camera predicts vertical subject departure");
    Utils::PointWillLeaveFrustrum(transform,{12,0,10,0},{0,0,0,0},90,90,&time);
    check(time == 0,"offscreen subject departs immediately");
    time = -1;
    check(!Utils::PointWillLeaveFrustrum(transform,{0,0,10,0},{0,0,0,0},90,90,&time) && time == -1, "stationary subject does not leave or overwrite the result");
    // Regression: a car beside a wall must recover onto its current road,
    // with a usable heading even after spinning or changing route.
    BrnAI::AISection roads[3]{};
    BrnAI::Portal portals[2]{};
    portals[1].mPositionZ = 10;
    portals[0].mu16LinkSection = 0; portals[1].mu16LinkSection = 2;
    roads[1].mpaPortals = portals; roads[1].mu8NumPortals = 2;
    roads[1].mpaCorners = corners;
    BrnAI::AISectionsData data{}; data.mpaSections = roads; data.muNumSections = 3;
    reset.mpAISectionData.mpResourceMemory = &data;
    reset.mpaAICars = &car;
    car.meCarState = static_cast<decltype(car.meCarState)>(0);
    car.muBestSectionIndex = 1;
    car.muResetOnTrackSectionIndex = 0x7fff;
    car.mLastGoodPosition = {3,0,5,0}; car.mPosition = car.mLastGoodPosition;
    car.mVelocity = {0,0,0,0}; car.mDirection = {0,0,1,0};
    reset.UpdateResetOnTrackSectionUsingCurrentSection(&car);
    check(car.muResetOnTrackSectionIndex == 1 && car.muResetOnTrackStartPortal == 0 && car.muResetOnTrackEndPortal == 1,
        "recovery tracks the current road and forward portals");
    BrnAI::ResetOnTrackManager::ResetOnTrackCoords coords{};
    check(reset.ComputeInitialCoordinatesStandard(&coords, static_cast<EGlobalRaceCarIndex>(0)) &&
          near(coords.mPosition.x,0) && near(coords.mPosition.z,5) && near(coords.mDirection.z,1),
          "standard recovery moves wall-side car onto the road");
    car.mVelocity = {0,0,-20,0};
    reset.UpdateResetOnTrackSectionUsingCurrentSection(&car);
    check(car.muResetOnTrackStartPortal == 1 && car.muResetOnTrackEndPortal == 0,
          "recovery follows meaningful reverse travel after a spin");
    car.muBestSectionIndex = 0x7fff;
    reset.UpdateResetOnTrackSectionUsingCurrentSection(&car);
    check(car.muResetOnTrackSectionIndex == 1,"missing current section preserves last usable recovery section");
    car.mLastGoodPosition = {300,0,5,0};
    check(!reset.ComputeInitialCoordinatesStandard(&coords, static_cast<EGlobalRaceCarIndex>(0)),
          "recovery refuses a stale road more than 200 metres away");
    auto* route = car.GetRoute(); route->miNodeCount = 2;
    route->meStatus = static_cast<BrnAI::Route::Status>(1);
    auto* nodes = reinterpret_cast<BrnAI::RouteNode*>(route->maNodes);
    nodes[0].muSectionIndex = 0; nodes[1].muSectionIndex = 1; nodes[1].muPad0x0E = 1;
    car.meRouteFindingStyle = static_cast<BrnAI::ERouteFindingStyle>(1); car.miNextRouteNodeIndex = 0;
    car.mbIsPlayer = false;
    check(reset.UpdateResetOnTrackSectionUsingRoute(&car) && car.muResetOnTrackStartPortal == 0 && car.muResetOnTrackEndPortal == 1,
          "rival race recovery follows route even while facing backwards");
    car.mbIsPlayer = true;
    check(!reset.UpdateResetOnTrackSectionUsingRoute(&car),"wrong-way player recovery falls back to current section");
    car.mVelocity = {0,0,20,0};
    check(reset.UpdateResetOnTrackSectionUsingRoute(&car),"forward player route is accepted");
    roads[1].mx8Flags = 2;
    check(!reset.UpdateResetOnTrackSectionUsingRoute(&car),"junction route does not overwrite safe recovery section");
    Utils::SpeedResponder speed; speed.Construct(); speed.Update(100,200);
    check(near(speed.GetSpeedRatio(),0.5f),"camera receives live speed ratio");
    speed.Update(300,200); check(near(speed.GetSpeedRatio(),1),"camera speed ratio saturates");
    Utils::TempCameraBoostResponder boost; boost.Construct(); boost.Update(true,1);
    check(near(boost.GetFOVBoostAmount(),0.5f),"boost FOV ramps through authored sine easing");
    boost.Update(false,0.5f); check(near(boost.GetFOVBoostAmount(),0),"boost FOV releases after boosting stops");
    std::printf("PASS: %d rival recovery and camera checks\n", checks);
}
