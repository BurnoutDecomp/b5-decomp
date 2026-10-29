// Native Xenon SaveLoad implementation. This historical filename is retained
// because the workflow's TU attribution keyed the class to the PS3 source file.
// ARTIST controls every path and the native layout in CgsSaveLoadX360.h.
#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "SDKs/Realmc/RealmcGameInfo.h"
#include "SDKs/Realmc/RealmcSaveInfo.h"
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"
#include "SDKs/Realmc/RealmcTitleInfo.h"
#include <cstring>

namespace
{
    EA::Allocator::ICoreAllocator* gpMemcardAllocator = 0; // 8305A6F8
}
namespace CgsGui
{
// 8284C050
void SaveLoadSystem::Construct(MessageDisplay* pDisplay, CgsLanguage::LanguageManager* pLanguage,
    const char* title, const char* contentPath, const char* savePath,
    s32 types, s32 perType, u32 extraBytes)
{
    CGS_ASSERT(pDisplay != 0, "lpMessageDisplay != NULL");
    CGS_ASSERT(pLanguage != 0, "lpLanguageManager != NULL");
    ConvertAsciiToWideCharSafe(macwTitle, title, 32);
    mpacContentInfoFilePath = contentPath;
    mpacSaveFilePath = savePath;
    mpLanguageManager = pLanguage;
    mpMessageDisplay = pDisplay;
    mpMemcardInterface = 0;
    mpSystemUserProfile = 0;
    mpResultHandler = 0;
    mMessageDisplayOptionFunc = 0;
    mpfnPendingUIClosed = 0;
    miField210 = -1;
    miNumberOfMugshotTypes = types;
    miNumberOfMugshotsPerType = perType;
    mbField180 = true;
    miExtraFilesSizeBytes = extraBytes;
    mbSilentMode = false;
    mbField183 = false;
    mbAsyncOpState = 0;
    mbAutosaveIconVisible = false;
}

// 82851FC0: the callback is IGameInterface +4, allocator is the object +18C.
bool SaveLoadSystem::Prepare(CgsMemory::HeapMalloc* pHeap,
    CgsMemory::LinearMalloc* pLinear, SystemUserProfile* pProfile)
{
    CGS_ASSERT(pHeap != 0, "lpHeapMalloc");
    CGS_ASSERT(pLinear != 0, "lpLinearMalloc");
    CGS_ASSERT(pProfile != 0, "lpSystemUserProfile");
    mMemcardAllocator.SetHeap(pHeap);
    RealmcIface::MemcardInterface::CreateParams params;
    params.mpAllocator = &mMemcardAllocator;
    params.mpfnGetStr = &SaveLoadSystem::LocaleGetStrCallback;
    params.mpThreadParams = 0;
    gpMemcardAllocator = &mMemcardAllocator;
    mpSystemUserProfile = pProfile;
    RealmcIface::GameInfo gameInfo(macwTitle);
    mpMemcardInterface = RealmcIface::MemcardInterface::CreateInstance(&params, this, gameInfo);
    mpMemcardInterface->SetActive(1);
    mpMugshotBufferData = pLinear->Malloc(miExtraFilesSizeBytes *
        miNumberOfMugshotsPerType * miNumberOfMugshotTypes);
    CGS_ASSERT(mpMugshotBufferData != 0, "mpMugshotBufferData");
    HANDLE file = CreateFileA(mpacSaveFilePath, GENERIC_READ, FILE_SHARE_READ,
                             0, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        miContentInfoFileSize = 0;
        mpContentInfoFileBuffer = 0;
        return true;
    }
    XenonFileInputStream stream(file, static_cast<s32>(GetFileSize(file, 0)));
    miContentInfoFileSize = stream.GetSize();
    mpContentInfoFileBuffer = gpMemcardAllocator->Alloc(miContentInfoFileSize, "", 0);
    stream.Read(mpContentInfoFileBuffer, miContentInfoFileSize);
    CloseHandle(file);
    return true;
}

// 8284C158
bool SaveLoadSystem::Release()
{
    gpMemcardAllocator->Free(mpContentInfoFileBuffer, miContentInfoFileSize);
    miContentInfoFileSize = 0;
    mpContentInfoFileBuffer = 0;
    return true;
}

// 8284C4E0
void SaveLoadSystem::Update()
{
    if (mbAsyncOpState == 1 && !mOverlapped.IsOperationInProgress())
        mbAsyncOpState = 0;
    if (mpMemcardInterface)
        mpMemcardInterface->Update(0);
}

// 8284C240: source 30/34 and destination 200/204 are one OpaqueBuffer.
void SaveLoadSystem::SetMetadata(const void* pMetadata, const void* pSaveInfo)
{
    const SaveLoadMetadata& metadata = *static_cast<const SaveLoadMetadata*>(pMetadata);
    const SaveInfo& info = *static_cast<const SaveInfo*>(pSaveInfo);
    ConvertAsciiToWideCharSafe(macwMetadataTitle, metadata.macTypename, 32);
    CGS_ASSERT(std::strlen(metadata.macFilename) < 32, "String too long");
    std::strncpy(macTitle, metadata.macFilename, 32);
    CGS_ASSERT(std::strlen(info.macTitle) < 32, "String too long");
    std::strncpy(macSaveInfoComment, info.macTitle, 32);
    CGS_ASSERT(std::strlen(info.macDescription) < 256, "String too long");
    std::strncpy(macSaveInfoDescription, info.macDescription, 256);
    mStoredData = metadata.mStoredData;
}

// 82859B70
void SaveLoadSystem::Load(SaveLoadTaskResultHandler* handler, const void* metadata)
{
    const char* options[2] = { "GENERAL_OPTION_YES", "GENERAL_OPTION_NO" };
    SaveInfo info;
    std::memset(&info, 0, sizeof(info));
    mpResultHandler = handler;
    SetMetadata(metadata, &info);
    mMessageDisplayOptionFunc = &SaveLoadSystem::LoadHandleConfirmLoad;
    mpMessageDisplay->ShowMessage(this, "SAVELOAD_CONFIRM_LOAD", options, 2);
}

// 828601D8
void SaveLoadSystem::Bootup(SaveLoadTaskResultHandler* handler, const void* metadata, bool autoLoad)
{
    SaveInfo info;
    std::memset(&info, 0, sizeof(info));
    mpResultHandler = handler;
    SetMetadata(metadata, &info);
    mbAutoLoad = autoLoad;
    if (miField210 >= 0)
    {
        WaitForUIClosed(&SaveLoadSystem::BootupShowAutosaveWarning);
        return;
    }
    mMessageDisplayOptionFunc = &SaveLoadSystem::BootupHandleNotSignedInOption;
    const char* options[2] = { "SAVELOAD_SIGN_IN", "SAVELOAD_CONTINUE_WITHOUT_SAVING" };
    mpMessageDisplay->ShowMessage(this, "SAVELOAD_NOT_SIGNED_IN", options, 2);
}

// 82856040
void SaveLoadSystem::Save()
{
    RealmcIface::LoadEntryInfo entries[2];
    {
        RealmcIface::SaveCheckParams params(0, 0);
        mpMemcardInterface->CheckSave(0, &params);
    }
    RealmcIface::DataBuffer empty, data;
    data.mpData = mStoredData.mpData;
    data.muSize = mStoredData.miSize;
    entries[0] = RealmcIface::LoadEntryInfo(macTitle, &empty, &data);
    entries[1] = CreateRealmcMugshotLoadEntryInfo();
    RealmcIface::TitleInfo& title = RealmcIface::TitleInfo::Empty();
    RealmcIface::SaveInfo info = CreateRealmcSaveInfo();
    mpMemcardInterface->WriteSave(&info, 2, entries, 0, &title);
    Update();
}

// 8285F268. The pre-boot path parks BootupStart/SignIn instead of writing.
void SaveLoadSystem::Save(SaveLoadTaskResultHandler* handler,
                         const SaveLoadMetadata& metadata, const SaveInfo& info)
{
    mpResultHandler = handler;
    SetMetadata(&metadata, &info);
    void (SaveLoadSystem::*continuation)() = static_cast<void (SaveLoadSystem::*)()>(&SaveLoadSystem::Save);
    if (!mbField183)
    {
        mbAutoLoad = false;
        // ARTIST uses the same 8-byte member pointer for the unused-option
        // BootupStart callback and for the zero-argument UI continuation.
        continuation = miField210 >= 0
            ? reinterpret_cast<void (SaveLoadSystem::*)()>(&SaveLoadSystem::BootupStart)
            : &SaveLoadSystem::SignIn;
    }
    WaitForUIClosed(continuation);
}

// 82855BC0. The native loop deliberately reuses the first image descriptor:
// there is no increment of r22 between its iterations at 82855D3C..82855D8C.
void SaveLoadSystem::Autosave(SaveLoadTaskResultHandler* handler,
    const SaveLoadMetadata& metadata, const SaveInfo& info, s32 count, const ImageFileInfo* images)
{
    CGS_ASSERT(mbField180, "SaveLoad: can't call Autosave while autosave is disabled");
    CGS_ASSERT(miField210 >= 0, "SaveLoad: user must be signed in before Autosave task can run");
    CGS_ASSERT(count <= 3, "liNumberOfImageFiles <= KI_MAX_IMAGE_FILES_PER_OPERATION");
    RealmcIface::LoadEntryInfo entries[2];
    mpResultHandler = handler;
    SetMetadata(&metadata, &info);
    for (s32 i = 0; i < count; ++i)
    {
        CGS_ASSERT(images->miSize == miExtraFilesSizeBytes,
                   "lpImageFile[liIndex].miSize == miExtraFilesSizeBytes");
        std::memcpy(GetMugshotBufferFromImageId(images->miId), images->mpBuffer,
                    static_cast<usize>(miExtraFilesSizeBytes));
    }
    RealmcIface::DataBuffer empty, data;
    data.mpData = mStoredData.mpData;
    data.muSize = mStoredData.miSize;
    entries[0] = RealmcIface::LoadEntryInfo(macTitle, &empty, &data);
    entries[1] = CreateRealmcMugshotLoadEntryInfo();
    RealmcIface::SaveInfo saveInfo = CreateRealmcSaveInfo();
    RealmcIface::TitleInfo& title = RealmcIface::TitleInfo::Empty();
    mpMemcardInterface->WriteSave(&saveInfo, 2, entries, 1, &title);
    Update();
    ShowAutosaveIcon(true);
}

// 8284CA78
void SaveLoadSystem::ShowMessage(const char* message, u32 count, const char** options)
{
    mMessageDisplayOptionFunc = &SaveLoadSystem::HandleMemcardOption;
    mpMessageDisplay->ShowMessage(this, message, options, count);
}
// 8284CA18: reload the flag after the display callback, then invert it.
void SaveLoadSystem::ShowAutosaveIcon(bool visible)
{
    if (visible != mbAutosaveIconVisible)
    {
        mpMessageDisplay->ShowAutosaveIcon(visible);
        mbAutosaveIconVisible = !mbAutosaveIconVisible;
    }
}
// 8284C730
void SaveLoadSystem::HandleOption(u32 option)
{
    CGS_ASSERT(mMessageDisplayOptionFunc != 0, "mpMessageDisplayOptionFunc != 0");
    mpMessageDisplay->HideMessage();
    void (SaveLoadSystem::*callback)(u32) = mMessageDisplayOptionFunc;
    mMessageDisplayOptionFunc = 0;
    (this->*callback)(option);
}
// 828599A0
void SaveLoadSystem::BootupShowAutosaveWarning()
{
    mpMessageDisplay->ShowAutosaveIcon(true);
    mMessageDisplayOptionFunc = &SaveLoadSystem::BootupStart;
    const char* options[1] = { "SAVELOAD_CONTINUE" };
    mpMessageDisplay->ShowMessage(this, "SAVELOAD_AUTOSAVE_WARNING", options, 1);
}
// 8284C6D8
void SaveLoadSystem::HandleMemcardOption(u32 option)
{
    mpMemcardInterface->MessageChoice(MessageChoiceForOptionIndex(option));
}
// 82855A60
void SaveLoadSystem::BootupStart(u32)
{
    mpMessageDisplay->ShowAutosaveIcon(false);
    if (mbAutoLoad)
    {
        RealmcIface::LoadEntryInfo entries[2];
        RealmcIface::DataBuffer empty, data;
        data.mpData = mStoredData.mpData;
        data.muSize = mStoredData.miSize;
        entries[0] = RealmcIface::LoadEntryInfo(macTitle, &empty, &data);
        entries[1] = CreateRealmcMugshotLoadEntryInfo();
        RealmcIface::TitleInfo& title = RealmcIface::TitleInfo::Empty();
        RealmcIface::SaveInfo info = CreateRealmcSaveInfo();
        mpMemcardInterface->Bootup(&info, 2, entries, &title);
    }
    else
    {
        RealmcIface::TitleInfo& title = RealmcIface::TitleInfo::Empty();
        RealmcIface::SaveInfo info = CreateRealmcSaveInfo();
        mpMemcardInterface->Bootup(&info, 0, 0, &title);
    }
    Update();
}
// 82855EC0
void SaveLoadSystem::LoadHandleConfirmLoad(u32 option)
{
    if (option != 1)
    {
        mpResultHandler->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_FAILURE);
        return;
    }
    RealmcIface::LoadEntryInfo entries[2];
    RealmcIface::DataBuffer empty, data;
    data.mpData = mStoredData.mpData;
    data.muSize = mStoredData.miSize;
    entries[0] = RealmcIface::LoadEntryInfo(macTitle, &empty, &data);
    entries[1] = CreateRealmcMugshotLoadEntryInfo();
    RealmcIface::EntryContentName name(macwMetadataTitle, macTitle,
        miExtraFilesSizeBytes * miNumberOfMugshotsPerType * miNumberOfMugshotTypes + mStoredData.miSize);
    RealmcIface::TitleInfo& title = RealmcIface::TitleInfo::Empty();
    mpMemcardInterface->ReadSave(&name, 2, entries, 0, &title);
    Update();
}
// 82859C30
void SaveLoadSystem::SaveDone(u32 result, u32, const char*)
{
    ShowAutosaveIcon(false);
    if (result == 0)
    {
        mbField180 = true;
        EnableAutosave(-1);
        mpResultHandler->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_SUCCESS);
    }
    else if (result < 3)
    {
        mMessageDisplayOptionFunc = &SaveLoadSystem::SaveHandleRetry;
        const char* options[2] = { "SAVELOAD_RETRY", "SAVELOAD_CONTINUE_WITHOUT_SAVING" };
        mpMessageDisplay->ShowMessage(this, "SAVELOAD_RETRY_BOOT", options, 2);
    }
    else
        CGS_ASSERT(false, "Should not get here");
}
// Folded bodies in table820CF148, not synthesized defaults.
void SaveLoadSystem::SaveCheckDone(u32, u32, u32, u32, const int*, s32) {} // 8284CB38
bool SaveLoadSystem::CheckLoadedData(u32, u32) { return false; }        // 827E2F38
void SaveLoadSystem::Callback3C(u32, u32, const void*) {}               // 8284CB38
void SaveLoadSystem::SetActiveCardDone(u32, u32) {}                    // 8284CB38
void SaveLoadSystem::ClearEntries()                                  // 8284CB40
{
    CGS_ASSERT(false, "ClearEntries called");
}
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
        mpResultHandler
            ->HandleSaveLoadTaskResult(static_cast<ESaveLoadTaskResult>(0));
    }

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
}
