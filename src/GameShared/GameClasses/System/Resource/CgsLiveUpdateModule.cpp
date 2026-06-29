#include "GameShared/GameClasses/System/Resource/CgsLiveUpdateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsLiveUpdateModuleIO.h"  // LiveUpdateIO::OutputBuffer (Get/SetStatus)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"    // Events::LoadBundleRequest
#include "GameShared/GameClasses/System/GameTalk/CgsGameTalk.h"            // CgsGameTalk::GameTalk::SendMessage
#include "SDKs/EA/GameTalk/GameTalk.h"                                     // EA::GameTalk::GameTalkMessage

#include <cstring>   // strncpy, strlen

// ============================================================================
// CgsResource::LiveUpdateModule -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Construct          @ 0x828E6548
//   Destruct           @ 0x828E6708
//   Release            @ 0x828E6680
//   GameTalkMsgHandler @ 0x828DAF60
//   Update             @ 0x82903128
// See CgsLiveUpdateModule.h for the layout and behaviour overview.
// ============================================================================

namespace CgsResource
{
    // X360 off_830EA3F0 -- the process-wide live-update singleton.
    LiveUpdateModule* LiveUpdateModule::spInstance = nullptr;

    // ---- Construct @ 0x828E6548 ------------------------------------------------
    void LiveUpdateModule::Construct(CgsGameTalk::GameTalk* lpGameTalk)
    {
        // Only one live-update module may exist at a time.
        CGS_ASSERT(spInstance == nullptr, "spInstance==NULL");

        // Chain the base single-buffered module Construct.
        ModuleSingleBuffered::Construct();

        // Bind + Clear the receiver queue (X360 sets capacity 512, alignment 16, buffer
        // = queue + 0x18, then Clear -- exactly EventReceiverQueue<512,16>::Construct()).
        mReceiverQueue.Construct();

        // Publish the singleton and the reply channel; init the state fields.
        spInstance     = this;
        mpGameTalk     = lpGameTalk;
        miField273     = 0;
        meReleaseStage = 1;
    }

    // ---- Destruct @ 0x828E6708 -------------------------------------------------
    void LiveUpdateModule::Destruct()
    {
        mReceiverQueue.Clear();
        ModuleSingleBuffered::Destruct();
    }

    // ---- Release @ 0x828E6680 --------------------------------------------------
    bool LiveUpdateModule::Release()
    {
        // The release stage must be either fresh (0) or already-released (1).
        if (meReleaseStage == 0)
        {
            mReceiverQueue.Clear();
        }
        else if (meReleaseStage != 1)
        {
            CGS_ASSERT(false, "Invalid stage\n");
            return false;
        }

        miField273     = 0;
        meReleaseStage = 1;
        return true;
    }

    // ---- GameTalkMsgHandler @ 0x828DAF60 ---------------------------------------
    // Static EA::GameTalk::MessageHandler. The X360 Hex-Rays mis-resolves the two
    // message reads as CgsNetwork::ServerInterfaceComponent::GetLastError / sub_82837E08;
    // the call sites pass the GameTalkMessage `this` (r3), so they are message accessors:
    // GetState() (compared == 1) and GetKeyContent("ReplaceResourcesFromFile").
    void LiveUpdateModule::GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage)
    {
        if (lpMessage->GetState() != 1)
            return;

        const char* lpcFileName = lpMessage->GetKeyContent("ReplaceResourcesFromFile");
        if (lpcFileName == nullptr)
            return;

        // Ignore the request if one is already armed/running (X360: spInstance + 0x44C != 0).
        if (spInstance->miUpdateState != 0)
            return;

        // OUTER guard (asm 0x828DAFAC: bge -> return): an oversized name aborts WITHOUT copying or
        // arming -- leave macFileName/miUpdateState untouched. This is a real early-out, not just an assert.
        if (std::strlen(lpcFileName) >= 256)
            return;

        // The requested path must fit the 256-byte buffer (the CgsStringUtils.h bounded-copy assert).
        CGS_ASSERT(std::strlen(lpcFileName) < 256, "String too long");

        std::strncpy(spInstance->macFileName, lpcFileName, 256);
        spInstance->miUpdateState = 1;
    }

    // ---- Update @ 0x82903128 ---------------------------------------------------
    void LiveUpdateModule::Update(CgsModule::IOBuffer* lpOutputBuffer)
    {
        LiveUpdateIO::OutputBuffer* lpBuffer =
            static_cast<LiveUpdateIO::OutputBuffer*>(lpOutputBuffer);
        lpBuffer->LockForWrite();

        switch (miUpdateState)
        {
        case 1:
            // Just armed -- start the settle countdown.
            mcUpdateCounter = 0;
            // fall through into the settle/advance path
            [[fallthrough]];
        case 2:
        {
            mcUpdateCounter = (s8)(mcUpdateCounter + 1);
            miUpdateState   = 2;
            if (mcUpdateCounter < 5)
                break;

            // Settled: push the live-update LoadBundleRequest onto the request queue.
            CgsModule::VariableEventQueue<512, 16>* lpRequestQueue = lpBuffer->Get();

            Events::LoadBundleRequest lRequest;
            lRequest.mpUser              = &mReceiverQueue;
            lRequest.miEventId           = 0;
            lRequest.SetFileName(macFileName);
            lRequest.miPoolId            = -1;
            lRequest.mbLiveUpdateReplace = true;
            lRequest.mbAllowFailiure     = false;
            lRequest.mbUseHDCache        = false;

            // Event type 2 (live-update load-bundle request); element size 148 on the
            // X360 -- use sizeof for the x64 build (the queue stores by byte image).
            const bool lbQueued = lpRequestQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequest), 2,
                (s32)sizeof(Events::LoadBundleRequest));
            CGS_ASSERT(lbQueued, "lpRequestQueue->LoadBundle(lBundleLoadRequest)");

            miUpdateState = 3;
            // fall through into the await-response check
            [[fallthrough]];
        }
        case 3:
        {
            miUpdateState = 3;

            const s32 liNumResponses = mReceiverQueue.GetCount();
            if (liNumResponses > 0)
            {
                CGS_ASSERT(liNumResponses == 1, "mReceiverQueue.GetLength()==1");

                // Read the loader's response. The X360 reads the s32 at offset +140 of the
                // first event payload (the BundleLoaderEvent base's miPoolId slot, which the
                // response repurposes as the result code): >= 0 means the replace succeeded.
                const char* lpcResult = "failed";
                const CgsModule::Event* lpResponse = nullptr;
                s32 liSize = 0;
                if (mReceiverQueue.GetFirstEvent(&lpResponse, &liSize) >= 0 && lpResponse != nullptr)
                {
                    const Events::BundleLoaderEvent* lpLoaderResponse =
                        reinterpret_cast<const Events::BundleLoaderEvent*>(lpResponse);
                    if (lpLoaderResponse->miPoolId >= 0)
                        lpcResult = "success";
                }

                mReceiverQueue.Clear();
                miUpdateState = 0;

                // Report the outcome back to the authoring tool over GameTalk.
                EA::GameTalk::GameTalkMessage lMessage("LiveUpdate");
                lMessage.AddKeyContent("Result", 0, lpcResult, (s32)std::strlen(lpcResult));
                mpGameTalk->SendMessage("Tool", &lMessage);
            }
            break;
        }
        default:
            break;
        }

        // Publish whether a live-update is still in flight (miUpdateState != 0).
        const u8 lbInProgress = (miUpdateState != 0) ? 1u : 0u;
        lpBuffer->SetStatus(&lbInProgress);

        lpBuffer->UnlockForWrite();
    }
}
