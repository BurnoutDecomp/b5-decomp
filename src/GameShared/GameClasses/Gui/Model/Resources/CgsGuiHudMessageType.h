#ifndef CGS_GUI_HUD_MESSAGE_TYPE_H
#define CGS_GUI_HUD_MESSAGE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised HUD message. Derives from CgsResource::Type;
// GetTypeID/FixDown/FixUp are virtual overrides. From DecFIGS DWARF.
class HudMessageResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
