#ifndef CGS_RW_VERTEX_DESC_RESOURCE_TYPE_PS3_H
#define CGS_RW_VERTEX_DESC_RESOURCE_TYPE_PS3_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised RenderWare vertex descriptor. Derives from
// CgsResource::Type; FixUp is a virtual override. From DecFIGS DWARF.
class RwVertexDescResourceType : public Type
{
public:
    void FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
