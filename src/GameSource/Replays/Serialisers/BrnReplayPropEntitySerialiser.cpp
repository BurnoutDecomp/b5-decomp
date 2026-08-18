// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::PropEntitySerialiser)
//
// The prop-entity replay serialiser: records / plays back the prop world's
// per-frame state. Like its sibling serialisers it derives from BaseSerialiser
// and forwards record/playback work to a PropSerialiserFrame held in the static
// layout buffer.
//
// Source-of-truth ladder: every signature, branch, constant and store below is
// taken from the X360 ASM (see scratchpad postmortem packet). Highlights:
//   * Construct @0x8264C6C0 -> BaseSerialiser::Construct(6,0,0x4000,0x7480,
//     "PropEntity",1); then `stb 0,0x5C(this)` clears mbPreviousFrameInitialized.
//   * The mode guards read meMode at *(this+0) and compare against the EMode
//     enumerators (4/5/6 == playing family -> "!IsPlaying()"; 1/2/3 == recording
//     family). The asserts' file/line are dropped per project convention.
//   * mbIsKeyFrame is read at *(this+0x50) to pick the key-frame vs delta path.
//   * The static layout buffer (GetStaticLayout) holds the previous frame at +0 and
//     the live frame at +0x3A20 (14880); record/playback drive the live frame then
//     copy it onto the previous frame (operator=).
//
// Callee declarations (BaseSerialiser, PropSerialiserFrame) live in their own TUs;
// included here for the compile-only gate.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"
// The class declaration moved to its DWARF home so other TUs (PropCellManager) can
// call it; this TU keeps the bodies. See the header banner.
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"

namespace BrnReplays
{
    // -------------------------------------------------------------------------

    s32 PropEntitySerialiser::Construct()
    {
        s32 liResult = BaseSerialiser::Construct(6, 0, 0x4000, 0x7480, "PropEntity", 1);
        mbPreviousFrameInitialized = false;
        return liResult;
    }

    PropSerialiserStaticLayout* PropEntitySerialiser::GetStaticLayout()
    {
        // asm: `lwz 0x24(this)` (miStaticBufferSize) compared `< 0x7480`; the named
        // member access is what matters on the 64-bit host.
        CGS_ASSERT(GetStaticBufferSize() >= 0x7480, "Static buffer size is too small");
        return reinterpret_cast<PropSerialiserStaticLayout*>(mpStaticBuffer);
    }

    // ------------------------------------------------------------------------
    // [FLAG PC boot gate] THE STATIC BUFFER IS NEVER HANDED OVER ON THIS BUILD.
    // ------------------------------------------------------------------------
    // BaseSerialiser::Construct sets mpStaticBuffer = 0 (faithfully -- the X360 does the
    // same at 0x8264C280); the pointer is filled in later by the replay manager's
    // serialiser registration/buffer hand-off, and NOTHING in this tree writes
    // BaseSerialiser::mpStaticBuffer -- grep it: the only assignment is the `= 0` in
    // Construct. So GetStaticLayout() returns NULL for every serialiser on this build.
    //
    // That was invisible until 2026-08-12, when the prop streamer first got far enough to
    // actually load a zone: PropEntityModule::LoadZone calls AddLoadedZone unconditionally
    // (correctly -- the console does too), which dereferenced the null layout and took an
    // access violation READING 0x4008 (== &mLiveFrame(0x3A20) + maLoadedZones.muLength(0x5E8)
    // off a null base), right after the "mbPreviousFrameInitialized" tripwire that
    // CheckPreviousFrameCleared had already declined to set for the same reason.
    //
    // The four record-side entry points below therefore no-op when the layout is absent.
    // This costs nothing observable on this build: the replay RECORD path is already inert
    // end-to-end (PropSerialiserFrame::Read / KeyFrameRead are parked in WorldLinkStubs, the
    // frame interior is still a padding ladder), and every caller discards the result.
    // RETIRE THIS GATE WHEN: the replay manager's serialiser buffer allocation lands and
    // mpStaticBuffer is real -- then these guards simply never fire.
    bool PropEntitySerialiser::HasStaticLayout()
    {
        if (GetStaticBuffer() != 0)
        {
            return true;
        }

        static bool sbLoggedOnce = false;
        if (!sbLoggedOnce && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedOnce = true;
            *CgsDev::Log::gpDebugPrint
                << "PropEntitySerialiser: no static buffer -- replay record inert "
                   "[FLAG PC boot gate]\n";
        }
        return false;
    }

    s32 PropEntitySerialiser::Read(PropSerialiserStaticLayout* lpStaticLayout)
    {
        Lock();
        CGS_ASSERT(lpStaticLayout != nullptr, "lpStaticLayout");

        // Only progress while actually playing back (mode == E_MODE_PLAYING). The asm
        // walks the playing family {4,5,6} then strips the prepare/stall/restore modes,
        // leaving E_MODE_PLAYING as the only mode that reaches the frame work.
        if (meMode == E_MODE_PLAYING)
        {
            PropSerialiserFrame* lpFrame = &lpStaticLayout->mLiveFrame;
            if (mbIsKeyFrame)
            {
                CGS_ASSERT(mbPreviousFrameInitialized, "mbPreviousFrameInitialized");
                lpFrame->KeyFrameRead(this);
            }
            else
            {
                CGS_ASSERT(mbPreviousFrameInitialized, "mbPreviousFrameInitialized");
                lpFrame->Read(this);
            }
            // Copy the freshly-read live frame onto the previous-frame slot.
            lpStaticLayout->mPreviousFrame = *lpFrame;
        }

        return Unlock() ? 1 : 0;
    }

    s32 PropEntitySerialiser::Write(PropSerialiserStaticLayout* lpStaticLayout)
    {
        Lock();
        CGS_ASSERT(lpStaticLayout != nullptr, "lpStaticLayout");

        // Only progress while recording (mode in the recording family {1,2,3}).
        if (meMode == E_MODE_RECORDING_PREPARING ||
            meMode == E_MODE_RECORDING ||
            meMode == E_MODE_RECORDING_STALLED)
        {
            // asm passes r5 = lpStaticLayout (== &mPreviousFrame, the static base).
            PropSerialiserFrame* lpFrame = &lpStaticLayout->mLiveFrame;
            PropSerialiserFrame* lpBase  = &lpStaticLayout->mPreviousFrame;
            if (mbIsKeyFrame)
            {
                CGS_ASSERT(mbPreviousFrameInitialized, "mbPreviousFrameInitialized");
                lpFrame->KeyFrameWrite(this, lpBase);
            }
            else
            {
                CGS_ASSERT(mbPreviousFrameInitialized, "mbPreviousFrameInitialized");
                lpFrame->Write(this, lpBase);
            }
            // Copy the freshly-written live frame onto the previous-frame slot.
            lpStaticLayout->mPreviousFrame = *lpFrame;
        }

        return Unlock() ? 1 : 0;
    }

    bool PropEntitySerialiser::CheckPreviousFrameCleared()
    {
        // GetStaticLayout asserts on a too-small buffer; the asm guards on its (non-null)
        // return before doing any work.
        PropSerialiserStaticLayout* lpStaticLayout = GetStaticLayout();
        if (lpStaticLayout == nullptr)
        {
            return false;
        }

        if (!mbPreviousFrameInitialized)
        {
            // Reset the live frame's per-sub-array "added to scene" flags, then snapshot
            // the cleared live frame into the previous-frame slot.
            PropSerialiserFrame& lrLive = lpStaticLayout->mLiveFrame;
            // static 0x4008 == frame 0x5E8 == the loaded-zone array's live count (see
            // BrnReplayPropSerialiserFrame.h): "no zones loaded" is count 0.
            lrLive.maLoadedZones.muLength = 0;
            // [2026-08-18, wave Q round 2] The eight `mbAddedFlagNNNN` placeholders were never
            // flags: each IS one sub-array's muLength byte. The frame is now nine real
            // BrnReplayArray members (BrnReplayPropSerialiserFrame.h), so these nine stores
            // read as what the console actually does -- "reset every record array to empty".
            // Same nine zero bytes, same order, same offsets (pinned by _AssertLayout).
            lrLive.maPropPositions.muLength    = 0; // static 0x5010 -> frame 0x15F0
            lrLive.maPropOrientations.muLength = 0; // static 0x6000 -> frame 0x25E0
            lrLive.maTypes.muLength            = 0; // static 0x620C -> frame 0x27EC
            lrLive.maPartPositions.muLength    = 0; // static 0x6A10 -> frame 0x2FF0
            lrLive.maPartOrientations.muLength = 0; // static 0x7220 -> frame 0x3800
            lrLive.maPartTypes.muLength        = 0; // static 0x7330 -> frame 0x3910
            lrLive.maPartIds.muLength          = 0; // static 0x7432 -> frame 0x3A12
            lrLive.maRecordedCells.muLength    = 0; // static 0x4020 -> frame 0x0600

            lpStaticLayout->mPreviousFrame = lrLive;
            mbPreviousFrameInitialized = true;
        }

        return true;
    }

    PropLoadedZoneRecord* PropEntitySerialiser::AddLoadedZone(s32 liZoneId)
    {
        CGS_ASSERT(meMode != E_MODE_PLAYING_PREPARING &&
                   meMode != E_MODE_PLAYING &&
                   meMode != E_MODE_PLAYING_STALLED,
                   "!IsPlaying()");
        // [FLAG PC boot gate] AHEAD of the console's own mbPreviousFrameInitialized tripwire:
        // with no static layout that flag can never be set (CheckPreviousFrameCleared bails on
        // the same condition), so leaving the assert in front would storm it once per loaded
        // zone for a state this build cannot reach. See HasStaticLayout above.
        if (!HasStaticLayout()) { return 0; }

        CGS_ASSERT(mbPreviousFrameInitialized, "mbPreviousFrameInitialized");

        PropSerialiserFrame* lpFrame = &GetStaticLayout()->mLiveFrame;
        PropLoadedZoneRecord* lpRecord = lpFrame->AllocateLoadedZoneRecord();
        // asm: store the zone id, then zero the 160-byte trailing payload as TWO 0x50-byte
        // runs of `std 0` -- which is exactly "clear both 600-bit arrays", 10 u64 fields each.
        lpRecord->miZoneId = liZoneId;
        lpRecord->maPropsAddedToScene.UnSetAll();
        lpRecord->maPropsPreviouslyHit.UnSetAll();
        return lpRecord;
    }

    void PropEntitySerialiser::RemoveLoadedZone(s32 liZoneId)
    {
        CGS_ASSERT(meMode != E_MODE_PLAYING_PREPARING &&
                   meMode != E_MODE_PLAYING &&
                   meMode != E_MODE_PLAYING_STALLED,
                   "!IsPlaying()");

        if (!HasStaticLayout()) { return; }   // [FLAG PC boot gate] see HasStaticLayout

        GetStaticLayout()->mLiveFrame.RemoveLoadedZone(liZoneId);
    }

    void PropEntitySerialiser::RemoveAllLoadedZones()
    {
        CGS_ASSERT(meMode != E_MODE_PLAYING_PREPARING &&
                   meMode != E_MODE_PLAYING &&
                   meMode != E_MODE_PLAYING_STALLED,
                   "!IsPlaying()");

        if (!HasStaticLayout()) { return; }   // [FLAG PC boot gate] see HasStaticLayout

        // asm: `stb 0,0x4008(staticBase)` -- the live frame's loaded-zone COUNT (frame +0x5E8).
        // One byte drops all nine records because the array is count-terminated.
        GetStaticLayout()->mLiveFrame.maLoadedZones.muLength = 0;
    }

    void PropEntitySerialiser::SetPropAddedToScene(s32 liArg2, u32 luArg3, s32 liArg4)
    {
        CGS_ASSERT(meMode != E_MODE_PLAYING_PREPARING &&
                   meMode != E_MODE_PLAYING &&
                   meMode != E_MODE_PLAYING_STALLED,
                   "!IsPlaying()");

        if (!HasStaticLayout()) { return; }   // [FLAG PC boot gate] see HasStaticLayout

        GetStaticLayout()->mLiveFrame.SetPropAddedToScene(liArg2, luArg3, liArg4);
    }
}
