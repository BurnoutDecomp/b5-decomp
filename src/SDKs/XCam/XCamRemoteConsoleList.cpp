#include "SDKs/XCam/XCamRemoteConsoleList.h"

#include <cstddef>  // offsetof
#include <cstring>  // memcpy

// ===========================================================================
// XCAM remote-console pool, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. This TU homes the fixed pool of remote-peer receive
// pipelines and the two intrusive circular doubly-linked lists (active / free)
// threaded over them. See XCamRemoteConsoleList.h for the layout and
// XCamRemoteConsole.h for the per-console shape.
//
// Each console carries an embedded ListLink (mLink); the lists are kept by two
// heads embedded in the pool (mActiveList / mFreeList). An empty list is the
// classic self-referential circular sentinel (head.next == head.prev == &head);
// a console belongs to exactly one list at a time. `XCAM` is an X360 SDK
// boundary, so its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

// Recover the owning console from its embedded list link (container_of).
static inline CRemoteConsole* ConsoleFromLink(ListLink* pLink)
{
    return reinterpret_cast<CRemoteConsole*>(
        reinterpret_cast<u8*>(pLink) - offsetof(CRemoteConsole, mLink));
}

// Reset a list head to the empty circular state (head.next = head.prev = head).
static inline void ListInit(ListLink* pHead)
{
    pHead->mpNext = pHead;
    pHead->mpPrev = pHead;
}

// Append pLink at the tail of the list whose head is pHead.
static inline void ListPushBack(ListLink* pHead, ListLink* pLink)
{
    pLink->mpNext = pHead;
    pLink->mpPrev = pHead->mpPrev;
    pHead->mpPrev->mpNext = pLink;
    pHead->mpPrev = pLink;
}

// Unlink pLink from its list, then leave it self-referential.
static inline void ListRemove(ListLink* pLink)
{
    pLink->mpNext->mpPrev = pLink->mpPrev;
    pLink->mpPrev->mpNext = pLink->mpNext;
    pLink->mpNext = pLink;
    pLink->mpPrev = pLink;
}

// @ 0x82981CE8
CRemoteConsoleList::CRemoteConsoleList()
{
    // maConsoles[30] are default-constructed by the array member (the X360 ctor
    // inlines each console's vtable store + embedded-subobject construction).
    ListInit(&mActiveList);
    ListInit(&mFreeList);
}

// @ 0x82982F30
int CRemoteConsoleList::Initialize(RemoteConsoleConfig* pConfig)
{
    mpInitData = pConfig;
    ListInit(&mActiveList);
    ListInit(&mFreeList);
    miActiveCount = 0;

    const int iCount = pConfig->miConsoleCount;
    for (int i = 0; i < iCount; ++i)
    {
        CRemoteConsole* pConsole = &maConsoles[i];
        const int iResult = pConsole->Initialize(pConfig);
        // The console joins the free pool whether or not it initialised cleanly.
        ListPushBack(&mFreeList, &pConsole->mLink);
        if (iResult != 0)
            return iResult;   // asm early-out: lock left uninitialised
    }

    ExInitializeReadWriteLock(&mLock);
    return 0;
}

// @ 0x82982FE8
int CRemoteConsoleList::Shutdown()
{
    // The X360 tail returns `this` when no console is configured; that r3 carry is
    // a compiler artifact (no caller reads the result), so this returns the last
    // depacketizer-shutdown code / 0 instead.
    int iResult = 0;
    const int iCount = mpInitData->miConsoleCount;
    for (int i = 0; i < iCount; ++i)
    {
        maConsoles[i].mDecoder.Shutdown();
        iResult = maConsoles[i].mDepacketizer.Shutdown();
    }
    return iResult;
}

// @ 0x82983048
int CRemoteConsoleList::DoWork()
{
    ExAcquireReadWriteLockShared(&mLock);
    for (ListLink* pLink = mActiveList.mpNext;
         pLink != &mActiveList && pLink != nullptr;
         pLink = pLink->mpNext)
    {
        ConsoleFromLink(pLink)->DoWork();
    }
    return ExReleaseReadWriteLock(&mLock);
}

// @ 0x82983168
int CRemoteConsoleList::GetActiveConsoleAddresses(void* pOutAddresses, u32* pInOutCount)
{
    u32 uCount = 0;
    ExAcquireReadWriteLockShared(&mLock);

    u8* pOut = static_cast<u8*>(pOutAddresses);
    ListLink* pLink = mActiveList.mpNext;
    while (pLink != &mActiveList && pLink != nullptr && uCount < *pInOutCount)
    {
        std::memcpy(pOut, ConsoleFromLink(pLink)->maAddress, 36);
        pLink = pLink->mpNext;
        ++uCount;
        pOut += 36;
    }

    const int iResult = ExReleaseReadWriteLock(&mLock);
    *pInOutCount = uCount;
    return iResult;
}

// @ 0x829831F8
CRemoteConsole* CRemoteConsoleList::GetFirstActiveRemoteConsole()
{
    ListLink* pLink = mActiveList.mpNext;
    if (pLink != &mActiveList && pLink != nullptr)
        return ConsoleFromLink(pLink);
    return nullptr;
}

// @ 0x82983228
CRemoteConsole* CRemoteConsoleList::GetNextActiveRemoteConsole(CRemoteConsole* pConsole)
{
    ListLink* pLink = pConsole->mLink.mpNext;
    if (pLink != &mActiveList && pLink != nullptr)
        return ConsoleFromLink(pLink);
    return nullptr;
}

// @ 0x82983258
int CRemoteConsoleList::SetEnabledStateForAllActiveConsoles(int iEnabled)
{
    ExAcquireReadWriteLockShared(&mLock);
    for (ListLink* pLink = mActiveList.mpNext;
         pLink != &mActiveList && pLink != nullptr;
         pLink = pLink->mpNext)
    {
        ConsoleFromLink(pLink)->mbDisabled = (iEnabled == 0);
    }
    return ExReleaseReadWriteLock(&mLock);
}

// @ 0x82983430
CRemoteConsole* CRemoteConsoleList::ActivateRemoteConsole(const void* pAddress)
{
    ExAcquireReadWriteLockShared(&mLock);

    CRemoteConsole* pConsole = FindRemoteConsole(pAddress);
    if (pConsole == nullptr)
    {
        ListLink* pLink = mFreeList.mpNext;
        if (pLink != &mFreeList)   // a free console is available
        {
            ListRemove(pLink);
            pConsole = ConsoleFromLink(pLink);
            std::memcpy(pConsole->maAddress, pAddress, 36);
            ListPushBack(&mActiveList, &pConsole->mLink);
        }
    }

    ExReleaseReadWriteLock(&mLock);
    return pConsole;
}

// @ 0x82983500
int CRemoteConsoleList::DeActivateRemoteConsole(const void* pAddress)
{
    CRemoteConsole* pConsole = FindRemoteConsole(pAddress);
    if (pConsole == nullptr)
        return 87;   // ERROR_INVALID_PARAMETER

    ExAcquireReadWriteLockExclusive(&mLock);
    ListRemove(&pConsole->mLink);
    ListPushBack(&mFreeList, &pConsole->mLink);
    ExReleaseReadWriteLock(&mLock);

    pConsole->ReInitialize(0, 0, 0, 0);
    return 0;
}

} // namespace XCAM
