// Explicit instantiation of CgsLocklessQueue<u32>.
// Callers: BrnGameState::RichPresenceManagerX360 (SetContext, PresenceThread,
//          CreatePresenceThread, SetRichPresenceState, SetCurrentPosition, SetDistrict).
// The queue stores 32-bit Xbox Live rich-presence context IDs.
// X360: *>::  @ 0x82369160 (the push instantiation forced here)
#include "GameShared/GameClasses/Containers/CgsLocklessQueue.h"

template class CgsLocklessQueue<u32>;
