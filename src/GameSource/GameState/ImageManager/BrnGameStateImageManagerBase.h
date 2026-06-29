#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h
// ============================================================================
// Canonical home for BrnGameState::GameStateImageManagerBase -- the in-game image / mugshot
// "gallery" manager embedded by value in the GameStateModule (BrnGameStateModule::Construct
// calls GameStateImageManagerBase::Construct, ::Prepare, ::Release, ::Destruct, ::PreWorldUpdate
// and routes the image game-events to its Handle* sinks). It owns three NetworkTexture mugshot
// buffers (the on-screen gallery slots), a small Array of pending image-load requests, and the
// per-slot "needs render" bit-set; it drives the gallery UI by reading the persisted mugshot
// records out of the player's Profile (mpProgression->GetProfile()) and pushing GUI/game-action
// events onto the OutputBuffer's GameActionQueue (CgsModule::VariableEventQueue<13312,16>).
//
// SHAPE is DWARF-authoritative (references/DecFIGS/dwarfdump/.../ImageManager/
// BrnGameStateImageManagerBase.h:81..212), gated against the X360 binary's Construct
// (0x8236D720) / Destruct (0x8236D7E0) store offsets, which pin every member to its exact byte
// slot (all offsets X360-PROVEN from the asm store list):
//   +0x008  NetworkTexture        maImageGalleryMugshots[3]   (3 * 28 == 84 bytes -> +0x5C)
//   +0x05C  Array<ImageLoadRequest,3> maImageLoadRequests     (3 * 12 data + 4 count word @+0x80)
//   +0x080  (Array miCount word for maImageLoadRequests)
//   +0x084  s32                   maLoadedImagesToSlotMapping[3]  (Construct -> 0 each)   @+0x84
//   +0x090  s32                   maiImagesLockedForSave[3]       (Construct -> -1 each)  @+0x90
//   +0x09C  WorldRegion           meCurrentWorldRegion            (county@+0x9C/district@+0xA0)
//   +0x0A4  WorldRegion           meWorldRegionAtLastTakedown     (county@+0xA4/district@+0xA8)
//   +0x0AC  s32                   miMugshotToSaveIndex            (Construct -> 0)
//   +0x0B0  BitArray<3>           mImagesToRenderBitArray         (Construct std 0)
//   +0x0B8  ProgressionManager*   mpProgression                   (Construct arg)
//   +0x0BC  bool                  mbLoadInProgress                (Construct -> 0)
// A vptr leads the object (ProcessExportRequest is virtual -- DWARF :164, called via vtable[0]
// in HandleImageGalleryRequest 0x82391C98). MSVC places the vptr at +0x00 and the first named
// member (maImageGalleryMugshots) at +0x08 (4-byte vptr + 4-byte pad on the X360 ABI; the host
// 64-bit gate uses an 8-byte vptr which still lands the first member at +0x08).
//
// ASSERT-PARITY NOTE: the X360 bakes verbatim "d:\\p4\\...BrnGameStateImageManagerBase.cpp" + exact
// line numbers into every FireAssert. The house CGS_ASSERT macro emits __FILE__/__LINE__ instead;
// this is the project-wide benign assert-machinery YELLOW.

#include <cstddef>   // offsetof (layout asserts)
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsArray.h"                           // Array<ImageLoadRequest,3>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                        // CgsContainers::BitArray<3>
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"            // CgsNetwork::NetworkTexture
#include "GameSource/BurnoutConstants.h"                                          // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"                            // GameStateModuleIO::EImageType / EImageGalleryType
#include "GameSource/GameState/BrnGameStateModuleIO.h"                            // OutputBuffer / GameActionQueue
#include "SharedClasses/World/BrnWorldRegion.h"                                   // BrnWorld::WorldRegion
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<TakedownEvent,8>
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"         // BrnGameState::TakedownEvent

namespace CgsMemory { class HeapMalloc; }                                         // Prepare arg (pointer-only)
namespace BrnProgression { class ProgressionManager; struct MugshotInfo; }        // mpProgression / GetMugshotInfo (pointer-only)

namespace BrnGameState
{
// ---------------------------------------------------------------------------
// The image game-event payloads this manager's Handle* sinks consume. Their full layouts live
// with the network-image / GUI-input subsystems; only the leading words touched here are needed,
// so each is modelled as the minimal concrete shape the consumer dereferences. Widen with the real
// types when those TUs land (offsets must not move).
// ---------------------------------------------------------------------------

// Read by HandleImageSaveEvent (X360 0x82391AA8). The X360 reads: a 16-byte unique-player-id at
// +0x00, the road id at *(a2+0)..its slots, the EImageType at +0x18 (a2[6]), and the texture
// pointer at +0x1C (a2[7]). Built by the mugshot-capture path; modelled as the words this sink reads.
struct ImageToSaveEvent
{
    u64                           mu64UniqueIdLo;     // +0x00 (UniquePlayerID lo)  -> AddMugshot arg0/1
    u64                           mu64UniqueIdHi;     // +0x08 (UniquePlayerID hi)
    u8                            maPad0x10[0x18 - 0x10];
    GameStateModuleIO::EImageType meImageType;        // +0x18 (a2[6]) -> gallery-type mapping
    CgsNetwork::NetworkTexture*   mpTexture;           // +0x1C (a2[7]) -> RequestImageSave
};

// Read by HandleImageFilesLoadedEvent (X360 0x82384548). One per-slot file-info record (the X360
// walks 3 of them, each 16 bytes / 4 words): miFileID@+0x00 (>-1 == valid), miSize@+0x04,
// mpBuffer@+0x08, and a trailing word. The event leads with a count word (a2[0]) then the records.
struct ImageFilesLoadedEvent
{
    struct ImageFileInfo
    {
        s32  miFileID;   // +0x00 (-1 == empty slot)
        s32  miSize;     // +0x04
        char* mpBuffer;  // +0x08
        s32  miWord0C;   // +0x0C
    };
    s32           miUnused00;        // +0x00 (a2[0]; the X360 reads from a2+4 onward)
    ImageFileInfo maFileInfos[3];    // +0x04 (3 * 0x10)
};

// Read by HandleImageFilesSavedEvent (X360 0x82357CD8). A count word then that many saved-file ids.
struct ImageFilesSavedEvent
{
    s32 miNumSavedFiles;     // +0x00
    s32 maSavedFileInfos[3 * 4]; // each saved entry is 4 words (the X360 strides a2 by 16); id @ entry+0
};

// Read by HandleWorldRegionChangeEvent (X360 0x82357C68). A WorldRegion (county + district).
struct WorldRegionChangeEvent
{
    BrnWorld::ECounty   meCounty;    // +0x00
    BrnWorld::EDistrict meDistrict;  // +0x04
};

// Read by HandleImageGalleryCountRequest (X360 0x82385E78). The gallery (image) type to count.
struct ImageGalleryCountRequestEvent
{
    GameStateModuleIO::EImageGalleryType meImageGalleryImageType; // +0x00 (validated < COUNT(5))
};

// Read by HandleImageGalleryDataRequest (X360 0x8238..). Declared-only; this sink has no body in
// this TU's function set.
struct ImageGalleryDataRequestEvent
{
    GameStateModuleIO::EImageGalleryType meImageGalleryImageType; // +0x00
};

// Read by HandleImageGalleryRequest (X360 0x82391C98) and the Process* workers. The X360 reads:
//   *(a2+0)   miImageIndex    (the gallery image index)
//   a2[1]     miSlotIndex     (0..2; the on-screen gallery slot)
//   a2[2]     meImageGalleryRequest (the request kind; switch in HandleImageGalleryRequest)
//   a2[3]     meImageGalleryImageType (the gallery image type, validated < COUNT(5))
struct ImageGalleryRequestEvent
{
    s32                                  miImageIndex;            // +0x00
    s32                                  miSlotIndex;             // +0x04
    GameStateModuleIO::EImageGalleryRequest meImageGalleryRequest; // +0x08
    GameStateModuleIO::EImageGalleryType meImageGalleryImageType; // +0x0C
};

// ============================================================================
// GameStateImageManagerBase (DWARF BrnGameStateImageManagerBase.h:81)
// ============================================================================
struct GameStateImageManagerBase
{
public:
    // Number of on-screen gallery picture slots (X360 asserts `miSlotIndex < 3` everywhere).
    static const s32 KI_IMAGE_GALLERY_NUM_PICTURES = 3;

    // A queued image-load request (element of maImageLoadRequests). DWARF :174 (12 bytes). This is
    // the canonical home; the Array<ImageLoadRequest,3> explicit-instantiation TU (and the leaf
    // containers header) include this header rather than redefining it (the documented promotion
    // pattern -- see DeveloperChallengeManager / StuntModeScoringOnline).
    struct ImageLoadRequest
    {
        s32                                  miImageIndex;            // 0x00
        s32                                  miSlotIndex;             // 0x04
        GameStateModuleIO::EImageGalleryType meImageGalleryImageType; // 0x08
    };

public:
    // ---- lifecycle ----
    // X360 0x8236D720. Cache the progression manager and clear every slot.
    void Construct(BrnProgression::ProgressionManager* lpProgression);
    // X360 0x8236D858. Allocate the three gallery mugshot textures.
    bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc);
    // X360 0x8236D918. Release the three gallery mugshot textures.
    bool Release();
    // X360 0x8236D7E0. Destruct the three gallery mugshot textures + clear every slot.
    void Destruct();

    // ---- per-frame hook ----
    // X360 0x82391A08. Drains the frame's takedown events, services pending image loads, and
    // re-publishes the per-slot render state.
    void PreWorldUpdate(::EActiveRaceCarIndex lePlayerRaceCarIndex,
                        const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                        GameStateModuleIO::OutputBuffer* lpOutput);

    // ---- event sinks (driven by GameStateModule::ProcessGameEvents) ----
    // X360 0x82391AA8. A mugshot was captured -> persist it into the profile + request a file save.
    void HandleImageSaveEvent(const ImageToSaveEvent* lpImageSaveEvent,
                              GameStateModuleIO::OutputBuffer* lpOutput,
                              bool lbIsAutomaticCapture);
    // X360 0x82384548. The mugshot image files finished loading -> copy them into the gallery slots.
    void HandleImageFilesLoadedEvent(const ImageFilesLoadedEvent* lpImageFilesLoadedEvent,
                                     GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82391C98. A gallery UI request arrived -> dispatch to the matching Process* worker.
    void HandleImageGalleryRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                   GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82385E78. UI asked how many mugshots of a given gallery type exist.
    void HandleImageGalleryCountRequest(const ImageGalleryCountRequestEvent* lpImageGalleryCountReqEvent,
                                        GameStateModuleIO::OutputBuffer* lpOutput);
    // DWARF :117. UI asked for the per-gallery image data. Declared-only (no body in this TU set).
    void HandleImageGalleryDataRequest(const ImageGalleryDataRequestEvent* lpImageGalleryDataReqEvent,
                                       GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82357C68. The world region changed -> remember the current region.
    void HandleWorldRegionChangeEvent(const WorldRegionChangeEvent* lpWorldRegionChangeEvent);
    // X360 0x82357CD8. Mugshot image files finished saving -> clear their locked-for-save slots.
    void HandleImageFilesSavedEvent(const ImageFilesSavedEvent* lpImageFilesSavedEvent);

protected:
    // DWARF :127/:130. Declared-only accessors (no body in this TU's function set).
    CgsNetwork::NetworkTexture* GetImageGalleryMugshot(s32 liSlotIndex);
    BrnProgression::MugshotInfo* GetMugshotInfo(GameStateModuleIO::EImageGalleryType leImageGalleryType,
                                                s32 liImageIndex);

    // DWARF :164. The export-request handler is VIRTUAL (HandleImageGalleryRequest dispatches the
    // export case through vtable[0]). The base body is reconstructed in another TU (it is not in
    // this TU's function set); declared virtual here so the vtable slot exists.
    virtual void ProcessExportRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                      GameStateModuleIO::OutputBuffer* lpOutput);

private:
    // ---- per-frame workers ----
    // X360 0x82363E80. Walk the takedown events; cache the region of the latest takedown we star in.
    void UpdateTakedownEvents(::EActiveRaceCarIndex lePlayerRaceCarIndex,
                              const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue);
    // X360 0x82383F90. Service pending image-load requests; publish each loaded slot to the UI.
    void UpdateNewImageRequests(GameStateModuleIO::OutputBuffer* lpOutput);
    // DWARF :140. Re-publish the per-slot render state. Declared-only (no body in this TU set).
    void UpdateImagesToRender(GameStateModuleIO::OutputBuffer* lpOutput);
    // DWARF :143. Declared-only (no body in this TU set).
    bool IsImageSaveSlotAvailable();

    // X360 0x82384868. Copy the just-saved texture into the next save slot + post a save action.
    void RequestImageSave(s32 liFileID, CgsNetwork::NetworkTexture* lpTexture,
                          GameStateModuleIO::OutputBuffer* lpOutput);

    // ---- gallery-request workers (dispatched by HandleImageGalleryRequest) ----
    // DWARF :149. Declared-only (no body in this TU's function set).
    void ProcessNewImageRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent);
    // X360 0x82384AC0. Scroll the gallery left/right; shuffle the slot textures + publish new data.
    void ProcessScrollImagesEvent(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                  GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82385040. Toggle a mugshot's locked flag + republish its slot.
    void ProcessLockRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                            GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82385210. Confirm-delete: ask the UI to line up a delete if the picture exists.
    void ProcessAskDeleteRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                 GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x823853B0. Delete a mugshot: shuffle slots, rebuild the live-mugshot bit-set, republish.
    void ProcessDeleteRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                              GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82385D00. Show the captured player's Xbox Live gamercard for a gallery slot.
    void ProcessShowGamerCardRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                     GameStateModuleIO::OutputBuffer* lpOutput);

    // DWARF :170. Map an EImageType to its EImageGalleryType. Declared-only (no body here; the
    // X360 inlines the KAE_IMAGE_TYPE_TO_GALLERY_TYPE_MAPPING lookup at the call sites).
    GameStateModuleIO::EImageGalleryType GetImageGalleryTypeFromImageType(GameStateModuleIO::EImageType leImageType);

private:
    // ---- data members at exact X360 byte offsets (vptr leads at +0x00) -----------------
    CgsNetwork::NetworkTexture maImageGalleryMugshots[KI_IMAGE_GALLERY_NUM_PICTURES]; // +0x08 (3 * 28)
    Array<ImageLoadRequest, 3u> maImageLoadRequests;                                  // +0x5C (count word @+0x80)
    s32                        maLoadedImagesToSlotMapping[KI_IMAGE_GALLERY_NUM_PICTURES]; // +0x84
    s32                        maiImagesLockedForSave[KI_IMAGE_GALLERY_NUM_PICTURES];      // +0x90
    BrnWorld::WorldRegion      meCurrentWorldRegion;                                  // +0x9C (8 bytes)
    BrnWorld::WorldRegion      meWorldRegionAtLastTakedown;                           // +0xA4 (8 bytes)
    s32                        miMugshotToSaveIndex;                                  // +0xAC
    CgsContainers::BitArray<3u> mImagesToRenderBitArray;                              // +0xB0 (8 bytes)
    BrnProgression::ProgressionManager* mpProgression;                               // +0xB8
    bool                       mbLoadInProgress;                                      // +0xBC

    // Compile-time offset guards. The X360 Construct/Destruct stores pin every offset; guard the
    // exact byte slots on the X360 4-byte-pointer ABI (the host gate is a 64-bit build whose vptr +
    // pointer members are 8 bytes wide -- still parity-correct, just not byte-identical).
    static void _AssertLayout()
    {
        static_assert(sizeof(::EActiveRaceCarIndex) == 4, "EActiveRaceCarIndex is a 4-byte enum");
        static_assert(sizeof(CgsContainers::BitArray<3u>) == 8, "BitArray<3> is one 8-byte field");
        static_assert(sizeof(BrnWorld::WorldRegion) == 8, "WorldRegion is 8 bytes");
    }
};
} // namespace BrnGameState
