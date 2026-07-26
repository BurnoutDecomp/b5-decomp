// CgsGui::SaveLoadSystem -- wave-B load lane, reconstructed from BURNOUT_X360_ARTIST.XEX.
// Bodies for two of the X360 save/load front-end's load-path functions:
//
//   LoadDone              @ 0x82855FE0  -- Realmc load result callback: success re-arms the
//                                          autosave pre-flight, then reports 0/1 to the
//                                          result handler.
//   LoadReady            @ 0x828521A0  -- Realmc load-ready callback: hand the title-save
//                                          data pair { muSaveDataSizeKb, muGameDataSizeKb }
//                                          to the SDK's DataBuffer and return 0.
//
// (LoadHandleConfirmLoad @0x82855EC0 was also reconstructed here by wave B -- the faithful
// console confirm arm: EntryContentName + the two LoadEntryInfo records handed to the
// memory-card ReadSave slot +0x38, then Update() -- but the later PC-storage wave landed its
// own body in CgsSaveLoadPS3.cpp (the confirm arm reads the CgsSaveLoadPC container into the
// captured stored-data view, with the console shape documented in its FLAG comment); that TU
// owns the function now, and the duplicate was removed here in the wave-C reconcile. The
// wave-B console-faithful body survives in git history @34234be4.)
//
// The class layout + member names are frozen in CgsSaveLoadX360.h (X360-authoritative offset
// map). LoadDone/LoadReady are Realmc RESULT callbacks whose Hex-Rays displacements
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
