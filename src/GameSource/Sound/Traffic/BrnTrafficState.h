#ifndef BRN_SOUND_LOGIC_TRAFFIC_STATE_H
#define BRN_SOUND_LOGIC_TRAFFIC_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficState
//   DWARF home: GameSource/Sound/Vehicles/Traffic/BrnTrafficState.{h,cpp}.
//   PROJECT home: GameSource/Sound/Traffic/ -- co-located with the committed
//   sibling BrnTrafficStateManager (the BrnSound::Logic::Traffic family's home in
//   this tree). Keeping the state and its manager together is the project choice.
//
// DWARF (references/DecFIGS/dwarfdump/.../BrnTrafficState.h) confirms:
//   BrnSound::Logic::Traffic::TrafficState : public BrnSound::Logic::BrnState
// with per-class RTTI hooks GetTypeInfo()/GetTypeName(), plus deferred members
// (mpTrafficEntity, Attach, CreateObject, GetStaticTypeInfo) NOT in this
// destructor's scope. This TU bodies only the destructor (@ 0x826CB1D0).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// Same vtable pair (off_820AE1F4 own-vtable install / off_820AA820 MemBase base
// re-install) as the sibling GlobalState @ 0x826D2250, and DWARF-confirmed
// TrafficState : public BrnState.
struct TrafficState : public BrnSound::Logic::BrnState
{
    TrafficState() {}

    // @ 0x826CB1D0 -- vector deleting destructor. Bodied out-of-line in
    // BrnTrafficState.cpp; observable body is the DestroyEffects() call, the
    // vtable installs + conditional allocator free are the compiler-synthesised
    // deleting-destructor parts (host `delete` stands in for off_82FFB954).
    virtual ~TrafficState();

    // Per-class RTTI hooks (vtable shape only; DEFERRED bodies, not in this
    // function's scope -- mirrors GlobalState's GetTypeInfo/GetTypeName split).
    // DWARF-confirmed return type/virtualness/const.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;

    // @ 0x826CB270 -- null-asserts the attachment, records it in mpTrafficEntity (+0x54),
    // then chains to the base State::Attach (reused BY NAME, the DestroyEffects idiom).
    virtual void Attach(void* lpvAttachment);

    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* GetTrafficEntity() const
    {
        return mpTrafficEntity;
    }

private:
    // +0x54  DWARF: const TrafficStateManager::Slot::TrafficSoundEntity* (un-homed -> const void*).
    const BrnTraffic::BrnTrafficIO::TrafficSoundEntity* mpTrafficEntity;
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_STATE_H
