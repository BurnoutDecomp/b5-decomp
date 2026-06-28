#include "GameShared/GameClasses/Development/MessageSystem/DebugComponent/CgsDebugComponentMessageFilter.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the inlined assert sequence)

// CgsDev::DebugComponentMessageFilter method bodies. Reconstructed from the X360 ARTIST build and
// the DecFIGS DWARF. The component edits CgsDev::Message::gxMessageFilterFlags (the 64-bit
// log-category mask) through the debug menu; OnActivate wires the menu page, the two static change
// callbacks keep the "Enabled" toggle and the mask in sync, and Construct/SetFilterName build the
// per-category name table the "Filter" combo box draws from.

namespace CgsDev
{
    // X360 gaMessageFilterNames (CgsDebugComponentMessageFilter.cpp:48): the option table the
    // "Filter" combo box selects from. One StringList entry per category bit; SetFilterName fills
    // the named ones, the rest stay "(unused)". Sized to KI_MAX_FILTER (the X360 array runs from
    // dword_83018CF0 up to mpInstance, i.e. 64 * sizeof(StringList)).
    DebugUI::StringList gaMessageFilterNames[Message::KI_MAX_FILTER];

    // X360 0x82817218 / 0x82817228.
    const char* DebugComponentMessageFilter::GetName() const
    {
        return "Message Filter";
    }

    const char* DebugComponentMessageFilter::GetPath() const
    {
        return "Core/Debug";
    }

    // X360 0x8282C8A8. Reset the selection state, then (re)initialise every option-table slot to its
    // own index + "(unused)" before naming the known categories.
    void DebugComponentMessageFilter::Construct()
    {
        miFilterIndex  = 0;
        mbFilterEnable = false;

        for (s32 liIndex = 0; liIndex < Message::KI_MAX_FILTER; ++liIndex)
        {
            gaMessageFilterNames[liIndex].miValue = liIndex;
            gaMessageFilterNames[liIndex].mpcName = "(unused)";
        }

        SetFilterName("Global",          Message::KX_FILTER_GLOBAL);
        SetFilterName("GS Containers",   Message::KX_FILTER_GSCONTAINERS);
        SetFilterName("GS Development",  Message::KX_FILTER_GSDEVELOPMENT);
        SetFilterName("GS Geometric",    Message::KX_FILTER_GSGEOMETRIC);
        SetFilterName("GS Graphics",     Message::KX_FILTER_GSGRAPHICS);
        SetFilterName("GS Language",     Message::KX_FILTER_GSLANGUAGE);
        SetFilterName("GS Memory",       Message::KX_FILTER_GSMEMORY);
        SetFilterName("GS Module",       Message::KX_FILTER_GSMODULE);
        SetFilterName("GS Network",      Message::KX_FILTER_GSNETWORK);
        SetFilterName("GS Physics",      Message::KX_FILTER_GSPHYSICS);
        SetFilterName("GS SceneManager", Message::KX_FILTER_GSSCENEMANAGER);
        SetFilterName("GS Sound",        Message::KX_FILTER_GSSOUND);
        SetFilterName("GS System",       Message::KX_FILTER_GSSYSTEM);
        SetFilterName("GS FileSystem",   Message::KX_FILTER_GSFILESYSTEM);
        SetFilterName("GS Resource",     Message::KX_FILTER_GSRESOURCE);
    }

    // X360 0x82824A20. Name the category identified by the single set bit in lxFlag. The X360 first
    // strips the GSCONTAINERS (bit 1) and GAME (bit 15) bits when they appear alongside others (the
    // composite GAMESHARED/GAME masks share those bits), then walks the mask to find the single set
    // bit's index; that index is the option-table slot. Exactly one bit must remain set (asserted).
    void DebugComponentMessageFilter::SetFilterName(const char* lpcName, Message::FilterFlag lxFlag)
    {
        if ((lxFlag & Message::KX_FILTER_GSCONTAINERS) != 0 && lxFlag != Message::KX_FILTER_GSCONTAINERS)
            lxFlag &= ~Message::KX_FILTER_GSCONTAINERS;

        if ((lxFlag & Message::KX_FILTER_GAME) != 0 && lxFlag != Message::KX_FILTER_GAME)
            lxFlag &= ~Message::KX_FILTER_GAME;

        s32 liBitIndex = -1;
        Message::FilterFlag lxProbe = 1;
        for (s32 liBit = 0; liBit < Message::KI_MAX_FILTER; ++liBit)
        {
            if (lxFlag == lxProbe)
            {
                liBitIndex = liBit;
                break;
            }
            lxProbe <<= 1;
        }

        CGS_ASSERT(liBitIndex != -1, "only one bit must be set");

        gaMessageFilterNames[liBitIndex].miValue = liBitIndex;
        gaMessageFilterNames[liBitIndex].mpcName = lpcName;
    }

    // X360 0x82831B90. Wire the filter page into the debug menu: the "Filter" combo box (the
    // category selector, options from gaMessageFilterNames), the "Enabled" toggle for the selected
    // category, and the two hidden u32 sliders exposing the raw 64-bit mask halves. Neither the
    // selection nor the toggle is saved across runs. Finally sync mbFilterEnable to the live mask.
    void DebugComponentMessageFilter::OnActivate()
    {
        // The "Filter" category selector.
        RegisterVariable(&miFilterIndex, "Filter");
        SetRange(&miFilterIndex, 0, Message::KI_MAX_FILTER - 1);
        SetOptions(&miFilterIndex, gaMessageFilterNames);
        SetChangeCallback(&miFilterIndex, &FilterEnableChangeCallback, this);
        SetSaveEnabled(&miFilterIndex, false);

        // The "Enabled" toggle for the selected category.
        RegisterVariable(&mbFilterEnable, "Enabled");
        SetChangeCallback(&mbFilterEnable, &FilterTypeChangeCallback, this);
        SetSaveEnabled(&mbFilterEnable, false);

        // The two hidden raw-mask halves (the X360 registers &gxMessageFilterFlags and that address
        // + 4 as two u32 variables, then hides both).
        u32* lpuFilterLow  = reinterpret_cast<u32*>(&Message::gxMessageFilterFlags);
        u32* lpuFilterHigh = lpuFilterLow + 1;

        RegisterVariable(lpuFilterLow,  "(filter value 1)");
        RegisterVariable(lpuFilterHigh, "(filter value 2)");
        SetVisible(lpuFilterLow,  false);
        SetVisible(lpuFilterHigh, false);

        // Sync the toggle to the selected category's current state in the mask.
        const Message::FilterFlag lxBit = static_cast<Message::FilterFlag>(1) << miFilterIndex;
        mbFilterEnable = (lxBit & Message::gxMessageFilterFlags) != 0;
    }

    // X360 0x828171A0. "Filter" combo change: refresh mbFilterEnable to mirror whether the
    // newly-selected category's bit is set in the live mask. lpUserData is the component (the X360
    // reads the members off r4 == the user-data pointer; lpValue is unused).
    void DebugComponentMessageFilter::FilterEnableChangeCallback(void* /*lpValue*/, void* lpUserData)
    {
        DebugComponentMessageFilter* lpComponent = static_cast<DebugComponentMessageFilter*>(lpUserData);

        const Message::FilterFlag lxBit = static_cast<Message::FilterFlag>(1) << lpComponent->miFilterIndex;
        lpComponent->mbFilterEnable = (lxBit & Message::gxMessageFilterFlags) != 0;
    }

    // X360 0x828171D8. "Enabled" toggle change: set or clear the selected category's bit in the live
    // mask according to mbFilterEnable. lpUserData is the component; lpValue is unused.
    void DebugComponentMessageFilter::FilterTypeChangeCallback(void* /*lpValue*/, void* lpUserData)
    {
        DebugComponentMessageFilter* lpComponent = static_cast<DebugComponentMessageFilter*>(lpUserData);

        const Message::FilterFlag lxBit = static_cast<Message::FilterFlag>(1) << lpComponent->miFilterIndex;
        if (lpComponent->mbFilterEnable)
            Message::gxMessageFilterFlags |= lxBit;
        else
            Message::gxMessageFilterFlags &= ~lxBit;
    }
}
