#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MaterialTechniqueResourceType::FixDown          @ 0x828A8740
//   CgsResource::MaterialTechniqueResourceType::FixUp            @ 0x828A8770
//   CgsResource::MaterialTechniqueResourceType::GetImportPointer @ 0x828A80A8
//   CgsResource::MaterialTechniqueResourceType::GetTypeID        @ 0x828A7EC0
//   CgsResource::MaterialTechniqueResourceType::PostFixUp        @ 0x828A7EC8
//
// FixUp/FixDown rebase the three serialised pointers in the technique header (+24/+28/+36).
// PostFixUp performs runtime setup: it queries the material's blend state for the alpha-blend
// / alpha-test flags, sets the instancing flag when either shader-constant table declares an
// InstancingMatrixArray, and computes the technique's sort/lookup hashes. Foreign helpers
// (renderengine::BlendState, ShaderConstantsExternal, CgsHash) live in other TUs.

namespace renderengine
{
    struct BlendState
    {
        static void GetParameters(void* pBlendState, void* pQuery);
    };
}

namespace ShaderConstantsExternal
{
    bool HasShaderConstant(const void* pTable, const char* pacName);
}

namespace CgsContainers
{
    struct CgsHash16 { static u32 CalculateHash(const void* pData, int liLen); };
    struct CgsHash12 { static u32 CalculateHash(const void* pData, int liLen); };
}

namespace CgsDev
{
namespace Assert
{
    void  BeginAssert();
    void  FireAssert(const char* pacMessage, const char* pacFile, int liLine);
    void* EndAssert();
}
}

namespace CgsResource
{
namespace
{
    struct Material
    {
        u32 muField0;            // [0]
        u32 muField1;            // [1]
        u8  mPad0[20];           // [2..6]
        u8  maShaderConstantsA[16]; // [7..10] shader constant table A
        u8  maShaderConstantsB[104]; // [11..36] shader constant table B (+ rest of material)
        u32 muStateField;        // [37] pointer; runtime state value lives at *muStateField
    };

    struct MaterialState
    {
        void** mppBlendState;    // [0] -> mpBlendState
    };

    struct MaterialTechnique
    {
        Material*      mpMaterial;        // [0]
        MaterialState* mpMaterialState;   // [1]
        u32 muField2;                     // [2]
        u32 muField3;                     // [3]
        u32 muFlags;                      // [4]
        u32 muLookupHash;                 // [5]
        u32 muSortHash;                   // [6]
        u32 muStateHash;                  // [7]
        u32 muStateValue;                 // [8]
    };

    // Blend-state query descriptor (matches the on-stack buffer passed to GetParameters).
    struct BlendStateQuery
    {
        u32 maIn[12];
        u8  mbAlphaBlend;        // out (v13) -> flag 0x1
        u8  maPad[5];            // out (v14..v18)
        u8  mbAlphaTest;         // out (v19) -> flag 0x8
    };

    const u32 KU_FLAG_ALPHA_BLEND = 0x1;
    const u32 KU_FLAG_ALPHA_TEST  = 0x8;
    const u32 KU_FLAG_INSTANCING  = 0x10;
}

class MaterialTechniqueResourceType
{
public:
    void* FixDown(void* pResource, const int* pDelta);
    void* FixUp(void* pResource, const int* pDelta);
    void* GetImportPointer();
    int   GetTypeID() { return KI_TYPE_ID; }
    u32   PostFixUp(void* pResource, MaterialTechnique* pTechnique);

private:
    static const int KI_TYPE_ID = 13;
};

void* MaterialTechniqueResourceType::FixDown(void* pResource, const int* pDelta)
{
    u32* lpHeader = static_cast<u32*>(pResource);
    const u32 luDelta = static_cast<u32>(*pDelta);

    const u32 lu28 = lpHeader[7] - luDelta;   // +28
    const u32 lu36 = lpHeader[9] - luDelta;   // +36
    lpHeader[6] -= luDelta;                    // +24
    lpHeader[7] = lu28;
    lpHeader[9] = lu36;
    return pResource;
}

void* MaterialTechniqueResourceType::FixUp(void* pResource, const int* pDelta)
{
    u32* lpHeader = static_cast<u32*>(pResource);
    const u32 luDelta = static_cast<u32>(*pDelta);

    const u32 lu28 = lpHeader[7] + luDelta;
    const u32 lu36 = lpHeader[9] + luDelta;
    lpHeader[6] += luDelta;
    lpHeader[7] = lu28;
    lpHeader[9] = lu36;
    return pResource;
}

void* MaterialTechniqueResourceType::GetImportPointer()
{
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(
        "This function should not be called at runtime",
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\cross/CgsMaterialTechniqueResourceType.cpp",
        371);
    return CgsDev::Assert::EndAssert();
}

u32 MaterialTechniqueResourceType::PostFixUp(void* /*pResource*/, MaterialTechnique* pTechnique)
{
    if (!pTechnique->mpMaterialState)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpMaterial->mpMaterialState",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\cross/CgsMaterialTechniqueResourceType.cpp",
            266);
        CgsDev::Assert::EndAssert();
    }
    if (!*pTechnique->mpMaterialState->mppBlendState)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpMaterial->mpMaterialState->mpBlendState",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\cross/CgsMaterialTechniqueResourceType.cpp",
            267);
        CgsDev::Assert::EndAssert();
    }

    BlendStateQuery lQuery;
    lQuery.maIn[0] = 117835526;
    lQuery.maIn[1] = 117835526;
    lQuery.maIn[2] = 117835526;
    lQuery.maIn[3] = 117835526;
    lQuery.maIn[4] = 7;
    lQuery.maIn[5] = 15;
    lQuery.maIn[6] = 15;
    lQuery.maIn[7] = 15;
    lQuery.maIn[8] = 15;
    lQuery.maIn[9] = 135;
    lQuery.maIn[10] = 0;
    lQuery.maIn[11] = static_cast<u32>(-1);
    lQuery.mbAlphaBlend = 0;
    lQuery.maPad[0] = lQuery.maPad[1] = lQuery.maPad[2] = lQuery.maPad[3] = lQuery.maPad[4] = 0;
    lQuery.mbAlphaTest = 0;

    renderengine::BlendState::GetParameters(*pTechnique->mpMaterialState->mppBlendState, &lQuery);
    if (lQuery.mbAlphaBlend)
        pTechnique->muFlags |= KU_FLAG_ALPHA_BLEND;
    if (lQuery.mbAlphaTest)
        pTechnique->muFlags |= KU_FLAG_ALPHA_TEST;

    Material* lpMaterial = pTechnique->mpMaterial;
    const u32 luField0 = lpMaterial->muField0;
    const u32 luField1 = lpMaterial->muField1;
    if (luField0
        && (ShaderConstantsExternal::HasShaderConstant(lpMaterial->maShaderConstantsA, "InstancingMatrixArray")
            || ShaderConstantsExternal::HasShaderConstant(lpMaterial->maShaderConstantsB, "InstancingMatrixArray")))
    {
        pTechnique->muFlags |= KU_FLAG_INSTANCING;
    }

    u32 luPrevLookup = pTechnique->muLookupHash;
    u32 luStateHashIn = luPrevLookup;
    u32 luLookupIn    = luField0;
    u32 luSortIn      = luField1;
    pTechnique->muStateHash  = CgsContainers::CgsHash16::CalculateHash(&luStateHashIn, 4);
    pTechnique->muLookupHash = CgsContainers::CgsHash12::CalculateHash(&luLookupIn, 4);
    u32 luSortHash           = CgsContainers::CgsHash12::CalculateHash(&luSortIn, 4);
    pTechnique->muSortHash   = luSortHash;

    pTechnique->muStateValue = *reinterpret_cast<u32*>(static_cast<uintptr_t>(lpMaterial->muStateField)) - 48;
    return luSortHash;
}
}
