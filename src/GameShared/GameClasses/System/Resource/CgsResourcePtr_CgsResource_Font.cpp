#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// CgsResource::Font is forward-declared (NOT #include "CgsFont.h") deliberately. The single
// attested accessor operator->() only does `static_cast<Font*>(void* mpResourceMemory)` and returns
// it -- a pointer static_cast from void* needs only an incomplete class type, never a dereference.
// Pulling in CgsFont.h here would be illegal anyway: CgsFont.h and the committed CgsResourceHandle.h
// (reached via CgsResourcePtr.h) BOTH define CgsResource::SafeResourceHandle<T> with different
// bodies -> C2953 redefinition the moment both land in one TU.
// *** DEP_FLAG (cross-home ODR conflict, NOT mine to fix here) ***
//   CgsResource::SafeResourceHandle<T> is defined TWICE:
//     - b5-decomp/src/GameShared/GameClasses/Fonts/CgsFont.h:20 (own shape)
//     - b5-decomp/src/GameShared/GameClasses/System/Resource/CgsResourceHandle.h:54 (: ResourceHandle)
//   Any TU that includes both fails to compile. CgsFont.h should drop its private
//   SafeResourceHandle and reuse the committed CgsResourceHandle.h one (grow that home if shapes
//   differ). Owned by the Fonts / Resource-handle home owners, not this instantiation TU.
namespace CgsResource { struct Font; }

// Per-instantiation TU for CgsResource::ResourcePtr<CgsResource::Font> (the
// "CgsResource::Font>" templated-instance ledger id -- a CgsResourcePtr<Font> sibling to the
// committed CgsResource::BaseResourcePtr). The bodies ARE the generic inline accessors in
// CgsResourcePtr.h; this .cpp only forces the out-of-line emission of the one symbol the X360
// ARTIST build attested.
//
//   operator->()  @ 0x823C1C20  (non-const)
//
// X360 @0x823C1C20: `if (!**a1) ASSERT-fire; return **a1;` -- reads offset 0 (mpResourceMemory),
// asserts the null-resource message, returns it as Font*. This is the generic non-const
// ResourcePtr<Type>::operator->(). CORRECTION: the X360 @0x823C1C20 DOUBLE-derefs --
// `if(!**a1) ASSERT; return **a1`. *a1 = mpResourceMemory (the +0 word); **a1 = *mpResourceMemory.
// So for ResourcePtr<Font> the +0 word is a Font** (a slot holding the Font*), ONE indirection
// more than the single-deref generic (which returns mpResourceMemory itself). Verified against the
// single-deref sibling TriggerData>::operator-> @0x823685E8 (one load; Font does two). So this is
// NOT the generic body -- it is an explicit member specialization that double-derefs. (Font stays
// incomplete: we follow the void* once to yield a Font*, never touch a Font member.) The baked
// CgsResourceHandle.h:403 file/line is discarded per project convention.
template<>
CgsResource::Font* CgsResource::ResourcePtr<CgsResource::Font>::operator->()
{
    CgsResource::Font* lpResource = *static_cast<CgsResource::Font* const*>(mpResourceMemory);
    CGS_ASSERT(lpResource, "Can not instance resource pointer - it has no main memory resource\n");
    return lpResource;
}
