#pragma once

// BrnGui::ProfileManager -- the GUI profile/save-load manager (X360 GuiModule member at
// +681696; Prepare @GuiModulePrepare 0x82518D68 hands it the module's heap/linear
// allocators). DWARF home: this header + BrnGuiProfile.cpp. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (behaviour + offsets) with the DecFIGS DWARF as shape/name
// authority (references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiProfile.h).
//
// The manager owns the persisted player profile as a 256KB serialised image
// (mStoredData: Progression / LiveRevenge / OptionsData segments + the DLC1 carve-outs)
// and drives the embedded CgsGui::SaveLoadSystem through the boot / load / save /
// autosave / load-image task machine. It is FOUR interface bases at once (X360
// sub-objects @+0/+4/+8/+12, ctor vtable stores @0x827E0CD0):
//   +0  CgsGui::SaveLoadTaskResultHandler -- the save/load system reports task results here
//   +4  CgsGui::MessageDisplay            -- the save/load system's prompt surface; the
//                                            manager forwards to the attached
//                                            BrnGui::ProfileMessageDisplay (the apt UI)
//   +8  CgsGui::MessageDisplay::OptionHandler -- the user's prompt choice comes back here
//   +12 CgsGui::SystemUserProfile::Listener   -- sign-in / user-name / settings watches
//
// Boot-time contract with BrnGui::GuiModule (X360 offsets from GuiModule::Update
// @0x82527A58 / UpdateCollisionWorldValidation @0x82519578):
//   * Bootup() does NOT touch storage directly: it parks meCollisionWorldState at
//     INVALIDATE and the module's collision-world swap dance
//     (PendingCollisionWorldInvalidate -> SetCollisionWorldValid(false) ->
//     SaveLoadSystem bootup -> BootupResult -> VALIDATE ->
//     PendingCollisionWorldValidate -> SetCollisionWorldValid(true) -> report)
//     frees/restores the collision pool around the storage work.
//   * the module drains GetOutEventQueue() (X360 +681776) into the GUI out channel and
//     polls AutosaveIsEnabled() (X360 +686280) for the autosave heartbeat.
//
// DWARF-only members NOT declared here (absent from the X360 ledger/ARTIST exports, so
// PS3-only per AGENTS "DWARF supplies names, the X360 ledger decides what exists"):
//   Destruct, SetSignedInUserIndex, CurrentUserIsSignedIn, BootupNoLoad, Reset,
//   SetInvalidCharactersInName, ExportImage, ExportImageResult, HandleResetProfileOption,
//   ShowResetProfileMessage, UpdateMessageTask, OnProfileSegmentLocked/Unlocked.

#include "types.hpp"

#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"          // the 3 CgsGui bases + SaveInfo/SaveLoadMetadata/ESaveLoadTaskResult
#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"      // CgsGui::SaveLoadSystem (embedded; X360 +4200)
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"  // CgsGui::SystemUserProfile (+ ::Listener base)
#include "GameShared/GameClasses/Gui/CgsImageFileInfo.h"     // CgsGui::ImageFileInfo
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"          // CgsGui::GuiEventQueueSmall (the out queue)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h" // CgsResource::BaseResourcePtr (X360 +20: PROFILEUPG handle)
#include "GameSource/Gui/BrnGuiOptionsDataProfileDLC1.h"     // BrnGui::OptionsDataProfileDLC1 (stored DLC1 options segment)
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileDLC1.h" // BrnGuiSaveLoad::ProfileDLC1 (stored DLC1 progression segment)

namespace CgsMemory   { class HeapMalloc; class LinearMalloc; }
namespace CgsLanguage { class LanguageManager; }
namespace CgsModule   { class BaseEventReceiverQueue; }
namespace CgsGui      { namespace CgsGuiModuleIO { struct OutputBuffer; } }
namespace BrnProgression { class Profile; struct ProgressionData; }
namespace BrnNetwork  { struct LiveRevengeProfile; }

namespace BrnGui
{
    class GuiCache;
    struct OptionsDataProfile;

    // DWARF BrnGuiProfile.h:61 -- the on-screen prompt surface the manager forwards its
    // MessageDisplay duties to (implemented by the apt-side ProfileMessageComponent).
    // X360 vtable order: ShowMessage(0), ShowNoSpaceMessage(1), HideMessage(2)
    // (ProfileManager::ShowMessage @0x824EF888 dispatches slot 0, ShowNoSpaceMessage
    // @0x824EFA08 slot 1, HideMessage @0x824EFB60 / HandleOption @0x824EF7F0 slot 2).
    class ProfileMessageDisplay
    {
    public:
        virtual void ShowMessage(const char* lpcMessage, u32 luNumberOfOptions,
                                 const char** lpacOptions) = 0;                       // :63
        virtual void ShowNoSpaceMessage(const char* lpcMessage, u32 luRequiredKb,
                                        u32 luFreeKb) = 0;                            // :64
        virtual void HideMessage() = 0;                                               // :65
    };

    // DWARF BrnGuiProfile.h:77 -- the completion callback a profile task reports to
    // (single virtual, dispatched by ReportTaskCompleted @0x82513EC0 slot 0).
    class ProfileTaskResultHandler
    {
    public:
        virtual void HandleProfileTaskResult() = 0;                                   // :79
    };

    // DWARF BrnGuiProfile.h:93 -- one framed mugshot picture in the manager's circular
    // buffer: magic + payload hash + the 9600-byte picture. X360 layout {muMagicNumber
    // @+0, muHash @+4, macBuffer @+8}; sizeof == 9608 on both targets (no pointers).
    class MugshotSaveBuffer
    {
    public:
        // :97 -- mark the slot empty (X360 inline in SetMugshotBufferFromCircularBuffer
        // @0x824EFC28: the null-source path stores only muMagicNumber = 0).
        void Construct();

        // :100 -- frame a picture into the slot: copy, hash, stamp the magic
        // (X360 inline in SetMugshotBufferFromCircularBuffer / CopyImageToBuffer).
        void Construct(const char* lpSourceBuffer, s32 liLength);

        // :103 @0x824EF6D0 -- true when the magic or the payload hash no longer match.
        bool IsCorrupt();

        // :107 -- the picture payload (the +8 skip the X360 image-load fixups apply).
        char* GetBuffer();

        // :111 -- the picture payload size (9600).
        s32 GetBufferLength() const;

    private:
        friend class ProfileManager;   // Prepare sizes the circular buffer allocation

        // DWARF :114 -- 1297434451 == 'MUGS' (big-endian constant kept verbatim).
        static const u32 muCorrectMagicNumber = 0x4D554753u;

        u32  muMagicNumber;            // :116 (X360 +0x00)
        u32  muHash;                   // :117 (X360 +0x04)
        char macBuffer[9600];          // :118 (X360 +0x08; KI_MUGSHOT_PICTURE_SIZE_BYTES)
    };

    // DWARF BrnGuiProfile.h:134. Base order == the X360 sub-object order (+0/+4/+8/+12).
    class ProfileManager
        : public CgsGui::SaveLoadTaskResultHandler        // X360 +0
        , public CgsGui::MessageDisplay                   // X360 +4
        , public CgsGui::MessageDisplay::OptionHandler    // X360 +8
        , public CgsGui::SystemUserProfile::Listener      // X360 +12
    {
    public:
        // Pending-task / prompt-option member callbacks (DWARF mpTaskResultFunc/mpOptionFunc;
        // the X360 stores each as a {function, this-delta} pair -- a member function pointer).
        typedef void (ProfileManager::*TaskResultFunc)(CgsGui::ESaveLoadTaskResult leResult);
        typedef void (ProfileManager::*OptionFunc)(u32 luOption);

        // @0x827E0CD0. Bases' vtables + the embedded sub-objects construct themselves on
        // the host; see the .cpp note for the two X360 stores this cannot reproduce yet.
        ProfileManager();

        // BrnGuiProfile.cpp:91 @0x824FEED0 (GuiModule::Construct @0x82518028 passes its
        // GuiCache/+1005376, SystemUserProfile/+949152 and LanguageManager/+163980).
        void Construct(GuiCache& lrGuiCache, CgsGui::SystemUserProfile& lrSystemUserProfile,
                       CgsLanguage::LanguageManager* lpLanguageManager);

        // BrnGuiProfile.cpp:212 @0x824F8CE0 (GuiModule::Prepare hands the 0x26 game-data
        // heap + a linear allocator). Attaches the SystemUserProfile listener, prepares the
        // save/load system, allocates the mugshot circular buffer. Returns true.
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, CgsMemory::LinearMalloc* lpLinearMalloc);

        // BrnGuiProfile.h:511 (X360 inline in ProfileHost::Release @0x824ED0D8 /
        // GuiModule::Release): detach the listener, release the save/load system.
        bool Release();

        // BrnGuiProfile.h:616 (X360 inline in GuiModule::Update @0x82527A58, which pumps
        // the embedded save/load system at manager+4200 == module+685896).
        void Update();

        // BrnGuiProfile.h:559 -- the live task's handler (null == idle).
        ProfileTaskResultHandler* GetTaskResultHandler() const { return mpTaskResultHandler; }

        // BrnGuiProfile.h:570 -- the save/load system's autosave gate (the X360
        // GuiModule::Update autosave heartbeat reads it at manager+4584). Bodied in the
        // .cpp (the flag is save/load-system internal state).
        bool AutosaveIsEnabled() const;

        // BrnGuiProfile.h:777 -- the manager-side autosave flag (set by Bootup/Load/Save
        // results once storage agrees).
        bool IsAutoSaveEnabled() const { return mbAutosaveEnabled; }

        // BrnGuiProfile.h:536 -- silent mode auto-answers every prompt with option 0
        // (ShowAutosaveEnabled/DisabledMessage @0x824F8D70/0x824F8E00 check it).
        void SetSilentMode(bool lbSilentMode) { mbSilentMode = lbSilentMode; }

        // BrnGuiProfile.h:592 @0x824731A0 / :604 @0x82473278.
        void AttachMessageDisplay(ProfileMessageDisplay* lpMessageDisplay);
        void DetachMessageDisplay(ProfileMessageDisplay& lrMessageDisplay);

        // BrnGuiProfile.h:632 @0x824ECC98 (event 350 in GuiModule::Update).
        void SetProgressionProfile(BrnProgression::Profile* lpProgressionProfile,
                                   const BrnProgression::ProgressionData* lpProgressionData);

        // BrnGuiProfile.h:650 (X360 inline at GuiModule::Update event 351; the inlined
        // assert cites BrnGuiProfile.h:672 "lpLiveRevengeProfile != NULL").
        void SetLiveRevengeProfile(BrnNetwork::LiveRevengeProfile* lpLiveRevengeProfile);

        // BrnGuiProfile.h:662 @0x82473330 -- route the user's prompt choice to the
        // registered option handler (in-range choices only).
        void HandleMessageChoice(u32 luChoice);

        // BrnGuiProfile.h:581 -- the module-drained out queue (X360 manager+80).
        CgsGui::GuiEventQueueSmall& GetOutEventQueue() { return mEventQueue; }

        // BrnGuiProfile.cpp:238 @0x82523CA0. The X360 build does not implement the
        // disk-space task: it binds the handler then fires "Should never get here".
        void CheckDiskSpace(ProfileTaskResultHandler* lpTaskResultHandler);

        // BrnGuiProfile.cpp:272 @0x82513870 -- start the boot-up task (see the
        // collision-world contract in the banner). lbAutoLoad == "load the profile as
        // part of boot-up" (BootupResult reports PROFILE_LOADED when set).
        void Bootup(ProfileTaskResultHandler& lrTaskResultHandler, bool lbAutoLoad);

        // BrnGuiProfile.cpp:334 @0x82513958 -- start (or buffer) an autosave, optionally
        // framing mugshot images into the circular buffer first.
        void Autosave(ProfileTaskResultHandler* lpTaskResultHandler, s32 liNumberOfImageFiles,
                      const CgsGui::ImageFileInfo* laImageFileInfo);

        // BrnGuiProfile.cpp:463 @0x82513B28 / :523 @0x82523D78.
        void Save(ProfileTaskResultHandler& lrTaskResultHandler);
        void Load(ProfileTaskResultHandler& lrTaskResultHandler);

        // BrnGuiProfile.cpp:587 @0x82513C00 -- load the pictures named by the (up to
        // KI_MAX_IMAGE_FILES_PER_OPERATION) valid records into circular-buffer slots.
        void LoadImageFiles(ProfileTaskResultHandler* lpTaskResultHandler,
                            const CgsGui::ImageFileInfo* laImageFileInfo);

        // BrnGuiProfile.cpp:149/:165 (X360 inline in GuiModule::
        // UpdateCollisionWorldValidation @0x82519578: state 1->2 / 4->5 latch-and-test).
        bool PendingCollisionWorldInvalidate();
        bool PendingCollisionWorldValidate();

        // BrnGuiProfile.cpp:181 @0x82517F50 -- the module's swap-dance completion signal
        // (false: INVALIDATING->INVALID + storage bootup; true: VALIDATING->VALID + report).
        void SetCollisionWorldValid(bool lbValid);

        // X360-only @0x824EFD40 (no PS3 DWARF twin; GuiModule::AutosaveProfile @0x8251A568
        // parks the mugshot when autosave is disabled): frame the picture into the circular
        // buffer and hand a copy-image request to the save/load system.
        s32 CopyImageToBuffer(const CgsGui::ImageFileInfo* lpSourceImageFileInfo);

        // X360-only @0x825135C8 (GuiModule::PreWorldUpdate drives it): the PROFILEUPG
        // upgrade-table load stages (bundle load -> resource acquire). Returns 1 once the
        // data is available (or the upgrade feature is absent), 0 while in flight.
        s32 LoadProfileUpgradeData(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutputBuffer,
                                   CgsModule::BaseEventReceiverQueue* lpResponseQueue);

    private:
        // DWARF BrnGuiProfile.h:266 -- the bufferable operations (BufferProfileOperation's
        // meBufferedOperation vocabulary; the X360 stores exactly these values).
        enum EProfileOperation
        {
            E_PROFILEOPERATION_NONE         = 0,
            E_PROFILEOPERATION_LOAD         = 1,
            E_PROFILEOPERATION_SAVE         = 2,
            E_PROFILEOPERATION_AUTOSAVE     = 3,
            E_PROFILEOPERATION_LOAD_IMAGES  = 4,
            E_PROFILEOPERATION_EXPORT_IMAGE = 5,
        };

        // DWARF BrnGuiProfile.h:297 -- what the finished task was (drives
        // ReportTaskCompleted's event fan-out).
        enum ESaveLoadResult
        {
            E_SAVELOADRESULT_INVALID                        = 0,
            E_SAVELOADRESULT_PROFILE_LOADED                 = 1,
            E_SAVELOADRESULT_PROFILE_AND_IMAGE_FILES_SAVED  = 2,
            E_SAVELOADRESULT_IMAGE_FILES_LOADED             = 3,
            E_SAVELOADRESULT_EXPORT_IMAGE                   = 4,
        };

        // DWARF BrnGuiProfile.h:306 -- the collision-world swap handshake with GuiModule.
        enum ECollisionWorldState
        {
            E_COLLISIONWORLDSTATE_VALID        = 0,
            E_COLLISIONWORLDSTATE_INVALIDATE   = 1,
            E_COLLISIONWORLDSTATE_INVALIDATING = 2,
            E_COLLISIONWORLDSTATE_INVALID      = 3,
            E_COLLISIONWORLDSTATE_VALIDATE     = 4,
            E_COLLISIONWORLDSTATE_VALIDATING   = 5,
        };

        // DWARF BrnGuiProfile.h:141 -- a raw byte image of one serialised profile segment.
        // The PS3 DWARF sizes each instantiation by that platform's sizeof(ProfileType)
        // (121392/30016/29552); the X360 images are 118064/30016/29552 (Construct's
        // "Size of" debug prints @0x824FEED0). The x64 host cannot evaluate sizeof for the
        // serialised console layouts, so the byte size is an explicit template argument
        // carrying the X360 value.
        template <typename ProfileType, s32 TI_SIZE_BYTES>
        class FixedSizeOpaqueBuffer
        {
        public:
            u8 maData[TI_SIZE_BYTES];   // DWARF :142
        };

        // Serialised segment sizes (X360; the Construct debug prints name each class).
        static const s32 KI_PROGRESSION_PROFILE_SIZE_BYTES  = 118064;  // "BrnGuiSaveLoad::Profile"
        static const s32 KI_LIVEREVENGE_PROFILE_SIZE_BYTES  = 30016;   // "BrnNetwork::LiveRevengeProfile"
        static const s32 KI_OPTIONSDATA_PROFILE_SIZE_BYTES  = 29552;   // "OptionsDataProfile"

        // DWARF :149 -- the whole stored image (0x40000; published to the metadata).
        static const s32 KI_PROFILE_SIZE = 262144;
        // DWARF :150 -- the tail padding. PS3 DWARF value 81184 (pre-DLC1 segment set);
        // the X360 v1.9 image carves the two DLC1 segments out of it, leaving 74696.
        static const s32 KI_REQUIRED_PADDING = 74696;

        // Mugshot framing (X360 immediates: 9600 picture, 3-slot circular buffer,
        // 3*9608 == 28824 allocated in Prepare @0x824F8CE0).
        static const s32 KI_MUGSHOT_PICTURE_SIZE_BYTES   = 9600;
        static const s32 KI_MUGSHOTS_IN_CIRCULAR_BUFFER  = 3;

        // DWARF BrnGuiProfile.h:153 -- the persisted 256KB image, segment by segment.
        // X360 offsets (from manager base): mProgressionProfile +4800, mLiveRevengeProfile
        // +122864 (0x1DFF0), mOptionsDataProfile +152880 (0x25530), mProgressionProfileDLC1
        // +182432 (0x2C8A0), mOptionsDataProfileDLC1 +192232 (0x2EEE8), macPadData +192248.
        // The two DLC1 segments are the reconstructed x64 types (their sizeof is
        // byte-identical to the X360 images: 9800 / 16 -- static_asserted in the .cpp).
        struct ProfileStoredData
        {
            FixedSizeOpaqueBuffer<BrnProgression::Profile, KI_PROGRESSION_PROFILE_SIZE_BYTES>
                mProgressionProfile;                            // DWARF :154
            FixedSizeOpaqueBuffer<BrnNetwork::LiveRevengeProfile, KI_LIVEREVENGE_PROFILE_SIZE_BYTES>
                mLiveRevengeProfile;                            // DWARF :156
            FixedSizeOpaqueBuffer<BrnGui::OptionsDataProfile, KI_OPTIONSDATA_PROFILE_SIZE_BYTES>
                mOptionsDataProfile;                            // DWARF :158
            BrnGuiSaveLoad::ProfileDLC1    mProgressionProfileDLC1;   // X360 v1.9 (ValidateProfiles @0x825073A0)
            BrnGui::OptionsDataProfileDLC1 mOptionsDataProfileDLC1;   // X360 v1.9 (ReadProfileData @0x824FF298)
            char macPadData[KI_REQUIRED_PADDING];               // DWARF :161
        };

        // ---- the save/load task machine (BrnGuiProfile.cpp) --------------------------

        // cpp:838 @0x824EF7A0 (SaveLoadTaskResultHandler override; the save/load system
        // reports here): consume-and-dispatch the pending task callback.
        virtual void HandleSaveLoadTaskResult(CgsGui::ESaveLoadTaskResult leResult);

        // cpp:1066 @0x824EF7F0 (MessageDisplay::OptionHandler override): hide the prompt,
        // dispatch the pending option callback.
        virtual void HandleOption(u32 luOption);

        // cpp:1079 (X360 body ICF-folded with CheckDiskSpaceResult @0x824EF880 -- both
        // reduce to ReportTaskCompleted): the autosave-warning prompt acknowledgement.
        void HandleWarningOption(u32 luOption);

        // cpp:1112 @0x824EF888 / :1146 @0x824EFA08 / :1175 @0x824EFB60 / :1193 @0x82511038
        // (MessageDisplay overrides; the save/load system prompts through these and the
        // manager forwards to the attached ProfileMessageDisplay).
        virtual void ShowMessage(CgsGui::MessageDisplay::OptionHandler* lpHandler,
                                 const char* lpcMessage, const char** lpacOptions,
                                 u32 luNumberOfOptions);
        virtual void ShowNoSpaceMessage(CgsGui::MessageDisplay::OptionHandler* lpHandler,
                                        const char* lpcMessage, u32 luRequiredKb, u32 luFreeKb);
        virtual void HideMessage();
        virtual void ShowAutosaveIcon(bool lbVisible);

        // h:712 @0x827E0DE0 / :722 @0x827E0E98 / :732 @0x827E2F40 / :766 @0x827E0F70
        // (SystemUserProfile::Listener overrides).
        virtual void UserNameChanged(const char* lpcUserName);
        virtual void ProfileSettingsChanged(const CgsGui::SystemUserProfile::ProfileSettings& lrSettings);
        virtual void SigninStateChanged(CgsGui::SystemUserProfile::EUserSigninState leState);
        virtual void UIClosed();

        // h:548 -- a task is live while its completion handler is bound.
        bool TaskIsRunning() const { return mpTaskResultHandler != 0; }

        // cpp:1255 (X360 inlines it into every task starter: bind the callback + handler,
        // post the task-started event 67).
        void StartTask(TaskResultFunc lpTaskResultFunc, ProfileTaskResultHandler* lpTaskResultHandler);

        // cpp:854 @0x824F8EF0 -- queue an operation (and its images) behind the live task.
        void BufferProfileOperation(EProfileOperation leProfileOperation,
                                    ProfileTaskResultHandler* lpTaskResultHandler,
                                    s32 liNumberOfImageFiles,
                                    const CgsGui::ImageFileInfo* laImageFileInfoArray);

        // cpp:909 @0x82513EC0 -- fan out the finished task's events, call the handler,
        // then replay the buffered operation (if any).
        void ReportTaskCompleted();

        // cpp:415 @0x824F8D70 / :431 @0x824F8E00 -- the autosave state-change prompts
        // (option callback: HandleWarningOption; silent mode auto-answers).
        void ShowAutosaveEnabledMessage();
        void ShowAutosaveDisabledMessage();

        // cpp:261 @0x824EF880 / :295 @0x82510F80 / :384 @0x824FF250 / :487 @0x824F8E98 /
        // :545 @0x82517FB8 / :639 @0x824EF748 -- the per-operation completion callbacks.
        void CheckDiskSpaceResult(CgsGui::ESaveLoadTaskResult leResult);
        void BootupResult(CgsGui::ESaveLoadTaskResult leResult);
        void AutosaveResult(CgsGui::ESaveLoadTaskResult leResult);
        void SaveResult(CgsGui::ESaveLoadTaskResult leResult);
        void LoadResult(CgsGui::ESaveLoadTaskResult leResult);
        void LoadImageFilesResult(CgsGui::ESaveLoadTaskResult leResult);

        // cpp:708 @0x824FF298 -- serialise the live profiles into mStoredData.
        void ReadProfileData();

        // cpp:733 @0x825073A0 -- validate every stored segment after a load (all five
        // checks always run; the X360 ANDs them without short-circuiting).
        bool ValidateProfiles();

        // cpp:1213 @0x824EFC28 -- point a transfer record at the next circular-buffer
        // slot, framing the source picture into it (or marking the slot empty).
        void SetMugshotBufferFromCircularBuffer(CgsGui::ImageFileInfo* lpDestImageFileInfo,
                                                const CgsGui::ImageFileInfo* lpSourceImageFileInfo);

        // ---- members (DWARF order; X360 byte offsets in comments -- the x64 host
        //      widens pointers, so access is BY NAME ONLY) -----------------------------

        // cpp:29 @0x82F276A0 / cpp:35 @0x82F276AC / cpp:39 @0x82F276B0 -- the prompt
        // string tables (values recovered from the XEX .data image; see the .cpp).
        // DWARF types them `const char*[N]` (the options tables feed the
        // MessageDisplay::ShowMessage `const char**` parameter).
        static const char* maMessages[3];
        static const char* maConfirmChoiceOptions[1];
        static const char* maResetProfileChoiceOptions[2];

        // X360-only pair (no PS3 DWARF twin; named from the @0x825135C8 assert text).
        s32 meProfileUpgradeLoadStage;                    // X360 +16  (Construct zeroes)
        CgsResource::BaseResourcePtr mProfileUpgradeResourcePtr; // X360 +20..+47 (PROFILEUPG handle)

        GuiCache* mpGuiCache;                             // h:426  X360 +52
        CgsGui::SystemUserProfile* mpSystemUserProfile;   // h:427  X360 +56
        TaskResultFunc mpTaskResultFunc;                  // h:429  X360 +64 {func,delta}
        OptionFunc mpOptionFunc;                          // h:430  X360 +72 {func,delta}
        CgsGui::GuiEventQueueSmall mEventQueue;           // h:431  X360 +80 (4096+16 -> +4192)
        CgsMemory::HeapMalloc* mpHeapMalloc;              // h:433  X360 +4192
        CgsGui::SaveLoadSystem mSaveLoadSystem;           // h:434  X360 +4200
        u32 muOptionCount;                                // h:435  X360 +4792
        CgsGui::MessageDisplay::OptionHandler* mpOptionHandler; // h:436  X360 +4796
        ProfileStoredData mStoredData;                    // h:438  X360 +4800 (KI_PROFILE_SIZE)
        CgsGui::SaveLoadMetadata mMetadata;               // h:439  X360 +266944
        CgsGui::SaveInfo mSaveInfo;                       // h:440  X360 +267000
        bool mbAutosaveEnabled;                           // h:441  X360 +267288
        ESaveLoadResult miSaveLoadResult;                 // h:442  X360 +267292 [DWARF name kept]
        bool mbSilentMode;                                // h:443  X360 +267296
        bool mbAutoLoad;                                  // h:444  X360 +267297
        ECollisionWorldState meCollisionWorldState;       // h:446  X360 +267300
        ProfileMessageDisplay* mpMessageDisplay;          // h:448  X360 +267304
        ProfileTaskResultHandler* mpTaskResultHandler;    // h:449  X360 +267308
        BrnProgression::Profile* mpProgressionProfile;    // h:452  X360 +267312
        const BrnProgression::ProgressionData* mpProgressionData; // h:453  X360 +267316
        BrnNetwork::LiveRevengeProfile* mpLiveRevengeProfile;     // h:454  X360 +267320
        CgsGui::ImageFileInfo maImageFileInfo[CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION]; // h:455  X360 +267324 (3 slots on X360; the tree constant carries the PS3 7)
        s32 miNumberOfImageFiles;                         // h:456  X360 +267372
        MugshotSaveBuffer* mpaMugshotCircularBuffer;      // h:459  X360 +267376
        s32 miNextMugshotInCircularBuffer;                // h:460  X360 +267380
        EProfileOperation meBufferedOperation;            // h:463  X360 +267384
        CgsGui::ImageFileInfo maBufferedImageFileInfo[CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION]; // h:464  X360 +267388
        s32 miBufferedNumberOfImageFiles;                 // h:465  X360 +267436
        ProfileTaskResultHandler* mpBufferedTaskResultHandler; // h:466  X360 +267440
    };
}
