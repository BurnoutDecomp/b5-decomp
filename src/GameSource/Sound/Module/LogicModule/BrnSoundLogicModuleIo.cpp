#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (lock-state guards)

// =============================================================================
// BrnSound::Module::Io::LogicOutputBuffer accessors (this group's two wave-7 TU
// functions: the GetAttribSysRequestInterface const/non-const overloads).
//
// Each body asserts the buffer's lock state (read-lock bit 4 for the const getter,
// write-lock bit 3 for the mutable getter) then returns &member-at-X360-offset. The
// assert message strings ("Not locked for reading\n" / "Not locked for writing\n")
// match the X360 build's baked strings; see BrnSoundLogicModuleIo.h for the offset map.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The
// prior GetResults/GetReplay bodies were removed: they modelled a results/replay shape
// contradicted by the DWARF (see BrnSoundLogicModuleIo.h reconciliation note).
// =============================================================================

namespace BrnSound
{
namespace Module
{
namespace Io
{

// X360 0x82695518 (IDA-truncated "BrnSound::Module::Io::LogicOut"). Read-lock accessor for
// the AttribSys request interface at this+0x04 (DWARF BrnSoundLogicModuleIo.h:81
// mAttribSysRequestInterface, the FIRST member; getter decl :69).
//   lbz r11,0(this) ; extrwi r11,r11,1,27 -> read-lock bit 4 ; assert if clear
//   addi r3, this, 4 -> return &mAttribSysRequestInterface
const RootOutputBuffer::AttribSysRequestInterface* LogicOutputBuffer::GetAttribSysRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const RootOutputBuffer::AttribSysRequestInterface*>(&mAttribSysRequestInterfaceStorage);
}

// X360 0x82695668 (IDA-truncated "BrnSound::Module::Io::LogicOutputBuf"). Write-lock
// accessor for the SAME AttribSys request interface at this+0x04 (getter decl :77).
//   lbz r11,0(this) ; extrwi r11,r11,1,28 -> write-lock bit 3 ; assert if clear
//   addi r3, this, 4 -> return &mAttribSysRequestInterface
RootOutputBuffer::AttribSysRequestInterface* LogicOutputBuffer::GetAttribSysRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<RootOutputBuffer::AttribSysRequestInterface*>(&mAttribSysRequestInterfaceStorage);
}

} // namespace Io
} // namespace Module
} // namespace BrnSound
