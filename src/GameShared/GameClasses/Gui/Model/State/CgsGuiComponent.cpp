#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"   // StateInterface, GuiAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                       // GuiAccessPointers (mpAptAux)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"        // CgsGui::AptAux (UpdateFlashComponent)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                    // CgsCore::SPrintf
#include "GameShared/GameClasses/Containers/CgsHash.h"                     // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // strncpy, strlen

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
// (2026-07-09, step 6: the title-bring-up fallback that mirrored each (key, value)
// through the BrnGui Apt runtime bridge is RETIRED -- the AS framework movie's
// component registration is live, so the faithful chain below IS the drive:
// FillAptViewMessage -> AptAux::UpdateFlashComponent -> AptCommunicator::
// UpdateComponent, flushed per frame by UpdateAllComponents into the movie AS.)

namespace CgsGui
{
    // @ 0x8285BB48 GuiComponent::Construct (not exported; the body is the composition of
    // its two homed callees, and the XB1 x64 arbiter shows the inlined result: the name
    // lands in macName[+8 after the vptr] and the interface behind it). Name + hash via
    // SetName @0x828488E8, then store the state interface via SetStateInterface
    // @0x82847030 (which asserts it non-null -- every X360 construct site passes one).
    void GuiComponent::Construct(const char* lpacName, StateInterface* lpStateInterface,
                                 const char* lpacParentName)
    {
        SetName(lpacName, lpacParentName);
        SetStateInterface(lpStateInterface);
    }

    // @ 0x8285BB48-adjacent (not exported). Post this component's (key, value) apt output:
    // the XB1 x64 arbiter inlines the whole chain to
    //   AptCommunicator::UpdateComponent(this->macName, key, value)
    // (e.g. XB1 sub_1401AF470: UpdateComponent(this, this+8, "SignName", value)), i.e.
    // AddOutputAptViewState(key, value, immediate) == FillAptViewMessage(GetName(), key,
    // value, immediate). The lpacAptName/lpacViewState parameter names below are the
    // DWARF's (they carry the KEY / VALUE of the component protocol).
    void GuiComponent::AddOutputAptViewState(const char* lpacAptName, const char* lpacViewState,
                                             bool lbImmediate)
    {
        FillAptViewMessage(GetName(), lpacAptName, lpacViewState, lbImmediate);
    }
    // @ 0x82847030 - assert the incoming state interface is non-null, then store it
    // in mpStateInterface (the X360 writes it at +0x88). Path/line from the baked
    // assert string (GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h:169).
    void GuiComponent::SetStateInterface(StateInterface* lpStateInterface)
    {
        CGS_ASSERT(lpStateInterface != nullptr,
                   "Invalid state interface sent to GuiComponent::SetStateInterface");
        mpStateInterface = lpStateInterface;
    }

    // @ 0x828583A8 - post an apt-view-state transition for this component through the
    // state interface's access pointers. The X360 body asserts mpStateInterface (+0x88)
    // is non-null, then reaches the AptAux via mpStateInterface->mpAccessPointers
    // (StateInterface +4, itself asserted non-null) and calls
    // AptAux::UpdateFlashComponent(mpAccessPointers->mpAptAux, ...) — the first member of
    // GuiAccessPointers (**mpAccessPointers).
    void GuiComponent::FillAptViewMessage(const char* lpacAptName, const char* lpacViewState,
                                          const char* lpacParam, bool lbImmediate)
    {
        CGS_ASSERT(mpStateInterface != nullptr, "mpStateInterface");
        // GetAccessPointers() reproduces the "mpAccessPointers != NULL" assert and the
        // +4 load the X360 inlined here.
        GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        AptAux::UpdateFlashComponent(lpAccessPointers->mpAptAux, lpacAptName, lpacViewState,
                                     lpacParam, lbImmediate);
    }

    // @ 0x828488E8 - build the component name and cache its hash.
    //   * Asserts lpacName non-null ("Invalid name sent to GuiComponent::SetName", :90).
    //   * If a parent name is supplied, the stored name is "<parent>_<name>"
    //     (SPrintf "%s_%s", parent first then name); the combined length (strlen(parent)
    //     + strlen(name) + 2 for the '_' and terminator) must stay under 128 (:94).
    //   * Otherwise the name is copied verbatim into macName[128] (the inlined StrCpy,
    //     whose debug bounds-check fires "String too long" at CgsStringUtils.h:55).
    //   * Finally the name is hashed into muHashedName (+0x84) and that hash returned.
    u32 GuiComponent::SetName(const char* lpacName, const char* lpacParentName)
    {
        CGS_ASSERT(lpacName != nullptr, "Invalid name sent to GuiComponent::SetName");

        if (lpacParentName != nullptr)
        {
            const u32 luNameLen   = static_cast<u32>(strlen(lpacName));
            const u32 luParentLen = static_cast<u32>(strlen(lpacParentName));
            CGS_ASSERT((luParentLen + luNameLen + 2) < 0x80, "Component name generated is too long");
            CgsCore::SPrintf(macName, KU_MAX_COMPONENT_NAME_LEN, "%s_%s", lpacParentName, lpacName);
        }
        else
        {
            // X360 checks the name length twice here: first against 0x7F with the same
            // "Component name generated is too long" message as the with-parent branch
            // (CgsGuiComponent.cpp:99), then the inlined StrCpy re-checks against 0x80
            // with "String too long: " (CgsStringUtils.h:55) before the 128-char strncpy.
            CGS_ASSERT(strlen(lpacName) < 0x7F, "Component name generated is too long");
            CGS_ASSERT(strlen(lpacName) < 0x80,
                       "String too long: ");
            strncpy(macName, lpacName, KU_MAX_COMPONENT_NAME_LEN);
        }

        muHashedName = CgsContainers::CgsHash::CalculateHash(macName, static_cast<s32>(strlen(macName)));
        return muHashedName;
    }
}
