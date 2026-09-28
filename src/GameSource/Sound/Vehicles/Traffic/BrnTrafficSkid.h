#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_SKID_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_SKID_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                           // EntityId
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"      // BrnEffectObject (base)
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"             // RootInputBuffer::PhysicalTrafficStateQueue
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"       // CgsSound::Logic::VoiceWrapper

// =============================================================================
// BrnSound::Logic::Traffic::TrafficSkid : public BrnSound::Logic::BrnEffectObject
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficSkid.{h,cpp}
//
// Effect 2 of the traffic state: one "AEMS_Skids_Traffic" voice on the PLAYER state
// manager's Skids.abi. It plays only while the car is a physical (simulated) body; the
// drift parameter comes from the car's PhysicalTrafficState wheels and fades out after
// the car has been physical for a while.
//
// Console layout (32-bit; BY NAME on the host): IResourceRequester vptr +0x00, the
// EffectObject sub-object from +0x04, +0x38 mSkidVoice, +0x88 mSkidFunctionPointer,
// +0xA0 mpTrafficControl, +0xA4 mfDriftFactor, +0xA8 mfTimeAsPhysical; sizeof 0xB0.
// =============================================================================

namespace BrnPhysics { namespace Vehicle { struct PhysicalTrafficState; } }

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

struct TrafficControl;

struct TrafficSkid : public BrnSound::Logic::BrnEffectObject
{
    TrafficSkid();
    virtual ~TrafficSkid();

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

    // Called with the queue alone (no `this`) on the console, so it is static.
    static s32 FindPhysicalTrafficState(
        const BrnSound::Module::Io::RootInputBuffer::PhysicalTrafficStateQueue* lpPhysicalTrafficStates,
        EntityId lEntityId );

private:
    void OnPostInitVoice( CgsSound::Logic::VoiceWrapper& lrVoice );

    // Called with the state alone (no `this`) on the console, so it is static.
    static f32 CalculateDriftParameter( const BrnPhysics::Vehicle::PhysicalTrafficState& lrPhysicalTrafficState );

    CgsSound::Logic::VoiceWrapper                                  mSkidVoice;
    CgsSound::Logic::VoiceWrapper::FunctorPointer<TrafficSkid>     mSkidFunctionPointer;
    BrnSound::Logic::Traffic::TrafficControl*                      mpTrafficControl;
    f32                                                            mfDriftFactor;
    f32                                                            mfTimeAsPhysical;
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_SKID_H
