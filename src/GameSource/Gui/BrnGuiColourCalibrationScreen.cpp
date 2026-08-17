#include "GameSource/Gui/BrnGuiColourCalibrationScreen.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // CgsDev::Log::WriteToLog ([calib-screen] trace)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // CgsResource::Events::AcquireResourceResponse
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOptionsBrightnessContrastPostFxControl
#include "GameSource/Resource/BrnGameDataModuleIO.h"                     // GameDataIO::InputBuffer / RequestInterface<32768>
#include "pc/gcm/renderengine/renderstates.h"                            // renderengine::TextureState (+ Parameters)
#include "pc/gcm/renderengine/texture.h"                                 // renderengine::Texture

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

    // The console publish is CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<T>; the
    // instantiation for this exact T is X360 0x82465D98 and its whole body is
    //     assert(IsBufferLockedForWriting());
    //     mOutEvents.AddEvent(&event, T::GetEventType() /*546*/, sizeof(T) /*X360 12*/);
    // FLAG PC-ABI adapter: with no live CgsGuiModuleIO::OutputBuffer on PC the screen is
    // handed that buffer's mOutEvents stand-in (GuiModule::mGuiOutQueue) directly, so this
    // helper IS AddGuiOutEvent<T>'s body, minus the lock assert the raw queue has no bit
    // for. Same AddEvent, same GetEventType() key, same sizeof(T) record.
    // DELETE-WHEN the GUI module owns a real CgsGuiModuleIO::OutputBuffer.
    template <class T>
    void AddGuiOutEvent(CgsModule::VariableEventQueue<18432, 16>* lpQueue, const T& lrEvent)
    {
        CGS_ASSERT(lpQueue != NULL, "lpQueue");
        if (lpQueue != NULL)
        {
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrEvent),
                              lrEvent.GetEventType(), static_cast<s32>(sizeof(T)));
        }
    }
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
        // [FLAG PC bring-up diagnostic] edge-triggered by construction (514 is only taken
        // out of CONSTRUCTED). DELETE-WHEN the calibration card is visible on screen.
        CgsDev::Log::WriteToLog("[calib-screen] show requested (GUI 514)\n");
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
                                     CgsModule::VariableEventQueue<18432, 16>* lpGuiOutEvents,
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

        // Build the sampler parameters the X360 stores (clamp addressing, point filters,
        // 13x aniso, the {0,0,0,1,1} trailing flag bytes) over the acquired texture.
        renderengine::TextureState::Parameters lTextureStateParameters = {};
        lTextureStateParameters.muAddressU      = 2;
        lTextureStateParameters.muAddressV      = 2;
        lTextureStateParameters.muAddressW      = 2;
        lTextureStateParameters.muMagFilter     = 0;
        lTextureStateParameters.muMinFilter     = 0;
        lTextureStateParameters.muMipFilter     = 0;
        lTextureStateParameters.muMaxAnisotropy = 13;
        lTextureStateParameters.muField10       = 1;
        lTextureStateParameters.mfMipLodBias    = 0.0f;
        lTextureStateParameters.mu8Field43      = 1;
        lTextureStateParameters.mu8Field44      = 1;
        lTextureStateParameters.mpTexture       = lpColourCalibrationTexture;

        // Size + carve the texture-state resource, then initialise the state over it.
        //
        // ⚠️ CORRECTED THIS WAVE -- the console's carve is rw::IResourceAllocator::DoAllocate,
        // NOT a fifth vtable slot. The asm @0x8246ACDC-F8 is
        //     r11 = *(r25)            ; the allocator's vptr
        //     r5  = r3                ; the descriptor GetResourceDescriptor just filled
        //     r6  = 0                 ; the debug NAME argument
        //     r4  = r25               ; this
        //     r3  = &var_90           ; the hidden STRUCT-RETURN pointer
        //     r11 = *(r11 + 0x10) ; bctrl
        // PowerPC returns a >8-byte struct through an sret pointer in r3, so Hex-Rays'
        // four-argument `Allocate(out, this, descriptor, flags)` is really the two-argument
        // `rw::Resource DoAllocate(const ResourceDescriptor&, const char* name)` with the
        // out-param hoisted in front (AGENTS.md rule 4). The five words the console then
        // copies into this+0x424 (@0x8246ACFC-AD20) are that returned rw::Resource in its
        // X360 BaseResourceDescriptors<5> width.
        // The previous body reached vtable slot +0x10 through a hand-declared interface
        // slice. That is a GUEST vtable index: the host's rw::IResourceAllocator has FOUR
        // virtuals (dtor / DoAllocate / Free / DoFree), so index 4 is past the end of the
        // real vtable and the call would have jumped through whatever follows it. Reaching
        // DoAllocate BY NAME is both the faithful call and the only safe one, and it is the
        // same conversion the reviewed CgsAptRenderHandler texture-state carve performs.
        u32 lauDescriptor[10];
        renderengine::TextureState::GetResourceDescriptor(lauDescriptor);

        if (lpAllocator != NULL)
        {
            // GetResourceDescriptor writes the X360 5-entry form (10 words); the host's
            // rw::ResourceDescriptor is the 4-entry one. Slot 0 carries the whole
            // texture-state object; slots 1..3 are the console's own {0, 1} empties.
            rw::ResourceDescriptor lAllocDescriptor;
            lAllocDescriptor.m_baseResourceDescriptors[0].m_size      = lauDescriptor[0];
            lAllocDescriptor.m_baseResourceDescriptors[0].m_alignment = lauDescriptor[1];
            for (u32 luSlot = 1; luSlot < 4; ++luSlot)
            {
                lAllocDescriptor.m_baseResourceDescriptors[luSlot].m_size      = 0u;
                lAllocDescriptor.m_baseResourceDescriptors[luSlot].m_alignment = 1u;
            }
            mTextureStateResource = lpAllocator->DoAllocate(lAllocDescriptor, NULL);
        }
        else
        {
            // [FLAG PC bring-up] the console cannot reach here (GuiModule::Prepare caches a
            // real allocator before any Update runs). On PC the bank-42 allocator can be
            // absent if GameDataModule::CreateAllocators could not carve it; say so once and
            // carry on -- the committed PC renderengine::TextureState::Initialize does not
            // read lpResourceMemory at all (texturestate.cpp: it `new`s the state and keeps
            // the sampler config), so an uncarved resource costs correctness nothing here.
            CgsDev::Log::WriteToLog("[calib-screen] FLAG: no RW linear resource allocator "
                                    "(bank 42) -- texture-state resource not carved\n");
        }

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
        AddGuiOutEvent(lpGuiOutEvents, lControl);
        break;
    }

    case E_COLOURCALIBRATIONSCREENSTATE_PREPARE_TO_HIDE:
    {
        GuiOptionsBrightnessContrastPostFxControl lControl;
        lControl.mColourCalibrationTextureHandle = CgsResource::NULLResourceHandle;
        lControl.mbEnablePostFx                  = true;
        AddGuiOutEvent(lpGuiOutEvents, lControl);

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
