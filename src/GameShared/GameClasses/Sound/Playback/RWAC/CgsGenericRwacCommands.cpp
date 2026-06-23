// ============================================================================
// CgsGenericRwacCommands.cpp -- generic-RWAC command / parameter copy ctors.
//
// Definition home for the fixed-size generic-RWAC command records dispatched
// through GenericRwacFactory::HandlePluginEvent. Bodied from
// BURNOUT_X360_ARTIST.XEX:
//   RwacCommandPlayerPlayParameters::ctor          @ 0x826AD578
//   RwacCommandPlayerIsRequestDoneParameters::ctor @ 0x82681570
//   RwacCommandApplyReverbIRFile::ctor             @ 0x826816A0
//   RwacCommandGinsuAttachDataParameters::ctor     @ 0x82681738
//
// Each constructor copy-constructs a command record from a source record of the
// same shape (X360: a chain of lwz N(r5) / stw N(r3) over the record's words),
// then runs two debug asserts:
//   (1) sizeof(*this)/sizeof(uintptr_t) == luCommandCount -- the source-supplied
//       word count matches this record's own word count (2 for the three
//       single-operand commands, 3 for PlayerPlay; X360 cmplwi r4 against 2 / 3);
//   (2) the copied leading tag word equals the class's expected ERwacCommandType
//       (PlayerPlay == 6 / PlayerIsRequestDone == 7 / ReverbIR_apply == 9 /
//       GinsuAttach == 10; X360 cmpwi the just-stored 0(this) against 6/7/9/10).
//
// PlayerPlay additionally AddRefs its first operand: when the play-request
// pointer (word 1) is non-null the X360 ctor does lwz r10,4(r11) / addi r10,r10,1
// / stw r10,4(r11) -- ++mpPlayRequest->mu32RefCount -- BEFORE storing word 2 and
// running the asserts (matching the X360 instruction order).
//
// The asserts are vacuous in the house CGS_ASSERT substitution but are reproduced
// so the validated invariants stay visible. Member access is BY NAME (the tag via
// the RwacCommand base, operands via the named slots); no raw-offset cast.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacCommands.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// RwacCommandPlayerPlayParameters::ctor  @ 0x826AD578
//   copy 3 words (tag + 2 operands) from arSource; if operand 0 (the play-request
//   pointer) is non-null, ++mpPlayRequest->mu32RefCount; assert count == 3, tag == 6.
// ---------------------------------------------------------------------------
RwacCommandPlayerPlayParameters::RwacCommandPlayerPlayParameters(
        u32 luCommandCount, const RwacCommandPlayerPlayParameters& arSource)
{
    muCommandType = arSource.muCommandType;  // lwz r11,0(r5); stw r11,0(r31)
    mpPlayRequest = arSource.mpPlayRequest;  // lwz r11,4(r5); stw r11,4(r31)
    if (mpPlayRequest != 0)                  // cmplwi cr6,r11,0; beq
    {
        ++mpPlayRequest->mu32RefCount;       // lwz r10,4(r11); addi r10,r10,1; stw r10,4(r11)
    }
    maOperand = arSource.maOperand;          // lwz r11,8(r5); stw r11,8(r31)

    CGS_ASSERT(luCommandCount == 3u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS,
               "E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandPlayerIsRequestDoneParameters::ctor  @ 0x82681570
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 7.
// ---------------------------------------------------------------------------
RwacCommandPlayerIsRequestDoneParameters::RwacCommandPlayerIsRequestDoneParameters(
        u32 luCommandCount, const RwacCommandPlayerIsRequestDoneParameters& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11,0(this)
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS,
               "E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandApplyReverbIRFile::ctor  @ 0x826816A0
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 9.
// ---------------------------------------------------------------------------
RwacCommandApplyReverbIRFile::RwacCommandApplyReverbIRFile(
        u32 luCommandCount, const RwacCommandApplyReverbIRFile& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11,0(this)
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA,
               "E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandGinsuAttachDataParameters::ctor  @ 0x82681738
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 10.
// ---------------------------------------------------------------------------
RwacCommandGinsuAttachDataParameters::RwacCommandGinsuAttachDataParameters(
        u32 luCommandCount, const RwacCommandGinsuAttachDataParameters& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11,0(this)
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS,
               "E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS == GetCommandType()");
}

} // namespace Playback
} // namespace CgsSound
