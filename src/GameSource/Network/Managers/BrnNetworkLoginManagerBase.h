#pragma once

// ===================================================================================
// BrnNetwork::LoginManagerBase -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkLoginManagerBase.{h,cpp}
//
// The network login state machine base. Driven each frame by BrnNetwork::StateManager,
// it walks the player from "not signed in" to "fully online" over the DirtySock server
// interface: connect to the online service, connect to the DirtySock (DS) server, log in,
// download the terms-of-service / configuration data, wait for the assigned player id,
// connect telemetry, download scoreboard headers, load settings and ping the regions.
// The platform-specific sign-in front end and the user-name/password supply are virtuals
// overridden by the per-platform leaf (BrnNetwork::LoginManagerX360).
//
// LAYOUT recovered from the X360 ARTIST asm (AnswerAgreeTOS @ 0x82566478 pins the byte
// offsets used by every sibling -- mpNetworkManager @ +4, mpNetworkModule @ +8,
// meSubState @ +0xC, mpTOS @ +0x20) and the DecFIGS DWARF (member types / names / the
// three enums / the virtual set). 32-bit X360 offsets:
//   +0x00 (4)  vptr
//   +0x04 (4)  mpNetworkManager        BrnNetworkManager*   (Construct: GetNetworkManager())
//   +0x08 (4)  mpNetworkModule         BrnNetworkModule*    (Construct a2)
//   +0x0C (4)  meSubState              ESubState            (the state-machine cursor)
//   +0x10 (4)  meSignInState           ESignInState
//   +0x14 (4)  meSignInType            ESignInType
//   +0x18 (4)  meCompletedSignInType   ESignInType
//   +0x1C (1)  mbAgreeShare1           bool
//   +0x1D (1)  mbAgreeShare2           bool
//   +0x20 (4)  mpTOS                   UnicodeBuffer::CgsUtf8*  (downloaded terms-of-service)
//
// The server-interface components the asm reaches as raw *(mpNetworkManager+0x38EC+8*N)
// slots are accessed BY NAME through the committed accessor convention
// (mpNetworkManager->GetServerInterface()->Get<Component>Component() / ->GetStatus(N) /
// ->IsSuspended() -- see BrnNetworkAutoLoginManager / BrnNetworkConnectionManager). The
// 32-bit slot offsets are NOT reproduced as raw pointer hacks.
//
// Naming follows references/CXX_NAMING_CONVENTIONS.md. Every enum value / constant is
// grounded in a stored immediate or cmpwi operand from the asm.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // CgsUnicode::CgsUtf8

namespace BrnNetwork
{
    class BrnNetworkManager;
    class BrnNetworkModule;

    namespace BrnNetworkModuleIO
    {
        struct PostSimulationInputBuffer;   // Update / PlatformSpecificUpdate param (pointer only)
    }

    class LoginManagerBase
    {
    public:
        // BrnNetworkLoginManagerBase.h:58 -- which sign-in flavour was requested.
        enum ESignInType
        {
            E_SIGN_IN_TYPE_NORMAL          = 0,
            E_SIGN_IN_TYPE_SILENT          = 1,
            E_SIGN_IN_TYPE_FREE_BURN_LOBBY = 2,
            E_SIGN_IN_TYPE_COUNT           = 3
        };

        // BrnNetworkLoginManagerBase.h:138 -- the overall sign-in result the front end polls.
        enum ESignInState
        {
            E_SIGN_IN_STATE_SIGNING_IN = 0,
            E_SIGN_IN_STATE_SUCCESS    = 1,
            E_SIGN_IN_STATE_FAILED     = 2,
            E_SIGN_IN_STATE_COUNT      = 3
        };

        // BrnNetworkLoginManagerBase.h:147 -- the login state-machine cursor (the Update switch).
        enum ESubState
        {
            E_SUBSTATE_PLATFORM_SPECIFIC_UPDATE     = 0,
            E_SUBSTATE_CONNECTING_ONLINE            = 1,
            E_SUBSTATE_CONNECTING_DS                = 2,
            E_SUBSTATE_LOGGING_IN                   = 3,
            E_SUBSTATE_DOWNLOADING_TOS              = 4,
            E_SUBSTATE_GET_CONFIGURATION_DATA       = 5,
            E_SUBSTATE_WAITING_FOR_PLAYER_ID        = 6,
            E_SUBSTATE_CONNECTING_TELEMETRY         = 7,
            E_SUBSTATE_DOWNLOADING_SCOREBOARD_HEADERS = 8,
            E_SUBSTATE_WAITING_SELECTION            = 9,
            E_SUBSTATE_RESUMING_SERVER_INTERFACE    = 10,
            E_SUBSTATE_LOAD_SETTINGS                = 11,
            E_SUBSTATE_PING_REGIONS                 = 12,
            E_SUBSTATE_NO_AGREEMENT                 = 13,
            E_SUBSTATE_DONE                         = 14,
            E_SUBSTATE_COUNT                        = 15
        };

        // ---- lifecycle (X360 @ 0x82543808 / .cpp:58 etc.) ---------------------------------
        void Construct(BrnNetworkModule* lpNetworkModule);
        bool Prepare();   // DWARF .cpp:86 -- no asm in this export (declaration-only).
        bool Release();   // DWARF .cpp:109 -- no asm in this export (declaration-only).
        void Destruct();  // DWARF .cpp:129 -- no asm in this export (declaration-only).

        // X360 0x8256CC60 -- per-frame tick; dispatches on meSubState. Returns true once the
        // flow has reached E_SUBSTATE_DONE.
        bool Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);

        // X360 0x8254F788 -- begin (or re-begin) a sign-in of the requested type. Virtual: the
        // per-platform leaf overrides to drive its own sign-in front end before chaining here.
        virtual void Start(ESignInType leSignInType);

        // X360 0x82543D18 -- the server interface signalled a disconnect: reset to the idle state
        // and free the cached terms-of-service.
        virtual void Disconnected();

        // ---- front-end answers (driven by BrnNetwork::StateManager::ProcessGuiEvents) ------
        void AnswerCreateAccount(bool lbAgree);          // DWARF .cpp:874 -- no asm (decl-only).
        void AnswerShareInfo(bool lbAgree1, bool lbAgree2); // DWARF .cpp:898 -- no asm (decl-only).

        // X360 0x82543B30 -- user (dis)agreed to open a US account.
        void AnswerOpenUsAccount(bool lbAgree);
        // X360 0x82566478 -- user (dis)agreed to the terms of service.
        void AnswerAgreeTOS(bool lbAgree);
        // X360 0x8254FDD0 -- user chose to restart sign-in (true) or abandon it (false) from the
        // "no agreement" screen.
        void AnswerNoAgreement(bool lbRestart);
        // X360 0x82543BB8 -- the platform sign-in completed (success/failure).
        void AnswerSignIn(bool lbSuccess);

        void AnswerChatRestricted(bool lbAcknowledged); // DWARF .cpp:1018 -- no asm (decl-only).

        // X360 0x8254FE60 -- the user cancelled the login flow.
        void CancelLogin();

        // ---- queries (inline; DWARF .h:282/288/294) ---------------------------------------
        bool IsSigningIn() const
        {
            return meSignInState == E_SIGN_IN_STATE_SIGNING_IN;
        }

        ESignInType GetSignInType() const
        {
            return meSignInType;
        }

        ESignInType GetCompletedSignInType() const
        {
            return meCompletedSignInType;
        }

        void FinishedWithTOS();   // DWARF .cpp:1247 -- no asm in this export (declaration-only).

    protected:
        // X360 .h:179 -- the per-platform sign-in front end step (LoginManagerX360 overrides).
        virtual void PlatformSpecificUpdate(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);
        // X360 .h:184 -- supply the user name / password for the DS login (LoginManagerX360 overrides).
        virtual void GetUserNameAndPassword(char* lpcUserName, char* lpcPassword);

        // X360 0x82543C30 -- end the flow (disconnecting on failure), recording the completed type.
        void Finished(bool lbSuccess);
        void ShowSignInGui();   // DWARF .cpp:1054 -- no asm in this export (declaration-only).

    private:
        // X360 0x82543AD0 -- ask the platform for credentials and issue the DS log-in.
        void LogInToServer();
        // X360 0x825438A8 -- kick off the terms-of-service HTTPS download.
        void PrepareDownloadingTOS();
        void PrepareDownloadingNews();   // DWARF .h:209 -- no asm in this export (declaration-only).
        // X360 0x8254FEC8 -- configure + connect the telemetry component.
        void PrepareConnectTelemetry();
        void PrepareDownloadingScoreboardHeadings(); // DWARF .cpp:1189 -- no asm (decl-only).
        // X360 0x82543D88 -- start loading the player settings.
        void PrepareLoadSettings();
        // X360 0x82543E08 -- start pinging the regions.
        void PreparePingRegions();

        // X360 0x82543E88 -- (private virtual) end the flow because chat is restricted.
        virtual void ShowChatRestrictionPopup();

        void SetTelemetryCountryList();  // DWARF .cpp:1279 -- no asm in this export (decl-only).
        void SendUPnPTelemetry();        // DWARF .cpp:1297 -- no asm in this export (decl-only).

        // ---- per-substate update steps (dispatched by Update) -----------------------------
        void UpdateConnectingOnline();   // X360 0x8254F880
        void UpdateConnectingDS();       // X360 0x8254F938
        void UpdateLoggingIn();          // X360 0x8254FA10
        void UpdateDownloadingTOS();     // X360 0x82566320
        void UpdateGetConfigurationData(); // X360 0x82543938
        void UpdateWaitingForPlayerID(); // X360 0x82561A10
        void UpdateConnectingTelemetry(); // X360 0x8254FC50
        void UpdateResumingServerInterface(); // DWARF .cpp:833 -- no asm (decl-only).
        void UpdateDownloadingScoreboardHeadings(); // DWARF .cpp:699 -- no asm (decl-only).
        void UpdateLoadSettings();       // X360 0x8254FD18
        void UpdatePingRegions();        // X360 0x82543A08

        // X360 0x82543C90 -- SuspensionManager::Resume completion callback (static; the user-data
        // pointer is the LoginManagerBase). On success it re-runs Finished(true) via the vtable;
        // on failure it disconnects and forces E_SUBSTATE_DONE.
        static void ResumeCompleteCallback(bool lbSuccess, void* lpUserData);

    protected:
        // ---- data members (X360 offsets in the header note) -------------------------------
        // Exposed protected (not private) so the per-platform leaf (BrnNetwork::LoginManagerX360)
        // can store the state-machine cursor / sign-in result members BY NAME from its own
        // lifecycle and update overrides (X360 stores into *(this+0x04..0x20)). The X360-only
        // mePlatformSpecificSubState member is added by the leaf, not here.
        BrnNetworkManager*       mpNetworkManager;       // +0x04
        BrnNetworkModule*        mpNetworkModule;        // +0x08
        ESubState                meSubState;             // +0x0C
        ESignInState             meSignInState;          // +0x10
        ESignInType              meSignInType;           // +0x14
        ESignInType              meCompletedSignInType;  // +0x18
        bool                     mbAgreeShare1;          // +0x1C
        bool                     mbAgreeShare2;          // +0x1D
        CgsUnicode::CgsUtf8*     mpTOS;                  // +0x20
    };
}
