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
    // SpeedStreamEffect() is implicit (DWARF: the class line h:152); ARTIST 0x826BA148.
    // No user-declared destructor: DWARF declares ~SpeedStreamEffect without a line (implicit); ARTIST's
    // deleting destructor stores no SpeedStreamEffect vftable.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 luAllocator);
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo;   // DWARF static data member (ARTIST 0x82F2F7E8)

    s32 GetController(s32 aiIndex) ;
    void AttachController(CgsSound::Logic::EffectBase* apController) ;
    void UpdateParams(f32 afTimeStep) ;
    bool Detach() ;

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const ;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) ;

    CgsSound::Logic::VoiceWrapper::CreateParams mParams;
    SpeedStreamControl* mpSpeedStreamControl;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_SPEED_STREAM_EFFECT_H
