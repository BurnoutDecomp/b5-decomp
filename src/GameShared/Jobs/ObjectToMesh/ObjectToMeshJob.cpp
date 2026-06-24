#include "GameShared/Jobs/ObjectToMesh/ObjectToMeshJob.h"

#include <cstddef>  // offsetof

namespace
{
    static_assert(offsetof(ObjectToMeshJob, mpInput) == 0x00, "mpInput @ +0x00");
}

// ObjectToMeshJob::Execute @ 0x82926A70
//
//   stw r4, 0(r3)                          ; *this = apInput  (latch the input pointer)
//   b   ObjectToMeshJob__ExecuteImplementation ; tail-call; its result is Execute's result
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). The X360 lowers Execute as a
// store of the argument into the job object followed by a tail-call into ExecuteImplementation
// (no prologue; r3 == this flows straight through). ExecuteImplementation is bodied in its own
// TU; here Execute simply stores and forwards.
int ObjectToMeshJob::Execute(void* apInput)
{
    mpInput = apInput;
    return ExecuteImplementation();
}
