#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"
#include "rw/rwcore_structs.h"               // rw::Resource, rw::BaseResourceDescriptors<5>
#include "pc/gcm/renderengine/texture.h"     // renderengine::Texture (the runtime resource)

// CgsResource::RwRasterResourceType -- the texture/raster resource-type handler (id 0).
// Bodies follow the X360 ARTIST handler; the descriptor path is a faithful port, the fixup
// pair is the documented PC divergence (D3D-texture create/release instead of the console's
// packed-GPU-offset rebase). The runtime resource object is a renderengine::Texture.

namespace CgsResource
{
    RwRasterResourceType::RwRasterResourceType()
        : Type()
    {
    }

    uint32_t RwRasterResourceType::GetTypeID() const
    {
        // "Texture" / RwRaster = 0 (wiki, cross-validated against the X360 GetTypeID siblings;
        // see CgsResourceTypeIds.h). The X360 body is a folded "return 0".
        return E_RESOURCETYPE_TEXTURE;
    }

    ResourceDescriptor RwRasterResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // Faithful to X360 0x828A9858:
        //     renderengine::Texture::Parameters params = {-1, 0, ...};
        //     renderengine::Texture::GetParameters(resource, &params);
        //     renderengine::Texture::GetResourceDescriptor(out, &params);
        // The descriptor is rw::BaseResourceDescriptors<5> (5 x {size, align}); GetResourceDescriptor
        // writes its ten u32s (slot0 = the Texture object, slot2 = the page-aligned pixel data).
        ResourceDescriptor lDescriptor;
        renderengine::Texture::Parameters lParameters;
        renderengine::Texture::GetParameters(static_cast<const renderengine::Texture*>(lpResource), &lParameters);
        renderengine::Texture::GetResourceDescriptor(reinterpret_cast<u32*>(&lDescriptor), &lParameters);
        return lDescriptor;
    }

    bool RwRasterResourceType::DeSerialise(void* /*lpResource*/) const
    {
        // The X360 body is folded/unnamed; the PS3 has only a variable hint
        // (CgsRwRasterResourceTypePS3.cpp:185). A raster carries no byte-swappable payload of its own
        // (the pixel bytes are opaque blobs and our platform-4 bundle is already native little-endian),
        // so on PC this is a no-op.
        //
        // CRITICAL ORDERING: Pool::FixUpEntry (0x828EB860) runs FixUp *before* DeSerialise, and FixUp
        // has already realised the runtime D3D texture (renderengine::Texture::Create). DeSerialise must
        // NOT clear mpD3DTexture here -- doing so discards the just-created texture and leaves the raster
        // textureless (the font then renders solid-colour quads). Create owns the handle's lifecycle.
        return true;
    }

    void RwRasterResourceType::FixDown(void* lpResource, const rw::Resource& /*lrResource*/) const
    {
        // PC DIVERGENCE of X360 FixDown (0x828A88E0): the console FixDown un-rebases the
        // raster's packed GPU-memory offsets (raster +0x20/+0x30) and resets its header. The
        // PC D3D9 raster has no packed offsets; FixDown is the inverse of FixUp, so it releases
        // the realised D3D texture.
        renderengine::Texture* lpTexture = static_cast<renderengine::Texture*>(lpResource);
        renderengine::Texture::Destroy(lpTexture);
    }

    void RwRasterResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // PC DIVERGENCE of X360/PS3 FixUp (0x828A8930): the console FixUp rebases the raster's
        // packed pixel offsets into the graphics allocation (the console raster IS GPU memory,
        // base = rw::Resource graphics slot). The PC D3D9 raster has no such offsets; FixUp
        // instead realises the texture -- create the managed D3D9 texture from the serialised
        // header and upload the pixel bytes the loader placed in the graphics-local resource
        // (rw slot 2, per CgsResource ConvertToRWResource: small[1] -> rw[2] = graphics-local).
        renderengine::Texture* lpTexture = static_cast<renderengine::Texture*>(lpResource);
        renderengine::Texture::Parameters lParameters;
        renderengine::Texture::GetParameters(lpTexture, &lParameters);
        const void* lpPixelData = lrResource.m_baseResources[2];
        renderengine::Texture::Create(lpTexture, &lParameters, lpPixelData);
    }

    uint32_t RwRasterResourceType::GetImportCount(const void* /*lpResource*/) const
    {
        // A raster is a leaf resource -- it imports nothing. (The importing party is
        // RwTextureState, which references a raster through its own GetImportPointer.)
        return 0;
    }

    void RwRasterResourceType::GetImportPointer(const void* /*lpResource*/, uint32_t /*luIndex*/, uint32_t* lpuOffset, const void** lppValue) const
    {
        // No imports (see GetImportCount); never called with a valid index, but stay defined.
        if (lpuOffset != 0)
            *lpuOffset = 0;
        if (lppValue != 0)
            *lppValue = 0;
    }

    EDebugResourceCategory RwRasterResourceType::GetDebugResourceCategory() const
    {
        return E_DEBUGRESOURCECATEGORY_TEXTURE;
    }

    void RegisterRwRasterResourceType()
    {
        static RwRasterResourceType sRwRasterResourceType;
        TypeRegistry::Register(&sRwRasterResourceType);
    }
}
