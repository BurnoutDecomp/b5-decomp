#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceBroadcastMessages.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "lobbyapi.h"                                 // DirtySDK LobbyApiSetCallback / LobbyApiClearCallback + LobbyApiRefT / LobbyApiMsgT / LobbyApiCallbackT
#include "lobbytagfield.h"                            // DirtySDK TagFieldFind / Get*

#include <string.h>   // memmove

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceBroadcastMessages::Construct     @ 0x828767C0
//   CgsNetwork::ServerInterfaceBroadcastMessages::Destruct      @ 0x828767F8
//   CgsNetwork::ServerInterfaceBroadcastMessages::Release       @ 0x82876818
//   CgsNetwork::ServerInterfaceBroadcastMessages::ChatCallback  @ 0x82876878
//   CgsNetwork::ServerInterfaceBroadcastMessages::Prepare       @ 0x82886030
//   CgsNetwork::ServerInterfaceBroadcastMessages::Update        @ 0x828860C8
//   CgsNetwork::ServerInterfaceBroadcastMessages::OnEvent       @ 0x82886240
//   (the X360 scalar-deleting ~ServerInterfaceBroadcastMessages @ 0x827DE1A8 and the
//    paired trivial constructor are emitted from the C++ dtor/ctor below.)
//
// The component owns a fixed ring of 10 incoming "arbitration" broadcast packets and the
// per-message-type delivery callbacks the game registers. While prepared it holds a
// DirtySock lobby chat callback (LobbyApiSetCallback on channel 1); each arriving 'cast'
// chat message is decoded (ChatCallback) into the next free ring slot, and Update drains
// the ring one packet per call, dispatching each decoded payload to the callback
// registered for its message type.

namespace CgsNetwork
{
    // The shared empty/error string the components point their base error-data slot at
    // (X360 &unk_820046A7); also the TagFieldGetString default value. Its bytes live in
    // unrecovered .rdata; declared extern as an honest placeholder (mirrors the sibling
    // ServerInfo / DownloadableConfig component homes).
    extern const char gpcEmptyErrorString[];

    namespace
    {
        // The DirtySock lobby chat channel the component subscribes to.
        const s32 KI_CHAT_CHANNEL = 1;

        // The 'cast' message-kind fourcc (big-endian packed, as the X360 immediate 0x63617374
        // shows). Incoming chat messages of this kind carry a broadcast packet.
        const s32 KI_KIND_CAST = 0x63617374;   // 'cast'

        // The lpMsg->code "self echo" suppression flag the callback skips on (asm:
        // `(code >> 21) & 1`).
        const s32 KI_CODE_SELF_ECHO_SHIFT = 21;

        // OnEvent ids (the asm dispatches on 0 / 1 / 5).
        const s32 KI_EVENT_PREPARE = 0;
        const s32 KI_EVENT_RELEASE = 1;
        const s32 KI_EVENT_RESET   = 5;

        // No registered chat callback handle.
        const s32 KI_NO_CHAT_CALLBACK = -1;
    }

    // ===========================================================================
    // Construct / destruct of the C++ object lifetime
    // ===========================================================================
    //
    // The C++ default constructor just runs the base constructor; the X360 scalar-deleting
    // destructor @ 0x827DE1A8 restores the shared component vtable slot and conditionally
    // frees -- the compiler synthesises that from this trivial body. (The behavioural
    // member init lives in Construct() below, called by the server-interface owner, not in
    // the C++ constructor.)
    ServerInterfaceBroadcastMessages::ServerInterfaceBroadcastMessages()
        : ServerInterfaceComponent()
    {
    }

    ServerInterfaceBroadcastMessages::~ServerInterfaceBroadcastMessages()
    {
    }

    // ===========================================================================
    // Lifecycle
    // ===========================================================================

    // @ 0x828767C0 -- bring the base component error slots and our own state to the "no
    // error / idle / no callback" baseline.
    void ServerInterfaceBroadcastMessages::Construct()
    {
        meStatus                = ServerInterfaceDirtySock::E_STATUS_IDLE;   // +0x08 (== 2)
        mpcCurrentAction        = gpcEmptyErrorString;                       // +0x04
        miLastError             = 0;                                         // +0x0C
        miNumArbPacketsBuffered = 0;                                         // +0x580
        meCurrentAction         = E_ACTION_COUNT;                           // +0x588 (== 1)
        miChatCallback          = KI_NO_CHAT_CALLBACK;                       // +0x58C (== -1)
        mpServerInterface       = 0;                                         // +0x584
    }

    // @ 0x828767F8 -- reset our own state (the base slots are left to the base teardown).
    void ServerInterfaceBroadcastMessages::Destruct()
    {
        mpServerInterface       = 0;                                         // +0x584
        meCurrentAction         = E_ACTION_COUNT;                           // +0x588
        miChatCallback          = KI_NO_CHAT_CALLBACK;                       // +0x58C
        miNumArbPacketsBuffered = 0;                                         // +0x580
    }

    // @ 0x82886030 -- bind the server interface and register the lobby chat callback.
    bool ServerInterfaceBroadcastMessages::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        CGS_ASSERT(miChatCallback == KI_NO_CHAT_CALLBACK, "miChatCallback == -1");

        mpServerInterface = lpServerInterface;                              // +0x584
        meCurrentAction   = E_ACTION_COUNT;                                // +0x588 (== 1)
        miChatCallback    = LobbyApiSetCallback(mpServerInterface->GetLobbyAPIRef(),
                                                KI_CHAT_CHANNEL,
                                                &ServerInterfaceBroadcastMessages::ChatCallback,
                                                this);                      // +0x58C
        miNumArbPacketsBuffered = 0;                                        // +0x580
        return true;
    }

    // @ 0x82876818 -- clear the chat callback (if registered) and reset our state.
    bool ServerInterfaceBroadcastMessages::Release()
    {
        if (miChatCallback > KI_NO_CHAT_CALLBACK)
        {
            LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miChatCallback);
            miChatCallback = KI_NO_CHAT_CALLBACK;                          // +0x58C
        }
        mpServerInterface       = 0;                                       // +0x584
        meCurrentAction         = E_ACTION_COUNT;                         // +0x588
        miNumArbPacketsBuffered = 0;                                       // +0x580
        return true;
    }

    // @ 0x82886240 -- the server-interface lifecycle event hook. Re-registers the chat
    // callback on prepare (id 0), clears it on release (id 1), and drops any buffered state
    // on reset (id 5).
    void ServerInterfaceBroadcastMessages::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        const s32 liEvent = static_cast<s32>(leEvent);
        if (liEvent == KI_EVENT_PREPARE)
        {
            if (miChatCallback == KI_NO_CHAT_CALLBACK)
            {
                miChatCallback = LobbyApiSetCallback(mpServerInterface->GetLobbyAPIRef(),
                                                     KI_CHAT_CHANNEL,
                                                     &ServerInterfaceBroadcastMessages::ChatCallback,
                                                     this);                // +0x58C
            }
        }
        else if (liEvent == KI_EVENT_RELEASE)
        {
            if (miChatCallback > KI_NO_CHAT_CALLBACK)
            {
                LobbyApiClearCallback(mpServerInterface->GetLobbyAPIRef(), miChatCallback);
                miChatCallback = KI_NO_CHAT_CALLBACK;                      // +0x58C
            }
        }
        else if (liEvent == KI_EVENT_RESET)
        {
            meCurrentAction         = E_ACTION_COUNT;                     // +0x588
            miNumArbPacketsBuffered = 0;                                   // +0x580
        }
    }

    // ===========================================================================
    // Incoming chat decode
    // ===========================================================================

    // @ 0x82876878 -- DirtySock chat-channel completion. lpData is the component; lpMsg
    // carries one incoming chat message. A 'cast' message that is not our own echo is
    // decoded out of its nested "T" tagfield record { T = type, S = size, D = binary } into
    // the next free ring slot (capacity 10).
    void ServerInterfaceBroadcastMessages::ChatCallback(LobbyApiRefT* /*lpApiRef*/,
                                                        LobbyApiMsgT* lpMsg, void* lpData)
    {
        ServerInterfaceBroadcastMessages* lpThis =
            static_cast<ServerInterfaceBroadcastMessages*>(lpData);

        CGS_ASSERT(lpThis->mpServerInterface->GetMessageBuffer() != 0, "mpacMessageBuffer");

        // Copy the incoming message's "T" field value (a nested broadcast record) into the
        // shared 2 KB message buffer, then parse the broadcast record's sub-fields from it.
        char* lpacMessageBuffer = lpThis->mpServerInterface->GetMessageBuffer();
        TagFieldGetString(TagFieldFind(lpMsg->pData, "T"),
                          lpacMessageBuffer, KI_MESSAGE_BUFFER_SIZE, gpcEmptyErrorString);

        // Only buffer 'cast' messages, and skip our own echoed broadcast.
        if (lpMsg->kind == KI_KIND_CAST &&
            ((lpMsg->code >> KI_CODE_SELF_ECHO_SHIFT) & 1) == 0)
        {
            const s32 liSlot = lpThis->miNumArbPacketsBuffered;
            if (liSlot < KI_NUM_BROADCAST_MESSAGES_TO_BUFFER)
            {
                ServerInterfaceBroadcastDataPacket& lrPacket = lpThis->maMessagePacket[liSlot];

                lrPacket.meType = static_cast<EBroadcastMessageTypes>(
                    TagFieldGetNumber(TagFieldFind(lpacMessageBuffer, "T"),
                                      E_BROADCAST_MESSAGE_TYPES_COUNT));
                lrPacket.miSize =
                    TagFieldGetNumber(TagFieldFind(lpacMessageBuffer, "S"), 0);

                CGS_ASSERT(static_cast<s32>(lrPacket.meType) >= 0 &&
                           static_cast<s32>(lrPacket.meType) < E_BROADCAST_MESSAGE_TYPES_COUNT,
                           "Invalid message type received");
                CGS_ASSERT(lrPacket.miSize > 0, "Broadcast message too small!");
                CGS_ASSERT(lrPacket.miSize <= KI_BROADCAST_MESSAGE_SIZE, "Broadcast message too big!");

                TagFieldGetBinary(TagFieldFind(lpacMessageBuffer, "D"),
                                  lrPacket.macData, lrPacket.miSize);

                ++lpThis->miNumArbPacketsBuffered;
            }
        }
    }

    // ===========================================================================
    // Drain / dispatch
    // ===========================================================================

    // @ 0x828860C8 -- pop every buffered packet in arrival order, dispatching each decoded
    // payload to the callback registered for its message type.
    void ServerInterfaceBroadcastMessages::Update()
    {
        for (;;)
        {
            EBroadcastMessageTypes leType = static_cast<EBroadcastMessageTypes>(
                E_BROADCAST_MESSAGE_TYPES_COUNT);
            s32   liSize  = 0;
            void* lpData  = 0;

            if (miNumArbPacketsBuffered == 0)
                break;

            // Capture the oldest packet's type/size and the pointer to its payload, then
            // shift the trailing packets down (X360 memmoves 9 * 136 == 0x4C8 bytes). NOTE:
            // the original reads type/size BEFORE the shift but hands the callback the
            // pointer to slot 0's buffer, which the shift has by then overwritten with the
            // next packet's bytes -- this behaviour is reproduced faithfully.
            liSize = maMessagePacket[0].miSize;
            leType = maMessagePacket[0].meType;
            lpData = maMessagePacket[0].macData;

            memmove(&maMessagePacket[0], &maMessagePacket[1],
                    (KI_NUM_BROADCAST_MESSAGES_TO_BUFFER - 1) * sizeof(ServerInterfaceBroadcastDataPacket));
            --miNumArbPacketsBuffered;

            // The X360 build additionally streams "New arb msg is of type <type>\n" to the
            // debug message stream here; that un-homed StrStreamBase side channel is
            // diagnostic only and is reduced to this note (mirrors the sibling
            // CgsPlayersConnectionManagerLookup reconstruction).

            CGS_ASSERT(static_cast<s32>(leType) >= 0 &&
                       static_cast<s32>(leType) < E_BROADCAST_MESSAGE_TYPES_COUNT,
                       "Broadcast message has invalid type");

            BroadcastMessageCallback lpfCallback = mapRegisteredCallbacks[leType];
            if (lpfCallback)
                lpfCallback(lpData, liSize, mapRegisteredCallbacksUserData[leType]);
        }
    }
}
