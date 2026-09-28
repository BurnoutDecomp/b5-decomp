#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"    // BrnEffectObject (TrafficEngine base)
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h" // Brn3DEffectControl (Traffic3DControl base)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"     // CgsSound::Logic::VoiceWrapper

// =============================================================================
// GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.{h,cpp}
//
// Traffic3DControl : public BrnSound::Logic::Brn3DEffectControl
//   Control 1 of the traffic state, the car's 3D emitter. Nothing of its own: the
//   console CreateObject allocates 0xD0 bytes, runs the Brn3DEffectControl constructor
//   and installs the leaf vtable (the Passby3DControl shape).
//
// TrafficEngine : public BrnSound::Logic::BrnEffectObject
//   Effect 0 of the traffic state: one "AEMS_TrafficEngineClass" voice on the manager's
//   Traffic_Bank.abi, started when the car's engine is on, fed each frame with the
//   dynamic-mixer outputs, the car's speed and its size class.
//   Console layout (32-bit; BY NAME on the host): IResourceRequester vptr +0x00, the
//   EffectObject sub-object from +0x04, +0x38 mu16AemsPatchTrafficIndex,
//   +0x3C mpTrafficControl, +0x40 mpTraffic3DControl, +0x44 mTrafficEngineVoice;
//   sizeof 0x94.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

struct TrafficControl;

struct Traffic3DControl : public BrnSound::Logic::Brn3DEffectControl
{
    Traffic3DControl() {}
    virtual ~Traffic3DControl();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );
};

struct TrafficEngine : public BrnSound::Logic::BrnEffectObject
{
    TrafficEngine();
    virtual ~TrafficEngine();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject( u32 luType );

    virtual s32  GetController( s32 liIndex );
    virtual void AttachController( CgsSound::Logic::EffectBase* lpController );
    virtual bool Attach();
    virtual void UpdateParams( f32 lfTimeStep );
    virtual void ProcessUpdate();
    virtual bool Detach();

private:
    u16                                         mu16AemsPatchTrafficIndex;
    BrnSound::Logic::Traffic::TrafficControl*   mpTrafficControl;
    BrnSound::Logic::Traffic::Traffic3DControl* mpTraffic3DControl;
    CgsSound::Logic::VoiceWrapper               mTrafficEngineVoice;
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H
