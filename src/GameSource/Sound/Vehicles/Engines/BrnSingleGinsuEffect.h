#ifndef BRN_SOUND_VEHICLES_ENGINES_SINGLE_GINSU_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_SINGLE_GINSU_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"

// =============================================================================
// BrnSound::Vehicles::Engines::SingleGinsuEffect
//   GameSource/Sound/Vehicles/Engines/BrnSingleGinsuEffect.h (DWARF home) +
//   GameSource/Sound/Vehicles/Engines/BrnSingleGinsuEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. SingleGinsuEffect is a single-voice
// Ginsu engine effect: DWARF shows
//   SingleGinsuEffect : public BrnSound::Logic::BrnEffectObject
// (the committed sound-logic effect-object base; reused BY NAME). It owns one
// optional HybridExhaustControl* controller and overrides the controller-query /
// controller-attach hooks.
//
// This TU bodies the three boot-trace functions:
//   GetTypeName      @ 0x82685098  -> static type-name string
//   GetController    @ 0x826850A8  -> map controller-slot index to a controller id
//   AttachController @ 0x826850C0  -> latch a HybridExhaustControl if the supplied
//                                     controller's class-id matches
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the controller-class test and the
// controller pointer recovery are described per-function in the .cpp. The owned
// mpHybridControl member is pinned BY NAME; the X360 stores it at +0x34 (+52),
// which assumes 4-byte pointers and a 4-byte vptr, so that absolute offset is NOT
// asserted on a 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// BrnSingleGinsuEffect.h:81 (DWARF). The hybrid-exhaust controller the effect
// latches onto in AttachController. Its full surface is reconstructed in its own
// home; here only the polymorphic effect-base relationship needed to recover the
// controller pointer by name is modelled.
// FLAG: HybridExhaustControl is not yet homed anywhere. The X360 AttachController
// recovers the controller's primary object from the EffectBase sub-object pointer
// it is handed; modelled here as `HybridExhaustControl : CgsSound::Logic::EffectBase`
// so that recovery is a by-name downcast rather than raw pointer arithmetic. Only
// the inheritance is asserted; no controller-specific members are fabricated.
struct HybridExhaustControl : public CgsSound::Logic::EffectBase
{
    HybridExhaustControl() {}
    virtual ~HybridExhaustControl() {}
};

// BrnSingleGinsuEffect.h:41 (DWARF). Single-voice Ginsu engine effect.
struct SingleGinsuEffect : public BrnSound::Logic::BrnEffectObject
{
    SingleGinsuEffect() : mpHybridControl(nullptr) {}
    virtual ~SingleGinsuEffect() {}

    // — per-class RTTI. DEFERRED bodies (declared for
    // the effect vtable shape; the sTypeInfo static-init lives in its own slice).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;

    // @ 0x82685098 — per-class RTTI type name. Bodied.
    virtual const char* GetTypeName() const;

    // @ 0x826850A8 — map a controller-slot index to the
    // controller id this effect exposes. Bodied.
    virtual s32 GetController(s32 aiSlot);

    // @ 0x826850C0 — latch a HybridExhaustControl if the
    // supplied controller's class-id matches. Bodied.
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);

    // — DEFERRED (outside this TU's recon'd set;
    // declared for the effect vtable shape).
    virtual void UpdateParams(f32 afTime);

    // — DEFERRED; declared for the vtable shape.
    virtual void ProcessUpdate();

private:
    // BrnSingleGinsuEffect.h:81 (DWARF). The latched controller (null until a
    // matching controller is attached). X360 stores it at +0x34 BY NAME; the
    // absolute offset is not asserted across the 32/64 pointer-width boundary.
    HybridExhaustControl* mpHybridControl;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_SINGLE_GINSU_EFFECT_H
