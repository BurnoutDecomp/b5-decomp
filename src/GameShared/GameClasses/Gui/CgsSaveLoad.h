#pragma once

#include "types.hpp"

// CgsGui save/load shared declarations -- the wide-char-to-ASCII conversion helpers and
// the save/load vocabulary (task-result + content-image-file enums, the SaveInfo /
// SaveLoadMetadata records, and the MessageDisplay / ContentInformation interfaces).
// Declaration surface only; the out-of-line bodies live in their own TUs. Member set,
// enums and signatures from the DecFIGS DWARF (GameShared/GameClasses/Gui/CgsSaveLoad.h).
//
// This header is the declaration home for CgsGui::ConvertWideCharToAsciiSafe /
// WideCharIsAscii (CgsSaveLoad.h:272/:284/:305), which the CrashNav on-screen-keyboard
// listeners (BrnGui::CrashNav*KeyboardListener::KeyboardClosed) call to copy the keyboard
// result text into their fixed ASCII string buffers.

namespace CgsGui
{
    // CgsSaveLoad.h:44/:45/:50 -- save/load sizing constants.
    const u32 KI_KB_SIZE_BYTES                  = 1024;
    const u32 KI_FILE_BLOCK_SIZE_BYTES          = 65536;
    const s32 KI_MAX_IMAGE_FILES_PER_OPERATION  = 7;

    // CgsSaveLoad.h:55 -- the result reported back to a SaveLoadTaskResultHandler.
    enum ESaveLoadTaskResult
    {
        E_SAVELOADTASKRESULT_SUCCESS   = 0,
        E_SAVELOADTASKRESULT_FAILURE   = 1,
        E_SAVELOADTASKRESULT_CANCELLED = 2,
        E_SAVELOADTASKRESULT_MAX       = 3,
    };

    // CgsSaveLoad.h:63 -- the content-image-file slots (icon/picture/sound) of a
    // save-data or game-data container.
    enum ESaveLoadCif
    {
        E_SAVELOADCIF_SAVEDATA_START = 0,
        E_SAVELOADCIF_SAVEDATA_ICON0 = 0,
        E_SAVELOADCIF_SAVEDATA_ICON1 = 1,
        E_SAVELOADCIF_SAVEDATA_PIC1  = 2,
        E_SAVELOADCIF_SAVEDATA_SND0  = 3,
        E_SAVELOADCIF_SAVEDATA_COUNT = 4,
        E_SAVELOADCIF_GAMEDATA_START = 4,
        E_SAVELOADCIF_GAMEDATA_ICON0 = 4,
        E_SAVELOADCIF_GAMEDATA_ICON1 = 5,
        E_SAVELOADCIF_GAMEDATA_PIC1  = 6,
        E_SAVELOADCIF_GAMEDATA_SND0  = 7,
        E_SAVELOADCIF_GAMEDATA_COUNT = 4,
        E_SAVELOADCIF_COUNT          = 8,
    };

    // CgsSaveLoad.h:284/:272 -- true when the wide character(s) are representable in 7-bit
    // ASCII. Defined out-of-line in this TU's .cpp companion family (not in scope here).
    extern bool WideCharIsAscii(wchar_t lwChar);
    extern bool WideCharIsAscii(const wchar_t* lpwString);

    // CgsSaveLoad.h:305 -- copy up to luMaxLen wide characters from lpwSource into the
    // ASCII buffer lpacDest (the on-screen-keyboard listeners pass their fixed string
    // buffer). Defined out-of-line (external to this TU); declared here so the CrashNav
    // keyboard listeners can call it.
    extern void ConvertWideCharToAsciiSafe(char* lpacDest, const wchar_t* lpwSource, size_t luMaxLen);
}
