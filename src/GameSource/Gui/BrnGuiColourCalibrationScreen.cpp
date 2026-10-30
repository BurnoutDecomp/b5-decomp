#include "GameSource/Gui/BrnGuiColourCalibrationScreen.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // CgsDev::Log::WriteToLog ([calib-screen] trace)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // CgsResource::Events::AcquireResourceResponse
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOptionsBrightnessContrastPostFxControl
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameSource/Resource/BrnGameDataModuleIO.h"                     // GameDataIO::InputBuffer / RequestInterface<32768>
// BrnGui::ColourCalibrationScreen -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (4 ledger functions, DWARF primary file GameSource/Gui/
// BrnGuiColourCalibrationScreen.cpp):
//   ColourCalibrationScreen::Construct @0x8244EE78  (GuiModule::Construct  @0x82518B24)
//   ColourCalibrationScreen::Destruct  @0x824471C0  (GuiModule::Destruct   @0x825076xx)
//   ColourCalibrationScreen::RecvEvent @0x824471D0  (GuiModule::HandleEventsPostBaseModuleUpdate
//                                                    @0x825079A8, ids 514/515)
//   ColourCalibrationScreen::Update    @0x8246AA28  (GuiModule::Update     @0x82529B04)
//
// (Construct is attributed to the CgsEventReceiverQueue.h catch-all in the ledger because
// its whole body is the inlined EventReceiverQueue<1024,16>::Construct; it is homed here,
// with its own class, and the catch-all is untouched.)
//
// RecvEvent (asm walk): assert lpEvent (cpp:252; the X360 streams "invalid event passed
// through " + the id -- folded static; no early-out), then: id 514 -> assert the state
// machine is idle (cpp:258) and start the show; id 515 -> start the hide; else ignore.
//
// Update (asm walk, switch on meState):
//   CONSTRUCTED(0)/RELEASED(5): nothing.
//   PREPARE_TO_SHOW(1): push an AcquireResourceRequest for
//     "brncrashnavcolourcalibrate_0.tif" (user = &mReceiverQueue, event id 595017, pool 9;
//     the inlined RequestInterface<32768>::AcquireResource, AddEvent type 4/24B) onto the
//     GDM input's request queue; -> ACQUIRINGTEXTURERESOURCE.
//   ACQUIRINGTEXTURERESOURCE(2): wait for the response (assert exactly one queued event,
//     cpp:128; assert it exists, cpp:136; assert liEventId ==
//     CgsResource::ResourceIO::EVENT_ACQUIRERESOURCE (4), cpp:137; assert the request's
//     event id round-tripped, cpp:144). Latch the handle (assert non-null, cpp:147), clear
//     the queue, deref the texture (the SmallResource first-word double-deref; assert,
//     cpp:155), build the sampler Parameters (address UVW=2/2/2, filters 0, aniso 13,
//     field10=1, lod bias 0, trailing flag bytes {0,0,0,1,1}, texture bound), size the
//     state via TextureState::GetResourceDescriptor, carve mTextureStateResource from the
//     allocator (X360 vtable+0x10 with the hidden sret = rw::IResourceAllocator::DoAllocate
//     by name on the host; the descriptor converted X360-5 -> host-4 exactly as the reviewed
//     CgsAptRenderHandler.cpp texture-state carve does) and Initialize the texture state;
//     FALL THROUGH to
//   PREPARED(3): state = PREPARED and publish
//     GuiOptionsBrightnessContrastPostFxControl{handle, false} each frame.
//   PREPARE_TO_HIDE(4): publish {NULLResourceHandle, true} and reset to CONSTRUCTED.
//   default: assert "Unhandled state N in ColourCalibrationScreen::Prepare" (cpp:199;
//     dynamic message folded static).

namespace BrnGui
{
namespace
{
    // The calibration texture request constants (X360 immediates at the Update site).
    const s32   KI_ACQUIRE_EVENT_ID  = 595017;   // 0x91449
    const s32   KI_TEXTURE_POOL_ID   = 9;
    const char* KPC_TEXTURE_NAME     = "brncrashnavcolourcalibrate_0.tif";

    // CgsResource::ResourceIO::EVENT_ACQUIRERESOURCE (the cpp:137 assert names it; the
    // pool-module request/response tag).
    const s32 KI_EVENT_ACQUIRERESOURCE = 4;


}

// @ 0x8244EE78 -- the console body is exactly two things, in this order: bring the
// embedded receiver queue up over its own backing buffer (the inlined
// EventReceiverQueue<1024,16>::Construct -- `stw 0x400, 0x10(queue)` / `stw 0x10,
// 0x14(queue)` / `stw &queue.maBuffer, 0(queue)` / `bl BaseEventReceiverQueue::Clear`),
// then seed the state machine (`li r11,0 / stw r11, 0(this)`). NOTHING else is touched:
// mColourCalibrationTextureHandle, mTextureStateResource and mpTextureState are left
// alone here -- Update's acquire arm is their only writer, exactly as on the console.
void ColourCalibrationScreen::Construct()
{
    mReceiverQueue.Construct();
    meState = E_COLOURCALIBRATIONSCREENSTATE_CONSTRUCTED;
}

// @ 0x824471C0
void ColourCalibrationScreen::Destruct()
{
    meState = E_COLOURCALIBRATIONSCREENSTATE_DESTRUCTED;
}

// @ 0x824471D0
void ColourCalibrationScreen::RecvEvent(const CgsModule::Event* lpEvent, s32 liId)
{
    // cpp:252 -- the X360 streams the offending id into the message; folded static.
    CGS_ASSERT(lpEvent != NULL, "invalid event passed through ");

    if (liId == 514)
    {
        CGS_ASSERT(E_COLOURCALIBRATIONSCREENSTATE_CONSTRUCTED == meState,
                   "E_COLOURCALIBRATIONSCREENSTATE_CONSTRUCTED == meState");
        meState = E_COLOURCALIBRATIONSCREENSTATE_PREPARE_TO_SHOW;
    }
    else if (liId == 515)
    {
        meState = E_COLOURCALIBRATIONSCREENSTATE_PREPARE_TO_HIDE;
        CgsDev::Log::WriteToLog("[calib-screen] hide requested (GUI 515)\n");
    }
}

// @ 0x8246AA28
void ColourCalibrationScreen::Update(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                                     const BrnResource::GameDataIO::OutputBuffer* lpGDMOutput,
                                     CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
                                     rw::IResourceAllocator* lpAllocator)
{
    (void)lpGDMOutput;   // r5: never read by the console body either (see the header note)

    switch (meState)
    {
    case E_COLOURCALIBRATIONSCREENSTATE_CONSTRUCTED:
    case E_COLOURCALIBRATIONSCREENSTATE_RELEASED:
        break;

    case E_COLOURCALIBRATIONSCREENSTATE_PREPARE_TO_SHOW:
        // Request the calibration texture from the resource pools.
        lpGDMInput->GetRequestInterface()->AcquireResource(
            &mReceiverQueue, KI_ACQUIRE_EVENT_ID, KI_TEXTURE_POOL_ID, KPC_TEXTURE_NAME);
        meState = E_COLOURCALIBRATIONSCREENSTATE_ACQUIRINGTEXTURERESOURCE;
        break;

    case E_COLOURCALIBRATIONSCREENSTATE_ACQUIRINGTEXTURERESOURCE:
    {
        if (mReceiverQueue.GetLength() == 0)
            break;   // still waiting for the pool's reply

        CGS_ASSERT(mReceiverQueue.GetLength() == 1, "mReceiverQueue.GetLength() == 1");

        const CgsModule::Event* lpEvent  = NULL;
        s32                     liSize   = 0;
        const s32               liEventId = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

        CGS_ASSERT(lpEvent != NULL, "lpEvent");
        CGS_ASSERT(liEventId == KI_EVENT_ACQUIRERESOURCE,
                   "liEventId == CgsResource::ResourceIO::EVENT_ACQUIRERESOURCE");

        const CgsResource::Events::AcquireResourceResponse* lpAcquire =
            reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
        CGS_ASSERT(KI_ACQUIRE_EVENT_ID == lpAcquire->miEventId,
                   "595017 == lpAcquire->GetEventId()");

        // Latch the resolved handle and drop the reply.
        mColourCalibrationTextureHandle.mpResourceMemory = lpAcquire->mpResourceMemory;
        mColourCalibrationTextureHandle.mpSourceEntry    = lpAcquire->mpSourceEntry;
        CGS_ASSERT(mColourCalibrationTextureHandle != CgsResource::NULLResourceHandle,
                   "mColourCalibrationTextureHandle != CgsResource::NULLResourceHandle");
        mReceiverQueue.Clear();

        // The handle's resource memory is the SmallResource slot whose first word is the
        // main-memory texture (the committed SafeResourceHandle double-deref idiom).
        renderengine::Texture* lpColourCalibrationTexture =
            *reinterpret_cast<renderengine::Texture* const*>(
                mColourCalibrationTextureHandle.mpResourceMemory);
        CGS_ASSERT(NULL != lpColourCalibrationTexture, "NULL != lpColourCalibrationTexture");

        // ARTIST8246AC6C..CCD: address UVW=2, all filters=0.
        // Mip filter differs from the native constructor default of 2.
        renderengine::TextureState::Parameters lTextureStateParameters;
        lTextureStateParameters.muAddressU      = 2;
        lTextureStateParameters.muAddressV      = 2;
        lTextureStateParameters.muAddressW      = 2;
        lTextureStateParameters.muMagFilter     = 0;
        lTextureStateParameters.muMinFilter     = 0;
        lTextureStateParameters.muMipFilter     = 0;
        lTextureStateParameters.muMinMipLevel = 13;
        lTextureStateParameters.muMaxAnisotropy       = 1;
        lTextureStateParameters.mfMipLodBias    = 0.0f;
        lTextureStateParameters.mbPointBorderEnable      = 1;
        lTextureStateParameters.mbUnidentified44      = 1;
        lTextureStateParameters.mpTexture       = lpColourCalibrationTexture;

        rw::ResourceDescriptor lDescriptor;
        renderengine::TextureState::GetResourceDescriptor(&lDescriptor);
        mTextureStateResource = lpAllocator->DoAllocate(lDescriptor, 0);

        mpTextureState = renderengine::TextureState::Initialize(&mTextureStateResource,
                                                                &lTextureStateParameters);
        CgsDev::Log::WriteToLog("[calib-screen] calibration texture acquired\n");
    }
        // FALLTHROUGH (the X360 case-2 body runs straight into the PREPARED publish).
    case E_COLOURCALIBRATIONSCREENSTATE_PREPARED:
    {
        meState = E_COLOURCALIBRATIONSCREENSTATE_PREPARED;

        GuiOptionsBrightnessContrastPostFxControl lControl;
        lControl.mColourCalibrationTextureHandle = mColourCalibrationTextureHandle;
        lControl.mbEnablePostFx                  = false;
        lpGuiOutput->AddGuiOutEvent(lControl);
        break;
    }

    case E_COLOURCALIBRATIONSCREENSTATE_PREPARE_TO_HIDE:
    {
        GuiOptionsBrightnessContrastPostFxControl lControl;
        lControl.mColourCalibrationTextureHandle = CgsResource::NULLResourceHandle;
        lControl.mbEnablePostFx                  = true;
        lpGuiOutput->AddGuiOutEvent(lControl);

        meState = E_COLOURCALIBRATIONSCREENSTATE_CONSTRUCTED;
        CgsDev::Log::WriteToLog("[calib-screen] hidden -- post-fx restored\n");
        break;
    }

    default:
        // cpp:199 -- the X360 streams "Unhandled state <N> in ColourCalibrationScreen::
        // Prepare"; folded static.
        CGS_ASSERT(false, "Unhandled state in ColourCalibrationScreen::Prepare");
        break;
    }
}
}
