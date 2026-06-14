#ifndef BRN_GUI_PFX_HOOKS_RESOURCE_H
#define BRN_GUI_PFX_HOOKS_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised GUI PFX hook bundle. Derives from
// CgsResource::Type; GetTypeID/FixUp/FixDown are virtual overrides, Serialise is the
// type's own virtual. From DecFIGS DWARF.
class PFXHookBundleResourceType : public Type
{
public:
    uint32_t      GetTypeID() const override;
    void          FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void          FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
