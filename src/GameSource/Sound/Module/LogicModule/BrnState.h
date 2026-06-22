#ifndef BRN_SOUND_LOGIC_BRN_STATE_H
#define BRN_SOUND_LOGIC_BRN_STATE_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::BrnState
//   GameSource/Sound/Module/LogicModule/BrnState.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnState is a sound-logic state
// (one node of the sound state machine). It derives from the engine
// CgsSound::Logic::State base (DWARF: BrnState : public State, where State is
// CgsSound::Logic::State : public CgsSound::MemBase) and overrides the per-class
// RTTI hooks. This TU bodies only GetTypeName().
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): no member offsets are touched by
// this TU's function (GetTypeName loads a static string literal and ignores
// `this`), so no absolute offsets are asserted here.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Per-class RTTI descriptor. CgsState.h (DWARF). Templated on the leaf class;
// only the shape (id/name/base/factory) is load-bearing here.
// FLAG: minimal — the owning CgsState/CgsEffectBase RTTI home is not yet
// reconstructed; this is the shape needed to declare BrnState's RTTI hooks.
template <typename T>
struct ClassTypeInfo
{
    s32               ObjectID;
    const char*       mpcTypeName;
    ClassTypeInfo<T>* mpBaseTypeInfo;
    T* (*mpfnCreateObject)(u32);
};

// CgsState.h:75 (DWARF): CgsSound::Logic::State : public CgsSound::MemBase.
// Engine sound-logic state base. Only the RTTI virtuals overridden by BrnState
// are declared here; the full base API is reconstructed in its own home.
// FLAG: minimal reconstruction of an un-homed base (MemBase base elided; only
// the polymorphic RTTI surface needed by this leaf is modelled).
struct State
{
    State() {}
    virtual ~State() {}

    virtual ClassTypeInfo<State>* GetTypeInfo() const;
    virtual const char*           GetTypeName() const;
};

} // namespace Logic
} // namespace CgsSound

namespace BrnSound
{
namespace Logic
{

// BrnState.h:60 (DWARF): BrnState : public State. Overrides the per-class RTTI
// hooks. GetTypeName() is bodied in this TU's .cpp.
struct BrnState : public CgsSound::Logic::State
{
    BrnState() {}
    virtual ~BrnState() {}

    // BrnState.cpp:32 — per-class RTTI.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char*                                            GetTypeName() const;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_STATE_H
