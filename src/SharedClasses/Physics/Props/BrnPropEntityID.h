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
// BrnWorld::EEntityTypeID (E_ENTITYTYPE_PROP == 3) -- the committed home.
// ⚠️ DE-FORKED 2026-08-03 (task #123). This header used to declare its own one-value
// `enum BrnWorld::EEntityType { E_ENTITYTYPE_PROP = 3 };`, described in-comment as
// "minimal, non-forking". It WAS a fork: BrnEntityTypes.h already owns the DWARF-attested
// `BrnWorld::EEntityTypeID` with the same enumerator, and unscoped-enum enumerators land in
// the enclosing namespace -- so any TU that saw both headers failed with
// C2365 "'BrnWorld::E_ENTITYTYPE_PROP': redefinition; previous definition was 'enumerator'".
// Nothing had tripped it only because no TU had yet embedded a ContactSpyData (-> BrnContactSpyQueue.h
// -> BrnEntityTypes.h) alongside a PropManager (-> here); PhysicsModule is the first, so it
// surfaced here. BrnEntityTypes.h has no #includes at all, so this costs no cascade and no cycle.
#include "GameSource/World/BrnEntityTypes.h"

namespace BrnWorld
{
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

        // ADDITIVE GROW: the owner byte (bits [24..31]) the owner tripwires compare against
        // E_ENTITYTYPE_PROP. The X360 inlines this at every use site as `srwi r,word,24`
        // (word >> 24); PropZoneManager::GetRespawnType calls lId.GetOwner() explicitly.
        u32 GetOwner() const { return mEntityId.muValue >> KU_OWNER_BASE; }

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

        // ⭐ SEEDS THE OWNER BYTE 2026-08-12 (prop-BOOT wave, agent B8) -- the direct twin of
        // PropVolumeID's seed above, and the fix for 980 of the boot's asserts. The embedded
        // PropEntityID lives in the HIGH dword (KU_ENTITY_ID_BASE == 32) and its owner byte is
        // at bits [24..31] of that word, i.e. bit 56 of the packed 64-bit id.
        // X360 WITNESSES (the compiler's fold of this constructor, emitted at the point each
        // local is declared, immediately before the first SetEntityIndex):
        //     PropZoneManager::UnloadZone     @0x82303790 -> 0x82303974  li r10,3
        //                                                    0x8230397C  sldi r10, r10, 56
        //                                                    0x8230398C  std  r10, var_B8(r1)
        //     PropZoneManager::UpdateInstance @0x822F0920 -> 0x822F1428/0x822F17CC, same pair
        // PropZoneManager::LoadProp already open-coded the same seed by hand (see its comment,
        // which calls it "the compiler's fold of the default-constructed prop handle") --
        // every OTHER declaration site was missing it, so UnloadZone / RemovePropFromScene /
        // RemovePropFromSim / the PropCellManager sites all tripped
        // "mVolumeInstanceId.GetEntityIDOwner() == E_ENTITYTYPE_PROP" and the nested
        // "mEntityId.GetOwner() == E_ENTITYTYPE_PROP" once per prop per unload.
        PropVolumeInstanceID()
        {
            mVolumeInstanceId.muId = static_cast<u64>( E_ENTITYTYPE_PROP )
                                     << ( KU_ENTITY_ID_BASE + PropEntityID::KU_OWNER_BASE );
        }

        // 0x822B7CE0 — Set(propEntityId, volumeNumber).
        void Set(PropEntityID lPropEntityId, u8 luVolumeNumber);

        // ---- the VOLUME-NUMBER splice (the wave-Q4 unpark) ---------------------------------
        // ADDED 2026-08-18 (wave Q4, collision seam). This is the accessor the four parked
        // PropCellManager contact-generation bodies were blocked on. DWARF names it
        // SetVolumeNumber, not SetVolumeIndex (BrnPropEntityID.h:181 / :199):
        //     :181  void     SetVolumeNumber(uint8_t);
        //     :199  uint32_t GetVolumeNumber();          <- non-const in the DWARF, kept so
        // (the WRAPPED CgsSceneManager::VolumeInstanceId spells the same field SetVolumeIndex
        //  / GetVolumeIndex, DWARF CgsVolumeInstanceId.h:92 / :115 -- these two forward to it,
        //  exactly as every other member of this wrapper forwards.)
        //
        // HEADER INLINES: neither has an out-of-line symbol anywhere in the X360 export set
        // (progress/identity.json lists seven PropVolumeInstanceID rows -- Set, SetEntityIndex,
        // SetPartIndex, GetEntityIndex, SetPropEntityId, GetPropEntityID and the
        // operator VolumeInstanceId -- and no *VolumeNumber row), so no new link symbol.
        //
        // X360 ATTESTATION -- the splice is folded at all four call sites, as the pair
        //     clrrdi rA, id, 8        ; muId & ~0xFF   (clear the low 8 bits, keep [8..63])
        //     or     id, rA, index    ; | (volumeNumber & 0xFF)
        // at 0x822DF8A0/0x822DF8AC (AddPropToContactGeneration),
        //    0x822DFB40/0x822DFB44 (AddPropPartsToContactGeneration),
        //    0x822C63A0/0x822C63A4 (RemovePropFromContactGeneration),
        //    0x822C64B4/0x822C64B8 (RemovePropPartsFromContactGeneration).
        // In every case the index register was just narrowed with `clrlwi rN,rN,24`, i.e. the
        // argument really is a byte -- matching the DWARF's uint8_t.
        void SetVolumeNumber(u8 lu8VolumeNumber)
        {
            mVolumeInstanceId.SetVolumeIndex(lu8VolumeNumber);
        }

        u32 GetVolumeNumber()
        {
            return mVolumeInstanceId.GetVolumeIndex();
        }

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

        // 0x822B7958 — operator class CgsSceneManager::VolumeInstanceId().
        // Volume-level owner tripwire (line 455) then whole-word copy of the packed
        // 64-bit muId into the returned VolumeInstanceId. Called by
        // BrnWorld::PropZoneManager::UpdateInstance. Direct analog of
        // PropEntityID::operator EntityId() @0x822B78E8.
        operator CgsSceneManager::VolumeInstanceId() const;

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

        // ⭐ SEEDS THE OWNER BYTE 2026-08-12 (prop-BOOT wave, agent B8). This was `{}`,
        // i.e. an uninitialised 64-bit word, and PropVolumeID::Set / AssertIsProp test the
        // owner byte that is ALREADY THERE (Set's tripwire runs before the new word is
        // stored) -- so the first Set on any default-constructed handle fired
        // "mVolumeId.GetOwner() == E_ENTITYTYPE_PROP" on every boot.
        // X360 WITNESS: PropEntityModule::InitializePropPhysicsData @0x822DA840 emits, at
        // the point the local is declared (0x822DA8F4 / 0x822DA908),
        //     lis r11, 3                  ; r11 = 3 << 16 == E_ENTITYTYPE_PROP at bits[16..23]
        //     std r11, 0x1A0+var_138(r1)  ; var_138 IS that local
        // -- which is the compiler's fold of exactly this constructor. A PropVolumeID can
        // only ever have owner E_ENTITYTYPE_PROP (that is what the type MEANS, and what its
        // three AssertIsProp tripwires enforce), so the seed belongs here rather than being
        // re-open-coded at every declaration site.
        PropVolumeID()                          // BrnPropEntityID.h:141
            : mVolumeId( static_cast<u64>( E_ENTITYTYPE_PROP )
                         << CgsSceneManager::VolumeId::KU_OWNER_BASE ) {}
        explicit PropVolumeID(CgsSceneManager::VolumeId lVolumeId) // :142
            : mVolumeId(lVolumeId) {}

        // 0x822B7C20 — pack (propType, volumeNumber) over the existing owner byte.
        void Set(u16 luPropType, u8 luVolumeNumber);

        // Owner tripwire (baked line 387).
        void AssertIsProp() const;

        // ADDED 2026-08-18 (wave Q4, collision seam). DWARF BrnPropEntityID.h:143 --
        //     VolumeId operator CgsSceneManager::VolumeId() const;
        // the exact sibling of PropVolumeInstanceID::operator VolumeInstanceId() @0x822B7958,
        // and the reason the "mVolumeId.GetOwner() == E_ENTITYTYPE_PROP" tripwire (line 387)
        // fires at the AddVolumeInstance CALL SITE rather than inside the producer: see
        // AddPropToContactGeneration 0x822DF8A8-0x822DF8D4 (`srdi r10,volId,16; clrlwi 24;
        // cmplwi 3; beq` then the assert block with `li r5, 0x183` == 387), immediately before
        // the `bl InSceneUpdateInterface::AddVolumeInstance` at 0x822DF910. The same block
        // appears at 0x822DFB50-0x822DFB74 in AddPropPartsToContactGeneration.
        // HEADER INLINE: no out-of-line symbol for it in the export set (identity.json's only
        // PropVolumeID row is Set @0x822B7C20), so no new link symbol.
        operator CgsSceneManager::VolumeId() const
        {
            AssertIsProp();
            return mVolumeId;
        }

        CgsSceneManager::VolumeId mVolumeId;
    };
}
