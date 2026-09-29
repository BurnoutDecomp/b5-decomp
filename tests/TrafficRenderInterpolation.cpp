// Presentation-only history: no game assets, no physics or render-device stubs.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficRenderPosePC.h"
#include <cstdio>
#include <cstring>

using namespace rw::math::vpu;
static int checks, failures;
static void Check(bool okay, const char* label)
{
    ++checks;
    if (!okay) { ++failures; std::printf("FAIL %s\n", label); }
}
static bool Near(float a, float b) { return std::fabs(a-b) < 0.0002f; }
static Matrix44Affine Pose(float x, float angle = 0)
{
    auto pose = MakeRotationY(angle);
    pose.wAxis.x = x;
    return pose;
}
int main()
{
    BrnTraffic::RenderPosePC history;
    Vector4 angles = {};
    auto first = Pose(10);
    Check(Near(history.SampleBody(first,0.5f).wAxis.x,10),"no history uses the authoritative pose");
    history.Latch(first,angles,nullptr,nullptr);
    Check(Near(history.SampleBody(first,0.5f).wAxis.x,10),"first sample does not smear from zero");
    auto current = Pose(20,1.4f); // an 80-degree tumble must still interpolate
    history.Latch(current,angles,nullptr,nullptr);
    const auto original = current;
    for (int frame=0; frame<=8; ++frame)
    {
        const float alpha = frame/8.0f;
        const auto display = history.SampleBody(current,alpha);
        Check(Near(display.wAxis.x,10+10*alpha),"body advances on every render between ticks");
        const auto expected = MakeRotationY(1.4f*alpha);
        Check(Near(display.xAxis.x,expected.xAxis.x) && Near(display.xAxis.z,expected.xAxis.z),"crash rotation follows the sampled arc");
        const auto repeated = history.SampleBody(current,alpha);
        Check(std::memcmp(&display,&repeated,sizeof(display))==0,"main/shadow/reflection passes use an identical pose");
    }
    Check(std::memcmp(&current,&original,sizeof(current))==0,"render interpolation never overwrites simulation input");
    auto endpoint = history.SampleBody(current,1);
    Check(std::memcmp(&endpoint,&current,sizeof(current))==0,"console-locked alpha one is bit-identical");

    // Several physics steps can happen between renders. Use the last two ticks,
    // not the last two times this vehicle happened to be visible.
    history.Latch(Pose(30),angles,nullptr,nullptr);
    history.Latch(Pose(40),angles,nullptr,nullptr);
    history.Latch(Pose(50),angles,nullptr,nullptr);
    Check(Near(history.SampleBody(Pose(50),0.5f).wAxis.x,45),"skipped render/visibility frames do not lengthen history");
    history.Latch(Pose(50),angles,nullptr,nullptr);
    Check(Near(history.SampleBody(Pose(50),0.25f).wAxis.x,50),"unchanged or paused ticks do not drift backwards");

    history.Reset();
    angles.w=6.2f;
    history.Latch(Pose(0),angles,nullptr,nullptr);
    angles.w=0.1f;
    history.Latch(Pose(10),angles,nullptr,nullptr);
    const auto spin = history.SampleAngles(angles,0.5f);
    Check(std::cos(spin.w)>0.99f,"normal traffic wheel phase crosses 2pi without reversing half a turn");
    Check(Near(history.SampleAngles(angles,1).w,0.1f),"wheel angle endpoint remains exact");

    Matrix44Affine wheels[4] = {Pose(22),Pose(18),Pose(22),Pose(18)};
    bool exists[4] = {true,true,true,true};
    history.Latch(Pose(20),angles,wheels,exists); // first physical sample after contact
    Check(Near(history.SampleBody(Pose(20),0.5f).wAxis.x,15),"body stays continuous on physical promotion");
    Check(Near(history.SampleWheel(0,wheels[0],0.5f).wAxis.x,17),"first physical wheels share the chassis tick fraction");
    const auto wheelInput = wheels[0];
    wheels[0]=Pose(32);
    wheels[1]=Pose(28);
    history.Latch(Pose(30),angles,wheels,exists);
    for (int frame=0;frame<=4;++frame)
    {
        float alpha=frame/4.0f;
        const auto body=history.SampleBody(Pose(30),alpha);
        const auto wheel=history.SampleWheel(0,wheels[0],alpha);
        Check(Near(wheel.wAxis.x-body.wAxis.x,2),"crashing wheel stays aligned with interpolated body");
    }
    Check(Near(wheelInput.wAxis.x,22) && Near(wheels[0].wAxis.x,32),"wheel inputs stay authoritative");
    exists[0]=false;
    history.Latch(Pose(40),angles,wheels,exists);
    Check(Near(history.SampleWheel(0,Pose(100),0.5f).wAxis.x,100),"removed wheel cannot reuse a stale history");
    history.Latch(Pose(50),angles,nullptr,nullptr);
    Check(Near(history.SampleBody(Pose(50),0.5f).wAxis.x,45),"recovery to nonphysical traffic preserves body continuity");

    history.Reset(); // despawn / recycled vehicle slot / traffic reset
    history.Latch(Pose(1000),angles,nullptr,nullptr);
    Check(Near(history.SampleBody(Pose(1000),0.2f).wAxis.x,1000),"recycled slot snaps to new vehicle without old-car trails");
    history.Reset();
    wheels[0]=Pose(1002); exists[0]=true;
    history.Latch(Pose(1000),angles,wheels,exists);
    Check(Near(history.SampleWheel(0,wheels[0],0.5f).wAxis.x,1002),"first physical spawn has no stale wheel endpoint");
    std::printf("TrafficRenderInterpolation: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
