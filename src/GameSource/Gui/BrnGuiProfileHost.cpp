#include "types.hpp"

// ===========================================================================
// BrnGui::ProfileHost  (GameSource/Gui/BrnGuiProfileHost.{cpp})
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. ProfileHost is the thin host that
// BrnGui::GuiModule / GuiDebugComponent drive; it embeds a BrnGui::ProfileManager
// at byte +16 and forwards the boot / load / save / release lifecycle to it. It
// also owns a CgsGui::SystemUserProfile* (kept inside the embedded manager region,
// at ProfileHost +72) whose listener it detaches on Release.
//
// This is an ISOLATED translation unit. The full BrnGuiProfile.h class header is
// not homed in the tree (it pulls in ~10 not-yet-reconstructed GUI/resource types),
// so — following the precedent already set in this file and in BrnGuiProfile.cpp —
// this TU keeps its own minimal declarations for the callees it touches rather than
// including the real headers, so those shims do not clash with the real headers in
// sibling TUs. Every member offset used below is pinned to the X360 store offsets.
//
// NOTE: this isolated decomp keeps its own minimal CgsDev::Log shim (vtable-slot call
// form) rather than the full CgsLog.h; it lives in its own TU so that shim does not
// clash with the real CgsLog.h pulled into BrnGuiModule.cpp. gxMessageFilterFlags is
// tested `& 1` here (the X360 clrldi keeps bit 0); typed u32 to match this TU's shim.

namespace CgsDev
{
    namespace Message
    {
        extern u32 gxMessageFilterFlags;
    }

    namespace Log
    {
        struct DebugPrint;
        // vtable slot signature: printf-style line writer (object, format string).
        typedef int (*DebugPrintFn)(DebugPrint*, const char*);
        struct DebugPrint
        {
            DebugPrintFn* mpVTable;
        };
        extern DebugPrint* gpDebugPrint;
    }
}

// ---------------------------------------------------------------------------
// Minimal callee declarations (isolated-TU shims; bodies live in their own TUs).
// ---------------------------------------------------------------------------
namespace CgsGui
{
    // CgsGui::SystemUserProfile::DetachListener(this, Listener&). The Listener is the
    // ProfileManager's SystemUserProfile::Listener sub-object (its 4th vtable ptr at
    // manager +12). Held/reached opaquely here.
    struct SystemUserProfileListener;

    struct SystemUserProfile
    {
        // CgsGui::SystemUserProfile::DetachListener @ 0x824EAC10.
        void DetachListener(SystemUserProfileListener& lrListener);
    };

    // CgsGui::SaveLoadSystem::Release @ (CgsSaveLoadPS3.cpp / CgsSaveLoadX360.h).
    // Embedded in ProfileManager at manager +4200.
    struct SaveLoadSystem
    {
        bool Release();
    };
}

namespace BrnGui
{
    // ProfileManager, embedded in ProfileHost at +16. Only the members / entry points
    // ProfileHost touches are modelled; every offset is X360-confirmed.
    //   +0x0C  SystemUserProfile::Listener sub-object (4th vtable ptr; a1+28)
    //   +0x38  mpSystemUserProfile   CgsGui::SystemUserProfile*   (a1+72)
    //   +4200  mSaveLoadSystem       CgsGui::SaveLoadSystem       (v1+4200)
    struct ProfileManager
    {
        // BrnGui::ProfileManager::Bootup @ (BrnGuiProfile.cpp). (this, host, bStartup)
        int Bootup(ProfileManager* lpManager, char lbStartup);
        // BrnGui::ProfileManager::Load @ 0x82525F58 callee. (this, host)
        void Load(ProfileManager* lpManager);
        // BrnGui::ProfileManager::Save @ callee. (this, host)
        int Save(ProfileManager* lpManager);

        // +0x38: the owning host's SystemUserProfile pointer.
        CgsGui::SystemUserProfile* GetSystemUserProfile()
        {
            return *reinterpret_cast<CgsGui::SystemUserProfile**>(
                reinterpret_cast<u8*>(this) + 0x38);
        }

        // +0x0C: the SystemUserProfile::Listener sub-object interior pointer. Guarded
        // against a null manager base (matches the a1 == -16 ? 0 : a1+28 branch).
        CgsGui::SystemUserProfileListener* GetListener()
        {
            if (this == nullptr)
            {
                return nullptr;
            }
            return reinterpret_cast<CgsGui::SystemUserProfileListener*>(
                reinterpret_cast<u8*>(this) + 0x0C);
        }

        // +4200 (0x1068): the embedded save/load system.
        CgsGui::SaveLoadSystem* GetSaveLoadSystem()
        {
            return reinterpret_cast<CgsGui::SaveLoadSystem*>(
                reinterpret_cast<u8*>(this) + 4200);
        }
    };

    struct ProfileHost
    {
        // +0x10 (16): the embedded profile manager.
        ProfileManager* GetProfileManager()
        {
            return reinterpret_cast<ProfileManager*>(
                reinterpret_cast<u8*>(this) + 16);
        }

        int  HandleProfileTaskResult();
        int  Bootup();
        void Load();
        int  Save();
        int  Release();
    };

    // -----------------------------------------------------------------------
    // ProfileHost::HandleProfileTaskResult  @ 0x827E21E0
    //   Behaviour-faithful: when the message filter has bit 0 set, log
    //   "SaveLoad: Finished" through vtable slot 1 and forward its result;
    //   otherwise the original returns an uninitialised register (effectively
    //   void) — reconstructed as `return 0`.
    // -----------------------------------------------------------------------
    int ProfileHost::HandleProfileTaskResult()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            return CgsDev::Log::gpDebugPrint->mpVTable[1](
                CgsDev::Log::gpDebugPrint, "SaveLoad: Finished\n");
        return 0;
    }

    // -----------------------------------------------------------------------
    // ProfileHost::Bootup  @ 0x82517C10
    //   Optionally logs "SaveLoad: Starting Bootup", then boots the embedded
    //   ProfileManager (this+16), passing the host (this) and startup flag 1.
    // -----------------------------------------------------------------------
    int ProfileHost::Bootup()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            CgsDev::Log::gpDebugPrint->mpVTable[1](
                CgsDev::Log::gpDebugPrint, "SaveLoad: Starting Bootup\n");

        return GetProfileManager()->Bootup(
            reinterpret_cast<ProfileManager*>(this), 1);
    }

    // -----------------------------------------------------------------------
    // ProfileHost::Load  @ 0x82525F58
    //   Optionally logs "SaveLoad: Starting Load", then drives the embedded
    //   ProfileManager's Load, passing the host (this).
    // -----------------------------------------------------------------------
    void ProfileHost::Load()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            CgsDev::Log::gpDebugPrint->mpVTable[1](
                CgsDev::Log::gpDebugPrint, "SaveLoad: Starting Load\n");

        GetProfileManager()->Load(reinterpret_cast<ProfileManager*>(this));
    }

    // -----------------------------------------------------------------------
    // ProfileHost::Release  @ 0x824ED0D8
    //   Detaches the manager's SystemUserProfile::Listener from the owned
    //   SystemUserProfile (manager +0x38), releases the embedded SaveLoadSystem
    //   (manager +4200), and returns 1.
    // -----------------------------------------------------------------------
    int ProfileHost::Release()
    {
        ProfileManager* lpManager = GetProfileManager();          // this + 16

        CgsGui::SystemUserProfileListener* lpListener =
            lpManager->GetListener();                             // manager +12, null-guarded

        lpManager->GetSystemUserProfile()->DetachListener(*lpListener); // *(this+72)

        lpManager->GetSaveLoadSystem()->Release();               // manager +4200

        return 1;
    }

    // -----------------------------------------------------------------------
    // ProfileHost::Save  @ 0x82517C80
    //   Optionally logs "SaveLoad: Starting Save", then drives the embedded
    //   ProfileManager's Save, passing the host (this), and forwards its result.
    // -----------------------------------------------------------------------
    int ProfileHost::Save()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            CgsDev::Log::gpDebugPrint->mpVTable[1](
                CgsDev::Log::gpDebugPrint, "SaveLoad: Starting Save\n");

        return GetProfileManager()->Save(reinterpret_cast<ProfileManager*>(this));
    }
}
