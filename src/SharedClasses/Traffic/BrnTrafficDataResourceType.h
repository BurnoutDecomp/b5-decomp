#ifndef BRN_TRAFFIC_DATA_RESOURCE_TYPE_H
#define BRN_TRAFFIC_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnTraffic
{
struct TrafficData
{
    // ADDITIVE GROW for GetSerialisedResourceDescriptor @ 0x82760660: the X360 reads
    // the serialised byte-size word at +4 (`lwz r11, 4(r30)`) — the total size of the
    // traffic-data block — and asserts it is non-zero ("Uninitialised Traffic Data
    // resource"). The +0 word is read by the type's FixDown path (forwarded to
    // TrafficData::FixDown) but its meaning is not part of this slice.
    // FLAG: muReserved0 (+0) is an opaque serialised header word (DEFERRED); only the
    //       +4 size word is X360-attested here.
    u32 muReserved0;   // +0  opaque serialised header word (DEFERRED)
    u32 muSizeInBytes; // +4  serialised total byte size (asm `*(a3+4)`)

    int FixDown();
};

class TrafficDataResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
