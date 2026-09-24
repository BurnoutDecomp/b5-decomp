#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_EFFECT_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // BrnSound::Logic::BrnEffectObject (committed base, reused BY NAME)
#include "GameSource/Sound/Module/LogicModule/Brn3DUserSpaceEffectControl.h" // Collision3DControl base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"           // CgsSound::Logic::Voice (mCrashVoice)
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"                 // Nicotine::DMixIO (GetGain reads the latched dmix handle)

// =============================================================================
// BrnSound::Logic::Collision::CollisionEffect
//   GameSource/Sound/Collision/BrnCollisionEffect.{h,cpp}
//   (canonical home -- the X360 mangled name is
//    BrnSound::Logic::Collision::CollisionEffect; the Sound/Collision/ dir already
//    hosts the sibling collision-audio classes -- BrnCollisionStateManager,
//    BrnCollisionDataStructures, BrnCollisionFrameInformation, BrnHingeStateCache.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// CollisionEffect is the collision sound-effect object -- the per-collision crash
// voice driver. DWARF (BrnCollisionEffect.h) shows
//   struct CollisionEffect : public BrnSound::Logic::BrnEffectObject
// (the committed sound-logic effect-object base; reused BY NAME, same as the done
// siblings SingleGinsuEffect / LoopModelEffect). It owns one crash Voice, a couple
// of Nicotine mixer-slider indices, and a small size-specific volume/pitch pair.
//
// This TU bodies the three dossier functions:
//   CollisionEffect()                @ 0x826AFD20  (ctor -- field init)
//   GetGain() const                  @ 0x82688138  (mixer-output gain * size volume)
//   `vector deleting destructor'      @ 0x826C90D8  (forwards to ~CollisionEffect)
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ctor lays the object out by
// absolute byte offset over 4-byte pointers/vptrs:
//   +0x44 (68)  mCrashVoice           (Voice vptr installed @ +0x44, then 0,0)
//   +0x50 (80)  mfIntensity           (= 0.0f)
//   +0x54 (84)  meNicotineVolumeSlider(= 5)
//   +0x58 (88)  meNicotinePitchSlider (= 5)
//   +0x5C (92)  mSizeSettings.mfVolume(= 1.0f)
//   +0x60 (96)  mSizeSettings.mfPitch (= 1.0f)
// On a 64-bit host pointer/vptr widths differ, so members are pinned BY NAME +
// SEQUENCE only and the absolute offsets above are NOT static_asserted.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

struct CollisionControl;
struct CollisionState;
struct OutputCollision;

struct CollisionEffect : public BrnSound::Logic::BrnEffectObject
{
    // BrnCollisionEffect.h:115 (DWARF). Per-collision-size volume/pitch pair. Its
    // default ctor sets both to 1.0f (the X360 CollisionEffect ctor stores 1.0f at
    // +0x5C and +0x60 -- mSizeSettings inlined).
    struct SizeSpecificSettings
    {
        SizeSpecificSettings() : mfVolume(1.0f), mfPitch(1.0f) {}

        f32 mfVolume; // BrnCollisionEffect.h:116
        f32 mfPitch;  // BrnCollisionEffect.h:117
    };

    // BrnCollisionEffect.h:144 (DWARF). Two-step voice-bring-up state.
    enum EPrepareState
    {
        E_PREPARE_STATE_CONSTRUCT_VOICE = 0,
        E_PREPARE_STATE_CONNECT_VOICE   = 1,
    };

    // @ 0x826AFD20 -- field-init ctor. Bodied in BrnCollisionEffect.cpp.
    CollisionEffect();

    // @ 0x826C90D8 -- the X360 `vector deleting destructor'. The leaf teardown is
    // the inherited BrnEffectObject dual-base settle (same store-for-store shape as
    // the committed BrnEffectObject dtor @ 0x826AF4C8 and the sibling SingleGinsuEffect
    // dtor); this leaf adds nothing of its own. Out-of-line so the deleting-destructor
    // thunk is emitted in this class's own TU.
    virtual ~CollisionEffect();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auAllocator);

    virtual bool Prepare(CgsSound::Logic::State* apState) override;
    virtual s32 GetController(s32 aiIndex) override;
    virtual void AttachController(CgsSound::Logic::EffectBase* apController) override;
    virtual bool Attach() override;
    virtual void UpdateParams(f32 afDeltaTime) override;
    virtual void ProcessUpdate() override;
    virtual bool Detach() override;

    // @ 0x82688138 -- collision crash gain. Bodied in BrnCollisionEffect.cpp.
    // DWARF (BrnCollisionEffect.cpp:231) declares this `float GetGain() const`.
    f32 GetGain() const;
    f32 GetPitch() const;

    // The DWARF home lists GetTypeInfo / GetTypeName / CreateObject / Prepare /
    // GetController / AttachController / Attach / UpdateParams / ProcessUpdate / Detach /
    // GetGain / GetPitch (above) and the private CalculateIntensity / InitWork<T> (below).

private:
    // BrnCollisionEffect.h:126 (DWARF): attach the bank, silence Send01, play the sample and
    // take the bin's mixer slider and size settings. Instantiated for crashbin @0x826EB240 and
    // propscrashbin @0x826EB368, called from Attach @0x826F8218 by pipeline.
    template <typename T>
    void InitWork(CollisionState* apState, const OutputCollision& arCollision);

    // The bin's volume / pitch for the collision's size (crashbin @0x826AABC8): Large -> z,
    // Medium -> y, Small -> x, anything else asserts "Bad Size".
    template <typename T>
    void GetSizeSpecificSettings(const OutputCollision& arCollision, const T& arBin,
                                 SizeSpecificSettings& arSettings) const;

    // BrnCollisionEffect.cpp:434 (DWARF) @0x82688240: the crash's ducking intensity, 0..100.
    f32 CalculateIntensity(const OutputCollision& arCollision);

    // --- members (DWARF order; X360 offsets in comments only, not asserted) ---

    // BrnCollisionEffect.h:151 (DWARF): the owning collision control, set by
    // AttachController @0x82688078 (`stw (ctrl-4),0x34(r3)` on the EffectBase sub-object,
    // which sits at +4 -- i.e. X360 +0x38; the ctor nulls it with `stw 0,0x38`).
    // NOTE: it is NOT the endpoint GetGain / GetPitch read: their +0x34 is EffectBase+0x30,
    // the effect's OWN EffectBase::mpDynamicMixIo (GetDMixIOPtr()). Reading the control's
    // endpoint there made every crash voice silent and pitch 0 (FX-VOICEPOOL 2026-09-24).
    CollisionControl* mpCollisionControl;

    // BrnCollisionEffect.h:152 (DWARF). Voice bring-up state. The X360 ctor zeroes
    // it (stw 0,0x3C); init to CONSTRUCT_VOICE.
    EPrepareState mePrepareState;

    // BrnCollisionEffect.h:154/155 (DWARF). First-update + azimuth-use flags (X360 +0x40 /
    // +0x41; the ctor leaves them, Attach @0x826F8218 sets both).
    bool mbFirstUpdate;
    bool mbUseAzimuth;

    // BrnCollisionEffect.h:157 (DWARF). The crash voice (CgsSound::Logic::Voice).
    // X360 ctor installs the Voice vtable @ +0x44 and nulls its handle/owner words.
    CgsSound::Logic::Voice mCrashVoice;

    // BrnCollisionEffect.h:159 (DWARF). Crash intensity; X360 ctor stores 0.0f @ +0x50.
    f32 mfIntensity;

    // BrnCollisionEffect.h:160/161 (DWARF). The Nicotine mixer-slider indices the
    // crash voice drives. X360 ctor stores 5 (== DMX_COUNT / the reserved slider
    // index) at +0x54 and +0x58. GetGain reads meNicotineVolumeSlider as the
    // GetDMixOutput slot. DWARF types these as AttribSys::Enums::eCollisionMixerSliders;
    // that enum home is not reconstructed, so they are modelled as the s32 slot
    // indices the X360 actually stores/passes.
    // FLAG: eCollisionMixerSliders enum un-homed -> modelled as s32 (the X360 store
    // width); the literal 5 is the X360 default, not a fabricated enum value.
    s32 meNicotineVolumeSlider;
    s32 meNicotinePitchSlider;

    // BrnCollisionEffect.h:163 (DWARF). Size-specific volume/pitch (defaults 1.0/1.0).
    SizeSpecificSettings mSizeSettings;
};

// =============================================================================
// BrnSound::Logic::Collision::Collision3DControl
//   (shares this DWARF source home, BrnCollisionEffect.h:38 -- a DISTINCT class from
//    the CollisionEffect object above: Collision3DControl is the effect CONTROL.)
//
// DWARF (BrnCollisionEffect.h:179):
//   struct Collision3DControl : public BrnSound::Logic::Brn3DUserSpaceEffectControl
// (DWARF-authoritative base -- NOT Brn3DEffectControl; the intermediate UserSpace base
//  owns mpTransform + the four Vector3s the ctor zero-inits.) Collision3DControl adds
// NO data members of its own per DWARF.
//
// This slice bodies three ledger functions (in BrnCollisionEffect.cpp):
//   Collision3DControl()           @ 0x826E88D0  (ctor -- chains Brn3DUserSpaceEffectControl())
//   CreateObject(u32)              @ 0x826F80E8  (the RTTI factory hook)
//   `scalar deleting destructor'   @ 0x826F8168  (empty leaf -- all teardown from the
//        inherited ~Brn3DUserSpaceEffectControl / ~Brn3DEffectControl chain)
//
// FLAG (shape vs full surface): the RTTI GetTypeInfo/GetTypeName/GetStaticTypeInfo
// surface is DEFERRED (outside this slice); only the inheritance spine, the ctor, the
// CreateObject factory and the virtual dtor are materialised. X360 byte offsets are
// NOT static_asserted on the 64-bit host (pointer/vptr widths differ).
// =============================================================================
struct Collision3DControl : public BrnSound::Logic::Brn3DUserSpaceEffectControl
{
    Collision3DControl();          // @ 0x826E88D0
    virtual ~Collision3DControl(); // @ 0x826F8168

    // BrnCollisionEffect.h:199 (DWARF) -- the RTTI factory hook (@ 0x826F80E8).
    // Non-virtual, non-const; returns the EffectControl* base view.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_EFFECT_H
