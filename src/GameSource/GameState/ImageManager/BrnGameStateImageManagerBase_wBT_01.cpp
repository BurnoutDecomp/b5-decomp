// BrnGameState::GameStateImageManagerBase -- lifecycle (Construct / Destruct / Prepare).
//
// The owning GameStateModule embeds the manager at gsm+0x2D4B0 and calls Construct (right after
// StreetManager::Construct, with the progression manager), Prepare (stage 24, with heap allocator
// 0x1B) and Destruct (early in GameStateModule::Destruct).

#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h"

namespace BrnGameState
{

// Cache the progression manager and clear every slot.
void GameStateImageManagerBase::Construct(BrnProgression::ProgressionManager* lpProgression)
{
    CGS_ASSERT(lpProgression != nullptr, "lpProgression");

    mpProgression       = lpProgression;        // this+0xB8
    miMugshotToSaveIndex = 0;                    // this+0xAC
    maImageLoadRequests.Clear();                 // this+0x80 count word -> 0
    mbLoadInProgress    = false;                 // this+0xBC
    mImagesToRenderBitArray.UnSetAll();          // this+0xB0 (std 0)

    // this+0x84: maLoadedImagesToSlotMapping[i] = 0; this+0x90: maiImagesLockedForSave[i] = -1.
    for (s32 liIndex = 0; liIndex < KI_IMAGE_GALLERY_NUM_PICTURES; ++liIndex)
    {
        maLoadedImagesToSlotMapping[liIndex] = 0;
        maiImagesLockedForSave[liIndex]      = -1;
    }

    // this+0x08: construct each gallery mugshot texture.
    for (s32 liIndex = 0; liIndex < KI_IMAGE_GALLERY_NUM_PICTURES; ++liIndex)
    {
        maImageGalleryMugshots[liIndex].Construct();
    }
}

// Destruct each texture and re-clear every slot.
void GameStateImageManagerBase::Destruct()
{
    for (s32 liIndex = 0; liIndex < KI_IMAGE_GALLERY_NUM_PICTURES; ++liIndex)
    {
        maImageGalleryMugshots[liIndex].Destruct();
    }

    for (s32 liIndex = 0; liIndex < KI_IMAGE_GALLERY_NUM_PICTURES; ++liIndex)
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

// Allocate and zero the three 160x120 DXT1 gallery mugshot textures out of the given heap.
bool GameStateImageManagerBase::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
{
    for (s32 liIndex = 0; liIndex < KI_IMAGE_GALLERY_NUM_PICTURES; ++liIndex)
    {
        CgsNetwork::NetworkTexture& lTexture = maImageGalleryMugshots[liIndex];
        lTexture.Prepare(lpHeapMalloc, 160, 120, renderengine::PIXELFORMAT_DXT1);
        lTexture.ClearPixels();
    }

    mImagesToRenderBitArray.UnSetAll(); // this+0xB0 = 0
    return true;
}

// The base export handler does nothing: the only slot of the vtable the module's constructor
// installs at gsm+0x2D4B0 is the image's shared empty return.
void GameStateImageManagerBase::ProcessExportRequest(const ImageGalleryRequestEvent* lpImageGalleryReqEvent,
                                                     GameStateModuleIO::OutputBuffer* lpOutput)
{
    (void)lpImageGalleryReqEvent;
    (void)lpOutput;
}

}
