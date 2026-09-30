#include <cstdio>
#include <cstring>
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsAllocatePoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"

static int checks, failures, assertions;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint=nullptr; }
namespace Message { u64 gxMessageFilterFlags=0; }
}
// Only addresses of these collaborators are used by the production marshallers.
// Their execution is outside this fixture and is never reported as covered.
namespace CgsMemory { class Relocator {}; struct RelocationParams {}; }
namespace CgsResource {
class ScratchPool {};
struct AllocationBoundary {
    Pool* pool=nullptr; AllocListSet* set=nullptr;
    u32 Update() { return AllocatePoolModuleState::E_RESULT_DEFRAGMENT; }
    Pool* GetPool() { return pool; }
    AllocListSet* GetAllocSet() { return set; }
    void GenerateResponse(Events::AllocateResourceListResponse*) {}
};
struct IntelligentBoundary {
    IntelliFragParams captured = {};
    void Begin(IntelliFragParams* p) { captured=*p; }
    u32 Update() { return IntelliFragPoolModuleState::E_RESULT_EMERGENCY; }
};
struct EmergencyBoundary {
    EmergencyFragParams captured = {};
    void Begin(EmergencyFragParams* p) { captured=*p; }
};
struct Driver {
    enum { KI_MAX_ALLOCATION_REQUESTS=4096,KU_MAX_POOL_ENTRIES=36863,
        KU_MAX_LINEAR_HEAP_LENGTH=65535,KU_MAX_DISTRIBUTION_COMMANDS=65535,
        E_UPDATESTATE_IDLE=0,E_UPDATESTATE_ALLOCATING_LIST=1,
        E_UPDATESTATE_INTELLIFRAG=3,E_UPDATESTATE_EMERGENCYFRAG=6 };
    AllocationBoundary mAllocateState;
    IntelligentBoundary mIntelliFragState;
    EmergencyBoundary mEmergencyFragState;
    ScratchPool mScratchPool;
    CgsMemory::Relocator mRelocator;
    CgsMemory::RelocationParams mRelocationParams;
    AllocRequestAddressed* mpAddressedAllocRequests;
    RelocateRequest* mpRelocateRequests;
    DistributionEntry* mpDistributionEntries;
    LinearHeapNode* mpLinearHeapNodes;
    RelocateSource* mpRelocateSources;
    int mProcessState=0,miAllocateRequestEventId=0;
    void UpdateAllocating(void*);
    void UpdateIntelliFrag(void*);
};
#include "pc_resource_defrag_params.inc"
}
static void Check(bool ok,const char* message) {
    ++checks;
    if(!ok) { ++failures; std::printf("FAIL %s\n",message); }
}
static void CheckParams(const CgsResource::BaseDefragParams& p,const CgsResource::Driver& d) {
    Check(p.mpPool==d.mAllocateState.pool && p.mpAllocListSet==d.mAllocateState.set,
          "handoff retains the actual allocation pool and work set");
    Check(p.mpAddressedAllocRequests==d.mpAddressedAllocRequests && p.mpRelocateRequests==d.mpRelocateRequests
        && p.mpDistributionEntries==d.mpDistributionEntries && p.mpLinearHeapNodes==d.mpLinearHeapNodes
        && p.mpRelocateSources==d.mpRelocateSources,"handoff preserves all five native working-buffer pointers");
    Check(p.muMaxAddressedAllocRequests==4096 && p.muMaxRelocateRequests==36863
        && p.muMaxRelocateSources==36863,"allocation, relocation and source capacities match original arrays");
    Check(p.muMaxDistributionRequests==36863 && p.muMaxLinearHeapNodes==65535,
          "distribution and linear-heap capacities are not swapped");
}
int main() {
    using namespace CgsResource;
    Driver d;
    d.mAllocateState.pool=reinterpret_cast<Pool*>(0x123400005000ull);
    d.mAllocateState.set=reinterpret_cast<AllocListSet*>(0x234500006000ull);
    d.mpAddressedAllocRequests=reinterpret_cast<AllocRequestAddressed*>(0x345600007000ull);
    d.mpRelocateRequests=reinterpret_cast<RelocateRequest*>(0x456700008000ull);
    d.mpDistributionEntries=reinterpret_cast<DistributionEntry*>(0x567800009000ull);
    d.mpLinearHeapNodes=reinterpret_cast<LinearHeapNode*>(0x67890000A000ull);
    d.mpRelocateSources=reinterpret_cast<RelocateSource*>(0x789A0000B000ull);
    d.UpdateAllocating(nullptr);
    CheckParams(d.mIntelliFragState.captured,d);
    Check(d.mIntelliFragState.captured.mpScratchPool==&d.mScratchPool && d.mProcessState==Driver::E_UPDATESTATE_INTELLIFRAG,
          "ordinary allocation arms the actual scratch strategy");
    d.UpdateIntelliFrag(nullptr);
    CheckParams(d.mEmergencyFragState.captured,d);
    Check(d.mEmergencyFragState.captured.mpRelocator==&d.mRelocator
        && d.mEmergencyFragState.captured.mpRelocationParams==&d.mRelocationParams
        && d.mProcessState==Driver::E_UPDATESTATE_EMERGENCYFRAG,"escalation supplies the owned relocator and parameter block");
    Check(assertions==0,"valid handoffs do not assert");
    std::printf("PCResourceDefragParams: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
