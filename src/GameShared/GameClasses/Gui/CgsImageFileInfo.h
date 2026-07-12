#pragma once

#include "types.hpp"

// CgsGui::ImageFileInfo -- one content-image (mugshot) transfer record handed between
// BrnGui::ProfileManager and the save/load system. Shape + names from the DecFIGS DWARF
// (GameShared/GameClasses/Gui/CgsImageFileInfo.h:39). X360 record: 16 bytes
// { miId @+0, miSize @+4, mpBuffer @+8, mbIsCorrupt @+12 }; the buffer pointer widens on
// the x64 host and all access is by name.
//
// X360 behaviour attestations:
//   * miId > -1 is the "slot in use" test (ProfileManager::LoadImageFiles @0x82513C00
//     scans `miId > -1`; Autosave @0x82513958 gates its buffered image on it).
//   * the invalidation idiom writes exactly { miId = -1, mbIsCorrupt = false }
//     (BufferProfileOperation @0x824F8EF0 resets its three buffered records that way) --
//     Construct() reproduces those two stores.

namespace CgsGui
{
    struct ImageFileInfo
    {
        s32   miId;             // CgsImageFileInfo.h:48 (X360 +0x00; -1 == unused slot)
        s32   miSize;           // CgsImageFileInfo.h:49 (X360 +0x04; bytes at mpBuffer)
        char* mpBuffer;         // CgsImageFileInfo.h:50 (X360 +0x08)
        bool  mbIsCorrupt;      // CgsImageFileInfo.h:51 (X360 +0x0C)

        // CgsImageFileInfo.h:43 -- reset to the unused state (the two X360-attested stores).
        void Construct()
        {
            miId        = -1;
            mbIsCorrupt = false;
        }

        // CgsImageFileInfo.h:46 -- a record is valid once it names a real image slot.
        bool IsValid() const
        {
            return miId > -1;
        }
    };
}
