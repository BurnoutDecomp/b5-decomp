#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::BootCompoundLoad - the "BF_COMPLOAD" boot HUD flow state (one of the 14 states the
// HUD flow pool owns; built by BrnHudFlow::Prepare @0x8251A620 as the size-72 / vtable
// off_820752C8 slot, between BF_ATTR and BF_PROFILE). Derives from CgsGui::State.
//
// FLAG (name inferred): unlike the other 13 HUD states, this state has NO file in the PS3
// DecFIGS DWARF (no Brn*CompoundLoad*.h, no "compound load" class), so its exact class/file
// name is not ground-truthed - it is reconstructed from the X360 CgsID "BF_COMPLOAD" (a
// compound/secondary load step) and the BrnHudFlow::Prepare slot. Should DWARF or a PDB later
// supply the real name, rename this file + struct accordingly.
//
// PHASE NOTE (boot-path push): the boot path reconstructed in this push sequences
// BF_PRELOAD->BF_LOADING->BF_VIDEOS->BF_LEGAL->BF_PROFILE; BF_COMPLOAD is part of the boot
// chain but its body lands with the other boot-state bodies. This is a faithful class shell so
// the HUD flow's 14-state pool is structurally complete and instantiable (base virtuals are
// non-pure -> constructible via CgsGui::State::Construct(id, fsm)).
namespace BrnGui
{
    struct BootCompoundLoad : public CgsGui::State
    {
    };
}
