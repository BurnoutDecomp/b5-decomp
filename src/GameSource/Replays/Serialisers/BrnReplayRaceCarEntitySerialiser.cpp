// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::RaceCarEntitySerialiser)
//
// The race-car-entity replay serialiser: records / plays back the whole race-car
// world's per-frame state. Like its sibling serialisers it derives from BaseSerialiser
// and drives a two-frame static owner -- the previous frame followed by the live frame and
// the original 64-byte reserve. X360 passes 0x1B000 bytes and serialises a 0xD7E0 frame;
// the typed native owner widens handles/ids and derives the PC capacities from sizeof.
// Read/Write still pass the entire live frame through BaseSerialiser's raw byte primitives.
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
// Typed frame fields are attested by UpdateOutputInterfaces @0x822F5CF8 and
// UpdateSerialiser @0x82306120; Frame::Clear and operator= are recovered in this TU.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h"
#include <cstring>

namespace BrnReplays
{
    // Both X360 capacities are 0x1B000. The native typed owner retains the two frames and
    // the original 64-byte reserve while allowing its ResourceHandle members to stay native.
    static const s32 KI_STATIC_BUFFER_SIZE =
        static_cast<s32>(sizeof(RaceCarEntitySerialiserStaticLayout));


    // -------------------------------------------------------------------------

    // @0x82657748. Clear the two interface snapshots, then zero and seed each visual
    // record. This method operates on the replay buffer's raw storage; after the console's
    // 5280-byte clear the queue is reconstructed so its inline event pointer is valid.
    RaceCarSerialiserFrame* RaceCarSerialiserFrame::Clear()
    {
        mActiveRaceCarOutputInterface.Clear();
        mGlobalRaceCarOutputInterface.Clear();

        for (u32 luSlot = 0; luSlot < KU_RACECAR_FRAME_SLOT_COUNT; ++luSlot)
        {
            BrnWorld::ActiveRaceCar::RenderParams& lrRenderParams = maRenderParams[luSlot];
            std::memset(&lrRenderParams, 0, sizeof(lrRenderParams));

            lrRenderParams.mBodyTransform.SetIdentity();
            for (u32 luWheel = 0; luWheel < 6u; ++luWheel)
            {
                lrRenderParams.mWheelTransforms[luWheel].SetIdentity();
                lrRenderParams.mWheelScaleTransforms[luWheel].SetIdentity();
            }

            lrRenderParams.mPaintColour.x = 1.0f;
            lrRenderParams.mPaintColour.y = 1.0f;
            lrRenderParams.mPaintColour.z = 1.0f;
            lrRenderParams.mPaintColour.w = 1.0f;
            lrRenderParams.mPearlescentColour.x = 1.0f;
            lrRenderParams.mPearlescentColour.y = 1.0f;
            lrRenderParams.mPearlescentColour.z = 1.0f;
            lrRenderParams.mPearlescentColour.w = 1.0f;

            // ARTIST 0x82657804 seeds r20=-1; std at 0x826579C8/CC fills both words.
            lrRenderParams.mBodyPartVisibility.SetBitField(0u, ~u64(0));
            lrRenderParams.mBodyPartVisibility.SetBitField(1u, ~u64(0));
            lrRenderParams.maDetachedParts.Construct();
            lrRenderParams.mLOD = CgsGraphics::Model::E_STATE_LOD_4;
            lrRenderParams.mbBluesAndTwosCanSwitchState = true;

            maRaceCarIds[luSlot] = 0;
            maWheelIds[luSlot] = 0;
        }

        return this;
    }

    // @0x822E7820. Copy the typed interfaces and all eight render records in slot order.
    // Their member-wise operators preserve native resource handles and queue storage.
    RaceCarSerialiserFrame& RaceCarSerialiserFrame::operator=(
            const RaceCarSerialiserFrame& lrOther)
    {
        if (this == &lrOther)
        {
            return *this;
        }

        mActiveRaceCarOutputInterface = lrOther.mActiveRaceCarOutputInterface;
        mGlobalRaceCarOutputInterface = lrOther.mGlobalRaceCarOutputInterface;
        for (u32 luSlot = 0; luSlot < KU_RACECAR_FRAME_SLOT_COUNT; ++luSlot)
        {
            maRenderParams[luSlot] = lrOther.maRenderParams[luSlot];
        }
        for (u32 luSlot = 0; luSlot < KU_RACECAR_FRAME_SLOT_COUNT; ++luSlot)
        {
            maRaceCarIds[luSlot] = lrOther.maRaceCarIds[luSlot];
            maWheelIds[luSlot] = lrOther.maWheelIds[luSlot];
        }
        return *this;
    }

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
        // asm: `lwz r11,0x24(this)` (miStaticBufferSize) compared `< 0x1B000`; the native
        // target uses sizeof(the typed owner), which keeps its 64-bit handles intact.
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
