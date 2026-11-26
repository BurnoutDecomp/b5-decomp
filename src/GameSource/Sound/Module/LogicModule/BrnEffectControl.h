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

namespace BrnSound
{
namespace Logic
{

// IResourceRequester + ResourceRegistrar now live in their canonical home
// (GameSource/Sound/BrnResourceRegistrar.h, included above) -- folded out of here to resolve the
// cross-header ODR.

// The ARTIST effect methods receive the EffectControl view at +4.
struct BrnEffectControl : public IResourceRequester,
                          public CgsSound::Logic::EffectControl
{
    // DWARF BrnEffectControl.h:256: defined in the class (ARTIST has no out-of-line copy; every
    // CreateObject inlines it, e.g. MixerControl::CreateObject 0x826D2328).
    BrnEffectControl() {}
    virtual ~BrnEffectControl();
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectControl* CreateObject(u32 luAllocator);
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo;   // DWARF static data member (ARTIST 0x82F2E80C)

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
