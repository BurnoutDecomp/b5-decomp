#include "GameSource/Gui/BrnGuiProfileHost.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnGui
{
    // ARTIST827E21E0; actual host vtable820D03DC slot0.
    void ProfileHost::HandleProfileTaskResult()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
            *CgsDev::Log::gpDebugPrint << "SaveLoad: Finished\n";
    }

    // ARTIST82517C10. The host itself is the real task-result handler.
    void ProfileHost::Bootup()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
            *CgsDev::Log::gpDebugPrint << "SaveLoad: Starting Bootup\n";
        mProfileManager.Bootup(*this, true);
    }

    // ARTIST82517C80, with original handler reference and no invented result.
    void ProfileHost::Save()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
            *CgsDev::Log::gpDebugPrint << "SaveLoad: Starting Save\n";
        mProfileManager.Save(*this);
    }

    // ARTIST82525F58.
    void ProfileHost::Load()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
            *CgsDev::Log::gpDebugPrint << "SaveLoad: Starting Load\n";
        mProfileManager.Load(*this);
    }

    // ARTIST824ED0D8. The canonical manager helper has these same detach/release/true effects.
    bool ProfileHost::Release()
    {
        return mProfileManager.Release();
    }
}
