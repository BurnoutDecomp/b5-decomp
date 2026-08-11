#ifndef BRN_STREET_DATA_RESOURCE_TYPE_H
#define BRN_STREET_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

#include <cstdint>   // uintptr_t (the full-width x64 load-base delta)

namespace BrnStreetData
{
// Minimal slice of the owning SharedClasses/StreetData/BrnStreetData.h class --
// declaration must MATCH that header's signatures exactly (x64 relayout wave
// 2026-08-11 widened the delta to uintptr_t; the old int(int) slice here kept
// mangling to the retired signature and broke the link).
struct StreetData
{
    void FixUp(uintptr_t luDelta);
    void FixDown(uintptr_t luDelta);
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
