#pragma once

#include "GameSource/Gui/BrnGuiProfile.h"

namespace BrnGui
{
    class GuiModule;

    // DecFIGS BrnGuiModule.h:133/187; ARTIST host vtable820D03DC and manager+10.
    // ARTIST constructs the manager at +0x10 within its host (827E5CE8..827E5D08).
    struct ProfileHost : public ProfileTaskResultHandler
    {
    public:
        // Original inline accessor (DecFIGS h:861), no offset arithmetic or shadow pointer.
        ProfileManager& GetProfileManager() { return mProfileManager; }

        // Original byte-result Release and void task commands.
        bool Release();
        void Bootup();
        void Save();
        void Load();

    private:
        // Preserve the module's existing const manager getter without a new host API.
        friend class GuiModule;
        void HandleProfileTaskResult() ;
        CGS_ALIGN(16) ProfileManager mProfileManager;
    };
}
