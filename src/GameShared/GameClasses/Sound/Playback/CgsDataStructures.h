#ifndef CGS_SOUND_PLAYBACK_CGSDATASTRUCTURES_H
#define CGS_SOUND_PLAYBACK_CGSDATASTRUCTURES_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"

// CgsSound::Playback::IEntityFixer -- the base of the sound-playback "entity
// fixer" type-handler hierarchy (canonical DWARF home CgsDataStructures.h:210).
//
// Every concrete fixer is an EntityFixer<T> subclass (AemsVoiceCsisClass,
// ContentClass, ContentSpec, VoiceSpec, VoiceSchema, FeatureSchema, SlotSchema,
// ParameterSchema, ContentType, ...). Each instance self-registers by pushing
// itself onto a single static intrusive singly-linked list (spHead, chained
// through mpNext) so the Registry can look one up by its type Name. The virtual
// interface (DoGetTypeName + Do{Unresolve,Resolve,Relocate,FixUp,FixDown}) is
// what makes a fixer polymorphic over the entity payload it serialises.
//
// FLAG: MINIMAL home for the boot-trace GetFixer TU only. Modelled member-for-
// member BY NAME: the vtable pointer (implicit), mpNext @ +4, and the static list
// head spHead. The full non-virtual surface (GetTypeName/Unresolve/Resolve/
// Relocate/FixUp/FixDown forwarders, the ctor that pushes onto spHead, the dtor
// that unlinks, and every EntityFixer<T> subclass + its SK_TYPE_NAME) is DEFERRED
// to its own CgsDataStructures.* TU(s); add only when a TU needs it. The virtuals
// are declared (pure) so slot 0 == DoGetTypeName matches the X360 vtable call in
// GetFixer; their bodies live with the concrete subclasses, not here.

namespace CgsSound
{
namespace Playback
{

// CgsDataStructures.h:210
struct IEntityFixer
{
public:
    // CgsDataStructures.h:301. FLAG: ctor not in this TU's set (it pushes onto
    // spHead). Defaulted here for shape so the type is constructible in the embed
    // check; the real push-onto-spHead ctor is DEFERRED to the CgsDataStructures
    // TU. Declared protected-style intent but kept public for the placeholder.
    IEntityFixer() : mpNext(0) {}
    virtual ~IEntityFixer() {}

    // CgsDataStructures.h:310. Walk the static spHead list and return the first
    // registered fixer whose type Name equals aName, or null. STATIC member (the
    // X360 reads the global spHead, never a `this`). Bodied store-for-store from
    // the X360 GetFixer @ 0x826809B0.
    static const IEntityFixer* GetFixer(Name aName);

protected:
    // ----- virtual interface (DWARF order; slot 0 == DoGetTypeName) -----
    // Slot 0 is the one GetFixer calls: it returns this fixer's type Name (by
    // value, via the X360 hidden return-pointer in r3). FLAG: pure here -- bodies
    // live with the concrete EntityFixer<T> subclasses (DEFERRED).
    virtual Name DoGetTypeName() const = 0;                 // CgsDataStructures.h:264
    virtual void DoUnresolve(Entity& arEntity) const = 0;   // :268
    virtual void DoResolve(Entity& arEntity,
                           const Registry& arRegistry) const = 0;        // :273
    virtual void DoRelocate(Entity& arEntity, u8* apu8Base,
                            const Registry& arFrom,
                            const Registry& arTo) const = 0;             // :284
    virtual void DoFixUp(Entity& arEntity) const = 0;       // :288
    virtual void DoFixDown(Entity& arEntity) const = 0;     // :292

private:
    // CgsDataStructures.h:295. Next fixer in the static registration list (+4 on
    // X360, read as `*(v2 + 4)` in GetFixer). Const-pointer to match the DWARF.
    const IEntityFixer* mpNext;

    // CgsDataStructures.h:296 / CgsDataStructures.cpp:38. The single static head of
    // the intrusive registration list (the X360 global dword_82FFB9DC). Defined in
    // CgsDataStructures.cpp. FLAG: the ctor/dtor that maintain it are DEFERRED.
    static const IEntityFixer* spHead;
};

// ============================================================================
// ADDITIVE HOME-GROW (Sound group): CgsSound::Playback::FeatureSchema +
// CgsSound::Playback::VoiceSchema -- the runtime "voice schema" entity and its
// per-feature sub-schema, both Entity subclasses (DWARF CgsDataStructures.h:887
// and :1233). Added for the VoiceSchema TU (ctor @ 0x82691E98 + SetFeatureSchema
// @ 0x82692058). These compose with the committed Entity (CgsRegistry.h) and Name
// (CgsCommon.h) sibling homes -- NOT forked. The existing IEntityFixer surface
// above is untouched.
//
// LAYOUT (X360, all words 4 bytes; matches the ctor/SetFeatureSchema asm):
//   Entity base:  mName @ +0, mTypeName @ +4   (CgsRegistry.h Entity)
//   VoiceSchema adds:
//     +0x8  mu32FeatureSchemaCount   (ctor: a1[2] = count arg)
//     +0xC  mu32SlotCount            (ctor 0; SetFeatureSchema += feature SlotSchemaCount)
//     +0x10 mu32ParameterCount       (ctor 0; SetFeatureSchema += feature ParameterSchemaCount)
//     +0x14 mu32OutputParamCount     (ctor 0; SetFeatureSchema += feature OutputParamCount)
//     +0x18 mapFeatureSchema[]       (flexible array of const FeatureSchema*, a1+6 dwords)
//   FeatureSchema adds (only the 3 counts this TU reads are modelled BY NAME):
//     +0x8  mu32ParameterSchemaCount (SetFeatureSchema reads *(schema+8))
//     +0xC  mu32SlotSchemaCount      (SetFeatureSchema reads *(schema+12))
//     +0x10 mu32OutputParamCount     (SetFeatureSchema reads *(schema+16))
// HOST-WIDTH FLAG: the flexible FeatureSchema* array widens on a 64-bit host;
// members are pinned BY NAME and SEQUENCE only -- absolute offsets not asserted.
// ============================================================================

// CgsDataStructures.h:887. FLAG: MINIMAL home-grow -- only the three allocation-
// accounting counts that the VoiceSchema TU reads are modelled BY NAME; the full
// FeatureSchema surface (GetAllocationSize/ctor/Get/SetParameterSchema/
// Get/SetSlotSchema/... and its own flexible ParameterSchema*/SlotSchema* arrays)
// is DEFERRED to the FeatureSchema TU. The leading three counts match the DWARF
// member order (CgsDataStructures.h:1075-1077) and the +8/+0xC/+0x10 asm reads.
// Forward declarations for the FeatureSchema flexible-array element types. Full
// homes below (ParameterSchema/SlotSchema are Entity subclasses).
struct ParameterSchema;
struct SlotSchema;

struct FeatureSchema : public Entity
{
    // CgsDataStructures.h (DWARF). This fixer/entity's interned type-name -- the
    // fixer hooks compare a fixed entity's mTypeName against it. DECLARED for the
    // fixer TUs; the interned-Name DEFINITION lives with the registration TU
    // (DEFERRED). FLAG.
    static const Name SK_TYPE_NAME;

    u32 mu32ParameterSchemaCount;   // CgsDataStructures.h:1075  (+0x8)
    u32 mu32SlotSchemaCount;        // CgsDataStructures.h:1076  (+0xC)
    u32 mu32OutputParamCount;       // CgsDataStructures.h:1077  (+0x10)

    u32 GetParameterSchemaCount() const { return mu32ParameterSchemaCount; }
    u32 GetSlotSchemaCount() const      { return mu32SlotSchemaCount; }
    u32 GetOutputParamCount() const     { return mu32OutputParamCount; }

    // CgsDataStructures.h. The ParameterSchema* then SlotSchema* pointers share one
    // trailing flexible array starting at dword +5 (+0x14): parameters occupy
    // [5 .. 5+ParameterSchemaCount), slots follow. FLAG (host-width: the element
    // widens to 8 bytes on a 64-bit host, so +0x14 is X360-only -- pinned BY NAME).
    const void* mapSchema[1];       // CgsDataStructures.h (+0x14)

    // @ 0x82691AB8 / 0x82691BE0. Resolved sub-schema accessors (assert in-range +
    // resolved -- low tag bit clear). Bodied in CgsFeatureSchema.cpp.
    const ParameterSchema* GetParameterSchema(u32 au32Index) const;
    const SlotSchema*      GetSlotSchema(u32 au32Index) const;

    // Address-of accessors the resolve/unresolve fixers index BY NAME. Parameters
    // start at dword [5]; slots start at [5 + mu32ParameterSchemaCount].
    const ParameterSchema** GetParameterSchemaAddress(u32 au32Index)
    {
        return reinterpret_cast<const ParameterSchema**>(&mapSchema[au32Index]);
    }
    const SlotSchema** GetSlotSchemaAddress(u32 au32Index)
    {
        return reinterpret_cast<const SlotSchema**>(
            &mapSchema[mu32ParameterSchemaCount + au32Index]);
    }

private:
    // @ 0x82680A30. Const address of slot au32Index, used by GetSlotSchema.
    const SlotSchema* const* GetSlotSchemaAddress(u32 au32Index) const;
};

// CgsDataStructures.h:1233. A playback voice schema: an Entity carrying a count
// of FeatureSchema sub-schemas plus the running totals of the slots/parameters/
// output-params those features contribute. The FeatureSchema pointers live in a
// trailing flexible array right after the header (the X360 `a1 + 6` dword base).
struct VoiceSchema : public Entity
{
    // CgsDataStructures.h:1236. The interned type-name the ctor seeds mTypeName
    // from (the X360 static word dword_83008280). DECLARED here for the ctor to
    // reference BY NAME; its DEFINITION (the interned-Name initializer) lives with
    // the EntityFixer<VoiceSchema> registration TU and is DEFERRED -- resolved at
    // TU consolidation, NOT defined in this TU. FLAG.
    static const Name SK_TYPE_NAME;

    // CgsDataStructures.h:1257. Construct for aFeatureSchemaCount features: store
    // the count, zero the running totals, and null every feature slot. Bodied
    // store-for-store from the X360 ctor @ 0x82691E98.
    explicit VoiceSchema(u32 au32FeatureSchemaCount);

    // CgsDataStructures.h:1271.
    u32 GetFeatureSchemaCount() const { return mu32FeatureSchemaCount; }

    // DWARF :322 (real out-of-line symbol -- AemsPlayerVoice::GetClientAllocationSize
    // @0x826A2B58 `bl VoiceSchema::GetFeatureSchema`). The resolved feature at
    // au32Index. FLAG (DEFER): declared-only -- bodied with the schema slices.
    const FeatureSchema& GetFeatureSchema(u32 au32Index) const;

    // CgsDataStructures.h:1342 / :1348 / :1354.
    u32 GetSlotCount() const        { return mu32SlotCount; }
    u32 GetParameterCount() const   { return mu32ParameterCount; }
    u32 GetOutputParamCount() const { return mu32OutputParamCount; }

    // CgsDataStructures.h:1289. Install the FeatureSchema at index au32Index:
    // back out the previously-installed (resolved) feature's contributions to the
    // running totals, store the new pointer, then add the new feature's totals.
    // Bodied store-for-store from the X360 SetFeatureSchema @ 0x82692058.
    void SetFeatureSchema(u32 au32Index, const FeatureSchema& arSchema);

    // CgsDataStructures.h:1362. Address of feature-schema slot au32Index in the
    // trailing flexible array (the X360 `&v3[a2 + 6]`). Named accessor replacing
    // the raw dword index. PUBLIC: the EntityFixer<VoiceSchema> resolve/relocate
    // hooks fix these slot pointers in place.
    const FeatureSchema** GetFeatureSchemaAddress(u32 au32Index)
    {
        return &mapFeatureSchema[au32Index];
    }

    // The three running totals the resolve/unresolve fixers recompute in place
    // (serialization-managed accumulators, mutated by EntityFixer<VoiceSchema>).
    u32 mu32FeatureSchemaCount;     // CgsDataStructures.h:1373  (+0x8)
    u32 mu32SlotCount;              // CgsDataStructures.h:1374  (+0xC)
    u32 mu32ParameterCount;         // CgsDataStructures.h:1375  (+0x10)
    u32 mu32OutputParamCount;       // CgsDataStructures.h:1376  (+0x14)

private:
    // CgsDataStructures.h. The trailing flexible array of resolved/unresolved
    // FeatureSchema pointers. On X360 it begins at +0x18 (the `a1 + 6` dword base
    // in the ctor and `&v3[a2 + 6]` in SetFeatureSchema). Modelled as a 1-length
    // trailing array so the slots can be indexed BY NAME; the real length is
    // mu32FeatureSchemaCount. FLAG (host-width: the array element widens to 8 bytes
    // on a 64-bit host, so the +0x18 base is X360-only -- pinned BY NAME).
    const FeatureSchema* mapFeatureSchema[1];
};

// ============================================================================
// ADDITIVE HOME-GROW (Sound group, Wave 6): the remaining CgsSound::Playback
// serialised-entity types the ContentType/ContentSpec/SlotSchema/ParameterSchema/
// VoiceSpec fixer + accessor TUs read, plus the EntityFixer<T> concrete-fixer
// template. All are Entity subclasses (mName @+0, mTypeName @+4, committed
// CgsRegistry.h) -- NOT forked. Members are pinned BY NAME + SEQUENCE (host-width
// FLAG: pointer members widen on the 64-bit host; no absolute-offset static_assert).
// Each SK_TYPE_NAME is DECLARED here for the fixer type-name compares; its interned
// DEFINITION lives with the registration TU (DEFERRED).
// ============================================================================

// CgsDataStructures.h. Parameter direction, compared by EntityFixer<FeatureSchema>::
// DoResolve when it recomputes the output-parameter total.
enum EParameterDirection
{
    E_PARAMETER_INPUT  = 0,
    E_PARAMETER_OUTPUT = 1
};

// CgsDataStructures.h:382 (DWARF). The content-class entity a ContentType / Slot /
// SlotSchema resolves to. Modelled minimally -- only used as the T of the member-
// pointer relocators (its layout is not read by any Wave-6 TU).
struct ContentClass : public Entity
{
    static const Name SK_TYPE_NAME;   // FLAG: definition DEFERRED.
};

// CgsDataStructures.h (DWARF). The generic-RWAC "feature implementation" entity. FLAG:
// MINIMAL home-grow -- an Entity subclass carrying only its interned type-name, added so
// Registry::GetEntity<GenericRwacFeatureImplementation> (the X360 word dword_83008368)
// can key on it. The full feature-implementation surface is DEFERRED to its own RWAC TU;
// only SK_TYPE_NAME is load-bearing for the registry lookup. Definition DEFERRED (lives
// with the GenericRwacFeatureImplementation registration TU).
struct GenericRwacFeatureImplementation : public Entity
{
    static const Name SK_TYPE_NAME;   // FLAG: definition DEFERRED.
};

// CgsDataStructures.h:440 (DWARF). ContentType : public Entity. Carries the resolved
// ContentClass* it points at, at +8.
struct ContentType : public Entity
{
    static const Name SK_TYPE_NAME;   // FLAG: definition DEFERRED.

    // @ 0x82691778. Return the resolved ContentClass; asserts present + resolved
    // (low tag bit clear). Bodied in CgsDataStructures.cpp.
    const ContentClass& GetContentClass() const;

    // CgsDataStructures.h:531 (+0x8). The resolved content class. On disk an
    // index with bit0 set as the "unresolved / pointer-context" sentinel; once
    // resolved a real ContentClass*. DWARF name+type.
    const ContentClass* mpContentClass;   // (+0x8)
};

// CgsDataStructures.h (DWARF). A parameter sub-schema entity: an Entity carrying its
// direction. Only GetDirection() is read (by EntityFixer<FeatureSchema>::DoResolve).
struct ParameterSchema : public Entity
{
    static const Name SK_TYPE_NAME;   // FLAG: definition DEFERRED.

    EParameterDirection GetDirection() const { return meDirection; }

    // The parameter direction word the resolve pass tests == E_PARAMETER_OUTPUT.
    // FLAG (host/offset): pinned BY NAME; the exact byte offset is DEFERRED to the
    // full ParameterSchema home (only the accessor is load-bearing in Wave 6).
    EParameterDirection meDirection;
};

// CgsDataStructures.h (DWARF). A slot sub-schema entity carrying a resolved
// ContentClass* at +8 (same layout role as ContentType::mpContentClass).
struct SlotSchema : public Entity
{
    static const Name SK_TYPE_NAME;   // FLAG: definition DEFERRED.

    const ContentClass** GetContentClassAddress()
    {
        return &mpContentClass;
    }

    const ContentClass* mpContentClass;   // (+0x8)
};

// CgsDataStructures.h:1631+ (DWARF). VoiceSpec : public Entity. The serialised voice
// specification: a resolved VoiceSchema* (at +8) plus a send-unit count byte. The
// count accessors forward the schema's accumulated totals; the fixers walk the send
// table by mu8SendCount.
struct VoiceSpec : public Entity
{
    static const Name SK_TYPE_NAME;   // FLAG: definition DEFERRED.

    // @ 0x82692398 / 0x82692498 / 0x82692598. Resolved-schema count forwarders
    // (assert mpVoiceSchema present + resolved). Bodied in CgsVoiceSpec.cpp.
    u32 GetSlotCount() const;
    u32 GetParameterCount() const;
    u32 GetOutputParameterCount() const;

    // The trailing send/tail-unit count byte the operator-new size math and the
    // fixer send loops read (lbz 0xC(spec)). Two names for the same byte (send count
    // for the RWAC voice, tail-unit count for the AEMS voice) -- both forward it.
    // (Confirmed 2026-08-25 wave 6: AemsPlayerVoice::operator new @0x826C2270 reads
    // this same +0xC byte -- the AEMS-side rival's separate mu8TailUnitCount member
    // was an invention.)
    u32 GetSendCount() const     { return mu8SendCount; }
    u32 GetTailUnitCount() const { return mu8SendCount; }

    // DWARF :380 (real out-of-line symbol -- `bl VoiceSpec::GetVoiceSchema` in
    // AemsPlayerVoice::GetClientAllocationSize @0x826A2B58 / operator new).
    // FLAG (DEFER): declared-only -- bodied with the spec slices (asserts the
    // schema present + resolved, like the count forwarders above).
    const VoiceSchema& GetVoiceSchema() const;

    // CgsDataStructures.h (+0x8). The resolved voice schema. Low tag bit set while
    // still a serialized index; a live VoiceSchema* once resolved.
    const VoiceSchema* mpVoiceSchema;   // (+0x8)
    u8                 mu8SendCount;    // (+0xC)
};

// ============================================================================
// CgsDataStructures.h (DWARF). EntityFixer<T> -- the concrete per-entity fixer,
// an IEntityFixer subclass parameterised on the entity type T it serialises. Every
// Do* override compares the fixed entity's mTypeName against T::SK_TYPE_NAME, then
// walks/relocates T's member pointers. The bodies are bodied per-T in their own
// fixer TUs (CgsVoiceSchemaFixer.cpp, CgsFeatureSchemaFixer.cpp, CgsSlotSchema.cpp,
// CgsParameterSchemaFixer.cpp, CgsVoiceSpec.cpp, CgsContentSpecFixer.cpp, and the
// ContentType overrides in CgsDataStructures.cpp). Declared here so those out-of-
// line definitions have a class to attach to; the ctor/dtor that self-register on
// IEntityFixer::spHead are DEFERRED.
// ============================================================================
template <typename T>
struct EntityFixer : public IEntityFixer
{
    // IEntityFixer virtual interface, specialised per T. Bodied in the per-T TUs.
    virtual Name DoGetTypeName() const;
    virtual void DoUnresolve(Entity& arEntity) const;
    virtual void DoResolve(Entity& arEntity, const Registry& arRegistry) const;
    virtual void DoRelocate(Entity& arEntity, u8* apu8Base,
                            const Registry& arFrom, const Registry& arTo) const;
    virtual void DoFixUp(Entity& arEntity) const;
    virtual void DoFixDown(Entity& arEntity) const;
};

}
}

#endif // CGS_SOUND_PLAYBACK_CGSDATASTRUCTURES_H
