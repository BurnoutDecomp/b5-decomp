#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (lock-state guards)

// =============================================================================
// BrnSound::Module::Io::LogicOutputBuffer accessors (this group's three TU functions).
//
// Each body asserts the buffer's lock state (read-lock bit 4 for the const getters,
// write-lock bit 3 for the mutable getter) then returns &member-at-X360-offset. The
// assert message strings ("Not locked for reading\n" / "Not locked for writing\n")
// match the X360 build's baked strings; see BrnSoundLogicModuleIo.h for the offset map.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// =============================================================================

namespace BrnSound
{
namespace Module
{
namespace Io
{

// X360 0x82695470 (IDA-truncated "BrnSound::Module::Io::LogicOutputBuffer::"). Read-lock
// accessor for the results sub-buffer at this+0x814.
//   lbz r11,0(this) ; extrwi r11,r11,1,27 -> read-lock bit 4 ; assert if clear
//   addi r3, this, 0x814 -> return &results
const LogicOutputResults& LogicOutputBuffer::GetResults() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return *reinterpret_cast<const LogicOutputResults*>(&maResultsStorage);
}

// X360 0x826955C0 (IDA-truncated "BrnSound::Module::Io::LogicOutputBuffer::GetRes").
// Write-lock accessor for the SAME results sub-buffer at this+0x814.
//   lbz r11,0(this) ; extrwi r11,r11,1,28 -> write-lock bit 3 ; assert if clear
//   addi r3, this, 0x814 -> return &results
LogicOutputResults& LogicOutputBuffer::GetResults()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return *reinterpret_cast<LogicOutputResults*>(&maResultsStorage);
}

// X360 0x82695320 ("BrnSound::Module::Io::LogicOutputBuffer::GetReplay"). Read-lock
// accessor for the replay-output sub-buffer at this+0x1824.
//   lbz r11,0(this) ; extrwi r11,r11,1,27 -> read-lock bit 4 ; assert if clear
//   addi r3, this, 0x1824 -> return &replay
const LogicOutputReplay& LogicOutputBuffer::GetReplay() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return *reinterpret_cast<const LogicOutputReplay*>(&maReplayStorage);
}

} // namespace Io
} // namespace Module
} // namespace BrnSound
