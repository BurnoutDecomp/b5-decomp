#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"

namespace CgsNetwork
{
// The console ctor only runs the embedded members' construction: the connection manager,
// the ack/nack signal messages and the debug component.
PlayerManager::PlayerManager()
{
}
}
