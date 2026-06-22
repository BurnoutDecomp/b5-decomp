// Embed check for CgsModule::EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent, 24>.
// Instantiates the queue, exercises Construct/AddEvent/Append, and reads members BY NAME so a
// layout/signature regression in the home or generic fails the gate. Not part of the game build.
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"

static_assert(sizeof(BrnWorld::CrashIO::CrashingTrafficUpdateEvent) == 80,
              "CrashingTrafficUpdateEvent must be 80 bytes (u16 head + 16-aligned Matrix44Affine tail)");

int CrashingTrafficUpdateEvent_embed_check()
{
    using namespace BrnWorld::CrashIO;

    CgsModule::EventQueue<CrashingTrafficUpdateEvent, 24> lDest;
    CgsModule::EventQueue<CrashingTrafficUpdateEvent, 24> lSource;
    lDest.Construct();
    lSource.Construct();

    CrashingTrafficUpdateEvent lEvent;
    lEvent.muVehicleId = 7u;
    lEvent.mTransform.wAxis.x = 1.0f;

    lSource.AddEvent(lEvent);          // BaseEventQueue<T>::AddEvent @ 0x825581E0
    lDest.Append(lSource);             // BaseEventQueue<T>::Append    @ 0x823C4148

    return lDest.GetLength() + lDest.GetMaxLength();
}
