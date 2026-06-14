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
    static const uint32_t KU_MATERIAL_TECHNIQUE_RESOURCE_TYPE_ID = 13;

    uint32_t MaterialTechniqueResourceType::GetTypeID() const
    {
        return KU_MATERIAL_TECHNIQUE_RESOURCE_TYPE_ID;
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
        void** pMaterial = static_cast<void**>(lpResource);
        CGS_ASSERT(pMaterial[1] != nullptr, "lpMaterial->mpMaterialState");
        void** lpMaterialState = reinterpret_cast<void**>(pMaterial[1]);
        CGS_ASSERT(lpMaterialState[0] != nullptr, "lpMaterial->mpMaterialState->mpBlendState");

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

        renderengine::BlendState::GetParameters(lpMaterialState[0], &lParams);

        u32* lpuFlags = reinterpret_cast<u32*>(pMaterial) + 4;
        if (lParams.mabEnable[0])
            *lpuFlags |= 1u;       // alpha blend
        if (lParams.mabEnable[6])
            *lpuFlags |= 8u;       // alpha test

        void*  lpShader = pMaterial[0];
        u32    luVtxShader = reinterpret_cast<u32*>(lpShader)[0];
        u32    luPixShader = reinterpret_cast<u32*>(lpShader)[1];
        if (luVtxShader
            && (ShaderConstantsExternal::HasShaderConstant(reinterpret_cast<u32*>(lpShader) + 7, "InstancingMatrixArray")
             || ShaderConstantsExternal::HasShaderConstant(reinterpret_cast<u32*>(lpShader) + 11, "InstancingMatrixArray")))
        {
            *lpuFlags |= 0x10u;    // hardware instancing
        }

        u32 luContextHash = reinterpret_cast<u32>(pMaterial[5]);
        u32* lpuOut = reinterpret_cast<u32*>(pMaterial);
        lpuOut[7] = CgsContainers::CgsHash16::CalculateHash(&luContextHash, 4);
        lpuOut[5] = CgsContainers::CgsHash12::CalculateHash(&luVtxShader, 4);
        u32 luResult = CgsContainers::CgsHash12::CalculateHash(&luPixShader, 4);
        lpuOut[6] = luResult;
        lpuOut[8] = reinterpret_cast<u32*>(lpShader)[37] - 48;
    }
}
