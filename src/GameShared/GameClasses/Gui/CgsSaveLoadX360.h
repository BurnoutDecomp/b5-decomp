#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"   // CgsGui::ESaveLoadCif, ContentInformationFileInterface, MessageDisplay, SaveLoadTaskResultHandler, SaveLoadMetadata, SaveInfo
#include "GameShared/GameClasses/Gui/CgsImageFileInfo.h"   // CgsGui::ImageFileInfo (Autosave records)

#include <Windows.h>   // HANDLE / OVERLAPPED (the X360 stream/async ops wrap Win32; host Win32 on PC)

namespace CgsMemory { class HeapMalloc; class LinearMalloc; }
class LanguageManager;
class SystemUserProfile;

// RealmcIface - the Realm memory-card (mc) interface family the X360 save/load path drives.
// These are the SDK-shaped types the in-scope SaveLoadSystem functions call into. The memory-card
// interface object itself (RealmcIface::MemcardInterface -- the vtable slots this front-end
// dispatches: Update @+0x08, UserSignedIn @+0x10, MessageChoice @+0x14, SetSilent @+0x18,
// Bootup @+0x24, WriteSave @+0x2C, CheckSave @+0x30, SetActive @+0x34, ReadSave @+0x38) is homed
// in its own SDK TU; include it so the type is defined once (never fork a type that has a home).
#include "SDKs/Realmc/RealmcMemcardInterface.h"   // RealmcIface::MemcardInterface

namespace RealmcIface
{
    // Forward-declared SDK helpers used by Save/Prepare; full bodies live in their own TUs.
    class GameInfo;
    class MemcardInterfaceFactory;
    // Wave-B additions: SDK value types the SaveLoadSystem Create*/Load* helpers build/return.
    // Complete definitions: SDKs/Realmc/RealmcIfaceSaveCheckParams.h /
    // RealmcLoadEntryInfo.h / RealmcDataBuffer.h (include those in the .cpp partfiles).
    class SaveCheckParams;
    class LoadEntryInfo;
    class DataBuffer;
}

// CgsGui::XenonFileInputStream - a read-only file input stream over a Win32 file HANDLE,
// used by the Xenon (X360) save/load path (CgsGui::SaveLoadSystem::Prepare opens a stream
// and reads its header through this). Recovered from the X360 ARTIST binary:
//   GetSize @ 0x8284BD88   Read @ 0x8284BE28
// (assert sites cite the X360 build's gui/CgsSaveLoadX360.h).
//
// Layout (X360 authoritative, from the GetSize/Read member loads):
//   mhFile  [+0x0]  HANDLE  -- the open file handle; INVALID_HANDLE_VALUE (-1) until/if open
//                              fails (both methods assert mhFile != -1, "File open failed!").
//   miSize  [+0x4]  s32     -- the file size (GetSize returns `this[1]` == +4).
namespace CgsGui
{
    class XenonFileInputStream
    {
    public:
        // X360 0x8284BD88. Asserts the file is open (mhFile != INVALID_HANDLE_VALUE),
        // returns the cached file size.
        s32 GetSize() const;

        // X360 0x8284BE28. Asserts the file is open, then reads luBytesToRead bytes from the
        // handle into lpBuffer via Win32 ReadFile (lpOverlapped == NULL). On a ReadFile
        // failure (returns FALSE) it fires "File read failed!". Returns the number of bytes
        // actually read.
        s32 Read(void* lpBuffer, u32 luBytesToRead);

    private:
        HANDLE mhFile; // +0x0 -- open Win32 file handle (-1 == INVALID_HANDLE_VALUE)
        s32    miSize; // +0x4 -- cached file size in bytes
    };

    // CgsGui::MemcardAllocator - a polymorphic allocator front-end the X360 (Xenon) save/load
    // path hands to the memory-card I/O so its dynamic buffers come out of a dedicated
    // HeapMalloc rather than the global heap. Recovered from the X360 ARTIST binary (assert
    // sites cite gui/CgsSaveLoadX360.h):
    //   Alloc                      @ 0x827DBB08  -> mpHeap->Malloc(luSize, /*align*/4); asserts non-null
    //   Free                       @ 0x827DBBC8  -> mpHeap->Free(lpBlock) (tail call)
    //   `vector deleting destructor'@ 0x827DBC00  -> set vtable, then operator delete if (flag & 1)
    //
    // Layout (X360 authoritative, from the member loads):
    //   vptr   [+0x0]  -- leading vtable pointer (the object is polymorphic; Alloc/Free are
    //                     virtuals, and the vector-deleting-destructor writes off_8200F5B4 here).
    //   mpHeap [+0x4]  HeapMalloc* -- the heap every allocation is serviced from (`lwz r3,4(r3)`
    //                     loads it as the `this` for both HeapMalloc::Malloc and HeapMalloc::Free).
    // The KI_DEFAULT_ALIGNMENT(4) the X360 passes to Malloc matches CgsMemory::HeapMalloc's own
    // default alignment. Alloc/Free are declared virtual (X360 dispatch + the destructor's vtable
    // store); the host vtable slot reproduces the +0x0 vptr without a raw offset cast.
    class MemcardAllocator
    {
    public:
        static const s32 KI_DEFAULT_ALIGNMENT = 4;   // X360 `li r5,4` alignment argument

        virtual ~MemcardAllocator();

        // X360 0x827DBB08. Allocate luSize bytes from mpHeap (4-byte aligned). Asserts the
        // HeapMalloc::Malloc result is non-null ("CgsMemory::HeapMalloc::Malloc failed.").
        virtual void* Alloc(s32 luSize);
        // X360 0x827DBBC8. Free a block previously returned by Alloc (forwards to mpHeap->Free).
        virtual void  Free(void* lpBlock);

    private:
        CgsMemory::HeapMalloc* mpHeap;   // +0x4 -- the heap allocations are serviced from
    };

    // CgsGui::SaveLoadSystem - the Xenon (X360) save/load front-end. Drives the memory-card
    // I/O (RealmcIface::MemcardInterface), a Win32 overlapped async op, a mugshot/image
    // buffer, the on-screen MessageDisplay prompts, and reports completion to a
    // SaveLoadTaskResultHandler. It IS the ContentInformationFileInterface the memory-card
    // layer queries for its CIF blobs (LoadCifFile et al). Reconstructed from the X360 ARTIST
    // binary; the assert sites cite gui/CgsSaveLoadX360.cpp.
    //
    // The byte offsets in the comments below are X360-AUTHORITATIVE (32-bit target): they are the
    // exact displacements the ASM loads/stores (mpMessageDisplay @this+0x134, mpMemcardInterface
    // @this+0x184, the overlapped @this+0x228, the autosave-icon flag @this+0x248, ...). Members
    // are declared in that order; gaps the in-scope 12 functions don't touch are reserved with
    // explicit padding. NOTE: this project compiles host x64, where pointers are 8 bytes, so the
    // realised host offsets are NOT byte-identical to the 32-bit X360 ones -- the offset comments
    // document the X360 layout (the source of truth for which member each ASM displacement names),
    // not the host struct layout. The PS3 DecFIGS DWARF lists a different (PS3-only) member set
    // (Thread/SaveDataSystem/sys_memory_container_t) that does NOT match these X360 offsets, so it
    // is used for names/intent only, never as offset authority.
    //
    // NOTE on the 0x194/0x198/0x19C region (three distinct words; the X360 stores prove the
    // split):
    //   +0x194  a single word. ShowMessage's 8-byte `std r10,0x194` writes the option-handler
    //           func into this word AND zeroes the next word (+0x198). Prepare's
    //           `stw r27,0x194` stores the SystemUserProfile pointer here. These two uses alias
    //           the same word by design (one is overwritten by the other at runtime), so the
    //           word is modelled once (mField194) and aliased with mpSystemUserProfile.
    //   +0x198  the ACTIVE member-function-pointer's FUNC word, and +0x19C its this-DELTA word
    //           (mMessageDisplayOptionFunc). Construct/Load/BootupShowAutosaveWarning write the
    //           pair via `std ...,0x198` ({func@+0x198, delta@+0x19C}); HandleOption null-checks
    //           and dispatches ONLY through this pair (`lwz r11,0x198`).
    // ShowMessage's +0x194 store's second word and this +0x198 func word alias the same address
    // by design -- ShowMessage zeroing +0x198 while writing +0x194 is the binary's actual
    // behaviour, reproduced here (no extra guard).
    // The X360 SaveLoadSystem carries an embedded MessageDisplay::OptionHandler sub-object (the
    // engine offsets to it as `this - 4` when passing itself as the prompt's option handler);
    // HandleOption is its override. Model that as a real base so the OptionHandler pointer the
    // manager dispatches through (ProfileManager::HandleMessageChoice -> mpOptionHandler->
    // HandleOption) resolves to SaveLoadSystem::HandleOption via a proper vtable rather than a
    // reinterpret_cast onto the ContentInformationFileInterface vtable (which mis-dispatched to
    // IsSavingCif). ContentInformationFileInterface has no data members, so this only adds a vptr.
    class SaveLoadSystem : public ContentInformationFileInterface,
                           public MessageDisplay::OptionHandler
    {
    public:
        // CgsSaveLoadX360.cpp:155. X360 0x8284C050. Wire up the message display, language
        // manager, the wide-char title (lpcTitle), the save/content file paths, the mugshot
        // grid dimensions (types x per-type; GetMugshotBufferFromImageId @0x8284C910 asserts
        // them as miNumberOfMugshotTypes/miNumberOfMugshotsPerType) and the per-mugshot size.
        // Asserts lpMessageDisplay/lpLanguageManager are non-null. luExtraFilesSizeBytes is
        // the last (28th) argument.
        void Construct(MessageDisplay* lpMessageDisplay, LanguageManager* lpLanguageManager,
                       const char* lpcTitle, const char* lpcContentInfoFilePath,
                       const char* lpcSaveFilePath, s32 liNumberOfMugshotTypes,
                       s32 liNumberOfMugshotsPerType, u32 luExtraFilesSizeBytes);

        // CgsSaveLoadX360.cpp:214. X360 0x82851FC0. Create the memory-card interface, allocate
        // the mugshot buffer from the linear allocator, and slurp the content-information file
        // into a buffer via XenonFileInputStream. Returns true.
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, CgsMemory::LinearMalloc* lpLinearMalloc,
                     SystemUserProfile* lpSystemUserProfile);

        // CgsSaveLoadX360.cpp:255. X360 0x8284C158. Free the content-information file buffer
        // (through the shared ContentInformation provider). Returns true.
        bool Release();

        // X360 0x8284C4E0. Pump the overlapped async op; if the memory-card interface exists,
        // forward Update to it.
        void Update();

        // X360 0x82473110. Mirror silent mode into the memory-card backend on a
        // transition, passing the all-users sentinel (-1), then latch the flag.
        void SetSilentMode(bool lbSilentMode);

        // X360 0x8284C210. Cache the signed-in user index (miField210); >= 0 forwards
        // UserSignedIn() to the memory-card interface, < 0 clears mbField183.
        // (Caller: BrnGui::ProfileManager::SigninStateChanged.) Body: CgsSaveLoadX360_wB_02.cpp.
        void SetSignedInUserIndex(s32 liUserIndex);

        // X360 0x828522D0. Copy ONE image record's bytes INTO the mugshot-buffer slot its
        // miId maps to -- the inbound direction of the same CgsGui::ImageFileInfo record
        // LoadImageFiles walks outbound ({ miId @+0, miSize @+4, mpBuffer @+8 }; 16 bytes on
        // the console, 24 on the x64 host -- see CgsImageFileInfo.h).
        // (Caller: BrnGui::ProfileManager::CopyImageToBuffer.) Body:
        // CgsSaveLoadX360_wB_03.cpp.
        void CopyImageToBuffer(const void* lpImageFile);

        // X360 0x82859B70. Begin a load: bind the result handler, push the metadata, prompt the
        // user (SAVELOAD_CONFIRM_LOAD) routing the choice to LoadHandleConfirmLoad.
        void Load(SaveLoadTaskResultHandler* lpResultHandler, const void* lpMetadata);

        // X360 0x828601D8 (entry not in the ARTIST export set; the body is composed from the
        // exported sibling Load @0x82859B70 -- identical bind-handler/SetMetadata setup, confirmed
        // by the SetMetadata xref @0x8284C240 listing Bootup as a caller -- plus its confirmed
        // callee BootupShowAutosaveWarning @0x828599A0). Begin the profile boot-up: bind the
        // result handler, push the metadata, and show the SAVELOAD_AUTOSAVE_WARNING prompt (routing
        // CONTINUE to BootupStart). ProfileManager::SetCollisionWorldValid tail-calls this.
        void Bootup(SaveLoadTaskResultHandler* lpResultHandler, const void* lpMetadata, bool lbAutoLoad);

        // X360 0x82856040. Write the current save (title + mugshot entries) to the memory card.
        void Save();

        // The manager-facing 3-arg save task (BrnGui::ProfileManager::Save @0x82513B28
        // drives it; the entry itself is not in the ARTIST export set -- the body is
        // composed from the exported sibling Load @0x82859B70's bind-handler + SetMetadata
        // setup, the same composition Bootup uses). Bind the result handler, push the
        // metadata/save-info, then run the storage write and report the result. On PC the
        // storage edge is the CgsSaveLoadPC container (FLAG'd leaf in the body); the
        // console's asynchronous WriteSave completion becomes a synchronous report.
        void Save(SaveLoadTaskResultHandler* lpResultHandler, const SaveLoadMetadata& lrMetadata,
                  const SaveInfo& lrSaveInfo);

        // The autosave task (BrnGui::ProfileManager::Autosave @0x82513958 drives it; same
        // unexported family, same composition). As the 3-arg Save, plus each valid image
        // record is committed into its mugshot-buffer slot (GetMugshotBufferFromImageId)
        // before the write, so the container's mugshot payload persists it -- the console
        // parked the same records in the memcard container's image files.
        void Autosave(SaveLoadTaskResultHandler* lpResultHandler, const SaveLoadMetadata& lrMetadata,
                      const SaveInfo& lrSaveInfo, s32 liNumberOfImageFiles,
                      const ImageFileInfo* laImageFileInfo);

        // True when a saved profile container exists for lrMetadata's save name -- the
        // existence primitive the boot-up flow consumes to branch "no save present" vs
        // "save found" (the console asked the memcard's find-entries machinery). Purely a
        // query; starts no I/O task and touches no task state.
        bool SaveFileExists(const SaveLoadMetadata& lrMetadata) const;

        // X360 0x828521E0. Copy each requested image's mugshot buffer OUT into the caller's
        // CgsGui::ImageFileInfo records, then report the load result to the ALREADY-BOUND
        // handler.
        //
        // SIGNATURE FROM THE ASM PROLOGUE (the Hex-Rays view of this entry is a3-short):
        // `this` in r3 plus FOUR arguments -- r4 (the result handler), r5 (the metadata),
        // r6 -> r30 (the record count) and r7 -> r31 (the record array). The sole caller
        // BrnGui::ProfileManager::LoadImageFiles @0x82513C00 loads exactly that shape
        // (@0x82513E9C..0x82513EB4): r4 = the ProfileManager, r5 = manager+0x412C0 -- the very
        // same &mMetadata ProfileManager::Load hands SaveLoadSystem::Load @0x82523E28 --
        // r6 = *(manager+0x4146C) (the count), r7 = manager+0x4143C (the ImageFileInfo array).
        // r4 and r5 are never referenced by the body: unlike Load/Bootup this task does NOT
        // bind +0x130 and does NOT call SetMetadata; it only READS the handler bound by the
        // preceding task starter. Both are therefore declared but unused.
        void LoadImageFiles(SaveLoadTaskResultHandler* lpResultHandler, const void* lpMetadata,
                            s32 liNumberOfImageFiles, const ImageFileInfo* laImageFileInfo);

    private:
        // X360 0x8284C240. Copy the metadata's title (wide + ascii) and the save-info comment
        // strings into the fixed buffers (asserting each fits) and cache the two size fields.
        void SetMetadata(const void* lpMetadata, const void* lpSaveInfo);

        // X360 0x8284CA78. Show a yes/no/etc. message, routing the user's choice back through
        // HandleMemcardOption. The X360 forwards (handler, this-4, a2, a4, a3) -- the a3/a4
        // arguments are swapped on the way out -- and MessageDisplay::ShowMessage's true order is
        // (handler, message, options, count). So this method's own params are
        // (message=a2, count=a3, options=a4); the forward reproduces the swap.
        void ShowMessage(const char* lpcMessage, u32 luNumberOfOptions, const char** lpacOptions);

        // X360 0x8284CA18. Toggle the autosave icon (only when its desired visibility differs
        // from the cached state).
        void ShowAutosaveIcon(bool lbVisible);

        // X360 0x8284C730. The MessageDisplay::OptionHandler entry point: hide the prompt and
        // dispatch to the pending message-display-option member function. (Reached through the
        // embedded OptionHandler sub-object the engine offsets to as `this - 4` at the call site.)
        virtual void HandleOption(u32 luOption);

        // X360 0x828599A0. Show the autosave warning prompt (SAVELOAD_AUTOSAVE_WARNING), routing
        // confirmation to BootupStart.
        void BootupShowAutosaveWarning();

        // X360 0x8284BF50 (assert cites the X360-baked CgsSaveLoadX360.h:745, i.e. an original
        // header-inline emitted out-of-line). Map a MessageDisplay option INDEX (0..3) to the
        // memory-card SDK's message CHOICE code (1..4); any other index fires
        // "SaveLoad: unexpected option index: " and yields 0.
        static u32 MessageChoiceForOptionIndex(u32 luOptionIndex);

        // X360 0x8284C910. Resolve an image id (liImageId / 1000 == mugshot type,
        // liImageId % 1000 == index within the type) to its slot inside the mugshot buffer:
        // mpMugshotBufferData + (type * miNumberOfMugshotsPerType + index) *
        // miExtraFilesSizeBytes. Asserts both components and the flattened index are in range.
        void* GetMugshotBufferFromImageId(s32 liImageId);

        // ---- wave B private surface (X360-attested; bodies in CgsSaveLoadX360_wB_*.cpp) -------
        //
        // `this` conventions in the Hex-Rays views of these members (X360 ABI mechanics only --
        // all of them are written as plain members using the NAMED members below):
        //   * The Realmc RESULT callbacks (BootupCheckDone / LoadDone / SetAutosaveDone /
        //     CardRemoved / ClearMessage) receive the +4 interface sub-object, so their raw
        //     displacements are (real X360 member offset - 4).
        //   * Everything else (option handlers dispatched by HandleOption, the SignIn family,
        //     the Create* helpers, direct members) receives the real `this`; displacements are
        //     the real X360 offsets.

        // X360 0x8284C1B0. The locale string-id lookup callback handed to the Realmc memory
        // card (Prepare's callback block): maps 0..27 into the maRealmemcardStringIDs table
        // (asserts the range). Static -- no `this`. Body: CgsSaveLoadX360_wB_01.cpp.
        static const char* LocaleGetStrCallback(u32 luStringID);

        // X360 0x8284CAD8. If a pending-option member function is armed
        // (mMessageDisplayOptionFunc.muFunc != 0), hide the prompt via mpMessageDisplay and
        // clear the pair. (Realmc result-callback family: Hex-Rays displacements are real-4.)
        void ClearMessage();

        // X360 0x82852368. (Re)arm the autosave pre-flight: build the SaveCheckParams (one
        // SaveReq for the current save) and hand it to the memory-card interface
        // (CheckSave(1, &params)). liArg is passed by every caller as -1 and never read.
        // Body: CgsSaveLoadX360_wB_04.cpp.
        void EnableAutosave(s32 liArg);

        // X360 0x8284C898. Build the autosave SaveCheckParams: one SaveReq built from
        // macTitle / CreateRealmcSaveInfo / 0x100 / muGameDataSizeKb / TitleInfo::Empty().
        // (X360 sret: dest in r3, `this` in r4.) Body: CgsSaveLoadX360_wB_04.cpp.
        RealmcIface::SaveCheckParams CreateRealmcSaveCheckParams();

        // X360 0x8284C818. Build the RealmcIface::SaveInfo for the current save into
        // lpSaveInfo (EntryContentName(macwMetadataTitle, macTitle, mugshotBytes +
        // muGameDataSizeKb) + the { mpContentInfoFileBuffer, miContentInfoFileSize } pair).
        // Returns lpSaveInfo. SaveInfo itself is an un-homed SDK type (declaration-only ctor
        // extern in the partfile); callers pass an opaque stack buffer. Body:
        // CgsSaveLoadX360_wB_04.cpp.
        void* CreateRealmcSaveInfo(void* lpSaveInfo);

        // X360 0x828523D0. Build the "Mugshots" load-entry record ({ mpMugshotBufferData,
        // total mugshot bytes }). (X360 sret: dest in r3, `this` in r4.) Body:
        // CgsSaveLoadX360_wB_03.cpp.
        RealmcIface::LoadEntryInfo CreateRealmcMugshotLoadEntryInfo();

        // X360 0x82859A38. Realmc bootup-check result callback (0 == ok, 1..2 == failure
        // lanes, >= 3 asserts "Should not get here"). Success: set mbField183/mbField180,
        // EnableAutosave(-1), report success. Failure with a signed-in user: prompt
        // SAVELOAD_RETRY_BOOT (options SAVELOAD_RETRY / SAVELOAD_CONTINUE_WITHOUT_SAVING)
        // routed to BootupHandleRetryBootup; otherwise report result 2 (cancelled).
        void BootupCheckDone(u32 luResult);

        // X360 0x82855FE0. Realmc load result callback: success re-arms the autosave
        // pre-flight (EnableAutosave(-1)); reports 0/1 to the result handler. Body:
        // CgsSaveLoadX360_wB_07.cpp.
        void LoadDone(u32 luResult);

        // X360 0x828521A0. Realmc load-ready callback: hand the title-save data pair
        // { muSaveDataSizeKb, muGameDataSizeKb } to the SDK's DataBuffer and return 0.
        // Only the last parameter is read (FLAG: the four leading parameter roles are
        // unrecovered -- shape from the r4..r8 register usage). Body:
        // CgsSaveLoadX360_wB_07.cpp.
        s32 LoadReady(s32 liArg1, s32 liArg2, s32 liArg3, s32 liArg4,
                      RealmcIface::DataBuffer* lpDataBuffer);

        // X360 0x8284C7D8. Realmc save-ready callback -- never expected on X360; fires
        // "SaveReady called.\n" and returns 0. Body: CgsSaveLoadX360_wB_00.cpp.
        s32 SaveReady();

        // X360 0x8284CC00. Realmc set-autosave result callback:
        // mbField180 = (liResult == 0 && liEnabled == 1). liArg2 is never read
        // (FLAG: parameter roles inferred from the r4/r6 compares). Body:
        // CgsSaveLoadX360_wB_02.cpp.
        void SetAutosaveDone(s32 liResult, s32 liArg2, s32 liEnabled);

        // X360 0x8284CC20. Realmc card-removed callback: clear mbField180, arm the
        // overlapped op (mbAsyncOpState = 1, Construct the XOVERLAPPED @+0x228) and pop the
        // system message box (XShowMessageBoxUI) with the localised
        // SAVELOAD_DEVICE_REMOVED_OR_CHANGED text; asserts ERROR_IO_PENDING.
        void CardRemoved();

        // X360 0x8284C648. Realmc delete result callback -- fires the streamed
        // "DeleteDone: not implemented." assert (folds to CGS_ASSERT). Body:
        // CgsSaveLoadX360_wB_00.cpp.
        void DeleteDone();

        // X360 0x8284CBC0 / 0x8284CB80. Realmc find-entries callbacks -- never expected on
        // X360; assert-only bodies. Bodies: CgsSaveLoadX360_wB_00.cpp.
        void FindEntriesDone();
        void FoundEntry();

        // X360 0x8285D9E8. Pop the Xbox sign-in UI (XShowSigninUI(1, 0)); on immediate
        // success mark mpSystemUserProfile's mbSignedIn (+0x1C flag byte), then
        // WaitForUIClosed(&SignInUIClosed). Body: CgsSaveLoadX360_wB_06.cpp.
        void SignIn();

        // X360 0x82859D48. Sign-in UI closed: a signed-in user (miField210 >= 0) continues
        // to BootupShowAutosaveWarning(); otherwise reports failure. Body:
        // CgsSaveLoadX360_wB_06.cpp.
        void SignInUIClosed();

        // DECLARATION ONLY (body is NOT in this TU's fan-out; do not define): park a
        // pending member-function to run when the current system UI closes. Callers:
        // BootupHandleNotSignedInOption (&SignIn), SignIn (&SignInUIClosed). The X360
        // passes the 8-byte { func, this-delta } pair by value in r4.
        void WaitForUIClosed(void (SaveLoadSystem::*lpfnUIClosedFunc)());

    public:
        // ADDITIVE GROW (profile link-closure wave): the three pending message-display option
        // members. The X360 stores each as a member-function pointer ({func @+0x198, delta
        // @+0x19C}) and dispatches through HandleOption; this reconstruction stores plain
        // function pointers to file-local thunks (see CgsSaveLoadPS3.cpp), so the members are
        // declared public for the thunks to forward to -- access level is a compile-time-only
        // relaxation, the dispatch shape and behaviour are unchanged.

        // X360 0x8284C6D8. Forward the user's message choice
        // (MessageChoiceForOptionIndex(luOption)) to the memory-card interface (vtable slot 5).
        void HandleMemcardOption(u32 luOption);

        // X360 0x82855A60. The autosave-warning CONTINUE handler: hide the autosave icon, then
        // start the boot-up load through the memory-card interface (RealmcIface load-entry
        // records + ReadSave) and pump Update.
        void BootupStart(u32 luOption);

        // X360 0x82855EC0. The confirm-load prompt handler: on YES (option 1) build the
        // RealmcIface load-entry records and start the memory-card read + pump Update; on NO
        // report E_SAVELOADTASKRESULT_FAILURE to the bound result handler (+0x130, vtable 0).
        void LoadHandleConfirmLoad(u32 luOption);

        // X360 0x82855B80. The retry-bootup prompt handler (armed by BootupCheckDone's
        // failure lane): option 1 == retry (BootupStart(0)); otherwise mbField183 = true,
        // mbField180 = false and report failure.
        void BootupHandleRetryBootup(u32 luOption);

        // X360 0x82856188. The save-retry prompt handler: option 1 == Save(); otherwise
        // mbField180 = false and report failure.
        void SaveHandleRetry(u32 luOption);

        // X360 0x8285F200. The not-signed-in prompt handler: option 1 ==
        // WaitForUIClosed(&SignIn); otherwise report failure. Body:
        // CgsSaveLoadX360_wB_06.cpp.
        void BootupHandleNotSignedInOption(u32 luOption);

        // ADDITIVE GROW (BrnGuiProfile wave): the inherited ContentInformationFileInterface
        // overrides. X360-attested by the class's provider role (Prepare publishes this as
        // the shared ContentInformation provider and the memory-card layer queries the CIF
        // slots through it), but NO named body exists in the ARTIST export for any of the
        // four (the CIF vocabulary -- ICON0/PIC1/SND0 -- is the PS3 container's; the X360
        // memcard layer never queries them by name). Bodied as FLAG'd PC-boundary leaves in
        // CgsSaveLoadPS3.cpp (the SaveLoadSystem body TU) so the class is instantiable by
        // value (BrnGui::ProfileManager embeds it at X360 manager+4200).
        virtual bool        IsSavingCif(ESaveLoadCif leCif);
        virtual int         GetCifFileType(ESaveLoadCif leCif);
        virtual const char* GetCifFileName(ESaveLoadCif leCif);
        virtual bool        LoadCifFile(ESaveLoadCif leCif, void** lppData, s32* lpiSize);

    private:

        // ---- X360 layout (byte offsets are authoritative) -------------------------------------
        // +0x00  : inherited ContentInformationFileInterface vptr
        u8   mPad04[0x08 - 0x04];                 // +0x04 .. +0x07

        char macSaveInfoComment[32];              // +0x08  SaveInfo comment (strncpy, 32)
        char macSaveInfoDescription[256];         // +0x28  SaveInfo description (strncpy, 256)

        u64  mField128;                           // +0x128 (Construct zeroes, 8 bytes)

        MessageDisplay* mpActiveMessageDisplay;   // +0x130 the display driving the live operation
        MessageDisplay* mpMessageDisplay;         // +0x134 the display passed to Construct
        wchar_t macwTitle[32];                    // +0x138 wide title (ConvertAsciiToWideCharSafe)

        const char* mpacContentInfoFilePath;      // +0x178 (Construct arg a5 == lpcContentInfoFilePath)
        const char* mpacSaveFilePath;             // +0x17C save-file path (CreateFileA)
        bool   mbField180;                        // +0x180 (Construct sets true)
        bool   mbSilentMode;                      // +0x181 (Construct sets false; SetSilentMode)
        u8     mPad182;                           // +0x182 alignment
        bool   mbField183;                        // +0x183 (Construct sets false)
        RealmcIface::MemcardInterface* mpMemcardInterface; // +0x184
        LanguageManager* mpLanguageManager;       // +0x188
        // +0x18C : ContentInformation provider (a vtable object). Its address (this+0x18C) is
        // published to the file-scope provider pointer in Prepare and used through its vtable
        // (Alloc @+8 / Free @+12). Modelled as its leading vptr; the in-scope code only takes
        // its address and calls those two slots.
        void*  mpContentInfoVptr;                 // +0x18C ContentInformation vptr (provider)
        CgsMemory::HeapMalloc* mpHeapMalloc;      // +0x190 (Prepare arg)

        // +0x194 : a single word, used two ways at different times (they alias the same address
        //          on purpose -- see the region note above). ShowMessage stores the
        //          HandleMemcardOption func word here (and its 8-byte store zeroes +0x198);
        //          Prepare stores the SystemUserProfile pointer here.
        union
        {
            u32 muField194;                       // +0x194 (ShowMessage's func word)
            SystemUserProfile* mpSystemUserProfile; // +0x194 (Prepare)
        };
        // X360 +0x198 / +0x19C: the ACTIVE pending-option member-function pointer { code-ptr,
        // this-delta }. Construct/Load/BootupShowAutosaveWarning store it; HandleOption null-checks
        // and dispatches through it. muFunc is WIDENED to a full 64-bit pointer for the PC x64
        // build: the console stored a 32-bit code address, but the PC option-handler trampolines
        // (SaveLoadSystem_BootupStart / _LoadHandleConfirmLoad) live in an image based at
        // 0x140000000 (>4GB), so a u32 truncates the high byte and HandleOption dispatches a garbage
        // pointer. muDelta stays 32-bit (the trampolines are free functions, delta 0). This widening
        // shifts the following members past their X360 offsets -- harmless here, every member is
        // accessed by name (no offset-poke, no layout static_assert); the +0x1xx labels below are
        // the X360 reference, already approximate on x64 (the +0x194 pointer union is 8 bytes too).
        struct { uintptr_t muFunc; u32 muDelta; } mMessageDisplayOptionFunc;

        wchar_t macwMetadataTitle[32];            // X360 +0x1A0 wide metadata title (ConvertAscii dest)

        char   macTitle[32];                      // +0x1E0 metadata title (strncpy, 32)

        u32    muSaveDataSizeKb;                  // +0x200 (metadata +48)
        u32    muGameDataSizeKb;                  // +0x204 (metadata +52)
        s32    miContentInfoFileSize;             // +0x208 content-info file size
        void*  mpContentInfoFileBuffer;           // +0x20C content-info file buffer
        s32    miField210;                        // +0x210 (Construct sets -1)
        // +0x214/+0x218: the mugshot grid. Names are X360-attested by the
        // GetMugshotBufferFromImageId @0x8284C910 assert strings ("liType <
        // miNumberOfMugshotTypes" / "liIndex < miNumberOfMugshotsPerType").
        s32    miNumberOfMugshotTypes;            // +0x214 (Construct arg a7)
        s32    miNumberOfMugshotsPerType;         // +0x218 (Construct arg a8)
        s32    miExtraFilesSizeBytes;             // +0x21C (Construct last arg; per-mugshot bytes)
        void*  mpMugshotBufferData;               // +0x220 mugshot buffer (LinearMalloc)
        u8     mbAsyncOpState;                    // +0x224 overlapped-op state (1 == in flight)
        u8     mPad225[0x228 - 0x225];            // +0x225 .. +0x227
        u8     maOverlapped[0x248 - 0x228];       // +0x228 X360 XOVERLAPPED (XGetOverlappedResult)
        bool   mbAutosaveIconVisible;             // +0x248 cached autosave-icon visibility
        u8     mPad249[0x24C - 0x249];            // +0x249 .. +0x24B
        u8     mField24C;                         // +0x24C (Construct zeroes)

        // ADDITIVE GROW (PC storage wave): the host-width capture of the metadata's
        // stored-data view. The X360 SetMetadata caches the metadata's +0x30/+0x34 words
        // (muSaveDataSizeKb/muGameDataSizeKb above) -- on the 32-bit console those two
        // words ARE the SaveLoadMetadata::mStoredData OpaqueBuffer {ptr, size} pair, and
        // Save's title entry passes them to WriteSave as the image buffer/size. On the
        // x64 host that pair no longer fits two u32 words, so the task starters
        // (Load / the 3-arg Save / Autosave) capture the same view here full-width via
        // CaptureStoredDataView; the storage edge reads/writes through it.
        OpaqueBuffer mStoredDataView;

        // Capture lrMetadata.mStoredData into mStoredDataView (the host-width form of
        // the X360 SetMetadata +0x30/+0x34 capture -- see mStoredDataView).
        void CaptureStoredDataView(const SaveLoadMetadata& lrMetadata);

        // The mugshot buffer's total byte size (per-mugshot size x grid), the second
        // container payload -- the same product Prepare allocates and the X360 Save's
        // "Mugshots" entry publishes.
        s32 GetMugshotBufferSizeBytes() const;
    };
}
