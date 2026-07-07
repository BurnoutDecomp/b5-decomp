#include "SDKs/EA/GameTalk/GameTalk.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h" // CgsAttribSys::AttribSysMemoryManager
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // CgsAttribSys::AttribSysPackageAllocator
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "SDKs/Packages/GameTalk/1.8.0/include/GameTalk/IGameTalkProtocol.h" // EA::GameTalk::IGameTalkProtocol
#include "rw/core/stdc/stdc.h"                       // rw::core::stdc::StringLength / StringnCopy

#include <cctype>  // tolower
#include <cstring> // memcpy
#include <cstdlib> // _byteswap_ulong
#include <new>     // placement new

// ============================================================================
// SDKs/EA/GameTalk/GameTalk.cpp
//
// EA::GameTalk namespace-level helpers, reconstructed store-for-store from the
// X360 .XEX (BURNOUT_X360_ARTIST.XEX):
//   EA::GameTalk::Alloc      @ 0x82838000
//   EA::GameTalk::Free       @ 0x82836E18
//   EA::GameTalk::StrIEqual  @ 0x82836C60
//
// Alloc/Free route GameTalk's allocations through the AttribSys GameTalk package
// allocator. The X360 asm inlines AttribSysMemoryManager::GetGameTalkAllocator()
// -- it reads the manager's sbHasLinearAllocator flag directly and uses the
// static GameTalk package-allocator instance (&dword_83011B64), asserting against
// CgsAttribSysMemoryManager.h:192 that the manager has been Prepare'd. The
// portable reconstruction keeps that observable behaviour by going through the
// GetGameTalkAllocator() accessor (which performs the same assert and hands back
// the same instance), then forwarding to Malloc(size, 0) / Free(block). The NULL
// guard short-circuits before the allocator is touched (Alloc returns 0; Free
// returns its argument unchanged).
// ============================================================================

namespace EA
{
namespace GameTalk
{
    // @ 0x82838000 -- allocate liSize bytes from the GameTalk package allocator.
    // r3 = size; NULL size returns 0 without touching the allocator. Otherwise the
    // asm asserts sbHasLinearAllocator then calls Malloc(&dword_83011B64, size, 0).
    void* Alloc(s32 liSize)
    {
        if (!liSize)
            return 0;

        return CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(
            static_cast<size_t>(liSize), 0);
    }

    // @ 0x82836E18 -- free a block previously returned by Alloc. r3 = block; a NULL
    // block is returned unchanged without touching the allocator. Otherwise the asm
    // asserts sbHasLinearAllocator then calls Free(&dword_83011B64, block). The X360
    // leaves r3 as the allocator Free result (Hex-Rays types the function as
    // returning the original pointer on the NULL-block path); modelled likewise.
    void* Free(void* lpBlock)
    {
        if (lpBlock)
        {
            CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Free(lpBlock);
        }
        return lpBlock;
    }

    // @ 0x82836C60 -- case-insensitive C-string equality. Walks both strings in
    // lockstep comparing tolower() of each byte; on the first mismatch returns false,
    // and on reaching the end of either string returns whether both terminated at the
    // same position (i.e. *lpcLhs == *lpcRhs at the stop point). Matches the X360
    // store order: lpcLhs is r31 (advanced via r2), lpcRhs is r30.
    bool StrIEqual(const char* lpcLhs, const char* lpcRhs)
    {
        const char* lpcL = lpcLhs;
        int liLeftChar = static_cast<signed char>(*lpcL);
        if (liLeftChar)
        {
            while (*lpcRhs)
            {
                int liRightLower = tolower(static_cast<signed char>(*lpcRhs++));
                ++lpcL;
                if (tolower(liLeftChar) != liRightLower)
                    return false;
                liLeftChar = static_cast<signed char>(*lpcL);
                if (!*lpcL)
                    break;
            }
        }
        return static_cast<signed char>(*lpcRhs) == static_cast<signed char>(*lpcL);
    }

    // ========================================================================
    // GameTalkMessage bodies (reconstructed from BURNOUT_X360_ARTIST.XEX).
    // ========================================================================

    // Shared entry-buffer capacity (X360 global dword_82F32FC4, doubled on grow).
    s32 GameTalkMessage::KsDataBufferCapacity = 0;

    // @ 0x828387A8 -- construct a message bound to the named tool channel. Zeroes the
    // flag/entry/count fields, then eagerly allocates the entry-pointer buffer.
    GameTalkMessage::GameTalkMessage(const char* lpcChannel)
        : mpVTable(0)
        , mbParsed(0)
        , mppEntries(0)
        , miNumKeys(0)
        , mpcChannel(lpcChannel)
    {
        mppEntries = static_cast<KeyContent**>(AllocateDataBuffer());
    }

    // @ 0x82837860 -- tear down the message. Frees the key/content storage. Two
    // strategies keyed on mbParsed (parsed-from-wire frees the channel + each entry's key
    // and content + the slot; built-locally frees only the slots), then the entry table.
    GameTalkMessage::~GameTalkMessage()
    {
        CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
            CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator();

        if (mbParsed)
        {
            lpAllocator->Free(const_cast<char*>(mpcChannel));
            for (s32 liIndex = 0; liIndex < miNumKeys; ++liIndex)
            {
                KeyContent* lpEntry = mppEntries[liIndex];
                lpAllocator->Free(const_cast<char*>(lpEntry->mpcKey));
                lpAllocator->Free(const_cast<void*>(lpEntry->mpContent));
                if (lpEntry)
                    lpAllocator->Free(lpEntry);
                mppEntries[liIndex] = 0;
            }
        }
        else
        {
            for (s32 liIndex = 0; liIndex < miNumKeys; ++liIndex)
            {
                KeyContent* lpEntry = mppEntries[liIndex];
                if (lpEntry)
                    lpAllocator->Free(lpEntry);
                mppEntries[liIndex] = 0;
            }
        }

        if (mppEntries)
            lpAllocator->Free(mppEntries);
        mppEntries = 0;
    }

    // @ 0x82838800 -- attach a key/content entry to the message, growing the entry-pointer
    // buffer (doubling the shared KsDataBufferCapacity) when it is full. Returns the freshly
    // package-allocated 16-byte entry (or NULL on allocation failure).
    void* GameTalkMessage::AddKeyContent(
        const char* lpcKey, s32 liType, const void* lpContent, s32 liLength)
    {
        if (miNumKeys >= KsDataBufferCapacity)
        {
            const s32 liOldCapacity = KsDataBufferCapacity;
            KsDataBufferCapacity *= 2;

            KeyContent** lppNewEntries =
                static_cast<KeyContent**>(AllocateDataBuffer());
            for (s32 liSlot = 0; liSlot < liOldCapacity; ++liSlot)
                lppNewEntries[liSlot] = mppEntries[liSlot];

            if (mppEntries)
                EA::GameTalk::Free(mppEntries);

            mppEntries = lppNewEntries;
        }

        CGS_ASSERT(CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator() != 0,
                   "sbHasLinearAllocator");
        KeyContent* lpEntry = static_cast<KeyContent*>(
            CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(
                sizeof(KeyContent), 0));
        if (lpEntry)
        {
            lpEntry->mpcKey    = lpcKey;
            lpEntry->miType    = liType;
            lpEntry->mpContent = lpContent;
            lpEntry->miSize    = liLength;
        }

        mppEntries[miNumKeys++] = lpEntry;
        return lpEntry;
    }

    // @ 0x82836DD0 / 0x82836DE8 / 0x82836E00 -- indexed entry accessors.
    const char* GameTalkMessage::GetKey(s32 liIndex) const
    {
        return mppEntries[liIndex]->mpcKey;
    }

    const void* GameTalkMessage::GetContent(s32 liIndex) const
    {
        return mppEntries[liIndex]->mpContent;
    }

    s32 GameTalkMessage::GetSize(s32 liIndex) const
    {
        return mppEntries[liIndex]->miSize;
    }

    // The message's tool channel string (the +0x10 member).
    const char* GameTalkMessage::GetChannel() const
    {
        return mpcChannel;
    }

    // @ 0x82837E88 -- parse a length-prefixed string from the wire cursor. Reads a
    // big-endian u32 length, advances past it, package-allocates len+1 bytes, copies len
    // bytes and NUL-terminates, advancing the cursor past the len string bytes.
    char* GameTalkMessage::ReadString(const char*& rlpCursor)
    {
        const u8* lpucLen = reinterpret_cast<const u8*>(rlpCursor);
        u32 luLength = (static_cast<u32>(lpucLen[0]) << 24) |
                       (static_cast<u32>(lpucLen[1]) << 16) |
                       (static_cast<u32>(lpucLen[2]) << 8) |
                        static_cast<u32>(lpucLen[3]);
        rlpCursor += 4;

        char* lpcString = static_cast<char*>(
            CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(
                static_cast<size_t>(luLength) + 1, 0));
        rw::core::stdc::StringnCopy(lpcString, rlpCursor, luLength);
        lpcString[luLength] = 0;
        rlpCursor += luLength;
        return lpcString;
    }

    // @ 0x82837F40 -- parse a length-prefixed binary blob from the wire cursor. Reads a
    // big-endian u32 length into *lpuLength, advances the cursor past it, package-allocates
    // that many bytes, copies the payload in and advances past it.
    void* GameTalkMessage::ReadBinary(const char*& rlpCursor, u32* lpuLength)
    {
        const u8* lpucLen = reinterpret_cast<const u8*>(rlpCursor);
        u32 luLength = (static_cast<u32>(lpucLen[0]) << 24) |
                       (static_cast<u32>(lpucLen[1]) << 16) |
                       (static_cast<u32>(lpucLen[2]) << 8) |
                        static_cast<u32>(lpucLen[3]);
        rlpCursor += 4;
        *lpuLength = luLength;

        void* lpBlock = CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(
            static_cast<size_t>(*lpuLength), 0);
        memcpy(lpBlock, rlpCursor, *lpuLength);
        rlpCursor += *lpuLength;
        return lpBlock;
    }

    // @ 0x82838D98 -- static factory: decode a big-endian wire buffer into a message.
    // Layout: [u32 header][channel string][u32 keyCount] then keyCount *
    // ([key string][u32 type][content]); type 0 = NUL string, type 1 = length-prefixed
    // binary, other = empty.
    GameTalkMessage* GameTalkMessage::Create(const char* lpBuffer)
    {
        u32 luSwap = 0;
        const char* lpcCursor = lpBuffer + 4;
        std::memcpy(&luSwap, lpBuffer, 4);
        if (static_cast<s32>(_byteswap_ulong(luSwap)) <= 0)
            return 0;

        CGS_ASSERT(CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator() != 0,
                   "sbHasLinearAllocator");
        GameTalkMessage* lpMessage = static_cast<GameTalkMessage*>(
            CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(
                sizeof(GameTalkMessage), 0));
        if (lpMessage)
        {
            const char* lpcChannel = ReadString(lpcCursor);
            new (lpMessage) GameTalkMessage(lpcChannel);
        }

        // The X360 stores mbParsed=1 unconditionally (even on the null-alloc path);
        // preserved store-for-store.
        lpMessage->mbParsed = 1;

        luSwap = 0;
        std::memcpy(&luSwap, lpcCursor, 4);
        s32 liKeyCount = static_cast<s32>(_byteswap_ulong(luSwap));
        lpcCursor += 4;
        while (liKeyCount > 0)
        {
            const char* lpcKey = ReadString(lpcCursor);

            luSwap = 0;
            std::memcpy(&luSwap, lpcCursor, 4);
            u32 luType = _byteswap_ulong(luSwap);
            lpcCursor += 4;

            const void* lpContent = 0;
            s32 liSize = 0;
            if (luType == 0)
            {
                lpContent = ReadString(lpcCursor);
                liSize = rw::core::stdc::StringLength(static_cast<const char*>(lpContent));
            }
            else if (luType == 1)
            {
                u32 luLength = 0;
                lpContent = ReadBinary(lpcCursor, &luLength);
                liSize = static_cast<s32>(luLength);
            }

            lpMessage->AddKeyContent(lpcKey, static_cast<s32>(luType), lpContent, liSize);
            --liKeyCount;
        }
        return lpMessage;
    }

    // @ 0x82837B50 -- serialize the message into a freshly package-allocated big-endian
    // wire buffer. Layout mirrors Create().
    u32 GameTalkMessage::CreateBuffer(const char* lpcKey, void** lppOutBuffer)
    {
        const u32 luKeyLen = static_cast<u32>(rw::core::stdc::StringLength(lpcKey));
        u32 luBodyLen = static_cast<u32>(rw::core::stdc::StringLength(mpcChannel)) + 8;

        for (s32 liEntry = 0; liEntry < miNumKeys; ++liEntry)
        {
            const s32 liEntryKeyLen =
                rw::core::stdc::StringLength(mppEntries[liEntry]->mpcKey);
            luBodyLen += mppEntries[liEntry]->miSize + liEntryKeyLen + 12;
        }

        const u32 luTotal = luBodyLen + luKeyLen + 12;

        CGS_ASSERT(CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator() != 0,
                   "sbHasLinearAllocator");
        char* lpcOut = static_cast<char*>(
            CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(luTotal, 0));
        *lppOutBuffer = lpcOut;

        u32 luSwap;

        luSwap = _byteswap_ulong(luTotal);
        std::memcpy(lpcOut, &luSwap, 4);
        lpcOut += 4;

        luSwap = _byteswap_ulong(luKeyLen);
        std::memcpy(lpcOut, &luSwap, 4);
        lpcOut += 4;

        std::memcpy(lpcOut, lpcKey, luKeyLen);
        lpcOut += luKeyLen;

        luSwap = _byteswap_ulong(luBodyLen);
        std::memcpy(lpcOut, &luSwap, 4);
        lpcOut += 4;

        const u32 luChannelLen =
            static_cast<u32>(rw::core::stdc::StringLength(mpcChannel));
        luSwap = _byteswap_ulong(luChannelLen);
        std::memcpy(lpcOut, &luSwap, 4);
        lpcOut += 4;

        std::memcpy(lpcOut, mpcChannel, luChannelLen);
        lpcOut += luChannelLen;

        const u32 luNumKeys = static_cast<u32>(miNumKeys);
        luSwap = _byteswap_ulong(luNumKeys);
        std::memcpy(lpcOut, &luSwap, 4);
        lpcOut += 4;

        for (s32 liEntry = 0; liEntry < static_cast<s32>(luNumKeys); ++liEntry)
        {
            const u32 luEntryKeyLen =
                static_cast<u32>(rw::core::stdc::StringLength(mppEntries[liEntry]->mpcKey));
            luSwap = _byteswap_ulong(luEntryKeyLen);
            std::memcpy(lpcOut, &luSwap, 4);
            lpcOut += 4;

            std::memcpy(lpcOut, mppEntries[liEntry]->mpcKey, luEntryKeyLen);
            lpcOut += luEntryKeyLen;

            luSwap = _byteswap_ulong(static_cast<u32>(mppEntries[liEntry]->miType));
            std::memcpy(lpcOut, &luSwap, 4);
            lpcOut += 4;

            const u32 luEntrySize = static_cast<u32>(mppEntries[liEntry]->miSize);
            luSwap = _byteswap_ulong(luEntrySize);
            std::memcpy(lpcOut, &luSwap, 4);
            lpcOut += 4;

            std::memcpy(lpcOut, mppEntries[liEntry]->mpContent, luEntrySize);
            lpcOut += luEntrySize;
        }

        return luTotal;
    }

    // ========================================================================
    // GameTalkManager bodies (reconstructed from BURNOUT_X360_ARTIST.XEX).
    //
    // The manager is the process-wide GameTalk singleton (off_8303577C). It owns a
    // transport (IGameTalkProtocol) and a table of per-channel message handlers, and
    // it ferries serialized GameTalkMessages between the running game and the
    // external authoring tools. Layout (X360 offsets, pointers widened to x64):
    //   +0x00 vtable  +0x04 mpcName  +0x08 mpProtocol  +0x0C mppHandlers
    //   +0x10 miNumHandlers  +0x14 miMaxChannels   (X360 sizeof == 0x18 == 24)
    // ========================================================================

    // Convenience: the GameTalk package allocator every manager allocation routes
    // through (asserts the AttribSys manager has been Prepare'd, matching the X360
    // sbHasLinearAllocator check inlined at each Malloc/Free site).
    namespace
    {
        CgsAttribSys::AttribSysPackageAllocator* GameTalkAllocator()
        {
            return CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator();
        }

        // Shared keyword-message scratch buffer (X360 &unk_830358C8, a fixed file-scope
        // region: +0x00 total length, +0x04 key length, +0x08 key bytes, then the value
        // length + value bytes). The exact X360 capacity is not recovered; 256 bytes
        // comfortably covers the short "gametalk.config.*" / "initialize" handshakes
        // that are the only messages routed through SendKeywordMessage.
        const s32 KI_KEYWORD_SCRATCH_SIZE = 256;
        u8 sacKeywordScratch[KI_KEYWORD_SCRATCH_SIZE];
    }

    // The published singleton (X360 off_8303577C).
    GameTalkManager* GameTalkManager::spInstance = 0;

    // @ 0x82836CF8 -- return the process-wide singleton.
    GameTalkManager* GameTalkManager::GetInstance()
    {
        return spInstance;
    }

    // @ 0x828392F8 -- construct the manager. Stores the transport, installs the
    // receive trampoline into it, allocates + zeroes the handler table, then (when the
    // transport is connected) emits the platform/version/initialize config handshake.
    GameTalkManager::GameTalkManager(void* lpOwner, s32 liMaxChannels, const char* lpcName)
        : mpcName(lpcName)
        , mpProtocol(static_cast<IGameTalkProtocol*>(lpOwner))
        , mppHandlers(0)
        , miNumHandlers(0)
        , miMaxChannels(liMaxChannels)
    {
        // Install the manager's receive trampoline into the transport (X360
        // `stw ReceiverCallback, 4(protocol)`).
        mpProtocol->mpfnMessageHandler = &GameTalkManager::ReceiverCallback;

        // Allocate the handler table (X360 Alloc(4 * maxChannels); widened to x64
        // pointer stride) and clear every slot.
        mppHandlers = static_cast<MessageHandlerEntry**>(
            EA::GameTalk::Alloc(liMaxChannels * static_cast<s32>(sizeof(MessageHandlerEntry*))));
        for (s32 liSlot = 0; liSlot < miMaxChannels; ++liSlot)
            mppHandlers[liSlot] = 0;

        // Only announce the config handshake once the transport reports connected.
        if (mpProtocol->IsConnected())
        {
            if (!mpcName)
                mpcName = "Game.Xenon";

            SendKeywordMessage("gametalk.config.platform", "xenon");
            SendKeywordMessage("gametalk.config.version", "2.0.0.0");
            SendKeywordMessage("initialize", mpcName);
        }
    }

    // @ 0x828384A0 -- free the handler table (the vtable restore is the implicit
    // dtor prologue; the deleting variant that also frees the object is the compiler
    // vtable thunk). EA::GameTalk::Free tolerates a null table.
    GameTalkManager::~GameTalkManager()
    {
        EA::GameTalk::Free(mppHandlers);
        mppHandlers = 0;
    }

    // @ 0x82839420 -- package-allocate + construct the singleton and publish it.
    void* GameTalkManager::CreateInstance(void* lpOwner, s32 liMaxChannels, const char* lpcName)
    {
        CGS_ASSERT(GameTalkAllocator() != 0, "sbHasLinearAllocator");
        void* lpMem = GameTalkAllocator()->Malloc(sizeof(GameTalkManager), 0);
        if (lpMem)
            spInstance = new (lpMem) GameTalkManager(lpOwner, liMaxChannels, lpcName);
        else
            spInstance = 0;
        return spInstance;
    }

    // @ 0x82837770 -- serialize a [key][value] keyword message into the shared scratch
    // buffer (each field a big-endian u32 length prefix + bytes) and Send it. The
    // total length prefixes the buffer: total = keyLen + valueLen + 12 (three u32
    // prefixes).
    s32 GameTalkManager::SendKeywordMessage(const char* lpcKey, const char* lpcValue)
    {
        const u32 luKeyLen   = static_cast<u32>(rw::core::stdc::StringLength(lpcKey));
        const u32 luValueLen = static_cast<u32>(rw::core::stdc::StringLength(lpcValue));
        const u32 luTotal    = luKeyLen + luValueLen + 12;

        u8* lpuOut = sacKeywordScratch;
        u32 luSwap;

        // [+0x00] total length (big-endian).
        luSwap = _byteswap_ulong(luTotal);
        std::memcpy(lpuOut, &luSwap, 4);

        // [+0x04] key length (big-endian), [+0x08] key bytes.
        luSwap = _byteswap_ulong(luKeyLen);
        std::memcpy(lpuOut + 4, &luSwap, 4);
        rw::core::stdc::StringnCopy(reinterpret_cast<char*>(lpuOut + 8), lpcKey, luKeyLen);

        // [after the key] value length (big-endian) + value bytes.
        u8* lpuValue = lpuOut + 8 + luKeyLen;
        luSwap = _byteswap_ulong(luValueLen);
        std::memcpy(lpuValue, &luSwap, 4);
        rw::core::stdc::StringnCopy(reinterpret_cast<char*>(lpuValue + 4), lpcValue, luValueLen);

        return mpProtocol->Send(lpuOut, static_cast<s32>(luTotal));
    }

    // @ 0x82838508 -- serialize rMessage under lpcEndpoint and Send it over the
    // singleton's transport, freeing the scratch wire buffer afterwards.
    s32 GameTalkManager::SendMessage(const char* lpcEndpoint, GameTalkMessage& rMessage)
    {
        if (!spInstance)
            return 0;

        void* lpBuffer = 0;
        const u32 luSize = rMessage.CreateBuffer(lpcEndpoint, &lpBuffer);
        const s32 liResult = spInstance->mpProtocol->Send(lpBuffer, static_cast<s32>(luSize));

        CGS_ASSERT(GameTalkAllocator() != 0, "sbHasLinearAllocator");
        GameTalkAllocator()->Free(lpBuffer);
        return liResult;
    }

    // Pointer overload (GROWN for the heap-built-message send path); forwards.
    s32 GameTalkManager::SendMessage(const char* lpcEndpoint, GameTalkMessage* lpMessage)
    {
        return SendMessage(lpcEndpoint, *lpMessage);
    }

    // @ 0x82838AA8 -- build a "Client Message" carrying an Add/RemoveChannelFilter key
    // for lpcChannelName and send it to the "GameTalkServer" endpoint. The message is a
    // stack temporary, constructed and destructed in place.
    s32 GameTalkManager::SendServerChannel(const char* lpcChannelName, bool lbAdd)
    {
        const char* lpcFilterKey = lbAdd ? "AddChannelFilter" : "RemoveChannelFilter";

        GameTalkMessage lMessage("Client Message");
        lMessage.AddKeyContent(lpcFilterKey, 0, lpcChannelName,
                               rw::core::stdc::StringLength(lpcChannelName));
        return SendMessage("GameTalkServer", lMessage);
    }

    // @ 0x82838B70 -- register lpHandler against the named channel filter in the first
    // free handler slot, then announce the filter to the GameTalk server.
    s32 GameTalkManager::RegisterMessageHandler(
        GameTalkManager* lpManager, MessageHandler lpHandler, const char* lpcChannel)
    {
        if (lpManager->miNumHandlers >= lpManager->miMaxChannels)
            return 0;

        if (lpManager->miMaxChannels > 0)
        {
            // Find the first free slot; if the table is unexpectedly full, just
            // (re-)announce the filter and bail without adding an entry.
            s32 liSlot = 0;
            while (lpManager->mppHandlers[liSlot])
            {
                ++liSlot;
                if (liSlot >= lpManager->miMaxChannels)
                    return SendServerChannel(lpcChannel, true);
            }

            CGS_ASSERT(GameTalkAllocator() != 0, "sbHasLinearAllocator");
            MessageHandlerEntry* lpEntry = static_cast<MessageHandlerEntry*>(
                GameTalkAllocator()->Malloc(sizeof(MessageHandlerEntry), 0));
            if (lpEntry)
            {
                lpEntry->mpcChannel        = lpcChannel;
                lpEntry->mpfnHandler       = lpHandler;
                lpEntry->mpfnContextHandler = 0;
                lpEntry->mpContext         = 0;
            }

            lpManager->mppHandlers[liSlot] = lpEntry;
            ++lpManager->miNumHandlers;
        }

        return SendServerChannel(lpcChannel, true);
    }

    // @ 0x82836D08 -- hierarchical dotted-name channel match.
    bool GameTalkManager::IsMessageMatching(const char* lpcChannel, const char* lpcPattern)
    {
        const char* lpcPat = lpcPattern ? lpcPattern : "";
        if (EA::GameTalk::StrIEqual(lpcPat, ""))
            return true;  // an empty pattern matches every channel

        const s32 liChannelLen = rw::core::stdc::StringLength(lpcChannel);
        const s32 liPatternLen = rw::core::stdc::StringLength(lpcPat);
        if (liPatternLen > liChannelLen)
            return false;

        // Case-sensitive compare over the pattern length.
        for (s32 i = 0; i < liPatternLen; ++i)
        {
            if (lpcChannel[i] != lpcPat[i])
                return false;
        }

        // Exact match, or a dot-delimited prefix ("a.b" matches "a.b.c").
        if (liPatternLen == liChannelLen)
            return true;
        return lpcChannel[liPatternLen] == '.';
    }

    // @ 0x82838FF0 -- handle a "Server Message". On an "UpdateTargetName" key, adopt
    // the new instance name from the first entry's content, re-announce the config
    // version, then re-register every live channel filter with the server.
    s32 GameTalkManager::ConfigHandler(GameTalkMessage* lpMessage)
    {
        s32 liResult = 0;
        if (lpMessage->GetNumKeys())
        {
            liResult = EA::GameTalk::StrIEqual(lpMessage->GetKey(0), "UpdateTargetName");
            if (liResult)
            {
                mpcName = static_cast<const char*>(lpMessage->GetContent(0));
                liResult = SendKeywordMessage("gametalk.config.version", "2.0.0.0");

                for (s32 liSlot = 0; liSlot < miMaxChannels; ++liSlot)
                {
                    MessageHandlerEntry* lpEntry = mppHandlers[liSlot];
                    if (lpEntry)
                        liResult = SendServerChannel(lpEntry->mpcChannel, true);
                }
            }
        }
        return liResult;
    }

    // @ 0x828390B8 -- route a decoded incoming message: hand a "Server Message" to the
    // config handler, then dispatch to every registered per-channel handler whose
    // filter matches the message channel (single-arg handler preferred, else the
    // context handler with the entry's context slot).
    s32 GameTalkManager::ReceiveMessage(GameTalkMessage* lpMessage)
    {
        GameTalkManager* lpManager = spInstance;
        s32 liResult = 0;
        if (!lpManager)
            return liResult;

        if (IsMessageMatching(lpMessage->GetChannel(), "Server Message"))
            liResult = lpManager->ConfigHandler(lpMessage);

        for (s32 liSlot = 0; liSlot < lpManager->miMaxChannels; ++liSlot)
        {
            MessageHandlerEntry* lpEntry = lpManager->mppHandlers[liSlot];
            if (!lpEntry)
                continue;

            if (IsMessageMatching(lpMessage->GetChannel(), lpEntry->mpcChannel))
            {
                // The registered handlers are void-returning (the X360 Hex-Rays types
                // them int-returning and captures r3, but the registered callbacks --
                // e.g. AttribulatorGameTalkHandler -- take only the message and return
                // nothing). Prefer the single-arg handler, else the context handler
                // with the entry's context slot.
                if (lpEntry->mpfnHandler)
                    lpEntry->mpfnHandler(lpMessage);
                else if (lpEntry->mpfnContextHandler)
                    lpEntry->mpfnContextHandler(lpMessage, &lpEntry->mpContext);
            }
        }
        return liResult;
    }
}
}
