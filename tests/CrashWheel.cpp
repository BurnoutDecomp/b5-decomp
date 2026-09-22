#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"
#undef protected
#undef private
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <initializer_list>
unsigned assertions=0,checks=0,failures=0;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}int FireAssert(const char*,const char*,int){++assertions;return 0;}void* EndAssert(){return nullptr;}
} namespace Log {DebugPrint* gpDebugPrint=nullptr;} }
using BrnPhysics::Vehicle::Wheel;
struct VehicleFixture {
    Vector4 mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare{0,.75f,0,0};
    Vector4 mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction{0,0,0,.75f};
    VecFloat speed{1,1,1,1};bool crashing=true;Wheel wheels[4]{};
    VecFloat GetSpeedMPH()const{return speed;} bool IsCrashing()const{return crashing;}
    Wheel* GetWheel(BrnPhysics::Vehicle::EVehicleDrivenWheel index){return &wheels[index];}
};
struct Fixture {
    VehicleFixture vehicle;u8 owner=1; s8 miNumBrokenWheels=0;u16 mu16DeformableObjectIndex=0;
    u8 GetHandlingBodyIdHighByte()const{return owner;}
    void Update(s32,CgsNumeric::Random&);
};
#include "crash_wheel_methods.inc"
void Check(bool yes,const char* msg){++checks;if(!yes){++failures;std::fprintf(stderr,"FAIL: %s\n",msg);}}
void Seed(CgsNumeric::Random& rng,float value){rng.Construct();rng.muOldestBufferIndex=1;rng.mafFloatBuffer[4]=value+1;}
int main(){
    for(float fraction : {.25f,.5f,.75f})for(unsigned state : {0u,1u,2u}){
        Fixture f;CgsNumeric::Random rng;Seed(rng,fraction);const auto seed=rng.muSeed;
        const auto oldAssert=assertions;const int chosen=fraction>.5f?0:1;
        for(auto& w:f.vehicle.wheels){w.mu8State=0;w.mIntegrationVariables={8,7,6,5};w.SetTwistAmount(.25f);}
        f.vehicle.wheels[chosen].mu8State=static_cast<u8>(state);f.Update(3,rng);
        Check(rng.muSeed==seed*CgsNumeric::KU_RANDOM_MULTIPLIER+1&&rng.muOldestBufferIndex==5,"one original vector-slot RNG draw");
        Check(f.vehicle.wheels[chosen].mu8State==(state==2?2:1),"chosen front wheel twists unless detached");
        Check(f.miNumBrokenWheels==(state==2?0:1),"count increments only for successful twist");
        Check(assertions-oldAssert==(state==2?1:0),"detached wheel assertion is non-gating with original skip");
        Check(f.vehicle.wheels[chosen].mIntegrationVariables.x==(state==2?8.f:0.f),"twist resets spin");
        Check(f.vehicle.wheels[chosen].mIntegrationVariables.y==7&&f.vehicle.wheels[chosen].mIntegrationVariables.z==6&&f.vehicle.wheels[chosen].mIntegrationVariables.w==5,"twist preserves remaining integration lanes");
        for(int i=0;i<4;++i)Check(f.vehicle.wheels[i].GetTwistAmount()==.25f,"no invented twist-angle write");
        Check(f.vehicle.wheels[1-chosen].IsAttached()&&f.vehicle.wheels[2].IsAttached()&&f.vehicle.wheels[3].IsAttached(),"other wheels untouched");
        if(state!=2){const auto next=rng.muSeed;f.Update(3,rng);Check(next==rng.muSeed&&f.miNumBrokenWheels==1,"only one broken wheel fallback");}
    }
    // Every gate prevents both wheel mutation and RNG consumption, including strict bounds.
    for(int gate=0;gate<14;++gate){
        Fixture f;CgsNumeric::Random rng;Seed(rng,.75f);int mode=3;
        if(gate==0)f.owner=2;if(gate==1)mode=2;if(gate==2)f.miNumBrokenWheels=1;
        if(gate==3)f.vehicle.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y=.5f;
        if(gate==4)f.vehicle.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y=1;
        if(gate==5)f.vehicle.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y=std::numeric_limits<float>::quiet_NaN();
        if(gate>=6&&gate<=9)(&f.vehicle.speed.x)[gate-6]=0;
        if(gate==10)f.vehicle.mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.w=.5f;
        if(gate==11)f.vehicle.crashing=false;
        if(gate==12)f.vehicle.speed.x=std::numeric_limits<float>::quiet_NaN();
        if(gate==13)f.vehicle.mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.w=std::numeric_limits<float>::quiet_NaN();
        const auto seed=rng.muSeed;const auto count=f.miNumBrokenWheels;const auto oldAssert=assertions;f.Update(mode,rng);
        Check(seed==rng.muSeed&&rng.muOldestBufferIndex==1,"failed gate does not consume RNG");
        Check(count==f.miNumBrokenWheels&&f.vehicle.wheels[0].IsAttached()&&f.vehicle.wheels[1].IsAttached(),"failed gate preserves wheels and count");
        Check(assertions==oldAssert,"failed gate does not assert");
    }
    std::printf("CrashWheel: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
