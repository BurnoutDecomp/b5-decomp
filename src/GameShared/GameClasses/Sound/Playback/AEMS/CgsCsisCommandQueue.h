#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSCSISCOMMANDQUEUE_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSCSISCOMMANDQUEUE_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsCommandQueue.h"

#include <cstdint> // std::uintptr_t

// =============================================================================
// CgsSound::Playback::CsisCommandQueue -- the typed enqueue (operator<<) family.
//   GameShared/GameClasses/Sound/Playback/aems/CgsAemsFactory.h (DWARF/source home;
//   the X360 overflow assert fires from CgsAemsFactory.h:248) +
//   GameShared/GameClasses/Sound/Playback/AEMS/CgsCsisCommandQueue.cpp (this TU).
//
// This TU homes the four explicit instantiations of the free operator<< that posts a
// fixed-size CSIS command record onto a CsisCommandQueue. Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   operator<< <CsisSetClassHandleCommand> @ 0x826C64A0  (posts 5 words)
//   operator<< <CsisUpdateCommand>         @ 0x826C6550  (posts 3 words)
//   operator<< <CsisCreateCommand>         @ 0x826C6600  (posts 4 words)
//   operator<< <CsisReleaseCommand>        @ 0x826C66B0  (posts 2 words)
// Each is the compiler's instantiation of one generic template body: post
// sizeof(T)/sizeof(uintptr_t) words (the X360 emits the literal 4/2/5/3 the sizeof
// folds to) through the queue's CommandQueue<>::PostCommand, and assert on overflow.
//
// REUSE (by name): CsisCommandQueue, CommandQueue<255u,uintptr_t>::PostCommand,
// CsisCommand::AsCommand() and CsisReleaseCommand are the committed types from
// CgsCommandQueue.h (this TU's owning class lives there); they are used, not forked.
//
// FLAG (irreconcilable committed homes -> the three sibling records are mirrored
// here): the other three CSIS command records (CsisCreateCommand /
// CsisSetClassHandleCommand / CsisUpdateCommand) are committed in CgsAemsFactory.h,
// but that header re-declares CsisCommand / CsisReleaseCommand / ECsisCommandType /
// CsisCommandQueue with shapes that differ from CgsCommandQueue.h, so the two headers
// cannot be included together. Because operator<< needs both the CsisCommandQueue ring
// (only in CgsCommandQueue.h) and every command record's size, the three sibling
// records are re-expressed here with the SAME word layout as CgsAemsFactory.h but
// re-parented onto CgsCommandQueue.h's CsisCommand (which carries AsCommand()). This
// duplication mirrors the pre-existing CsisCommand/CsisReleaseCommand duplication
// between the two committed headers; a full reconciliation of the two homes is a
// separate keystone task and is deliberately NOT attempted here.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsAemsFactory.h:130 (DWARF). Set-class-handle command: 5 words (tag + 4 operands).
// Type tag E_CSIS_COMMAND_SET_CLASS_HANDLE (== 0). Word count posted: 5.
struct CsisSetClassHandleCommand : public CsisCommand
{
    std::uintptr_t maOperands[4]; // +4  words 1..4 -- carried verbatim through the queue
};

// CgsAemsFactory.h:160 (DWARF). Create command: 4 words (tag + 3 operands).
// Type tag E_CSIS_COMMAND_CREATE (== 1). Word count posted: 4.
struct CsisCreateCommand : public CsisCommand
{
    std::uintptr_t maOperands[3]; // +4  words 1..3 -- carried verbatim through the queue
};

// CgsAemsFactory.h:212 (DWARF). Update command: 3 words (tag + 2 operands).
// Type tag E_CSIS_COMMAND_UPDATE (== 3). Word count posted: 3.
struct CsisUpdateCommand : public CsisCommand
{
    std::uintptr_t maOperands[2]; // +4  words 1..2 -- carried verbatim through the queue
};

// CgsAemsFactory.h:240 (DWARF). Post a typed CSIS command record onto the queue and
// return the queue (for chaining). X360: forwards to CommandQueue<>::PostCommand with
// the record's word count (sizeof(T)/sizeof(uintptr_t)) and asserts on overflow.
template <typename T>
CsisCommandQueue& operator<<(CsisCommandQueue& arQueue, const T& arCommand);

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSCSISCOMMANDQUEUE_H
