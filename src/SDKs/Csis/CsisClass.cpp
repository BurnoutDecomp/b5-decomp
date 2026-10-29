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

Result Class::CreateInstanceFast(ClassHandle* phHandle, void* pParameters,
                              Class** ppClass)
{
    *ppClass = 0;
    Result leValid = ValidHandle<ClassHandle, CsisDef::FunctionDesc>(phHandle, 0);
    if (leValid < 0) return leValid;
    void* lpMemory = System::Allocate(sizeof(Class), "CsisAlloc", 1);
    if (!lpMemory) return RESULT_ERR_MEM;
    Class* lpClass = new (lpMemory) Class;
    CsisDef::ClassDesc* lpDescriptor = static_cast<CsisDef::ClassDesc*>(phHandle->mpDescriptor);
    lpClass->mpClassDesc = lpDescriptor;
    ClassClientNode* lpClient = static_cast<ClassClientNode*>(lpDescriptor->clients);
    while (lpClient)
    {
        lpClient->mpfnConstructor(lpClass, pParameters, lpClient->mpClientData);
        lpClient = lpClient->mpNext;
    }
    lpClass->SetMemberDataFast(pParameters);
    *ppClass = lpClass;
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::GetRefCount @ 0x82B0F5B8
// ---------------------------------------------------------------------------
Result Class::GetRefCount(s32* lpiOutRefCount)
{
    *lpiOutRefCount = miRefCount;
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::ReleaseFast @ 0x82B0F518 -- fire every destructor client, DecRef, and
// notify the global observer when the count reaches zero.
// ---------------------------------------------------------------------------
Result Class::ReleaseFast()
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
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::Release @ 0x82B0FD60 -- lock the global registry mutex, ReleaseFast, unlock.
// ---------------------------------------------------------------------------
Result Class::Release()
{
    System::Lock();
    Result liResult = ReleaseFast();
    System::Unlock();
    return liResult;
}

// ---------------------------------------------------------------------------
// Class::SetMemberDataFast @ 0x82B0F5C8 -- broadcast member-data parameters via
// ClassData::SendParameters (this aliases its ClassData registry record), return 0.
// ---------------------------------------------------------------------------
Result Class::SetMemberDataFast(void* pMemberData)
{
    for (ClassClientNode* lpClient = mpMemberDataClients; lpClient; lpClient = lpClient->mpNext)
        lpClient->mpfnMemberData(pMemberData, lpClient->mpClientData);
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::SubscribeConstructorFast @ 0x82B0FDB8 -- validate the handle, then push
// lpNode onto the head of the resolved Class's constructor-client list.
// ---------------------------------------------------------------------------
Result Class::SubscribeConstructorFast(ClassHandle* phHandle, ClassClientNode* lpNode)
{
    Result leValid = ValidHandle<ClassHandle, CsisDef::FunctionDesc>(phHandle, 0);
    if (leValid < 0) return leValid;
    ClassClientNode** lppHead = reinterpret_cast<ClassClientNode**>(phHandle->mpDescriptor);

    lpNode->mpPrev = 0;
    lpNode->mpNext = *lppHead;
    if (*lppHead)
        (*lppHead)->mpPrev = lpNode;
    *lppHead = lpNode;
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::SubscribeDestructorFast @ 0x82B0F5F0 -- push onto the destructor-client
// list (+0xC) and AddRef.
// ---------------------------------------------------------------------------
Result Class::SubscribeDestructorFast(ClassClientNode* lpNode)
{
    lpNode->mpPrev = 0;
    lpNode->mpNext = mpDestructorClients;
    if (mpDestructorClients)
        mpDestructorClients->mpPrev = lpNode;
    mpDestructorClients = lpNode;

    ++miRefCount;
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::SubscribeMemberDataFast @ 0x82B0F6C8 -- push onto the member-data-client
// list (+0x8) and AddRef.
// ---------------------------------------------------------------------------
Result Class::SubscribeMemberDataFast(ClassClientNode* lpNode)
{
    lpNode->mpPrev = 0;
    lpNode->mpNext = mpMemberDataClients;
    if (mpMemberDataClients)
        mpMemberDataClients->mpPrev = lpNode;
    mpMemberDataClients = lpNode;

    ++miRefCount;
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::UnsubscribeConstructorFast @ 0x82B0FE28 -- validate the handle, then
// unlink lpNode from the resolved Class's constructor-client list.
// ---------------------------------------------------------------------------
Result Class::UnsubscribeConstructorFast(ClassHandle* phHandle, ClassClientNode* lpNode)
{
    Result leValid = ValidHandle<ClassHandle, CsisDef::FunctionDesc>(phHandle, 0);
    if (leValid < 0) return leValid;
    ClassClientNode** lppHead = reinterpret_cast<ClassClientNode**>(phHandle->mpDescriptor);

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
    return RESULT_OK;
}

// ---------------------------------------------------------------------------
// Class::UnsubscribeDestructorFast @ 0x82B0F630 -- unlink lpNode from the
// destructor-client list (+0xC), DecRef, notify the observer when it hits zero.
// ---------------------------------------------------------------------------
Result Class::UnsubscribeDestructorFast(ClassClientNode* lpNode)
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
    return RESULT_OK;
}

Result Class::UnsubscribeMemberDataFast(ClassClientNode* lpNode)
{
    if (lpNode == mpMemberDataClients)
        mpMemberDataClients = lpNode->mpNext;
    if (lpNode->mpPrev)
        lpNode->mpPrev->mpNext = lpNode->mpNext;
    if (lpNode->mpNext)
        lpNode->mpNext->mpPrev = lpNode->mpPrev;

    if (--miRefCount == 0)
        System::Free(this);
    return RESULT_OK;
}

} // namespace Csis
