#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

// Out-of-line bodies for BrnWorld::PropVolumeInstanceID — the prop-volume-instance id
// wrapper that the X360 ARTIST build emitted as its own accessors. Each body wraps the
// embedded PropEntityID word stored in the HIGH 32 bits of the 64-bit VolumeInstanceId
// (CgsVolumeInstanceId.h: KU_ENTITY_ID_START_INDEX == 32). Store order, bit-fields and
// the per-tripwire assert strings are reproduced exactly from the X360 asm (addresses on
// each function; layout banner in BrnPropEntityID.h).
//
// HOST-vs-X360 NOTE: the asm reads the owner via `ld; srdi 32; srwi 24` (i.e. the top
// byte of the embedded entity word, which is the high dword of the big-endian 64-bit
// word). On a little-endian host the raw byte read would alias differently, so the owner
// comparison is expressed as the field extraction `(muId >> 56)` — semantically the owner
// byte of the embedded entity, endian-independent. The compared constant is
// E_ENTITYTYPE_PROP (== 3), exactly as the asm.

namespace BrnWorld
{
    // Volume-level owner tripwire: baked
    // "mVolumeInstanceId.GetEntityIDOwner() == E_ENTITYTYPE_PROP"
    // (..\\..\\..\\SharedClasses\\Physics/Props/BrnPropEntityID.h:455).
    void PropVolumeInstanceID::AssertIsProp() const
    {
        CGS_ASSERT(((mVolumeInstanceId.muId >> 56) & 0xFFu) == E_ENTITYTYPE_PROP,
                   "mVolumeInstanceId.GetEntityIDOwner() == E_ENTITYTYPE_PROP");
    }

    // 0x822B7CE0: assert the existing volume owner == 3, assert the supplied
    // PropEntityID owner == 3, then pack
    //     muId = (entityWord << 32) | (volumeNumber & 0xFF)
    // (`sldi r11,r29,32; clrldi r10,r28,56; or; std`). The whole 64-bit word is
    // rewritten (reserved bits cleared to 0).
    void PropVolumeInstanceID::Set(PropEntityID lPropEntityId, u8 luVolumeNumber)
    {
        AssertIsProp();
        CGS_ASSERT((lPropEntityId.mEntityId.muValue >> PropEntityID::KU_OWNER_BASE) == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
        mVolumeInstanceId.muId =
            (static_cast<u64>(lPropEntityId.mEntityId.muValue) << KU_ENTITY_ID_BASE)
            | (static_cast<u64>(luVolumeNumber) & KU_VOLUME_INDEX_MASK);
    }

    // 0x822B7FE0: assert the supplied PropEntityID owner == 3, then install the entity
    // word into the high dword while preserving the low dword (volume index). The asm
    // clears the high dword (`stw 0,0(r31)`), reloads, then ORs in (entityWord << 32):
    //     muId = (muId & 0xFFFFFFFF) | (entityWord << 32).
    void PropVolumeInstanceID::SetPropEntityId(PropEntityID lPropEntityId)
    {
        CGS_ASSERT((lPropEntityId.mEntityId.muValue >> PropEntityID::KU_OWNER_BASE) == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
        mVolumeInstanceId.muId =
            (mVolumeInstanceId.muId & 0xFFFFFFFFull)
            | (static_cast<u64>(lPropEntityId.mEntityId.muValue) << KU_ENTITY_ID_BASE);
    }

    // 0x822B8058: assert the volume owner == 3, copy the embedded entity word out of the
    // high dword into the result PropEntityID, assert that entity owner == 3, return it.
    PropEntityID PropVolumeInstanceID::GetPropEntityID()
    {
        AssertIsProp();
        PropEntityID lResult;
        lResult.mEntityId.muValue =
            static_cast<u32>(mVolumeInstanceId.muId >> KU_ENTITY_ID_BASE);
        CGS_ASSERT((lResult.mEntityId.muValue >> PropEntityID::KU_OWNER_BASE) == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
        return lResult;
    }

    // 0x822B7F30: assert the volume owner == 3, take the embedded entity word, assert its
    // owner == 3, then `extrwi r3,r31,14,8` -> (entityWord >> 10) & 0x3FFF, i.e. the
    // embedded PropEntityID::GetEntityIndex().
    u32 PropVolumeInstanceID::GetEntityIndex() const
    {
        AssertIsProp();
        const u32 luEntityWord =
            static_cast<u32>(mVolumeInstanceId.muId >> KU_ENTITY_ID_BASE);
        CGS_ASSERT((luEntityWord >> PropEntityID::KU_OWNER_BASE) == E_ENTITYTYPE_PROP,
                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");
        return (luEntityWord & PropEntityID::KU_ENTITY_INDEX_MASK)
               >> PropEntityID::KU_ENTITY_INDEX_BASE;
    }

    // 0x822B7D70: assert the volume owner == 3; copy the embedded entity word into a
    // scratch PropEntityID; assert its owner == 3; call PropEntityID::SetEntityIndex on
    // the scratch; assert the result owner == 3; then re-install the modified entity word
    // into the high dword, preserving the low dword:
    //     muId = (muId & 0xFFFFFFFF) | (entityWord << 32).
    void PropVolumeInstanceID::SetEntityIndex(u16 luEntityIndex)
    {
        AssertIsProp();
        PropEntityID lEntity;
        lEntity.mEntityId.muValue =
            static_cast<u32>(mVolumeInstanceId.muId >> KU_ENTITY_ID_BASE);
        lEntity.SetEntityIndex(luEntityIndex); // runs its own entity-owner assert
        mVolumeInstanceId.muId =
            (mVolumeInstanceId.muId & 0xFFFFFFFFull)
            | (static_cast<u64>(lEntity.mEntityId.muValue) << KU_ENTITY_ID_BASE);
    }

    // 0x822B7E50: identical shape to SetEntityIndex but routed through
    // PropEntityID::SetPartIndex.
    void PropVolumeInstanceID::SetPartIndex(u32 luPartIndex)
    {
        AssertIsProp();
        PropEntityID lEntity;
        lEntity.mEntityId.muValue =
            static_cast<u32>(mVolumeInstanceId.muId >> KU_ENTITY_ID_BASE);
        lEntity.SetPartIndex(luPartIndex); // runs its own entity-owner assert
        mVolumeInstanceId.muId =
            (mVolumeInstanceId.muId & 0xFFFFFFFFull)
            | (static_cast<u64>(lEntity.mEntityId.muValue) << KU_ENTITY_ID_BASE);
    }
}
