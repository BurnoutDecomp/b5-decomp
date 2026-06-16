#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

// CgsModule::EventQueue<CgsInput::InputIO::BindResult, 8>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsInput::InputIO::BindResult, 8>::Construct();

// CgsModule::BaseEventQueue<CgsInput::InputIO::BindResult> methods (bodies inline in CgsBaseEventQueue.h).
//   AddEvent  @ 0x828EA960   (called by ProcessBindRequestQueue)
//   Append    @ 0x82369BF0   (called by PreWorldUpdate / BridgeControllerToGameState)
//   GetEvent  @ 0x8235D0A8   (called by ProcessBindResults; ledger short-name "::G")
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::BindResult>::AddEvent(const CgsInput::InputIO::BindResult&);
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::BindResult>::Append(const CgsModule::BaseEventQueue<CgsInput::InputIO::BindResult>&);
template CgsInput::InputIO::BindResult& CgsModule::BaseEventQueue<CgsInput::InputIO::BindResult>::GetEvent(s32);
