// Tiny embed check: instantiate the MassiveAdClient3 surface so the gate proves
// the header + .cpp form a coherent, compilable unit. Provides trivial local
// definitions for the two heap hooks (normally supplied by the MassiveAd heap
// layer) so this check can stand alone under `cl /c`.
#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

#include <cstdlib>

namespace MassiveAdClient3
{
void* MassiveMalloc(std::size_t nSize) { return std::malloc(nSize); }
void  MassiveFree(void* pBlock)        { std::free(pBlock); }
}

void MassiveAdClient3_EmbedCheck()
{
    using namespace MassiveAdClient3;

    CMassiveBaseObject lObject("AdSurface");
    lObject.SetLastError(-1, "code %d", 7);
    int lnValid = CMassiveBaseObject::IsValidString("x");
    (void)lnValid;

    CMassiveListNode* lpNode = new CMassiveListNode(&lObject);

    CMassiveList lList;
    lList.Append(lpNode);
    lList.Append(new CMassiveListNode(&lObject));

    for (CMassiveListNode* lpIt = lList.GoToStart(); lpIt; lpIt = lList.GoToNext())
    {
        void* lpData = lList.GetCurrData();
        (void)lpData;
    }

    lList.Remove(lpNode, 1);
    lList.RemoveAll();
}
