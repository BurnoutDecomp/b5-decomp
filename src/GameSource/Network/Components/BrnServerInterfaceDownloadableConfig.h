#ifndef BRN_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H
#define BRN_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// BrnNetwork::BrnServerInterfaceDownloadableConfig
//   Home: GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.{h,cpp}
//
// Thin Burnout-side "downloadable config" server-interface component. It is a
// leaf derivative of the committed CgsNetwork::ServerInterfaceComponent base
// (the polymorphic component base shared by every DirtySock server-interface
// component). BrnServerInterfaceBase embeds one of these as mDownloadableConfig.
//
// The X360 build's deleting destructor (@ 0x827DE378) shows the component owns
// no heap members of its own: the body restores only the single base vtable
// slot at this+0 (off_820CDBF8 -- the same component base vtable used by every
// CgsNetwork component leaf) and conditionally frees:
//
//     *result = &off_820CDBF8;            // restore the component base vtable
//     if ( a2 & 1 ) operator delete(result);
//     return result;
//
// MSVC synthesises exactly that store + conditional-free from the trivial
// virtual ~BrnServerInterfaceDownloadableConfig(). No own data members are
// declared because the destructor touches none and the dossier exposes none;
// any genuine config-cache members would be added additively when their
// owning getters/setters are reconstructed.
// ===========================================================================

namespace BrnNetwork
{
    class BrnServerInterfaceDownloadableConfig : public CgsNetwork::ServerInterfaceComponent
    {
    public:
        BrnServerInterfaceDownloadableConfig();

        // Vector deleting destructor @ 0x827DE378.
        virtual ~BrnServerInterfaceDownloadableConfig();

        // ---- ADDITIVE GROW (BrnNetworkAutoLoginManager TU) --------------------------------
        // The downloaded server config carries the auto-login timeout (seconds) the
        // AutoLoginManager arms its wait/connect timer from (the X360 reads it as the f32 at
        // this+0x350 in AutoLoginManager::Connect / ::UpdateWaitAutoLogin). Declared-only; the
        // backing config-cache member and the body land when this component's own config-load
        // TU is reconstructed.
        f32 GetAutoLoginTimeout() const;

        // ---- ADDITIVE GROW (BrnNetworkEventScoresManager TU) ------------------------------
        // The downloaded retry interval (seconds) the event-scores manager re-arms its upload
        // timer with after a successful batch upload (X360 _UploadEventScoreCallback @ 0x825654A0:
        // lfs f1, 0x34C(downloadableConfig) then CgsSystem::Time::SetFloatVal). Read as the f32 at
        // this+0x34C. Declared-only; the backing config-cache member and the body land with this
        // component's own config-load TU. FLAG: re-home onto a real member once the storage exists.
        f32 GetEventScoreUploadRetryInterval() const;

        // ---- ADDITIVE GROW (BrnNetworkBuddyManagerBase TU) --------------------------------
        // The downloaded retry delay (seconds) the buddy manager arms its
        // mTimeUntilRetryAfterFailedBuddyUpload timer with after a failed server-buddy
        // download/upload (X360 _GetFriendsListFromServerComplete @ 0x82549C58: lfs from
        // this+0x354 of the downloadable-config component then CgsSystem::Time::SetFloatVal).
        // Read as the f32 at this+0x354. Declared-only; the backing config-cache member and
        // the body land with this component's own config-load TU.
        f32 GetBuddyUploadRetryDelay() const;

        // ---- ADDITIVE GROW (BrnNetworkLoginManagerBase TU) --------------------------------
        // The login state machine drives the downloadable-config component through several entry
        // points (X360 reaches the embedded config component as *(mpNetworkManager+0x38EC) + the
        // downloadable-config slot):
        //
        //   Suspend() -- LoginManagerBase::AnswerAgreeTOS @ 0x82566478 / ::CancelLogin @ 0x8254FE60
        //                call CgsNetwork::ServerInterfaceDownloadableConfig::Suspend on the slot to
        //                abort any in-flight config download. (The behavioural body lives on the
        //                Cgs base @ 0x8287AD50; declared here so the slot type the game embeds
        //                exposes it by name.)
        //
        //   GetConfigDataFromNews(liNewsIndex) -- LoginManagerBase::UpdateLoggingIn @ 0x8254FA10
        //                and ::UpdateGetConfigurationData re-request the configuration data from the
        //                news feed (every recovered call site passes the literal 8). Declared-only;
        //                the body lands with this component's own config-load TU.
        //
        //   GetTelemetryDisabledList() -- LoginManagerBase::PrepareConnectTelemetry @ 0x8254FEC8
        //                returns the parsed telemetry-disabled country list the telemetry component
        //                is configured with. Declared-only; the body lands with the config-load TU.
        //
        //   GetTelemetryFirstUsageEventFilters() / GetTelemetryNormalUsageEventFilters() --
        //                PrepareConnectTelemetry passes the two parsed event-filter blobs (the X360
        //                +280 / +536 config-cache fields) to ServerInterfaceTelemetry::SetEventFilters.
        //                Declared-only; the backing members and bodies land with the config-load TU.
        //
        //   GetTosDownloadBufferSize() -- LoginManagerBase::PrepareDownloadingTOS @ 0x825438A8
        //                passes the parsed TOS download buffer size (the X360 +804 config-cache field)
        //                to ServerInterfaceHttp::StartHttpsDownload. Declared-only; the backing member
        //                and body land with the config-load TU.
        void       Suspend();
        void       GetConfigDataFromNews(s32 liNewsIndex);
        s32        GetTelemetryDisabledList() const;
        const u8*  GetTelemetryFirstUsageEventFilters() const;
        const u8*  GetTelemetryNormalUsageEventFilters() const;
        s32        GetTosDownloadBufferSize() const;
    };
}

#endif // BRN_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H
