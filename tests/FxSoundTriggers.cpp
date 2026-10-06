#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <memory>
#include "types.hpp"
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerQueryId.h"
int assertions=0;
#define CGS_ASSERT(c,m) do { if(!(c)) ++assertions; } while(0)
struct Vector3 { float x=0,y=0,z=0,w=0; };
Vector3 operator-(const Vector3&a,const Vector3&b){return {a.x-b.x,a.y-b.y,a.z-b.z,a.w-b.w};}
struct EntityId{u32 muValue=0;};
enum EActiveRaceCarIndex{E_ACTIVE_RACE_CAR_INDEX_0=0,E_ACTIVE_RACE_CAR_INDEX_COUNT=8};
enum EGlobalRaceCarIndex{E_GLOBAL_RACE_CAR_INDEX_0=0};
using CgsID=u64;
using LightTriggerId=u32;
namespace CgsDev {struct PerfMonCpu {static void StartMonitor(int){}static void StopMonitor(int){}};}
static int gsiPostWorldUpdatePM=0;
template<class T,int N> struct Array {
    std::vector<T> a; void Construct(){a.clear();}void Clear(){a.clear();}
    u32 GetLength()const{return (u32)a.size();}
    auto& operator[](u32 i){return a.at(i);}const auto& operator[](u32 i)const{return a.at(i);}
    void Append(const T&v){a.push_back(v);}
};
struct PackedIndex {int g,a;void SetGlobalRaceCarIndex(int n){g=n;}void SetActiveRaceCarIndex(int n){a=n;}int GetPackedRaceCarIndex()const{return (a<<8)+g;}};
namespace CgsModule {struct Event{};}
namespace BrnTrigger {
struct TriggerRegion {enum{E_TYPE_LANDMARK=0,E_TYPE_GENERIC_REGION=2};int category=2;int GetType()const{return category;}};
struct GenericRegion:TriggerRegion {enum{E_TYPE_JUMP=7,E_TYPE_ROAD_LIMIT=11};int type=19;CgsID group=0,id=0;
    int GetType()const{return type;}CgsID GetGroupId()const{return group;}CgsID GetId()const{return id;}};
struct Data{GenericRegion a[20];const TriggerRegion*GetRegion(u32 n)const{return &a[n];}};
}
namespace BrnWorld {namespace TriggerEntityModuleIO {
struct InLineTestEvent:CgsModule::Event {TriggerQueryId mQueryID;u8 mTriggerTypeFlags;Vector3 mLineStart,mLineEnd;};
struct OutLineTestResultEvent:CgsModule::Event {
    TriggerQueryId mQueryID;int miNumTriggers;std::vector<u32> hits;
    const u32*GetTriggerIds()const{return hits.data();}
};
}}
namespace BrnGameState {namespace GameStateModuleIO {
struct SoundTriggerAction {
    enum eType{E_TYPE_INVALID,E_TYPE_AT_ENTITY,E_TYPE_AHEAD_OF_ENTITY};
    Vector3 mQueryPos;EntityId mEntityId;eType meResultType=E_TYPE_INVALID;u32 muActiveTriggers=0;
    bool IsEmpty(){return mQueryPos.x==0&&mQueryPos.y==0&&mQueryPos.z==0&&mEntityId.muValue==0&&meResultType==E_TYPE_INVALID&&muActiveTriggers==0;}
};
struct OutputBuffer {
    struct Queries {std::vector<BrnWorld::TriggerEntityModuleIO::InLineTestEvent> a;int bad=0;
        bool AddEvent(const BrnWorld::TriggerEntityModuleIO::InLineTestEvent*e,int id){bad+=id!=3;a.push_back(*e);return true;}} queries;
    struct Actions {std::vector<SoundTriggerAction>a;int bad=0;
        bool AddEvent(const SoundTriggerAction*e,int id){bad+=id!=218;a.push_back(*e);return true;}} actions;
    auto*GetTriggerQueryInputInterface(){return &queries;}auto*GetGameActionQueue(){return &actions;}
};
}}
struct RCEntityActiveRaceCarOutputInterface {
    struct Car {Vector3 mPosition,mPreviousPosition,mDirection;int meActiveRaceCarIndex=0,meGlobalRaceCarIndex=0;bool mbIsPlayer=true;};
    struct State {struct Transform{Vector3 p;Vector3 Pos()const{return p;}}mTransform;EntityId mEntityId;};
    Array<Car,8>maCarsInTheRace;State states[8];bool active[8]={};int player=0;
    bool IsRaceCarActive(EActiveRaceCarIndex i)const{return i>=0&&i<8&&active[i];}
    bool IsPlayerCarActive()const{return player>=0&&active[player];}
    const State*GetRaceCarState(EActiveRaceCarIndex i)const{return &states[i];}
    const State*GetPlayerRaceCarState()const{return &states[player];}
};
namespace BrnGameState {namespace GameStateModuleIO {
struct PostWorldInputBuffer {
    RCEntityActiveRaceCarOutputInterface* cars;
    struct Results {
        using R=BrnWorld::TriggerEntityModuleIO::OutLineTestResultEvent;
        std::vector<std::unique_ptr<R>> a;int kind=1;
        int GetFirstEvent(const CgsModule::Event**e,int*n)const{*e=a.empty()?nullptr:a[0].get();*n=8;return kind;}
        int GetNextEvent(const CgsModule::Event*e,const CgsModule::Event**next,int*n)const{
            for(size_t i=0;i<a.size();++i)if(a[i].get()==e){*next=i+1<a.size()?a[i+1].get():nullptr;break;}*n=8;return kind;}
        void Add(u32 query,std::initializer_list<u32>hits){auto r=std::make_unique<R>();r->mQueryID.Set((u8)(query>>24),query);r->hits=hits;r->miNumTriggers=(int)hits.size();a.push_back(std::move(r));}
    } results;
    const auto*GetTriggerEntityOutputInterface()const{return &results;}
    const auto*GetActiveRaceCarOutputInterface()const{return cars;}
};
}
using LandmarkIndex=s16;
struct ModeManager {
    struct Hit {int global,active,landmark;bool player;};
    std::vector<Hit> hits;bool online=false,atStart=true;int clears=0;
    void ClearModeStartRegion(){atStart=false;++clears;}
    bool IsOnlineGameMode()const{return online;}
    void RaceCarTriggersLandmark(const RCEntityActiveRaceCarOutputInterface*,EGlobalRaceCarIndex g,
        EActiveRaceCarIndex a,LandmarkIndex l,bool p){hits.push_back({int(g),int(a),int(l),p});atStart=true;}
};
struct TriggerQueryManager {
    Array<GameStateModuleIO::SoundTriggerAction,16>maSoundActions;
    Vector3 maActiveRaceCarPosLastFrame[8],mPlayerLookAheadPos;
    bool mbCarHasTeleported=true,mbDoSoundLookAheadThisFrame=true;
    GameStateModuleIO::SoundTriggerAction mCachedLookAheadSoundAction;
    BrnTrigger::Data*mpTriggerData;
    Array<u16,32>maLastPlayerTriggers;
    bool mbPlayerInTrafficLightRegion=true;
    LightTriggerId mPlayerCurrentTrafficLightId=0;
    CgsID mPlayerSigTakedownGroupID=1,mPlayerSuperJumpGroupID=2,mPlayerRoadLimitGroupID=3;
    void SubmitTriggerQueries(GameStateModuleIO::OutputBuffer*,const RCEntityActiveRaceCarOutputInterface*);
    void CacheSoundQueryPositions(const RCEntityActiveRaceCarOutputInterface*);
    void PostSoundActions(GameStateModuleIO::OutputBuffer*);
    bool IsSoundActionPresent(EntityId,GameStateModuleIO::SoundTriggerAction::eType)const;
    void CheckSoundActions(const RCEntityActiveRaceCarOutputInterface*);
    void PostWorldUpdate(const GameStateModuleIO::PostWorldInputBuffer*,ModeManager*,EActiveRaceCarIndex);
};
#include "fx_sound_triggers.inc"
}
int main(){
    using namespace BrnGameState;using namespace GameStateModuleIO;
    using Car=RCEntityActiveRaceCarOutputInterface::Car;
    int n=0,f=0;auto ck=[&](bool ok,const char*s){++n;if(!ok){++f;printf("FAIL %s\n",s);}};
    const auto slot=[](int i){return static_cast<EActiveRaceCarIndex>(i);};
    RCEntityActiveRaceCarOutputInterface cars;cars.active[0]=cars.active[3]=true;
    cars.states[0].mEntityId.muValue=101;cars.states[3].mEntityId.muValue=303;
    cars.states[0].mTransform.p={10,20,30,0};cars.states[3].mTransform.p={40,50,60,0};
    BrnTrigger::Data data;TriggerQueryManager m{};m.mpTriggerData=&data;ModeManager mode;
    OutputBuffer output;PostWorldInputBuffer input;input.cars=&cars;
    Car car;car.mPosition={100,10,50,0};car.mPreviousPosition={99,10,50,0};car.mDirection={20,0,0,0};
    car.meGlobalRaceCarIndex=17;cars.maCarsInTheRace.Append(car);
    m.SubmitTriggerQueries(&output,&cars);
    ck(output.queries.a.size()==2,"motion plus ahead query");
    if(output.queries.a.size()>=2){
        const auto&a=output.queries.a[0];const auto&b=output.queries.a[1];
        ck((u32)a.mQueryID==0x38000011,"owner and slot encoding");
        ck(a.mLineStart.x==99&&a.mLineEnd.x==100,"previous to current segment");
        ck(a.mTriggerTypeFlags==4&&b.mTriggerTypeFlags==4,"box volume mask");
        ck((u32)b.mQueryID==0x380003e7,"ahead marker");
        ck(b.mLineStart.x==105&&b.mLineEnd.x==109,"quarter second plus four metre ray");
    }else for(int i=0;i<5;++i)ck(false,"query data absent");
    ck(!m.mbCarHasTeleported&&!m.mbDoSoundLookAheadThisFrame,"eligible flags advance");
    output.queries.a.clear();m.SubmitTriggerQueries(&output,&cars);
    ck(output.queries.a.size()==1&&m.mbDoSoundLookAheadThisFrame,"alternating ahead submission");
    cars.maCarsInTheRace[0].mDirection={100,0,0,0};output.queries.a.clear();m.SubmitTriggerQueries(&output,&cars);
    ck(m.mPlayerLookAheadPos.x==115,"ahead distance capped at15m");
    cars.maCarsInTheRace[0].mPreviousPosition.x=90;output.queries.a.clear();const bool before=m.mbDoSoundLookAheadThisFrame;
    m.SubmitTriggerQueries(&output,&cars);ck(output.queries.a.empty()&&m.mbCarHasTeleported&&m.mbDoSoundLookAheadThisFrame==before,"10m teleport discards without toggling");
    cars.maCarsInTheRace[0].mPreviousPosition.x=std::numeric_limits<float>::quiet_NaN();
    m.SubmitTriggerQueries(&output,&cars);ck(output.queries.a.empty()&&m.mbCarHasTeleported,"NaN displacement discards");
    cars.maCarsInTheRace[0]=car;cars.maCarsInTheRace[0].mDirection={};m.mbDoSoundLookAheadThisFrame=true;
    m.SubmitTriggerQueries(&output,&cars);
    ck(output.queries.a.size()==2&&std::isnan(output.queries.a.back().mLineEnd.x),"zero velocity keeps original unguarded normalization");
    m.CacheSoundQueryPositions(&cars);ck(m.maActiveRaceCarPosLastFrame[3].x==40&&m.maActiveRaceCarPosLastFrame[0].y==20,"active car position cache");
    m.maSoundActions.Clear();m.CheckSoundActions(&cars);
    ck(m.maSoundActions.GetLength()==3,"zero-overlap actions for both cars and player ahead");
    ck(m.IsSoundActionPresent({303},SoundTriggerAction::E_TYPE_AT_ENTITY),"entity match");
    ck(!m.IsSoundActionPresent({303},SoundTriggerAction::E_TYPE_AHEAD_OF_ENTITY),"query kind also matches");
    m.CheckSoundActions(&cars);ck(m.maSoundActions.GetLength()==3,"no duplicate empty actions");
    m.PostSoundActions(&output);ck(output.actions.a.size()==3&&m.maSoundActions.GetLength()==0&&output.actions.bad==0,"forward218 then clear");
    data.a[0].type=19;data.a[1].type=31;data.a[2].type=18;data.a[3].type=32;data.a[4].type=25;
    input.results.Add(0x38000011,{0x38000000,0x38000001,0x38000002,0x38000003,0x39000001});
    input.results.Add(0x380003e7,{0x38000004});m.mbCarHasTeleported=false;m.mbDoSoundLookAheadThisFrame=false;
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.maSoundActions.GetLength()==3,"hits plus missing rival result");
    if(m.maSoundActions.GetLength()>=2){
        ck(m.maSoundActions[0].muActiveTriggers==4097,"sound types19 through31 only");
        ck(m.maSoundActions[0].mEntityId.muValue==101&&m.maSoundActions[0].mQueryPos.x==10,"hit carries car identity and saved position");
        ck(m.maSoundActions[1].muActiveTriggers==64&&m.maSoundActions[1].meResultType==SoundTriggerAction::E_TYPE_AHEAD_OF_ENTITY,"look-ahead has separate sound bits");
    }else for(int i=0;i<3;++i)ck(false,"sound hit absent");
    ck(m.mCachedLookAheadSoundAction.muActiveTriggers==64,"ahead result retained for alternate frame");
    m.maSoundActions.Clear();input.results.a.clear();m.mbDoSoundLookAheadThisFrame=true;m.mPlayerLookAheadPos={200,3,4,0};
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.maSoundActions.GetLength()==3&&m.maSoundActions[0].muActiveTriggers==64&&m.maSoundActions[0].mQueryPos.x==200,"cached ahead replay updates position");
    ck(m.mCachedLookAheadSoundAction.IsEmpty(),"replay clears cached action");
    m.maSoundActions.Clear();m.PostWorldUpdate(&input,&mode,slot(0));
    bool empty=m.maSoundActions.GetLength()==3;for(auto&a:m.maSoundActions.a)empty &=a.muActiveTriggers==0;
    ck(empty,"leaving region clears all bits");
    m.mCachedLookAheadSoundAction.meResultType=SoundTriggerAction::E_TYPE_AHEAD_OF_ENTITY;
    m.mCachedLookAheadSoundAction.muActiveTriggers=64;m.mbCarHasTeleported=true;m.maSoundActions.Clear();
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.mCachedLookAheadSoundAction.muActiveTriggers==64,"teleport does not replay or clear cached ahead");
    m.mbDoSoundLookAheadThisFrame=false;m.maSoundActions.Clear();input.results.Add(0x38000311,{0x38000000});
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.maSoundActions.GetLength()>=1&&m.maSoundActions[0].mEntityId.muValue==303,"active index comes from bits8..15");
    ck(assertions==0&&output.queries.bad==0,"valid flows have no asserts or wrong event types");
    // ARTIST82386D90..DA8 distinguishes owner, global slot and active slot.
    // The sound-only control still passes all checks above but drops these hits.
    auto reset=[&](){input.results.a.clear();m.maLastPlayerTriggers.Clear();m.maSoundActions.Clear();
        mode.hits.clear();m.mCachedLookAheadSoundAction={};m.mbDoSoundLookAheadThisFrame=false;};
    reset();data.a[0].type=11;data.a[0].id=396111;data.a[1].type=7;data.a[1].id=9;data.a[1].group=4030;
    data.a[2].category=0;
    input.results.Add(0x38000011,{0x38000000,0x38000001,0x38000002,0x39007a08});
    input.results.Add(0x38000327,{0x38000002});
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.maLastPlayerTriggers.a==std::vector<u16>({0,1,2}),"all player region hits retained in query order");
    ck(m.mPlayerRoadLimitGroupID==396111,"road-limit ID falls back to region ID");
    ck(m.mPlayerSuperJumpGroupID==4030,"jump uses group ID");
    ck(m.mPlayerSigTakedownGroupID==0,"unused signature group clears each frame");
    ck(m.mbPlayerInTrafficLightRegion&&m.mPlayerCurrentTrafficLightId==0x39007a08,"actual light hit carries full packed handle");
    ck(mode.hits.size()==2&&mode.hits[0].global==17&&mode.hits[0].active==0&&mode.hits[0].player,
       "player landmark uses packed global and active slots");
    ck(mode.hits.size()==2&&mode.hits[1].global==39&&mode.hits[1].active==3&&!mode.hits[1].player,
       "offline AI landmark reaches mode");
    reset();mode.online=true;input.results.Add(0x38000327,{0x38000002,0x39000203});
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(mode.hits.empty()&&m.maLastPlayerTriggers.GetLength()==0,"online AI cannot trigger player or landmark handlers");
    ck(!m.mbPlayerInTrafficLightRegion&&m.mPlayerCurrentTrafficLightId==0xffffffff,"AI light cannot set player's junction");
    input.results.a.clear();input.results.Add(0x38000011,{0x38000002});
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(mode.hits.size()==1&&mode.hits[0].player,"online player landmark remains enabled");
    reset();input.results.Add(0x380003e7,{0x38000000,0x38000001,0x38000002,0x39007a08});
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.maLastPlayerTriggers.GetLength()==0&&mode.hits.empty()&&!m.mbPlayerInTrafficLightRegion,
       "look-ahead query never delivers gameplay or light hits");
    ck(m.mPlayerRoadLimitGroupID==0&&m.mPlayerSuperJumpGroupID==0,"look-ahead never changes gameplay groups");
    reset();input.results.Add(0x37000011,{0x38000000,0x39007a08});
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.maLastPlayerTriggers.GetLength()==0&&!m.mbPlayerInTrafficLightRegion,"unowned query ignored");
    reset();cars.active[0]=false;input.results.Add(0x38000011,{0x38000000});
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(m.maLastPlayerTriggers.GetLength()==1&&m.mPlayerRoadLimitGroupID==396111,
       "gameplay result survives entity becoming inactive after query submission");
    cars.active[0]=true;reset();mode.atStart=true;
    m.PostWorldUpdate(&input,&mode,slot(0));
    ck(!mode.atStart&&!m.mbPlayerInTrafficLightRegion&&m.mPlayerRoadLimitGroupID==0&&m.mPlayerSuperJumpGroupID==0,
       "empty result frame clears start region and trigger groups");
    ck(assertions==0,"all valid gameplay result cases remain assertion free");
    printf("FxSoundTriggers: %d checks, %d failures\n",n,f);return f?1:0;
}
