// Production pool traversal and local force accumulator, with UpdateRW captured
// at its boundary to verify force ordering without substituting the body math.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"
#undef protected
#undef private
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <memory>

static unsigned assertions=0, visits=0;
static const BrnPhysics::Deformation::PhysicalBodyPart* visited[50];
static Vector3 forceBefore[50];
static VecFloat timeSteps[50];
namespace vpu=rw::math::vpu;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
} }
namespace BrnPhysics { namespace Deformation {
void PhysicalBodyPart::UpdateRW(CgsPhysics::PhysicsSimulationIO::InputBuffer*,VecFloat dt)
{ visited[visits]=this; forceBefore[visits]=mRwBody.mTotalLinearForce; timeSteps[visits++]=dt; }
} }
#include "part_gravity_methods.inc"

int main()
{
    using namespace BrnPhysics::Deformation;
    unsigned checks=0,failures=0;
    auto Check=[&](bool pass,const char* name){++checks;if(!pass){++failures;std::fprintf(stderr,"FAIL: %s\n",name);}};
    auto Vector=[&](Vector3 a,Vector3 b,const char* name){Check(std::fabs(a.x-b.x)<.0001f&&std::fabs(a.y-b.y)<.0001f&&std::fabs(a.z-b.z)<.0001f&&std::fabs(a.w-b.w)<.0001f,name);};
    auto pool=std::make_unique<PhysicalBodyPartPool>();
    pool->mUsedParts.UnSetAll();
    VecFloat dt{.02f,.02f,.02f,.02f};
    pool->UpdateRWBodies(nullptr,dt);
    Check(visits==0,"empty pool visits nothing");
    const unsigned slots[]={0,32,49};
    const float masses[]={2,5,11};
    const Vector3 ups[]={{0,1,0,0},{1,0,0,0},{0,-1,0,0}};
    for(unsigned i=0;i<3;++i){
        pool->mUsedParts.SetBit(slots[i]);
        auto& body=pool->maParts[slots[i]].mRwBody;
        Matrix44Affine transform; transform.SetIdentity();
        if(i==1){transform.xAxis={0,-1,0,0};transform.yAxis=ups[i];}
        if(i==2){transform.xAxis={-1,0,0,0};transform.yAxis=ups[i];}
        body.SetTransform(transform);
        body.mfMass={masses[i],masses[i],masses[i],masses[i]};
    }
    for(float gravity:{0.f,-9.81f,3.f}){
        kfPartExtraGravity=gravity;
        visits=0;
        for(auto slot:slots)pool->maParts[slot].mRwBody.mTotalLinearForce={1,2,3,4};
        pool->UpdateRWBodies(nullptr,dt);
        Check(visits==3,"all used slots visited exactly once");
        for(unsigned i=0;i<3;++i){
            Check(visited[i]==&pool->maParts[slots[i]],"sparse used-slot order preserved");
            Vector(forceBefore[i],{1,2,3,4},"UpdateRW runs before additional gravity");
            Check(timeSteps[i].x==.02f&&timeSteps[i].w==.02f,"timestep forwarded");
            const float force=gravity*masses[i];
            Vector(pool->maParts[slots[i]].mRwBody.mTotalLinearForce,
                   {1+ups[i].x*force,2+ups[i].y*force,3+ups[i].z*force,4},
                   "mass-scaled local gravity rotates and accumulates");
        }
    }
    Check(assertions==0,"no assertions for valid pool and forces");
    std::printf("PartGravity: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
