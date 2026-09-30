#ifndef CGS_ENTRY_LIST_RESOURCE_H
#define CGS_ENTRY_LIST_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include <cstddef>

namespace CgsResource
{
// DWARF CgsEntryListResource.h:34; ARTIST CreateResourceList 0x828FF480
// stores the count at +0x100 and the 64-bit IDs at +0x108.
struct EntryListResource
{
    static const u32 KI_MAXOWNERLENGTH = 256;
    static const u32 KI_HEADERSIZE = 264;
    char macOwnerName[KI_MAXOWNERLENGTH];
    u32 muNumEntries;
    ID mIds[1]; // variable-length allocation
};
static_assert(offsetof(EntryListResource, muNumEntries) == 0x100, "entry-list count");
static_assert(offsetof(EntryListResource, mIds) == EntryListResource::KI_HEADERSIZE, "entry-list IDs");

class EntryListResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
