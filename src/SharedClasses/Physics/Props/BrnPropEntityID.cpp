#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

// Out-of-line bodies for the BrnWorld::PropEntityID accessors the X360 ARTIST build
// emitted. Each accessor first runs the owner tripwire (AssertIsProp) then performs a
// pure bit-field read or read-modify-write on the packed EntityId word. Store order
// and bit-fields are reproduced exactly from the X360 asm (see BrnPropEntityID.h banner
// for the per-symbol addresses and the 8/14/10-bit owner/entity/part layout).
//
// HOST-vs-X360 NOTE: the asm reads the owner via `lbz r11,0(r31)` (byte 0 of the
// big-endian word == the high/owner byte). On a little-endian host that raw byte-0
// read would alias the LOW byte, so the owner comparison is expressed here as the
// field extraction `(muValue >> KU_OWNER_BASE)` — semantically the owner byte, endian-
// independent. The compared constant is E_ENTITYTYPE_PROP (== 3), exactly as the asm.

namespace BrnWorld
{
    // Owner tripwire: baked "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"
    // (..\\..\\..\\SharedClasses\\Physics/Props/BrnPropEntityID.h:278).
    void PropEntityID::AssertIsProp() const
    {
        CGS_ASSERT((mEntityId.muValue >> KU_OWNER_BASE) == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
    }

    // 0x822B7888: store the supplied word, THEN assert the owner byte == 3 (the store
    // precedes the check in the asm — `stw r4,0(r31)` at 0x822B78A4, assert after).
    PropEntityID::PropEntityID(u32 luEntityId)
    {
        mEntityId.muValue = luEntityId;
        AssertIsProp();
    }

    // 0x822B7B08: AssertIsProp, then extract the 14-bit entity index at bit 10
    // (`extrwi r3,r11,14,8` -> (muValue >> 10) & 0x3FFF).
    u32 PropEntityID::GetEntityIndex() const
    {
        AssertIsProp();
        return (mEntityId.muValue & KU_ENTITY_INDEX_MASK) >> KU_ENTITY_INDEX_BASE;
    }

    // 0x822B7B68: AssertIsProp, then extract the low 10-bit part index
    // (`clrlwi r3,r11,22` -> muValue & 0x3FF).
    u32 PropEntityID::GetPartIndex() const
    {
        AssertIsProp();
        return (mEntityId.muValue & KU_PART_INDEX_MASK) >> KU_PART_INDEX_BASE;
    }

    // 0x822B7BC8: AssertIsProp, then return the raw packed word.
    u32 PropEntityID::GetValue() const
    {
        AssertIsProp();
        return mEntityId.muValue;
    }

    // 0x822B79D0: AssertIsProp; then assert the (16-bit-truncated) index < 1<<14
    // (baked CgsEntityId.h:160 "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
    // finally splice the field: muValue = (muValue & 0xFF0003FF) | (idx << 10)
    // (`rlwinm r11,r11,0,22,7` clears bits [10..23], `or` with idx<<10).
    void PropEntityID::SetEntityIndex(u16 luEntityIndex)
    {
        AssertIsProp();
        CGS_ASSERT(luEntityIndex < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
        mEntityId.muValue = (mEntityId.muValue & ~KU_ENTITY_INDEX_MASK)
                          | (static_cast<u32>(luEntityIndex) << KU_ENTITY_INDEX_BASE);
    }

    // 0x822B78E8: operator CgsSceneManager::EntityId(). The asm reads the owner byte
    // (`lbz r11,0(r30); cmplwi r11,3`) and fires the owner tripwire when it is not 3
    // BEFORE copying, then copies the full packed word into the result EntityId
    // (`lwz r11,0(r30); stw r11,0(r31)`). The tripwire here is the AssertIsProp field
    // extraction (endian-independent owner read, == the asm's high-byte compare); the
    // copy is the whole-word muValue assignment. CgsSceneManager::EntityId is the
    // project-aliased EntityId, so the return type is EntityId.
    PropEntityID::operator EntityId() const
    {
        AssertIsProp();
        EntityId lResult;
        lResult.muValue = mEntityId.muValue;
        return lResult;
    }

    // 0x822B7A70: AssertIsProp; then assert index < 1<<10 (baked CgsEntityId.h:167
    // "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)"); finally splice the field:
    // muValue = (muValue & 0xFFFFFC00) | idx (`clrrwi r11,r11,10`, `or`).
    void PropEntityID::SetPartIndex(u32 luPartIndex)
    {
        AssertIsProp();
        CGS_ASSERT(luPartIndex < (1u << KU_NUM_BITS_FOR_PART_NUM),
                   "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)");
        mEntityId.muValue = (mEntityId.muValue & ~KU_PART_INDEX_MASK)
                          | (luPartIndex << KU_PART_INDEX_BASE);
    }
}
