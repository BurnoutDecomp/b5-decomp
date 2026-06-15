#pragma once

#include "types.hpp"

// CgsGui::GuiComponent - base of every GUI screen component. Carries its name (and
// hashed name) plus a back-pointer to the StateInterface it outputs through, and the
// apt-view-state helpers components use to drive Flash/apt transitions. Layout/method
// set from the DecFIGS DWARF (CgsGuiComponent.h).
namespace CgsLanguage { class LanguageManager; }

namespace CgsGui
{
    class StateInterface;

    struct GuiComponent
    {
        static const u8 KU_MAX_COMPONENT_NAME_LEN = 128;

        virtual void Construct(const char* lpacName, StateInterface* lpStateInterface,
                               const char* lpacParentName);

        void AddOutputAptViewState(const char* lpacAptName, const char* lpacViewState, bool lbImmediate);
        const char* GetName() const { return macName; }
        u32 GetNameHash() const { return muHashedName; }
        void FillAptViewMessage(const char* lpacAptName, const char* lpacViewState,
                                const char* lpacParam, bool lbImmediate);
        CgsLanguage::LanguageManager* GetLanguageManager() const;

    protected:
        void SetName(const char* lpacName, const char* lpacParentName);
        void SetStateInterface(StateInterface* lpStateInterface) { mpStateInterface = lpStateInterface; }

        char            macName[KU_MAX_COMPONENT_NAME_LEN];
        u32             muHashedName;
        StateInterface* mpStateInterface;
    };
}
