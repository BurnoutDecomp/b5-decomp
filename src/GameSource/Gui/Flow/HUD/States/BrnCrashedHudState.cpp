// ===================================================================================
// BrnGui::CrashedHudState  -- the "CRASHED" HUD flow state (impact-time slice)
//   class:BrnGui::CrashedHudState
//
//   SetExpectedComponent   @ 0x82473780
//   EnterSteerWreckScreen  @ 0x82473868
//   EnterImpactTimeScreen  @ 0x824738C0
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedHudState.h"

#include "GameShared/GameClasses/Containers/CgsHash.h"    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

#include <cstring>   // std::strlen

namespace BrnGui
{
    // @ 0x82473780 -- hash the component name and append it to the expected-apt-component id list.
    // DWARF declares this void; the X360 leaves the hash in r3 (the id it just stored). The
    // observable effect is the store into mauExpectedComponentIds[count] + count++.
    void CrashedHudState::SetExpectedComponent(const char* lpacComponentName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        // X360: inline strlen (char* walk to the NUL) then CalculateHash(name, len).
        const s32 liLength = static_cast<s32>(std::strlen(lpacComponentName));
        const u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacComponentName), liLength);

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
    }

    // @ 0x82473868 -- switch the impact-time apt page to "SteerWreck" and show the LTHUMB glyph.
    void CrashedHudState::EnterSteerWreckScreen()
    {
        ImpactTimePageChanger().AddOutputAptViewState("apt_Transition", "SteerWreck", false); // this+0x4B8
        ImpactTimeButton().SetButton(ButtonIconComponent::E_PADBUTTON_LTHUMB,
                                     ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);          // this+0x544, (13, 0)
    }

    // @ 0x824738C0 -- switch the impact-time apt page to "ImpactTime" and show the SELECT glyph.
    void CrashedHudState::EnterImpactTimeScreen()
    {
        ImpactTimePageChanger().AddOutputAptViewState("apt_Transition", "ImpactTime", false); // this+0x4B8
        ImpactTimeButton().SetButton(ButtonIconComponent::E_PADBUTTON_SELECT,
                                     ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);          // this+0x544, (4, 0)
    }
}
