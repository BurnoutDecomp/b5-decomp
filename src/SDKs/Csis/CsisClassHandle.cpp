#include "SDKs/Csis/CsisClassHandle.h"
namespace Csis
{
// ARTIST 82B0F150 leaves the descriptor untouched.
ClassHandle::ClassHandle() { miIndex = RESULT_ERR_HANDLEEXPIRED; }
Result ClassHandle::SetFast(const InterfaceId* pId)
{
    return SetHandle<ClassHandle, const InterfaceId, CsisDef::FunctionDesc>(this, pId, 0, 24, 12);
}
}
