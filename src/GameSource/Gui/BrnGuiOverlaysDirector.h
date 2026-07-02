#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"        // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h" // CgsGuiModuleIO::InputBuffer (+ the 18432 queue type)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"        // GuiOverlayFullInfoResponse (moved to its DWARF home)

namespace CgsGui { class ModelModule; }

// BrnGui::GuiOverlaysDirector - routes overlay show/hide traffic between the game and
// the overlay GUI flow: it queues one pending overlay (current + one buffered), forwards
// full-info requests, tracks the wait-to-finish handshake, and manages the entering-
// online splash timing. DWARF home BrnGuiOverlaysDirector.h:43. This TU bodies
// HandleOverlayRequest / HandleWaitFinishRequest / Update; the rest of the surface is
// declaration-only (their own ledger functions).
namespace BrnGui
{
    class GuiCache;
    struct PopupController;                 // held by const pointer only (own home pending)
    struct GuiEventNetworkShowFreeBurnIntro;// declaration-only consumer param (own home pending)

    // GuiOverlayFullInfoResponse moved to its DWARF home (BrnGuiEventTypeDefs.h:5768) with
    // the full DWARF-named layout when BrnBaseOverlayState landed (its SetupOverlay pins
    // meIcon/macTitleId/macMessageId/maMessageParams; the old partial names here --
    // mOverlayId/macOverlayName/muOverlayRequestId -- were reconciled to the DWARF's
    // mNameId/macName/meStyle at the same X360 offsets).

    // The wait-finish handshake record (events 188; X360 8-byte record).
    struct GuiOverlayWaitFinishRequest
    {
        CgsID mOverlayId;   // +0x00

        // @ the X360 GuiOverlayWaitFinishRequest::Construct the Update tail drives
        // four times: compress the overlay name into the id. Defined inline (the whole
        // attested body).
        GuiOverlayWaitFinishRequest* Construct(const char* lpcOverlayName)
        {
            mOverlayId = CgsIDCompress(lpcOverlayName);
            return this;
        }
    };

    // The overlay-hidden (event 189) / overlay-showing (event 190) notifications: both
    // lead with the overlay id (the X360 Update compares their +0x00 qword). The
    // showing notification is POSTED by BrnGui::BaseOverlayState::Update through the
    // OutputGuiEvent<GuiOverlayShowingNotification> instantiation @0x824B2C78 (an
    // 8-byte record, id 190 supplied by GetEventType) -- hence the accessor.
    struct GuiOverlayHiddenNotification  { CgsID mOverlayId; };
    struct GuiOverlayShowingNotification : public CgsModule::Event   // empty base; record stays the bare 8-byte id
    {
        CgsID mOverlayId;
        s32 GetEventType() const { return 190; }
    };

    struct GuiOverlaysDirector
    {
        // DWARF h:48/h:56/h:60 -- declaration-only (their own ledger functions).
        void Construct(CgsGui::ModelModule* lpModelModule);
        void BridgeOutEvents(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer);
        void SetController(const PopupController* lpController);

        // @0x82520668 (this TU, DWARF h:52) -- drain the GUI module input queue and
        // dispatch the overlay traffic (called by BrnGui::GuiModule::Update).
        void Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer);

    private:
        // @0x825162C8 / @0x824F39E0 (this TU, DWARF h:81/h:89).
        void HandleOverlayRequest(const GuiOverlayRequest* lpRequest);
        void HandleWaitFinishRequest(const GuiOverlayWaitFinishRequest* lpRequest);

        // DWARF h:85/h:93/h:99/h:103/h:107 -- declaration-only (their own ledger functions).
        void HandleOverlayFullInfoRequest();
        void HandleOverlayShowingNotification(const GuiOverlayShowingNotification* lpNotification);
        void SetUpOverlayInfo(GuiOverlayFullInfoResponse* lpInfo, const GuiOverlayRequest* lpRequest);
        void StartCurrentOverlay();
        void HandleShowFreeBurnIntroRequest(const GuiEventNetworkShowFreeBurnIntro* lpRequest);

        // ---- members (DWARF h:64-77; X360 offsets in the .cpp notes) ----
        const PopupController*                   mpController;                    // +0x00
        CgsGui::CgsGuiModuleIO::InputBuffer*     mpGuiInputBuffer;                // +0x04
        CgsGui::ModelModule*                     mpModelModule;                   // +0x08
        GuiOverlayFullInfoResponse               mCurrentOverlay;                 // +0x10
        GuiOverlayFullInfoResponse               mBufferedOverlay;                // +0x1D0
        CgsModule::VariableEventQueue<18432, 16> mOutputQueue;                    // +0x390 (DWARF: InputBuffer::GuiEventQueue)
        GuiCache*                                mpGuiCache;                      // +0x4BA0
        bool                                     mbInOverlay;                     // +0x4BA4
        CgsID                                    mWaitEndRequestId;               // +0x4BA8
        bool                                     mbIsWaitRequestValid;            // +0x4BB0
        s32                                      miFramesToShowEnteringOnline;    // +0x4BB4
    };
}
