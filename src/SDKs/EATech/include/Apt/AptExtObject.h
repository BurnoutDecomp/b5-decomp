#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptExtObject.
//
// The base for native "extension" objects the apt VM exposes to ActionScript:
// a garbage-collected AptValue that owns a native hash of members and forwards
// ActionScript variable get/set onto a bound apt context value. Burnout's
// CgsGui::ObjectController derives from it (DecFIGS DWARF:
// CgsAptObjectController.h:37 -> `struct ObjectController : public AptExtObject`).
//
// SHAPE verbatim from the Feb-2007 leak
// (SDKs/Packages/Apt/2.00.00/include/Apt/AptExtObject.h) for the type layout and
// the member/method names; this is the minimal owning home for the surface the
// X360 ARTIST build's CgsAptObjectController / CgsAptAnimator translation units
// reach through:
//   - operator new(size_t)                    (Animator::Construct allocates one)
//   - AptExtObject(int32_t iNumMembers)        (its construction)
//   - static GetVariable(context, key)         (ObjectController::GetObjectValue)
//   - static SetVariable(context, key, value)  (ObjectController::SetObjectVariableBoolean)
//   - virtual GetName()                        (ObjectController overrides it)
//
// The leak declares GetName()/Initialize() as pure virtuals; here they are given
// trivial default bodies so this minimal base is concrete (the real per-extension
// overrides live in their own classes). The large remainder of the leak class is
// left out as an honest minimal owning header rather than fabricated.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueGC base, AptValue, AptNativeString

struct AptNativeHash;      // SDKs/EATech/include/Apt/AptNativeHash.h (held by pointer only)
class  AptNativeFunction;  // SDKs/EATech/include/Apt/AptNativeFunction.h
struct AptObject;          // SDKs/EATech/include/Apt/AptObject.h

// The native-callback pointer type (leak AptExtObject.h:116) the apt VM wraps in an
// AptNativeFunction. Defined as `void*` in AptNativeFunction.h; forward-typedef'd
// here so CreateNewAptFunction/SetFunction can take it without pulling that header.
typedef void* AptExtFunctionPtr;

class AptExtObject : public AptValueGC
{
public:
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Construct an extension object reserving room for iNumMembers native members.
    explicit AptExtObject(const int32_t iNumMembers);
    virtual ~AptExtObject();

    // The apt VM queries each extension's display name through this virtual; every
    // concrete extension (e.g. CgsGui::ObjectController) overrides it. Leak declares
    // it pure; given a trivial default here so this minimal base stays concrete.
    virtual const char* GetName() { return 0; }

    // One-time setup hook the apt VM calls after construction. Leak: pure virtual.
    virtual void Initialize() {}

    // GC-required virtuals (AptValue leaves RegisterReferences pure; satisfy it here
    // so this base is concrete). Leak AptExtObject.h:293-294. Both forward to the
    // owned native-member hash (X360 RegisterReferences @0x82ADCFD8 / DestroyGCPointers
    // @0x82AEDDB0); bodies in AptExtObject.cpp.
    virtual void RegisterReferences();
    virtual void DestroyGCPointers();

    // ---- native-hash object-model overrides (X360 AptExtObject vtbl[2]/[3]) ------
    // The extension owns its native-member hash at mpNativeHash (console +8). Expose
    // it through the AptValue object-model virtuals so CallMethod's Extension-typed
    // (tag 29) method lookup (`GetNativeHashVirtual()->Lookup(name)` in
    // AptInterp_HashLookupName) and SetMember reach the installed sMethod_* natives.
    // X360 vtbl[2] returns *(this+8); the base returns 0 -- the MISSING override made
    // every `CAptCommunicator.<native>()` (SendAptEvent / SetCommunicationObject / ...)
    // resolve to null on x64, so onLoad's SendAptEvent was a silent no-op and no menu
    // component ever registered. Pairs with ContainsNativeHashVirtual (cf.
    // AptValueWithHash's {&mHash, true} convention).
    virtual AptNativeHash* GetNativeHashVirtual()            { return mpNativeHash; }
    virtual bool           ContainsNativeHashVirtual() const { return mpNativeHash != nullptr; }

    // ---- object-model override (leak AptExtObject.h) ----------------------
    // Store pValue under pName in the native-member hash, but only when no member of
    // that name already exists (the X360 objectMemberSet @0x82AF9720 is "set if
    // absent"); always returns true. pThis is ignored (the bound apt value is the
    // hash itself).
    virtual bool objectMemberSet(AptValue* const pThis, const AptNativeString* const pName,
                                 AptValue* const pValue);

    uint32_t GetSize() const { return mnObjectSize; }

    // ---- native-member factories / setters (leak AptExtObject.h) ----------
    // Allocate a fresh generic AS Object (value type AptVFT_Object) from the GC pool.
    // X360 CreateNewAptObject @0x82AF07E0 (inlines AptObject::operator new(32) +
    // AptValueWithHash(AptVFT_Object,8) + the AptObject vtable). Null on exhaustion.
    static AptObject* CreateNewAptObject();

    // Allocate a fresh AS native-function value wrapping pAptExtFnc from the GC pool.
    // X360 CreateNewAptFunction @0x82AF08D0. Null on exhaustion.
    static AptNativeFunction* CreateNewAptFunction(AptExtFunctionPtr pAptExtFnc);

    // Add a native function member named pName to this extension's member hash.
    // X360 SetFunction @0x82AF7288 (build a scoped EAStringC key, Set it on the hash).
    void SetFunction(const char* pName, AptValue* pFunction);

protected:
    // Look the named ActionScript variable up on / store it onto the bound apt
    // context value. Static helpers in the leak (the context is passed explicitly).
    static AptValue* GetVariable(AptValue* pContext, const AptNativeString* const pVariable);
    static bool      SetVariable(AptValue* pContext, const AptNativeString* const pName,
                                 AptValue* const pValue);

    // The apt native-method argument fetch -- a native sMethod_* reads its i-th
    // ActionScript call argument off the apt interpreter operand stack via this
    // helper (X360 AptExtObject::GetParam @0x82ADC270, HOMED in AptExtObject.cpp).
    static AptValue* GetParam(int iIndex);

private:
    AptNativeHash* mpNativeHash;   // native member hash table
    uint32_t       mnObjectSize;   // reserved object size
};

// Apt VM extension registry (leak Apt.h:1431-1432): register / unregister a native
// extension object with the apt ActionScript VM so its members become scriptable.
void AptRegisterExtension(AptExtObject* pExtObject);
void AptUnRegisterExtension(AptExtObject* pExtObject);
