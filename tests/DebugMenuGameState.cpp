// Whole production bodies are injected by the runner. The route, resource,
// timers and draw surfaces below are boundary spies, not native game layouts.
#include "BrnCommonTypes.h"
#include "pc/math/VmxFloat.h"
#include "SDKs/XboxMath/XMVectorACos.h"
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

static unsigned checks=0,failures=0;
static void Check(bool ok,const char* name) {++checks;if(!ok) {++failures;std::printf("FAIL %s\n",name);}}
static bool Near(f32 a,f32 b) {return std::fabs(a-b)<1e-4f;}
static std::vector<char> calls;
static std::vector<std::string> labels;
struct Arrow {Vector2 from,to;u32 colour;};static std::vector<Arrow> arrows;
namespace CgsDev {
namespace DebugUI {struct Metrics {f32 mfScreenWidth=1280;};}
struct Debug2DImmediateRender {void DrawText(const char* t,f32,f32,f32,u32) {labels.emplace_back(t);calls.push_back('D');}};
struct DebugRender {
    enum {E_JUSTIFY_CENTRE,E_JUSTIFY_RIGHT};
    void Draw2DTextJustified(const char* t,Vector2,int,f32,u32) {labels.emplace_back(t);}
    void Draw2DArrow(Vector2 a,Vector2 b,u32 c) {arrows.push_back({a,b,c});}
};
}
namespace CgsCore {
static void SPrintf(char* out,u32 n,const char* fmt,...) {va_list a;va_start(a,fmt);std::vsnprintf(out,n,fmt,a);va_end(a);}
}
namespace CgsSystem {
struct Time {f32 seconds=0;Time()=default;explicit Time(f32 x):seconds(x) {}s32 GetSeconds() const {return static_cast<s32>(seconds);}};
}
namespace BrnStreetData {
using RoadIndex=s32;
struct Road {CgsID id=0;CgsID GetId() const {return id;}};
struct StreetData {Road road;const Road* GetRoad(RoadIndex) const {return &road;}};
}
namespace BrnTrigger {
struct Box {Vector3 position{};Vector3 GetPosition() const {return position;}};
struct Landmark {Box box;s32 GetDesignIndex() const {return 0;}const Box* GetBoxRegion() const {return &box;}};
struct TriggerData {Landmark landmark;const Landmark* GetLandmarkFromRegionIndex(s32) const {return &landmark;}};
}
using EGlobalRaceCarIndex=s32;
namespace BrnGameState {
namespace GameStateModuleIO {enum {E_MODE_ROAD_RAGE=3};}
struct Timer {
    CgsSystem::Time time;mutable unsigned reads=0;
    CgsSystem::Time GetTime() const {++reads;return time;}
};
struct TimerInterface {Timer timer;Timer* GetSimTimerStatus() {return &timer;}};
struct RoadRageModeScoring {s32 GetNumTakedownsAchieved() const {return 0;}s32 GetTargetNumTakedowns() const {return 1;}};
struct ScoringSystem {
    f32 limit=110,expiredInput=-1,remainingInput=-1;bool active=true;RoadRageModeScoring rage;
    bool HasModeTimeExpired(CgsSystem::Time t) {expiredInput=t.seconds;return t.seconds>=limit;}
    bool IsTimeLimitActive() const {return active;}
    CgsSystem::Time GetModeTimeRemaining(CgsSystem::Time t) {remainingInput=t.seconds;return CgsSystem::Time(limit-t.seconds);}
    const RoadRageModeScoring* GetRoadRageScoring() const {return &rage;}
};
struct ModeManager {
    TimerInterface mTimerStatusInterface;ScoringSystem scoring;bool current=true;u32 muNumLandmarks=1;s16 maLandmarkIndices[1]{0};
    ScoringSystem* GetScoringSystem() {return &scoring;}
    s32 GetCurrentGameModeType() const {return -1;}
    void* GetCurrentGameMode() {return current?this:nullptr;}
    s32 GetNextLandmarkIndex(EGlobalRaceCarIndex) const {return 0;}
};
struct StreetManager {
    BrnStreetData::RoadIndex mLeftRoadIndex=-1,mRightRoadIndex=-1;
    BrnStreetData::StreetData data;bool available=false;unsigned acquisitions=0,failedAcquisitions=0;
    const BrnStreetData::StreetData* GetStreetData() {
        ++acquisitions;if(!available)++failedAcquisitions;calls.push_back('A');
        // Preserve the observable failed-acquisition witness without crashing
        // the fixture; neither invalid-road arm dereferences the returned data.
        return &data;
    }
};
struct GameStateModule {
    ModeManager mode;StreetManager street;TimerInterface mTimerStatusInterface;
    Matrix44Affine mPlayerCarTransform{};BrnTrigger::TriggerData trigger;
    ModeManager* GetModeManager() {return &mode;}StreetManager* GetStreetManager() {return &street;}
    s32 GetPlayerGlobalRaceCarIndex() const {return 0;}
    const BrnTrigger::TriggerData* GetTrackTriggerData() const {return &trigger;}
};
struct GameStateDebugComponent {
    static const s32 KI_SCORE_TEXT_LENGTH=256;char macScoreText[KI_SCORE_TEXT_LENGTH];
    GameStateModule* mpGameStateModule=nullptr;
    bool mbDrawMiscInfo=false,mbShowUpComingRoadRules=false,mbDrawStuntBreakdown=false,mbDebugThisLandmark=false;
    s32 FindRoadNameIndexFromId(CgsID);
    void DrawUpcomingRoads(CgsDev::Debug2DImmediateRender*);
    void RenderHUD(CgsDev::Debug2DImmediateRender*);
    void RenderNextLandmarkInfo(CgsDev::DebugRender&,const CgsDev::DebugUI::Metrics&) const;
    void DrawStuntBreakdown(CgsDev::Debug2DImmediateRender*) {} // unselected boundary
};
}
#include "debug_menu_gamestate.inc"

static void Landmark() {
    using namespace BrnGameState;GameStateModule m;GameStateDebugComponent d;d.mpGameStateModule=&m;m.mPlayerCarTransform.SetIdentity();
    CgsDev::DebugRender render;CgsDev::DebugUI::Metrics metrics;const f32 cx=metrics.mfScreenWidth-80;
    m.trigger.landmark.box.position={1,0,0,0};arrows.clear();d.RenderNextLandmarkInfo(render,metrics);
    Check(arrows.size()==1 && Near(arrows[0].from.x,cx+30) && Near(arrows[0].to.x,cx-30),"F1 positive right-dot points toward decreasing screen X");
    Check(arrows.size()==1 && Near(arrows[0].from.y,60) && Near(arrows[0].to.y,60) && arrows[0].colour==0xFF00FFFF,"landmark Y orientation and colour retained");
    m.trigger.landmark.box.position={0,1,0,0};arrows.clear();d.RenderNextLandmarkInfo(render,metrics);
    Check(arrows.size()==1 && Near(arrows[0].from.x,cx) && Near(arrows[0].to.x,cx),"F1 zero right-dot has a distinct zero projection");
    m.trigger.landmark.box.position={-1,0,0,0};arrows.clear();d.RenderNextLandmarkInfo(render,metrics);
    Check(arrows.size()==1 && Near(arrows[0].from.x,cx-30) && Near(arrows[0].to.x,cx+30),"F1 negative right-dot mirrors only horizontal projection");
    m.trigger.landmark.box.position={1e-40f,1,0,0};arrows.clear();d.RenderNextLandmarkInfo(render,metrics);
    Check(arrows.size()==1 && Near(arrows[0].from.x,cx) && Near(arrows[0].to.x,cx),"F1 original displacement-dot flushes subnormal side operand");
    m.mode.current=false;arrows.clear();d.RenderNextLandmarkInfo(render,metrics);Check(arrows.empty(),"no current mode leaves landmark renderer untouched");
}
static void Roads() {
    using namespace BrnGameState;GameStateModule m;GameStateDebugComponent d;d.mpGameStateModule=&m;CgsDev::Debug2DImmediateRender render;
    calls.clear();labels.clear();d.DrawUpcomingRoads(&render);
    Check(m.street.acquisitions==0 && m.street.failedAcquisitions==0,"F2 invalid roads never acquire absent resource");
    Check(labels==std::vector<std::string>({"_","_"}) && calls==std::vector<char>({'D','D'}),"invalid roads retain both fallback draws");
    m.street.available=true;m.street.mLeftRoadIndex=0;m.street.acquisitions=0;calls.clear();d.DrawUpcomingRoads(&render);
    Check(m.street.acquisitions==1 && calls==std::vector<char>({'A','D','D'}),"F2 left-road acquisition occurs before left draw only");
    m.street.mRightRoadIndex=0;m.street.acquisitions=0;calls.clear();d.DrawUpcomingRoads(&render);
    Check(m.street.acquisitions==2 && calls==std::vector<char>({'A','D','A','D'}),"F2 separate right lookup follows left draw");
}
static void Timers() {
    using namespace BrnGameState;GameStateModule m;GameStateDebugComponent d;d.mpGameStateModule=&m;d.mbDrawMiscInfo=true;CgsDev::Debug2DImmediateRender render;
    m.mode.mTimerStatusInterface.timer.time=CgsSystem::Time(100);m.mTimerStatusInterface.timer.time=CgsSystem::Time(101);
    labels.clear();d.RenderHUD(&render);
    Check(m.mode.scoring.expiredInput==100 && m.mode.scoring.remainingInput==101,"F3 expiry and remaining-time queries retain distinct caches");
    Check(labels==std::vector<std::string>({"Time remaining 0.9"}),"F3 refreshed module cache yields nine seconds");
    m.mode.scoring.limit=99;m.mTimerStatusInterface.timer.reads=0;labels.clear();d.RenderHUD(&render);
    Check(m.mTimerStatusInterface.timer.reads==0 && labels==std::vector<std::string>({"Come in number 5, your time is up!"}),"expired mode does not acquire unused module Time");
}
int main() {Landmark();Roads();Timers();std::printf("DebugMenuGameState: %u checks, %u failures\n",checks,failures);return failures?1:0;}
