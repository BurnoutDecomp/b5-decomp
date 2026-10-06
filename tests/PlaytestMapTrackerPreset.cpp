#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include <cstdio>
#include <cstring>
#include <vector>
static unsigned guChecks, guFailures, guAsserts;
#undef CGS_ASSERT
#define CGS_ASSERT(test,msg) do {if(!(test)) ++guAsserts;} while(0)
struct FixtureRecord {s32 id; std::vector<u8> bytes;};
struct FixtureQueue {
    std::vector<FixtureRecord> records;
    bool AddEvent(const CgsModule::Event* p,s32 id,s32 size) {
        auto bytes=reinterpret_cast<const u8*>(p);
        records.push_back({id,std::vector<u8>(bytes,bytes+size)});return true;
    }
};
namespace BrnGame {
template<class T> void PushGuiEvent(const T& event,FixtureQueue* q) {
    q->AddEvent(reinterpret_cast<const CgsModule::Event*>(&event),event.GetEventType(),sizeof(T));
}
}
namespace BrnProgression {
struct ProgressionManager {
    std::vector<Race> maPresetRaces;
    u32 muNumPresetRaces=0;
    u32 GetRacesAtLandmark(Race*,u32,BrnGameState::LandmarkIndex,bool) const;
};
static void FireConsoleAssert(const char*,s32) {++guAsserts;}
}
struct FixtureEventQueue {
    std::vector<s32> types;
    CgsModule::Event marker[4];
    s32 GetFirstEvent(const CgsModule::Event** p,s32* size) const {
        *size=1;*p=types.empty()?nullptr:&marker[0];return types.empty()?-1:types[0];
    }
    s32 GetNextEvent(const CgsModule::Event* prev,const CgsModule::Event** p,s32* size) const {
        auto i=static_cast<size_t>(prev-marker)+1;*size=1;
        *p=i<types.size()?&marker[i]:nullptr;return i<types.size()?types[i]:-1;
    }
};
namespace BrnAI {namespace RouteMapModuleIO {enum {E_OWNER_GUI=1};}}
namespace BrnGameState {
struct FixtureModeManager {
    LandmarkIndex mPlayerCurrentLandmark{10};
    LandmarkIndex GetPlayerCurrentLandmark() const;
};
struct GameStateModule {
    FixtureModeManager mModeManager;
    BrnProgression::ProgressionManager mProgressionManager;
    s32 routeCalls=0;
    void SendSetLandmarkRacesAction(FixtureQueue*);
    void ProcessGameEventsLandmarkRouteRequestBringUp(const FixtureEventQueue*,FixtureQueue*);
    void SendRouteRequestAction(const GameStateModuleIO::LandmarkRouteRequestEvent*,FixtureQueue*,s32 owner) {
        if(owner==1) ++routeCalls;
    }
};
}
namespace BrnGui {
struct GuiCache {
    s32 meGameModeType=0, meCurrentMedalTarget=0, miOnlineRoundIndex=0, miSatNavZoomLevel=9;
    u32 muEventID=0,muJunctionID=0;
    bool mbOnlineStartInProgress=false,mbOnlineTimeoutPending=false,mbEventPreparedForModeStart=false;
    f32 mfEventTime=0,mfTargetTime=0,mafTargetScores[4]={},mfDistanceInEvent=0,mfDistanceDriven=0;
    s32 miPursuitRivalDamageLeft_9FE8=0,miTakedownsCurrent=0,miScoreCurrent=0,miScoreTarget=0;
    s32 miScoreCombo=0,miComboMultiplier=0,miPursuitRivalTotalDamage=0,miCheckpointReached=0;
    CgsID mPursuedCarID=0;
    u8 muCheckpointsInEvent=0;
    s32 miTakedownTarget=0;
    s8 miOpponentsInEvent=0;
    s32 maPerRaceCarWord_4B7C[8]={},maCheckpointDistricts[16]={};
    u16 maCheckpointLandmarks[16]={},mEventDestinationLandmarkIndex=0;
    s32 mEventDestinationDistrict=0;
    alignas(8) u8 maOnlineGameModeOptionsStorage[10*44]={};
    u16 maTargetLandmarkIndices[512]={},mau16ActiveLandmarks[512]={};
    s32 miNumRemainingCheckpoints=0,maCurrentPlayerTeam[8]={},miNumPresetRaces=0;
    using PresetRace=BrnProgression::Race;
    PresetRace maPresetRaces[6];
    std::vector<u16> tracked;
    s32 trackerCalls=0,onlineCalls=0;
    const void* lastOnline=nullptr;
    void UpdateTrackerInfo(const u16* p,s32 n) {++trackerCalls;tracked.assign(p,p+n);}
    void UpdateTrackerInfoFromOnlineEvent(const BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event* p) {
        ++onlineCalls;lastOnline=p;
    }
    void RecEvent(const CgsModule::Event*,s32);
    const PresetRace* GetPresetRace(s32) const;
};
using PresetRace=BrnProgression::Race;
}
#include "playtest_map_tracker_preset_bodies.inc"
static void Check(bool v,const char* s) {++guChecks;if(!v){++guFailures;std::printf("FAIL %s\n",s);}}
static BrnProgression::Race MakeRace(s32 start,s32 finish,u8 fill) {
    BrnProgression::Race r;std::memset(&r,fill,sizeof(r));
    r.muNumLandmarks=2;r.maLandmarkIndices[0]=BrnGameState::LandmarkIndex(start);
    r.maLandmarkIndices[1]=BrnGameState::LandmarkIndex(finish);return r;
}
int main() {
    using namespace BrnGameState::GameStateModuleIO;
    Check(sizeof(SetLandmarkRacesAction)==728,"raw original728B game action");
    Check(sizeof(BrnGui::GuiEventSetAvailablePresetRaces)==728,"raw original728B GUI event");
    Check(offsetof(SetLandmarkRacesAction,muNumRaces)==720,"original action count offset");
    Check(offsetof(BrnGui::GuiEventSetAvailablePresetRaces,miNumPresetRaces)==720,"original GUI count offset");
    BrnGameState::GameStateModule game;
    game.mProgressionManager.maPresetRaces={MakeRace(10,20,0x51),MakeRace(99,30,0x63),MakeRace(10,40,0x75)};
    game.mProgressionManager.muNumPresetRaces=3;
    for(bool originalFlag:{false,true}) {
        BrnProgression::Race out[6];std::memset(out,0xA5,sizeof(out));
        auto n=game.mProgressionManager.GetRacesAtLandmark(out,6,BrnGameState::LandmarkIndex(10),originalFlag);
        Check(n==2,"original start-landmark selection");
        Check(std::memcmp(&out[0],&game.mProgressionManager.maPresetRaces[0],120)==0,"complete first120B row including padding");
        Check(std::memcmp(&out[1],&game.mProgressionManager.maPresetRaces[2],120)==0,"complete second120B row and original order");
        Check(reinterpret_cast<u8*>(&out[2])[119]==0xA5,"unused progression rows retained");
    }
    FixtureQueue actions;game.SendSetLandmarkRacesAction(&actions);
    Check(actions.records.size()==1&&actions.records[0].id==51&&actions.records[0].bytes.size()==728,"sender posts original51+728");
    SetLandmarkRacesAction posted;std::memcpy(&posted,actions.records[0].bytes.data(),728);
    Check(posted.muNumRaces==2,"sender count is actual selected races");
    FixtureQueue gui;
    Check(BrnGame::TranslateRaces(51,reinterpret_cast<const CgsModule::Event*>(&posted),&gui),"action51 handled");
    Check(gui.records.size()==1&&gui.records[0].id==170&&gui.records[0].bytes.size()==728,"bridge posts original170+728");
    BrnGui::GuiEventSetAvailablePresetRaces event;std::memcpy(&event,gui.records[0].bytes.data(),728);
    Check(event.miNumPresetRaces==2,"bridge signed GUI count");
    Check(std::memcmp(event.maPresetRaces,posted.maRaces,240)==0,"bridge preserves both live120B rows");
    BrnGui::GuiCache cache;std::memset(cache.maPresetRaces,0xA5,sizeof(cache.maPresetRaces));
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&event),170);
    Check(cache.miNumPresetRaces==2,"cache publishes race count");
    Check(std::memcmp(cache.maPresetRaces,event.maPresetRaces,240)==0,"cache publishes complete live rows");
    Check(reinterpret_cast<u8*>(&cache.maPresetRaces[2])[119]==0xA5,"cache leaves unused rows untouched");
    Check(cache.trackerCalls==1&&cache.tracked==std::vector<u16>({10,20}),"170 publishes first race route");
    Check(cache.GetPresetRace(1)==&cache.maPresetRaces[1],"typed preset accessor points to correct120B row");
    event.miNumPresetRaces=0;cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&event),170);
    Check(cache.miNumPresetRaces==0&&cache.trackerCalls==2&&cache.tracked==std::vector<u16>({10,20}),"170 count0 still refreshes row0 as original");
    FixtureEventQueue requests;requests.types={15,84,85,1};FixtureQueue dispatched;
    game.ProcessGameEventsLandmarkRouteRequestBringUp(&requests,&dispatched);
    Check(dispatched.records.size()==2&&game.routeCalls==1,"actual dispatcher15/85 sender and existing84 preserved");
    BrnGui::GuiEventPrepareForModeStart prepare{};
    prepare.mu8CheckpointCount=2;prepare.mau16CheckpointLandmark[0]=13;prepare.mau16CheckpointLandmark[1]=27;
    prepare.maiCheckpointDistrict[0]=4;prepare.maiCheckpointDistrict[1]=5;
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&prepare),93);
    Check(cache.tracked==std::vector<u16>({13,27}),"93 offline tracker gets original wire landmarks");
    Check(cache.maCheckpointLandmarks[0]==13&&cache.maCheckpointLandmarks[15]==0,"93 checkpoint table copy and original remainder fill");
    Check(cache.mEventDestinationLandmarkIndex==13&&cache.mEventDestinationDistrict==4,"93 destination retained");
    Check(cache.miSatNavZoomLevel==0,"93 original zoom reset retained");
    prepare.mu8CheckpointCount=0;cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&prepare),93);
    Check(cache.tracked.empty(),"93 zero-checkpoint event clears tracker");
    Check(cache.mEventDestinationLandmarkIndex==0xFFFF&&cache.mEventDestinationDistrict==18,"93 original invalid destination pair");
    prepare.mbIsOnline=1;prepare.meGameModeType=12;prepare.miCurrentRound=3;
    auto offlineCalls=cache.trackerCalls;cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&prepare),93);
    Check(cache.onlineCalls==1&&cache.lastOnline==cache.maOnlineGameModeOptionsStorage+3*44,"93 online selects exact44B round");
    Check(cache.trackerCalls==offlineCalls,"93 online uses online tracker helper");
    BrnGui::GuiEventCurrentStatus status{};
    status.miNumRemainingCheckpoints=3;status.maiRemainingCheckpointIndexes[0]=4;
    status.maiRemainingCheckpointIndexes[1]=1;status.maiRemainingCheckpointIndexes[2]=5;
    status.mfDistanceDrivenInCurrentCar=321.5f;
    for(int i=0;i<8;++i) status.maePlayerTeam[i]=i;
    cache.mau16ActiveLandmarks[4]=101;cache.mau16ActiveLandmarks[1]=205;cache.mau16ActiveLandmarks[5]=309;
    for(int mode:{0,3,10,13,15}) {
        cache.meGameModeType=mode;cache.miNumRemainingCheckpoints=2;auto before=cache.trackerCalls;
        std::memset(cache.maTargetLandmarkIndices,0,sizeof(cache.maTargetLandmarkIndices));
        cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&status),492);
        Check(cache.maTargetLandmarkIndices[0]==101&&cache.maTargetLandmarkIndices[1]==205&&cache.maTargetLandmarkIndices[2]==309,"492 maps actual active-landmark indices in every mode");
        Check(cache.trackerCalls==before+(mode==13),"492 publishes only changed count in mode13");
        Check(cache.mfDistanceDriven==321.5f&&cache.maCurrentPlayerTeam[7]==7,"492 other original side effects preserved");
        before=cache.trackerCalls;status.maiRemainingCheckpointIndexes[0]=1;
        cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&status),492);
        Check(cache.maTargetLandmarkIndices[0]==205,"492 still maps when count unchanged");
        Check(cache.trackerCalls==before,"492 same-count status does not republish tracker");
        status.maiRemainingCheckpointIndexes[0]=4;
    }
    cache.meGameModeType=13;status.miNumRemainingCheckpoints=0;auto before=cache.trackerCalls;
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&status),492);
    Check(cache.trackerCalls==before+1&&cache.tracked.empty(),"492 final changed zero count clears online tracker");
    Check(guAsserts==0,"all valid original paths retain their assertions");
    std::printf("PlaytestMapTrackerPreset: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures?1:0;
}
