#pragma once

// BrnWorld::PropEntityID — a prop entity handle that packs an owner / entity index /
// part index into a single 32-bit EntityId word. Reconstructed from the DecFIGS DWARF
// (member name) + the X360 ARTIST spine (bit layout + accessor bodies, authoritative).
//
// BIT LAYOUT (X360 retail, asm-authoritative):
//   The asm for the out-of-line accessors fixes the packed layout as
//       owner       : bits [24..31]  (8 bits)   high byte; E_ENTITYTYPE_PROP == 3
//       entityIndex : bits [10..23]  (14 bits)
//       partIndex   : bits  [0..9]   (10 bits)
//   This is proven by:
//     GetEntityIndex @ 0x822B7B08  `extrwi r3,r11,14,8`  -> (muValue >> 10) & 0x3FFF
//     GetPartIndex   @ 0x822B7B68  `clrlwi r3,r11,22`     -> muValue & 0x3FF
//     SetEntityIndex @ 0x822B79D0  mask 0xFF0003FF | (idx<<10), idx < 0x4000 (14 bits)
//     SetPartIndex   @ 0x822B7A70  `clrrwi r11,r11,10`    & idx, idx < 0x400 (10 bits)
//   An earlier CgsEntityId.h revision used a 12/12 entity/part split; the shipped
//   X360 build uses 14/10. The shipped binary is the source of truth here, so the
//   masks/shifts below match the asm exactly (NOT the 12/12 layout).
//
// The owner-byte assert ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", baked
// ..\\..\\..\\SharedClasses\\Physics/Props/BrnPropEntityID.h:278) reads the high byte
// of the big-endian word and compares == 3. The bounds asserts on the setters are
// baked into CgsEntityId.h (lines 160 / 167) and reproduced as house CGS_ASSERTs.
#include "BrnCommonTypes.h"                               // EntityId { u32 muValue }
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // VolumeInstanceId { u64 muId }
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"         // VolumeId { u64 mId } (PropVolumeID home)

namespace BrnWorld
{
    // Prop entity owner tag. The X360 asm only ever compares the owner byte against
    // the literal 3, so only the prop value is needed here (minimal, non-forking).
    enum EEntityType
    {
        E_ENTITYTYPE_PROP = 3
    };

    struct PropEntityID
    {
        // --- packed-field geometry (X360-authoritative; see header banner) ---
        static const u32 KU_NUM_BITS_FOR_OWNER      = 8;
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;
        static const u32 KU_NUM_BITS_FOR_PART_NUM   = 10;

        static const u32 KU_OWNER_BASE        = 24;
        static const u32 KU_ENTITY_INDEX_BASE = 10;
        static const u32 KU_PART_INDEX_BASE   = 0;

        static const u32 KU_OWNER_MASK        = 0xFF000000u; // bits [24..31]
        static const u32 KU_ENTITY_INDEX_MASK = 0x00FFFC00u; // bits [10..23]
        static const u32 KU_PART_INDEX_MASK   = 0x000003FFu; // bits  [0..9]

        // Out-of-line accessors the X360 ARTIST build emitted (bodies in
        // BrnPropEntityID.cpp). Signatures follow the Hex-Rays prototypes reconciled
        // against the call-site asm (the const-qualified getters read the owner byte
        // then return the masked field; the setters read-modify-write muValue).
        PropEntityID() {}                       // trivial (X360-inlined)
        explicit PropEntityID(u32 luEntityId);  // 0x822B7888 — store word + AssertIsProp

        u32 GetEntityIndex() const;             // 0x822B7B08
        u32 GetPartIndex() const;               // 0x822B7B68
        u32 GetValue() const;                   // 0x822B7BC8

        void SetEntityIndex(u16 luEntityIndex); // 0x822B79D0
        void SetPartIndex(u32 luPartIndex);     // 0x822B7A70

        void AssertIsProp() const;              // inlined tripwire (owner byte == 3)

        // ADDITIVE GROW (FLAG): conversion to CgsSceneManager::EntityId.
        // 0x822B78E8 — BrnWorld::PropEntityID::operator class CgsSceneManager::EntityId().
        // The X360 body reads the owner byte (`lbz r11,0(r30)`), fires the owner tripwire
        // ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278) when it is
        // not 3, then copies the whole packed word into the returned EntityId
        // (`lwz r11,0(r30); stw r11,0(r31)` -> result.muValue = mEntityId.muValue). Called by
        // BrnWorld::PropCellManager::AddPropToScene and BrnWorld::PropZoneManager::UpdateInstance.
        // CgsSceneManager::EntityId is the same packed handle the project aliases as EntityId
        // (BrnCommonTypes.h struct EntityId { u32 muValue; }), so the operator yields it.
        operator EntityId() const;              // 0x822B78E8

        // ADDITIVE GROW (Array<PropEntityID,N> group): the explicit class instantiations
        // Array<BrnWorld::PropEntityID, 15> / <,30> (CgsArrayPropEntityID15.cpp /
        // CgsArrayPropEntityID30.cpp) force the generic Array<T,N> equality-based members
        // (FindFirstInstanceOf / Contains / CountInstancesOf / EraseInstancesOf) to
        // instantiate, which need an element operator==. The packed handle compares by its
        // single 32-bit word (the only state of the type), so equality is muValue equality.
        // Pure addition: no layout/sizeof/offset change (sizeof remains the one EntityId word).
        bool operator==(const PropEntityID& lrOther) const
        {
            return mEntityId.muValue == lrOther.mEntityId.muValue;
        }

        EntityId mEntityId;
    };

    // ---------------------------------------------------------------------------
    // FLAGGED ADDITIVE GROW (PropVolumeInstanceID group): the DecFIGS DWARF places
    // BrnWorld::PropVolumeInstanceID in this same header (BrnPropEntityID.h:170) and
    // the X360 accessor asm bakes its assert strings against this file (lines 455 /
    // 278), so its home is here alongside its sibling PropEntityID. Nothing above is
    // modified; this only adds the new type + a forward to its CgsVolumeInstanceId
    // include.
    //
    // BrnWorld::PropVolumeInstanceID — wraps a CgsSceneManager::VolumeInstanceId whose
    // 64-bit packed word (CgsVolumeInstanceId.h) is laid out as
    //     entityId    : bits [32..63]  (32 bits) — an embedded PropEntityID word
    //     reserved    : bits  [8..31]  (24 bits)
    //     volumeIndex : bits  [0..7]   ( 8 bits)
    // The X360 asm is authoritative for this layout:
    //   Set            @ 0x822B7CE0  `sldi r,entity,32; or volume&0xFF; std`
    //                                -> muId = (entityWord << 32) | (volume & 0xFF)
    //   GetEntityIndex @ 0x822B7F30  reads (muId>>32) then `extrwi r,w,14,8`
    //                                -> embedded PropEntityID::GetEntityIndex()
    //   SetEntityIndex @ 0x822B7D70  splices a PropEntityID::SetEntityIndex result back
    //   SetPartIndex   @ 0x822B7E50  splices a PropEntityID::SetPartIndex result back
    //   GetPropEntityID@ 0x822B8058  returns the high-dword word as a PropEntityID
    //   SetPropEntityId@ 0x822B7FE0  installs a PropEntityID into the high dword
    // Two owner tripwires fire in these bodies: the volume-level one bakes
    // "mVolumeInstanceId.GetEntityIDOwner() == E_ENTITYTYPE_PROP" (line 455) and the
    // entity-level one bakes "mEntityId.GetOwner() == E_ENTITYTYPE_PROP" (line 278),
    // both comparing the owner byte (top byte of the embedded entity word) against
    // E_ENTITYTYPE_PROP (== 3).
    struct PropVolumeInstanceID
    {
        // Position of the embedded EntityId word inside the 64-bit VolumeInstanceId
        // (matches CgsVolumeInstanceId.h KU_ENTITY_ID_START_INDEX == 32).
        static const u32 KU_ENTITY_ID_BASE        = 32;
        static const u64 KU_VOLUME_INDEX_MASK     = 0x00000000000000FFull; // bits [0..7]

        PropVolumeInstanceID() {}

        // 0x822B7CE0 — Set(propEntityId, volumeNumber).
        void Set(PropEntityID lPropEntityId, u8 luVolumeNumber);

        // 0x822B7FE0 — install a PropEntityID into the high dword (keep volume index).
        void SetPropEntityId(PropEntityID lPropEntityId);

        // 0x822B8058 — return the embedded PropEntityID (high-dword word).
        PropEntityID GetPropEntityID();

        // 0x822B7F30 — embedded PropEntityID::GetEntityIndex().
        u32 GetEntityIndex() const;

        // 0x822B7D70 — splice PropEntityID::SetEntityIndex back into the entity word.
        void SetEntityIndex(u16 luEntityIndex);

        // 0x822B7E50 — splice PropEntityID::SetPartIndex back into the entity word.
        void SetPartIndex(u32 luPartIndex);

        // Volume-level owner tripwire (baked line 455).
        void AssertIsProp() const;

        CgsSceneManager::VolumeInstanceId mVolumeInstanceId;
    };

    // ---------------------------------------------------------------------------
    // FLAGGED ADDITIVE GROW (BrnWorld group): the DecFIGS DWARF places
    // BrnWorld::PropVolumeID in this same header (BrnPropEntityID.h:71, members at
    // :116-:158) and the X360 Set asm bakes its assert strings against this file
    // (lines 387 / 311 / 312), so its home is here alongside its siblings
    // PropEntityID / PropVolumeInstanceID. Nothing above is modified; this only adds
    // the new type + the CgsVolumeId include (added at the top).
    //
    // BrnWorld::PropVolumeID — wraps a CgsSceneManager::VolumeId whose 64-bit packed
    // word carries an owner byte + a packed (type-id, volume-number) payload in its
    // low 32 bits. The X360 Set body (0x822B7C20) is authoritative on the layout:
    //     owner       : bits [16..23]  (8 bits)  — VolumeId::GetOwner(); E_ENTITYTYPE_PROP == 3
    //     typeId      : bits  [6..15]  (10 bits) — KU_BITS_FOR_TYPE_ID == 10
    //     volumeNumber: bits  [0..5]   ( 6 bits) — KU_BITS_FOR_VOLUME_NUMBER == 6
    //   Set @ 0x822B7C20  builds  mId = (E_ENTITYTYPE_PROP << 16)
    //                                 | ((luPropType << KU_TYPE_ID_OFFSET) & KU_TYPE_ID_MASK)
    //                                 | (luVolumeNumber & KU_VOLUME_NUMBER_MASK)
    //     with three tripwires (asm order):
    //       GetOwner()==E_ENTITYTYPE_PROP            (line 387) — reads the pre-existing owner byte
    //       (luPropType >> KU_BITS_FOR_TYPE_ID)==0   (line 311)
    //       (luVolumeNumber >> KU_BITS_FOR_VOLUME_NUMBER)==0 (line 312)
    // The owner tripwire reads the OWNER BYTE of the word *before* the new value is
    // stored (`ld; srdi 16; clrlwi 24; cmplwi 3` precedes the `std`), so the asserted
    // owner is whatever Construct() seeded (E_ENTITYTYPE_PROP). Field constants/masks
    // are the DecFIGS DWARF values (BrnPropEntityID.h:147-155).
    struct PropVolumeID
    {
        // --- packed-field geometry (DWARF BrnPropEntityID.h:147-155; X360-consistent) ---
        static const u32 KU_BITS_FOR_VOLUME_NUMBER = 6;       // :147
        static const u32 KU_BITS_FOR_TYPE_ID       = 10;      // :148
        static const u32 KU_VOLUME_NUMBER_OFFSET   = 0;       // :150
        static const u32 KU_TYPE_ID_OFFSET         = 6;       // :151
        static const u32 KU_VOLUME_NUMBER_MASK     = 0x0000003Fu; // 63   :154 bits [0..5]
        static const u32 KU_TYPE_ID_MASK           = 0x0000FFC0u; // 65472 :155 bits [6..15]

        PropVolumeID() {}                       // BrnPropEntityID.h:141 (trivial)
        explicit PropVolumeID(CgsSceneManager::VolumeId lVolumeId) // :142
            : mVolumeId(lVolumeId) {}

        // 0x822B7C20 — pack (propType, volumeNumber) over the existing owner byte.
        void Set(u16 luPropType, u8 luVolumeNumber);

        // Owner tripwire (baked line 387).
        void AssertIsProp() const;

        CgsSceneManager::VolumeId mVolumeId;
    };
}
