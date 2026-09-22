// ARTIST 0x825BA5C0 velocity-cap regression. Execute production LimitVelocities
// and UpdateRW with real body/queue types; only force integration is a captured neighbor.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>

static unsigned assertions=0, integrations=0;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
} }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
namespace BrnPhysics {
void ExternalPhysicsBody::CalculateNewVelocity(VecFloat) { ++integrations; }
}
#include "part_velocity_methods.inc"

int main()
{
    using BrnPhysics::Deformation::PhysicalBodyPart;
    unsigned checks=0, failures=0;
    auto Check=[&](bool pass,const char* name){++checks;if(!pass){++failures;std::fprintf(stderr,"FAIL: %s\n",name);}};
    auto Near=[](float a,float b){return std::fabs(a-b)<.0002f;};
    auto Vector=[&](Vector3 a,Vector3 b,const char* name){Check(Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z)&&Near(a.w,b.w),name);};
    const float inf=std::numeric_limits<float>::infinity();
    const float nan=std::numeric_limits<float>::quiet_NaN();
    // Independent expected caps from the original rodata and fsel endpoints.
    struct Row {float dt,linear,angular;};
    const Row rows[]={{0,120,100},{1.f/240,105,76.25f},{1.f/120,90,52.5f},
                      {1.f/60,60,5},{1.f/30,60,5},{.1f,60,5},
                      {-.01f,120,100},{inf,60,5},{-inf,120,100},{nan,120,100}};
    for(const auto& row:rows)
    {
        PhysicalBodyPart part{};
        part.mRwBody.SetLinearVelocity({120,160,0,40}); // magnitude 200
        part.mRwBody.SetAngularVelocity({0,-120,160,-60});
        Check(part.LimitVelocities({row.dt,row.dt,row.dt,row.dt}),"over-limit part requires update");
        Vector(part.mRwBody.GetLinearVelocity(),{.48f*row.linear,.64f*row.linear,0,.16f*row.linear},"linear cap and full vector store");
        Vector(part.mRwBody.GetAngularVelocity(),{0,-.48f*row.angular,.64f*row.angular,-.24f*row.angular},"angular cap and full vector store");
        Check(!part.LimitVelocities({row.dt,row.dt,row.dt,row.dt}),"clamped part remains below cap");
        part.mbJoinedToVehicle=true;
        part.mRwBody.SetLinearVelocity({120,160,0,40});
        part.mRwBody.SetAngularVelocity({0,-120,160,-60});
        Check(!part.LimitVelocities({row.dt,row.dt,row.dt,row.dt}),"joined part bypasses free-body limiter");
        Vector(part.mRwBody.GetLinearVelocity(),{120,160,0,40},"joined linear velocity preserved");
        Vector(part.mRwBody.GetAngularVelocity(),{0,-120,160,-60},"joined angular velocity preserved");
    }
    PhysicalBodyPart part{};
    const VecFloat dt{1.f/30,1.f/30,1.f/30,1.f/30};
    part.mRwBody.SetLinearVelocity({60,0,0,7});
    part.mRwBody.SetAngularVelocity({0,5,0,9});
    Check(!part.LimitVelocities(dt),"exact caps do not clamp (strict greater-than)");
    Vector(part.mRwBody.GetLinearVelocity(),{60,0,0,7},"boundary linear unchanged");
    Vector(part.mRwBody.GetAngularVelocity(),{0,5,0,9},"boundary angular unchanged");
    part.mRwBody.SetLinearVelocity({std::nextafter(60.f,inf),0,0,0});
    Check(part.LimitVelocities(dt),"one ULP above linear cap clamps");
    Vector(part.mRwBody.GetLinearVelocity(),{48,0,0,0},"linear clamp retains 80 percent of cap");
    part.mRwBody.SetAngularVelocity({0,std::nextafter(5.f,inf),0,0});
    Check(part.LimitVelocities(dt),"one ULP above angular cap clamps");
    Vector(part.mRwBody.GetAngularVelocity(),{0,4,0,0},"angular clamp retains 80 percent of cap");

    // The former 120/100 selections skipped this real UpdateRW queue path.
    using CgsPhysics::PhysicsSimulationIO::InputBuffer;
    auto input=std::make_unique<InputBuffer>();
    input->mUpdateExternalBodyQueue.Construct();
    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    part.mRigidBodyId.muEntityWord=0x06001c02;
    part.mRigidBodyId.muSubA=0x1234; part.mRigidBodyId.muSubB=0xabcd;
    Matrix44Affine pose; pose.SetIdentity(); pose.wAxis={20,30,40,0};
    part.mRwBody.SetTransform(pose);
    part.mRwBody.SetLinearVelocity({75,0,0,15});
    part.mRwBody.SetAngularVelocity({0,8,0,2});
    part.UpdateRW(input.get(),dt);
    Check(integrations==1 && input->mUpdateExternalBodyQueue.GetLength()==1,"real limiter triggers simulation publication");
    if(input->mUpdateExternalBodyQueue.GetLength()==1){
        const auto& event=input->mUpdateExternalBodyQueue.GetEvent(0);
        Check(event.mID==0x06001c021234abcdull,"published body ID preserved");
        Vector(event.mVel,{48,0,0,9.6f},"published linear velocity is capped");
        Vector(event.mAngularVel,{0,4,0,1},"published angular velocity is capped");
        Vector(event.mTransform.wAxis,{20,30,40,0},"publication preserves pose");
    }
    part.UpdateRW(input.get(),dt);
    Check(integrations==1 && input->mUpdateExternalBodyQueue.GetLength()==1,"no repeated event after velocities settle");
    Check(assertions==0,"all timestep cases satisfy original range assertions");
    std::printf("PartVelocity: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
