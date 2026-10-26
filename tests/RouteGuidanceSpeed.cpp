#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <cstring>

// The fixture injects a telemetry snapshot; it does not run vehicle physics.
void BrnPhysics::Vehicle::RaceCarState::Clear(){std::memset(this,0,sizeof(*this));}

namespace CgsDev { namespace Assert {
static int assertions;
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
} }
namespace BrnStreetData {using RoadIndex=s32;static constexpr s32 KI_INVALID_ROAD_INDEX=-1;}
namespace BrnAI { namespace AIModuleIO {
struct AICarOutputInterface {
    Route route{};s32 current=0;
    const Route* GetPlayerRoute(){return &route;}
    s32 GetPlayerRouteNodeIndex(){return current;}
}; } }
namespace BrnWorld { namespace RaceCarEntityModuleIO {
struct RCEntityActiveRaceCarOutputInterface {
    BrnPhysics::Vehicle::RaceCarState state{};
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex(){return static_cast<EActiveRaceCarIndex>(0);}
    const BrnPhysics::Vehicle::RaceCarState* GetRaceCarState(EActiveRaceCarIndex){return &state;}
}; } }
namespace GameStateModuleIO {struct OutputBuffer {};}
using ScalarVector=rw::math::fpu::Vector3Template<f32>;
static constexpr s32 KI_INTERSTATE_SECTION_1=52,KI_INTERSTATE_SECTION_2=54,KI_INTERSTATE_SECTION_3=48,KI_INTERSTATE_SECTION_4=49;
static constexpr s32 KI_ROUTE_ROAD_PREDICTION_LOOKAHEAD_NODES=13,KI_NUM_END_OF_ROUTE_IGNORE_NODES=2,KI_MIN_NODES_FOR_DECISION=2;
struct Sections {
    BrnAI::AISection sections[8]{};
    const BrnAI::AISection* GetAISection(u16 i){return &sections[i];}
};
struct Guidance {
    Sections data;Sections* mpAISectionData=&data;
    s32 miCurrentPlayerRoadIndex=10,mLeftRoadIndex=20,mRightRoadIndex=21;
    s32 miHopefulLeftPlayerRoadIndex=20,miHopefulRightPlayerRoadIndex=21;
    s32 miOriginalInterStateRoadIndex=-1,miNewRoadNodeIndex=0,miLastRoadNodeIndex=0;
    ScalarVector mCurrentPlayerSectionExitRight{},mLastNode{},mSecondLastNode{};
    bool mbLeftRoadHighlighted=false,mbRightRoadHighlighted=false,mbLockLeftSign=false,mbLockRightSign=false;
    f32 mfGuidanceLockTime=0,distance=150;
    int messages=0;
    s32 roads[8]={10,20,20,20,20,20,20,20};
    s32 GetRoadIndexFromAISectionIndex(u16 i){return roads[i];}
    f32 CalculateDistanceToTurning(BrnAI::AIModuleIO::AICarOutputInterface*,BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*){return distance;}
    void SendUpcomingRoadMessage(GameStateModuleIO::OutputBuffer*,BrnAI::AIModuleIO::AICarOutputInterface*,bool,BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*){++messages;}
    void FindUpcomingStreetsFromRoute(BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*,BrnAI::AIModuleIO::AICarOutputInterface*,GameStateModuleIO::OutputBuffer*,f32);
};
#include "route_guidance_speed.inc"
static int checks,failures;
static void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
static Guidance Run(bool left,f32 speed,f32 top,f32 distance,bool lock=false,f32 lockTime=0){
    Guidance g;BrnAI::AIModuleIO::AICarOutputInterface ai;BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface active;
    g.mCurrentPlayerSectionExitRight.Set(1,0,0);g.distance=distance;
    const s32 road=left?20:21;
    for(int i=0;i<8;++i){g.roads[i]=road;auto& n=ai.route.maNodes[i];n.Construct(float(left?i:-i),float(i),u16(i),0);}
    ai.route.miNodeCount=8;ai.route.meStatus=BrnAI::Route::E_STATUS_COMPLETE;
    active.state.mfSpeedMPH=speed;active.state.mfMaxSpeedMPH=top;
    g.mbLockLeftSign=left&&lock;g.mbLockRightSign=!left&&lock;g.mfGuidanceLockTime=lockTime;
    g.FindUpcomingStreetsFromRoute(&active,&ai,nullptr,1.0f/60.0f);
    return g;
}
int main(){
    for(bool left:{false,true}){
        auto g=Run(left,10,200,150);
        Check(!g.mbLeftRoadHighlighted&&!g.mbRightRoadHighlighted,"slow car does not signal a distant turn based on top speed");
        Check(!g.mbLockLeftSign&&!g.mbLockRightSign,"distant hint does not establish a stale sign lock");
        g=Run(left,100,200,150);
        Check((left?g.mbLeftRoadHighlighted:g.mbRightRoadHighlighted),"current four-second travel range activates matching side");
        Check((left?g.mbLockLeftSign:g.mbLockRightSign),"near hint establishes matching side lock");
        g=Run(left,100,1,150);
        Check((left?g.mbLeftRoadHighlighted:g.mbRightRoadHighlighted),"current speed works independently of low maximum-speed field");
        g=Run(left,0,200,150);
        Check(!g.mbLeftRoadHighlighted&&!g.mbRightRoadHighlighted,"stopped car does not invent a distant turn");
        g=Run(left,0,200,49.9f);
        Check((left?g.mbLeftRoadHighlighted:g.mbRightRoadHighlighted),"original 50-metre near-turn rule retained");
        g=Run(left,0,200,50);
        Check(!g.mbLeftRoadHighlighted&&!g.mbRightRoadHighlighted,"50-metre boundary stays strict");
        g=Run(left,100,200,0);
        Check(!g.mbLeftRoadHighlighted&&!g.mbRightRoadHighlighted,"zero distance suppresses new guidance");
        g=Run(left,0,200,150,true,0.2f);
        Check((left?g.mbLeftRoadHighlighted:g.mbRightRoadHighlighted),"existing one-second lock retained");
        Check(g.messages==1,"one guidance message emitted");
    }
    Check(CgsDev::Assert::assertions==0,"no assertions");
    std::printf("RouteGuidanceSpeed: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
