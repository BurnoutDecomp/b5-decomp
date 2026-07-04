#include "SDKs/EA/GameTalk/GameTalk.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h" // CgsAttribSys::AttribSysMemoryManager
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // CgsAttribSys::AttribSysPackageAllocator
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
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
}
}
