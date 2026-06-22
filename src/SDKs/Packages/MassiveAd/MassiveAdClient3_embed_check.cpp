// Tiny embed check: instantiate the MassiveAdClient3 surface so the gate proves
// the header + .cpp form a coherent, compilable unit. Provides trivial local
// definitions for the two heap hooks (normally supplied by the MassiveAd heap
// layer) so this check can stand alone under `cl /c`.
#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace MassiveAdClient3
{
void* MassiveMalloc(std::size_t nSize) { return std::malloc(nSize); }
void  MassiveFree(void* pBlock)        { std::free(pBlock); }

int MassiveFormatString(char* pcBuffer, unsigned int nCount, const char* pcFormat, ...)
{
    std::va_list largs;
    va_start(largs, pcFormat);
    int lnLen = std::vsnprintf(pcBuffer, nCount, pcFormat, largs);
    va_end(largs);
    return lnLen;
}

unsigned int MassiveGetTickCount() { return 0u; }
void         MassiveGetSystemTimeBlock(unsigned char* pacSystemTime)
{
    std::memset(pacSystemTime, 0, 16);
}
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

    // CMassiveSystem singleton surface.
    CMassiveSystem* lpSys = CMassiveSystem::Initialize();
    (void)CMassiveSystem::Instance();
    (void)CMassiveSystem::GetSystemTime();

    if (lpSys)
    {
        char* lpcGuid = 0;
        (void)CMassiveSystem::CreateMassiveGuid(&lpcGuid);
        (void)lpSys->SetGuid("00000000-0000-0000");
        (void)lpSys->ClearMassiveGuid();
    }

    char lacTime[64];
    (void)CMassiveSystem::GetServerTimeFormatted(0, 1700000000000u, lacTime, sizeof(lacTime));

    char* lpcMac = 0;
    (void)CMassiveSystem::GetHardwareAddress(0, &lpcMac);

    (void)CMassiveSystem::Shutdown();
}
