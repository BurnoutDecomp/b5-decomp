#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // CgsModule::BaseEventQueue<T>::GetEvent const (inline generic)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                        // CompletedFburnChallengesData (264B element, already committed)

// CgsModule::BaseEventQueue<...CompletedFburnChallengesData>::GetEvent(s32) const  @ 0x82318998
// Explicit instantiation only -- the checked const accessor body lives inline in the committed
// CgsBaseEventQueue.h (const T& GetEvent(s32) const, line 78). Asserts mpEvents != NULL (:272),
// liIndex < GetLength() (:274), liIndex >= 0 (:275), then returns &mpEvents[liIndex] via
// `mulli r11, r29, 0x108; add r3, r11, mpEvents` == liIndex*264 + mpEvents. The 264-byte stride
// (0x108) == sizeof(CompletedFburnChallengesData) (s32 mNetworkPlayerID@0x00 + 4B pad +
// FastBitArray<2000> 256B @0x08 == 264B), the same value baked into the sibling AddEvent
// (0x8258C160) and Append (0x823C46A8) instantiations for this type. mpEvents @ +0 (lwz r11,0),
// miLength @ +8 (lwz r11,8 == a1[2]). Called by
// BrnGameState::ChallengeManager::UpdateRemotePlayerSuccessStatus.
template const BrnGameState::GameStateModuleIO::CompletedFburnChallengesData&
CgsModule::BaseEventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData>::GetEvent(s32) const;
