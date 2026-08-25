#ifndef CGS_SOUND_PLAYBACK_CGSREGISTRY_H
#define CGS_SOUND_PLAYBACK_CGSREGISTRY_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"

// CgsSound::Playback::Registry - a fixed-capacity, open-addressed hash table of
// Entity pointers laid out over a single externally-supplied data blob (the
// canonical DWARF home is CgsRegistry.h:55). This file models the slice the
// boot-trace Registry TU needs: the RegistrySpec, a minimal Entity, and the
// Registry's data layout + the (RegistrySpec) ctor and AddEntity.
namespace CgsSound
{
namespace Playback
{

// CgsDataStructures.h:106. One registry entry: an interned name plus an interned
// type-name. FLAG: MINIMAL home for the Registry TU only -- the full Entity
// (CgsDataStructures.h: Entity(Name)/Entity(Name,Name)/operator==/GetName/SetName/
// GetTypeName/EndianSwap and its ContentClass/ContentType/... subclasses) is
// DEFERRED to its own CgsDataStructures.* TU. Modelled member-for-member by NAME:
// mName @ +0, mTypeName @ +4 on X360 (each a 4-byte Name). Registry::AddEntity
// reads mName.GetValue() (the X360 `*a2`) and tests mTypeName.GetValue() != 0
// (the X360 `a2[1]`), so only those two members and their order are load-bearing.
struct Entity
{
    Name mName;      // CgsDataStructures.h:201  (+0) interned entry name
    Name mTypeName;  // CgsDataStructures.h:202  (+4) interned entry type-name

    // CgsDataStructures.h (DWARF, part of the deferred Entity surface above). The
    // interned entry name accessor. FLAG (additive home-grow): the SpliceBankStatistics
    // TTY dump reads a ContentSpec's name via GetName() (X360 SpliceBankStatistics::
    // DumpToTty inlines the `*(spec + 0)` mName read). Shape-only trivial getter.
    const Name& GetName() const { return mName; }

    // CgsDataStructures.h:178/187/198 (DWARF). The serialization member-pointer
    // fixup helpers, static templates on Entity. A serialized entity stores every
    // referenced entity pointer as a low-bit-tagged interned-Name index; these
    // convert between that index form and a live pointer:
    //   Resolve   -- index -> live pointer (look the Name up in the registry)
    //   Unresolve -- live pointer -> index (re-tag)
    //   Relocate  -- rebase a live pointer between two registry data blobs
    // The fixer TUs (EntityFixer<ContentType>::Do*, EntityFixer<SlotSchema>::Do*)
    // call these as Entity::ResolveMemberPointer<ContentClass>(&slot, registry),
    // etc. FLAG (DEFER): declared-only -- bodied in their own Entity TU.
    template <typename T>
    static void ResolveMemberPointer(const T** appMember, const class Registry& arRegistry);
    template <typename T>
    static void UnresolveMemberPointer(const T** appMember);
    template <typename T>
    static void RelocateMemberPointer(const T** appMember, u8* apu8Base,
                                      const class Registry& arFrom,
                                      const class Registry& arTo);
};

// CgsDataStructures.h:178/187/198 (DWARF). Namespace-scope free-function template
// forms of the member-pointer fixup helpers. Several fixers (EntityFixer<VoiceSpec>,
// EntityFixer<VoiceSchema>, EntityFixer<FeatureSchema>) call the UNqualified free
// templates rather than the Entity:: statics -- the X360 mangling attests both
// exist. FLAG (DEFER): declared-only, bodied in their own TU.
template <typename T>
void ResolveMemberPointer(const T** appMember, const Registry& arRegistry);
template <typename T>
void UnresolveMemberPointer(const T** appMember);
template <typename T>
void RelocateMemberPointer(const T** appMember, u8* apu8Base,
                           const Registry& arFrom, const Registry& arTo);

// CgsRegistry.h:42. The sizing descriptor passed to Registry construction.
struct RegistrySpec
{
    u32    mu32EntityCount;     // CgsRegistry.h:43  hash-table slot count
    size_t muDataSize;          // CgsRegistry.h:44  entity data-blob byte size
    size_t muStringTableSize;   // CgsRegistry.h:45  string-table byte size

    // CgsRegistry.h:47. FLAG: ctor not in this TU's set; defaulted for shape.
    RegistrySpec() : mu32EntityCount(0), muDataSize(0), muStringTableSize(0) {}
};

// CgsRegistry.h:55. Open-addressed Entity-pointer hash table.
//
// HOST-WIDTH FLAG: on the X360 every member below is 4 bytes and the slot array
// begins immediately after muNameHashMask at +0x1C (the asm `addi r9, r23, 0x1C`
// and word indices a1[0..6], slots at a1[7+]). On the 64-bit host the pointer
// members widen to 8 bytes, so those absolute X360 offsets do NOT hold -- members
// are pinned BY NAME and SEQUENCE only, never by static_assert on absolute offset.
struct Registry
{
private:
    u32        mu32EntityCount;     // CgsRegistry.h:232  (a1[0]) live entry count
    u32        mu32EntityCapacity;  // CgsRegistry.h:233  (a1[1]) slot-array length
    size_t     muDataSize;          // CgsRegistry.h:234  (a1[2]) entity-blob size
    u8*        mpu8Data;            // CgsRegistry.h:235  (a1[3]) entity-blob start
    size_t     muStringTableSize;   // CgsRegistry.h:236  (a1[4]) string-table size
    char*      mpcStringTable;      // CgsRegistry.h:237  (a1[5]) string-table start
    uintptr_t  muNameHashMask;      // CgsRegistry.h:239  (a1[6]) capacity-1 mask

    // CgsRegistry.h. The open-addressing slot array. On X360 it is the words at
    // a1[7..] (in-place, contiguous with the header in the supplied blob). Modelled
    // as a trailing 1-length array so GetFirstEntity()/the probe loops can index it
    // BY NAME; the real length is mu32EntityCapacity. FLAG (host-width: the X360
    // header occupies 0x1C bytes before this; host header is wider, but AddEntity
    // never assumes the blob byte size, so the named indexing stays faithful).
    const Entity* mapEntity[1];

    // CgsRegistry.h:215. First slot of the open-addressing array (named accessor
    // replacing the X360 `a1 + 7`). FLAG: shape helper, not a recon'd TU.
    const Entity** GetFirstEntity() { return mapEntity; }

public:
    // CgsRegistry.h:73. Build the table in place over a supplied blob.
    explicit Registry(const RegistrySpec& lSpec);

    // CgsRegistry.h:97. Insert an Entity pointer by open addressing. Returns true
    // on success, false if the table is full. Bodied store-for-store from the
    // X360 AddEntity @ 0x82692DA0.
    bool AddEntity(const Entity& lEntity);

    // CgsRegistry.h (DWARF). Type-checked entity lookup by interned Name. The X360
    // `bl Registry::GetEntity<T>` body is reconstructed out-of-line below (ONE shared
    // template body; the explicit instantiations live in CgsRegistry.cpp). Non-const
    // Name& per DWARF. Returns 0 when the name is not present.
    template <typename T>
    const T* GetEntity(Name& arName) const;

    // FLAG (additive accessor exposure, 2026-08-25 wave 4): named access for the
    // three serialised-size words RegistryResourceType::GetSerialisedResourceDescriptor
    // @0x82667DE8 reads (the X360 a3[1]/a3[2]/a3[4] word reads -- wrong on the host
    // where the pointer members widen, hence by name).
    u32    GetEntityCapacity() const  { return mu32EntityCapacity; }
    size_t GetDataSize() const        { return muDataSize; }
    size_t GetStringTableSize() const { return muStringTableSize; }

    // DWARF CgsRegistry.h:137 (header-inline on console). Debug-TTY dump of the
    // table. FLAG (DEFER): declared-only -- bodied with the Registry slices
    // (caller: Module::DumpRegistries @0x82694188).
    void Dump();
};

// =============================================================================
// Registry::GetEntity<T>  (X360 family @ 0x8268E388 .. 0x826906A8; representative
// GetEntity<ContentClass> @ 0x8268FFA0). ONE shared out-of-line template body --
// every instantiation is identical apart from T::SK_TYPE_NAME (the X360 per-type
// static type-name word `dword_XXXX` loaded once before the probe). Reconstructed
// store-for-store from the asm:
//
//   lppSlot   = this + 0x1C            (the open-addressing slot array, GetFirstEntity)
//   maskedHash = (name >> 1) & mask    ((*a2 >> 1) & *(this+0x18))
//   assert maskedHash < capacity       (CgsRegistry.h:432 "Masked hash ...")
//   typeName  = T::SK_TYPE_NAME        (the per-T static word read into r7)
//   -- forward probe [maskedHash, capacity): first empty slot => miss (return 0);
//      first slot whose mName == name AND mTypeName == typeName => hit (return it);
//      running past capacity falls through to the wrap span.
//   -- wrap probe [0, maskedHash): same empty-slot/match test; exhausting => miss.
//
// The matched slot is returned as the raw Entity* re-typed to T* -- the X360 does
// `result = mapEntity[i]` with NO pointer adjustment (Entity is the sole base at
// offset 0 for every element type), so reinterpret_cast is the faithful form and
// also tolerates element types modelled as standalone (non-Entity-derived) structs.
// =============================================================================
template <typename T>
const T* Registry::GetEntity(Name& arName) const
{
    // this + 0x1C == the slot array (GetFirstEntity()). Const-view of mapEntity.
    const Entity* const* lppSlot = mapEntity;

    // Masked hash = (name >> 1) & (capacity - 1). X360: (*a2 >> 1) & *(this+0x18).
    const uintptr_t luMaskedHash = (arName.GetValue() >> 1) & muNameHashMask;
    CGS_ASSERT(luMaskedHash < mu32EntityCapacity, "Masked hash has gone out of range\n");

    // The per-T interned type-name the matching slot must also carry (X360 dword_XXXX,
    // loaded once into r7 before the probe).
    const uintptr_t luTypeName = T::SK_TYPE_NAME.GetValue();
    const uintptr_t luName     = arName.GetValue();

    // Forward probe span [maskedHash, capacity). X360: entered only when
    // maskedHash < capacity (else it jumps straight to the wrap span).
    if (luMaskedHash < mu32EntityCapacity)
    {
        for (u32 luI = static_cast<u32>(luMaskedHash); luI < mu32EntityCapacity; ++luI)
        {
            const Entity* lpEntity = lppSlot[luI];
            if (lpEntity == 0)
            {
                return 0;   // empty slot -> not present
            }
            if (luName == lpEntity->mName.GetValue() &&
                luTypeName == lpEntity->mTypeName.GetValue())
            {
                return reinterpret_cast<const T*>(lpEntity);
            }
        }
    }

    // Wrap probe span [0, maskedHash). X360 LABEL_9: only entered when maskedHash != 0.
    if (luMaskedHash != 0)
    {
        for (u32 luI = 0; luI < static_cast<u32>(luMaskedHash); ++luI)
        {
            const Entity* lpEntity = lppSlot[luI];
            if (lpEntity == 0)
            {
                return 0;
            }
            if (luName == lpEntity->mName.GetValue() &&
                luTypeName == lpEntity->mTypeName.GetValue())
            {
                return reinterpret_cast<const T*>(lpEntity);
            }
        }
    }

    return 0;
}

}
}

#endif // CGS_SOUND_PLAYBACK_CGSREGISTRY_H
