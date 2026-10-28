#ifndef CGS_GUI_POPUP_RESOURCE_TYPE_H
#define CGS_GUI_POPUP_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised GUI popup. Derives from CgsResource::Type;
// the listed methods are virtual overrides. From DecFIGS DWARF.
class GuiPopupResourceType : public Type
{
public:
    uint32_t           GetTypeID() const ;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const ;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const ;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
};
}

#endif
