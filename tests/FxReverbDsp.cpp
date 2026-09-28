#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include "rw/audio/core/ReverbFilters.h"
#include "rw/audio/core/DelayLine.h"
#include "rw/audio/core/IFilter.h"
namespace rw { namespace audio { namespace core {
const f32 KF_ZERO=0.0f, KF_DENORMAL_FLUSH=1.0e-18f;
#include "fx_reverb_dsp.inc"
}}}
using namespace rw::audio::core;
int main() {
    int total=0,failures=0;
    auto check=[&](bool ok,const char* n){++total;if(!ok){++failures;std::printf("FAIL %s\n",n);}};
    check(std::is_base_of<IFilter,CombFilter>::value,"comb carries native IFilter base");
    check(std::is_base_of<IFilter,AllPassFilter>::value,"allpass carries native IFilter base");
    check(sizeof(CombFilter::Context)==sizeof(DelayLine::TapContext),"comb context keeps all six pointers");
    check(sizeof(AllPassFilter::Context)==sizeof(DelayLine::TapContext),"allpass context keeps all six pointers");
    CombFilter comb; AllPassFilter allpass;
    CombFilter::CombFilter_ctor(&comb); AllPassFilter::AllPassFilter_ctor(&allpass);
    auto* ci=reinterpret_cast<IFilter*>(&comb);auto* ai=reinterpret_cast<IFilter*>(&allpass);
    check(ci->miReadLength==2&&ci->miChannels==1,"comb constructor initializes native filter fields");
    check(ai->miReadLength==1&&ai->miChannels==1,"allpass constructor initializes native filter fields");
    // Real ApplyFilter context in the caller's layout; old layouts are rejected
    // above before a call could reproduce the null-pointer write from the live run.
    float input[4]={1,2,3,4},tap[5]={.5f,.75f,1,1.25f,1.5f},feedback[4]{},output[4]{};
    DelayLine::TapContext ctx{input,tap,nullptr,nullptr,feedback,output};
    const bool abi=sizeof(CombFilter::Context)==sizeof(ctx);
    if(abi) {
        CombFilter::SetGains(&comb,0,0,0,1);
        CombFilter::CombFilterApplyFunc(&comb,4,0,0,reinterpret_cast<CombFilter::Context*>(&ctx));
    }
    check(abi&&feedback[0]==1&&feedback[3]==4,"comb writes caller feedback buffer");
    check(abi&&output[0]==.75f&&output[3]==1.5f,"comb writes caller output buffer");
    if(abi) {
        AllPassFilter::SetGains(&allpass,0,1);
        AllPassFilter::AllPassFilterApplyFunc(&allpass,4,0,0,reinterpret_cast<AllPassFilter::Context*>(&ctx));
    }
    check(abi&&output[0]==.5f&&output[3]==1.25f,"allpass writes correct output");
    ctx.mpTap2=tap;
    if(abi)CombFilter::CombFilterApplyFunc(&comb,4,0,0,reinterpret_cast<CombFilter::Context*>(&ctx));
    check(abi&&output[0]==0&&output[3]==0,"crossfade comb branch clears output");
    output[0]=output[3]=1;
    if(abi)AllPassFilter::AllPassFilterApplyFunc(&allpass,4,0,0,reinterpret_cast<AllPassFilter::Context*>(&ctx));
    check(abi&&output[0]==0&&output[3]==0,"crossfade allpass branch clears output");
    // Double intermediates independently model the console's one-rounding
    // scalar fused instructions for these bounded single-precision operands.
    auto fma=[](float a,float b,float c){return static_cast<float>(double(a)*double(b)+double(c));};
    const float g1=.573812f,g2=.937197f,g3=-.318367f,g4=.623719f;
    float source[32],work[33],actual[32],expect[32],accum[32],expectAccum[32];
    for(int i=0;i<33;++i){work[i]=(i-11)*.0773127f;if(i<32){source[i]=(i%5-2)*.617231f;}}
    for(int add=0;add<2;++add) {
        float state=.391231f;
        for(int i=0;i<32;++i) {
            actual[i]=expect[i]=float(i)*.127681f;
            state=fma(-work[i+1],g2,fma(-state,g1,source[i]));expectAccum[i]=state;
            float t=fma(work[i],g3,work[i+1]);expect[i]=add?fma(t,g4,expect[i]):t*g4;
        }
        float result=CombFilterFunc(32,g1,g2,g3,g4,.391231f,source,work,accum,actual,add);
        check(std::memcmp(accum,expectAccum,sizeof(accum))==0,"comb fused feedback matches ARTIST");
        check(std::memcmp(actual,expect,sizeof(actual))==0&&result==state,"comb output and final state match ARTIST");
    }
    for(int add=0;add<2;++add) {
        float w[33];std::memcpy(w,work,sizeof(w));
        for(int i=0;i<32;++i) {
            actual[i]=expect[i]=float(i)*.127681f;
            float bias=w[i]+KF_DENORMAL_FLUSH;float x=bias-KF_DENORMAL_FLUSH;
            float y=fma(-g1,x,source[i]);float yb=y+KF_DENORMAL_FLUSH;
            expectAccum[i]=yb-KF_DENORMAL_FLUSH;float t=fma(expectAccum[i],g1,x);
            expect[i]=add?fma(t,g2,expect[i]):t*g2;
        }
        AllPassFilterFunc(32,g1,g2,source,w,accum,actual,add);
        check(std::memcmp(actual,expect,sizeof(actual))==0&&std::memcmp(accum,expectAccum,sizeof(accum))==0,"allpass fused recurrence matches ARTIST");
    }
    comb.mfState=1;CombFilter::CombFilterResetFunc(&comb);check(comb.mfState==0,"comb reset clears history");
    std::printf("FxReverbDsp: %d checks, %d failures\n",total,failures);return failures?1:0;
}
