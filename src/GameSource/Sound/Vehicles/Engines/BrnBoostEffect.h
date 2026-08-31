#ifndef BRN_SOUND_VEHICLES_ENGINES_BOOST_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_BOOST_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// =============================================================================
// BrnSound::Vehicles::Engines::BoostEffect
//   GameSource/Sound/Vehicles/Engines/BrnBoostEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BoostEffect is the boost/turbo engine
// sound EFFECT OBJECT. Its X360 deleting destructor installs/tears down the same
// BrnEffectObject dual-vptr pair as the committed siblings (LoopModelEffect /
// CollisionEffect), so it reuses the committed BrnEffectObject base BY NAME.
//
// The ARTIST IStreamUser inheritance, voice/create parameters, AEMS controls,
// boost timing DataPoints, and physics/speed-stream controller links are homed
// here with their DecFIGS declaration shape.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment { struct SpeedStreamControl; }
namespace Engines
{

struct PhysicsControl;

struct BoostEffect : public BrnSound::Logic::BrnEffectObject,
                     public BrnSound::Logic::Streaming::IStreamUser
{
    BoostEffect();
    virtual ~BoostEffect();

    virtual const char* GetTypeName() const;
    virtual s32 GetController(s32 aiSlot);
    virtual void AttachController(CgsSound::Logic::EffectBase* apController);
    virtual void SetupLoadData();
    virtual bool Attach();
    virtual void UpdateParams(f32 afTimeStep);
    virtual void ProcessUpdate();
    virtual bool Detach();

    virtual const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const;
    virtual void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                   f32 afGain, f32 afElapsedTime);

    void OnPostInit(CgsSound::Logic::VoiceWrapper& arVoice);

private:
    void UpdateAemsBoostParameters();
    void UpdateBoostStream();

    CgsSound::Logic::VoiceWrapper mBoostVoice;
    CgsSound::Logic::VoiceWrapper::FunctorPointer<BoostEffect> mBoostFunctionPointer;
    CgsSound::Logic::VoiceWrapper::CreateParams mParams;
    f32 mfParam_AEMS_velocity;
    f32 mfParam_AEMS_start_stage_2;
    f32 mfParam_AEMS_boost_remaining;
    f32 mfParam_AEMS_car_speed;
    f32 mfParam_AEMS_volume;
    f32 mfParam_AEMS_control;
    f32 mfParam_AEMS_time_since_last_boostin;
    f32 mfParam_AEMS_time_since_last_boostout;
    f32 mfParam_AEMS_time_boosting;
    f32 mfParam_AEMS_is_boost_blue;
    f32 mfParam_AEMS_skid_intensity;
    CgsSound::Utils::DataPoint<f32> mTimeOfLastBoostOut;
    CgsSound::Utils::DataPoint<f32> mTimeOfLastBoostIn;
    CgsSound::Utils::DataPoint<f32> mTimeInBoost;
    PhysicsControl* mpPhysicsControl;
    BrnSound::Vehicles::Environment::SpeedStreamControl* mpSpeedStreamControl;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_BOOST_EFFECT_H
