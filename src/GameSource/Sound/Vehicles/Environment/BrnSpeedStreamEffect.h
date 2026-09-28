#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_SPEED_STREAM_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_SPEED_STREAM_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // committed IStreamUser third base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::SpeedStreamEffect
//   GameSource/Sound/Vehicles/Environment/BrnSpeedStreamEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The wind/speed-stream EFFECT OBJECT.
// The X360 ctor installs THREE leaf vptrs (this+0 primary + this+4 IResourceRequester
// == the committed BrnEffectObject dual base; this+0x38 == the IStreamUser interface
// sub-object), so it multiply-inherits the committed BrnEffectObject + the committed
// BrnSound::Logic::Streaming::IStreamUser, matching the committed SpeechEffect /
// PresentationEffect / StreamingEffect triple-base pattern.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct SpeedStreamControl;

struct SpeedStreamEffect : public BrnSound::Logic::BrnEffectObject,
                           public BrnSound::Logic::Streaming::IStreamUser
{
    SpeedStreamEffect();            // @ 0x826BA148
    virtual ~SpeedStreamEffect();   // anchor for the scalar deleting destructor @ 0x826BA1F8

    // @ 0x826D13D8 -- static allocate+construct factory. Returns the +4 IResourceRequester view.
    static BrnSound::Logic::IResourceRequester* Create( int aiFlavour );

    s32 GetController(s32 aiIndex) override;
    void AttachController(CgsSound::Logic::EffectBase* apController) override;
    void UpdateParams(f32 afTimeStep) override;
    bool Detach() override;

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) override;

    CgsSound::Logic::VoiceWrapper::CreateParams mParams;
    SpeedStreamControl* mpSpeedStreamControl;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_SPEED_STREAM_EFFECT_H
