// Per-instantiation .cpp for FifoQueue<f32,10>. The generic body is fully inline in
// CgsFifoQueue.h, so this TU is just the explicit class instantiation (the X360 emits one
// out-of-line copy per using-TU; FifoQueue<float,10>::Push @ 0x8235E7C0, called by
// BrnGameState::DeveloperChallengeManager::OnTakedown). f32 == float (types.hpp).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::FifoQueue<float,10u>.
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"

template class FifoQueue<f32, 10>;
