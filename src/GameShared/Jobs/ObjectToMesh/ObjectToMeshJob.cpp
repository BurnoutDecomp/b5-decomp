#include "GameShared/Jobs/ObjectToMesh/ObjectToMeshJob.h"

// ObjectToMeshJob::Execute @ 0x82926A70
//
//   stw r4, 0(r3)                          ; *this = lpData
//   b   ObjectToMeshJob__ExecuteImplementation ; tail-call with r4 still lpData
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). The X360 lowers Execute as a
// store of the argument into the job object followed by a tail-call into ExecuteImplementation
// (no prologue; r3 == this flows straight through). ExecuteImplementation is bodied in its own
// TU; here Execute simply stores and forwards.
void ObjectToMeshJob::Execute(ObjectToMeshJobInfo* lpData)
{
    mpData = lpData;
    ExecuteImplementation(lpData);
}
