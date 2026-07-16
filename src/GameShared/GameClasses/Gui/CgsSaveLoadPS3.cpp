// CgsGui::SaveLoadSystem - the Xenon (X360) save/load front-end, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The ledger keys this TU as GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp;
// the class itself is the X360 save/load system (its assert sites cite gui/CgsSaveLoadX360.cpp and
// it is homed in CgsSaveLoadX360.h). The PS3 DecFIGS DWARF describes a different, PS3-only member
// set; the layout used here is the X360-authoritative offset map from the ASM (see the class in
// CgsSaveLoadX360.h).
//
// Twelve functions are in scope for this TU:
//   Construct, Prepare, Release, Update, Load, Save, LoadImageFiles,
//   SetMetadata, ShowMessage, ShowAutosaveIcon, HandleOption, BootupShowAutosaveWarning
//
// Cross-TU callees that are not yet reconstructed (RealmcIface SDK helpers, the localisation
// string table, a few Win32/Xenon shims) are reached through extern declarations whose
// signatures are taken from the call-site argument registers in the X360 ASM. They resolve to
// their own TUs at link time; this TU only needs their declarations to compile.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"     // CgsGui::ConvertAsciiToWideCharSafe, MessageDisplay
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

#include <cstddef>   // offsetof (layout pins)
#include <cstring>   // strncpy, memset

#include <Windows.h> // CreateFileA, GetFileSize, CloseHandle, XGetOverlappedResult shim

namespace CgsMemory
{
    // CgsMemory::LinearMalloc::Malloc -- declared here for the Prepare mugshot-buffer allocation
    // (real body in GameShared/GameClasses/Memory/CgsLinearMalloc.cpp).
    class LinearMalloc
    {
    public:
        void* Malloc(s32 liSize);
    };
}

// Xenon (X360) overlapped-result query. Not part of the host Win32 <Windows.h>; declared here as
// the X360 shim the Update pump calls (returns the overlapped extended error, e.g. ERROR_IO_PENDING).
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

    // ---- Unrecovered cross-TU SDK callees (signatures from the X360 call-site registers) --------
    // RealmcIface memory-card SDK helpers. Declared (not defined) here; their bodies are in the
    // RealmcIface TUs. Argument counts/order match the ASM.
    void  RealmcGameInfo_Construct(void* lpGameInfo, const wchar_t* lpwTitle);          // RealmcIface::GameInfo::GameInfo
    RealmcIface::MemcardInterface* RealmcMemcard_CreateInstance(const void* lpCallbacks,
                                                                const void* lpAllocator,
                                                                const void* lpGameInfo); // RealmcIface::MemcardInterface::CreateInstance
    void* RealmcSaveCheckParams_Construct(void* lpParams, int liA, int liB);            // RealmcIface::SaveCheckParams::SaveCheckParams
    void  RealmcSaveCheckParams_Destruct(void* lpParams);                               // RealmcIface::SaveCheckParams::~SaveCheckParams
    void  RealmcLoadEntryInfo_Construct(void* lpEntry);                                 // RealmcIface::LoadEntryInfo::LoadEntryInfo
    void  RealmcLoadEntryInfo_Assign(void* lpDest, const void* lpSrc);                  // RealmcIface::LoadEntryInfo::operator=
    void* RealmcTitleInfo_Empty();                                                      // RealmcIface::TitleInfo::Empty
    void  RealmcBuildEntryRecord(void* lpRecord, const void* lpName,
                                 const void* lpA, const void* lpB);                     // sub_82B51A08

    // The save/load locale-string callback the memory-card interface is given.
    void* gpfnLocaleGetStrCallback = nullptr;   // CgsGui::SaveLoadSystem::LocaleGetStrCallback

    // (wave B) The GetMugshotBufferFromImageId / CreateRealmcSaveInfo placeholders that used
    // to be declared here became real class members (CgsSaveLoadX360.h); the call sites below
    // now call the members directly.

    // Pending message-display option member functions (the X360 stores these as member-function
    // pointers in mActiveOptionFunc / mMessageDisplayOptionFunc and later dispatches them).
    // Bodied out-of-scope; referenced only by address here.
    void SaveLoadSystem_HandleMemcardOption(void* lpSystem, u32 luOption);             // HandleMemcardOption
    void SaveLoadSystem_BootupStart(void* lpSystem, u32 luOption);                     // BootupStart
    void SaveLoadSystem_LoadHandleConfirmLoad(void* lpSystem, u32 luOption);           // LoadHandleConfirmLoad

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
    // and save-file paths, a7/a8 the image dimensions and a28 the extra-file size (the last
    // stack argument loaded from arg_54).
    void SaveLoadSystem::Construct(MessageDisplay* lpMessageDisplay, LanguageManager* lpLanguageManager,
                                   const char* lpcTitle, const char* lpcContentInfoFilePath,
                                   const char* lpcSaveFilePath, s32 liImageWidth, s32 liImageHeight,
                                   u32 luExtraFilesSizeBytes)
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
        miImageWidth            = liImageWidth;              // +0x214 (a7)
        miImageHeight           = liImageHeight;             // +0x218 (a8)
        miExtraFilesSizeBytes   = static_cast<s32>(luExtraFilesSizeBytes); // +0x21C (a28)
        mbField180              = true;                      // +0x180 (stb r9=1,0x180)
        mbField181              = false;                     // +0x181 (stb r11=0,0x181)
        mbField183              = false;                     // +0x183 (stb r11=0,0x183)
        mbAsyncOpState          = 0;                         // +0x224 (stb r11=0,0x224)
        mField24C               = 0;                         // +0x24C (stb r11=0,0x24C)
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

        // Mugshot buffer = extraFilesSize * imageHeight * imageWidth.
        const s32 liMugshotBytes = miExtraFilesSizeBytes * miImageHeight * miImageWidth;
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

        // Pending option handler = &LoadHandleConfirmLoad. The X360 builds the member-fn pointer
        // { func = LoadHandleConfirmLoad @+0x198, delta = 0 @+0x19C } and stores it via
        // `std r11,0x198`.
        mMessageDisplayOptionFunc.muFunc =
            static_cast<u32>(reinterpret_cast<uintptr_t>(&SaveLoadSystem_LoadHandleConfirmLoad));
        mMessageDisplayOptionFunc.muDelta = 0;

        mpActiveMessageDisplay->ShowMessage(reinterpret_cast<MessageDisplay::OptionHandler*>(this),
                                            KSTR_CONFIRM_LOAD, lapcOptions, 2);
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
                               static_cast<u32>(miImageHeight) * static_cast<u32>(miImageWidth);
        u32 laMugSizes[2];
        laMugSizes[0] = reinterpret_cast<u32>(mpMugshotBufferData);
        laMugSizes[1] = luMugBytes;
        u8  laMugRecord[48];
        RealmcBuildEntryRecord(laMugRecord, "Mugshots", laMugA, laMugSizes);
        RealmcLoadEntryInfo_Assign(laEntries[1], laMugRecord);

        void* lpTitleInfo = RealmcTitleInfo_Empty();
        u8    lSaveInfo;
        void* lpSaveInfo = CreateRealmcSaveInfo(&lSaveInfo);
        mpMemcardInterface->WriteSave(lpSaveInfo, 2, laEntries[0], 0, lpTitleInfo);

        Update();
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
        MessageDisplay::OptionHandler* lpHandler =
            reinterpret_cast<MessageDisplay::OptionHandler*>(reinterpret_cast<u8*>(this) - 4);
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
        const u32 luFunc  = mMessageDisplayOptionFunc.muFunc;
        const u32 luDelta = mMessageDisplayOptionFunc.muDelta;
        mMessageDisplayOptionFunc.muFunc  = 0;
        mMessageDisplayOptionFunc.muDelta = 0;
        // Member-function-pointer dispatch: add the delta to `this` and call through the function
        // word with (adjustedThis, option).
        void* lpfn       = reinterpret_cast<void*>(static_cast<uintptr_t>(luFunc));
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
        // delta = 0 @+0x19C }, stored via `std r10,0x198`.
        mMessageDisplayOptionFunc.muFunc =
            static_cast<u32>(reinterpret_cast<uintptr_t>(&SaveLoadSystem_BootupStart));
        mMessageDisplayOptionFunc.muDelta = 0;

        const char* lapcOptions[1];
        lapcOptions[0] = KSTR_CONTINUE;
        mpMessageDisplay->ShowMessage(reinterpret_cast<MessageDisplay::OptionHandler*>(this),
                                      KSTR_AUTOSAVE_WARNING, lapcOptions, 1);
    }
}
