#include "GameShared/GameClasses/RenderWare/CgsRwTextureStateResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"
#include "rw/rwcore_structs.h"                   // rw::Resource, rw::BaseResourceDescriptors<5>
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::TextureState

#include <cstddef>   // offsetof
#include <cstdint>   // uintptr_t

// CgsResource::RwTextureStateResourceType -- the texture-state (sampler) handler (id 0xE).
// A faithful port of the X360 ARTIST bodies: fixed descriptor, one raster import at the state's
// raster slot, debug-validate that the import resolved. The X360 ARTIST FixDown is a stub and
// FixUp folded; the raster import is resolved by the pool (via GetImportPointer), so the state
// needs no per-resource fixup of its own.

namespace CgsResource
{
    RwTextureStateResourceType::RwTextureStateResourceType()
        : Type()
    {
    }

    uint32_t RwTextureStateResourceType::GetTypeID() const
    {
        return E_RESOURCETYPE_TEXTURE_STATE;  // 0xE = 14 (X360 0x828A8118: "return 14")
    }

    ResourceDescriptor RwTextureStateResourceType::GetSerialisedResourceDescriptor(const void* /*lpResource*/) const
    {
        // Faithful to X360 0x828A9A60: a fixed rw::BaseResourceDescriptors<5> -- slot0 = the
        // TextureState object (X360 hardcoded {0x24=36, 0x10=16}; PC uses sizeof), slots 1..4 =
        // {0, 1}. Written through the ten u32s of the descriptor.
        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = static_cast<u32>(sizeof(renderengine::TextureState));  // X360: 0x24
        lpData[1] = 16u;
        lpData[2] = 0u;  lpData[3] = 1u;
        lpData[4] = 0u;  lpData[5] = 1u;
        lpData[6] = 0u;  lpData[7] = 1u;
        lpData[8] = 0u;  lpData[9] = 1u;
        return lDescriptor;
    }

    bool RwTextureStateResourceType::DeSerialise(void* /*lpResource*/) const
    {
        // PS3 has a body (CgsRwTextureStateResourceTypePS3.cpp:137); the X360 ARTIST body is
        // folded. The sampler block is opaque platform data and the raster is an import resolved
        // by the pool, so the PC deserialise is a no-op success. NOTE: pending exact-body recovery.
        return true;
    }

    void RwTextureStateResourceType::FixDown(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
        // X360 ARTIST FixDown (0x828A8120) is a stub (no-op); the import is undone by the pool's
        // import machinery, not here.
    }

    void RwTextureStateResourceType::FixUp(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
        // X360 FixUp folded; with FixDown stubbed and the raster import resolved by the pool via
        // GetImportPointer (ResolveImportForEntry), the state needs no fixup of its own. No-op.
    }

    uint32_t RwTextureStateResourceType::GetImportCount(const void* /*lpResource*/) const
    {
        // A texture state imports exactly one raster (see GetImportPointer). X360 body folded;
        // inferred from the single import GetImportPointer reports.
        return 1;
    }

    void RwTextureStateResourceType::GetImportPointer(const void* lpResource, uint32_t /*luIndex*/, uint32_t* lpuOffset, const void** lppValue) const
    {
        // Faithful to X360 0x828A8128: import 0 lives at the state's raster slot (X360 +0x20).
        //   *lpuOffset = 0x20;  *lppValue = *(state + 0x20);
        const renderengine::TextureState* lpState = static_cast<const renderengine::TextureState*>(lpResource);
        if (lpuOffset != 0)
            *lpuOffset = static_cast<u32>(offsetof(renderengine::TextureState, mpRaster));  // X360: 0x20
        if (lppValue != 0)
            *lppValue = lpState->mpRaster;
    }

    bool RwTextureStateResourceType::DebugValidate(const void* lpResource) const
    {
        // Faithful to X360 0x828A8140: the raster slot must be a resolved import, i.e. not the
        // -1 unresolved sentinel (the X360 asserts "Texture state has invalid texture value").
        const renderengine::TextureState* lpState = static_cast<const renderengine::TextureState*>(lpResource);
        const uintptr_t luRaster = reinterpret_cast<uintptr_t>(lpState->mpRaster);
        return luRaster != static_cast<uintptr_t>(renderengine::TextureState::KU_INVALID_TEXTURE)
            && luRaster != 0;
    }

    EDebugResourceCategory RwTextureStateResourceType::GetDebugResourceCategory() const
    {
        return E_DEBUGRESOURCECATEGORY_TEXTURE;
    }

    void RegisterRwTextureStateResourceType()
    {
        static RwTextureStateResourceType sRwTextureStateResourceType;
        TypeRegistry::Register(&sRwTextureStateResourceType);
    }
}
