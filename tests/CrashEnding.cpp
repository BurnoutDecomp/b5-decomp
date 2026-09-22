// Production bodies, event types, queue storage and IO locks. Fixture receivers omit
// unrelated module lifecycle and traffic processing, with the existing impact arm observed.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
static unsigned assertions=0;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
} namespace Log { DebugPrint* gpDebugPrint=nullptr; } }
namespace CgsDev { namespace Message { u64 gxMessageFilterFlags=0; } }
using namespace BrnWorld;
using namespace BrnGameState;
using namespace BrnGameState::GameStateModuleIO;
static void LogCrashPark(bool&, const char*) {}
struct CrashFixture {
    bool mbNeedToSendEndingMessage=false;
    unsigned processed=0;
    void ProcessCrashedRaceCarEvents(const CrashIO::InputBuffer_PostPhysics*,CrashIO::OutputBuffer_PostPhysics*){++processed;}
    void PostPhysicsUpdate(CgsModule::IOBufferStack*,CgsModule::IOBufferStack*,
        const CrashIO::InputBuffer_PostPhysics*,CrashIO::OutputBuffer_PostPhysics*,BrnUpdateSet);
};
struct GameStateFixture {
    unsigned impacts=0;
    void SendVehicleImpactMessages(const VehicleImpactEvent*,GameActionQueue*){++impacts;}
    void ProcessGameEventsVehicleImpactBringUp(const CgsModule::VariableEventQueue<1536,16>*,GameActionQueue*);
};
#include "crash_ending_methods.inc"
int main(){
    unsigned checks=0,failures=0;
    auto Check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",name);}};
    auto input=std::make_unique<CrashIO::InputBuffer_PostPhysics>();
    auto output=std::make_unique<CrashIO::OutputBuffer_PostPhysics>();
    input->CgsModule::IOBuffer::Construct();output->CgsModule::IOBuffer::Construct();
    output->mGameEventQueue.Construct();
    CrashFixture crash;GameStateFixture state;
    crash.mbNeedToSendEndingMessage=true;
    crash.PostPhysicsUpdate(nullptr,nullptr,input.get(),output.get(),1);
    Check(crash.mbNeedToSendEndingMessage&&crash.processed==0,"skipped simulation preserves pending notification");
    Check(!input->IsBufferLocked()&&!output->IsBufferLocked(),"skip releases both IO locks");
    const CgsModule::Event* event=nullptr;s32 size=0;
    Check(output->mGameEventQueue.GetFirstEvent(&event,&size)==-1&&!event,"skip posts nothing");
    crash.PostPhysicsUpdate(nullptr,nullptr,input.get(),output.get(),2);
    Check(!crash.mbNeedToSendEndingMessage&&crash.processed==1,"non-skip update consumes latch after processing");
    Check(!input->IsBufferLocked()&&!output->IsBufferLocked(),"normal update releases locks");
    Check(output->mGameEventQueue.GetFirstEvent(&event,&size)==42&&event&&size==1,"original one-byte event42 published");
    const CgsModule::Event* first=event;
    if(event)Check(output->mGameEventQueue.GetNextEvent(event,&event,&size)==-1&&!event,"producer publishes one event");
    crash.PostPhysicsUpdate(nullptr,nullptr,input.get(),output.get(),0);
    event=nullptr;output->mGameEventQueue.GetFirstEvent(&event,&size);
    if(event)Check(output->mGameEventQueue.GetNextEvent(event,&event,&size)==-1&&!event,"consumed latch never reposts");
    // Model the existing world's Append and game-state carry Append with real queue types.
    CgsModule::VariableEventQueue<1536,16> world,carry;world.Construct();carry.Construct();
    output->LockForRead();
    world.Append(*static_cast<const CrashIO::OutputBuffer_PostPhysics*>(output.get())->GetGameEventQueue());
    output->UnlockForRead();carry.Append(world);
    GameActionQueue actions;actions.Construct();
    state.ProcessGameEventsVehicleImpactBringUp(&carry,&actions);
    Check(actions.GetFirstEvent(&event,&size)==17&&event&&size==1,"world event reaches original action17 relay");
    Check(carry.GetFirstEvent(&event,&size)==42&&event,"relay does not clear source queue");
    actions.Clear();carry.Clear();
    PlayerCrashEndingEvent ending{};VehicleImpactEvent impact{};
    carry.AddEvent(reinterpret_cast<const CgsModule::Event*>(&ending),999,sizeof(ending));
    carry.AddEvent(reinterpret_cast<const CgsModule::Event*>(&impact),31,sizeof(impact));
    carry.AddEvent(reinterpret_cast<const CgsModule::Event*>(&ending),42,sizeof(ending));
    carry.AddEvent(reinterpret_cast<const CgsModule::Event*>(&ending),42,sizeof(ending));
    state.ProcessGameEventsVehicleImpactBringUp(&carry,&actions);
    Check(state.impacts==1,"existing impact arm still dispatches");
    Check(actions.GetFirstEvent(&event,&size)==17&&event&&size==1,"unknown events skipped before ending event");
    if(event)Check(actions.GetNextEvent(event,&event,&size)==17&&event&&size==1,"each notification produces an action");
    if(event)Check(actions.GetNextEvent(event,&event,&size)==-1&&!event,"no additional actions");
    Check(assertions==0,"valid full chain produces no lock assertions");
    const unsigned before=assertions;output->GetGameEventQueue();
    Check(assertions==before+1,"mutable getter checks write lock");
    output->LockForWrite();output->GetGameEventQueue();output->UnlockForWrite();
    Check(assertions==before+1,"mutable getter accepts write lock");
    std::printf("CrashEnding: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
