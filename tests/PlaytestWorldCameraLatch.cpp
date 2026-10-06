// Production PC world-camera latch against the original whole-camera copy.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include <cmath>
#include <cstdio>

struct CameraFixture
{
    Matrix44Affine mTransform;
    struct Effects { bool mbSetTimeOfDay=false;f32 mfTimeOfDay=0; } effects;
    u32 mState_uFlags=0;
    Effects& GetEffects() { return effects; }
};
struct LatchFixture
{
    CameraFixture mLastCameraInput;
    bool mbBringUpCameraSetTimeOfDayBringUp=true;
    f32 mfBringUpCameraTimeOfDayHoursBringUp=18;
    u32 muBringUpCameraStateFlagsBringUp=0x400000;
    void Latch(bool lbUseDirectorCamera,const Matrix44Affine& lDirectorTransform,
               Vector3 lPvsPosition,Vector3 lEye,Vector3 lLookAt)
    {
#include "world_camera_latch_body.inc"
    }
};
static u32 suChecks=0,suFailures=0;
static void Check(bool ok,const char* message)
{
    ++suChecks;
    if(!ok){++suFailures;if(suFailures<12)std::fprintf(stderr,"%s\n",message);}
}
static Matrix44Affine Rotated(f32 yaw,f32 pitch,f32 roll)
{
    const f32 cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll);
    Matrix44Affine m;
    m.xAxis={cr*cy+sr*sp*sy,sr*cp,-cr*sy+sr*sp*cy,0};
    m.yAxis={-sr*cy+cr*sp*sy,cr*cp,sr*sy+cr*sp*cy,0};
    m.zAxis={cp*sy,-sp,cp*cy,0};
    m.wAxis={123,45,-678,1};
    return m;
}
static f32 Dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
int main()
{
    const f32 angles[][3]={{0,0,0},{1.57079637f,0,0},{-1.57079637f,0,0},{3.14159274f,0,0},
                           {.8f,.4f,.3f},{-.8f,-.4f,-.3f}};
    LatchFixture f;
    for(const auto& angle:angles)
    {
        const auto staged=Rotated(angle[0],angle[1],angle[2]);
        f.mLastCameraInput.mTransform.SetIdentity();
        const Vector3 eye={staged.wAxis.x,staged.wAxis.y,staged.wAxis.z,0};
        const Vector3 look={eye.x+staged.zAxis.x,eye.y+staged.zAxis.y,eye.z+staged.zAxis.z,0};
        f.Latch(true,staged,eye,eye,look);
        const auto* actual=reinterpret_cast<const f32*>(&f.mLastCameraInput.mTransform);
        const auto* expected=reinterpret_cast<const f32*>(&staged);
        for(u32 i=0;i<16;++i)Check(actual[i]==expected[i],"director camera row differs from whole-copy source");
        const auto& m=f.mLastCameraInput.mTransform;
        Check(std::fabs(Dot(m.xAxis,m.yAxis))<0.000001f && std::fabs(Dot(m.xAxis,m.zAxis))<0.000001f && std::fabs(Dot(m.yAxis,m.zAxis))<0.000001f,
              "rotated camera basis is not orthogonal");
        Check(m.wAxis.x==eye.x && m.wAxis.y==eye.y && m.wAxis.z==eye.z,"director streamer eye changed");
        Check(f.mLastCameraInput.GetEffects().mbSetTimeOfDay && f.mLastCameraInput.GetEffects().mfTimeOfDay==18 && f.mLastCameraInput.mState_uFlags==0x400000,
              "existing camera effects/flags latch changed");
    }
    f.mLastCameraInput.mTransform.SetIdentity();
    const auto staged=Rotated(.8f,.4f,.3f);
    f.Latch(false,staged,{7,8,9,0},{0,0,0,0},{0,0,5,0});
    const auto& m=f.mLastCameraInput.mTransform;
    Check(m.wAxis.x==7 && m.wAxis.y==8 && m.wAxis.z==9 && m.wAxis.w==0,"synthetic streamer position changed");
    Check(m.zAxis.x==0 && m.zAxis.y==0 && m.zAxis.z==1,"synthetic direction normalization changed");
    Check(m.xAxis.x==1 && m.yAxis.y==1,"synthetic camera inherited staged director axes");
    std::printf("World camera latch: %s (%u checks, %u failures)\n",suFailures?"FAIL":"PASS",suChecks,suFailures);
    return suFailures?1:0;
}
