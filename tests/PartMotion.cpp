// Production pose and publication methods; integrator/velocity limiter are
// captured neighbors. The output queue and event are the real production types.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"
#undef protected
#undef private
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <memory>

namespace vpu = rw::math::vpu;
static unsigned assertions=0, limitCalls=0, integrateCalls=0;
static bool limitResult=false;
static VecFloat capturedDt{};
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
} }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
namespace BrnPhysics {
void ExternalPhysicsBody::CalculateNewVelocity(VecFloat dt)
{ ++integrateCalls; capturedDt=dt; SetLinearVelocity({2,3,4,5}); SetAngularVelocity({6,7,8,9}); }
namespace Deformation {
bool PhysicalBodyPart::LimitVelocities(VecFloat) { ++limitCalls; return limitResult; }
// Only the old source snapshot calls this discarded-output hook.
void EmitUpdateExternalBodyEvent(CgsPhysics::PhysicsSimulationIO::InputBuffer*,const void*) {}
} }
struct TransformFixture
{
    Matrix44Affine mPreviousTransform, mTransform;
    Matrix44Affine GetTransformDelta() const;
};
struct OwnerFixture
{
    TransformFixture vehicle;
    const TransformFixture* GetVehiclePhysics() const { return &vehicle; }
};
using BrnPhysics::Deformation::PhysicalBodyPart;
struct PartPoseFixture
{
    decltype(PhysicalBodyPart::mRwBody) mRwBody;
    decltype(PhysicalBodyPart::mWorldPenetrationPlusCollisionMagnitude) mWorldPenetrationPlusCollisionMagnitude;
    decltype(PhysicalBodyPart::mAverageCollisionPointPlusNumCollisions) mAverageCollisionPointPlusNumCollisions;
    const OwnerFixture* mpDeformableObject;
    void PostVehicleUpdate();
};
#include "part_motion_methods.inc"

int main()
{
    unsigned checks=0,failures=0;
    auto Check=[&](bool pass,const char* name){++checks;if(!pass){++failures;std::fprintf(stderr,"FAIL: %s\n",name);}};
    auto Vector=[&](Vector3 a,Vector3 b,const char* name){Check(std::fabs(a.x-b.x)<.0001f && std::fabs(a.y-b.y)<.0001f && std::fabs(a.z-b.z)<.0001f && std::fabs(a.w-b.w)<.0001f,name);};
    OwnerFixture owner;
    owner.vehicle.mPreviousTransform.SetIdentity();
    owner.vehicle.mPreviousTransform.wAxis={100,200,300,0};
    owner.vehicle.mTransform.SetIdentity();
    owner.vehicle.mTransform.xAxis={0,0,-1,0};
    owner.vehicle.mTransform.zAxis={1,0,0,0};
    owner.vehicle.mTransform.wAxis={110,220,330,0};
    PartPoseFixture part;
    part.mpDeformableObject=&owner;
    Matrix44Affine pose; pose.SetIdentity(); pose.wAxis={102,203,304,0};
    part.mRwBody.SetTransform(pose);
    part.mRwBody.SetLinearVelocity({1,2,3,4});
    part.mRwBody.SetAngularVelocity({5,6,7,8});
    part.mWorldPenetrationPlusCollisionMagnitude.SetVector3({1,2,3,0});
    part.mWorldPenetrationPlusCollisionMagnitude.SetPlus(4);
    part.mAverageCollisionPointPlusNumCollisions.SetVector3({5,6,7,0});
    part.mAverageCollisionPointPlusNumCollisions.SetPlus(8);
    part.PostVehicleUpdate();
    const auto moved=part.mRwBody.GetTransform();
    Vector(moved.xAxis,{0,0,-1,0},"part orientation follows vehicle");
    Vector(moved.yAxis,{0,1,0,0},"part up axis");
    Vector(moved.zAxis,{1,0,0,0},"part forward axis");
    Vector(moved.wAxis,{114,223,328,0},"part offset rotates about vehicle, then translates");
    Vector(part.mWorldPenetrationPlusCollisionMagnitude.GetVector3(),{},"penetration reset");
    Vector(part.mAverageCollisionPointPlusNumCollisions.GetVector3(),{},"contact point reset");
    Check(part.mWorldPenetrationPlusCollisionMagnitude.GetPlus()==0 && part.mAverageCollisionPointPlusNumCollisions.GetPlus()==0,"both accumulator scalar lanes reset");
    Vector(part.mRwBody.GetLinearVelocity(),{1,2,3,4},"pose update preserves linear velocity");
    Vector(part.mRwBody.GetAngularVelocity(),{5,6,7,8},"pose update preserves angular velocity");

    using CgsPhysics::PhysicsSimulationIO::InputBuffer;
    auto input=std::make_unique<InputBuffer>();
    input->mUpdateExternalBodyQueue.Construct();
    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    PhysicalBodyPart body{};
    body.mRigidBodyId.muEntityWord=0x06001c02; body.mRigidBodyId.muSubA=0x1234; body.mRigidBodyId.muSubB=0xabcd;
    body.mRwBody.SetTransform(pose);
    VecFloat dt{.02f,.02f,.02f,.02f};
    body.mbNeedsWritingIntoRenderware=false; limitResult=false;
    body.UpdateRW(input.get(),dt);
    Check(limitCalls==1 && integrateCalls==0 && input->mUpdateExternalBodyQueue.GetLength()==0,"clean unclamped part emits nothing");
    body.mbNeedsWritingIntoRenderware=true;
    body.UpdateRW(input.get(),dt);
    Check(limitCalls==1 && integrateCalls==1,"dirty part skips limiter and integrates once");
    Check(capturedDt.x==.02f && capturedDt.w==.02f,"integrator receives timestep");
    Check(!body.mbNeedsWritingIntoRenderware,"dirty flag consumed");
    Check(input->mUpdateExternalBodyQueue.GetLength()==1,"simulation event actually enqueued");
    if(input->mUpdateExternalBodyQueue.GetLength()==1){
        const auto& event=input->mUpdateExternalBodyQueue.GetEvent(0);
        Check(event.mID==0x06001c021234abcdull,"full body ID preserved");
        Vector(event.mTransform.wAxis,pose.wAxis,"event carries part pose");
        Vector(event.mVel,{2,3,4,5},"event contains integrated linear velocity");
        Vector(event.mAngularVel,{6,7,8,9},"event contains integrated angular velocity");
    }
    limitResult=true;
    body.UpdateRW(input.get(),dt);
    Check(limitCalls==2 && integrateCalls==2 && input->mUpdateExternalBodyQueue.GetLength()==2,"clamped clean part also publishes");
    Check(assertions==0,"valid writes need no assertions");
    input->mxStatusFlags.UnSetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    input->GetUpdateExternalBodyQueue();
    Check(assertions==1,"write accessor checks lock");
    std::printf("PartMotion: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
