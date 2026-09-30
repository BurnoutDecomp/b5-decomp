#include <cstdio>
#include <cstdlib>
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int checks, failures, assertions, begins, finals, lastMemoryType;
static bool acceptPlan=true;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log {
static DebugPrint sink;
DebugPrint* gpDebugPrint=&sink;
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags=0; }
}

namespace CgsResource {
// The planner and final allocator are explicit boundaries. This fixture runs
// production polling/lookup bodies, not the relocation algorithm or byte copies.
bool BaseDefragPoolModuleState::BeginDefragment(s32 memoryType) {
    ++begins; lastMemoryType=memoryType; return acceptPlan;
}
bool BaseDefragPoolModuleState::DoFinalAllocations() { ++finals; return true; }
#define UNUSED_STRATEGY_HOOKS(T) \
    bool T::RunDefragAlgorithm(AllocListSet*, LinearHeapNode*, s32, s32) { std::abort(); } \
    void T::RunPoolDefragmentation(RelocateRequest*, RelocateSource*, u32, s32) { std::abort(); }
UNUSED_STRATEGY_HOOKS(BaseDefragPoolModuleState)
UNUSED_STRATEGY_HOOKS(IntelliFragPoolModuleState)
UNUSED_STRATEGY_HOOKS(EmergencyFragPoolModuleState)
#undef UNUSED_STRATEGY_HOOKS
}
#include "pc_resource_defrag_state.inc"

static void Check(bool condition,const char* message) {
    ++checks;
    if(!condition) { ++failures; std::printf("FAIL %s\n",message); }
}

template<class State> static void CheckState(State& state,CgsResource::Pool& pool,CgsResource::AllocListSet& set) {
    state.mpPool=&pool; state.mpAllocListSet=&set;
    state.meState=State::E_STATE_DEFRAGMENTING_HEAP;
    begins=finals=0; acceptPlan=true;
    pool.miNumResourcesInPurgatory=0;
    pool.miDefragFrame=0; pool.miDefragMemType=-1;
    Check(state.Update()==State::E_RESULT_PEND && finals==0 && state.meState==State::E_STATE_DEFRAGMENTING_HEAP,
          "frame zero is active even when the unrelated memory selector is -1");
    state.meState=State::E_STATE_DEFRAGMENTING_HEAP; finals=0;
    pool.miDefragFrame=7; pool.miDefragMemType=2;
    Check(state.Update()==State::E_RESULT_PEND && finals==0,
          "positive relocation frame waits without publishing final allocations");
    pool.miDefragFrame=-1;
    Check(state.Update()==State::E_RESULT_PEND && finals==1 && state.meState==State::E_STATE_START_DEFRAGMENTING,
          "idle frame latch resumes final allocations while memory selector retains its last value");
    for(int i=0;i<3;++i) set.maeAllocRequestResults[i]=CgsResource::E_BATCHALLOCRESULT_SUCCESS;
    Check(state.Update()==State::E_RESULT_SUCCESS && state.meState==State::E_STATE_IDLE && finals==1,
          "next poll completes after all memory types succeed");
    state.meState=State::E_STATE_START_DEFRAGMENTING;
    pool.miNumResourcesInPurgatory=1;
    set.maeAllocRequestResults[2]=CgsResource::E_BATCHALLOCRESULT_FAIL_NEED_DEFRAG;
    Check(state.Update()==State::E_RESULT_PEND && begins==0,"purgatory prevents starting another relocation plan");
    pool.miNumResourcesInPurgatory=0;
    Check(state.Update()==State::E_RESULT_PEND && begins==1 && lastMemoryType==2
        && state.meState==State::E_STATE_DEFRAGMENTING_HEAP,"planner starts the memory type that still needs defrag");
    state.meState=State::E_STATE_START_DEFRAGMENTING; acceptPlan=false;
    Check(state.Update()==State::E_RESULT_EMERGENCY,"failed planning preserves original escalation result");
    state.meState=static_cast<typename State::EInternalState>(3);
    const int before=assertions;
    Check(state.Update()==State::E_RESULT_ERROR && assertions==before+1,"invalid state preserves original assertion and error result");
    assertions=before;
}

int main() {
    using namespace CgsResource;
    Pool pool;
    AllocListSet set = {};
    IntelliFragPoolModuleState intelligent;
    EmergencyFragPoolModuleState emergency;
    emergency.miCountdown=0;
    CheckState(intelligent,pool,set);
    CheckState(emergency,pool,set);
    begins=0; acceptPlan=true;
    emergency.meState=EmergencyFragPoolModuleState::E_STATE_START_DEFRAGMENTING;
    emergency.miCountdown=2;
    Check(emergency.Update()==EmergencyFragPoolModuleState::E_RESULT_PEND && emergency.miCountdown==1 && begins==0,
          "emergency countdown waits one whole poll");
    Check(emergency.Update()==EmergencyFragPoolModuleState::E_RESULT_PEND && emergency.miCountdown==0 && begins==0,
          "emergency countdown reaching zero still waits this poll");
    Check(emergency.Update()==EmergencyFragPoolModuleState::E_RESULT_PEND && begins==1,
          "emergency planning starts on the following poll");
    Check(assertions==0,"no unexpected assertions");
    std::printf("PCResourceDefragState: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
