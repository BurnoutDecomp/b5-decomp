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
// SHAPE verbatim from the LegacySrc reference
// for the type layout and
// the member/method names; this is the minimal owning home for the surface the
// X360 ARTIST build's CgsAptObjectController / CgsAptAnimator translation units
// reach through:
//   - operator new(size_t)                    (Animator::Construct allocates one)
//   - AptExtObject(int32_t iNumMembers)        (its construction)
//   - static GetVariable(context, key)         (ObjectController::GetObjectValue)
//   - static SetVariable(context, key, value)  (ObjectController::SetObjectVariableBoolean)
//   - virtual GetName()                        (ObjectController overrides it)
//
// The reference declares GetName()/Initialize() as pure virtuals; here they are given
// trivial default bodies so this minimal base is concrete (the real per-extension
// overrides live in their own classes). The large remainder of the reference class is
// left out as an honest minimal owning header rather than fabricated.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueGC base, AptValue, AptNativeString

class AptNativeHash;       // SDKs/EATech/include/Apt/AptNativeHash.h (held by pointer only)
class AptNativeFunction;

class AptExtObject : public AptValueGC
{
public:
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Construct an extension object reserving room for iNumMembers native members.
    explicit AptExtObject(const int32_t iNumMembers);
    virtual ~AptExtObject();

    // The apt VM queries each extension's display name through this virtual; every
    // concrete extension (e.g. CgsGui::ObjectController) overrides it. Reference declares
    // it pure; given a trivial default here so this minimal base stays concrete.
    virtual const char* GetName() { return 0; }

    // One-time setup hook the apt VM calls after construction. Reference: pure virtual.
    virtual void Initialize() {}

    // GC-required virtuals (AptValue leaves RegisterReferences pure; satisfy it here
    // so this base is concrete).
    virtual void RegisterReferences() {}
    virtual void DestroyGCPointers() {}

    uint32_t GetSize() const { return mnObjectSize; }

protected:
    // Look the named ActionScript variable up on / store it onto the bound apt
    // context value. Static helpers in the reference (the context is passed explicitly).
    static AptValue* GetVariable(AptValue* pContext, const AptNativeString* const pVariable);
    static bool      SetVariable(AptValue* pContext, const AptNativeString* const pName,
                                 AptValue* const pValue);

    // FLAG: the apt native-method argument fetch -- a native sMethod_* reads its i-th
    // ActionScript call argument off the apt interpreter operand stack via this helper
    // (X360 AptExtObject::GetParam). Declared here (body its own TU) so the native
    // methods of derived extensions (e.g. CgsGui::AptCommunicator) compile.
    static AptValue* GetParam(int iIndex);

private:
    AptNativeHash* mpNativeHash;   // native member hash table
    uint32_t       mnObjectSize;   // reserved object size
};

// Apt VM extension registry: register / unregister a native
// extension object with the apt ActionScript VM so its members become scriptable.
void AptRegisterExtension(AptExtObject* pExtObject);
void AptUnRegisterExtension(AptExtObject* pExtObject);
