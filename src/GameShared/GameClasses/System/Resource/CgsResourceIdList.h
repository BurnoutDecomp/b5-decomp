#ifndef CGS_RESOURCE_ID_LIST_H
#define CGS_RESOURCE_ID_LIST_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // CgsResource::ID

// CgsResource::ResourceIdList -- the serialised payload of an "id list" resource: a flat array
// of CgsResource::ID (each a 64-bit resource hash) plus a count. The pool's
// DoAcquireResourceListRequest (@0x828FCE40) loads one through a ResourcePtr<ResourceIdList>,
// reads GetNumIds() and walks GetId(i) to resolve every listed resource. The leading mpaIds is
// a file-relative pointer the IdListResourceType FixUp/FixDown rebase by the load-base delta.
//
// Layout recovered from the DecFIGS DWARF (CgsResourceIdList.h:43): mpaIds @ +0, muNumIds @ +4,
// muPad[2] @ +8. The X360 spine for DoAcquireResourceListRequest confirms it (it reads the
// id-array pointer at +0 and the count at +4 via the ResourcePtr accessor).
namespace CgsResource
{
    struct ResourceIdList
    {
        void Construct();                  // CgsResourceIdList.h:48
        void Destruct();                   // CgsResourceIdList.h:52
        void FixUp(void* lpBaseAddress);   // CgsResourceIdList.h:57
        void FixDown(void* lpBaseAddress); // CgsResourceIdList.h:62
        void ConstructIds();               // CgsResourceIdList.h:66

        // CgsResourceIdList.h:85 -- bounds-checked element access (asserts "Index out of range"
        // at CgsResourceIdList.h:145 when luIndex >= muNumIds).
        ID       GetId(u32 luIndex) const;
        // CgsResourceIdList.h:89 -- the number of ids in the list.
        u32      GetNumIds() const { return muNumIds; }

    private:
        ID* mpaIds;       // +0x00  CgsResourceIdList.h:94  file-relative -> loaded id-array pointer
        u32 muNumIds;     // +0x04  CgsResourceIdList.h:95  number of ids
        u8  muPad[2];     // +0x08  CgsResourceIdList.h:96  trailing padding
    };
}

#endif // CGS_RESOURCE_ID_LIST_H
