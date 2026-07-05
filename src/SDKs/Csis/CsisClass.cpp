#include "SDKs/Csis/CsisClass.h"
#include "SDKs/Csis/CsisClassData.h"   // Csis::ClassData::SendParameters (SetMemberDataFast broadcast)

#include <Windows.h>   // WaitForSingleObject / ReleaseMutex / HANDLE (Release's registry lock)

// Csis::Class -- reconstructed from BURNOUT_X360_ARTIST.XEX (vendor Csis boundary).
//   GetRefCount              @ 0x82B0F5B8
//   ReleaseFast              @ 0x82B0F518
//   Release                  @ 0x82B0FD60
//   SetMemberDataFast        @ 0x82B0F5C8
//   SubscribeConstructorFast @ 0x82B0FDB8
//   SubscribeDestructorFast  @ 0x82B0F5F0
//   SubscribeMemberDataFast  @ 0x82B0F6C8
//   UnsubscribeConstructorFast @ 0x82B0FE28
//   UnsubscribeDestructorFast  @ 0x82B0F630

namespace
{
    // The process-global Csis registry mutex HANDLE (X360 dword_8324E8FC). Created by
    // Csis::System::Init (owned there as a file-static ghCsisMutex with internal linkage);
    // Release calls WaitForSingleObject/ReleaseMutex on it DIRECTLY (not via System::Lock/
    // Unlock). Modelled per-TU as a file-static HANDLE aliasing the same module symbol.
    HANDLE ghCsisMutex = 0;   // dword_8324E8FC
}

namespace Csis
{

// The process-global class-lifecycle observer singleton (off_8324E904); null until installed.
IClassObserver* gpClassDestroyNotifier = 0;

// ---------------------------------------------------------------------------
// Class::GetRefCount @ 0x82B0F5B8
// ---------------------------------------------------------------------------
int Class::GetRefCount(s32* lpiOutRefCount)
{
    *lpiOutRefCount = miRefCount;
    return 0;
}

// ---------------------------------------------------------------------------
// Class::ReleaseFast @ 0x82B0F518 -- fire every destructor client, DecRef, and
// notify the global observer when the count reaches zero.
// ---------------------------------------------------------------------------
int Class::ReleaseFast()
{
    // Fire every registered destructor client (in list order), following the
    // forward link before invoking so a callback may unlink its own node.
    for (ClassClientNode* lpClient = mpDestructorClients; lpClient != 0;)
    {
        ClassClientNode* lpNext = lpClient->mpNext;
        lpClient->mpfnClient(this, lpClient->mpClientData);
        lpClient = lpNext;
    }

    if (--miRefCount == 0 && gpClassDestroyNotifier != 0)
    {
        gpClassDestroyNotifier->OnClassDestroyed(this, 0);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Class::Release @ 0x82B0FD60 -- lock the global registry mutex, ReleaseFast, unlock.
// ---------------------------------------------------------------------------
int Class::Release()
{
    ::WaitForSingleObject(ghCsisMutex, 0xFFFFFFFFu);  // INFINITE
    int liResult = ReleaseFast();
    ::ReleaseMutex(ghCsisMutex);
    return liResult;
}

// ---------------------------------------------------------------------------
// Class::SetMemberDataFast @ 0x82B0F5C8 -- broadcast member-data parameters via
// ClassData::SendParameters (this aliases its ClassData registry record), return 0.
// ---------------------------------------------------------------------------
int Class::SetMemberDataFast(void* pMemberData)
{
    reinterpret_cast<ClassData*>(this)->SendParameters(
        reinterpret_cast<Parameter*>(pMemberData));
    return 0;
}

// ---------------------------------------------------------------------------
// Class::SubscribeConstructorFast @ 0x82B0FDB8 -- validate the handle, then push
// lpNode onto the head of the resolved Class's constructor-client list.
// ---------------------------------------------------------------------------
int Class::SubscribeConstructorFast(ClassHandle** phHandle, ClassClientNode* lpNode)
{
    int liResult = static_cast<int>(
        ValidHandle<ClassHandle, CsisDef::FunctionDesc>(phHandle, 0));
    if (liResult < 0)
    {
        return liResult;
    }

    // The resolved handle record holds the constructor-client list head in its
    // first word (asm: v5 = *a1, head = *v5).
    ClassClientNode** lppHead = *reinterpret_cast<ClassClientNode***>(phHandle);

    lpNode->mpPrev = 0;
    lpNode->mpNext = *lppHead;
    if (*lppHead)
        (*lppHead)->mpPrev = lpNode;
    *lppHead = lpNode;
    return 0;
}

// ---------------------------------------------------------------------------
// Class::SubscribeDestructorFast @ 0x82B0F5F0 -- push onto the destructor-client
// list (+0xC) and AddRef.
// ---------------------------------------------------------------------------
int Class::SubscribeDestructorFast(ClassClientNode* lpNode)
{
    lpNode->mpPrev = 0;
    lpNode->mpNext = mpDestructorClients;
    if (mpDestructorClients)
        mpDestructorClients->mpPrev = lpNode;
    mpDestructorClients = lpNode;

    ++miRefCount;
    return 0;
}

// ---------------------------------------------------------------------------
// Class::SubscribeMemberDataFast @ 0x82B0F6C8 -- push onto the member-data-client
// list (+0x8) and AddRef.
// ---------------------------------------------------------------------------
int Class::SubscribeMemberDataFast(ClassClientNode* lpNode)
{
    lpNode->mpPrev = 0;
    lpNode->mpNext = mpMemberDataClients;
    if (mpMemberDataClients)
        mpMemberDataClients->mpPrev = lpNode;
    mpMemberDataClients = lpNode;

    ++miRefCount;
    return 0;
}

// ---------------------------------------------------------------------------
// Class::UnsubscribeConstructorFast @ 0x82B0FE28 -- validate the handle, then
// unlink lpNode from the resolved Class's constructor-client list.
// ---------------------------------------------------------------------------
int Class::UnsubscribeConstructorFast(ClassHandle** phHandle, ClassClientNode* lpNode)
{
    int liResult = static_cast<int>(
        ValidHandle<ClassHandle, CsisDef::FunctionDesc>(phHandle, 0));
    if (liResult < 0)
    {
        return liResult;
    }

    // The resolved handle record holds the constructor-client list head in its
    // first word (asm: r11 = *a1 ; head = *r11).
    ClassClientNode** lppHead = *reinterpret_cast<ClassClientNode***>(phHandle);

    if (lpNode == *lppHead)
    {
        *lppHead = lpNode->mpNext;
    }
    if (lpNode->mpPrev)
    {
        lpNode->mpPrev->mpNext = lpNode->mpNext;
    }
    if (lpNode->mpNext)
    {
        lpNode->mpNext->mpPrev = lpNode->mpPrev;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Class::UnsubscribeDestructorFast @ 0x82B0F630 -- unlink lpNode from the
// destructor-client list (+0xC), DecRef, notify the observer when it hits zero.
// ---------------------------------------------------------------------------
int Class::UnsubscribeDestructorFast(ClassClientNode* lpNode)
{
    if (lpNode == mpDestructorClients)
    {
        mpDestructorClients = lpNode->mpNext;
    }
    if (lpNode->mpPrev)
    {
        lpNode->mpPrev->mpNext = lpNode->mpNext;
    }
    if (lpNode->mpNext)
    {
        lpNode->mpNext->mpPrev = lpNode->mpPrev;
    }

    if (--miRefCount == 0 && gpClassDestroyNotifier)
    {
        gpClassDestroyNotifier->OnClassDestroyed(this, 0);
    }
    return 0;
}

} // namespace Csis
