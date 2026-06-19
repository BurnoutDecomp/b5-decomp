#ifndef CGS_DICTIONARY_RESOURCE_TYPE_H
#define CGS_DICTIONARY_RESOURCE_TYPE_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsDictionary.h"        // CgsContainers::Dictionary<T>
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h" // CgsResource::Type (base + rw::Resource fwd)

// ============================================================================
// GameShared/GameClasses/Containers/CgsDictionaryResourceType.h
//
// The resource-type HANDLER family for serialised CgsContainers::Dictionary<T>
// resources. Recovered from the DecFIGS DWARF
// (GameShared/GameClasses/Containers/CgsDictionaryResourceType.h):
//
//   struct DictionaryResourceTypeBase : public CgsResource::Type
//   {
//       virtual ResourceDescriptor GetSerialisedResourceDescriptor(const void*) const; // :47
//   };
//   template <class T>
//   struct DictionaryResourceType<T> : public DictionaryResourceTypeBase
//   {
//       virtual uint32_t GetTypeID() const;                       // :101
//       virtual void     FixDown(void*, const Resource&) const;   // :162
//       virtual void     FixUp(void*, const Resource&) const;     // :183
//   };
//
// These are VIRTUAL overrides of the CgsResource::Type vtable (GetTypeID slot 0,
// FixDown slot 3, FixUp slot 4 -- see CgsResourceType.h). The wrapper FixUp/FixDown
// are the per-T resource-system entry points; the X360 ARTIST build INLINES the
// generic CgsContainers::Dictionary<T>::FixUp/FixDown(const Resource&) body straight
// into each (e.g. DictionaryResourceType<ICE::ICETakeData>::FixUp @0x82665FF8,
// ::FixDown @0x82666090, ::GetTypeID @0x826659E0 returning 65). Restored here as the
// logical forward-to-Dictionary<T> form (semantic parity; the explicit instantiation
// for ICE::ICETakeData lives in CgsDictionaryResourceType.cpp).
//
// FLAG: the X360 fixup virtuals pass their `const rw::Resource&` argument STRAIGHT
// THROUGH to T::FixUp/FixDown, which the committed homes (CgsDictionary.h / ICEData.hpp)
// declare as taking `const CgsResource::Resource&`. CgsResource::Resource is the
// project's opaque resource-fixup placeholder (forward-declared, never dereferenced by
// the committed bodies -- ICETakeData::FixDown ignores it entirely). The same logical
// resource object reaches both layers, so the wrapper bridges the two const& spellings
// with a reference-reinterpret (documented, no value is read through it). When
// CgsResource::Resource is unified with rw::Resource the bridge drops out.
// ============================================================================

namespace CgsContainers
{

// CgsDictionaryResourceType.h:44 (DWARF). The non-templated base shared by every
// DictionaryResourceType<T>: it sits between CgsResource::Type and the per-T wrapper
// and supplies the type-independent GetSerialisedResourceDescriptor override. Declared
// here; bodies are out-of-line (its own future TU). Modelled stateless (no data members
// beyond CgsResource::Type's), matching the DWARF (no fields declared on the base).
struct DictionaryResourceTypeBase : public CgsResource::Type
{
    // CgsDictionaryResourceType.cpp:47 (DWARF). Declaration-only here (the /c gate does
    // not link; its body is a sibling deliverable).
    virtual CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const;
};

// CgsDictionaryResourceType.h:63 (DWARF). Per-T resource-type handler. GetTypeID returns
// the T-specific registry id; FixUp/FixDown forward to the generic Dictionary<T> pass.
//
// NOTE: the template parameter is named DataType (not Type) deliberately -- the base
// CgsResource::Type injects its own class name `Type` into this derived scope, which
// would shadow a parameter named `Type` and silently bind Dictionary<Type> to
// Dictionary<CgsResource::Type>.
template <class DataType>
struct DictionaryResourceType : public DictionaryResourceTypeBase
{
    // CgsDictionaryResourceType.h:101 (DWARF). The T-specific resource registry id.
    // Body is per-instantiation (a T-specific constant) -> explicitly specialised in
    // CgsDictionaryResourceType.cpp for each concrete T.
    virtual u32 GetTypeID() const;

    // CgsDictionaryResourceType.h:183 (DWARF). Relocate-on-load: forward to the generic
    // Dictionary<T>::FixUp pass on the resource cast to its dictionary type.
    virtual void FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // Bridge rw::Resource& -> the opaque CgsResource::Resource& the Dictionary<T>
        // pass declares (same logical object; never dereferenced). See header FLAG.
        static_cast<Dictionary<DataType>*>(lpResource)->FixUp(
            reinterpret_cast<const CgsResource::Resource&>(lrResource));
    }

    // CgsDictionaryResourceType.h:162 (DWARF). Relocate-for-save: forward to the generic
    // Dictionary<T>::FixDown pass (entries-first, then base).
    virtual void FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<Dictionary<DataType>*>(lpResource)->FixDown(
            reinterpret_cast<const CgsResource::Resource&>(lrResource));
    }
};

} // namespace CgsContainers

#endif // CGS_DICTIONARY_RESOURCE_TYPE_H
