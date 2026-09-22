#include "CgsPlayersConnectionManager.h"

namespace CgsNetwork
{
// The console ctor only seeds the embedded members of the seven entries: the test-connection
// and connection-status message objects and the test timestamp (CgsSystem::Time's own ctor).
PlayersConnectionManager::PlayersConnectionManager()
{
}
}
