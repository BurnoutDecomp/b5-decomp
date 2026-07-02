#include "GameSource/Gui/BrnGuiOverlaysDirector.h"

#include <cstring>   // std::memcpy (the buffered -> current overlay promote)

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStreamBase (operator<<)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::GuiOverlayRequest

// BrnGui::GuiOverlaysDirector -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, DWARF primary file GameSource/Gui/
// BrnGuiOverlaysDirector.cpp):
//   GuiOverlaysDirector::HandleOverlayRequest    @0x825162C8
//   GuiOverlaysDirector::HandleWaitFinishRequest @0x824F39E0
//   GuiOverlaysDirector::Update                  @0x82520668  (GuiModule::Update)
//
// Event ids on the module input queue (the Update dispatch):
//   64  the GuiCache-ready event (payload = GuiCache*)
//   184 an overlay request (GuiOverlayRequest)         -> HandleOverlayRequest
//   186 a full-info request                            -> publish mCurrentOverlay (187, 448B)
//   188 a wait-finish request                          -> HandleWaitFinishRequest
//   189 overlay hidden notification                    -> promote the buffered overlay
//   190 overlay showing notification                   -> bounce the stored wait-finish (188)
//   279 show-freeburn-intro request                    -> HandleShowFreeBurnIntroRequest
//   322 entering-online state                          -> drop a buffered Onl* overlay
// and on the output queue: 185 = overlay started (the request word), 187 = full info,
// 188 = wait finished.

namespace BrnGui
{
namespace
{
    // BrnGuiOverlaysDirector.cpp:23 (DWARF) -- how long the entering-online splash stays
    // up (the X360 stores 120 at the four-Onl*-id match).
    const s32 KI_FRAMES_TO_SHOW_ENTERING_FREEBURN = 120;

    // The four entering/returning-online overlay names the director special-cases.
    const char* KPC_ONLINE_OVERLAYS[4] =
    {
        "OnHReturnOn", "OnCReturnOn", "OnHEnterOn", "OnCEnterOn"
    };

    // True when the id names one of the four online transition overlays (the X360 emits
    // the four CgsIDCompress compares inline at each of the three sites).
    bool IsOnlineTransitionOverlay(CgsID lOverlayId)
    {
        return lOverlayId == CgsIDCompress(KPC_ONLINE_OVERLAYS[0]) ||
               lOverlayId == CgsIDCompress(KPC_ONLINE_OVERLAYS[1]) ||
               lOverlayId == CgsIDCompress(KPC_ONLINE_OVERLAYS[2]) ||
               lOverlayId == CgsIDCompress(KPC_ONLINE_OVERLAYS[3]);
    }
}

// @ 0x825162C8
void GuiOverlaysDirector::HandleOverlayRequest(const GuiOverlayRequest* lpRequest)
{
    CGS_ASSERT(mpController != NULL, "mpController");

    if (mbInOverlay)
    {
        // Already showing one: park the request in the buffered slot (warning when that
        // overwrites an as-yet-unshown queued overlay; category bit 0 of the message
        // filter gates the debug print, exactly as the X360 does).
        if (mBufferedOverlay.mNameId != 0)
        {
            char lacRequestName[16];
            CgsIDConvertToString(lpRequest->GetOverlayId(), lacRequestName);
            lacRequestName[12] = 0;   // the X360 truncates the printable id at 12 chars

            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "WARNING - OVERWRITING A QUEUED OVERLAY!\n    Adding overlay named "
                    << lacRequestName
                    << " over queued overlay "
                    << mBufferedOverlay.macName
                    << "\n\n";
            }
        }
        SetUpOverlayInfo(&mBufferedOverlay, lpRequest);
    }
    else
    {
        // Idle: make it the current overlay and announce it (event 185 carries the
        // popup style word -- DWARF-reconciled name; the X360 posts the response's
        // +0x18 word).
        SetUpOverlayInfo(&mCurrentOverlay, lpRequest);
        u32 luStyle = static_cast<u32>(mCurrentOverlay.meStyle);
        mbInOverlay = true;
        mOutputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&luStyle), 185, 4);
    }
}

// @ 0x824F39E0
void GuiOverlaysDirector::HandleWaitFinishRequest(const GuiOverlayWaitFinishRequest* lpRequest)
{
    if (!mbInOverlay)
        return;

    // Cancel a matching buffered overlay outright.
    if (mBufferedOverlay.mNameId == lpRequest->mOverlayId)
        mBufferedOverlay.mNameId = 0;

    if (mCurrentOverlay.mNameId == lpRequest->mOverlayId)
    {
        if (mbIsWaitRequestValid)
        {
            // cpp:392 -- the X360 streams both printable ids into the message; folded
            // static per convention.
            CGS_ASSERT(lpRequest->mOverlayId == mWaitEndRequestId,
                       "Received a finish wait request when one is already stored");
        }
        else
        {
            mbIsWaitRequestValid = true;
            mWaitEndRequestId    = lpRequest->mOverlayId;
        }
    }
}

// @ 0x82520668
void GuiOverlaysDirector::Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer)
{
    // The whole update is gated on a bound controller.
    if (!mpController)
        return;

    CGS_ASSERT(lpInputBuffer != NULL,
               "Input buffer is not valid. Is it definately locked for reading?");
    mpGuiInputBuffer = lpInputBuffer;

    CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpEventQueue =
        lpInputBuffer->GetGuiEvents();
    CGS_ASSERT(lpEventQueue != NULL, "lpEventQueue != NULL");

    // ---- drain the inbound GUI events ----
    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liEventId = lpEventQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != NULL)
    {
        switch (liEventId)
        {
        case 64:   // the GuiCache is ready (payload = GuiCache*)
            CGS_ASSERT(lpEvent != NULL, "lpCacheEvent");
            mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
            break;

        case 184:  // an overlay request
            HandleOverlayRequest(reinterpret_cast<const GuiOverlayRequest*>(lpEvent));
            break;

        case 186:  // full-info request: publish the current overlay description
            mOutputQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&mCurrentOverlay),
                                  187, sizeof(GuiOverlayFullInfoResponse));
            break;

        case 188:  // wait-finish request
            HandleWaitFinishRequest(
                reinterpret_cast<const GuiOverlayWaitFinishRequest*>(lpEvent));
            break;

        case 189:  // the current overlay finished hiding
        {
            const GuiOverlayHiddenNotification* lpHidden =
                reinterpret_cast<const GuiOverlayHiddenNotification*>(lpEvent);
            if (lpHidden->mOverlayId == mCurrentOverlay.mNameId)
            {
                mbInOverlay          = false;
                mbIsWaitRequestValid = false;

                if (mBufferedOverlay.mNameId != 0)
                {
                    // Promote the buffered overlay; an online transition overlay also
                    // arms the entering-online splash countdown.
                    if (IsOnlineTransitionOverlay(mBufferedOverlay.mNameId))
                        miFramesToShowEnteringOnline = KI_FRAMES_TO_SHOW_ENTERING_FREEBURN;

                    std::memcpy(&mCurrentOverlay, &mBufferedOverlay,
                                sizeof(GuiOverlayFullInfoResponse));
                    u32 luStyle = static_cast<u32>(mCurrentOverlay.meStyle);
                    mBufferedOverlay.mNameId = 0;
                    mbInOverlay = true;
                    mOutputQueue.AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&luStyle), 185, 4);
                }
            }
            break;
        }

        case 190:  // the current overlay finished showing
        {
            const GuiOverlayShowingNotification* lpShowing =
                reinterpret_cast<const GuiOverlayShowingNotification*>(lpEvent);
            if (mbIsWaitRequestValid && lpShowing->mOverlayId == mWaitEndRequestId)
            {
                // Bounce the stored wait-finish downstream now the overlay is up.
                mOutputQueue.AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&mWaitEndRequestId), 188, 8);
            }
            break;
        }

        case 279:  // show the freeburn intro
            HandleShowFreeBurnIntroRequest(
                reinterpret_cast<const GuiEventNetworkShowFreeBurnIntro*>(lpEvent));
            break;

        case 322:  // entering the online state: drop a buffered online-transition overlay
            if (mBufferedOverlay.mNameId != 0 &&
                IsOnlineTransitionOverlay(mBufferedOverlay.mNameId))
            {
                mBufferedOverlay.mNameId = 0;
            }
            break;

        default:
            break;
        }

        liEventId = lpEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }

    // ---- the entering-online splash countdown: on expiry, wave all four online
    //      transition overlays through their wait-finish. ----
    if (miFramesToShowEnteringOnline >= 0)
    {
        if (--miFramesToShowEnteringOnline < 0)
        {
            miFramesToShowEnteringOnline = -1;

            GuiOverlayWaitFinishRequest lRequest;
            for (s32 li = 0; li < 4; ++li)
            {
                lRequest.Construct(KPC_ONLINE_OVERLAYS[li]);
                mOutputQueue.AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest), 188, 8);
            }
        }
    }

    mpGuiInputBuffer = NULL;
}
}
