#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"

#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"
#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameSource/Replays/BrnReplayShared.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::WriteStream).
//
//   ResetStream            @ 0x8264D100
//   InvalidateFrunksAhead  @ 0x8264D190
//   AddFrunk               @ 0x8265EB70 (raw ARTIST image; no named JSON export)
//
// When a frunk is written at index liStartFrunk, later frunks whose disk bytes
// it overwrites are stale. This marks those overlapped frunks VOID, then trims
// the leading edge of the live ring up to
// the first surviving keyframe so playback resumes from a self-contained frame.

namespace BrnReplays
{
    namespace
    {
        // ARTIST AddFrunk materialises 0x82FDA380 and uses it as a shared 0x20000-byte
        // staging buffer for each frunk before passing its 64 KiB-rounded blocks to
        // GPUDiskWriteStream::AddBlock. The name is unresolved; this is the one recovered
        // source home for the original scratch storage.
        const s32 KI_FRUNK_SCRATCH_CAPACITY = 0x20000;
        alignas(0x80) u8 gacFrunkScratchBuffer[KI_FRUNK_SCRATCH_CAPACITY];

        void FireOriginalWriteStreamAssert(const char* lpcMessage, s32 liLine)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lpcMessage,
                "..\\..\\..\\GameSource\\Replays/Stream/BrnReplayWriteStream.cpp",
                liLine);
            CgsDev::Assert::EndAssert();
        }
    }

    // ARTIST raw body @ 0x8265EB70; declaration shape from the DecFIGS mangled name.
    // Preserve the assertion-and-continue path, staged-header bytes, 128 KiB buffer
    // capacity, block rounding, and ring updates as observed in the X360 instructions.
    void WriteStream::AddFrunk(s32 liFrameIndex, f32 lfFrameTime,
                               FrunkSerialiserEntry* lpEntries, void** lppBlocks,
                               s32 liNumEntries, bool lbIsKeyFrame)
    {
        if (mbEnded)
            FireOriginalWriteStreamAssert("Stream has ended - need to start it again!", 137);
        if (mbPaused)
            FireOriginalWriteStreamAssert("Stream is paused - must continue it first", 138);
        if (lppBlocks == 0)
            FireOriginalWriteStreamAssert("No blocks passed in", 139);
        if (lpEntries == 0)
            FireOriginalWriteStreamAssert("No block sizes in", 140);

        StreamOffset* lpFrunkOffset = 0;
        if (miNumFrunksAllocated < KI_MAX_FRUNKS)
        {
            lpFrunkOffset = static_cast<StreamOffset*>(mHeaderMalloc.Malloc(
                static_cast<s32>(sizeof(StreamOffset))));
            if (lpFrunkOffset == 0)
                FireOriginalWriteStreamAssert("Failed to allocate memory for new offset entry", 151);

            if (mpStreamHeader->mpFrameOffsets == 0)
                mpStreamHeader->mpFrameOffsets = lpFrunkOffset;
            ++miNumFrunksAllocated;
        }
        else
        {
            if (mpStreamHeader->miNumFrunks > KI_MAX_FRUNKS)
                FireOriginalWriteStreamAssert("Should never go past max frames", 164);

            lpFrunkOffset = mpStreamHeader->mpFrameOffsets + miCurrentWriteIndex;
        }

        mCurrentFrunkHeader.mfFrameTime = lfFrameTime;
        mCurrentFrunkHeader.miFrameNumber = liFrameIndex;
        mCurrentFrunkHeader.mxFlags = lbIsKeyFrame ? KU_FLAG_KEYFRAME : 0;
        mCurrentFrunkHeader.miNumSerialisers = liNumEntries;
        static const char KacFrunkMagic[4] = { 'f', 'r', 'k', '\0' };
        std::memcpy(mCurrentFrunkHeader.macMagicNumber, KacFrunkMagic, sizeof(KacFrunkMagic));

        lpFrunkOffset->mfFrameTime = lfFrameTime;
        lpFrunkOffset->miFrameNumber = liFrameIndex;
        lpFrunkOffset->mxFlags = mCurrentFrunkHeader.mxFlags;
        lpFrunkOffset->miFileOffset = miFilePosition;

        const s32 liHeaderBytes = static_cast<s32>(sizeof(FrunkHeader));
        const s32 luEntryBytes = static_cast<s32>(static_cast<u32>(liNumEntries) << 3);
        s32 liHeaderCopied = 0;
        if (KI_FRUNK_SCRATCH_CAPACITY - miBufferPosition >= liHeaderBytes)
        {
            std::memcpy(gacFrunkScratchBuffer + miBufferPosition,
                        &mCurrentFrunkHeader,
                        static_cast<size_t>(liHeaderBytes));
            miBufferPosition += liHeaderBytes;
            liHeaderCopied = liHeaderBytes;
        }

        s32 liEntriesCopied = 0;
        if (KI_FRUNK_SCRATCH_CAPACITY - miBufferPosition >= luEntryBytes)
        {
            std::memcpy(gacFrunkScratchBuffer + miBufferPosition,
                        lpEntries,
                        static_cast<size_t>(luEntryBytes));
            miBufferPosition += luEntryBytes;
            liEntriesCopied = luEntryBytes;
        }

        const s32 liExpectedPayloadOffset = static_cast<s32>(
            static_cast<u32>(liHeaderBytes) + static_cast<u32>(luEntryBytes));
        s32 liBytesWritten = static_cast<s32>(
            static_cast<u32>(liHeaderCopied) + static_cast<u32>(liEntriesCopied));
        for (s32 liEntry = 0; liEntry < liNumEntries; ++liEntry)
        {
            const s32 liBlockSize = lpEntries[liEntry].miSize;
            s32 liBlockBytesCopied = 0;
            if (KI_FRUNK_SCRATCH_CAPACITY - miBufferPosition >= liBlockSize)
            {
                std::memcpy(gacFrunkScratchBuffer + miBufferPosition,
                            lppBlocks[liEntry],
                            static_cast<size_t>(liBlockSize));
                miBufferPosition = static_cast<s32>(
                    static_cast<u32>(miBufferPosition) + static_cast<u32>(liBlockSize));
                liBlockBytesCopied = liBlockSize;
            }
            liBytesWritten = static_cast<s32>(
                static_cast<u32>(liBytesWritten) + static_cast<u32>(liBlockBytesCopied));
        }

        const s32 liPayloadSize = static_cast<s32>(
            static_cast<u32>(liBytesWritten) - static_cast<u32>(liExpectedPayloadOffset));
        if (liPayloadSize != 0)
        {
            const s32 liRoundedBlockSize = static_cast<s32>(
                (static_cast<u32>(liBytesWritten) + 0xFFFFu) & 0xFFFF0000u);
            if (mpStream->AddBlock(miFilePosition, gacFrunkScratchBuffer, liRoundedBlockSize))
            {
                lpFrunkOffset->miFrunkSize = liBytesWritten;
                miFilePosition = static_cast<s32>(
                    static_cast<u32>(miFilePosition) + static_cast<u32>(liRoundedBlockSize));
            }
            else
            {
                lpFrunkOffset->miFrunkSize = 0;
                lpFrunkOffset->mxFlags |= KU_FLAG_VOID;
                ++miStallCount;
            }
        }

        if (mpStreamHeader->miNumFrunks < KI_MAX_FRUNKS)
        {
            ++mpStreamHeader->miNumFrunks;
        }
        else
        {
            mpStreamHeader->miFirstFrunk = (miCurrentWriteIndex + 1) % KI_MAX_FRUNKS;
        }

        InvalidateFrunksAhead(miCurrentWriteIndex);
        ++miCurrentWriteIndex;
        if (miCurrentWriteIndex > KI_MAX_FRUNKS)
            FireOriginalWriteStreamAssert("Should never go past max frames for write index", 253);
        if (miCurrentWriteIndex >= KI_MAX_FRUNKS)
        {
            miCurrentWriteIndex = 0;
            miFilePosition = 0;
        }
        miBufferPosition = 0;
    }

    // @ 0x8264D100
    // Reset the write stream to an empty, freshly-allocated header. Frees every
    // allocation the header allocator has made, carves a new StreamHeader out of it,
    // stamps the "REPLAY " magic (through the terminating NUL) and zeroes the header's
    // frunk index, then clears the per-recording bookkeeping (write cursor, stall/alloc
    // counters, ended/paused flags, file + intermediate-buffer positions). The
    // DiskWriteStream link (mpStream @0x50) is intentionally left untouched --
    // StartNewStream owns it. Returns the freshly allocated StreamHeader (X360 r3).
    StreamHeader* WriteStream::ResetStream()
    {
        mHeaderMalloc.FreeAll();

        StreamHeader* lpHeader =
            static_cast<StreamHeader*>(mHeaderMalloc.Malloc(sizeof(StreamHeader)));
        mpStreamHeader = lpHeader;

        // Stamp the 8-byte magic ("REPLAY " + NUL). The X360 body is a byte copy that
        // runs through and including the terminating NUL of the rodata literal.
        static const char KacReplayMagic[8] = { 'R', 'E', 'P', 'L', 'A', 'Y', ' ', '\0' };
        for (s32 liByte = 0; ; ++liByte)
        {
            lpHeader->macMagicNumber[liByte] = KacReplayMagic[liByte];
            if (KacReplayMagic[liByte] == '\0')
                break;
        }

        lpHeader->miVersion      = 0;
        lpHeader->miNumFrunks    = 0;
        lpHeader->miFirstFrunk   = 0;
        lpHeader->mpFrameOffsets = nullptr;

        miCurrentWriteIndex  = 0; // @0x40
        miStallCount         = 0; // @0x44
        mbEnded              = false; // @0x48
        mbPaused             = false; // @0x49
        miNumFrunksAllocated = 0; // @0x4C
        miFilePosition       = 0; // @0x54  (mpStream @0x50 deliberately untouched)
        miBufferPosition     = 0; // @0x58

        return lpHeader;
    }

    // @ 0x8264D190
    void WriteStream::InvalidateFrunksAhead(s32 liStartFrunk)
    {
        StreamHeader* lpHeader = mpStreamHeader;
        StreamOffset* lpOffsets = lpHeader->mpFrameOffsets;

        // --- pass 1: void later frunks whose disk byte ranges are overwritten ---
        if (lpOffsets[liStartFrunk].miFrunkSize != 0)
        {
            const s32 liDataMin = lpOffsets[liStartFrunk].miFileOffset;
            const s32 liDataMax =
                lpOffsets[liStartFrunk].miFrunkSize + liDataMin - 1;

            for (s32 liIndex = liStartFrunk + 1;
                 liIndex < lpHeader->miNumFrunks;
                 ++liIndex)
            {
                if (lpOffsets[liIndex].miFileOffset > liDataMax)
                    break;

                lpOffsets[liIndex].mxFlags |= KU_FLAG_VOID;

                // (re-read of mpStreamHeader each iteration in the X360 body is
                // elided: lpHeader/lpOffsets are loop-invariant here.)
            }
        }

        // --- pass 2: advance the ring start past leading voided non-keyframes ---
        // Walk the ring from the current first frunk, counting frunks that are either
        // voided or not a keyframe, and stop at the first live keyframe (a frame the
        // stream can be replayed from). Everything skipped is dropped from the count.
        s32 liDropped = 0;
        const s32 liNumFrunks = lpHeader->miNumFrunks;
        if (liNumFrunks > 0)
        {
            s32 liFrunkIndex = lpHeader->miFirstFrunk;
            for (;; ++liFrunkIndex)
            {
                const u16 luFlags =
                    lpOffsets[liFrunkIndex % KI_MAX_FRUNKS].mxFlags;

                if ((luFlags & KU_FLAG_VOID) == 0 && (luFlags & KU_FLAG_KEYFRAME) != 0)
                    break;

                if (++liDropped >= liNumFrunks)
                    return;
            }

            lpHeader->miFirstFrunk = liFrunkIndex % KI_MAX_FRUNKS;
            lpHeader->miNumFrunks -= liDropped;
        }
    }

    // ARTIST 8264D2D0..8264D50C. The replay rate is 60.0f at 82004C6C.
    // 82F2A638 starts at one and prevents the original diagnostic recursion
    // from reporting the same invalid range twice.
    void WriteStream::ResetStartFrame(f32 lfHistorySeconds)
    {
        static bool sbReportOutOfRange = true;
        const s32 liHistoryFrunks = static_cast<s32>(lfHistorySeconds * 60.0f);
        if (mpStreamHeader->miNumFrunks <= liHistoryFrunks)
            return;

        const s32 liFirst = mpStreamHeader->miFirstFrunk;
        s32 liNewFirst = liFirst;
        const s32 liDropCount = mpStreamHeader->miNumFrunks - liHistoryFrunks;
        for (s32 liIndex = liFirst; liIndex < liFirst + liDropCount; ++liIndex)
        {
            const s32 liSlot = liIndex % KI_MAX_FRUNKS;
            const u16 luFlags = mpStreamHeader->mpFrameOffsets[liSlot].mxFlags;
            if ((luFlags & KU_FLAG_KEYFRAME) != 0 && (luFlags & KU_FLAG_VOID) == 0)
                liNewFirst = liSlot;
        }
        s32 liNewCount = mpStreamHeader->miNumFrunks + liFirst - liNewFirst;
        if (liNewFirst < liFirst)
            liNewCount -= KI_MAX_FRUNKS;
        if (liNewCount < 0 || liNewCount > KI_MAX_FRUNKS)
        {
            if (!sbReportOutOfRange)
                return;
            if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                *CgsDev::Log::gpDebugPrint << "New num frunks gone out of range: "
                                         << liNewCount << " - re-calling function\n";
            sbReportOutOfRange = false;
            ResetStartFrame(lfHistorySeconds);
            sbReportOutOfRange = true;
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "New num frunks is out of range: " << liNewCount << "\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        mpStreamHeader->miNumFrunks = liNewCount;
        mpStreamHeader->miFirstFrunk = liNewFirst;
    }
}
