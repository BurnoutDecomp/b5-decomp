// Per-instantiation .cpp for Array<MainGameFlowState*, 7u>. The generic
// Array<T,N> body is fully inline in CgsArray.h, so this TU is just the
// explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The method the X360 emitted for this instance:
//   Array<MainGameFlowState*,7>::GetItem @ 0x823B09E8
//       (BrnGameMainFlowController::GameMainFlowController::Construct,
//        ::SendEvent, BrnGame::BrnGameModule::GameMain,
//        MainGameFlowStateCompleteLoading::Update, MainGameFlowStateInGame::Update)
//
// Layout: maElements[7] (7 * 4 = 28B; MainGameFlowState* is a 32-bit pointer on X360)
// + miCount @ +0x1C, matching the X360 count word read at *(this+28) in GetItem
// and the 4-byte element stride (return 4*index + base in the asm).
//
// Spelled unqualified to match the committed Array<T,N> container convention
// (CgsArray.h); the DWARF spells it CgsContainers::Array<MainGameFlowState*,7u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

template class Array<MainGameFlowState*, 7u>;
