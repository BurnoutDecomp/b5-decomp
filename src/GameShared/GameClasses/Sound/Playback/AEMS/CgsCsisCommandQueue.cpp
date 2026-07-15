// ============================================================================
// CgsCsisCommandQueue.cpp -- CSIS command-queue typed enqueue (operator<<).
//
// Definition home for the four explicit instantiations of the free operator<< that
// posts a fixed-size CSIS command record onto a CsisCommandQueue. Bodied from
// BURNOUT_X360_ARTIST.XEX:
//   operator<< <CsisSetClassHandleCommand> @ 0x826C64A0  (posts 5 words)
//   operator<< <CsisUpdateCommand>         @ 0x826C6550  (posts 3 words)
//   operator<< <CsisCreateCommand>         @ 0x826C6600  (posts 4 words)
//   operator<< <CsisReleaseCommand>        @ 0x826C66B0  (posts 2 words)
//
// Each instantiation is one generic template body: the X360 loads the record's word
// count into r4 (the literal 4/2/5/3 the sizeof(T)/sizeof(uintptr_t) constant-folds
// to), the command pointer into r5 and the queue into r3, calls
// CommandQueue<>::PostCommand(this=queue, count, words), and -- when the post fails
// (the ring is full) -- runs the BeginAssert / FireAssert / EndAssert sequence that
// the house CGS_ASSERT substitution reproduces. The X360 FireAssert cites
// ".../Sound/Playback/Aems/CgsAemsFactory.h":248 with the message
// "Possibly need to increase CsisCommandQueue::E_QUEUE_LENGTH"; the operator is
// inline-defined in CgsAemsFactory.h in the source build. Member access is BY NAME
// (the ring via CsisCommandQueue::mQueue, the command words via CsisCommand::
// AsCommand()); no raw-offset cast. The function returns the queue (X360: r3 = the
// saved queue pointer) for chaining.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsCsisCommandQueue.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdint> // std::uintptr_t

namespace CgsSound
{
namespace Playback
{

// CgsAemsFactory.h:240 (X360 0x826C64A0 / 0x826C6550 / 0x826C6600 / 0x826C66B0).
// Post the command record's words onto the ring; assert if the ring overflowed. The
// word count is the record's pointer-word footprint (X360: the literal immediate the
// compiler folded sizeof(T)/sizeof(uintptr_t) to -- 5/3/4/2 for the four records).
template <typename T>
CsisCommandQueue& operator<<(CsisCommandQueue& arQueue, const T& arCommand)
{
    const bool lbPosted = arQueue.mQueue.PostCommand(
        static_cast<unsigned int>(sizeof(T) / sizeof(std::uintptr_t)),
        arCommand.AsCommand());

    CGS_ASSERT(lbPosted, "Possibly need to increase CsisCommandQueue::E_QUEUE_LENGTH");

    return arQueue;
}

// The four explicit instantiations the X360 emitted (one per CSIS command record).
template CsisCommandQueue& operator<< <CsisSetClassHandleCommand>(
    CsisCommandQueue& arQueue, const CsisSetClassHandleCommand& arCommand); // 0x826C64A0
template CsisCommandQueue& operator<< <CsisUpdateCommand>(
    CsisCommandQueue& arQueue, const CsisUpdateCommand& arCommand);         // 0x826C6550
template CsisCommandQueue& operator<< <CsisCreateCommand>(
    CsisCommandQueue& arQueue, const CsisCreateCommand& arCommand);         // 0x826C6600
template CsisCommandQueue& operator<< <CsisReleaseCommand>(
    CsisCommandQueue& arQueue, const CsisReleaseCommand& arCommand);        // 0x826C66B0

} // namespace Playback
} // namespace CgsSound
