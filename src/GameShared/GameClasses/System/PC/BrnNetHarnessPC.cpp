// ============================================================================
// BrnNetHarnessPC.cpp -- the two-instance LAN test harness (see BrnNetHarnessPC.h).
//
// [PC HARNESS, NOT X360] Test scaffolding for the PC build only. It posts the same GUI events
// the front end's online menus post, and prints bounded [net] witness lines; it never touches
// network state directly. DELETE-WHEN the PC front end can take two instances online by itself
// under the unattended harness.
// ============================================================================

#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"
#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                         // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                                                 // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                                    // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                              // EGameModeType
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                               // GuiEventNetworkConnect / QuickMatch
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"                                     // GuiEventNetworkCreateGame
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnServerInterface.h"
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"
#include "GameSource/Network/Managers/X360/BrnNetworkLoginManagerX360.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    // The channel the GUI's OutputGuiEvent records travel on (the network StateManager unwraps
    // it: word 1 is the event id, word 2 the payload offset).
    const s32 KI_GUI_CHANNEL_OUT = 40;

    // GUI event 272's payload word: a full (not silent) sign-in.
    const s32 KI_CONNECT_TYPE_FULL = 0;

    // GUI event 256's options for the host's game.
    const s32  KI_CREATE_SECURITY = 2;
    const bool KB_CREATE_RANKED   = false;

    // GUI event 251's options for the joiner.
    const bool KB_QUICK_MATCH_RANKED   = false;
    const bool KB_QUICK_MATCH_FREEBURN = true;

    // Pacing (seconds) and retry budgets.
    const f64 KF_DEFAULT_DELAY_S     = 30.0;
    const f64 KF_LOGIN_RETRY_S       = 15.0;
    const s32 KI_MAX_LOGIN_POSTS     = 4;
    const f64 KF_SETTLE_AFTER_LOGIN_S = 2.0;
    const f64 KF_CREATE_RETRY_S      = 10.0;
    const s32 KI_MAX_CREATE_POSTS    = 3;
    const f64 KF_QUICK_MATCH_RETRY_S = 5.0;
    const s32 KI_MAX_QUICK_MATCH_POSTS = 40;

    // Witness bookkeeping.
    const s32 KI_MAX_WITNESS_SITES   = 32;
    const s32 KI_WITNESS_SITE_NAME   = 24;
    const s32 KI_WITNESS_TOTAL_LINES = 600;
    const s32 KI_PLAYER_LIST_ENTRIES = 8;

    enum ERole
    {
        E_ROLE_NONE = 0,
        E_ROLE_HOST,
        E_ROLE_JOIN,
    };

    enum EStage
    {
        E_STAGE_ARMED = 0,       // waiting for the start delay and an idle network state manager
        E_STAGE_WAIT_LOGIN,      // 272 posted
        E_STAGE_READY,           // signed in; create / quick match next
        E_STAGE_WAIT_GAME,       // 256 or 251 posted
        E_STAGE_IN_GAME,
        E_STAGE_GAVE_UP,
    };

    struct HarnessState
    {
        bool   mbResolved;
        ERole  meRole;
        f64    mfDelay;
        bool   mbStarted;
        std::chrono::steady_clock::time_point mStart;
        EStage meStage;
        f64    mfStageTime;
        f64    mfLastPost;
        s32    miLoginPosts;
        s32    miGamePosts;
        s32    miLastPlayers;

        // The network manager's state as Observe() saw it after the last network update.
        bool   mbObserved;
        bool   mbLoggedIn;
        bool   mbSigningIn;
        bool   mbIdle;
        bool   mbInGame;
        bool   mbHost;
        s32    miPlayersInGame;
    };

    HarnessState gHarness = {};

    struct WitnessSite
    {
        char mac[KI_WITNESS_SITE_NAME];
        s32  miLines;
    };

    WitnessSite gaWitnessSites[KI_MAX_WITNESS_SITES] = {};
    s32         giWitnessLinesLeft = KI_WITNESS_TOTAL_LINES;
    char        gacLastPlayerList[256] = {};

    // The GUI player-list record entry (GameBridgeNetworkToX.cpp builds it at these offsets).
    struct GuiPlayerListEntry
    {
        s32  miPlayerID;     // +0x00
        char macName[16];    // +0x04, not always NUL-terminated
    };
    static_assert(sizeof(GuiPlayerListEntry) == 20, "player-list entries are 20 bytes apart");

    bool EnvFlag(const char* lpcName)
    {
        const char* lpcValue = std::getenv(lpcName);
        return lpcValue != nullptr && lpcValue[0] != '\0' && !(lpcValue[0] == '0' && lpcValue[1] == '\0');
    }

    void Resolve()
    {
        if (gHarness.mbResolved)
        {
            return;
        }
        gHarness.mbResolved = true;
        const bool lbHost = EnvFlag("BRN_NET_HOST");
        const bool lbJoin = EnvFlag("BRN_NET_JOIN");
        gHarness.meRole  = lbHost ? E_ROLE_HOST : (lbJoin ? E_ROLE_JOIN : E_ROLE_NONE);
        gHarness.mfDelay = KF_DEFAULT_DELAY_S;
        const char* lpcDelay = std::getenv("BRN_NET_DELAY");
        if (lpcDelay != nullptr && lpcDelay[0] != '\0')
        {
            char* lpcEnd = nullptr;
            const f64 lfDelay = std::strtod(lpcDelay, &lpcEnd);
            if (lpcEnd != nullptr && *lpcEnd == '\0' && lfDelay >= 0.0 && lfDelay <= 600.0)
            {
                gHarness.mfDelay = lfDelay;
            }
        }
        if (gHarness.meRole != E_ROLE_NONE)
        {
            BrnNetHarnessPC::Witness("harness", "armed role=%s%s name=%s ident=%08X delay=%.1fs",
                                     lbHost ? "host" : "join", (lbHost && lbJoin) ? " (BRN_NET_JOIN ignored)" : "",
                                     CgsPcNetIdentityName(), CgsPcNetIdentityLobbyIdent(), gHarness.mfDelay);
        }
    }

    void SetStage(EStage leStage, f64 lfNow)
    {
        gHarness.meStage     = leStage;
        gHarness.mfStageTime = lfNow;
    }

    bool PostConnect(CgsModule::VariableEventQueue<18432, 16>* lpQueue)
    {
        // GuiEventNetworkConnect already carries the channel-40 record header (size, id 272,
        // payload offset) in front of its payload word.
        const BrnGui::GuiEventNetworkConnect lConnectEvent(KI_CONNECT_TYPE_FULL);
        return lpQueue->AddEvent(&lConnectEvent, KI_GUI_CHANNEL_OUT, static_cast<s32>(sizeof(lConnectEvent)));
    }

    bool PostCreateGame(CgsModule::VariableEventQueue<18432, 16>* lpQueue)
    {
        BrnGui::GuiEventNetworkCreateGame lCreateEvent;
        lCreateEvent.Construct();
        lCreateEvent.meGameMode = BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
        lCreateEvent.meSecurity = KI_CREATE_SECURITY;
        lCreateEvent.mbRanked   = KB_CREATE_RANKED;
        CgsGui::GuiEventWrapper<BrnGui::GuiEventNetworkCreateGame, KI_GUI_CHANNEL_OUT> lRecord(lCreateEvent);
        return lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_GUI_CHANNEL_OUT,
                                 static_cast<s32>(sizeof(lRecord)));
    }

    bool PostQuickMatch(CgsModule::VariableEventQueue<18432, 16>* lpQueue)
    {
        BrnGui::GuiEventNetworkQuickMatch lQuickMatchEvent;
        lQuickMatchEvent.mbRanked   = KB_QUICK_MATCH_RANKED;
        lQuickMatchEvent.mbFreeburn = KB_QUICK_MATCH_FREEBURN;
        CgsGui::GuiEventWrapper<BrnGui::GuiEventNetworkQuickMatch, KI_GUI_CHANNEL_OUT> lRecord(lQuickMatchEvent);
        return lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_GUI_CHANNEL_OUT,
                                 static_cast<s32>(sizeof(lRecord)));
    }

    void PostGameRequest(CgsModule::VariableEventQueue<18432, 16>* lpQueue, f64 lfNow)
    {
        gHarness.miGamePosts += 1;
        gHarness.mfLastPost = lfNow;
        if (gHarness.meRole == E_ROLE_HOST)
        {
            const bool lbQueued = PostCreateGame(lpQueue);
            BrnNetHarnessPC::Witness("harness", "t=%.1fs post 256 create game mode=15 security=%d ranked=0 (#%d)%s",
                                     lfNow, KI_CREATE_SECURITY, gHarness.miGamePosts, lbQueued ? "" : " QUEUE FULL");
        }
        else
        {
            const bool lbQueued = PostQuickMatch(lpQueue);
            BrnNetHarnessPC::Witness("harness", "t=%.1fs post 251 quick match ranked=0 freeburn=1 (#%d)%s",
                                     lfNow, gHarness.miGamePosts, lbQueued ? "" : " QUEUE FULL");
        }
        SetStage(E_STAGE_WAIT_GAME, lfNow);
    }

    void PostLogin(CgsModule::VariableEventQueue<18432, 16>* lpQueue, f64 lfNow)
    {
        gHarness.miLoginPosts += 1;
        gHarness.mfLastPost = lfNow;
        const bool lbQueued = PostConnect(lpQueue);
        BrnNetHarnessPC::Witness("harness", "t=%.1fs post 272 connect type=%d (#%d)%s", lfNow, KI_CONNECT_TYPE_FULL,
                                 gHarness.miLoginPosts, lbQueued ? "" : " QUEUE FULL");
        SetStage(E_STAGE_WAIT_LOGIN, lfNow);
    }
}

namespace BrnNetHarnessPC
{
    bool WitnessEnabled()
    {
        Resolve();
        return gHarness.meRole != E_ROLE_NONE || CgsPcNetLanEnabled();
    }

    void Witness(const char* lpcSite, const char* lpcFormat, ...)
    {
        if (!WitnessEnabled() || giWitnessLinesLeft <= 0 || lpcSite == nullptr)
        {
            return;
        }

        WitnessSite* lpSite = nullptr;
        for (s32 li = 0; li < KI_MAX_WITNESS_SITES; ++li)
        {
            if (gaWitnessSites[li].mac[0] == '\0')
            {
                std::strncpy(gaWitnessSites[li].mac, lpcSite, KI_WITNESS_SITE_NAME - 1);
                lpSite = &gaWitnessSites[li];
                break;
            }
            if (std::strncmp(gaWitnessSites[li].mac, lpcSite, KI_WITNESS_SITE_NAME - 1) == 0)
            {
                lpSite = &gaWitnessSites[li];
                break;
            }
        }
        if (lpSite == nullptr || lpSite->miLines >= KI_WITNESS_LINES_PER_SITE)
        {
            return;
        }
        lpSite->miLines += 1;
        giWitnessLinesLeft -= 1;

        char lacText[512];
        s32 liPrefix = std::snprintf(lacText, sizeof(lacText), "[net] %s ", lpcSite);
        if (liPrefix < 0 || liPrefix >= static_cast<s32>(sizeof(lacText)))
        {
            return;
        }
        va_list lArgs;
        va_start(lArgs, lpcFormat);
        std::vsnprintf(lacText + liPrefix, sizeof(lacText) - static_cast<size_t>(liPrefix), lpcFormat, lArgs);
        va_end(lArgs);

        const size_t luLength = std::strlen(lacText);
        const char*  lpcTail  = (lpSite->miLines == KI_WITNESS_LINES_PER_SITE) ? " (site budget spent)\n" : "\n";
        std::strncat(lacText, lpcTail, sizeof(lacText) - luLength - 1);
        CgsDev::Log::WriteToLog(lacText);
    }

    void WitnessPlayerList(const void* lpRecord, s32 liNumPlayers, s32 liTotalPlayers)
    {
        if (!WitnessEnabled() || lpRecord == nullptr)
        {
            return;
        }
        const GuiPlayerListEntry* lpEntries = static_cast<const GuiPlayerListEntry*>(lpRecord);
        const s32 liCount = (liNumPlayers < 0) ? 0 : ((liNumPlayers > KI_PLAYER_LIST_ENTRIES) ? KI_PLAYER_LIST_ENTRIES : liNumPlayers);

        char lacList[256];
        s32 liUsed = std::snprintf(lacList, sizeof(lacList), "n=%d total=%d:", liNumPlayers, liTotalPlayers);
        for (s32 li = 0; li < liCount && liUsed > 0 && liUsed < static_cast<s32>(sizeof(lacList)); ++li)
        {
            liUsed += std::snprintf(lacList + liUsed, sizeof(lacList) - static_cast<size_t>(liUsed), " %d=%.16s",
                                    lpEntries[li].miPlayerID, lpEntries[li].macName);
        }
        if (std::strcmp(lacList, gacLastPlayerList) == 0)
        {
            return;
        }
        std::strncpy(gacLastPlayerList, lacList, sizeof(gacLastPlayerList) - 1);
        Witness("playerlist", "%s", lacList);
    }

    void Observe(BrnNetwork::BrnNetworkManager* lpNetworkManager)
    {
        Resolve();
        if (gHarness.meRole == E_ROLE_NONE || lpNetworkManager == nullptr)
        {
            return;
        }
        BrnNetwork::BrnServerInterface* lpServerInterface = lpNetworkManager->GetServerInterface();
        gHarness.mbLoggedIn  = lpServerInterface->GetConnectionComponent()->IsLoggedIn();
        gHarness.mbSigningIn = lpNetworkManager->GetLoginManager()->IsSigningIn();
        gHarness.mbIdle      = lpNetworkManager->GetStateManager()->IsIdle();
        gHarness.mbInGame    = lpServerInterface->GetGameComponent()->IsLocalPlayerInGame();
        gHarness.mbHost      = gHarness.mbInGame && lpServerInterface->GetGameComponent()->IsLocalPlayerHost();
        gHarness.miPlayersInGame = gHarness.mbInGame ? lpServerInterface->GetGameComponent()->GetNumberPlayersInGame() : 0;
        gHarness.mbObserved  = true;
    }

    void Update(CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue)
    {
        Resolve();
        if (gHarness.meRole == E_ROLE_NONE || lpGuiEventQueue == nullptr)
        {
            return;
        }

        const std::chrono::steady_clock::time_point lNow = std::chrono::steady_clock::now();
        if (!gHarness.mbStarted)
        {
            gHarness.mbStarted = true;
            gHarness.mStart    = lNow;
        }
        const f64 lfNow = std::chrono::duration<f64>(lNow - gHarness.mStart).count();
        if (!gHarness.mbObserved)
        {
            return;   // the network manager has not run yet
        }

        const bool lbLoggedIn  = gHarness.mbLoggedIn;
        const bool lbSigningIn = gHarness.mbSigningIn;
        const bool lbIdle      = gHarness.mbIdle;
        const bool lbInGame    = gHarness.mbInGame;

        switch (gHarness.meStage)
        {
        case E_STAGE_ARMED:
            if (lfNow >= gHarness.mfDelay && lbIdle && !lbSigningIn)
            {
                if (lbLoggedIn)
                {
                    Witness("harness", "t=%.1fs already signed in", lfNow);
                    SetStage(E_STAGE_READY, lfNow);
                }
                else if (gHarness.miLoginPosts < KI_MAX_LOGIN_POSTS)
                {
                    PostLogin(lpGuiEventQueue, lfNow);
                }
                else
                {
                    Witness("harness", "t=%.1fs GAVE UP: not signed in after %d connect posts", lfNow, gHarness.miLoginPosts);
                    SetStage(E_STAGE_GAVE_UP, lfNow);
                }
            }
            break;

        case E_STAGE_WAIT_LOGIN:
            if (lbLoggedIn && !lbSigningIn && lbIdle)
            {
                Witness("harness", "t=%.1fs signed in (%.1fs after the connect post)", lfNow, lfNow - gHarness.mfLastPost);
                SetStage(E_STAGE_READY, lfNow);
            }
            else if (!lbLoggedIn && !lbSigningIn && lbIdle && (lfNow - gHarness.mfLastPost) >= KF_LOGIN_RETRY_S)
            {
                Witness("harness", "t=%.1fs not signed in %.1fs after the connect post", lfNow, lfNow - gHarness.mfLastPost);
                SetStage(E_STAGE_ARMED, lfNow);
            }
            break;

        case E_STAGE_READY:
            if (lbInGame)
            {
                Witness("harness", "t=%.1fs already in a game", lfNow);
                SetStage(E_STAGE_IN_GAME, lfNow);
            }
            else if (!lbLoggedIn && !lbSigningIn && lbIdle)
            {
                Witness("harness", "t=%.1fs signed out again", lfNow);
                SetStage(E_STAGE_ARMED, lfNow);
            }
            else if (lbIdle && (lfNow - gHarness.mfStageTime) >= KF_SETTLE_AFTER_LOGIN_S)
            {
                PostGameRequest(lpGuiEventQueue, lfNow);
            }
            break;

        case E_STAGE_WAIT_GAME:
            if (lbInGame)
            {
                Witness("harness", "t=%.1fs in game: players=%d host=%d (%.1fs after the request)", lfNow,
                        gHarness.miPlayersInGame, gHarness.mbHost ? 1 : 0, lfNow - gHarness.mfLastPost);
                gHarness.miLastPlayers = -1;
                SetStage(E_STAGE_IN_GAME, lfNow);
            }
            else if (!lbLoggedIn && !lbSigningIn && lbIdle)
            {
                Witness("harness", "t=%.1fs signed out while waiting for the game", lfNow);
                SetStage(E_STAGE_ARMED, lfNow);
            }
            else if (lbIdle)
            {
                const bool lbHost    = (gHarness.meRole == E_ROLE_HOST);
                const f64  lfRetry   = lbHost ? KF_CREATE_RETRY_S : KF_QUICK_MATCH_RETRY_S;
                const s32  liMaxPost = lbHost ? KI_MAX_CREATE_POSTS : KI_MAX_QUICK_MATCH_POSTS;
                if ((lfNow - gHarness.mfLastPost) >= lfRetry)
                {
                    if (gHarness.miGamePosts < liMaxPost)
                    {
                        PostGameRequest(lpGuiEventQueue, lfNow);
                    }
                    else
                    {
                        Witness("harness", "t=%.1fs GAVE UP: no game after %d requests", lfNow, gHarness.miGamePosts);
                        SetStage(E_STAGE_GAVE_UP, lfNow);
                    }
                }
            }
            break;

        case E_STAGE_IN_GAME:
        {
            if (!lbInGame)
            {
                Witness("harness", "t=%.1fs left the game", lfNow);
                SetStage(E_STAGE_READY, lfNow);
                break;
            }
            const s32 liPlayers = gHarness.miPlayersInGame;
            if (liPlayers != gHarness.miLastPlayers)
            {
                Witness("harness", "t=%.1fs players in game=%d", lfNow, liPlayers);
                gHarness.miLastPlayers = liPlayers;
            }
            break;
        }

        case E_STAGE_GAVE_UP:
        default:
            break;
        }
    }
}
