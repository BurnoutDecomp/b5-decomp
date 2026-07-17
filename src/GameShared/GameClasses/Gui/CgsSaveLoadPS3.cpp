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
// The RealmcIface memory-card SDK callees (record ctors + the MemcardInterface
// implementation) are the console storage backend; they are bodied file-locally as FLAG'd
// PC-platform boundary leaves (zeroed records / an inert no-op interface) so the console-
// shaped write path stays safe -- see the anonymous namespace below. The REAL PC storage
// is the CgsSaveLoadPC profile container (GameShared/GameClasses/Gui/PC/CgsSaveLoadPC.cpp):
// the 3-arg Save/Autosave write it and LoadHandleConfirmLoad's confirm arm reads it back,
// so the profile image genuinely persists across runs.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"       // CgsGui::ConvertAsciiToWideCharSafe, MessageDisplay
#include "GameShared/GameClasses/Gui/PC/CgsSaveLoadPC.h"   // the PC profile-container storage backend
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log (storage-edge diagnostics)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h" // CgsMemory::LinearMalloc (Prepare mugshot buffer)

#include <cstddef>   // offsetof (layout pins)
#include <cstring>   // strncpy, memset

#include <Windows.h> // CreateFileA, GetFileSize, CloseHandle, XGetOverlappedResult shim

// Xenon (X360) overlapped-result query. Not part of the host Win32 <Windows.h>; declared here as
// the X360 shim the Update pump calls (returns the overlapped extended error, e.g. ERROR_IO_PENDING).
// PC boundary definition lives in GameSource/BrnBaselineLinkStubs.cpp.
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

    ContentInfoProvider* gpContentInfoProvider = nullptr;   // X360 off_8305A6F8

    // ---- RealmcIface memory-card SDK boundary (PC) ----------------------------------------------
    // The Realmc ("real memcard") interface layer is the console storage backend: its X360
    // bodies (RealmcIface::GameInfo/SaveCheckParams/LoadEntryInfo/TitleInfo/EntryContentName
    // ctors @0x82B519E8..0x82B51C10 + the MemcardInterface implementation) are an unrecovered
    // SDK with no PC storage backend behind it. Each helper below keeps the X360 call-site
    // signature (argument counts/order from the ASM) but fails/no-ops SAFELY: records are
    // zeroed, the created interface is an inert no-op object, and no I/O ever starts -- so the
    // save/load task machine runs its no-save path without crashing.

    // FLAG PC-platform leaf: inert stand-in for the RealmcIface::MemcardInterface
    // implementation (unrecovered SDK; no PC storage backend). Every operation no-ops.
    class NoOpMemcardInterface : public RealmcIface::MemcardInterface
    {
    public:
        virtual void Reserved00() {}
        virtual void Reserved01() {}
        virtual int  Update(int /*liArg*/) { return 0; }
        virtual void Reserved03() {}
        virtual void Reserved04() {}
        virtual void Reserved05() {}
        // FLAG PC-platform leaf: the host has no console memory-card UI to silence.
        virtual void SetSilentMode(bool /*lbSilentMode*/, s32 /*liUserIndex*/) {}
        virtual void Reserved07() {}
        virtual void Reserved08() {}
        virtual void Reserved09() {}
        virtual void Reserved10() {}
        virtual void WriteSave(void* /*lpSaveInfo*/, int /*liCount*/, void* /*lpEntries*/,
                               int /*liFlags*/, void* /*lpTitleInfo*/) {}
        virtual void CheckSave(int /*liArg*/, void* /*lpCheckParams*/) {}
        virtual void SetActive(int /*liActive*/) {}
    };

    NoOpMemcardInterface gNoOpMemcardInterface;

    // FLAG PC-platform leaf: RealmcIface::GameInfo::GameInfo (X360 stack record, 144 bytes at
    // the Prepare call site) -- zero the record; nothing consumes it on PC.
    void RealmcGameInfo_Construct(void* lpGameInfo, const wchar_t* /*lpwTitle*/)
    {
        std::memset(lpGameInfo, 0, 144);
    }

    // FLAG PC-platform leaf: RealmcIface::MemcardInterface::CreateInstance -- return the inert
    // no-op interface. Deliberately NON-NULL: Prepare immediately calls SetActive(1) on the
    // result (no null-check in the X360 body), so a null return would crash where the no-op
    // object keeps every downstream memcard call a safe no-op.
    RealmcIface::MemcardInterface* RealmcMemcard_CreateInstance(const void* /*lpCallbacks*/,
                                                                const void* /*lpAllocator*/,
                                                                const void* /*lpGameInfo*/)
    {
        return &gNoOpMemcardInterface;
    }

    // FLAG PC-platform leaf: RealmcIface::SaveCheckParams ctor (16-byte stack record) -- zero it;
    // it is only ever handed to the no-op CheckSave.
    void* RealmcSaveCheckParams_Construct(void* lpParams, int /*liA*/, int /*liB*/)
    {
        std::memset(lpParams, 0, 16);
        return lpParams;
    }

    // FLAG PC-platform leaf: RealmcIface::SaveCheckParams dtor -- nothing to release on PC.
    void RealmcSaveCheckParams_Destruct(void* /*lpParams*/)
    {
    }

    // FLAG PC-platform leaf: RealmcIface::LoadEntryInfo::LoadEntryInfo (48-byte record) -- zero it.
    void RealmcLoadEntryInfo_Construct(void* lpEntry)
    {
        std::memset(lpEntry, 0, 48);
    }

    // FLAG PC-platform leaf: RealmcIface::LoadEntryInfo::operator= -- byte-copy the 48-byte
    // record (the real X360 operator= @0x82B51A78 is a memberwise record copy).
    void RealmcLoadEntryInfo_Assign(void* lpDest, const void* lpSrc)
    {
        std::memcpy(lpDest, lpSrc, 48);
    }

    // FLAG PC-platform leaf: RealmcIface::TitleInfo::Empty (X360 @0x82B51B88 returns the SDK's
    // static empty record) -- return a static zeroed record; only the no-op WriteSave sees it.
    void* RealmcTitleInfo_Empty()
    {
        static u8 saEmptyTitleInfo[64] = { 0 };
        return saEmptyTitleInfo;
    }

    // FLAG PC-platform leaf: the 48-byte entry-record builder (X360 sub_82B51A08 packs
    // name/id/size words into a RealmcIface entry record) -- zero the record; it is only ever
    // assigned into entries consumed by the no-op WriteSave/ReadSave.
    void RealmcBuildEntryRecord(void* lpRecord, const void* /*lpName*/,
                                const void* /*lpA*/, const void* /*lpB*/)
    {
        std::memset(lpRecord, 0, 48);
    }

    // The save/load locale-string callback the memory-card interface is given.
    void* gpfnLocaleGetStrCallback = nullptr;   // CgsGui::SaveLoadSystem::LocaleGetStrCallback

    // FLAG PC-platform leaf: SaveLoadSystem::CreateRealmcSaveInfo (X360 @0x8284C818 builds a
    // RealmcIface::SaveInfo via the unrecovered EntryContentName/SaveInfo SDK ctors) -- return
    // the caller's out slot untouched (Save passes a 1-byte slot; only the no-op WriteSave
    // consumes the result, so nothing may be written through it).
    void* CreateRealmcSaveInfo(void* lpOut, void* /*lpSystem*/)
    {
        return lpOut;
    }

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

    // Localisation string-table entries (X360 rodata pointers).
    const char* const KSTR_AUTOSAVE_WARNING = "SAVELOAD_AUTOSAVE_WARNING"; // off_82F3319C
    const char* const KSTR_CONTINUE         = "SAVELOAD_CONTINUE";         // off_82F331BC
    const char* const KSTR_CONFIRM_LOAD     = "SAVELOAD_CONFIRM_LOAD";     // off_82F331A4
    const char* const KSTR_OPTION_YES       = "GENERAL_OPTION_YES";        // off_82F331C8
    const char* const KSTR_OPTION_NO        = "GENERAL_OPTION_NO";         // off_82F331CC
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
        CGS_ASSERT(lpMessageDisplay != nullptr, "lpMessageDisplay != NULL");
        CGS_ASSERT(lpLanguageManager != nullptr, "lpLanguageManager != NULL");

        ConvertAsciiToWideCharSafe(macwTitle, lpcTitle, 32);

        mpacContentInfoFilePath = lpcContentInfoFilePath;   // +0x178
        mpacSaveFilePath        = lpcSaveFilePath;           // +0x17C
        mpLanguageManager       = lpLanguageManager;         // +0x188
        mpMessageDisplay        = lpMessageDisplay;          // +0x134
        mpMemcardInterface      = nullptr;                   // +0x184 (stw r11,0x184)
        mpActiveMessageDisplay  = nullptr;                   // +0x130 (stw r11,0x130)
        muField194              = 0;                         // +0x194 (stw r11,0x194)
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
        mField24C               = 0;                         // +0x24C (stb r11=0,0x24C)

        // Host-width stored-data view (see the header note): empty until a task starter
        // captures it.
        mStoredDataView.mpData = nullptr;
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
            mpMemcardInterface->SetSilentMode(false, -1);
        if (!mbSilentMode && lbSilentMode)
            mpMemcardInterface->SetSilentMode(true, -1);
        mbSilentMode = lbSilentMode;
    }

    // X360 0x82851FC0. Create the memory-card interface, allocate the mugshot buffer and read the
    // content-information file into a buffer.
    bool SaveLoadSystem::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, CgsMemory::LinearMalloc* lpLinearMalloc,
                                 SystemUserProfile* lpSystemUserProfile)
    {
        CGS_ASSERT(lpHeapMalloc != nullptr, "lpHeapMalloc");
        CGS_ASSERT(lpLinearMalloc != nullptr, "lpLinearMalloc");
        CGS_ASSERT(lpSystemUserProfile != nullptr, "lpSystemUserProfile");

        mpHeapMalloc = lpHeapMalloc;                         // +0x190

        // Build the memory-card callback block { &mContentInfoVptr, LocaleGetStrCallback, 0 } and
        // publish the provider to the file-scope pointer.
        void* lpCallbacks[3];
        lpCallbacks[0] = &mpContentInfoVptr;                 // this+0x18C
        lpCallbacks[1] = gpfnLocaleGetStrCallback;
        lpCallbacks[2] = nullptr;
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

        // Mugshot buffer = per-mugshot size * mugshots-per-type * type count.
        const s32 liMugshotBytes = miExtraFilesSizeBytes * miNumberOfMugshotsPerType * miNumberOfMugshotTypes;
        mpMugshotBufferData = lpLinearMalloc->Malloc(liMugshotBytes);   // +0x220
        CGS_ASSERT(mpMugshotBufferData != nullptr, "mpMugshotBufferData");

        HANDLE lhFile = ::CreateFileA(mpacSaveFilePath, 0x80000000u, 1, nullptr, 3, 0, nullptr);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            miContentInfoFileSize   = 0;                     // +0x208
            mpContentInfoFileBuffer = nullptr;               // +0x20C
            return true;
        }

        // Wrap the handle in a XenonFileInputStream { mhFile = lhFile, miSize = GetFileSize }.
        struct { HANDLE mhFile; s32 miSize; } lStream;
        lStream.mhFile = lhFile;
        lStream.miSize = static_cast<s32>(::GetFileSize(lhFile, nullptr));

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
        gpContentInfoProvider->mpVtbl->mpfnFree(gpContentInfoProvider,
                                                mpContentInfoFileBuffer, miContentInfoFileSize);
        miContentInfoFileSize   = 0;     // +0x208
        mpContentInfoFileBuffer = nullptr; // +0x20C
        return true;
    }

    // X360 0x8284C4E0. Pump the overlapped async op, then forward Update to the memory-card
    // interface if present.
    void SaveLoadSystem::Update()
    {
        if (mbAsyncOpState == 1)
        {
            const DWORD luResult = ::XGetOverlappedResult(maOverlapped, nullptr, FALSE);
            // 997 (ERROR_IO_PENDING) / 996 (ERROR_IO_INCOMPLETE) mean the op is still running.
            const bool lbStillRunning = (luResult == 997u) || (luResult == 996u);
            if (!lbStillRunning)
            {
                mbAsyncOpState = 0;
            }
        }

        if (mpMemcardInterface != nullptr)
        {
            mpMemcardInterface->Update(0);
        }
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

        mpActiveMessageDisplay->ShowMessage(static_cast<MessageDisplay::OptionHandler*>(this),
                                            KSTR_CONFIRM_LOAD, lapcOptions, 2);
    }

    // X360 0x828601D8 (entry NOT in the ARTIST export set). Begin the profile boot-up. The body is
    // composed from the exported sibling Load @0x82859B70 (identical bind-handler + SetMetadata
    // setup) and the confirmed callee BootupShowAutosaveWarning @0x828599A0 (SetMetadata's xref set
    // @0x8284C240 lists Bootup as a caller, so the SetMetadata call is attested). Bind the active
    // result handler, push the boot-up metadata, then show the SAVELOAD_AUTOSAVE_WARNING prompt --
    // BootupShowAutosaveWarning stores &BootupStart as the pending CONTINUE handler and renders the
    // one-option prompt through the message display. The boot-up therefore BLOCKS on the prompt
    // (the console behaviour) until the user answers CONTINUE; the prior PC leaf finished the task
    // synchronously here, which is why the prompt never appeared.
    void SaveLoadSystem::Bootup(SaveLoadTaskResultHandler* lpResultHandler, const void* lpMetadata,
                                bool /*lbAutoLoad*/)
    {
        u8 laSaveInfo[336];
        std::memset(laSaveInfo, 0, 288);

        mpActiveMessageDisplay = reinterpret_cast<MessageDisplay*>(lpResultHandler);   // +0x130

        SetMetadata(lpMetadata, laSaveInfo);

        BootupShowAutosaveWarning();
    }

    // X360 0x82856040. Write the save: build two load-entry records (the title save + the mugshot
    // blob), then hand them to the memory-card interface and pump Update.
    void SaveLoadSystem::Save()
    {
        u8 laEntries[2][48];
        for (int liIndex = 1; liIndex >= 0; --liIndex)
        {
            RealmcLoadEntryInfo_Construct(laEntries[liIndex]);
        }

        // Pre-flight check: CheckSave(0, SaveCheckParams).
        u8 laCheckParams[16];
        void* lpCheckParams = RealmcSaveCheckParams_Construct(laCheckParams, 0, 0);
        mpMemcardInterface->CheckSave(0, lpCheckParams);
        RealmcSaveCheckParams_Destruct(laCheckParams);

        // Entry 0: the title save (name = macTitle, sizes = the cached metadata sizes).
        u32 laTitleA[2] = { 0, 0 };
        u32 laTitleSizes[2] = { muSaveDataSizeKb, muGameDataSizeKb };
        u8  laTitleRecord[48];
        RealmcBuildEntryRecord(laTitleRecord, macTitle, laTitleA, laTitleSizes);
        RealmcLoadEntryInfo_Assign(laEntries[0], laTitleRecord);

        // Entry 1: the mugshot blob ("Mugshots", buffer + total byte size).
        u32 laMugA[2] = { 0, 0 };
        const u32 luMugBytes = static_cast<u32>(miExtraFilesSizeBytes) *
                               static_cast<u32>(miNumberOfMugshotsPerType) *
                               static_cast<u32>(miNumberOfMugshotTypes);
        u32 laMugSizes[2];
        laMugSizes[0] = reinterpret_cast<u32>(mpMugshotBufferData);
        laMugSizes[1] = luMugBytes;
        u8  laMugRecord[48];
        RealmcBuildEntryRecord(laMugRecord, "Mugshots", laMugA, laMugSizes);
        RealmcLoadEntryInfo_Assign(laEntries[1], laMugRecord);

        void* lpTitleInfo = RealmcTitleInfo_Empty();
        u8    lSaveInfo;
        void* lpSaveInfo = CreateRealmcSaveInfo(&lSaveInfo, this);
        mpMemcardInterface->WriteSave(lpSaveInfo, 2, laEntries[0], 0, lpTitleInfo);

        Update();
    }

    // The manager-facing 3-arg save task (BrnGui::ProfileManager::Save @0x82513B28 drives
    // it; the entry itself is not in the ARTIST export set -- composed from the exported
    // sibling Load @0x82859B70's bind-handler + SetMetadata setup, the same composition
    // Bootup uses). Bind the result handler, push the metadata/save-info, then write.
    void SaveLoadSystem::Save(SaveLoadTaskResultHandler* lpResultHandler,
                              const SaveLoadMetadata& lrMetadata, const SaveInfo& lrSaveInfo)
    {
        mpActiveMessageDisplay = reinterpret_cast<MessageDisplay*>(lpResultHandler);   // +0x130

        SetMetadata(&lrMetadata, &lrSaveInfo);
        CaptureStoredDataView(lrMetadata);

        // FLAG PC-platform leaf: the X360 continues into the memory-card write (the
        // CheckSave pre-flight + WriteSave entries the 0-arg Save builds), whose ASYNC
        // completion later reports the result to the bound handler. On PC the storage
        // edge is the CgsSaveLoadPC container: write it synchronously (the profile image
        // named by the metadata -- macTitle carries the metadata's save filename after
        // SetMetadata -- plus the mugshot blob, the console's second save entry) and
        // report the real I/O outcome here.
        const bool lbOk = SaveLoadPC::WriteContainer(
            macTitle, mStoredDataView.mpData, mStoredDataView.miSize,
            mpMugshotBufferData, static_cast<u32>(GetMugshotBufferSizeBytes()),
            macSaveInfoComment, macSaveInfoDescription);

        CgsDev::Log::WriteToLog(lbOk ? "[SaveLoadPC] profile container WRITTEN\n"
                                     : "[SaveLoadPC] profile container write FAILED\n");

        Update();
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(lbOk ? E_SAVELOADTASKRESULT_SUCCESS
                                            : E_SAVELOADTASKRESULT_FAILURE);
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

    // True when a saved profile container exists for lrMetadata's save name. Purely a
    // query -- no task state, no I/O task started.
    bool SaveLoadSystem::SaveFileExists(const SaveLoadMetadata& lrMetadata) const
    {
        return SaveLoadPC::ContainerExists(lrMetadata.macFilename);
    }

    // X360 0x828521E0. Copy each requested image's mugshot buffer into the caller's image-file
    // records (each record: { imageId, ?, miSize, dest }), then report the load result.
    void SaveLoadSystem::LoadImageFiles(SaveLoadTaskResultHandler* lpResultHandler,
                                        s32 liNumberOfImageFiles, const void* lpImageFile)
    {
        CGS_ASSERT(liNumberOfImageFiles > 0, "liNumberOfImageFiles > 0");
        CGS_ASSERT(lpImageFile != nullptr, "lpImageFile != NULL");

        // Each ImageFileInfo record is 16 bytes: [0]=imageId, [1]=?, [2]=miSize, [3]=destBuffer.
        const u32* lpRecord = reinterpret_cast<const u32*>(lpImageFile);
        for (s32 liIndex = 0; liIndex < liNumberOfImageFiles; ++liIndex)
        {
            const s32   liImageId    = static_cast<s32>(lpRecord[0]);
            const s32   liRecordSize = static_cast<s32>(lpRecord[2]);
            void* const lpDest       = reinterpret_cast<void*>(static_cast<uintptr_t>(lpRecord[3]));

            CGS_ASSERT(liRecordSize == miExtraFilesSizeBytes,
                       "lpImageFile[liIndex].miSize == miExtraFilesSizeBytes");

            void* lpSource = GetMugshotBufferFromImageId(liImageId);
            std::memcpy(lpDest, lpSource, static_cast<usize>(miExtraFilesSizeBytes));

            lpRecord += 4;
        }

        mpActiveMessageDisplay = reinterpret_cast<MessageDisplay*>(lpResultHandler);
        // (***(this+0x130))(this+0x130, 0) -- the result handler's first vtable slot.
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
    // lpacOptions = a4. The X360 forwards (handler, this-4, a2, a4, a3) -- a3/a4 swapped on the
    // way out -- which lands as MessageDisplay::ShowMessage(handler, message, options, count).
    void SaveLoadSystem::ShowMessage(const char* lpcMessage, u32 luNumberOfOptions,
                                     const char** lpacOptions)
    {
        // Pending option handler = &HandleMemcardOption. The X360 ShowMessage `std r10,0x194`
        // writes the func into the +0x194 word and zeroes the next word (+0x198) -- which aliases
        // mMessageDisplayOptionFunc.muFunc. Reproduce that aliasing store exactly (no guard).
        muField194 =
            static_cast<u32>(reinterpret_cast<uintptr_t>(&SaveLoadSystem_HandleMemcardOption));
        mMessageDisplayOptionFunc.muFunc = 0;   // +0x198, zeroed by the same 8-byte store

        // The OptionHandler passed is `this - 4` (the engine offsets to the option-handler
        // sub-object embedded just before the active-display pointer); the active display's
        // ShowMessage is invoked as ShowMessage(handler, message, options, count) -- so the
        // a3/a4 swap means options=lpacOptions and count=luNumberOfOptions.
        // The OptionHandler passed is `this` adjusted to the embedded OptionHandler sub-object.
        // static_cast performs the correct base-offset adjustment on x64 (the X360's fixed `-4`
        // pointer fixup does not survive the wider vptr / different base offsets).
        MessageDisplay::OptionHandler* lpHandler = static_cast<MessageDisplay::OptionHandler*>(this);
        mpActiveMessageDisplay->ShowMessage(lpHandler, lpcMessage, lpacOptions, luNumberOfOptions);
    }

    // X360 0x8284CA18. Toggle the autosave icon only when the desired visibility differs from the
    // cached state.
    void SaveLoadSystem::ShowAutosaveIcon(bool lbVisible)
    {
        if (lbVisible != mbAutosaveIconVisible)
        {
            mpActiveMessageDisplay->ShowAutosaveIcon(lbVisible);
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
    // dispatches mpMemcardInterface vtable slot 5 (+0x14) with
    // MessageChoiceForOptionIndex(luOption).
    void SaveLoadSystem::HandleMemcardOption(u32 luOption)
    {
        const u32 luChoice = MessageChoiceForOptionIndex(luOption);
        (void)luChoice;
        // FLAG PC-platform leaf: the memcard forward is vtable slot 5 of the unrecovered
        // RealmcIface::MemcardInterface (declared Reserved05 -- its SDK semantics are not
        // recovered), and on PC the interface is the inert no-op boundary object anyway; the
        // choice is computed (faithful) and the SDK forward is elided.
    }

    // X360 0x82855A60. The autosave-warning CONTINUE handler: hide the autosave icon
    // (mpMessageDisplay vtable +0x0C == ShowAutosaveIcon(false)), then start the boot-up load
    // through the memory-card interface and pump Update.
    void SaveLoadSystem::BootupStart(u32 /*luOption*/)
    {
        mpMessageDisplay->ShowAutosaveIcon(false);   // X360: (*(vtbl+12))(mpMessageDisplay, 0)

        // FLAG PC-platform leaf: the X360 continues by building RealmcIface load-entry records
        // (sub_82B51A08 + CreateRealmcMugshotLoadEntryInfo + CreateRealmcSaveInfo) and starting the
        // memory-card boot-up READ (mpMemcardInterface vtable +0x24), then pumps Update(); the
        // read's ASYNC completion later reports the result to the bound handler (+0x130). The
        // Realmc SDK is the unrecovered console storage backend and the PC interface is the inert
        // no-op boundary, so no read starts or completes. Report the boot-up result to the bound
        // handler HERE -- the PC substitute for the async read's completion. SUCCESS is the "no
        // save present" outcome the prior PC boot-up leaf reported: ProfileManager::BootupResult
        // enables autosave and the fresh profile is reset by the version-mismatch path. This runs
        // only after the user answered CONTINUE (HandleOption -> BootupStart), so it is NOT a
        // shortcut past the autosave-warning prompt (which the prior leaf skipped entirely).
        Update();
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_SUCCESS);
    }

    // X360 0x82855EC0. The confirm-load prompt handler routed from Load(). luOption == 1 is the
    // CONFIRM arm: build the two RealmcIface load entries and start the memory-card read; any
    // other option is the decline arm: report failure to the bound result handler
    // ((***(this+0x130))(this+0x130, 1) == vtable slot 0, arg 1 == E_SAVELOADTASKRESULT_FAILURE).
    void SaveLoadSystem::LoadHandleConfirmLoad(u32 luOption)
    {
        if (luOption == 1)
        {
            // FLAG PC-platform leaf: the X360 confirm arm builds the RealmcIface load-entry
            // records (sub_82B51A08 + CreateRealmcMugshotLoadEntryInfo + EntryContentName) and
            // starts the memory-card read (mpMemcardInterface vtable +0x38), whose ASYNC
            // completion later reports the result. On PC the storage edge is the
            // CgsSaveLoadPC container: read it synchronously into the stored-data view the
            // Load starter captured (plus the mugshot blob into the mugshot buffer, the
            // console's second load entry) and report the real outcome here. A missing,
            // corrupt, or other-build container reports FAILURE -- the manager's no-save
            // path, exactly what the console's failed read reported.
            bool lbOk = false;
            if (mStoredDataView.mpData != nullptr)
            {
                const SaveLoadPC::EContainerReadResult leResult = SaveLoadPC::ReadContainer(
                    macTitle, mStoredDataView.mpData, mStoredDataView.miSize,
                    mpMugshotBufferData, static_cast<u32>(GetMugshotBufferSizeBytes()));
                lbOk = leResult == SaveLoadPC::E_CONTAINERREAD_OK;
                CgsDev::Log::WriteToLog(
                    leResult == SaveLoadPC::E_CONTAINERREAD_OK      ? "[SaveLoadPC] profile container READ\n"
                    : leResult == SaveLoadPC::E_CONTAINERREAD_MISSING ? "[SaveLoadPC] profile container read: MISSING\n"
                    : leResult == SaveLoadPC::E_CONTAINERREAD_CORRUPT ? "[SaveLoadPC] profile container read: CORRUPT\n"
                                                                      : "[SaveLoadPC] profile container read: layout MISMATCH\n");
            }
            reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
                ->HandleSaveLoadTaskResult(lbOk ? E_SAVELOADTASKRESULT_SUCCESS
                                                : E_SAVELOADTASKRESULT_FAILURE);
            return;
        }

        // The decline arm, faithful to the X360 else-arm: (***(this+0x130))(this+0x130, 1).
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_FAILURE);
    }

    // ---- ContentInformationFileInterface overrides (PC boundary) ------------------------------
    // No named X360 ARTIST body exists for any of the four (the CIF vocabulary is the PS3 save
    // container's; grep of the export set finds no IsSavingCif/GetCifFile*/LoadCifFile), and the
    // PC build has no memcard layer to query them. Each reports the canonical "no CIF data"
    // answer so any caller takes its missing-file branch.

    // FLAG PC-platform leaf: no CIF save is ever in flight without a storage backend.
    bool SaveLoadSystem::IsSavingCif(ESaveLoadCif /*leCif*/)
    {
        return false;
    }

    // FLAG PC-platform leaf: no CIF file type without a storage backend.
    int SaveLoadSystem::GetCifFileType(ESaveLoadCif /*leCif*/)
    {
        return 0;
    }

    // FLAG PC-platform leaf: no CIF file name without a storage backend (null == "no file").
    const char* SaveLoadSystem::GetCifFileName(ESaveLoadCif /*leCif*/)
    {
        return 0;
    }

    // FLAG PC-platform leaf: CIF load always takes the missing-file branch (no data, size 0).
    bool SaveLoadSystem::LoadCifFile(ESaveLoadCif /*leCif*/, void** lppData, s32* lpiSize)
    {
        if (lppData != 0)
        {
            *lppData = 0;
        }
        if (lpiSize != 0)
        {
            *lpiSize = 0;
        }
        return false;
    }
}
