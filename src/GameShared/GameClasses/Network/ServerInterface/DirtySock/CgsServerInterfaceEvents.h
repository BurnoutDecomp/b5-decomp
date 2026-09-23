#ifndef CGS_SERVER_INTERFACE_EVENTS_H
#define CGS_SERVER_INTERFACE_EVENTS_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork::EServerInterfaceEvent
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h
//
// The events the DirtySock server interface raises to its components (OnEvent) and to the
// game (BrnNetwork::BrnNetworkManager::TriggerEventFromServerInterface). Every other header
// forward-declares it with a fixed underlying type (`enum EServerInterfaceEvent : s32;`),
// so the definition carries the same one.
//
// The enumerator set and values are the reference ones. The console build agrees on every
// value its TriggerEventFromServerInterface switch dispatches on (0, 3, 5, 7, 9, 10, 11,
// 15, 16, 17, 18): each case calls the manager hook its name describes.
// ===========================================================================

namespace CgsNetwork
{
    enum EServerInterfaceEvent : s32
    {
        E_SERVER_INTERFACE_GENERAL_EVENT_START                   = 0,
        E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_CREATED       = 0,
        E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_DESTROYING    = 1,
        E_SERVER_INTERFACE_GENERAL_EVENT_END                     = 2,

        E_SERVER_INTERFACE_CONNECTION_EVENT_START                = 3,
        E_SERVER_INTERFACE_CONNECTION_EVENT_CONNECTED            = 3,
        E_SERVER_INTERFACE_CONNECTION_EVENT_CONNECT_FAILED       = 4,
        E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED         = 5,
        E_SERVER_INTERFACE_CONNECTION_EVENT_END                  = 6,

        E_SERVER_INTERFACE_GAMES_EVENT_START                     = 7,
        E_SERVER_INTERFACE_GAMES_EVENT_SEARCH_UPDATED            = 7,
        E_SERVER_INTERFACE_GAMES_EVENT_GAME_STARTED              = 8,
        E_SERVER_INTERFACE_GAMES_EVENT_KICKED                    = 9,
        E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_ADDED              = 10,
        E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_REMOVED            = 11,
        E_SERVER_INTERFACE_GAMES_EVENT_REMOVED_FROM_GAME         = 12,
        E_SERVER_INTERFACE_GAMES_EVENT_ADDED_TO_GAME             = 13,
        E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_LIST_CHANGED       = 14,
        E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_PARAMETERS_CHANGED = 15,
        E_SERVER_INTERFACE_GAMES_EVENT_GAME_PARAMETERS_CHANGED   = 16,
        E_SERVER_INTERFACE_GAMES_EVENT_GAME_DELETED              = 17,
        E_SERVER_INTERFACE_GAMES_EVENT_GAME_ID_CHANGED           = 18,
        E_SERVER_INTERFACE_GAMES_EVENT_END                       = 19,

        E_SERVER_INTERFACE_PLAYER_INFO_EVENT_START               = 20,
        E_SERVER_INTERFACE_PLAYER_INFO_EVENT_STATS_CHANGED       = 20,
        E_SERVER_INTERFACE_PLAYER_INFO_EVENT_END                 = 21,

        E_SERVER_INTERFACE_TELEMETRY_EVENT_START                 = 22,
        E_SERVER_INTERFACE_TELEMETRY_FIRST_USAGE_DATA_UPLOADED   = 22,
        E_SERVER_INTERFACE_TELEMETRY_EVENT_END                   = 23,

        E_SERVER_INTERFACE_USERSETS_EVENT_START                  = 24,
        E_SERVER_INTERFACE_USERSETS_EVENT_USERLIST_CHANGED       = 24,
        E_SERVER_INTERFACE_USERSETS_EVENT_END                    = 25,
    };
}

#endif // CGS_SERVER_INTERFACE_EVENTS_H
