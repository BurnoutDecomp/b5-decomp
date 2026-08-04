#include "GameShared/GameClasses/RenderWare/cross/CgsModelResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (FixUp version tripwire)
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"        // PostFixUp walks the graph
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"    // mu8InstanceCount / mpMaterialAssembly
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"        // GetLength()

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ModelResourceType::FixDown                       @ 0x828A8548
//   CgsResource::ModelResourceType::GetImportCount                @ 0x828A7D48
//   CgsResource::ModelResourceType::GetImportPointer              @ 0x828A7D08
//   CgsResource::ModelResourceType::GetSerialisedResourceDescriptor @ 0x828A9418
//   CgsResource::ModelResourceType::GetTypeID                     @ 0x828A7A60
//
// FixDown rebases the three pointers at 0/4/8 of the model resource by the
// relocation delta (the rw::Resource's load base). GetImportCount/GetImportPointer
// read the import table (count at +16, pointer base at +0).
// GetSerialisedResourceDescriptor returns a five-entry descriptor whose first entry's
// size is the rounded sum of three sub-block sizes (from the counts at +16/+18) + 20.

namespace CgsResource
{
    namespace
    {
        // Rotate-left by 2, matching the X360 __ROL4__ idiom (== *4 for in-range counts).
        inline u32 RotL2(u32 luValue)
        {
            return (luValue << 2) | (luValue >> 30);
        }

        inline u32 Align4(u32 luValue)
        {
            return (luValue + 3) & ~static_cast<u32>(3);
        }
    }

    static const uint32_t KU_MODEL_RESOURCE_TYPE_ID = 42;

    uint32_t ModelResourceType::GetTypeID() const
    {
        return KU_MODEL_RESOURCE_TYPE_ID;
    }

    // FixUp @ 0x828A8578 -- the serialise-IN counterpart of FixDown. Tripwires the model
    // data version (a BYTE at +19; the X360 streams "Model data version mismatch. Code
    // version = 2 Data version = <n>" at CgsModelResourceType.cpp:219), then rebases the
    // three u32 pointer slots at +0/+4/+8 by the load base -- reading +4 and +8 into
    // temporaries BEFORE storing +0, exactly like FixDown. Without this override the model
    // keeps its file-relative LOD-radius / renderable tables and every consumer
    // (InstanceListResourceType::PostFixUp first) walks from a raw file offset.
    void ModelResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        CGS_ASSERT(*reinterpret_cast<const u8*>(lBase + 19) == 2,   // serialised blob
                   "Model data version mismatch. Code version = ");

        u32 luP1 = *reinterpret_cast<u32*>(lBase + 4) + luDelta;   // serialised blob
        u32 luP2 = *reinterpret_cast<u32*>(lBase + 8) + luDelta;   // serialised blob
        *reinterpret_cast<u32*>(lBase + 0) += luDelta;   // serialised blob
        *reinterpret_cast<u32*>(lBase + 4) = luP1;
        *reinterpret_cast<u32*>(lBase + 8) = luP2;
    }

    // PostFixUp @ 0x828A7A68 (DWARF CgsModelResourceType.cpp:241) -- the pass the pool runs
    // after EVERY resource in the bundle has been fixed up AND had its imports resolved
    // (CgsResourceBundleLoader: FixUp-all -> ResolveImports-all -> PostFixUp-all), so the
    // renderable pointers below are live and their own graphs are already relocated.
    //
    // Its whole job is to RAISE Model::E_FLAG_MODEL_USES_INSTANCE_SHADER. That bit is never
    // authored on disc -- measured 0x00 in the X360 retail wheel bundle, in our port and in
    // the BPR Remaster -- it is COMPUTED HERE from the per-mesh mu8InstanceCount:
    //
    //     for each renderable of the model
    //         for each mesh of the renderable
    //             assert mu8NumVertexDescriptors == mpMaterialAssembly->GetLength()
    //             if (mu8InstanceCount) { anyInstanced = true; break; }   // MESH loop only
    //     if (anyInstanced) mu8Flags |= 1
    //
    // Store-for-store against the asm: mu8NumRenderables is re-read from +0x10 on every
    // outer iteration; the inner break leaves the remaining meshes of THAT renderable
    // unasserted but the renderable loop runs to completion; the local flag is set once and
    // never cleared; the OR happens exactly once at the end.
    //
    // WIDTHS. The Model is the console's own 20-byte on-disc header -- its three pointer
    // slots stay 32-bit on x64 (Ptr32, see CgsModel.h) and Pool::ResolveImportForEntry writes
    // the narrow store into the renderable table -- so the header is walked by OFFSET with
    // the member name on every line, exactly as FixUp/FixDown above do. The Renderable and
    // RenderableMesh graphs are the deliberately WIDENED x64 relayout, so those are walked by
    // NAMED MEMBER and never by a pinned offset.
    //
    // ⛔ Do not "simplify" this to a Model method call: the DWARF makes mppRenderables /
    // mu8NumRenderables / mu8Flags protected with no by-index accessor, and the console body
    // makes no method calls at all.
    void ModelResourceType::PostFixUp(void* lpResource, const rw::Resource& /*lrResource*/) const
    {
        const uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);

        bool lbAnyInstanced = false;

        for (u32 luRenderable = 0;
             luRenderable < *reinterpret_cast<const u8*>(lBase + 16);   // serialised blob: mu8NumRenderables
             ++luRenderable)
        {
            // mppRenderables (+0x00) -> a table of 32-bit renderable slots; the console
            // re-reads the table base every iteration.
            const uintptr_t lTable =
                static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lBase + 0));   // serialised blob
            const Renderable* lpRenderable = reinterpret_cast<const Renderable*>(
                static_cast<uintptr_t>(*reinterpret_cast<const u32*>(   // serialised blob: mppRenderables[i]
                    lTable + 4u * luRenderable)));

            // [FLAG PC boot gate] The console always has every dependency bundle resident, so
            // this slot is never null there. On this build a cross-bundle renderable import
            // can still be unresolved, and Pool::ResolveImportForEntry writes a NULL for it;
            // the console's unguarded read would be an access violation here. Skip, do not
            // assert -- the pool already logs the unresolved id.
            if (lpRenderable == 0)
                continue;

            const u32 luNumMeshes = lpRenderable->mu16NumMeshes;
            for (u32 luMesh = 0; luMesh < luNumMeshes; ++luMesh)
            {
                const RenderableMesh* lpMesh = lpRenderable->mppMeshes[luMesh];
                if (lpMesh == 0)
                    continue;

                // CgsModelResourceType.cpp:261 -- "THIS ASSERT IS WANTED DEAD OR ALIVE."
                // [FLAG PC boot gate] guarded on the assembly pointer for the same reason
                // DrawRenderable::Interpret guards it: a mesh whose Material import lives in a
                // bundle the pool refused keeps a null assembly, and reading GetLength() off
                // null is an access violation. The assert itself is NOT downgraded.
                const CgsGraphics::MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly;
                if (lpAssembly != 0)
                {
                    CGS_ASSERT(lpMesh->mu8NumVertexDescriptors == lpAssembly->GetLength(),
                               "lpMesh->GetNumVertexDescriptors() == lpMesh->mpMaterialAssembly->GetLength()");
                }

                if (lpMesh->mu8InstanceCount != 0)
                {
                    lbAnyInstanced = true;
                    break;
                }
            }
        }

        if (lbAnyInstanced)
        {
            // mu8Flags |= E_FLAG_MODEL_USES_INSTANCE_SHADER   (`lbz/ori 1/stb` @0x828A7CF4)
            *reinterpret_cast<u8*>(lBase + 17) |= 1u;   // serialised blob
        }
    }

    void ModelResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        u32 luP1 = *reinterpret_cast<u32*>(lBase + 4) - luDelta;
        u32 luP2 = *reinterpret_cast<u32*>(lBase + 8) - luDelta;
        *reinterpret_cast<u32*>(lBase + 0) -= luDelta;
        *reinterpret_cast<u32*>(lBase + 4) = luP1;   // serialised blob
        *reinterpret_cast<u32*>(lBase + 8) = luP2;   // serialised blob
    }

    uint32_t ModelResourceType::GetImportCount(const void* lpResource) const
    {
        // FLAG: ARTIST 0x828A7D48 `lbz r3,0x10(r4)` (and DecFIGS 0xC44A00 `lbz`) -- the
        // import count at +16 is a BYTE field (u8), not u16. Was incorrectly read as u16.
        return *reinterpret_cast<const u8*>(reinterpret_cast<uintptr_t>(lpResource) + 16);
    }

    void ModelResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        // FLAG: ARTIST 0x828A7D08 `lbz r11,0x10(r4)` -- count at +16 is a BYTE (u8), not u16.
        u32 luCount = *reinterpret_cast<const u8*>(lBase + 16);

        if (luIndex >= luCount)
        {
            *lppValue = nullptr;
            *lpuOffset = 0;
        }
        else
        {
            uintptr_t lTable = *reinterpret_cast<const u32*>(lBase + 0);
            *lppValue = reinterpret_cast<const void*>(*reinterpret_cast<const u32*>(lTable + 4 * luIndex));
            *lpuOffset = 4 * (luIndex + 5);
        }
    }

    ResourceDescriptor ModelResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);

        // FLAG: ARTIST 0x828A9418 `lbz r9,0x12(r5)` / `lbz r8,0x10(r5)` (DecFIGS 0xC4705C
        // same) -- fields at +16 and +18 are BYTE fields (u8), not u16. Were read as u16.
        u32 luField18   = *reinterpret_cast<const u8*>(lRes + 18);
        u32 luField16x4 = RotL2(*reinterpret_cast<const u8*>(lRes + 16));

        // First entry: total rounded size (alignment 4); the remaining four empty.
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = Align4(luField16x4)
                                                + Align4(RotL2(luField18))
                                                + Align4(luField18)
                                                + 20;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }
}
