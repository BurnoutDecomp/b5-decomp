#ifndef CGS_SOUND_PLAYBACK_CGSCOMMANDQUEUE_H
#define CGS_SOUND_PLAYBACK_CGSCOMMANDQUEUE_H

#include "types.hpp"

#include <cstdint> // std::uintptr_t

// =============================================================================
// GameShared/GameClasses/Sound/Playback/CgsCommandQueue.h  (DWARF home)
//
// CgsSound::Playback::CommandQueue<N, T>: a fixed-capacity single-producer/
// single-consumer ring of N command words, plus the CSIS command-queue family
// (CsisCommand / CsisReleaseCommand / CsisCommandQueue and the free operator<<
// that posts a typed command). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This TU's one bodied function is CgsSound::Playback::AemsPlayerVoice::Stop
// @ 0x826DAF10, defined in the sibling CgsCommandQueue.cpp. Stop builds a
// CsisReleaseCommand from the voice's outstanding request handle and posts it onto
// the owning factory's CsisCommandQueue, then clears the handle.
//
// FLAG (CROSS-TU callee): operator<< <CsisReleaseCommand> @ 0x826C66B0 is its own
// (not-yet-done) TU -- it forwards to CommandQueue::PostCommand and asserts on
// overflow. It is DECLARED here (so Stop can call it by name) and resolved at
// consolidation; NOT defined.
//
// FLAG (queue-owner reached BY NAME): the X360 computes the queue address from a
// member of the voice's RWAC base (`*(this+8) - 4 + 104`), an offset/downcast into
// the un-homed GenericRwacVoice/AemsFactory layout. The project rule forbids raw-
// offset member access, so CgsCommandQueue.cpp obtains the queue through a named
// accessor on a minimal modelled owner instead of reproducing that pointer math.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsCommandQueue.h:45 (DWARF): CommandQueue<255u, std::uintptr_t>. A fixed ring of
// QUEUE_LENGTH command words with separate read/write cursors. Templated on the
// capacity and the stored word type.
template <unsigned int QUEUE_LENGTH, typename T>
struct CommandQueue
{
    // CgsCommandQueue.h:79..81 (DWARF).
    T   mauCommandQueue[QUEUE_LENGTH]; // +0    the command-word ring
    u32 mu32Read;                      // +N    consumer cursor
    u32 mu32Write;                     // +N+4  producer cursor

    // CgsCommandQueue.h:47/50.
    CommandQueue() : mu32Read(0u), mu32Write(0u) {}
    ~CommandQueue() {}

    // CgsCommandQueue.h:53. Advance the read cursor past consumed commands.
    void Update();

    // CgsCommandQueue.h:89. Post a multi-word command (auCount words at apWords);
    // returns false when the ring is full. The single-word overloads below are the
    // common case.
    bool PostCommand(unsigned int auCount, const T* apWords);
    // CgsCommandQueue.h:117. Pop a command: arCount words written through apWords.
    bool GetCommand(u32& arCount, T* apWords);
    // CgsCommandQueue.h:148. Post a single-word command.
    bool PostCommand(unsigned int auWord);
    // CgsCommandQueue.h:162. Pop a single-word command.
    bool GetCommand(T& arWord);
};

// CgsAemsFactory.h:59 (DWARF). CSIS command discriminants.
enum ECsisCommandType
{
    E_CSIS_COMMAND_SET_CLASS_HANDLE = 0,
    E_CSIS_COMMAND_CREATE           = 1,
    E_CSIS_COMMAND_RELEASE          = 2,
    E_CSIS_COMMAND_UPDATE           = 3,
};

// CgsAemsFactory.h:40 (DWARF). Base of the CSIS command family: just the type word.
struct CsisCommand
{
    std::uintptr_t muCommandType; // +0

    explicit CsisCommand(ECsisCommandType aeType)
        : muCommandType(static_cast<std::uintptr_t>(aeType)) {}

    ECsisCommandType GetCommandType() const
    {
        return static_cast<ECsisCommandType>(muCommandType);
    }
    const std::uintptr_t* AsCommand() const { return &muCommandType; }

protected:
    explicit CsisCommand(u32 auType) : muCommandType(auType) {}
};

// CgsAemsFactory.h:182 (DWARF). "Release a CSIS class instance" command. Carries the
// class handle to release (modelled as a uintptr_t so the by-handle ctor the X360
// uses can store it directly, mirroring the {type, handle} pair Stop builds).
struct CsisReleaseCommand : public CsisCommand
{
    std::uintptr_t mhClass; // +4  the Csis::Class* / request handle to release

    explicit CsisReleaseCommand(std::uintptr_t ahClass)
        : CsisCommand(E_CSIS_COMMAND_RELEASE), mhClass(ahClass) {}
};

// CgsAemsFactory.h:230 (DWARF). The CSIS command queue: a 255-deep uintptr_t ring.
struct CsisCommandQueue
{
    static const unsigned int E_QUEUE_LENGTH = 255u;

    CommandQueue<E_QUEUE_LENGTH, std::uintptr_t> mQueue; // CgsAemsFactory.h:265

    // CgsAemsFactory.h:256. Pop one command's words.
    bool GetCommand(u32& arCount, std::uintptr_t* apWords);
};

// CgsAemsFactory.h:240 (DWARF). Free operator that posts a typed command onto the
// queue and returns the queue (for chaining). FLAG: own TU @ 0x826C66B0 -- declared
// here, defined elsewhere.
CsisCommandQueue& operator<<(CsisCommandQueue& arQueue, const CsisReleaseCommand& arCommand);

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSCOMMANDQUEUE_H
