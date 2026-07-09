#include "GameSource/Replays/BrnReplayBaseSerialiser.h"

#include "GameSource/Replays/BrnReplayShared.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>
#include <cstddef>  // offsetof

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::BaseSerialiser).
//
//   IsPlaying    @ 0x821F3440 : mode in {PLAYING_PREPARING, PLAYING, PLAYING_STALLED}
//   IsRecording  @ 0x821F3470 : mode in {RECORDING_PREPARING, RECORDING, RECORDING_STALLED}
//   Lock         @ 0x823A6718 : assert !mbLocked, then set mbLocked
//   Unlock       @ 0x823A67C0 : assert  mbLocked, then clear mbLocked
//   SetMode      @ 0x8264B0F8 : assert  mbLocked, then store new mode
//   Read         @ 0x8264C188 : assert playing+locked+buffer+space, memcpy out, advance read cursor
//   Write        @ 0x8264C050 : assert recording+locked+buffer+space, memcpy in (only while
//                               E_MODE_RECORDING), advance used cursor
//
// The asserts' file/line in the X360 strings point at BrnReplayBaseSerialiser.h
// (Lock/Unlock/SetMode) and BrnReplayBaseSerialiserDataFunctions.h (Read/Write);
// CGS_ASSERT records this file's __FILE__/__LINE__, which is the project-faithful
// equivalent (the messages are preserved verbatim).

namespace BrnReplays
{
    // @ 0x821F3440
    // True while the serialiser is in any of the three playback sub-states.
    bool BaseSerialiser::IsPlaying() const
    {
        return meMode == E_MODE_PLAYING_PREPARING
            || meMode == E_MODE_PLAYING
            || meMode == E_MODE_PLAYING_STALLED;
    }

    // @ 0x821F3470
    // True while the serialiser is in any of the three recording sub-states.
    bool BaseSerialiser::IsRecording() const
    {
        return meMode == E_MODE_RECORDING_PREPARING
            || meMode == E_MODE_RECORDING
            || meMode == E_MODE_RECORDING_STALLED;
    }

    // @ 0x823A6718
    // Take the buffer lock. Asserts the serialiser is not already locked, then
    // raises mbLocked. Returns true (the X360 body always returns 1).
    bool BaseSerialiser::Lock()
    {
        CGS_ASSERT(!mbLocked, "Already locked\n");

        mbLocked = true;
        return true;
    }

    // @ 0x823A67C0
    // Release the buffer lock. Asserts the serialiser is currently locked, then
    // clears mbLocked. Returns true (the X360 body always returns 1).
    bool BaseSerialiser::Unlock()
    {
        CGS_ASSERT(mbLocked, "Not locked\n");

        mbLocked = false;
        return true;
    }

    // @ 0x8264B0F8
    // Change the serialiser mode. Mode may only be changed while the buffer is
    // locked.
    void BaseSerialiser::SetMode(EMode leMode)
    {
        CGS_ASSERT(mbLocked, "Can not change mode while not locked\n");

        meMode = leMode;
    }

    // @ 0x8264C188
    // Pop liSize bytes off the playback buffer into lpData. Validates that the
    // serialiser is playing, locked, has an allocated buffer, and holds enough
    // unread data; copies from mpBuffer + miBufferRead and advances the read
    // cursor. Returns the number of bytes read.
    s32 BaseSerialiser::Read(void* lpData, s32 liSize)
    {
        CGS_ASSERT(meMode == E_MODE_PLAYING, "Not playing\n");
        CGS_ASSERT(mbLocked, "Not locked\n");
        CGS_ASSERT(mpBuffer != nullptr, "Buffer not allocated\n");
        CGS_ASSERT(miBufferUsed - miBufferRead >= liSize,
                   "Not enough data in buffer\n");

        std::memcpy(lpData,
                    static_cast<u8*>(mpBuffer) + miBufferRead,
                    liSize);
        miBufferRead += liSize;
        return liSize;
    }

    // @ 0x8264C050
    // Append liSize bytes from lpData to the record buffer. Validates that the
    // serialiser is recording, locked, has an allocated buffer, and holds enough
    // free space. The bytes are only actually committed (and the used cursor
    // advanced) while meMode == E_MODE_RECORDING; in the other recording
    // sub-states the call validates but writes nothing and returns 0.
    s32 BaseSerialiser::Write(const void* lpData, s32 liSize)
    {
        CGS_ASSERT(IsRecording(), "Not recording\n");
        CGS_ASSERT(mbLocked, "Not locked\n");
        CGS_ASSERT(mpBuffer != nullptr, "Buffer not allocated\n");
        CGS_ASSERT(miBufferSize - miBufferUsed >= liSize,
                   "Not enough space in buffer\n");

        if (meMode == E_MODE_RECORDING)
        {
            std::memcpy(static_cast<u8*>(mpBuffer) + miBufferUsed,
                        lpData,
                        liSize);
            miBufferUsed += liSize;
            return liSize;
        }

        return 0;
    }

    // @ 0x8264C470
    // Mode-directed serialise dispatch. The X360 body switches on meMode (read at
    // *this+0): in E_MODE_RECORDING it tail-calls Write(buffer, size); in
    // E_MODE_PLAYING it tail-calls Read(buffer, size); any other mode returns 0
    // without touching the buffer. The compares are against the exact RECORDING(2)
    // and PLAYING(5) modes -- the prepare/stalled sub-states do NOT serialise here.
    // (Both branches are tail-calls that leave the buffer/size arg registers
    // untouched, so the same lpBuffer/liSize pass straight through.)
    s32 BaseSerialiser::Serialise(void* lpBuffer, s32 liSize)
    {
        if (meMode == E_MODE_RECORDING)
            return Write(lpBuffer, liSize);

        if (meMode == E_MODE_PLAYING)
            return Read(lpBuffer, liSize);

        return 0;
    }

    // -------- explicit instantiations (force the out-of-line emission the X360 ARTIST build
    //          produced for each queue capacity; generic bodies live in the header) --------
    //   ReadVariableQueue<13312,16>  @ 0x82653A60 (DirectorBridgeSerialiser::SerialiseGameActionQueue)
    //   ReadVariableQueue<512,16>    @ 0x82656CF0 (SoundSerialiser::Read)
    //   WriteVariableQueue<13312,16> @ 0x826539B8 (DirectorBridgeSerialiser::SerialiseGameActionQueue)
    template s32 BaseSerialiser::ReadVariableQueue<13312, 16>(CgsModule::VariableEventQueue<13312, 16>*);
    template s32 BaseSerialiser::ReadVariableQueue<512, 16>(CgsModule::VariableEventQueue<512, 16>*);
    template s32 BaseSerialiser::WriteVariableQueue<13312, 16>(CgsModule::VariableEventQueue<13312, 16>*);
}
