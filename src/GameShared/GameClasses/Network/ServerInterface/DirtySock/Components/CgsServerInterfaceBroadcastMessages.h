#ifndef CGS_SERVER_INTERFACE_BROADCAST_MESSAGES_H
#define CGS_SERVER_INTERFACE_BROADCAST_MESSAGES_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceBroadcastMessages
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceBroadcastMessages.{h,cpp}
//
// The broadcast-messages server-interface component (the in-game "arbitration"
// message / chat broadcast stage owner). Derives from
// CgsNetwork::ServerInterfaceComponent (Feb-2007 source +
// CgsServerInterfaceBroadcastMessages.h DWARF:
//   `ServerInterfaceBroadcastMessages : public CgsNetwork::ServerInterfaceComponent`).
//
// The component registers a DirtySock lobby chat callback (LobbyApiSetCallback,
// channel 1) while prepared; each incoming 'cast' chat message is decoded out of
// its tagfield record (ChatCallback) into a fixed ring of buffered packets, and
// Update drains that ring one packet per iteration, dispatching each to the
// per-message-type callback the game registered.
//
// LAYOUT (X360 ARTIST asm), after the 4-word ServerInterfaceComponent base (vptr /
// mpcCurrentAction / meStatus / miLastError):
//   +0x10  mapRegisteredCallbacks[4]          BroadcastMessageCallback   words 4..7
//   +0x20  mapRegisteredCallbacksUserData[4]  void*                      words 8..11
//   +0x30  maMessagePacket[10]                ServerInterfaceBroadcastDataPacket
//                                             (136 bytes each: macData[128] +
//                                              meType@+0x80 + miSize@+0x84)
//   +0x580 miNumArbPacketsBuffered            s32                        word 352
//   +0x584 mpServerInterface                  ServerInterfaceDirtySock*  word 353
//   +0x588 meCurrentAction                    EAction (== E_ACTION_COUNT/1 idle)  word 354
//   +0x58C miChatCallback                     s32 (LobbyApiSetCallback handle; -1 = none)
// (offsets read off Construct @0x828767C0 / ChatCallback @0x82876878 /
//  Update @0x828860C8 / Prepare @0x82886030 / OnEvent @0x82886240.)
// ===========================================================================

namespace CgsNetwork
{
    // Forward-declared DirtySock facade; the component reaches the lobby ref and the
    // shared message buffer through it (see CgsServerInterfaceDirtySock.h).
    struct ServerInterfaceDirtySock;
}

// DirtySDK lobby request types used by the chat-callback path (vendor SDK; declared at
// global scope to match vendor/dirtysdk/include/lobbyapi.h).
struct LobbyApiRefT;
struct LobbyApiMsgT;

namespace CgsNetwork
{
    // CgsNetwork-owned broadcast message id enum (home: GameSource/Network/
    // BroadcastMessageTypes.h in the Feb-2007 source). Modelled here -- this is the
    // component that owns the buffered-packet ring keyed by it -- since no committed
    // TU homes it yet. The asm validates a received type against count == 4.
    enum EBroadcastMessageTypes
    {
        E_BROADCAST_MESSAGE_TYPES_START          = 0,
        E_BROADCAST_MESSAGE_TYPES_GOPLAY         = E_BROADCAST_MESSAGE_TYPES_START,
        E_BROADCAST_MESSAGE_TYPES_LEAVEGAME      = 1,
        E_BROADCAST_MESSAGE_TYPES_FINISHEDSAVING = 2,
        E_BROADCAST_MESSAGE_TYPES_STANDINGSGAME  = 3,

        E_BROADCAST_MESSAGE_TYPES_COUNT          = 4
    };

    // CgsServerInterfaceBroadcastMessages.h:53 -- the per-message-type delivery callback:
    // (decoded payload, payload byte size, registered user data).
    typedef void (*BroadcastMessageCallback)(void* lpData, s32 liSize, void* lpUserData);

    // CgsServerInterfaceBroadcastMessages.h:171 -- ring-buffer depth (the asm bounds-checks
    // miNumArbPacketsBuffered against 10 in ChatCallback, and memmoves 9 trailing packets
    // (0x4C8 == 9 * 136) down in Update).
    const s32 KI_NUM_BROADCAST_MESSAGES_TO_BUFFER = 10;

    // CgsServerInterfaceBroadcastMessages.h:93 -- max decoded payload (the asm asserts a
    // received size in (0, 128]).
    const s32 KI_BROADCAST_MESSAGE_SIZE = 128;

    // CgsServerInterfaceBroadcastMessages.h:66 -- a decoded broadcast message handed to a
    // registered callback (built by GetLastMessage in a sibling dossier; declared here so
    // the broadcast/send paths can name it).
    class ServerInterfaceBroadcastMessage
    {
    public:
        void Construct();

        void*                  mpData;   // +0x00
        EBroadcastMessageTypes meType;   // +0x04
        s32                    miSize;   // +0x08
    };

    // CgsServerInterfaceBroadcastMessages.h:86 -- one buffered incoming packet. 136 bytes;
    // ChatCallback writes meType@+0x80 / miSize@+0x84 and the binary payload into macData,
    // and Update reads them back. macData also doubles as the payload pointer passed to the
    // callback (Update passes &maMessagePacket[0].macData).
    class ServerInterfaceBroadcastDataPacket
    {
    public:
        void Construct();

        char                   macData[KI_BROADCAST_MESSAGE_SIZE];   // +0x00
        EBroadcastMessageTypes meType;                               // +0x80
        s32                    miSize;                               // +0x84
    };

    class ServerInterfaceBroadcastMessages : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceBroadcastMessages.h:109
        enum EAction
        {
            E_ACTION_BROADCAST_MESSAGE = 0,
            E_ACTION_COUNT             = 1,
        };

        ServerInterfaceBroadcastMessages();

        // CgsServerInterfaceBroadcastMessages.h:107 -- scalar deleting destructor @ 0x827DE1A8.
        virtual ~ServerInterfaceBroadcastMessages();

        // ---- Lifecycle ------------------------------------------------------------
        // (Non-virtual in this build, matching the Feb-2007 interface; BrnServerInterfaceBase
        //  drives them on the embedded component by name.)
        void Construct();                                            // @ 0x828767C0
        void Destruct();                                             // @ 0x828767F8
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);   // @ 0x82886030
        bool Release();                                              // @ 0x82876818
        void Update();                                               // @ 0x828860C8
        void Suspend();
        void Resume();

        // ---- Public send / register API (bodied in their own dossiers) -------------
        void SendGameMessage(EBroadcastMessageTypes leMessageType, void* lpData, s32 liSize);
        void SendUserSetMessage(const char* lpcUserSetName, EBroadcastMessageTypes leMessageType,
                                void* lpData, s32 liSize);
        void RegisterMessageCallback(EBroadcastMessageTypes leMessageType,
                                     BroadcastMessageCallback lpCallback, void* lpUserData);

        // ---- vtable ---------------------------------------------------------------
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);   // @ 0x82886240

    private:
        // Pop the oldest buffered packet into lpMessage (bodied in its own dossier).
        bool GetLastMessage(ServerInterfaceBroadcastMessage* lpMessage);

        // Assemble + send one broadcast packet (optionally scoped to a user set; bodied in
        // its own dossier).
        void BroadcastMessage(ServerInterfaceBroadcastMessage* lpMessage,
                              const char* lpcUserSet = 0);

        // The DirtySock chat-channel callback: decode an incoming 'cast' message into the
        // buffered-packet ring.
        static void ChatCallback(LobbyApiRefT* lpApiRef, LobbyApiMsgT* lpMsg, void* lpData);

        // --- Static lookup table (homed in a send-path dossier; VALUE not recoverable from
        //     the available exports, so it is declared but intentionally left undefined here
        //     -- none of this TU's bodied functions reference it).
        static const int KAI_ACTION_CODE_MAPPING[E_ACTION_COUNT];

        // --- Data members (after the ServerInterfaceComponent base) -----------------
        BroadcastMessageCallback mapRegisteredCallbacks[E_BROADCAST_MESSAGE_TYPES_COUNT];          // +0x10
        void*                    mapRegisteredCallbacksUserData[E_BROADCAST_MESSAGE_TYPES_COUNT];  // +0x20
        ServerInterfaceBroadcastDataPacket maMessagePacket[KI_NUM_BROADCAST_MESSAGES_TO_BUFFER];   // +0x30
        s32                      miNumArbPacketsBuffered;   // +0x580
        ServerInterfaceDirtySock* mpServerInterface;        // +0x584
        EAction                  meCurrentAction;           // +0x588
        s32                      miChatCallback;            // +0x58C (LobbyApiSetCallback handle; -1 == none)
    };
}

#endif // CGS_SERVER_INTERFACE_BROADCAST_MESSAGES_H
