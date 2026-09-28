#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
using u8=uint8_t; using u16=uint16_t; using s16=int16_t; using s32=int32_t; using f32=float;
float gfAemsStepMilliseconds=10.0f;
#include "fx_aems_envelope.inc"
int main()
{
    // Extra backing keeps the old unsigned -1 release-index bug in bounds.
    alignas(8) u8 data[4096] = {};
    auto* b=reinterpret_cast<EnvelopeBlock*>(data);
    auto* control=reinterpret_cast<s32*>(data+80);
    int total=0,failures=0;
    auto check=[&](bool ok,const char* name){++total;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    auto init=[&](){
        std::memset(data,0,sizeof(data));b->muControlOffset=80;b->muPointCount=2;b->muReleasePoint=1;
        const f32 points[]={10,100,110,100,0};std::memcpy(b->maPoints,points,sizeof(points));
    };
    init();*control=1;
    check(UpdateEnvelope(b)==10,"start emits initial value");
    check(b->mfTimeRemaining==100,"start does not consume a tick");
    check(b->mfStep==10,"initial slope");
    check(UpdateEnvelope(b)==20,"second tick advances");
    *control=2;check(UpdateEnvelope(b)==20,"pause holds value");
    check(b->mfTimeRemaining==90,"pause holds clock");
    *control=1;check(UpdateEnvelope(b)==30,"resume continues instead of restarting");
    check(b->mfTimeRemaining==80,"resume continues clock");
    *control=3;check(UpdateEnvelope(b)==30,"release transition holds current value");
    check(b->muPoint==1 && b->mfTimeRemaining==100,"release enters target segment without a tick");
    check(b->mfStep==-3,"release slope starts from current value");
    check(UpdateEnvelope(b)==27,"release second tick advances");
    for(int i=0;i<8;++i)UpdateEnvelope(b);
    check(UpdateEnvelope(b)==0,"finished envelope is zero on final transition tick");
    check(b->muPoint==2,"finished point index");
    check(UpdateEnvelope(b)==0,"finished stays silent");
    *control=1;check(UpdateEnvelope(b)==0,"release-to-play does not restart");
    *control=0;check(UpdateEnvelope(b)==0,"stop clears output");
    *control=1;check(UpdateEnvelope(b)==10,"stopped envelope restarts");
    init();b->muPointCount=1;b->maPoints[1]=10;b->maPoints[2]=123;*control=1;
    check(UpdateEnvelope(b)==10,"one-segment initial value");
    check(UpdateEnvelope(b)==0,"nonzero final target is cleared immediately");
    init();b->muReleasePoint=0xffff;b->mfValue=37;b->mfStep=2;b->mfTimeRemaining=40;*control=3;
    check(UpdateEnvelope(b)==39,"negative release sentinel advances current segment");
    check(b->muPoint==0,"negative release sentinel is not an index");
    init();b->maPoints[1]=0;*control=1;check(UpdateEnvelope(b)==10,"zero-duration start keeps initial value");
    check(std::isinf(b->mfStep),"zero duration retains original division result");
    std::printf("FxAemsEnvelope: %d checks, %d failures\n",total,failures);
    return failures?1:0;
}
