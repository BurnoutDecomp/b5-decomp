#include <cmath>
#include <cstdio>
#include <limits>
#include <cstring>
#include <cstdint>
#include "BrnCommonTypes.h"
#include "rw/math/vpu/matrix44affine_operation.h"

static int checks = 0, failures = 0, assertions = 0;
#undef CGS_ASSERT
#define CGS_ASSERT(c, msg) do { if (!(c)) ++assertions; } while (0)
static void check(bool value, const char* text)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", text); }
}
static bool close(const Vector3& a, const Vector3& b)
{
    return std::fabs(a.x-b.x)<0.0005f && std::fabs(a.y-b.y)<0.0005f &&
           std::fabs(a.z-b.z)<0.0005f;
}
namespace CgsSound { namespace Logic {
struct Cgs3dEffectControl {
    const Vector3* mpEmitterPosition = nullptr;
    void AttachEmitterPosition(const Vector3* p) { mpEmitterPosition=p; }
};
}}
namespace BrnSound { namespace Logic {
struct Brn3DEffectControl : CgsSound::Logic::Cgs3dEffectControl {
    int updates = 0;
    Vector3 sampled{};
    void UpdateParams(f32) { ++updates; if(mpEmitterPosition) sampled=*mpEmitterPosition; }
};
struct Brn3DUserSpaceEffectControl : Brn3DEffectControl {
    const Matrix44Affine* mpTransform = nullptr;
    Vector3 mPositionInUserSpace{}, mDirectionInUserSpace{};
    Vector3 mGeneratedPosition{}, mGeneratedDirection{};
    void AttachTransform(const Matrix44Affine*);
    void AttachEmitterPosition(const Vector3*);
    void UpdateParams(f32);
};
#include "fx_collision_space.inc"
}}
using BrnSound::Logic::Brn3DUserSpaceEffectControl;
static Matrix44Affine identity()
{
    Matrix44Affine m;
    m.xAxis={1,0,0,0}; m.yAxis={0,1,0,0}; m.zAxis={0,0,1,0}; m.wAxis={0,0,0,0};
    return m;
}
static void scenario(Matrix44Affine m, Vector3 world, Vector3 local)
{
    Brn3DUserSpaceEffectControl c;
    c.mGeneratedPosition={17,19,23,0};
    c.AttachTransform(&m);
    c.AttachEmitterPosition(&world);
    check(c.mpTransform==&m,"attached transform identity");
    check(close(c.mPositionInUserSpace,local),"world point becomes local");
    check(close(c.mGeneratedPosition,{17,19,23,0}),"attach leaves generated position until update");
    check(c.mpEmitterPosition==&c.mGeneratedPosition,"stable generated member binding");
    c.UpdateParams(1.0f/60.0f);
    check(close(c.sampled,world),"first frame remains at the real collision");
    check(c.updates==1,"common positional control runs once");
    world={-999,-999,-999,0}; // Caller storage is only a snapshot, never retained.
    m.wAxis.x+=10; m.wAxis.y+=2; m.wAxis.z-=5;
    Vector3 expected=c.sampled; expected.x+=10; expected.y+=2; expected.z-=5;
    c.UpdateParams(1.0f/60.0f);
    check(close(c.sampled,expected),"emitter follows its frame without double translation");
}
int main()
{
    auto m=identity();
    scenario(m,{2,3,4,99},{2,3,4,0});
    m.wAxis={3249.75f,-3.5f,-1925.25f,0};
    scenario(m,{3251.75f,-2.5f,-1928.25f,0},{2,1,-3,0});
    m.xAxis={0,0,-1,0}; m.zAxis={1,0,0,0};
    scenario(m,{3251.75f,-2.5f,-1928.25f,0},{3,1,2,0});
    m.xAxis={0.6f,0,0.8f,0}; m.zAxis={-0.8f,0,0.6f,0};
    m.wAxis={320,-3,-913,0};
    scenario(m,{321.375f,-4.25f,-912.625f,0},{1.125f,-1.25f,-0.875f,0});
    // Golden scalar lanes from the six VMX operations at 82696BFC..6C10.
    // Unfused SDK inverse/transform helpers yield different bits in both lanes.
    Brn3DUserSpaceEffectControl precise; precise.AttachTransform(&m);
    Vector3 point={321.375f,-4.25f,-912.625f,0};
    precise.AttachEmitterPosition(&point);
    std::uint32_t x=0,z=0;
    std::memcpy(&x,&precise.mPositionInUserSpace.x,4);
    std::memcpy(&z,&precise.mPositionInUserSpace.z,4);
    check(x==0x3f9000d8,"original fused X lane");
    check(z==0xbf5ffca0,"original fused Z lane");
    m=identity(); Brn3DUserSpaceEffectControl c; c.AttachTransform(&m);
    Vector3 tiny={std::numeric_limits<float>::denorm_min(),0,0,0};
    c.AttachEmitterPosition(&tiny);
    check(c.mPositionInUserSpace.x==0,"VMX flushes subnormal world input");
    check(assertions==0,"valid fixture triggers no original asserts");
    std::printf("FxCollisionSpace: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
