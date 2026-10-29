// ARTIST native Csis ABI; DecFIGS corroborates member/signature names.
#include "SDKs/Csis/CsisClassData.h"


namespace Csis
{

void ClassData::SendParameters(Parameter* pParameter)
{
    for (ClassDataClient* lpClient = mUpdateClients.mpHead;
         lpClient != 0;
         lpClient = lpClient->mpNext)
    {
        lpClient->mpfnNotify(pParameter, lpClient->mpContext);
    }

}

} // namespace Csis
