// Explicit instantiation of CgsContainers::LocklessQueue over a 32-bit payload, so the generic
// Post / Pop / Lock / Unlock bodies compile on their own. The game's one instantiation (the rich
// presence manager's context-pointer queues) is generated where it is used.
#include "GameShared/GameClasses/Containers/CgsLocklessQueue.h"

template struct CgsContainers::LocklessQueue<u32>;
