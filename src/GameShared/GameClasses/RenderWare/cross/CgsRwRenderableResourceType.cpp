#include "GameShared/GameClasses/RenderWare/cross/CgsRwRenderableResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // CgsResource::GetLoadBase64
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"     // Renderable / ObjectScopeTextureInfo
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h" // RenderableMesh
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"      // CgsGraphics::MaterialAssembly
#include "pc/gcm/renderengine/IndexBuffer.h"                          // renderengine::IndexBuffer
#include "pc/gcm/renderengine/VertexBuffer.h"                         // renderengine::VertexBuffer

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwRenderableResourceType::GetTypeID        @ 0x828A7D58  (executed)
//   CgsResource::RwRenderableResourceType::FixDown          @ 0x828A7D60
//   CgsResource::RwRenderableResourceType::GetImportPointer @ 0x828A7DA0
//   CgsResource::RwRenderableResourceType::DebugValidate    @ 0x828A7DE0
//
// FixDown and GetImportPointer are "not at runtime" stubs (the X360 unconditionally
// fires its assert). DebugValidate walks the serialised renderable via the NAMED members
// of Renderable / RenderableMesh / CgsGraphics::MaterialAssembly.
// SEAM (platform-4 widened blob): the converted x64 data is the natural widening of those
// committed layouts -- mesh count u16 @+0x12 (unchanged from X360), mesh pointer table
// @+0x18 with u64 entries (X360: u32 table @+0x14, lwz 0x14), mesh material assembly u64
// @+0x20 (X360: u32 @+32). Field roles from the ARTIST asm: +0x12 mesh count (lhz, 16-bit),
// +0x14 mesh-pointer-array base (lwz), mesh+32 material assembly, assembly+8 non-empty
// count (lbz byte = mu8NumMaterials), assembly[0] technique-table pointer (mappMaterials).
// NB: the X360 offsets are RAW BYTES — the Hex-Rays pseudocode renders them as
// *(a2+18)/*(a2+20) in int* arithmetic (=72/80 bytes), but the ARTIST asm is lhz 0x12 /
// lwz 0x14 = byte 18 / byte 20. Trust the asm.

namespace CgsResource
{
    // ---- file-local rotate/align helpers (mirror CgsModelResourceType.cpp) --------
    namespace
    {
        // Rotate-left by 2, matching the X360 __ROL4__ idiom (== *4 for in-range 16-bit counts).
        inline u32 RotL2(u32 luValue) { return (luValue << 2) | (luValue >> 30); }
        // Rotate-left by 1 (== *2 for in-range 16-bit counts).
        inline u32 RotL1(u32 luValue) { return (luValue << 1) | (luValue >> 31); }
        inline u32 Align4(u32 luValue) { return (luValue + 3) & ~static_cast<u32>(3); }
    }

    static const uint32_t KU_RW_RENDERABLE_RESOURCE_TYPE_ID = 12;

    // The single renderable data version the code understands (X360 fires BOTH the
    // "Data is out of date" (<) and "Code is out of date" (>) asserts against 11).
    static const u32 KU_RENDERABLE_DATA_VERSION = 11;

    // Renderable::mu16Flags bit set once the graph has been relocated (X360 `ori r11,r11,8`).
    static const u16 KU_RENDERABLE_FLAG_FIXED_UP = 8;

    uint32_t RwRenderableResourceType::GetTypeID() const
    {
        return KU_RW_RENDERABLE_RESOURCE_TYPE_ID;
    }

    namespace
    {
        // Relocate one serialised pointer slot: the converted platform-4 blob stores a
        // resource-relative OFFSET in a host-width slot, so the load base is simply added.
        template <typename T>
        inline T* RebaseSlot(T* lpSlot, uintptr_t luBase)
        {
            return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lpSlot) + luBase);
        }
    }

    // @0x828A9498 -- override. The serialise-IN relocation for a whole renderable graph.
    //
    // X360 store-for-store (asm, byte offsets: version lhz 0x10, mesh count lhz 0x12, mesh table
    // lwz 0x14, object-scope texture info lwz 0x18, flags lhz 0x1C):
    //   1) two version tripwires against 11
    //   2) *(this+0x14) += delta                                   (the mesh pointer table)
    //   3) if (*(this+0x18)) { *(this+0x18) += delta; then its +4/+8/+0xC slots += delta;
    //      then for i < *(u16*)(osti+0): ((u32*)osti->+0xC)[i] += delta }   (the name pool)
    //   4) for i < numMeshes: table[i] += delta; FixUpRenderableMesh(this, table[i], a3)
    //   5) flags |= 8 (only when clear)
    //
    // SEAM (platform-4 widened data): the pointer slots are 8 bytes wide in the converted blob
    // (tools/assets/bundles/renderable_transcode.py emits the natural MSVC x64 relayout:
    // Renderable 0x30 with mppMeshes @+0x18 / mpObjectScopeTextureInfo @+0x20 / flags @+0x28,
    // ObjectScopeTextureInfo 0x20 with the three tables @+0x08/+0x10/+0x18), so the walk is by
    // NAME over the committed Renderable / RenderableMesh layouts and the delta is the full-width
    // load base. The graph is otherwise identical store-for-store to the console's.
    void RwRenderableResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        Renderable* lpRenderable = static_cast<Renderable*>(lpResource);

        RwRenderableFixUpData lFixUp;
        lFixUp.muPointerBase  = CgsResource::GetLoadBase64(lrResource);               // *a3
        lFixUp.muBufferOffset = static_cast<u32>(reinterpret_cast<uintptr_t>(
                                    lrResource.m_baseResources[2]));                  // a3[2]

        // CgsRwRenderableResourceType.cpp:490 / :492 -- the two halves of the version tripwire.
        CGS_ASSERT(lpRenderable->mu16VersionNumber >= KU_RENDERABLE_DATA_VERSION,
                   "Error loading renderable. Data is out of date. Data version : ");
        CGS_ASSERT(lpRenderable->mu16VersionNumber <= KU_RENDERABLE_DATA_VERSION,
                   "Error loading renderable. Code is out of date. Data version : ");

        // 2) the mesh pointer table (X360 `lwz r10,0x14` / `add` / `stw` -- unconditional).
        lpRenderable->mppMeshes = RebaseSlot(lpRenderable->mppMeshes, lFixUp.muPointerBase);

        // 3) the object-scope texture-info block and everything it points at.
        Renderable::ObjectScopeTextureInfo* lpTextureInfo = lpRenderable->mpObjectScopeTextureInfo;
        if (lpTextureInfo != 0)
        {
            lpTextureInfo = RebaseSlot(lpTextureInfo, lFixUp.muPointerBase);
            lpRenderable->mpObjectScopeTextureInfo = lpTextureInfo;

            // The three interior tables (X360 +4 / +8 / +0xC, read into temporaries before the
            // stores). The purpose map and the texture-pointer array are relocated even when the
            // count is zero, exactly as the console does.
            lpTextureInfo->mpi16PurposeToIndexMapping =
                RebaseSlot(lpTextureInfo->mpi16PurposeToIndexMapping, lFixUp.muPointerBase);
            lpTextureInfo->mppTextures =
                RebaseSlot(lpTextureInfo->mppTextures, lFixUp.muPointerBase);
            lpTextureInfo->mppcTextureNames =
                RebaseSlot(lpTextureInfo->mppcTextureNames, lFixUp.muPointerBase);

            // ...then each texture-name string pointer in the name table.
            const u32 luNumTextures = lpTextureInfo->mu16NumTextures;
            for (u32 luName = 0; luName < luNumTextures; ++luName)
            {
                lpTextureInfo->mppcTextureNames[luName] =
                    RebaseSlot(lpTextureInfo->mppcTextureNames[luName], lFixUp.muPointerBase);
            }
        }

        // 4) each mesh pointer, then the mesh's own buffer relocation.
        const u32 luNumMeshes = lpRenderable->mu16NumMeshes;
        for (u32 luMesh = 0; luMesh < luNumMeshes; ++luMesh)
        {
            RenderableMesh* lpMesh =
                RebaseSlot(lpRenderable->mppMeshes[luMesh], lFixUp.muPointerBase);
            lpRenderable->mppMeshes[luMesh] = lpMesh;
            FixUpRenderableMesh(reinterpret_cast<RwRenderableMesh*>(lpMesh), &lFixUp);
        }

        // 5) mark the graph relocated.
        if ((lpRenderable->mu16Flags & KU_RENDERABLE_FLAG_FIXED_UP) == 0)
            lpRenderable->mu16Flags |= KU_RENDERABLE_FLAG_FIXED_UP;
    }

    void RwRenderableResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        (void)lpResource;
        (void)lrResource;
        CGS_ASSERT(false, "Don't call this function at runtime");
    }

    void RwRenderableResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const
    {
        (void)lpResource;
        (void)luIndex;
        (void)lpuOffset;
        (void)lppValue;
        CGS_ASSERT(false, "Should not be called at runtime");
    }

    bool RwRenderableResourceType::DebugValidate(const void* lpResource) const
    {
        // SEAM (platform-4 widened-blob walk): named-struct access replaces the X360
        // 32-bit-slot byte-offset pokes; the converted data matches the committed
        // Renderable / RenderableMesh / MaterialAssembly x64 layouts (count u16 @+0x12
        // unchanged, mesh table u64 entries @+0x18, assembly u64 @+0x20).
        const Renderable* lpRenderable = static_cast<const Renderable*>(lpResource);

        // FLAG: ARTIST `lhz r11,0x12(r29)` — byte offset 18, 16-bit zero-extended count.
        const u32 luNumMeshes = lpRenderable->mu16NumMeshes;                 // lhz 0x12(a2)
        if (luNumMeshes == 0)
            return true;                                                     // LABEL_7: result = 1

        // FLAG: ARTIST `lwz r11,0x14(r29)` — X360 byte 20 mesh-ptr array base; widened
        // blob: u64-entry table @+0x18 (mppMeshes).
        RenderableMesh* const* lppMeshes = lpRenderable->mppMeshes;          // lwz 0x14(a2)

        for (u32 luI = 0; luI < luNumMeshes; ++luI)                          // v2 < *(a2+18); v3 += 4
        {
            const RenderableMesh* lpMesh = lppMeshes[luI];                   // *(*(a2+20)+v3)
            const CgsGraphics::MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly; // X360 *(mesh+32); widened +0x20 u64
            if (lpAssembly)
            {
                // FLAG: ARTIST `lbz r11,8(r31)` — byte load at X360 assembly+8 = the
                // mu8NumMaterials technique count (natural x64 home @+0x0C; by NAME).
                if (lpAssembly->mu8NumMaterials == 0)                        // lbz 8(v4)
                {
                    CGS_ASSERT(false, "Mesh material assembly is empty");                 // line 1273
                    return false;                                                         // result = 0
                }
                // X360 assembly[0] = the technique-table pointer (mappMaterials); its first
                // entry must be a non-NULL MaterialTechnique*.
                if (lpAssembly->mappMaterials[0] == nullptr)                 // if (!**v4)
                {
                    // X360 breaks out of the loop here, then fires the NULL-material
                    // assert; flattened to the in-loop assert + return (same result).
                    CGS_ASSERT(false, "Material assembly contains a NULL material");      // line 1278
                    return false;                                                         // result = 0
                }
            }
        }
        return true;                                                         // all meshes valid
    }

    // @0x828A96F0 -- size the fixed per-renderable overhead: the mesh table, the object-scope
    // texture-info block (texture-pointer array + purpose->index map) and the packed
    // texture-name string pool. Returns a five-entry descriptor whose first entry is
    // {overhead, 4} and whose remaining four are {0, 1}.
    //   luSize  = align4(numMeshes*4) + 0x20
    //           + 2*(align4(numTextures*4) + 8) + align4(maxTextures*2)
    //           + sum over each texture name of ((strlen+4) & ~3)
    // Members reached by name via Renderable.h (mu16NumMeshes @+0x12,
    // mpObjectScopeTextureInfo @+0x18; ObjectScopeTextureInfo: mu16NumTextures @+0x00,
    // mu16MaxNumTextures @+0x02, mppcTextureNames @+0x0C).
    CgsResource::ResourceDescriptor
    RwRenderableResourceType::GetRenderableBasicResourceDescriptor(const Renderable* lpRenderable) const
    {
        u32 luSize = Align4(RotL2(lpRenderable->mu16NumMeshes)) + 0x20u;

        const Renderable::ObjectScopeTextureInfo* lpTextures = lpRenderable->mpObjectScopeTextureInfo;
        if (lpTextures)
        {
            const u32 luNumTextures = lpTextures->mu16NumTextures;
            const u32 luMaxTextures = lpTextures->mu16MaxNumTextures;

            luSize += 2u * (Align4(RotL2(luNumTextures)) + 8u) + Align4(RotL1(luMaxTextures));

            if (luNumTextures != 0u)
            {
                char** lppcNames  = lpTextures->mppcTextureNames;
                u32    luRemaining = luNumTextures;
                do
                {
                    const char* lpcName = *lppcNames;
                    u32 luLen = 0u;
                    while (lpcName[luLen] != '\0')
                        ++luLen;
                    luSize += (luLen + 4u) & ~static_cast<u32>(3);
                    ++lppcNames;
                    --luRemaining;
                }
                while (luRemaining != 0u);
            }
        }

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;
        for (u32 luIndex = 1u; luIndex < 5u; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // @0x828A9AB0 -- override. Total serialised allocation for a renderable: the fixed
    // per-renderable overhead (mesh table + texture-info + name pool) plus, for each mesh,
    // its vertex-descriptor block, its GPU index buffer, and each of its GPU vertex buffers.
    // Accumulated with rw::BaseResourceDescriptors<5>::operator+= exactly as the X360 folds
    // each sub-descriptor into the running total. The private per-block helpers the DWARF
    // lists (GetIndexBufferResourceDescriptor / GetVertexBufferResourceDescriptor / ...)
    // were inlined by the X360 compiler into this body; reconstructed in that flattened form.
    CgsResource::ResourceDescriptor
    RwRenderableResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const Renderable* lpRenderable = reinterpret_cast<const Renderable*>(lpResource);

        // Seed with the fixed per-renderable overhead.
        CgsResource::ResourceDescriptor lTotal = GetRenderableBasicResourceDescriptor(lpRenderable);

        for (u32 luMesh = 0u; luMesh < lpRenderable->mu16NumMeshes; ++luMesh)
        {
            RenderableMesh* lpMesh = lpRenderable->mppMeshes[luMesh];

            // 1) The mesh's own vertex-descriptor block. (DWARF renderablemesh.h:83; the asm
            //    passes numVertexBuffers @+0x26 then numVertexDescriptors @+0x24.)
            CgsResource::ResourceDescriptor lMeshDescriptor =
                lpMesh->GetResourceDescriptor(lpMesh->mu8NumVertexBuffers, lpMesh->mu8NumVertexDescriptors);
            lTotal += lMeshDescriptor;

            // 2) The GPU index buffer (maBuffers[0]). Read its bit width + index count, then
            //    build a five-entry descriptor inline: slot0 = 36-byte header {36,4},
            //    slot2 = the 16-byte-aligned index bytes {aligned,4}, the rest {0,1}.
            //    bytesPerIndex = 2 for 16-bit indices, 4 otherwise.
            renderengine::IndexBufferHeader* lpIndexBuffer =
                reinterpret_cast<renderengine::IndexBufferHeader*>(lpMesh->maBuffers[0]);
            renderengine::IndexBufferParamsOut lIndexParams = { 0u, 0u, 0u };
            renderengine::IndexBuffer::GetParameters(lpIndexBuffer, &lIndexParams);

            const u32 luBytesPerIndex = (lIndexParams.muBits == 16u) ? 2u : 4u;
            const u32 luIndexBytes    = (luBytesPerIndex * lIndexParams.muCount + 15u) & ~static_cast<u32>(15);

            CgsResource::ResourceDescriptor lIndexDescriptor;
            lIndexDescriptor.m_baseResourceDescriptors[0].m_size      = 36u;   // index-buffer header
            lIndexDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;
            lIndexDescriptor.m_baseResourceDescriptors[1].m_size      = 0u;
            lIndexDescriptor.m_baseResourceDescriptors[1].m_alignment = 1u;
            lIndexDescriptor.m_baseResourceDescriptors[2].m_size      = luIndexBytes;
            lIndexDescriptor.m_baseResourceDescriptors[2].m_alignment = 4u;
            lIndexDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;
            lIndexDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;
            lIndexDescriptor.m_baseResourceDescriptors[4].m_size      = 0u;
            lIndexDescriptor.m_baseResourceDescriptors[4].m_alignment = 1u;
            lTotal += lIndexDescriptor;

            // 3) Each GPU vertex buffer (maBuffers[1 + vb]). Read its parameters, ask the
            //    vertex buffer for its descriptor, fold it in.
            for (u32 luVb = 0u; luVb < lpMesh->mu8NumVertexBuffers; ++luVb)
            {
                renderengine::VertexBufferHeader* lpVertexBuffer =
                    reinterpret_cast<renderengine::VertexBufferHeader*>(lpMesh->maBuffers[1u + luVb]);

                u32 laVbParams[2] = { 0u, 0u };
                renderengine::VertexBuffer::GetParameters(lpVertexBuffer, laVbParams);

                CgsResource::ResourceDescriptor lVbDescriptor;
                renderengine::VertexBuffer::GetResourceDescriptor(
                    reinterpret_cast<u64*>(&lVbDescriptor),
                    static_cast<int>(reinterpret_cast<usize>(laVbParams)));
                lTotal += lVbDescriptor;
            }
        }

        return lTotal;
    }
}
