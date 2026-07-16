// CgsGui::SaveLoadSystem -- wave-B load lane, reconstructed from BURNOUT_X360_ARTIST.XEX.
// Bodies for three of the X360 save/load front-end's load-path functions:
//
//   LoadHandleConfirmLoad @ 0x82855EC0  -- confirm-load prompt choice: build the entry
//                                          content name + the two load-entry records and
//                                          hand them to the memory-card interface (ReadSave
//                                          slot +0x38), then pump Update.
//   LoadDone              @ 0x82855FE0  -- Realmc load result callback: success re-arms the
//                                          autosave pre-flight, then reports 0/1 to the
//                                          result handler.
//   LoadReady            @ 0x828521A0  -- Realmc load-ready callback: hand the title-save
//                                          data pair { muSaveDataSizeKb, muGameDataSizeKb }
//                                          to the SDK's DataBuffer and return 0.
//
// The class layout + member names are frozen in CgsSaveLoadX360.h (X360-authoritative offset
// map). Two of these three (LoadDone) are Realmc RESULT callbacks whose Hex-Rays displacements
// are (real member offset - 4) -- the X360 multiple-inheritance +4 sub-object mechanics; they
// are written here as plain member functions using the NAMED members (see the header's `this`
// convention note). Cross-TU SDK callees resolve to their own RealmcIface TUs at link time.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"   // CgsGui::SaveLoadSystem (FIRST)

#include "SDKs/Realmc/RealmcMemcardInterface.h"      // RealmcIface::MemcardInterface (ReadSave slot)
#include "SDKs/Realmc/RealmcMemcardInterfaceImpl.h"  // concrete interface impl (TU completeness)
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"         // RealmcIface::LoadEntryInfo
#include "SDKs/Realmc/RealmcDataBuffer.h"            // RealmcIface::DataBuffer
#include "SDKs/Realmc/RealmcEntryContentName.h"      // RealmcIface::EntryContentName
#include "SDKs/Realmc/RealmcTitleInfo.h"             // RealmcIface::TitleInfo::Empty()

#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"  // CgsGui::SystemUserProfile (after X360.h)

namespace CgsGui
{
    // X360 0x82855EC0. Confirm-load prompt choice. Option 1 rebuilds the two load-entry
    // records (the title save + the "Mugshots" blob) exactly like the bootup reload lane,
    // wraps the entry-content name and hands them to the memory-card interface via the
    // ReadSave slot (+0x38), then pumps Update. Any other choice reports failure.
    void SaveLoadSystem::LoadHandleConfirmLoad(u32 luOption)
    {
        if (luOption != 1)
        {
            // (***(this+0x130))(this+0x130, 1) -- the active display IS the result handler.
            reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
                ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(1));
            return;
        }

        // Two default-constructed load-entry records (the X360 loop constructs [0] then [1]).
        RealmcIface::LoadEntryInfo laEntries[2];

        // Entry 0: the title save. The { muSaveDataSizeKb, muGameDataSizeKb } pair is
        // historically misnamed -- the X360 moves it as one 8-byte { data, size } DataBuffer:
        // muSaveDataSizeKb becomes the data word, muGameDataSizeKb the size word.
        RealmcIface::DataBuffer lZeroPair;   // {0,0}
        RealmcIface::DataBuffer lSavePair;
        lSavePair.mpData = reinterpret_cast<void*>(static_cast<uintptr_t>(muSaveDataSizeKb));
        lSavePair.muSize = muGameDataSizeKb;
        RealmcIface::LoadEntryInfo lTitleRecord(macTitle, &lZeroPair, &lSavePair);
        laEntries[0] = lTitleRecord;

        // Entry 1: the "Mugshots" blob record.
        laEntries[1] = CreateRealmcMugshotLoadEntryInfo();

        // The entry-content name: wide metadata title + ascii device title + the derived size
        // (total mugshot bytes + muGameDataSizeKb).
        RealmcIface::EntryContentName lName(
            macwMetadataTitle, macTitle,
            static_cast<s32>(miExtraFilesSizeBytes * miImageHeight * miImageWidth)
                + static_cast<s32>(muGameDataSizeKb));

        // ReadSave(&entryContentName, count == 2, entries, flags == 0, TitleInfo::Empty()).
        mpMemcardInterface->ReadSave(&lName, 2, laEntries, 0, &RealmcIface::TitleInfo::Empty());

        Update();
    }

    // X360 0x82855FE0. Realmc load result callback (+4 sub-object -- written as a plain member).
    // A non-zero result reports failure (1); success re-arms the autosave pre-flight
    // (EnableAutosave(-1)) then reports success (0). Both go through mpActiveMessageDisplay,
    // which acts as the result handler.
    void SaveLoadSystem::LoadDone(u32 luResult)
    {
        if (luResult)
        {
            reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
                ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(1));
            return;
        }

        EnableAutosave(-1);
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(0));
    }

    // X360 0x828521A0. Realmc load-ready callback. Only the last parameter (the caller's
    // DataBuffer) is read: fill it with the title-save data pair
    // { muSaveDataSizeKb, muGameDataSizeKb } (misnamed -- moved as one { data, size } pair,
    // see the header's pitfall note) and return 0. The four leading parameters are unrecovered.
    s32 SaveLoadSystem::LoadReady(s32 /*liArg1*/, s32 /*liArg2*/, s32 /*liArg3*/, s32 /*liArg4*/,
                                  RealmcIface::DataBuffer* lpDataBuffer)
    {
        RealmcIface::DataBuffer lPair;
        lPair.mpData = reinterpret_cast<void*>(static_cast<uintptr_t>(muSaveDataSizeKb));
        lPair.muSize = muGameDataSizeKb;
        *lpDataBuffer = lPair;
        return 0;
    }
}
