#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_CAR_STEREO_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_CAR_STEREO_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::CarStereoEffect
//   GameSource/Sound/Vehicles/Environment/BrnCarStereoEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnCarStereoEffect.h:105-107):
// CarStereoEffect : public BrnEffectObject. The in-car stereo (radio) EFFECT OBJECT;
// embeds a voice wrapper + a back-pointer to the owning sound logic module + a
// has-stereo flag.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE.
// =============================================================================

// mpLogicModule is pointer-only here -> forward declaration avoids a header cascade.
namespace BrnSound { namespace Module { struct SoundLogicModule; } }

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct CarStereoEffect : public BrnSound::Logic::BrnEffectObject
{
    CarStereoEffect();              // @ 0x826D1858
    virtual ~CarStereoEffect();     // anchor for the scalar deleting destructor @ 0x826E62F8

    // @ 0x826E6298 -- RTTI factory hook.
    static CgsSound::Logic::EffectObject* CreateObject( u32 luType );

    bool Attach() override;
    void UpdateParams(f32 afTimeStep) override;
    void ProcessUpdate() override;
    bool Detach() override;

private:
    // True while the player is in free roam (no current game mode).
    bool ShouldPlayMusic();

public:

    // DWARF BrnCarStereoEffect.h:105-107.
    CgsSound::Logic::VoiceWrapper        mVoice;        // @ +0x38
    BrnSound::Module::SoundLogicModule*  mpLogicModule;
    bool                                 mbHasStereo;

    // The number of cars whose stereo is playing right now (capped at two).
    static s32 siNumStereos;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_CAR_STEREO_EFFECT_H
