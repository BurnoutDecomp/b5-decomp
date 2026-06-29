#include "GameShared/GameClasses/Graphics/Resources/CgsShaderTechniqueResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"          // ShaderConstantsInternal/External::FixUp
#include "GameShared/GameClasses/Graphics/CgsShaderConstantHashTable.h"  // CgsGraphics::ShaderConstantHashTable::FixUp
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // CgsResource::GetLoadBase
#include "rw/rwcore_structs.h"   // rw::Resource complete for the FixUp body

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ShaderTechniqueResourceType::GetTypeID        @ 0x827E9C28
//   CgsResource::ShaderTechniqueResourceType::GetImportPointer @ 0x827E9D00
//   CgsResource::ShaderTechniqueResourceType::FixUp            @ 0x827EEB30
//   CgsResource::ShaderTechniqueResourceType::PostFixUp        @ 0x827EEBF0  (DEFERRED - see note at end)
//
// GetTypeID returns the shader-technique registry id (50). GetImportPointer publishes the
// two embedded shader-program imports (vertex @ offset 0, pixel @ offset 4) for the
// resource loader's import-pointer table. FixUp re-runs every nested constant block's
// relocation FixUp and rebases the technique's sampler table, all by the rw::Resource's
// load base.

namespace CgsResource
{
    // Shader-technique handler's registry id. X360 0x827E9C28 returns the literal 50.
    static const uint32_t KU_SHADER_TECHNIQUE_RESOURCE_TYPE_ID = 50;

    // --- GetTypeID @ 0x827E9C28 -------------------------------------------------------
    // Trivial constant return (X360: `li r3, 50; blr`).
    uint32_t ShaderTechniqueResourceType::GetTypeID() const
    {
        return KU_SHADER_TECHNIQUE_RESOURCE_TYPE_ID;
    }

    // --- GetImportPointer @ 0x827E9D00 ------------------------------------------------
    // The serialised shader-technique blob begins with two embedded shader-program
    // pointers: the vertex shader at word 0 (byte offset 0) and the pixel shader at word 1
    // (byte offset 4). The loader walks GetImportCount(==2) and asks for each import's
    // (offset, value) pair so it can register / relocate them. Index 0 -> {offset 0,
    // value = *(blob+0)}; index 1 -> {offset 4, value = *(blob+4)}. Any other index is
    // ignored (the X360 falls through with the out-params untouched).
    //
    // Raw-offset access on lpResource is the DOCUMENTED exception: this is the serialised
    // rw resource blob (a flat streamed image), not a C++ object with a recovered layout.
    void ShaderTechniqueResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                                       uint32_t* lpuOffset, const void** lppValue) const
    {
        const uint32_t* lpuBlob = static_cast<const uint32_t*>(lpResource);
        if (luIndex == 0)
        {
            *lpuOffset = 0;                                                  // vertex shader at byte offset 0
            *lppValue  = reinterpret_cast<const void*>(lpuBlob[0]);
        }
        else if (luIndex == 1)
        {
            *lpuOffset = 4;                                                  // pixel shader at byte offset 4
            *lppValue  = reinterpret_cast<const void*>(lpuBlob[1]);
        }
    }

    // --- FixUp @ 0x827EEB30 -----------------------------------------------------------
    // Relocate the streamed-in shader technique. The relocation delta is the rw::Resource's
    // load base (m_baseResources[0]; the X360 reads it as `*a3`, the first word of the
    // resource passed by reference). The blob embeds six shader-constant sub-blocks plus the
    // constant hash table at fixed byte offsets, each of which carries file-relative pointers
    // that its own FixUp turns back into absolute pointers:
    //
    //   +8   ShaderConstantsInternal  (engine-internal, hash-keyed; vertex stage)
    //   +28  ShaderConstantsExternal  (material, name-keyed)
    //   +44  ShaderConstantsExternal
    //   +60  ShaderConstantsInternal  (pixel stage)
    //   +80  ShaderConstantsExternal
    //   +96  ShaderConstantsExternal
    //   +128 CgsGraphics::ShaderConstantHashTable
    //
    // The committed relocation overloads are `this`-methods that take the load base as their
    // argument (ShaderConstantsInternal/External::FixUp(u8*) and
    // ShaderConstantHashTable::FixUp(u8*)). The X360 calls them as
    // `Internal::FixUp(blob+8, *a3)` etc. -- i.e. `this = blob+offset`, base = *a3 -- so each
    // call here is a placement view of the sub-block at its offset, then ->FixUp(loadBase).
    // (The X360 pseudocode shows the base as un-named register temporaries v4/v5 on some
    // calls because Hex-Rays lost the live-range name; it is the same load-base delta on
    // every call, confirmed by the hash-array rebase below adding that same delta.)
    //
    // After the sub-block FixUps, the technique's sampler table is rebased: word @ +144 is the
    // sampler count, word @ +140 is the sampler-array pointer (rebased by the load base), and
    // each 8-byte sampler entry's leading word is itself a pointer that is rebased; finally
    // word @ +148 (the sampler-state-block pointer) is rebased.
    //
    // Raw-offset access on lpResource is the DOCUMENTED exception (serialised rw blob).
    void ShaderTechniqueResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        u8* const lpBlob   = static_cast<u8*>(lpResource);
        const u32 luDelta  = CgsResource::GetLoadBase(lrResource);
        u8* const lpBase   = reinterpret_cast<u8*>(static_cast<uintptr_t>(luDelta));

        // Relocate the six embedded shader-constant sub-blocks (placement views into the blob).
        reinterpret_cast<ShaderConstantsInternal*>(lpBlob + 8)->FixUp(lpBase);
        reinterpret_cast<ShaderConstantsExternal*>(lpBlob + 28)->FixUp(lpBase);
        reinterpret_cast<ShaderConstantsExternal*>(lpBlob + 44)->FixUp(lpBase);
        reinterpret_cast<ShaderConstantsInternal*>(lpBlob + 60)->FixUp(lpBase);
        reinterpret_cast<ShaderConstantsExternal*>(lpBlob + 80)->FixUp(lpBase);
        reinterpret_cast<ShaderConstantsExternal*>(lpBlob + 96)->FixUp(lpBase);

        // Relocate the constant hash table.
        reinterpret_cast<CgsGraphics::ShaderConstantHashTable*>(lpBlob + 128)->FixUp(lpBase);

        // Rebase the sampler table: pointer @ +140, count @ +144, each entry's leading
        // pointer (8-byte stride), then the sampler-state-block pointer @ +148.
        u32 luSamplerCount = *reinterpret_cast<u32*>(lpBlob + 144);
        *reinterpret_cast<u32*>(lpBlob + 140) += luDelta;
        if (static_cast<s32>(luSamplerCount) > 0)
        {
            u8* lpSamplers = reinterpret_cast<u8*>(*reinterpret_cast<u32*>(lpBlob + 140));
            for (u32 luI = 0; luI < luSamplerCount; ++luI)
            {
                *reinterpret_cast<u32*>(lpSamplers + 8 * luI) += luDelta;
            }
        }
        *reinterpret_cast<u32*>(lpBlob + 148) += luDelta;
    }

    // --- PostFixUp @ 0x827EEBF0  (DEFERRED - declared only) ---------------------------
    // Intentionally NOT bodied here. PostFixUp is a ~150-line shader-profile-classification
    // routine: it strstr-scans the technique name (blob word 37) against an 8-entry rodata
    // string table (off_82F30F78, the shader-profile names) in a stride-2 loop, writes a
    // profile code from the parallel rodata table (dword_82F30F7C) back into the name head,
    // then runs a block of WORLD / INSTANCING_MATRIX_ARRAY / WORLD_VIEW_PROJECTION
    // first-constant validation that fires CgsContainers::BasePriorityQueue-backed assert
    // log messages. It also calls an un-recovered helper sub_827ED8D0(u32*, int) four times
    // (at blob words 7/11/20/24, semantics unknown).
    //
    // Both rodata tables (off_82F30F78's 8 profile-name strings and dword_82F30F7C's codes)
    // are NOT present in the repo exports (un-recoverable rodata), and sub_827ED8D0 is
    // unidentified, so a faithful body cannot be reconstructed without fabrication. Deferred.
    // A declared-but-undefined virtual compiles fine under the per-TU `cl /c` gate (which does
    // not link or instantiate this class), so leaving it bodyless does not break the gate.
}
