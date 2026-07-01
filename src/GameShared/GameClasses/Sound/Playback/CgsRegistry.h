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

    // CgsRegistry.h (DWARF). Type-checked entity lookup by interned Name. FLAG
    // (DEFER): declared-only template forwarder -- the resolve-pass fixers look a
    // serialized-index Name up to recover its live entity; the X360 `bl
    // Registry::GetEntity<T>` body lives in its own Registry TU. Non-const Name&
    // per DWARF. Returns 0 when the name is not present.
    template <typename T>
    const T* GetEntity(Name& arName) const;
};

}
}

#endif // CGS_SOUND_PLAYBACK_CGSREGISTRY_H
