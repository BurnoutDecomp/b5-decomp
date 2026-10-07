// ARTIST ProcessDebugResetDeformationModels82641E50 and post-scene caller825A7104.
// Execute the production sweep and the actual caller prefix with recording IO/callees.
// Geometry reset itself is deliberately outside this orchestration regression.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include <cstdio>
#include <string>
#include <vector>

static unsigned checks=0,failures=0,assertions=0;
static std::vector<std::string> trace;
static void Check(bool passed,const char* label){++checks;if(!passed){++failures;std::printf("FAIL %s\n",label);}}
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
}}
namespace CgsModule { struct Event {}; }
namespace CgsPhysics { namespace PhysicsSimulationIO { struct InputBuffer {}; } }
namespace CgsSceneManager { namespace SceneManagerIO { struct InSceneUpdateInterface {}; } }
namespace BrnGameState { namespace GameStateModuleIO {
struct GameActionQueue {
    s32 GetFirstEvent(const CgsModule::Event** event,s32* size) const {
        trace.push_back("queue-first");*event=nullptr;*size=0;return -1;
    }
};
}}
namespace BrnPhysics { namespace Deformation {
struct DetachedPartManager {};
struct DetachedWheelManager {};
enum DeformationResetType : s32 { E_DEFORMATION_RESET_NONE=0 };
struct ResetCall {
    int index;CgsPhysics::PhysicsSimulationIO::InputBuffer* input;
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* scene;
    DetachedPartManager* parts;DetachedWheelManager* wheels;
    VecFloat damage;DeformationResetType type;bool flag;CgsNumeric::Random* random;
};
static std::vector<ResetCall> calls;
struct DeformableObject {
    int index=-1;
    bool mbResetDeformationNextUpdate=false;
    #include "deformation_pending_accessors.inc"
    void ResetDeformation(CgsPhysics::PhysicsSimulationIO::InputBuffer* input,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* scene,
        DetachedPartManager* parts,DetachedWheelManager* wheels,VecFloat damage,
        DeformationResetType type,bool flag,CgsNumeric::Random& random) {
        Check(mbResetDeformationNextUpdate,"request remains set at reset entry");
        trace.push_back("reset:"+std::to_string(index));
        calls.push_back({index,input,scene,parts,wheels,damage,type,flag,&random});
        // Original FA0 must clear after the callee, including any still-set request.
        mbResetDeformationNextUpdate=true;
    }
};
struct DeformationManager {
    CgsNumeric::Random mRandom;
    DetachedPartManager mDetachedPartManager;
    DetachedWheelManager mDetachedWheelManager;
    CgsContainers::BitArray<28> mModelsAdded;
    DeformableObject* mpaModels=nullptr;
    void ProcessDebugResetDeformationModels(CgsPhysics::PhysicsSimulationIO::InputBuffer*,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*);
};
}}
namespace BrnPhysics {
struct PhysicsModule {
    Deformation::DeformationManager& mDeformationManager;
    void HandleGameActionsPostScene(const BrnGameState::GameStateModuleIO::GameActionQueue*,
        CgsPhysics::PhysicsSimulationIO::InputBuffer*,CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*);
};
// Recording sink for the captured pre-fix caller control only.
void ReportDeferral(bool&,const char*){trace.push_back("deferred");}
}
#include "deformation_pending_reset.inc"

static void Args(const BrnPhysics::Deformation::ResetCall& c,
                 const BrnPhysics::Deformation::DeformationManager& manager,
                 CgsPhysics::PhysicsSimulationIO::InputBuffer* input,
                 CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* scene){
    Check(c.input==input&&c.scene==scene,"original supplied SIM/scene IO identities");
    Check(c.parts==&manager.mDetachedPartManager&&c.wheels==&manager.mDetachedWheelManager,"original owned part/wheel managers");
    Check(c.random==&manager.mRandom,"original leading RNG reference");
    Check(c.damage.x==0&&c.damage.y==0&&c.damage.z==0&&c.damage.w==0,"original zero VecFloat in all lanes");
    Check(static_cast<int>(c.type)==0&&!c.flag,"original reset type0 and final false flag");
}

int main(){
    using namespace BrnPhysics::Deformation;
    CgsPhysics::PhysicsSimulationIO::InputBuffer input;
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface scene;
    DeformationManager manager{};DeformableObject pool[28];
    for(int i=0;i<28;++i)pool[i].index=i;
    manager.mModelsAdded.UnSetAll();
    // Empty original iterator must return without touching the absent pool.
    manager.ProcessDebugResetDeformationModels(&input,&scene);
    Check(calls.empty(),"empty live set with absent pool does nothing");
    manager.mpaModels=pool;
    for(int i:{0,5,17,27})manager.mModelsAdded.SetBit(i);
    for(int i:{0,2,17,27})pool[i].ResetDeformationNextUpdate(true);
    manager.ProcessDebugResetDeformationModels(&input,&scene);
    Check(calls.size()==3,"only requested live models reset");
    std::vector<int> order;for(const auto& c:calls){order.push_back(c.index);Args(c,manager,&input,&scene);}
    Check(order==std::vector<int>({0,17,27}),"original ascending live-slot order including boundary27");
    for(int i:{0,5,17,27})Check(!pool[i].ShouldResetDeformationNextUpdate(),"live request clear after sweep");
    Check(pool[2].ShouldResetDeformationNextUpdate(),"inactive pending model is untouched");
    calls.clear();manager.ProcessDebugResetDeformationModels(&input,&scene);
    Check(calls.empty(),"completed request does not replay next sweep");
    manager.mModelsAdded.SetBit(2);manager.ProcessDebugResetDeformationModels(&input,&scene);
    Check(calls.size()==1&&calls[0].index==2,"previous inactive request consumed on becoming live");
    Check(!pool[2].ShouldResetDeformationNextUpdate(),"newly active request clears");
    // Execute the actual production prefix through the queue-empty boundary.
    manager.mModelsAdded.UnSetAll();manager.mModelsAdded.SetBit(7);
    pool[7].ResetDeformationNextUpdate(true);calls.clear();trace.clear();
    BrnGameState::GameStateModuleIO::GameActionQueue emptyQueue;
    BrnPhysics::PhysicsModule physics{manager};
    physics.HandleGameActionsPostScene(&emptyQueue,&input,&scene);
    Check(calls.size()==1&&calls[0].index==7,"empty game-action queue still consumes debug request");
    Check(trace==std::vector<std::string>({"queue-first","reset:7"}),"original first-event then debug-sweep caller order");
    Check(!pool[7].ShouldResetDeformationNextUpdate(),"caller delivery clears the request");
    if(!calls.empty())Args(calls[0],manager,&input,&scene);
    Check(assertions==0,"valid live iterator raises no assertions");
    std::printf("DeformationPendingReset: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
