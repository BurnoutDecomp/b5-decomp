#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_EFFECT_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // BrnSound::Logic::BrnEffectObject (committed base, reused BY NAME)
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

    // @ 0x82688138 -- collision crash gain. Bodied in BrnCollisionEffect.cpp.
    // DWARF (BrnCollisionEffect.cpp:231) declares this `float GetGain() const`.
    f32 GetGain() const;

    // ---- DEFERRED virtuals (declared for the effect vtable shape; outside this
    // TU's recon'd function set -- not bodied by this group). The DWARF home lists
    // GetTypeInfo / GetTypeName / CreateObject / Prepare / GetController /
    // AttachController / Attach / UpdateParams / ProcessUpdate / Detach /
    // GetPitch / CalculateIntensity; none are in this TU's dossier, so they are
    // NOT declared here to avoid fabricating a vtable shape the binary does not
    // pin in this slice.

private:
    // --- members (DWARF order; X360 offsets in comments only, not asserted) ---

    // BrnCollisionEffect.h:151 (DWARF): the owning collision control. The X360
    // GetGain @ 0x82688138 reads THIS member slot (+0x34) and dispatches
    // Nicotine::DMixIO::GetDMixOutput through it -- i.e. at runtime the slot holds
    // the latched dynamic-mixer I/O the crash voice reads its output level from.
    // CollisionControl is not yet homed, so the member is modelled as the
    // Nicotine::DMixIO* the ASM actually dereferences (GetDMixOutput is a homed
    // member of Nicotine::DMixIO).
    // FLAG: DWARF names this member `mpCollisionControl` (BrnSound::Logic::Collision::
    // CollisionControl*); the X360 GetGain uses the +0x34 slot directly as the
    // Nicotine::DMixIO receiver. Per the source-of-truth ladder the ASM (rung 1)
    // overrides the DWARF name (rung 2): the slot is modelled by its proven runtime
    // type (Nicotine::DMixIO*) so GetGain can be bodied without re-homing the
    // un-reconstructed CollisionControl. The ctor nulls it (X360 stw 0,+0x34).
    Nicotine::DMixIO* mpCollisionDMixIo;

    // BrnCollisionEffect.h:152 (DWARF). Voice bring-up state. The X360 ctor leaves
    // this in the +0x38 region (one of the zero stores); init to CONSTRUCT_VOICE.
    EPrepareState mePrepareState;

    // BrnCollisionEffect.h:154/155 (DWARF). First-update + azimuth-use flags; both
    // zeroed by the ctor (the +0x3C zero-store region).
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

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_EFFECT_H
