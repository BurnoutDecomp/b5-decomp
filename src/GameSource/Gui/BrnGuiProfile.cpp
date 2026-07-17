#include "GameSource/Gui/BrnGuiProfile.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"                // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"              // CgsMemory::HeapMalloc::Malloc
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"  // CgsModule::BaseEventReceiverQueue (upgrade-load responses)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                // CgsGui::CgsGuiModuleIO::OutputBuffer (upgrade-load requests)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // LoadBundleRequest / AcquireResourceRequest
#include "GameSource/Gui/BrnGuiCache.h"                               // BrnGui::GuiCache (GetOptionsDataProfile)
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                  // BrnGui::OptionsDataProfile (stored-segment validate/notify)
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfile.h"            // BrnGuiSaveLoad::Profile (stored-segment validate)
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"                    // BrnGui::GuiOverlayWaitFinishRequest (event 188)

#include <cstring>   // std::memcpy / std::strncpy

// ===========================================================================
// BrnGui::ProfileManager  (GameSource/Gui/BrnGuiProfile.{h,cpp})
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; per-function addresses on each
// definition. DWARF (DecFIGS) supplies the names/signatures; the X360 pseudocode
// and assembly supply the behaviour and the member offsets recorded in the header.
//
// PLATFORM/STORAGE BOUNDARY. The embedded CgsGui::SaveLoadSystem is only partially
// reconstructed (CgsSaveLoadPS3.cpp: Construct/Prepare/Release/Update/Load/
// LoadImageFiles + prompts). The X360 entry points this manager drives that are NOT
// yet reconstructed there (Bootup @0x8285EE80-family, the 3-arg Save @0x8285F268,
// Autosave, CopyImageToBuffer @0x828522D0, SetSignedInUserIndex, and the private
// autosave-enabled / signed-in-user words) are modelled below as file-local FLAG'd
// PC-platform leaves, exactly like the BrnBootProfile.cpp boundary no-ops: the
// manager-side control flow and state machine stay X360-faithful, only the
// storage-facing edge is stubbed. The synchronous-success leaves keep the task
// machine able to complete on PC (no real storage yet).
// ===========================================================================

namespace BrnGui
{
namespace
{
    // ---- manager -> module event ids (X360 AddEvent immediates) -------------------
    const s32 KI_EVENT_PROFILE_LOAD_COMPLETE   = 19;   // broadcast after a validated load
    const s32 KI_EVENT_PROFILE_TASK_STARTED    = 67;   // busy/autosave spinner on
    const s32 KI_EVENT_PROFILE_TASK_FINISHED   = 68;   // busy/autosave spinner off
    const s32 KI_EVENT_USER_SIGNED_OUT         = 86;   // SigninStateChanged sign-out
    const s32 KI_EVENT_USER_SIGNED_IN          = 87;   // SigninStateChanged sign-in
    const s32 KI_EVENT_BOOTUP_CANCELLED        = 89;   // BootupResult: user cancelled boot-up
    const s32 KI_EVENT_OVERLAY_WAIT_FINISH     = 188;  // GuiOverlayWaitFinishRequest channel
    const s32 KI_EVENT_PROGRESSION_LOADED      = 352;  // BrnProgression::Profile* payload
    const s32 KI_EVENT_LIVEREVENGE_LOADED      = 353;  // BrnNetwork::LiveRevengeProfile* payload
    const s32 KI_EVENT_AUTOSAVE_ICON           = 355;  // ShowAutosaveIcon flag payload
    const s32 KI_EVENT_AUTOSAVE_IMAGES_SAVED   = 357;  // count + image-record payload
    const s32 KI_EVENT_IMAGE_FILES_LOADED      = 360;  // image-record payload

    // PROFILEUPG upgrade-load request tags/ids (X360 @0x825135C8 immediates).
    const s32 KI_RESOURCE_REQUEST_LOAD_BUNDLE      = 2;
    const s32 KI_RESOURCE_REQUEST_ACQUIRE_RESOURCE = 4;
    const s32 KI_PROFILEUPG_POOL_ID                = 5;

    // The X360 content title id ProfileManager::Construct hands the save/load system
    // (lis 0x4541 / ori 0x0806). The committed SaveLoadSystem::Construct signature types
    // the slot as its 4th string pointer and only stores it, so the id rides through.
    const u32 KU_CONTENT_TITLE_ID = 0x45410806u;

    // The mugshot transfer-record size once a record points at a framed circular-buffer
    // slot (X360 immediate 9608 == sizeof(MugshotSaveBuffer) on both targets).
    const s32 KI_MUGSHOT_REQUEST_SIZE_BYTES = 9608;

    typedef CgsGui::GuiEventQueueSmall ProfileEventQueue;

    void PostByteEvent(ProfileEventQueue& lrQueue, s32 liEventId)
    {
        const u8 lu8Payload = 0;
        lrQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Payload), liEventId, 1);
    }

    // ---- CgsGui::SaveLoadSystem boundary (un-reconstructed X360 entry points /
    //      private state; see the banner) -------------------------------------------

    // FLAG PC-platform leaf: SaveLoadSystem's autosave-enabled word (X360 system+0x180,
    // read by the manager at manager+4584 and by GuiModule::Update at module+686280) is
    // private un-accessored state of the committed recon; the X360 Construct defaults it
    // TRUE, so the PC leaf reports storage-always-writable until the memcard TU lands.
    bool SaveLoadSystem_AutosaveIsEnabled(const CgsGui::SaveLoadSystem& /*lrSystem*/)
    {
        return true;
    }

    // FLAG PC-platform leaf: SaveLoadSystem's signed-in-user word (X360 system+0x210;
    // -1 == none). The PC build has one implicit local user; report "signed in" slot 0.
    s32 SaveLoadSystem_GetSignedInUserIndex(const CgsGui::SaveLoadSystem& /*lrSystem*/)
    {
        return 0;
    }

    // FLAG PC-platform leaf: CgsGui::SaveLoadSystem::SetSignedInUserIndex (X360-attested
    // via SigninStateChanged @0x827E2F40) is not in the committed recon; no-op.
    void SaveLoadSystem_SetSignedInUserIndex(CgsGui::SaveLoadSystem& /*lrSystem*/, s32 /*liUserIndex*/)
    {
    }

    // (CgsGui::SaveLoadSystem::Bootup -- the profile boot-up task -- is now reconstructed in
    // CgsSaveLoadPS3.cpp and shows the SAVELOAD_AUTOSAVE_WARNING prompt; SetCollisionWorldValid
    // below calls it directly, so the prior fabricated auto-complete leaf is retired.)

    // (The 3-arg save task now has a real body -- CgsGui::SaveLoadSystem::Save in
    // CgsSaveLoadPS3.cpp writes the PC profile container; the prior fabricated
    // synchronous-SUCCESS leaf is retired. This shim only forwards.)
    void SaveLoadSystem_Save(CgsGui::SaveLoadSystem& lrSystem,
                             CgsGui::SaveLoadTaskResultHandler& lrHandler,
                             const CgsGui::SaveLoadMetadata& lrMetadata,
                             const CgsGui::SaveInfo& lrSaveInfo)
    {
        lrSystem.Save(&lrHandler, lrMetadata, lrSaveInfo);
    }

    // (The autosave task now has a real body -- CgsGui::SaveLoadSystem::Autosave in
    // CgsSaveLoadPS3.cpp commits the image records and writes the PC profile container;
    // the prior fabricated synchronous-SUCCESS leaf is retired. This shim only forwards.)
    void SaveLoadSystem_Autosave(CgsGui::SaveLoadSystem& lrSystem,
                                 CgsGui::SaveLoadTaskResultHandler& lrHandler,
                                 const CgsGui::SaveLoadMetadata& lrMetadata,
                                 const CgsGui::SaveInfo& lrSaveInfo,
                                 s32 liNumberOfImageFiles,
                                 const CgsGui::ImageFileInfo* laImageFileInfo)
    {
        lrSystem.Autosave(&lrHandler, lrMetadata, lrSaveInfo,
                          liNumberOfImageFiles, laImageFileInfo);
    }

    // FLAG PC-platform leaf: CgsGui::SaveLoadSystem::CopyImageToBuffer @0x828522D0 is
    // un-reconstructed; the mugshot stays framed in the circular buffer, nothing exports.
    s32 SaveLoadSystem_CopyImageToBuffer(CgsGui::SaveLoadSystem& /*lrSystem*/,
                                         const CgsGui::ImageFileInfo& /*lrRequest*/)
    {
        return 0;
    }

    // UIClosed's forward into the save/load system (X360 consumes the pending
    // member-callback pair at system+0x128) targets private un-accessored state of the
    // committed recon; no-op until the memcard prompts land.
    // FLAG PC-platform leaf: save/load-system internal continuation (see above).
    void SaveLoadSystem_NotifyUIClosed(CgsGui::SaveLoadSystem& /*lrSystem*/)
    {
    }

    // ---- CgsGui::SystemUserProfile boundary (private members the X360 pokes) -------

    // FLAG PC-platform leaf: the current user index (X360 reads mpSystemUserProfile->+0)
    // is private in the committed CgsGuideIntegration.h; PC single local user == 0.
    u32 SystemUserProfile_GetUserIndex(const CgsGui::SystemUserProfile* /*lpProfile*/)
    {
        return 0;
    }

    // The X360 sign-out path stamps KU_USERINDEX_NONE(4) into the profile watcher and
    // re-runs its (private) UpdateUserSigninState; both are committed-private, and the
    // PC build never signs its single user out. No-op.
    // FLAG PC-platform leaf: console sign-out path (see above).
    void SystemUserProfile_ClearSignedInUser(CgsGui::SystemUserProfile* /*lpProfile*/)
    {
    }

    // ---- BrnGui::GuiCache boundary (unnamed far bytes the X360 pokes) --------------

    // GuiCache byte +77577 mirrors the autosave-icon visibility for the view side; that
    // far byte is not yet named in BrnGuiCache.h. No-op (the event-355 post below is the
    // real observable).
    // FLAG PC-platform leaf: un-named GuiCache far byte (see above).
    void GuiCache_SetAutosaveIconVisible(GuiCache* /*lpGuiCache*/, bool /*lbVisible*/)
    {
    }

    // FLAG PC-platform leaf: the sign-in notification gate (X360 GuiCache byte +19285)
    // is not yet named in BrnGuiCache.h; the PC user never re-signs-in mid-session.
    bool GuiCache_SignedInEventWanted(const GuiCache* /*lpGuiCache*/)
    {
        return false;
    }

    // After a validated load the X360 mirrors the loaded progression profile's licence
    // byte (+118033, inside the serialised image) into GuiCache+19320; both offsets are
    // un-named interiors of foreign TUs. No-op.
    // FLAG PC-platform leaf: un-named GuiCache/profile-image interiors (see above).
    void GuiCache_MirrorLicenceFlagFromProfile(GuiCache* /*lpGuiCache*/,
                                               const BrnProgression::Profile* /*lpProfile*/)
    {
    }

    // FLAG PC-platform leaf: the live DLC1 options block (X360 GuiCache+76776, right
    // after the live OptionsDataProfile at +47224) has no committed GuiCache accessor
    // yet; a null return skips the stored-segment refresh.
    const OptionsDataProfileDLC1* GuiCache_GetOptionsDataProfileDLC1(GuiCache* /*lpGuiCache*/)
    {
        return 0;
    }

    // ---- BrnProgression / BrnNetwork boundary (un-reconstructed profile codecs) -----

    // FLAG PC-platform leaf: BrnProgression::Profile::Serialise (live -> stored image,
    // with the optional PROFILEUPG table and the DLC1 image) is a Progression-TU
    // boundary; the stored image keeps its previous bytes.
    void ProgressionProfile_Serialise(BrnProgression::Profile* /*lpProfile*/,
                                      u8* /*lpSaveImage*/, const void* /*lpUpgradeData*/,
                                      BrnGuiSaveLoad::ProfileDLC1* /*lpSaveImageDLC1*/)
    {
    }

    // FLAG PC-platform leaf: BrnProgression::Profile::Deserialise (stored image -> live,
    // same operands) is the same Progression-TU boundary; the live profile is unchanged.
    void ProgressionProfile_Deserialise(BrnProgression::Profile* /*lpProfile*/,
                                        const u8* /*lpSaveImage*/, const void* /*lpUpgradeData*/,
                                        const BrnGuiSaveLoad::ProfileDLC1* /*lpSaveImageDLC1*/)
    {
    }

    // FLAG PC-platform leaf: BrnNetwork::LiveRevengeProfile::ValidateProfile is
    // declared in BrnNetworkLiveRevengeManager.h but its body TU is not committed (and
    // the X360 call site passes no live arguments); report the stored segment valid.
    bool LiveRevengeProfile_ValidateProfile(const u8* /*lpSaveImage*/)
    {
        return true;
    }

    // FLAG PC-platform leaf: the PROFILEUPG availability gate is the un-recovered
    // DLC/entitlement rodata test ((dword_82FFA7F4 & dword_82FFA7F8[dword_82FFA86C]) ==
    // the mask -- same family BrnEventScoresManager documents); no upgrade content on PC.
    bool ProfileUpgradeFeatureIsEnabled()
    {
        return false;
    }

    // Binding the acquired PROFILEUPG handle out of the acquire-response payload goes
    // through CgsResource::BaseResourcePtr's protected CreateFromHandle (and the
    // response's handle layout); deferred with the (PC-disabled) upgrade feature.
    // FLAG PC-platform leaf: PROFILEUPG handle bind (see above).
    void ProfileUpgradeResource_BindFromEvent(CgsResource::BaseResourcePtr& /*lrResourcePtr*/,
                                              const CgsModule::Event* /*lpEvent*/)
    {
    }
}

// The stored-image segments must stay byte-identical to the X360 image the metadata
// publishes (KI_PROFILE_SIZE == 0x40000).
static_assert(sizeof(BrnGuiSaveLoad::ProfileDLC1) == 9800,
              "stored DLC1 progression segment must match the X360 image");
static_assert(sizeof(OptionsDataProfileDLC1) == 16,
              "stored DLC1 options segment must match the X360 image");

// ---------------------------------------------------------------------------
// The prompt string tables (X360 .data @0x82F276A0/@0x82F276AC/@0x82F276B0;
// values read from the XEX image -- localisation keys plus the one literal).
// ---------------------------------------------------------------------------
const char* ProfileManager::maMessages[3] =
{
    "SAVELOAD_AUTOSAVE_ENABLED",                              // [0] autosave regained
    "SAVELOAD_AUTOSAVE_DISABLED",                             // [1] autosave lost
    "Your progress will be lost if you choose to continue.",  // [2] reset warning (PS3-only flow)
};
const char* ProfileManager::maConfirmChoiceOptions[1] =
{
    "SAVELOAD_CONTINUE",
};
const char* ProfileManager::maResetProfileChoiceOptions[2] =
{
    "SAVELOAD_CANCEL",
    "SAVELOAD_CONTINUE",
};

// ===========================================================================
// MugshotSaveBuffer
// ===========================================================================

// BrnGuiProfile.h:97 (X360 inline: the null-source arm of
// SetMugshotBufferFromCircularBuffer @0x824EFC28 stores only the cleared magic).
void MugshotSaveBuffer::Construct()
{
    muMagicNumber = 0;
}

// BrnGuiProfile.h:100 (X360 inline in SetMugshotBufferFromCircularBuffer /
// CopyImageToBuffer @0x824EFD40: copy, hash, stamp).
void MugshotSaveBuffer::Construct(const char* lpSourceBuffer, s32 liLength)
{
    std::memcpy(macBuffer, lpSourceBuffer, static_cast<usize>(liLength));
    muHash        = CgsContainers::CgsHash::CalculateHash(macBuffer, liLength);
    muMagicNumber = muCorrectMagicNumber;
}

// BrnGuiProfile.h:103 @0x824EF6D0.
bool MugshotSaveBuffer::IsCorrupt()
{
    return muMagicNumber != muCorrectMagicNumber ||
           muHash != CgsContainers::CgsHash::CalculateHash(macBuffer, static_cast<s32>(sizeof(macBuffer)));
}

// BrnGuiProfile.h:107 / :111.
char* MugshotSaveBuffer::GetBuffer()
{
    return macBuffer;
}

s32 MugshotSaveBuffer::GetBufferLength() const
{
    return static_cast<s32>(sizeof(macBuffer));
}

// ===========================================================================
// ProfileManager -- lifecycle
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::ProfileManager  @ 0x827E0CD0
//   On the host the four base vtables (+0/+4/+8/+12 on X360), the embedded
//   mProfileUpgradeResourcePtr (the X360 self-linking stores at +20..+47) and the
//   embedded save/load system construct themselves. The remaining X360 ctor work --
//   the eight -1 "no signed-in user" stamps into per-licence-segment words DEEP
//   inside the serialised progression/DLC1 images (mStoredData interiors at
//   +0x460/+0x8C8/+0xD30/+0x1198/+0x1600/+0xE620 and DLC1 +0x7B0/+0xAC8) -- lives
//   inside segments owned by the un-reconstructed save-image codecs, so it is NOT
//   fabricated here; it lands with the BrnGuiSaveLoad::Profile serialiser TU.
// ---------------------------------------------------------------------------
ProfileManager::ProfileManager()
{
}

// ---------------------------------------------------------------------------
// ProfileManager::Construct  @ 0x824FEED0  (BrnGuiProfile.cpp:91)
//   Debug-print the three segment sizes, construct the out queue and the save/load
//   system (this as its MessageDisplay, the X360 content title id, the memcard
//   image path, 5/20 image metrics, one 9608-byte extra file), fill the metadata /
//   save-info strings, and zero the task machine.
// ---------------------------------------------------------------------------
void ProfileManager::Construct(GuiCache& lrGuiCache, CgsGui::SystemUserProfile& lrSystemUserProfile,
                               CgsLanguage::LanguageManager* lpLanguageManager)
{
    using namespace CgsDev;

    // The three "Size of" prints (each gated on message-filter bit 0; the X360 emits
    // an empty chunk between the caption and the class name).
    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint << "Size of \t" << "BrnGuiSaveLoad::Profile" << "\t\t"
                           << KI_PROGRESSION_PROFILE_SIZE_BYTES << "\n";
    }
    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint << "Size of \t" << "BrnNetwork::LiveRevengeProfile" << "\t\t"
                           << KI_LIVEREVENGE_PROFILE_SIZE_BYTES << "\n";
    }
    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint << "Size of \t" << "OptionsDataProfile" << "\t\t"
                           << KI_OPTIONSDATA_PROFILE_SIZE_BYTES << "\n";
    }

    mEventQueue.Construct();                                    // X360 +80

    mpGuiCache           = &lrGuiCache;                         // X360 +52
    mpSystemUserProfile  = &lrSystemUserProfile;                // X360 +56
    mpTaskResultHandler  = 0;                                   // X360 +267308
    mbAutosaveEnabled    = false;                               // X360 +267288
    mpTaskResultFunc     = 0;                                   // X360 +64
    mpOptionFunc         = 0;                                   // X360 +72
    mbSilentMode         = false;                               // X360 +267296
    mpHeapMalloc         = 0;                                   // X360 +4192
    mpMessageDisplay     = 0;                                   // X360 +267304
    muOptionCount        = 0;                                   // X360 +4792
    mpOptionHandler      = 0;                                   // X360 +4796

    // The save/load system: this manager IS its MessageDisplay (X360 passes the +4
    // sub-object). The 4th argument carries the X360 content title id through the
    // committed recon's string-typed slot (stored, never dereferenced in scope).
    mSaveLoadSystem.Construct(this, reinterpret_cast<::LanguageManager*>(lpLanguageManager),
                              "Burnout Paradise",
                              reinterpret_cast<const char*>(static_cast<usize>(KU_CONTENT_TITLE_ID)),
                              "Memcard\\SaveImage.png", 5, 20,
                              static_cast<u32>(KI_MUGSHOT_REQUEST_SIZE_BYTES));

    std::strncpy(mMetadata.macTypename, "Burnout Paradise", sizeof(mMetadata.macTypename)); // X360 +266944
    std::strncpy(mMetadata.macFilename, "Profile", sizeof(mMetadata.macFilename));          // X360 +266976
    std::strncpy(mSaveInfo.macTitle, "Burnout Paradise", sizeof(mSaveInfo.macTitle));       // X360 +267000

    meProfileUpgradeLoadStage      = 0;                          // X360 +16
    mpProgressionProfile           = 0;                          // X360 +267312
    mpProgressionData              = 0;                          // X360 +267316
    mpLiveRevengeProfile           = 0;                          // X360 +267320
    meCollisionWorldState          = E_COLLISIONWORLDSTATE_VALID; // X360 +267300
    miNextMugshotInCircularBuffer  = 0;                          // X360 +267380
    meBufferedOperation            = E_PROFILEOPERATION_NONE;    // X360 +267384
}

// ---------------------------------------------------------------------------
// ProfileManager::Prepare  @ 0x824F8CE0  (BrnGuiProfile.cpp:212)
// ---------------------------------------------------------------------------
bool ProfileManager::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, CgsMemory::LinearMalloc* lpLinearMalloc)
{
    CGS_ASSERT(lpHeapMalloc != 0, "lpHeapMalloc != NULL");

    mpHeapMalloc = lpHeapMalloc;

    mpSystemUserProfile->AttachListener(*this);   // the +12 Listener sub-object on X360

    // The committed SaveLoadSystem::Prepare types its watcher param with the global-ns
    // SystemUserProfile forward; it is the same CgsGui::SystemUserProfile object.
    mSaveLoadSystem.Prepare(lpHeapMalloc, lpLinearMalloc,
                            reinterpret_cast<::SystemUserProfile*>(mpSystemUserProfile));

    mpaMugshotCircularBuffer = static_cast<MugshotSaveBuffer*>(mpHeapMalloc->Malloc(
        KI_MUGSHOTS_IN_CIRCULAR_BUFFER * static_cast<s32>(sizeof(MugshotSaveBuffer)), 4));

    return true;
}

// ---------------------------------------------------------------------------
// ProfileManager::Release  (BrnGuiProfile.h:511; X360 inline in
// ProfileHost::Release @0x824ED0D8 / the module release path): detach the
// sign-in listener, release the save/load system.
// ---------------------------------------------------------------------------
bool ProfileManager::Release()
{
    mpSystemUserProfile->DetachListener(*this);
    mSaveLoadSystem.Release();
    return true;
}

// ---------------------------------------------------------------------------
// ProfileManager::Update  (BrnGuiProfile.h:616; X360 inline in GuiModule::Update
// @0x82527A58, which pumps the embedded system at manager+4200 == module+685896).
// ---------------------------------------------------------------------------
void ProfileManager::Update()
{
    mSaveLoadSystem.Update();
}

// ---------------------------------------------------------------------------
// ProfileManager::AutosaveIsEnabled  (BrnGuiProfile.h:570)
// FLAG PC-platform leaf: reads the save/load system's private autosave word
// (X360 manager+4584) through the boundary helper above.
bool ProfileManager::AutosaveIsEnabled() const
{
    return SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem);
}

// ===========================================================================
// ProfileManager -- message display attachment / prompt plumbing
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::AttachMessageDisplay  @ 0x824731A0  (BrnGuiProfile.h:592)
// ---------------------------------------------------------------------------
void ProfileManager::AttachMessageDisplay(ProfileMessageDisplay* lpMessageDisplay)
{
    CGS_ASSERT(lpMessageDisplay != 0, "lpMessageDisplay != NULL");
    CGS_ASSERT(mpMessageDisplay == 0, "AttachMessageDisplay: MessageDisplay already attached");

    mpMessageDisplay = lpMessageDisplay;
}

// ---------------------------------------------------------------------------
// ProfileManager::DetachMessageDisplay  @ 0x82473278  (BrnGuiProfile.h:604)
// ---------------------------------------------------------------------------
void ProfileManager::DetachMessageDisplay(ProfileMessageDisplay& lrMessageDisplay)
{
    CGS_ASSERT(mpMessageDisplay == &lrMessageDisplay,
               "DetachMessageDisplay: MessageDisplay was not attached");

    mpMessageDisplay = 0;
}

// ---------------------------------------------------------------------------
// ProfileManager::HandleMessageChoice  @ 0x82473330  (BrnGuiProfile.h:662)
//   In-range choices go to the registered prompt option handler's virtual.
// ---------------------------------------------------------------------------
void ProfileManager::HandleMessageChoice(u32 luChoice)
{
    if (luChoice < muOptionCount)
    {
        CGS_ASSERT(mpOptionHandler != 0, "mpOptionHandler != 0");
        mpOptionHandler->HandleOption(luChoice);
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::ShowMessage  @ 0x824EF888  (BrnGuiProfile.cpp:1112)
//   MessageDisplay override (the save/load system prompts through it): remember
//   the option handler/count, forward to the attached apt-side display.
// ---------------------------------------------------------------------------
void ProfileManager::ShowMessage(CgsGui::MessageDisplay::OptionHandler* lpHandler,
                                 const char* lpcMessage, const char** lpacOptions,
                                 u32 luNumberOfOptions)
{
    CGS_ASSERT(lpHandler != 0, "lpHandler != NULL");
    CGS_ASSERT(lpcMessage != 0, "lpMessage != NULL");
    CGS_ASSERT(luNumberOfOptions <= 2, "luOptionCount <= 2");
    CGS_ASSERT(muOptionCount == 0, "A message with options is already being shown");

    if (mpMessageDisplay != 0 && mpOptionHandler != 0)
    {
        mpMessageDisplay->HideMessage();
    }

    muOptionCount   = luNumberOfOptions;
    mpOptionHandler = lpHandler;

    if (mpMessageDisplay != 0)
    {
        mpMessageDisplay->ShowMessage(lpcMessage, luNumberOfOptions, lpacOptions);
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::ShowNoSpaceMessage  @ 0x824EFA08  (BrnGuiProfile.cpp:1146)
//   As ShowMessage, but the no-space prompt carries no options.
// ---------------------------------------------------------------------------
void ProfileManager::ShowNoSpaceMessage(CgsGui::MessageDisplay::OptionHandler* lpHandler,
                                        const char* lpcMessage, u32 luRequiredKb, u32 luFreeKb)
{
    CGS_ASSERT(lpHandler != 0, "lpHandler != NULL");
    CGS_ASSERT(lpcMessage != 0, "lpMessage != NULL");
    CGS_ASSERT(muOptionCount == 0, "A message with options is already being shown");

    if (mpMessageDisplay != 0 && mpOptionHandler != 0)
    {
        mpMessageDisplay->HideMessage();
    }

    muOptionCount   = 0;
    mpOptionHandler = lpHandler;

    if (mpMessageDisplay != 0)
    {
        mpMessageDisplay->ShowNoSpaceMessage(lpcMessage, luRequiredKb, luFreeKb);
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::HideMessage  @ 0x824EFB60  (BrnGuiProfile.cpp:1175)
// ---------------------------------------------------------------------------
void ProfileManager::HideMessage()
{
    CGS_ASSERT(mpOptionHandler != 0, "SaveLoad::HideMessage: a message is not being shown");

    mpOptionHandler = 0;
    muOptionCount   = 0;

    if (mpMessageDisplay != 0)
    {
        mpMessageDisplay->HideMessage();
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::ShowAutosaveIcon  @ 0x82511038  (BrnGuiProfile.cpp:1193)
//   Post the icon flag to the module (event 355) and mirror it into the cache.
// ---------------------------------------------------------------------------
void ProfileManager::ShowAutosaveIcon(bool lbVisible)
{
    const u8 lu8Visible = lbVisible ? 1 : 0;
    mEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Visible),
                         KI_EVENT_AUTOSAVE_ICON, 1);

    GuiCache_SetAutosaveIconVisible(mpGuiCache, lbVisible);
}

// ---------------------------------------------------------------------------
// ProfileManager::HandleOption  @ 0x824EF7F0  (BrnGuiProfile.cpp:1066)
//   OptionHandler override (the user answered a prompt this manager registered):
//   hide the prompt, dispatch the pending option callback.
// ---------------------------------------------------------------------------
void ProfileManager::HandleOption(u32 luOption)
{
    HideMessage();   // X360: virtual dispatch through the MessageDisplay base

    CGS_ASSERT(mpOptionFunc != 0, "mpOptionFunc");
    (this->*mpOptionFunc)(luOption);
}

// ---------------------------------------------------------------------------
// ProfileManager::HandleWarningOption  (BrnGuiProfile.cpp:1079; the X360 body is
// ICF-folded with CheckDiskSpaceResult @0x824EF880 -- both reduce to a report).
// ---------------------------------------------------------------------------
void ProfileManager::HandleWarningOption(u32 /*luOption*/)
{
    ReportTaskCompleted();
}

// ===========================================================================
// ProfileManager -- SystemUserProfile::Listener overrides
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::UserNameChanged  @ 0x827E0DE0  (BrnGuiProfile.h:712)
// ---------------------------------------------------------------------------
void ProfileManager::UserNameChanged(const char* lpcUserName)
{
    using namespace CgsDev;
    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint << "User name changed: "
                           << (lpcUserName != 0 ? lpcUserName : "<NULLSTRING>") << "\n";
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::ProfileSettingsChanged  @ 0x827E0E98  (BrnGuiProfile.h:722)
// ---------------------------------------------------------------------------
void ProfileManager::ProfileSettingsChanged(const CgsGui::SystemUserProfile::ProfileSettings& lrSettings)
{
    using namespace CgsDev;
    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint << "Profile settings changed: external camera: "
                           << (lrSettings.mbExternalCamera ? "yes" : "no") << "\n";
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::SigninStateChanged  @ 0x827E2F40  (BrnGuiProfile.h:732)
//   Sign-in: hand the user index to the save/load system + notify the GUI (87).
//   Sign-out (only if a user was bound): drop autosave, unbind the save/load
//   system's user, notify the GUI (86), reset the profile watcher.
// ---------------------------------------------------------------------------
void ProfileManager::SigninStateChanged(CgsGui::SystemUserProfile::EUserSigninState leState)
{
    using namespace CgsDev;
    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint << "Signin state changed: "
                           << (leState == CgsGui::SystemUserProfile::E_USERSIGNINSTATE_SIGNEDIN
                                   ? "signed-in" : "not signed-in") << "\n";
    }

    if (leState == CgsGui::SystemUserProfile::E_USERSIGNINSTATE_SIGNEDIN)
    {
        SaveLoadSystem_SetSignedInUserIndex(
            mSaveLoadSystem, static_cast<s32>(SystemUserProfile_GetUserIndex(mpSystemUserProfile)));

        if (GuiCache_SignedInEventWanted(mpGuiCache))
        {
            PostByteEvent(mEventQueue, KI_EVENT_USER_SIGNED_IN);
        }
    }
    else if (SaveLoadSystem_GetSignedInUserIndex(mSaveLoadSystem) >= 0)
    {
        mbAutosaveEnabled = false;
        SaveLoadSystem_SetSignedInUserIndex(mSaveLoadSystem, -1);
        PostByteEvent(mEventQueue, KI_EVENT_USER_SIGNED_OUT);
        SystemUserProfile_ClearSignedInUser(mpSystemUserProfile);
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::UIClosed  @ 0x827E0F70  (BrnGuiProfile.h:766)
//   The system UI closed: let the save/load system run its pending continuation.
// ---------------------------------------------------------------------------
void ProfileManager::UIClosed()
{
    SaveLoadSystem_NotifyUIClosed(mSaveLoadSystem);
}

// ===========================================================================
// ProfileManager -- live profile bindings
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::SetProgressionProfile  @ 0x824ECC98  (BrnGuiProfile.h:632)
// ---------------------------------------------------------------------------
void ProfileManager::SetProgressionProfile(BrnProgression::Profile* lpProgressionProfile,
                                           const BrnProgression::ProgressionData* lpProgressionData)
{
    CGS_ASSERT(lpProgressionProfile != 0, "lpProgressionProfile != NULL");

    mpProgressionProfile = lpProgressionProfile;
    mpProgressionData    = lpProgressionData;
}

// ---------------------------------------------------------------------------
// ProfileManager::SetLiveRevengeProfile  (BrnGuiProfile.h:650; X360 inline at
// GuiModule::Update event 351, storing manager+267320).
// ---------------------------------------------------------------------------
void ProfileManager::SetLiveRevengeProfile(BrnNetwork::LiveRevengeProfile* lpLiveRevengeProfile)
{
    CGS_ASSERT(lpLiveRevengeProfile != 0, "lpLiveRevengeProfile != NULL");

    mpLiveRevengeProfile = lpLiveRevengeProfile;
}

// ===========================================================================
// ProfileManager -- the collision-world swap handshake
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::PendingCollisionWorldInvalidate  (BrnGuiProfile.cpp:149; X360
// inline in GuiModule::UpdateCollisionWorldValidation @0x82519578: 1 -> 2 latch).
// ---------------------------------------------------------------------------
bool ProfileManager::PendingCollisionWorldInvalidate()
{
    if (meCollisionWorldState == E_COLLISIONWORLDSTATE_INVALIDATE)
    {
        meCollisionWorldState = E_COLLISIONWORLDSTATE_INVALIDATING;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ProfileManager::PendingCollisionWorldValidate  (BrnGuiProfile.cpp:165; X360
// inline in the same module updater: 4 -> 5 latch).
// ---------------------------------------------------------------------------
bool ProfileManager::PendingCollisionWorldValidate()
{
    if (meCollisionWorldState == E_COLLISIONWORLDSTATE_VALIDATE)
    {
        meCollisionWorldState = E_COLLISIONWORLDSTATE_VALIDATING;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ProfileManager::SetCollisionWorldValid  @ 0x82517F50  (BrnGuiProfile.cpp:181)
//   The module finished a swap: INVALIDATING -> INVALID starts the storage
//   boot-up (the world memory is now free); VALIDATING -> VALID finishes the
//   boot-up task.
// ---------------------------------------------------------------------------
void ProfileManager::SetCollisionWorldValid(bool lbValid)
{
    if (lbValid)
    {
        if (meCollisionWorldState == E_COLLISIONWORLDSTATE_VALIDATING)
        {
            meCollisionWorldState = E_COLLISIONWORLDSTATE_VALID;
            ReportTaskCompleted();
        }
    }
    else if (meCollisionWorldState == E_COLLISIONWORLDSTATE_INVALIDATING)
    {
        meCollisionWorldState = E_COLLISIONWORLDSTATE_INVALID;
        // X360 0x82517F98 tail-call: CgsGui::SaveLoadSystem::Bootup(&mSaveLoadSystem, this,
        // &mMetadata, mbAutoLoad). This shows the SAVELOAD_AUTOSAVE_WARNING prompt and BLOCKS the
        // boot-up task until the user answers CONTINUE (SaveLoadSystem::BootupStart then reports the
        // result, which lands in ProfileManager::BootupResult). The prior fabricated
        // SaveLoadSystem_Bootup leaf reported SUCCESS synchronously right here, so the prompt never
        // showed and BF_PROFILE flashed straight through into the BF_COMPLOAD intro video.
        mSaveLoadSystem.Bootup(this, &mMetadata, mbAutoLoad);
    }
}

// ===========================================================================
// ProfileManager -- the task machine
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::StartTask  (BrnGuiProfile.cpp:1255; the X360 inlines this
// bind-and-announce into every task starter).
// ---------------------------------------------------------------------------
void ProfileManager::StartTask(TaskResultFunc lpTaskResultFunc,
                               ProfileTaskResultHandler* lpTaskResultHandler)
{
    mpTaskResultHandler = lpTaskResultHandler;
    mpTaskResultFunc    = lpTaskResultFunc;
    PostByteEvent(mEventQueue, KI_EVENT_PROFILE_TASK_STARTED);
}

// ---------------------------------------------------------------------------
// ProfileManager::HandleSaveLoadTaskResult  @ 0x824EF7A0  (BrnGuiProfile.cpp:838)
//   The save/load system finished: consume the pending callback and dispatch it.
// ---------------------------------------------------------------------------
void ProfileManager::HandleSaveLoadTaskResult(CgsGui::ESaveLoadTaskResult leResult)
{
    const TaskResultFunc lpTaskResultFunc = mpTaskResultFunc;
    mpTaskResultFunc = 0;
    (this->*lpTaskResultFunc)(leResult);
}

// ---------------------------------------------------------------------------
// ProfileManager::CheckDiskSpace  @ 0x82523CA0  (BrnGuiProfile.cpp:238)
//   The X360 build does not implement the disk-space task: it binds the task
//   state, then unconditionally fires. Reproduced verbatim.
// ---------------------------------------------------------------------------
void ProfileManager::CheckDiskSpace(ProfileTaskResultHandler* lpTaskResultHandler)
{
    CGS_ASSERT(lpTaskResultHandler != 0, "lpTaskResultHandler != NULL");
    CGS_ASSERT(!TaskIsRunning(), "!TaskIsRunning()");

    mpTaskResultHandler   = lpTaskResultHandler;
    mpTaskResultFunc      = &ProfileManager::CheckDiskSpaceResult;
    mMetadata.mStoredData = CgsGui::MakeOpaqueBuffer(mStoredData);

    CGS_ASSERT(false, "Should never get here");
}

// ---------------------------------------------------------------------------
// ProfileManager::CheckDiskSpaceResult  @ 0x824EF880  (BrnGuiProfile.cpp:261)
// ---------------------------------------------------------------------------
void ProfileManager::CheckDiskSpaceResult(CgsGui::ESaveLoadTaskResult /*leResult*/)
{
    ReportTaskCompleted();
}

// ---------------------------------------------------------------------------
// ProfileManager::Bootup  @ 0x82513870  (BrnGuiProfile.cpp:272)
//   Serialise the live profiles, bind the boot-up task, then park the collision
//   world at INVALIDATE -- the module's swap dance performs the storage work
//   (see SetCollisionWorldValid).
// ---------------------------------------------------------------------------
void ProfileManager::Bootup(ProfileTaskResultHandler& lrTaskResultHandler, bool lbAutoLoad)
{
    CGS_ASSERT(!TaskIsRunning(), "!TaskIsRunning()");

    mbAutoLoad        = lbAutoLoad;
    mbAutosaveEnabled = false;

    ReadProfileData();

    mMetadata.mStoredData = CgsGui::MakeOpaqueBuffer(mStoredData);
    StartTask(&ProfileManager::BootupResult, &lrTaskResultHandler);

    meCollisionWorldState = E_COLLISIONWORLDSTATE_INVALIDATE;
}

// ---------------------------------------------------------------------------
// ProfileManager::BootupResult  @ 0x82510F80  (BrnGuiProfile.cpp:295)
//   SUCCESS: autosave on (and PROFILE_LOADED when auto-loading). CANCELLED:
//   notify the GUI (89). FAILURE: adopt the save/load system's autosave word.
//   Always: ask the module to re-validate the collision world.
// ---------------------------------------------------------------------------
void ProfileManager::BootupResult(CgsGui::ESaveLoadTaskResult leResult)
{
    CGS_ASSERT(!mbAutosaveEnabled, "!mbAutosaveEnabled");

    if (leResult == CgsGui::E_SAVELOADTASKRESULT_SUCCESS)
    {
        if (mbAutoLoad)
        {
            miSaveLoadResult = E_SAVELOADRESULT_PROFILE_LOADED;
        }
        mbAutosaveEnabled = true;
    }
    else if (leResult == CgsGui::E_SAVELOADTASKRESULT_CANCELLED)
    {
        PostByteEvent(mEventQueue, KI_EVENT_BOOTUP_CANCELLED);
    }
    else
    {
        mbAutosaveEnabled = SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem);
    }

    meCollisionWorldState = E_COLLISIONWORLDSTATE_VALIDATE;
}

// ---------------------------------------------------------------------------
// ProfileManager::Autosave  @ 0x82513958  (BrnGuiProfile.cpp:334)
// ---------------------------------------------------------------------------
void ProfileManager::Autosave(ProfileTaskResultHandler* lpTaskResultHandler,
                              s32 liNumberOfImageFiles, const CgsGui::ImageFileInfo* laImageFileInfo)
{
    CGS_ASSERT(SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem), "mSaveLoadSystem.AutosaveIsEnabled()");

    if (TaskIsRunning())
    {
        // The X360 buffers a single image when the (first) record names a real slot.
        if (laImageFileInfo != 0 && laImageFileInfo->IsValid())
        {
            BufferProfileOperation(E_PROFILEOPERATION_AUTOSAVE, lpTaskResultHandler, 1, laImageFileInfo);
        }
        else
        {
            BufferProfileOperation(E_PROFILEOPERATION_AUTOSAVE, lpTaskResultHandler, 0, 0);
        }
        return;
    }

    mSaveInfo.macDescription[0] = '\0';
    ReadProfileData();

    miNumberOfImageFiles  = liNumberOfImageFiles;
    mMetadata.mStoredData = CgsGui::MakeOpaqueBuffer(mStoredData);

    if (liNumberOfImageFiles > 0)
    {
        std::memcpy(maImageFileInfo, laImageFileInfo,
                    static_cast<usize>(liNumberOfImageFiles) * sizeof(CgsGui::ImageFileInfo));
        for (s32 liIndex = 0; liIndex < miNumberOfImageFiles; ++liIndex)
        {
            SetMugshotBufferFromCircularBuffer(&maImageFileInfo[liIndex], &laImageFileInfo[liIndex]);
        }
    }

    StartTask(&ProfileManager::AutosaveResult, lpTaskResultHandler);

    SaveLoadSystem_Autosave(mSaveLoadSystem, *this, mMetadata, mSaveInfo,
                            miNumberOfImageFiles, maImageFileInfo);
}

// ---------------------------------------------------------------------------
// ProfileManager::AutosaveResult  @ 0x824FF250  (BrnGuiProfile.cpp:384)
//   A failure with autosave up but storage down shows the "autosave disabled"
//   prompt (the report happens when the prompt is acknowledged).
// ---------------------------------------------------------------------------
void ProfileManager::AutosaveResult(CgsGui::ESaveLoadTaskResult leResult)
{
    if (leResult != CgsGui::E_SAVELOADTASKRESULT_SUCCESS)
    {
        if (!mbAutosaveEnabled || SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem))
        {
            ReportTaskCompleted();
        }
        else
        {
            mbAutosaveEnabled = false;
            ShowAutosaveDisabledMessage();
        }
    }
    else
    {
        miSaveLoadResult = E_SAVELOADRESULT_PROFILE_AND_IMAGE_FILES_SAVED;
        ReportTaskCompleted();
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::Save  @ 0x82513B28  (BrnGuiProfile.cpp:463)
// ---------------------------------------------------------------------------
void ProfileManager::Save(ProfileTaskResultHandler& lrTaskResultHandler)
{
    if (TaskIsRunning())
    {
        BufferProfileOperation(E_PROFILEOPERATION_SAVE, &lrTaskResultHandler, 0, 0);
        return;
    }

    mSaveInfo.macDescription[0] = '\0';
    ReadProfileData();

    mMetadata.mStoredData = CgsGui::MakeOpaqueBuffer(mStoredData);
    StartTask(&ProfileManager::SaveResult, &lrTaskResultHandler);

    SaveLoadSystem_Save(mSaveLoadSystem, *this, mMetadata, mSaveInfo);
}

// ---------------------------------------------------------------------------
// ProfileManager::SaveResult  @ 0x824F8E98  (BrnGuiProfile.cpp:487)
//   A successful manual save with autosave down but storage up re-enables
//   autosave (prompting); a failure with storage down disables it (prompting).
// ---------------------------------------------------------------------------
void ProfileManager::SaveResult(CgsGui::ESaveLoadTaskResult leResult)
{
    if (leResult == CgsGui::E_SAVELOADTASKRESULT_SUCCESS)
    {
        if (!mbAutosaveEnabled && SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem))
        {
            mbAutosaveEnabled = true;
            ShowAutosaveEnabledMessage();
            return;
        }
        ReportTaskCompleted();
        return;
    }

    if (!mbAutosaveEnabled || SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem))
    {
        ReportTaskCompleted();
        return;
    }

    mbAutosaveEnabled = false;
    ShowAutosaveDisabledMessage();
}

// ---------------------------------------------------------------------------
// ProfileManager::Load  @ 0x82523D78  (BrnGuiProfile.cpp:523)
// ---------------------------------------------------------------------------
void ProfileManager::Load(ProfileTaskResultHandler& lrTaskResultHandler)
{
    if (TaskIsRunning())
    {
        BufferProfileOperation(E_PROFILEOPERATION_LOAD, &lrTaskResultHandler, 0, 0);
        return;
    }

    miSaveLoadResult      = E_SAVELOADRESULT_INVALID;
    mMetadata.mStoredData = CgsGui::MakeOpaqueBuffer(mStoredData);
    StartTask(&ProfileManager::LoadResult, &lrTaskResultHandler);

    mSaveLoadSystem.Load(this, &mMetadata);
}

// ---------------------------------------------------------------------------
// ProfileManager::LoadResult  @ 0x82517FB8  (BrnGuiProfile.cpp:545)
//   A successful load with autosave down but storage up re-enables autosave
//   (prompting); a failure with storage down disables it (prompting).
// ---------------------------------------------------------------------------
void ProfileManager::LoadResult(CgsGui::ESaveLoadTaskResult leResult)
{
    if (leResult == CgsGui::E_SAVELOADTASKRESULT_SUCCESS)
    {
        miSaveLoadResult = E_SAVELOADRESULT_PROFILE_LOADED;

        if (!mbAutosaveEnabled && SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem))
        {
            mbAutosaveEnabled = true;
            ShowAutosaveEnabledMessage();
            return;
        }
        ReportTaskCompleted();
        return;
    }

    if (!mbAutosaveEnabled || SaveLoadSystem_AutosaveIsEnabled(mSaveLoadSystem))
    {
        ReportTaskCompleted();
        return;
    }

    mbAutosaveEnabled = false;
    ShowAutosaveDisabledMessage();
}

// ---------------------------------------------------------------------------
// ProfileManager::LoadImageFiles  @ 0x82513C00  (BrnGuiProfile.cpp:587)
//   Count/validate the requested records, point each at a circular-buffer slot,
//   then start the image-load task.
// ---------------------------------------------------------------------------
void ProfileManager::LoadImageFiles(ProfileTaskResultHandler* lpTaskResultHandler,
                                    const CgsGui::ImageFileInfo* laImageFileInfo)
{
    CGS_ASSERT(lpTaskResultHandler != 0, "lpTaskResultHandler != NULL");
    CGS_ASSERT(laImageFileInfo != 0, "laImageFileInfo != NULL");

    s32 liNumberOfImageFiles = 0;
    for (s32 liIndex = 0; liIndex < CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION; ++liIndex)
    {
        if (laImageFileInfo[liIndex].IsValid())
        {
            ++liNumberOfImageFiles;
            CGS_ASSERT(laImageFileInfo[liIndex].miSize > 0, "laImageFileInfo[liIndex].miSize > 0");
            CGS_ASSERT(laImageFileInfo[liIndex].mpBuffer != 0, "laImageFileInfo[liIndex].mpBuffer != NULL");
        }
    }
    CGS_ASSERT(liNumberOfImageFiles > 0, "liNumberOfImageFiles > 0");

    if (TaskIsRunning())
    {
        BufferProfileOperation(E_PROFILEOPERATION_LOAD_IMAGES, lpTaskResultHandler,
                               liNumberOfImageFiles, laImageFileInfo);
        return;
    }

    miNumberOfImageFiles = liNumberOfImageFiles;
    std::memcpy(maImageFileInfo, laImageFileInfo, sizeof(maImageFileInfo));

    // Give each record a (cleared) circular-buffer slot to load into.
    for (s32 liIndex = 0; liIndex < miNumberOfImageFiles; ++liIndex)
    {
        SetMugshotBufferFromCircularBuffer(&maImageFileInfo[liIndex], 0);
    }

    mMetadata.mStoredData = CgsGui::MakeOpaqueBuffer(mStoredData);
    StartTask(&ProfileManager::LoadImageFilesResult, lpTaskResultHandler);

    // The X360 call also carries &mMetadata; the committed recon reads the record
    // array + count only, so the metadata rides implicitly.
    mSaveLoadSystem.LoadImageFiles(this, miNumberOfImageFiles, maImageFileInfo);
}

// ---------------------------------------------------------------------------
// ProfileManager::LoadImageFilesResult  @ 0x824EF748  (BrnGuiProfile.cpp:639)
//   On FAILURE every requested record is marked corrupt before the report.
// ---------------------------------------------------------------------------
void ProfileManager::LoadImageFilesResult(CgsGui::ESaveLoadTaskResult leResult)
{
    miSaveLoadResult = E_SAVELOADRESULT_IMAGE_FILES_LOADED;

    if (leResult == CgsGui::E_SAVELOADTASKRESULT_FAILURE)
    {
        for (s32 liIndex = 0; liIndex < miNumberOfImageFiles; ++liIndex)
        {
            maImageFileInfo[liIndex].mbIsCorrupt = true;
        }
    }

    ReportTaskCompleted();
}

// ---------------------------------------------------------------------------
// ProfileManager::BufferProfileOperation  @ 0x824F8EF0  (BrnGuiProfile.cpp:854)
//   Queue one operation (and up to KI_MAX_IMAGE_FILES_PER_OPERATION images)
//   behind the live task; ReportTaskCompleted replays it.
// ---------------------------------------------------------------------------
void ProfileManager::BufferProfileOperation(EProfileOperation leProfileOperation,
                                            ProfileTaskResultHandler* lpTaskResultHandler,
                                            s32 liNumberOfImageFiles,
                                            const CgsGui::ImageFileInfo* laImageFileInfoArray)
{
    CGS_ASSERT(TaskIsRunning() == true, "TaskIsRunning() == true");
    CGS_ASSERT(meBufferedOperation == leProfileOperation || meBufferedOperation == E_PROFILEOPERATION_NONE,
               "meBufferedOperation == leProfileOperation || meBufferedOperation == E_PROFILEOPERATION_NONE");

    if (meBufferedOperation == E_PROFILEOPERATION_NONE)
    {
        meBufferedOperation          = leProfileOperation;
        miBufferedNumberOfImageFiles = 0;
        mpBufferedTaskResultHandler  = lpTaskResultHandler;
        for (s32 liIndex = 0; liIndex < CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION; ++liIndex)
        {
            maBufferedImageFileInfo[liIndex].Construct();
        }
    }

    if (liNumberOfImageFiles > 0)
    {
        CGS_ASSERT(laImageFileInfoArray != 0, "laImageFileInfoArray != NULL");
        CGS_ASSERT(miBufferedNumberOfImageFiles + liNumberOfImageFiles <= CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION,
                   "miBufferedNumberOfImageFiles + liNumberOfImageFiles <= CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION");

        std::memcpy(&maBufferedImageFileInfo[miBufferedNumberOfImageFiles], laImageFileInfoArray,
                    static_cast<usize>(liNumberOfImageFiles) * sizeof(CgsGui::ImageFileInfo));

        for (s32 liIndex = 0; liIndex < liNumberOfImageFiles; ++liIndex)
        {
            CgsGui::ImageFileInfo* lpDestImageFileInfo =
                &maBufferedImageFileInfo[miBufferedNumberOfImageFiles + liIndex];

            if (meBufferedOperation == E_PROFILEOPERATION_LOAD_IMAGES)
            {
                // Loads get a cleared slot to land in (the X360 inlines the
                // null-source arm of SetMugshotBufferFromCircularBuffer here).
                SetMugshotBufferFromCircularBuffer(lpDestImageFileInfo, 0);
            }
            else
            {
                SetMugshotBufferFromCircularBuffer(lpDestImageFileInfo, &laImageFileInfoArray[liIndex]);
            }
        }

        miBufferedNumberOfImageFiles += liNumberOfImageFiles;
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::ReportTaskCompleted  @ 0x82513EC0  (BrnGuiProfile.cpp:909)
//   Fan out the finished task's events, call the completion handler, close the
//   task, then replay any buffered operation.
// ---------------------------------------------------------------------------
void ProfileManager::ReportTaskCompleted()
{
    switch (miSaveLoadResult)
    {
        case E_SAVELOADRESULT_PROFILE_LOADED:
        {
            if (ValidateProfiles())
            {
                // Deserialise the stored image into the live progression profile
                // (with the optional PROFILEUPG table -- disabled on PC, see the leaf).
                const void* lpUpgradeData = 0;
                if (ProfileUpgradeFeatureIsEnabled())
                {
                    void* lpResource = 0;
                    mProfileUpgradeResourcePtr.GetResource(&lpResource);
                    lpUpgradeData = lpResource;
                }
                ProgressionProfile_Deserialise(mpProgressionProfile,
                                               mStoredData.mProgressionProfile.maData,
                                               lpUpgradeData,
                                               &mStoredData.mProgressionProfileDLC1);

                BrnProgression::Profile* lpProgressionProfile = mpProgressionProfile;
                mEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lpProgressionProfile),
                                     KI_EVENT_PROGRESSION_LOADED, sizeof(lpProgressionProfile));

                // The live-revenge segment is a straight serialised-image copy.
                std::memcpy(mpLiveRevengeProfile, mStoredData.mLiveRevengeProfile.maData,
                            KI_LIVEREVENGE_PROFILE_SIZE_BYTES);
                BrnNetwork::LiveRevengeProfile* lpLiveRevengeProfile = mpLiveRevengeProfile;
                mEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lpLiveRevengeProfile),
                                     KI_EVENT_LIVEREVENGE_LOADED, sizeof(lpLiveRevengeProfile));

                reinterpret_cast<OptionsDataProfile*>(mStoredData.mOptionsDataProfile.maData)
                    ->NotifyOtherModulesLoadComplete(
                        reinterpret_cast<GuiEventQueueSmall*>(&mEventQueue));

                PostByteEvent(mEventQueue, KI_EVENT_PROFILE_LOAD_COMPLETE);

                GuiCache_MirrorLicenceFlagFromProfile(mpGuiCache, mpProgressionProfile);
            }
            break;
        }

        case E_SAVELOADRESULT_PROFILE_AND_IMAGE_FILES_SAVED:
        {
            struct AutosaveImagesRecord
            {
                s32                   miNumberOfImageFiles;
                CgsGui::ImageFileInfo maImageFileInfo[CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION];
            } lRecord;
            lRecord.miNumberOfImageFiles = miNumberOfImageFiles;
            std::memcpy(lRecord.maImageFileInfo, maImageFileInfo, sizeof(lRecord.maImageFileInfo));
            mEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                 KI_EVENT_AUTOSAVE_IMAGES_SAVED, sizeof(lRecord));
            break;
        }

        case E_SAVELOADRESULT_IMAGE_FILES_LOADED:
        {
            // Resolve each framed record: corruption check, then expose the picture
            // payload (the record's buffer pointed at the whole framed slot).
            for (s32 liIndex = 0; liIndex < miNumberOfImageFiles; ++liIndex)
            {
                CgsGui::ImageFileInfo* lpImageFileInfo = &maImageFileInfo[liIndex];
                if (lpImageFileInfo->IsValid())
                {
                    MugshotSaveBuffer* lpSlot =
                        reinterpret_cast<MugshotSaveBuffer*>(lpImageFileInfo->mpBuffer);
                    lpImageFileInfo->mbIsCorrupt = lpSlot->IsCorrupt();
                    lpImageFileInfo->miSize      = lpSlot->GetBufferLength();
                    lpImageFileInfo->mpBuffer    = lpSlot->GetBuffer();
                }
            }

            CgsGui::ImageFileInfo laRecord[CgsGui::KI_MAX_IMAGE_FILES_PER_OPERATION];
            std::memcpy(laRecord, maImageFileInfo, sizeof(laRecord));
            mEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(laRecord),
                                 KI_EVENT_IMAGE_FILES_LOADED, sizeof(laRecord));
            break;
        }

        case E_SAVELOADRESULT_EXPORT_IMAGE:
        {
            GuiOverlayWaitFinishRequest lRequest;
            lRequest.Construct("LineUpExport");
            mEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest),
                                 KI_EVENT_OVERLAY_WAIT_FINISH, sizeof(lRequest));
            break;
        }

        default:
            break;
    }

    // Consume the handler, announce completion, close the task.
    ProfileTaskResultHandler* lpTaskResultHandler = mpTaskResultHandler;
    mpTaskResultHandler = 0;
    lpTaskResultHandler->HandleProfileTaskResult();

    PostByteEvent(mEventQueue, KI_EVENT_PROFILE_TASK_FINISHED);
    miSaveLoadResult     = E_SAVELOADRESULT_INVALID;
    miNumberOfImageFiles = 0;

    // Replay the buffered operation (if any).
    switch (meBufferedOperation)
    {
        case E_PROFILEOPERATION_LOAD:
            meBufferedOperation = E_PROFILEOPERATION_NONE;
            Load(*mpBufferedTaskResultHandler);
            break;
        case E_PROFILEOPERATION_SAVE:
            meBufferedOperation = E_PROFILEOPERATION_NONE;
            Save(*mpBufferedTaskResultHandler);
            break;
        case E_PROFILEOPERATION_AUTOSAVE:
            meBufferedOperation = E_PROFILEOPERATION_NONE;
            Autosave(mpBufferedTaskResultHandler, miBufferedNumberOfImageFiles, maBufferedImageFileInfo);
            break;
        case E_PROFILEOPERATION_LOAD_IMAGES:
            meBufferedOperation = E_PROFILEOPERATION_NONE;
            LoadImageFiles(mpBufferedTaskResultHandler, maBufferedImageFileInfo);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::ShowAutosaveEnabledMessage  @ 0x824F8D70  (BrnGuiProfile.cpp:415)
// ProfileManager::ShowAutosaveDisabledMessage @ 0x824F8E00  (BrnGuiProfile.cpp:431)
//   One-option confirm prompts routed back through HandleWarningOption; silent
//   mode answers immediately.
// ---------------------------------------------------------------------------
void ProfileManager::ShowAutosaveEnabledMessage()
{
    mpOptionFunc = &ProfileManager::HandleWarningOption;
    ShowMessage(this, maMessages[0], maConfirmChoiceOptions, 1);

    if (mbSilentMode)
    {
        HandleMessageChoice(0);
    }
}

void ProfileManager::ShowAutosaveDisabledMessage()
{
    mpOptionFunc = &ProfileManager::HandleWarningOption;
    ShowMessage(this, maMessages[1], maConfirmChoiceOptions, 1);

    if (mbSilentMode)
    {
        HandleMessageChoice(0);
    }
}

// ===========================================================================
// ProfileManager -- the stored image
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::ReadProfileData  @ 0x824FF298  (BrnGuiProfile.cpp:708)
//   Serialise every live segment into the stored image.
// ---------------------------------------------------------------------------
void ProfileManager::ReadProfileData()
{
    // The optional PROFILEUPG upgrade table (feature-gated; disabled on PC -- the
    // X360 instances mProfileUpgradeResourcePtr when the gate passes).
    const void* lpUpgradeData = 0;
    if (ProfileUpgradeFeatureIsEnabled())
    {
        void* lpResource = 0;
        mProfileUpgradeResourcePtr.GetResource(&lpResource);
        lpUpgradeData = lpResource;
    }

    ProgressionProfile_Serialise(mpProgressionProfile, mStoredData.mProgressionProfile.maData,
                                 lpUpgradeData, &mStoredData.mProgressionProfileDLC1);

    CGS_ASSERT(mpLiveRevengeProfile != 0, "mpLiveRevengeProfile");
    std::memcpy(mStoredData.mLiveRevengeProfile.maData, mpLiveRevengeProfile,
                KI_LIVEREVENGE_PROFILE_SIZE_BYTES);

    OptionsDataProfile* lpOptionsDataProfile = mpGuiCache->GetOptionsDataProfile();
    CGS_ASSERT(lpOptionsDataProfile != 0, "lpOptionsDataProfile");
    std::memcpy(mStoredData.mOptionsDataProfile.maData, lpOptionsDataProfile,
                KI_OPTIONSDATA_PROFILE_SIZE_BYTES);

    // The live DLC1 options block sits right after the live options in the cache
    // (X360 cache+76776); reached through the boundary leaf until it is named there.
    CGS_ASSERT(lpOptionsDataProfile != 0, "lpOptionsDataProfile");
    const OptionsDataProfileDLC1* lpOptionsDataProfileDLC1 =
        GuiCache_GetOptionsDataProfileDLC1(mpGuiCache);
    if (lpOptionsDataProfileDLC1 != 0)
    {
        mStoredData.mOptionsDataProfileDLC1 = *lpOptionsDataProfileDLC1;
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::ValidateProfiles  @ 0x825073A0  (BrnGuiProfile.cpp:733)
//   Validate all five stored segments; every check runs (the X360 combines
//   them with non-short-circuiting ANDs so each segment logs its own verdict).
// ---------------------------------------------------------------------------
bool ProfileManager::ValidateProfiles()
{
    // The progression save image validates against the build's expected-version
    // manifest, which the progression data leads with.
    const bool lbProgressionValid =
        reinterpret_cast<BrnGuiSaveLoad::Profile*>(mStoredData.mProgressionProfile.maData)
            ->ValidateProfile(*reinterpret_cast<const BrnGuiSaveLoad::Profile::ExpectedManifest*>(
                mpProgressionData));

    const bool lbProgressionDLC1Valid = mStoredData.mProgressionProfileDLC1.ValidateProfile();

    const bool lbOptionsValid =
        reinterpret_cast<OptionsDataProfile*>(mStoredData.mOptionsDataProfile.maData)
            ->ValidateProfile();

    const bool lbOptionsDLC1Valid = mStoredData.mOptionsDataProfileDLC1.ValidateProfile();

    const bool lbLiveRevengeValid =
        LiveRevengeProfile_ValidateProfile(mStoredData.mLiveRevengeProfile.maData);

    return lbProgressionValid & lbProgressionDLC1Valid & lbOptionsValid &
           lbOptionsDLC1Valid & lbLiveRevengeValid;
}

// ===========================================================================
// ProfileManager -- mugshot framing
// ===========================================================================

// ---------------------------------------------------------------------------
// ProfileManager::SetMugshotBufferFromCircularBuffer  @ 0x824EFC28  (cpp:1213)
//   Point a transfer record at the next circular-buffer slot; frame the source
//   picture into it, or clear the slot for a pending load.
// ---------------------------------------------------------------------------
void ProfileManager::SetMugshotBufferFromCircularBuffer(CgsGui::ImageFileInfo* lpDestImageFileInfo,
                                                        const CgsGui::ImageFileInfo* lpSourceImageFileInfo)
{
    CGS_ASSERT(lpDestImageFileInfo != 0, "lpDestImageFileInfo != NULL");

    if (lpDestImageFileInfo->miSize == KI_MUGSHOT_REQUEST_SIZE_BYTES)
    {
        return;   // already framed into a slot
    }

    MugshotSaveBuffer* lpSlot = &mpaMugshotCircularBuffer[miNextMugshotInCircularBuffer];

    lpDestImageFileInfo->miSize   = KI_MUGSHOT_REQUEST_SIZE_BYTES;
    lpDestImageFileInfo->mpBuffer = reinterpret_cast<char*>(lpSlot);

    if (lpSourceImageFileInfo != 0)
    {
        CGS_ASSERT(lpSourceImageFileInfo->miSize == KI_MUGSHOT_PICTURE_SIZE_BYTES,
                   "lpSourceImageFileInfo->miSize == KI_MUGSHOT_PICTURE_SIZE_BYTES");
        lpSlot->Construct(lpSourceImageFileInfo->mpBuffer, KI_MUGSHOT_PICTURE_SIZE_BYTES);
    }
    else
    {
        lpSlot->Construct();
    }

    miNextMugshotInCircularBuffer = (miNextMugshotInCircularBuffer + 1) % KI_MUGSHOTS_IN_CIRCULAR_BUFFER;
}

// ---------------------------------------------------------------------------
// ProfileManager::CopyImageToBuffer  @ 0x824EFD40  (X360-only; GuiModule::
// AutosaveProfile @0x8251A568 parks the mugshot when autosave is disabled)
//   Frame the picture into the circular buffer's head slot and hand a request
//   describing it to the save/load system.
// ---------------------------------------------------------------------------
s32 ProfileManager::CopyImageToBuffer(const CgsGui::ImageFileInfo* lpSourceImageFileInfo)
{
    CGS_ASSERT(lpSourceImageFileInfo != 0, "lpSourceImageFileInfo != NULL");

    MugshotSaveBuffer* lpSlot = mpaMugshotCircularBuffer;

    CgsGui::ImageFileInfo lRequest;
    lRequest.miId        = lpSourceImageFileInfo->miId;
    lRequest.miSize      = KI_MUGSHOT_REQUEST_SIZE_BYTES;
    lRequest.mpBuffer    = reinterpret_cast<char*>(lpSlot);
    lRequest.mbIsCorrupt = lpSourceImageFileInfo->mbIsCorrupt;

    CGS_ASSERT(lpSourceImageFileInfo->miSize == KI_MUGSHOT_PICTURE_SIZE_BYTES,
               "lpSourceImageFileInfo->miSize == KI_MUGSHOT_PICTURE_SIZE_BYTES");

    lpSlot->Construct(lpSourceImageFileInfo->mpBuffer, KI_MUGSHOT_PICTURE_SIZE_BYTES);

    return SaveLoadSystem_CopyImageToBuffer(mSaveLoadSystem, lRequest);
}

// ===========================================================================
// ProfileManager -- the PROFILEUPG upgrade-table load (X360-only @0x825135C8;
// GuiModule::PreWorldUpdate drives it)
// ===========================================================================
s32 ProfileManager::LoadProfileUpgradeData(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutputBuffer,
                                           CgsModule::BaseEventReceiverQueue* lpResponseQueue)
{
    // Feature gate: without the upgrade content the load reports "complete".
    if (!ProfileUpgradeFeatureIsEnabled())
    {
        return 1;
    }

    switch (meProfileUpgradeLoadStage)
    {
        case 0:
        {
            // Request the PROFILEUPG.BIN bundle load (resource request tag 2).
            CgsResource::Events::LoadBundleRequest lRequest;
            lRequest.mpUser    = lpResponseQueue;
            lRequest.miEventId = 1;
            lRequest.SetFileName("PROFILEUPG.BIN");

            lpOutputBuffer->GetGuiResourceRequestQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                KI_RESOURCE_REQUEST_LOAD_BUNDLE, sizeof(lRequest));

            meProfileUpgradeLoadStage = 1;
            return 0;
        }

        case 1:
        {
            // Wait for (then drain) the bundle-load responses.
            if (lpResponseQueue->GetLength() == 0)
            {
                return 0;
            }

            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            for (lpResponseQueue->GetFirstEvent(&lpEvent, &liSize); lpEvent != 0;
                 lpResponseQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                CGS_ASSERT(lpEvent != 0, "lpResponse");
            }
            lpResponseQueue->Clear();

            meProfileUpgradeLoadStage = 2;
            return 0;
        }

        case 2:
        {
            // Request the PROFILEUPG resource acquire (tag 4, pool 5, type-5 id).
            CgsResource::Events::AcquireResourceRequest lRequest;
            lRequest.mpUser    = lpResponseQueue;
            lRequest.miEventId = 1;
            lRequest.miPoolId  = KI_PROFILEUPG_POOL_ID;
            lRequest.mResourceId.SetHash(
                static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>("PROFILEUPG")))) |
                (static_cast<u64>(KI_PROFILEUPG_POOL_ID) << 32));
            lRequest.mbCheckRefCount = false;

            lpOutputBuffer->GetGuiResourceRequestQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest),
                KI_RESOURCE_REQUEST_ACQUIRE_RESOURCE, sizeof(lRequest));

            meProfileUpgradeLoadStage = 3;
            return 0;
        }

        case 3:
        {
            // Bind the acquired handle from each response, then finish.
            if (lpResponseQueue->GetLength() == 0)
            {
                return 0;
            }

            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            for (lpResponseQueue->GetFirstEvent(&lpEvent, &liSize); lpEvent != 0;
                 lpResponseQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                CGS_ASSERT(lpEvent != 0, "lpAcquire");
                ProfileUpgradeResource_BindFromEvent(mProfileUpgradeResourcePtr, lpEvent);
            }
            lpResponseQueue->Clear();

            meProfileUpgradeLoadStage = 4;
            return 1;
        }

        case 4:
            return 1;

        default:
            CGS_ASSERT(false, "ProgressionManager::meProfileUpgradeLoadStage in a weird state");
            return 0;
    }
}

} // namespace BrnGui
