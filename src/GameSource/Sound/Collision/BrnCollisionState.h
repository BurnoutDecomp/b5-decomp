#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Logic::Collision::CollisionState
//   GameSource/Sound/Collision/BrnCollisionState.h (DWARF home) +
//   GameSource/Sound/Collision/BrnCollisionState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// CollisionState is the sound-logic state node for collision effects. DWARF
// (references/DecFIGS/dwarfdump/GameSource/Sound/Collision/BrnCollisionState.h):
//   struct BrnSound::Logic::Collision::CollisionState : public BrnSound::Logic::BrnState
// so it inherits the COMMITTED BrnState base (the CgsSound::Logic::State sound-logic
// state base) reused BY NAME from
// GameSource/Sound/Module/LogicModule/BrnState.h, and overrides the per-class RTTI
// hooks. This TU bodies only the destructor (@ 0x826D3380).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the destructor touches no member
// offsets (it re-installs the two vtable pointers at +0 -- compiler-synthesised --
// and calls the base's DestroyEffects()), so none are asserted here. The committed
// BrnState base (BrnState.h) is reused BY NAME. The DWARF-attested data members
// (DataPoint<ELifetime> meLifetime, OutputCollision mOutputCollision,
// f32 mfTimeWeAttached) are NOT in this destructor's scope and are DEFERRED to
// their own recon slices -- same treatment as the sibling GlobalState home.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// DWARF-confirmed CollisionState : public BrnState. Same vtable pair (off_820AE1F4
// own-vtable install / off_820AA820 MemBase base re-install) as the sibling
// GlobalState @ 0x826D2250. Only the surface needed for the destructor's vtable
// slot is declared; the remaining RTTI hook bodies are DEFERRED.
struct CollisionState : public BrnSound::Logic::BrnState
{
    CollisionState() {}

    // @ 0x826D3380 -- scalar/deleting destructor. The X360 thunk installs
    // CollisionState's own vtable (off_820AE1F4), calls State::DestroyEffects() to
    // tear down attached effects, re-installs the MemBase base vtable
    // (off_820AA820) as the chain unwinds, and (deleting flavour) routes the
    // storage back through the sound allocator. The observable source-level body is
    // the DestroyEffects() call; the vtable re-installs and allocator-routed free
    // are the compiler-synthesised deleting-destructor parts (host `delete` stands
    // in for off_82FFB954). Bodied out-of-line in BrnCollisionState.cpp.
    virtual ~CollisionState();

    // -- per-class RTTI. DEFERRED bodies (declared for the state vtable shape;
    // sTypeInfo static-init lives in its own recon slice). DWARF-confirmed
    // return type/virtualness/const.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_H
