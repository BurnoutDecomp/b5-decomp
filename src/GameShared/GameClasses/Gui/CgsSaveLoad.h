#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Gui/CgsOpaqueBuffer.h"   // CgsGui::OpaqueBuffer (SaveLoadMetadata::mStoredData)

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

    // CgsSaveLoad.h:139 -- the callback interface the save/load system reports a finished
    // task to (success/failure/cancelled). DWARF: single virtual HandleSaveLoadTaskResult.
    class SaveLoadTaskResultHandler
    {
    public:
        virtual void HandleSaveLoadTaskResult(ESaveLoadTaskResult leResult) = 0;
    };

    // CgsSaveLoad.h:107/:117/:119/:121-:127 -- the on-screen message/icon display interface
    // the save/load system drives (yes/no prompts, no-space prompt, autosave icon). DWARF
    // vtable order: ShowMessage(0), ShowNoSpaceMessage(1), HideMessage(2), ShowAutosaveIcon(3).
    class MessageDisplay
    {
    public:
        // CgsSaveLoad.h:117 -- the option-choice callback a shown message routes the user's
        // selection through. DWARF: single virtual HandleOption(uint32_t).
        class OptionHandler
        {
        public:
            virtual void HandleOption(u32 luOption) = 0;
        };

        virtual void ShowMessage(OptionHandler* lpHandler, const char* lpcMessage,
                                 const char** lpacOptions, u32 luNumberOfOptions) = 0;
        virtual void ShowNoSpaceMessage(OptionHandler* lpHandler, const char* lpcMessage,
                                        u32 luRequiredKb, u32 luFreeKb) = 0;
        virtual void HideMessage() = 0;
        virtual void ShowAutosaveIcon(bool lbVisible) = 0;
    };

    // ADDITIVE GROW (BrnGuiProfile wave): the two save-description records this header's
    // banner already names. Shape + names from the DecFIGS DWARF (CgsSaveLoad.h:153/:170).
    // BrnGui::ProfileManager embeds one of each (X360 mSaveInfo @+267000 / mMetadata @+266944);
    // ProfileManager::Construct @0x824FEED0 fills the three strings and the save/autosave
    // starters publish the stored-data image through mStoredData (MakeOpaqueBuffer).

    // CgsSaveLoad.h:153 -- the user-facing description of one save.
    struct SaveInfo
    {
        static const usize TITLELENGTH_MAX       = 32;    // CgsSaveLoad.h:154 [DWARF names kept]
        static const usize DESCRIPTIONLENGTH_MAX = 256;   // CgsSaveLoad.h:155

        char macTitle[TITLELENGTH_MAX];              // CgsSaveLoad.h:157 (X360 +0x00)
        char macDescription[DESCRIPTIONLENGTH_MAX];  // CgsSaveLoad.h:158 (X360 +0x20)
    };

    // CgsSaveLoad.h:170 -- the container/type description of one save plus the raw image view.
    struct SaveLoadMetadata
    {
        static const s32 TYPENAMELENGTH_MAX = 32;    // CgsSaveLoad.h:171 [DWARF names kept]
        static const s32 FILENAMELENGTH_MAX = 16;    // CgsSaveLoad.h:172

        char         macTypename[TYPENAMELENGTH_MAX];  // CgsSaveLoad.h:174 (X360 +0x00)
        char         macFilename[FILENAMELENGTH_MAX];  // CgsSaveLoad.h:175 (X360 +0x20)
        OpaqueBuffer mStoredData;                      // CgsSaveLoad.h:177 (X360 +0x30: {ptr,size})
    };

    // CgsSaveLoad.h:189/:195-:212 -- the content-information-file provider the save/load
    // system inherits from so the memory-card layer can query/load its CIF blobs.
    // DWARF vtable order: IsSavingCif(0), GetCifFileType(1), GetCifFileName(2), LoadCifFile(3).
    class ContentInformationFileInterface
    {
    public:
        virtual bool        IsSavingCif(ESaveLoadCif leCif) = 0;
        virtual int         GetCifFileType(ESaveLoadCif leCif) = 0;
        virtual const char* GetCifFileName(ESaveLoadCif leCif) = 0;
        virtual bool        LoadCifFile(ESaveLoadCif leCif, void** lppData, s32* lpiSize) = 0;
    };

    // CgsSaveLoad.h:284/:272 -- true when the wide character(s) are representable in 7-bit
    // ASCII. Defined out-of-line in this TU's .cpp companion family (not in scope here).
    extern bool WideCharIsAscii(wchar_t lwChar);
    extern bool WideCharIsAscii(const wchar_t* lpwString);

    // CgsSaveLoad.h:305 -- copy up to luMaxLen wide characters from lpwSource into the
    // ASCII buffer lpacDest (the on-screen-keyboard listeners pass their fixed string
    // buffer). The body lives in CgsSaveLoad.cpp; declared here so the CrashNav keyboard
    // listeners can call it.
    extern void ConvertWideCharToAsciiSafe(char* lpacDest, const wchar_t* lpwSource, size_t luMaxLen);

    // CgsSaveLoad.h:326 -- the reverse of ConvertWideCharToAsciiSafe: copy the
    // null-terminated ASCII string lpacSource into the wide-character buffer lpwDest,
    // sign-extending each byte to a wide character. luMaxLen is the destination capacity;
    // the source length (excluding the terminator) must be strictly less than it.
    // Body in CgsSaveLoad.cpp. Called by CgsGui::SaveLoadSystem::Construct / ::SetMetadata.
    extern void ConvertAsciiToWideCharSafe(wchar_t* lpwDest, const char* lpacSource, size_t luMaxLen);
}
