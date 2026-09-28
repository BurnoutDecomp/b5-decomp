#include <cmath>
#include <cstdio>
#include <cstddef>
#include <vector>
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerTypes.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
int assertions=0, checks=0, failures=0;
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint=nullptr; } namespace Message {u64 gxMessageFilterFlags=0;} }
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;} int FireAssert(const char*,const char*,int){++assertions;return 0;} void* EndAssert(){return nullptr;}
}}
void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
namespace rw {
struct Resource {void* m_baseResources[5];};
namespace collision {
struct Volume {struct VTable{u32 muTypeID;};u32 muVTableSlot=1,muSurfaceID=0,muGroupID=0;int fixture=0;};
Volume::VTable normal{3},aggregate{6}; Volume::VTable* gVolumeVTable[8]={nullptr,&normal,&aggregate};
namespace VolRef {struct Vec4 {float x,y,z,w;};}
struct VolumeLineSegIntersectResult {VolRef::Vec4 position,normal;float lineParam;struct {size_t muVolumePtr;}vRef;};
struct VolumeLineQuery {
    std::vector<std::vector<VolumeLineSegIntersectResult>> batches[4];int calls=0,batch=0,which=0;bool inputsOk=true;
    VolumeLineSegIntersectResult* m_resBuffer=nullptr;
    void InitQuery(const Volume**v,const Matrix44Affine**m,int n,VolRef::Vec4 s,VolRef::Vec4 e,float f){
        ++calls;which=v[0]->fixture;batch=0;inputsOk &= n==1&&f==0&&s.x==2&&e.x==8&&m[0]->wAxis.x==which;
    }
    bool Finished()const{return batch==(int)batches[which].size();}
    u32 GetAllIntersections(){auto&b=batches[which][batch++];m_resBuffer=b.data();return (u32)b.size();}
};
float extents[3];struct BoxVolume {static BoxVolume* Initialize(Resource&,float x,float y,float z){extents[0]=x;extents[1]=y;extents[2]=z;static BoxVolume b;return &b;}};
}}
float VectorMagnitude(float x,float y,float z){return std::sqrt(x*x+y*y+z*z);}
namespace BrnWorld {using namespace TriggerEntityModuleIO;
void DecodeBox(const InAddTriggerEvent*lpEvent){float lfEntityBoundingSphereRadius;u8 lu8TriggerTypeFlag;alignas(16)u8 laVolumeStorage[128];bool lbVolumeOk;
switch(2){
#include "fx_sound_trigger_box.inc"
}}
}
namespace CgsSceneManager {
struct VolumeInstance{Matrix44Affine mWorldSpaceTransform;s32 miVolumeIndex,miNextEntityVolumeInstance;};
struct EntityManager {
u32 ids[4]={0x01000400,0x01000401,0x01000800,0x02000400};int first[4]={0,1,2,-1};VolumeInstance instances[4];
EntityId GetEntityIdByIndex(u16 i)const{return EntityId(ids[i]);}
const VolumeInstance*GetVolumeInstance(s32 i)const{return i<0?nullptr:&instances[i];}
const VolumeInstance*GetFirstEntityVolumeInstance(u16 i,s32*out)const{*out=first[i];return GetVolumeInstance(*out);}
};
struct VolumeManager {u8 flags[4]={4,4,4,1};rw::collision::Volume volumes[4];
u8 GetVolumeTypeFlags(s32 i)const{return flags[i];}const rw::collision::Volume*GetRwVolume(s32 i)const{return &volumes[i];}};
namespace FineIntersectionTestIO {
struct OutputBuffer {using LineTestIntersectionArray=Array<LineTestIntersection,256>;};
struct InEventLineTestFine {Vector3 mLineStart,mLineEnd;SceneQueryId mQueryId;const u16*mpau16EntityIndices;u16 mu16NumEntities,mu16ExcludeEntityIndex;u8 mxVolumeTypeFlags;bool mbExcludeParts;};
struct OutEventLineTestFineResult {SceneQueryId mQueryId;s32 miNumResults;const LineTestIntersection*mpaResults;};
}
struct FineIntersectionTestModule {
using InEventLineTestFine=FineIntersectionTestIO::InEventLineTestFine;using OutEventLineTestFineResult=FineIntersectionTestIO::OutEventLineTestFineResult;
EntityManager*mpEntityManager;VolumeManager*mpVolumeManager;rw::collision::VolumeLineQuery*mpVolumeLineQuery;
void ComputeLineTestFine(const InEventLineTestFine*,OutEventLineTestFineResult*,void*);
};
#include "fx_sound_trigger_fine.inc"
}
struct RetireFixture {
BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface management;
CgsModule::VariableEventQueue<4096,16> query;
auto*GetTriggerManagementInputInterface(){return &management;}auto*GetTriggerQueryInputInterface(){return &query;}
};
void Retire(RetireFixture*lpGameStateOutput){
#include "fx_sound_trigger_retire.inc"
}
struct RegistrationFixture {
u64 volume=0,instance=0,instanceVolume=0;Vector3 centre{};
void AddDynamicVolume(CgsSceneManager::EntityId id,const void*,u8){volume=u32(id);}
void AddDynamicVolume(CgsSceneManager::VolumeId id,const void*,u8){volume=id.mId;}
void AddEntity(CgsSceneManager::EntityId,u32,float){}
void AddEntity(CgsSceneManager::EntityId,u32,Vector3 p,float){centre=p;}
void AddVolumeInstance(CgsSceneManager::EntityId,const Matrix44Affine&){}
void AddVolumeInstance(CgsSceneManager::VolumeInstanceId i,CgsSceneManager::VolumeId v,const Matrix44Affine&){instance=i.muId;instanceVolume=v.mId;}
};
void RegisterTrigger(RegistrationFixture*lpOutSceneInputInterface){
    CgsSceneManager::EntityId lEntityId(0x04000800);
    struct Trigger {CgsSceneManager::VolumeInstanceId mVolumeInstanceID;Matrix44Affine mTransform;}lrTrigger;
    lrTrigger.mTransform.SetIdentity();lrTrigger.mTransform.Pos()={3013,-7.5f,-1150,0};
    alignas(16)u8 laVolumeStorage[128];u8 lu8TriggerTypeFlag=4;u32 KU_ENTITYTYPEFLAG_TRIGGER=32;float lfEntityBoundingSphereRadius=3.5f;
#include "fx_sound_trigger_register.inc"
}
int main(){
    using namespace BrnWorld::TriggerEntityModuleIO;
    InAddBoxTriggerEvent box{};box.mDimensions.x=20;box.mDimensions.y=8;box.mDimensions.z=12;
    // Run the production consumer's entire BOX arm on the producer's actual event type.
    BrnWorld::DecodeBox(reinterpret_cast<InAddTriggerEvent*>(&box));
    Check(rw::collision::extents[0]==10,"box width round-trip");Check(rw::collision::extents[1]==4,"box height round-trip");Check(rw::collision::extents[2]==6,"box depth round-trip");
    Check(offsetof(InAddBoxTriggerEvent,mDimensions)==80&&sizeof(box)==96,"box payload ABI");
    RetireFixture lifecycle;auto&add=lifecycle.management.GetAddTriggerEventQueue();auto&remove=lifecycle.management.GetRemoveTriggerEventQueue();
    add.Construct();remove.Construct();lifecycle.query.Construct();
    add.AddEvent(&box,2);InRemoveTriggerEvent rm{};remove.AddEvent(rm);InLineTestEvent q{};lifecycle.query.AddEvent(&q,3);
    Retire(&lifecycle);const CgsModule::Event*event;int size;
    add.GetFirstEvent(&event,&size);Check(!event,"add-trigger queue retired");
    lifecycle.query.GetFirstEvent(&event,&size);Check(!event,"motion-query queue retired");
    Check(remove.GetLength()==0,"remove-trigger queue retired");
    RegistrationFixture registration;RegisterTrigger(&registration);
    Check(registration.volume==0x0400080000000000ull,"registration preserves full64-bit ID");
    Check(registration.instance==registration.volume,"real instance registration");
    Check(registration.instanceVolume==0x0400080000000000ull,"instance references registered volume");
    Check(registration.centre.x==3013&&registration.centre.y==-7.5f&&registration.centre.z==-1150,"broadphase uses trigger position");
    using namespace CgsSceneManager;
    EntityManager em;VolumeManager vm;rw::collision::VolumeLineQuery walk;
    for(int i=0;i<4;++i){em.instances[i].miVolumeIndex=i;em.instances[i].miNextEntityVolumeInstance=-1;em.instances[i].mWorldSpaceTransform.wAxis.x=float(i);vm.volumes[i].fixture=i;}
    em.instances[0].miNextEntityVolumeInstance=3;
    vm.volumes[0].muSurfaceID=0x123456;vm.volumes[0].muGroupID=0xabcdef;
    auto hit=[&](float t,size_t p){return rw::collision::VolumeLineSegIntersectResult{{t,2,3,7},{4,5,6,8},t,{p}};};
    walk.batches[0]={{hit(.7f,(size_t)&vm.volumes[0])},{hit(.2f,0)}};
    walk.batches[1]={{hit(.8f,0)}};walk.batches[2]={{hit(.3f,0)}};walk.batches[3]={{hit(.1f,0)}};
    FineIntersectionTestModule module{&em,&vm,&walk};FineIntersectionTestIO::OutputBuffer::LineTestIntersectionArray results;results.Construct();
    LineTestIntersection prefix{};prefix.mfLineParam=123;results.Append(prefix);
    u16 indices[]={0,1,2,3};FineIntersectionTestIO::InEventLineTestFine query{};
    query.mLineStart.x=2;query.mLineEnd.x=8;query.mQueryId.mId=0x38000000;query.mpau16EntityIndices=indices;query.mu16NumEntities=4;query.mu16ExcludeEntityIndex=0xffff;query.mxVolumeTypeFlags=4;
    FineIntersectionTestIO::OutEventLineTestFineResult out{};module.ComputeLineTestFine(&query,&out,&results);
    Check(assertions==0,"no primitive trap");Check(out.mQueryId.mId==query.mQueryId.mId,"query identity");Check(out.miNumResults==4,"all batches and candidates");
    Check(out.mpaResults==&results[1]&&results.GetLength()==5&&results[0].mfLineParam==123,"append to shared result array");
    Check(results.GetLength()>2&&results[1].mfLineParam==.7f&&results[2].mfLineParam==.2f,"preserve encounter order");
    Check(results.GetLength()>2&&results[1].mu16MaterialTag==0x3456&&results[1].mu16GroupTag==0xcdef&&results[2].mu16MaterialTag==0,"tags and null hit volume");
    Check(results.GetLength()>4&&u32(results[4].mEntityId)==2&&results[4].mVolumeInstanceId.muId==2,"internal indices for caller resolution");
    Check(results.GetLength()>1&&results[1].mPosition.w==7&&results[1].mNormal.w==8,"four position and normal lanes");Check(walk.calls==3&&walk.inputsOk,"volume mask and query inputs");
    results.Clear();query.mu16ExcludeEntityIndex=0;query.mbExcludeParts=true;module.ComputeLineTestFine(&query,&out,&results);Check(out.miNumResults==1&&results.GetLength()==1,"exclude entity parts");
    results.Clear();query.mbExcludeParts=false;module.ComputeLineTestFine(&query,&out,&results);Check(out.miNumResults==2,"exclude exact entity only");
    results.Clear();query.mxVolumeTypeFlags=0;module.ComputeLineTestFine(&query,&out,&results);Check(out.miNumResults==0&&!out.mpaResults&&results.GetLength()==0,"no-hit output");
    results.Clear();query.mu16NumEntities=0;module.ComputeLineTestFine(&query,&out,&results);Check(out.miNumResults==0&&!out.mpaResults,"empty candidates");
    std::printf("FxSoundTriggerCollision: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
