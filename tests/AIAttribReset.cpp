// Production reset bodies, generated attribute classes and PID controller.
// Registry/Instance lifetime are bounded observations over actual record layouts.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehiclesteeringattribs.h"
#undef private
#include "GameSource/World/AI/PID/BrnPIDController.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include <cstdio>
#include <cstdlib>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <string>
#include <new>
#include <cstring>
using namespace BrnAI;
unsigned checks=0,failures=0,assertions=0,lookups=0,defaults=0;
std::string trace;
void Check(bool yes,const char* text){++checks;if(!yes){++failures;std::fprintf(stderr,"FAIL: %s\n",text);}}
namespace CgsDev {namespace Log { DebugPrint* gpDebugPrint=nullptr; } namespace Assert {int BeginAssert(){return 0;}int FireAssert(const char*,const char*,int){++assertions;return 0;}void* EndAssert(){return nullptr;}}}
namespace Attrib {
HashMap::HashMap(unsigned,u8,u8){}
Collection::Collection(const CollectionLoadData&,Vault*):HashMap(0,0,0){mpData=nullptr;mKey=0;}
Collection::~Collection(){}
}
Attrib::CollectionLoadData load{};
Attrib::Collection collections[6]={{load,nullptr},{load,nullptr},{load,nullptr},{load,nullptr},{load,nullptr},{load,nullptr}};
alignas(16) unsigned char assets[2][0x228]{};
alignas(16) unsigned char handling[2][0xC0]{};
float steering[2][14]{};
char Role(const Attrib::Collection* c){return "AHS"[(c-collections)%3];}
namespace Attrib {
Collection* FindCollection(u64 klass,u64 key){++lookups;Check(klass==Gen::burnoutcarasset::KU_BURNOUTCARASSET_CLASS_KEY,"full car class key");Check(key==0x100000001ULL||key==0x200000002ULL,"full car collection key");return &collections[key==0x100000001ULL?0:3];}
const Collection* RefSpec::GetCollection(){Check(mCollectionKey<6,"RefSpec collection index");return &collections[mCollectionKey];}
Instance::Instance(Collection* c,void* owner):mpCollection(c),mpAttributeData(c->mpData),mpOwner(owner),muFlags(0){trace+=Role(c);}
Instance::~Instance(){trace+=static_cast<char>(Role(mpCollection)+32);}
int Instance::GetClass()const{switch(Role(mpCollection)){case 'H':return Gen::physicsvehiclehandling::KI_PHYSICSVEHICLEHANDLING_CLASS;case 'S':return 556409804;default:return Gen::burnoutcarasset::KI_BURNOUTCARASSET_CLASS_LOW;}}
u64 Instance::GetCollection()const{return mpCollection->mKey;}
void AssertOnClassCheck(int,int,u64){++assertions;}
void* DefaultDataArea(u32 bytes){static float zero[256]{};++defaults;Check(bytes==0x38,"steering default size");return zero;}
}
struct CarFixture {u64 mCarAssetAttribKey;};
struct DriverFixture {
    CarFixture* mpCarHost=nullptr;float mfTimeToLookAheadForDrift=-1,mfMinDistanceToLookAheadForDrift=-1;
    PIDController mPIDController,mPIDControllerDrift;
    void ResetAttribSysValues();void ResetPIDTuningState();
};
#include "ai_attrib_methods.inc"
int main(){
    for(int i=0;i<2;++i){
        collections[i*3].mpData=assets[i];collections[i*3+1].mpData=handling[i];collections[i*3+2].mpData=steering[i];
        new(assets[i]+0x158) Attrib::RefSpec(0,i*3+1);
        new(handling[i]+0x18) Attrib::RefSpec(0,i*3+2);
        steering[i][12]=i?37.f:6.f;steering[i][13]=i?2.75f:.4f;
    }
    DriverFixture driver;
    auto PID=[&](){
        Check(driver.mPIDController.mfPCoefficient==1.5f&&driver.mPIDController.mfICoefficient==0&&driver.mPIDController.mfDCoefficient==.5f,"normal PID gains");
        Check(driver.mPIDControllerDrift.mfPCoefficient==2&&driver.mPIDControllerDrift.mfICoefficient==0&&driver.mPIDControllerDrift.mfDCoefficient==1,"drift PID gains");
        Check(driver.mPIDController.mn8NumErrorsRecorded==0&&driver.mPIDControllerDrift.mn8NumErrorsRecorded==0,"PID history cleared");
    };
    driver.ResetAttribSysValues();
    Check(driver.mfMinDistanceToLookAheadForDrift==10&&driver.mfTimeToLookAheadForDrift==1.1f,"unbound driver original defaults");
    Check(trace.empty()&&lookups==0,"no car means no attribute lookup");PID();
    for(int i=0;i<2;++i){
        CarFixture car{i?0x200000002ULL:0x100000001ULL};driver.mpCarHost=&car;trace.clear();lookups=0;
        driver.mPIDController.mn8NumErrorsRecorded=5;driver.mPIDControllerDrift.mn8NumErrorsRecorded=7;
        driver.ResetAttribSysValues();
        Check(driver.mfMinDistanceToLookAheadForDrift==steering[i][12]&&driver.mfTimeToLookAheadForDrift==steering[i][13],"per-car distinct look-ahead values");
        Check(trace=="AHSshaAHSsha"&&lookups==2,"validation chain and read chain have balanced lifetimes");PID();
        trace.clear();lookups=0;driver.ResetPIDTuningState();
        Check(trace=="AHSsha"&&lookups==1,"direct PID reset validates bound attributes");
    }
    CarFixture car{0x100000001ULL};driver.mpCarHost=&car;collections[2].mpData=nullptr;defaults=0;
    driver.ResetAttribSysValues();
    Check(defaults==2&&driver.mfMinDistanceToLookAheadForDrift==0&&driver.mfTimeToLookAheadForDrift==0,"missing steering layout uses original data area, not generic AI defaults");
    Check(assertions==0,"valid records produce no assertions");
    std::printf("AIAttribReset: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
