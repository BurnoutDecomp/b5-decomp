#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"   // Message::FilterFlag

// CgsDev::DebugComponentMessageFilter - the in-game "Message Filter" debug component (menu path
// "Core/Debug"). It exposes the log-category filter mask (CgsDev::Message::gxMessageFilterFlags)
// as an editable debug-menu page: a "Filter" combo box picks one of the named categories
// (gaMessageFilterNames, built by Construct/SetFilterName), an "Enabled" checkbox toggles that
// category's bit, and two hidden u32 sliders expose the raw 64-bit mask halves. Recovered from the
// DecFIGS DWARF (Development/MessageSystem/DebugComponent/CgsDebugComponentMessageFilter.h) and the
// X360 ARTIST build (Construct 0x8282C8A8, OnActivate 0x82831B90, SetFilterName 0x82824A20, the two
// change callbacks 0x828171A0 / 0x828171D8, GetName 0x82817218, GetPath 0x82817228).
//
// The two change callbacks are registered with the VariableManager as VariableCallbackFunction
// (void(*)(void*,void*)) with the component as the user-data argument, so they are static and
// recover the component from lpUserData (the X360 reads the members off r4 == the user-data).

namespace CgsDev
{
    struct DebugComponentMessageFilter : public DebugComponent
    {
    public:
        // X360 0x8282C8A8. Reset the filter state and (re)build the category name table: every
        // entry starts as "(unused)", then SetFilterName names the 15 known categories.
        void Construct();

        // X360 0x82824A20. Record a display name for the single category bit set in lxFlag. The flag
        // must have exactly one bit set (asserted); the bit index is the slot in gaMessageFilterNames.
        void SetFilterName(const char* lpcName, Message::FilterFlag lxFlag);

    protected:
        // X360 0x82817218 / 0x82817228. The component's menu identity.
        virtual const char* GetName() const;
        virtual const char* GetPath() const;

        // X360 0x82831B90. Register the filter page's variables with the debug menu (the "Filter"
        // combo, the "Enabled" toggle, and the two hidden raw-mask sliders) and sync the toggle.
        virtual void OnActivate();

    private:
        // X360 0x828171A0. Change callback for the "Filter" combo: refresh mbFilterEnable to reflect
        // whether the currently-selected category bit is set in the mask.
        static void FilterEnableChangeCallback(void* lpValue, void* lpUserData);

        // X360 0x828171D8. Change callback for the "Enabled" toggle: set or clear the selected
        // category's bit in gxMessageFilterFlags according to mbFilterEnable.
        static void FilterTypeChangeCallback(void* lpValue, void* lpUserData);

        // X360 +0xC / +0x10. The selected category index (0..KI_MAX_FILTER-1) and whether it is
        // currently enabled in the mask.
        s32  miFilterIndex;
        bool mbFilterEnable;
    };
}
