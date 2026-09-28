// Reverb effect state machine: independently controlled controllers, presets,
// time and mixer; the extracted production methods perform all tested writes.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
using f32 = float; using s32 = int; using u32 = uint32_t;
int gAsserts = 0;
#define CGS_ASSERT(c, m) do { if (!(c)) ++gAsserts; } while (0)
namespace CgsDev {
namespace Log { struct Stream { template<class T> Stream& operator<<(const T&) {return *this;} }; Stream output; auto* gpDebugPrint = &output; }
struct Render { void Draw2DText(const char*, float, float, float, unsigned) {} };
struct DebugInterface { Render render; Render& Get2dRender() { return render; } };
}
namespace CgsSound {
namespace Playback {
struct Name {
    static unsigned MakeHash(const char* s) {
        if (!std::strcmp(s, "ParamReverbTime")) return 10;
        if (!std::strcmp(s, "ParamReverbSpaceSize")) return 11;
        if (!std::strcmp(s, "ParamReverbBrightness")) return 12;
        if (!std::strcmp(s, "Send01")) return 13;
        return 999;
    }
};
}
namespace Utils {
struct Curve { enum ECurveType { E_LINEAR, E_POWER }; };
template<class T> struct DataPoint {
    T now{}, previous{};
    void Update(T value) { previous = now; now = value; }
    bool HasChanged() const { return now != previous; }
    T GetCurrent() const { return now; }
};
struct InterpolateLine {
    float mfElapsedTime=0, mfLength=.01f, mfStart=1, mfFinish=1;
    Curve::ECurveType meCurveTypes=Curve::E_LINEAR;
    float mfCurrentValue=1; bool mbComplete=false;
    // A clock spy: only completion is modeled; fade curves have their own tests.
    void Update(float dt) { mfElapsedTime+=dt; if (mfElapsedTime>mfLength) { mbComplete=true; mfCurrentValue=mfFinish; } }
    void Initialize(float start, float finish, float ms, Curve::ECurveType curve) {
        mfStart=mfCurrentValue=start; mfFinish=finish; mfLength=ms>0?ms*.001f:.01f;
        mfElapsedTime=0; meCurveTypes=curve; mbComplete=false;
    }
    float GetValueFloat() const { return mfCurrentValue; }
    bool IsFinished() const { return mbComplete; }
};
struct SlopeParams { float low, high, from, to; SlopeParams(float a,float b,float c,float d):low(a),high(b),from(c),to(d){} };
struct Slope {
    SlopeParams p; explicit Slope(SlopeParams a):p(a){}
    float GetValue(float input, Curve::ECurveType) const {
        float x=(input-p.low)/(p.high-p.low);
        x=-x>=0?0:x; x=1-x>=0?x:1; return std::fmaf(x,p.to-p.from,p.from);
    }
};
}
}
namespace Attrib { namespace Gen {
struct Preset { float time, space, gain, brightness; };
struct reverbparams {
    Preset p; explicit reverbparams(const Preset& a):p(a){}
    float Time() const{return p.time;} float SpaceSize() const{return p.space;}
    float Gain() const{return p.gain;} float Brightness() const{return p.brightness;}
};
struct Global {
    Preset presets[12]; int reads=0, last=-1;
    const Preset& ReverbSettings(int i) { ++reads; last=i; return presets[i]; }
};
}}
namespace Nicotine { struct DMixIO { enum { DMX_VOL=0 }; }; }
namespace BrnSound {
namespace Module {
struct Voice {
    float params[3]{}, gain=-1; unsigned names[4]{}; int writes=0, send=-1;
    void SetParameter(int i,float v,const unsigned* n) { params[i]=v; names[i]=*n; ++writes; }
    void SetGain(unsigned s,float v,const unsigned* n) { gain=v; send=s; names[3]=*n; ++writes; }
    bool IsReady() const { return true; }
};
struct SoundLogicModule {
    Attrib::Gen::Global data; Voice voice;
    auto& GetGlobalData(){return data;} auto& GetGlobalReverbVoice(){return voice;}
};
}
namespace Vehicles { namespace Environment {
bool SndEnvDiagBudget(int&) { return false; }
struct Physics {
    struct Data { CgsSound::Utils::DataPoint<float> mSpeedMPH; } data;
    const Data& GetPhysicsData() const { return data; }
};
struct ReverbEffect {
    enum { E_REVERB_STATE_NONE, E_REVERB_STATE_INTERPOLATING };
    float mfTime=0, mfSpaceSize=15, mfBrightness=1, mfGain=1, mix=.5f;
    CgsSound::Utils::InterpolateLine mInterpolateReverb;
    int meReverbState=0, wanted=0;
    CgsSound::Utils::DataPoint<int> mReverbType{11,11};
    Physics* mpPhysicsControl; void* mpLogicModule;
    int mixSlot=-1, mixPreset=-1;
    int GetActiveReverb() const { return wanted; }
    float GetRWACMixerOutputValue(int slot,int preset) { mixSlot=slot; mixPreset=preset; return mix; }
    void UpdateParams(float); void ProcessUpdate();
};
#include "fx_reverb.inc"
}}}
#include "fx_reverb_attribute.inc"
int main() {
    using namespace BrnSound::Vehicles::Environment;
    using CgsSound::Utils::Curve;
    int total=0,failures=0;
    auto check=[&](bool ok,const char* name){++total;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    auto near=[](float x,float y){return std::fabs(x-y)<1e-6f;};
    ReverbAttributeTest::Class attribClass{0xA59AD4BD63B62A88ull};
    ReverbAttributeTest::Collection collection{&attribClass};
    ReverbAttributeTest::Accessor accessor{&collection};
    check(accessor.GetClass()==0xA59AD4BD63B62A88ull,"reverb class identity retains both words");
    collection.mpClass=nullptr;
    check(accessor.GetClass()==0,"null class has zero identity");
    accessor.mpCollection=nullptr;
    check(accessor.GetClass()==0,"null collection has zero identity");
    BrnSound::Module::SoundLogicModule module;
    for(int i=0;i<12;++i)module.data.presets[i]={.25f+i,15.f+i,.7f+i*.01f,.3f+i*.01f};
    Physics physics; ReverbEffect fx; fx.mpPhysicsControl=&physics;fx.mpLogicModule=&module;
    fx.UpdateParams(0);
    check(fx.meReverbState==1,"first update starts transition");
    check(fx.mReverbType.now==0&&fx.mReverbType.previous==11,"preset history advances");
    check(near(fx.mInterpolateReverb.mfLength,.5f),"zero MPH fades over 500 ms");
    check(fx.mInterpolateReverb.meCurveTypes==Curve::E_POWER,"original POWER fade curve");
    check(module.data.reads==0&&fx.mfTime==0,"old preset retained during fade");
    fx.UpdateParams(.501f);
    check(module.data.reads==1&&module.data.last==0,"completed fade resolves selected RefSpec");
    check(near(fx.mfTime,.25f)&&near(fx.mfSpaceSize,15),"time and space load from preset");
    check(near(fx.mfGain,.7f)&&near(fx.mfBrightness,.3f),"gain and brightness not interchanged");
    check(fx.meReverbState==0&&fx.mInterpolateReverb.GetValueFloat()==1,"new preset restores wet envelope");
    check(near(fx.mInterpolateReverb.mfLength,.01f)&&fx.mInterpolateReverb.meCurveTypes==Curve::E_LINEAR,"constant envelope uses original 10 ms floor");
    fx.UpdateParams(.02f);check(module.data.reads==1,"unchanged preset not reloaded");
    physics.data.mSpeedMPH.now=60;fx.wanted=1;fx.UpdateParams(0);
    check(near(fx.mInterpolateReverb.mfLength,.1f),"60 MPH fades over 100 ms");
    physics.data.mSpeedMPH.now=30;fx.mInterpolateReverb.mfCurrentValue=.4f;fx.wanted=2;fx.UpdateParams(0);
    check(near(fx.mInterpolateReverb.mfLength,.12f),"interrupted fade scales remaining level");
    check(fx.mInterpolateReverb.mfStart==.4f&&fx.mInterpolateReverb.mfFinish==0,"new fade starts at current envelope");
    fx.mInterpolateReverb.mfCurrentValue=0;fx.wanted=3;fx.UpdateParams(0);
    check(near(fx.mInterpolateReverb.mfLength,.01f),"zero level has 10 ms floor");
    fx.mInterpolateReverb.mfCurrentValue=std::numeric_limits<float>::quiet_NaN();fx.wanted=4;fx.UpdateParams(0);
    check(near(fx.mInterpolateReverb.mfLength,.01f),"unordered duration has 10 ms floor");
    physics.data.mSpeedMPH.now=-10;fx.mInterpolateReverb.mfCurrentValue=1;fx.wanted=5;fx.UpdateParams(0);
    check(near(fx.mInterpolateReverb.mfLength,.5f),"negative speed clamps low");
    physics.data.mSpeedMPH.now=100;fx.wanted=6;fx.UpdateParams(0);
    check(near(fx.mInterpolateReverb.mfLength,.1f),"above 60 speed clamps high");
    physics.data.mSpeedMPH.now=std::numeric_limits<float>::quiet_NaN();fx.wanted=7;fx.UpdateParams(0);
    check(near(fx.mInterpolateReverb.mfLength,.1f),"NaN speed follows fsel high endpoint");
    fx.mfTime=2.3f;fx.mfSpaceSize=40;fx.mfBrightness=.2f;fx.mfGain=.8f;
    fx.mInterpolateReverb.mfCurrentValue=.25f;fx.mix=.5f;fx.ProcessUpdate();
    check(module.voice.writes==4,"three parameters and wet send written");
    check(module.voice.params[0]==2.3f&&module.voice.params[1]==40&&module.voice.params[2]==.2f,"voice receives all authored parameters");
    check(module.voice.names[0]==10&&module.voice.names[1]==11&&module.voice.names[2]==12&&module.voice.names[3]==13,"voice parameter and send names agree");
    check(near(module.voice.gain,.1f)&&module.voice.send==0,"gain is envelope times preset times mixer");
    check(fx.mixSlot==0&&fx.mixPreset==0,"volume output zero selected");
    fx.mix=0;fx.ProcessUpdate();check(module.voice.gain==0,"zero mixer output silences reverb");
    KB_DEBUG_REVERB_ZONE=true;fx.wanted=8;fx.UpdateParams(0);
    check(module.data.last==8,"debug preset resolves immediately");
    check(near(fx.mfTime,8.25f)&&near(fx.mfSpaceSize,23),"debug preset refreshes parameters");
    KB_DEBUG_REVERB_ZONE=false;
    check(gAsserts==0,"valid flow raises no assertion");
    std::printf("FxReverb: %d checks, %d failures\n",total,failures);
    return failures?1:0;
}
