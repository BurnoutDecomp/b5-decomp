#ifndef CSIS_CLASS_H
#define CSIS_CLASS_H

#include "types.hpp"

#include "SDKs/Csis/CsisClassHandle.h" // Csis::ClassHandle, Csis::Result, CsisDef::FunctionDesc

// ===========================================================================
// SDKs/Csis/CsisClass.h
//
// Csis::Class -- the runtime per-class notification/refcount record in the "Csis"
// class/interface system (a vendor library boundary in BURNOUT_X360_ARTIST.XEX,
// the Csis::* / CsisDef::* family used by the AEMS audio voice/factory code, e.g.
// SNDAEMSI_CreateModuleInstance / SNDAEMSI_updatedestroy / Snd9::Aems). This
// header is the canonical OWNING home for the Csis::Class *Fast list-management +
// refcount methods:
//
//     Csis::Class::GetRefCount                 @ 0x82B0F5B8
//     Csis::Class::ReleaseFast                 @ 0x82B0F518
//     Csis::Class::Release                     @ 0x82B0FD60
//     Csis::Class::SetMemberDataFast           @ 0x82B0F5C8
//     Csis::Class::SubscribeConstructorFast    @ 0x82B0FDB8
//     Csis::Class::SubscribeDestructorFast     @ 0x82B0F5F0
//     Csis::Class::SubscribeMemberDataFast     @ 0x82B0F6C8
//     Csis::Class::UnsubscribeConstructorFast  @ 0x82B0FE28
//     Csis::Class::UnsubscribeDestructorFast   @ 0x82B0F630
//
// LAYOUT: Csis::Class shares the exact record shape the committed CsisClassData.h
// gives for the sibling Csis::ClassData -- the X360 asm is authoritative and
// confirms it store-for-store:
//     +0x00  mpClassDesc          SystemDesc::ClassDesc*   (owning class descriptor)
//     +0x04  miRefCount           s32                      (Sub* AddRef, Unsub*/ReleaseFast DecRef)
//     +0x08  mpMemberDataClients  ClassClientNode*         (member-data notification list head)
//     +0x0C  mpDestructorClients  ClassClientNode*         (destructor notification list head)
// (SubscribeMemberDataFast pushes at +0x8 (lwz/stw 8(r11)), SubscribeDestructor-
// Fast/UnsubscribeDestructorFast at +0xC (0xC(r11)/0xC(r3)); both ++/-- the
// refcount at +0x4.)
//
// The intrusive client node is doubly-linked; the asm proves mpNext at +0x0 and
// mpPrev at +0x4. ReleaseFast also invokes each destructor client's callback as
// mpfnClient(this, mpClientData), reading the callback @ +0x8 and its context @
// +0xC (same node shape as CsisClassData.h's client). So:
//     ClassClientNode { mpNext@0; mpPrev@4; mpfnClient@8; mpClientData@0xC; }
//
// UnsubscribeConstructorFast/SubscribeConstructorFast take a Csis::ClassHandle**
// and resolve it via the vendor ValidHandle<ClassHandle, FunctionDesc> template;
// the constructor-client list head then sits at the first word of the record the
// handle resolves to (one indirection: (*phHandle)[0] == **a1).
//
// `Csis` is a vendor library boundary, so its identifiers (Csis, Class, ClassHandle,
// ClassClientNode, ValidHandle, Result, CsisDef::FunctionDesc, SystemDesc::ClassDesc)
// are preserved verbatim per the naming convention.
// ===========================================================================

namespace SystemDesc
{
// Forward declaration only -- Class stores a pointer to it (out of this TU's scope).
struct ClassDesc;
}

namespace Csis
{

class Class; // fwd (client callbacks / the observer callback take a Class*)

// Vendor handle-validation free template used by the *ConstructorFast methods; its
// body lives in another Csis TU. Declared (not defined) here for compile/link, as a
// flagged vendor extern. It returns a Csis::Result whose sign gates success. The
// second arg is the "kind" selector (asm: li r4,0 before the call).
template <class THandle, class TFunctionDesc>
Result ValidHandle(THandle** ppHandle, int liKind);

// ---------------------------------------------------------------------------
// Csis::ClassClientNode -- one intrusive doubly-linked notification-client node.
// The X360 walks mpNext at +0x0 and mpPrev at +0x4. Nodes live in the two
// Csis::Class client lists (member-data @+0x8, destructor @+0xC) and in the
// constructor-client list reached through a ClassHandle. ReleaseFast fires a
// destructor client as mpfnClient(pClass, mpClientData) (callback @+0x8, context @+0xC).
// ---------------------------------------------------------------------------
struct ClassClientNode
{
    // Destructor-client callback shape (ReleaseFast: mpfnClient(pClass, data)).
    typedef void (*ClientFn)(Class* pClass, void* pClientData);

    ClassClientNode* mpNext;       // +0x00  forward link (0 == end)
    ClassClientNode* mpPrev;       // +0x04  back link    (0 == head)
    ClientFn         mpfnClient;   // +0x08  callback (only read by ReleaseFast)
    void*            mpClientData; // +0x0C  callback context
};

// ---------------------------------------------------------------------------
// Csis::IClassObserver -- the process-global class-lifecycle observer singleton
// (X360 module-data symbol off_8324E904). ReleaseFast and UnsubscribeDestructorFast
// invoke its vtable slot +0xC when a Class's refcount reaches zero, passing
// (this observer, pClass, 0). Only that one vtable slot is reached from this TU;
// the concrete observer type lives in the vendor SystemDesc TU, so this is a
// minimal typed stand-in (flagged vendor extern).
// ---------------------------------------------------------------------------
class IClassObserver
{
public:
    virtual void Reserved0() = 0; // vtable +0x0
    virtual void Reserved4() = 0; // vtable +0x4
    virtual void Reserved8() = 0; // vtable +0x8
    // vtable +0xC -- invoked as OnClassDestroyed(pClass, 0) once refcount hits 0.
    virtual void OnClassDestroyed(Class* lpClass, int liFlags) = 0;
};

// ---------------------------------------------------------------------------
// Csis::Class
// ---------------------------------------------------------------------------
class Class
{
public:
    // @ 0x82B0F5B8 -- write miRefCount through the out-param; return 0.
    int GetRefCount(s32* lpiOutRefCount);

    // @ 0x82B0F518 -- fire every destructor client, DecRef, and notify the global
    // observer when the count reaches zero; return 0.
    int ReleaseFast();

    // @ 0x82B0FD60 -- Lock the global registry mutex, ReleaseFast, Unlock; return
    // ReleaseFast's result.
    int Release();

    // @ 0x82B0F5C8 -- broadcast member-data parameters (ClassData::SendParameters);
    // return 0.
    int SetMemberDataFast(void* pMemberData);

    // @ 0x82B0FDB8 -- validate the handle, then push lpNode onto the constructor-
    // client list; return 0 or the negative ValidHandle Result.
    int SubscribeConstructorFast(ClassHandle** phHandle, ClassClientNode* lpNode);

    // @ 0x82B0F5F0 -- push lpNode onto the destructor-client list (+0xC) and AddRef;
    // return 0.
    int SubscribeDestructorFast(ClassClientNode* lpNode);

    // @ 0x82B0F6C8 -- push lpNode onto the member-data-client list (+0x8) and AddRef;
    // return 0.
    int SubscribeMemberDataFast(ClassClientNode* lpNode);

    // @ 0x82B0FE28 -- validate the handle, then unlink lpNode from the constructor-
    // client list; return 0 or the negative ValidHandle Result. STATIC: the X360 call
    // passes only (phHandle, lpNode) with no `this` (the body resolves everything through
    // the handle and never touches a member), e.g. Snd9::Aems::BeginRemoveModuleBank calls
    // Csis::Class::UnsubscribeConstructorFast(&record.mpHandle, &record.mNode) with 2 args.
    static int UnsubscribeConstructorFast(ClassHandle** phHandle, ClassClientNode* lpNode);

    // @ 0x82B0F630 -- unlink lpNode from the destructor-client list (+0xC), DecRef,
    // and notify the global observer when the count reaches zero; return 0.
    int UnsubscribeDestructorFast(ClassClientNode* lpNode);

    SystemDesc::ClassDesc* mpClassDesc;         // +0x00
    s32                    miRefCount;          // +0x04
    ClassClientNode*       mpMemberDataClients; // +0x08
    ClassClientNode*       mpDestructorClients; // +0x0C
};

// The process-global class-lifecycle observer singleton (off_8324E904). Defined in
// this TU's .cpp; null until the vendor system installs it.
extern IClassObserver* gpClassDestroyNotifier;

} // namespace Csis

#endif // CSIS_CLASS_H
