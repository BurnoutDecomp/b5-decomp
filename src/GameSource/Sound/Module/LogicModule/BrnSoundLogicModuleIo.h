#ifndef BRN_SOUND_MODULE_LOGIC_MODULE_BRN_SOUND_LOGIC_MODULE_IO_H
#define BRN_SOUND_MODULE_LOGIC_MODULE_BRN_SOUND_LOGIC_MODULE_IO_H

#include <cstddef>   // offsetof (buffer layout asserts)
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (base; lock state machine)

// =============================================================================
// BrnSound::Module::Io::LogicOutputBuffer
//   GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h (assert-cited home) +
//   GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// LogicOutputBuffer is the per-frame payload the sound LOGIC module produces for the
// ROOT module to consume. Like every CgsModule::IOBuffer it carries a 1-byte status
// flag at offset 0; the lock-guarded Get* accessors assert the right lock (read-lock
// bit 4 for const readers, write-lock bit 3 for the mutable writer) then return
// &member-at-X360-offset. Same shape as the committed BrnRootSoundModuleIo.h buffers.
//
// This group bodies the THREE TU functions (all assert against
// BrnSoundLogicModuleIo.h baked file string):
//   * GetReplay() const  (X360 0x82695320) -- read-lock ("Not locked for reading\n",
//     baked :79), returns the replay-output sub-buffer at this+0x1824 (6180).
//   * GetResults() const (X360 0x82695470, IDA-truncated to "LogicOutputBuffer::")
//     -- read-lock ("Not locked for reading\n", baked :113), returns the results
//     sub-buffer at this+0x814 (2068).
//   * GetResults()       (X360 0x826955C0, IDA-truncated to "GetRes") -- write-lock
//     ("Not locked for writing\n", baked :128), returns the SAME results sub-buffer
//     at this+0x814 (2068). The const/non-const pair guards the same member with the
//     read vs write lock respectively (the established IOBuffer accessor idiom).
//
// LOCK-BIT MAP (authoritative, from the asm `extrwi` field extracts on the lbz'd
// status byte at offset 0):
//   GetReplay   0x82695324..: extrwi r11,r11,1,27  -> bit 4 (mask 0x10) read-lock
//   GetResults  0x82695474..: extrwi r11,r11,1,27  -> bit 4 (mask 0x10) read-lock
//   GetResults  0x826955D4..: extrwi r11,r11,1,28  -> bit 3 (mask 0x08) write-lock
// These match CgsModule::IOBuffer::eStatusLockedForRead/Write exactly, so the bodies
// reuse IsBufferLockedForReading()/IsBufferLockedForWriting() by NAME.
//
// MINIMAL SLICE: the buffer models only its two touched members, each pinned to its
// exact X360 byte offset with explicit u8 storage for the gaps (the BrnRootSoundModuleIo.h
// precedent). Only the IOBuffer base (1 byte) + u8 storage precede each touched member
// (no host-width pointers), so offsetof IS byte-faithful on the 64-bit gate and the
// member offsets are static_asserted.
// FLAG (un-DWARF'd member names/types): the two returned sub-objects ("results" @ +0x814
// and "replay" @ +0x1824) are named opaque byte storage of unknown element type; only
// their +0x814 / +0x1824 offsets and the lock-state guards are X360 facts. The IDA-
// truncated method names (GetResults / GetReplay) are inferred from the call-site usage
// (RootSoundModule::Update / BridgeLogicToRoot / RegistryLoad) + the lock direction; the
// X360 bodies (lock assert + return &member-at-offset) are exact.
// =============================================================================

namespace BrnSound
{
namespace Module
{
namespace Io
{
    // The "results" sub-buffer returned (by reference) from GetResults @ +0x814. Its
    // real element type is UNVERIFIED (the X360 only does `addi r3, this, 0x814` --
    // take-address); modelled as named opaque storage so the buffer's later full
    // reconstruction can replace it without moving anything.
    struct LogicOutputResults;

    // The "replay" sub-buffer returned (by const reference) from GetReplay @ +0x1824.
    // Same opaque-storage treatment.
    struct LogicOutputReplay;

    // BrnSound::Module::Io::LogicOutputBuffer -- the per-frame logic OUTPUT payload the
    // sound logic module fills for the root module. Derives from CgsModule::IOBuffer.
    struct LogicOutputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x82695470 (read-lock; "Not locked for reading\n", baked :113) -- the
        // results sub-buffer at this+0x814. Const reader.
        const LogicOutputResults& GetResults() const;

        // X360 0x826955C0 (write-lock; "Not locked for writing\n", baked :128) -- the
        // SAME results sub-buffer at this+0x814. Mutable writer.
        LogicOutputResults& GetResults();

        // X360 0x82695320 (read-lock; "Not locked for reading\n", baked :79) -- the
        // replay-output sub-buffer at this+0x1824. Const reader.
        const LogicOutputReplay& GetReplay() const;

    private:
        // base end (1 byte) -> +0x0814 results sub-buffer.
        u8 maPadToResults[0x814 - sizeof(CgsModule::IOBuffer)];
        // results sub-buffer @ +0x0814; named opaque storage (full layout deferred).
        u8 maResultsStorage[0x1824 - 0x814];                     // @ +0x0814
        // replay sub-buffer @ +0x1824; named opaque storage (full layout deferred).
        u8 maReplayStorage[0x40];                                // @ +0x1824

        static void _AssertLayout()
        {
            static_assert(offsetof(LogicOutputBuffer, maResultsStorage) == 0x814,
                          "LogicOutputBuffer results sub-buffer @ +0x814");
            static_assert(offsetof(LogicOutputBuffer, maReplayStorage) == 0x1824,
                          "LogicOutputBuffer replay sub-buffer @ +0x1824");
        }
    };

} // namespace Io
} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_LOGIC_MODULE_BRN_SOUND_LOGIC_MODULE_IO_H
