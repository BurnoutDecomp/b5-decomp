// Original instruction/branch counterexamples from the frozen 130-site review.
// Production bodies are injected by the runner, including for old revisions.
// Fixtures replace owner, renderer, pool and particle boundaries, not game layout.
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include "pc/math/VmxFloat.h"
#include "SDKs/XboxMath/XMVectorATan.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

static unsigned checks=0, failures=0;
static void Check(bool ok,const char* label) { ++checks;if(!ok) {++failures;std::printf("FAIL %s\n",label);} }
static u32 Bits(f32 x) {u32 b;std::memcpy(&b,&x,4);return b;}
static f32 FromBits(u32 b) {f32 x;std::memcpy(&x,&b,4);return x;}
static bool Near(f32 a,f32 b) {return std::fabs(a-b)<1e-5f;}
static void Assert(bool ok) {if(!ok) {++failures;std::puts("fixture assertion");}}
#undef CGS_ASSERT
#define CGS_ASSERT(value,message) Assert(value)

namespace BrnPhysics { namespace Vehicle {
struct Wheel {f32 mfRadiansPerSecond=0,mfRadius=0;};
struct RaceCarState {Vector3 mLinearVelocity{};Matrix44Affine mTransform{};Wheel maWheels[4];bool mbFullyDrivableFromCrash=false;};
}}
using RaceCarState=BrnPhysics::Vehicle::RaceCarState;
namespace BrnParticle {
struct LionEffect {
    enum {EPPE_FLAG_ENABLED=1,EPPE_FLAG_CHANGED=2};
    Matrix44Affine mTransform{};u16 muFlags=0;
    const Matrix44Affine& GetTransform() const {return mTransform;}
};
struct ParticleDescription {static u32 HashString(const char*) {return 99;}};
struct ParticleModule {
    LionEffect effects[16];bool live[16]{};f32 mfSimulationRate=2;unsigned starts=0,stops=0;
    LionEffect* GetLionEffect(u32 n) {return n<16 && live[n]?&effects[n]:nullptr;}
    u32 StartLionEffect(u32,const char*,u32) {++starts;live[3]=true;return 3;}
};
}
namespace BrnEffects {
static const u32 KU_MAX_JUNKYARD_VFX=10;
struct EffectsDebugJumping {
    EffectsDebugJumping();bool mbForceJumping;f32 mfJumpLandingSparkSpeed,mfLandingSparksTime,mfVapourDelayTime,mfVapourRampTime;
    bool IsForceJumping() const {return mbForceJumping;}
    f32 VapourStartDelay() const {return mfVapourDelayTime;}
    f32 VapourRampEndTime() const {return mfVapourRampTime;}
};
struct EffectsDebugProps {EffectsDebugProps();bool mbEnable;u32 mMaterialIndex;};
struct EffectsModule;
struct EffectsDebugComponent {
    EffectsModule* mpEffectsModule=nullptr;
    bool mbJunkyardVfxEdit=false,enabled=false,qa=false,force=false,transition=false;
    s32 mnJunkyardVfxId=0;f32 mfJunkyardVfxPosX=0,mfJunkyardVfxPosY=0,mfJunkyardVfxPosZ=0,mfJunkyardVfxAngle=0;
    EffectsDebugJumping jumping;f32 blend=.625f,transitionMin=3,transitionMax=6;
    const EffectsDebugJumping& JumpParams() const {return jumping;}
    bool IsEnabled() const {return enabled;} bool QAEnabled() const {return qa;}
    bool IsForceStateBlend() const {return force;} f32 ForceStateBlendValue() const {return blend;}
    bool IsBoostTransitionEditing() const {return transition;}
    f32 BoostTransitionMin() const {return transitionMin;} f32 BoostTransitionMax() const {return transitionMax;}
    static void OnJunkyardEffectIndexChange(void*,void*);
    static void OnJunkyardEffectValueChange(void*,void*);
};
struct EffectsModule {
    BrnParticle::ParticleModule mParticleModule;EffectsDebugComponent mDebugComponent;
    u32 mEffectInstanceHandle=1,mQAEffectHandle=2;
    BrnParticle::ParticleModule& ParticleModule() {return mParticleModule;}
    u32 GetJunkyardEffectHandle(u32 i) {return i+1;}
    void HandleQADebugTests(f32,f32,const RaceCarState*);
};
struct ParticleEffectHelper {
    BrnParticle::ParticleModule& p;explicit ParticleEffectHelper(BrnParticle::ParticleModule& x):p(x) {}
    void StopEffect(u32& h) {++p.stops;if(h<16)p.live[h]=false;h=~0u;}
};
static bool sbRestartEffects=false;static u32 guQADebugEffectPointInitFlags=0;
static Vector4 gaQADebugEffectPoints[11]{};static s32 liEffectInstanceIndex=0;
static const char* KAC_QA_EFFECT="fixture QA asset";
struct ActiveRaceCarData {u32 mJumpEffectHandle=7;};
struct RaceCarParticleEffectHelper {
    const BrnPhysics::Vehicle::RaceCarState* car=nullptr;EffectsDebugComponent* debug=nullptr;
    ActiveRaceCarData active;f32 recordedBlend=-1;u32 recordedHandle=0;int count=0;
    const BrnPhysics::Vehicle::RaceCarState* RaceCarState() const {return car;}
    EffectsDebugComponent* DebugComponent() const {return debug;}
    ActiveRaceCarData* ActiveRaceCar() {return &active;}
    int CurrentGameMode() const {return 0;}
    void SetEffectStateBlend(const u32* h,int n,f32 b) {recordedHandle=*h;count=n;recordedBlend=b;}
};
namespace Curves {struct SmoothStep {f32 Evaluate(const Vector3&,const Vector2&,f32) const;};}
struct CarState {
    const BrnPhysics::Vehicle::RaceCarState* mpCarState=nullptr;
    bool mbIsBoosting=false,mbExhaustPopThisFrame=false,mbEngineRunning=true,mbCrashing=false,mbJumping=false;
    f32 mfSpeedMPH=0,mfBoostAmount=0,dt=.1f;f32 GetDt() const {return dt;}
};
enum EffectsState {EffectsStateAllOff=0,EffectsStateBoosting=1,EffectsStateExhaustSmokeOn=2,
    EffectsStateExhaustSmokeStopping=3,EffectsStateBoostStopping=4,EffectsStateExhaustPopRestart=5,EffectsStateExhaustPop=6,EffectsState_MAX=20};
struct EffectsStateMachine {
    f32 mTime=0;EffectsState mState=EffectsStateAllOff;bool expired=false;unsigned entries=0,ticks=0;
    virtual EffectsState OnDetermineNextState(CarState&,bool e,EffectsState s,RaceCarParticleEffectHelper&) {expired=e;return e?EffectsStateAllOff:s;}
    void OnChangeState(EffectsState,RaceCarParticleEffectHelper&,CarState&) {++entries;}
    void OnTick(CarState&,RaceCarParticleEffectHelper&) {++ticks;}
    void Tick(CarState&,RaceCarParticleEffectHelper&);
};
struct BoostStateMachine : EffectsStateMachine {
    f32 mfExhaustSmokeBlend=0;
    EffectsState OnDetermineNextState(CarState&,bool,EffectsState,RaceCarParticleEffectHelper&) override;
    void SetBlendValue(RaceCarParticleEffectHelper& h,f32 b) {h.recordedBlend=b;}
};
struct JumpStateMachine {void SetVapourBlend(f32,RaceCarParticleEffectHelper&) const;};
}
struct CoronaParams {Vector2 mvSize{};};
struct Arrow {Vector3 a,b;u32 colour;};static std::vector<Arrow> arrows;
namespace CgsDev {
struct DebugRender {void DrawArrow(Vector3 a,Vector3 b,u32 c) {arrows.push_back({a,b,c});}};
struct DebugInterface {DebugRender& GetRender() {static DebugRender r;return r;}};
}
struct Journal {Vector3 p{};mutable unsigned reads=0;u32 GetSize() const {return 2;} Vector3 operator[](int) const {++reads;return p;}};
struct Tracker {Journal journal;Vector3 velocity{};mutable unsigned reads=0;const Journal& GetPositionJournal() const {return journal;} Vector3 GetLinearVelocityLastFrame() const {++reads;return velocity;}};
struct WorldMap {bool safe=false;unsigned queries=0;bool GetSafePositionNearestPointWithDisplacement(Vector3,Vector3,Vector3& p) {++queries;p={2,3,4,0};return safe;}};
struct IO {WorldMap* mpWorldMap;};
enum {E_FLAG_TAIL_SHOW_WORLD_MAP_DEBUG=0};
struct MainDirector {u8 maStateFlagTail[1]{1};Tracker mVehicleTracker;void DrawWorldMap(IO*);};
namespace BrnDirector {
struct LogString {f32 mfTimeLeft=0;};
struct Pool {LogString data[4];std::vector<s32> freed;unsigned reads=0;LogString& operator[](s32 i) {++reads;return data[i];} void FreeObject(s32 i) {freed.push_back(i);}};
struct Indices {std::vector<s32> ids;u32 GetLength() const {return static_cast<u32>(ids.size());} s32 operator[](u32 i) const {return ids[i];} void Erase(u32 i) {ids.erase(ids.begin()+i);}};
struct DebugLog {Pool mStringPool;Indices mStringIndices;void Update(f32);};
}

#include "debug_menu_effects_director.inc"

static void DefaultsAndQA() {
    using namespace BrnEffects;
    // Object-representation checks detect actual constructor stores, even when
    // the old empty constructor leaves the seeded storage untouched.
    alignas(EffectsDebugJumping) unsigned char jumping[sizeof(EffectsDebugJumping)];
    std::memset(jumping,0xA5,sizeof(jumping));new(jumping) EffectsDebugJumping;
    f32 values[4];std::memcpy(values,jumping+4,sizeof(values));
    Check(jumping[0]==0 && Bits(values[0])==Bits(50) && Bits(values[1])==Bits(.1f) && Bits(values[2])==Bits(.2f) && Bits(values[3])==Bits(.2f),"E1 original Jump defaults are stored");
    alignas(EffectsDebugProps) unsigned char props[sizeof(EffectsDebugProps)];
    std::memset(props,0xA5,sizeof(props));new(props) EffectsDebugProps;u32 material;std::memcpy(&material,props+4,4);
    Check(props[0]==0 && material==0,"E1 original Props defaults are stored");
    EffectsModule m;RaceCarState car;car.mTransform.SetIdentity();m.mParticleModule.live[1]=true;
    guQADebugEffectPointInitFlags=0;liEffectInstanceIndex=8;m.HandleQADebugTests(0,0,&car);
    auto& effect=m.mParticleModule.effects[1];
    Check(Near(effect.mTransform.wAxis.y,-.4f) && Near(effect.mTransform.wAxis.z,1.4f),"E2 QA point eight reaches live effect");
    Check((guQADebugEffectPointInitFlags&1)!=0 && (effect.muFlags&BrnParticle::LionEffect::EPPE_FLAG_CHANGED)!=0,"QA lazy latch and changed flag");
    liEffectInstanceIndex=5;car.mTransform.wAxis={1e-40f,2,3,4};m.HandleQADebugTests(0,0,&car);
    Check(Bits(effect.mTransform.wAxis.x)==0 && effect.mTransform.wAxis.w==4,"E3 QA placement flushes through four-lane stages");
    m.mDebugComponent.qa=true;m.mQAEffectHandle=2;m.mParticleModule.live[2]=true;m.HandleQADebugTests(0,0,&car);
    Check(Near(m.mParticleModule.effects[2].mTransform.wAxis.y,2.75f),"QA handle receives original vertical offset");
    sbRestartEffects=true;m.mDebugComponent.enabled=false;m.mParticleModule.mfSimulationRate=3;m.HandleQADebugTests(0,0,&car);
    Check(!sbRestartEffects && m.mParticleModule.mfSimulationRate==1,"QA restart latch resets disabled simulation rate");
}
static void Junkyard() {
    using namespace BrnEffects;EffectsModule m;auto& d=m.mDebugComponent;d.mpEffectsModule=&m;d.mbJunkyardVfxEdit=true;
    m.mParticleModule.live[1]=true;d.mfJunkyardVfxPosX=2;d.mfJunkyardVfxPosY=3;d.mfJunkyardVfxPosZ=4;
    auto& e=m.mParticleModule.effects[1];d.mfJunkyardVfxAngle=0;EffectsDebugComponent::OnJunkyardEffectValueChange(nullptr,&d);
    Check(e.mTransform.xAxis.z>.99f && e.mTransform.zAxis.x<-.99f && std::fabs(e.mTransform.zAxis.z)<1e-5f,"E4 zero-angle junkyard basis has original handedness");
    Check(e.mTransform.wAxis.x==2 && e.mTransform.wAxis.y==3 && e.mTransform.wAxis.z==4 && Bits(e.mTransform.wAxis.w)==0,"junkyard edited translation retains zero W lane");
    d.mfJunkyardVfxAngle=90;EffectsDebugComponent::OnJunkyardEffectValueChange(nullptr,&d);
    Check(e.mTransform.xAxis.x>.99f && e.mTransform.zAxis.z>.99f,"junkyard folded-cycle phase at ninety degrees");
    e.mTransform.xAxis={0,0,0,0};EffectsDebugComponent::OnJunkyardEffectIndexChange(nullptr,&d);
    Check(Near(d.mfJunkyardVfxAngle,90),"E5 zero X/Z reads positive ninety degrees");
    e.mTransform.xAxis={-0.0f,0,0,0};EffectsDebugComponent::OnJunkyardEffectIndexChange(nullptr,&d);
    Check(Near(d.mfJunkyardVfxAngle,270),"E5 negative-zero X selects negative half-pi then wraps");
}
static void StateMachines() {
    using namespace BrnEffects;RaceCarState car;car.mTransform.SetIdentity();EffectsDebugComponent d;
    d.jumping.mbForceJumping=false;d.jumping.mfVapourDelayTime=.2f;d.jumping.mfVapourRampTime=.2f;
    RaceCarParticleEffectHelper h;h.car=&car;h.debug=&d;JumpStateMachine jump;
    car.mLinearVelocity={0,0,50,0};jump.SetVapourBlend(.3f,h);
    Check(h.recordedBlend>.2f && h.recordedBlend<.3f && h.recordedHandle==7 && h.count==1,"E6 horizontal fast jump publishes positive vapour blend");
    car.mLinearVelocity.y=-1;jump.SetVapourBlend(.3f,h);Check(h.recordedBlend==0,"descending jump rejects vapour");
    d.force=true;jump.SetVapourBlend(.3f,h);Check(h.recordedBlend==d.blend,"force blend reaches jump handle");d.force=false;
    BoostStateMachine boost;CarState s;s.mpCarState=&car;
    car.mLinearVelocity={1e-20f,0,0,0};boost.OnDetermineNextState(s,false,EffectsStateExhaustSmokeOn,h);
    Check(Bits(h.recordedBlend)==0,"E7 exhaust vector magnitude flushes tiny squared speed");
    s.mfSpeedMPH=std::numeric_limits<f32>::quiet_NaN();
    Check(boost.OnDetermineNextState(s,false,EffectsStateExhaustSmokeOn,h)==EffectsStateExhaustSmokeStopping,"E7 unordered speed takes smoke stopping arm");
    s.mfSpeedMPH=0;car.mLinearVelocity={};car.maWheels[3].mfRadiansPerSecond=std::numeric_limits<f32>::quiet_NaN();car.maWheels[3].mfRadius=1;
    boost.OnDetermineNextState(s,false,EffectsStateExhaustSmokeOn,h);Check(h.recordedBlend==1,"E7 unordered final wheel flows through original fsel clamps");
    d.transition=true;d.transitionMin=FromBits(0x00800001);d.transitionMax=FromBits(0x00800002);
    s.mbIsBoosting=true;s.mfBoostAmount=d.transitionMin;
    boost.OnDetermineNextState(s,false,EffectsStateBoosting,h);
    Check(h.recordedBlend==1,"E10 Boost transition midpoint keeps original scalar fused rounding");
    EffectsStateMachine timer;timer.mState=EffectsStateBoostStopping;timer.mTime=1;s.dt=std::numeric_limits<f32>::quiet_NaN();timer.Tick(s,h);
    Check(timer.expired && timer.mState==EffectsStateAllOff && timer.entries==1 && timer.ticks==1,"E9 unordered timer expires before entry and tick hooks");
}
static void CoronaAndWorldMap() {
    CoronaParams c;c.mvSize={1e-40f,2,3,4};Vector2 size=ScaleCorona(c,2);
    Check(Bits(size.x)==0 && size.y==4 && size.z==6 && size.w==8,"E8 corona scaling flushes input and retains all four lanes");
    WorldMap map;IO io{&map};MainDirector d;d.mVehicleTracker.journal.p={FromBits(0x00800000),0,0,0};d.mVehicleTracker.velocity={FromBits(0x80800002),0,0,0};
    arrows.clear();d.DrawWorldMap(&io);
    Check(arrows.size()==1 && Bits(arrows[0].b.x)==0x80000000,"D2 failed safe query still publishes VMX-flushed endpoint");
    Check(map.queries==1 && d.mVehicleTracker.journal.reads==2 && d.mVehicleTracker.reads==2 && arrows[0].colour==0xFF0000FF,"WorldMap failed-query read order and colour");
    map.safe=true;arrows.clear();d.mVehicleTracker.journal.reads=0;d.mVehicleTracker.reads=0;d.DrawWorldMap(&io);
    Check(arrows.size()==2 && Bits(arrows[0].a.x)==0x80000000 && Bits(arrows[1].b.x)==0x80000000,"D2 safe query publishes both flushed arrows");
    Check(d.mVehicleTracker.journal.reads==3 && d.mVehicleTracker.reads==3 && arrows[0].colour==0xFF00FF00 && arrows[1].colour==0xFF00FF00,"WorldMap repeated journal reads and safe colours");
}
static void Log() {
#if DEBUG_LOG_BODY_PRESENT
    BrnDirector::DebugLog log;log.mStringIndices.ids={0,1,2};log.mStringPool.data[0].mfTimeLeft=.01f;log.mStringPool.data[1].mfTimeLeft=.02f;log.mStringPool.data[2].mfTimeLeft=.1f;
    log.Update(.1f);Check(log.mStringPool.freed==std::vector<s32>({0,1}) && log.mStringIndices.ids==std::vector<s32>({2}),"D1 adjacent expired log entries are erased without skipping");
    Check(log.mStringPool.data[2].mfTimeLeft==0 && log.mStringPool.reads==4,"D1 equal-lifetime entry survives and repeats pool access");
    log.mStringPool.data[2].mfTimeLeft=std::numeric_limits<f32>::quiet_NaN();log.Update(.1f);
    Check(log.mStringIndices.ids.size()==1 && std::isnan(log.mStringPool.data[2].mfTimeLeft),"D1 unordered log lifetime remains live");
#else
    Check(false,"D1 adjacent-expiry witness unavailable: production body missing");
    Check(false,"D1 equal-lifetime witness unavailable: production body missing");
    Check(false,"D1 unordered-lifetime witness unavailable: production body missing");
#endif
}
int main() {DefaultsAndQA();Junkyard();StateMachines();CoronaAndWorldMap();Log();std::printf("DebugMenuEffectsDirector: %u checks, %u failures\n",checks,failures);return failures?1:0;}
