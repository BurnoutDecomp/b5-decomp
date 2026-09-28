#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // IStreamUser third base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceEffect
//   GameSource/Sound/Vehicles/Environment/BrnAmbienceEffect.{h,cpp}  (DWARF home)
//
// The per-car environmental ambience EFFECT OBJECT. Like the speed and crash stream
// effects it is a BrnEffectObject (primary + IResourceRequester sub-objects) plus an
// IStreamUser interface sub-object: it streams the ambience bed of the region the
// ambience control reports, fading each new bed in over two seconds.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct AmbienceControl;

struct AmbienceEffect : public BrnSound::Logic::BrnEffectObject,
                        public BrnSound::Logic::Streaming::IStreamUser
{
    AmbienceEffect();
    virtual ~AmbienceEffect();

    // Effect-object factory hook. Returns the IResourceRequester view.
    static BrnSound::Logic::IResourceRequester* CreateObj( u32 luFlavour );

    s32 GetController(s32 aiIndex) override;
    void AttachController(CgsSound::Logic::EffectBase* apController) override;
    void UpdateParams(f32 afTimeStep) override;
    bool Detach() override;

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) override;

    // Members in declaration order.
    CgsSound::Logic::VoiceWrapper::CreateParams mParams;
    AmbienceControl*                            mpAmbienceControl;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H
