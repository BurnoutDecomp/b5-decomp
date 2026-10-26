#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace CgsDev {
namespace DebugUI {
enum InputEvent {E_INPUTEVENT_NONE,E_INPUTEVENT_DOWN};
struct Metrics {f32 mfAutoRepeatDelay=.3f,mfAutoRepeatRate=.05f,mfAutoRepeatAcceleration=.001f;};
}
struct DebugController {
    DebugUI::Metrics metrics;DebugUI::Metrics* mpMetrics=&metrics;
    DebugUI::InputEvent meLastInputEvent=DebugUI::E_INPUTEVENT_NONE;
    f32 mfEventRepeatDelay=0,mfEventRepeatDelayMax=0;
    f32 mfRepeatTimeScalePC=1;
    DebugUI::InputEvent TranslateControllerInputEvent(){return DebugUI::E_INPUTEVENT_DOWN;}
    DebugUI::InputEvent GetInputEvent(f32);
    void SetRepeatTimeScalePC(f32);
};
}
struct ClockOwner {
    CgsSystem::FrameRateManager mFrameRateManager{};
    CgsSystem::Timer mGameTimer{};
    ClockOwner(f32 scale){mGameTimer.Prepare(1.0f/60);mGameTimer.SetRunning(true);mGameTimer.SetScaleTarget(scale);mGameTimer.Update();}
    f32 Delta() const;
    void Configure(CgsDev::DebugController&) const;
};
#include "debug_ui_frame_time.inc"
static int checks,failures;
static void Check(bool ok,const char* text){++checks;if(!ok){++failures;printf("FAIL %s\n",text);}}
static void Run(int fps,float scale,bool jitter){
    ClockOwner clock(scale);CgsDev::DebugController controller;
    clock.Configure(controller);
    controller.GetInputEvent(0);
    const int ticksBefore=clock.mGameTimer.GetTicks();
    double elapsed=0,firstRepeat=-1,totalDelta=0,maxStep=0;
    int frame=0;
    while(elapsed<2){
        const float dt=jitter?((frame%2)?1.0f/30:1.0f/240):1.0f/fps;
        clock.mFrameRateManager.mfLastFrameSeconds=dt;
        const float actual=clock.Delta();
        Check(std::fabs(actual-dt*scale)<1e-7f,"debug update receives real frame duration with game scaling");
        elapsed+=dt;totalDelta+=actual;if(dt>maxStep)maxStep=dt;
        if(controller.GetInputEvent(actual)!=CgsDev::DebugUI::E_INPUTEVENT_NONE&&firstRepeat<0)firstRepeat=elapsed;
        ++frame;
    }
    Check(std::fabs(totalDelta-elapsed*scale)<2e-5,"UI elapsed time is independent of presentation count");
    Check(firstRepeat>=.3/scale-1e-5&&firstRepeat<=.3/scale+maxStep+1e-5,"0.3-second repeat delay preserves real duration and game multiplier");
    Check(clock.mGameTimer.GetTicks()==ticksBefore,"debug updates do not advance the simulation timer");
    printf("cadence %d FPS%s scale=%.1f repeat=%.6f real seconds\n",fps,jitter?" jitter":"",scale,firstRepeat);
}
static int CountRepeats(int fps,int seconds,float acceleration,float scale=1){
    CgsDev::DebugController controller;controller.metrics.mfAutoRepeatAcceleration=acceleration;
    ClockOwner clock(scale);clock.mFrameRateManager.mfLastFrameSeconds=1.0f/fps;clock.Configure(controller);
    int events=controller.GetInputEvent(0)!=CgsDev::DebugUI::E_INPUTEVENT_NONE;
    for(int frame=0;frame<fps*seconds;++frame)
        events+=controller.GetInputEvent(clock.Delta())!=CgsDev::DebugUI::E_INPUTEVENT_NONE;
    return events;
}
int main(){
    for(int fps:{30,60,120,144,240})for(float scale:{.5f,1.0f,2.0f})Run(fps,scale,false);
    Run(0,1,true);
    ClockOwner paused(0);paused.mFrameRateManager.mfLastFrameSeconds=1.0f/144;
    Check(paused.Delta()==0,"zero game-time multiplier retained");
    ClockOwner zero(1);zero.mFrameRateManager.mfLastFrameSeconds=0;
    Check(zero.Delta()==0,"zero-duration frame does not consume UI time");
    for(int fps:{30,60,120,144,240}){
        Check(std::abs(CountRepeats(fps,10,0)-CountRepeats(60,10,0))<=1,"repeat overshoot preserves average cadence at non-divisor frame rates");
        Check(std::abs(CountRepeats(fps,10,.001f)-CountRepeats(60,10,.001f))<=1,"repeat acceleration is independent of presentation rate");
    }
    for(float scale:{.5f,1.0f,2.0f})for(int fps:{60,120,144,240})
        Check(std::abs(CountRepeats(fps,3,2,scale)-CountRepeats(60,3,2,scale))<=1,"maximum repeat speed retains the console's real-time 60 Hz ceiling at every game scale");
    CgsDev::DebugController stopped;
    Check(stopped.GetInputEvent(0)==CgsDev::DebugUI::E_INPUTEVENT_DOWN,"a newly pressed key is immediate even without elapsed time");
    Check(stopped.GetInputEvent(0)==CgsDev::DebugUI::E_INPUTEVENT_NONE,"zero-duration presentation does not repeat a held key");
    Check(stopped.GetInputEvent(1)==CgsDev::DebugUI::E_INPUTEVENT_DOWN,"a long frame emits at most one repeat");
    Check(stopped.GetInputEvent(.001f)==CgsDev::DebugUI::E_INPUTEVENT_NONE,"a long frame does not queue deferred repeats");
    printf("DebugUiFrameTime: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
