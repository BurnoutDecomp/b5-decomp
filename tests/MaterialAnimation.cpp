#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "GameShared/GameClasses/Graphics/Dispatch/CgsMaterialAnimation.h"
#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"

static unsigned checks, failures, uploads, lastRegister, lastCount;
static Vector4 lastValue;
namespace renderengine {
    void WorldShaderConstants_Set(bool pixel, u32 reg, const void* data, u32 count) {
        if (pixel) std::abort();
        ++uploads; lastRegister=reg; lastCount=count;
        std::memcpy(&lastValue,data,sizeof(lastValue));
    }
}
#include "material_animation.inc"

static void check(bool ok,const char* message) {
    ++checks; if (!ok) {++failures;std::printf("FAIL %s\n",message);}
}
static bool CloseEnough(float a,float b) {return std::fabs(a-b)<0.000001f;}
static unsigned char* arena;
static unsigned offset;
template<class T> static T* alloc(unsigned count=1) {
    offset=(offset+15)&~15u;
    T* p=reinterpret_cast<T*>(arena+offset);offset+=sizeof(T)*count;
    std::memset(p,0,sizeof(T)*count);return p;
}
template<class T> static CgsGraphics::Ptr32<T> ptr(T* p) {
    return {static_cast<u32>(reinterpret_cast<uintptr_t>(p))};
}
static void numeric(CgsGraphics::MaterialAssembly* material,ShaderConstantsCPU* cpu,
                    Vector4* values,float period,float framesU,float framesV,
                    float time,float expectedU,float expectedV) {
    values[0].x=period;values[1].x=framesU;values[2].x=framesV;
    material->FixupAnimatedMaterial();uploads=0;
    cpu->Dispatch(time,material,material->GetMaterial(0));
    check(uploads==1,"CPU shader uploads once");
    check(CloseEnough(lastValue.x,expectedU)&&CloseEnough(lastValue.y,expectedV)
          &&lastValue.z==0&&lastValue.w==0,"original UV offset and zero spare lanes");
    check(lastRegister==37&&lastCount==1,"matching binding register, not hash array index");
}
int main() {
    arena=static_cast<unsigned char*>(CgsMemory::LowMemory::Reserve(65536));
    check(CgsMemory::LowMemory::IsLowAddress(arena),"real serialized fixtures below 4 GB");
    auto* material=alloc<CgsGraphics::MaterialAssembly>();
    auto* cpu=alloc<ShaderConstantsCPU>();
    auto* internal=alloc<ShaderConstantsInternal>();
    auto* techniques=alloc<CgsGraphics::MaterialTechnique>(2);
    auto* table=alloc<CgsGraphics::Ptr32<CgsGraphics::MaterialTechnique>>(2);
    table[0]=ptr(techniques);table[1]=ptr(techniques+1);
    material->mappMaterials=ptr(table);material->mu8NumMaterials=2;
    material->mpCPUShaderConstants=ptr(cpu);material->mpVertexShaderConstants=ptr(internal);
    auto* names=alloc<CgsGraphics::Ptr32<const char>>(3);
    const char* literals[]={"AnimDuration","AnimNumberOfFramesU","AnimNumberOfFramesV"};
    auto* values=alloc<Vector4>(3);
    auto* data=alloc<CgsGraphics::Ptr32<u32>>(3);
    for(unsigned i=0;i<3;++i) {
        char* name=alloc<char>(32);std::strcpy(name,literals[i]);
        names[i]=ptr(static_cast<const char*>(name));data[i]=ptr(reinterpret_cast<u32*>(values+i));
    }
    cpu->muNumConstantsInstances=3;cpu->mppacNames=ptr(names);cpu->mppaConstantsInstanceData=ptr(data);
    auto* hashes=alloc<u32>(3);hashes[0]=0x11111111;hashes[1]=0x22222222;hashes[2]=0x77691959;
    internal->muNumConstantsInstances=3;internal->mpauNamesHash=ptr(hashes);
    auto* handles=alloc<ShaderConstantHandle>(2);handles[0]={5,1,1};handles[1]={37,2,1};
    techniques[0].mpaVertexShaderInternalConstantsHandles=ptr(handles);
    techniques[0].mi8NumVertexShaderInternalConstants=2;
    techniques[0].mu16StateFlags=8;techniques[1].mu16StateFlags=1;
    material->mpCPUShaderConstants.muSlot=0;material->FixupAnimatedMaterial();
    check(techniques[0].mu16StateFlags==8,"no CPU block leaves static techniques untouched");
    material->mpCPUShaderConstants=ptr(cpu);material->FixupAnimatedMaterial();
    check(cpu->mpCPUShader.Get()==nullptr,"zero duration leaves CPU block static");
    check(techniques[0].mu16StateFlags==8&&techniques[1].mu16StateFlags==1,"zero duration preserves flags");
    numeric(material,cpu,values,2,0,0,0.25f,0.125f,0);
    check(techniques[0].mu16StateFlags==14&&techniques[1].mu16StateFlags==7,"all techniques gain only flags 6");
    numeric(material,cpu,values,2,0,0,2.5f,0.25f,0);
    numeric(material,cpu,values,2,0,7,-0.25f,-0.125f,0);
    numeric(material,cpu,values,-2,0,0,0.25f,-0.125f,0);
    numeric(material,cpu,values,2,8,0,0.75f,0.375f,0);
    numeric(material,cpu,values,2,8,0,2.0f,0,0);
    numeric(material,cpu,values,2,8,0,-0.25f,-0.125f,0);
    numeric(material,cpu,values,2,4,3,2.25f,0,1.0f/3.0f);
    numeric(material,cpu,values,2,4,3,5.75f,0.75f,2.0f/3.0f);
    numeric(material,cpu,values,2,4,3,6.0f,0,0);
    hashes[2]=0;uploads=0;cpu->Dispatch(0.5f,material,techniques);
    check(uploads==0,"missing AnimPrivate binding does not alter another constant");
    hashes[2]=0x77691959;techniques[0].mi8NumVertexShaderInternalConstants=0;
    cpu->Dispatch(0.5f,material,techniques);
    check(uploads==0,"zero vertex bindings do not upload");
    cpu->mpCPUShader.muSlot=0;cpu->Dispatch(0.5f,material,techniques);
    check(uploads==0,"unattached CPU block does not dispatch");
    Vector4 out={};
    check(cpu->GetValue("AnimDuration",out)&&out.x==2,"CPU constants read the serialized pointer table");
    check(!cpu->GetValue("AnimDurationX",out),"constant lookup requires the complete name");
    std::printf("MaterialAnimation: %u checks, %u failures\n",checks,failures);
    CgsMemory::LowMemory::Release(arena);return failures?1:0;
}
