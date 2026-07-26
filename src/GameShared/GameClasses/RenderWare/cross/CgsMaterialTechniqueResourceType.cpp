#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialTechniqueResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MaterialTechniqueResourceType::FixDown          @ 0x828A8740
//   CgsResource::MaterialTechniqueResourceType::FixUp            @ 0x828A8770
//   CgsResource::MaterialTechniqueResourceType::GetImportPointer @ 0x828A80A8
//   CgsResource::MaterialTechniqueResourceType::GetTypeID        @ 0x828A7EC0
//   CgsResource::MaterialTechniqueResourceType::PostFixUp        @ 0x828A7EC8
//
// FixDown/FixUp (un)relocate the three serialised pointers at +24/+28/+36 of the
// material-technique blob (delta = the rw::Resource's load base). PostFixUp resolves
// a loaded material: it queries the blend-state parameters, sets the technique's
// render flags (alpha-blend, alpha-test, hardware instancing when an
// InstancingMatrixArray shader constant is present), and caches three content hashes
// plus the shader's parameter count.

namespace renderengine
{
    class BlendState
    {
    public:
        // Fills the caller's parameter block (a 48-byte descriptor followed by the
        // per-channel enable flags) for the given blend state.
        static void* GetParameters(void* pBlendState, void* pParams);
    };
    void* BlendState::GetParameters(void*, void*) { __debugbreak(); return nullptr; }
}

namespace ShaderConstantsExternal
{
    bool HasShaderConstant(const void* pShader, const char* pName);
    bool HasShaderConstant(const void*, const char*) { __debugbreak(); return false; }
}

namespace CgsContainers
{
    namespace CgsHash16 { u32 CalculateHash(const void* pData, int liLen); u32 CalculateHash(const void*, int) { __debugbreak(); return 0; } }
    namespace CgsHash12 { u32 CalculateHash(const void* pData, int liLen); u32 CalculateHash(const void*, int) { __debugbreak(); return 0; } }
}

namespace CgsResource
{
    namespace
    {
        // SEAM (platform-4 flip-type blob): the MaterialTechnique blob KEEPS the console's
        // 32-bit u32-slot serialised convention (it is byte-flipped, NOT widened), so every
        // stored pointer is a u32 slot widened to a host pointer on read (PointerFromU32,
        // matching the other u32-slot resource handlers).
        template <typename T>
        T* PointerFromU32(u32 luAddress)
        {
            return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
        }
    }

    static const uint32_t KU_MATERIAL_TECHNIQUE_RESOURCE_TYPE_ID = 13;

    uint32_t MaterialTechniqueResourceType::GetTypeID() const
    {
        return KU_MATERIAL_TECHNIQUE_RESOURCE_TYPE_ID;
    }

    // Faithful port of X360 0x828A97D8. The serialised material-technique blob's tail
    // bytes describe the per-instance shader-parameter payload: byte[32]+byte[33] is the
    // parameter count, byte[34]/byte[35] are two trailing byte sizes. The whole-resource
    // size is (count+10) parameter words (4 bytes each) plus the two trailing bytes; the
    // remaining four descriptor entries are the empty {size 0, alignment 1} default. Slot 0
    // alignment is 16. The X360 reads each header byte with lbz + extsb (SIGN-extend to 32-bit),
    // so the blob bytes are read as signed (s8) -- matters only for byte values >= 0x80.
    ResourceDescriptor MaterialTechniqueResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const s8* lpBytes = static_cast<const s8*>(lpResource);
        const s32 liCount = static_cast<s32>(lpBytes[33]) + static_cast<s32>(lpBytes[32]);
        const u32 luSize  = static_cast<u32>(4 * (liCount + 10)
                            + static_cast<s32>(lpBytes[35]) + static_cast<s32>(lpBytes[34]));

        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = luSize;  lpData[1] = 16u;   // slot0: {whole-resource size, align 16}
        lpData[2] = 0u;  lpData[3] = 1u;
        lpData[4] = 0u;  lpData[5] = 1u;
        lpData[6] = 0u;  lpData[7] = 1u;
        lpData[8] = 0u;  lpData[9] = 1u;
        return lDescriptor;
    }

    // The relocation delta is the leading word of the rw::Resource (the load base).
    void MaterialTechniqueResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);
        u32 lu28 = *reinterpret_cast<u32*>(lRes + 28) - luDelta;
        u32 lu36 = *reinterpret_cast<u32*>(lRes + 36) - luDelta;
        *reinterpret_cast<u32*>(lRes + 24) -= luDelta;
        *reinterpret_cast<u32*>(lRes + 28) = lu28;
        *reinterpret_cast<u32*>(lRes + 36) = lu36;
    }

    void MaterialTechniqueResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);
        u32 lu28 = *reinterpret_cast<u32*>(lRes + 28) + luDelta;
        u32 lu36 = *reinterpret_cast<u32*>(lRes + 36) + luDelta;
        *reinterpret_cast<u32*>(lRes + 24) += luDelta;
        *reinterpret_cast<u32*>(lRes + 28) = lu28;
        *reinterpret_cast<u32*>(lRes + 36) = lu36;
    }

    void MaterialTechniqueResourceType::GetImportPointer(const void*, uint32_t, uint32_t*, const void**) const
    {
        CGS_ASSERT(false, "This function should not be called at runtime");
    }

    void MaterialTechniqueResourceType::PostFixUp(void* lpResource, const rw::Resource&) const
    {
        // SEAM (platform-4 flip-type blob): all technique/material-state/shader slots are
        // 32-bit u32 slots, indexed with u32 stride — NOT host-pointer stride. The previous
        // void** indexing read pMaterial[1] at byte +8; the serialised material-state slot
        // is the u32 @+4 (X360 lwz 4(r3)). Same for pMaterial[0] (shader, u32 @+0) and
        // pMaterial[5] (context hash source, u32 @+20). Slot values become host pointers
        // via PointerFromU32.
        u32* lpuMaterial = static_cast<u32*>(lpResource);
        CGS_ASSERT(lpuMaterial[1] != 0, "lpMaterial->mpMaterialState");         // u32 slot @+4
        const u32* lpuMaterialState = PointerFromU32<const u32>(lpuMaterial[1]);
        CGS_ASSERT(lpuMaterialState[0] != 0, "lpMaterial->mpMaterialState->mpBlendState"); // u32 slot @+0

        // Blend-state default parameter block (0x07070706 RGBA write masks etc.),
        // populated in place by GetParameters; the trailing bytes are the per-stage
        // enable flags read back below.
        struct BlendParams
        {
            u32 mauDword[12];
            u8  mabEnable[8];
        } lParams = {};
        lParams.mauDword[0] = 0x07060706u; // 117835526
        lParams.mauDword[1] = 0x07060706u;
        lParams.mauDword[2] = 0x07060706u;
        lParams.mauDword[3] = 0x07060706u;
        lParams.mauDword[4] = 7;
        lParams.mauDword[5] = 15;
        lParams.mauDword[6] = 15;
        lParams.mauDword[7] = 15;
        lParams.mauDword[8] = 15;
        lParams.mauDword[9] = 135;
        lParams.mauDword[10] = 0;
        lParams.mauDword[11] = static_cast<u32>(-1);

        renderengine::BlendState::GetParameters(PointerFromU32<void>(lpuMaterialState[0]), &lParams);

        u32* lpuFlags = lpuMaterial + 4;   // u32 slot @+16 (unchanged: already u32 stride)
        if (lParams.mabEnable[0])
            *lpuFlags |= 1u;       // alpha blend
        if (lParams.mabEnable[6])
            *lpuFlags |= 8u;       // alpha test

        // SEAM: shader pointer = u32 slot @+0 (was pMaterial[0], which read the u64 @+0);
        // the shader blob itself is likewise walked with u32 stride (slots 0/1/7/11/37).
        u32*   lpuShader   = PointerFromU32<u32>(lpuMaterial[0]);
        u32    luVtxShader = lpuShader[0];
        u32    luPixShader = lpuShader[1];
        if (luVtxShader
            && (ShaderConstantsExternal::HasShaderConstant(lpuShader + 7, "InstancingMatrixArray")
             || ShaderConstantsExternal::HasShaderConstant(lpuShader + 11, "InstancingMatrixArray")))
        {
            *lpuFlags |= 0x10u;    // hardware instancing
        }

        // SEAM: context hash source = u32 slot @+20 (was pMaterial[5] = byte +40 on x64).
        u32 luContextHash = lpuMaterial[5];
        lpuMaterial[7] = CgsContainers::CgsHash16::CalculateHash(&luContextHash, 4);
        lpuMaterial[5] = CgsContainers::CgsHash12::CalculateHash(&luVtxShader, 4);
        u32 luResult = CgsContainers::CgsHash12::CalculateHash(&luPixShader, 4);
        lpuMaterial[6] = luResult;
        lpuMaterial[8] = lpuShader[37] - 48;
    }
}
