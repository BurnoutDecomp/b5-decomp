#include "SDKs/Csis/CsisGlobalVariableHandle.h"
namespace Csis
{
Result GlobalVariableHandle::SetFast(const InterfaceId* pId)
{
    return SetHandle<GlobalVariableHandle, const InterfaceId, CsisDef::GlobalVariableDesc>(this, pId, 0, 28, 14);
}
}
