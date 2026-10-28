#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"

#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"
#include <cstring>
#include "GameSource/Replays/BrnReplayShared.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::WriteStream).
//
//   ResetStream            @ 0x8264D100
//   InvalidateFrunksAhead  @ 0x8264D190
//
// When a frunk is written at index liStartFrunk, later frunks whose disk bytes
// it overwrites are stale. This marks those overlapped frunks VOID, then trims
// the leading edge of the live ring up to
// the first surviving keyframe so playback resumes from a self-contained frame.

namespace BrnReplays
{
    char WriteStream::mpcIntermediateBuffer[KI_INTERMEDIATEBUFFERSIZE];

    // Inlined by ARTIST ReplayModule::Construct @0x82656E20. The original
    // initializer does not store miCurrentWriteIndex or miNumFrunksAllocated here.
    void WriteStream::Construct()
    {
        mHeaderMalloc.Construct();
        mpStreamHeader = 0;
        miStallCount = 0;
        miFilePosition = 0;
        mbEnded = true;
        mbPaused = false;
        mpStream = 0;
        miBufferPosition = 0;
    }

    // @ 0x8264D100
    // Reset the write stream to an empty, freshly-allocated header. Frees every
    // allocation the header allocator has made, carves a new StreamHeader out of it,
    // stamps the "REPLAY " magic (through the terminating NUL) and zeroes the header's
    // frunk index, then clears the per-recording bookkeeping (write cursor, stall/alloc
    // counters, ended/paused flags, file + intermediate-buffer positions). The
    // GPUDiskWriteStream link (mpStream @0x50) is intentionally left untouched --
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
        lpHeader->mpFrameOffsets = 0;

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
    // The original private Write is inlined in AddFrunk. Failure neither copies
    // partial bytes nor advances the cursor; the enclosing AddFrunk records the stall.
    s32 WriteStream::Write(void* lpData, s32 liSize)
    {
        if (KI_INTERMEDIATEBUFFERSIZE - miBufferPosition < liSize)
            return 0;
        std::memcpy(mpcIntermediateBuffer + miBufferPosition, lpData, liSize);
        miBufferPosition += liSize;
        return liSize;
    }

    // ARTIST 8265EB70..F0F0. The GPU sink consumes 64 KiB-rounded byte spans; the
    // index stores the unrounded payload size. Padding bytes are not zero-filled.
    void WriteStream::AddFrunk(s64 liFrameNumber, f32 lfFrameTime,
                               FrunkSerialiserEntry* lpEntries, void** lppBuffers,
                               s32 liNumSerialisers, bool lbIsKeyFrame)
    {
        CGS_ASSERT(!mbEnded, "Stream has ended - need to start it again!\n");
        CGS_ASSERT(!mbPaused, "Stream is paused - must continue it first\n");
        CGS_ASSERT(lppBuffers != 0, "No blocks passed in\n");
        CGS_ASSERT(lpEntries != 0, "No block sizes in\n");

        StreamOffset* lpOffset;
        if (miNumFrunksAllocated < KI_MAX_FRUNKS)
        {
            lpOffset = static_cast<StreamOffset*>(mHeaderMalloc.Malloc(sizeof(StreamOffset)));
            CGS_ASSERT(lpOffset != 0, "Failed to allocate memory for new offset entry\n");
            if (mpStreamHeader->mpFrameOffsets == 0)
                mpStreamHeader->mpFrameOffsets = lpOffset;
            ++miNumFrunksAllocated;
        }
        else
        {
            CGS_ASSERT(mpStreamHeader->miNumFrunks <= KI_MAX_FRUNKS,
                       "Should never go past max frames\n");
            lpOffset = &mpStreamHeader->mpFrameOffsets[miCurrentWriteIndex];
        }

        const u16 luFlags = lbIsKeyFrame ? KU_FLAG_KEYFRAME : 0;
        lpOffset->mfFrameTime = lfFrameTime;
        lpOffset->miFrameNumber = liFrameNumber;
        lpOffset->mxFlags = luFlags;
        lpOffset->miFileOffset = miFilePosition;

        static const char KacFrunkMagic[4] = {'f','r','k','\0'};
        for (s32 liByte = 0; ; ++liByte)
        {
            mCurrentFrunk.macMagicNumber[liByte] = KacFrunkMagic[liByte];
            if (KacFrunkMagic[liByte] == '\0')
                break;
        }
        mCurrentFrunk.miFrameNumber = liFrameNumber;
        mCurrentFrunk.mfFrameTime = lfFrameTime;
        mCurrentFrunk.miNumSerialisers = liNumSerialisers;
        mCurrentFrunk.mxFlags = luFlags;

        const s32 liEntriesSize = liNumSerialisers * static_cast<s32>(sizeof(FrunkSerialiserEntry));
        s32 liExpectedSize = static_cast<s32>(sizeof(FrunkHeader)) + liEntriesSize;
        s32 liWritten = Write(&mCurrentFrunk, static_cast<s32>(sizeof(FrunkHeader)));
        liWritten += Write(lpEntries, liEntriesSize);
        for (s32 liEntry = 0; liEntry < liNumSerialisers; ++liEntry)
        {
            liExpectedSize += lpEntries[liEntry].miSize;
            liWritten += Write(lppBuffers[liEntry], lpEntries[liEntry].miSize);
        }

        bool lbSuccess = liWritten == liExpectedSize;
        const s32 liRoundedSize = (liWritten + 0xFFFF) & ~0xFFFF;
        if (lbSuccess)
            lbSuccess = mpStream->AddBlock(lpOffset->miFileOffset,
                reinterpret_cast<const u8*>(mpcIntermediateBuffer), liRoundedSize);
        if (lbSuccess)
        {
            lpOffset->miFrunkSize = liWritten;
            miFilePosition += liRoundedSize;
        }
        else
        {
            lpOffset->miFrunkSize = 0;
            lpOffset->mxFlags |= KU_FLAG_VOID;
            ++miStallCount;
        }

        if (mpStreamHeader->miNumFrunks < KI_MAX_FRUNKS)
            ++mpStreamHeader->miNumFrunks;
        else
            mpStreamHeader->miFirstFrunk = (miCurrentWriteIndex + 1) % KI_MAX_FRUNKS;
        InvalidateFrunksAhead(miCurrentWriteIndex);
        ++miCurrentWriteIndex;
        if (miCurrentWriteIndex >= KI_MAX_FRUNKS)
        {
            CGS_ASSERT(miCurrentWriteIndex == KI_MAX_FRUNKS,
                       "Should never go past max frames for write index\n");
            miCurrentWriteIndex = 0;
            miFilePosition = 0;
        }
        miBufferPosition = 0;
    }

}
