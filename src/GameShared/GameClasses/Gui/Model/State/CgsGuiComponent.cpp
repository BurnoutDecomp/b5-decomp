#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"   // StateInterface, GuiAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                       // GuiAccessPointers (mpAptAux)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"        // CgsGui::AptAux (UpdateFlashComponent)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                    // CgsCore::SPrintf
#include "GameShared/GameClasses/Containers/CgsHash.h"                     // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"               // CGS_ASSERT_STREAM messages

#include <cstring>   // strncpy, strlen

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
// (2026-07-09, step 6: the title-bring-up fallback that mirrored each (key, value)
// through the BrnGui Apt runtime bridge is RETIRED -- the AS framework movie's
// component registration is live, so the faithful chain below IS the drive:
// FillAptViewMessage -> AptAux::UpdateFlashComponent -> AptCommunicator::
// UpdateComponent, flushed per frame by UpdateAllComponents into the movie AS.)
//
// LINE LAYOUT: the definitions sit on their DecFIGS DWARF lines (CgsGuiComponent.h dump:
// AddOutputAptViewState cpp:48, FillAptViewMessage cpp:70, SetName cpp:88, Construct
// cpp:117), in that order; ARTIST bakes the asserts' __LINE__ (AddOutputAptViewState's
// four streamed asserts li r5,0x32..0x35 == 50..53, FillAptViewMessage's li r5,0x48 == 72).

namespace CgsGui
{

















    // @ 0x8285BB48 (an export hole: no IDA JSON; ppcdis). Four streamed asserts on cpp:50-53
    // (texts 0x820E4658/4620/45E8/45A8) then this component's (key, value) apt output
    // through FillAptViewMessage(GetName(), key, value, immediate). The XB1 x64 arbiter
    // inlines the chain to AptCommunicator::UpdateComponent(this->macName, key, value).
    void GuiComponent::AddOutputAptViewState(const char* lpacAptName, const char* lpacViewState, bool lbImmediate)
    {
        CGS_ASSERT_STREAM(GetName() != NULL, "Name not set in GuiComponent::AddOutputAptViewState");
        CGS_ASSERT_STREAM(lpacAptName != NULL, "Invalid Variable in GuiComponent::AddOutputAptViewState");
        CGS_ASSERT_STREAM(lpacViewState != NULL, "Invalid value in GuiComponent::AddOutputAptViewState");
        CGS_ASSERT_STREAM(mpStateInterface != NULL, "Invalid state interface in GuiComponent::AddOutputAptViewState");
        FillAptViewMessage(GetName(), lpacAptName, lpacViewState, lbImmediate);
    }











    // @ 0x828583A8 - post an apt-view-state transition through the state interface's access
    // pointers: AptAux::UpdateFlashComponent(mpAccessPointers->mpAptAux, ...). ARTIST inlines
    // StateInterface::GetAccessPointers (CgsGuiStateInterface.h:344 assert).
    void GuiComponent::FillAptViewMessage(const char* lpacAptName, const char* lpacViewState, const char* lpacParam, bool lbImmediate)
    {
        CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");   // cpp:72 (li r5,0x48)
        // GetAccessPointers() reproduces the "mpAccessPointers != NULL" assert and the
        // +4 load the X360 inlined here.
        GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        AptAux::UpdateFlashComponent(lpAccessPointers->mpAptAux, lpacAptName, lpacViewState,
                                     lpacParam, lbImmediate);
    }








    // @ 0x828488E8 - build the component name ("<parent>_<name>" or the name) and cache its hash.
    u32 GuiComponent::SetName(const char* lpacName, const char* lpacParentName)
    {
        CGS_ASSERT(lpacName != 0, "Invalid name sent to GuiComponent::SetName");

        if (lpacParentName != 0)
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


    // GuiComponent::Construct (DWARF cpp:117): SetName, then SetStateInterface.
    void GuiComponent::Construct(const char* lpacName, StateInterface* lpStateInterface,
                                 const char* lpacParentName)
    {
        SetName(lpacName, lpacParentName);
        SetStateInterface(lpStateInterface);
    }

    // @ 0x82847030 - header-inline in the original (CgsGuiComponent.h:169 assert); ARTIST keeps
    // an out-of-line copy. Assert the incoming state interface, store it at +0x88.
    void GuiComponent::SetStateInterface(StateInterface* lpStateInterface)
    {
        CGS_ASSERT(lpStateInterface != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid state interface sent to GuiComponent::SetStateInterface", "..\\..\\..\\GameShared\\GameClasses\\Gui/Model/State/CgsGuiComponent.h", 169));

        mpStateInterface = lpStateInterface;
    }
}
