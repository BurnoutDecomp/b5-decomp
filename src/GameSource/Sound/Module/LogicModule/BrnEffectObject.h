#ifndef BRN_SOUND_LOGIC_BRN_EFFECT_OBJECT_H
#define BRN_SOUND_LOGIC_BRN_EFFECT_OBJECT_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsClassTypeInfo.h"  // ClassTypeInfo<T> (canonical)
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/BrnResourceRegistrar.h"   // BrnSound::Logic::IResourceRequester + ResourceRegistrar (canonical home)

// =============================================================================
// BrnSound::Logic::BrnEffectObject
//   GameSource/Sound/Module/LogicModule/BrnEffectObject.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnEffectObject.cpp
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
// cross-header ODR (they were duplicated in BrnEffectControl.h + BrnStateManager.h too).

// BrnEffectObject.h:51 (DWARF). Multiply inherits the engine effect-object base
// (primary vptr @ this+0) and IResourceRequester (sub-object vptr @ this+4).
struct BrnEffectObject : public CgsSound::Logic::EffectObject,
                         public IResourceRequester
{
    // BrnEffectObject.h:87 (DWARF). A resolved sample reference.
    struct SampleTag
    {
        f32 mfVolume;
        s16 miSampleIndex;

        void Construct();
    };

    BrnEffectObject();
    virtual ~BrnEffectObject();

    // — per-class RTTI.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;
    virtual const char*                                                    GetTypeName() const;

    // BrnEffectObject.h:139 — IResourceRequester override, bodied inline in this
    // DWARF home. ASM @ 0x82696778: mark resources-ready, assert the effect is
    // still waiting for its data, then advance the attach state to PREPARING.
    virtual void ResourcesAreReady()
    {
        // X360 STORE ORDER (0x82696778):
        //   stb 1, +0x31   (mbHasLoadedData = true)   -- precedes the assert
        //   lwz    +0x24   (read meAttachState)
        //   assert meAttachState == E_ATTACH_STATE_WAITING_FOR_DATA
        //   stw 2, +0x24   (meAttachState = E_ATTACH_STATE_PREPARING)
        mbHasLoadedData = true;
        CGS_ASSERT(GetAttachState() == CgsSound::Logic::EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA,
                   "GetAttachState() == E_ATTACH_STATE_WAITING_FOR_DATA");
        meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING;
    }

    // BrnEffectObject.h:157 — IResourceRequester override; bodied in the .cpp
    // (ASM @ 0x82696850 forwards to the owning module's embedded registrar).
    virtual ResourceRegistrar& GetResourceRegistrar();

    // ARTIST 826EBFA0 reads EffectBase+0x2D. ResourcesAreReady's
    // 82696794 store uses IResourceRequester+0x31: the SAME inherited
    // mbHasLoadedData, because the console EffectBase subobject starts +4.
    // Do not split it into a separate request-issued latch: LoadAsset may be
    // called directly through IResourceRequester (boost/crumple do this).
    virtual bool Detach()
    {
        if (mbHasLoadedData)
            GetResourceRegistrar().RemoveRequests(static_cast<IResourceRequester*>(this));
        mbHasLoadedData = false;
        return CgsSound::Logic::EffectBase::Detach();
    }

    // BrnEffectObject.h:207 — resolve a sample tag. Declared for home
    // completeness; not bodied by this group (outside this TU's func set).
    bool GetSampleTag(u32 eTag, u32 uIndex, u32 uCount, SampleTag& rTag) const;

};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_EFFECT_OBJECT_H
