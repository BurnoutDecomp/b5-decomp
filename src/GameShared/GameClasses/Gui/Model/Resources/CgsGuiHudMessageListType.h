#ifndef CGS_GUI_HUD_MESSAGE_LIST_TYPE_H
#define CGS_GUI_HUD_MESSAGE_LIST_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised HUD message list. Derives from
// CgsResource::Type; GetTypeID/FixDown are virtual overrides. From DecFIGS DWARF.
class HudMessageListResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
