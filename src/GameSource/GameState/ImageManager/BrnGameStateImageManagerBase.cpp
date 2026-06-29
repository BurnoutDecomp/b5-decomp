// ============================================================================
// b5-decomp/src/GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.cpp
// ============================================================================
// Bodies for BrnGameState::GameStateImageManagerBase (home: BrnGameStateImageManagerBase.h).
// Reconstructed store-for-store from the X360 ARTIST.XEX (asm authoritative; DWARF for
// names/shapes; the Feb-2007 partial source has nothing for this TU).
//
// The manager owns three on-screen gallery mugshot textures (maImageGalleryMugshots) and drives
// the in-game photo gallery: it reads the persisted mugshot records out of the player's Profile
// (mpProgression->GetProfile()) and pushes GUI / game-action records onto the OutputBuffer's
// GameActionQueue (CgsModule::VariableEventQueue<13312,16>::AddEvent).
//
// GUI / GAME-ACTION EVENT TYPE IDS (the X360 AddEvent `li r5,<imm>` immediates -- AUTHORITATIVE):
//   290 image-info record (48B)   291 gallery-count reply (8B)   292 load-complete bitfield (16B)
//   293 single empty/abort byte (1B) 294 image-save request (16B) 295 prepare-render record (48B)
//   299 show-gamercard (8B)        49  rival-hit / line-up-delete (8B)
// The DecFIGS DWARF spells these as E_ACTION_IMAGE_GALLERY_* but with PS3-DRIFTED values
// (275..279, off by 15 from the X360 immediates); the X360 immediates above win. They are named
// as local constants (not enum members against the wrong values) -- mirrors the BrnMugshotManager
// precedent (KI_GAME_ACTION_START_MUGSHOT = 213 used directly).
//
// ASSERT-PARITY NOTE: the X360 bakes the verbatim source path + exact line numbers into every
// FireAssert; CGS_ASSERT emits __FILE__/__LINE__ instead (project-wide benign assert YELLOW).

#include <cstring>   // std::memcpy / std::memset (model the X360 XMemCpy / XMemSet block intrinsics)
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsID.h"                     // CgsID, CgsIDCompress
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h" // CgsSystem::DateAndTime
#include "GameSource/GameState/Progression/BrnProfile.h"           // BrnProgression::Profile / MugshotInfo
#include "GameSource/GameState/Progression/BrnProgressionManager.h"// BrnProgression::ProgressionManager
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnMugshotAdded

namespace BrnGameState
{
namespace
{
    // --- X360-attested AddEvent type immediates (see file header) -------------------------------
    const s32 KI_EVENT_IMAGE_INFO        = 290; // 48-byte gallery image-info record
    const s32 KI_EVENT_GALLERY_COUNT     = 291; // 8-byte  count reply
    const s32 KI_EVENT_LOAD_COMPLETE     = 292; // 16-byte load-complete bitfield
    const s32 KI_EVENT_EMPTY_SLOT        = 293; // 1-byte  empty/abort marker
    const s32 KI_EVENT_IMAGE_SAVE        = 294; // 16-byte save request
    const s32 KI_EVENT_PREPARE_RENDER    = 295; // 48-byte prepare-render record
    const s32 KI_EVENT_SHOW_GAMERCARD    = 299; // 8-byte  show-gamercard
    const s32 KI_EVENT_LINEUP_DELETE     = 49;  // 8-byte  line-up-delete (== E_ACTION_RIVAL_HIT_PLAYER slot)

    const s32 KI_NUM_PICTURES = GameStateImageManagerBase::KI_IMAGE_GALLERY_NUM_PICTURES; // 3

    // EImageType -> EImageGalleryType map (X360 dword_8202AC60[6]). FULLY grounded by the two enums'
    // shared naming (DWARF BrnGameStateSharedIO.h:519-525 / :65-72): each image type maps to the
    // like-named gallery type; the two road-rule image variants both fold to the single ROAD_RULE
    // gallery. NOT fabricated -- the value at index i is the gallery enumerator of the same name.
    const GameStateModuleIO::EImageGalleryType KAE_IMAGE_TYPE_TO_GALLERY_TYPE_MAPPING[GameStateModuleIO::E_IMAGE_TYPE_COUNT] =
    {
        GameStateModuleIO::E_IMAGE_GALLERY_TYPE_FREEBURN_MUGSHOT, // E_IMAGE_TYPE_FREEBURN_MUGSHOT (0) -> 0
        GameStateModuleIO::E_IMAGE_GALLERY_TYPE_MUGSHOT,          // E_IMAGE_TYPE_MUGSHOT          (1) -> 4
        GameStateModuleIO::E_IMAGE_GALLERY_TYPE_PAYBACK_MUGSHOT,  // E_IMAGE_TYPE_PAYBACK_MUGSHOT  (2) -> 1
        GameStateModuleIO::E_IMAGE_GALLERY_TYPE_ROAD_RULE_MUGSHOT, // E_IMAGE_TYPE_ROAD_RULE_TIME_MUGSHOT  (3) -> 2
        GameStateModuleIO::E_IMAGE_GALLERY_TYPE_ROAD_RULE_MUGSHOT, // E_IMAGE_TYPE_ROAD_RULE_CRASH_MUGSHOT (4) -> 2
        GameStateModuleIO::E_IMAGE_GALLERY_TYPE_VICTORY_MUGSHOT,  // E_IMAGE_TYPE_VICTORY_MUGSHOT  (5) -> 3
    };

    // The GameActionQueue concrete type (forward-declared in BrnGameStateModuleIO.h) is the variable
    // event queue the X360 calls VariableEventQueue<13312,16>::AddEvent straight on. Bridge the
    // opaque return to that queue type (mirrors the BrnMugshotManager AsVeq precedent).
    inline CgsModule::VariableEventQueue<13312, 16>* AsVeq(GameStateModuleIO::GameActionQueue* lpQueue)
    {
        return reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(lpQueue);
    }

    // ---- the GUI event payloads the X360 builds on the stack and memcpy's into the queue --------
    // Each layout is grounded by the per-field stack stores in the X360 asm (the field offsets in
    // the comments are the BYREF stack-record offsets the X360 fills before AddEvent).

    // type 290 (48 bytes): one gallery image-info record (built from a MugshotInfo). Field offsets are
    // the X360 stack-store map of ProcessLockRequest (0x82385194..0x823851D8, base var_60) and the
    // populated branch of ProcessScrollImagesEvent (0x82384E04..0x82384E40, base var_100): the three
    // image words lead at +0x00/+0x04/+0x08, then the 8-byte date (NOT 8-aligned), word2C, the slot
    // index, and the 16-byte unique-player-id is the TRAILING field (memcpy'd to +0x1C), with the two
    // trailing flag bytes at +0x2C/+0x2D.
    // pack(1): the u64 date sits at the non-8-aligned +0x0C, so natural alignment would insert a
    // 4-byte pad and shift every later field. Byte-packing reproduces the exact X360 record bytes.
#pragma pack(push, 1)
    struct ImageInfoEvent
    {
        s32 miImageWord0;   // +0x00 (MugshotInfo +0x18; std lwz 0(r31+0x18))
        s32 miImageWord1;   // +0x04 (MugshotInfo +0x1C)
        s32 miImageWord2;   // +0x08 (MugshotInfo +0x20)
        u64 mu64DateTaken;  // +0x0C (MugshotInfo +0x24, 8B std at var_54; not 8-aligned)
        s32 miImageWord2C;  // +0x14 (MugshotInfo +0x2C)
        s32 miSlotIndex;    // +0x18 (the on-screen slot the record is for; stw from a2[1])
        BrnProgression::MugshotInfo::UniquePlayerIDImage mUniquePlayerID; // +0x1C (16B, memcpy from MugshotInfo+0)
        u8  mu8LockedFlag;  // +0x2C (MugshotInfo +0x32)
        u8  mbIsDeletePreview; // +0x2D (1 in delete preview, 0 otherwise)
        u8  maPad[0x30 - 0x2E];
    };
    static_assert(offsetof(ImageInfoEvent, mu64DateTaken) == 0x0C, "date@0x0C");
    static_assert(offsetof(ImageInfoEvent, miImageWord2C) == 0x14, "word2C@0x14");
    static_assert(offsetof(ImageInfoEvent, miSlotIndex)   == 0x18, "slot@0x18");
    static_assert(offsetof(ImageInfoEvent, mUniquePlayerID) == 0x1C, "uniqueId@0x1C");
    static_assert(offsetof(ImageInfoEvent, mu8LockedFlag) == 0x2C, "locked@0x2C");
    static_assert(sizeof(ImageInfoEvent) == 0x30, "ImageInfoEvent==48");

    // type 290 (48 bytes): an EMPTY gallery slot record (no mugshot present). Field offsets are the
    // X360 stack-store map of the empty branch of ProcessScrollImagesEvent (0x82384E7C..0x82384EA8,
    // base var_D0): three leading zero words, then COUNTY @+0x0C (DistrictToCounty(18) result),
    // DISTRICT @+0x10 (18), a zero word @+0x14, the slot index @+0x18 (NOT +0x14 -- it agrees with the
    // populated ImageInfoEvent's +0x18 slot), then the trailing flag bytes @+0x1C/+0x2C/+0x2D.
    struct EmptySlotInfoEvent
    {
        s32 miWord00;       // +0x00 (0)
        s32 miWord04;       // +0x04 (0)
        s32 miWord08;       // +0x08 (0)
        s32 meCounty;       // +0x0C (DistrictToCounty(18); stw r3 result @var_C4)
        s32 meDistrict;     // +0x10 (18 == E_DISTRICT_COUNT "no region"; stw r22 @var_C0)
        s32 miWord14;       // +0x14 (0)
        s32 miSlotIndex;    // +0x18 (the empty slot; stw r23/r27 @var_B8)
        u8  mu8FlagA;       // +0x1C (0; stb @var_B4)
        u8  maPad1D[0x2C - 0x1D]; // +0x1D..+0x2B (the empty record's image/id region is unused)
        u8  mu8FlagB;       // +0x2C (0; stb @var_A4)
        u8  mu8FlagC;       // +0x2D (0; stb @var_A3)
        u8  maPad[0x30 - 0x2E];
    };
#pragma pack(pop)
    static_assert(offsetof(EmptySlotInfoEvent, meCounty)   == 0x0C, "county@0x0C");
    static_assert(offsetof(EmptySlotInfoEvent, meDistrict) == 0x10, "district@0x10");
    static_assert(offsetof(EmptySlotInfoEvent, miSlotIndex) == 0x18, "slot@0x18");
    static_assert(offsetof(EmptySlotInfoEvent, mu8FlagB)   == 0x2C, "flagB@0x2C");
    static_assert(offsetof(EmptySlotInfoEvent, mu8FlagC)   == 0x2D, "flagC@0x2D");
    static_assert(sizeof(EmptySlotInfoEvent) == 0x30, "EmptySlotInfoEvent==48");

    // type 292 (16 bytes): the gallery's live-mugshot bitfield (rebuilt after a delete). Field offsets
    // are the X360 stack-store map of ProcessDeleteRequest (0x82385660/0x82385668, base var_E0): the
    // gallery type word @+0x00 (stw r10), then the 8-byte live-bit accumulator @+0x08 (std r21) -- the
    // bits are at +0x08, NOT +0x04. The +0x04 word is padding the asm never writes.
    struct LoadCompleteEvent
    {
        s32 meImageGalleryType; // +0x00 (stw r10 @var_E0)
        s32 miPad04;            // +0x04 (never written by the asm)
        u64 mu64LiveBits;       // +0x08 (one bit per live mugshot index; std r21 @var_D8)
    };

    // type 294 (16 bytes): a save request (file id, texture size, texture buffer ptr). The asm
    // (RequestImageSave 0x82384868) writes only +0x00/+0x04/+0x08; +0x0C is trailing pad it never
    // stores (size 16 is still passed to AddEvent).
    struct ImageSaveRequestEvent
    {
        s32   miFileID;       // +0x00
        s32   miTextureSize;  // +0x04
        char* mpTextureBuffer;// +0x08
        s32   miPad0C;        // +0x0C (never written by the asm)
    };
}

// ---------------------------------------------------------------------------
// Construct -- X360 0x8236D720. Cache the progression manager and clear every slot.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::Construct(BrnProgression::ProgressionManager* lpProgression)
{
    CGS_ASSERT(lpProgression != nullptr, "lpProgression");

    mpProgression       = lpProgression;        // this+0xB8
    miMugshotToSaveIndex = 0;                    // this+0xAC
    maImageLoadRequests.Clear();                 // this+0x80 count word -> 0
    mbLoadInProgress    = false;                 // this+0xBC
    mImagesToRenderBitArray.UnSetAll();          // this+0xB0 (std 0)

    // this+0x84: maLoadedImagesToSlotMapping[i] = 0; this+0x90: maiImagesLockedForSave[i] = -1.
    for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
    {
        maLoadedImagesToSlotMapping[liIndex] = 0;
        maiImagesLockedForSave[liIndex]      = -1;
    }

    // this+0x08: construct each gallery mugshot texture.
    for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
    {
        maImageGalleryMugshots[liIndex].Construct();
    }
}

// ---------------------------------------------------------------------------
// Destruct -- X360 0x8236D7E0. Destruct each texture and re-clear every slot.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::Destruct()
{
    for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
    {
        maImageGalleryMugshots[liIndex].Destruct();
    }

    for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
    {
        maLoadedImagesToSlotMapping[liIndex] = 0;
        maiImagesLockedForSave[liIndex]      = -1;
    }

    miMugshotToSaveIndex = 0;
    mImagesToRenderBitArray.UnSetAll();
    mbLoadInProgress     = false;
    maImageLoadRequests.Clear();
    mpProgression        = nullptr;
}

// ---------------------------------------------------------------------------
// Prepare -- X360 0x8236D858. Allocate + zero the three gallery mugshot textures.
// (X360 immediates: 160x120, format 438304850.)
// ---------------------------------------------------------------------------
bool GameStateImageManagerBase::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
{
    for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
    {
        CgsNetwork::NetworkTexture& lTexture = maImageGalleryMugshots[liIndex];
        lTexture.Prepare(lpHeapMalloc, 160, 120,
                         static_cast<renderengine::PixelFormat>(438304850));
        CGS_ASSERT(lTexture.GetTexture() != nullptr, "mpcTexture");
        lTexture.ClearPixels();
    }

    mImagesToRenderBitArray.UnSetAll(); // this+0xB0 = 0
    return true;
}

// ---------------------------------------------------------------------------
// Release -- X360 0x8236D918. Release the three gallery mugshot textures.
// ---------------------------------------------------------------------------
bool GameStateImageManagerBase::Release()
{
    mImagesToRenderBitArray.UnSetAll();

    for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
    {
        maImageGalleryMugshots[liIndex].Release();
    }
    return true;
}

// ---------------------------------------------------------------------------
// PreWorldUpdate -- X360 0x82391A08. Per-frame hook.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::PreWorldUpdate(::EActiveRaceCarIndex lePlayerRaceCarIndex,
                                               const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                                               GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpTakedownEventQueue != nullptr, "lpQueue");
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");

    UpdateTakedownEvents(lePlayerRaceCarIndex, lpTakedownEventQueue);
    UpdateNewImageRequests(lpOutput);
    UpdateImagesToRender(lpOutput);
}

// ---------------------------------------------------------------------------
// HandleWorldRegionChangeEvent -- X360 0x82357C68. Remember the current world region.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::HandleWorldRegionChangeEvent(const WorldRegionChangeEvent* lpWorldRegionChangeEvent)
{
    CGS_ASSERT(lpWorldRegionChangeEvent != nullptr, "lpWorldRegionChangeEvent");

    // v3[39]=*a2 / v3[40]=a2[1] -> meCurrentWorldRegion {county@+0x9C, district@+0xA0}.
    meCurrentWorldRegion.Construct(lpWorldRegionChangeEvent->meDistrict);
}

// ---------------------------------------------------------------------------
// HandleImageFilesSavedEvent -- X360 0x82357CD8. Clear the locked-for-save slot of each saved file.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::HandleImageFilesSavedEvent(const ImageFilesSavedEvent* lpImageFilesSavedEvent)
{
    CGS_ASSERT(lpImageFilesSavedEvent != nullptr, "lpImageFilesSavedEvent");

    for (s32 liSavedFileIndex = 0; liSavedFileIndex < lpImageFilesSavedEvent->miNumSavedFiles; ++liSavedFileIndex)
    {
        // Each saved entry strides 4 words; the file id is the entry's leading word.
        const s32 liSavedFileID = lpImageFilesSavedEvent->maSavedFileInfos[liSavedFileIndex * 4];
        CGS_ASSERT(liSavedFileID >= 0, "liSavedFileID >= 0");

        for (s32 liSlotIndex = 0; liSlotIndex < KI_NUM_PICTURES; ++liSlotIndex)
        {
            if (maiImagesLockedForSave[liSlotIndex] == liSavedFileID)
            {
                maiImagesLockedForSave[liSlotIndex] = -1;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateTakedownEvents -- X360 0x82363E80. Cache the region of the latest takedown we star in.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::UpdateTakedownEvents(::EActiveRaceCarIndex lePlayerRaceCarIndex,
                                                     const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue)
{
    const s32 liNumEvents = static_cast<s32>(lpTakedownEventQueue->GetLength());
    for (s32 liEventIndex = 0; liEventIndex < liNumEvents; ++liEventIndex)
    {
        const TakedownEvent& lrTakedownEvent = lpTakedownEventQueue->GetEvent(liEventIndex);
        const ::EActiveRaceCarIndex leAggressor =
            static_cast<::EActiveRaceCarIndex>(lrTakedownEvent.meAggressorIndex);

        CGS_ASSERT(leAggressor != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "leTakedownAggressorRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");
        CGS_ASSERT(leAggressor >= ::E_ACTIVE_RACE_CAR_INDEX_0 && leAggressor < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leTakedownAggressorRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && leTakedownAggressorRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        // We are the aggressor of this takedown -> remember where it happened.
        if (lePlayerRaceCarIndex == leAggressor)
        {
            meWorldRegionAtLastTakedown = meCurrentWorldRegion;
        }
    }
}

// ---------------------------------------------------------------------------
// HandleImageGalleryCountRequest -- X360 0x82385E78. Reply with how many mugshots of a type exist.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::HandleImageGalleryCountRequest(const ImageGalleryCountRequestEvent* lpImageGalleryCountReqEvent,
                                                               GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpImageGalleryCountReqEvent != nullptr, "lpImageGalleryCountReqEvent");
    CGS_ASSERT(lpImageGalleryCountReqEvent->meImageGalleryImageType < GameStateModuleIO::E_IMAGE_GALLERY_TYPE_COUNT,
               "lpImageGalleryCountReqEvent->meImageGalleryImageType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT");
    CGS_ASSERT(mpProgression != nullptr, "mpProgression");

    struct CountReply { s32 meImageGalleryType; s32 miCount; } lReply;
    lReply.meImageGalleryType = lpImageGalleryCountReqEvent->meImageGalleryImageType;
    lReply.miCount            = mpProgression->GetProfile()->GetNumMugshots(lpImageGalleryCountReqEvent->meImageGalleryImageType);

    AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lReply), KI_EVENT_GALLERY_COUNT, (s32)sizeof(lReply));
}

// ---------------------------------------------------------------------------
// HandleImageFilesLoadedEvent -- X360 0x82384548. Copy the loaded image files into the gallery slots.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::HandleImageFilesLoadedEvent(const ImageFilesLoadedEvent* lpImageFilesLoadedEvent,
                                                            GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
    CGS_ASSERT(lpImageFilesLoadedEvent != nullptr, "lpImageFilesLoadedEvent");

    // If the load request was already cancelled (the request list was emptied), just clear the flag.
    if (maImageLoadRequests.GetLength() != 0)
    {
        mbLoadInProgress = false;
        return;
    }

    for (s32 liSlotIndex = 0; liSlotIndex < KI_NUM_PICTURES; ++liSlotIndex)
    {
        const ImageFilesLoadedEvent::ImageFileInfo& lrFileInfo = lpImageFilesLoadedEvent->maFileInfos[liSlotIndex];
        if (lrFileInfo.miFileID > -1)
        {
            const s32 liDestSlot = maLoadedImagesToSlotMapping[liSlotIndex];

            CGS_ASSERT(lrFileInfo.miSize > 0, "lpImageFileInfo->miSize > 0");
            CGS_ASSERT(lrFileInfo.mpBuffer != nullptr, "lpImageFileInfo->mpBuffer != NULL");

            CgsNetwork::NetworkTexture& lDestTexture = maImageGalleryMugshots[liDestSlot];
            CGS_ASSERT(lDestTexture.GetTextureSize() == lrFileInfo.miSize,
                       "maImageGalleryMugshots[liSlotIndex].GetTextureSize() == lpImageFileInfo->miSize");

            std::memcpy(lDestTexture.GetTexture(), lrFileInfo.mpBuffer, static_cast<size_t>(lrFileInfo.miSize));

            CGS_ASSERT(static_cast<u32>(liDestSlot) < static_cast<u32>(KI_NUM_PICTURES), "Index out of range");
            mImagesToRenderBitArray.SetBit(static_cast<u32>(liDestSlot));
        }
    }

    maImageLoadRequests.Clear();

    char lEmptyMarker = 0;
    AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lEmptyMarker), KI_EVENT_EMPTY_SLOT, 1);
    mbLoadInProgress = false;
}

// ---------------------------------------------------------------------------
// HandleImageSaveEvent -- X360 0x82391AA8. Persist a captured mugshot then request a file save.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::HandleImageSaveEvent(const ImageToSaveEvent* lpImageSaveEvent,
                                                     GameStateModuleIO::OutputBuffer* lpOutput,
                                                     bool lbIsAutomaticCapture)
{
    // The X360 bails early if the next save slot is already locked (maiImagesLockedForSave >= 0).
    if (maiImagesLockedForSave[miMugshotToSaveIndex] >= 0)
    {
        return;
    }

    CGS_ASSERT(mpProgression != nullptr, "mpProgression");
    CGS_ASSERT(lpImageSaveEvent != nullptr, "lpImageSaveEvent");
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");

    const GameStateModuleIO::EImageGalleryType leImageGalleryType =
        KAE_IMAGE_TYPE_TO_GALLERY_TYPE_MAPPING[lpImageSaveEvent->meImageType];
    CGS_ASSERT(leImageGalleryType != GameStateModuleIO::E_IMAGE_GALLERY_TYPE_COUNT,
               "leImageGalleryType != GsmIO::E_IMAGE_GALLERY_TYPE_COUNT");
    CGS_ASSERT(leImageGalleryType < GameStateModuleIO::E_IMAGE_GALLERY_TYPE_COUNT,
               "leImageTypeToSave < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT");

    // Victory mugshots (and non-automatic captures) are always persisted; automatic captures of
    // other types are persisted only when not a duplicate (X360 `!a4 || v8 == 3`).
    if (!lbIsAutomaticCapture || leImageGalleryType == GameStateModuleIO::E_IMAGE_GALLERY_TYPE_VICTORY_MUGSHOT)
    {
        CgsSystem::DateAndTime lDateTaken;
        lDateTaken.SetLocal(true);
        lDateTaken.Update();

        // Victory / road-rule mugshots are tagged with the current region; others with the region
        // of the takedown that triggered them.
        const BrnWorld::WorldRegion& lrRegion =
            (leImageGalleryType == GameStateModuleIO::E_IMAGE_GALLERY_TYPE_VICTORY_MUGSHOT
             || leImageGalleryType == GameStateModuleIO::E_IMAGE_GALLERY_TYPE_ROAD_RULE_MUGSHOT)
                ? meCurrentWorldRegion
                : meWorldRegionAtLastTakedown;

        BrnProgression::Profile* lpProfile = mpProgression->GetProfile();

        BrnProgression::Profile::MugshotUniqueIdArg lUniqueID;
        lUniqueID.mu64Lo = lpImageSaveEvent->mu64UniqueIdLo;
        lUniqueID.mu64Hi = lpImageSaveEvent->mu64UniqueIdHi;

        const s32 liNewMugshotIndex = lpProfile->AddMugshot(
            static_cast<s32>(leImageGalleryType), lUniqueID, lDateTaken,
            static_cast<s32>(lrRegion.GetDistrict()));

        mpProgression->OnTrophyUnlock(34); // X360 li r4, 0x22

        const s32 liNumAllMugshots = lpProfile->GetNumAllMugshots();
        mpProgression->GetAchievementManager()->OnMugshotAdded(static_cast<u32>(liNumAllMugshots));

        if (liNewMugshotIndex >= 0)
        {
            CGS_ASSERT(lpImageSaveEvent->mpTexture != nullptr, "lpImageSaveEvent->mpTexture");
            RequestImageSave(liNewMugshotIndex, lpImageSaveEvent->mpTexture, lpOutput);
        }
    }
}

// ---------------------------------------------------------------------------
// RequestImageSave -- X360 0x82384868. Copy the texture into the next save slot + post a save action.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::RequestImageSave(s32 liFileID, CgsNetwork::NetworkTexture* lpTexture,
                                                 GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpTexture != nullptr, "lpTexture");
    CGS_ASSERT(miMugshotToSaveIndex < KI_NUM_PICTURES, "miMugshotToSaveIndex < KI_IMAGE_GALLERY_NUM_PICTURES");

    CgsNetwork::NetworkTexture& lSaveSlot = maImageGalleryMugshots[miMugshotToSaveIndex];
    CGS_ASSERT(lpTexture->GetTextureSize() == lSaveSlot.GetTextureSize(),
               "lpTexture->GetTextureSize() == maImageGalleryMugshots[miMugshotToSaveIndex].GetTextureSize()");
    CGS_ASSERT(liFileID >= 0, "liFileID >= 0");

    std::memcpy(lSaveSlot.GetTexture(), lpTexture->GetTexture(),
           static_cast<size_t>(lSaveSlot.GetTextureSize()));

    CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");

    ImageSaveRequestEvent lSaveRequest;
    lSaveRequest.miFileID        = liFileID;
    lSaveRequest.miTextureSize   = lSaveSlot.GetTextureSize();
    lSaveRequest.mpTextureBuffer = lSaveSlot.GetTexture();
    // +0x0C is left uninitialised: the X360 asm never writes that word.
    AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lSaveRequest), KI_EVENT_IMAGE_SAVE, (s32)sizeof(lSaveRequest));

    CGS_ASSERT(maiImagesLockedForSave[miMugshotToSaveIndex] < 0,
               "maiImagesLockedForSave[miMugshotToSaveIndex] < 0");
    maiImagesLockedForSave[miMugshotToSaveIndex] = liFileID;
    miMugshotToSaveIndex = (miMugshotToSaveIndex + 1) % KI_NUM_PICTURES;
}

// ---------------------------------------------------------------------------
// HandleImageGalleryRequest -- X360 0x82391C98. Dispatch a gallery UI request.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::HandleImageGalleryRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                                          GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpImageGalleryReqEvent != nullptr, "lpImageGalleryReqEvent");

    switch (lpImageGalleryReqEvent->meImageGalleryRequest)
    {
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_NEW_IMAGES:
            ProcessNewImageRequest(lpImageGalleryReqEvent);
            break;
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT:
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_LEFT:
            ProcessScrollImagesEvent(lpImageGalleryReqEvent, lpOutput);
            break;
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_ASK_DELETE:
            ProcessAskDeleteRequest(lpImageGalleryReqEvent, lpOutput);
            break;
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_DELETE:
            ProcessDeleteRequest(lpImageGalleryReqEvent, lpOutput);
            break;
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_LOCK:
            ProcessLockRequest(lpImageGalleryReqEvent, lpOutput);
            break;
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_EXPORT:
        {
            CGS_ASSERT(mpProgression != nullptr, "mpProgression");
            if (lpImageGalleryReqEvent->miImageIndex
                >= mpProgression->GetProfile()->GetNumMugshots(lpImageGalleryReqEvent->meImageGalleryImageType))
            {
                // Out of range -> reply with an empty-slot marker rather than exporting.
                CGS_ASSERT(lpOutput != nullptr, "lpOutput");
                CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
                char lEmptyMarker = 0;
                AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEmptyMarker), KI_EVENT_EMPTY_SLOT, 1);
            }
            else
            {
                // The X360 dispatches the export through vtable[0] (ProcessExportRequest is virtual).
                ProcessExportRequest(lpImageGalleryReqEvent, lpOutput);
            }
            break;
        }
        case GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SHOW_GAMERCARD:
            ProcessShowGamerCardRequest(lpImageGalleryReqEvent, lpOutput);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// ProcessShowGamerCardRequest -- X360 0x82385D00. Show the captured player's gamercard.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::ProcessShowGamerCardRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                                            GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpImageGalleryReqEvent != nullptr, "lpImageGalleryReqEvent");
    CGS_ASSERT(lpImageGalleryReqEvent->meImageGalleryRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SHOW_GAMERCARD,
               "GsmIO::E_IMAGE_GALLERY_REQUEST_SHOW_GAMERCARD == lpImageGalleryReqEvent->meImageGalleryRequest");
    CGS_ASSERT(lpImageGalleryReqEvent->miSlotIndex < KI_NUM_PICTURES,
               "lpImageGalleryReqEvent->miSlotIndex < KI_IMAGE_GALLERY_NUM_PICTURES");
    CGS_ASSERT(lpImageGalleryReqEvent->miSlotIndex >= 0, "lpImageGalleryReqEvent->miSlotIndex >= 0");
    CGS_ASSERT(mpProgression != nullptr, "mpProgression");

    BrnProgression::MugshotInfo* lpMugshotInfo = mpProgression->GetProfile()->GetMugshotInfo(
        lpImageGalleryReqEvent->meImageGalleryImageType, lpImageGalleryReqEvent->miImageIndex);
    if (lpMugshotInfo != nullptr)
    {
        // The X360 posts the 8-byte qword at MugshotInfo +0x10 (ld r11,0x10(r3); std @var_30), the
        // gamercard XUID -- NOT the unique-player-id at +0x00.
        u64 lu64GamerCardXuid = lpMugshotInfo->GetGamerCardXuid();

        CGS_ASSERT(lpOutput != nullptr, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lu64GamerCardXuid), KI_EVENT_SHOW_GAMERCARD, 8);
    }
}

// ---------------------------------------------------------------------------
// ProcessAskDeleteRequest -- X360 0x82385210. Confirm-delete: line up a delete if the picture exists.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::ProcessAskDeleteRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                                        GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpImageGalleryReqEvent != nullptr, "lpImageGalleryReqEvent");
    CGS_ASSERT(lpImageGalleryReqEvent->meImageGalleryRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_ASK_DELETE,
               "GsmIO::E_IMAGE_GALLERY_REQUEST_ASK_DELETE == lpImageGalleryReqEvent->meImageGalleryRequest");
    CGS_ASSERT(lpImageGalleryReqEvent->miSlotIndex < KI_NUM_PICTURES,
               "lpImageGalleryReqEvent->miSlotIndex < KI_IMAGE_GALLERY_NUM_PICTURES");
    CGS_ASSERT(lpImageGalleryReqEvent->miSlotIndex >= 0, "lpImageGalleryReqEvent->miSlotIndex >= 0");
    CGS_ASSERT(mpProgression != nullptr, "mpProgression");

    if (mpProgression->GetProfile()->GetMugshotInfo(lpImageGalleryReqEvent->meImageGalleryImageType,
                                                    lpImageGalleryReqEvent->miImageIndex) != nullptr)
    {
        CgsID lLineUpDeleteId = CgsIDCompress("LineUpDelete");
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lLineUpDeleteId), KI_EVENT_LINEUP_DELETE, 8);
    }
    else
    {
        CGS_ASSERT(lpOutput != nullptr, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
        char lEmptyMarker[8] = { 0 };
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lEmptyMarker), KI_EVENT_EMPTY_SLOT, 1);
    }
}

// ---------------------------------------------------------------------------
// ProcessLockRequest -- X360 0x82385040. Toggle a mugshot's locked flag + republish its slot.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::ProcessLockRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                                   GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
    CGS_ASSERT(lpImageGalleryReqEvent != nullptr, "lpImageGalleryReqEvent");
    CGS_ASSERT(lpImageGalleryReqEvent->meImageGalleryRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_LOCK,
               "GsmIO::E_IMAGE_GALLERY_REQUEST_LOCK == lpImageGalleryReqEvent->meImageGalleryRequest");
    CGS_ASSERT(mpProgression != nullptr, "mpProgression");
    CGS_ASSERT(lpImageGalleryReqEvent->meImageGalleryImageType < GameStateModuleIO::E_IMAGE_GALLERY_TYPE_COUNT,
               "lpImageGalleryReqEvent->meImageGalleryImageType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT");

    BrnProgression::Profile* lpProfile = mpProgression->GetProfile();
    if (lpProfile->LockOrUnlockMugshot(lpImageGalleryReqEvent->meImageGalleryImageType,
                                       lpImageGalleryReqEvent->miImageIndex))
    {
        BrnProgression::MugshotInfo* lpMugshotInfo = lpProfile->GetMugshotInfo(
            lpImageGalleryReqEvent->meImageGalleryImageType, lpImageGalleryReqEvent->miImageIndex);

        ImageInfoEvent lImageInfo;
        lImageInfo.mUniquePlayerID = lpMugshotInfo->GetUniquePlayerID();
        lImageInfo.miSlotIndex     = lpImageGalleryReqEvent->miSlotIndex;
        lImageInfo.mu8LockedFlag   = lpMugshotInfo->GetLockedFlag();
        lImageInfo.mbIsDeletePreview = 0;
        lImageInfo.mu64DateTaken   = lpMugshotInfo->GetDateTaken();
        lImageInfo.miImageWord2C   = lpMugshotInfo->GetImageWord2C();
        lImageInfo.miImageWord0    = lpMugshotInfo->GetImageWord0();
        lImageInfo.miImageWord1    = lpMugshotInfo->GetImageWord1();
        lImageInfo.miImageWord2    = lpMugshotInfo->GetImageWord2();
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lImageInfo), KI_EVENT_IMAGE_INFO, (s32)sizeof(ImageInfoEvent));
    }

    char lEmptyMarker[16] = { 0 };
    AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(lEmptyMarker), KI_EVENT_EMPTY_SLOT, 1);
}

// ---------------------------------------------------------------------------
// Helper: build the type-290 record for one gallery slot from a (possibly null) MugshotInfo.
// Mirrors the X360 stack-record build the Process* / UpdateNewImageRequests workers share.
// ---------------------------------------------------------------------------
namespace
{
    void PostSlotImageInfo(CgsModule::VariableEventQueue<13312, 16>* lpQueue,
                           BrnProgression::MugshotInfo* lpMugshotInfo, s32 liStartSlotIndex,
                           u8 lu8IsDeletePreview)
    {
        if (lpMugshotInfo != nullptr)
        {
            ImageInfoEvent lImageInfo;
            lImageInfo.mUniquePlayerID  = lpMugshotInfo->GetUniquePlayerID();
            lImageInfo.miSlotIndex      = liStartSlotIndex;
            lImageInfo.mbIsDeletePreview = lu8IsDeletePreview;
            lImageInfo.mu8LockedFlag    = lpMugshotInfo->GetLockedFlag();
            lImageInfo.mu64DateTaken    = lpMugshotInfo->GetDateTaken();
            lImageInfo.miImageWord2C    = lpMugshotInfo->GetImageWord2C();
            lImageInfo.miImageWord0     = lpMugshotInfo->GetImageWord0();
            lImageInfo.miImageWord1     = lpMugshotInfo->GetImageWord1();
            lImageInfo.miImageWord2     = lpMugshotInfo->GetImageWord2();
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lImageInfo),
                              KI_EVENT_IMAGE_INFO, (s32)sizeof(ImageInfoEvent));
        }
        else
        {
            EmptySlotInfoEvent lEmptySlot;
            lEmptySlot.miWord00   = 0;
            lEmptySlot.miWord04   = 0;
            lEmptySlot.miWord08   = 0;
            lEmptySlot.meCounty   = BrnWorld::WorldRegion::DistrictToCounty(BrnWorld::E_DISTRICT_COUNT);
            lEmptySlot.meDistrict = BrnWorld::E_DISTRICT_COUNT; // 18 ("no region")
            lEmptySlot.miWord14   = 0;
            lEmptySlot.miSlotIndex = liStartSlotIndex;
            lEmptySlot.mu8FlagA   = 0;
            lEmptySlot.mu8FlagB   = 0;
            lEmptySlot.mu8FlagC   = 0;
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEmptySlot),
                              KI_EVENT_IMAGE_INFO, (s32)sizeof(EmptySlotInfoEvent));
        }
    }
}

// ---------------------------------------------------------------------------
// ProcessScrollImagesEvent -- X360 0x82384AC0. Scroll the gallery left/right.
// Shuffles the gallery slot textures one place and publishes the freshly-uncovered images.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::ProcessScrollImagesEvent(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                                         GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");
    CGS_ASSERT(lpImageGalleryReqEvent != nullptr, "lpImageGalleryReqEvent");
    CGS_ASSERT(lpImageGalleryReqEvent->meImageGalleryRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_LEFT
               || lpImageGalleryReqEvent->meImageGalleryRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT,
               "( GsmIO::E_IMAGE_GALLERY_REQUEST_SCROLL_LEFT == lpImageGalleryReqEvent->meImageGalleryRequest ) || "
               "( GsmIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT == lpImageGalleryReqEvent->meImageGalleryRequest )");
    CGS_ASSERT(mpProgression != nullptr, "mpProgression");

    CgsModule::VariableEventQueue<13312, 16>* lpQueue = AsVeq(lpOutput->GetGameActionQueue());
    BrnProgression::Profile* lpProfile = mpProgression->GetProfile();
    const s32 leGalleryType = lpImageGalleryReqEvent->meImageGalleryImageType;

    mImagesToRenderBitArray.UnSetAll(); // X360 stores -1 then masks; the net effect is "republish all".

    const s32 liSlotIndex   = lpImageGalleryReqEvent->miSlotIndex;
    const s32 liSlotOffset  = (KI_NUM_PICTURES - 1) - liSlotIndex;

    s32 liFirstNewImage;
    s32 liLastNewImage;
    s32 liStartSlotIndex;
    s32 liNewCentreSlot;

    if (lpImageGalleryReqEvent->meImageGalleryRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT)
    {
        // Shift slot textures one place towards index 0 (the new image enters at the high end).
        liStartSlotIndex = liSlotIndex - liSlotOffset;
        const s32 liFirstShiftSlot = liStartSlotIndex + 1;
        for (s32 liShiftSlot = liFirstShiftSlot; liShiftSlot < KI_NUM_PICTURES; ++liShiftSlot)
        {
            CGS_ASSERT(liShiftSlot - 1 >= 0, "liSlotIndex - 1 >= 0");
            CgsNetwork::NetworkTexture& lDst = maImageGalleryMugshots[liShiftSlot - 1];
            CgsNetwork::NetworkTexture& lSrc = maImageGalleryMugshots[liShiftSlot];
            std::memcpy(lDst.GetTexture(), lSrc.GetTexture(), static_cast<size_t>(lDst.GetTextureSize()));
        }
        liFirstNewImage = lpImageGalleryReqEvent->miImageIndex - liSlotOffset;
        liLastNewImage  = lpImageGalleryReqEvent->miImageIndex + liSlotOffset;
        liNewCentreSlot = liLastNewImage;
    }
    else
    {
        // Scroll left: shift slot textures one place towards the high end.
        const s32 liShiftCount = liSlotIndex + liSlotOffset;
        for (s32 liShiftSlot = liShiftCount; liShiftSlot > 0; --liShiftSlot)
        {
            CGS_ASSERT(liShiftSlot < KI_NUM_PICTURES, "liSlotIndex < KI_IMAGE_GALLERY_NUM_PICTURES");
            CGS_ASSERT(liShiftSlot > 0, "liSlotIndex > 0");
            CgsNetwork::NetworkTexture& lDst = maImageGalleryMugshots[liShiftSlot];
            CgsNetwork::NetworkTexture& lSrc = maImageGalleryMugshots[liShiftSlot - 1];
            std::memcpy(lDst.GetTexture(), lSrc.GetTexture(), static_cast<size_t>(lDst.GetTextureSize()));
        }
        liFirstNewImage  = lpImageGalleryReqEvent->miImageIndex - liSlotOffset + 1;
        liLastNewImage   = lpImageGalleryReqEvent->miImageIndex + liSlotOffset + 1;
        liStartSlotIndex = liSlotIndex - liSlotOffset + 1;
        liNewCentreSlot  = liFirstNewImage - 1;
    }

    const s32 liEndSlotIndexAfterScroll = (lpImageGalleryReqEvent->meImageGalleryRequest
                                           == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT)
                                          ? (lpImageGalleryReqEvent->miSlotIndex + liSlotOffset)
                                          : (liSlotIndex + liSlotOffset);

    // Publish every freshly-uncovered image (or empty slot).
    s32 liStartSlot = (lpImageGalleryReqEvent->meImageGalleryRequest
                       == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT)
                      ? (liSlotIndex - liSlotOffset)
                      : (liSlotIndex - liSlotOffset + 1);
    for (s32 liImageIndex = liFirstNewImage; liImageIndex < liLastNewImage; ++liImageIndex, ++liStartSlot)
    {
        CGS_ASSERT(mpProgression != nullptr, "mpProgression");
        BrnProgression::MugshotInfo* lpMugshotInfo = lpProfile->GetMugshotInfo(leGalleryType, liImageIndex);

        CGS_ASSERT(liStartSlot < KI_NUM_PICTURES, "liStartSlotIndex < KI_IMAGE_GALLERY_NUM_PICTURES");
        PostSlotImageInfo(lpQueue, lpMugshotInfo, liStartSlot, 0);
        if (lpMugshotInfo == nullptr && liStartSlot >= 0 && liStartSlot < KI_NUM_PICTURES)
        {
            mImagesToRenderBitArray.UnSetBit(static_cast<u32>(liStartSlot));
        }
    }

    CGS_ASSERT(liEndSlotIndexAfterScroll < KI_NUM_PICTURES,
               "lpImageGalleryReqEvent->miSlotIndex + liSlotOffset < KI_IMAGE_GALLERY_NUM_PICTURES");

    // Re-publish the slot the scroll re-centred on, if it still holds a picture.
    if (liNewCentreSlot >= 0
        && lpProfile->GetMugshotInfo(leGalleryType, liNewCentreSlot) != nullptr)
    {
        ImageLoadRequest lImageLoadRequest;
        lImageLoadRequest.miImageIndex            = liNewCentreSlot;
        lImageLoadRequest.miSlotIndex             = liEndSlotIndexAfterScroll;
        lImageLoadRequest.meImageGalleryImageType = lpImageGalleryReqEvent->meImageGalleryImageType;

        if (maImageLoadRequests.GetLength() == static_cast<u32>(KI_NUM_PICTURES))
        {
            maImageLoadRequests.Erase(0);
        }
        maImageLoadRequests.Append(lImageLoadRequest);
    }

    // Publish the now-empty centre slot record + clear its render bit.
    EmptySlotInfoEvent lEmptySlot;
    lEmptySlot.miWord00   = 0;
    lEmptySlot.miWord04   = 0;
    lEmptySlot.miWord08   = 0;
    lEmptySlot.meCounty   = BrnWorld::WorldRegion::DistrictToCounty(BrnWorld::E_DISTRICT_COUNT);
    lEmptySlot.meDistrict = BrnWorld::E_DISTRICT_COUNT;
    lEmptySlot.miWord14   = 0;
    lEmptySlot.miSlotIndex = liEndSlotIndexAfterScroll;
    lEmptySlot.mu8FlagA   = 0;
    lEmptySlot.mu8FlagB   = 0;
    lEmptySlot.mu8FlagC   = 0;
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEmptySlot),
                      KI_EVENT_IMAGE_INFO, (s32)sizeof(EmptySlotInfoEvent));

    CGS_ASSERT(static_cast<u32>(liEndSlotIndexAfterScroll) < static_cast<u32>(KI_NUM_PICTURES), "luIndex < NUMBITS");
    if (liEndSlotIndexAfterScroll >= 0 && liEndSlotIndexAfterScroll < KI_NUM_PICTURES)
    {
        mImagesToRenderBitArray.UnSetBit(static_cast<u32>(liEndSlotIndexAfterScroll));
    }
    (void)liStartSlotIndex;
}

// ---------------------------------------------------------------------------
// ProcessDeleteRequest -- X360 0x823853B0. Delete a mugshot, shuffle slots, rebuild the bit-set.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::ProcessDeleteRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                                     GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpImageGalleryReqEvent != nullptr, "lpImageGalleryReqEvent");
    CGS_ASSERT(lpImageGalleryReqEvent->meImageGalleryRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_DELETE,
               "GsmIO::E_IMAGE_GALLERY_REQUEST_DELETE == lpImageGalleryReqEvent->meImageGalleryRequest");
    CGS_ASSERT(lpImageGalleryReqEvent->miSlotIndex < KI_NUM_PICTURES,
               "lpImageGalleryReqEvent->miSlotIndex < KI_IMAGE_GALLERY_NUM_PICTURES");
    CGS_ASSERT(lpImageGalleryReqEvent->miSlotIndex >= 0, "lpImageGalleryReqEvent->miSlotIndex >= 0");
    CGS_ASSERT(mpProgression != nullptr, "mpProgression");

    CgsModule::VariableEventQueue<13312, 16>* lpQueue = AsVeq(lpOutput->GetGameActionQueue());
    BrnProgression::Profile* lpProfile = mpProgression->GetProfile();
    const s32 leGalleryType = lpImageGalleryReqEvent->meImageGalleryImageType;

    if (lpProfile->DeleteMugshot(leGalleryType, lpImageGalleryReqEvent->miImageIndex) < 0)
    {
        char lEmptyMarker[16] = { 0 };
        AsVeq(lpOutput->GetGameActionQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lEmptyMarker), KI_EVENT_EMPTY_SLOT, 1);
        return;
    }

    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");

    const s32 liSlotIndex  = lpImageGalleryReqEvent->miSlotIndex;
    const s32 liFirstImage = lpImageGalleryReqEvent->miImageIndex;
    const s32 liUncoverEnd = lpImageGalleryReqEvent->miImageIndex - liSlotIndex + KI_NUM_PICTURES;

    // Republish each image that shifts up to fill the gap left by the deleted one.
    s32 liStartSlot = liSlotIndex;
    for (s32 liImageIndex = liFirstImage; liImageIndex < liUncoverEnd; ++liImageIndex, ++liStartSlot)
    {
        BrnProgression::MugshotInfo* lpMugshotInfo = lpProfile->GetMugshotInfo(leGalleryType, liImageIndex);
        CGS_ASSERT(liStartSlot < KI_NUM_PICTURES, "liSlotIndex < KI_IMAGE_GALLERY_NUM_PICTURES");
        PostSlotImageInfo(lpQueue, lpMugshotInfo, liStartSlot, 1 /* delete preview */);
    }

    // Rebuild + publish the gallery's live-mugshot bitfield.
    CGS_ASSERT(mpProgression != nullptr, "mpProgression");
    const s32 liNumMugshots = lpProfile->GetNumMugshots(leGalleryType);
    LoadCompleteEvent lLoadComplete;
    lLoadComplete.meImageGalleryType = leGalleryType;
    lLoadComplete.miPad04            = 0;
    lLoadComplete.mu64LiveBits       = 0;
    for (s32 liMugshotIndex = 0; liMugshotIndex < liNumMugshots; ++liMugshotIndex)
    {
        CGS_ASSERT(liMugshotIndex < 20, "Index is out of range (max bits: 20)");
        lLoadComplete.mu64LiveBits |= (static_cast<u64>(1) << (liMugshotIndex & 0x3F));
    }
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lLoadComplete),
                      KI_EVENT_LOAD_COMPLETE, (s32)sizeof(LoadCompleteEvent));

    // Clear the render bits of the slots from the deleted one onwards.
    for (s32 liClearSlot = liSlotIndex; liClearSlot < KI_NUM_PICTURES; ++liClearSlot)
    {
        CGS_ASSERT(static_cast<u32>(liClearSlot) < static_cast<u32>(KI_NUM_PICTURES), "luIndex < NUMBITS");
        mImagesToRenderBitArray.UnSetBit(static_cast<u32>(liClearSlot));
    }

    // Shuffle the slot textures up one place to fill the gap.
    for (s32 liSlot = liSlotIndex + 1; liSlot < KI_NUM_PICTURES; ++liSlot)
    {
        CGS_ASSERT(liSlot >= 1, "liSlotIndex - 1 >= 0");
        CgsNetwork::NetworkTexture& lDst = maImageGalleryMugshots[liSlot - 1];
        CgsNetwork::NetworkTexture& lSrc = maImageGalleryMugshots[liSlot];
        std::memcpy(lDst.GetTexture(), lSrc.GetTexture(), static_cast<size_t>(lDst.GetTextureSize()));
    }

    // Clear the now-trailing slot texture.
    CgsNetwork::NetworkTexture& lLastSlot = maImageGalleryMugshots[KI_NUM_PICTURES - 1];
    CGS_ASSERT(lLastSlot.GetTexture() != nullptr, "mpcTexture");
    std::memset(lLastSlot.GetTexture(), 0, static_cast<size_t>(lLastSlot.GetTextureSize()));

    // If a freshly-uncovered image exists past the deletion, prepare it for render (type 295).
    const s32 liUncoverImageIndex = liFirstImage - liSlotIndex + (KI_NUM_PICTURES - 1);
    BrnProgression::MugshotInfo* lpUncoverInfo = lpProfile->GetMugshotInfo(leGalleryType, liUncoverImageIndex);
    if (lpUncoverInfo != nullptr)
    {
        struct PrepareRenderEvent
        {
            s32   maClear[3];      // -1, -1, -1
            s32   miSlotClear[3];  // 0, 0, 0 (interleaved with the above as 3 {id,?,?,size?})
            s32   miFileID;        // +0x00 (MugshotInfo +0x30)
            s32   miTextureSize;   // +0x04
            char* mpTextureBuffer; // +0x08
        } lPrepareRender;
        for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
        {
            lPrepareRender.maClear[liIndex]     = -1;
            lPrepareRender.miSlotClear[liIndex] = 0;
        }
        lPrepareRender.miFileID        = lpUncoverInfo->GetFileID();
        lPrepareRender.mpTextureBuffer = lLastSlot.GetTexture();
        lPrepareRender.miTextureSize   = lLastSlot.GetTextureSize();
        miMugshotToSaveIndex           = 2; // X360 a1[33] = 2
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPrepareRender),
                          KI_EVENT_PREPARE_RENDER, 48);
    }
    else
    {
        char lEmptyMarker[16] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lEmptyMarker), KI_EVENT_EMPTY_SLOT, 1);
    }

    // Re-set the render bits of the slots that still hold an image (up to the deleted slot).
    if (lpProfile->GetMugshotInfo(leGalleryType, liFirstImage) != nullptr)
    {
        for (s32 liSetSlot = liSlotIndex; liSetSlot < KI_NUM_PICTURES - 1; ++liSetSlot)
        {
            CGS_ASSERT(static_cast<u32>(liSetSlot) < static_cast<u32>(KI_NUM_PICTURES), "luIndex < NUMBITS");
            mImagesToRenderBitArray.SetBit(static_cast<u32>(liSetSlot));
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateNewImageRequests -- X360 0x82383F90. Service pending image loads; publish each loaded slot.
// ---------------------------------------------------------------------------
void GameStateImageManagerBase::UpdateNewImageRequests(GameStateModuleIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpOutput->GetGameActionQueue() != nullptr, "lpOutput->GetGameActionQueue()");

    CgsModule::VariableEventQueue<13312, 16>* lpQueue = AsVeq(lpOutput->GetGameActionQueue());
    BrnProgression::Profile* lpProfile = (mpProgression != nullptr) ? mpProgression->GetProfile() : nullptr;

    // Local scratch list of slots whose textures need loading from disk.
    struct PendingLoad { s32 miFileID; s32 miTextureSize; char* mpTextureBuffer; };
    PendingLoad laPendingLoads[KI_NUM_PICTURES];
    for (s32 liIndex = 0; liIndex < KI_NUM_PICTURES; ++liIndex)
    {
        laPendingLoads[liIndex].miFileID       = -1;
        laPendingLoads[liIndex].miTextureSize  = 0;
        laPendingLoads[liIndex].mpTextureBuffer = nullptr;
    }

    s32 liNumLoadsQueued  = 0;
    s32 liNumProcessed    = 0;
    s32* lpSlotMapping    = maLoadedImagesToSlotMapping;

    while (true)
    {
        if (liNumProcessed >= static_cast<s32>(maImageLoadRequests.GetLength()))
        {
            break;
        }

        const ImageLoadRequest& lrImageLoadRequest = maImageLoadRequests[static_cast<u32>(liNumProcessed)];
        const s32 liSlotIndex = lrImageLoadRequest.miSlotIndex;

        CGS_ASSERT(static_cast<u32>(liSlotIndex) < static_cast<u32>(KI_NUM_PICTURES), "luIndex < NUMBITS");
        if (liSlotIndex >= 0 && liSlotIndex < KI_NUM_PICTURES)
        {
            mImagesToRenderBitArray.UnSetBit(static_cast<u32>(liSlotIndex));
        }

        if (lrImageLoadRequest.miImageIndex >= 0)
        {
            CGS_ASSERT(mpProgression != nullptr, "mpProgression");
            CGS_ASSERT(lrImageLoadRequest.meImageGalleryImageType < GameStateModuleIO::E_IMAGE_GALLERY_TYPE_COUNT,
                       "lImageLoadRequest.meImageGalleryImageType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT");

            BrnProgression::MugshotInfo* lpMugshotInfo = lpProfile->GetMugshotInfo(
                lrImageLoadRequest.meImageGalleryImageType, lrImageLoadRequest.miImageIndex);
            if (lpMugshotInfo != nullptr)
            {
                PostSlotImageInfo(lpQueue, lpMugshotInfo, liSlotIndex, 0);
                ++liNumLoadsQueued;
                ++liNumProcessed;

                *lpSlotMapping++ = liSlotIndex;
                CgsNetwork::NetworkTexture& lSlotTexture = maImageGalleryMugshots[liSlotIndex];
                laPendingLoads[liNumLoadsQueued - 1].miFileID       = lpMugshotInfo->GetFileID();
                laPendingLoads[liNumLoadsQueued - 1].miTextureSize  = lSlotTexture.GetTextureSize();
                laPendingLoads[liNumLoadsQueued - 1].mpTextureBuffer = lSlotTexture.GetTexture();
            }
            else
            {
                PostSlotImageInfo(lpQueue, nullptr, liSlotIndex, 0);
                ++liNumProcessed;
            }
        }
        else
        {
            PostSlotImageInfo(lpQueue, nullptr, liSlotIndex, 0);
            ++liNumProcessed;
        }
    }

    if (!mbLoadInProgress)
    {
        if (liNumLoadsQueued > 0)
        {
            // Kick off the file load (type 295: prepare-render record carrying the pending loads).
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(laPendingLoads),
                              KI_EVENT_PREPARE_RENDER, 48);
            maImageLoadRequests.Clear();
            mbLoadInProgress = true;
        }

        if (maImageLoadRequests.GetLength() != 0 && liNumLoadsQueued == 0)
        {
            maImageLoadRequests.Clear();
            char lEmptyMarker = 0;
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEmptyMarker), KI_EVENT_EMPTY_SLOT, 1);
        }
    }
}

} // namespace BrnGameState
