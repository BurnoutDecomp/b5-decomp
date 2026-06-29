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
    };
}

#endif // BRN_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H
