#pragma once

#include "types.hpp"

// CgsGui::SaveLoadPC -- the PC-platform profile storage backend.
//
// FLAG PC-platform leaf (whole TU): the storage behind CgsGui::SaveLoadSystem on the
// console is the RealmcIface memory-card SDK (XContent containers written/read through
// asynchronous MemcardInterface operations). That SDK is console-platform code with no
// PC counterpart, so -- exactly like the D3D/input/sound PC leaves -- the platform edge
// is realised natively: one versioned container file on disk holding the serialised
// 256KB profile image (BrnGui::ProfileManager::ProfileStoredData) plus the mugshot
// buffer blob the console stored as its second save entry. Only the storage edge lives
// here; the save/load task machine, the prompts, and the ProfileManager control flow
// stay the X360-faithful reconstructions in CgsSaveLoadPS3.cpp / BrnGuiProfile.cpp.
//
// The container lands in the game working directory as "Memcard\<name>.sav", next to
// the "Memcard\SaveImage.png" content image the save/load system already reads. The
// payload is guarded by a checksum so a torn/edited file reads back as CORRUPT (the
// console's equivalent signal is the XContent signature check).

namespace CgsGui
{
namespace SaveLoadPC
{
    // The outcome of a container read, so callers can distinguish the console outcomes:
    // no save present (MISSING), save damaged (CORRUPT), or save from a different
    // layout/build (MISMATCH -- payload sizes disagree with the running build's).
    enum EContainerReadResult
    {
        E_CONTAINERREAD_OK       = 0,
        E_CONTAINERREAD_MISSING  = 1,
        E_CONTAINERREAD_CORRUPT  = 2,
        E_CONTAINERREAD_MISMATCH = 3,
    };

    // True when the named container exists on disk ("Memcard\<name>.sav").
    bool ContainerExists(const char* lpacName);

    // Write the profile container atomically (temp file + rename): the profile image,
    // the mugshot blob (optional -- pass null/0 to omit), and the user-facing SaveInfo
    // strings for a future save-listing UI. Returns false on any I/O failure (the
    // caller reports E_SAVELOADTASKRESULT_FAILURE, the console's storage-down signal).
    bool WriteContainer(const char* lpacName,
                        const void* lpImage, u32 luImageSize,
                        const void* lpMugshots, u32 luMugshotsSize,
                        const char* lpacTitle, const char* lpacDescription);

    // Read the container back: validates the header (magic/version), requires the
    // stored payload sizes to match the destination sizes exactly, verifies the
    // checksum, then fills both destinations. The mugshot destination is optional
    // (null/0 skips that payload). Nothing is written to the destinations unless the
    // whole read validates.
    EContainerReadResult ReadContainer(const char* lpacName,
                                       void* lpImage, u32 luImageSize,
                                       void* lpMugshots, u32 luMugshotsSize);
}
}
