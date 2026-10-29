#pragma once
#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"
#include "GameShared/GameClasses/Gui/CgsImageFileInfo.h"
#include "GameShared/GameClasses/System/CgsXOverlapped.h"
#include "SDKs/Realmc/RealmcMemcardInterface.h"
#include "SDKs/Realmc/RealmcIGameInterface.h"
#include <coreallocator/icoreallocator_interface.h>
#include <xtl.h>

namespace CgsMemory { class HeapMalloc; class LinearMalloc; }
namespace CgsLanguage { class LanguageManager; }
namespace RealmcIface { class SaveCheckParams; class LoadEntryInfo; class DataBuffer; class SaveInfo; }
namespace CgsGui
{
class SystemUserProfile;
class XenonFileInputStream
{
public:
    XenonFileInputStream(HANDLE hFile, s32 iSize) : mhFile(hFile), miSize(iSize) {}
    s32 GetSize() const;                         // 8284BD88
    s32 Read(void* lpBuffer, u32 luBytesToRead); // 8284BE28
private:
    HANDLE mhFile;
    s32 miSize;
};

// 820CE2E0: deleting destructor, aligned Alloc, plain Alloc, sized Free.
class MemcardAllocator : public EA::Allocator::ICoreAllocator
{
public:
    static const s32 KI_DEFAULT_ALIGNMENT = 4;
    virtual ~MemcardAllocator();
    virtual void* Alloc(size_t, const char*, unsigned int); // 827DBB08
    virtual void* Alloc(size_t, const char*, unsigned int, unsigned int,
                        unsigned int = 0);                 // 827DBBB0
    virtual void Free(void*, size_t = 0);                  // 827DBBC8
    void SetHeap(CgsMemory::HeapMalloc* lpHeap) { mpHeap = lpHeap; }
private:
    CgsMemory::HeapMalloc* mpHeap; // +4; Prepare writes this at SaveLoad+190
};

// Native bases: OptionHandler at +0 (820CF18C), IGameInterface at +4
// (820CF148). SaveReady/LoadReady extend the primary table at +4/+8;
// they are not entries in the 17-slot Realmc interface.
class SaveLoadSystem : public MessageDisplay::OptionHandler,
                       public RealmcIface::IGameInterface
{
public:
    void Construct(MessageDisplay*, CgsLanguage::LanguageManager*, const char*,
                   const char*, const char*, s32, s32, u32);
    bool Prepare(CgsMemory::HeapMalloc*, CgsMemory::LinearMalloc*, SystemUserProfile*);
    bool Release();
    void Update();
    void SetSilentMode(bool);
    void SetSignedInUserIndex(s32);
    s32 GetSignedInUserIndex() const { return miField210; }
    void RunPendingUIClosed();
    void CopyImageToBuffer(const void*);
    void Load(SaveLoadTaskResultHandler*, const void*);
    void Bootup(SaveLoadTaskResultHandler*, const void*, bool);
    void Save();
    void Save(SaveLoadTaskResultHandler*, const SaveLoadMetadata&, const SaveInfo&);
    void Autosave(SaveLoadTaskResultHandler*, const SaveLoadMetadata&, const SaveInfo&,
                  s32, const ImageFileInfo*);
    void LoadImageFiles(SaveLoadTaskResultHandler*, const void*, s32, const ImageFileInfo*);
    void SetMetadata(const void*, const void*);
    virtual void ShowMessage(const char*, u32, const char**);
    virtual void ShowAutosaveIcon(bool);
    virtual void HandleOption(u32);
    virtual s32 SaveReady();
    virtual s32 LoadReady(s32, s32, s32, s32, RealmcIface::DataBuffer*);
    void BootupShowAutosaveWarning();
    static u32 MessageChoiceForOptionIndex(u32);
    void* GetMugshotBufferFromImageId(s32);
    static const char* LocaleGetStrCallback(u32);
    virtual void ClearMessage();
    void EnableAutosave(s32);
    RealmcIface::SaveCheckParams CreateRealmcSaveCheckParams();
    RealmcIface::SaveInfo CreateRealmcSaveInfo();
    RealmcIface::LoadEntryInfo CreateRealmcMugshotLoadEntryInfo();
    virtual void BootupCheckDone(u32);
    virtual void SaveCheckDone(u32, u32, u32, u32, const int*, s32);
    virtual void SaveDone(u32, u32, const char*);
    virtual bool CheckLoadedData(u32, u32);
    virtual void LoadDone(u32, s32, u32, void*);
    virtual void SetAutosaveDone(s32, s32, s32);
    virtual void CardRemoved();
    virtual void DeleteDone();
    virtual void ClearEntries();
    virtual void FindEntriesDone(u32, u32);
    virtual void FoundEntry(const void*);
    virtual void Callback3C(u32, u32, const void*);
    virtual void SetActiveCardDone(u32, u32);
    void SignIn();
    void SignInUIClosed();
    void WaitForUIClosed(void (SaveLoadSystem::*)());
    void HandleMemcardOption(u32);
    void BootupStart(u32);
    void LoadHandleConfirmLoad(u32);
    void BootupHandleRetryBootup(u32);
    void SaveHandleRetry(u32);
    void BootupHandleNotSignedInOption(u32);

private:
    char macSaveInfoComment[32];                     // 008
    char macSaveInfoDescription[256];                // 028
    void (SaveLoadSystem::*mpfnPendingUIClosed)();    // 128
    SaveLoadTaskResultHandler* mpResultHandler;       // 130
    MessageDisplay* mpMessageDisplay;                // 134
    wchar_t macwTitle[32];                           // 138
    const char* mpacContentInfoFilePath;              // 178
    const char* mpacSaveFilePath;                     // 17C
    bool mbField180;                                 // 180 autosave enabled
    bool mbSilentMode;                               // 181
    bool mbAutoLoad;                                 // 182
    bool mbField183;                                 // 183 bootup checked
    RealmcIface::MemcardInterface* mpMemcardInterface;// 184
    CgsLanguage::LanguageManager* mpLanguageManager;  // 188
    MemcardAllocator mMemcardAllocator;              // 18C
    SystemUserProfile* mpSystemUserProfile;           // 194
    void (SaveLoadSystem::*mMessageDisplayOptionFunc)(u32); // 198
    wchar_t macwMetadataTitle[32];                    // 1A0
    char macTitle[32];                               // 1E0
    OpaqueBuffer mStoredData;                         // 200 {pointer,size}
    s32 miContentInfoFileSize;                        // 208
    void* mpContentInfoFileBuffer;                    // 20C
    s32 miField210;                                   // 210 user index
    s32 miNumberOfMugshotTypes;                       // 214
    s32 miNumberOfMugshotsPerType;                    // 218
    s32 miExtraFilesSizeBytes;                        // 21C
    void* mpMugshotBufferData;                        // 220
    u8 mbAsyncOpState;                               // 224
    CgsSystem::CgsXOverlapped mOverlapped;             // 228, 28 native bytes
    MESSAGEBOX_RESULT mMessageBoxResult;             // 244, 8 native bytes
    bool mbAutosaveIconVisible;                      // 24C
};
CGS_STATIC_ASSERT(sizeof(MemcardAllocator) == 8, "ARTIST MemcardAllocator size");
CGS_STATIC_ASSERT(sizeof(SaveLoadSystem) == 0x250, "ARTIST SaveLoadSystem extent");
}
