#ifndef BRN_SOUND_LOGIC_WORLD_EMITTER_STATE_H
#define BRN_SOUND_LOGIC_WORLD_EMITTER_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"

// =============================================================================
// BrnSound::Logic::World::EmitterState
//   DWARF home: GameSource/Sound/World/BrnEmitterState.{h,cpp}.
//   PROJECT home: GameSource/Sound/Module/LogicModule/ -- co-located with the
//   committed sibling BrnEmitterStateManager (whose DWARF also homes at
//   Sound/World/ but is committed at Module/LogicModule/).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// DWARF (references/DecFIGS/dwarfdump/GameSource/Sound/World/BrnEmitterState.h:56):
//   struct BrnSound::Logic::World::EmitterState : public BrnSound::Logic::BrnState
// so it inherits the COMMITTED BrnState base (the CgsSound::Logic::State sound-logic
// state base) reused BY NAME from
// GameSource/Sound/Module/LogicModule/BrnState.h, and overrides the per-class RTTI
// hooks plus the state lifecycle (Attach / UpdateParams / IsAttachedToThis). This
// TU bodies only the destructor (@ 0x826D1F70).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the destructor touches no member
// offsets (it re-installs the two vtable pointers at +0 -- compiler-synthesised --
// and calls the base's DestroyEffects()), so none are asserted here. The committed
// BrnState base (BrnState.h) is reused BY NAME. The DWARF-attested private member
// StaticSoundEntity mEntity (BrnEmitterState.h:117) is NOT in this destructor's
// scope and is DEFERRED to its own recon slice (StaticSoundEntity is un-homed in
// this group; modelling it by value would force a header cascade). Same treatment
// as the sibling GlobalState / TrafficState homes.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// DWARF-confirmed EmitterState : public BrnState (BrnEmitterState.h:56). Same
// vtable pair (off_820AE1F4 own-vtable install / off_820AA820 MemBase base
// re-install) as the sibling GlobalState @ 0x826D2250. Vtable order per DWARF:
// GetTypeInfo, GetTypeName, ~EmitterState, Attach, UpdateParams, IsAttachedToThis;
// only the destructor is bodied by this TU, the rest are declared for vtable shape.
struct EmitterState : public BrnSound::Logic::BrnState
{
    EmitterState() {}

    // -- per-class RTTI. DEFERRED bodies (declared for the state vtable shape;
    // sTypeInfo static-init lives in its own recon slice). DWARF-confirmed
    // return type/virtualness/const. Precede the destructor per the DWARF vtable
    // order.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();
    static CgsSound::Logic::State* CreateObject(u32 auType);

    // @ 0x826D1F70 -- scalar deleting destructor. The X360 thunk installs
    // EmitterState's own vtable (off_820AE1F4), calls State::DestroyEffects() to
    // tear down attached effects, re-installs the MemBase base vtable
    // (off_820AA820) as the chain unwinds, and (deleting flavour) routes the
    // storage back through the sound allocator. Observable body is the
    // DestroyEffects() call; the vtable re-installs and allocator-routed free are
    // the compiler-synthesised deleting-destructor parts (host `delete` stands in
    // for off_82FFB954). Bodied out-of-line in BrnEmitterState.cpp.
    virtual ~EmitterState();

    // -- state lifecycle overrides (DWARF vtable slots after the dtor).
    // Attach (@ 0x826D2010) and IsAttachedToThis (@ 0x826BADA0) are bodied in the
    // .cpp; UpdateParams stays a DEFERRED declaration (not in this TU's function set).
    virtual void Attach(void* lpvAttachment);
    virtual void UpdateParams(f32 lfDeltaTime);
    virtual bool IsAttachedToThis(void* lpvTestAttachment);

    const BrnSound::World::StaticSoundEntity& GetSoundEntity() const
    {
        CGS_ASSERT(IsAttached(), "IsAttached()");
        return mEntity;
    }

private:
    // mEntity -- StaticSoundEntity (DWARF BrnEmitterState.h:117), the static sound
    // entity this state is attached to. StaticSoundEntity's full layout is deferred
    // project-wide (forward-decl only in SharedClasses/.../BrnStaticSoundMap.h); its
    // sole member is a 16-byte Vector3Plus mPosPlus -- position xyz plus a packed w
    // whose HIGH 16 bits carry the entity type (GetType()). Modelled here as a
    // correctly-sized 16-byte span (the only X360-attested size is the 16-byte
    // lvx/stvx copy in Attach) so this home need not force the StaticSoundEntity
    // layout cascade the header note warned about. The X360 places it at this+0x60
    // (its w at this+0x6C); Attach's whole-vector copy and IsAttachedToThis's
    // per-lane float compare + w-high-word type compare are the only accesses.
    BrnSound::World::StaticSoundEntity mEntity;
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_WORLD_EMITTER_STATE_H
