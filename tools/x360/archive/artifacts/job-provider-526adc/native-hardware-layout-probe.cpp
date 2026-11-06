#define private public
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#undef private
#include <stddef.h>
extern "C" const unsigned native_hardware_layout[] = {
 sizeof(EA::Jobs::JobScheduler),__alignof(EA::Jobs::JobScheduler),
 offsetof(EA::Jobs::JobScheduler,mSchedulers),offsetof(EA::Jobs::JobScheduler,mLock),
 offsetof(EA::Jobs::JobScheduler,mEnableProfiling),offsetof(EA::Jobs::JobScheduler,mInitialized),
 sizeof(EA::Jobs::JobThreadParameters),
 offsetof(EA::Jobs::JobThreadParameters,environment),offsetof(EA::Jobs::JobThreadParameters,stackSize),
 offsetof(EA::Jobs::JobThreadParameters,priority),offsetof(EA::Jobs::JobThreadParameters,processor),
 offsetof(EA::Jobs::JobThreadParameters,affinity),offsetof(EA::Jobs::JobThreadParameters,ptrToName),
 sizeof(CgsMemory::HeapMallocCoreAllocator),__alignof(CgsMemory::HeapMallocCoreAllocator),
 offsetof(CgsMemory::HeapMallocCoreAllocator,mAllocator),offsetof(CgsMemory::HeapMalloc,mAllocator),
 sizeof(CgsSystem::HardwareInit::macJobManagerBuffer)
};
