#ifndef CGS_FONT_RESOURCE_TYPE_H
#define CGS_FONT_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // The font resource-type handler -- resource type id 0x21 ("Font"). Reconstructed from the
    // DecFIGS PS3 header (GameShared/GameClasses/Fonts/Resources/CgsFontResourceType.h) and the
    // X360 ARTIST bodies:
    //   GetTypeID         0x82834E50  (=33)
    //   FixUp             0x82835AF0  (rebase glyph arrays + page array; null texture state)
    //   FixDown           0x82835BF8  (inverse, incl. each page pointer)
    //   GetImportPointer  0x82834E60  (import i = the i-th atlas page, mpapTextures[i])
    //
    // The runtime resource is a CgsResource::Font. Its atlas pages are imported RwRasters, so the
    // pool resolves muNumTexturePages imports into mpapTextures[]. Overrides the load/save-path
    // base virtuals it needs; DeSerialise is inherited (the PS3 handler overrides Serialise, the
    // save path, which is out of scope for loading and not modelled here).
    class FontResourceType : public Type
    {
    public:
        FontResourceType();

        virtual uint32_t           GetTypeID() const;
        virtual ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const;
        virtual void               FixDown(void* lpResource, const rw::Resource& lrResource) const;
        virtual void               FixUp(void* lpResource, const rw::Resource& lrResource) const;
        virtual uint32_t           GetImportCount(const void* lpResource) const;
        virtual void               GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const;
    };

    // Construct the singleton handler and register it (type id 0x21). Call once at bring-up.
    void RegisterFontResourceType();
}

#endif
