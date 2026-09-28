#include <cmath>
#include <cstdio>
#include <limits>
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
int assertions=0;
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint=nullptr; }
namespace Message {u64 gxMessageFilterFlags=0;}
namespace Assert {
int BeginAssert(){return 0;} int FireAssert(const char*,const char*,int){++assertions;return 0;} void* EndAssert(){return nullptr;}
}}
namespace CgsSound { namespace Logic {
struct State {void* manager=nullptr;void* GetStateManager()const{return manager;}};
struct EffectBase {int id=0,attached=0;State state;void* mpLogicModule=nullptr;
    int GetEffectID()const{return id;}bool Attach(){++attached;return true;}
    const State* GetStateBase()const{return &state;}
};
}}
namespace AttribSys {namespace Enums {namespace ePassbyTypes {using ePassbyTypes=int;}}}
namespace BrnSound { namespace Logic {namespace Passby {
struct PassbyStateManager {
    struct Passby {Vector3 pos;float speed;int type;bool moving;float gain;
        Passby(Vector3 p,float s,int t,bool m,float g):pos(p),speed(s),type(t),moving(m),gain(g){}}
        last{{},-1,-1,true,0};int count=0;
    bool PostPassby(const Passby&p){last=p;++count;return true;}
};
}} namespace Module {
struct SoundLogicModule {struct Env {
    Logic::Passby::PassbyStateManager manager;int requested=-1;
    auto* GetStateManager(int id){requested=id;return &manager;}
}env;auto&GetEnvironment(){return env;}};
} namespace Vehicles {
namespace Engines {struct PhysicsControl: CgsSound::Logic::EffectBase {
    template<class T>struct Value{T value{};T GetCurrent()const{return value;}};
    struct Data{Value<Vector3>mPosition3d;Value<float>mSpeedMPH;}data;
    const Data&GetPhysicsData()const{return data;}
};}
struct PlayerVehicleStateManager {
    mutable int queries=0;mutable Vector3 pos{};mutable float radius=0;mutable int max=0;
    World::StaticSoundEntity entities[16];int count=0;
    int Query(Vector3 p,float r,World::StaticSoundEntity*out,int m,bool)const {
        ++queries;pos=p;radius=r;max=m;for(int i=0;i<count;++i)out[i]=entities[i];return count;
    }
};
namespace Environment {
struct StaticPassbyControl:CgsSound::Logic::EffectBase {
    struct PassbyRecord{Vector3 mvPosition;float mfTimeStamp;};
    struct PassbyHistory{Array<PassbyRecord,5>mPassbyRecords;
        bool Record(Vector3);void Update(float);};
    PassbyHistory mafHistoryTimeouts[19];Engines::PhysicsControl*mpPhysicsControl=nullptr;
    s32 GetController(s32);void AttachController(CgsSound::Logic::EffectBase*);
    bool Attach();void UpdateParams(float);void UpdateHistory(float);
    void ProcessPassbys(Vector3,float,const PlayerVehicleStateManager*);
    void TriggerPassby(const World::StaticSoundEntity&);
};
bool KB_SHOW_STATIC_ENVIRONMENT=false;
bool SndEnvDiagBudget(s32&){return false;}
#include "fx_static_passby.inc"
}}}
int main(){
    using namespace BrnSound::Vehicles;
    using Environment::StaticPassbyControl;
    int checks=0,failures=0;
    auto check=[&](bool ok,const char*n){++checks;if(!ok){++failures;std::printf("FAIL %s\n",n);}};
    StaticPassbyControl c;Engines::PhysicsControl physics;
    for(auto&h:c.mafHistoryTimeouts)h.mPassbyRecords.Construct();
    check(c.GetController(0)==0&&c.GetController(1)==-1&&c.GetController(-1)==-1,"physics controller slot");
    c.AttachController(&physics);check(c.mpPhysicsControl==&physics,"attach physics controller");
    CgsSound::Logic::EffectBase wrong;wrong.id=1;c.AttachController(&wrong);
    check(assertions==1&&c.mpPhysicsControl==&physics,"unexpected controller asserts");assertions=0;
    for(auto&h:c.mafHistoryTimeouts)h.Record({0,0,0,0});c.Attach();
    bool empty=true;for(auto&h:c.mafHistoryTimeouts)empty&=h.mPassbyRecords.GetLength()==0;
    check(c.attached==1&&empty,"attach clears all19 histories");
    // Clear the fixture even on the old body so the remaining history tests are independent.
    for(auto&h:c.mafHistoryTimeouts)h.mPassbyRecords.Clear();
    auto&h=c.mafHistoryTimeouts[0];
    check(h.Record({0,0,0,0})&&h.mPassbyRecords[0].mfTimeStamp==1,"new record holds for one second");
    check(!h.Record({0x1p-16f,0,0,4}),"position tolerance includes boundary and ignores w");
    h.mPassbyRecords.Clear();h.Record({0,0,0,0});
    check(h.Record({0x1p-15f,0,0,0}),"position beyond tolerance is distinct");
    h.mPassbyRecords.Clear();h.Record({0,0,0,0});
    check(!h.Record({0,0,0,5}),"metadata does not retrigger");
    h.Update(.5f);check(h.mPassbyRecords.GetLength()==1,"record survives half second");
    h.Update(.5f);check(h.mPassbyRecords.GetLength()==1,"zero timer remains live");
    h.Update(.01f);check(h.mPassbyRecords.GetLength()==0,"negative timer expires");
    h.Record({0,0,0,0});h.mPassbyRecords[0].mfTimeStamp=std::numeric_limits<float>::quiet_NaN();h.Update(1);
    check(h.mPassbyRecords.GetLength()==1,"unordered timestamp is retained");
    h.mPassbyRecords.Clear();for(int i=0;i<5;++i)h.Record({float(i),0,0,0});
    check(!h.Record({6,0,0,0})&&h.mPassbyRecords.GetLength()==5,"full history rejects new record");
    for(int i=0;i<5;++i)h.mPassbyRecords[i].mfTimeStamp=i%2?.8f:0;
    h.Update(.1f);check(h.mPassbyRecords.GetLength()==2&&h.mPassbyRecords[0].mvPosition.x==1&&h.mPassbyRecords[1].mvPosition.x==3,"expiry preserves survivor order");
    BrnSound::Module::SoundLogicModule module;PlayerVehicleStateManager scene;c.mpLogicModule=&module;c.state.manager=&scene;
    // Feed the actual packed world-map entity format to the production consumer.
    BrnSound::World::StaticSoundEntity::UFloatHelper bits;bits.mu32Bits=(7u<<16)|4;
    scene.entities[0].mPosPlus={11,12,13,bits.mfPlusComponent};scene.count=1;
    c.ProcessPassbys({1,2,3,0},49.99f,&scene);
    check(scene.queries==1&&scene.radius==30&&scene.max==16&&scene.pos.x==1,"scene query runs below speed threshold");
    check(module.env.manager.count==0,"below50mph suppresses passbys");
    c.ProcessPassbys({1,2,3,0},50,&scene);
    check(module.env.manager.count==1&&module.env.requested==4,"at50mph posts passby to manager4");
    auto&p=module.env.manager.last;
    check(p.pos.x==11&&p.pos.y==12&&p.pos.z==13&&p.pos.w==bits.mfPlusComponent,"packed position preserved");
    check(p.type==7&&p.speed==0&&!p.moving&&p.gain==1,"static passby parameters");
    c.ProcessPassbys({},70,&scene);check(module.env.manager.count==1,"same source suppressed during cooldown");
    c.UpdateHistory(1.1f);c.ProcessPassbys({},70,&scene);check(module.env.manager.count==2,"expired source can retrigger");
    scene.entities[0].mPosPlus.x=14;c.ProcessPassbys({},std::numeric_limits<float>::quiet_NaN(),&scene);
    check(module.env.manager.count==3,"unordered speed follows console fallthrough");
    for(auto&history:c.mafHistoryTimeouts)history.mPassbyRecords.Clear();
    scene.entities[0].mPosPlus.x=15;physics.data.mPosition3d.value={21,22,23,0};physics.data.mSpeedMPH.value=60;
    c.mpPhysicsControl=&physics;c.UpdateParams(.25f);
    check(scene.pos.x==21&&scene.pos.y==22&&scene.pos.z==23,"per-frame query uses physics position");
    check(module.env.manager.count==4,"per-frame update dispatches passby");
    check(c.mafHistoryTimeouts[7].mPassbyRecords.GetLength()==1&&c.mafHistoryTimeouts[7].mPassbyRecords[0].mfTimeStamp==.75f,"per-frame history ages after posting");
    for(auto&history:c.mafHistoryTimeouts){history.mPassbyRecords.Clear();history.Record({0,0,0,0});}
    c.UpdateHistory(1.1f);empty=true;for(auto&history:c.mafHistoryTimeouts)empty&=history.mPassbyRecords.GetLength()==0;
    check(empty,"all19 histories age");check(assertions==0,"valid inputs have no assertions");
    std::printf("FxStaticPassby: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
