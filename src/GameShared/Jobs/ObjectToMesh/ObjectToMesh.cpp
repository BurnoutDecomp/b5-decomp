#include "GameShared/Jobs/ObjectToMesh/ObjectToMeshJob.h"

// ARTIST82926940 receives the job data in r4 and calls ObjectToMeshJob::Execute.
// FLAG PC-platform leaf: a native invocation owns its job object on the stack;
// Xbox hardware-thread IDs cannot index the console's six-element object array.
void ObjectToMeshEntry(EA::Jobs::Param, EA::Jobs::Param lData,
                       EA::Jobs::Param, EA::Jobs::Param)
{
    ObjectToMeshJob lJob;
    lJob.Execute(static_cast<ObjectToMeshJobInfo*>(lData.mpValue));
}
