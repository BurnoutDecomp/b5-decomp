// ============================================================================
// CgsCsisCommands.cpp -- CSIS command-record constructors.
//
// Definition home for the fixed-size CSIS command records queued through the AEMS
// CsisCommandQueue. Bodied from BURNOUT_X360_ARTIST.XEX:
//   CsisSetClassHandleCommand::CsisSetClassHandleCommand  @ 0x826819E0
//   CsisCreateCommand::CsisCreateCommand                  @ 0x82681A90
//   CsisReleaseCommand::CsisReleaseCommand                @ 0x82681B38
//
// Each constructor copy-constructs a command record from a source record of the same
// shape (X360: a chain of lwz N(r5) / stw N(r3) over the record's words), then runs
// two debug asserts:
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
//   copy 5 words (tag + 4 operands) from arSource; assert count == 5 and tag == 0.
// ---------------------------------------------------------------------------
CsisSetClassHandleCommand::CsisSetClassHandleCommand(u32 luCommandCount,
                                                     const CsisSetClassHandleCommand& arSource)
{
    muCommandType  = arSource.muCommandType;  // stw r11, 0(this)
    maOperands[0]  = arSource.maOperands[0];  // stw r11, 4(this)
    maOperands[1]  = arSource.maOperands[1];  // stw r11, 8(this)
    maOperands[2]  = arSource.maOperands[2];  // stw r11, 0xC(this)
    maOperands[3]  = arSource.maOperands[3];  // stw r11, 0x10(this)

    CGS_ASSERT(luCommandCount == 5u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_CSIS_COMMAND_SET_CLASS_HANDLE,
               "E_CSIS_COMMAND_SET_CLASS_HANDLE == GetCommandType()");
}

// ---------------------------------------------------------------------------
// CsisCreateCommand::CsisCreateCommand  @ 0x82681A90
//   copy 4 words (tag + 3 operands) from arSource; assert count == 4 and tag == 1.
// ---------------------------------------------------------------------------
CsisCreateCommand::CsisCreateCommand(u32 luCommandCount, const CsisCreateCommand& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11, 0(this)
    maOperands[0] = arSource.maOperands[0]; // stw r11, 4(this)
    maOperands[1] = arSource.maOperands[1]; // stw r11, 8(this)
    maOperands[2] = arSource.maOperands[2]; // stw r11, 0xC(this)

    CGS_ASSERT(luCommandCount == 4u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_CSIS_COMMAND_CREATE,
               "E_CSIS_COMMAND_CREATE == GetCommandType()");
}

// ---------------------------------------------------------------------------
// CsisReleaseCommand::CsisReleaseCommand  @ 0x82681B38
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2 and tag == 2.
// ---------------------------------------------------------------------------
CsisReleaseCommand::CsisReleaseCommand(u32 luCommandCount, const CsisReleaseCommand& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11, 0(this)
    maOperand     = arSource.maOperand;     // stw r11, 4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_CSIS_COMMAND_RELEASE,
               "E_CSIS_COMMAND_RELEASE == GetCommandType()");
}

} // namespace Playback
} // namespace CgsSound
