// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BaseEventQueue_DirtyTrickEvent_GetEvent.cpp
// ============================================================================
// CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent>::GetEvent(s32)  @ 0x823186F0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnGameState::ScoringSystem::UpdatePaybackTakedowns to index the payback DirtyTrickQueue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:292), liIndex < GetLength() (:294)
// and liIndex >= 0 (:295) -- the NON-const GetEvent(int) overload (header decl line 90) -- then
// returns &mpEvents[liIndex] (result = 16*liIndex + mpEvents; slwi r11,liIndex,4). The 16-byte
// stride is sizeof(DirtyTrickEvent) == 4 x 4-byte enum. The Hex-Rays `int` return is the
// ABI-returned T& pointer; DWARF gives the real DirtyTrickEvent&.

#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"

template BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent&
CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent>::GetEvent(s32);
