#ifndef SDKS_EA_GAMETALK_GAMETALK_H
#define SDKS_EA_GAMETALK_GAMETALK_H

#include "types.hpp"

// ============================================================================
// SDKs/EA/GameTalk/GameTalk.h
//
// EA::GameTalk -- EA's in-game <-> external-tool messaging channel (the debug
// "GameTalk" wire used by GameExplorer and the other authoring tools). A
// GameTalkMessage is a keyed bag of named content blobs addressed to a tool
// "channel"; GameTalkManager is the process-wide singleton that ferries a
// finished message out to a named tool endpoint.
//
// HOMED HERE because the EA GameTalk SDK is not modelled anywhere else in the
// tree and ICE::ICEFileHandler::FileClose (@0x8252C960) is the first
// reconstructed caller -- it ships the accumulated take buffer to the
// "Tool.GameExplorer" endpoint under the "SaveTake" key.
//
// MINIMAL SLICE -- declaration-only, modelling exactly the three entry points
// FileClose touches (from the X360 asm):
//
//   GameTalkMessage::GameTalkMessage(const char* lpcChannel)
//       ctor; r4 = "Camera" channel string.
//   GameTalkMessage::AddKeyContent(const char* lpcKey, s32 liType,
//                                  const void* lpContent, s32 liLength)
//       r4 = "SaveTake", r5 = 0 (type/flags), r6 = the content buffer (the
//       ICEFileHandler `this`/macWriteBuffer), r7 = its StringLength. The asm
//       captures the return in r3 but FileClose discards it; the X360 Hex-Rays
//       renders it as a `_DWORD*`, so it is modelled as a pointer return.
//   GameTalkManager::GetInstance()
//       static singleton accessor (no args in the asm; the r3 it reads is the
//       leftover AddKeyContent result, not a real argument).
//   GameTalkManager::SendMessage(const char* lpcEndpoint, GameTalkMessage& rMsg)
//       STATIC: r3 = "Tool.GameExplorer", r4 = &msg (no `this`). The asm calls
//       GetInstance() then overwrites r3 with the endpoint string before the call,
//       discarding GetInstance's result -- so SendMessage takes the endpoint in r3,
//       which is only possible if it is a static/free function.
//
// The message is a stack temporary in FileClose (BYREF, 24 bytes of frame),
// constructed and destructed in place, and passed to SendMessage by reference.
//
// DECLARATION-ONLY: the real bodies live in the EA GameTalk SDK TU and the
// per-TU `cl /c` compile gate does not link. When the GameTalk SDK surface is
// reconstructed, GROW this header in place -- do NOT fork a parallel type.
// ============================================================================

namespace EA
{
namespace GameTalk
{
    // The abstract transport the manager drives (full definition in
    // SDKs/Packages/GameTalk/1.8.0/include/GameTalk/IGameTalkProtocol.h). Only a
    // pointer is held here, so a forward declaration keeps this header light.
    class IGameTalkProtocol;

    // ------------------------------------------------------------------------
    // GameTalk namespace-level helpers (X360 .XEX, BURNOUT_X360_ARTIST.XEX).
    //
    //   Alloc(s32 liSize)              @ 0x82838000
    //   Free(void* lpBlock)            @ 0x82836E18
    //   StrIEqual(const char*, const char*) @ 0x82836C60
    //
    // Alloc/Free are GameTalk's allocation shims: they route every GameTalk
    // allocation through the AttribSys GameTalk package allocator. The X360 asm
    // inlines CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator() (it
    // reads the manager's sbHasLinearAllocator flag and the static GameTalk
    // package-allocator instance &dword_83011B64 directly, asserting the manager
    // has been Prepare'd against CgsAttribSysMemoryManager.h:192) and forwards to
    // that allocator's Malloc(size, 0) / Free(block). A NULL size / NULL block is
    // a no-op (Alloc returns 0, Free returns its argument).
    //
    // StrIEqual is a case-insensitive C-string equality test (used by
    // GameTalkManager::IsMessageMatching / ConfigHandler to match channel keys).
    void* Alloc(s32 liSize);
    void* Free(void* lpBlock);
    bool  StrIEqual(const char* lpcLhs, const char* lpcRhs);

    class GameTalkMessage
    {
    public:
        // One key/content entry (16-byte record, X360 ctor 0x828387A8 / dtor 0x82837860):
        //   +0x00  const char* mpcKey     the key name string.
        //   +0x04  s32         miType     the content type/flags word.
        //   +0x08  const void* mpContent  the raw content blob.
        //   +0x0C  s32         miSize     the content byte size.
        struct KeyContent
        {
            const char* mpcKey;
            s32         miType;
            const void* mpContent;
            s32         miSize;
        };

        // Construct a message bound to the named tool channel ("Camera").
        explicit GameTalkMessage(const char* lpcChannel);
        ~GameTalkMessage();

        // @0x82838D98 -- static factory: decode a big-endian wire buffer into a message.
        static GameTalkMessage* Create(const char* lpBuffer);

        // @0x82837B50 -- serialize the message into a freshly package-allocated big-endian
        // wire buffer (lpcKey prefixed at the front); *lppOutBuffer receives it. Returns the
        // total buffer size.
        u32 CreateBuffer(const char* lpcKey, void** lppOutBuffer);

        // Attach a named content blob to the message. liType is the content
        // type/flags word (0 from FileClose); lpContent/liLength are the raw
        // payload (the take buffer + its length). Returns an opaque slot
        // pointer (discarded by FileClose).
        void* AddKeyContent(const char* lpcKey, s32 liType, const void* lpContent, s32 liLength);

        // GROWN for CgsGameTalk::GameTalkProtocol::SendPingResponseToGameExplorer
        // (X360 @0x82838978). The X360 builds the outgoing GameTalkMessage by
        // package-allocating the message object and then calling AllocateDataBuffer
        // to attach its key/content storage before AddKeyContent populates it.
        // Modelled as a member that lazily allocates the message's data buffer.
        void* AllocateDataBuffer();

        // GROWN for CgsDev::DebugGameTalkInterface::GameTalkMsgHandler (X360
        // @0x82833D20), SEMANTICS PINNED by BrnDirector::Camera::
        // BehaviourRenderMetrics::GameTalkMessageReceiver (X360 @0x8220FB60). The
        // X360 Hex-Rays mis-resolves the shared accessor @0x82836DC8 as
        // CgsNetwork::ServerInterfaceComponent::GetLastError; both call sites pass
        // the GameTalkMessage `this` in r3.
        //
        // GetNumKeys() @0x82836DC8: the number of key/content entries attached to
        //   the message (`lwz r3, 0xC(r3)` -- the count word behind the +0x8 entry
        //   table). The render-metrics receiver uses it as its key-index loop
        //   bound (indices feed GetKey/GetContent/GetSize below), which is what
        //   pins the count semantics; the debug handler's `!= 1` gate is thus an
        //   "exactly one key" check. (Previously modelled as GetState() from the
        //   debug handler's compare alone -- reconciled 2026-07.)
        s32 GetNumKeys() const;

        // Indexed entry accessors, GROWN for BehaviourRenderMetrics::
        // GameTalkMessageReceiver (X360 @0x8220FB60). Each reads the entry-pointer
        // table at message+0x8 (`lwzx` entry = table[liIndex]) and returns one
        // entry field:
        //   GetKey     @0x82836DD0  entry+0x0  the key name string.
        //   GetContent @0x82836DE8  entry+0x8  the raw content blob (callers cast;
        //                                      the metrics receiver reads float32s).
        //   GetSize    @0x82836E00  entry+0xC  the content byte size.
        const char* GetKey(s32 liIndex) const;
        const void* GetContent(s32 liIndex) const;
        s32         GetSize(s32 liIndex) const;

        // The tool channel this message is addressed to (the +0x10 member). GROWN
        // for GameTalkManager::ReceiveMessage / ConfigHandler (X360 @0x828390B8 /
        // @0x82838FF0), which match an incoming message's channel (`lwz r,0x10(msg)`)
        // against "Server Message" and each registered channel filter.
        const char* GetChannel() const;

        // GetKeyContent(): look up the named key and return its content blob (the script
        //                  source for "ExecuteScript"); NULL when the key is absent. The
        //                  handler treats a non-NULL result as the script text to execute.
        const char* GetKeyContent(const char* lpcKey) const;

        // Shared entry-buffer capacity (X360 global dword_82F32FC4, doubled on grow by
        // AddKeyContent; seeded by AllocateDataBuffer in its own TU).
        static s32 KsDataBufferCapacity;

    private:
        // Wire-cursor parse helpers (X360 statics). Each reads a big-endian u32 length
        // prefix off the cursor, advances past it, package-allocates the payload and copies
        // it in, advancing the cursor past the payload.
        //   ReadString @0x82837E88  NUL-terminated string copy (len+1 bytes).
        //   ReadBinary @0x82837F40  length-prefixed binary blob (*lpuLength bytes).
        static char* ReadString(const char*& rlpCursor);
        static void* ReadBinary(const char*& rlpCursor, u32* lpuLength);

        void* mpVTable;         // +0x00  vtable
        s32   mbParsed;         // +0x04  mbCreatedFromBuffer (parsed-from-wire flag)
        KeyContent** mppEntries;// +0x08  entry-pointer table
        s32   miNumKeys;        // +0x0C  live key/content entry count
        const char* mpcChannel; // +0x10  tool channel string
    };

    // Signature of a GameTalk message-received handler: the manager hands the
    // decoded incoming message to the registered handler. CgsGameTalk's
    // GameTalkProtocol::OnMessageReceived is registered through this.
    typedef void (*MessageHandler)(GameTalkMessage* lpMessage);

    class GameTalkManager
    {
    public:
        // Process-wide GameTalk singleton (X360 GetInstance @0x82836CF8 -> the
        // off_8303577C global).
        static GameTalkManager* GetInstance();

        // X360 @0x82839420. Package-allocate + construct the process-wide singleton
        // and publish it. lpOwner is the transport the manager drives (an
        // IGameTalkProtocol; passed opaquely as the CgsGameTalk front-door `this`,
        // whose embedded protocol subobject sits at offset 0). liMaxChannels sizes
        // the handler table; lpcName is the instance name seeding the "initialize"
        // handshake (defaults to "Game.Xenon" when null). The return is the new
        // instance; CgsGameTalk::GameTalk::Prepare reads it back via GetInstance().
        static void* CreateInstance(void* lpOwner, s32 liMaxChannels, const char* lpcName);

        // GROWN for CgsGameTalk::GameTalk::Update (X360 @0x828376C0): pump the
        // singleton's transport once per frame. The X360 reaches the singleton's
        // embedded protocol and calls its per-frame virtual; modelled here as the
        // manager's own per-frame Update.
        void Update();

        // X360 @0x82838508. Serialize rMessage under the lpcEndpoint key
        // (GameTalkMessage::CreateBuffer) and ship the wire buffer over the
        // singleton's transport, then free the buffer. STATIC (the asm passes the
        // endpoint in r3, not a `this`); FileClose's `GetInstance()->SendMessage(...)`
        // evaluates GetInstance() for its side effect then calls this static method.
        static s32 SendMessage(const char* lpcEndpoint, GameTalkMessage& rMessage);

        // GROWN for CgsGameTalk::GameTalkProtocol::SendPingResponseToGameExplorer
        // (X360 @0x82838978): the X360 sends a heap-built message by pointer.
        // Forwards to the reference overload.
        static s32 SendMessage(const char* lpcEndpoint, GameTalkMessage* lpMessage);

        // X360 @0x82838B70. Register lpHandler against the named channel filter in
        // the first free handler slot, then announce the filter to the GameTalk
        // server (SendServerChannel). Returns the announce result (forwarded by
        // CgsGameTalk::GameTalk::RegisterMessageHandler, X360 @0x828248D0).
        static s32 RegisterMessageHandler(GameTalkManager* lpManager,
                                          MessageHandler lpHandler,
                                          const char* lpcChannel);

        // X360 @0x828390B8. Static receive entry: route a decoded incoming message
        // to the server config handler (when it matches "Server Message") and to
        // every registered per-channel handler whose filter matches the message
        // channel. Called by the transport receive trampoline (ReceiverCallback).
        static s32 ReceiveMessage(GameTalkMessage* lpMessage);

        // X360 @0x82836D08. Hierarchical dotted-name channel match: an empty pattern
        // matches everything; otherwise lpcPattern must equal lpcChannel exactly or
        // be a dot-delimited prefix of it ("a.b" matches "a.b" and "a.b.c", but not
        // "a.bc"). Case-sensitive byte compare over the pattern length.
        static bool IsMessageMatching(const char* lpcChannel, const char* lpcPattern);

    private:
        // 16-byte handler-table entry (X360 Malloc(...,16) in RegisterMessageHandler).
        struct MessageHandlerEntry
        {
            const char*    mpcChannel;                          // +0x00 channel filter
            MessageHandler mpfnHandler;                         // +0x04 message handler
            void (*mpfnContextHandler)(GameTalkMessage*, void*);// +0x08 context handler
            void*          mpContext;                           // +0x0C context slot
        };

        // X360 @0x828392F8. Wire the manager onto its transport (installs
        // ReceiverCallback as the transport's receive callback), allocate + clear the
        // handler table, and -- when the transport reports connected -- emit the
        // platform / version / initialize config handshake.
        GameTalkManager(void* lpOwner, s32 liMaxChannels, const char* lpcName);

        // X360 @0x828384A0. Free the handler table and restore the vtable. The
        // scalar-deleting variant that also releases the manager object is the
        // compiler-emitted vtable thunk (not written by hand).
        virtual ~GameTalkManager();

        // X360 @0x82837770. Build a keyword message ([key][value], each length-
        // prefixed big-endian) in the shared scratch buffer and send it over the
        // transport. Returns the transport Send result.
        s32 SendKeywordMessage(const char* lpcKey, const char* lpcValue);

        // X360 @0x82838AA8. Build a "Client Message" carrying an Add/RemoveChannelFilter
        // key for lpcChannelName and send it to the "GameTalkServer" endpoint.
        static s32 SendServerChannel(const char* lpcChannelName, bool lbAdd);

        // X360 @0x82838FF0. Handle a "Server Message": on an "UpdateTargetName" key
        // adopt the new instance name (from the first entry's content), re-announce
        // the config version, and re-register every live channel filter.
        s32 ConfigHandler(GameTalkMessage* lpMessage);

        // The transport receive trampoline installed into IGameTalkProtocol::
        // mpfnMessageHandler; decodes raw wire bytes and forwards to ReceiveMessage.
        // Defined in its own TU -- referenced here (and stored) by address only.
        static void ReceiverCallback(const char* lpacMessage, u32 luLength);

        // --- members (X360 offsets 0x00..0x18; pointers widened to x64) ---
        // +0x00 is the implicit vtable (the class is polymorphic via ~GameTalkManager).
        const char*           mpcName;        // +0x04  instance name ("Game.Xenon")
        IGameTalkProtocol*    mpProtocol;     // +0x08  transport the manager drives
        MessageHandlerEntry** mppHandlers;    // +0x0C  handler table (miMaxChannels slots)
        s32                   miNumHandlers;  // +0x10  live handler count
        s32                   miMaxChannels;  // +0x14  handler table capacity

        // Process-wide singleton pointer (X360 off_8303577C).
        static GameTalkManager* spInstance;
    };
}
}

#endif // SDKS_EA_GAMETALK_GAMETALK_H
