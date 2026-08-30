#include "SDKs/Csis/CsisClass.h"
#include "SDKs/Csis/CsisSystem.h"

#include <new>

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

namespace Csis
{

Class::Class()
    : mpClassDesc(0), miRefCount(1), mpMemberDataClients(0),
      mpDestructorClients(0)
{
}

int Class::CreateInstanceFast(ClassHandle* phHandle, void* pParameters,
                              Class** ppClass)
{
    if (ppClass)
        *ppClass = 0;
    if (!phHandle || !ppClass || phHandle->miIndex < 0)
        return phHandle ? phHandle->miIndex : -3;
    if (!phHandle->mpDescriptor)
        return -6;

    SystemClient24* lpDescriptor =
        static_cast<SystemClient24*>(phHandle->mpDescriptor);
    if (phHandle->miIndex != lpDescriptor->mState.miStatus)
    {
        phHandle->mpDescriptor = 0;
        phHandle->miIndex = -3;
        return -3;
    }

    void* lpMemory = System::Allocate(sizeof(Class), "CsisAlloc", 1);
    Class* lpClass = lpMemory ? new (lpMemory) Class : 0;
    if (!lpClass)
        return -1;
    lpClass->mpClassDesc =
        reinterpret_cast<SystemDesc::ClassDesc*>(lpDescriptor);
    *ppClass = lpClass;

    ClassClientNode* lpClient =
        reinterpret_cast<ClassClientNode*>(lpDescriptor->muRuntimeLink);
    while (lpClient)
    {
        ClassClientNode* lpNext = lpClient->mpNext;
        lpClient->mpfnConstructor(lpClass, pParameters,
                                  lpClient->mpClientData);
        lpClient = lpNext;
    }
    lpClass->SetMemberDataFast(pParameters);
    return 0;
}

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
        lpClient->mpfnDestructor(this, lpClient->mpClientData);
        lpClient = lpNext;
    }

    if (--miRefCount == 0)
        System::Free(this);
    return 0;
}

// ---------------------------------------------------------------------------
// Class::Release @ 0x82B0FD60 -- lock the global registry mutex, ReleaseFast, unlock.
// ---------------------------------------------------------------------------
int Class::Release()
{
    System::Lock();
    int liResult = ReleaseFast();
    System::Unlock();
    return liResult;
}

// ---------------------------------------------------------------------------
// Class::SetMemberDataFast @ 0x82B0F5C8 -- broadcast member-data parameters via
// ClassData::SendParameters (this aliases its ClassData registry record), return 0.
// ---------------------------------------------------------------------------
int Class::SetMemberDataFast(void* pMemberData)
{
    for (ClassClientNode* lpClient = mpMemberDataClients; lpClient != 0;)
    {
        ClassClientNode* lpNext = lpClient->mpNext;
        lpClient->mpfnMemberData(pMemberData, lpClient->mpClientData);
        lpClient = lpNext;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Class::SubscribeConstructorFast @ 0x82B0FDB8 -- validate the handle, then push
// lpNode onto the head of the resolved Class's constructor-client list.
// ---------------------------------------------------------------------------
int Class::SubscribeConstructorFast(ClassHandle* phHandle, ClassClientNode* lpNode)
{
    if (phHandle == 0 || phHandle->miIndex < 0)
        return phHandle ? phHandle->miIndex : -3;
    if (phHandle->mpDescriptor == 0)
        return -6;
    ClassClientNode** lppHead = reinterpret_cast<ClassClientNode**>(
        phHandle->mpDescriptor);
    if (phHandle->miIndex != *reinterpret_cast<s32*>(
            reinterpret_cast<u8*>(phHandle->mpDescriptor) + 0x10))
    {
        phHandle->mpDescriptor = 0;
        phHandle->miIndex = -3;
        return -3;
    }

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
int Class::UnsubscribeConstructorFast(ClassHandle* phHandle, ClassClientNode* lpNode)
{
    if (phHandle == 0 || phHandle->miIndex < 0)
        return phHandle ? phHandle->miIndex : -3;
    if (phHandle->mpDescriptor == 0)
        return -6;
    ClassClientNode** lppHead = reinterpret_cast<ClassClientNode**>(
        phHandle->mpDescriptor);
    if (phHandle->miIndex != *reinterpret_cast<s32*>(
            reinterpret_cast<u8*>(phHandle->mpDescriptor) + 0x10))
    {
        phHandle->mpDescriptor = 0;
        phHandle->miIndex = -3;
        return -3;
    }

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

    if (--miRefCount == 0)
        System::Free(this);
    return 0;
}

int Class::UnsubscribeMemberDataFast(ClassClientNode* lpNode)
{
    if (lpNode == mpMemberDataClients)
        mpMemberDataClients = lpNode->mpNext;
    if (lpNode->mpPrev)
        lpNode->mpPrev->mpNext = lpNode->mpNext;
    if (lpNode->mpNext)
        lpNode->mpNext->mpPrev = lpNode->mpPrev;

    if (--miRefCount == 0)
        System::Free(this);
    return 0;
}

} // namespace Csis
