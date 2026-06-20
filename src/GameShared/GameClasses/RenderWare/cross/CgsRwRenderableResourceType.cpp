#include "GameShared/GameClasses/RenderWare/cross/CgsRwRenderableResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwRenderableResourceType::GetTypeID        @ 0x828A7D58  (executed)
//   CgsResource::RwRenderableResourceType::FixDown          @ 0x828A7D60
//   CgsResource::RwRenderableResourceType::GetImportPointer @ 0x828A7DA0
//   CgsResource::RwRenderableResourceType::DebugValidate    @ 0x828A7DE0
//
// FixDown and GetImportPointer are "not at runtime" stubs (the X360 unconditionally
// fires its assert). DebugValidate walks the serialised renderable by raw offset — the
// renderable is an external rw serialised format with no named struct here, so members
// are reached via reinterpret_cast<const u32*>(luData + offset). Field roles are
// inferred from the asm: +72 mesh count, +80 mesh-pointer-array base, mesh+32 material
// assembly, assembly+8 non-empty flag/count, assembly[0] first material pointer.

namespace CgsResource
{
    static const uint32_t KU_RW_RENDERABLE_RESOURCE_TYPE_ID = 12;

    uint32_t RwRenderableResourceType::GetTypeID() const
    {
        return KU_RW_RENDERABLE_RESOURCE_TYPE_ID;
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
        const uintptr_t luData = reinterpret_cast<uintptr_t>(lpResource);

        const u32 luNumMeshes = *reinterpret_cast<const u32*>(luData + 72);  // *(a2+18)
        if (luNumMeshes == 0)
            return true;                                                     // LABEL_7: result = 1

        const u32 luMeshArray = *reinterpret_cast<const u32*>(luData + 80);  // *(a2+20) — mesh-ptr array base

        for (u32 luI = 0; luI < luNumMeshes; ++luI)                          // v2 < *(a2+18); v3 += 4
        {
            // Serialised renderable holds 32-bit pointers; widen each to the host
            // pointer through uintptr_t (avoids C4312 on the 64-bit gate).
            const u32 luMesh     = *reinterpret_cast<const u32*>(static_cast<uintptr_t>(luMeshArray) + luI * 4);  // *(*(a2+20)+v3)
            const u32 luAssembly = *reinterpret_cast<const u32*>(static_cast<uintptr_t>(luMesh) + 32);            // *(mesh+32) = v4
            if (luAssembly)
            {
                if (*reinterpret_cast<const u32*>(static_cast<uintptr_t>(luAssembly) + 8) == 0)  // if (!*(v4+8))
                {
                    CGS_ASSERT(false, "Mesh material assembly is empty");                 // line 1273
                    return false;                                                         // result = 0
                }
                const u32 luFirstMaterial = *reinterpret_cast<const u32*>(static_cast<uintptr_t>(luAssembly));  // *v4 (assembly[0])
                if (*reinterpret_cast<const u32*>(static_cast<uintptr_t>(luFirstMaterial)) == 0)  // if (!**v4)
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
}
