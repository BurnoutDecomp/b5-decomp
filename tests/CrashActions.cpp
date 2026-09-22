// Production crash action/cleanup functions and real event queues. The receiver
// omits only the unrelated module base/lifecycle; all tested state types are real.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameSource/GameState/BrnGameActions.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <cstring>

static unsigned assertions=0;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* message,const char*,int){++assertions;std::fprintf(stderr,"ASSERT: %s\n",message);return 0;}
void* EndAssert(){return nullptr;}
} namespace Log { DebugPrint* gpDebugPrint=nullptr; void WriteToLog(const char*){} }
namespace Message { u64 gxMessageFilterFlags=0; } }
using namespace BrnWorld;
using namespace BrnGameState::GameStateModuleIO;
using BrnGameState::GameModeParams;
struct CrashFixture
{
    static constexpr u32 KU_INVALID_CRASH=0xffffffffu;
    decltype(CrashModule::mRaceCarCrashes) mRaceCarCrashes;
    decltype(CrashModule::mTrafficCrashes) mTrafficCrashes;
    decltype(CrashModule::mCrashingRaceCars) mCrashingRaceCars;
    decltype(CrashModule::mCrashingTraffic) mCrashingTraffic;
    decltype(CrashModule::mCrashingNetworkTraffic) mCrashingNetworkTraffic;
    decltype(CrashModule::maiSlammedTrafficOwners) maiSlammedTrafficOwners;
    decltype(CrashModule::maCrashingTrafficForPlayers) maCrashingTrafficForPlayers;
    EActiveRaceCarIndex meLocalActiveRaceCarIndex=E_ACTIVE_RACE_CAR_INDEX_0;
    f32 mfPlayerCrashTime=4;
    bool mbFastCrashesForAI=false, mbIsOnlineGameMode=false, mbIsShowtimeGameMode=false;
    bool mbClearUpEnabled=true, mbIsInAGameMode=false;
    s8 miNumCrashExtensions=10;
    CrashFixture(){
        mRaceCarCrashes.Clear();mTrafficCrashes.Clear();mCrashingRaceCars.UnSetAll();
        mCrashingTraffic.UnSetAll();mCrashingNetworkTraffic.UnSetAll();
        std::memset(maiSlammedTrafficOwners,-1,sizeof(maiSlammedTrafficOwners));
        for(auto& set:maCrashingTrafficForPlayers)set.Clear();
    }
    void HandleGameActions(const CrashIO::InputBuffer_PreScene*,CrashIO::OutputBuffer_PreScene*);
    void ClearupCrashes(const CrashIO::InputBuffer_PreScene*,CrashIO::OutputBuffer_PreScene*);
    void ForceClearupAllCrashes(CrashIO::OutputBuffer_PreScene*);
    void OnNetworkPlayerDisconnected(EActiveRaceCarIndex,CrashIO::OutputBuffer_PreScene*);
    void OnTrafficCarRemovedFromCrash(u32,EActiveRaceCarIndex);
    void ResetRaceCarFromCrashIndex(CrashIO::OutputBuffer_PreScene*,u32,bool);
    u32 FindCrashForRaceCar(EActiveRaceCarIndex) const;
};
#include "crash_actions_methods.inc"

int main()
{
    unsigned checks=0,failures=0;
    auto Check=[&](bool pass,const char* name){++checks;if(!pass){++failures;std::fprintf(stderr,"FAIL: %s\n",name);}};
    CrashFixture module;
    auto input=std::make_unique<CrashIO::InputBuffer_PreScene>();
    auto output=std::make_unique<CrashIO::OutputBuffer_PreScene>();
    auto* race=output->mRaceCarOutputInterface.GetRaceCarCrashCompleteEventQueue();
    auto& traffic=output->mTrafficOutputInterface.GetCleanupTrafficEventQueue();
    race->Construct();traffic.Construct();
    output->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    using Queue=CrashIO::InputBuffer_PreScene::GameActionQueue;
    // Deliberately use a different queue base alignment to test host-safe copying.
    alignas(16) unsigned char storage[sizeof(Queue)+16];
    auto* queue=new(storage+4) Queue;
    auto Run=[&](s32 id,const auto& action){
        queue->Construct();queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&action),id,sizeof(action));
        input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
        input->SetGameActionQueue(queue);
        input->mxStatusFlags.UnSetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
        input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);
        module.HandleGameActions(input.get(),output.get());
    };
    auto AddRace=[&](u32 owner){
        RaceCarCrash crash{};crash.mRaceCarVolumeInstanceId.muId=static_cast<u64>(0x01000000u|(owner<<10))<<32;
        crash.mfSecondsBeforeCleanup=4;module.mRaceCarCrashes.Append(crash);module.mCrashingRaceCars.SetBit(owner);
    };
    SetTakedownCameraAction camera{};camera.mbActive=true;Run(6,camera);
    Check(!module.mbClearUpEnabled,"takedown camera pauses cleanup");
    camera.mbActive=false;Run(6,camera);Check(module.mbClearUpEnabled,"camera exit resumes cleanup");
    const CgsModule::Event* copiedEvent=nullptr;s32 copiedSize=0;
    Check(input->GetGameActionQueue()->GetFirstEvent(&copiedEvent,&copiedSize)==6
          && (reinterpret_cast<uintptr_t>(copiedEvent)&15)==0 && copiedSize==sizeof(camera),
          "copied queue payload stays aligned at its new host address");
    queue->Construct();camera.mbActive=true;
    queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&camera),6,sizeof(camera));
    queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&camera),999,sizeof(camera));
    camera.mbActive=false;queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&camera),6,sizeof(camera));
    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    input->SetGameActionQueue(queue);module.HandleGameActions(input.get(),output.get());
    Check(module.mbClearUpEnabled,"ordered camera actions continue past unknown events");

    const u64 flagBits[]={GameModeParams::KU_FLAG_RAPID_CRASHES,GameModeParams::KU_FLAG_DISABLE_CRASH_CLEAN_UP,
        GameModeParams::KU_FLAG_DISABLE_CRASH_EXTENSIONS,GameModeParams::KU_FLAG_LIMITED_CRASH_EXTENSIONS,
        GameModeParams::KU_FLAG_SHORT_CRASH_TIME};
    for(unsigned mask=0;mask<32;++mask){
        PrepareForModeAction action{};action.mePrepareForModeStage=PrepareForModeAction::E_PFM_STAGE_ALL_IN_ONE;
        action.mGameModeParams.meGameModeType=E_MODE_ROAD_RAGE;
        for(unsigned bit=0;bit<5;++bit)if(mask&(1u<<bit))action.mGameModeParams.muFlags|=flagBits[bit];
        module.meLocalActiveRaceCarIndex=static_cast<EActiveRaceCarIndex>(2);
        Run(23,action);
        Check(module.mbFastCrashesForAI==bool(mask&1),"rapid AI crashes flag");
        Check(module.mbClearUpEnabled==!bool(mask&2),"mode cleanup policy");
        Check(module.miNumCrashExtensions==((mask&4)?0:((mask&8)?3:10)),"extension count and disable precedence");
        Check(module.mfPlayerCrashTime==((mask&16)?3.f:4.f),"mode crash duration");
        Check(module.mbIsInAGameMode&&!module.mbIsOnlineGameMode&&!module.mbIsShowtimeGameMode,"mode state applied");
        Check(module.meLocalActiveRaceCarIndex==E_ACTIVE_RACE_CAR_INDEX_INVALID,"ordinary mode invalidates cached player slot");
    }
    PrepareForModeAction prepare{};prepare.mePrepareForModeStage=PrepareForModeAction::E_PFM_STAGE_SECOND_OF_TWO;
    module.mfPlayerCrashTime=17;Run(23,prepare);Check(module.mfPlayerCrashTime==17,"second prepare stage has no effects");
    prepare.mePrepareForModeStage=PrepareForModeAction::E_PFM_STAGE_FIRST_OF_TWO;
    prepare.mbComingFromOnlineLobbyMode=true;prepare.mGameModeParams.mbIsOnline=true;
    prepare.mGameModeParams.meGameModeType=E_MODE_ONLINE_SHOWTIME;
    module.meLocalActiveRaceCarIndex=static_cast<EActiveRaceCarIndex>(3);Run(23,prepare);
    Check(module.mbIsOnlineGameMode&&module.mbIsShowtimeGameMode&&module.meLocalActiveRaceCarIndex==3,"online lobby/showtime transition preserves player slot");
    StopModeAction stop{};stop.mu8Field14=1;module.mfPlayerCrashTime=3;module.miNumCrashExtensions=0;
    module.mbClearUpEnabled=false;Run(39,stop);
    Check(!module.mbIsOnlineGameMode&&!module.mbIsShowtimeGameMode&&module.mfPlayerCrashTime==3&&!module.mbClearUpEnabled&&module.miNumCrashExtensions==0,"showtime-bound stop preserves policy but clears mode kinds");
    stop.mu8Field14=0;Run(39,stop);
    Check(module.mfPlayerCrashTime==4&&module.miNumCrashExtensions==10&&module.mbClearUpEnabled&&!module.mbIsInAGameMode,"ordinary stop restores policy");

    AddRace(1);AddRace(2);ResetRaceCarCrashingAction reset{};reset.meActiveRaceCarIndex=static_cast<EActiveRaceCarIndex>(1);Run(10,reset);
    Check(module.mRaceCarCrashes.GetLength()==1&&race->GetLength()==1&&!race->GetEvent(0).mbRemoveRaceCar,"specific reset posts completion without removal");
    Run(10,reset);Check(race->GetLength()==1,"missing crash reset does nothing");
    RoadRagePlayerDamageAction damage{};damage.mbPlayerTotalled=true;Run(205,damage);
    Check(module.mRaceCarCrashes.GetItem(0).mfSecondsBeforeCleanup==60,"offline totalled crash retained for sixty seconds");
    module.mbIsOnlineGameMode=true;module.mRaceCarCrashes.GetItem(0).mfSecondsBeforeCleanup=9;Run(205,damage);
    Check(module.mRaceCarCrashes.GetItem(0).mfSecondsBeforeCleanup==9,"online totalled action leaves timers alone");
    AddRace(2);AddRace(3);
    TrafficCrash network{};network.mliOwner=2;network.meCrashState=2;network.muVehicleIndex=17;network.mfStartTime=4;
    module.mTrafficCrashes.Append(network);
    RemotePlayerDisconnectedAction disconnect{};disconnect.meActiveRaceCarIndex=static_cast<EActiveRaceCarIndex>(2);Run(11,disconnect);
    Check(module.mRaceCarCrashes.GetLength()==1&&module.mRaceCarCrashes.GetItem(0).GetOwner()==3,"disconnect removes all matching records despite swap erase");
    Check(race->GetLength()==3&&race->GetEvent(1).mbRemoveRaceCar&&race->GetEvent(2).mbRemoveRaceCar,"disconnect posts removal completions");
    Check(module.mTrafficCrashes.GetItem(0).mfStartTime==-1&&(module.mTrafficCrashes.GetItem(0).meCrashState&1),"disconnect expires network traffic");
    module.mCrashingTraffic.SetBit(17);module.mCrashingNetworkTraffic.SetBit(17);
    module.maiSlammedTrafficOwners[17]=2;module.maCrashingTrafficForPlayers[2].Insert(17);
    ResetCrashingAction resetAll{};Run(9,resetAll);
    Check(module.mRaceCarCrashes.GetLength()==0&&module.mTrafficCrashes.GetLength()==0,"reset all clears both crash arrays");
    Check(traffic.GetLength()==1&&traffic.GetEvent(0).mVolumeInstanceId.muId==(u64(0x02000000u|(17u<<10))<<32),"traffic cleanup carries original packed ID");
    Check(!module.mCrashingTraffic.IsBitSet(17)&&!module.mCrashingNetworkTraffic.IsBitSet(17)&&module.maiSlammedTrafficOwners[17]==-1&&module.maCrashingTrafficForPlayers[2].GetLength()==0,"traffic cleanup clears ownership and bits");
    module.maCrashingTrafficForPlayers[4].Insert(21);module.maCrashingTrafficForPlayers[5].Insert(21);
    module.OnTrafficCarRemovedFromCrash(21,E_ACTIVE_RACE_CAR_INDEX_INVALID);
    Check(module.maCrashingTrafficForPlayers[4].GetLength()==0&&module.maCrashingTrafficForPlayers[5].GetLength()==1,"unknown-owner cleanup erases first matching owner only");
    module.mbIsOnlineGameMode=false;AddRace(2);const auto beforeDisconnect=race->GetLength();Run(11,disconnect);
    Check(module.mRaceCarCrashes.GetLength()==1&&race->GetLength()==beforeDisconnect,"offline disconnect ignored");
    damage.mbPlayerTotalled=false;module.mRaceCarCrashes.GetItem(0).mfSecondsBeforeCleanup=7;Run(205,damage);
    Check(module.mRaceCarCrashes.GetItem(0).mfSecondsBeforeCleanup==7,"non-totalled damage leaves cleanup timer alone");
    prepare.mbComingFromOnlineLobbyMode=false;prepare.mGameModeParams.mbIsOnline=false;
    prepare.mGameModeParams.meGameModeType=E_MODE_ROAD_RAGE;
    prepare.mGameModeParams.muFlags=GameModeParams::KU_FLAG_DISABLE_CRASH_CLEAN_UP;Run(23,prepare);
    Check(module.mRaceCarCrashes.GetLength()==1,"prepare with cleanup disabled preserves current crashes");
    prepare.mGameModeParams.muFlags=0;Run(23,prepare);
    Check(module.mRaceCarCrashes.GetLength()==0&&race->GetLength()==beforeDisconnect+1,"prepare with cleanup enabled clears current crashes");
    auto& active=input->mActiveRaceCarInterface;
    active.mePlayerActiveRaceCarIndex=E_ACTIVE_RACE_CAR_INDEX_0;
    active.mbIsPlayerCarActive=true;
    active.maRaceCarStates[0].mTransform.Pos()=Vector3{100,20,300,0};
    active.maRaceCarStates[0].mTransform.At()=Vector3{0,0,1,0};
    auto Cleanup=[&](float x,float y,float z,float timer,bool inMode,bool rival,bool player,bool network){
        module.mRaceCarCrashes.Clear();module.mCrashingRaceCars.UnSetAll();race->Construct();
        module.mbIsInAGameMode=inMode;active.mbIsPlayerCarActive=player;
        active.maxRaceCarFlags[1]=(rival?4:0)|(network?8:0);
        active.maRaceCarStates[1].mTransform.Pos()=Vector3{100+x,20+y,300+z,0};
        AddRace(1);module.mRaceCarCrashes.GetItem(0).mfSecondsBeforeCleanup=timer;
        module.ClearupCrashes(input.get(),output.get());
        return module.mRaceCarCrashes.GetLength()==1;
    };
    Check(Cleanup(0,0,20,-1,true,true,true,false),"near forward rival retained");
    Check(Cleanup(0,0,-4.99f,-1,true,true,true,false),"five unit rear allowance retained");
    Check(!Cleanup(0,0,-5,-1,true,true,true,false),"rear boundary is strict");
    Check(Cleanup(39.99f,0,0,-1,true,true,true,false),"inside forty unit sphere retained");
    Check(!Cleanup(40,0,0,-1,true,true,true,false),"radius boundary is strict");
    Check(!Cleanup(0,41,0,-1,true,true,true,false),"height contributes to distance");
    Check(Cleanup(0,0,20,-19.99f,true,true,true,false),"before hard backstop retained");
    Check(!Cleanup(0,0,20,-20,true,true,true,false),"twenty second backstop clears rival");
    Check(!Cleanup(0,0,20,-1,false,true,true,false),"freeburn expired wreck clears");
    Check(!Cleanup(0,0,20,-1,true,false,true,false),"nonrival expired wreck clears");
    Check(Cleanup(0,0,80,0,true,true,true,false),"zero timer has not expired");
    Check(Cleanup(0,0,80,-1,true,true,false,false),"no active player suppresses cleanup");
    Check(!Cleanup(0,0,80,-1,true,true,true,true)&&race->GetEvent(0).mbRemoveRaceCar,"network cleanup requests removal");
    Check(assertions==0,"valid actions and queues produce no assertions");
    std::printf("CrashActions: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
