#ifndef BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H
#define BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsClassTypeInfo.h"  // ClassTypeInfo<T> (canonical)
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/BrnResourceRegistrar.h"   // BrnSound::Logic::IResourceRequester + ResourceRegistrar (canonical home)

// =============================================================================
// BrnSound::Logic::BrnEffectControl
//   GameSource/Sound/Module/LogicModule/BrnEffectControl.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnEffectControl.cpp
//
// The ARTIST whole-object base is IResourceRequester; EffectBase is +4.
// ResourcesAreReady (82696778) receives the former and sets +0x31;
// Detach (826EBF88 object / 826EB8B0 control) receives the latter and
// tests/clears +0x2D. Both address EffectBase::mbHasLoadedData, named by
// DecFIGS CgsEffectBase.h:754. The host expresses interface adjustments
// through C++ base conversions instead of console offsets.
// =============================================================================

#if 0 // RETIRED: the former minimal rival engine-effect definitions; canonical CgsEffectBase.h is used above.
namespace CgsSound
{
namespace Logic
{

// SHARED minimal engine-effect types (ClassTypeInfo + EffectBase) -- see the matching
// note in BrnEffectObject.h. Both headers gate these on the SAME guard macro so they
// can be co-included (e.g. by BrnTrafficEngine.h, which homes both a BrnEffectObject-
// derived TrafficEngine and a BrnEffectControl-derived Traffic3DControl) without an ODR
// clash. Whichever header is parsed first defines them; the other skips. BrnEffectObject
// .h's EffectBase is a superset of this one, so it satisfies EffectControl's members too.
// ⛔⛔ THE REDUCED CgsSound::Logic::EffectBase / EffectObject / EffectControl BLOCK THAT
// STOOD HERE IS GONE -- IT WAS UNREACHABLE, AND IT LIED ABOUT THE BASE SURFACE.
//
// This header includes the CANONICAL engine home,
// GameShared/GameClasses/Sound/Logic/CgsEffectBase.h, at the top of the file. That header
// defines EffectBase (the full DWARF surface: mpNextEffectBase / mpState / mu16RefCount /
// mu16AttachCount / mu16AllocatorIndex / miObjectId / mfRunningTime / mfDeltaTime /
// meAttachState / meDetachState / mpLogicModule / mbEnabled / mbHasLoadedData /
// mpDynamicMixIo, and the virtuals Prepare / Attach / GetController / AttachController /
// SetupLoadData / UpdateParams / ProcessUpdate / Detach / Notify / Destroy) plus
// EffectObject and EffectControl. The block that used to sit here re-declared the same
// three types, in the same namespace, with FEWER members and NO virtuals, behind
// `#ifndef CGS_SOUND_LOGIC_EFFECT_ENGINE_TYPES_DEFINED`.
//
// PROVEN DEAD 2026-09-16 by compile gate: a probe deriving from CameraControl (this
// header's family) and from SubmixesEffect (the sibling's) resolves mu16AttachCount,
// mu16RefCount, mfRunningTime and mpDynamicMixIo and overrides Attach / UpdateParams /
// ProcessUpdate / Notify. Every one of those exists ONLY in the canonical header, so the
// canonical definition is what every leaf has always seen; if a reduced block had ever
// won, each of those would have been a compile error instead.
//
// ⚠️ THE COST OF LEAVING IT: the reduced block's own banner claimed it was the definition
// in use, and grepping THIS FILE for the base surface is what produced the false finding
// "BrnEffectControl declares neither `virtual bool Attach()` nor mu16AttachCount, so
// CameraControl::Attach cannot be written without growing every sound control" -- a
// blocker that never existed and that stopped a wave. Derive the base surface from
// CgsEffectBase.h, or from a compile gate; never from a re-declaration.

// CgsEffectBase.h:765 (DWARF). EffectControl : public EffectBase. The control-side
// counterpart of EffectObject; carries the per-class RTTI hook used by
// BrnEffectControl's GetTypeInfo/CreateObject.
struct EffectControl : public EffectBase
{
    EffectControl() {}
    virtual ~EffectControl() {}

    virtual ClassTypeInfo<EffectControl>* GetTypeInfo() const;
    virtual const char*                   GetTypeName() const;
};

} // namespace Logic
} // namespace CgsSound
#endif

namespace BrnSound
{
namespace Logic
{

// IResourceRequester + ResourceRegistrar now live in their canonical home
// (GameSource/Sound/BrnResourceRegistrar.h, included above) -- folded out of here to resolve the
// cross-header ODR.

// BrnEffectControl.h:56 (DWARF). Multiply inherits the engine effect-control base
// (primary vptr @ this+0) and IResourceRequester (sub-object vptr @ this+4).
struct BrnEffectControl : public CgsSound::Logic::EffectControl,
                          public IResourceRequester
{
    BrnEffectControl();
    virtual ~BrnEffectControl();

    // BrnEffectControl.cpp:38 — per-class RTTI (DEFERRED bodies; declared for the
    // control vtable shape).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const;
    virtual const char*                                                     GetTypeName() const;

    // BrnEffectControl.h:220 — IResourceRequester override. DEFERRED (outside this
    // TU's recon'd function set); declared for the second-base vtable shape.
    virtual void ResourcesAreReady();

    // BrnEffectControl.h:231 — IResourceRequester override. DEFERRED; declared for
    // the second-base vtable shape.
    virtual ResourceRegistrar& GetResourceRegistrar();

    // BrnEffectControl.h:239 — override of the effect Detach. DEFERRED; declared
    // for the control vtable shape.
    virtual bool Detach();


};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H
