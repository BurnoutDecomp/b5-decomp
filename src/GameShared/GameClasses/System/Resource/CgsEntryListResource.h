#ifndef CGS_ENTRY_LIST_RESOURCE_H
#define CGS_ENTRY_LIST_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
class EntryListResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
