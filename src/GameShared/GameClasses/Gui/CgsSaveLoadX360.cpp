#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Gui/CgsSaveLoadX360.cpp"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // CGS_ASSERT

// includes folded in from the CgsSaveLoadX360_w*.cpp partfiles (2026-09-15)
#include "SDKs/Realmc/RealmcMemcardInterface.h"
#include "SDKs/Realmc/RealmcMemcardInterfaceImpl.h"
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"            // RealmcIface::LoadEntryInfo
#include "SDKs/Realmc/RealmcDataBuffer.h"               // RealmcIface::DataBuffer
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"
#include <cstring>   // std::memcpy



// CgsGui::XenonFileInputStream member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::XenonFileInputStream) bodies the two
// X360-emitted stream methods:
//
//   GetSize @ 0x8284BD88  -> assert open, return miSize (this[1] == +4)
//   Read    @ 0x8284BE28  -> assert open, ReadFile(mhFile, buf, n, &read, NULL), return read
//
// X360 store-for-store notes:
//   - Both load mhFile from `*this` (+0) and assert it != -1 (INVALID_HANDLE_VALUE),
//     firing "File open failed!" otherwise.
//   - GetSize returns `lwz r3, 4(this)` == miSize.
//   - Read sets the bytes-read out-word to luBytesToRead first (stw r22, var_90), then calls
//     ReadFile(hFile=*this, lpBuffer=arg, nNumberOfBytesToRead=luBytesToRead,
//     lpNumberOfBytesRead=&var, lpOverlapped=NULL). On a FALSE return it fires "File read
//     failed!". The return value is the bytes-read out-word.
//   - The X360-baked CgsSaveLoadX360.h file/line (596/611/614) are discarded per project
//     convention; the assert strings are X360 rodata, reproduced verbatim.

namespace CgsGui
{
    // X360 0x8284BD88.
    s32 XenonFileInputStream::GetSize() const
    {
        CGS_ASSERT(mhFile != INVALID_HANDLE_VALUE, CGS_ASSERT_STREAM_MESSAGE_AT("File open failed!", "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\gui\\CgsSaveLoadX360.h", 596));
        return miSize;
    }

    // X360 0x8284BE28.
    s32 XenonFileInputStream::Read(void* lpBuffer, u32 luBytesToRead)
    {
        CGS_ASSERT(mhFile != INVALID_HANDLE_VALUE, "File open failed!");

        DWORD luBytesRead = static_cast<DWORD>(luBytesToRead);
        if (!::ReadFile(mhFile, lpBuffer, static_cast<DWORD>(luBytesToRead), &luBytesRead, 0))
        {
            CGS_ASSERT(false, "File read failed!");
        }

        return static_cast<s32>(luBytesRead);
    }
}

// ============================================================================
// FOLDED FROM CgsSaveLoadX360_wB_03.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// CgsGui::SaveLoadSystem - wave-B partfile (group 4: the mugshot buffer family).
// Reconstructed from BURNOUT_X360_ARTIST.XEX per the keystone spec
// (scratchpad/waveB/class_CgsGui__SaveLoadSystem.spec.md, rows 14-16). The class
// layout is frozen in CgsSaveLoadX360.h; this file only bodies functions.
//
// Functions in scope for this partfile:
//   CopyImageToBuffer                @ 0x828522D0
//   CreateRealmcMugshotLoadEntryInfo @ 0x828523D0
//
// (GetMugshotBufferFromImageId @0x8284C910 was also reconstructed here by wave B, but the
// later profile link-closure wave landed its own body in CgsSaveLoadPS3.cpp -- that TU owns
// it now; the duplicate was removed here in the wave-C reconcile, which also renamed the
// mugshot-grid members to the header's X360-assert-attested names
// miNumberOfMugshotTypes/miNumberOfMugshotsPerType.)

// Layout home FIRST (fwd-declares ::SystemUserProfile / ::LanguageManager at global
// scope; any other CgsGui header must follow it).

namespace CgsGui
{
    // X360 0x828522D0. Copy one image record's bytes INTO the mugshot buffer slot the
    // record's imageId maps to. The record is the homed CgsGui::ImageFileInfo:
    // lwz r4,0(r31)=miId, lwz r11,4(r31)=miSize, lwz r29,8(r31)=mpBuffer. (The +8 word is
    // the source pointer on the 32-bit console; access it by name so the host's 8-byte
    // pointer is not truncated -- BrnGui::ProfileManager::CopyImageToBuffer hands us a live
    // heap pointer there.)
    void SaveLoadSystem::CopyImageToBuffer(const void* lpImageFile)
    {
        CGS_ASSERT_AT(lpImageFile != 0, "lpImageFile", CGS_ARTIST_FILE, 818);

        const ImageFileInfo* lpRecord = reinterpret_cast<const ImageFileInfo*>(lpImageFile);

        CGS_ASSERT_AT(lpRecord->miSize == miExtraFilesSizeBytes,
                   "lpImageFile->miSize == miExtraFilesSizeBytes", CGS_ARTIST_FILE, 819);

        void* lpDest = GetMugshotBufferFromImageId(lpRecord->miId);

        std::memcpy(lpDest, lpRecord->mpBuffer, static_cast<usize>(miExtraFilesSizeBytes));   // XMemCpy
    }

    // X360 0x828523D0. Build the "Mugshots" Realmc load-entry record: the data half
    // points at the whole mugshot buffer (miExtraFilesSizeBytes * miNumberOfMugshotsPerType *
    // miNumberOfMugshotTypes bytes); the first DataBuffer pair is a zeroed {0,0}. Returned by
    // value (X360 sret).
    RealmcIface::LoadEntryInfo SaveLoadSystem::CreateRealmcMugshotLoadEntryInfo()
    {
        RealmcIface::DataBuffer lZeroPair;   // {0, 0} (default ctor)
        RealmcIface::DataBuffer lDataPair;
        lDataPair.mpData = mpMugshotBufferData;
        lDataPair.muSize = static_cast<u32>(miExtraFilesSizeBytes * miNumberOfMugshotsPerType
                                            * miNumberOfMugshotTypes);

        return RealmcIface::LoadEntryInfo("Mugshots", &lZeroPair, &lDataPair);
    }

    // ---- folded from CgsSaveLoadX360_wB_02.cpp (wave B, spec group G3 -- the state setters) ----

    // X360 0x8284C210. Cache the signed-in user index. A valid index is passed
    // in r4 through vtable slot +0x10 to MemcardInterfaceImpl::SetSignedUserIndex;
    // a negative index clears the bootup-check flag.
    void SaveLoadSystem::SetSignedInUserIndex(s32 liUserIndex)
    {
        miField210 = liUserIndex;                            // stw r4,0x210
        if (liUserIndex >= 0)                                // bge -> tail call
            mpMemcardInterface->SetSignedUserIndex(liUserIndex); // slot 4 +0x10, r4 = index
        else
            mbField183 = false;                              // stb 0,0x183
    }

    // X360 0x82473110. Change the interface message-state mode only when the
    // requested silent state differs, then cache the new byte. The native calls
    // are slot +0x18 with (0|-1) or (1|-1).
    void SaveLoadSystem::SetSilentMode(bool bSilent)
    {
        if (mbSilentMode && !bSilent)
            mpMemcardInterface->SetMessageState(0, -1);

        if (!mbSilentMode && bSilent)
            mpMemcardInterface->SetMessageState(1, -1);

        mbSilentMode = bSilent;
    }

    // X360 0x8284CC00. Realmc set-autosave result callback (the +4 interface sub-object; raw
    // displacement 0x17C == real member +0x180). Autosave is considered armed only when the SDK
    // reported success (liResult == 0) AND the enable flag was set (liEnabled == 1). liArg2 (r5) is
    // never read.
    void SaveLoadSystem::SetAutosaveDone(s32 liResult, s32 liArg2, s32 liEnabled)
    {
        mbField180 = (liResult == 0 && liEnabled == 1);      // stb r11,0x17C (real +0x180)
    }

    // ---- folded from CgsSaveLoadX360_wB_06.cpp (wave B, the sign-in lane), plus the two
    //      UI-closed helpers that lane calls ----

    // X360 0x8284C570. Run lpfnUIClosedFunc at once when no system UI is up, else park it in the
    // +0x128 pair until ProfileManager::UIClosed. The flag is the SystemUserProfile's +0x1C byte
    // (`lwz r11,0x194; lbz r11,0x1C(r11)`), the same byte SignIn sets when XShowSigninUI opened.
    void SaveLoadSystem::WaitForUIClosed(void (SaveLoadSystem::*lpfnUIClosedFunc)())
    {
        CGS_ASSERT(mpSystemUserProfile != 0, "SaveLoadSystem: uninitialised");   // :382

        if (!mpSystemUserProfile->mbSignedIn)
        {
            (this->*lpfnUIClosedFunc)();
            return;
        }
        mpfnPendingUIClosed = lpfnUIClosedFunc;              // std r27,0x128
    }

    // X360 ProfileManager::UIClosed @0x827E0F70, inlined there over system+0x128: if a
    // continuation is parked, clear the pair first, then call it.
    void SaveLoadSystem::RunPendingUIClosed()
    {
        if (mpfnPendingUIClosed == 0)
            return;
        void (SaveLoadSystem::*lpfnUIClosedFunc)() = mpfnPendingUIClosed;
        mpfnPendingUIClosed = 0;
        (this->*lpfnUIClosedFunc)();
    }

    // X360 0x8285F200. Bootup "not signed in" prompt choice. Option 1 pops the sign-in UI
    // once the message box closes; any other option reports failure to the live handler.
    void SaveLoadSystem::BootupHandleNotSignedInOption(u32 luOption)
    {
        if (luOption == 1)
        {
            WaitForUIClosed(&SaveLoadSystem::SignIn);   // 0x194 pair -> &SignIn
            return;
        }

        // 0x130 is the RESULT HANDLER (committed idiom): report failure (1).
        mpResultHandler
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(1));
    }

    // X360 0x8285D9E8. Pop the Xbox sign-in UI (XShowSigninUI(1, 0)). When the UI really opened
    // (return 0) the profile's +0x1C "system UI up" byte is set directly through the wave-B
    // friendship, mirroring `stb r11,0x1C(profile)`; then wait for the UI to close.
    void SaveLoadSystem::SignIn()
    {
        SystemUserProfile* lpProfile = mpSystemUserProfile;          // r30 = lwz 0x194(this)
        if (XShowSigninUI(1, 0) == 0)
            lpProfile->mbSignedIn = true;
        WaitForUIClosed(&SaveLoadSystem::SignInUIClosed);
    }

    // X360 0x82859D48. Sign-in UI closed: a signed-in user (miField210 >= 0) continues to the
    // autosave warning; otherwise reports failure to the live handler.
    void SaveLoadSystem::SignInUIClosed()
    {
        if (miField210 >= 0)
        {
            BootupShowAutosaveWarning();
            return;
        }

        mpResultHandler
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(1));
    }
}

// FOLDED FROM CgsSaveLoadX360_wB_00.cpp
// CgsGui::SaveLoadSystem -- wave-B fan-out part 00 (group G1: assert-only stubs).
//
// Realmc result callbacks that are never expected to fire on X360. Each is a
// streamed-assert stub: the X360 builds an assert message through the
// gpcMessageBuffer / StrStream machinery (BeginAssert / BasePriorityQueue::Clear /
// FireAssert / EndAssert), which folds to a plain CGS_ASSERT(false, "literal")
// per the established project rule (see CgsScriptedFsm.cpp). Assert messages are
// verbatim from the X360 rodata; the file/line arg is dropped (committed style).
//
// Include order is load-bearing: CgsSaveLoadX360.h FIRST (it fwd-declares
// ::SystemUserProfile / ::LanguageManager at global scope).

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h" // CGS_ASSERT

static const char KAC_ARTIST_ASSERT_FILE[] = "..\\..\\..\\GameShared\\GameClasses\\Gui/CgsSaveLoadX360.cpp";

namespace CgsGui
{
    // X360 0x8284C7D8. Realmc save-ready callback -- never expected on X360.
    s32 SaveLoadSystem::SaveReady()
    {
        CGS_ASSERT(false,
            CGS_ASSERT_MESSAGE_AT("SaveReady called.\n", KAC_ARTIST_ASSERT_FILE, 917 /* ARTIST 0x8284C7EC */));
        return 0;
    }

    // X360 0x8284CB80. Realmc find-entry callback -- never expected on X360.
    void SaveLoadSystem::FoundEntry(const void*)
    {
        CGS_ASSERT(false,
            CGS_ASSERT_MESSAGE_AT("FoundEntry called", KAC_ARTIST_ASSERT_FILE, 1252 /* ARTIST 0x8284CB94 */));
    }

    // X360 0x8284CBC0. Realmc find-entries-done callback -- never expected on X360.
    void SaveLoadSystem::FindEntriesDone(u32, u32)
    {
        CGS_ASSERT(false,
            CGS_ASSERT_MESSAGE_AT(
                "FindEntriesDone called", KAC_ARTIST_ASSERT_FILE, 1274 /* ARTIST 0x8284CBD4 */));
    }

    // X360 0x8284C648. Realmc delete result callback -- streamed assert stub.
    void SaveLoadSystem::DeleteDone()
    {
        CGS_ASSERT(false, CGS_ASSERT_STREAM_MESSAGE_AT("DeleteDone: not implemented.", CGS_ARTIST_FILE, 775));
    }
}

// FOLDED FROM CgsSaveLoadX360_wB_01.cpp
// CgsGui::SaveLoadSystem - wave-B partfile 01 (locale plumbing).
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The class is homed in CgsSaveLoadX360.h
// (its assert sites cite gui/CgsSaveLoadX360.cpp); the base functions live in the
// committed sibling CgsSaveLoadPS3.cpp. This partfile bodies one attested wave-B member:
//
//   LocaleGetStrCallback        @ 0x8284C1B0  (static locale-id lookup)
//
// (MessageChoiceForOptionIndex @0x8284BF50 and HandleMemcardOption @0x8284C6D8 were also
// reconstructed in this partfile by wave B, but the later profile link-closure wave landed
// its own bodies in CgsSaveLoadPS3.cpp -- that TU owns them now; the duplicates were removed
// here in the wave-C reconcile.)
//
// Named-member accessors come from the frozen headers; no raw offset arithmetic.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"   // MUST be first (fwd-decls ::LanguageManager/::SystemUserProfile at global scope)

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"        // CGS_ASSERT
#include "SDKs/Realmc/RealmcMemcardInterface.h"           // RealmcIface::MemcardInterface::MessageChoice

namespace
{
    // ---- Locale string-id table (X360 off_82F331B0, idat-dumped) --------------------------------
    // maRealmemcardStringIDs[28] -- LocaleGetStrCallback maps a 0..27 string id onto the localisation
    // key the Realmc memory-card layer looks up. File-scope static per the keystone spec (A3); the
    // table is this partfile's private rodata.
    static const char* const maRealmemcardStringIDs[28] =
    {
        /*  0 */ "SAVELOAD_NO_STORAGE_SELECTED",
        /*  1 */ "MCM_MEMCARD_MESSAGES_VERSION",
        /*  2 */ "MCM_COMPATIBLE_REALMEMCARD_VERSION",
        /*  3 */ "SAVELOAD_CONTINUE",
        /*  4 */ "SAVELOAD_CONTINUE_WITHOUT_SAVING",
        /*  5 */ "SAVELOAD_SELECT_A_DEVICE",
        /*  6 */ "GENERAL_OPTION_YES",
        /*  7 */ "GENERAL_OPTION_NO",
        /*  8 */ "SAVELOAD_CANCEL",
        /*  9 */ "SAVELOAD_NO_DEVICE_SELECTED",
        /* 10 */ "SAVELOAD_LOAD_CORRUPT",
        /* 11 */ "SAVELOAD_LOAD_FAILED",
        /* 12 */ "SAVELOAD_LOAD_FAILED_CARD_REMOVED",
        /* 13 */ "TCR_FILE_EXISTS_OK_TO_PROCEED",
        /* 14 */ "SAVELOAD_SAVE_OVERWRITE_PLAYER_DATA",
        /* 15 */ "SAVELOAD_SAVE_WARNING",
        /* 16 */ "SAVELOAD_SAVE_FAILED",
        /* 17 */ "SAVELOAD_SAVE_FAILED_CARD_REMOVED",
        /* 18 */ "SAVELOAD_SAVE_DELETE_CONFIRM",
        /* 19 */ "TCR_DELETE_WARNING",
        /* 20 */ "TCR_DELETE_FAILED",
        /* 21 */ "TCR_DELETE_FAILED_CARD_REMOVED",
        /* 22 */ "SAVELOAD_SAVE_INSUFFICIENT_SPACE",
        /* 23 */ "SAVELOAD_NOT_SIGNED_IN",
        /* 24 */ "SAVELOAD_DEVICE_REMOVED_OR_CHANGED",
        /* 25 */ "SAVELOAD_SAVE_OVERWRITE_CORRUPT_PLAYER_DATA",
        /* 26 */ "TCR_NOT_SIGNED_IN_A",
        /* 27 */ "SAVELOAD_NO_DATA",
    };
}

namespace CgsGui
{
    // X360 0x8284C1B0. The locale string-id lookup callback handed to the Realmc memory card.
    // Range-asserts the id, then returns the table entry. The X360 indexes the table even on a
    // failed assert (no clamp) -- reproduced faithfully.
    const char* SaveLoadSystem::LocaleGetStrCallback(u32 luStringID)
    {
        CGS_ASSERT(luStringID < (sizeof(maRealmemcardStringIDs) / sizeof(maRealmemcardStringIDs[0])),
            CGS_ASSERT_MESSAGE_AT(
                "liStringID < M_ARRAY_SIZE(maRealmemcardStringIDs)",
                CGS_ARTIST_FILE, 309 /* ARTIST 0x8284C1D4 */));
        return maRealmemcardStringIDs[luStringID];
    }
}

// FOLDED FROM CgsSaveLoadX360_wB_04.cpp
#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"
#include "SDKs/Realmc/RealmcSaveInfo.h"
#include "SDKs/Realmc/RealmcTitleInfo.h"
#include "SDKs/Realmc/RealmcSaveReq.h"
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"
namespace CgsGui
{
// 8284C818: native aggregate return, hidden destination r3 and this r4.
RealmcIface::SaveInfo SaveLoadSystem::CreateRealmcSaveInfo()
{
    RealmcIface::DataBuffer data;
    data.mpData = mpContentInfoFileBuffer;
    data.muSize = static_cast<u32>(miContentInfoFileSize);
    RealmcIface::EntryContentName name(macwMetadataTitle, macTitle,
        miExtraFilesSizeBytes * miNumberOfMugshotsPerType * miNumberOfMugshotTypes
        + mStoredData.miSize);
    return RealmcIface::SaveInfo(name, data);
}
// 8284C898
RealmcIface::SaveCheckParams SaveLoadSystem::CreateRealmcSaveCheckParams()
{
    RealmcIface::TitleInfo& title = RealmcIface::TitleInfo::Empty();
    RealmcIface::SaveInfo info = CreateRealmcSaveInfo();
    RealmcIface::SaveReq request(macTitle, &info, 0x100u, mStoredData.miSize, &title.muTitle);
    RealmcIface::SaveReq* pRequest = &request;
    return RealmcIface::SaveCheckParams(1, &pRequest);
}
// 82852368
void SaveLoadSystem::EnableAutosave(s32)
{
    RealmcIface::SaveCheckParams params = CreateRealmcSaveCheckParams();
    mpMemcardInterface->SetAutosave(1, &params);
}
}

// FOLDED FROM CgsSaveLoadX360_wB_07.cpp
// CgsGui::SaveLoadSystem -- wave-B load lane, reconstructed from BURNOUT_X360_ARTIST.XEX.
// Bodies for two of the X360 save/load front-end's load-path functions:
//
//   LoadDone              @ 0x82855FE0  -- Realmc load result callback: success re-arms the
//                                          autosave pre-flight, then reports 0/1 to the
//                                          result handler.
//   LoadReady            @ 0x828521A0  -- Realmc load-ready callback: hand the title-save
//                                          data pair { muSaveDataSizeKb, mStoredData.miSize }
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
    void SaveLoadSystem::LoadDone(u32 luResult, s32, u32, void*)
    {
        if (luResult)
        {
            mpResultHandler
                ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(1));
            return;
        }

        EnableAutosave(-1);
        mpResultHandler
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(0));
    }

    // X360 0x828521A0. Realmc load-ready callback. Only the last parameter (the caller's
    // DataBuffer) is read: fill it with the title-save data pair
    // mStoredData { data, size } pair and return 0. The four leading parameters are unrecovered.
    s32 SaveLoadSystem::LoadReady(s32 /*liArg1*/, s32 /*liArg2*/, s32 /*liArg3*/, s32 /*liArg4*/,
                                  RealmcIface::DataBuffer* lpDataBuffer)
    {
        RealmcIface::DataBuffer lPair;
        lPair.mpData = mStoredData.mpData;
        lPair.muSize = mStoredData.miSize;
        *lpDataBuffer = lPair;
        return 0;
    }
}

// FOLDED FROM CgsSaveLoadX360_wC_00.cpp
// CgsGui::SaveLoadSystem -- wave-C bootup/retry lane, reconstructed from BURNOUT_X360_ARTIST.XEX.
// Bodies for the three remaining bootup/save retry-prompt functions:
//
//   BootupCheckDone         @ 0x82859A38 -- Realmc bootup-check result callback: success latches
//                                           mbField183/mbField180 + re-arms the autosave
//                                           pre-flight and reports success; the 1..2 failure
//                                           lanes either arm the SAVELOAD_RETRY_BOOT prompt (a
//                                           signed-in user exists) or report cancelled; >= 3
//                                           fires "Should not get here".
//   BootupHandleRetryBootup @ 0x82855B80 -- the retry-bootup prompt handler: RETRY restarts the
//                                           boot-up load, otherwise the boot-up is abandoned
//                                           (mbField183 = true, mbField180 = false) and failure
//                                           is reported.
//   SaveHandleRetry         @ 0x82856188 -- the save-retry prompt handler: RETRY re-runs Save(),
//                                           otherwise mbField180 = false and failure is reported.
//
// The class layout + member names are frozen in CgsSaveLoadX360.h (X360-authoritative offset map);
// nothing here edits a header. `this` conventions (see the header's note):
//   * BootupCheckDone is a Realmc RESULT callback -- Hex-Rays shows it the +4 interface
//     sub-object, so every displacement in its pseudocode is (real member offset - 4)
//     (raw 0x12C -> mpActiveMessageDisplay +0x130, raw 0x130 -> mpMessageDisplay +0x134,
//     raw 0x17C -> mbField180 +0x180, raw 0x17F -> mbField183 +0x183, raw 0x194 -> the
//     mMessageDisplayOptionFunc pair +0x198, raw 0x20C -> miField210 +0x210). Its
//     `EnableAutosave(a1 - 4)` call is therefore the plain member call EnableAutosave(-1)
//     (r3 = the real `this`, r4 = -1).
//   * BootupHandleRetryBootup / SaveHandleRetry are HandleOption-dispatched option handlers and
//     receive the real `this`; their displacements are the real X360 offsets.
// All three are written as plain member functions over the NAMED members -- no this-arithmetic
// and no raw offset casts.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"   // CgsGui::SaveLoadSystem (FIRST -- see note)

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace
{
    // Localisation string-table entries (X360 rodata pointers).
    const char* const KSTR_RETRY_BOOT             = "SAVELOAD_RETRY_BOOT";              // off_82F331A0
    const char* const KSTR_RETRY                  = "SAVELOAD_RETRY";                   // off_82F331AC
    const char* const KSTR_CONTINUE_WITHOUT_SAVING = "SAVELOAD_CONTINUE_WITHOUT_SAVING"; // off_82F331C0
}

namespace CgsGui
{
    // X360 0x82859A38. Realmc bootup-check result callback (+4 sub-object -- written as a plain
    // member). luResult 0 == the check succeeded, 1..2 are the failure lanes, >= 3 is impossible.
    void SaveLoadSystem::BootupCheckDone(u32 luResult)
    {
        if (luResult != 0)                                   // cmplwi r4,1; blt -> success lane
        {
            if (luResult >= 3)                               // cmplwi r4,3; blt -> prompt lane
            {
                CGS_ASSERT_AT(false, "Should not get here", CGS_ARTIST_FILE, 558);    // CgsSaveLoadX360.cpp:558
                return;
            }

            if (miField210 >= 0)                             // lwz r10,0x20C(r11); cmpwi/blt
            {
                // The retry prompt's pending option handler = &BootupHandleRetryBootup. The X360
                // builds the member-fn pointer { func @+0x198, delta = 0 @+0x19C } on the stack and
                // stores the native 8-byte member pointer with `std r9,0x194(r11)`.
                mMessageDisplayOptionFunc = &SaveLoadSystem::BootupHandleRetryBootup;

                // The X360 reuses the same stack slot for the pair and then the options array.
                const char* lapcOptions[2];
                lapcOptions[0] = KSTR_RETRY;
                lapcOptions[1] = KSTR_CONTINUE_WITHOUT_SAVING;

                // Direct virtual on the display passed to Construct (r3 = raw 0x130 == +0x134),
                // handler = this as the embedded OptionHandler (r4 = r11 - 4), count = 2.
                mpMessageDisplay->ShowMessage(static_cast<MessageDisplay::OptionHandler*>(this),
                                              KSTR_RETRY_BOOT, lapcOptions, 2);
                return;
            }

            // No signed-in user: nothing to retry with -- report cancelled through the bound
            // result handler (r3 = raw 0x12C == mpActiveMessageDisplay +0x130, r4 = 2).
            mpResultHandler
                ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_CANCELLED);
            return;
        }

        mbField183 = true;      // stb r10,0x17F(r11) -- +0x183 first
        mbField180 = true;      // stb r10,0x17C(r11) -- +0x180
        EnableAutosave(-1);     // r3 = the real this (r11 - 4), r4 = -1
        mpResultHandler
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_SUCCESS);
    }

    // X360 0x82855B80. The retry-bootup prompt handler armed by BootupCheckDone's failure lane
    // (real `this`). Option 1 == RETRY: restart the boot-up load (the X360 tail-calls BootupStart
    // with r4 = 0 -- its parameter is the never-read HandleOption dispatch argument). Otherwise
    // the boot-up is finished without a save and failure is reported.
    void SaveLoadSystem::BootupHandleRetryBootup(u32 luOption)
    {
        if (luOption == 1)
        {
            BootupStart(0);
            return;
        }

        mbField183 = true;      // stb r10,0x183(r3)
        mbField180 = false;     // stb r9,0x180(r3)
        mpResultHandler
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_FAILURE);
    }

    // X360 0x82856188. The save-retry prompt handler (real `this`). Option 1 == RETRY: re-run the
    // write (the X360 tail-calls the zero-argument Save @0x82856040). Otherwise disarm the autosave
    // flag and report failure.
    void SaveLoadSystem::SaveHandleRetry(u32 luOption)
    {
        if (luOption == 1)
        {
            Save();
            return;
        }

        mbField180 = false;     // stb r10,0x180(r3)
        mpResultHandler
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_FAILURE);
    }
}

// FOLDED FROM CgsSaveLoadX360_wC_01.cpp
// CgsGui::SaveLoadSystem -- wave-C fan-out partfile 01 (message / card lane).
// X360 ARTIST reconstruction of the two remaining Realmc result callbacks:
//   CardRemoved  (0x8284CC20), ClearMessage (0x8284CAD8).
// Bodies only -- the layout and every declaration are frozen in the headers.
//
// `this` convention: both are Realmc RESULT callbacks, so Hex-Rays shows them the +4
// interface sub-object and every displacement in the pseudocode is (real member offset - 4)
// -- e.g. raw 0x17C == mbField180 (+0x180), raw 0x194 == mMessageDisplayOptionFunc (+0x198).
// They are written here as plain member functions over the NAMED members; no this+/-4
// arithmetic and no raw offset casts appear anywhere below.
//
// Include-order rule (load-bearing): CgsSaveLoadX360.h FIRST -- it forward-declares
// ::SystemUserProfile / ::LanguageManager at GLOBAL scope; pulling a CgsGui/CgsLanguage
// header in first would flip the member types inside namespace CgsGui (ODR hazard).
#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"            // CgsUnicode::ConvertUtf8ToUtf16
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // CgsLanguage::LanguageManager::FindString
#include "GameShared/GameClasses/System/CgsXOverlapped.h"       // CgsSystem::CgsXOverlapped::Construct

namespace
{
    // Localisation keys the card-removed prompt looks up. X360 rodata pointers (both are
    // entries of the maRealmemcardStringIDs table LocaleGetStrCallback indexes).
    const char* const KSTR_DEVICE_REMOVED_OR_CHANGED = "SAVELOAD_DEVICE_REMOVED_OR_CHANGED"; // off_82F33210



}

namespace CgsGui
{
    // X360 0x8284CC20. Realmc card-removed callback: disarm autosave, mark the overlapped op
    // in flight, stage the localised "device removed or changed" strings into UTF-16 stack
    // buffers and pop the system message box with a single CONTINUE-WITHOUT-SAVING button.
    // The call is expected to complete asynchronously (ERROR_IO_PENDING).
    void SaveLoadSystem::CardRemoved()
    {
        mbField180     = false;   // stb r10(0), raw 0x17C -> real 0x180
        mbAsyncOpState = 1;       // stb r9(1),  raw 0x220 -> real 0x224

        // Stack buffer sizes are the asm's: title 64 bytes, button 128 bytes, text 288 bytes.
        u16 laTitleText[32];      // var_1E0 (v6)
        u16 laMessageText[144];   // var_120 (v8)
        u16 laButtonText[64];     // var_1A0 (v7)

        CgsUnicode::ConvertUtf8ToUtf16(
            reinterpret_cast<const CgsUnicode::CgsUtf8*>("Warning"), laTitleText);

        // raw 0x184 -> real 0x188 (mpLanguageManager)
        const u8* lpMessageText = mpLanguageManager
                                      ->FindString(KSTR_DEVICE_REMOVED_OR_CHANGED);
        CgsUnicode::ConvertUtf8ToUtf16(lpMessageText, laMessageText);

        const u8* lpButtonText = mpLanguageManager
                                     ->FindString(KSTR_CONTINUE_WITHOUT_SAVING);
        CgsUnicode::ConvertUtf8ToUtf16(lpButtonText, laButtonText);

        const wchar_t* lapwButtons[1] = { reinterpret_cast<const wchar_t*>(laButtonText) };   // var_1F0 (v5)

        // raw 0x224 -> real 0x228: the embedded XOVERLAPPED gets zeroed before the async call.
        mOverlapped.Construct();

        const u32 luResult = XShowMessageBoxUI(
            static_cast<u32>(miField210),                  // dwUserIndex, raw 0x20C -> real 0x210
            reinterpret_cast<const wchar_t*>(laTitleText),                                   // wszTitle
            reinterpret_cast<const wchar_t*>(laMessageText),                                 // wszText
            1,                                             // cButtons
            lapwButtons,                                   // pwszButtons
            0,                                             // dwFocusButton
            2,                                             // dwFlags
            &mMessageBoxResult,   // pResult, raw 0x240 -> real 0x244
            reinterpret_cast<PXOVERLAPPED>(&mOverlapped));                                 // the stack-passed XOVERLAPPED*

        CGS_ASSERT_AT(luResult == ERROR_IO_PENDING, "lResult == ERROR_IO_PENDING", CGS_ARTIST_FILE, 1352);
    }

    // X360 0x8284CAD8. If a pending message-display option member function is armed, hide the
    // prompt and clear the { func, delta } pair (the X360 zeroes both words with one 8-byte std).
    void SaveLoadSystem::ClearMessage()
    {
        // The test reads ONLY the func word (`lwz r11,0x194` -> real 0x198).
        if (mMessageDisplayOptionFunc != 0)
        {
            // raw 0x130 -> real 0x134 (mpMessageDisplay), vtable slot +0x8 == HideMessage.
            mpMessageDisplay->HideMessage();

            mMessageDisplayOptionFunc = 0;
        }
    }
}

#undef CGS_ARTIST_FILE
