#ifndef BRN_SOUND_LOGIC_TRAFFIC_STATE_H
#define BRN_SOUND_LOGIC_TRAFFIC_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"   // TrafficStateManager (GetTrafficStateManager)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficState : public BrnSound::Logic::BrnState
//   Declared home: GameSource/Sound/Vehicles/Traffic/BrnTrafficState.{h,cpp}.
//   Project home: GameSource/Sound/Traffic/ (BrnInAirEffect.cpp includes it here).
//
// One of the TrafficStateManager's six states. TrafficStateManager::AttachEntity hands
// it the address of a slot's TrafficSoundEntity copy; its controls and effects read the
// entity through GetTrafficEntity().
//
// Console layout (32-bit; BY NAME on the host): the State base to +0x54,
// +0x54 mpTrafficEntity; sizeof 0x58 (CreateObject allocates 88 bytes).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

struct TrafficState : public BrnSound::Logic::BrnState
{
    // The console CreateObject inlines this: the State base zero-inits, the vtable,
    // and mpTrafficEntity = 0.
    TrafficState() : mpTrafficEntity(0) {}

    virtual ~TrafficState();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();
    static CgsSound::Logic::State* CreateObject( u32 luType );

    virtual void Attach( void* lpvAttachment );

    TrafficStateManager* GetTrafficStateManager()
    {
        return static_cast<TrafficStateManager*>( GetStateManager() );
    }

    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* GetTrafficEntity() const
    {
        return mpTrafficEntity;
    }

private:
    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* mpTrafficEntity;
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_STATE_H
