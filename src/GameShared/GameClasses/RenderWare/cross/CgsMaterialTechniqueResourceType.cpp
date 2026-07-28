#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialTechniqueResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the unresolved-import boot gate
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h" // renderengine::BlendState::GetParameters
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"          // ShaderConstantsExternal::HasShaderConstant

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

// The four helpers PostFixUp calls all have REAL committed homes; this TU used to carry
// local `__debugbreak()` placeholders for them because nothing ever reached PostFixUp
// while SHADERS.BNDL was unstaged (every technique's shader import came back null and the
// boot gate below returned early). With the converted SHADERS.BNDL staged and
// BrnGameModule::GamePrepare really loading it, PostFixUp runs for real -- so the
// placeholders are gone and the real bodies are used:
//   renderengine::BlendState::GetParameters               @0x82B60A50
//       SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.cpp
//   ::ShaderConstantsExternal::HasShaderConstant
//       GameShared/GameClasses/Graphics/CgsShaderConstants.cpp (serialised-block view --
//       exactly what this call site hands it: a block inside a streamed technique blob)
//   CgsContainers::CgsHash16::CalculateHash / CgsHash12::CalculateHash
//       GameShared/GameClasses/Containers/CgsHash16.cpp / CgsHash12.cpp (no header exists
//       for either; declared here against their definitions, as their other callers do).
namespace CgsContainers
{
    // Return types differ between the two homes (CgsHash16.cpp returns int, CgsHash12.cpp
    // returns unsigned int); both are the same 32-bit CRC accumulator.
    namespace CgsHash16 { int CalculateHash(char* a1, int a2); }
    namespace CgsHash12 { unsigned int CalculateHash(char* a1, int a2); }
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
        // ============================================================================
        // SERIALISED MATERIAL-TECHNIQUE BLOB -- the field map is ASM-ATTESTED (0x828A7EC8).
        //
        // Hex-Rays prints this function's `a2` as `int **`, so its `*(a2 + 4/5/6/7/8)`
        // read as byte offsets 16/20/24/28/32. THE ASM SAYS OTHERWISE: every one of those
        // accesses is a HALFWORD at half that offset --
        //     lhz/sth  8(r31)   flags        (|= 1 alpha blend, |= 8 alpha test, |= 0x10 instancing)
        //     sth      0xA(r31) CgsHash12(vertex program slot)
        //     sth      0xC(r31) CgsHash12(pixel program slot)
        //     sth      0xE(r31) CgsHash16(the u32 @ +0x14)
        //     sth     0x10(r31) shader-profile digit
        //     lwz     0x14(r31) the CgsHash16 source word
        // and only mpShader/mpMaterialState (lwz 0/4) and the hash source are 32-bit.
        //
        // The doubled mapping was ACTIVELY DESTRUCTIVE: byte +24 and +28 are the technique's
        // two per-stage BINDING LIST pointers (relocated by FixUp @+24/+28/+36, consumed by
        // MaterialResourceType::PostFixUpShaderConstants @0x828A77E8 as `*(v7+24)` /
        // `*(v7+28)`), so writing the two CgsHash12 results there overwrote both with
        // 16-bit hashes -- the observed crash was a store through `lpTech+0x18 == 0x1F4`.
        // Bug class (c): a field read/written at the wrong byte offset.
        // ============================================================================
        struct SerialisedMaterialTechnique
        {
            u32 muShader;              // +0x00  ShaderTechnique*   (import)
            u32 muMaterialState;       // +0x04  MaterialState*     (import)
            u16 mu16Flags;             // +0x08
            u16 mu16VertexShaderHash;  // +0x0A
            u16 mu16PixelShaderHash;   // +0x0C
            u16 mu16MaterialStateHash; // +0x0E
            u16 mu16ShaderProfile;     // +0x10
            u16 mu16Pad12;             // +0x12
            u32 muHashSource;          // +0x14
            u32 muVertexBindingList;   // +0x18  relocated by FixUp
            u32 muPixelBindingList;    // +0x1C  relocated by FixUp
            u8  mu8VertexConstantCount;// +0x20  written by MaterialResourceType::PostFixUpShaderConstants
            u8  mu8PixelConstantCount; // +0x21
            u16 mu16Pad22;             // +0x22
            u32 muSamplerBindingList;  // +0x24  relocated by FixUp (the third slot, X360 +36)
        };

        SerialisedMaterialTechnique* const lpTechnique =
            static_cast<SerialisedMaterialTechnique*>(lpResource);

        // [FLAG PC boot gate] mpShader and mpMaterialState are cross-bundle IMPORTS out of
        // SHADERS.BNDL. That bundle IS staged now, so this normally resolves; the gate stays
        // for the world bundles whose technique imports still come back null (the console
        // never sees a null here and would dereference it). DELETE when every world bundle
        // resolves its shader imports.
        if (lpTechnique->muShader == 0 || lpTechnique->muMaterialState == 0)
        {
            static bool sbLoggedUnresolved = false;
            if (!sbLoggedUnresolved && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                sbLoggedUnresolved = true;
                *CgsDev::Log::gpDebugPrint
                    << "MaterialTechniqueResourceType::PostFixUp: shader/material-state import"
                       " unresolved -- technique left as serialised [FLAG PC boot gate]\n";
            }
            return;
        }

        CGS_ASSERT(lpTechnique->muMaterialState != 0, "lpMaterial->mpMaterialState");
        const u32* lpuMaterialState = PointerFromU32<const u32>(lpTechnique->muMaterialState);
        CGS_ASSERT(lpuMaterialState[0] != 0, "lpMaterial->mpMaterialState->mpBlendState"); // u32 slot @+0
        if (lpuMaterialState[0] == 0)
        {
            return;   // same gate: the blend state never arrived
        }

        // The X360 seeds the 56-byte stack parameter block with the engine's blend defaults
        // (v12[0..11] + the seven trailing flag bytes) before calling GetParameters. Every
        // one of those fields is overwritten by GetParameters, so the seed only matters for
        // faithfulness; reproduced verbatim in the X360's store order.
        renderengine::BlendStateParameters lParams;
        lParams.maBlendFactor[0] = 117835526u;   // v12[0]
        lParams.maBlendFactor[1] = 117835526u;   // v12[1]
        lParams.maBlendFactor[2] = 117835526u;   // v12[2]
        lParams.maBlendFactor[3] = 117835526u;   // v12[3]
        lParams.muState4  = 15u;                 // v12[5]
        lParams.muState5  = 15u;                 // v12[6]
        lParams.muState6  = 15u;                 // v12[7]
        lParams.muState7  = 15u;                 // v12[8]
        lParams.muState15 = 7u;                  // v12[4]
        lParams.muState8  = 135u;                // v12[9]
        lParams.muState9  = static_cast<u32>(-1);// v12[11]
        lParams.mbHasCustomBlendFactors = 0;     // v13
        lParams.mbState10 = 0;                   // v14
        lParams.mbState11 = 0;                   // v15
        lParams.mbState12 = 0;                   // v16
        lParams.mbState13 = 0;                   // v17
        lParams.mbState14 = 0;                   // v18
        lParams.muState17 = 0u;                  // v12[10]
        lParams.mbState16 = 0;                   // v19

        renderengine::BlendState::GetParameters(
            PointerFromU32<const renderengine::BlendMaterialState>(lpuMaterialState[0]), &lParams);

        // X360 v13 == params byte +48, v19 == params byte +54 (the first and last of the
        // seven trailing flag bytes) -- BlendStateParameters' mbHasCustomBlendFactors and
        // mbState16 by the committed names. The flag word is the HALFWORD @+8
        // (lhz/ori/sth 8(r31)).
        if (lParams.mbHasCustomBlendFactors)
            lpTechnique->mu16Flags |= 1u;       // alpha blend
        if (lParams.mbState16)
            lpTechnique->mu16Flags |= 8u;       // alpha test

        // The shader technique blob is walked with u32 stride (slots 0/1/7/11/37 == bytes
        // 0/4/0x1C/0x2C/0x94 -- the asm's `addi r3, r11, 0x1C` / `0x2C` / `lwz 0x94(r11)`).
        u32*      lpuShader   = PointerFromU32<u32>(lpTechnique->muShader);
        const u32 luVtxShader = lpuShader[0];
        const u32 luPixShader = lpuShader[1];
        // Bytes +0x1C and +0x2C are the technique's two vertex-stage ShaderConstantsExternal
        // sub-blocks -- an interior VIEW of the streamed blob, not a stored pointer, so this
        // is a placement cast (HasShaderConstant walks the serialised word layout, matching
        // this blob).
        const ::ShaderConstantsExternal* const lpObjectVsBlock =
            reinterpret_cast<const ::ShaderConstantsExternal*>(lpuShader + 7);
        const ::ShaderConstantsExternal* const lpGlobalVsBlock =
            reinterpret_cast<const ::ShaderConstantsExternal*>(lpuShader + 11);
        if (luVtxShader
            && (lpObjectVsBlock->HasShaderConstant("InstancingMatrixArray")
             || lpGlobalVsBlock->HasShaderConstant("InstancingMatrixArray")))
        {
            lpTechnique->mu16Flags |= 0x10u;    // hardware instancing
        }

        // The three cached hashes, in the X360's call order (lwz 0x14 -> CgsHash16 -> sth 0xE,
        // CgsHash12(vertex) -> sth 0xA, CgsHash12(pixel) -> sth 0xC). All three results are
        // stored as HALFWORDS.
        u32 luHashSource = lpTechnique->muHashSource;
        u32 luVtxSource  = luVtxShader;
        u32 luPixSource  = luPixShader;
        lpTechnique->mu16MaterialStateHash = static_cast<u16>(CgsContainers::CgsHash16::CalculateHash(
            reinterpret_cast<char*>(&luHashSource), 4));
        lpTechnique->mu16VertexShaderHash = static_cast<u16>(CgsContainers::CgsHash12::CalculateHash(
            reinterpret_cast<char*>(&luVtxSource), 4));
        lpTechnique->mu16PixelShaderHash = static_cast<u16>(CgsContainers::CgsHash12::CalculateHash(
            reinterpret_cast<char*>(&luPixSource), 4));

        // X360 tail: `lwz r11,0(r31); lwz r11,0x94(r11); lbz r11,0(r11); extsb; addis +1;
        // addi -0x30; sth r11,0x10(r31)` -- load the shader technique's NAME pointer
        // (blob byte +0x94 == +148), DEREFERENCE its first byte, sign-extend, subtract 48.
        // (The addis +0x10000 is discarded by the halfword store.) The stored value is the
        // technique's shader-PROFILE digit, which ShaderTechniqueResourceType::PostFixUp
        // @0x827EEBF0 stamps into that first byte during its strstr classification.
        // [FLAG PC boot gate] that classification pass is still inert (see the WorldLinkStubs
        // gate), so this reads the un-stamped first character of the technique name. Correct
        // by construction the moment ShaderTechnique PostFixUp lands; no consumer of the
        // profile field is reconstructed yet.
        const char* const lpcTechniqueName = PointerFromU32<const char>(lpuShader[37]);
        lpTechnique->mu16ShaderProfile = lpcTechniqueName
            ? static_cast<u16>(static_cast<s8>(*lpcTechniqueName) - 48)
            : static_cast<u16>(0);
    }
}
