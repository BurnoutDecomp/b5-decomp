#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"

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
    enum ELifetime
    {
        E_NONE      = 0,
        E_COLLISION = 1,
        E_SCRAPE    = 2,
    };

    CollisionState()
        : meLifetime(E_NONE)
        , mOutputCollision()
        , mfTimeWeAttached(0.0f)
    {
    }

    // @ 0x826D3380 -- scalar/deleting destructor. The X360 thunk installs
    // CollisionState's own vtable (off_820AE1F4), calls State::DestroyEffects() to
    // tear down attached effects, re-installs the MemBase base vtable
    // (off_820AA820) as the chain unwinds, and (deleting flavour) routes the
    // storage back through the sound allocator. The observable source-level body is
    // the DestroyEffects() call; the vtable re-installs and allocator-routed free
    // are the compiler-synthesised deleting-destructor parts (host `delete` stands
    // in for off_82FFB954). Bodied out-of-line in BrnCollisionState.cpp.
    virtual ~CollisionState();

    // -- per-class RTTI. ARTIST supplies the name and factory; the DecFIGS static
    // initializer pins ObjectID 0x50000 and registers this descriptor as a BrnState.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();
    static CgsSound::Logic::State* CreateObject(u32 auAllocator);

    virtual void Attach(void* apvAttachment) override;
    virtual void UpdateParams(f32 afDeltaTime) override;
    virtual bool Detach() override;

    const CgsSound::Utils::DataPoint<ELifetime>& GetLifetime() const
    {
        return meLifetime;
    }

    void SetLifetime(ELifetime aeLifetime) { meLifetime.Update(aeLifetime); }
    const OutputCollision& GetOutputCollision() const { return mOutputCollision; }
    OutputCollision& GetOutputCollision() { return mOutputCollision; }
    f32 GetTimeWeAttached() const { return mfTimeWeAttached; }
    f32 GetCurrentTime() const { return mfCurTime; }

private:
    CgsSound::Utils::DataPoint<ELifetime> meLifetime;
    OutputCollision mOutputCollision;
    f32 mfTimeWeAttached;
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_H
