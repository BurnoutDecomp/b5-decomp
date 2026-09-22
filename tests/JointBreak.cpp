// Extracted production decision; neighboring IK predicates and AddToSim are observations.
// Real joint spec, ExternalPhysicsBody, packed vectors, notification queue and event.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <initializer_list>
#include <cstdio>
using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;
static unsigned assertions=0,checks=0,failures=0;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;} int FireAssert(const char*,const char*,int){++assertions;return 0;} void* EndAssert(){return nullptr;}
} }
static bool DetachProbeOn(){return false;}
static unsigned JbDecade(float){return 0;}
static u32 gxJbCalls=0,gxJbNeverBreak=0,gxJbRotGate=0,gxJbType3=0,gxJbSensorGate=0,gxJbAxisIdle=0,gxJbArmA=0,gxJbBreak=0;
static u32 gxJbRotHist[7]{},gxJbForceHist[7]{},gxJbPenHist[7]{};
static float kfJointForceMultiplier=.4f,kfJointPenetrationMultiplier=1.5f;
struct IKFixture {
    DeformationJointSpec joint{}; int type=0; bool sensor=true,toughSeen=false; unsigned calls=0;
    unsigned GetActiveJointIndex()const{return 0;}
    const DeformationJointSpec* GetActiveJointSpec()const{return &joint;}
    int GetPartType()const{return type;}
    bool CheckSensorForcesForJointDetachment(bool tough){++calls;toughSeen=tough;return sensor;}
};
struct OwnerFixture { ExternalPhysicsBody body; const ExternalPhysicsBody& GetVehicleBody()const{return body;} };
struct OutputFixture { DeformationOutputInterface out; DeformationOutputInterface* GetDeformationOutputInterface(){return &out;} };
struct JointBreakFixture {
    ExternalPhysicsBody mRwBody; IKFixture* mpIKPart; OwnerFixture* mpDeformableObject;
    BurnoutBodyPartID mRigidBodyId{}; EntityId mGlobalVehicleId{};
    rw::math::vpu::Vector3Plus mWorldPenetrationPlusCollisionMagnitude{},mLocalInitialJointPositionPlusLimitStress{};
    bool mbJoinedToVehicle=true; float proportion=1; unsigned simCalls=0;
    Matrix44Affine capturedTransform; Vector3 capturedLinear{},capturedAngular{};
    VecFloat GetJointRotationProportion()const{return {proportion,proportion,proportion,proportion};}
    Matrix44Affine GetRigidBodyTransform()const{return mRwBody.GetTransform();}
    Vector3 GetLinearVelocity()const{return mRwBody.GetLinearVelocity();}
    void AddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer*,const Matrix44Affine& transform,Vector3 linear,Vector3 angular){++simCalls;capturedTransform=transform;capturedLinear=linear;capturedAngular=angular;}
    bool TestJointForBreaking(CgsPhysics::PhysicsSimulationIO::InputBuffer*,OutputFixture*);
};
#include "joint_break_methods.inc"
void Check(bool pass,const char* name){++checks;if(!pass){++failures;std::fprintf(stderr,"FAIL: %s\n",name);}}
void Vector(Vector3 a,Vector3 b,const char* name){Check(std::fabs(a.x-b.x)<.0001f&&std::fabs(a.y-b.y)<.0001f&&std::fabs(a.z-b.z)<.0001f,name);}
int main(){
    // Exercise both breaking arms with independent axis-aligned analytic point velocities.
    for(int axis=0;axis<3;++axis)for(int sign : {-1,1})for(bool force : {false,true}){
        IKFixture ik;OwnerFixture owner;OutputFixture output;JointBreakFixture part;
        output.out.mDetachedPartNotificationQueue.Construct();
        part.mpIKPart=&ik;part.mpDeformableObject=&owner;part.mGlobalVehicleId.muValue=0x01000c00;
        part.mRigidBodyId.muEntityWord=0x06000c02;
        ik.joint.mfJointDetachThreshold=1;
        Matrix44Affine vehicle;vehicle.SetIdentity();vehicle.wAxis={100,200,300,0};owner.body.SetTransform(vehicle);
        owner.body.SetLinearVelocity({10,20,30,0});
        Vector3 omega{},lever{},expected{10,20,30,0};
        if(axis==0){omega={2.f*sign,0,0,0};lever={0,3,0,0};expected.z+=6*sign;}
        if(axis==1){omega={0,2.f*sign,0,0};lever={0,0,3,0};expected.x+=6*sign;}
        if(axis==2){omega={0,0,2.f*sign,0};lever={3,0,0,0};expected.y+=6*sign;}
        owner.body.SetAngularVelocity(omega);
        Matrix44Affine pose=vehicle;pose.wAxis=vehicle.wAxis+lever;part.mRwBody.SetTransform(pose);
        part.mRwBody.SetLinearVelocity({-99,-88,-77,0});part.mRwBody.SetAngularVelocity({-66,-55,-44,0});
        ik.joint.mJointAxis={1,0,0,0};
        part.mWorldPenetrationPlusCollisionMagnitude.SetVector3(force?Vector3{1,0,0,0}:Vector3{});
        part.mLocalInitialJointPositionPlusLimitStress.SetPlus(force?0.f:2.f);
        Check(part.TestJointForBreaking(nullptr,&output),"force or penetration breaks joint");
        Check(part.simCalls==1&&!part.mbJoinedToVehicle,"handoff once and unjoin");
        Vector(part.capturedLinear,expected,"inherits vehicle point velocity");
        Vector(part.capturedAngular,omega,"inherits vehicle angular velocity");
        Check(output.out.mDetachedPartNotificationQueue.GetLength()==1,"one notification");
        const auto& event=output.out.mDetachedPartNotificationQueue.GetEvent(0);
        Vector(event.mPointOnA,pose.wAxis,"notification at part origin");
        Check(event.mVehicleId.muValue==part.mGlobalVehicleId.muValue,"notification vehicle preserved");
    }
    // Early gates and below-threshold result must not publish a simulator handoff.
    for(int gate=0;gate<5;++gate){
        IKFixture ik;OwnerFixture owner;OutputFixture output;JointBreakFixture part;
        output.out.mDetachedPartNotificationQueue.Construct();part.mpIKPart=&ik;part.mpDeformableObject=&owner;
        Matrix44Affine pose;pose.SetIdentity();owner.body.SetTransform(pose);part.mRwBody.SetTransform(pose);
        owner.body.SetLinearVelocity({});owner.body.SetAngularVelocity({});ik.joint.mJointAxis={1,0,0,0};ik.joint.mfJointDetachThreshold=1;
        part.mLocalInitialJointPositionPlusLimitStress.SetPlus(gate==4?0.f:2.f);
        if(gate==0)ik.joint.mfJointDetachThreshold=-1;
        if(gate==1)part.proportion=.2f;
        if(gate==2)ik.type=3;
        if(gate==3)ik.sensor=false;
        Check(!part.TestJointForBreaking(nullptr,&output),"no-break gate");
        Check(part.simCalls==0&&part.mbJoinedToVehicle&&output.out.mDetachedPartNotificationQueue.GetLength()==0,"no-break preserves state and queues");
    }
    Check(assertions==0,"valid fixtures produce no assertions");
    std::printf("JointBreak: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
