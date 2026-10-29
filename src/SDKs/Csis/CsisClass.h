// ARTIST native Csis ABI; DecFIGS corroborates member/signature names.
#ifndef CSIS_CLASS_H
#define CSIS_CLASS_H

#include "types.hpp"

#include "SDKs/Csis/CsisDefinition.h" // Csis::ClassHandle, Csis::Result, CsisDef::FunctionDesc

namespace Csis
{

class Class;

struct ClassClientNode
{
    typedef void (*ConstructorFn)(Class* pClass, void* pParameters,
                                  void* pClientData);
    typedef void (*DestructorFn)(Class* pClass, void* pClientData);
    typedef void (*MemberDataFn)(void* pParameters, void* pClientData);

    ClassClientNode* mpNext;       // +0x00  forward link (0 == end)
    ClassClientNode* mpPrev;       // +0x04  back link    (0 == head)
    union
    {
        ConstructorFn mpfnConstructor;
        DestructorFn  mpfnDestructor;
        MemberDataFn  mpfnMemberData;
        void*         mpfnRaw;
    };                            // +0x08
    void* mpClientData;           // +0x0C  callback context
};

CGS_STATIC_ASSERT((sizeof(ClassClientNode) == 16), "native CSIS class-client node");

class Class
{
public:
    Class();

    static Result CreateInstanceFast(ClassHandle* phHandle, void* pParameters,
                                  Class** ppClass);

    Result GetRefCount(s32* lpiOutRefCount);

    Result ReleaseFast();

    Result Release();

    Result SetMemberDataFast(void* pMemberData);

    static Result SubscribeConstructorFast(ClassHandle* phHandle, ClassClientNode* lpNode);

    Result SubscribeDestructorFast(ClassClientNode* lpNode);

    Result SubscribeMemberDataFast(ClassClientNode* lpNode);

    static Result UnsubscribeConstructorFast(ClassHandle* phHandle, ClassClientNode* lpNode);

    Result UnsubscribeDestructorFast(ClassClientNode* lpNode);

    Result UnsubscribeMemberDataFast(ClassClientNode* lpNode);

    CsisDef::ClassDesc* mpClassDesc;         // +0x00
    s32                    miRefCount;          // +0x04
    ClassClientNode*       mpMemberDataClients; // +0x08
    ClassClientNode*       mpDestructorClients; // +0x0C
};

} // namespace Csis

#endif // CSIS_CLASS_H

CGS_STATIC_ASSERT(sizeof(Csis::Class) == 16, "ARTIST class allocation");
CGS_STATIC_ASSERT(offsetof(Csis::ClassClientNode, mpfnRaw) == 8, "ARTIST callback");
CGS_STATIC_ASSERT(offsetof(Csis::ClassClientNode, mpClientData) == 12, "ARTIST context");
