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
    // word @ +148 is rebased -- that word is the technique NAME pointer (`mpacName` on
    // burnout.wiki; GetSerialisedResourceDescriptor @0x827F7A68 strlens it), not a
    // sampler-state block (correction from tools/assets/shaders/FORMAT_MAP.md section 2).
    //
    // Every offset here is a CONSOLE 32-bit word offset and every slot a u32 -- the platform-4
    // conversion keeps the console layout byteswapped (FORMAT_MAP.md section 2), so the nested
    // FixUps below must read u32 slots too. They do: ShaderConstants{Internal,External}::FixUp
    // and ShaderConstantHashTable::FixUp each walk their own serialised word view rather than
    // their host-width members (see those TUs' banners).
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

    // --- GetConstantHashTableSerialisedResourceDescriptorSize @ 0x827E9D38 ------------
    // Size of the serialised constant-hash-table sub-block. The hash table's word @ +8 is the
    // entry count and word @ +4 is the array of NUL-terminated key-name char*'s. The serialised
    // size is 4*count (the returned base term, X360 r6) plus a running total also seeded at
    // 4*count (X360 r8) that adds, per entry, the name length rounded up to a 4-byte boundary
    // INCLUDING the terminator: (strlen + 4) & ~3. Net result = 8*count + Sum((strlen(key)+4)&~3).
    // Raw-offset access is the DOCUMENTED serialised-blob exception.
    uint32_t ShaderTechniqueResourceType::GetConstantHashTableSerialisedResourceDescriptorSize(
        const CgsGraphics::ShaderConstantHashTable* lpHashTable) const
    {
        // SERIALISED-BLOB walk: the hash table is three u32 slots (+0 keys, +4 names, +8 count)
        // -- the same console word view CgsShaderConstantHashTable.cpp's banner documents, NOT
        // the host-width members. The name ARRAY is therefore also u32 slots.
        const u32* const lpaWords = reinterpret_cast<const u32*>(lpHashTable);
        u32 luCount = lpaWords[2];                                    // serialised blob: count @+8
        const u32 luBase  = 4u * luCount;   // X360 r6 (returned base term)
        u32       luTotal = 4u * luCount;   // X360 r8 (running total, seeded identically)
        if (luCount)
        {
            const u32* lpaNames =
                reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpaWords[1]));  // serialised blob: names @+4
            do
            {
                const char* lpName =
                    reinterpret_cast<const char*>(static_cast<uintptr_t>(*lpaNames++));
                const char* lpScan = lpName;
                while (*lpScan++)
                    ;
                const u32 luLen = static_cast<u32>(lpScan - lpName - 1);   // strlen
                --luCount;
                luTotal += (luLen + 4u) & 0xFFFFFFFCu;                     // round up incl. NUL
            } while (luCount);
        }
        return luBase + luTotal;
    }

    // --- GetShaderSamplersSerialisedResourceDescriptorSize @ 0x827E9C30 ---------------
    // Size of the serialised sampler table. The sampler COUNT is a signed byte @ blob+144
    // (X360 lbz+extsb) and the sampler-array pointer is @ blob+140. Each 8-byte sampler entry
    // begins with a NUL-terminated name char*; the serialised size is 8*count plus, per
    // sampler, the name length rounded up to a 4-byte boundary including the terminator
    // ((strlen + 4) & ~3). The loop runs only for a strictly positive count. Raw-offset access
    // is the DOCUMENTED serialised-blob exception.
    uint32_t ShaderTechniqueResourceType::GetShaderSamplersSerialisedResourceDescriptorSize(
        CgsGraphics::ShaderTechnique* lpTechnique) const
    {
        const u8* const lpBlob = reinterpret_cast<const u8*>(lpTechnique);
        const s32 liCount = static_cast<s32>(*reinterpret_cast<const s8*>(lpBlob + 144)); // signed byte
        u32 luResult = static_cast<u32>(8 * liCount);
        if (liCount > 0)
        {
            const char* const* lppNames = *reinterpret_cast<const char* const* const*>(lpBlob + 140);
            s32 liI = 0;
            do
            {
                const char* lpName = lppNames[liI];
                const char* lpScan = lpName;
                while (*lpScan++)
                    ;
                const u32 luLen = static_cast<u32>(lpScan - lpName - 1);   // strlen
                ++liI;
                luResult += (luLen + 4u) & 0xFFFFFFFCu;
            } while (liI < liCount);
        }
        return luResult;
    }

    // --- GetSerialisedResourceDescriptor @ 0x827F7A68 ---------------------------------
    // Compute the whole-resource serialised size of one shader technique and package it as a
    // five-entry rw::BaseResourceDescriptors<5>. The technique blob embeds, at fixed byte
    // offsets, two engine-internal constant blocks (+8, +60), four external constant blocks
    // (+28, +44, +80, +96), one constant hash table (+128), the sampler table (base @ +0), and
    // the technique name string (ptr @ +148). Slot 0 of the descriptor is {size, alignment 16};
    // slots 1..4 are the identity {0, 1}. Raw-offset access is the DOCUMENTED serialised-blob
    // exception.
    ResourceDescriptor ShaderTechniqueResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u8* const lpBlob = static_cast<const u8*>(lpResource);

        // --- first internal-constant block @ +8 (count @ +8, ptr @ +12) --------------------
        u32 luCount0 = *reinterpret_cast<const u32*>(lpBlob + 8);
        const u32 luFixed0 = 8u * luCount0;      // X360 v7 (r27, contributes via luPrefix)
        u32 luBlock0 = 8u * luCount0;            // X360 v8 (r28, running per-entry size)
        if (luCount0)
        {
            luBlock0 = ((luBlock0 + 175u) & 0xFFFFFFF0u) - 160u;
            const u32* lpuElems = *reinterpret_cast<const u32* const*>(lpBlob + 12);
            do
            {
                --luCount0;
                const u32 luElem = 16u * *lpuElems++;
                luBlock0 += (luElem + 15u) & 0xFFFFFFF0u;
            } while (luCount0);
        }

        // --- external-constant blocks @ +44 and +28 ---------------------------------------
        const u32 luExtA = GetShaderConstantExternalSerialisedResourceDescriptorSize(
                               reinterpret_cast<const ShaderConstantsExternal*>(lpBlob + 44));
        const u32 luExtB = GetShaderConstantExternalSerialisedResourceDescriptorSize(
                               reinterpret_cast<const ShaderConstantsExternal*>(lpBlob + 28));

        // Accumulated prefix size (X360 v12/r24): the two external blocks + the two block-0
        // terms + a fixed 160-byte header block.
        const u32 luPrefix = luExtA + luExtB + luFixed0 + luBlock0 + 160u;

        // --- second internal-constant block @ +60 (count @ +60, ptr @ +64) ----------------
        u32 luCount1 = *reinterpret_cast<const u32*>(lpBlob + 60);
        const u32 luFixed1 = 8u * luCount1;      // X360 v14/r25
        u32 luBlock1 = 8u * luCount1;            // X360 v15
        if (luCount1)
        {
            luBlock1 = ((luBlock1 + luPrefix + 15u) & 0xFFFFFFF0u) - luPrefix;
            const u32* lpuElems = *reinterpret_cast<const u32* const*>(lpBlob + 64);
            do
            {
                --luCount1;
                const u32 luElem = 16u * *lpuElems++;
                luBlock1 += (luElem + 15u) & 0xFFFFFFF0u;
            } while (luCount1);
        }

        // --- technique name string @ +148 : plain strlen (X360 v20/r27) -------------------
        const char* lpName = *reinterpret_cast<const char* const*>(lpBlob + 148);
        const char* lpScan = lpName;
        while (*lpScan++)
            ;
        const u32 luNameLen = static_cast<u32>(lpScan - lpName - 1);

        // --- hash table @ +128 and the two remaining external blocks @ +96 / +80 ----------
        const u32 luHash = GetConstantHashTableSerialisedResourceDescriptorSize(
                               reinterpret_cast<const CgsGraphics::ShaderConstantHashTable*>(lpBlob + 128));
        const u32 luExtC = GetShaderConstantExternalSerialisedResourceDescriptorSize(
                               reinterpret_cast<const ShaderConstantsExternal*>(lpBlob + 96));
        const u32 luHashPlusC = luHash + luExtC;                                 // X360 v22 (r26)
        const u32 luExtD = GetShaderConstantExternalSerialisedResourceDescriptorSize(
                               reinterpret_cast<const ShaderConstantsExternal*>(lpBlob + 80)); // X360 v23
        const u32 luTail = luHashPlusC + luExtD;   // X360 r5 (also passed to the sampler sizer)

        // --- sampler table @ +0 -----------------------------------------------------------
        // The X360 passes luTail as a third argument here, but the sampler sizer ignores it;
        // its value is still folded into the total below.
        const u32 luSamplers = GetShaderSamplersSerialisedResourceDescriptorSize(
                                   reinterpret_cast<CgsGraphics::ShaderTechnique*>(const_cast<u8*>(lpBlob)));

        const u32 luSize = luTail + luSamplers + luFixed1 + luNameLen + luBlock1 + luPrefix + 1u;

        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = luSize;  lpData[1] = 16u;   // slot0: {whole-resource size, align 16}
        lpData[2] = 0u;  lpData[3] = 1u;
        lpData[4] = 0u;  lpData[5] = 1u;
        lpData[6] = 0u;  lpData[7] = 1u;
        lpData[8] = 0u;  lpData[9] = 1u;
        return lDescriptor;
    }
}
