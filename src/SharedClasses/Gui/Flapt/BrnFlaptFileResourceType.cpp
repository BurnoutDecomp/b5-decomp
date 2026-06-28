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
// The accessor bodies read the SERIALISED Flapt movie image directly (a fixed
// on-disk byte layout, not a C++ class), so the documented serialised-blob
// exception applies — its header words are reached by offset (the same pattern as
// CgsResource::AptDataHeaderType). The few fields touched:
//   image+0x04  luSerialisedSize  (0 until loaded; "Uninitialised" guard + slot-0 size)
//   image+0x14  the import-range end word    } their difference is the import count
//   image+0x48  the import-range start word  }
//   image+0x18  the import table (array of serialised import-entry pointers)

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
// word (image+0x04). Build a five-entry descriptor whose first slot is
// {luSerialisedSize, alignment = 16} and whose remaining four slots are {0, 1}
// (no sub-allocations). Matches CgsResource::FontResourceType's descriptor build.
CgsResource::ResourceDescriptor
FlaptFileResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    const uintptr_t lImage = reinterpret_cast<uintptr_t>(lpResource);
    const u32 luSerialisedSize = *reinterpret_cast<const u32*>(lImage + 0x04);

    CGS_ASSERT(luSerialisedSize != 0, "Uninitialised Flapt File resource");

    CgsResource::ResourceDescriptor lDescriptor;
    u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
    lpData[0] = luSerialisedSize;  lpData[1] = 16u;  // slot 0 (main): {size, align}
    lpData[2] = 0u;  lpData[3] = 1u;
    lpData[4] = 0u;  lpData[5] = 1u;
    lpData[6] = 0u;  lpData[7] = 1u;
    lpData[8] = 0u;  lpData[9] = 1u;
    return lDescriptor;
}

// ---- GetImportCount @ 0x8246D7F8 -----------------------------------------
// Number of imports = (header word @0x14) - (header word @0x48).
uint32_t FlaptFileResourceType::GetImportCount(const void* lpResource) const
{
    const uintptr_t lImage = reinterpret_cast<uintptr_t>(lpResource);

    CGS_ASSERT(*reinterpret_cast<const u32*>(lImage + 0x04) != 0,
        "Uninitialised Flapt File resource");

    const u32 luImportRangeEnd   = *reinterpret_cast<const u32*>(lImage + 0x14);
    const u32 luImportRangeStart = *reinterpret_cast<const u32*>(lImage + 0x48);
    return luImportRangeEnd - luImportRangeStart;
}

// ---- GetImportPointer @ 0x8246D858 ---------------------------------------
// Report import luIndex: its value is importTable[luIndex] (the serialised entry
// pointer), and its offset is that slot's byte position within the resource image.
void FlaptFileResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
{
    const uintptr_t lImage = reinterpret_cast<uintptr_t>(lpResource);

    CGS_ASSERT(*reinterpret_cast<const u32*>(lImage + 0x04) != 0,
        "Uninitialised Flapt File resource");
    CGS_ASSERT(luIndex < *reinterpret_cast<const u32*>(lImage + 0x14),
        "Tried to acces out-of-range import on Flapt File");

    u32* const lpImportTable = *reinterpret_cast<u32* const*>(lImage + 0x18);
    *lppValue  = reinterpret_cast<const void*>(lpImportTable[luIndex]);
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
