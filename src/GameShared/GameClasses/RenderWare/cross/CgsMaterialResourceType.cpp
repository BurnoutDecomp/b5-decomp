#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialResourceType.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MaterialResourceType::FixUp  @ 0x828A8280
//
// FixUp relocates a streamed-in material (a serialised CgsGraphics::MaterialAssembly
// image): it rebases the vertex/pixel internal shader-constant sub-blocks and the CPU
// (material-animation) sub-block, re-runs each sub-block's own relocation, rebases the
// sampler array and each sampler's leading name pointer, and resolves each sampler's
// type index by name against the engine's sampler-type name table.
//
// PostFixUp (@0x828A83B8) and PostFixUpShaderConstants (@0x828A77E8) are declared in the
// header but bodied by their own TUs; they are intentionally not defined here.

namespace CgsResource
{
    // FixUp's sampler-type name table (X360 unk_83011A78, 7 entries / 28-byte stride; only
    // the leading char* of each slot is dereferenced). The strings are NOT present in the
    // repo rodata exports, so the CONTENTS are un-attested: placeholder empty strings are
    // used, which makes every real sampler resolve to -1. The lookup loop is faithful.
    const char* const KapcMaterialSamplerTypeNames[KU_MATERIAL_SAMPLER_TYPE_COUNT] =
    {
        "", "", "", "", "", "", ""
    };

// --- FixUp @ 0x828A8280 -----------------------------------------------------------
    // Relocate a streamed-in material. Delta = the rw::Resource's load base (asm reads *a3,
    // the first word of the resource; GetLoadBase == m_baseResources[0], same word). The
    // serialised blob IS a CgsGraphics::MaterialAssembly: word 0 the technique table, +0x0C
    // the sampler array, +0x10/+0x14/+0x18 the vertex/pixel/CPU shader-constant sub-blocks.
    // Each pointer is rebased by the delta; the two internal and one CPU sub-block then re-run
    // their own relocation FixUp(u8* base). Finally each 20-byte sampler's leading name ptr is
    // rebased and its type index (+12) is resolved by name against the sampler-type name table.
    //
    // Raw-offset access on the serialised blob is the documented exception.
    void MaterialResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        u8* const lpBlob  = static_cast<u8*>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);
        u8* const lpBase  = reinterpret_cast<u8*>(static_cast<uintptr_t>(luDelta));

        // Rebase + relocate the vertex-stage internal constant block (+0x10), then the table (+0x00).
        u32 luVtx = *reinterpret_cast<u32*>(lpBlob + 0x10) + luDelta;
        *reinterpret_cast<u32*>(lpBlob + 0x10) = luVtx;
        *reinterpret_cast<u32*>(lpBlob + 0x00) += luDelta;
        reinterpret_cast<ShaderConstantsInternal*>(luVtx)->FixUp(lpBase);

        // Rebase + relocate the pixel-stage internal constant block (+0x14).
        u32 luPix = *reinterpret_cast<u32*>(lpBlob + 0x14) + luDelta;
        *reinterpret_cast<u32*>(lpBlob + 0x14) = luPix;
        reinterpret_cast<ShaderConstantsInternal*>(luPix)->FixUp(lpBase);

        // Rebase + relocate the CPU (material-animation) constant block (+0x18), if present.
        u32 luCpu = *reinterpret_cast<u32*>(lpBlob + 0x18);
        if (luCpu)
        {
            luCpu += luDelta;
            *reinterpret_cast<u32*>(lpBlob + 0x18) = luCpu;
            reinterpret_cast<ShaderConstantsCPU*>(luCpu)->FixUp(lpBase);
        }

        // Rebase the sampler array (+0x0C); mi8NumSamplers is a signed byte @+0x09.
        const s32 liNumSamplers = static_cast<s32>(*reinterpret_cast<s8*>(lpBlob + 0x09));
        *reinterpret_cast<u32*>(lpBlob + 0x0C) += luDelta;
        if (liNumSamplers > 0)
        {
            u32 luSampler = 0;
            do
            {
                // Sampler stride 20 bytes; leading word is a name pointer, rebased.
                u8* const lpSamplers = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpBlob + 0x0C));
                u8* const lpSampler  = lpSamplers + 20 * luSampler;
                *reinterpret_cast<u32*>(lpSampler) += luDelta;

                // Resolve the sampler's type index by matching its name against the engine's
                // sampler-type name table (7 entries; on no match -> -1).
                const char* const lpName = *reinterpret_cast<const char* const*>(lpSampler);
                s32 liType = -1;
                for (u32 luEntry = 0; luEntry < KU_MATERIAL_SAMPLER_TYPE_COUNT; ++luEntry)
                {
                    const char* lpCandidate = KapcMaterialSamplerTypeNames[luEntry];
                    const char* lpScan      = lpName;
                    while (*lpCandidate == *lpScan)
                    {
                        if (*lpCandidate == '\0')
                        {
                            liType = static_cast<s32>(luEntry);
                            break;
                        }
                        ++lpCandidate;
                        ++lpScan;
                    }
                    if (liType >= 0)
                        break;
                }
                *reinterpret_cast<s32*>(lpSampler + 12) = liType;

                ++luSampler;
            }
            while (static_cast<s32>(luSampler) < static_cast<s32>(*reinterpret_cast<s8*>(lpBlob + 0x09)));
        }
    }
}
