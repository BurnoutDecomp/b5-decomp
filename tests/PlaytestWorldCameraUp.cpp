#include "BrnCommonTypes.h"
#include <cstdio>
#include <cmath>
#include <initializer_list>
static unsigned guChecks,guFailures;
struct FixtureCamera {
    Vector3 eye,up,target;
    void LookAt(Vector3 e,Vector3 u,Vector3 t) {eye=e;up=u;target=t;}
};
static void Deliver(bool lbUseDirectorCamera,const rw::math::vpu::Matrix44Affine& lDirectorTransform,
                    Vector3 lEye,Vector3 lLookAt,FixtureCamera& sBringUpCamera) {
#include "playtest_world_camera_up_body.inc"
}
static void Check(bool v,const char* s) {++guChecks;if(!v){++guFailures;std::printf("FAIL %s\n",s);}}
static bool Same(Vector3 a,Vector3 b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
int main() {
    for(float roll:{0.f,0.7f,1.5707963f,2.1f,3.1415927f}) {
        rw::math::vpu::Matrix44Affine transform;transform.SetIdentity();
        transform.Right()={std::cos(roll),std::sin(roll),0,0};
        transform.Up()={-std::sin(roll),std::cos(roll),0,0};
        transform.Pos()={55,3,-99,0};
        Vector3 target={55,3,-98,0};FixtureCamera camera;
        Deliver(true,transform,transform.Pos(),target,camera);
        Check(Same(camera.up,transform.Up()),"original director Up including roll delivered");
        Check(Same(camera.eye,transform.Pos())&&Same(camera.target,target),"director eye/target unchanged");
        Check(std::fabs(camera.up.x*transform.Right().x+camera.up.y*transform.Right().y)<0.000001f,"rolled up retains original orthogonal basis");
        const Vector3 syntheticEye={100,20,30,0},syntheticTarget={100,20,35,0};
        Deliver(false,transform,syntheticEye,syntheticTarget,camera);
        Check(Same(camera.up,Vector3{0,1,0,0}),"synthetic world Up retained");
        Check(Same(camera.eye,syntheticEye)&&Same(camera.target,syntheticTarget),"synthetic eye/target/distance unchanged");
    }
    std::printf("PlaytestWorldCameraUp: %u checks, %u failures\n",guChecks,guFailures);return guFailures?1:0;
}
