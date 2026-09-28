#include <cmath>
#include <cstdio>
#include <limits>
#include "types.hpp"
#include "GameShared/GameClasses/Sound/Io/CgsMessage.h"
using EntityId = int;
struct Vec { float x,y,z; };
int asserts=0;
#define CGS_ASSERT(c,m) do { if(!(c)) ++asserts; } while(0)
namespace CgsDev { namespace Log {
struct Stream { template<class T> Stream& operator<<(const T&) { return *this; } } output;
auto* gpDebugPrint=&output;
}}
namespace BrnTrigger { struct GenericRegion { enum Type { First=19,Last=31 }; }; }
namespace AttribSys { namespace Enums { namespace ePassbyTypes { using ePassbyTypes=int; }}}
namespace BrnGameState { namespace GameStateModuleIO {
struct SoundTriggerAction {
    enum eType { E_TYPE_INVALID, E_TYPE_AT_ENTITY, E_TYPE_AHEAD_OF_ENTITY };
    Vec mQueryPos{}; EntityId mEntityId=7; eType meResultType=E_TYPE_INVALID;
    u32 muActiveTriggers=0;
};
}}
namespace BrnSound { namespace Logic { namespace Passby {
struct PassbyStateManager {
    struct Passby {
        Vec pos; float speed; int type; bool suppress; float volume;
        Passby(Vec p,float s,int t,bool b,float v):pos(p),speed(s),type(t),suppress(b),volume(v){}
    } last{{0,0,0},-1,-1,true,0};
    int count=0; bool accept=true;
    bool PostPassby(const Passby& p) { last=p; ++count;return accept; }
};
}}}
namespace BrnSound { namespace Module {
using BrnGameState::GameStateModuleIO::SoundTriggerAction;
struct Env {
    Logic::Passby::PassbyStateManager manager; int requested=-1;
    auto* GetStateManager(int id) { requested=id;return &manager; }
};
struct SoundLogicModule {
    Env env; SoundTriggerAction* here=nullptr; SoundTriggerAction* ahead=nullptr;
    int messages=0,queries=0,lastEntity=-1; CgsSound::Io::Message<bool> last;
    auto& GetEnvironment(){return env;}
    const SoundTriggerAction* GetSoundTriggerAction(EntityId id,SoundTriggerAction::eType t) {
        ++queries;lastEntity=id;return t==SoundTriggerAction::E_TYPE_AT_ENTITY?here:ahead;
    }
    bool PostMessage(const CgsSound::Io::Message<bool>& m){last=m;++messages;return true;}
};
}}
namespace BrnSound { namespace Vehicles { namespace Environment {
enum eTriggerPosition {E_TRIGGER_POSITION_AT_ENTITY,E_TRIGGER_POSITION_AHEAD_OF_ENTITY,E_TRIGGER_POSITION_COUNT};
struct EntityTriggerInfo {
    u32 muActiveTriggers=0,muPrevTriggers=0;
    bool HasChanged()const{return muActiveTriggers!=muPrevTriggers;}
    BrnTrigger::GenericRegion::Type GetChangeType() const;
};
struct Physics {
    struct Raw {EntityId mEntityId=7;} raw;
    struct Value {float value=60;float GetCurrent()const{return value;}};
    struct Data {Value mSpeedMPH;} data;
    auto* GetRawPhysicsData()const{return &raw;}
    const auto& GetPhysicsData()const{return data;}
};
struct EnclosureControl {
    EntityTriggerInfo maTriggerInfo[2]; Physics* mpPhysicsControl; void* mpLogicModule;
    float mfTimeSinceTrigger=.5f; int draws=0;
    int ConvertRegionTypeToIndex(int) const;
    void UpdateParams(float);
    void ProcessTriggerAction(const BrnGameState::GameStateModuleIO::SoundTriggerAction&,eTriggerPosition);
    void DrawDebug()const{}
};
static const float KF_MIN_TIME_BETWEEN_TRIGGERS=.5f;
static bool KB_SHOW_STATIC_ENVIRONMENT=false,sbDebugWhoosh=false;
bool SndEnvDiagBudget(int&){return false;}
#include "fx_enclosure.inc"
}}}
int main() {
    using namespace BrnSound::Vehicles::Environment;
    using Action=BrnGameState::GameStateModuleIO::SoundTriggerAction;
    int total=0,failures=0;
    auto check=[&](bool ok,const char* n){++total;if(!ok){++failures;std::printf("FAIL %s\n",n);}};
    EntityTriggerInfo info;
    for(int bit=0;bit<12;++bit) {
        info={1u<<bit,0};check(info.GetChangeType()==19+bit,"enter region selects changed type");
        info={0,1u<<bit};check(info.GetChangeType()==19+bit,"exit region selects changed type");
    }
    info={0x841,0};check(info.GetChangeType()==30,"highest changed bit wins");
    info={0x1000,0};check(info.GetChangeType()==19,"console excludes region31");
    info={1,1};info.GetChangeType();check(asserts==1,"unchanged query asserts");asserts=0;
    Physics physics;BrnSound::Module::SoundLogicModule module;EnclosureControl c;
    c.mpPhysicsControl=&physics;c.mpLogicModule=&module;
    Action here,ahead;ahead.muActiveTriggers=1<<6;ahead.mQueryPos={4,5,6};
    c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AT_ENTITY);
    check(c.maTriggerInfo[0].muActiveTriggers==(1<<6)&&module.env.manager.count==0,"at-entity only updates history");
    c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==1&&module.env.requested==4,"ahead change posts to passby manager4");
    auto& p=module.env.manager.last;
    check(p.type==9&&p.speed==0&&p.volume==1&&!p.suppress,"tunnel whoosh parameters match original");
    check(p.pos.x==4&&p.pos.y==5&&p.pos.z==6,"static passby uses query position");
    check(c.mfTimeSinceTrigger==0,"successful attempt resets timer");
    c.mfTimeSinceTrigger=1;c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==1&&c.mfTimeSinceTrigger==1,"unchanged ahead has no whoosh");
    ahead.muActiveTriggers=0;c.mfTimeSinceTrigger=.49f;c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==1&&c.maTriggerInfo[1].muPrevTriggers==(1<<6),"cooldown suppresses but advances history");
    ahead.muActiveTriggers=1<<7;c.mfTimeSinceTrigger=.5f;physics.data.mSpeedMPH.value=59.99f;c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==1&&c.mfTimeSinceTrigger==.5f,"below 60mph suppressed");
    ahead.muActiveTriggers=0;physics.data.mSpeedMPH.value=60;c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==2&&module.env.manager.last.type==13,"exact time/speed thresholds and exit whoosh pass");
    ahead.muActiveTriggers=1;c.mfTimeSinceTrigger=1;c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==2&&c.mfTimeSinceTrigger==1,"non-passby region does not reset timer");
    ahead.muActiveTriggers=1<<8;c.mfTimeSinceTrigger=std::numeric_limits<float>::quiet_NaN();physics.data.mSpeedMPH.value=std::numeric_limits<float>::quiet_NaN();c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==3&&module.env.manager.last.type==8,"unordered thresholds follow both original blt arms");
    ahead.muActiveTriggers=1<<9;c.mfTimeSinceTrigger=1;module.env.manager.accept=false;c.ProcessTriggerAction(ahead,E_TRIGGER_POSITION_AHEAD_OF_ENTITY);
    check(module.env.manager.count==4&&c.mfTimeSinceTrigger==0,"full queue still consumes cooldown");
    c.maTriggerInfo[0]={0,0};c.maTriggerInfo[1]={0,0};c.mfTimeSinceTrigger=0;
    here.muActiveTriggers=1;module.here=&here;module.ahead=nullptr;c.UpdateParams(.25f);
    check(module.queries==2&&module.lastEntity==7,"both trigger queries use physics entity");
    check(c.mfTimeSinceTrigger==.25f,"frame time advances cooldown");
    check(c.maTriggerInfo[0].muActiveTriggers==1&&c.maTriggerInfo[0].muPrevTriggers==0,"at-entity action reaches enclosure");
    check(module.messages==1&&module.last.mData,"tunnel entry publishes true");
    check(module.last.GetEventId()==39&&module.last.GetStateManagerId()==2&&module.last.GetInstanceId()==0xFFFF&&module.last.GetEffectId()==0&&module.last.GetEffectType()==CgsSound::Io::MessageHeader::E_EFFECT_TYPE_CONTROL,"tunnel message destination matches assembly");
    c.UpdateParams(0);check(module.messages==1,"unchanged at-entity action has no message");
    here.muActiveTriggers=2;c.UpdateParams(0);check(module.messages==2&&!module.last.mData,"exit tunnel publishes false even in another enclosure");
    module.here=nullptr;c.UpdateParams(0);check(module.messages==3,"missing action retains previous edge as on console");
    c.maTriggerInfo[1]={0,0};module.ahead=&ahead;physics.data.mSpeedMPH.value=60;c.mfTimeSinceTrigger=.5f;
    c.UpdateParams(0);check(module.env.manager.count==5,"ahead query reaches ProcessTriggerAction");
    check(asserts==0,"valid update raises no assertion");
    std::printf("FxEnclosure: %d checks, %d failures\n",total,failures);return failures?1:0;
}
