// ============================================================================
// CgsCsisCommands.cpp -- CSIS command-record constructors.
//
// Definition home for the fixed-size CSIS command records queued through the AEMS
// CsisCommandQueue. Bodied from BURNOUT_X360_ARTIST.XEX:
//   CsisSetClassHandleCommand::CsisSetClassHandleCommand  @ 0x826819E0
//   CsisCreateCommand::CsisCreateCommand                  @ 0x82681A90
//   CsisReleaseCommand::CsisReleaseCommand                @ 0x82681B38
//   CsisUpdateCommand::CsisUpdateCommand                  @ 0x82681BD0
//
// Each constructor copies the fixed word count from the queue's uintptr_t buffer
// (X360: a chain of lwz N(r5) / stw N(r3) over the record's words), then runs two
// debug asserts:
//   (1) sizeof(*this)/sizeof(uintptr_t) == luCommandCount  -- the source-supplied
//       word count matches this record's own word count (4 for Create, 5 for
//       SetClassHandle, 2 for Release; X360 cmplwi r4 against 4 / 5 / 2);
//   (2) the copied leading tag word equals the class's expected ECsisCommandType
//       (E_CSIS_COMMAND_SET_CLASS_HANDLE == 0 / _CREATE == 1 / _RELEASE == 2; X360
//       cmpwi the just-stored 0(this) against 0 / 1 / 2).
// The asserts are vacuous in the house CGS_ASSERT substitution but are reproduced so
// the validated invariants stay visible. Member access is BY NAME (the tag word via
// the CsisCommand base, the operands via the named array); no raw-offset cast.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// CsisSetClassHandleCommand::CsisSetClassHandleCommand  @ 0x826819E0
//   copy 5 words (tag + 4 operands) from the command buffer; assert count == 5 and tag == 0.
// ---------------------------------------------------------------------------
CsisSetClassHandleCommand::CsisSetClassHandleCommand(uintptr_t luCommandCount,
                                                     uintptr_t* lpuCommandBuffer)
    : CsisCommand(static_cast<u32>(lpuCommandBuffer[0]))
{
    maOperands[0] = lpuCommandBuffer[1];  // stw r11, 4(this)
    maOperands[1] = lpuCommandBuffer[2];  // stw r11, 8(this)
    maOperands[2] = lpuCommandBuffer[3];  // stw r11, 0xC(this)
    maOperands[3] = lpuCommandBuffer[4];  // stw r11, 0x10(this)

    CGS_ASSERT(luCommandCount == 5u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_CSIS_COMMAND_SET_CLASS_HANDLE,
               "E_CSIS_COMMAND_SET_CLASS_HANDLE == GetCommandType()");
}

// ---------------------------------------------------------------------------
// CsisCreateCommand::CsisCreateCommand  @ 0x82681A90
//   copy 4 words (tag + 3 operands) from the command buffer; assert count == 4 and tag == 1.
// ---------------------------------------------------------------------------
CsisCreateCommand::CsisCreateCommand(uintptr_t luCommandCount,
                                     uintptr_t* lpuCommandBuffer)
    : CsisCommand(static_cast<u32>(lpuCommandBuffer[0]))
{
    maOperands[0] = lpuCommandBuffer[1]; // stw r11, 4(this)
    maOperands[1] = lpuCommandBuffer[2]; // stw r11, 8(this)
    maOperands[2] = lpuCommandBuffer[3]; // stw r11, 0xC(this)

    CGS_ASSERT(luCommandCount == 4u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_CSIS_COMMAND_CREATE,
               "E_CSIS_COMMAND_CREATE == GetCommandType()");
}

// ---------------------------------------------------------------------------
// CsisReleaseCommand::CsisReleaseCommand  @ 0x82681B38
//   copy 2 words (tag + 1 operand) from the command buffer; assert count == 2 and tag == 2.
// ---------------------------------------------------------------------------
CsisReleaseCommand::CsisReleaseCommand(uintptr_t luCommandCount,
                                       uintptr_t* lpuCommandBuffer)
    : CsisCommand(static_cast<u32>(lpuCommandBuffer[0]))
{
    mhClass = lpuCommandBuffer[1]; // stw r11, 4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_CSIS_COMMAND_RELEASE,
               "E_CSIS_COMMAND_RELEASE == GetCommandType()");
}

// ---------------------------------------------------------------------------
// CsisUpdateCommand::CsisUpdateCommand  @ 0x82681BD0
//   copy 3 words (tag + 2 operands) from the command buffer; assert count == 3 and tag == 3.
// ---------------------------------------------------------------------------
CsisUpdateCommand::CsisUpdateCommand(uintptr_t luCommandCount,
                                     uintptr_t* lpuCommandBuffer)
    : CsisCommand(static_cast<u32>(lpuCommandBuffer[0]))
{
    maOperands[0] = lpuCommandBuffer[1]; // stw r11, 4(this)
    maOperands[1] = lpuCommandBuffer[2]; // stw r11, 8(this)

    CGS_ASSERT(luCommandCount == 3u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_CSIS_COMMAND_UPDATE,
               "E_CSIS_COMMAND_UPDATE == GetCommandType()");
}

} // namespace Playback
} // namespace CgsSound
