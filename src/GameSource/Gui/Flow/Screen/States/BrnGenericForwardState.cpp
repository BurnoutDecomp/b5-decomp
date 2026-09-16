#include "GameSource/Gui/Flow/Screen/States/BrnGenericForwardState.h"

// BrnGui::GenericForwardState::maResourcesToLoad @ 0x82F26BB8 (.rdata), read from the
// decrypted image: three {id, type} pairs, every type 4 == E_GUI_RESOURCETYPE_APT, and the
// count word at 0x82F26BD0 reads 3. Ids named through gGuiResourceIdentifier
// (BrnGuiCache.cpp:57). Without this table the loader was handed nothing and the state
// forwarded before its own art existed.
const CgsGui::sResourceTuple BrnGui::GenericForwardState::maResourcesToLoad[3] =
{
    { 217u, CgsGui::E_GUI_RESOURCETYPE_APT },   // Results
    {  70u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5CarsIcon
    {  55u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5ManufacturersIcon
};
const u32 BrnGui::GenericForwardState::muNumResourcesToLoad = 3;

// BrnGui::GenericForwardState::Update @ 0x82500950.
//
// X360 asm is a single tail-call: load "ADVANCE", branch to CgsGui::State::SendStateEvent.
// The state does no per-frame work of its own -- it just requests the flow advance.
void BrnGui::GenericForwardState::Update()
{
    SendStateEvent("ADVANCE");
}
