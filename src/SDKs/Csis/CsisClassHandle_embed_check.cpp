// Tiny embed/compile check for the Csis::ClassHandle home: includes the header
// and touches each reconstructed entry point so the gate exercises the full TU.
#include "SDKs/Csis/CsisClassHandle.h"

namespace
{
void CsisClassHandleEmbedCheck()
{
    using namespace Csis;

    sizeof(ClassHandle);
    sizeof(Result);

    ClassHandle h;
    const InterfaceId* pId = nullptr;
    (void)h.SetFast(pId);
}
} // namespace
