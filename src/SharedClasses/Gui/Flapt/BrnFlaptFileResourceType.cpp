#include "SharedClasses/Gui/Flapt/BrnFlaptFileResourceType.h"

#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"   // BrnFlapt::FlaptFile (FixUp/FixDown)
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "rw/rwcore_structs.h"                       // rw::Resource, rw::BaseResourceDescriptors<5>

#include <cstdint>   // uintptr_t

// BrnFlapt::FlaptFileResourceType member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   GetTypeID                       @ 0x8246D7E8   -> 0x10020
//   GetSerialisedResourceDescriptor @ 0x8246FF98
//   GetImportCount                  @ 0x8246D7F8
//   GetImportPointer                @ 0x8246D858
//   FixDown                         @ 0x82471610   -> FlaptFile::FixDown
//   FixUp                           @ 0x82471618   -> FlaptFile::FixUp
//
// The x64 converter emits the native BrnFlapt::FlaptFile declaration, so these
// accessors use its named members. GetImportCount excludes the runtime-bound
// special tail; GetImportPointer retains ARTIST's total-texture defensive bound.

namespace BrnFlapt
{

// ---- GetTypeID @ 0x8246D7E8 ----------------------------------------------
static const uint32_t KU_FLAPT_FILE_RESOURCE_TYPE_ID = 0x10020;  // 65568

uint32_t FlaptFileResourceType::GetTypeID() const
{
    return KU_FLAPT_FILE_RESOURCE_TYPE_ID;
}

// ---- GetSerialisedResourceDescriptor @ 0x8246FF98 ------------------------
// The whole movie loads as one main block sized by the serialised header's size
// word (image+0x04). ARTIST writes that size into all five serialised-memory
// slots; the main slot is 16-byte aligned and the remaining slots use alignment 1.
CgsResource::ResourceDescriptor
FlaptFileResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    const FlaptFile* lpFile = static_cast<const FlaptFile*>(lpResource);
    const u32 luSerialisedSize = lpFile->muSizeInBytes;

    CGS_ASSERT(luSerialisedSize != 0, "Uninitialised Flapt File resource");

    CgsResource::ResourceDescriptor lDescriptor;
    for (u32 luBlock = 0; luBlock < 5; ++luBlock)
    {
        lDescriptor.m_baseResourceDescriptors[luBlock].m_size = luSerialisedSize;
        lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment =
            (luBlock == 0) ? 16u : 1u;
    }
    return lDescriptor;
}

// ---- GetImportCount @ 0x8246D7F8 -----------------------------------------
// Number of imports = all texture slots minus the runtime-bound special tail.
uint32_t FlaptFileResourceType::GetImportCount(const void* lpResource) const
{
    const FlaptFile* lpFile = static_cast<const FlaptFile*>(lpResource);
    CGS_ASSERT(lpFile->muSizeInBytes != 0, "Uninitialised Flapt File resource");
    CGS_ASSERT(lpFile->muNumTextures >= lpFile->muNumSpecialTextures,
               "Invalid special-texture count in Flapt File");
    return lpFile->muNumTextures - lpFile->muNumSpecialTextures;
}

// ---- GetImportPointer @ 0x8246D858 ---------------------------------------
// Report import luIndex: its value is importTable[luIndex] (the serialised entry
// pointer), and its offset is that slot's byte position within the resource image.
void FlaptFileResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
{
    const FlaptFile* lpFile = static_cast<const FlaptFile*>(lpResource);
    const uintptr_t lImage = reinterpret_cast<uintptr_t>(lpFile);
    CGS_ASSERT(lpFile->muSizeInBytes != 0, "Uninitialised Flapt File resource");
    CGS_ASSERT(luIndex < lpFile->muNumTextures,
        "Tried to acces out-of-range import on Flapt File");

    FlaptFile::GuiTexture** const lpImportTable = lpFile->mpapTextures;
    *lppValue  = lpImportTable[luIndex];
    *lpuOffset = static_cast<u32>(reinterpret_cast<uintptr_t>(&lpImportTable[luIndex]) - lImage);
}

// ---- FixDown @ 0x82471610 / FixUp @ 0x82471618 ---------------------------
// Thin wrappers: delegate the (un)relocation to the FlaptFile movie image itself.
void FlaptFileResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
{
    static_cast<FlaptFile*>(lpResource)->FixDown(lrResource);
}

void FlaptFileResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
{
    static_cast<FlaptFile*>(lpResource)->FixUp(lrResource);
}

}
