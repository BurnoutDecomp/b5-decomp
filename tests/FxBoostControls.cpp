#include <cstdio>
#include <cmath>
#include <limits>
#include <algorithm>
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#undef CGS_ASSERT
#define CGS_ASSERT(c,m) do { if (!(c)) ++assertions; } while (0)
int assertions=0;
namespace Nicotine {struct DMixIO {enum {DMX_VOL};};}
namespace BrnSound {namespace Vehicles {namespace Engines {
struct PhysicsControl {
    struct PhysicsData {
        bool IsBlueBoost=false;
        CgsSound::Utils::DataPoint<bool> IsBoosting;
        CgsSound::Utils::DataPoint<float> mDrifting,mSpeedMPH;
        float mfBoostRemaining=0;
    } data;
    const PhysicsData& GetPhysicsData()const{return data;}
};
struct BoostEffect {
    struct State {float mfCurTime=0;} state;
    State* mpState=&state;
    float mfRunningTime=0;
    PhysicsControl* mpPhysicsControl=nullptr;
    float mfParam_AEMS_velocity=0,mfParam_AEMS_start_stage_2=0;
    float mfParam_AEMS_boost_remaining=0,mfParam_AEMS_car_speed=0;
    float mfParam_AEMS_volume=0,mfParam_AEMS_control=2;
    float mfParam_AEMS_time_since_last_boostin=0,mfParam_AEMS_time_since_last_boostout=0;
    float mfParam_AEMS_time_boosting=0,mfParam_AEMS_is_boost_blue=0,mfParam_AEMS_skid_intensity=0;
    CgsSound::Utils::DataPoint<float> mTimeOfLastBoostOut,mTimeOfLastBoostIn,mTimeInBoost;
    int streams=0;
    float GetMixerOutputValue(int,int)const{return 12345;}
    void UpdateBoostStream(){++streams;}
    void UpdateParams(float);
};
#include "fx_boost_controls.inc"
}}}
int main(){
    using namespace BrnSound::Vehicles::Engines;
    int pass=0,total=0;
    const auto check=[&](bool ok,const char*name){++total;if(ok)++pass;else std::printf("FAIL %s\n",name);};
    PhysicsControl p; BoostEffect b;b.mpPhysicsControl=&p;
    const auto update=[&](float time,float dt=0.016f){b.state.mfCurTime=time;b.mfRunningTime=time;b.UpdateParams(dt);};
    p.data.mSpeedMPH.Flush(128);p.data.mfBoostRemaining=.25f;p.data.mDrifting.Flush(.5f);p.data.IsBlueBoost=true;
    update(0);
    check(b.mfParam_AEMS_car_speed==512,"128mph maps to512");
    check(b.mfParam_AEMS_time_boosting==0,"initial elapsed boost time is0");
    check(b.mfParam_AEMS_time_since_last_boostin==0,"initial elapsed since in is0");
    check(b.mfParam_AEMS_time_since_last_boostout==0,"initial elapsed since out is0");
    check(b.mfParam_AEMS_velocity==1024&&b.mfParam_AEMS_start_stage_2==0,"velocity/stage constants");
    check(b.mfParam_AEMS_volume==12345,"mixer volume");
    check(b.mfParam_AEMS_boost_remaining==256&&b.mfParam_AEMS_skid_intensity==512&&b.mfParam_AEMS_is_boost_blue==1,"authored normalized inputs");
    p.data.mSpeedMPH.Flush(256);update(1);check(b.mfParam_AEMS_car_speed==1024,"256mph cap");
    p.data.mSpeedMPH.Flush(320);update(2);check(b.mfParam_AEMS_car_speed==1024,"above cap");
    p.data.mSpeedMPH.Flush(-5);update(3);check(b.mfParam_AEMS_car_speed==-20,"negative speed not lower-clamped");
    p.data.mSpeedMPH.Flush(std::numeric_limits<float>::quiet_NaN());update(4);check(std::isnan(b.mfParam_AEMS_car_speed),"unordered speed keeps input");
    p.data.mSpeedMPH.Flush(100);p.data.IsBoosting.Update(true);update(10,.25f);
    check(b.mfParam_AEMS_control==1,"boost-in edge");
    check(b.mfParam_AEMS_time_boosting==25,"quarter second=25 timer units");
    check(b.mTimeOfLastBoostIn.GetCurrent()==10&&b.mTimeOfLastBoostIn.GetPrevious()==0,"boost-in history");
    check(b.mfParam_AEMS_time_since_last_boostin==1000,"since-in uses previous start");
    p.data.IsBoosting.Update(true);update(10.5f,.5f);check(b.mfParam_AEMS_time_boosting==75,"timer grows while boosting");
    p.data.IsBoosting.Update(false);update(11);
    check(b.mfParam_AEMS_control==2,"boost-out edge");
    check(b.mfParam_AEMS_time_since_last_boostout==0,"time since boost-out starts at0");
    p.data.IsBoosting.Update(false);update(12);
    check(b.mfParam_AEMS_time_since_last_boostout==100,"elapsed after boost-out");
    check(b.mfParam_AEMS_time_boosting==75,"boost duration holds after release");
    p.data.IsBoosting.Update(true);update(20,.1f);
    check(b.mfParam_AEMS_time_since_last_boostin==1000,"second start measures previous boost-in");
    p.data.IsBoosting.Flush(false);b.mTimeInBoost.Flush(500);update(1000);
    check(b.mfParam_AEMS_time_boosting==32767&&b.mfParam_AEMS_time_since_last_boostout==32767,"timers upper saturate");
    b.mTimeInBoost.Flush(-5);update(-1);check(b.mfParam_AEMS_time_boosting==0&&b.mfParam_AEMS_time_since_last_boostout==0,"negative timers lower saturate");
    b.mTimeInBoost.Flush(std::numeric_limits<float>::quiet_NaN());update(0);check(b.mfParam_AEMS_time_boosting==32767,"fsel unordered timer upper limit");
    b.mTimeInBoost.Flush(std::numeric_limits<float>::infinity());update(0);check(b.mfParam_AEMS_time_boosting==32767,"positive infinity saturates");
    b.mTimeInBoost.Flush(-std::numeric_limits<float>::infinity());update(0);check(b.mfParam_AEMS_time_boosting==0,"negative infinity lower limit");
    b.mTimeOfLastBoostOut.Flush(0);b.state.mfCurTime=2;b.mfRunningTime=17;b.UpdateParams(.1f);
    check(b.mfParam_AEMS_time_since_last_boostout==200,"clock is owning state time");
    check(assertions==0&&b.streams==16,"stream update and no asserts");
    std::printf("FxBoostControls: %d checks, %d failures\n",total,total-pass);return pass==total?0:1;
}
