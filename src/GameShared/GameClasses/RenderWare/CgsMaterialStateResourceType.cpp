#include "GameShared/GameClasses/RenderWare/CgsMaterialStateResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "rw/rwcore_structs.h"                   // rw::Resource, rw::BaseResourceDescriptors<5>
#include "GameShared/GameClasses/Graphics/CgsMaterialState.h"
#include "types.hpp"   // uintptr_t

// CgsResource::MaterialStateResourceType -- the material-state (blend-state) handler (id 0xF).
// A faithful port of the X360 ARTIST bodies. FixUp/FixDown rebase three main-memory self-pointers
// by the resource's main base (m_baseResources[0]); this is ordinary serialised-pointer fixup
// (offset <-> absolute), identical in intent on PC, so it is a port rather than a divergence.

namespace CgsResource
{
    MaterialStateResourceType::MaterialStateResourceType()
        : Type()
    {
    }

    uint32_t MaterialStateResourceType::GetTypeID() const
    {
        return E_RESOURCETYPE_MATERIAL_STATE;  // 0xF = 15 (X360 0x828A8108: "return 15")
    }

    ResourceDescriptor MaterialStateResourceType::GetSerialisedResourceDescriptor(const void* /*lpResource*/) const
    {
        // ARTIST 828A98C0 sizes the serialized material and its embedded state data.
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0] = rw::BaseResourceDescriptor(0xFC, 16);
        return lDescriptor;
    }

    bool MaterialStateResourceType::DeSerialise(void* /*lpResource*/) const
    {
        // PS3 has a body (CgsMaterialStateResourceTypePS3.cpp:158); the X360 ARTIST body is
        // folded. The block is opaque render state fixed up by FixUp, so the PC deserialise is a
        // no-op success. NOTE: pending exact-body recovery.
        return true;
    }

    void MaterialStateResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        // Faithful port of X360 0x828A8AB8:
        //     base = *a2;                       // a2[0] = rw::Resource main base
        //     result[0] += base; result[1] += base; result[2] += base;
        // i.e. rebase the three serialised state-object self-pointers offset -> absolute. The
        // native pointer slots are 32-bit at offsets 0, 4 and 8.
        CgsGraphics::MaterialState* lpState = static_cast<CgsGraphics::MaterialState*>(lpResource);
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);
        lpState->mpBlendState = reinterpret_cast<renderengine::BlendState*>(
            reinterpret_cast<uintptr_t>(lpState->mpBlendState) + luBase);
        lpState->mpDepthStencilState = reinterpret_cast<renderengine::DepthStencilState*>(
            reinterpret_cast<uintptr_t>(lpState->mpDepthStencilState) + luBase);
        lpState->mpRasterizerState = reinterpret_cast<renderengine::RasterizerState*>(
            reinterpret_cast<uintptr_t>(lpState->mpRasterizerState) + luBase);
    }

    void MaterialStateResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        // Faithful port of X360 0x828A8A80: inverse of FixUp (absolute -> offset), and -- before
        // un-rebasing the third pointer -- mark the rasterizer object it references:
        //     v3 = result[2];                   // the (still absolute) rasterizer pointer
        //     result[0] -= base; result[1] -= base;
        //     *(u32*)(v3 + 0x28) = 1;           // rasterizer word 10 (serialised-blob poke)
        //     result[2] -= base;
        CgsGraphics::MaterialState* lpState = static_cast<CgsGraphics::MaterialState*>(lpResource);
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);
        renderengine::RasterizerState* lpReferenced = lpState->mpRasterizerState;
        lpState->mpBlendState = reinterpret_cast<renderengine::BlendState*>(
            reinterpret_cast<uintptr_t>(lpState->mpBlendState) - luBase);
        lpState->mpDepthStencilState = reinterpret_cast<renderengine::DepthStencilState*>(
            reinterpret_cast<uintptr_t>(lpState->mpDepthStencilState) - luBase);
        lpReferenced->muPrimitiveResetEnable = 1u;
        lpState->mpRasterizerState = reinterpret_cast<renderengine::RasterizerState*>(
            reinterpret_cast<uintptr_t>(lpState->mpRasterizerState) - luBase);
    }

    uint32_t MaterialStateResourceType::GetImportCount(const void* /*lpResource*/) const
    {
        // The three fixup pointers are main-memory self-references, not external resource imports,
        // so a material state imports nothing. NOTE: the X360 GetImportCount body is folded;
        // inferred 0 from the fixup (no import-style resolution) -- verify if a material ever
        // resolves through this.
        return 0;
    }

    void MaterialStateResourceType::GetImportPointer(const void* /*lpResource*/, uint32_t /*luIndex*/, uint32_t* lpuOffset, const void** lppValue) const
    {
        // No imports (see GetImportCount).
        if (lpuOffset != 0)
            *lpuOffset = 0;
        if (lppValue != 0)
            *lppValue = 0;
    }
}
