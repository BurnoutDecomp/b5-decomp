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

namespace BrnReplays
{
    // DWARF home: GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h.
    class PropEntitySerialiser : public BaseSerialiser
    {
    public:
        // @0x8264C6C0
        s32 Construct();
        // @0x822A4090 -- returns the static-layout buffer, asserting it is big enough.
        PropSerialiserStaticLayout* GetStaticLayout();
        // @0x8265A8E8 / @0x8265AA40 -- playback / record one frame.
        s32 Read(PropSerialiserStaticLayout* lpStaticLayout);
        s32 Write(PropSerialiserStaticLayout* lpStaticLayout);
        // @0x822CD650 -- lazily clear the previous frame on the first frame after a
        // (re)start, then mark it initialised.
        bool CheckPreviousFrameCleared();
        // @0x822E7738 / @0x822CD5B8 / @0x822CD790 -- zone bookkeeping (record-side only).
        PropLoadedZoneRecord* AddLoadedZone(s32 liZoneId);
        void RemoveLoadedZone(s32 liZoneId);
        void RemoveAllLoadedZones();
        // @0x822CD700 -- forward a prop's scene-membership change to the live frame.
        void SetPropAddedToScene(s32 liArg2, u32 luArg3, s32 liArg4);

    private:
        // True once the previous-frame slot has been cleared for this recording/playback
        // session. X360: byte at this+0x5C, cleared by Construct, set by
        // CheckPreviousFrameCleared. (BaseSerialiser occupies this+0x00..0x5A.)
        bool mbPreviousFrameInitialized; // @0x5C
    };

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
            lrLive.mbZonesLoaded   = false; // static 0x4008
            lrLive.mbAddedFlag15F0 = false; // static 0x5010
            lrLive.mbAddedFlag25E0 = false; // static 0x6000
            lrLive.mbAddedFlag27EC = false; // static 0x620C
            lrLive.mbAddedFlag2FF0 = false; // static 0x6A10
            lrLive.mbAddedFlag3800 = false; // static 0x7220
            lrLive.mbAddedFlag3910 = false; // static 0x7330
            lrLive.mbAddedFlag3A12 = false; // static 0x7432
            lrLive.mbAddedFlag0600 = false; // static 0x4020

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
        CGS_ASSERT(mbPreviousFrameInitialized, "mbPreviousFrameInitialized");

        PropSerialiserFrame* lpFrame = &GetStaticLayout()->mLiveFrame;
        PropLoadedZoneRecord* lpRecord = lpFrame->AllocateLoadedZoneRecord();
        // asm: store the zone id then zero the 160-byte trailing payload.
        lpRecord->miZoneId = liZoneId;
        for (u32 luByte = 0; luByte < sizeof(lpRecord->maZeroed); ++luByte)
        {
            lpRecord->maZeroed[luByte] = 0;
        }
        return lpRecord;
    }

    void PropEntitySerialiser::RemoveLoadedZone(s32 liZoneId)
    {
        CGS_ASSERT(meMode != E_MODE_PLAYING_PREPARING &&
                   meMode != E_MODE_PLAYING &&
                   meMode != E_MODE_PLAYING_STALLED,
                   "!IsPlaying()");

        GetStaticLayout()->mLiveFrame.RemoveLoadedZone(liZoneId);
    }

    void PropEntitySerialiser::RemoveAllLoadedZones()
    {
        CGS_ASSERT(meMode != E_MODE_PLAYING_PREPARING &&
                   meMode != E_MODE_PLAYING &&
                   meMode != E_MODE_PLAYING_STALLED,
                   "!IsPlaying()");

        // asm: `stb 0,0x4008(staticBase)` -- the live frame's zones-loaded flag.
        GetStaticLayout()->mLiveFrame.mbZonesLoaded = false;
    }

    void PropEntitySerialiser::SetPropAddedToScene(s32 liArg2, u32 luArg3, s32 liArg4)
    {
        CGS_ASSERT(meMode != E_MODE_PLAYING_PREPARING &&
                   meMode != E_MODE_PLAYING &&
                   meMode != E_MODE_PLAYING_STALLED,
                   "!IsPlaying()");

        GetStaticLayout()->mLiveFrame.SetPropAddedToScene(liArg2, luArg3, liArg4);
    }
}
