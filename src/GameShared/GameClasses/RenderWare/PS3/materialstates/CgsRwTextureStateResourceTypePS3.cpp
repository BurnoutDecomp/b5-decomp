#include "GameShared/GameClasses/RenderWare/PS3/materialstates/CgsRwTextureStateResourceTypePS3.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwTextureStateResourceType::GetTypeID        @ 0x828A8118
//   CgsResource::RwTextureStateResourceType::FixDown          @ 0x828A8120
//   CgsResource::RwTextureStateResourceType::GetImportPointer @ 0x828A8128
//   CgsResource::RwTextureStateResourceType::DebugValidate    @ 0x828A8140
//
// A texture state is a tiny serialised resource: its only import is the texture
// handle at +32. FixDown is a no-op (nothing to relocate); GetImportPointer reports
// the single texture import; DebugValidate checks the handle is set. The serialised
// blob is accessed by raw offset (external rw format). Sibling of the committed
// CgsRwShaderParameterResourceTypePS3 / CgsMaterialStateResourceTypePS3.

namespace CgsResource
{
    // Recovered verbatim from GetTypeID @ 0x828A8118 ("return 14"). 14 = 0x0E.
    static const uint32_t KU_RW_TEXTURE_STATE_RESOURCE_TYPE_ID = 14;

    // The texture import handle lives at +32 in the serialised texture state.
    static const u32 KU_TEXTURE_IMPORT_OFFSET = 32;

    uint32_t RwTextureStateResourceType::GetTypeID() const
    {
        return KU_RW_TEXTURE_STATE_RESOURCE_TYPE_ID;
    }

    // FixDown @ 0x828A8120. Empty: the X360 body is a no-op stub (a texture state has
    // no load-relative pointers to un-relocate — only its import handle, which the
    // resource loader binds separately). FLAG: X360 body is a STUB (empty).
    void RwTextureStateResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        (void)lpResource;
        (void)lrResource;
    }

    // GetImportPointer @ 0x828A8128. The texture state has exactly one import: the
    // texture handle stored at +32. Report its offset (32) and value. (luIndex is
    // unused — there is only one import.) The X360 returns the resource pointer in r3
    // as a fastcall artifact; the contract is void.
    void RwTextureStateResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                                      uint32_t* lpuOffset, const void** lppValue) const
    {
        (void)luIndex;
        *lpuOffset = KU_TEXTURE_IMPORT_OFFSET;                                   // *a3 = 32
        // *a4 = *(lpResource + 32): the stored texture handle (a serialised u32),
        // widened to the host pointer. FLAG: raw-offset access on the serialised blob.
        *lppValue = reinterpret_cast<const void*>(static_cast<uintptr_t>(
            *reinterpret_cast<const u32*>(static_cast<const u8*>(lpResource) + KU_TEXTURE_IMPORT_OFFSET)));
    }

    // DebugValidate @ 0x828A8140. Valid iff the texture handle at +32 is not the
    // -1 (0xFFFFFFFF) "unset" sentinel. The X360 baked path names the X360 variant
    // (CgsRwTextureStateResourceTypeX360.cpp:343); the message is shared, and the
    // simple CGS_ASSERT supplies __FILE__/__LINE__ (no baked line reproduced).
    bool RwTextureStateResourceType::DebugValidate(const void* lpResource) const
    {
        const u32 luTextureHandle =
            *reinterpret_cast<const u32*>(static_cast<const u8*>(lpResource) + KU_TEXTURE_IMPORT_OFFSET);
        if (luTextureHandle == 0xFFFFFFFFu)   // *(a2+32) == -1
        {
            CGS_ASSERT(false, "Texture state has invalid texture value\n");
            return false;                     // result = 0
        }
        return true;                          // result = 1
    }
}
