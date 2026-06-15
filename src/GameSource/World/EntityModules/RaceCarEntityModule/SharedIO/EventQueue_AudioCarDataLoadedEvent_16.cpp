#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

// CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent, 16>::Construct  @ 0x822E3670
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (16) queue of audio
// car-data-loaded events: Construct points the base queue at its inline maEvents
// buffer, sets the max length (16), and clears the live count. The X360 build
// asserts the buffer is non-null before storing it (CgsBaseEventQueue.h); that
// assert lives in BaseEventQueue::Construct.
template void CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent, 16>::Construct();
