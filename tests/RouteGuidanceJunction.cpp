// Issue #70: production junction traversal and guidance at the reported Lambert/Newton junction.
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
// Expose only the serialized portal fields to initialize the bounded map fixture.
#define private public
#include "GameSource/World/AI/BrnAIPortal.h"
#undef private
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
static int giAsserts;
namespace CgsDev { namespace Assert {int BeginAssert(){return 0;} int FireAssert(const char* x,const char*,int){++giAsserts;printf("ASSERT %s\n",x);return 0;} void* EndAssert(){return nullptr;}}
namespace PerfMonCpu {s32 AddMonitor(const char*,PerfMonCpuPage,bool,f32,bool){return -1;} void StartMonitor(s32){} void StopMonitor(s32){}}
namespace Log {decltype(gpDebugPrint) gpDebugPrint=nullptr;}}
u64 CgsDev::Message::gxMessageFilterFlags=0;

void BrnPhysics::Vehicle::RaceCarState::Clear(){std::memset(this,0,sizeof(*this));}
namespace BrnAI { namespace AIModuleIO {struct AICarOutputInterface {
    Route route{}; s32 index=0;
    const Route* GetPlayerRoute() const{return &route;}
    s32 GetPlayerRouteNodeIndex() const{return index;}
};}}
namespace BrnWorld { namespace RaceCarEntityModuleIO {struct RCEntityActiveRaceCarOutputInterface {
    BrnPhysics::Vehicle::RaceCarState state{};
    Vector3 pos{},dir{};
    Vector3 GetPlayerPosition()const{return pos;}
    Vector3 GetPlayerDirection()const{return dir;}
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex()const{return static_cast<EActiveRaceCarIndex>(0);}
    const BrnPhysics::Vehicle::RaceCarState* GetPlayerRaceCarState()const{return &state;}
    const BrnPhysics::Vehicle::RaceCarState* GetRaceCarState(EActiveRaceCarIndex)const{return &state;}
};}}
struct StreetTableFixture {
    struct Entry {s32 road=-1;s32 GetRoadIndex() const{return road;}};
    std::vector<Entry> streets;
    s32 GetStreetCount() const{return static_cast<s32>(streets.size());}
    const Entry* GetStreet(s16 index) const{return &streets.at(index);}
};
struct SignalMessage {
    s32 miCurrentRoadHighlightState=-1,miLeftRoadHighlightState=-1,miRightRoadHighlightState=-1;
    u64 mLeftRoadId=1,mRightRoadId=1;
    f32 mfLeftSignValue=0,mfRightSignValue=0;
};
template<class T> struct DataRef {T* ptr;T* operator->()const{return ptr;}};
namespace BrnGameState {
struct Guidance {
    DataRef<BrnAI::AISectionsData> mpAISectionData;
    DataRef<StreetTableFixture> mpStreetData;
    s32 miCurrentPlayerRoadIndex=-1,miLastPlayerRoadIndex=-1,mLeftRoadIndex=-1,mRightRoadIndex=-1;
    s32 miHopefulForwardPlayerRoadIndex=-1,miHopefulLeftPlayerRoadIndex=-1,miHopefulRightPlayerRoadIndex=-1;
    s32 miNewestForwardPlayerRoadIndex=-1,miNewestLeftPlayerRoadIndex=-1,miNewestRightPlayerRoadIndex=-1;
    s32 miOriginalInterStateRoadIndex=-1,miNewRoadNodeIndex=0,miLastRoadNodeIndex=0,miNextFreeSectionSlot=0;
    rw::math::fpu::Vector3Template<f32> mCurrentPlayerSectionExitRight{},mFirstJunctionPosition{},mLastNode{},mSecondLastNode{};
    f32 mfHopefulRoadTimer=0,mfGuidanceLockTime=0;
    bool mbLeftRoadHighlighted=false,mbRightRoadHighlighted=false,mbLockLeftSign=false,mbLockRightSign=false;
    bool mbPlayerOnInterstateExit=false,mbJunctionPosSet=false,mbNeedToUpdateUpcomingRoads=false;
    u16 mauWalkedSections[KI_MAX_SECTIONS_TO_WALK]{};
    s32 miUpcomingRoadsQSortPM=-1;
    StreetTableFixture* GetStreetData(){return mpStreetData.ptr;}
    SignalMessage mMessage;
    void SendUpcomingRoadMessage(GameStateModuleIO::OutputBuffer*,BrnAI::AIModuleIO::AICarOutputInterface*,bool lbForce,BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*) {
        SignalMessage lMessage;
#include "junction_signal.inc"
        mMessage=lMessage;
    }
#include "junction_declarations.inc"
};
}
#include "junction_methods.inc"

struct RouteCase { const char* name; int turn; BrnAI::Route route; };
struct GeometryFixture {
    BrnAI::AISectionsData data{};
    StreetTableFixture streets;
    std::vector<BrnAI::AISection> sections;
    std::vector<std::array<BrnAI::AISection::Vector2,4>> corners;
    std::vector<std::vector<BrnAI::Portal>> portals;
    std::vector<RouteCase> cases;
    Vector3 position{},direction{};
    u16 startSection=0;
    s32 currentRoad=-1,leftRoad=-1,rightRoad=-1;
    GeometryFixture() {
#include "junction_geometry.inc"
    }
};
static int giChecks,giFailures;
static void Check(bool pass,const char* message) {
    ++giChecks;
    if(!pass){++giFailures;printf("FAIL %s\n",message);}
}

int main() {
    GeometryFixture fixture;
    const float speeds[]={0,25,100,180};
    const float offsets[]={-.25f,0,.25f};
    for(const auto& routeCase:fixture.cases) for(float speed:speeds) for(float offset:offsets) {
        BrnGameState::Guidance guidance;
        guidance.mpAISectionData.ptr=&fixture.data;guidance.mpStreetData.ptr=&fixture.streets;
        guidance.miCurrentPlayerRoadIndex=fixture.currentRoad;
        BrnAI::AIModuleIO::AICarOutputInterface ai;ai.route=routeCase.route;
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface active;
        active.pos=fixture.position;active.pos.x+=offset;active.dir=fixture.direction;
        active.state.mTransform.SetIdentity();
        active.state.mTransform.xAxis={active.dir.z,0,-active.dir.x,0};
        active.state.mTransform.wAxis=active.pos;active.state.mfMaxSpeedMPH=200;
        Check(fixture.sections[fixture.startSection].IsInside(active.pos.x,active.pos.z),"pose remains in the reported approach section");
        for(int frame=0;frame<120;++frame) {
            // Stabilize the road labels, observe an approach-speed snapshot, then stop.
            active.state.mfSpeedMPH=frame==30?speed:0;
            guidance.FindUpcomingStreetsByRecursiveWalking(&active,nullptr,&ai,1.0f/60,fixture.startSection,false);
            guidance.FindUpcomingStreetsFromRoute(&active,&ai,nullptr,1.0f/60);
            if(frame>=30) {
                Check(guidance.mLeftRoadIndex==fixture.leftRoad && guidance.mRightRoadIndex==fixture.rightRoad,
                      "immediate side exits win over bends in the forward road");
                Check(guidance.mbLeftRoadHighlighted==(routeCase.turn==1) && guidance.mbRightRoadHighlighted==(routeCase.turn==2),
                      routeCase.name);
                // The vehicle's original action-276 handler lights the side whose state is 2.
                Check((guidance.mMessage.miLeftRoadHighlightState==2)==(routeCase.turn==1)
                       && (guidance.mMessage.miRightRoadHighlightState==2)==(routeCase.turn==2),
                      "emitted guidance preserves straight / left / right indicator choice after stopping");
            }
        }
    }
    BrnGameState::SectionWalkData first{},second{};
    first.mfEntryAngle=.8f;second.mfEntryAngle=-.8f;
    Check(BrnGameState::CompareSectionWalkData(&first,&second)==-1,"equal magnitudes retain ARTIST's -1 comparator result");
    Check(giAsserts==0,"no assertions");
    printf("RouteGuidanceJunction: %d checks, %d failures\n",giChecks,giFailures);
    return giFailures?1:0;
}
