#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_CONTROL_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficControl : public BrnSound::Logic::BrnEffectControl
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.{h,cpp}
//
// Control 0 of the traffic state: it binds to the state's traffic entity, points the
// Traffic3DControl emitter at the entity's position, feeds the entity's crash flag to
// mixer input 0, and posts ONE traffic passby per attachment when the car closes on the
// player microphone fast enough.
//
// Console layout (32-bit; BY NAME on the host): IResourceRequester vptr +0x00, the
// EffectControl sub-object from +0x04, +0x38 mbPassbyTriggered, +0x3C mpTrafficEntity,
// +0x40 mpTraffic3dControl; sizeof 0x44. The console constructor (inlined into
// CreateObject) initialises only the base, not these three members.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

struct Traffic3DControl;

struct TrafficControl : public BrnSound::Logic::BrnEffectControl
{
    TrafficControl() {}
    virtual ~TrafficControl();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );

    virtual s32  GetController( s32 liIndex ) override;
    virtual void AttachController( CgsSound::Logic::EffectBase* lpController ) override;
    virtual bool Attach() override;
    virtual void UpdateParams( f32 lfTimeStep ) override;

    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* GetTrafficEntity() const
    {
        return mpTrafficEntity;
    }

private:
    bool                                                mbPassbyTriggered;
    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* mpTrafficEntity;
    Traffic3DControl*                                   mpTraffic3dControl;
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_CONTROL_H
