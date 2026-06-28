#ifndef BRN_FLAPT_FILE_RESOURCE_TYPE_H
#define BRN_FLAPT_FILE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

// ============================================================================
// SharedClasses/Gui/Flapt/BrnFlaptFileResourceType.h
//
// BrnFlapt::FlaptFileResourceType -- the resource-type handler for a serialised
// "Flapt" (Flash-derived) GUI movie file (registry type id 0x10020 / 65568).
// Derives from CgsResource::Type; the listed methods are virtual overrides whose
// shapes are recovered from the DecFIGS DWARF (BrnFlaptFileResourceType.h) and
// verified against the X360 ARTIST bodies. Reconstructed from BURNOUT_X360_ARTIST.XEX.
// ============================================================================

namespace BrnFlapt
{
    class FlaptFileResourceType : public CgsResource::Type
    {
    public:
        uint32_t                       GetTypeID() const override;
        CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
        void                           FixDown(void* lpResource, const rw::Resource& lrResource) const override;
        void                           FixUp(void* lpResource, const rw::Resource& lrResource) const override;
        uint32_t                       GetImportCount(const void* lpResource) const override;
        void                           GetImportPointer(const void* lpResource, uint32_t luIndex,
                                                        uint32_t* lpuOffset, const void** lppValue) const override;
    };
}

#endif // BRN_FLAPT_FILE_RESOURCE_TYPE_H
