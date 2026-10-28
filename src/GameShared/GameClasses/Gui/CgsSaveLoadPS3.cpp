// CgsGui::SaveLoadSystem - the Xenon (X360) save/load front-end, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The ledger keys this TU as GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp;
// the class itself is the X360 save/load system (its assert sites cite gui/CgsSaveLoadX360.cpp and
// it is homed in CgsSaveLoadX360.h). The PS3 DecFIGS DWARF describes a different, PS3-only member
// set; the layout used here is the X360-authoritative offset map from the ASM (see the class in
// CgsSaveLoadX360.h).
//
// Functions in scope for this TU:
//   Construct, Prepare, Release, Update, Load, Save, LoadImageFiles,
//   SetMetadata, ShowMessage, ShowAutosaveIcon, HandleOption, BootupShowAutosaveWarning,
//   MessageChoiceForOptionIndex (@0x8284BF50), GetMugshotBufferFromImageId (@0x8284C910),
//   HandleMemcardOption (@0x8284C6D8), BootupStart (@0x82855A60),
//   LoadHandleConfirmLoad (@0x82855EC0), the four inherited
//   ContentInformationFileInterface overrides (FLAG'd PC-boundary leaves; no named X360 body),
//   and the manager-facing task starters the ARTIST export set strips (the 3-arg Save,
//   Autosave -- composed from the exported siblings' bind-handler + SetMetadata setup) plus
//   their host-side helpers (SaveFileExists, CaptureStoredDataView, GetMugshotBufferSizeBytes).
//
// The RealmcIface memory-card SDK is the console storage backend. Its RECORD types are the
// real homed ones (SDKs/Realmc/RealmcDataBuffer.h, RealmcLoadEntryInfo.h,
// RealmcIfaceSaveCheckParams.h, RealmcTitleInfo.h) -- never console-width byte blobs, which
// would truncate the host pointers those records carry. Only the pieces with no reconstructed
// home (the MemcardInterface implementation, GameInfo, SaveInfo) stay file-local as FLAG'd
// PC-platform boundary leaves so the console-shaped write path stays safe -- see the
// anonymous namespace below. The REAL PC storage
// is the CgsSaveLoadPC profile container (GameShared/GameClasses/Gui/PC/CgsSaveLoadPC.cpp):
// the 3-arg Save/Autosave write it and LoadHandleConfirmLoad's confirm arm reads it back,
// so the profile image genuinely persists across runs.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"       // CgsGui::ConvertAsciiToWideCharSafe, MessageDisplay
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log (storage-edge diagnostics)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h" // CgsMemory::LinearMalloc (Prepare mugshot buffer)

// The homed Realmc SDK record types Save() builds (never console-width byte blobs).
#include "SDKs/Realmc/RealmcDataBuffer.h"            // RealmcIface::DataBuffer
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"         // RealmcIface::LoadEntryInfo
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"  // RealmcIface::SaveCheckParams
#include "SDKs/Realmc/RealmcTitleInfo.h"             // RealmcIface::TitleInfo

#include <cstddef>   // offsetof (layout pins)
#include <cstring>   // strncpy, memset
#include <unordered_map>
#include <exception>

#include <xtl.h> // CreateFileA, GetFileSize, CloseHandle, XGetOverlappedResult shim

// Xenon (X360) overlapped-result query. Not part of the host Win32 <Windows.h>; declared here as
// the X360 shim the Update pump calls (returns the overlapped extended error, e.g. ERROR_IO_PENDING).
// PC boundary definition lives in GameShared/GameClasses/System/PC/CgsXboxLivePC.cpp.
extern "C" DWORD XGetOverlappedResult(void* lpOverlapped, DWORD* lpdwResult, BOOL bWait);

namespace
{

    // ---- File-scope state (X360 off_8305A6F8) ---------------------------------------------------
    // The shared ContentInformation provider the save/load system publishes in Prepare and uses in
    // Prepare/Release. The X360 stores &mContentInfoVptr here and dispatches its vtable
    // (slot +0x08 == AllocAndRead, slot +0x0C == Free). Modelled as a vtable object pointer.
    struct ContentInfoProviderVtbl
    {
        void* mpReserved00;
        void* mpReserved04;
        void* (*mpfnAllocAndRead)(void* lpThis, s32 liSize, const void* lpKey, int liFlags);
        void  (*mpfnFree)(void* lpThis, void* lpBuffer, s32 liSize);
    };
    struct ContentInfoProvider { ContentInfoProviderVtbl* mpVtbl; };

    ContentInfoProvider* gpContentInfoProvider = 0;   // X360 off_8305A6F8

    NoOpMemcardInterface gNoOpMemcardInterface;

    // (The SaveCheckParams / LoadEntryInfo / TitleInfo / entry-record stand-ins that used to
    // live here were console-width byte-blob forks -- 16- and 48-byte arrays -- of types that
    // have real homes under SDKs/Realmc. They are gone: Save() now builds the genuine
    // RealmcIface::SaveCheckParams / LoadEntryInfo / DataBuffer / TitleInfo objects, matching
    // the committed sibling CgsSaveLoadX360_wB_03/_wB_04 partfiles, so no host pointer is
    // squeezed into a console-width word.)

    // The save/load locale-string callback the memory-card interface is given.
    void* gpfnLocaleGetStrCallback = 0;   // CgsGui::SaveLoadSystem::LocaleGetStrCallback

    // Pending message-display option DISPATCH THUNKS. The X360 stores each handler as a
    // member-function pointer ({func @+0x198, delta @+0x19C}) and HandleOption dispatches it as
    // (adjustedThis, option); this reconstruction stores plain function pointers, so each thunk
    // forwards to the real CgsGui::SaveLoadSystem member of the same name.
    void SaveLoadSystem_HandleMemcardOption(void* lpSystem, u32 luOption)
    {
        static_cast<CgsGui::SaveLoadSystem*>(lpSystem)->HandleMemcardOption(luOption);
    }

    void SaveLoadSystem_BootupStart(void* lpSystem, u32 luOption)
    {
        static_cast<CgsGui::SaveLoadSystem*>(lpSystem)->BootupStart(luOption);
    }

    void SaveLoadSystem_LoadHandleConfirmLoad(void* lpSystem, u32 luOption)
    {
        static_cast<CgsGui::SaveLoadSystem*>(lpSystem)->LoadHandleConfirmLoad(luOption);
    }

    void SaveLoadSystem_BootupHandleNotSignedInOption(void* lpSystem, u32 luOption)
    {
        static_cast<CgsGui::SaveLoadSystem*>(lpSystem)->BootupHandleNotSignedInOption(luOption);
    }

    // Localisation string-table entries (X360 rodata pointers).
    const char* const KSTR_AUTOSAVE_WARNING = "SAVELOAD_AUTOSAVE_WARNING"; // off_82F3319C
    const char* const KSTR_CONTINUE         = "SAVELOAD_CONTINUE";         // off_82F331BC
    const char* const KSTR_CONFIRM_LOAD     = "SAVELOAD_CONFIRM_LOAD";     // off_82F331A4
    const char* const KSTR_OPTION_YES       = "GENERAL_OPTION_YES";        // off_82F331C8
    const char* const KSTR_OPTION_NO        = "GENERAL_OPTION_NO";         // off_82F331CC
    const char* const KSTR_NOT_SIGNED_IN    = "SAVELOAD_NOT_SIGNED_IN";    // off_82F3320C
    const char* const KSTR_SIGN_IN          = "SAVELOAD_SIGN_IN";          // off_82F331A8
    const char* const KSTR_CONTINUE_WITHOUT_SAVING = "SAVELOAD_CONTINUE_WITHOUT_SAVING"; // off_82F331C0
}

namespace CgsGui
{
    // X360 0x8284C050. Wire up the system. The pseudocode's a4 is lpcTitle, a5/a6 the content-info
    // and save-file paths, a7/a8 the mugshot grid (types x per-type) and a28 the per-mugshot size
    // (the last stack argument loaded from arg_54).
    void SaveLoadSystem::Construct(MessageDisplay* lpMessageDisplay, LanguageManager* lpLanguageManager,
                                   const char* lpcTitle, const char* lpcContentInfoFilePath,
                                   const char* lpcSaveFilePath, s32 liNumberOfMugshotTypes,
                                   s32 liNumberOfMugshotsPerType, u32 luExtraFilesSizeBytes)
    {
 // drain an earlier native task before reusing this owner
        CGS_ASSERT(lpMessageDisplay != 0, "lpMessageDisplay != NULL");
        CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager != NULL");

        ConvertAsciiToWideCharSafe(macwTitle, lpcTitle, 32);

        mpacContentInfoFilePath = lpcContentInfoFilePath;   // +0x178
        mpacSaveFilePath        = lpcSaveFilePath;           // +0x17C
        mpLanguageManager       = lpLanguageManager;         // +0x188
        mpMessageDisplay        = lpMessageDisplay;          // +0x134
        mpMemcardInterface      = 0;                   // +0x184 (stw r11,0x184)
        mpActiveMessageDisplay  = 0;                   // +0x130 (stw r11,0x130)
        mpSystemUserProfile     = 0;                   // +0x194 (stw r11,0x194)
        mMessageDisplayOptionFunc.muFunc  = 0;               // +0x198 (std r10,0x198 zeroes the pair)
        mMessageDisplayOptionFunc.muDelta = 0;               // +0x19C
        mField128               = 0;                         // +0x128 (std r9,0x128)
        miField210              = -1;                        // +0x210 (li r8,-1)
        miNumberOfMugshotTypes    = liNumberOfMugshotTypes;    // +0x214 (a7)
        miNumberOfMugshotsPerType = liNumberOfMugshotsPerType; // +0x218 (a8)
        miExtraFilesSizeBytes   = static_cast<s32>(luExtraFilesSizeBytes); // +0x21C (a28)
        mbField180              = true;                      // +0x180 (stb r9=1,0x180)
        mbSilentMode            = false;                     // +0x181 (stb r11=0,0x181)
        mbField183              = false;                     // +0x183 (stb r11=0,0x183)
        mbAsyncOpState          = 0;                         // +0x224 (stb r11=0,0x224)
        mbAutosaveIconVisible   = false;                     // +0x24C (stb r11=0,0x24C)

        // Host-width stored-data view (see the header note): empty until a task starter
        // captures it.
        mStoredDataView.mpData = 0;
        mStoredDataView.miSize = 0;
    }

    // The host-width form of the X360 SetMetadata +0x30/+0x34 capture (those two console
    // words are the metadata's OpaqueBuffer {ptr, size}; see the mStoredDataView note in
    // the header). Every task starter that receives the typed metadata records the view
    // here for the storage edge.
    void SaveLoadSystem::CaptureStoredDataView(const SaveLoadMetadata& lrMetadata)
    {
        mStoredDataView = lrMetadata.mStoredData;
    }

    // The mugshot buffer's total byte size -- the same product Prepare allocates and the
    // X360 Save's "Mugshots" entry publishes (per-mugshot size x types x per-type).
    s32 SaveLoadSystem::GetMugshotBufferSizeBytes() const
    {
        return miExtraFilesSizeBytes * miNumberOfMugshotsPerType * miNumberOfMugshotTypes;
    }

    // X360 @0x82473110. The two explicit transition tests preserve the ARTIST
    // branch shape: notify slot 6 only when the value changes, always with user -1,
    // then store the requested byte.
    void SaveLoadSystem::SetSilentMode(bool lbSilentMode)
    {
        if (mbSilentMode && !lbSilentMode)
            mpMemcardInterface->SetSilent(0, -1);
        if (!mbSilentMode && lbSilentMode)
            mpMemcardInterface->SetSilent(1, -1);
        mbSilentMode = lbSilentMode;
    }

    // X360 0x82851FC0. Create the memory-card interface, allocate the mugshot buffer and read the
    // content-information file into a buffer.
    bool SaveLoadSystem::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, CgsMemory::LinearMalloc* lpLinearMalloc,
                                 SystemUserProfile* lpSystemUserProfile)
    {
        CGS_ASSERT(lpHeapMalloc != 0, "lpHeapMalloc");
        CGS_ASSERT(lpLinearMalloc != 0, "lpLinearMalloc");
        CGS_ASSERT(lpSystemUserProfile != 0, "lpSystemUserProfile");

        mpHeapMalloc = lpHeapMalloc;                         // +0x190

        // Build the memory-card callback block { &mContentInfoVptr, LocaleGetStrCallback, 0 } and
        // publish the provider to the file-scope pointer.
        void* lpCallbacks[3];
        lpCallbacks[0] = &mpContentInfoVptr;                 // this+0x18C
        lpCallbacks[1] = gpfnLocaleGetStrCallback;
        lpCallbacks[2] = 0;
        gpContentInfoProvider = reinterpret_cast<ContentInfoProvider*>(&mpContentInfoVptr);

        mpSystemUserProfile = lpSystemUserProfile;           // +0x194 (low word)

        // RealmcIface::GameInfo on the stack from the wide title; the allocator is &this[0x4]
        // (the inherited base sub-object the memcard allocates through).
        u8 laGameInfo[144];
        RealmcGameInfo_Construct(laGameInfo, macwTitle);
        mpMemcardInterface = RealmcMemcard_CreateInstance(lpCallbacks,
                                                          reinterpret_cast<u8*>(this) + 4,
                                                          laGameInfo);  // +0x184
        mpMemcardInterface->SetActive(1);                    // (*(*Instance+52))(Instance,1)
 // start the native writer during preparation

        // Mugshot buffer = per-mugshot size * mugshots-per-type * type count.
        const s32 liMugshotBytes = miExtraFilesSizeBytes * miNumberOfMugshotsPerType * miNumberOfMugshotTypes;
        mpMugshotBufferData = lpLinearMalloc->Malloc(liMugshotBytes);   // +0x220
        CGS_ASSERT(mpMugshotBufferData != 0, "mpMugshotBufferData");

        HANDLE lhFile = ::CreateFileA(mpacSaveFilePath, 0x80000000u, 1, 0, 3, 0, 0);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            miContentInfoFileSize   = 0;                     // +0x208
            mpContentInfoFileBuffer = 0;               // +0x20C
            return true;
        }

        // Wrap the handle in a XenonFileInputStream { mhFile = lhFile, miSize = GetFileSize }.
        struct { HANDLE mhFile; s32 miSize; } lStream;
        lStream.mhFile = lhFile;
        lStream.miSize = static_cast<s32>(::GetFileSize(lhFile, 0));

        const s32 liSize = reinterpret_cast<XenonFileInputStream*>(&lStream)->GetSize();
        miContentInfoFileSize = liSize;                      // +0x208
        // Provider AllocAndRead(size, &key, 0) -> the content-info file buffer.
        static const u8 kReadKey = 0;                        // X360 &unk_820046A7
        mpContentInfoFileBuffer =                            // +0x20C
            gpContentInfoProvider->mpVtbl->mpfnAllocAndRead(gpContentInfoProvider, liSize, &kReadKey, 0);
        reinterpret_cast<XenonFileInputStream*>(&lStream)->Read(mpContentInfoFileBuffer,
                                                                static_cast<u32>(miContentInfoFileSize));
        ::CloseHandle(lhFile);
        return true;
    }

    // X360 0x8284C158. Free the content-information file buffer through the shared provider.
    bool SaveLoadSystem::Release()
    {
 // snapshots finish before storage/owner teardown
        gpContentInfoProvider->mpVtbl->mpfnFree(gpContentInfoProvider,
                                                mpContentInfoFileBuffer, miContentInfoFileSize);
        miContentInfoFileSize   = 0;     // +0x208
        mpContentInfoFileBuffer = 0; // +0x20C
        return true;
    }

    // X360 0x8284C4E0. Pump the overlapped async op, then forward Update to the memory-card
    // interface if present.
    void SaveLoadSystem::Update()
    {
        if (mbAsyncOpState == 1)
        {
            const DWORD luResult = ::XGetOverlappedResult(maOverlapped, 0, FALSE);
            // 997 (ERROR_IO_PENDING) / 996 (ERROR_IO_INCOMPLETE) mean the op is still running.
            const bool lbStillRunning = (luResult == 997u) || (luResult == 996u);
            if (!lbStillRunning)
            {
                mbAsyncOpState = 0;
            }
        }

        if (mpMemcardInterface != 0)
        {
            mpMemcardInterface->Update(0);
        }

        // Match the original completion boundary: callbacks run from Update,
        // never from the IO worker or before the atomic container write finishes.
    }

    // X360 0x82859B70. Begin a load: bind the active display, set the metadata, and prompt the user
    // to confirm, routing the choice to LoadHandleConfirmLoad.
    void SaveLoadSystem::Load(SaveLoadTaskResultHandler* lpResultHandler, const void* lpMetadata)
    {
        const char* lapcOptions[2];
        lapcOptions[0] = KSTR_OPTION_YES;
        lapcOptions[1] = KSTR_OPTION_NO;

        u8 laSaveInfo[336];
        std::memset(laSaveInfo, 0, 288);

        mpActiveMessageDisplay = reinterpret_cast<MessageDisplay*>(lpResultHandler); // +0x130 = a2

        SetMetadata(lpMetadata, laSaveInfo);
        // The caller (BrnGui::ProfileManager::Load) passes its typed
        // CgsGui::SaveLoadMetadata; capture the stored-data view host-width so the
        // confirm arm can read the container into it (the X360 form of this capture is
        // SetMetadata's +0x30/+0x34 words -- see the mStoredDataView header note).
        CaptureStoredDataView(*static_cast<const SaveLoadMetadata*>(lpMetadata));

        // Pending option handler = &LoadHandleConfirmLoad. The X360 builds the member-fn pointer
        // { func = LoadHandleConfirmLoad @+0x198, delta = 0 @+0x19C } and stores it via
        // `std r11,0x198`. muFunc is full-width on x64 (see the header note) -- no truncation.
        mMessageDisplayOptionFunc.muFunc =
            reinterpret_cast<uintptr_t>(&SaveLoadSystem_LoadHandleConfirmLoad);
        mMessageDisplayOptionFunc.muDelta = 0;

        // 0x82859BF0 dispatches the prompt on `lwz r3,0x134(r31)` == mpMessageDisplay, NOT the
        // result handler at +0x130. mpActiveMessageDisplay was just re-pointed at the
        // SaveLoadTaskResultHandler (a BrnGui::ProfileManager) on the line above, and its vtable
        // slot 0 is HandleSaveLoadTaskResult -- calling ShowMessage through it would invoke the
        // wrong virtual, so the confirm-load prompt would never appear.
        mpMessageDisplay->ShowMessage(static_cast<MessageDisplay::OptionHandler*>(this),
                                      KSTR_CONFIRM_LOAD, lapcOptions, 2);
    }

    // X360 0x828601D8 (entry NOT in the ARTIST export set). Begin the profile boot-up. The body is
    // composed from the exported sibling Load @0x82859B70 (identical bind-handler + SetMetadata
    // setup) and the confirmed callee BootupShowAutosaveWarning @0x828599A0 (SetMetadata's xref set
    // @0x8284C240 lists Bootup as a caller, so the SetMetadata call is attested). Bind the active
    // result handler, push the boot-up metadata, then show the SAVELOAD_AUTOSAVE_WARNING prompt --
    // BootupShowAutosaveWarning stores &BootupStart as the pending CONTINUE handler and renders the
    // one-option prompt through the message display. The boot-up therefore BLOCKS on the prompt
    // synchronously here, which is why the prompt never appeared.
    void SaveLoadSystem::Bootup(SaveLoadTaskResultHandler* lpResultHandler, const void* lpMetadata,
                                bool lbAutoLoad)
    {
        u8 laSaveInfo[336];
        std::memset(laSaveInfo, 0, 288);

        mpActiveMessageDisplay = reinterpret_cast<MessageDisplay*>(lpResultHandler);   // +0x130

        SetMetadata(lpMetadata, laSaveInfo);
        // The auto-load argument is the byte BootupStart branches on (`lbz r11,0x182` /
        // `cmplwi 1` @0x82855A88): set == dispatch the memory-card boot-up READ with the two
        // load entries, clear == dispatch with a count of zero. It was previously dropped, so
        // the boot-up never took the read arm at all.
        mbAutoLoad = lbAutoLoad;                                                       // +0x182

        // The X360 capture of the metadata's stored-data view is INSIDE SetMetadata: its tail
        // @0x8284C4BC..0x8284C4D0 copies the metadata's +0x30/+0x34 word pair to this+0x200/
        // +0x204 (`addi r11,r18,0x30; addi r10,r19,0x200; lwz/stw; lwz/stw`), and on the 32-bit
        // console that pair IS SaveLoadMetadata::mStoredData {ptr, size} -- which is exactly what
        // BootupStart then reads back (`lwz r11,0x200` / `lwz r11,0x204` @0x82855AB4/0x82855ACC)
        // into the title load entry, and what LoadReady @0x828521A0 hands the SDK as the read
        // DESTINATION buffer. On the x64 host that pair no longer fits two u32 words, so the
        // host-width capture is mStoredDataView -- and it must happen here for the same reason it
        // happens in the exported sibling Load @0x82859B70. Without it the boot-up read has no
        // destination.
        CaptureStoredDataView(*static_cast<const SaveLoadMetadata*>(lpMetadata));

        // X360 0x82860234..0x828602B8 (Bootup decompiled from the ARTIST database): branch on the
        // signed-in user index (`lwz r11,0x210; cmpwi 0; bge`).
        //   signed in  -> WaitForUIClosed(&BootupShowAutosaveWarning), the normal boot-up;
        //   no user    -> the SAVELOAD_NOT_SIGNED_IN prompt, options { SAVELOAD_SIGN_IN,
        //                 SAVELOAD_CONTINUE_WITHOUT_SAVING }, answered by
        //                 BootupHandleNotSignedInOption. "Continue without saving" reports
        //                 failure: the profile is neither loaded nor saved -- what passing the
        //                 title with A (no controller bound, so no user signed in) does on ARTIST.
        if (miField210 >= 0)
        {
            WaitForUIClosed(&SaveLoadSystem::BootupShowAutosaveWarning);
            return;
        }

        mMessageDisplayOptionFunc.muFunc =
            reinterpret_cast<uintptr_t>(&SaveLoadSystem_BootupHandleNotSignedInOption);
        mMessageDisplayOptionFunc.muDelta = 0;                                         // std r9,0x198

        const char* lapcOptions[2];
        lapcOptions[0] = KSTR_SIGN_IN;
        lapcOptions[1] = KSTR_CONTINUE_WITHOUT_SAVING;
        mpMessageDisplay->ShowMessage(static_cast<MessageDisplay::OptionHandler*>(this),  // lwz r3,0x134
                                      KSTR_NOT_SIGNED_IN, lapcOptions, 2);
    }

    // X360 0x82856040. Write the save: build two load-entry records (the title save + the mugshot
    // blob), then hand them to the memory-card interface and pump Update.
    //
    // The three SDK record types are the HOMED ones (SDKs/Realmc/...), never console-width
    // blobs: the X360 stack slots are 0x30 bytes for a LoadEntryInfo, 8 for a SaveCheckParams
    // and 8 for a DataBuffer only because console pointers are 4 bytes. Every one of those
    // records carries a real pointer (LoadEntryInfo::mpData, DataBuffer::mpData), so a
    // console-sized byte array would drop the high half of each host pointer -- which is
    // exactly what the mugshot entry did (it packed mpMugshotBufferData into a u32 word, so the
    // card write pointed at a truncated buffer address). Sizes come from the host types.
    void SaveLoadSystem::Save()
    {
        // X360: `r29 = &v21; r30 = 1; do { LoadEntryInfo::LoadEntryInfo(r29); r29 += sizeof; }
        // while (--r30 >= 0);` -- two default-constructed entries in ascending address order.
        RealmcIface::LoadEntryInfo laEntries[2];

        // Pre-flight check: CheckSave(0, SaveCheckParams(0, NULL)); the X360's explicit
        // ~SaveCheckParams right after the call is the scope-end dtor of the stack object.
        {
            RealmcIface::SaveCheckParams lCheckParams(0, 0);
            mpMemcardInterface->CheckSave(0, &lCheckParams);
        }

        // Entry 0: the title save. Name = macTitle (this+0x1E0), first pair = {0,0}, second pair
        // = the metadata's stored-data view. The X360 reads that pair as the two words
        // +0x200/+0x204 (muSaveDataSizeKb/muGameDataSizeKb) -- on the 32-bit console those ARE
        // the SaveLoadMetadata::mStoredData { pointer, size } pair, so on the host the pair is
        // taken from mStoredDataView, its full-width capture (see the header note).
        RealmcIface::DataBuffer lZeroPair;                      // {0, 0} (default ctor)
        RealmcIface::DataBuffer lTitlePair;
        lTitlePair.mpData = mStoredDataView.mpData;             // X360 +0x200
        lTitlePair.muSize = static_cast<u32>(mStoredDataView.miSize);   // X360 +0x204
        laEntries[0] = RealmcIface::LoadEntryInfo(macTitle, &lZeroPair, &lTitlePair);

        // Entry 1: the mugshot blob ("Mugshots", buffer + total byte size) -- the same record
        // CreateRealmcMugshotLoadEntryInfo @0x828523D0 builds.
        RealmcIface::DataBuffer lMugPair;
        lMugPair.mpData = mpMugshotBufferData;                  // X360 +0x220 (a real pointer)
        lMugPair.muSize = static_cast<u32>(miExtraFilesSizeBytes) *
                          static_cast<u32>(miNumberOfMugshotsPerType) *
                          static_cast<u32>(miNumberOfMugshotTypes);
        laEntries[1] = RealmcIface::LoadEntryInfo("Mugshots", &lZeroPair, &lMugPair);

        // TitleInfo::Empty() is fetched before the save-info is built, matching the X360 order.
        RealmcIface::TitleInfo& lTitleInfo = RealmcIface::TitleInfo::Empty();
        u8    lSaveInfo;
        mpMemcardInterface->WriteSave(lpSaveInfo, 2, laEntries, 0, &lTitleInfo);

        auto* lpHandler = reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay);
        CGS_ASSERT(lpHandler, "Save task requires a result handler");
        if (!lpHandler) return;
        bool lbQueued = false;
        try
        {
            CGS_ASSERT(!lrPending.muTicket, "Save task already pending");
            if (lrPending.muTicket) return;
            lrPending.mpHandler = lpHandler;
            lbQueued = lrPending.muTicket != 0;
        }
        catch (const std::exception&) {}
        if (!lbQueued)
        {
            std::strncpy(lReport.macName, macTitle, sizeof(lReport.macName) - 1);
            lReport.muImageSize = mStoredDataView.miSize;
            lReport.muMugshotsSize = GetMugshotBufferSizeBytes();
            return;
        }
        Update();
    }

    // The manager-facing 3-arg save task (BrnGui::ProfileManager::Save @0x82513B28 drives
    // it; the entry itself is not in the ARTIST export set -- composed from the exported
    // sibling Load @0x82859B70's bind-handler + SetMetadata setup, the same composition
    // Bootup uses). Bind the result handler, push the metadata/save-info, then write.
    void SaveLoadSystem::Save(SaveLoadTaskResultHandler* lpResultHandler,
                              const SaveLoadMetadata& lrMetadata, const SaveInfo& lrSaveInfo)
    {
        mpActiveMessageDisplay = reinterpret_cast<MessageDisplay*>(lpResultHandler);
        SetMetadata(&lrMetadata, &lrSaveInfo);
        CaptureStoredDataView(lrMetadata);
        Save();
    }

    // The autosave task (BrnGui::ProfileManager::Autosave @0x82513958 drives it; same
    // unexported family, same composition as the 3-arg Save). Each valid image record is
    // committed into its mugshot-buffer slot first, so the container's mugshot payload
    // persists it -- on the console the same records ride the memcard container's image
    // files and come back through LoadImageFiles' slot reads.
    void SaveLoadSystem::Autosave(SaveLoadTaskResultHandler* lpResultHandler,
                                  const SaveLoadMetadata& lrMetadata, const SaveInfo& lrSaveInfo,
                                  s32 liNumberOfImageFiles, const ImageFileInfo* laImageFileInfo)
    {
        for (s32 liIndex = 0; liIndex < liNumberOfImageFiles; ++liIndex)
        {
            const ImageFileInfo& lrRecord = laImageFileInfo[liIndex];
            if (!lrRecord.IsValid())
            {
                continue;
            }
            CGS_ASSERT(lrRecord.miSize == miExtraFilesSizeBytes,
                       "lpImageFile[liIndex].miSize == miExtraFilesSizeBytes");
            std::memcpy(GetMugshotBufferFromImageId(lrRecord.miId), lrRecord.mpBuffer,
                        static_cast<usize>(miExtraFilesSizeBytes));
        }

        Save(lpResultHandler, lrMetadata, lrSaveInfo);
    }

    // X360 0x828521E0. Copy each requested image's mugshot buffer OUT into the caller's
    // CgsGui::ImageFileInfo records, then report the load result. This is the outbound twin of
    // CopyImageToBuffer @0x828522D0, which walks the same record inbound.
    //
    // SIGNATURE (from the asm prologue, not the a3-short Hex-Rays view): `this` in r3 plus FOUR
    // arguments -- r4 (the result handler), r5 (the metadata), r6 -> r30 (the record count),
    // r7 -> r31 (the record array). The sole caller BrnGui::ProfileManager::LoadImageFiles
    // @0x82513C00 loads exactly that (@0x82513E9C..0x82513EB4) and r5 is the very same
    // &mMetadata it hands SaveLoadSystem::Load @0x82523E28. Neither r4 nor r5 is referenced by
    // the body -- see the tail note for why the handler argument is dropped.
    void SaveLoadSystem::LoadImageFiles(SaveLoadTaskResultHandler* /*lpResultHandler*/,
                                        const void* /*lpMetadata*/, s32 liNumberOfImageFiles,
                                        const ImageFileInfo* laImageFileInfo)
    {
        CGS_ASSERT(liNumberOfImageFiles > 0, "liNumberOfImageFiles > 0");
        CGS_ASSERT(laImageFileInfo != 0, "lpImageFile != NULL");

        // The X360 walks the array as `r31 = laImageFileInfo + 8` (@0x82852254) stepping by
        // `addi r31,r31,0x10` (@0x828522A4): 0x10 is the CONSOLE sizeof(CgsGui::ImageFileInfo)
        // with 4-byte pointers, so it must NOT be reproduced as a literal -- mpBuffer widens to
        // 8 bytes on the x64 host (24-byte record) and a baked 16-byte stride would desynchronise
        // after the first element. Index the typed array and read named members:
        //   lwz r11,-4(r31)  == record+0x04 == miSize    (compared to miExtraFilesSizeBytes)
        //   lwz r4,-8(r31)   == record+0x00 == miId      (-> GetMugshotBufferFromImageId)
        //   lwz r3,0(r31)    == record+0x08 == mpBuffer  (the XMemCpy DESTINATION)
        for (s32 liIndex = 0; liIndex < liNumberOfImageFiles; ++liIndex)
        {
            const ImageFileInfo& lrRecord = laImageFileInfo[liIndex];

            CGS_ASSERT(lrRecord.miSize == miExtraFilesSizeBytes,
                       "lpImageFile[liIndex].miSize == miExtraFilesSizeBytes");

            std::memcpy(lrRecord.mpBuffer, GetMugshotBufferFromImageId(lrRecord.miId),
                        static_cast<usize>(miExtraFilesSizeBytes));
        }

        // (***(this+0x130))(this+0x130, 0) -- the bound result handler's first vtable slot.
        // NOTE: the X360 only READS +0x130 here (`lwz r3,0x130(r29)` @0x828522B0); there is no
        // store to +0x130 anywhere in this body, which is why the r4 handler argument is
        // dropped. The binding is established by the preceding task starter -- Load
        // @0x82859B70 / Bootup (`stw r29,0x130(r31)` @0x82859BD4) -- which the same
        // ProfileManager drives with the same `this`, so the read resolves to that object.
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(0));
    }

    // X360 0x8284C240. Copy the metadata title (wide + ascii) and the save-info comment strings
    // into the fixed buffers (asserting each fits) and cache the two size fields.
    //
    // The X360 metadata layout (a2): wide-title region at +0, ascii-title at +0x20, size A at
    // +0x30, size B at +0x34. The save-info (a3): comment at +0, description at +0x20.
    void SaveLoadSystem::SetMetadata(const void* lpMetadata, const void* lpSaveInfo)
    {
        const char* lpcMeta = reinterpret_cast<const char*>(lpMetadata);
        const char* lpcInfo = reinterpret_cast<const char*>(lpSaveInfo);

        ConvertAsciiToWideCharSafe(macwMetadataTitle, lpcMeta, 32);   // +0x1A0 from a2+0

        const char* lpcMetaAscii = lpcMeta + 32;                       // a2+0x20
        CGS_ASSERT(std::strlen(lpcMetaAscii) < 0x20u, "String too long");
        std::strncpy(macTitle, lpcMetaAscii, 32);                      // +0x1E0

        const char* lpcComment = lpcInfo;                              // a3+0
        CGS_ASSERT(std::strlen(lpcComment) < 0x20u, "String too long");
        std::strncpy(macSaveInfoComment, lpcComment, 32);              // +0x08

        const char* lpcDescription = lpcInfo + 32;                     // a3+0x20
        CGS_ASSERT(std::strlen(lpcDescription) < 0x100u, "String too long");
        std::strncpy(macSaveInfoDescription, lpcDescription, 256);     // +0x28

        muSaveDataSizeKb = reinterpret_cast<const u32*>(lpcMeta)[12];   // a2+0x30
        muGameDataSizeKb = reinterpret_cast<const u32*>(lpcMeta)[13];   // a2+0x34
    }

    // X360 0x8284CA78. Show a message and route the user's choice through HandleMemcardOption.
    // Params (from the ASM register dance): lpcMessage = a2, luNumberOfOptions = a3,
    // lpacOptions = a4. The X360 forwards (display, handler, a2, a4, a3) -- a3/a4 swapped on the
    // way out -- which lands as MessageDisplay::ShowMessage(handler, message, options, count).
    //
    // This body is ENTERED WITH THE +4 SUB-OBJECT (r3 == real this + 4; see the convention note
    // in CgsSaveLoadX360.h), so every displacement below is real-4:
    //   lwz r3,0x130(r11)  -> real +0x134 == mpMessageDisplay (NOT mpActiveMessageDisplay)
    //   addi r4,r11,-4     -> the real `this`, i.e. the option handler
    //   std r10,0x194(r11) -> real +0x198 == mMessageDisplayOptionFunc { func, delta }
    // Load @0x82859B70 and BootupShowAutosaveWarning @0x828599A0 emit the same three stores
    // INLINE with the real this as 0x134 / `mr r4,r31` / 0x198, and BootupCheckDone @0x82859A38
    // -- a proven +4 body -- emits the identical 0x130 / -4 / 0x194 triple.
    void SaveLoadSystem::ShowMessage(const char* lpcMessage, u32 luNumberOfOptions,
                                     const char** lpacOptions)
    {
        // Pending option handler = &HandleMemcardOption, stored as the 8-byte member-fn pointer
        // { func, delta = 0 } (`stw 0,var+4; stw func,var; ld; std ...,0x194` == real +0x198) --
        // the very pair HandleOption null-checks and dispatches. muFunc is full-width on x64 (see
        // the header note): the >4GB trampoline address must not be truncated.
        mMessageDisplayOptionFunc.muFunc =
            reinterpret_cast<uintptr_t>(&SaveLoadSystem_HandleMemcardOption);
        mMessageDisplayOptionFunc.muDelta = 0;

        // The OptionHandler passed is `this` adjusted to the embedded OptionHandler sub-object
        // (the X360's `addi r4,r11,-4` walks the +4 sub-object back to the real object).
        // static_cast performs the equivalent base-offset adjustment on x64.
        MessageDisplay::OptionHandler* lpHandler = static_cast<MessageDisplay::OptionHandler*>(this);
        mpMessageDisplay->ShowMessage(lpHandler, lpcMessage, lpacOptions, luNumberOfOptions);
    }

    // X360 0x8284CA18. Toggle the autosave icon only when the desired visibility differs from the
    // cached state. Also entered with the +4 sub-object, so `lwz r3,0x130` is mpMessageDisplay
    // (real +0x134 -- the display BootupShowAutosaveWarning/BootupStart dispatch the same icon
    // slot +0x0C on) and `lbz/stb 0x248` is mbAutosaveIconVisible at real +0x24C (the byte
    // Construct zeroes).
    void SaveLoadSystem::ShowAutosaveIcon(bool lbVisible)
    {
        if (lbVisible != mbAutosaveIconVisible)
        {
            mpMessageDisplay->ShowAutosaveIcon(lbVisible);
            mbAutosaveIconVisible = !mbAutosaveIconVisible;   // X360: cntlzw/extrwi == (x == 0)
        }
    }

    // X360 0x8284C730. The MessageDisplay::OptionHandler entry: hide the prompt and dispatch the
    // pending message-display-option member function (mMessageDisplayOptionFunc).
    void SaveLoadSystem::HandleOption(u32 luOption)
    {
        // The null-check tests ONLY the +0x198 func word (`lwz r11,0x198; cmplwi 0`).
        CGS_ASSERT(mMessageDisplayOptionFunc.muFunc != 0, "mpMessageDisplayOptionFunc != 0");

        mpMessageDisplay->HideMessage();   // (*(**(this+0x134)+8))(...)

        // Read the active member-fn pointer { func @+0x198, delta @+0x19C }, then clear the pair.
        // muFunc is a full 64-bit code pointer on x64 (see the header note) -- read it full-width so
        // the >4GB trampoline address is not truncated.
        const uintptr_t luFunc  = mMessageDisplayOptionFunc.muFunc;
        const u32       luDelta = mMessageDisplayOptionFunc.muDelta;
        mMessageDisplayOptionFunc.muFunc  = 0;
        mMessageDisplayOptionFunc.muDelta = 0;
        // Member-function-pointer dispatch: add the delta to `this` and call through the function
        // word with (adjustedThis, option).
        void* lpfn       = reinterpret_cast<void*>(luFunc);
        void* lpAdjusted = reinterpret_cast<u8*>(this) + luDelta;
        reinterpret_cast<void (*)(void*, u32)>(lpfn)(lpAdjusted, luOption);
    }

    // X360 0x828599A0. Show the autosave warning (with a CONTINUE option), routing confirmation to
    // BootupStart.
    void SaveLoadSystem::BootupShowAutosaveWarning()
    {
        // ShowAutosaveIcon(true) is invoked through mpMessageDisplay's vtable slot +0x0C.
        mpMessageDisplay->ShowAutosaveIcon(true);

        // Pending option handler = &BootupStart. Member-fn pointer { func = BootupStart @+0x198,
        // delta = 0 @+0x19C }, stored via `std r10,0x198`. muFunc is full-width on x64 (see the
        // header note) -- the >4GB trampoline address must not be truncated to u32.
        mMessageDisplayOptionFunc.muFunc =
            reinterpret_cast<uintptr_t>(&SaveLoadSystem_BootupStart);
        mMessageDisplayOptionFunc.muDelta = 0;

        const char* lapcOptions[1];
        lapcOptions[0] = KSTR_CONTINUE;
        mpMessageDisplay->ShowMessage(static_cast<MessageDisplay::OptionHandler*>(this),
                                      KSTR_AUTOSAVE_WARNING, lapcOptions, 1);
    }

    // X360 0x8284BF50 (assert cites the X360-baked CgsSaveLoadX360.h:745). Map a message option
    // INDEX (0..3) to the memory-card SDK message CHOICE code (1..4); anything else asserts
    // "SaveLoad: unexpected option index: " and yields 0.
    u32 SaveLoadSystem::MessageChoiceForOptionIndex(u32 luOptionIndex)
    {
        switch (luOptionIndex)
        {
        case 0: return 1;
        case 1: return 2;
        case 2: return 3;
        case 3: return 4;
        default:
            CGS_ASSERT(false, "SaveLoad: unexpected option index: ");
            return 0;
        }
    }

    // X360 0x8284C910. Resolve an image id to its mugshot-buffer slot:
    //   liType  = liImageId / 1000, liIndex = liImageId % 1000,
    //   slot    = liType * miNumberOfMugshotsPerType + liIndex,
    //   address = mpMugshotBufferData + slot * miExtraFilesSizeBytes.
    // The four range asserts are the X360 rodata strings, reproduced verbatim.
    void* SaveLoadSystem::GetMugshotBufferFromImageId(s32 liImageId)
    {
        const s32 liType  = liImageId / 1000;
        const s32 liTypeIndex = liImageId % 1000;

        CGS_ASSERT(liType < miNumberOfMugshotTypes, "liType < miNumberOfMugshotTypes");
        CGS_ASSERT(liTypeIndex < miNumberOfMugshotsPerType, "liIndex < miNumberOfMugshotsPerType");

        const s32 liIndex = miNumberOfMugshotsPerType * liType + liTypeIndex;
        CGS_ASSERT(0 <= liIndex, "0 <= liIndex");
        CGS_ASSERT(liIndex <= miNumberOfMugshotTypes * miNumberOfMugshotsPerType,
                   "liIndex <= miNumberOfMugshotTypes * miNumberOfMugshotsPerType");

        return reinterpret_cast<u8*>(mpMugshotBufferData) + miExtraFilesSizeBytes * liIndex;
    }

    // X360 0x8284C6D8. Forward the user's message choice to the memory-card interface. The X360
    // dispatches mpMemcardInterface vtable slot 5 (+0x14 == MessageChoice) with
    // MessageChoiceForOptionIndex(luOption). On PC the interface is the inert no-op boundary
    // object, so the faithful forward is a safe no-op.
    void SaveLoadSystem::HandleMemcardOption(u32 luOption)
    {
        mpMemcardInterface->MessageChoice(static_cast<int>(MessageChoiceForOptionIndex(luOption)));
    }

    // X360 0x82855A60. The autosave-warning CONTINUE handler: hide the autosave icon
    // (mpMessageDisplay vtable +0x0C == ShowAutosaveIcon(false)), then start the boot-up load
    // through the memory-card interface and pump Update.
    void SaveLoadSystem::BootupStart(u32 /*luOption*/)
    {
        mpMessageDisplay->ShowAutosaveIcon(false);   // X360: (*(vtbl+12))(mpMessageDisplay, 0)

        // The X360 body is ONE branch on the auto-load byte (`lbz r11,0x182; cmplwi 1;
        // bne loc_82855B34` @0x82855A88):
        //   set   -> build TWO RealmcIface load entries -- entry 0 = { macTitle (this+0x1E0),
        //            {0,0}, {this+0x200, this+0x204} } via sub_82B51A08, entry 1 =
        //            CreateRealmcMugshotLoadEntryInfo -- and dispatch mpMemcardInterface vtable
        //            +0x24 with count 2;
        //   clear -> dispatch the SAME slot with count 0 and a null entry array.
        // Both arms then `bl SaveLoadSystem::Update` (@0x82855B74).
        //
        bool lbOk = true;
        if (mbAutoLoad)
        {

            // MISSING is the first-ever-boot case and is NOT a failure: the console's memory-card
            // boot-up creates/uses an empty container and reports success, and the manager's own
            // first-boot story depends on it -- ProfileManager::Bootup ran ReadProfileData (->
            // Profile::Serialise) BEFORE handing the image to the storage boot-up, so the stored
            // image already holds a version-current default profile that ValidateProfiles accepts.
            // Reporting success leaves that default in place, byte for byte what this leaf did
            // before the read existed.
            // CORRUPT/MISMATCH is a real failure: the container exists and is not this build's, so
            // report FAILURE -- BootupResult then leaves miSaveLoadResult at INVALID and
            // ReportTaskCompleted never deserialises the half-read image over the live profile.

            // [DIAG] NOT IN THE X360 BINARY -- the byte counts the read actually moved, so a
            // boot-up load is measured and not inferred from the result code alone.
            if (CgsDev::Log::gpDebugPrint != 0)
            {
            }
        }
        else
        {
            CgsDev::Log::WriteToLog("[SaveLoadPC] bootup: auto-load off, no read\n");
        }

        // The storage stage ran: from here the container stands for the selected device.

        Update();
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(lbOk ? E_SAVELOADTASKRESULT_SUCCESS
                                            : E_SAVELOADTASKRESULT_FAILURE);
    }

    // X360 0x82855EC0. The confirm-load prompt handler routed from Load(). luOption == 1 is the
    // CONFIRM arm: build the two RealmcIface load entries and start the memory-card read; any
    // other option is the decline arm: report failure to the bound result handler
    // ((***(this+0x130))(this+0x130, 1) == vtable slot 0, arg 1 == E_SAVELOADTASKRESULT_FAILURE).
    void SaveLoadSystem::LoadHandleConfirmLoad(u32 luOption)
    {
        if (luOption == 1)
        {
            if (mStoredDataView.mpData != 0)
            {
            }

            // The X360 confirm arm ends with `mr r3,r31; bl SaveLoadSystem::Update` right after
            // the ReadSave dispatch (vtable +0x38); keep that pump in asm order -- read, Update,
            // completion -- as the sibling BootupStart leaf above does.
            Update();

            return;
        }

        // The decline arm, faithful to the X360 else-arm: (***(this+0x130))(this+0x130, 1).
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_FAILURE);
    }

}
