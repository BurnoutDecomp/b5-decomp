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

}
}

#endif // CGS_SOUND_PLAYBACK_CGSDATASTRUCTURES_H
