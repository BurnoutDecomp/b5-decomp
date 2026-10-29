#include "SDKs/Csis/CsisFunctionHandle.h"
namespace Csis
{
Result FunctionHandle::SetFast(const InterfaceId* pId)
{
    return SetHandle<FunctionHandle, const InterfaceId, CsisDef::FunctionDesc>(this, pId, 0, 20, 10);
}
}
