// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::RaceCarEntitySerialiser)
//
// The race-car-entity replay serialiser: records / plays back the whole race-car
// world's per-frame state. Like its sibling serialisers it derives from BaseSerialiser
// and drives a fixed 0x1B000-byte static-layout buffer that holds two frames -- the
// "previous" frame at +0 and the "live" frame at +0xD7E0. Unlike the traffic/prop
// serialisers it does NOT walk the frame field-by-field: Read/Write serialise the whole
// 0xD7E0-byte live frame through the raw BaseSerialiser::Read/Write byte primitives.
//
// Source-of-truth ladder: every signature, branch, constant and store below is taken
// from the X360 ASM (dossier w33_carserial). Highlights:
//   * Construct @0x8264C4C0 -> BaseSerialiser::Construct(0,0,0x1B000,0x1B000,
//     "RaceCarEntity",1); then `stb 0,0x5C(this)` clears mbStaticLayoutCleared.
//   * GetStaticLayout @0x822A3400 asserts miStaticBufferSize >= 0x1B000 (ONE assert,
//     "Static buffer size is too small\n"; no non-null assert) then returns mpStaticBuffer.
//   * CheckStaticLayoutCleared @0x82657A48 lazily Clear()s the live frame (+0xD7E0) then
//     the previous frame (+0) on the first frame, then sets mbStaticLayoutCleared.
//   * Read @0x8264C518 reaches the frame path only for E_MODE_PLAYING (5); Write @0x8264C618
//     only for the recording family {1,2,3}. Both raw-serialise the live frame (0xD7E0 bytes).
//
// Callee declarations (BaseSerialiser, RaceCarSerialiserFrame) live in their own TUs;
// included here for the compile-only gate.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayRaceCarSerialiserFrame.h"

namespace BrnReplays
{
    // Static buffer size (Construct arg / GetStaticLayout guard): 0x1B000 == 110592.
    static const s32 KI_STATIC_BUFFER_SIZE = 0x1B000;

    // DWARF home: GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h.
    class RaceCarEntitySerialiser : public BaseSerialiser
    {
    public:
        // @0x8264C4C0
        s32 Construct();
        // @0x822A3400 -- returns the static-layout buffer, asserting it is big enough.
        RaceCarEntitySerialiserStaticLayout* GetStaticLayout();
        // @0x82657A48 -- lazily Clear() both frames on the first frame, mark it done.
        RaceCarSerialiserFrame* CheckStaticLayoutCleared();
        // @0x8264C518 / @0x8264C618 -- playback / record one frame (top-level dispatch).
        s32 Read();
        s32 Write();

    private:
        // X360: byte at this+0x5C, cleared by Construct, set by CheckStaticLayoutCleared
        // once both frames have been Clear()ed for this record/playback session.
        bool mbStaticLayoutCleared; // @0x5C
    };

    // -------------------------------------------------------------------------

    // @0x8264C4C0 (already homed in a prior wave; re-based here onto the real
    // BaseSerialiser home). id 0; mode 0; 0x1B000 dynamic + 0x1B000 static;
    // "RaceCarEntity"; flag 1; then clears the +0x5C byte.
    s32 RaceCarEntitySerialiser::Construct()
    {
        s32 liResult = BaseSerialiser::Construct(0, 0, KI_STATIC_BUFFER_SIZE,
                                                 KI_STATIC_BUFFER_SIZE, "RaceCarEntity", 1);
        mbStaticLayoutCleared = false;
        return liResult;
    }

    // @0x822A3400
    // Assert the static buffer is big enough, then return it. The X360 fires a SINGLE
    // assert (size), unlike the traffic sibling which also asserts non-null.
    RaceCarEntitySerialiserStaticLayout* RaceCarEntitySerialiser::GetStaticLayout()
    {
        // asm: `lwz r11,0x24(this)` (miStaticBufferSize) compared `< 0x1B000`; named-member
        // access is what matters on the 64-bit host.
        CGS_ASSERT(GetStaticBufferSize() >= KI_STATIC_BUFFER_SIZE,
                   "Static buffer size is too small\n");
        return reinterpret_cast<RaceCarEntitySerialiserStaticLayout*>(GetStaticBuffer());
    }

    // @0x82657A48
    // On the first frame after (re)start, when the static layout has not yet been cleared
    // and the static buffer exists, Clear() the live frame (+0xD7E0) then the previous
    // frame (+0) and mark it cleared. The X360 re-fetches GetStaticLayout() before each
    // access (three calls total; each re-runs the size assert).
    RaceCarSerialiserFrame* RaceCarEntitySerialiser::CheckStaticLayoutCleared()
    {
        if (mbStaticLayoutCleared)
        {
            // asm: the already-cleared early-out leaves `this` in r3 (an incidental return
            // value the caller does not consume); reproduced verbatim.
            return reinterpret_cast<RaceCarSerialiserFrame*>(this);
        }

        if (GetStaticLayout() == nullptr)
        {
            return nullptr;
        }

        // Clear the live frame first, then the previous frame (asm order 0x82657A84 then
        // 0x82657A90). CheckStaticLayoutCleared's result is the second Clear()'s return.
        GetStaticLayout()->mLiveFrame.Clear();
        RaceCarSerialiserFrame* lpResult = GetStaticLayout()->mPreviousFrame.Clear();
        mbStaticLayoutCleared = true;
        return lpResult;
    }

    // @0x8264C518
    // Top-level playback dispatch. The X360 mode cascade (0x8264C56C-0x8264C5DC) strips
    // every mode except E_MODE_PLAYING (5) from the frame path: {3,6}, then {1,4}, then
    // {7}, then the final {4,5,6} gate -- leaving only 5. The frame is read as a raw
    // 0xD7E0-byte blob into the live-frame region.
    s32 RaceCarEntitySerialiser::Read()
    {
        Lock();
        RaceCarEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
        CGS_ASSERT(lpLayout != nullptr, "lpStatic");

        if (meMode == E_MODE_PLAYING)
        {
            BaseSerialiser::Read(&lpLayout->mLiveFrame, sizeof(RaceCarSerialiserFrame));
        }

        return Unlock() ? 1 : 0;
    }

    // @0x8264C618
    // Top-level record dispatch (the write mirror of Read). The X360 guards on a non-null
    // static buffer (no assert, unlike Read), then reaches the frame path only for the
    // recording family {1,2,3}. The live frame is written as a raw 0xD7E0-byte blob.
    s32 RaceCarEntitySerialiser::Write()
    {
        Lock();
        RaceCarEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();

        if (lpLayout != nullptr)
        {
            if (meMode == E_MODE_RECORDING_PREPARING ||
                meMode == E_MODE_RECORDING ||
                meMode == E_MODE_RECORDING_STALLED)
            {
                BaseSerialiser::Write(&lpLayout->mLiveFrame, sizeof(RaceCarSerialiserFrame));
            }
        }

        return Unlock() ? 1 : 0;
    }
}
