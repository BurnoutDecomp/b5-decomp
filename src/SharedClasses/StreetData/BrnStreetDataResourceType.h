#ifndef BRN_STREET_DATA_RESOURCE_TYPE_H
#define BRN_STREET_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnStreetData
{
struct StreetData
{
    int FixUp(int liDelta);
    int FixDown(int liDelta);
};

class StreetDataResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
