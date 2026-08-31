#ifndef BRN_SOUND_VEHICLES_WHEELS_SKID_EFFECT_H
#define BRN_SOUND_VEHICLES_WHEELS_SKID_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"

namespace BrnSound
{
namespace Vehicles
{
namespace Engines { struct PhysicsControl; }
namespace Wheels
{

struct WheelControl;

// The player-car skid AEMS effect. The member order is the DecFIGS
// BrnSkidEffect.h declaration; ARTIST fixes the controller IDs, voice wiring,
// parameter indices and dynamic-mixer routing used by the implementation.
struct SkidEffect : public BrnSound::Logic::BrnEffectObject
{
    SkidEffect();
    virtual ~SkidEffect();

    virtual const char* GetTypeName() const override;
    virtual s32 GetController(s32 aiSlot) override;
    virtual void AttachController(CgsSound::Logic::EffectBase* apController) override;
    virtual bool Attach() override;
    virtual void UpdateParams(f32 afTimeStep) override;
    virtual void ProcessUpdate() override;
    virtual bool Detach() override;

    void OnPostInit(CgsSound::Logic::VoiceWrapper& arVoice);

private:
    CgsResource::ResourceHandle mDataHandle;
    WheelControl* mpWheelControl;
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;
    f32 mfOverallMax;
    f32 mfSkidAzimuth;
    CgsSound::Utils::DataPoint<bool> mbSkidsLatched;
    CgsSound::Logic::VoiceWrapper mSkidsVoice;
    CgsSound::Logic::VoiceWrapper::FunctorPointer<SkidEffect> mSkidFunctorPointer;
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_SKID_EFFECT_H
