#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"

// CgsModule::EventQueue<CgsResource::Events::LoadBundleRequest, 256>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (256) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsResource::Events::LoadBundleRequest, 256>::Construct();
