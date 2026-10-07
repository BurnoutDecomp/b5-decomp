#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"

#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
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
