#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIdList.h"   // complete CgsResource::ResourceIdList

// Per-instantiation TU for CgsResource::ResourcePtr<CgsResource::ResourceIdList>
// (ledger id: class:CgsResource::ResourceIdList>).
//
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only forces the
// out-of-line emission of the single symbol the X360 ARTIST build attested:
//
//   operator->()  @ 0x828EA4C0  (non-const; null-resource assert at CgsResourcePtr.h:544 --
//                                "Can not instance resource pointer - it has no main memory
//                                resource"; returns the ResourceIdList* at the base's
//                                mpResourceMemory). Called by
//                                CgsResource::PoolModule::DoAcquireResourceListRequest
//                                (@0x828FCE40), which then reads GetNumIds()/GetId(i).
template CgsResource::ResourceIdList*
CgsResource::ResourcePtr<CgsResource::ResourceIdList>::operator->();
