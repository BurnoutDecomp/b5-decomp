#ifndef BRN_SOUND_VEHICLES_DEFORMATION_DEFORMATION_EFFECT_H
#define BRN_SOUND_VEHICLES_DEFORMATION_DEFORMATION_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // DataPoint / InterpolateLine / Curve / Average (BY NAME, DWARF-named members)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // embedded CgsSound::Logic::VoiceWrapper (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Deformation::DeformationEffect
//   GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnDeformationEffect.h:54):
//   DeformationEffect : public BrnSound::Logic::BrnEffectObject
// The car-body crumple/deformation sound EFFECT OBJECT: drives an AEMS "crumple"
// patch voice whose intensity tracks the deformation delta, ramping out via mFadeOut.
//
// This TU bodies (in this batch) four functions:
//   DeformationEffect()  @ 0x826CF6C0  -- MSVC inlined full-object ctor
//   GetTypeName()        @ 0x82685730  -- interned type-tag leaf
//   SetupLoadData()      @ 0x826E4C88  -- request the crumple patch bank bundle
//   Detach()             @ 0x826F39C8  -- base detach + release the patch voice
// (Attach @ 0x826F37E8 and AttachController @ 0x82685740 are declared here for the
//  vtable shape but DEFERRED -- not store-for-store faithful in this batch's dossier.)
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): leaf members are pinned BY NAME + DWARF
// SEQUENCE (offsets in comments; the ctor is self-consistent -- mPatchVoice @+0x8C --
// while the Attach/Detach/AttachController tail reads shift -4 across a pointer-adjacent
// gap in the X360 dossier; by-name access is self-consistent on host so no absolute
// offset is asserted).
//
// AVERAGE / ODR NOTE: CgsSound::Utils::Average is homed in CgsSoundUtils.h with the
// DWARF member names (maPoints / muNextPoint / mfAverage) and a Flush() reset. Do NOT
// also include GameShared/GameClasses/Sound/Utils/CgsSoundAverage.h -- it re-defines
// the same class under a different include guard with non-DWARF member names, which is
// an ODR redefinition when both land in one TU.
// =============================================================================

// Pointer-only control back-reference -> forward declaration (mirrors ReverbEffect.h).
// The full type (PhysicsControl : BrnEffectControl : EffectControl : EffectBase) is
// pulled in by the .cpp for the AttachController downcast (a deferred slice).
namespace BrnSound { namespace Vehicles { namespace Engines { struct PhysicsControl; } } }

namespace BrnSound
{
namespace Vehicles
{
namespace Deformation
{

struct DeformationEffect : public BrnSound::Logic::BrnEffectObject
{
    // DWARF BrnDeformationEffect.h:87. The rolling-average window width.
    static const u32 KU_AVERAGE_TERMS = 4;

    DeformationEffect();            // @ 0x826CF6C0
    virtual ~DeformationEffect();   // anchor for the X360 deleting destructor (DEFERRED body)

    // ---- effect vtable overrides (only the four this TU bodies are defined; the
    //      rest are declared for the vtable shape and bodied in their own slices) ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const; // DEFERRED
    virtual const char* GetTypeName() const;                 // @ 0x82685730
    virtual s32         GetController(s32 aiSlot);           // (DEFERRED body)
    virtual void        AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82685740 (DEFERRED body)
    virtual void        SetupLoadData();                     // @ 0x826E4C88
    virtual bool        Attach();                            // @ 0x826F37E8 (DEFERRED body)
    virtual void        UpdateParams(f32 afTimeStep);        // (DEFERRED body)
    virtual void        ProcessUpdate();                     // (DEFERRED body)
    virtual bool        Detach();                            // @ 0x826F39C8

    // FLAG (un-homed back-reference): the Attach voice-creation gate reads a vehicle
    // game-mode/context word the X360 reaches through r30(=this+0x88)+0x48 == this+0xD0
    // (Hex-Rays *(a1+208)) -- outside this object's own layout, via an un-homed
    // vehicle-state link. Exposed BY NAME so the (deferred) Attach gate is expressed
    // rather than a raw offset fabricated; its own reconstruction is a separate slice.
    s32 GetGameModeWord() const;

    // ---- members in DWARF order (BrnDeformationEffect.h:89..100). X360 offsets are
    //      facts (comments); not static_asserted across the 32/64 pointer boundary. ----
    CgsSound::Utils::DataPoint<bool>       mbDeforming;            // +0x38 (:89)
    CgsSound::Utils::DataPoint<f32>        mDeformAmount;          // +0x3C (:90)
    CgsSound::Utils::Average<KU_AVERAGE_TERMS, f32> mDeformDeltaAverage; // +0x44 (:91)
    CgsSound::Utils::DataPoint<f32>        mDeformIntensityLagged; // +0x5C (:94)
    f32                                    mfAemsIntensity;        // +0x64 (:95; ctor leaves UNINIT)
    CgsSound::Utils::InterpolateLine       mFadeOut;               // +0x68 (:96)
    f32                                    mfTimeDeforming;        // +0x84 (:97; ctor leaves UNINIT)
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl; // +0x88 (:99; ctor leaves UNINIT)
    CgsSound::Logic::VoiceWrapper          mPatchVoice;            // +0x8C (:100)
};

} // namespace Deformation
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_DEFORMATION_DEFORMATION_EFFECT_H
