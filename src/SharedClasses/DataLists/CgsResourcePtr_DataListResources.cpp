#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for the DataLists resource-list smart pointers
// (CgsResource::ResourcePtr<T> for the Challenge / Vehicle / Wheel list resource
// element types). The generic accessor bodies are inline in CgsResourcePtr.h; this
// .cpp only forces the out-of-line emission of the symbols the X360 ARTIST build
// attested under the IDA-truncated "BrnResource::*" names.
//
// The accessors only static_cast the base mpResourceMemory to Type* / bind a Type&
// reference and return it (no member access), so a forward declaration of each
// element type is sufficient -- the full layouts live in their own TUs and are not
// needed (and intentionally not pulled in) to emit these thin wrappers.
//
//   ChallengeListRes      @ 0x82678E40  ResourcePtr<ChallengeListResource>::operator->()        (baked assert line 544, non-const)
//   ChallengeL            @ 0x82324D20  ResourcePtr<ChallengeListResource>::operator->() const  (baked assert line 563)
//   VehicleListR          @ 0x82210B08  ResourcePtr<VehicleListResource>::operator->() const    (baked assert line 563)
//   VehicleListResourc    @ 0x82665E88  ResourcePtr<VehicleListResource>::operator*()  const    (baked assert line 637, "dereference")
//   WheelListResou        @ 0x822C9AE8  ResourcePtr<WheelListResource>::operator->()   const    (baked assert line 563)

namespace BrnResource
{
    // Forward declarations -- the explicit instantiations below never dereference the
    // element layout (the wrappers only cast/return), so the complete types (which
    // live in VehicleListResourceType.cpp / WheelList.h / their own deferred TUs) are
    // deliberately not required here.
    class ChallengeListResource;
    struct VehicleListResource;
    class WheelListResource;
}

template BrnResource::ChallengeListResource*
    CgsResource::ResourcePtr<BrnResource::ChallengeListResource>::operator->();
template const BrnResource::ChallengeListResource*
    CgsResource::ResourcePtr<BrnResource::ChallengeListResource>::operator->() const;

template const BrnResource::VehicleListResource*
    CgsResource::ResourcePtr<BrnResource::VehicleListResource>::operator->() const;
template const BrnResource::VehicleListResource&
    CgsResource::ResourcePtr<BrnResource::VehicleListResource>::operator*() const;

template const BrnResource::WheelListResource*
    CgsResource::ResourcePtr<BrnResource::WheelListResource>::operator->() const;
