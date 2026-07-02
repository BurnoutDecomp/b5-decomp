#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"

// BrnSound::Logic::Traffic::TrafficStateManager -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.cpp):
//   TrafficStateManager::~TrafficStateManager @0x826FC308
//
// The X360 body is entirely compiler destructor mechanics: the four
// CgsSound::Logic::Content member dtors inlined in reverse declaration order
// (mHornCsisInterface @+0xCC4 first -- each restores the Content vtable
// off_820B3250 and Release()s the held object), then the inlined base
// StateManager teardown (the RegisteredContent<4> ObjectPool dtor + vtable
// walk-down). The C++ source is the empty destructor below; every one of those
// steps is implicit.

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// @ 0x826FC308
TrafficStateManager::~TrafficStateManager()
{
}

}
}
}
