#include "GameSource/Gui/Flow/Screen/States/BrnGenericForwardState.h"

// BrnGui::GenericForwardState::Update @ 0x82500950.
//
// X360 asm is a single tail-call: load "ADVANCE", branch to CgsGui::State::SendStateEvent.
// The state does no per-frame work of its own -- it just requests the flow advance.
void BrnGui::GenericForwardState::Update()
{
    SendStateEvent("ADVANCE");
}
