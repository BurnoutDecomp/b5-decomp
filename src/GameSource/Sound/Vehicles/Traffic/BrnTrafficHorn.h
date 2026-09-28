#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_HORN_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_HORN_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"      // BrnEffectObject (base)
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h" // ETrafficSize
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"       // CgsSound::Logic::VoiceWrapper

// =============================================================================
// BrnSound::Logic::Traffic::TrafficHorn : public BrnSound::Logic::BrnEffectObject
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficHorn.{h,cpp}
//
// Effect 1 of the traffic state: one "AEMS_class_horns" voice on the manager's
// patch_bank_horns.abi. Attach only creates it; ProcessUpdate plays it when the car's
// alarm is on (patch mode 3) or when its horn starts (patch mode 0), and feeds the
// horn / alarm mixer outputs and the beep flag every frame.
//
// Console layout (32-bit; BY NAME on the host): IResourceRequester vptr +0x00, the
// EffectObject sub-object from +0x04, +0x38 mHornVoice, +0x88 mHornFunctionPointer,
// +0xA0 mpTrafficControl, +0xA4 meTrafficSize, +0xA8 mfAemsPatchMode,
// +0xAC mbPrevHornState; sizeof 0xB0. The console constructor constructs the voice
// wrapper and the functor's vtable only.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

struct TrafficControl;

struct TrafficHorn : public BrnSound::Logic::BrnEffectObject
{
    TrafficHorn();
    virtual ~TrafficHorn();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject( u32 luType );

    virtual s32  GetController( s32 liIndex );
    virtual void AttachController( CgsSound::Logic::EffectBase* lpController );
    virtual void UpdateParams( f32 lfTimeStep );
    virtual void ProcessUpdate();
    virtual bool Attach();
    virtual bool Detach();

    void OnPostInitVoice( CgsSound::Logic::VoiceWrapper& lrVoice );

private:
    void SetAemsTypeParameter();

    CgsSound::Logic::VoiceWrapper                                  mHornVoice;
    CgsSound::Logic::VoiceWrapper::FunctorPointer<TrafficHorn>     mHornFunctionPointer;
    BrnSound::Logic::Traffic::TrafficControl*                      mpTrafficControl;
    BrnSound::Logic::Traffic::ETrafficSize                         meTrafficSize;
    f32                                                            mfAemsPatchMode;
    bool                                                           mbPrevHornState;
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_HORN_H
