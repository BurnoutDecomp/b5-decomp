#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialResourceType.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"          // MaterialAssembly::FixupAnimatedMaterial
#include "GameShared/GameClasses/Graphics/CgsShaderConstantHashTable.h"   // ShaderConstantHashTable::GetName
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h" // ProgramBuffer::GetVariableHandleByName
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the unresolved-technique boot gate
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
    // E_RESOURCETYPE_MATERIAL (CgsResourceTypeIds.h); see the header note.
    static const uint32_t KU_MATERIAL_RESOURCE_TYPE_ID = 0x1;

    uint32_t MaterialResourceType::GetTypeID() const
    {
        return KU_MATERIAL_RESOURCE_TYPE_ID;
    }

    // FixUp's sampler-type name table (X360 unk_83011A78, 7 entries / 28-byte stride; only
    // the leading char* of each slot is dereferenced). The strings are NOT present in the
    // repo rodata exports, so the CONTENTS are un-attested: placeholder empty strings are
    // used, which makes every real sampler resolve to -1. The lookup loop is faithful.
    const char* const KapcMaterialSamplerTypeNames[KU_MATERIAL_SAMPLER_TYPE_COUNT] =
    {
        "", "", "", "", "", "", ""
    };

    // [FLAG PC boot gate] A material's per-technique program buffer is an IMPORT: it lives in
    // SHADERS.BNDL, which this build does not stage yet (every technique is routed through the
    // renderer's fallback shader instead). CgsResource::Pool::ResolveImportForEntry therefore
    // writes a NULL into the technique slot, and the console-faithful PostFixUp walk -- which
    // never sees a null on the real hardware -- dereferences it. Report once and skip the
    // affected technique so the rest of the world data still fixes up. DELETE this gate when
    // SHADERS.BNDL is staged.
    namespace
    {
        void LogUnresolvedTechniqueOnce()
        {
            static bool sbLogged = false;
            if (!sbLogged && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                sbLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "MaterialResourceType::PostFixUp: technique/program import unresolved"
                       " (SHADERS.BNDL not staged) -- skipping the binding fix-up"
                       " [FLAG PC boot gate]\n";
            }
        }
    }

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
                // SERIALISED SLOT: the sampler's name is a 32-BIT slot (the whole material
                // blob is the console 32-bit layout -- every other access in this function
                // is a u32 poke). Reading it as a host pointer splices the following word in
                // and faults; low-4GB PointerFromU32 is the project convention here.
                const char* const lpName = reinterpret_cast<const char*>(
                    static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpSampler)));
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
                *reinterpret_cast<s32*>(lpSampler + 12) = liType;   // serialised blob

                ++luSampler;
            }
            while (static_cast<s32>(luSampler) < static_cast<s32>(*reinterpret_cast<s8*>(lpBlob + 0x09)));
        }
    }

// --- PostFixUpShaderConstants @ 0x828A77E8 --------------------------------------------
    // For each technique in the material, resolve one shader stage's (vertex or pixel)
    // named constants against that technique's program buffer and record, per constant, the
    // register handle plus the index of the matching entry in the material's ShaderConstants
    // block. The DWARF-attested ShaderConstantsInternal* argument is unused: the per-stage
    // block is re-derived from the blob (+0x10 vertex / +0x14 pixel). Raw-offset access on
    // the serialised blob / program / technique structures is the documented exception.
    //
    // Program block (technique+0x00): +0x00 vertex ProgramBufferData* / +0x04 pixel,
    // +0x08 vertex constant count / +0x3C pixel, +0x14 vertex constant-id array / +0x48 pixel,
    // +0x80 ShaderConstantHashTable. Technique: +0x18 vertex binding list / +0x1C pixel,
    // +0x20 vertex count byte / +0x21 pixel. Each 4-byte binding entry: [0..1] register-set
    // (u16-widened byte), [2] block index, [3] register count.
    void MaterialResourceType::PostFixUpShaderConstants(void* lpResource,
                                                        ShaderConstantsInternal* /*lpConstants*/,
                                                        bool lbPixelStage) const
    {
        u8* const lpBlob = static_cast<u8*>(lpResource);
        const u32 luNumMaterials = *reinterpret_cast<u8*>(lpBlob + 0x08);
        if (luNumMaterials == 0)
            return;

        // Stage-selected offsets (vertex when !lbPixelStage, pixel otherwise).
        const u32 luCountFieldOff   = lbPixelStage ? 0x3C : 0x08;  // program: per-stage constant count byte
        const u32 luDataFieldOff    = lbPixelStage ? 0x04 : 0x00;  // program: per-stage ProgramBufferData*
        const u32 luIdArrayFieldOff = lbPixelStage ? 0x48 : 0x14;  // program: per-stage constant-id array ptr
        const u32 luTechCountOff    = lbPixelStage ? 0x21 : 0x20;  // technique: per-stage count byte
        const u32 luTechListOff     = lbPixelStage ? 0x1C : 0x18;  // technique: per-stage binding list ptr
        const u32 luBlockOff        = lbPixelStage ? 0x14 : 0x10;  // blob: per-stage ShaderConstants block

        for (u32 luMat = 0; luMat < luNumMaterials; ++luMat)
        {
            // SERIALISED SLOT: the technique-pointer array is 32-bit slots (see FixUp).
            u8* const lpTech    = reinterpret_cast<u8*>(static_cast<uintptr_t>(
                reinterpret_cast<const u32*>(static_cast<uintptr_t>(*reinterpret_cast<u32*>(lpBlob + 0x00)))[luMat]));
            if (lpTech == 0)
            {
                LogUnresolvedTechniqueOnce();
                continue;
            }
            u8* const lpProgram = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpTech + 0x00));   // serialised blob
            if (lpProgram == 0)
            {
                LogUnresolvedTechniqueOnce();
                continue;
            }

            // Mirror the program's per-stage constant count into the technique and read the
            // per-stage ProgramBufferData* used for the register lookup.
            const u8 lu8Count = static_cast<u8>(*reinterpret_cast<u32*>(lpProgram + luCountFieldOff));
            *reinterpret_cast<u8*>(lpTech + luTechCountOff) = lu8Count;
            const renderengine::ProgramBufferData* const lpData =
                reinterpret_cast<const renderengine::ProgramBufferData*>(
                    *reinterpret_cast<u32*>(lpProgram + luDataFieldOff));

            const u32 luCount = lu8Count;
            if (luCount == 0)
                continue;

            const CgsGraphics::ShaderConstantHashTable* const lpHashTable =
                reinterpret_cast<const CgsGraphics::ShaderConstantHashTable*>(lpProgram + 0x80);

            for (u32 luConst = 0; luConst < luCount; ++luConst)
            {
                const u32 luOffset = luConst * 4;
                const u32 luConstantId = *reinterpret_cast<u32*>(
                    reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpProgram + luIdArrayFieldOff)) + luOffset);

                const char* const lpcName = lpHashTable->GetName(luConstantId);
                CGS_ASSERT(lpcName != nullptr, "lpcConstantName");

                renderengine::ProgramVariableHandle lHandle;
                if (renderengine::ProgramBuffer::GetVariableHandleByName(
                        lpData, reinterpret_cast<const u8*>(lpcName), &lHandle))
                {
                    // Locate the constant in the material's ShaderConstants block (parallel id array
                    // at block+0x0C, count at block+0x00) and stash the found index at binding+2.
                    u8* const lpBlock = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpBlob + luBlockOff));
                    const u32 luBlockCount = *reinterpret_cast<u32*>(lpBlock);

                    u8 lu8Index = 0;
                    if (luBlockCount != 0)
                    {
                        const u32* const lpuIds =
                            reinterpret_cast<u32*>(*reinterpret_cast<u32*>(lpBlock + 0x0C));   // serialised blob
                        u32  luEntry = 0;
                        bool lbFound = false;
                        while (true)
                        {
                            if (lpuIds[luEntry] == luConstantId)
                            {
                                lbFound = true;
                                break;
                            }
                            lu8Index = static_cast<u8>(luEntry + 1);
                            ++luEntry;
                            if (luEntry >= luBlockCount)
                                break;
                        }
                        if (lbFound)
                        {
                            u8* const lpList = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpTech + luTechListOff));
                            *reinterpret_cast<u8*>(lpList + luOffset + 2) = lu8Index;
                        }
                    }

                    CGS_ASSERT(lu8Index < luBlockCount,
                               "Tyring to postfixup a constant not present in the programbuffer");

                    u8* const lpList = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpTech + luTechListOff));
                    *reinterpret_cast<u8*>(lpList + luOffset + 3)  = lHandle.mu8RegisterCount;
                    *reinterpret_cast<u16*>(lpList + luOffset)     = lHandle.mu8RegisterSet;
                }
                else
                {
                    CGS_ASSERT(false, "Tyring to postfixup a constant not present in the programbuffer");
                }
            }
        }
    }

// --- PostFixUp @ 0x828A83B8 -----------------------------------------------------------
    // Second-stage material fix-up (runs after FixUp). Resolves the vertex- and pixel-stage
    // shader-constant bindings (PostFixUpShaderConstants), rebuilds the animated-material
    // binding tables (MaterialAssembly::FixupAnimatedMaterial), then for every technique walks
    // the sampler list building the per-technique sampler-binding table: for each sampler whose
    // 16-bit id (sampler+0x08) matches a program constant-table entry (8-byte stride, id @+0x04),
    // append the sampler index to the technique's binding list (technique+0x24) at the running
    // count (technique+0x22), and bump the external-sampler count (technique+0x23) when the
    // sampler is flagged external (sampler+0x0A != 0). Finally fold the external count out of the
    // total. The rw::Resource argument is unused by this override.
    void MaterialResourceType::PostFixUp(void* lpResource, const rw::Resource& /*lrResource*/) const
    {
        u8* const lpBlob = static_cast<u8*>(lpResource);

        PostFixUpShaderConstants(lpResource,
            reinterpret_cast<ShaderConstantsInternal*>(*reinterpret_cast<u32*>(lpBlob + 0x10)), false);
        PostFixUpShaderConstants(lpResource,
            reinterpret_cast<ShaderConstantsInternal*>(*reinterpret_cast<u32*>(lpBlob + 0x14)), true);

        reinterpret_cast<CgsGraphics::MaterialAssembly*>(lpBlob)->FixupAnimatedMaterial();

        const u32 luNumMaterials = *reinterpret_cast<u8*>(lpBlob + 0x08);
        if (luNumMaterials == 0)
            return;

        for (u32 luMat = 0; luMat < luNumMaterials; ++luMat)
        {
            // SERIALISED SLOT: the technique-pointer array is 32-bit slots (see FixUp).
            u8* const lpTech = reinterpret_cast<u8*>(static_cast<uintptr_t>(
                reinterpret_cast<const u32*>(static_cast<uintptr_t>(*reinterpret_cast<u32*>(lpBlob + 0x00)))[luMat]));
            if (lpTech == 0)
            {
                LogUnresolvedTechniqueOnce();
                continue;
            }
            *reinterpret_cast<u8*>(lpTech + 0x22) = 0;   // sampler-binding total (serialised blob)
            *reinterpret_cast<u8*>(lpTech + 0x23) = 0;   // external-sampler count (serialised blob)

            const s32 liNumSamplers = *reinterpret_cast<s8*>(lpBlob + 0x09);
            for (s32 liSampler = 0; liSampler < liNumSamplers; ++liSampler)
            {
                u8* const lpProgram   = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpTech + 0x00));   // serialised blob
                if (lpProgram == 0)
                {
                    LogUnresolvedTechniqueOnce();
                    break;
                }
                const s32 liNumEntries = *reinterpret_cast<s8*>(lpProgram + 0x90);   // serialised blob
                if (liNumEntries > 0)
                {
                    u8* const lpEntries = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpProgram + 0x8C));   // serialised blob
                    u8* const lpSampler = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpBlob + 0x0C)) + 20 * liSampler;
                    const s16 li16SamplerId = *reinterpret_cast<s16*>(lpSampler + 0x08);   // serialised blob

                    s32  liEntry = 0;
                    bool lbMatch = false;
                    while (true)
                    {
                        if (*reinterpret_cast<s16*>(lpEntries + 8 * liEntry + 4) == li16SamplerId)   // serialised blob
                        {
                            lbMatch = true;
                            break;
                        }
                        ++liEntry;
                        if (liEntry >= liNumEntries)
                            break;
                    }

                    if (lbMatch)
                    {
                        const s8  li8Count = *reinterpret_cast<s8*>(lpTech + 0x22);   // serialised blob
                        u8* const lpList   = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpTech + 0x24));   // serialised blob
                        lpList[li8Count]   = static_cast<u8>(liSampler);
                        ++*reinterpret_cast<u8*>(lpTech + 0x22);   // serialised blob
                        if (*reinterpret_cast<u16*>(lpSampler + 0x0A) != 0)   // serialised blob
                            ++*reinterpret_cast<u8*>(lpTech + 0x23);   // serialised blob
                    }
                }
            }

            *reinterpret_cast<u8*>(lpTech + 0x22) =   // serialised blob
                static_cast<u8>(*reinterpret_cast<u8*>(lpTech + 0x22) - *reinterpret_cast<u8*>(lpTech + 0x23));   // serialised blob
        }
    }
}
