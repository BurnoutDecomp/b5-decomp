// ============================================================================
// CgsFeatureSchema.cpp -- CgsSound::Playback::FeatureSchema resolved-sub-schema
// accessors.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetParameterSchema     @ 0x82691AB8
//   GetSlotSchema          @ 0x82691BE0
//   GetSlotSchemaAddress   @ 0x82680A30
//
// The ParameterSchema* then SlotSchema* pointers share one trailing flexible array
// (dword +5): parameters occupy [5 .. 5+ParameterSchemaCount), slots follow. Each
// accessor bounds-checks the index and (for the pointer accessors) asserts the slot
// is present and resolved -- the low bit is the serialized "unresolved index" tag.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// @ 0x82691AB8.
const ParameterSchema* FeatureSchema::GetParameterSchema(u32 au32Index) const
{
    CGS_ASSERT(au32Index < mu32ParameterSchemaCount,
               "lu32I < mu32ParameterSchemaCount");

    const void* lpSchema = mapSchema[au32Index];        // *(this + 4*(a2+5))
    CGS_ASSERT(lpSchema != 0, "lpSchema");

    if ((reinterpret_cast<uintptr_t>(lpSchema) & 1) != 0)   // clrlwi ; bne
    {
        CGS_ASSERT(false,
                   "This Data Structure is not resolved. (Name  found in a pointer context.)");
    }

    return reinterpret_cast<const ParameterSchema*>(lpSchema);
}

// @ 0x82691BE0.
const SlotSchema* FeatureSchema::GetSlotSchema(u32 au32Index) const
{
    const void* lpSchema = *GetSlotSchemaAddress(au32Index);     // lwz r29,0(r3)
    CGS_ASSERT(lpSchema != 0, "lpSchema");

    if ((reinterpret_cast<uintptr_t>(lpSchema) & 1) != 0)        // clrlwi ; bne
    {
        CGS_ASSERT(false,
                   "This Data Structure is not resolved. (Name  found in a pointer context.)");
    }

    return reinterpret_cast<const SlotSchema*>(lpSchema);
}

// @ 0x82680A30. Const address of slot au32Index (dereferenced by GetSlotSchema).
// Slots follow the parameter sub-array: slot i lives at dword
// (5 + mu32ParameterSchemaCount + i) == mapSchema[mu32ParameterSchemaCount + i].
const SlotSchema* const* FeatureSchema::GetSlotSchemaAddress(u32 au32Index) const
{
    CGS_ASSERT(au32Index < mu32SlotSchemaCount,
               "lu32I < mu32SlotSchemaCount");

    return reinterpret_cast<const SlotSchema* const*>(
               &mapSchema[mu32ParameterSchemaCount + au32Index]);
}

} // namespace Playback
} // namespace CgsSound
